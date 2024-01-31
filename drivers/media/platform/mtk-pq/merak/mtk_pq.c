// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#include "mtk_pq.h"
#include "mtk_iommu_dtv_api.h"
#include "mtk_pq_common_irq.h"
#include "mtk_pq_common_clk.h"
#include "mtk_pq_common_capability.h"
#include "mtk_pq_svp.h"
#include "mtk_pq_buffer.h"
#include "mtk_pq_hdr.h"
#include "mtk_pq_thermal.h"
#include "mtk_pq_slice.h"
#include "mtk_pq_pqd.h"
#include "mtk_pq_b2r.h"

#include "apiXC.h"
#include "hwreg_common_bin.h"
#include "ext_command_video_if.h"
#include "pqu_msg.h"
#include "m_pqu_pq.h"
#include "hwreg_pq_display_aisr.h"
#include "mtk-efuse.h"

#define PQ_V4L2_DEVICE_NAME		"v4l2-mtk-pq"
#define PQ_VIDEO_DRIVER_NAME		"mtk-pq-drv"
#define PQ_VIDEO_DEVICE_NAME		"mtk-pq-dev"
#define PQ_VIDEO_DEVICE_NAME_LENGTH	20
#define fh_to_ctx(f)	(container_of(f, struct mtk_pq_ctx, fh))
#define ALIGN_DOWNTO_16(x)  ((x >> 4) << 4)
#define STI_PQ_LOG_OFF 0
#define STI_PQ_LOG_INTERVAL 30
#define PQ_PAT_WIN	2

#define MEMC_SHOW_SIZE 255
#define PQ_UCD_ENGINE_R2 1

#define STREAM_ON_ONE_SHOT (2)

__u32 u32DbgLevel = STI_PQ_LOG_OFF;
__u32 u32LogInterval = STI_PQ_LOG_INTERVAL;

struct mtk_pq_platform_device *pqdev;
bool bPquEnable;
__u32 atrace_enable_pq;

static char *devID[PQ_WIN_MAX_NUM] = {"0", "1", "2", "3", "4", "5", "6", "7",
	"8", "9", "10", "11", "12", "13", "14", "15"};

uint8_t requeue_cnt[PQ_WIN_MAX_NUM];
struct output_meta_compare_info _premeta[PQ_WIN_MAX_NUM];
MS_PQBin_Header_Info stPQBinHeaderInfo;

static const struct of_device_id mtk_pq_match[] = {
	{.compatible = "mediatek,pq",},
	{},
};
__u32 u32PQMapBufLen;

//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
//static ssize_t mtk_pq_capability_show(struct device *dev,
	//struct device_attribute *attr, char *buf);
//static ssize_t mtk_pq_capability_store(struct device *dev,
	//struct device_attribute *attr, const char *buf, size_t count);

//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------
static __u64 *_mtk_pq_config_open_bin(__u8 *pName, __u64 *filesize,
	bool is_share, struct mtk_pq_platform_device *pdev)
{
	long lfilezize;
	__u32 ret;
	const char *filename = NULL;
	__u64 *buffer = NULL;
	const struct firmware *fw = NULL;

	if (is_share) {
		if (!pqdev) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
			goto exit;
		}
		if (pqdev->hwmap_config.va == NULL) {
			filename = (const char *)pName;
			ret = request_firmware_direct(&fw, filename, pdev->dev);
			if (ret)
				return NULL;

			lfilezize = fw->size;

			*filesize = lfilezize;
			if (lfilezize <= 0) {
				release_firmware(fw);
				return NULL;
			}
			ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_CONTROLMAP_INDEX,
						&pqdev->hwmap_config.pa, &pqdev->hwmap_config.size);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Failed to mtk_pq_get_dedicate_memory_info(index=%d)\n",
					MMAP_CONTROLMAP_INDEX);
				goto exit;
			}
			pqdev->hwmap_config.va =
				(__u64)memremap(pqdev->hwmap_config.pa, pqdev->hwmap_config.size,
						MEMREMAP_WC);
			if (pqdev->hwmap_config.va == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Failed to memremap(0x%llx, 0x%x)\n",
					pqdev->hwmap_config.pa, pqdev->hwmap_config.size);
				goto exit;
			}
			buffer = (__u64 *)pqdev->hwmap_config.va;
			memcpy(buffer, fw->data, fw->size);
		} else
			buffer = (__u64 *)pqdev->hwmap_config.va;
	} else {
		filename = (const char *)pName;
		ret = request_firmware_direct(&fw, filename, pdev->dev);
		if (ret)
			return NULL;

		lfilezize = fw->size;

		*filesize = lfilezize;
		if (lfilezize <= 0) {
			release_firmware(fw);
			return NULL;
		}
		PQ_MALLOC(buffer, lfilezize);
		memcpy(buffer, fw->data, fw->size);
	}
	if (buffer == NULL) {
		goto exit;
	}
exit:
	release_firmware(fw);

	return buffer;
}

static v4l2_PQ_dv_config_info_t *_mtk_pq_init_dolby_config(void)
{
	int ret = 0;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;

	/* get dolby config memory */
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return NULL;
	}
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_DV_CONFIG_INDEX,
					&pqdev->dv_config.pa, &pqdev->dv_config.size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to mtk_pq_get_dedicate_memory_info(index=%d)\n", MMAP_DV_CONFIG_INDEX);
		return NULL;
	}
	if (pqdev->dv_config.va == NULL) {
		pqdev->dv_config.va =
			(__u64)memremap(pqdev->dv_config.pa, pqdev->dv_config.size, MEMREMAP_WC);
		if (pqdev->dv_config.va == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Failed to memremap(0x%llx, 0x%x)\n",
				pqdev->dv_config.pa, pqdev->dv_config.size);
			return NULL;
		}
	}

	dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
	if (dv_config_info == NULL)
		return NULL;

	/* init debuggability */
	memset(dv_config_info->sysfs_ctrl, 0, V4L2_DV_MAX_DBG_SYSFS_CTRL_SIZE);
	memset(dv_config_info->dovi_pqu_report, 0, V4L2_DV_MAX_PQU_REPORT_SIZE);
	memset(dv_config_info->dv_core_report, 0, V4L2_DV_MAX_CORE_REPORT_SIZE);
	memset(dv_config_info->dv_mipq_report, 0, V4L2_DV_MAX_MIPQ_REPORT_SIZE);

	mtk_pq_dv_refresh_dolby_support(dv_config_info, NULL);

	/* update PR cap */
	mtk_pq_dv_update_pr_cap(dv_config_info, pqdev);

	dv_config_info->bin_info.pa = (__u64)pqdev->dv_config.pa + DV_CONFIG_BIN_OFFSET;
	dv_config_info->bin_info.va = (__u64)pqdev->dv_config.va + DV_CONFIG_BIN_OFFSET;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "dolby bin: va=0x%llx, pa=0x%llx, size=%llu\n",
		(__u64)dv_config_info->bin_info.va,
		(__u64)dv_config_info->bin_info.pa,
		(__u64)dv_config_info->bin_info.size);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "dolby bin: en=%u (bin_info = %zu)\n",
		dv_config_info->bin_info.en,
		sizeof(dv_config_info->bin_info));
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"dolby picture mode: en=%u, num=%u (mode_info size = %zu)\n",
		dv_config_info->mode_info.en,
		dv_config_info->mode_info.num,
		sizeof(dv_config_info->mode_info));

	return dv_config_info;
}

static int _mtk_pq_config_open(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;
	char config_bin_path[PQ_FILE_PATH_LENGTH] = "HWMAP.bin";
	__u64 filesize = 0;
	struct device_node *np;
	__u16 idx = 0;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;
	struct msg_config_info config_info_rv55 = {0};

	//irq
	np = of_find_matching_node(NULL, mtk_pq_match);
	if (np != NULL) {
		idx = of_irq_get(np, 1);
		if (idx < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "of_irq_get failed\n");
			return -EINVAL;
		}
	}
	pdev->config_info.idx = idx;
	pdev->config_info.idx_en = true;

	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "%s is fail\n", config_bin_path);
	} else {
		//load common
		pdev->hwmap_config.va = (__u64)_mtk_pq_config_open_bin(
								(__u8 *)config_bin_path,
								&filesize,
								true, pdev);
		stPQBinHeaderInfo.u32BinStartAddress = (size_t)pdev->hwmap_config.va;

		// send msg to pqu
		pdev->config_info.config = stPQBinHeaderInfo.u32BinStartAddress;
		pdev->config_info.config_en = true;
	}

	/* init dolby config */
	dv_config_info = _mtk_pq_init_dolby_config();

	/* send msg to pqu */
	pdev->config_info.dvconfig = (__u64)dv_config_info;
	pdev->config_info.dvconfigsize = sizeof(v4l2_PQ_dv_config_info_t);
	pdev->config_info.dvconfig_en = true;
	pdev->config_info.dvconfig = pdev->dv_config.va;

	//irq version
	pdev->config_info.u8IRQ_Version = (u8)pdev->pqcaps.u32IRQ_Version;

	//config table version
	pdev->config_info.u8Config_Version   = (u8)pdev->pqcaps.u32Config_Version;
	pdev->config_info.u8DV_PQ_Version    = (u8)pdev->pqcaps.u32DV_PQ_Version;
	pdev->config_info.u8HSY_PQ_Version   = (u8)pdev->pqcaps.u32HSY_Version;
	pdev->config_info.u8YLite_PQ_Version = (u8)pdev->pqcaps.u32YLite_Version;
	pdev->config_info.u8VIP_PQ_Version   = (u8)pdev->pqcaps.u32VIP_Version;
	pdev->config_info.u8AISR_PQ_Version  = (u8)pdev->pqcaps.u32AISR_Version;

	pdev->config_info.ignore_init_en = false;

	/* send ucd msg to pqu */
	pdev->config_info.u16ucd_alg_engine = pdev->ucd_dev.u16ucd_alg_engine;
	pdev->config_info.u64ucd_shm_pa = pdev->ucd_dev.ucd_shm.pa;
	pdev->config_info.u32ucd_shm_size = pdev->ucd_dev.ucd_shm.size;

	mtk_pq_common_config(&(pdev->config_info), false);
	memcpy(&config_info_rv55, &(pdev->config_info), sizeof(struct msg_config_info));
	config_info_rv55.dvconfig = pdev->dv_config.pa;
	config_info_rv55.config = pdev->hwmap_config.pa;
	mtk_pq_common_config(&config_info_rv55, true);

	return ret;
}

static int _mtk_pq_parse_dts_rv55_enable(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;
	struct device *property_dev = pdev->dev;

	bPquEnable |= of_property_read_bool(property_dev->of_node, "rv55_boot");

	return ret;
}

