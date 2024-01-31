// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_pqmap.h"
#include "mtk_tv_drm_autogen.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_sm_ml.h"
#include "hwreg_render_stub.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_PQMAP
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static char _mtk_render_pqmap_pim_callback(
	int tag,
	const char *data,
	const size_t datalen,
	void *autogen)
{
	reg_func *regfunc = (reg_func *)data;
	uint32_t reg_idx;
	uint32_t reg_value;
	int ret = true;
	int i;

	if (!regfunc) {
		ERR("regfunc = NULL");
		return false;
	}
	if (tag == EXTMEM_TAG_REGISTER_SETTINGS) {
		// LOG("NTS_HWREG num = %u", regfunc->padded_data.fn_num);
		for (i = 0; i < regfunc->padded_data.fn_num; ++i) {
			reg_idx = regfunc->fn_tbl[i].fn_idx;
			reg_value = regfunc->fn_tbl[i].fn_para;
			// LOG("  NTS_HWREG[0x%x]:%u", reg_idx, reg_value);
			if (mtk_tv_drm_autogen_set_nts_hw_reg(autogen, reg_idx, reg_value)) {
				ERR("set NTS_HW reg(0x%x, 0x%x) error", reg_idx, reg_value);
				ret = false;
			}
		}
	} else if (tag == EXTMEM_TAG_VREGISTER_SETTINGS) {
		// LOG("VRREG num = %u", regfunc->padded_data.fn_num);
		for (i = 0; i < regfunc->padded_data.fn_num; ++i) {
			reg_idx = regfunc->fn_tbl[i].fn_idx;
			reg_value = regfunc->fn_tbl[i].fn_para;
			// LOG("  SWREG[0x%x]:%u", reg_idx, reg_value);
			if (mtk_tv_drm_autogen_set_sw_reg(autogen, reg_idx, reg_value)) {
				ERR("set SW reg(0x%x, 0x%x) error", reg_idx, reg_value);
				ret = false;
			}
		}
	} else {
		ret = false;
	}
	return ret;
}

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_pqmap_init(
	struct mtk_pqmap_context *ctx,
	uint8_t pqmap_idx,
	struct drm_mtk_tv_pqmap_param *param)
{
	struct mtk_pqmap_info *pqmap = NULL;
	struct mtk_tv_drm_metabuf metabuf = {0};
	uint8_t i;
	int ret = -EINVAL;

	if (ctx == NULL || param == NULL) {
		ERR("Invalid input, ctx: 0x%p, param: 0x%p", ctx, param);
		return -EINVAL;
	}
	mtk_tv_drm_pqmap_deinit(ctx);
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i) {
		pqmap = &ctx->pqmap[i];
		LOG("qmap %d", i);
		LOG("  pim fd   = %d", param->pim_dma_fd[i]);
		LOG("  pim size = %d", param->pim_size[i]);
		drv_STUB("PQMAP_PIM_SIZE", param->pim_size[i]);
		drv_STUB("PQMAP_PIM_FD",   param->pim_dma_fd[i]);

		// check pim size
		if (param->pim_size[i] == 0) {
			LOG("do nothing, pim %d size = %u", i, param->pim_size[i]);
			continue;
		}

		// convert ION to metabuf
		memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
		if (mtk_tv_drm_metabuf_alloc_by_ion(&metabuf, param->pim_dma_fd[i])) {
			ERR("mtk_tv_drm_metabuf_alloc_by_ion(%d) fail", param->pim_dma_fd[i]);
			ret = -EPERM;
			continue;
		}
		if (param->pim_size[i] > metabuf.size) {
			ERR("ION size error (pim %u > dma %u)", param->pim_size[i], metabuf.size);
			mtk_tv_drm_metabuf_free(&metabuf);
			ret = -EPERM;
			continue;
		}

		// store pim
		pqmap->pim_va = kvmalloc(param->pim_size[i], GFP_KERNEL);
		if (pqmap->pim_va == NULL) {
			ERR("kmalloc(%u) failed", param->pim_size[i]);
			mtk_tv_drm_metabuf_free(&metabuf);
			ret = -EPERM;
			continue;
		}
		memcpy(pqmap->pim_va, metabuf.addr, param->pim_size[i]);
		pqmap->pim_size = param->pim_size[i];
		mtk_tv_drm_metabuf_free(&metabuf);

		// init pqmap
		if (!pqmap_pim_init(&pqmap->pim_handle, pqmap->pim_va, pqmap->pim_size)) {
			ERR("render PIM init failed");
			kvfree(pqmap->pim_va);
			pqmap->pim_va = NULL;
			pqmap->pim_size = 0;
			ret = -EPERM;
			continue;
		}

		pqmap->init = true;
		LOG("pqmap %d init done", i);
	}
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i) {
		// if anyone is inited successfully
		if (ctx->pqmap[i].init == true)
			return 0;
	}
	return ret;
}

