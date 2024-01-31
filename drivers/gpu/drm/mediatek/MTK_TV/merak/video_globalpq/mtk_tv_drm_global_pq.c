// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_global_pq.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_pqmap.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "hwreg_render_stub.h"
#include "hwreg_render_common.h"
#include "mtk-efuse.h"
#include "hwreg_render_pnlgamma.h"
#include "hwreg_render_pqgamma.h"

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_GLOBAL_PQ
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define MIN(x, y)         (((x) < (y)) ? (x) : (y))
#define _READY_CB_COUNT 4
#define _CB_INC(x) ((x++)&(_READY_CB_COUNT-1))
#define _CB_DEC(x) (x == 0 ? 0 : --x)

/* Ambilight pixel data*/
#define OLED_SHM_OFFSET 0x603000
#define RGB_API_BASE 4
#define RGB_API_VALID 5
#define BUFFER_INDEX_ADDR 6
#define RGB_REPORT_BASE 0x100
#define Y_0 0
#define Y_540 540
#define Y_1080 1080
#define Y_1620 1620
#define BIT4 0x10
#define AMBILIGHT_WD_TIMEOUT 60
#define AMB_H_UNIT_SIZE 480
#define AMB_V_UNIT_SIZE 540
#define AMB_WIN_NUM 16
#define AMB_DRAM_SIZE 576

//-------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//-------------------------------------------------------------------------------------------------
static struct pqu_render_global_pq _global_pq_msg_info[_READY_CB_COUNT];
static int _global_pq_count;
static void __iomem *u64AmbilightVaddr;

typedef struct AMB_s {
	struct drm_mtk_tv_pixels_report pixel[AMB_WIN_NUM];
} AMB_t;

//-------------------------------------------------------------------------------------------------
//  Local Functions
//-------------------------------------------------------------------------------------------------
static void _ready_cb_pqu_render_global_pq(void)
{
	int ret = 0;

	LOG("pqu cpuif ready callback function\n");

	ret = pqu_render_global_pq(&_global_pq_msg_info[_CB_DEC(_global_pq_count)], NULL);
	if (ret != 0)
		ERR("pqu_render_global_pq fail! (ret=%d)\n", ret);
}