static int _mtk_pq_parse_dts_capbability_misc(struct mtk_pq_platform_device *pdev)
{

	int ret = 0;
	int ret_aisr = 0;
	struct device *property_dev = pdev->dev;
	struct device_node *bank_node;
	struct mtk_pq_caps *pqcaps = &pdev->pqcaps;
	__u32 temp = 0;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "capability");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("Failed to get capability node\r\n");
		return -EINVAL;
	}

	ret |= of_property_read_u32(bank_node, "Support_TS",
		&pqcaps->u32Support_TS);
	PQ_CAPABILITY_CHECK_RET(ret, "Support_TS");
	PQ_MSG_DEBUG("Support_TS = %x\r\n", pqcaps->u32Support_TS);

	ret |= of_property_read_u32(bank_node, "Input_Throughput_Single_Win",
		&pqcaps->u32input_throughput_single_win);
	PQ_CAPABILITY_CHECK_RET(ret, "Input_Throughput_Single_Win");
	PQ_MSG_DEBUG("Input_Throughput_Single_Win = %x\r\n",
	pqcaps->u32input_throughput_single_win);

	ret |= of_property_read_u32(bank_node, "Input_Throughput_Multi_Win",
		&pqcaps->u32input_throughput_multi_win);
	PQ_CAPABILITY_CHECK_RET(ret, "Input_Throughput_Multi_Win");
	PQ_MSG_DEBUG("Input_Throughput_Multi_Win = %x\r\n", pqcaps->u32input_throughput_multi_win);

	ret |= of_property_read_u32(bank_node, "Output_Throughput_Single_Win",
		&pqcaps->u32output_throughput_single_win);
	PQ_CAPABILITY_CHECK_RET(ret, "Output_Throughput_Single_Win");
	PQ_MSG_DEBUG("Output_Throughput_Single_Win = %x\r\n",
	pqcaps->u32output_throughput_single_win);

	ret |= of_property_read_u32(bank_node, "Output_Throughput_Multi_Win",
		&pqcaps->u32output_throughput_multi_win);
	PQ_CAPABILITY_CHECK_RET(ret, "Output_Throughput_Multi_Win");
	PQ_MSG_DEBUG("Output_Throughput_Multi_Win = %x\r\n",
	pqcaps->u32output_throughput_multi_win);

	ret |= of_property_read_u32(bank_node, "Output_Throughput_Win_Num",
		&pqcaps->u32output_throughput_win_num);
	PQ_CAPABILITY_CHECK_RET(ret, "Output_Throughput_Win_Num");
	PQ_MSG_DEBUG("Output_Throughput_Win_Num = %x\r\n", pqcaps->u32output_throughput_win_num);

	ret |= of_property_read_u32(bank_node, "Bw_Vsd_Coe",
		&pqcaps->u32bw_vsd_coe);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Vsd_Coe");
	PQ_MSG_DEBUG("Bw_Vsd_Coe = %x\r\n", pqcaps->u32bw_vsd_coe);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_W_Coe",
		&pqcaps->u32bw_hdmi_w_coe);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_W_Coe");
	PQ_MSG_DEBUG("Bw_Hdmi_W_Coe = %x\r\n", pqcaps->u32bw_hdmi_w_coe);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_Sd_W_Coe",
		&pqcaps->u32bw_hdmi_sd_w_coe);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_Sd_W_Coe");
	PQ_MSG_DEBUG("Bw_Hdmi_Sd_W_Coe = %x\r\n", pqcaps->u32bw_hdmi_sd_w_coe);

	ret |= of_property_read_u32(bank_node, "Bw_Mdla_Coe",
		&pqcaps->u32bw_mdla_coe);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Mdla_Coe");
	PQ_MSG_DEBUG("Bw_Mdla_Coe = %x\r\n", pqcaps->u32bw_mdla_coe);

	ret |= of_property_read_u32(bank_node, "P_Inprocess_Time",
		&pqcaps->u32p_inprocess_time);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Inprocess_Time");
	PQ_MSG_DEBUG("P_Inprocess_Time = %x\r\n", pqcaps->u32p_inprocess_time);

	ret |= of_property_read_u32(bank_node, "I_Inprocess_Time",
		&pqcaps->u32i_inprocess_time);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Inprocess_Time");
	PQ_MSG_DEBUG("I_Inprocess_Time = %x\r\n", pqcaps->u32i_inprocess_time);

	ret |= of_property_read_u32(bank_node, "Outprocess_Time",
		&pqcaps->u32outprocess_time);
	PQ_CAPABILITY_CHECK_RET(ret, "Outprocess_Time");
	PQ_MSG_DEBUG("Outprocess_Time = %x\r\n", pqcaps->u32outprocess_time);

	ret |= of_property_read_u32(bank_node, "Bw_B2r",
		&pqcaps->u32bw_b2r);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_B2r");
	PQ_MSG_DEBUG("Bw_B2r = %x\r\n", pqcaps->u32bw_b2r);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_R",
		&pqcaps->u32bw_hdmi_r);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_R");
	PQ_MSG_DEBUG("Bw_Hdmi_R = %x\r\n", pqcaps->u32bw_hdmi_r);

	ret |= of_property_read_u32(bank_node, "Bw_Dvpr",
		&pqcaps->u32bw_dvpr);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Dvpr");
	PQ_MSG_DEBUG("Bw_Dvpr = %x\r\n", pqcaps->u32bw_dvpr);

	ret |= of_property_read_u32(bank_node, "Bw_Scmi",
		&pqcaps->u32bw_scmi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Scmi");
	PQ_MSG_DEBUG("Bw_Scmi = %x\r\n", pqcaps->u32bw_scmi);

	ret |= of_property_read_u32(bank_node, "Bw_Znr",
		&pqcaps->u32bw_znr);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Znr");
	PQ_MSG_DEBUG("Bw_Znr = %x\r\n", pqcaps->u32bw_znr);

	ret |= of_property_read_u32(bank_node, "Bw_Mdw",
		&pqcaps->u32bw_mdw);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Mdw");
	PQ_MSG_DEBUG("Bw_Mdw = %x\r\n", pqcaps->u32bw_mdw);

	ret |= of_property_read_u32(bank_node, "Bw_B2r_Effi",
		&pqcaps->u32bw_b2r_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_B2r_Effi");
	PQ_MSG_DEBUG("Bw_B2r_Effi = %x\r\n", pqcaps->u32bw_b2r_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_R_Effi",
		&pqcaps->u32bw_hdmi_r_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_R_Effi");
	PQ_MSG_DEBUG("Bw_Hdmi_R_Effi = %x\r\n", pqcaps->u32bw_hdmi_r_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Dvpr_Effi",
		&pqcaps->u32bw_dvpr_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Dvpr_Effi");
	PQ_MSG_DEBUG("Bw_Dvpr_Effi = %x\r\n", pqcaps->u32bw_dvpr_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Scmi_Effi",
		&pqcaps->u32bw_scmi_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Scmi_Effi");
	PQ_MSG_DEBUG("Bw_Scmi_Effi = %x\r\n", pqcaps->u32bw_scmi_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Znr_Effi",
		&pqcaps->u32bw_znr_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Znr_Effi");
	PQ_MSG_DEBUG("Bw_Znr_Effi = %x\r\n", pqcaps->u32bw_znr_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Mdw_Effi",
		&pqcaps->u32bw_mdw_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Mdw_Effi");
	PQ_MSG_DEBUG("Bw_Mdw_Effi = %x\r\n", pqcaps->u32bw_mdw_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Memc_Ip_Effi",
		&pqcaps->u32bw_memc_ip_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Memc_Ip_Effi");
	PQ_MSG_DEBUG("Bw_Memc_Ip_Effi = %x\r\n", pqcaps->u32bw_memc_ip_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Aipq_Effi",
		&pqcaps->u32bw_aipq_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Aipq_Effi");
	PQ_MSG_DEBUG("Bw_Aipq_Effi = %x\r\n", pqcaps->u32bw_aipq_effi);

	ret |= of_property_read_u32(bank_node, "I_Tsin_Phase",
		&pqcaps->u32i_tsin_phase);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Tsin_Phase");
	PQ_MSG_DEBUG("I_Tsin_Phase = %x\r\n", pqcaps->u32i_tsin_phase);

	ret |= of_property_read_u32(bank_node, "P_Tsin_Phase",
		&pqcaps->u32p_tsin_phase);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Tsin_Phase");
	PQ_MSG_DEBUG("P_Tsin_Phase = %x\r\n", pqcaps->u32p_tsin_phase);

	ret |= of_property_read_u32(bank_node, "I_Tsin_Clk_Rate",
		&pqcaps->u32i_tsin_clk_rate);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Tsin_Clk_Rate");
	PQ_MSG_DEBUG("I_Tsin_Clk_Rate = %x\r\n", pqcaps->u32i_tsin_clk_rate);

	ret |= of_property_read_u32(bank_node, "P_Tsin_Clk_Rate",
		&pqcaps->u32p_tsin_clk_rate);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Tsin_Clk_Rate");
	PQ_MSG_DEBUG("P_Tsin_Clk_Rate = %x\r\n", pqcaps->u32p_tsin_clk_rate);

	ret |= of_property_read_u32(bank_node, "Tsout_Phase",
		&pqcaps->u32tsout_phase);
	PQ_CAPABILITY_CHECK_RET(ret, "Tsout_Phase");
	PQ_MSG_DEBUG("Tsout_Phase = %x\r\n", pqcaps->u32tsout_phase);

	ret |= of_property_read_u32(bank_node, "Tsout_Clk_Rate",
		&pqcaps->u32tsout_clk_rate);
	PQ_CAPABILITY_CHECK_RET(ret, "Tsout_Clk_Rate");
	PQ_MSG_DEBUG("Tsout_Clk_Rate = %x\r\n", pqcaps->u32tsout_clk_rate);

	ret |= of_property_read_u32(bank_node, "H_Blk_Reserve_4k",
		&pqcaps->u32h_blk_reserve_4k);
	PQ_CAPABILITY_CHECK_RET(ret, "H_Blk_Reserve_4k");
	PQ_MSG_DEBUG("H_Blk_Reserve_4k = %x\r\n", pqcaps->u32h_blk_reserve_4k);

	ret |= of_property_read_u32(bank_node, "H_Blk_Reserve_8k",
		&pqcaps->u32h_blk_reserve_8k);
	PQ_CAPABILITY_CHECK_RET(ret, "H_Blk_Reserve_8k");
	PQ_MSG_DEBUG("H_Blk_Reserve_8k = %x\r\n", pqcaps->u32h_blk_reserve_8k);

	ret |= of_property_read_u32(bank_node, "Frame_V_Active_Time_60",
		&pqcaps->u32frame_v_active_time_60);
	PQ_CAPABILITY_CHECK_RET(ret, "Frame_V_Active_Time_60");
	PQ_MSG_DEBUG("Frame_V_Active_Time_60 = %x\r\n", pqcaps->u32frame_v_active_time_60);

	ret |= of_property_read_u32(bank_node, "Frame_V_Active_Time_120",
		&pqcaps->u32frame_v_active_time_120);
	PQ_CAPABILITY_CHECK_RET(ret, "Frame_V_Active_Time_120");
	PQ_MSG_DEBUG("Frame_V_Active_Time_120 = %x\r\n", pqcaps->u32frame_v_active_time_120);

	ret |= of_property_read_u32(bank_node, "Chipcap_Tsin_Outthroughput",
		&pqcaps->u32chipcap_tsin_outthroughput);
	PQ_CAPABILITY_CHECK_RET(ret, "Chipcap_Tsin_Outthroughput");
	PQ_MSG_DEBUG("Chipcap_Tsin_Outthroughput = %x\r\n", pqcaps->u32chipcap_tsin_outthroughput);

	ret |= of_property_read_u32(bank_node, "Chipcap_Mdw_Outthroughput",
		&pqcaps->u32chipcap_mdw_outthroughput);
	PQ_CAPABILITY_CHECK_RET(ret, "Chipcap_Mdw_Outthroughput");
	PQ_MSG_DEBUG("Chipcap_Mdw_Outthroughput = %x\r\n", pqcaps->u32chipcap_mdw_outthroughput);


	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi0",
		&pqcaps->u32p_mode_pqlevel_scmi0);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi0");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi0 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi0);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi1",
		&pqcaps->u32p_mode_pqlevel_scmi1);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi1");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi1 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi1);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi2",
		&pqcaps->u32p_mode_pqlevel_scmi2);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi2");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi2 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi2);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi3",
		&pqcaps->u32p_mode_pqlevel_scmi3);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi3");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi3 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi3);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi4",
		&pqcaps->u32p_mode_pqlevel_scmi4);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi4");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi4 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi4);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Scmi5",
		&pqcaps->u32p_mode_pqlevel_scmi5);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Scmi5");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Scmi5 = %x\r\n", pqcaps->u32p_mode_pqlevel_scmi5);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr0",
		&pqcaps->u32p_mode_pqlevel_znr0);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr0");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr0 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr0);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr1",
		&pqcaps->u32p_mode_pqlevel_znr1);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr1");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr1 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr1);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr2",
		&pqcaps->u32p_mode_pqlevel_znr2);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr2");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr2 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr2);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr3",
		&pqcaps->u32p_mode_pqlevel_znr3);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr3");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr3 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr3);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr4",
		&pqcaps->u32p_mode_pqlevel_znr4);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr4");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr4 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr4);

	ret |= of_property_read_u32(bank_node, "P_Mode_Pqlevel_Znr5",
		&pqcaps->u32p_mode_pqlevel_znr5);
	PQ_CAPABILITY_CHECK_RET(ret, "P_Mode_Pqlevel_Znr5");
	PQ_MSG_DEBUG("P_Mode_Pqlevel_Znr5 = %x\r\n", pqcaps->u32p_mode_pqlevel_znr5);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi0",
		&pqcaps->u32i_mode_pqlevel_scmi0);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi0");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi0 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi0);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi1",
		&pqcaps->u32i_mode_pqlevel_scmi1);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi1");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi1 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi1);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi2",
		&pqcaps->u32i_mode_pqlevel_scmi2);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi2");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi2 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi2);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi3",
		&pqcaps->u32i_mode_pqlevel_scmi3);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi3");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi3 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi3);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi4",
		&pqcaps->u32i_mode_pqlevel_scmi4);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi4");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi4 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi4);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Scmi5",
		&pqcaps->u32i_mode_pqlevel_scmi5);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Scmi5");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Scmi5 = %x\r\n", pqcaps->u32i_mode_pqlevel_scmi5);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr0",
		&pqcaps->u32i_mode_pqlevel_znr0);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr0");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr0 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr0);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr1",
		&pqcaps->u32i_mode_pqlevel_znr1);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr1");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr1 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr1);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr2",
		&pqcaps->u32i_mode_pqlevel_znr2);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr2");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr2 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr2);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr3",
		&pqcaps->u32i_mode_pqlevel_znr3);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr3");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr3 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr3);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr4",
		&pqcaps->u32i_mode_pqlevel_znr4);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr4");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr4 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr4);

	ret |= of_property_read_u32(bank_node, "I_Mode_Pqlevel_Znr5",
		&pqcaps->u32i_mode_pqlevel_znr5);
	PQ_CAPABILITY_CHECK_RET(ret, "I_Mode_Pqlevel_Znr5");
	PQ_MSG_DEBUG("I_Mode_Pqlevel_Znr5 = %x\r\n", pqcaps->u32i_mode_pqlevel_znr5);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table0",
		&pqcaps->u32reserve_ml_table0);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table0");
	PQ_MSG_DEBUG("Reserve_Ml_Table0 = %x\r\n", pqcaps->u32reserve_ml_table0);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table1",
		&pqcaps->u32reserve_ml_table1);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table1");
	PQ_MSG_DEBUG("Reserve_Ml_Table1 = %x\r\n", pqcaps->u32reserve_ml_table1);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table2",
		&pqcaps->u32reserve_ml_table2);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table2");
	PQ_MSG_DEBUG("Reserve_Ml_Table2 = %x\r\n", pqcaps->u32reserve_ml_table2);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table3",
		&pqcaps->u32reserve_ml_table3);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table3");
	PQ_MSG_DEBUG("Reserve_Ml_Table3 = %x\r\n", pqcaps->u32reserve_ml_table3);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table4",
		&pqcaps->u32reserve_ml_table4);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table4");
	PQ_MSG_DEBUG("Reserve_Ml_Table4 = %x\r\n", pqcaps->u32reserve_ml_table4);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table5",
		&pqcaps->u32reserve_ml_table5);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table5");
	PQ_MSG_DEBUG("Reserve_Ml_Table5 = %x\r\n", pqcaps->u32reserve_ml_table5);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table6",
		&pqcaps->u32reserve_ml_table6);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table6");
	PQ_MSG_DEBUG("Reserve_Ml_Table6 = %x\r\n", pqcaps->u32reserve_ml_table6);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table7",
		&pqcaps->u32reserve_ml_table7);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table7");
	PQ_MSG_DEBUG("Reserve_Ml_Table7 = %x\r\n", pqcaps->u32reserve_ml_table7);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table8",
		&pqcaps->u32reserve_ml_table8);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table8");
	PQ_MSG_DEBUG("Reserve_Ml_Table8 = %x\r\n", pqcaps->u32reserve_ml_table8);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table9",
		&pqcaps->u32reserve_ml_table9);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table9");
	PQ_MSG_DEBUG("Reserve_Ml_Table9 = %x\r\n", pqcaps->u32reserve_ml_table9);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table10",
		&pqcaps->u32reserve_ml_table10);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table10");
	PQ_MSG_DEBUG("Reserve_Ml_Table10 = %x\r\n", pqcaps->u32reserve_ml_table10);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table11",
		&pqcaps->u32reserve_ml_table11);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table11");
	PQ_MSG_DEBUG("Reserve_Ml_Table11 = %x\r\n", pqcaps->u32reserve_ml_table11);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table12",
		&pqcaps->u32reserve_ml_table12);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table12");
	PQ_MSG_DEBUG("Reserve_Ml_Table12 = %x\r\n", pqcaps->u32reserve_ml_table12);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table13",
		&pqcaps->u32reserve_ml_table13);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table13");
	PQ_MSG_DEBUG("Reserve_Ml_Table13 = %x\r\n", pqcaps->u32reserve_ml_table13);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table14",
		&pqcaps->u32reserve_ml_table14);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table14");
	PQ_MSG_DEBUG("Reserve_Ml_Table14 = %x\r\n", pqcaps->u32reserve_ml_table14);

	ret |= of_property_read_u32(bank_node, "Reserve_Ml_Table15",
		&pqcaps->u32reserve_ml_table15);
	PQ_CAPABILITY_CHECK_RET(ret, "Reserve_Ml_Table15");
	PQ_MSG_DEBUG("Reserve_Ml_Table15 = %x\r\n", pqcaps->u32reserve_ml_table15);

	ret |= of_property_read_u32(bank_node, "Bwofts_Height",
		&pqcaps->u32bwofts_height);
	PQ_CAPABILITY_CHECK_RET(ret, "Bwofts_Height");
	PQ_MSG_DEBUG("Bwofts_Height = %x\r\n", pqcaps->u32bwofts_height);

	ret |= of_property_read_u32(bank_node, "Bwofts_Width",
		&pqcaps->u32bwofts_width);
	PQ_CAPABILITY_CHECK_RET(ret, "Bwofts_Width");
	PQ_MSG_DEBUG("Bwofts_Width = %x\r\n", pqcaps->u32bwofts_width);

	ret |= of_property_read_u32(bank_node, "Bw_Vsd_Effi",
		&pqcaps->u32bw_vsd_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Vsd_Effi");
	PQ_MSG_DEBUG("Bw_Vsd_Effi = %x\r\n", pqcaps->u32bw_vsd_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_W_Effi",
		&pqcaps->u32bw_hdmi_w_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_W_Effi");
	PQ_MSG_DEBUG("Bw_Hdmi_W_Effi = %x\r\n", pqcaps->u32bw_hdmi_w_effi);

	ret |= of_property_read_u32(bank_node, "Bw_Hdmi_Sd_W_Effi",
		&pqcaps->u32bw_hdmi_sd_w_effi);
	PQ_CAPABILITY_CHECK_RET(ret, "Bw_Hdmi_Sd_W_Effi");
	PQ_MSG_DEBUG("Bw_Hdmi_Sd_W_Effi = %x\r\n", pqcaps->u32bw_hdmi_sd_w_effi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Single_Hdmi",
		&pqcaps->u32memc_ip_single_hdmi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Single_Hdmi");
	PQ_MSG_DEBUG("Memc_Ip_Single_Hdmi = %x\r\n", pqcaps->u32memc_ip_single_hdmi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Single_Mm",
		&pqcaps->u32memc_ip_single_mm);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Single_Mm");
	PQ_MSG_DEBUG("Memc_Ip_Single_Mm = %x\r\n", pqcaps->u32memc_ip_single_mm);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Multi",
		&pqcaps->u32memc_ip_multi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Multi");
	PQ_MSG_DEBUG("Memc_Ip_Multi = %x\r\n", pqcaps->u32memc_ip_multi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Repeat",
		&pqcaps->u32memc_ip_repeat);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Repeat");
	PQ_MSG_DEBUG("Memc_Ip_Repeat = %x\r\n", pqcaps->u32memc_ip_repeat);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Pcmode",
		&pqcaps->u32memc_ip_pcmode);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Pcmode");
	PQ_MSG_DEBUG("Memc_Ip_Pcmode = %x\r\n", pqcaps->u32memc_ip_pcmode);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Memclevel_Single_Hdmi",
		&pqcaps->u32memc_ip_memclevel_single_hdmi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Memclevel_Single_Hdmi");
	PQ_MSG_DEBUG("Memc_Ip_Memclevel_Single_Hdmi = %x\r\n",
	pqcaps->u32memc_ip_memclevel_single_hdmi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Memclevel_Single_Mm",
		&pqcaps->u32memc_ip_memclevel_single_mm);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Memclevel_Single_Mm");
	PQ_MSG_DEBUG("Memc_Ip_Memclevel_Single_Mm = %x\r\n",
	pqcaps->u32memc_ip_memclevel_single_mm);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Memclevel_Multi",
		&pqcaps->u32memc_ip_memclevel_multi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Memclevel_Multi");
	PQ_MSG_DEBUG("Memc_Ip_Memclevel_Multi = %x\r\n", pqcaps->u32memc_ip_memclevel_multi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Colorformat_Single_Hdmi",
		&pqcaps->u32memc_ip_colorformat_single_hdmi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Colorformat_Single_Hdmi");
	PQ_MSG_DEBUG("Memc_Ip_Colorformat_Single_Hdmi = %x\r\n",
	pqcaps->u32memc_ip_colorformat_single_hdmi);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Colorformat_Single_Mm",
		&pqcaps->u32memc_ip_colorformat_single_mm);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Colorformat_Single_Mm");
	PQ_MSG_DEBUG("Memc_Ip_Colorformat_Single_Mm = %x\r\n",
		pqcaps->u32memc_ip_colorformat_single_mm);

	ret |= of_property_read_u32(bank_node, "Memc_Ip_Colorformat_Multi",
		&pqcaps->u32memc_ip_colorformat_multi);
	PQ_CAPABILITY_CHECK_RET(ret, "Memc_Ip_Colorformat_Multi");
	PQ_MSG_DEBUG("Memc_Ip_Colorformat_Multi = %x\r\n", pqcaps->u32memc_ip_colorformat_multi);

	ret |= of_property_read_u32(bank_node, "Forcep_Pqlevel_H",
		&pqcaps->u32forcep_pqlevel_h);
	PQ_CAPABILITY_CHECK_RET(ret, "Forcep_Pqlevel_H");
	PQ_MSG_DEBUG("Forcep_Pqlevel_H = %x\r\n", pqcaps->u32forcep_pqlevel_h);

	ret |= of_property_read_u32(bank_node, "Forcep_Pqlevel_L",
		&pqcaps->u32forcep_pqlevel_l);
	PQ_CAPABILITY_CHECK_RET(ret, "Forcep_Pqlevel_L");
	PQ_MSG_DEBUG("Forcep_Pqlevel_L = %x\r\n", pqcaps->u32forcep_pqlevel_l);

	ret |= of_property_read_u32(bank_node, "Ic_Code",
		&pqcaps->u32ic_code);
	PQ_CAPABILITY_CHECK_RET(ret, "Ic_Code");
	PQ_MSG_DEBUG("Ic_Code = %x\r\n", pqcaps->u32ic_code);

	ret |= of_property_read_u32(bank_node, "HFR_Check", &temp);
	PQ_CAPABILITY_CHECK_RET(ret, "HFR_Check");
	PQ_MSG_DEBUG("HFR_Check = %x\r\n", temp);
	pqcaps->u8HFR_Check = (__u8)temp;

	ret = of_property_read_u32(bank_node, "Window_Num",
		&pqcaps->u32Window_Num);
	PQ_CAPABILITY_CHECK_RET(ret, "Window_Num");
	PQ_MSG_DEBUG("Window_Num = %x\r\n", pqcaps->u32Window_Num);

	ret |= of_property_read_u32(bank_node, "SRS_SUPPORT",
		&pqcaps->u32SRS_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "SRS_SUPPORT");
	PQ_MSG_DEBUG("SRS_SUPPORT = %x\r\n", pqcaps->u32SRS_Support);

	ret |= of_property_read_u32(bank_node, "AISR_SUPPORT",
		&pqcaps->u32AISR_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "AISR_SUPPORT");
	ret_aisr = drv_hwreg_pq_display_aisr_enable_efuse(pqcaps->u32AISR_Support);
	pqcaps->u32AISR_Support = (pqcaps->u32AISR_Support && ret_aisr);
	PQ_MSG_DEBUG("AISR_SUPPORT = %x, EFUSE = %d\r\n", pqcaps->u32AISR_Support, ret_aisr);

	ret |= of_property_read_u32(bank_node, "Phase_IP2",
		&pqcaps->u32Phase_IP2);
	PQ_CAPABILITY_CHECK_RET(ret, "Phase_IP2");
	PQ_MSG_DEBUG("Phase_IP2 = %x\r\n", pqcaps->u32Phase_IP2);

	ret |= of_property_read_u32(bank_node, "AISR_VERSION",
		&pqcaps->u32AISR_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "AISR_VERSION");
	PQ_MSG_DEBUG("AISR_VERSION = %x\r\n", pqcaps->u32AISR_Version);

	ret |= of_property_read_u32(bank_node, "HSY_SUPPORT",
		&pqcaps->u32HSY_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "HSY_SUPPORT");
	PQ_MSG_DEBUG("HSY_SUPPORT = %x\r\n", pqcaps->u32HSY_Support);

	ret |= of_property_read_u32(bank_node, "IRQ_Version",
		&pqcaps->u32IRQ_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "IRQ_Version");
	PQ_MSG_DEBUG("IRQ_Version = %x\r\n", pqcaps->u32IRQ_Version);

	ret |= of_property_read_u32(bank_node, "CFD_PQ_HWVersion",
		&pqcaps->u32CFD_PQ_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "CFD_PQ_HWVersion");
	PQ_MSG_DEBUG("CFD_PQ_HWVersion = %x\r\n", pqcaps->u32CFD_PQ_Version);

	ret |= of_property_read_u32(bank_node, "MDW_VERSION",
		&pqcaps->u32MDW_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "MDW_VERSION");
	PQ_MSG_DEBUG("MDW_VERSION = %x\r\n", pqcaps->u32MDW_Version);

	ret |= of_property_read_u32(bank_node, "Config_Version",
		&pqcaps->u32Config_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "Config_Version");
	PQ_MSG_DEBUG("Config_Version = %x\r\n", pqcaps->u32Config_Version);

	ret |= of_property_read_u32(bank_node, "HAL_Version",
		&pqcaps->u32HAL_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "HAL_Version");
	PQ_MSG_INFO("HAL_Version = %x\r\n", pqcaps->u32HAL_Version);

	ret |= of_property_read_u32(bank_node, "IDR_SWMODE_SUPPORT",
		&pqcaps->u32Idr_SwMode_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "IDR_SWMODE_SUPPORT");
	PQ_MSG_DEBUG("IDR_SWMODE_SUPPORT = %x\r\n", pqcaps->u32Idr_SwMode_Support);

	ret |= of_property_read_u32(bank_node, "DV_PQ_Version",
		&pqcaps->u32DV_PQ_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "DV_PQ_Version");
	PQ_MSG_DEBUG("DV_PQ_Version = %x\r\n", pqcaps->u32DV_PQ_Version);

	ret |= of_property_read_u32(bank_node, "HSY_VERSION",
		&pqcaps->u32HSY_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "HSY_VERSION");
	PQ_MSG_DEBUG("HSY_VERSION = %x\r\n", pqcaps->u32HSY_Version);

	ret |= of_property_read_u32(bank_node, "QMAP_VERSION",
		&pqcaps->u32Qmap_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "QMAP_VERSION");
	PQ_MSG_DEBUG("QMAP_VERSION = %x\r\n", pqcaps->u32Qmap_Version);

	ret |= of_property_read_u32(bank_node, "IP2_CROP_Version",
		&pqcaps->u32IP2_Crop_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "IP2_CROP_Version");
	PQ_MSG_DEBUG("IP2_CROP_Version = %x\r\n", pqcaps->u32IP2_Crop_Version);

	ret |= of_property_read_u32(bank_node, "SCREEN_SEAMLESS_SUPPORT",
		&pqcaps->u32ScreenSeamless_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "SCREEN_SEAMLESS_SUPPORT");
	PQ_MSG_DEBUG("SCREEN_SEAMLESS_SUPPORT = %x\r\n",
	pqcaps->u32ScreenSeamless_Support);

	ret |= of_property_read_u32(bank_node, "PQ_Input_Queue_Min", &temp);
	PQ_CAPABILITY_CHECK_RET(ret, "PQ_Input_Queue_Min");
	PQ_MSG_DEBUG("PQ_Input_Queue_Min = %x\r\n", temp);
	pqcaps->u8PQ_InQueue_Min = (__u8)temp;

	ret |= of_property_read_u32(bank_node, "PQ_Output_Queue_Min", &temp);
	PQ_CAPABILITY_CHECK_RET(ret, "PQ_Output_Queue_Min");
	PQ_MSG_DEBUG("PQ_Output_Queue_Min = %x\r\n", temp);
	pqcaps->u8PQ_OutQueue_Min = (__u8)temp;

	ret |= of_property_read_u32(bank_node, "YLITE_Version",
		&pqcaps->u32YLite_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "YLITE_Version");
	PQ_MSG_DEBUG("YLITE_Version = %x\r\n", pqcaps->u32YLite_Version);

	ret |= of_property_read_u32(bank_node, "VIP_Version",
		&pqcaps->u32VIP_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "VIP_Version");
	PQ_MSG_DEBUG("VIP_Version = %x\r\n", pqcaps->u32VIP_Version);

	ret |= of_property_read_u32(bank_node, "DIPMAP_VERSION",
		&pqcaps->u32Dipmap_Version);
	PQ_CAPABILITY_CHECK_RET(ret, "DIPMAP_VERSION");
	PQ_MSG_DEBUG("DIPMAP_VERSION = %x\r\n", pqcaps->u32Dipmap_Version);

	ret |= of_property_read_u32(bank_node, "SCMI_WRITE_LIMIT_SUPPORT", &temp);
	PQ_CAPABILITY_CHECK_RET(ret, "SCMI_WRITE_LIMIT_SUPPORT");
	PQ_MSG_DEBUG("SCMI_WRITE_LIMIT_SUPPORT = %x\r\n", temp);
	pqcaps->u8Scmi_Write_Limit_Support = (__u8)temp;

	ret |= of_property_read_u32(bank_node, "UCM_WRITE_LIMIT_SUPPORT", &temp);
	PQ_CAPABILITY_CHECK_RET(ret, "UCM_WRITE_LIMIT_SUPPORT");
	PQ_MSG_DEBUG("UCM_WRITE_LIMIT_SUPPORT = %x\r\n", temp);
	pqcaps->u8Ucm_Write_Limit_Support = (__u8)temp;

	ret |= of_property_read_u32(bank_node, "FULLY_PQ_CNT", &pqcaps->u32Fully_PQ_Cnt);
	PQ_CAPABILITY_CHECK_RET(ret, "FULLY_PQ_CNT");
	PQ_MSG_DEBUG("FULLY_PQ_CNT = %x\r\n", pqcaps->u32Fully_PQ_Cnt);

	ret |= of_property_read_u32(bank_node, "ZNR_SUPPORT", &pqcaps->u32ZNR_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "ZNR_SUPPORT");
	PQ_MSG_DEBUG("ZNR_SUPPORT = %u\r\n", pqcaps->u32ZNR_Support);

	ret |= of_property_read_u32(bank_node, "QMAP_HEAP_SUPPORT",
		&pqcaps->u32Qmap_Heap_Support);
	PQ_CAPABILITY_CHECK_RET(ret, "QMAP_HEAP_SUPPORT");
	PQ_MSG_DEBUG("QMAP_HEAP_SUPPORT = %x\r\n", pqcaps->u32Qmap_Heap_Support);

	if (ret)
		return -EINVAL;

	return 0;
}

static int _mtk_pq_parse_dts_cus_id(struct mtk_pq_platform_device *pdev)
{

	int ret = 0, idx = 0, len = 0;
	__u32 u32Tmp = 0;
	struct device *property_dev = pdev->dev;
	struct device_node *bank_node;
	char tmp_dev[PQ_VIDEO_DEVICE_NAME_LENGTH] = {'\0'};

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "cus-dev-id");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("Failed to get cus-dev-id node\r\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(bank_node, "enable", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get cus_dev resource\r\n");
		goto Fail;
	}
	pdev->cus_dev = u32Tmp;

	for (idx = 0; idx < PQ_WIN_MAX_NUM; idx++) {
		len = snprintf(tmp_dev, sizeof(tmp_dev), "%s", PQ_VIDEO_DEVICE_NAME);
		snprintf(tmp_dev + len, sizeof(tmp_dev) - len, "%s", devID[idx]);

		ret = of_property_read_u32(bank_node, tmp_dev, &u32Tmp);
		if (ret) {
			PQ_MSG_ERROR("Failed to get %s resource\r\n", tmp_dev);
			goto Fail;
		}
		pdev->cus_id[idx] = u32Tmp;
		PQ_MSG_INFO("pdev->cus_id[%d] = %d\r\n", idx, pdev->cus_id[idx]);
	}

	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}
static int _mtk_pq_parse_dts_p_engine(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;
	struct device *property_dev = pdev->dev;
	struct device_node *bank_node;
	struct mtk_pq_caps *pqcaps = &pdev->pqcaps;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "p-engine");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("Failed to get p-engine node\r\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_IP2", &pqcaps->u32P_ENGINE_IP2);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_IP2 resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_DI", &pqcaps->u32P_ENGINE_DI);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_DI resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_AISR", &pqcaps->u32P_ENGINE_AISR);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_AISR resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_SCL2", &pqcaps->u32P_ENGINE_SCL2);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_SCL2 resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_SRS", &pqcaps->u32P_ENGINE_SRS);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_SRS resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_VIP", &pqcaps->u32P_ENGINE_VIP);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_VIP resource\r\n");
		goto Fail;
	}

	ret = of_property_read_u32(bank_node, "P_ENGINE_MDW", &pqcaps->u32P_ENGINE_MDW);
	if (ret) {
		PQ_MSG_ERROR("Failed to get P_ENGINE_MDW resource\r\n");
		goto Fail;
	}

	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

static int _mtk_pq_parse_dts_ucd(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	struct device *property_dev = NULL;
	struct mtk_pq_ucd_device *ucd_dev = NULL;
	struct of_mmap_info_data of_mmap_info;
	struct device_node *np = NULL;
	__u32 u32temp = 0;
	char *name = NULL;

	if (pqdev == NULL) {
		PQ_MSG_ERROR("Pointer is NULL!\n");
		return -EINVAL;
	}
	property_dev = pqdev->dev;
	ucd_dev = &pqdev->ucd_dev;

	if ((property_dev == NULL) || (ucd_dev == NULL)) {
		PQ_MSG_ERROR("Failed for null ponter\r\n");
		return -EINVAL;
	}

	memset(&of_mmap_info, 0, sizeof(struct of_mmap_info_data));

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	np = of_find_node_by_name(property_dev->of_node, "mtk-ucd");

	if (np == NULL) {
		PQ_MSG_ERROR("Failed to get mtk-ucd node\r\n");
		return -EINVAL;
	}

	name = "Ucd_Alg_Engine";
	ret = of_property_read_u32(np, name, &u32temp);
	if (ret != 0x0) {
		PQ_MSG_ERROR("Failed to get Ucd_Alg_Engine\r\n");
		goto Fail;
	}
	ucd_dev->u16ucd_alg_engine = (u16)u32temp;

	/* only ucd algo in R2 needs shared memory */
	if (ucd_dev->u16ucd_alg_engine == PQ_UCD_ENGINE_R2) {
		ret = of_mtk_get_reserved_memory_info_by_idx(np, MMAP_UCD_INDEX, &of_mmap_info);
		if (ret) {
			PQ_MSG_ERROR("Failed to of_mtk_get_reserved_memory_info_by_idx(index=%d)\n",
				MMAP_UCD_INDEX);
			goto Fail;
		}
		ucd_dev->ucd_shm.pa = of_mmap_info.start_bus_address;
		ucd_dev->ucd_shm.size = of_mmap_info.buffer_size;
	} else {
		ucd_dev->ucd_shm.pa = 0;
		ucd_dev->ucd_shm.size = 0;
	}

Fail:
	if (np != NULL)
		of_node_put(np);

	return ret;
}