int mtk_tv_drm_pqmap_deinit(
	struct mtk_pqmap_context *ctx)
{
	struct mtk_pqmap_info *pqmap = NULL;
	uint8_t i;

	if (ctx == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i) {
		pqmap = &ctx->pqmap[i];
		if (pqmap->init == false)
			continue;
		if (pqmap->pim_handle)
			pqmap_pim_destroy(&pqmap->pim_handle);
		if (pqmap->pim_va != NULL)
			kvfree(pqmap->pim_va);
	}
	memset(ctx, 0, sizeof(struct mtk_pqmap_context));
	LOG("pqmap deinit done");
	return 0;
}

int mtk_tv_drm_pqmap_update(
	struct mtk_pqmap_context *pqmap_ctx,
	struct mtk_sm_ml_context *ml_ctx,
	struct drm_mtk_tv_pqmap_node_array *node_array)
{
	if (!pqmap_ctx || !ml_ctx || !node_array) {
		ERR("Invalid input");
		return -EINVAL;
	}
	return mtk_tv_drm_pqmap_write_ml(pqmap_ctx, ml_ctx,
		node_array->qmap_num, node_array->node_array);
}

int mtk_tv_drm_pqmap_write_ml(
	struct mtk_pqmap_context *pqmap_ctx,
	struct mtk_sm_ml_context *ml_ctx,
	uint16_t *node_num,
	uint16_t *node_buf)
{
	struct mtk_tv_kms_context *kms = NULL;
	struct mtk_autogen_context *autogen = NULL;

	struct mtk_pqmap_info *pqmap = NULL;
	void *ml_mem_start_addr = NULL;
	void *ml_mem_max_addr = NULL;
	uint32_t autogen_cmd_size = 0;
	uint32_t i, j, k = 0;
	int ret = 0;

	if (!pqmap_ctx || !ml_ctx || !node_num || !node_buf) {
		ERR("Invalid input");
		return -EINVAL;
	}
	kms = container_of(pqmap_ctx, struct mtk_tv_kms_context, pqmap_priv);
	autogen = &kms->autogen_priv;

	// get ml buffer from ML
	ret = mtk_tv_sm_ml_get_mem_info(ml_ctx, &ml_mem_start_addr, &ml_mem_max_addr);
	if (ret != 0) {
		ERR("ML get memory info error");
		return -EPERM;
	}

	ret = mtk_tv_drm_autogen_lock(autogen);
	if (ret) {
		mtk_tv_sm_ml_put_mem_info(ml_ctx, ml_mem_start_addr);
		ERR("Set ML memory info error");
		return ret;
	}

	// set ml buffer to Autogen
	ret = mtk_tv_drm_autogen_set_mem_info(autogen, ml_mem_start_addr, ml_mem_max_addr);
	if (ret != 0) {
		ERR("Set ML memory info error");
		ret = -EPERM;
		goto ERROR;
	}

	// foreach pqmap
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i) {
		pqmap = &pqmap_ctx->pqmap[i];

		// check does the pqmap be inited
		if (pqmap->init == false) {
			ERR("pqmap %d is not inited", i);
			ret = -EPERM;
			continue;
		}

		// skip if the pqmap is no node
		LOG("pqmap %d: node_num %u", i, node_num[i]);
		if (node_num[i] == 0)
			continue;

		// write cmds to ml buffer by pqmap
		drv_STUB("PQMAP_TRIG_NUM", node_num[i]);
		for (j = 0; j < node_num[i]; ++j) {
			LOG("  node[%d] = 0x%x", j, node_buf[k]);
			pqmap_pim_load_setting(pqmap->pim_handle, node_buf[k++], 0,
				_mtk_render_pqmap_pim_callback, autogen);
		}
	}

	// gen cmd number of AutoGen written
	ret = mtk_tv_drm_autogen_get_cmd_size(autogen, &autogen_cmd_size);
	if (ret != 0) {
		ERR("Get Autogen command size error");
		ret = -EPERM;
		goto ERROR;
	}

	// write cmds to ML
	ret = mtk_tv_sm_ml_set_mem_cmds(ml_ctx, ml_mem_start_addr, autogen_cmd_size);
	if (ret != 0) {
		ERR("Add ML command size error");
		ret = -EPERM;
	}
