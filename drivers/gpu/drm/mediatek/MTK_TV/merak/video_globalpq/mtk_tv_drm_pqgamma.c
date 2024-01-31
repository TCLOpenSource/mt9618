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
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "../mtk_tv_drm_plane.h"
#include "../mtk_tv_drm_kms.h"
#include "mtk_tv_drm_common.h"
#include "../mtk_tv_drm_log.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "drv_scriptmgt.h"
#include "pqu_msg.h"
#include "m_pqu_render.h"

#include "mtk_tv_drm_pqgamma.h"
#include "ext_command_render_if.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_PNLGAMMA

int mtk_video_pqgamma_set(
	struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_curve *curve)
{
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !curve) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p,  curve=%p\n",
			__func__, __LINE__, mtk_tv_crtc, curve);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_curve_size = curve->size;
	pqu_gamma_data->pqgamma.pqgamma_version = pctx->globalpq_hw_ver.pqgamma_version;

	DRM_INFO("[%s, %d] pqgamma_curve_r[size-1] %d, pqgamma_curve_g[size-1] %d\n",
		__func__, __LINE__, curve->curve_r[curve->size-1], curve->curve_g[curve->size-1]);
	DRM_INFO("pqgamma_curve_b[size-1] %d\n", curve->curve_b[curve->size-1]);

	memcpy(pqu_gamma_data->pqgamma.pqgamma_curve_r, curve->curve_r,
		sizeof(uint16_t)*curve->size);
	memcpy(pqu_gamma_data->pqgamma.pqgamma_curve_g, curve->curve_g,
		sizeof(uint16_t)*curve->size);
	memcpy(pqu_gamma_data->pqgamma.pqgamma_curve_b, curve->curve_b,
		sizeof(uint16_t)*curve->size);

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_curve = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_curve = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_video_pqgamma_enable(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_enable *pqgamma_enable)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !pqgamma_enable) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, pqgamma_enable=%p\n",
			__func__, __LINE__, mtk_tv_crtc, pqgamma_enable);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_enable = pqgamma_enable->enable;

	DRM_INFO("[%s, %d] pqgamma_enable %d\n",
		__func__, __LINE__, pqu_gamma_data->pqgamma.pqgamma_enable);

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_video_pqgamma_gainoffset(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_gainoffset *gainoffset)
{
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !gainoffset) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, gainoffset=%p\n",
			__func__, __LINE__, mtk_tv_crtc, gainoffset);
		return -EINVAL;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	DRM_INFO("[%s, %d] pqgamma_offset_r 0x%x, pqgamma_offset_g 0x%x, pqgamma_offset_b 0x%x\n",
		__func__, __LINE__, gainoffset->pqgamma_offset_r, gainoffset->pqgamma_offset_g,
		gainoffset->pqgamma_offset_b);

	DRM_INFO("pqgamma_gain_r 0x%x, pqgamma_gain_g 0x%x, pqgamma_gain_b 0x%x\n",
		gainoffset->pqgamma_gain_r, gainoffset->pqgamma_gain_g,
		gainoffset->pqgamma_gain_b);

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_offset_r = gainoffset->pqgamma_offset_r;
	pqu_gamma_data->pqgamma.pqgamma_offset_g = gainoffset->pqgamma_offset_g;
	pqu_gamma_data->pqgamma.pqgamma_offset_b = gainoffset->pqgamma_offset_b;
	pqu_gamma_data->pqgamma.pqgamma_gain_r = gainoffset->pqgamma_gain_r;
	pqu_gamma_data->pqgamma.pqgamma_gain_g = gainoffset->pqgamma_gain_g;
	pqu_gamma_data->pqgamma.pqgamma_gain_b = gainoffset->pqgamma_gain_b;
	pqu_gamma_data->pqgamma.pqgamma_pregainoffset  = gainoffset->pregainoffset;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_gainoffset = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_gainoffset = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_video_pqgamma_gainoffset_enable(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_gainoffset_enable *gainoffsetenable)
{
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !gainoffsetenable) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, gainoffsetenable=%p\n",
			__func__, __LINE__, mtk_tv_crtc, gainoffsetenable);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_gainoffset_enbale = gainoffsetenable->enable;
	pqu_gamma_data->pqgamma.pqgamma_pregainoffset = gainoffsetenable->pregainoffset;

	DRM_INFO("[%s, %d] gainoffsetenable %x, pregainoffset %x\n",
		__func__, __LINE__, gainoffsetenable->enable, gainoffsetenable->pregainoffset);

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_gainoffset_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_gainoffset_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;
}

int mtk_video_pqgamma_set_maxvalue(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_maxvalue *maxvalue)
{
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !maxvalue) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, gainoffset=%p\n",
			__func__, __LINE__, mtk_tv_crtc, maxvalue);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_max_r = maxvalue->pqgamma_max_r;
	pqu_gamma_data->pqgamma.pqgamma_max_g = maxvalue->pqgamma_max_g;
	pqu_gamma_data->pqgamma.pqgamma_max_b = maxvalue->pqgamma_max_b;

	DRM_INFO("[%s, %d] pqgamma_max_r 0x%x, pqgamma_max_g 0x%x, pqgamma_max_b 0x%x\n",
		__func__, __LINE__, maxvalue->pqgamma_max_r, maxvalue->pqgamma_max_g,
		maxvalue->pqgamma_max_b);

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_maxvalue = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_maxvalue = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_video_pqgamma_maxvalue_enable(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_pqgamma_maxvalue_enable *maxvalue_enable)
{
	int ret = 0;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;

	if (!mtk_tv_crtc || !maxvalue_enable) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, gainoffset=%p\n",
			__func__, __LINE__, mtk_tv_crtc, maxvalue_enable);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_maxvalue_enable = maxvalue_enable->enable;

	DRM_INFO("[%s, %d] pqgamma_maxvalue_enable %x\n",
		__func__, __LINE__, maxvalue_enable->enable);

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_maxvalue_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_maxvalue_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_video_pqgamma_resume(struct mtk_tv_kms_context *pctx)
{
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;

	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_enable = TRUE;
	updateInfo.pqgamma_curve = TRUE;
	updateInfo.pqgamma_gainoffset = TRUE;
	updateInfo.pqgamma_gainoffset_enable = TRUE;
	updateInfo.pqgamma_maxvalue = TRUE;
	updateInfo.pqgamma_maxvalue_enable = TRUE;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_enable = TRUE;
	pqupdate.pqgamma_curve = TRUE;
	pqupdate.pqgamma_gainoffset = TRUE;
	pqupdate.pqgamma_gainoffset_enable = TRUE;
	pqupdate.pqgamma_maxvalue = TRUE;
	pqupdate.pqgamma_maxvalue_enable = TRUE;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);
	mtk_tv_drm_metabuf_free(&metabuf);
	return 0;
}