static int _mtk_pq_parse_dts_customization(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	__u32 u32Tmp = 0;
	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;
	struct cus01_pq_config_info *pq_config_info = NULL;

	if (pqdev == NULL) {
		PQ_MSG_ERROR("Pointer is NULL!\n");
		return -EINVAL;
	}

	property_dev = pqdev->dev;
	pq_config_info = &pqdev->pq_config_info;

	if ((property_dev == NULL) || (pq_config_info == NULL)) {
		PQ_MSG_ERROR("Failed for null ponter\r\n");
		return -EINVAL;
	}

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "cus01-customization");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("Failed to get cus01-customization node\r\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(bank_node, "G9_MUTE", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get G9_MUTE resource\r\n");
		goto Fail;
	}

	if (u32Tmp == 1)
		pq_config_info->g9_mute = true;
	else
		pq_config_info->g9_mute = false;

	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}


static int _mtk_pq_parse_dts(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;

	ret = _mtk_pq_parse_dts_rv55_enable(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = _mtk_pq_parse_dts_capbability_misc(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = _mtk_pq_parse_dts_cus_id(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = _mtk_pq_parse_dts_p_engine(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = _mtk_pq_parse_dts_ucd(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = _mtk_pq_parse_dts_customization(pqdev);
	if (ret == -EINVAL)
		return ret;

	return 0;
}

static int _mtk_pq_dv_init(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	struct mtk_pq_dv_ctrl_init_in dv_ctrl_in;
	struct mtk_pq_dv_ctrl_init_out dv_ctrl_out;
	struct mtk_pq_dv_win_init_in dv_win_in;
	struct mtk_pq_dv_win_init_out dv_win_out;
	struct of_mmap_info_data of_mmap_info = {0};
	__u8 win_num = 0;
	__u8 cnt = 0;
	struct mtk_pq_device *pq_dev = NULL;

	/* initialize Dolby control */
	memset(&dv_ctrl_in, 0, sizeof(struct mtk_pq_dv_ctrl_init_in));
	memset(&dv_ctrl_out, 0, sizeof(struct mtk_pq_dv_ctrl_init_out));
	memset(&of_mmap_info, 0, sizeof(struct of_mmap_info_data));

	dv_ctrl_in.pqdev = pqdev;
	dv_ctrl_in.mmap_size = 0;
	dv_ctrl_in.mmap_addr = 0;

	/* get dolby dma address from MMAP */
	ret = of_mtk_get_reserved_memory_info_by_idx(
		pqdev->dev->of_node, MMAP_DV_PYR_INDEX, &of_mmap_info);
	if (ret < 0)
		PQ_MSG_ERROR("mmap return %d\n", ret);
	else {
		dv_ctrl_in.mmap_size = of_mmap_info.buffer_size;
		dv_ctrl_in.mmap_addr = of_mmap_info.start_bus_address;
	}

	ret = mtk_pq_dv_ctrl_init(&dv_ctrl_in, &dv_ctrl_out);
	if (ret < 0) {
		PQ_MSG_ERROR("mtk_pq_dv_ctrl_init return %d\n", ret);
		goto exit;
	}

	/* initialize Dolby window info */
	win_num = pqdev->xc_win_num;
	if (win_num > PQ_WIN_MAX_NUM)
		win_num = PQ_WIN_MAX_NUM;

	for (cnt = 0; cnt < win_num; cnt++) {
		pq_dev = pqdev->pq_devs[cnt];
		if (!pq_dev)
			goto exit;

		memset(&dv_win_in, 0, sizeof(struct mtk_pq_dv_win_init_in));
		memset(&dv_win_out, 0, sizeof(struct mtk_pq_dv_win_init_out));

		dv_win_in.dev = pq_dev;

		ret = mtk_pq_dv_win_init(&dv_win_in, &dv_win_out);
		if (ret < 0) {
			PQ_MSG_ERROR("mtk_pq_dv_win_init return %d\n", ret);
			goto exit;
		}
	}

exit:
	return ret;
}

static int _mtk_pq_cfd_init(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;

	if (!mtk_hashkey_check_IP(IPAUTH_VIDEO_HDR10PLUS)) {
		PQ_MSG_INFO("get hdr10+ hash key not support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support &= ~(BIT(MTK_PQ_CFD_HASH_HDR10PLUS));
	} else {
		PQ_MSG_INFO("get hdr10+ hash key support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support |= BIT(MTK_PQ_CFD_HASH_HDR10PLUS);
	}

	if (!mtk_hashkey_check_IP(IPAUTH_VIDEO_CUVAHDR)) {
		PQ_MSG_INFO("get cuva hash key not support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support &= ~(BIT(MTK_PQ_CFD_HASH_CUVA));
	} else {
		PQ_MSG_INFO("get cuva hash key support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support |= BIT(MTK_PQ_CFD_HASH_CUVA);
	}

	if (!mtk_hashkey_check_IP(IPAUTH_VIDEO_SLHDR)) {
		PQ_MSG_INFO("get TCH hash key not support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support &= ~(BIT(MTK_PQ_CFD_HASH_TCH));
	} else {
		PQ_MSG_INFO("get TCH hash key support!\n");
		pqdev->pqcaps.u8CFD_Hash_Support |= BIT(MTK_PQ_CFD_HASH_TCH);
	}

	ret = mtk_efuse_query(E_EFUSE_IDX_33);
	if (!ret) {
		PQ_MSG_INFO("get TCH efuse key not support!\n");
		pqdev->pqcaps.u8CFD_Efuse_Support = 0;
	} else {
		PQ_MSG_INFO("get TCH efuse key support!\n");
		pqdev->pqcaps.u8CFD_Efuse_Support = 1;
	}

	PQ_MSG_INFO("Efuse(%d), hash(%d)\n",
		pqdev->pqcaps.u8CFD_Efuse_Support, pqdev->pqcaps.u8CFD_Hash_Support);

	return ret;
}

static ssize_t help_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Debug Help:\n"
				"- u32DbgLevel <RW>: To control debug log level.\n"
				"		STI_PQ_LOG_OFF			0x00\n"
				"		STI_PQ_LOG_LEVEL_ERROR		0x01\n"
				"		STI_PQ_LOG_LEVEL_COMMON		0x02\n"
				"		STI_PQ_LOG_LEVEL_ENHANCE	0x04\n"
				"		STI_PQ_LOG_LEVEL_B2R		0x08\n"
				"		STI_PQ_LOG_LEVEL_DISPLAY	0x10\n"
				"		STI_PQ_LOG_LEVEL_XC		0x20\n"
				"		STI_PQ_LOG_LEVEL_3D		0x40\n"
				"		STI_PQ_LOG_LEVEL_BUFFER		0x80\n"
				"		STI_PQ_LOG_LEVEL_SVP		0x100\n"
				"		STI_PQ_LOG_LEVEL_IRQ		0x200\n"
				"		STI_PQ_LOG_LEVEL_PATTERN	0x400\n"
				"		STI_PQ_LOG_LEVEL_DBG		0x800\n"
				"		STI_PQ_LOG_LEVEL_ALL		0xFFF\n");
}

static ssize_t log_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "u32DbgLevel = 0x%x\n", u32DbgLevel);
}

static ssize_t log_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	u32DbgLevel = val;
	return count;
}

static ssize_t log_interval_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "u32LogInterval = %u\n", u32LogInterval);
}

static ssize_t log_interval_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	u32LogInterval = val;
	return count;
}

static ssize_t atrace_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "atrace_enable_pq = 0x%x\n", atrace_enable_pq);
}

static ssize_t atrace_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	atrace_enable_pq = val;
	return count;
}

static ssize_t mtk_pq_capability_support_ts_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Support_TS", pqdev->pqcaps.u32Support_TS);

	return ssize;
}

static ssize_t mtk_pq_capability_input_throughput_single_win_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Input_Throughput_Single_Win",
	pqdev->pqcaps.u32input_throughput_single_win);

	return ssize;
}

static ssize_t mtk_pq_capability_input_throughput_single_win_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_input_throughput_multi_win_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Input_Throughput_Multi_Win",
	pqdev->pqcaps.u32input_throughput_multi_win);

	return ssize;
}

static ssize_t mtk_pq_capability_input_throughput_multi_win_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_output_throughput_single_win_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Output_Throughput_Single_Win",
	pqdev->pqcaps.u32output_throughput_single_win);

	return ssize;
}

static ssize_t mtk_pq_capability_output_throughput_single_win_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_output_throughput_multi_win_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Output_Throughput_Multi_Win",
	pqdev->pqcaps.u32output_throughput_multi_win);

	return ssize;
}

static ssize_t mtk_pq_capability_output_throughput_multi_win_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_output_throughput_win_num_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Output_Throughput_Win_Num", pqdev->pqcaps.u32output_throughput_win_num);

	return ssize;
}

static ssize_t mtk_pq_capability_output_throughput_win_num_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_vsd_coe_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Vsd_Coe", pqdev->pqcaps.u32bw_vsd_coe);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_vsd_coe_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_w_coe_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_W_Coe", pqdev->pqcaps.u32bw_hdmi_w_coe);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_w_coe_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_sd_w_coe_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_Sd_W_Coe", pqdev->pqcaps.u32bw_hdmi_sd_w_coe);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_sd_w_coe_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_mdla_coe_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Mdla_Coe", pqdev->pqcaps.u32bw_mdla_coe);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_mdla_coe_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_inprocess_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Inprocess_Time", pqdev->pqcaps.u32p_inprocess_time);

	return ssize;
}

static ssize_t mtk_pq_capability_p_inprocess_time_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_inprocess_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Inprocess_Time", pqdev->pqcaps.u32i_inprocess_time);

	return ssize;
}

static ssize_t mtk_pq_capability_i_inprocess_time_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_outprocess_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Outprocess_Time", pqdev->pqcaps.u32outprocess_time);

	return ssize;
}

static ssize_t mtk_pq_capability_outprocess_time_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_b2r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_B2r", pqdev->pqcaps.u32bw_b2r);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_b2r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_R", pqdev->pqcaps.u32bw_hdmi_r);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_dvpr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Dvpr", pqdev->pqcaps.u32bw_dvpr);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_dvpr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_scmi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Scmi", pqdev->pqcaps.u32bw_scmi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_scmi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_znr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Znr", pqdev->pqcaps.u32bw_znr);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_znr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_mdw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Mdw", pqdev->pqcaps.u32bw_mdw);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_mdw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_b2r_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_B2r_Effi", pqdev->pqcaps.u32bw_b2r_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_b2r_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_r_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_R_Effi", pqdev->pqcaps.u32bw_hdmi_r_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_r_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_dvpr_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Dvpr_Effi", pqdev->pqcaps.u32bw_dvpr_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_dvpr_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_scmi_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Scmi_Effi", pqdev->pqcaps.u32bw_scmi_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_scmi_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_znr_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Znr_Effi", pqdev->pqcaps.u32bw_znr_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_znr_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_mdw_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Mdw_Effi", pqdev->pqcaps.u32bw_mdw_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_mdw_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_memc_ip_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Memc_Ip_Effi", pqdev->pqcaps.u32bw_memc_ip_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_memc_ip_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_aipq_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Aipq_Effi", pqdev->pqcaps.u32bw_aipq_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_aipq_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_tsin_phase_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Tsin_Phase", pqdev->pqcaps.u32i_tsin_phase);

	return ssize;
}

static ssize_t mtk_pq_capability_i_tsin_phase_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_tsin_clk_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Tsin_Clk_Rate", pqdev->pqcaps.u32i_tsin_clk_rate);

	return ssize;
}

static ssize_t mtk_pq_capability_i_tsin_clk_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_tsin_phase_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Tsin_Phase", pqdev->pqcaps.u32p_tsin_phase);

	return ssize;
}

static ssize_t mtk_pq_capability_p_tsin_phase_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_tsin_clk_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Tsin_Clk_Rate", pqdev->pqcaps.u32p_tsin_clk_rate);

	return ssize;
}

static ssize_t mtk_pq_capability_p_tsin_clk_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_tsout_phase_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Tsout_Phase", pqdev->pqcaps.u32tsout_phase);

	return ssize;
}

static ssize_t mtk_pq_capability_tsout_phase_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_tsout_clk_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Tsout_Clk_Rate", pqdev->pqcaps.u32tsout_clk_rate);

	return ssize;
}

static ssize_t mtk_pq_capability_tsout_clk_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_h_blk_reserve_4k_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("H_Blk_Reserve_4k", pqdev->pqcaps.u32h_blk_reserve_4k);

	return ssize;
}

static ssize_t mtk_pq_capability_h_blk_reserve_4k_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_h_blk_reserve_8k_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("H_Blk_Reserve_8k", pqdev->pqcaps.u32h_blk_reserve_8k);

	return ssize;
}

static ssize_t mtk_pq_capability_h_blk_reserve_8k_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_frame_v_active_time_60_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Frame_V_Active_Time_60", pqdev->pqcaps.u32frame_v_active_time_60);

	return ssize;
}

static ssize_t mtk_pq_capability_frame_v_active_time_60_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_frame_v_active_time_120_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Frame_V_Active_Time_120", pqdev->pqcaps.u32frame_v_active_time_120);

	return ssize;
}

static ssize_t mtk_pq_capability_frame_v_active_time_120_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_chipcap_tsin_outthroughput_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Chipcap_Tsin_Outthroughput",
				pqdev->pqcaps.u32chipcap_tsin_outthroughput);

	return ssize;
}

static ssize_t mtk_pq_capability_chipcap_tsin_outthroughput_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_chipcap_mdw_outthroughput_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Chipcap_Mdw_Outthroughput", pqdev->pqcaps.u32chipcap_mdw_outthroughput);

	return ssize;
}

static ssize_t mtk_pq_capability_chipcap_mdw_outthroughput_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi0", pqdev->pqcaps.u32p_mode_pqlevel_scmi0);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi1", pqdev->pqcaps.u32p_mode_pqlevel_scmi1);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi2", pqdev->pqcaps.u32p_mode_pqlevel_scmi2);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi3", pqdev->pqcaps.u32p_mode_pqlevel_scmi3);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi4", pqdev->pqcaps.u32p_mode_pqlevel_scmi4);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi5_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Scmi5", pqdev->pqcaps.u32p_mode_pqlevel_scmi5);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_scmi5_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr0", pqdev->pqcaps.u32p_mode_pqlevel_znr0);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr1", pqdev->pqcaps.u32p_mode_pqlevel_znr1);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr2", pqdev->pqcaps.u32p_mode_pqlevel_znr2);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr3", pqdev->pqcaps.u32p_mode_pqlevel_znr3);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr4", pqdev->pqcaps.u32p_mode_pqlevel_znr4);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr5_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("P_Mode_Pqlevel_Znr5", pqdev->pqcaps.u32p_mode_pqlevel_znr5);

	return ssize;
}

static ssize_t mtk_pq_capability_p_mode_pqlevel_znr5_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi0", pqdev->pqcaps.u32i_mode_pqlevel_scmi0);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi1", pqdev->pqcaps.u32i_mode_pqlevel_scmi1);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi2", pqdev->pqcaps.u32i_mode_pqlevel_scmi2);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi3", pqdev->pqcaps.u32i_mode_pqlevel_scmi3);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi4", pqdev->pqcaps.u32i_mode_pqlevel_scmi4);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi5_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Scmi5", pqdev->pqcaps.u32i_mode_pqlevel_scmi5);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_scmi5_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr0", pqdev->pqcaps.u32i_mode_pqlevel_znr0);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr1", pqdev->pqcaps.u32i_mode_pqlevel_znr1);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr2", pqdev->pqcaps.u32i_mode_pqlevel_znr2);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr3", pqdev->pqcaps.u32i_mode_pqlevel_znr3);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr4", pqdev->pqcaps.u32i_mode_pqlevel_znr4);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr5_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("I_Mode_Pqlevel_Znr5", pqdev->pqcaps.u32i_mode_pqlevel_znr5);

	return ssize;
}

static ssize_t mtk_pq_capability_i_mode_pqlevel_znr5_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table0", pqdev->pqcaps.u32reserve_ml_table0);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table0_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table1", pqdev->pqcaps.u32reserve_ml_table1);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table2", pqdev->pqcaps.u32reserve_ml_table2);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table3", pqdev->pqcaps.u32reserve_ml_table3);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table4", pqdev->pqcaps.u32reserve_ml_table4);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table5_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table5", pqdev->pqcaps.u32reserve_ml_table5);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table5_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table6_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table6", pqdev->pqcaps.u32reserve_ml_table6);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table6_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table7_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table7", pqdev->pqcaps.u32reserve_ml_table7);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table7_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table8_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table8", pqdev->pqcaps.u32reserve_ml_table8);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table8_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table9_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table9", pqdev->pqcaps.u32reserve_ml_table9);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table9_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table10_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table10", pqdev->pqcaps.u32reserve_ml_table10);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table10_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table11_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table11", pqdev->pqcaps.u32reserve_ml_table11);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table11_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table12_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table12", pqdev->pqcaps.u32reserve_ml_table12);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table12_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table13_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table13", pqdev->pqcaps.u32reserve_ml_table13);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table13_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table14_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table14", pqdev->pqcaps.u32reserve_ml_table14);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table14_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_reserve_ml_table15_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Reserve_Ml_Table15", pqdev->pqcaps.u32reserve_ml_table15);

	return ssize;
}

static ssize_t mtk_pq_capability_reserve_ml_table15_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bwofts_height_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bwofts_Height", pqdev->pqcaps.u32bwofts_height);

	return ssize;
}

static ssize_t mtk_pq_capability_bwofts_height_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bwofts_width_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bwofts_Width", pqdev->pqcaps.u32bwofts_width);

	return ssize;
}

static ssize_t mtk_pq_capability_bwofts_width_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_vsd_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Vsd_Effi", pqdev->pqcaps.u32bw_vsd_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_vsd_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_w_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_W_Effi", pqdev->pqcaps.u32bw_hdmi_w_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_w_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_bw_hdmi_sd_w_effi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Bw_Hdmi_Sd_W_Effi", pqdev->pqcaps.u32bw_hdmi_sd_w_effi);

	return ssize;
}

static ssize_t mtk_pq_capability_bw_hdmi_sd_w_effi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_single_hdmi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Single_Hdmi", pqdev->pqcaps.u32memc_ip_single_hdmi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_single_hdmi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_single_mm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Single_Mm", pqdev->pqcaps.u32memc_ip_single_mm);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_single_mm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_multi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Multi", pqdev->pqcaps.u32memc_ip_multi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_multi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_repeat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Repeat", pqdev->pqcaps.u32memc_ip_repeat);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_repeat_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_pcmode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Pcmode", pqdev->pqcaps.u32memc_ip_pcmode);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_pcmode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_memclevel_single_hdmi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Memclevel_Single_Hdmi",
				pqdev->pqcaps.u32memc_ip_memclevel_single_hdmi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_memclevel_single_hdmi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_memc_ip_memclevel_single_mm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Memclevel_Single_Mm",
				pqdev->pqcaps.u32memc_ip_memclevel_single_mm);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_memclevel_single_mm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_memclevel_multi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Memclevel_Multi", pqdev->pqcaps.u32memc_ip_memclevel_multi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_memclevel_multi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_colorformat_single_hdmi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Colorformat_Single_Hdmi",
				pqdev->pqcaps.u32memc_ip_colorformat_single_hdmi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_colorformat_single_hdmi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_memc_ip_colorformat_single_mm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Colorformat_Single_Mm",
				pqdev->pqcaps.u32memc_ip_colorformat_single_mm);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_colorformat_single_mm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_memc_ip_colorformat_multi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Memc_Ip_Colorformat_Multi", pqdev->pqcaps.u32memc_ip_colorformat_multi);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_ip_colorformat_multi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_forcep_pqlevel_h_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Forcep_Pqlevel_H", pqdev->pqcaps.u32forcep_pqlevel_h);

	return ssize;
}

static ssize_t mtk_pq_capability_forcep_pqlevel_h_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_forcep_pqlevel_l_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Forcep_Pqlevel_L", pqdev->pqcaps.u32forcep_pqlevel_l);

	return ssize;
}

static ssize_t mtk_pq_capability_forcep_pqlevel_l_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static ssize_t mtk_pq_capability_ic_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Ic_Code", pqdev->pqcaps.u32ic_code);

	return ssize;
}

static ssize_t mtk_pq_capability_ic_code_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t mtk_pq_capability_hfr_condition_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("HFR_Check", pqdev->pqcaps.u8HFR_Check);

	return ssize;
}

static ssize_t mtk_pq_capability_winnum_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Window_Num", pqdev->pqcaps.u32Window_Num);

	return ssize;
}

static ssize_t mtk_pq_capability_srs_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("SRS_SUPPORT", pqdev->pqcaps.u32SRS_Support);

	return ssize;
}

static ssize_t mtk_pq_capability_aisr_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("AISR_SUPPORT", pqdev->pqcaps.u32AISR_Support);

	return ssize;
}
//////////////////////////////////////////
static ssize_t mtk_pq_capability_hsy_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("HSY_SUPPORT", pqdev->pqcaps.u32HSY_Support);

	return ssize;
}

static ssize_t mtk_pq_capability_screenseamless_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("SCREEN_SEAMLESS_SUPPORT",
	    pqdev->pqcaps.u32ScreenSeamless_Support);

	return ssize;
}

static ssize_t mtk_pq_capability_hsy_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	count += snprintf(buf + count, u8MaxSize - count,
		"HSY_Version=%d\n", pqdev->pqcaps.u32HSY_Version);

	return count;
}

static ssize_t pq_debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	if (dev == NULL || attr == NULL || buf == NULL) {
		PQ_MSG_ERROR("pq debug show fail\n");
		return -EINVAL;
	}
	return count;
}