ERROR:
	mtk_tv_drm_autogen_unlock(autogen);
	mtk_tv_sm_ml_put_mem_info(ml_ctx, ml_mem_start_addr);
	return ret;
}

int mtk_tv_drm_pqmap_check_buffer(
	void *data,
	uint32_t size)
{
	struct drm_mtk_tv_pqmap_node_array *node_array = NULL;
	uint32_t expect_size = 0;
	uint32_t node_num = 0;
	uint32_t i;

	if (size < sizeof(struct drm_mtk_tv_pqmap_node_array)) {
		ERR("blob too small: size=%u, sturct=%zu",
			size, sizeof(struct drm_mtk_tv_pqmap_node_array));
		return -1;
	}
	node_array = (struct drm_mtk_tv_pqmap_node_array *)data;

	// sum of all qmap nodes
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i) {
		node_num += node_array->qmap_num[i];
		LOG("qmap %d num = %u", i, node_array->qmap_num[i]);
	}

	// expected buffer size
	expect_size = sizeof(struct drm_mtk_tv_pqmap_node_array);
	expect_size += sizeof(uint16_t) * node_array->node_num;

	// check condition
	LOG("node num     = %u", node_array->node_num);
	LOG("buffer size  = %u", size);
	LOG("expect size  = %u", expect_size);
	if (node_num != node_array->node_num || size != expect_size) {
		ERR("blob check error: size(%u != %u) or num(%u != %u)\n",
			size, expect_size, node_num, node_array->node_num);
		return -1;
	}
	return 0;
}

int mtk_tv_drm_pqmap_set_info_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_tv_kms_context *kms = NULL;

	// get kms
	drm_for_each_crtc(crtc, drm) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		mplane = mtk_tv_crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO];
		kms = (struct mtk_tv_kms_context *)mplane->crtc_private;
		break;
	}
	return mtk_tv_drm_pqmap_init(&kms->pqmap_priv, 0, (struct drm_mtk_tv_pqmap_param *)data);
}

int mtk_tv_drm_pqmap_get_version(
	struct mtk_pqmap_context *ctx,
	enum PQMAP_VERSION_TYPE_E qmap_ver,
	enum PQMAP_TYPE_E type,
	struct pqmap_version *output_version)
{
	void *pim_handle = NULL;
	struct pqmap_version version;

	if (!ctx || !output_version) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (IS_PQMAP_INVALID(type)) {
		ERR("Invalid type %d", type);
		return -EINVAL;
	}
	pim_handle = &ctx->pqmap[type];

	memset(&version, 0, sizeof(struct pqmap_version));
	switch (qmap_ver) {
	case EN_PQMAP_VERSION_TYPE_TOTAL_VER:
		pqmap_pim_get_version(pim_handle, &version);
		break;
	case EN_PQMAP_VERSION_TYPE_HWREG_VER:
		pqmap_pim_get_hwreg_version(pim_handle, &version);
		break;
	case EN_PQMAP_VERSION_TYPE_SWREG_VER:
		pqmap_pim_get_swreg_version(pim_handle, &version);
		break;
	default:
		ERR("Invalid version type %d\n", qmap_ver);
		return -EINVAL;
	}
	memcpy(output_version, &version, sizeof(struct pqmap_version));
	return 0;
}