static int _mtk_global_pq_get_memory_info(u32 *addr)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;

	// find memory_info node in dts
	target_memory_np = of_find_node_by_name(NULL, "memory_info");

	if (!target_memory_np)
		return -ENODEV;

	// query cpu_emi0_base value from memory_info node
	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);

	if (p != NULL) {
		*addr = be32_to_cpup(p);
		of_node_put(target_memory_np);
		p = NULL;
	} else {
		DRM_ERROR("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}

static int _mtk_tv_drm_ambilight_shm_addr_init(struct mtk_tv_kms_context *pctx)
{
	int ret = 0;
	uint32_t chip_miu0_bus_base = 0;

	ret = _mtk_global_pq_get_memory_info(&chip_miu0_bus_base);

	if (ret < 0) {
		DRM_ERROR("Failed to get memory info\n");
		return ret;
	}

	if ((pctx->ld_priv.u64LDM_phyAddress != 0) &&
		(pctx->ld_priv.u32LDM_mmapsize != 0) &&
		(pctx->ld_priv.u32LDM_mmapsize >= OLED_SHM_OFFSET)) {
		if (u64AmbilightVaddr == NULL) {
			u64AmbilightVaddr =
			memremap((size_t)(pctx->ld_priv.u64LDM_phyAddress +
				chip_miu0_bus_base + OLED_SHM_OFFSET),
				pctx->ld_priv.u32LDM_mmapsize - OLED_SHM_OFFSET,
				MEMREMAP_WB);
		}

		if ((u64AmbilightVaddr == NULL) || (u64AmbilightVaddr == (void *)((size_t)-1))) {
			DRM_ERROR("[%s, %d][AMBilight]Ambilight memremap error \r\n",
			__func__, __LINE__);
			return -EINVAL;
		}

		DRM_INFO("[AMBilight] LDM phyAddress : 0x%llx\n", pctx->ld_priv.u64LDM_phyAddress);
		DRM_INFO("[AMBilight] VirtAddress : 0x%p\n", u64AmbilightVaddr);
		memset((u64AmbilightVaddr + RGB_REPORT_BASE), 0, AMB_DRAM_SIZE);

	} else {
		DRM_ERROR("[%s, %d][AMBilight]Local Diming Physical Address/Size = 0\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	return ret;
}

static int _mtk_tv_drm_ambilight_is_data_enable(void)
{
	/* Addr: LD base addr + 0x603000 + 0x04, BIT: bit4 */
	if ((*(__u8 *)(u64AmbilightVaddr + RGB_API_BASE)) & (BIT4))
		return TRUE;
	else
		return FALSE;
}

static void _mtk_tv_drm_ambilight_set_buffer_index(enum mtk_global_ambilight_buffer eBuffer)
{
	/* Addr: LD base addr + 0x603000 + 0x06 */
	memset((u64AmbilightVaddr + BUFFER_INDEX_ADDR), eBuffer, 1);
}

static int _mtk_tv_drm_ambilight_is_data_write_done(void)
{
	/* Addr: LD base addr + 0x603000 + 0x05 */
	if ((*(__u8 *)(u64AmbilightVaddr + RGB_API_VALID)) == 1)
		return TRUE;
	else
		return FALSE;
}

static void _mtk_tv_drm_ambilight_set_read_done(void)
{
	/* Addr: LD base addr + 0x603000 + 0x05 */
	memset((u64AmbilightVaddr + RGB_API_VALID), 0, 1);
}

static enum mtk_global_ambilight_buffer _mtk_tv_drm_ambilight_check_buffer_index
(struct drm_mtk_tv_vidwin_rect vidwin_rect)
{
	enum mtk_global_ambilight_buffer eBuffer = EN_BUFFER_MAX;

	switch (vidwin_rect.u16Y) {
	case Y_0:
		if (vidwin_rect.u16Height <= Y_540)
			eBuffer = EN_BUFFER_2;
		else if (vidwin_rect.u16Height <= Y_1080)
			eBuffer = EN_BUFFER_0;
		else
			DRM_ERROR("[%s, %d][AMBilight] window not supported\n", __func__, __LINE__);
		break;
	case Y_540:
		if (vidwin_rect.u16Height <= Y_1080)
			eBuffer = EN_BUFFER_3;
		else
			DRM_ERROR("[%s, %d][AMBilight] window not supported\n", __func__, __LINE__);
		break;
	case Y_1080:
		if (vidwin_rect.u16Height <= Y_1080)
			eBuffer = EN_BUFFER_1;
		else
			DRM_ERROR("[%s, %d][AMBilight] window not supported\n", __func__, __LINE__);
		break;
	case Y_1620:
		eBuffer = EN_BUFFER_2;
		break;
	default:
		break;
	}

	DRM_INFO("[%s, %d][AMBilight] x = %d, y = %d, w = %d, h = %d, Buffer index = %d\n",
		__func__, __LINE__,
		vidwin_rect.u16X, vidwin_rect.u16Y,
		vidwin_rect.u16Width, vidwin_rect.u16Height, eBuffer);

	return eBuffer;
}

static struct drm_mtk_tv_pixels_report _mtk_tv_drm_ambilight_pixels_report_sum
(struct drm_mtk_tv_pixels_report pixels_report_1, struct drm_mtk_tv_pixels_report pixels_report_2)
{
	struct drm_mtk_tv_pixels_report pixels_report_sum;

	if (pixels_report_2.u32RcrMin < pixels_report_1.u32RcrMin)
		pixels_report_sum.u32RcrMin = pixels_report_2.u32RcrMin;
	else
		pixels_report_sum.u32RcrMin = pixels_report_1.u32RcrMin;

	if (pixels_report_2.u32RcrMax > pixels_report_1.u32RcrMax)
		pixels_report_sum.u32RcrMax = pixels_report_2.u32RcrMax;
	else
		pixels_report_sum.u32RcrMax = pixels_report_1.u32RcrMax;

	if (pixels_report_2.u32GYMin < pixels_report_1.u32GYMin)
		pixels_report_sum.u32GYMin = pixels_report_2.u32GYMin;
	else
		pixels_report_sum.u32GYMin = pixels_report_1.u32GYMin;

	if (pixels_report_2.u32GYMax > pixels_report_1.u32GYMax)
		pixels_report_sum.u32GYMax = pixels_report_2.u32GYMax;
	else
		pixels_report_sum.u32GYMax = pixels_report_1.u32GYMax;

	if (pixels_report_2.u32BcbMin < pixels_report_1.u32BcbMin)
		pixels_report_sum.u32BcbMin = pixels_report_2.u32BcbMin;
	else
		pixels_report_sum.u32BcbMin = pixels_report_1.u32BcbMin;

	if (pixels_report_2.u32BcbMax > pixels_report_1.u32BcbMax)
		pixels_report_sum.u32BcbMax = pixels_report_2.u32BcbMax;
	else
		pixels_report_sum.u32BcbMax = pixels_report_1.u32BcbMax;

	pixels_report_sum.u32RCrSum = pixels_report_1.u32RCrSum + pixels_report_2.u32RCrSum;
	pixels_report_sum.u32GYSum = pixels_report_1.u32GYSum + pixels_report_2.u32GYSum;
	pixels_report_sum.u32BcbSum = pixels_report_1.u32BcbSum + pixels_report_2.u32BcbSum;

	return pixels_report_sum;
}

//-------------------------------------------------------------------------------------------------
//  Sysfs
//-------------------------------------------------------------------------------------------------
static ssize_t support_ambient_light_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.support_amb_light);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t render_qmap_total_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pqmap_version qmap_total_version;
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	memset(&qmap_total_version, 0, sizeof(struct pqmap_version));
	ret = mtk_tv_drm_pqmap_get_version(&pctx->pqmap_priv,
		EN_PQMAP_VERSION_TYPE_TOTAL_VER,
		EN_PQMAP_TYPE_MAIN,
		&qmap_total_version);
	if (ret) {
		DRM_ERROR("[%s, %d]: PQmap get version fail!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "qmap_total_ver : Major=0x%x, Minor=0x%x, Revision=0x%x\n",
		qmap_total_version.u16MajorNumber,
		qmap_total_version.u16MinorNumber,
		qmap_total_version.u32Revision);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}


static ssize_t pq_gamma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;
	uint8_t gammaenable = 0;
	uint8_t pregainenable, preoffsetenable = 0;
	uint8_t postgainenable, postoffsetenable = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	drv_hwreg_render_pqgamma_get_gammaenable(&gammaenable);
	drv_hwreg_render_pqgamma_get_pregainoffsetenable(&pregainenable, &preoffsetenable);
	drv_hwreg_render_pqgamma_get_postgainoffsetenable(&postgainenable, &postoffsetenable);

	ret = snprintf(buf, PAGE_SIZE, "pq gamma enable %d\n"
					"pq gamma pre_gain enable %d\n"
					"pq gamma pre_offset enable %d\n"
					"pq gamma post_gain enable %d\n"
					"pq gamma post_offset enable %d\n",
					gammaenable,
					pregainenable, preoffsetenable,
					postgainenable, postoffsetenable);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_gamma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;
	uint8_t gammaenable = 0;
	uint8_t gammaebypass = 0;
	uint8_t gainenable = 0;
	uint8_t offsetenable = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	drv_hwreg_render_pnlgamma_get_gammaenable(&gammaebypass, &gammaenable);
	drv_hwreg_render_pnlgamma_get_gainoffsetenable(&gainenable, &offsetenable);

	ret = snprintf(buf, PAGE_SIZE, "panel gamma bypass %d\n"
					"panel gamma enable %d\n"
					"panel gamma gain enable %d\n"
					"panel gamma offset enable %d\n",
					gammaebypass, gammaenable, gainenable, offsetenable);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static bool _mtk_tv_drm_global_pq_get_value_from_string(char *buf, char *name,
	unsigned int len, int *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		DRM_INFO("Pointer is NULL!\n");
		return false;
	}

	*value = 0;
	cmd = buf;

	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0) {
			if (kstrtoint(tmp_value, 0, value)) {
				DRM_INFO("kstrtoint fail!\n");
				return find;
			}
			find = true;
			DRM_INFO("name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	DRM_INFO("name(%s) was not found!\n", name);

	return find;
}

static DEVICE_ATTR_RO(support_ambient_light);
static DEVICE_ATTR_RO(render_qmap_total_version);
static DEVICE_ATTR_RO(pq_gamma);
static DEVICE_ATTR_RO(panel_gamma);

static struct attribute *mtk_tv_drm_global_pq_attrs[] = {
	&dev_attr_support_ambient_light.attr,
	&dev_attr_render_qmap_total_version.attr,
	&dev_attr_pq_gamma.attr,
	&dev_attr_panel_gamma.attr,
	NULL
};

static const struct attribute_group mtk_tv_drm_global_pq_attr_group = {
	.name = "global_pq",
	.attrs = mtk_tv_drm_global_pq_attrs
};

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
void mtk_tv_drm_global_pq_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_tv_drm_global_pq_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}
}

void mtk_tv_drm_global_pq_remove_sysfs(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove device attribute files");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_tv_drm_global_pq_attr_group);
}

int mtk_tv_drm_global_pq_parse_dts(struct mtk_tv_kms_context *pctx)
{
	int ret = 0;
	u32 value = 0;
	const char *name;
	struct device_node *node;
	struct mtk_global_pq_hw_version *pq_hw_version = &pctx->globalpq_hw_ver;

	node = of_find_compatible_node(NULL, NULL, "MTK-drm-tv-kms");
	if (!node) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		return -EINVAL;
	}

	// read pnlgamma version
	name = "pnlgamma_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pq_hw_version->pnlgamma_version = value;

	// read pqgamma version
	name = "pqgamma_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pq_hw_version->pqgamma_version = value;

	// read Ambient version
	name = "ambient_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: get DTS failed, name = %s \r\n",
			__func__, __LINE__, name);
		return ret;
	}
	pq_hw_version->ambient_version = value;

	return 0;
}