static ssize_t pq_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	unsigned long win_idx;
	int idx = 0;

	if (dev == NULL || attr == NULL || buf == NULL) {
		PQ_MSG_ERROR("pq debug store fail\n");
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &win_idx);
	if (err || win_idx > 15)
		return err;
	pr_err("win_idx = %lu\n", win_idx);

	for (idx = 0; idx < 6; idx++) {
		pr_err("Idr_Duplicate info w_index:(%u,%u), field:(%u,%u)\n",
			pqdev->pq_devs[win_idx]->pq_debug.src_dup_idx[idx],
			pqdev->pq_devs[win_idx]->pq_debug.idr_dup_idx[idx],
			pqdev->pq_devs[win_idx]->pq_debug.src_dup_field[idx],
			pqdev->pq_devs[win_idx]->pq_debug.idr_dup_field[idx]);
	}
	//EN_PQ_METATAG_SH_FRM_INFO
	pr_err("%s %s%llx, %s%llx, %s%llx, %s%llx, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u\n",
	"[b2r_info->main]",
	"luma_fb_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.main.luma_fb_offset,
	"chroma_fb_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.main.chroma_fb_offset,
	"luma_blen_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.main.luma_blen_offset,
	"chroma_blen_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.main.chroma_blen_offset,
	"x = ", pqdev->pq_devs[win_idx]->pq_debug.main.x,
	"y = ", pqdev->pq_devs[win_idx]->pq_debug.main.y,
	"width = ", pqdev->pq_devs[win_idx]->pq_debug.main.width,
	"height = ", pqdev->pq_devs[win_idx]->pq_debug.main.height,
	"pitch = ", pqdev->pq_devs[win_idx]->pq_debug.main.pitch,
	"bitdepth = ", pqdev->pq_devs[win_idx]->pq_debug.main.bitdepth);

	pr_err("%s %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"[b2r_info->main]",
	"modifier.tile = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.tile,
	"modifier.raster = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.raster,
	"modifier.compress = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.compress,
	"modifier.jump = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.jump,
	"modifier.vsd_mode = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.vsd_mode,
	"modifier.vsd_ce_mode = ", pqdev->pq_devs[win_idx]->pq_debug.main.modifier.vsd_ce_mode);

	pr_err("%s %s%u, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"[b2r_info->main]",
	"aid = ", pqdev->pq_devs[win_idx]->pq_debug.main.aid,
	"order_type = ", pqdev->pq_devs[win_idx]->pq_debug.main.order_type,
	"scan_type = ", pqdev->pq_devs[win_idx]->pq_debug.main.scan_type,
	"field_type = ", pqdev->pq_devs[win_idx]->pq_debug.main.field_type,
	"rotate_type = ", pqdev->pq_devs[win_idx]->pq_debug.main.rotate_type,
	"color_fmt_422 = ", pqdev->pq_devs[win_idx]->pq_debug.main.color_fmt_422);

	pr_err("%s %s%llx, %s%llx, %s%llx, %s%llx, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"[b2r_info->sub1]",
	"luma_fb_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.sub1.luma_fb_offset,
	"chroma_fb_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.sub1.chroma_fb_offset,
	"luma_blen_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.sub1.luma_blen_offset,
	"chroma_blen_offset = 0x", pqdev->pq_devs[win_idx]->pq_debug.sub1.chroma_blen_offset,
	"x = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.x,
	"y = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.y,
	"width = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.width,
	"height = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.height,
	"pitch = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.pitch,
	"bitdepth = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.bitdepth);

	pr_err("%s %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"[b2r_info->sub1]",
	"modifier.tile = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.tile,
	"modifier.raster = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.raster,
	"modifier.compress = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.compress,
	"modifier.jump = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.jump,
	"modifier.vsd_mode = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.vsd_mode,
	"modifier.vsd_ce_mode = ", pqdev->pq_devs[win_idx]->pq_debug.sub1.modifier.vsd_ce_mode);
	//EN_PQ_METATAG_DISPLAY_FLOW_INFO Input
	pr_err("m_pq_display_flow_ctrl : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"content = ", pqdev->pq_devs[win_idx]->pq_debug.content_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.content_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.content_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.content_in.height,
	"cap_win = ", pqdev->pq_devs[win_idx]->pq_debug.capture_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.capture_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.capture_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.capture_in.height,
	"crop_win = ", pqdev->pq_devs[win_idx]->pq_debug.crop_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.crop_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.crop_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.crop_in.height,
	"disp_win = ", pqdev->pq_devs[win_idx]->pq_debug.display_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.display_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.display_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.display_in.height,
	"displayArea = ", pqdev->pq_devs[win_idx]->pq_debug.displayArea_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_in.height,
	"displayRange = ", pqdev->pq_devs[win_idx]->pq_debug.displayRange_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_in.height,
	"displayBase = ", pqdev->pq_devs[win_idx]->pq_debug.displayBase_in.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_in.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_in.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_in.height);

	pr_err("m_pq_display_get_ip_size : %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d)\n",
	"scmi out = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_scmi_out].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_scmi_out].height,
	"aisr_in = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_aisr_in].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_aisr_in].height,
	"aisr_out = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_aisr_out].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_aisr_out].height,
	"hvsp_in = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_hvsp_in].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_hvsp_in].height,
	"hvsp_out = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_hvsp_out].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_hvsp_out].height,
	"display = ", pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_display].width,
		pqdev->pq_devs[win_idx]->pq_debug.ip_win[pq_debug_ip_display].height);

	pr_err("[FC]Flow control enable:%d factor:%d\n",
		pqdev->pq_devs[win_idx]->pq_debug.flowControlEn,
		pqdev->pq_devs[win_idx]->pq_debug.flowControlFactor);

	pr_err("[WinInfo]pq_win_type:%d multiple_window:%d\n",
		pqdev->pq_devs[win_idx]->pq_debug.pq_win_type,
		pqdev->pq_devs[win_idx]->pq_debug.multiple_window);

	pr_err("[AISR]Active win enable:%u x:%u, y:%u, w:%u, h:%u\n",
		pqdev->pq_devs[win_idx]->pq_debug.bAisrActiveWinEn,
		pqdev->pq_devs[win_idx]->pq_debug.aisr_active_win_info.x,
		pqdev->pq_devs[win_idx]->pq_debug.aisr_active_win_info.y,
		pqdev->pq_devs[win_idx]->pq_debug.aisr_active_win_info.width,
		pqdev->pq_devs[win_idx]->pq_debug.aisr_active_win_info.height);
	//EN_PQ_METATAG_DISPLAY_FLOW_INFO Output
	pr_err("meta_pq_display_flow_info (dqbuf): %s%d, %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"win_id = ", pqdev->pq_devs[win_idx]->pq_debug.win_id_debug,
	"content = ", pqdev->pq_devs[win_idx]->pq_debug.content_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.content_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.content_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.content_out.height,
	"capture = ", pqdev->pq_devs[win_idx]->pq_debug.capture_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.capture_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.capture_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.capture_out.height,
	"crop = ", pqdev->pq_devs[win_idx]->pq_debug.crop_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.crop_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.crop_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.crop_out.height,
	"display = ", pqdev->pq_devs[win_idx]->pq_debug.display_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.display_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.display_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.display_out.height,
	"displayArea = ", pqdev->pq_devs[win_idx]->pq_debug.displayArea_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayArea_out.height,
	"displayRange = ", pqdev->pq_devs[win_idx]->pq_debug.displayRange_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayRange_out.height,
	"displayBase = ", pqdev->pq_devs[win_idx]->pq_debug.displayBase_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.displayBase_out.height);

	pr_err("meta_pq_display_flow_info (dqbuf): %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"outcrop = ", pqdev->pq_devs[win_idx]->pq_debug.outcrop_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.outcrop_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.outcrop_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.outcrop_out.height,
	"outdisplay = ", pqdev->pq_devs[win_idx]->pq_debug.outdisplay_out.x,
		pqdev->pq_devs[win_idx]->pq_debug.outdisplay_out.y,
		pqdev->pq_devs[win_idx]->pq_debug.outdisplay_out.width,
		pqdev->pq_devs[win_idx]->pq_debug.outdisplay_out.height);
	//EN_PQ_METATAG_INPUT_QUEUE_EXT_INFO
	pr_err("m_pq_queue_ext_info : %s%llu, %s%llu, %s%llu, %s%llu, %s%llu, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%d, %s%u, %s%u\n",
	"u64Pts = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u64Pts,
	"u64UniIdx = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u64UniIdx,
	"u64ExtUniIdx = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u64ExtUniIdx,
	"u64TimeStamp = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u64TimeStamp,
	"u64RenderTimeNs = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u64RenderTimeNs,
	"u8BufferValid = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8BufferValid,
	"u32GenerationIndex = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32GenerationIndex,
	"u32StreamUniqueId = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32StreamUniqueId,
	"u8RepeatStatus = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8RepeatStatus,
	"u8FrcMode = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8FrcMode,
	"u8Interlace = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8Interlace,
	"u32InputFps = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32InputFps,
	"u32OriginalInputFps = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32OriginalInputFps,
	"bEOS = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.bEOS,
	"u8MuteAction = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8MuteAction,
	"u8PqMuteAction = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8PqMuteAction,
	"u8SignalStable = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8SignalStable,
	"u8DotByDotType = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u8DotByDotType,
	"u32RefreshRate = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32RefreshRate,
	"bReportFrameStamp = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.bReportFrameStamp,
	"bBypassAvsync = ", pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.bBypassAvsync,
	"u32HdrApplyType = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32HdrApplyType,
	"u32QueueOutputIndex = ",
		pqdev->pq_devs[win_idx]->pq_debug.input_queue_ext_info.u32QueueInputIndex);
	//EN_PQ_METATAG_OUTPUT_QUEUE_EXT_INFO
	pr_err("meta_pq_output_queue_ext_info : %s%llu, %s%llu, %s%llu, %s%llu, %s%llu, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%d, %s%u, %s%u, %s%u\n",
	"u64Pts = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u64Pts,
	"u64UniIdx = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u64UniIdx,
	"u64ExtUniIdx = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u64ExtUniIdx,
	"u64TimeStamp = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u64TimeStamp,
	"u64RenderTimeNs = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u64RenderTimeNs,
	"u8BufferValid = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8BufferValid,
	"u32GenerationIndex = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32GenerationIndex,
	"u32StreamUniqueId = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32StreamUniqueId,
	"u8RepeatStatus = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8RepeatStatus,
	"u8FrcMode = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8FrcMode,
	"u8Interlace = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8Interlace,
	"u32InputFps = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32InputFps,
	"u32OriginInputFps = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32OriginalInputFps,
	"bEOS = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.bEOS,
	"u8MuteAction = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8MuteAction,
	"u8PqMuteAction = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8PqMuteAction,
	"u8SignalStable = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8SignalStable,
	"u8DotByDotType = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u8DotByDotType,
	"u32RefreshRate = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32RefreshRate,
	"bReportFrameStamp = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.bReportFrameStamp,
	"bBypassAvsync = ", pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.bBypassAvsync,
	"u32HdrApplyType = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32HdrApplyType,
	"u32QueOutputIndex = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32QueueOutputIndex,
	"u32QueInputIndex = ",
		pqdev->pq_devs[win_idx]->pq_debug.output_dqueue_ext_info.u32QueueInputIndex);
	//EN_PQ_METATAG_OUTPUT_DISP_IDR_CTRL
	pr_err("meta_disp_idr_crtl : %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"mem_fmt = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.mem_fmt,
	"width = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.width,
	"height = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.height,
	"index = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.index,
	"crop.left = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.crop.left,
	"crop.top = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.crop.top,
	"crop.width = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.crop.width,
	"crop.height = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.crop.height,
	"path = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.path,
	"bypass_ipdma = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.bypass_ipdma,
	"v_flip = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.v_flip,
	"aid = ", pqdev->pq_devs[win_idx]->pq_debug.display_idr_ctrl.aid);
	//PQ_DROP_PTS
	pr_err("pts_idx:%d\n", pqdev->pq_devs[win_idx]->pq_debug.pts_idx);
	pr_err("drop_pts_idx:%d\n", pqdev->pq_devs[win_idx]->pq_debug.drop_pts_idx);
	for (idx = 0; idx < DROP_ARY_IDX; idx++) {
		pr_err("pq_queue_pts:(%u) drop_pts:(%u)\n",
			pqdev->pq_devs[win_idx]->pq_debug.queue_pts[idx],
			pqdev->pq_devs[win_idx]->pq_debug.drop_frame_pts[idx]);
	}
	pr_err("total_que:%d\n", pqdev->pq_devs[win_idx]->pq_debug.total_que);
	pr_err("total_dque:%d\n", pqdev->pq_devs[win_idx]->pq_debug.total_dque);
	//HWMAP_SWBIT
	pr_err("swbit : %s(%u,%u,%u), %s%u, %s%u, %s%u, %s%u\n",
	"scmi_ch_bit = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_ch0_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_ch1_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_ch2_bit,
	"scmi_comp_bit = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_comp_bit,
	"scmi_format = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_format,
	"scmi_frame = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_frame,
	"scmi_delay = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.scmi_delay);
	pr_err("swbit : %s(%u,%u,%u), %s%u, %s%u, %s%u, %s%u\n",
	"ucm_ch_bit = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_ch0_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_ch1_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_ch2_bit,
	"ucm_format = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_format,
	"ucm_frame = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_frame,
	"ucm_delay = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_delay,
	"ucm_diff = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.ucm_diff);
	pr_err("swbit : %s(%u,%u), %s%u, %s%u, %s%u\n",
	"znr_ch_bit = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_ch0_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_ch1_bit,
	"znr_aul_en = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_aul_en,
	"znr_i_mode = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_i_mode,
	"znr_frame = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_frame);
	pr_err("swbit : %s%u, %s%u, %s(%u,%u,%u), %s%u, %s%u, %s%u, %s%u"
		", %s%u, %s%u, %s%u, %s%u, %s%u\n",
	"spf_aul_en = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.spf_aul_en,
	"abf_frame = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.abf_frame,
	"abf_ch_bit = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.abf_ch0_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.abf_ch1_bit,
	pqdev->pq_devs[win_idx]->pq_debug.swbit.abf_ch2_bit,
	"mcdi_frame = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.mcdi_frame,
	"dnr_func_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.dnr_func_on,
	"dnr_motion_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.dnr_motion_on,
	"znr_func_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.znr_func_on,
	"pnr_motion_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.pnr_motion_on,
	"di_func_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.di_func_on,
	"di_mode = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.di_mode,
	"spf_func_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.spf_func_on,
	"abf_func_on = ", pqdev->pq_devs[win_idx]->pq_debug.swbit.abf_func_on);
	pr_err("%s%d\n",
	"extra_frame", pqdev->pq_devs[win_idx]->pq_debug.extra_frame);

	return count;
}

static ssize_t mtk_pq_capability_ylite_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("YLite_Version", pqdev->pqcaps.u32YLite_Version);

	return ssize;
}

static ssize_t mtk_pq_vtap_spf_size_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("Spf_Vtap_Size", pqdev->display_dev.spf_vtap_size);

	return ssize;
}

static ssize_t mtk_pq_capability_vip_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("VIP_Version", pqdev->pqcaps.u32VIP_Version);

	return ssize;
}

static ssize_t mtk_pq_capability_qmap_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	count += snprintf(buf + count, u8MaxSize - count,
		"Qmap_Version=%d\n", pqdev->pqcaps.u32Qmap_Version);

	return count;
}

static ssize_t mtk_pq_capability_dipmap_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	count += snprintf(buf + count, u8MaxSize - count,
		"Dipmap_Version=%d\n", pqdev->pqcaps.u32Dipmap_Version);

	return count;
}

static ssize_t mtk_pq_capability_fully_pq_count_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);

	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PQ_CAPABILITY_SHOW("Fully_PQ_Cnt", pqdev->pqcaps.u32Fully_PQ_Cnt);
	return ssize;
}

static ssize_t pattern_ip2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_IP2;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_ip2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_IP2;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_nr_dnr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_DNR;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_nr_dnr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_DNR;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_nr_ipmr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_IPMR;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_nr_ipmr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_IPMR;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}
static ssize_t pattern_nr_opm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_OPM;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_nr_opm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_OPM;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}
static ssize_t pattern_nr_di_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_DI;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_nr_di_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_NR_DI;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}
static ssize_t pattern_di_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_DI;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_di_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_DI;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}
static ssize_t pattern_srs_in_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SRS_IN;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_srs_in_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SRS_IN;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_srs_out_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SRS_OUT;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_srs_out_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SRS_OUT;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_vop_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VOP;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_vop_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VOP;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_ip2_post_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_IP2_POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_ip2_post_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_IP2_POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_scip_dv_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SCIP_DV;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_scip_dv_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SCIP_DV;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_scdv_dv_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SCDV_DV;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_scdv_dv_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_SCDV_DV;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_dma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_DMA;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_dma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_DMA;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_lite1_dma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_LITE1_DMA;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_lite1_dma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_LITE1_DMA;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_pre_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_pre_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_post_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_post_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_vip_in_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VIP_IN;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}
static ssize_t pattern_vip_in_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VIP_IN;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}
static ssize_t pattern_vip_out_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VIP_OUT;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_vip_out_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_VIP_OUT;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_hdr10_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_HDR10;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_hdr10_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_HDR10;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_mdw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	return count;
}

static ssize_t pattern_mdw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_MDW;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store_only_enable(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_pre1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_pre1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_pre2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_pre2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_PRE_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_post1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_post1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST_1;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_b2r_post2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST_2;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_b2r_post2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_B2R_POST_2;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t pattern_dw_hw5post_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_DW_HW5POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = mtk_pq_pattern_show(buf, dev, position);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t pattern_dw_hw5post_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum pq_pattern_position position = PQ_PATTERN_POSITION_DW_HW5POST;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_pattern_store(buf, dev, position);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_store failed!\n");
		return ret;
	}

	return count;
}

static ssize_t dv_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_dv_show(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_dv_show return %d\n", ret);
		return ret;
	}

	return mtk_pq_dv_attr_show(dev, buf);
}

static ssize_t dv_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_dv_store(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_dv_store return %d\n", ret);
		return ret;
	}

	return count;
}

static ssize_t mtk_pq_capability_h_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("H_MAX_SIZE", pqdev->display_dev.h_max_size);

	return ssize;
}

static ssize_t mtk_pq_capability_v_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("V_MAX_SIZE", pqdev->display_dev.v_max_size);

	return ssize;
}

static ssize_t mtk_pq_capability_input_width_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("PQ_INPUT_WIDTH_MAX_SIZE", pqdev->display_dev.pq_cap_width_max_size);

	return ssize;
}

static ssize_t mtk_pq_capability_input_height_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("PQ_INPUT_HEIGHT_MAX_SIZE", pqdev->display_dev.pq_cap_height_max_size);

	return ssize;
}


static ssize_t mtk_pq_capability_rotate_support_h_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_SUPPORT_H", pqdev->b2r_dev.rotate_support_h);

	return ssize;
}

static ssize_t mtk_pq_capability_rotate_support_v_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_SUPPORT_V", pqdev->b2r_dev.rotate_support_v);

	return ssize;
}

static ssize_t mtk_pq_capability_b2r_max_h_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("B2R_MAX_H", pqdev->b2r_dev.h_max_size);

	return ssize;
}

static ssize_t mtk_pq_capability_b2r_max_v_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("B2R_MAX_V", pqdev->b2r_dev.v_max_size);

	return ssize;
}

static ssize_t mtk_pq_capability_rotate_support_interlace_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_SUPPORT_INTERLACE", pqdev->b2r_dev.rotate_interlace);

	return ssize;
}

static ssize_t mtk_pq_capability_rotate_support_vsd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_SUPPORT_VSD", pqdev->b2r_dev.rotate_support_vsd);

	return ssize;
}

static ssize_t mtk_pq_capability_rotate_vsd_support_h_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_VSD_SUPPORT_H", pqdev->b2r_dev.rotate_vsd_support_h);

	return ssize;
}

static ssize_t mtk_pq_capability_rotate_vsd_support_v_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("ROTATE_VSD_SUPPORT_V", pqdev->b2r_dev.rotate_vsd_support_v);

	return ssize;
}

static ssize_t mtk_pq_capability_memc_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MEMC_SHOW_SIZE;

	PQ_CAPABILITY_SHOW("MEMC", pqdev->pqcaps.u32Memc);

	return ssize;
}

static ssize_t mtk_pq_capability_input_queue_min_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("PQ_Input_Queue_Min", pqdev->pqcaps.u8PQ_InQueue_Min);

	return ssize;
}

static ssize_t mtk_pq_capability_output_queue_min_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("PQ_Output_Queue_Min", pqdev->pqcaps.u8PQ_OutQueue_Min);

	return ssize;
}

static ssize_t mtk_pq_capability_cfd_efuse_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("CFD_Efuse_Support", pqdev->pqcaps.u8CFD_Efuse_Support);

	return ssize;
}

static ssize_t mtk_pq_capability_cfd_hash_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("CFD_Hash_Support", pqdev->pqcaps.u8CFD_Hash_Support);

	return ssize;
}

static ssize_t mtk_pq_capability_fmm_hash_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("FMM_Hash_Support", pqdev->pqcaps.u8FMM_Hash_Support);

	return ssize;
}

static ssize_t mtk_pq_dev_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int i, checker, ssize, splitLen;
	char tmp[MAX_DEV_NAME_LEN] = { 0 };
	char devName[] = "/dev/video";
	char split[] = "\n";

	if ((buf == NULL) || (pqdev->usable_win > MAX_DEV_NAME_LEN))
		return 0;

	splitLen = strlen(split) + 1;
	checker = snprintf(tmp, sizeof(tmp), "%d", pqdev->usable_win);
	if (checker < 0 || checker >= sizeof(tmp))
		return 0;

	strncat(buf, tmp, strlen(tmp));
	strncat(buf, split, splitLen);
	for (i = 0; i < pqdev->usable_win; ++i) {
		checker = snprintf(tmp, sizeof(tmp), "%s%d",
			devName, pqdev->pqcaps.u32Device_register_Num[i]);
		if (checker < 0 || checker >= sizeof(tmp))
			return 0;
		strncat(buf, tmp, strlen(tmp));
		strncat(buf, split, splitLen);
	}
	ssize = strlen(buf) + 1;
	pr_err("[***%s%d***] buf:%s, ssize:%d", __func__, __LINE__, buf, ssize);
	return ssize;
}

static ssize_t mtk_pq_idkdump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = _mtk_pq_idkdump_show(dev, buf);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_pattern_show failed!\n");
		return count;
	}

	return count;
}

static ssize_t mtk_pq_idkdump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_pq_idkdump_store(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pq_idkdump_store failed!!\n");
		return ret;
	}

	return count;
}

static ssize_t mtk_pq_aisr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_aisr_dbg_show(dev, buf);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_aisr_dbg_show failed!!\n");
		return count;
	}

	return count;
}

static ssize_t mtk_pq_aisr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_aisr_dbg_store(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_aisr_dbg_store failed!!\n");
		return ret;
	}

	return count;
}

static ssize_t slice_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	__u8 slice_dbg_mode;

	slice_dbg_mode = mtk_pq_slice_get_dbg_mode();

	return scnprintf(buf, PAGE_SIZE, "dbg_mode = 0x%x\n", slice_dbg_mode);
}

static ssize_t slice_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	mtk_pq_slice_set_dbg_mode(val);

	return count;
}

static ssize_t mtk_pq_pixel_engine_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	PQ_CAPABILITY_SHOW("u32P_ENGINE_IP2 ", pqdev->pqcaps.u32P_ENGINE_IP2);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_DI ", pqdev->pqcaps.u32P_ENGINE_DI);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_AISR ", pqdev->pqcaps.u32P_ENGINE_AISR);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_SCL2 ", pqdev->pqcaps.u32P_ENGINE_SCL2);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_SRS ", pqdev->pqcaps.u32P_ENGINE_SRS);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_VIP ", pqdev->pqcaps.u32P_ENGINE_VIP);
	PQ_CAPABILITY_SHOW("u32P_ENGINE_MDW ", pqdev->pqcaps.u32P_ENGINE_MDW);

	return ssize;
}

static ssize_t attr_mdw_buf_num_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = -1;
	unsigned int win = 0;
	unsigned long val = 0;
	char *ptr = NULL, *tmp = NULL;
	char *pwin = NULL, *pnum = NULL;
	int ptr_size = 0, num = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_device *pq_dev = NULL;

	if (count <= 0)
		return -EINVAL;

	if (dev == NULL || attr == NULL || buf == NULL) {
		PQ_MSG_ERROR("Pointer is NULL!\n");
		return -EINVAL;
	}

	PQ_MALLOC(tmp, count);
	if (tmp == NULL) {
		PQ_MSG_ERROR("Failed to malloc(%zu)!\n", count);
		return -ENOBUFS;
	}

	memcpy(tmp, buf, count);
	ptr = tmp;
	ptr_size = 0;
	while (*ptr != '\n' && ptr_size < count) {
		if (pwin == NULL) {
			pwin = ptr;
		} else {
			if (*ptr == ' ') {
				*ptr = '\0';
			} else {
				if (pnum == NULL)
					pnum = ptr;
			}
		}
		ptr++;
		ptr_size++;
	}

	if (pwin != NULL) {
		ret = kstrtoul(pwin, 0, &val);
		if (ret)
			goto EXIT;
		win = (unsigned int)val;

		if (win >= 0 && win < PQ_WIN_MAX_NUM) {
			pqdev = dev_get_drvdata(dev);
			if (pqdev == NULL)
				goto EXIT;

			pq_dev = pqdev->pq_devs[win];
			if (pq_dev == NULL)
				goto EXIT;

			if (pnum != NULL) {
				ret = kstrtoul(pnum, 0, &val);
				if (ret)
					goto EXIT;
				num = val;

				pq_dev->attr_mdw_buf_num = num;

				PQ_MSG_INFO("win=%u set attr_mdw_buf_num=%d\n", win, num);
			} else {
				pq_dev->attr_mdw_buf_num = -1;
				PQ_MSG_INFO("win=%u unset attr_mdw_buf_num\n", win);
			}
		} else {
			PQ_MSG_ERROR("Invalid win=%u (0~%d)\n", win, PQ_WIN_MAX_NUM-1);
		}
	}

EXIT:
	PQ_FREE(tmp, count);
	return count;
}

static ssize_t debug_calc_num_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = -1;
	unsigned int win = 0;
	unsigned long val = 0;
	char *ptr = NULL, *tmp = NULL;
	char *pwin = NULL, *pnum = NULL;
	int ptr_size = 0, num = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_device *pq_dev = NULL;

	if (count <= 0)
		return -EINVAL;

	if (dev == NULL || attr == NULL || buf == NULL) {
		PQ_MSG_ERROR("Pointer is NULL!\n");
		return -EINVAL;
	}

	PQ_MALLOC(tmp, count);
	if (tmp == NULL) {
		PQ_MSG_ERROR("Failed to malloc(%zu)!\n", count);
		return -ENOBUFS;
	}

	memcpy(tmp, buf, count);
	ptr = tmp;
	ptr_size = 0;
	while (*ptr != '\n' && ptr_size < count) {
		if (pwin == NULL) {
			pwin = ptr;
		} else {
			if (*ptr == ' ') {
				*ptr = '\0';
			} else {
				if (pnum == NULL)
					pnum = ptr;
			}
		}
		ptr++;
		ptr_size++;
	}

	if (pwin != NULL) {
		ret = kstrtoul(pwin, 0, &val);
		if (ret)
			goto EXIT;
		win = (unsigned int)val;

		if (win >= 0 && win < PQ_WIN_MAX_NUM) {
			pqdev = dev_get_drvdata(dev);
			if (pqdev == NULL)
				goto EXIT;

			pq_dev = pqdev->pq_devs[win];
			if (pq_dev == NULL)
				goto EXIT;

			if (pnum != NULL) {
				ret = kstrtoul(pnum, 0, &val);
				if (ret)
					goto EXIT;
				num = val;

				pq_dev->debug_calc_num = num;

				PQ_MSG_INFO("win=%u set debug_calc_num=%d\n", win, num);
			} else {
				pq_dev->debug_calc_num = -1;
				PQ_MSG_INFO("win=%u unset debug_calc_num\n", win);
			}
		} else {
			PQ_MSG_ERROR("Invalid win=%u (0~%d)\n", win, PQ_WIN_MAX_NUM-1);
		}
	}

EXIT:
	PQ_FREE(tmp, count);
	return count;
}

static ssize_t mtk_pq_bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_enhance_bypass_cmd_show(dev, buf);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_enhance_bypass_cmd_show failed!!\n");
		return count;
	}

	return count;
}

static ssize_t mtk_pq_bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_enhance_bypass_cmd_store(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_enhance_bypass_cmd_store failed!!\n");
		return ret;
	}

	return count;
}

static ssize_t mtk_pq_b2r_gtgen_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	struct mtk_pq_device *pq_dev;
	struct b2r_reg_timing reg_timing_info;
	unsigned int win = 0;
	unsigned int enable_win = B2R_WIN_NONE;
	unsigned int potential_win = B2R_WIN_NONE;
	__u32 fps = 0;

	memset(&reg_timing_info, 0, sizeof(struct b2r_reg_timing));

	for (win = 0; win < PQ_WIN_MAX_NUM; win++) {
		if (pqdev->pq_devs[win]) {
			if (pqdev->pq_devs[win]->b2r_info.global_timing.enable == TRUE) {
				if (enable_win != B2R_WIN_NONE) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"enable window not only one !!!\n");
				} else {
					enable_win = win;
				}
			} else {
				/* case: set timing but not enable yet */
				if ((pqdev->pq_devs[win]->b2r_info.timing_in.v_freq)
					&& (potential_win == B2R_WIN_NONE)) {
					potential_win = win;
				} else {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"potential_win window not only one !\n");
				}
			}
		}
	}

	if (enable_win != B2R_WIN_NONE && enable_win < PQ_WIN_MAX_NUM)
		pq_dev = pqdev->pq_devs[enable_win];
	else if (potential_win != B2R_WIN_NONE && potential_win < PQ_WIN_MAX_NUM)
		pq_dev = pqdev->pq_devs[potential_win];
	else
		return snprintf(buf, PAGE_SIZE, "WARN : no b2r gtgen info\n");

	fps = pq_dev->b2r_info.timing_in.v_freq;

	/* get hw setting */
	if (mtk_b2r_get_hw_gtgen(enable_win, &reg_timing_info) < 0)
		return snprintf(buf, PAGE_SIZE, "WARN : get hw value failed, please try again!\n");

	return snprintf(buf, PAGE_SIZE, "b2r log:\n"
		"       - enable = %d\n"
		"       - win_id = %d\n"
		"       - de_width = %d\n"
		"       - de_height = %d\n"
		"       - framerate = %d\n"
		"       - interlace = %d\n"
		"       - hw_vtotal = %d\n"
		"       - hw_htotal = %d\n"
		"       - hw_finetune = %d\n"
		"       - hw framerate = %d\n",
		pq_dev->b2r_info.global_timing.enable,
		pq_dev->dev_indx,
		pq_dev->b2r_info.timing_in.de_width,
		pq_dev->b2r_info.timing_in.de_height,
		fps,
		pq_dev->b2r_info.timing_in.interlance,
		reg_timing_info.V_TotalCount,
		reg_timing_info.H_TotalCount,
		reg_timing_info.Hcnt_FineTune,
		reg_timing_info.vfreq
		);

}

static ssize_t mtk_pq_b2r_frame_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_pq_device *pq_dev = NULL;
	unsigned int win = 0;
	unsigned int enable_win = B2R_WIN_NONE;
	unsigned int potential_win = B2R_WIN_NONE;
	struct b2r_ctrl reg_b2r_ctrl;
	struct b2r_enable b2r_en;

	memset(&reg_b2r_ctrl, 0, sizeof(struct b2r_ctrl));

	for (win = 0; win < PQ_WIN_MAX_NUM; win++) {
		if (pqdev->pq_devs[win]) {
			if (pqdev->pq_devs[win]->b2r_info.global_timing.enable == TRUE) {
				if (enable_win != B2R_WIN_NONE) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"enable window not only one !!!\n");
				} else {
					enable_win = win;
				}
			} else {
				/* case: set timing but not enable yet */
				if ((pqdev->pq_devs[win]->b2r_info.timing_in.v_freq)
					&& (potential_win == B2R_WIN_NONE)) {
					potential_win = win;
				} else {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"potential_win window not only one !\n");
				}
			}
		}
	}

	if (enable_win != B2R_WIN_NONE && enable_win < PQ_WIN_MAX_NUM)
		pq_dev = pqdev->pq_devs[enable_win];
	else if (potential_win != B2R_WIN_NONE && potential_win < PQ_WIN_MAX_NUM)
		pq_dev = pqdev->pq_devs[potential_win];
	else
		return snprintf(buf, PAGE_SIZE, "WARN : no b2r frame info\n");

	mtk_b2r_get_hw_addr(enable_win, &reg_b2r_ctrl);
	b2r_en = mtk_b2r_get_enable(&reg_b2r_ctrl);
	mtk_b2r_set_reg_ctrl(&reg_b2r_ctrl);

	return snprintf(buf, PAGE_SIZE, "b2r log:\n"
		"       - win_id = %d\n"
		"       - hw main luma addr = 0x%x\n"
		"       - hw main chroma addr =0x%x\n"
		"       - hw sub luma addr = 0x%x\n"
		"       - hw sub chroma addr = 0x%x\n"
		"       - b2r main addr update = %d\n"
		"       - b2r sub addr update = %d\n",
		pq_dev->dev_indx,
		reg_b2r_ctrl.main.luma_fb,
		reg_b2r_ctrl.main.chroma_fb,
		reg_b2r_ctrl.sub1.luma_fb,
		reg_b2r_ctrl.sub1.chroma_fb,
		b2r_en.main,
		b2r_en.sub1);
}

static ssize_t mtk_pq_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_enhance_status_cmd_show(dev, buf);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_enhance_bypass_cmd_show failed!!\n");
		return count;
	}

	return count;
}

static ssize_t mtk_pq_pqu_loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_pqu_loglevel_dbg_show(dev, buf);
	if (count < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_pqu_loglevel_dbg_show failed!!, count:%zd\n", count);
	return count;
}

static ssize_t mtk_pq_pqu_loglevel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int iRetCheck = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"attr.name:%s, buf:%s, count:%zd\n", attr->attr.name, buf, count);
	iRetCheck = mtk_pq_pqu_loglevel_dbg_store(dev, buf);
	if (iRetCheck < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_pqu_loglevel_dbg_store failed!!, iRetCheck:%d\n", iRetCheck);
	} else {
		count = iRetCheck;
	}
	return count;
}

static ssize_t mtk_pq_srs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_srs_dbg_show(dev, buf);
	if (count < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_srs_dbg_show failed!!, count:%zd\n", count);
	return count;
}

static ssize_t mtk_pq_srs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int iRetCheck = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"attr.name:%s, buf:%s, count:%zd\n", attr->attr.name, buf, count);
	iRetCheck = mtk_pq_srs_dbg_store(dev, buf);
	if (iRetCheck < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_srs_dbg_store failed!!, iRetCheck:%d\n", iRetCheck);
	} else {
		count = iRetCheck;
	}
	return count;
}

#if IS_ENABLED(CONFIG_OPTEE)
static ssize_t mtk_pq_svp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	if ((dev == NULL) || (buf == NULL) || (attr == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	count = mtk_pq_svp_show_cmd(dev, buf);
	if (count < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_svp_show_cmd failed!!\n");
		return count;
	}

	return count;
}

static ssize_t mtk_pq_svp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL) || (attr == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = mtk_pq_svp_store_cmd(dev, buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_svp_store_cmd failed!!\n");
		return ret;
	}

	return count;
}
#endif

static ssize_t mtk_pq_capability_qmap_heap_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	count += snprintf(buf + count, u8MaxSize - count,
		"u32Qmap_Heap_Support=%d\n", pqdev->pqcaps.u32Qmap_Heap_Support);

	return count;
}

static ssize_t mtk_pq_dip_win_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}
	pqdev = dev_get_drvdata(dev);
	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	count = snprintf(buf, MAX_SYSFS_SIZE,
		"DIP_WIN_NUM=%u\n", pqdev->dip_win_num);

	if (count < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"snprintf failed!!, count:%zd\n", count);

	return count;
}

static ssize_t mtk_pq_xc_win_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}
	pqdev = dev_get_drvdata(dev);
	if (attr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attr is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	count = snprintf(buf, MAX_SYSFS_SIZE,
		"XC_WIN_NUM=%u\n", pqdev->xc_win_num);

	if (count < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"snprintf failed!!, count:%zd\n", count);

	return count;
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(log_level);
static DEVICE_ATTR_RW(log_interval);
static DEVICE_ATTR_RW(atrace_enable);
static DEVICE_ATTR_RW(mtk_pq_capability_input_throughput_single_win);
static DEVICE_ATTR_RW(mtk_pq_capability_input_throughput_multi_win);
static DEVICE_ATTR_RW(mtk_pq_capability_output_throughput_single_win);
static DEVICE_ATTR_RW(mtk_pq_capability_output_throughput_multi_win);
static DEVICE_ATTR_RW(mtk_pq_capability_output_throughput_win_num);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_vsd_coe);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_w_coe);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_sd_w_coe);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_mdla_coe);
static DEVICE_ATTR_RW(mtk_pq_capability_p_inprocess_time);
static DEVICE_ATTR_RW(mtk_pq_capability_i_inprocess_time);
static DEVICE_ATTR_RW(mtk_pq_capability_outprocess_time);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_b2r);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_r);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_dvpr);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_scmi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_znr);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_mdw);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_b2r_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_r_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_dvpr_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_scmi_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_znr_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_mdw_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_memc_ip_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_aipq_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_i_tsin_phase);
static DEVICE_ATTR_RW(mtk_pq_capability_i_tsin_clk_rate);
static DEVICE_ATTR_RW(mtk_pq_capability_p_tsin_phase);
static DEVICE_ATTR_RW(mtk_pq_capability_p_tsin_clk_rate);
static DEVICE_ATTR_RW(mtk_pq_capability_tsout_phase);
static DEVICE_ATTR_RW(mtk_pq_capability_tsout_clk_rate);
static DEVICE_ATTR_RW(mtk_pq_capability_h_blk_reserve_4k);
static DEVICE_ATTR_RW(mtk_pq_capability_h_blk_reserve_8k);
static DEVICE_ATTR_RW(mtk_pq_capability_frame_v_active_time_60);
static DEVICE_ATTR_RW(mtk_pq_capability_frame_v_active_time_120);
static DEVICE_ATTR_RW(mtk_pq_capability_chipcap_tsin_outthroughput);
static DEVICE_ATTR_RW(mtk_pq_capability_chipcap_mdw_outthroughput);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi0);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi1);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi2);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi3);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi4);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_scmi5);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr0);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr1);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr2);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr3);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr4);
static DEVICE_ATTR_RW(mtk_pq_capability_p_mode_pqlevel_znr5);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi0);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi1);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi2);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi3);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi4);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_scmi5);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr0);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr1);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr2);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr3);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr4);
static DEVICE_ATTR_RW(mtk_pq_capability_i_mode_pqlevel_znr5);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table0);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table1);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table2);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table3);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table4);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table5);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table6);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table7);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table8);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table9);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table10);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table11);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table12);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table13);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table14);
static DEVICE_ATTR_RW(mtk_pq_capability_reserve_ml_table15);
static DEVICE_ATTR_RW(mtk_pq_capability_bwofts_height);
static DEVICE_ATTR_RW(mtk_pq_capability_bwofts_width);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_vsd_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_w_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_bw_hdmi_sd_w_effi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_single_hdmi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_single_mm);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_multi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_repeat);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_pcmode);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_memclevel_single_hdmi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_memclevel_single_mm);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_memclevel_multi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_colorformat_single_hdmi);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_colorformat_single_mm);
static DEVICE_ATTR_RW(mtk_pq_capability_memc_ip_colorformat_multi);
static DEVICE_ATTR_RW(mtk_pq_capability_forcep_pqlevel_h);
static DEVICE_ATTR_RW(mtk_pq_capability_forcep_pqlevel_l);
static DEVICE_ATTR_RW(mtk_pq_capability_ic_code);
static DEVICE_ATTR_RO(mtk_pq_capability_hfr_condition);

static DEVICE_ATTR_RW(pattern_ip2);
static DEVICE_ATTR_RW(pattern_nr_dnr);
static DEVICE_ATTR_RW(pattern_nr_ipmr);
static DEVICE_ATTR_RW(pattern_nr_opm);
static DEVICE_ATTR_RW(pattern_nr_di);
static DEVICE_ATTR_RW(pattern_di);
static DEVICE_ATTR_RW(pattern_vip_in);
static DEVICE_ATTR_RW(pattern_vip_out);
static DEVICE_ATTR_RW(pattern_hdr10);
static DEVICE_ATTR_RW(pattern_srs_in);
static DEVICE_ATTR_RW(pattern_srs_out);
static DEVICE_ATTR_RW(pattern_vop);
static DEVICE_ATTR_RW(pq_debug);
static DEVICE_ATTR_RW(pattern_ip2_post);
static DEVICE_ATTR_RW(pattern_scip_dv);
static DEVICE_ATTR_RW(pattern_scdv_dv);
static DEVICE_ATTR_RW(pattern_b2r_dma);
static DEVICE_ATTR_RW(pattern_b2r_lite1_dma);
static DEVICE_ATTR_RW(pattern_b2r_pre);
static DEVICE_ATTR_RW(pattern_b2r_post);
static DEVICE_ATTR_RW(pattern_b2r_pre1);
static DEVICE_ATTR_RW(pattern_b2r_post1);
static DEVICE_ATTR_RW(pattern_b2r_pre2);
static DEVICE_ATTR_RW(pattern_b2r_post2);
static DEVICE_ATTR_RW(pattern_mdw);
static DEVICE_ATTR_RW(pattern_dw_hw5post);

static DEVICE_ATTR_RW(dv);
static DEVICE_ATTR_RO(mtk_pq_dev);
static DEVICE_ATTR_RW(mtk_pq_idkdump);
static DEVICE_ATTR_RW(mtk_pq_aisr);
static DEVICE_ATTR_RW(slice_mode);
static DEVICE_ATTR_RO(mtk_pq_pixel_engine);
static DEVICE_ATTR_RO(mtk_pq_vtap_spf_size);

static DEVICE_ATTR_RO(mtk_pq_capability_support_ts);
static DEVICE_ATTR_RO(mtk_pq_capability_winnum);
static DEVICE_ATTR_RO(mtk_pq_capability_memc);
static DEVICE_ATTR_RO(mtk_pq_capability_srs_support);
static DEVICE_ATTR_RO(mtk_pq_capability_h_max);
static DEVICE_ATTR_RO(mtk_pq_capability_v_max);
static DEVICE_ATTR_RO(mtk_pq_capability_b2r_max_h);
static DEVICE_ATTR_RO(mtk_pq_capability_b2r_max_v);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_support_h);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_support_v);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_support_interlace);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_support_vsd);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_vsd_support_h);
static DEVICE_ATTR_RO(mtk_pq_capability_rotate_vsd_support_v);
static DEVICE_ATTR_RO(mtk_pq_capability_input_width_max);
static DEVICE_ATTR_RO(mtk_pq_capability_input_height_max);

static DEVICE_ATTR_RO(mtk_pq_capability_hsy_support);
static DEVICE_ATTR_RO(mtk_pq_capability_aisr_support);
static DEVICE_ATTR_RO(mtk_pq_capability_hsy_version);
static DEVICE_ATTR_RO(mtk_pq_capability_qmap_version);
static DEVICE_ATTR_RO(mtk_pq_capability_screenseamless_support);
static DEVICE_ATTR_RO(mtk_pq_capability_input_queue_min);
static DEVICE_ATTR_RO(mtk_pq_capability_output_queue_min);
static DEVICE_ATTR_RO(mtk_pq_capability_ylite_version);
static DEVICE_ATTR_RO(mtk_pq_capability_vip_version);
static DEVICE_ATTR_RO(mtk_pq_capability_dipmap_version);
static DEVICE_ATTR_RO(mtk_pq_capability_cfd_efuse);
static DEVICE_ATTR_RO(mtk_pq_capability_cfd_hash);
static DEVICE_ATTR_RO(mtk_pq_capability_fmm_hash);
static DEVICE_ATTR_RO(mtk_pq_capability_fully_pq_count);
static DEVICE_ATTR_WO(attr_mdw_buf_num);
static DEVICE_ATTR_WO(debug_calc_num);
static DEVICE_ATTR_RW(mtk_pq_bypass);
static DEVICE_ATTR_RO(mtk_pq_b2r_gtgen_info);
static DEVICE_ATTR_RO(mtk_pq_b2r_frame_info);
static DEVICE_ATTR_RO(mtk_pq_status);
static DEVICE_ATTR_RW(mtk_pq_pqu_loglevel);
#if IS_ENABLED(CONFIG_OPTEE)
static DEVICE_ATTR_RW(mtk_pq_svp);
#endif
static DEVICE_ATTR_RW(mtk_pq_srs);
static DEVICE_ATTR_RO(mtk_pq_capability_qmap_heap);
static DEVICE_ATTR_RO(mtk_pq_dip_win);
static DEVICE_ATTR_RO(mtk_pq_xc_win);