int mtk_tv_drm_global_pq_init(
	struct mtk_global_pq_context *ctx)
{
	struct mtk_tv_drm_metabuf metabuf = {0};

	if (ctx == NULL) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (ctx->init == true) {
		ERR("global pq is already inited");
		return 0;
	}
	if (mtk_tv_drm_metabuf_alloc_by_mmap(
			&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GLOBAL_PQ)) {
		ERR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GLOBAL_PQ);
		return -EPERM;
	}
	if (metabuf.size < MTK_GLOBAL_PQ_STRING_LEN_MAX) {
		ERR("Metabuf too small, %d < %d", metabuf.size, MTK_GLOBAL_PQ_STRING_LEN_MAX);
		mtk_tv_drm_metabuf_free(&metabuf);
		return -EPERM;
	}
	memset(ctx, 0, sizeof(struct mtk_global_pq_context));
	memcpy(&ctx->metabuf, &metabuf, sizeof(struct mtk_tv_drm_metabuf));
	ctx->string_va = metabuf.addr;
	ctx->string_pa = metabuf.mmap_info.phy_addr;
	ctx->init = true;
	return 0;
}

int mtk_tv_drm_global_pq_deinit(
	struct mtk_global_pq_context *ctx)
{
	if (ctx == NULL) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (ctx->init == false) {
		ERR("global pq is not inited");
		return 0;
	}
	mtk_tv_drm_metabuf_free(&ctx->metabuf);
	memset(ctx, 0, sizeof(struct mtk_global_pq_context));
	return 0;
}