static struct attribute *mtk_pq_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_log_level.attr,
	&dev_attr_log_interval.attr,
	&dev_attr_atrace_enable.attr,

	&dev_attr_mtk_pq_capability_input_throughput_single_win.attr,
	&dev_attr_mtk_pq_capability_input_throughput_multi_win.attr,
	&dev_attr_mtk_pq_capability_output_throughput_single_win.attr,
	&dev_attr_mtk_pq_capability_output_throughput_multi_win.attr,
	&dev_attr_mtk_pq_capability_output_throughput_win_num.attr,
	&dev_attr_mtk_pq_capability_bw_vsd_coe.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_w_coe.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_sd_w_coe.attr,
	&dev_attr_mtk_pq_capability_bw_mdla_coe.attr,
	&dev_attr_mtk_pq_capability_p_inprocess_time.attr,
	&dev_attr_mtk_pq_capability_i_inprocess_time.attr,
	&dev_attr_mtk_pq_capability_outprocess_time.attr,
	&dev_attr_mtk_pq_capability_bw_b2r.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_r.attr,
	&dev_attr_mtk_pq_capability_bw_dvpr.attr,
	&dev_attr_mtk_pq_capability_bw_scmi.attr,
	&dev_attr_mtk_pq_capability_bw_znr.attr,
	&dev_attr_mtk_pq_capability_bw_mdw.attr,
	&dev_attr_mtk_pq_capability_bw_b2r_effi.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_r_effi.attr,
	&dev_attr_mtk_pq_capability_bw_dvpr_effi.attr,
	&dev_attr_mtk_pq_capability_bw_scmi_effi.attr,
	&dev_attr_mtk_pq_capability_bw_znr_effi.attr,
	&dev_attr_mtk_pq_capability_bw_mdw_effi.attr,
	&dev_attr_mtk_pq_capability_bw_memc_ip_effi.attr,
	&dev_attr_mtk_pq_capability_bw_aipq_effi.attr,
	&dev_attr_mtk_pq_capability_i_tsin_phase.attr,
	&dev_attr_mtk_pq_capability_i_tsin_clk_rate.attr,
	&dev_attr_mtk_pq_capability_p_tsin_phase.attr,
	&dev_attr_mtk_pq_capability_p_tsin_clk_rate.attr,
	&dev_attr_mtk_pq_capability_tsout_phase.attr,
	&dev_attr_mtk_pq_capability_tsout_clk_rate.attr,
	&dev_attr_mtk_pq_capability_h_blk_reserve_4k.attr,
	&dev_attr_mtk_pq_capability_h_blk_reserve_8k.attr,
	&dev_attr_mtk_pq_capability_frame_v_active_time_60.attr,
	&dev_attr_mtk_pq_capability_frame_v_active_time_120.attr,
	&dev_attr_mtk_pq_capability_chipcap_tsin_outthroughput.attr,
	&dev_attr_mtk_pq_capability_chipcap_mdw_outthroughput.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi0.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi1.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi2.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi3.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi4.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_scmi5.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr0.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr1.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr2.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr3.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr4.attr,
	&dev_attr_mtk_pq_capability_p_mode_pqlevel_znr5.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi0.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi1.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi2.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi3.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi4.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_scmi5.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr0.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr1.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr2.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr3.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr4.attr,
	&dev_attr_mtk_pq_capability_i_mode_pqlevel_znr5.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table0.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table1.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table2.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table3.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table4.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table5.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table6.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table7.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table8.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table9.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table10.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table11.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table12.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table13.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table14.attr,
	&dev_attr_mtk_pq_capability_reserve_ml_table15.attr,
	&dev_attr_mtk_pq_capability_bwofts_height.attr,
	&dev_attr_mtk_pq_capability_bwofts_width.attr,
	&dev_attr_mtk_pq_capability_bw_vsd_effi.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_w_effi.attr,
	&dev_attr_mtk_pq_capability_bw_hdmi_sd_w_effi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_single_hdmi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_single_mm.attr,
	&dev_attr_mtk_pq_capability_memc_ip_multi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_repeat.attr,
	&dev_attr_mtk_pq_capability_memc_ip_pcmode.attr,
	&dev_attr_mtk_pq_capability_memc_ip_memclevel_single_hdmi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_memclevel_single_mm.attr,
	&dev_attr_mtk_pq_capability_memc_ip_memclevel_multi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_colorformat_single_hdmi.attr,
	&dev_attr_mtk_pq_capability_memc_ip_colorformat_single_mm.attr,
	&dev_attr_mtk_pq_capability_memc_ip_colorformat_multi.attr,
	&dev_attr_mtk_pq_capability_forcep_pqlevel_h.attr,
	&dev_attr_mtk_pq_capability_forcep_pqlevel_l.attr,
	&dev_attr_mtk_pq_capability_ic_code.attr,
	&dev_attr_mtk_pq_capability_hfr_condition.attr,

	&dev_attr_pattern_ip2.attr,
	&dev_attr_pattern_nr_dnr.attr,
	&dev_attr_pattern_nr_ipmr.attr,
	&dev_attr_pattern_nr_opm.attr,
	&dev_attr_pattern_nr_di.attr,
	&dev_attr_pattern_di.attr,
	&dev_attr_pattern_srs_in.attr,
	&dev_attr_pattern_srs_out.attr,
	&dev_attr_pattern_vop.attr,
	&dev_attr_pq_debug.attr,
	&dev_attr_pattern_ip2_post.attr,
	&dev_attr_pattern_scip_dv.attr,
	&dev_attr_pattern_scdv_dv.attr,
	&dev_attr_pattern_b2r_dma.attr,
	&dev_attr_pattern_b2r_lite1_dma.attr,
	&dev_attr_pattern_b2r_pre.attr,
	&dev_attr_pattern_b2r_post.attr,
	&dev_attr_pattern_vip_in.attr,
	&dev_attr_pattern_vip_out.attr,
	&dev_attr_pattern_hdr10.attr,
	&dev_attr_pattern_b2r_pre1.attr,
	&dev_attr_pattern_b2r_post1.attr,
	&dev_attr_pattern_b2r_pre2.attr,
	&dev_attr_pattern_b2r_post2.attr,
	&dev_attr_pattern_mdw.attr,
	&dev_attr_pattern_dw_hw5post.attr,

	&dev_attr_dv.attr,
	&dev_attr_mtk_pq_dev.attr,
	&dev_attr_mtk_pq_idkdump.attr,
	&dev_attr_mtk_pq_aisr.attr,
	&dev_attr_slice_mode.attr,
	&dev_attr_mtk_pq_pixel_engine.attr,
	&dev_attr_mtk_pq_vtap_spf_size.attr,

	&dev_attr_mtk_pq_capability_support_ts.attr,
	&dev_attr_mtk_pq_capability_winnum.attr,
	&dev_attr_mtk_pq_capability_memc.attr,
	&dev_attr_mtk_pq_capability_srs_support.attr,
	&dev_attr_mtk_pq_capability_h_max.attr,
	&dev_attr_mtk_pq_capability_v_max.attr,
	&dev_attr_mtk_pq_capability_b2r_max_h.attr,
	&dev_attr_mtk_pq_capability_b2r_max_v.attr,
	&dev_attr_mtk_pq_capability_rotate_support_h.attr,
	&dev_attr_mtk_pq_capability_rotate_support_v.attr,
	&dev_attr_mtk_pq_capability_rotate_support_interlace.attr,
	&dev_attr_mtk_pq_capability_rotate_support_vsd.attr,
	&dev_attr_mtk_pq_capability_rotate_vsd_support_h.attr,
	&dev_attr_mtk_pq_capability_rotate_vsd_support_v.attr,
	&dev_attr_mtk_pq_capability_hsy_support.attr,
	&dev_attr_mtk_pq_capability_aisr_support.attr,
	&dev_attr_mtk_pq_capability_hsy_version.attr,
	&dev_attr_mtk_pq_capability_qmap_version.attr,
	&dev_attr_mtk_pq_capability_screenseamless_support.attr,
	&dev_attr_mtk_pq_capability_input_queue_min.attr,
	&dev_attr_mtk_pq_capability_output_queue_min.attr,
	&dev_attr_mtk_pq_capability_ylite_version.attr,
	&dev_attr_mtk_pq_capability_vip_version.attr,
	&dev_attr_mtk_pq_capability_dipmap_version.attr,
	&dev_attr_mtk_pq_capability_cfd_efuse.attr,
	&dev_attr_mtk_pq_capability_cfd_hash.attr,
	&dev_attr_mtk_pq_capability_fmm_hash.attr,
	&dev_attr_mtk_pq_capability_fully_pq_count.attr,
	&dev_attr_attr_mdw_buf_num.attr,
	&dev_attr_debug_calc_num.attr,
	&dev_attr_mtk_pq_bypass.attr,
	&dev_attr_mtk_pq_b2r_gtgen_info.attr,
	&dev_attr_mtk_pq_b2r_frame_info.attr,
	&dev_attr_mtk_pq_status.attr,
	&dev_attr_mtk_pq_pqu_loglevel.attr,
#if IS_ENABLED(CONFIG_OPTEE)
	&dev_attr_mtk_pq_svp.attr,
#endif
	&dev_attr_mtk_pq_srs.attr,
	&dev_attr_mtk_pq_capability_qmap_heap.attr,
	&dev_attr_mtk_pq_dip_win.attr,
	&dev_attr_mtk_pq_xc_win.attr,
	&dev_attr_mtk_pq_capability_input_width_max.attr,
	&dev_attr_mtk_pq_capability_input_height_max.attr,
	NULL,
};

static const struct attribute_group mtk_pq_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_pq_attrs
};

static void mtk_pq_CreateSysFS(struct device *pdv)
{
	int ret = 0;
	PQ_MSG_INFO("Device_create_file initialized\n");

	ret = sysfs_create_group(&pdv->kobj, &mtk_pq_attr_group);
	if (ret)
		dev_err(pdv, "[%d]Fail to create sysfs files: %d\n", __LINE__, ret);

	/* CLK */
	mtk_pq_common_clk_create_sysfs(pdv);

	/* Capability*/
	mtk_pq_common_capability_create_sysfs(pdv);
}

static void mtk_pq_RemoveSysFS(struct device *pdv)
{
	dev_info(pdv, "Remove device attribute files");
	sysfs_remove_group(&pdv->kobj, &mtk_pq_attr_group);

	/* CLK */
	mtk_pq_common_clk_remove_sysfs(pdv);

	/* Capability*/
	mtk_pq_common_capability_remove_sysfs(pdv);
}

static int mtk_pq_querycap(struct file *file,
	void *priv, struct v4l2_capability *cap)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	strncpy(cap->driver, PQ_VIDEO_DRIVER_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, pq_dev->video_dev.name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_CAPTURE|
		V4L2_CAP_DEVICE_CAPS;
	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_CAPTURE;
	return 0;
}

static int mtk_pq_s_input(struct file *file, void *fh, unsigned int i)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_set_input(pq_dev, i);
}

static int mtk_pq_g_input(struct file *file, void *fh, unsigned int *i)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_get_input(pq_dev, i);
}

static int mtk_pq_s_fmt_vid_cap_mplane(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_set_fmt_cap_mplane(pq_dev, f);
}

static int mtk_pq_s_fmt_vid_out_mplane(
	struct file *file,
	void *fh,
	struct v4l2_format *f)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_set_fmt_out_mplane(pq_dev, f);
}

static int mtk_pq_g_fmt_vid_cap_mplane(
	struct file *file,
	void *fh,
	struct v4l2_format *f)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_get_fmt_cap_mplane(pq_dev, f);
}

static int mtk_pq_g_fmt_vid_out_mplane(
	struct file *file,
	void *fh,
	struct v4l2_format *f)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	return mtk_pq_common_get_fmt_out_mplane(pq_dev, f);
}

static int mtk_pq_streamon(struct file *file,
	void *fh, enum v4l2_buf_type type)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	struct msg_stream_on_info msg_info;
	struct mtk_pq_dv_streamon_in dv_in;
	struct mtk_pq_dv_streamon_out dv_out;
	u32 source = pq_dev->common_info.input_source;
	int ret = 0;

	memset(&msg_info, 0, sizeof(struct msg_stream_on_info));
	memset(&(_premeta[pq_dev->dev_indx]), 0, sizeof(struct output_meta_compare_info));
	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Window is not open!\n");
		ret = -EINVAL;
		return ret;
	}

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		if (pq_dev->input_stream.streaming == TRUE)
			return -EINVAL;
	} else {
		if (pq_dev->output_stream.streaming == TRUE)
			return -EINVAL;
	}

	if (pq_dev->stream_on_ref >= STREAM_ON_ONE_SHOT) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Error control flow! ID=%d"
			" Thread info: %s(%d %d), ref_count=%u, stream_on_ref=%u\n",
			pq_dev->dev_indx, current->comm, current->pid, current->tgid,
			pq_dev->ref_count, pq_dev->stream_on_ref);
		ret = -EINVAL;
		return ret;
	}

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		msg_info.stream_type = PQU_MSG_BUF_TYPE_OUTPUT;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		msg_info.stream_type = PQU_MSG_BUF_TYPE_INPUT;
		if (IS_INPUT_SRCCAP(source)) {
			ret = mtk_pq_display_idr_streamon(&pq_dev->display_info.idr,
								pq_dev->dev_indx);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Stream on fail!Buffer Type = %d, ret=%d\n", type, ret);
				return ret;
			}
		} else if (IS_INPUT_B2R(source)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "stream on b2r source!\n");
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"%s: unknown source %d!\n", __func__, source);
			return -EINVAL;
		}

		ret = mtk_pq_buffer_allocate(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_buffer_allocate fail! ret=%d\n", ret);
			return ret;
		}

#if IS_ENABLED(CONFIG_OPTEE)
		/* svp stream on process start */
		ret = mtk_pq_svp_out_streamon(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_svp_out_streamon fail! ret=%d\n", ret);
			return ret;
		}
		/* svp stream on process end */
#endif

		/* dolby stream on process start */
		memset(&dv_in, 0, sizeof(struct mtk_pq_dv_streamon_in));
		memset(&dv_out, 0, sizeof(struct mtk_pq_dv_streamon_out));
		dv_in.dev = pq_dev;
#if IS_ENABLED(CONFIG_OPTEE)
		dv_in.svp_en = pq_dev->display_info.secure_info.svp_enable;
		dv_in.svp_en |= pq_dev->display_info.secure_info.force_svp;
#endif
		ret = mtk_pq_dv_streamon(&dv_in, &dv_out);
		if (ret)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_dv_streamon return %d\n", ret);
		/* reset mcdi start */
		memset((void *)pqdev->DDBuf[MMAP_MCDI_INDEX].va,
			0, pqdev->DDBuf[MMAP_MCDI_INDEX].size);
		/* reset mcdi end */
		requeue_cnt[pq_dev->dev_indx] = 0;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid Buffer Type: %d\n", type);
		return -EINVAL;
	}

	ret = v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"m2m stream on fail!Buffer Type = %d\n", type);
		return ret;
	}

	ret = mtk_pq_common_stream_on(file, type, pq_dev, &msg_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_stream_on fail!\n");
		return ret;
	}

	if (V4L2_TYPE_IS_OUTPUT(type))
		pq_dev->input_stream.streaming = TRUE;
	else
		pq_dev->output_stream.streaming = TRUE;

	pq_dev->common_info.diff_count = 0;

	ctx->state = MTK_STATE_INIT;

	return ret;
}

static int mtk_pq_streamoff(struct file *file,
		void *fh, enum v4l2_buf_type type)
{
	struct mtk_pq_device *pq_dev = video_drvdata(file);
	struct msg_stream_off_info msg_info;
	struct v4l2_event ev;
	struct mtk_pq_dv_streamoff_in dv_in;
	struct mtk_pq_dv_streamoff_out dv_out;
	u32 source = pq_dev->common_info.input_source;
	int ret = 0;

	memset(&msg_info, 0, sizeof(struct msg_stream_off_info));
	memset(&ev, 0, sizeof(struct v4l2_event));

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Window is not open!\n");
		ret = -EINVAL;
		return ret;
	}

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
		pq_dev->input_stream.streaming == FALSE) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "input_stream is not opened\n");
		return -EINVAL;
	}

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
		pq_dev->output_stream.streaming == FALSE) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "output_stream is not opened\n");
		return -EINVAL;
	}

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		msg_info.stream_type = PQU_MSG_BUF_TYPE_OUTPUT;
		ret = mtk_pq_display_mdw_streamoff(&pq_dev->display_info.mdw);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Stream off fail!Buffer Type = %d, ret=%d\n", type, ret);
			return ret;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		msg_info.stream_type = PQU_MSG_BUF_TYPE_INPUT;
		if (IS_INPUT_SRCCAP(source)) {
			ret = mtk_pq_display_idr_streamoff(&pq_dev->display_info.idr);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Stream off fail!Buffer Type = %d, ret=%d\n", type, ret);
				return ret;
			}
		} else if (IS_INPUT_B2R(source)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "stream off b2r source!\n");
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"%s: unknown source %d!\n", __func__, source);
			return -EINVAL;
		}

		/* dolby stream off process start */
		memset(&dv_in, 0, sizeof(struct mtk_pq_dv_streamoff_in));
		memset(&dv_out, 0, sizeof(struct mtk_pq_dv_streamoff_out));
		dv_in.dev = pq_dev;
		ret = mtk_pq_dv_streamoff(&dv_in, &dv_out);
		if (ret)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_dv_streamoff return %d\n", ret);
		/* dolby stream off process end */
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid Buffer Type: %d\n", type);
		return -EINVAL;
	}

	/* prepare msg to pqu */
	ret = mtk_pq_common_stream_off(pq_dev, type, &msg_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_stream_off fail!\n");
		return ret;
	}

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		pq_dev->input_stream.streaming = FALSE;
		ev.type = V4L2_EVENT_MTK_PQ_INPUT_STREAMOFF;

		v4l2_event_queue(&pq_dev->video_dev, &ev);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "queue input stream off event!\n");
	} else {
		pq_dev->output_stream.streaming = FALSE;
		ev.type = V4L2_EVENT_MTK_PQ_OUTPUT_STREAMOFF;

		v4l2_event_queue(&pq_dev->video_dev, &ev);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "queue output stream off event!\n");
	}

	if ((!pq_dev->input_stream.streaming) && (!pq_dev->output_stream.streaming)) {
		ev.type = V4L2_EVENT_MTK_PQ_STREAMOFF;
		v4l2_event_queue(&pq_dev->video_dev, &ev);
		pq_dev->bInvalid = false;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "queue stream off event!\n");

		mtk_pq_common_set_path_disable(pq_dev, V4L2_PQ_PATH_PQ);
	}

	pq_dev->common_info.diff_count = 0;

	return ret;
}

static int mtk_pq_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct mtk_pq_device *pq_dev = video_get_drvdata(fh->vdev);

	switch (sub->type) {
	case V4L2_EVENT_HDR_SEAMLESS_MUTE:
	case V4L2_EVENT_HDR_SEAMLESS_UNMUTE:
		if (mtk_pq_enhance_subscribe_event(pq_dev, sub->type)) {
			v4l2_err(&pq_dev->v4l2_dev,
				"failed to subscribe %d event\n", sub->type);
			return -EPERM;
		}
		break;
	case V4L2_EVENT_MTK_PQ_INPUT_DONE:
	case V4L2_EVENT_MTK_PQ_OUTPUT_DONE:
	case V4L2_EVENT_MTK_PQ_CALLBACK:
	case V4L2_EVENT_MTK_PQ_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_INPUT_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_OUTPUT_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_RE_QUEUE_TRIGGER:
	case V4L2_EVENT_MTK_PQ_BBD_TRIGGER:
	case V4L2_EVENT_MTK_PQ_DOVI_FRAME_META:
		break;
	default:
		return -EINVAL;
	}

	return v4l2_event_subscribe(fh, sub, PQ_NEVENTS, NULL);
}

static int mtk_pq_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct mtk_pq_device *pq_dev = video_get_drvdata(fh->vdev);

	switch (sub->type) {
	case V4L2_EVENT_MTK_PQ_INPUT_DONE:
	case V4L2_EVENT_MTK_PQ_OUTPUT_DONE:
	case V4L2_EVENT_MTK_PQ_CALLBACK:
	case V4L2_EVENT_MTK_PQ_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_INPUT_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_OUTPUT_STREAMOFF:
	case V4L2_EVENT_MTK_PQ_RE_QUEUE_TRIGGER:
	case V4L2_EVENT_MTK_PQ_BBD_TRIGGER:
	case V4L2_EVENT_MTK_PQ_DOVI_FRAME_META:
		break;
	case V4L2_EVENT_HDR_SEAMLESS_MUTE:
	case V4L2_EVENT_HDR_SEAMLESS_UNMUTE:
		if (mtk_pq_enhance_unsubscribe_event(pq_dev, sub->type)) {
			v4l2_err(&pq_dev->v4l2_dev,
				"failed to unsubscribe %d event\n", sub->type);
			return -EPERM;
		}
		break;
	case V4L2_EVENT_ALL:
		if (mtk_pq_enhance_unsubscribe_event(pq_dev, sub->type)) {
			v4l2_err(&pq_dev->v4l2_dev,
				"failed to unsubscribe %d event\n", sub->type);
			return -EPERM;
		}
		break;
	default:
		return -EINVAL;
	}

	return v4l2_event_unsubscribe(fh, sub);
}

static int mtk_pq_reqbufs(struct file *file,
	void *fh, struct v4l2_requestbuffers *reqbufs)
{
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Buffer Type = %d!\n", reqbufs->type);

	ret = v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Request buffer fail!Buffer Type = %d, ret=%d\n",
			reqbufs->type, ret);

	return ret;
}

static int mtk_pq_querybuf(struct file *file,
	void *fh, struct v4l2_buffer *buffer)
{
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Buffer Type = %d!\n", buffer->type);

	ret = v4l2_m2m_querybuf(file, ctx->m2m_ctx, buffer);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Query buffer fail!Buffer Type = %d\n",
			buffer->type);

	return ret;
}

static int mtk_pq_qbuf(struct file *file,
	void *fh, struct v4l2_buffer *buffer)
{
	int ret = 0;
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Buffer Type = %d!\n", buffer->type);

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Window is not open!\n");
		ret = -EINVAL;
		return ret;
	}

	if (pq_dev->bInvalid) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Queue is invalid dev_indx=%u, buffer type = %d!\n",
			pq_dev->dev_indx, buffer->type);
		ret = -EADDRNOTAVAIL;
		return ret;
	}

#if IS_ENABLED(CONFIG_OPTEE)
	if (V4L2_TYPE_IS_OUTPUT(buffer->type)) {
		ret = mtk_pq_svp_cfg_outbuf_sec(pq_dev, buffer);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_svp_cfg_outbuf_sec fail!\n");
			return ret;
		}
	}
#endif

	ret =  v4l2_m2m_qbuf(file, ctx->m2m_ctx, buffer);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Queue buffer fail! Buffer Type = %d, ret = %d\n",
			buffer->type, ret);
		return ret;
	}

	return ret;
}

static int mtk_pq_dqbuf(struct file *file,
	void *fh, struct v4l2_buffer *buffer)
{
	int ret = 0;
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	struct mtk_pq_device *pq_dev = video_drvdata(file);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Buffer Type = %d!\n", buffer->type);

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Window is not open!\n");
		ret = -EINVAL;
		return ret;
	}

	if (V4L2_TYPE_IS_OUTPUT(buffer->type)) {
		if (pq_dev->input_stream.streaming == FALSE)
			return -ENODATA;
	} else {
		if (pq_dev->output_stream.streaming == FALSE)
			return -ENODATA;
	}

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buffer);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Dequeue buffer fail!Buffer Type = %d\n",
			buffer->type);
		return ret;
	}

	return ret;
}

static int mtk_pq_s_selection(struct file *file, void *fh, struct v4l2_selection *s)
{
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	int ret = 0;

	if (V4L2_TYPE_IS_OUTPUT(s->type)) {
		switch (s->target) {
		case V4L2_SEL_TGT_CROP:
			ret = mtk_pq_display_idr_s_crop(&ctx->pq_dev->display_info.idr, s);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"mtk_pq_display_idr_s_crop fail!\n");
				return ret;
			}
			break;
		default:
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Unsupported target type!\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int mtk_pq_g_selection(struct file *file, void *fh, struct v4l2_selection *s)
{
	struct mtk_pq_ctx *ctx = fh_to_ctx(fh);
	int ret = 0;

	if (V4L2_TYPE_IS_OUTPUT(s->type)) {
		switch (s->target) {
		case V4L2_SEL_TGT_CROP:
			ret = mtk_pq_display_idr_g_crop(&ctx->pq_dev->display_info.idr, s);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"mtk_pq_display_idr_g_crop fail!\n");
				return ret;
			}
			break;
		default:
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Unsupported target type!\n");
			return -EINVAL;
		}
	}

	return 0;
}

/* v4l2_ioctl_ops */
static const struct v4l2_ioctl_ops mtk_pq_dec_ioctl_ops = {
	.vidioc_querycap		= mtk_pq_querycap,
	.vidioc_s_input			= mtk_pq_s_input,
	.vidioc_g_input			= mtk_pq_g_input,
	.vidioc_s_fmt_vid_cap_mplane	= mtk_pq_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane	= mtk_pq_s_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= mtk_pq_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out_mplane	= mtk_pq_g_fmt_vid_out_mplane,
	.vidioc_streamon		= mtk_pq_streamon,
	.vidioc_streamoff		= mtk_pq_streamoff,
	.vidioc_subscribe_event		= mtk_pq_subscribe_event,
	.vidioc_unsubscribe_event	= mtk_pq_unsubscribe_event,
	.vidioc_reqbufs			= mtk_pq_reqbufs,
	.vidioc_querybuf		= mtk_pq_querybuf,
	.vidioc_qbuf			= mtk_pq_qbuf,
	.vidioc_dqbuf			= mtk_pq_dqbuf,
	.vidioc_s_selection		= mtk_pq_s_selection,
	.vidioc_g_selection		= mtk_pq_g_selection,
};

static int _mtk_pq_queue_setup(struct vb2_queue *vq,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_pq_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_pq_device *pq_dev = ctx->pq_dev;
	u32 source = pq_dev->common_info.input_source;
	int ret = 0;

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (IS_INPUT_SRCCAP(source)) {
			ret = mtk_pq_display_idr_queue_setup(
				vq, num_buffers, num_planes, sizes, alloc_devs);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_queue_setup_out fail, ret=%d!\n", ret);
				return ret;
			}
		} else if (IS_INPUT_B2R(source)) {
			ret = mtk_pq_display_b2r_queue_setup(
				vq, num_buffers, num_planes, sizes, alloc_devs);
			*num_planes = 2;
			sizes[0] = 4;
			sizes[1] = 4;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "reset buffer count!\n");
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"%s: unknown source %d!\n", __func__, source);
			return -EINVAL;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = mtk_pq_display_mdw_queue_setup(
			vq, num_buffers, num_planes, sizes, alloc_devs);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_queue_setup_cap fail, ret=%d!\n", ret);
			return ret;
		}
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid buffer type %d!\n", vq->type);
		return -EINVAL;
	}

	return 0;
}