int mtk_tv_drm_global_pq_flush(
	struct mtk_global_pq_context *ctx,
	char *global_pq_string)
{
	uint32_t len;
	int ret;

	if (ctx == NULL || global_pq_string == NULL) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (ctx->init == false) {
		ERR("global pq is not inited");
		return -EPERM;
	}

	len = strlen(global_pq_string) + 1;
	if (len >= MTK_GLOBAL_PQ_STRING_LEN_MAX) {
		ERR("Global PQ too long, strlen = %u, max = %u", len, MTK_GLOBAL_PQ_STRING_LEN_MAX);
		return -EPERM;
	}
	strncpy(ctx->string_va, global_pq_string, len);
	LOG("string: '%s'", global_pq_string);
	LOG("length: %d", len);
	drv_STUB("GLOBAL_PQ_LENGTH", len);

	if (bPquEnable) {
		struct pqu_render_global_pq global_pq;

		memset(&global_pq, 0, sizeof(struct pqu_render_global_pq));
		global_pq.pa  = ctx->string_pa;
		global_pq.len = len;
		ret = pqu_render_global_pq((const struct pqu_render_global_pq *)&global_pq, NULL);
		if (ret == -ENODEV) {
			memcpy(&_global_pq_msg_info[_CB_INC(_global_pq_count)],
				&global_pq,
				sizeof(struct pqu_render_global_pq));
			ERR("pqu_render_global_pq register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_render_global_pq);
		} else if (ret != 0) {
			ERR("pqu_render_global_pq fail (ret=%d)\n", ret);
		}
	} else {
		struct msg_render_global_pq global_pq;

		memset(&global_pq, 0, sizeof(struct msg_render_global_pq));
		global_pq.pa  = ctx->string_pa;
		global_pq.len = len;
		pqu_msg_send(PQU_MSG_SEND_RENDER_GLOBAL_PQ, &global_pq);
	}
	return 0;
}

int mtk_tv_drm_global_pq_check(
	void *global_pq_buffer,
	uint32_t size)
{
	char *str = (char *)global_pq_buffer;

	return ((str[size - 1] == '\0') && ((strlen(str) + 1) == size)) ? 0 : -1;
}

int mtk_tv_drm_ambilight_get_data(
	struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ambilight_data *data)
{
	struct mtk_tv_kms_context *pctx = NULL;
	enum mtk_global_ambilight_buffer eCurBufferIndex = EN_BUFFER_MAX;
	uint8_t u8ThreadWaitTime = 0;
	uint8_t u8TotalWindNum = 0;
	uint8_t u8StartWinNum = 0;
	struct drm_mtk_tv_pixels_report pixels_report_sum;
	struct drm_mtk_tv_pixels_report pixels_report_avg;
	AMB_t *pixels_report = NULL;
	int x = 0;
	int ret = 0;

	if (!mtk_tv_crtc || !data) {
		DRM_ERROR("[%s][%d][AMBilight] null ptr, crtc=%p, data=%p\n",
			__func__, __LINE__, mtk_tv_crtc, data);
		return -EINVAL;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	if (pctx->panel_priv.hw_info.pnl_lib_version != DRV_PNL_VERSION0500) {
		DRM_ERROR("[%s][%d][AMBilight] Ambilight not support!!\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (u64AmbilightVaddr == NULL) {
		ret = _mtk_tv_drm_ambilight_shm_addr_init(pctx);
		if (ret != 0) {
			DRM_ERROR("[%s, %d][AMBilight] Get Ambilight SHM address failed!! \r\n",
				__func__, __LINE__);
			return ret;
		}
	}

	if (_mtk_tv_drm_ambilight_is_data_enable() == TRUE) {

		eCurBufferIndex = _mtk_tv_drm_ambilight_check_buffer_index(data->vidwin_rect);
		/* Set Data Report Buffer Index*/
		_mtk_tv_drm_ambilight_set_buffer_index(eCurBufferIndex);

		/* Set SW Read DONE for DARM update */
		_mtk_tv_drm_ambilight_set_read_done();

		/* Wair DRAM write done */
		while (_mtk_tv_drm_ambilight_is_data_write_done() == 0) {
			mdelay(1);
			u8ThreadWaitTime++;
			if (u8ThreadWaitTime == AMBILIGHT_WD_TIMEOUT) {
				DRM_ERROR("[AMBilight] %s %d, DRAM write done timeout\n",
				__func__, __LINE__);
				break;
			}
		}

		u8TotalWindNum = (data->vidwin_rect.u16Width/AMB_H_UNIT_SIZE)*
			(data->vidwin_rect.u16Height/AMB_V_UNIT_SIZE);

		pixels_report = (AMB_t *)((size_t)(u64AmbilightVaddr + RGB_REPORT_BASE));

		/* Find Start window num in 4x8 windows*/
		switch (eCurBufferIndex) {
		case EN_BUFFER_0:
			if (data->vidwin_rect.u16Y == 0)
				u8StartWinNum = data->vidwin_rect.u16X/AMB_H_UNIT_SIZE;
			else
				u8StartWinNum =
				data->vidwin_rect.u16X/AMB_H_UNIT_SIZE + AMB_WIN_NUM/2;
			break;
		case EN_BUFFER_1:
			if (data->vidwin_rect.u16Y == Y_1080)
				u8StartWinNum = data->vidwin_rect.u16X/AMB_H_UNIT_SIZE;
			else
				u8StartWinNum =
				data->vidwin_rect.u16X/AMB_H_UNIT_SIZE + AMB_WIN_NUM/2;
			break;
		case EN_BUFFER_2:
			if (data->vidwin_rect.u16Y == 0)
				u8StartWinNum = data->vidwin_rect.u16X/AMB_H_UNIT_SIZE;
			else
				u8StartWinNum =
				data->vidwin_rect.u16X/AMB_H_UNIT_SIZE + AMB_WIN_NUM/2;
			break;
		case EN_BUFFER_3:
			if (data->vidwin_rect.u16Y == Y_540)
				u8StartWinNum = data->vidwin_rect.u16X/AMB_H_UNIT_SIZE;
			else
				u8StartWinNum =
				data->vidwin_rect.u16X/AMB_H_UNIT_SIZE + AMB_WIN_NUM/2;
			break;
		default:
			u8StartWinNum = 0;
			DRM_ERROR("[%s, %d][AMBilight] Buffer failed\n", __func__, __LINE__);
			break;
		}

		if (pixels_report == NULL) {
			DRM_ERROR("[%s][%d][AMBilight] pixels_report is NULL\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		pixels_report_sum = pixels_report->pixel[u8StartWinNum];
		DRM_INFO("[%s, %d][AMBilight] u8StartWin[%d]\n",
			__func__, __LINE__, u8StartWinNum);

		if (data->vidwin_rect.u16Width/AMB_H_UNIT_SIZE > 1) {
			for (x = 1; x <= (data->vidwin_rect.u16Width/AMB_H_UNIT_SIZE - 1); x++) {
				pixels_report_sum =
					_mtk_tv_drm_ambilight_pixels_report_sum(pixels_report_sum,
						pixels_report->pixel[u8StartWinNum + x]);
			}
		}

		if (data->vidwin_rect.u16Height/AMB_V_UNIT_SIZE > 1) {
			for (x = 0; x <= (data->vidwin_rect.u16Width/AMB_H_UNIT_SIZE - 1); x++) {
				pixels_report_sum =
					_mtk_tv_drm_ambilight_pixels_report_sum(pixels_report_sum,
					pixels_report->pixel[u8StartWinNum + AMB_WIN_NUM/2 + x]);
			}
		}

		pixels_report_avg.u32RcrMin = pixels_report_sum.u32RcrMin;
		pixels_report_avg.u32RcrMax = pixels_report_sum.u32RcrMax;
		pixels_report_avg.u32GYMin = pixels_report_sum.u32GYMin;
		pixels_report_avg.u32GYMax = pixels_report_sum.u32GYMax;
		pixels_report_avg.u32BcbMin = pixels_report_sum.u32BcbMin;
		pixels_report_avg.u32BcbMax = pixels_report_sum.u32BcbMax;
		pixels_report_avg.u32RCrSum = pixels_report_sum.u32RCrSum/u8TotalWindNum;
		pixels_report_avg.u32GYSum = pixels_report_sum.u32GYSum/u8TotalWindNum;
		pixels_report_avg.u32BcbSum = pixels_report_sum.u32BcbSum/u8TotalWindNum;

		data->pixels_report = pixels_report_avg;
	} else {
		DRM_ERROR("[%s][%d][AMBilight] ALG is not enable\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ret;

}


int mtk_tv_drm_get_ambilight_data_ioctl(struct drm_device *dev, void *data,
						struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;

	if (!dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	ret = mtk_tv_drm_ambilight_get_data(mtk_tv_crtc,
		(struct drm_mtk_tv_ambilight_data *)data);

	return ret;
}

int mtk_tv_drm_ambilight_debug(const char *buf, struct device *dev)
{
	int len = 0, n = 0;
	int value = 0;
	char *cmd = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *crtc = NULL;
	struct drm_mtk_tv_ambilight_data stAmbilightData;

	if ((dev == NULL) || (buf == NULL)) {
		pr_err("[Ambilight][%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	crtc = &pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO];
	if (!pctx) {
		pr_err("[Ambilight] drm_ctx is NULL!\n");
		return -EINVAL;
	}

	cmd = vmalloc(sizeof(char) * 0x1000);

	memset(cmd, 0, sizeof(char) * 0x1000);
	len = strlen(buf);

	n = snprintf(cmd, len + 1, "%s", buf);
	if (n < 0) {
		pr_err("snprintf :%d", n);
		return -EPERM;
	}

	memset(&stAmbilightData, 0, sizeof(struct drm_mtk_tv_ambilight_data));

	if (_mtk_tv_drm_global_pq_get_value_from_string(cmd, "x", len, &value))
		stAmbilightData.vidwin_rect.u16X = (__u16)value;

	if (_mtk_tv_drm_global_pq_get_value_from_string(cmd, "y", len, &value))
		stAmbilightData.vidwin_rect.u16Y = (__u16)value;

	if (_mtk_tv_drm_global_pq_get_value_from_string(cmd, "w", len, &value))
		stAmbilightData.vidwin_rect.u16Width = (__u16)value;

	if (_mtk_tv_drm_global_pq_get_value_from_string(cmd, "h", len, &value))
		stAmbilightData.vidwin_rect.u16Height = (__u16)value;

	pr_err("[Ambilight][Input]  x = %d, y = %d, w = %d, h = %d\n",
		stAmbilightData.vidwin_rect.u16X,
		stAmbilightData.vidwin_rect.u16Y,
		stAmbilightData.vidwin_rect.u16Width,
		stAmbilightData.vidwin_rect.u16Height);

	mtk_tv_drm_ambilight_get_data(crtc, &stAmbilightData);

	pr_err("[Ambilight] Data Report\n"
				"RcrMin : %d\n"
				"RcrMax : %d\n"
				"GYMin : %d\n"
				"GYMax : %d\n"
				"BcbMin : %d\n"
				"BcbMax : %d\n"
				"RCrSum : %d\n"
				"GYSum : %d\n"
				"BcbSum : %d\n",
				stAmbilightData.pixels_report.u32RcrMin,
				stAmbilightData.pixels_report.u32RcrMax,
				stAmbilightData.pixels_report.u32GYMin,
				stAmbilightData.pixels_report.u32GYMax,
				stAmbilightData.pixels_report.u32BcbMin,
				stAmbilightData.pixels_report.u32BcbMax,
				stAmbilightData.pixels_report.u32RCrSum,
				stAmbilightData.pixels_report.u32GYSum,
				stAmbilightData.pixels_report.u32BcbSum);

	vfree(cmd);

	return 0;
}