static int _mtk_pq_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_pq_buffer *pq_buf = NULL;
	struct mtk_pq_ctx *ctx = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct scatterlist *sg = NULL;

	if ((!vb) || (!vb->vb2_queue)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY, "Buffer Type = %d!\n", vb->type);

	ctx = vb2_get_drv_priv(vb->vb2_queue);
	if ((!ctx) || (!ctx->pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pq_dev = ctx->pq_dev;

	if (vb->memory != VB2_MEMORY_DMABUF) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid Memory Type!\n");
		return -EINVAL;
	}

	pq_buf = container_of(container_of(vb, struct vb2_v4l2_buffer, vb2_buf),
		struct mtk_pq_buffer, vb);
	if (pq_buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pq_buf->meta_buf.fd = vb->planes[vb->num_planes - 1].m.fd;
	pq_buf->meta_buf.db = dma_buf_get(pq_buf->meta_buf.fd);
	if (pq_buf->meta_buf.db == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pq_buf->meta_buf.va = vb2_plane_vaddr(vb, (vb->num_planes - 1));
	pq_buf->meta_buf.size = vb2_plane_size(vb, (vb->num_planes - 1));
	pq_buf->meta_buf.sgt = vb2_plane_cookie(vb, (vb->num_planes - 1));
	if (pq_buf->meta_buf.sgt == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		dma_buf_put(pq_buf->meta_buf.db);
		return -EINVAL;
	}

	sg = pq_buf->meta_buf.sgt->sgl;
	pq_buf->meta_buf.pa = page_to_phys(sg_page(sg)) + sg->offset - pq_dev->memory_bus_base;
	pq_buf->meta_buf.iova = 0; //sg_dma_address(sg);

	pq_buf->frame_buf.fd = vb->planes[0].m.fd;
	pq_buf->frame_buf.va = 0; //vb2_plane_vaddr(vb, 0);
	pq_buf->frame_buf.db = dma_buf_get(pq_buf->frame_buf.fd);
	if (pq_buf->frame_buf.db == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		dma_buf_put(pq_buf->meta_buf.db);
		return -EINVAL;
	}

	pq_buf->frame_buf.size = vb2_plane_size(vb, 0);
	pq_buf->frame_buf.sgt = vb2_plane_cookie(vb, 0);
	if (pq_buf->frame_buf.sgt == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		dma_buf_put(pq_buf->meta_buf.db);
		dma_buf_put(pq_buf->frame_buf.db);
		return -EINVAL;
	}
	sg = pq_buf->frame_buf.sgt->sgl;
	//pq_buf->frame_buf.pa = //page_to_phys(sg_page(sg)) + sg->offset - pq_dev->memory_bus_base;
	pq_buf->frame_buf.pa = 0;
	pq_buf->frame_buf.iova = sg_dma_address(sg);
	if (pq_buf->frame_buf.iova < 0x200000000) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Error iova=0x%llx\n", pq_buf->frame_buf.iova);
		dma_buf_put(pq_buf->meta_buf.db);
		dma_buf_put(pq_buf->frame_buf.db);
		return -EINVAL;
	}

	return 0;
}

static void _mtk_pq_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_pq_ctx *ctx = NULL;

	if (!vb) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}
	ctx = vb2_get_drv_priv(vb->vb2_queue);
	if (!ctx) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
}

static void _mtk_pq_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_pq_buffer *pq_buf = NULL;

	if (vb == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Buffer Type = %d!\n", vb->type);

	pq_buf = container_of(container_of(vb, struct vb2_v4l2_buffer, vb2_buf),
		struct mtk_pq_buffer, vb);
	if (pq_buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	dma_buf_put(pq_buf->meta_buf.db);
	dma_buf_put(pq_buf->frame_buf.db);

	return;
}

static const struct vb2_ops mtk_pq_vb2_qops = {
	.queue_setup	= _mtk_pq_queue_setup,
	.buf_queue	= _mtk_pq_buf_queue,
	.buf_prepare	= _mtk_pq_buf_prepare,
	.buf_finish	= _mtk_pq_buf_finish,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
	struct vb2_queue *dst_vq)
{
	struct mtk_pq_ctx *ctx = priv;
	int ret = 0;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_pq_vb2_qops;
	src_vq->mem_ops = &vb2_dma_sg_memops;
	src_vq->buf_struct_size = sizeof(struct mtk_pq_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->pq_dev->mutex;
	src_vq->dev = ctx->pq_dev->dev;
	src_vq->allow_zero_bytesused = 1;
	src_vq->min_buffers_needed = 1;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "vb2_queue_init fail!\n");
		return ret;
	}

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_pq_vb2_qops;
	dst_vq->mem_ops = &vb2_dma_sg_memops;
	dst_vq->buf_struct_size = sizeof(struct mtk_pq_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->pq_dev->mutex;
	dst_vq->dev = ctx->pq_dev->dev;
	dst_vq->allow_zero_bytesused = 1;
	dst_vq->min_buffers_needed = 1;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "vb2_queue_init fail!\n");
		return ret;
	}

	return 0;
}

static int mtk_pq_open(struct file *file)
{
	int ret = 0;
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_ctx *pq_ctx = NULL;

	if (file == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	pq_dev = video_drvdata(file);

	mutex_lock(&pq_dev->mutex);

	if (pq_dev->ref_count) {
		pq_dev->ref_count++;
		file->private_data = &pq_dev->m2m.ctx->fh;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Increase Window (%d) Reference count: %d!\n",
			pq_dev->dev_indx, pq_dev->ref_count);
		goto unlock;
	}

	pq_ctx = kzalloc(sizeof(struct mtk_pq_ctx), GFP_KERNEL);
	if (!pq_ctx) {
		ret = -ENOMEM;
		goto unlock;
	}

	pq_ctx->pq_dev = pq_dev;
	pq_ctx->m2m_ctx = v4l2_m2m_ctx_init(pq_dev->m2m.m2m_dev,
				pq_ctx, queue_init);

	pq_dev->m2m.ctx = pq_ctx;

	v4l2_fh_init(&pq_ctx->fh, video_devdata(file));
	pq_ctx->fh.m2m_ctx = pq_ctx->m2m_ctx;
	file->private_data = &pq_ctx->fh;
	v4l2_fh_add(&pq_ctx->fh);

	ret = mtk_pq_buffer_buf_init(pq_dev);
	if (ret < 0) {
		pr_err("MTK PQ : pq buffer init failed\n");
		goto unlock;
	}

#if IS_ENABLED(CONFIG_OPTEE)
	ret = mtk_pq_svp_init(pq_dev);
	if (ret < 0) {
		pr_err("MTK PQ : pq svp init failed\n");
		goto unlock;
	}
#endif

	ret = mtk_pq_enhance_open(pq_dev);
	if (ret) {
		v4l2_err(&pq_dev->v4l2_dev,
			"failed to open enhance subdev!\n");
		goto unlock;
	}

	pq_dev->ref_count = 1;
	pq_dev->input_stream.streaming = FALSE;
	pq_dev->output_stream.streaming = FALSE;

unlock:
	mutex_unlock(&pq_dev->mutex);
	return ret;
}

static int mtk_pq_release(struct file *file)
{
	int ret = 0;
	struct mtk_pq_device *pq_dev = NULL;
	struct v4l2_fh *fh = NULL;
	struct mtk_pq_ctx *ctx = NULL;

	if (file == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	pq_dev = video_drvdata(file);
	fh = (struct v4l2_fh *)file->private_data;
	ctx = fh_to_ctx(fh);

	mutex_lock(&pq_dev->mutex);

	if (pq_dev->ref_count > 1) {
		pq_dev->ref_count--;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Decrease Window (%d) Reference count: %d!\n",
			pq_dev->dev_indx, pq_dev->ref_count);
		goto unlock;
	}

	if (pq_dev->stream_on_ref) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Error control flow! ID=%d"
			" Thread info: %s(%d %d), ref_count=%u, stream_on_ref=%u\n",
			pq_dev->dev_indx, current->comm, current->pid, current->tgid,
			pq_dev->ref_count, pq_dev->stream_on_ref);

		ret = mtk_pq_streamoff(file, fh,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Try to stream off(OUTPUT) fail!\n");
		}
		ret = mtk_pq_streamoff(file, fh,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Try to stream off(CAPTURE) fail!\n");
		}

		if (pq_dev->stream_on_ref != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Recover stream off fail!\n");
			goto unlock;
		}

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Recover stream off done!\n");
	}

	if (pq_dev->stream_on_ref == 0) {
		mtk_pq_common_put_dma_buf(&pq_dev->stream_dma_info);
	}

	v4l2_m2m_ctx_release(ctx->m2m_ctx);

#if IS_ENABLED(CONFIG_OPTEE)
	mtk_pq_svp_exit(pq_dev);
#endif

	ret = mtk_pq_buffer_buf_exit(pq_dev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "mtk_pq_buffer_buf_exit fail!\n");
		goto unlock;
	}

	ret = mtk_pq_enhance_close(pq_dev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_enhance_close fail!\n");
		goto unlock;
	}

	v4l2_fh_del(fh);
	v4l2_fh_exit(fh);
	PQ_FREE(fh, sizeof(struct v4l2_fh));

	pq_dev->ref_count = 0;
	pq_dev->input_stream.streaming = FALSE;
	pq_dev->output_stream.streaming = FALSE;

unlock:
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Release done!\n");
	mutex_unlock(&pq_dev->mutex);

	return ret;
}

static __poll_t mtk_pq_poll(struct file *file, struct poll_table_struct *wait)
{
	struct v4l2_fh *fh;
	__poll_t ret = 0;

	if (file == NULL || wait == NULL)
		return 0;

	fh = file->private_data;

	if (v4l2_event_pending(fh))
		ret = EPOLLPRI;
	else
		poll_wait(file, &fh->wait, wait);

	return ret;
}

static int mmap_userdev_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	// test_1: mtk_ltp_mmap_map_user_va_rw
	int ret;
	u64 start_bus_pfn;
	struct mtk_pq_device *pq_dev = video_drvdata(file);
	struct mtk_pq_platform_device *pqdev = NULL;
	bool cached = false;

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	// step_1: get mmap info
	if (mtk_hdr_GetDVBinMmpStatus()) {
		if (pqdev->dv_config.pa == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid DV CFG PA\n");
			return -EINVAL;
		}
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"mmap index: %d\n", pqdev->pq_memory_index);
		if (pqdev->DDBuf[pqdev->pq_memory_index].pa == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid PQMAP PA\n");
			return -EINVAL;
		}
	}

	// step_2: mmap to user_va
	if (mtk_hdr_GetDVBinMmpStatus()) {
		start_bus_pfn = pqdev->dv_config.pa >> PAGE_SHIFT;
		//check buf size
		if ((vma->vm_end - vma->vm_start) > pqdev->dv_config.size) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqdev->dv_config.size = 0x%x, (vma->vm_end - vma->vm_start) = 0x%lx\n",
				pqdev->dv_config.size, (vma->vm_end - vma->vm_start));
			return -EINVAL;
		}

		cached = false;
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	} else {
		start_bus_pfn = pqdev->DDBuf[pqdev->pq_memory_index].pa >> PAGE_SHIFT;
		//check buf size
		if ((vma->vm_end - vma->vm_start) > pqdev->DDBuf[pqdev->pq_memory_index].size) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqdev->DDBuf.size = 0x%x, (vma->vm_end - vma->vm_start) = 0x%lx\n",
				pqdev->DDBuf[pqdev->pq_memory_index].size,
				(vma->vm_end - vma->vm_start));
			return -EINVAL;
		}

		if (mtk_pq_get_dedicate_memory_cacheable(pqdev->pq_memory_index, &cached)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_get_dedicate_memory_cacheable fail, mem idx=%d\n",
				pqdev->pq_memory_index);
			return -EINVAL;
		}

		if (!cached)
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	}

	if (io_remap_pfn_range(vma, vma->vm_start,
		start_bus_pfn, (vma->vm_end - vma->vm_start),
		vma->vm_page_prot)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mmap user space cached(%d) va failed\n", cached);
		ret = -EAGAIN;
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"mmap user space cached(%d) va: 0x%lX ~ 0x%lX\n", cached,
			vma->vm_start, vma->vm_end);
	}
	ret = 0;

	return ret;
}

/* v4l2 ops */
static const struct v4l2_file_operations mtk_pq_fops = {
	.owner              = THIS_MODULE,
	.open               = mtk_pq_open,
	.release            = mtk_pq_release,
	.poll               = mtk_pq_poll,
	.unlocked_ioctl     = video_ioctl2,
	.mmap               = mmap_userdev_mmap,
};

static void _mtk_pq_debug_calc_num(struct mtk_pq_device *pq_dev)
{
#define DEBUG_PQ_CALC_NUM_MSLEEP_TIME    100
#define DEBUG_PQ_CALC_NUM_MSLEEP_NUM     1000000
	int retry_num = 0;

	while (pq_dev->debug_calc_num == 0) {
		retry_num++;
		if (retry_num > DEBUG_PQ_CALC_NUM_MSLEEP_NUM)
			break;
		msleep(DEBUG_PQ_CALC_NUM_MSLEEP_TIME);
	}
	if (pq_dev->debug_calc_num > 0) {
		pq_dev->debug_calc_num--;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "debug_calc_num = %u\n", pq_dev->debug_calc_num);
	}
}

static int _mtk_pq_compare_frame_metadata(struct mtk_pq_device *pq_dev,
	struct msg_queue_info *msg_info)
{
	struct mtk_pq_caps *pqcaps;
	int ret = 0;
	bool meta_change = false;
	u8 u8Maxcount = 0;
	u8 u8count = 0;
	struct meta_pq_stream_info *stream_meta = NULL;
	struct m_pq_queue_ext_info *queue_ext_info = NULL;

	pqcaps = &pqdev->pqcaps;

	if ((pq_dev == NULL) || (msg_info == NULL) | (pqcaps == NULL)) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	stream_meta = &(pq_dev->stream_meta);
	queue_ext_info = &(pq_dev->pqu_queue_ext_info);

	if (stream_meta->pq_mode == meta_pq_mode_legacy) {
		ret = mtk_display_compare_frame_metadata(pq_dev,
		&(_premeta[pq_dev->dev_indx]), &meta_change);

		//check meta change and count r/w diff
		if ((pq_dev->common_info.op2_diff + 1) > MTK_PQ_MAX_RWDIFF)
			u8Maxcount = MTK_PQ_MAX_RWDIFF - 1;
		else
			u8Maxcount = pq_dev->common_info.op2_diff + 1;

		for (u8count = 0; u8count < u8Maxcount; u8count++) {
			if ((u8count == (u8Maxcount - 1))) {
				pq_dev->output_meta_change[u8count] = meta_change;
			} else {
				pq_dev->output_meta_change[u8count] =
					pq_dev->output_meta_change[u8count + 1];
			}
		}

		//check need output or not
		if (pq_dev->output_meta_change[0] == true) {
			pq_dev->need_output = true;
			msg_info->need_output_frame = true;
		}

	} else {
		msg_info->need_output_frame = true;
		pq_dev->need_output = true;
	}

	if ((stream_meta->pq_mode == meta_pq_mode_legacy) &&
		IS_INPUT_SRCCAP(pq_dev->common_info.input_source) &&
		(pq_dev->pqu_queue_ext_info.bPerFrameMode == false) && (meta_change == true)) {
		uint8_t idx = 0;

		// need rwdiff + 1 to refresh hwmap
		for (idx = requeue_cnt[pq_dev->dev_indx];
				idx < pq_dev->common_info.op2_diff + 1; idx++) {
			struct v4l2_event ev;

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY,
				"Requeue frame cnt=%u, idx=%u, op2_diff=%u\n",
				requeue_cnt[pq_dev->dev_indx],
				idx,
				pq_dev->common_info.op2_diff);

			ev.type = V4L2_EVENT_MTK_PQ_RE_QUEUE_TRIGGER;
			v4l2_event_queue(&(pq_dev->video_dev), &ev);

			requeue_cnt[pq_dev->dev_indx] += 1;
		}
	} else {
		if (requeue_cnt[pq_dev->dev_indx] > 0)
			requeue_cnt[pq_dev->dev_indx] -= 1;
	}

	return ret;
}

static void mtk_m2m_device_run(void *priv)
{
	int ret = 0;
	u32 source = 0;
	struct mtk_pq_ctx *pq_ctx = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct vb2_v4l2_buffer *src_buf = NULL;
	struct vb2_v4l2_buffer *dst_buf = NULL;
	struct mtk_pq_buffer *pq_src_buf = NULL;
	struct mtk_pq_buffer *pq_dst_buf = NULL;
	struct msg_queue_info msg_info;


	if (!priv) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Priv Pointer is NULL!\n");
		return;
	}

	memset(&msg_info, 0, sizeof(struct msg_queue_info));

	pq_ctx = priv;
	pq_dev = pq_ctx->pq_dev;
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev is NULL!\n");
		return;
	}

	_mtk_pq_debug_calc_num(pq_dev);

	if (pq_ctx->state == MTK_STATE_ABORT) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "job was cancelled!\n");
		goto job_finish;
	}

	source = pq_dev->common_info.input_source;


	STI_PQ_CD_LOG_RUN(pq_dev->log_cd);

	//input buffer
	src_buf = v4l2_m2m_src_buf_remove(pq_ctx->m2m_ctx);
	if (src_buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Input Buffer is NULL!\n");
		goto job_finish;
	}

	pq_src_buf = container_of(src_buf, struct mtk_pq_buffer, vb);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[input] frame fd = %d, meta fd = %d, index = %d!\n",
		pq_src_buf->frame_buf.fd, pq_src_buf->meta_buf.fd,
		pq_src_buf->vb.vb2_buf.index);

	if (IS_INPUT_SRCCAP(source)) {
		ret = mtk_pq_display_idr_qbuf(pq_dev, pq_src_buf);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_display_idr_qbuf fail!\n");
			goto job_finish;
		}
	} else if (IS_INPUT_B2R(source)) {
		ret = mtk_pq_display_b2r_qbuf(pq_dev, pq_src_buf);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_display_b2r_qbuf fail!\n");
			goto job_finish;
		}
		mtk_pq_common_is_slice(pq_dev, pq_src_buf);
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"%s: unknown source %d!\n", __func__, source);
		goto job_finish;
	}

	ret = mtk_display_set_frame_metadata(pq_dev, pq_src_buf);

	ret = mtk_pq_dv_qbuf(pq_dev, pq_src_buf);

	ret = mtk_pq_display_mdw_qbuf(pq_dev, pq_src_buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_display_mdw_qbuf fail!\n");
		goto job_finish;
	}

	msg_info.frame_type = PQU_MSG_BUF_TYPE_INPUT;

	_mtk_pq_compare_frame_metadata(pq_dev, &msg_info);

	ret = mtk_pq_common_qbuf(pq_dev, pq_src_buf, &msg_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Common Queue Input Buffer Fail!\n");
		goto job_finish;
	}

	//output buffer
	if (pq_dev->common_info.diff_count >= pq_dev->common_info.op2_diff) {
		if (pq_dev->need_output == true) {
			dst_buf = v4l2_m2m_dst_buf_remove(pq_ctx->m2m_ctx);
			if (src_buf == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Output Buffer is NULL!\n");
				goto job_finish;
			}

			pq_dst_buf = container_of(dst_buf, struct mtk_pq_buffer, vb);

			STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
				"[output] frame fd = %d, meta fd = %d, index = %d!\n",
				pq_dst_buf->frame_buf.fd, pq_dst_buf->meta_buf.fd,
				pq_dst_buf->vb.vb2_buf.index);

#if IS_ENABLED(CONFIG_OPTEE)
			ret = mtk_pq_svp_cfg_capbuf_sec(pq_dev, pq_dst_buf);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"mtk_pq_cfg_capture_buf_sec fail!\n");
				goto job_finish;
			}
#endif

			ret = mtk_pq_display_mdw_qbuf(pq_dev, pq_dst_buf);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"mtk_pq_display_mdw_qbuf fail!\n");
				goto job_finish;
			}
			pq_dst_buf->slice_mode = pq_src_buf->slice_mode;
			pq_dst_buf->slice_qbuf = pq_src_buf->slice_qbuf;
			pq_dst_buf->slice_need_output = true;
			msg_info.frame_type = PQU_MSG_BUF_TYPE_OUTPUT;
			ret = mtk_pq_common_qbuf(pq_dev, pq_dst_buf, &msg_info);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Common Queue Output Buffer Fail!\n");
				goto job_finish;
			}
			pq_dev->need_output = false;
		}
	} else {
		pq_dev->common_info.diff_count++;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "diff_count = %d, diff = %d!\n",
			pq_dev->common_info.diff_count, pq_dev->common_info.op2_diff);
	}

	if (IS_INPUT_B2R(source))
		mtk_pq_common_qslice(pq_dev, pq_src_buf, pq_dst_buf);

	STI_PQ_CD_LOG_RESET(pq_dev->log_cd, u32LogInterval);

job_finish:
	v4l2_m2m_job_finish(pq_dev->m2m.m2m_dev, pq_ctx->m2m_ctx);

	return;
}

static void mtk_m2m_job_abort(void *priv)
{
	struct mtk_pq_ctx *ctx = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter job abort!\n");

	if (!priv) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Priv Pointer is NULL!\n");
		return;
	}

	ctx = priv;

	ctx->state = MTK_STATE_ABORT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "exit job abort!\n");

	return;
}

// pq_init_subdevices - Initialize subdev structures and resources
// @pq_dev: pq device
// Return 0 on success or a negative error code on failure

static int pq_init_subdevices(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;

	/* ret = mtk_pq_b2r_subdev_init(pqdev->dev, &pqdev->b2r_dev);*/
	/* move to pqu */
	if (ret < 0) {
		PQ_MSG_WARNING("[B2R] get resource failed\n");
		ret = -1;
	}

	return ret;
}

static struct v4l2_m2m_ops mtk_pq_m2m_ops = {
	.device_run = mtk_m2m_device_run,
	.job_abort = mtk_m2m_job_abort,
};

static void _mtk_pq_unregister_v4l2dev(struct mtk_pq_device *pq_dev)
{
	mtk_pq_unregister_enhance_subdev(&pq_dev->subdev_enhance);
	mtk_pq_unregister_display_subdev(&pq_dev->subdev_display);
	mtk_pq_unregister_b2r_subdev(&pq_dev->subdev_b2r);
	mtk_pq_unregister_common_subdev(&pq_dev->subdev_common);
	v4l2_m2m_release(pq_dev->m2m.m2m_dev);
	video_unregister_device(&pq_dev->video_dev);
	v4l2_device_unregister(&pq_dev->v4l2_dev);
}

static int _mtk_pq_register_v4l2dev(struct mtk_pq_device *pq_dev,
	__u8 dev_id, struct mtk_pq_platform_device *pqdev)
{
	struct video_device *vdev;
	struct v4l2_device *v4l2_dev;
	unsigned int len;
	int ret = 0, id = 0, checker;

	if ((!pq_dev) || (!pqdev))
		return -ENOMEM;

	spin_lock_init(&pq_dev->slock);
	mutex_init(&pq_dev->mutex);
	mutex_init(&pq_dev->mutex_set_path);
	mutex_init(&pq_dev->mutex_stream_off);

	vdev = &pq_dev->video_dev;
	v4l2_dev = &pq_dev->v4l2_dev;

	pq_dev->dev = pqdev->dev;
	pq_dev->dev_indx = dev_id;

	//patch here
	pq_dev->b2r_dev.id = pqdev->b2r_dev.id;
	pq_dev->b2r_dev.irq = pqdev->b2r_dev.irq;
	spin_lock_init(&pqdev->b2r_dev.b2r_slock);

	checker = snprintf(v4l2_dev->name, sizeof(v4l2_dev->name),
		"%s", PQ_V4L2_DEVICE_NAME);
	if (checker < 0 || checker > sizeof(v4l2_dev->name)) {
		v4l2_err(v4l2_dev, "failed to register v4l2 device (snprintf failed)\n");
		ret = -EINVAL;
		goto exit;
	}
	ret = v4l2_device_register(NULL, v4l2_dev);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register v4l2 device\n");
		goto exit;
	}

	pq_dev->m2m.m2m_dev = v4l2_m2m_init(&mtk_pq_m2m_ops);
	if (IS_ERR(pq_dev->m2m.m2m_dev)) {
		ret = PTR_ERR(pq_dev->m2m.m2m_dev);
		v4l2_err(v4l2_dev, "failed to init m2m device\n");
		goto exit;
	}

	v4l2_ctrl_handler_init(&pq_dev->ctrl_handler, 0);
	v4l2_dev->ctrl_handler = &pq_dev->ctrl_handler;

	ret = mtk_pq_register_common_subdev(v4l2_dev, &pq_dev->subdev_common,
		&pq_dev->common_ctrl_handler);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register common sub dev\n");
		goto exit;
	}
	ret = mtk_pq_register_enhance_subdev(v4l2_dev, &pq_dev->subdev_enhance,
		&pq_dev->enhance_ctrl_handler);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register drv pq sub dev\n");
		goto exit;
	}
	ret = mtk_pq_register_display_subdev(v4l2_dev, &pq_dev->subdev_display,
		&pq_dev->display_ctrl_handler);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register display sub dev\n");
		goto exit;
	}
	ret = mtk_pq_register_b2r_subdev(v4l2_dev, &pq_dev->subdev_b2r,
		&pq_dev->b2r_ctrl_handler);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register b2r sub dev\n");
		goto exit;
	}

	vdev->fops = &mtk_pq_fops;
	vdev->ioctl_ops = &mtk_pq_dec_ioctl_ops;
	vdev->release = video_device_release;
	vdev->v4l2_dev = v4l2_dev;
	vdev->vfl_dir = VFL_DIR_M2M;
	//device name is "mtk-pq-dev" + "0"/"1"/...
	len = snprintf(vdev->name, sizeof(vdev->name), "%s",
		PQ_VIDEO_DEVICE_NAME);
	snprintf(vdev->name + len, sizeof(vdev->name) - len, "%s",
		devID[dev_id]);

	if (pqdev->cus_dev)
		id = pqdev->cus_id[dev_id];
	else
		id = -1;

	ret = video_register_device(vdev, VFL_TYPE_GRABBER, id);
	if (ret) {
		v4l2_err(v4l2_dev, "failed to register video device!\n");
		goto exit;
	}

	video_set_drvdata(vdev, pq_dev);
	v4l2_info(v4l2_dev, "mtk-pq registered as /dev/video%d\n", vdev->num);
	pqdev->pqcaps.u32Device_register_Num[dev_id] = vdev->num;
	return 0;

exit:
	_mtk_pq_unregister_v4l2dev(pq_dev);

	return -EPERM;
}

static void _mtk_pq_probe_unmap_dedicated_buffer(
	struct mtk_pq_platform_device *pqdev)
{
	int idx = 0;
	struct mtk_pq_device *pq_dev = NULL;

	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return;
	}

	if (pqdev->DDBuf[MMAP_PQMAP_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].va);
		pqdev->DDBuf[MMAP_PQMAP_INDEX].va = NULL;
	}
	if (pqdev->DDBuf[MMAP_PQPARAM_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va);
		pqdev->DDBuf[MMAP_PQPARAM_INDEX].va = NULL;
	}
	if (pqdev->DDBuf[MMAP_MCDI_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_MCDI_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_MCDI_INDEX].va);
		pqdev->DDBuf[MMAP_MCDI_INDEX].va = NULL;
	}
	if (pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va);
		pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va = NULL;
	}
	for (idx = 0; idx < PQ_WIN_MAX_NUM; idx++) {
		pq_dev = pqdev->pq_devs[idx];
		if (pq_dev) {
			if (pq_dev->pqu_stream_dma_info.va != NULL) {
				memset(&pq_dev->pqu_stream_dma_info, 0,
						sizeof(struct mtk_pq_remap_buf));
				memunmap((void *)pq_dev->pqu_stream_dma_info.va);
				pq_dev->pqu_stream_dma_info.va = NULL;
			}
		}
	}
}

static void _mtk_pq_probe_memremap(struct mtk_pq_platform_device *pqdev,
	 struct mtk_pq_remap_buf *buf, unsigned long flag)
{
	if ((pqdev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return;
	}

	if (buf->size) {
		/* do memremap when valid size */
		buf->va = (__u64)memremap(buf->pa, buf->size, flag);
		if (buf->va == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Failed to memremap(0x%llx, 0x%x)\n",
				buf->pa, buf->size);
			_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
		}
	}
}

static int _mtk_pq_probe_mmap_dedicated_buffer(
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0, cnt = 0;
	__u8 win_num = 0;
	__u64 stream_pa = 0;
	__u32 stream_size = 0;
	__u32 stream_offset = 0;
	struct mtk_pq_device *pq_dev = NULL;
	struct meta_buffer meta_buffer;

	if (IS_ERR_OR_NULL(pqdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev Pointer is NULL!");
		return -EINVAL;
	}

	win_num = pqdev->xc_win_num;
	if (win_num > PQ_WIN_MAX_NUM)
		win_num = PQ_WIN_MAX_NUM;
	memset(&meta_buffer, 0, sizeof(struct meta_buffer));

	/* MMAP PQMAP Buffer (Cached) */
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_PQMAP_INDEX,
		&pqdev->DDBuf[MMAP_PQMAP_INDEX].pa, &pqdev->DDBuf[MMAP_PQMAP_INDEX].size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to mtk_pq_get_dedicate_memory_info(index=%d)\n", MMAP_PQMAP_INDEX);
		_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
	}
	if (pqdev->DDBuf[MMAP_PQMAP_INDEX].va == NULL)
		_mtk_pq_probe_memremap(pqdev, &pqdev->DDBuf[MMAP_PQMAP_INDEX], MEMREMAP_WB);

	/* MMAP PQPARAM Buffer (Non-cached) */
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_PQPARAM_INDEX,
		&pqdev->DDBuf[MMAP_PQPARAM_INDEX].pa, &pqdev->DDBuf[MMAP_PQPARAM_INDEX].size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to mtk_pq_get_dedicate_memory_info(index=%d)\n", MMAP_PQPARAM_INDEX);
		_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
		return ret;
	}
	if (pqdev->DDBuf[MMAP_PQPARAM_INDEX].va == NULL) {
#ifdef CONFIG_ARM64
		pqdev->DDBuf[MMAP_PQPARAM_INDEX].va =
			(__u64)ioremap_np(pqdev->DDBuf[MMAP_PQPARAM_INDEX].pa,
			pqdev->DDBuf[MMAP_PQPARAM_INDEX].size);
#else
		pqdev->DDBuf[MMAP_PQPARAM_INDEX].va =
			(__u64)memremap(pqdev->DDBuf[MMAP_PQPARAM_INDEX].pa,
			pqdev->DDBuf[MMAP_PQPARAM_INDEX].size, MEMREMAP_WB);
#endif
		if (pqdev->DDBuf[MMAP_PQPARAM_INDEX].va == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to memremap(0x%llx, 0x%x)\n",
				pqdev->DDBuf[MMAP_PQPARAM_INDEX].pa,
				pqdev->DDBuf[MMAP_PQPARAM_INDEX].size);
			_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
			return -ENOMEM;
		}
	}
	meta_buffer.paddr = (uint8_t *)(uintptr_t)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va;
	meta_buffer.size = pqdev->DDBuf[MMAP_PQPARAM_INDEX].size;
	init_metadata(&meta_buffer);

	/* MMAP MCDI Buffer (Cached) */
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_MCDI_INDEX,
		&pqdev->DDBuf[MMAP_MCDI_INDEX].pa, &pqdev->DDBuf[MMAP_MCDI_INDEX].size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to _mtk_pq_get_memory_info(index=%d)\n", MMAP_MCDI_INDEX);
		_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
		return ret;
	}
	_mtk_pq_probe_memremap(pqdev, &pqdev->DDBuf[MMAP_MCDI_INDEX], MEMREMAP_WB);

	memset((void *)pqdev->DDBuf[MMAP_MCDI_INDEX].va,
	0, pqdev->DDBuf[MMAP_MCDI_INDEX].size);

	/* MMAP CFD Buffer (Cached) */
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_CFD_BUF_INDEX,
		&pqdev->DDBuf[MMAP_CFD_BUF_INDEX].pa, &pqdev->DDBuf[MMAP_CFD_BUF_INDEX].size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to _mtk_pq_get_memory_info(index=%d)\n", MMAP_CFD_BUF_INDEX);
		_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
		return ret;
	}
	if (pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va == NULL)
		_mtk_pq_probe_memremap(pqdev, &pqdev->DDBuf[MMAP_CFD_BUF_INDEX], MEMREMAP_WB);

	memset((void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va,
	0, pqdev->DDBuf[MMAP_CFD_BUF_INDEX].size);

	/* MMAP STREAM META Buffer (Cached) */
	ret = mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_STREAM_METADATA_INDEX,
					&stream_pa, &stream_size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"Failed to mtk_pq_get_dedicate_memory_info(index=%d)\n",
		MMAP_STREAM_METADATA_INDEX);
		return ret;
	}
	stream_offset = stream_size / win_num;
	stream_offset = ALIGN_DOWNTO_16(stream_offset);
	for (cnt = 0; cnt < win_num; cnt++) {
		pq_dev = pqdev->pq_devs[cnt];
		if (pq_dev) {
			pq_dev->pqu_stream_dma_info.pa = stream_pa + stream_offset*cnt;
			pq_dev->pqu_stream_dma_info.size = stream_offset;
			if (pq_dev->pqu_stream_dma_info.va == NULL) {
				pq_dev->pqu_stream_dma_info.va =
					(__u64)memremap(pq_dev->pqu_stream_dma_info.pa,
					pq_dev->pqu_stream_dma_info.size, MEMREMAP_WC);
				if (pq_dev->pqu_stream_dma_info.va == NULL) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"memremap fail, cnt=%d\n", cnt);
					_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
					return -ENOMEM;
				}
			}
		}
	}

	return 0;
}

static int _mtk_pq_probe_basic_setup(
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;

	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	ret = mtk_pq_common_init(pqdev);
	if (ret) {
		PQ_MSG_ERROR("mtk_pq_common_init failed\r\n");
		return ret;
	}

	ret = mtk_pq_common_clk_setup(pqdev, true);
	if (ret) {
		PQ_MSG_ERROR("Failed to init clock\r\n");
		return ret;
	}

	ret = _mtk_pq_config_open(pqdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to parse pq config dts\r\n");
		return ret;
	}

	ret = mtk_pq_buffer_reserve_buf_init(pqdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to mtk_pq_buffer_reserve_buf_init\r\n");
		return ret;
	}

	ret = pq_init_subdevices(pqdev);
	if (ret != 0) {
		PQ_MSG_ERROR("Failed to pq_init_subdevices\r\n");
		return ret;
	}

	ret = mtk_pq_common_irq_init(pqdev);
	if (ret != 0) {
		PQ_MSG_ERROR("Failed to init irq\r\n");
		return ret;
	}

	ret = mtk_display_dynamic_ultra_init(pqdev);
	if (ret != 0) {
		PQ_MSG_ERROR("Failed to init dynamic_ultra\r\n");
		return ret;
	}

	ret = _mtk_pq_cfd_init(pqdev);
	if (ret < 0)
		PQ_MSG_ERROR("_mtk_pq_cfd_init return %d\n", ret);

	return 0;
}
static int _mtk_pq_probe_parse_dts(
	struct platform_device *pdev,
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;

	if (pdev == NULL || pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}

	ret = _mtk_pq_parse_dts(pqdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to parse pq cap dts\r\n");
		return ret;
	}

	ret = mtk_pq_common_capability_parse(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = mtk_pq_common_clk_parse_dts(pqdev);
	if (ret == -EINVAL)
		return ret;

	ret = mtk_pq_enhance_parse_dts(&pqdev->pq_enhance, pdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to parse pq enhance dts\r\n");
		return ret;
	}

	ret = mtk_display_parse_dts(&pqdev->display_dev, pqdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to parse display dts\r\n");
		return ret;
	}

	ret = mtk_b2r_parse_dts(&pqdev->b2r_dev, pdev);
	if (ret) {
		PQ_MSG_ERROR("Failed to parse b2r dts\r\n");
		return ret;
	}

	return ret;
}

static int _mtk_pq_probe(struct platform_device *pdev)
{
	struct mtk_pq_device *pq_dev = NULL;
	int ret = 0;
	__u8 win_num = 0, cnt = 0;

	boottime_print("MTK PQ insmod [begin]\n");
	pqdev = devm_kzalloc(&pdev->dev,
		sizeof(struct mtk_pq_platform_device), GFP_KERNEL);

	if (IS_ERR_OR_NULL(pqdev))
	{
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev Pointer is NULL!\n");
		return -ENOMEM;
	}
	pqdev->display_dev.pReserveBufTbl = NULL;
	pqdev->dev = &pdev->dev;

	/* Parse DTS */
	ret = _mtk_pq_probe_parse_dts(pdev, pqdev);
	if (ret) {
		PQ_MSG_ERROR("_mtk_pq_probe_parse_dts fail!\n");
		return ret;
	}

	/* Setup driver which only need pqdev */
	ret = _mtk_pq_probe_basic_setup(pqdev);
	if (ret) {
		PQ_MSG_ERROR("_mtk_pq_probe_basic_setup fail!\n");
		return ret;
	}

	ret = mtk_pq_cdev_probe(pdev);
	if (ret != 0)
		PQ_MSG_ERROR("Failed to cdev_probe\r\n");

	win_num = pqdev->usable_win;
	if (win_num > PQ_WIN_MAX_NUM) {
		PQ_MSG_ERROR("DTS error, can not create too many devices\r\n");
		win_num = PQ_WIN_MAX_NUM;
	}

	for (cnt = 0; cnt < win_num; cnt++) {
		pq_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct mtk_pq_device), GFP_KERNEL);
		pqdev->pq_devs[cnt] = pq_dev;
		if (!pq_dev) {
			ret = -ENOMEM;
			goto exit;
		}

		/* for kernel 5.4.1 */
		pqdev->pq_devs[cnt]->video_dev.device_caps = V4L2_CAP_VIDEO_M2M_MPLANE |
			V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_CAPTURE;

		ret = _mtk_pq_register_v4l2dev(pq_dev, cnt, pqdev);
		if (ret) {
			PQ_MSG_ERROR("Failed to register v4l2 dev\r\n");
			goto exit;
		}

		pq_dev->stream_dma_info.dev = pq_dev->dev;
		pq_dev->memory_bus_base = 0x20000000;//fix me
		pq_dev->display_info.idr.input_mode = MTK_PQ_INPUT_MODE_HW;
		pq_dev->debug_calc_num = -1;
		memcpy(&pq_dev->b2r_dev, &pqdev->b2r_dev, sizeof(struct b2r_device));
		init_waitqueue_head(&(pq_dev->wait));
	}

	/* Prepare dedicated buffer */
	ret = _mtk_pq_probe_mmap_dedicated_buffer(pqdev);
	if (ret) {
		PQ_MSG_ERROR("_mtk_pq_probe_mmap_dedicated_buffer fail!\n");
		goto exit;
	}

	/* dolby init process start */
	ret = _mtk_pq_dv_init(pqdev);
	if (ret < 0)
		PQ_MSG_ERROR("_mtk_pq_dv_init return %d\n", ret);
	/* dolby init process end */

	// get filmmaker mode(FMM) capability
	if (!mtk_hashkey_check_IP(IPAUTH_VIDEO_FMM)) {
		PQ_MSG_INFO("get FMM hash key not support!\n");
		pqdev->pqcaps.u8FMM_Hash_Support &= ~(BIT(MTK_PQ_HASH_FMM));
	} else {
		PQ_MSG_INFO("get FMM hash key support!\n");
		pqdev->pqcaps.u8FMM_Hash_Support |= BIT(MTK_PQ_HASH_FMM);
	}

	// equal to dev_set_drvdata(&pdev->dev, pqdev);
	// you can get "pqdev" by platform_get_drvdata(pdev)
	// or dev_get_drvdata(&pdev->dev)
	platform_set_drvdata(pdev, pqdev);

	mtk_pq_common_trigger_gen_init(false);

	/* In order to disable b2r clock */
	ret = mtk_pq_b2r_init(pq_dev);
	if (ret < 0)
		PQ_MSG_INFO("Failed to mtk_pq_b2r_init\n");

	ret = mtk_pq_b2r_exit(pq_dev);
	if (ret < 0)
		PQ_MSG_INFO("Failed to mtk_pq_b2r_exit\n");

	mtk_pq_CreateSysFS(&pdev->dev);

	boottime_print("MTK PQ insmod [end]\n");

	ret = mtk_pq_buffer_frc_pq_allocate(pqdev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_buffer_frc_pq_allocate fail! ret=%d\n", ret);
		return ret;
	}

	return 0;

exit:
	for (cnt = 0; cnt < win_num; cnt++) {
		pq_dev = pqdev->pq_devs[cnt];
		if (pq_dev)
			_mtk_pq_unregister_v4l2dev(pq_dev);
	}
	_mtk_pq_probe_unmap_dedicated_buffer(pqdev);
	return ret;
}

static int _mtk_pq_remove(struct platform_device *pdev)
{
	struct mtk_pq_platform_device *pqdev = platform_get_drvdata(pdev);
	struct mtk_pq_device *pq_dev = NULL;
	__u8 win_num = 0;
	__u8 cnt = 0;

	if (IS_ERR_OR_NULL(pqdev))
	{
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev Pointer is NULL!\n");
		return -EINVAL;
	}

	win_num = pqdev->usable_win;

	mtk_pq_buffer_reserve_buf_exit(pqdev);

	mtk_pq_cdev_remove(pdev);

	mtk_pq_RemoveSysFS(&pdev->dev);
	for (cnt = 0; cnt < win_num; cnt++) {
		pq_dev = pqdev->pq_devs[cnt];
		if (pq_dev) {
			_mtk_pq_unregister_v4l2dev(pq_dev);

			if (pq_dev->pqu_stream_dma_info.va != NULL) {
				memset(&pq_dev->pqu_stream_dma_info, 0,
						sizeof(struct mtk_pq_remap_buf));
				memunmap((void *)pq_dev->pqu_stream_dma_info.va);
				pq_dev->pqu_stream_dma_info.va = NULL;
			}
		}
	}
	if (pqdev->hwmap_config.va != NULL) {
		memset((void *)pqdev->hwmap_config.va, 0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->hwmap_config.va);
		pqdev->hwmap_config.va = NULL;
	}
	if (pqdev->dv_config.va != NULL) {
		memset((void *)pqdev->dv_config.va, 0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->dv_config.va);
		pqdev->dv_config.va = NULL;
	}
	if (pqdev->DDBuf[MMAP_PQMAP_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].va);
		pqdev->DDBuf[MMAP_PQMAP_INDEX].va = NULL;
	}
	if (pqdev->DDBuf[MMAP_PQPARAM_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va);
		pqdev->DDBuf[MMAP_PQPARAM_INDEX].va = NULL;
	}
	if (pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va != NULL) {
		memset((void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va,
			0, sizeof(struct mtk_pq_remap_buf));
		memunmap((void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va);
		pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va = NULL;
	}

	return 0;
}

static int _mtk_pq_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	mtk_pq_common_irq_suspend(false);

	return 0;
}

static int _mtk_pq_resume(struct platform_device *pdev)
{
	mtk_pq_common_irq_resume(false, pqdev->pqcaps.u32IRQ_Version);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pq_pm_runtime_force_suspend(struct device *dev)
{
	int ret = 0;
	int win_num = 0;
	struct mtk_pq_platform_device *pdev = dev_get_drvdata(dev);

	if (bPquEnable) {
		for (win_num = 0; win_num < PQ_WIN_MAX_NUM; win_num++) {
			if (pdev->pq_devs[win_num] == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"pdev->pq_devs[%d] = NULL!!\r\n", win_num);
				continue;
			}
			if (pdev->pq_devs[win_num]->stream_on_ref > 0)
				pdev->pq_devs[win_num]->bInvalid = true;
			pdev->pq_devs[win_num]->bSetPath = false;
		}
		pqu_video_shuttle_deinit();
	}

	ret = mtk_pq_common_suspend(false);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to suspend pq\r\n");

	mtk_pq_common_clk_setup(pdev, false);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to disable clock\r\n");

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "%s\n", __func__);

	return pm_runtime_force_suspend(dev);
}

static int pq_pm_runtime_force_resume(struct device *dev)
{
	int ret = 0;
	int win_num = 0;

	struct mtk_pq_platform_device *pdev = dev_get_drvdata(dev);

	ret = mtk_pq_common_clk_setup(pdev, true);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to enable clock\r\n");

	ret = _mtk_pq_config_open(pdev);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to parse pq config dts\r\n");

	ret = mtk_pq_common_init(pdev);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to resume common pq init\r\n");

	ret = mtk_pq_common_resume(false, pdev->pqcaps.u32IRQ_Version);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to resume pq\r\n");

	ret = mtk_pqd_resumeSetPQMapInfo();
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to resume set pq map\r\n");

	for (win_num = 0; win_num < PQ_WIN_MAX_NUM; win_num++) {
		ret = mtk_pq_b2r_init(pdev->pq_devs[win_num]);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Failed to b2r init win=%d in resume\r\n", win_num);
			break;
		}
	}

	ret = mtk_display_dynamic_ultra_init(pdev);
	if (ret != 0) {
		PQ_MSG_ERROR("Failed to init dynamic_ultra\r\n");
		return ret;
	}

	if (pdev->pq_devs[0]->pattern_info.enable)
		for (win_num = 0; win_num < PQ_PAT_WIN; win_num++)
			mtk_pq_pattern_init(pdev->pq_devs[win_num]);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "%s\n", __func__);

	return pm_runtime_force_resume(dev);
}

static const struct dev_pm_ops pq_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		pq_pm_runtime_force_suspend,
		pq_pm_runtime_force_resume
	)
};
#endif

static struct platform_driver pq_pdrv = {
	.probe      = _mtk_pq_probe,
	.remove     = _mtk_pq_remove,
	.suspend    = _mtk_pq_suspend,
	.resume     = _mtk_pq_resume,
	.driver     = {
		.name = "mtk-pq",
		.of_match_table = mtk_pq_match,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &pq_pm_ops,
#endif
	},
};

static int __init _mtk_pq_init(void)
{
	boottime_print("MTK PQ init [begin]\n");
	platform_driver_register(&pq_pdrv);
	boottime_print("MTK PQ init [end]\n");
	return 0;
}

static void __exit _mtk_pq_exit(void)
{
	platform_driver_unregister(&pq_pdrv);
}

module_param_named(pqu_enable, bPquEnable, bool, 0660);
module_init(_mtk_pq_init);
module_exit(_mtk_pq_exit);

MODULE_AUTHOR("Kevin Ren <kevin.ren@mediatek.com>");
MODULE_DESCRIPTION("mtk pq device driver");
MODULE_LICENSE("GPL v2");

