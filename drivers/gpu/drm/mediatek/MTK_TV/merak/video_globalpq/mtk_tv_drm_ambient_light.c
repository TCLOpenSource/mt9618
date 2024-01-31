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

#include "drv_scriptmgt.h"
#include "mtk_tv_drm_ambient_light.h"
#include "mtk-efuse.h"
#include "hwreg_render_video_globalpq.h"
#include "hwreg_render_stub.h"


#define AMBIENT_WIDTH_NUM          (32)
#define AMBIENT_HEIGHT_NUM         (20)


void mtk_video_display_set_ambient_framesize(
	struct mtk_video_context *pctx_video, bool enable)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_DISP_WIN];
	struct hwregAmbientFrameInfo ambientParamIn;
	struct hwregOut paramOut;

	pctx = pctx_video_to_kms(pctx_video);
	pctx_pnl = &(pctx->panel_priv);

	memset(reg, 0, sizeof(reg));
	memset(&ambientParamIn, 0, sizeof(struct hwregAmbientFrameInfo));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	if (!pctx->hw_caps.support_amb_light)
		return;
	paramOut.reg = reg;

	ambientParamIn.RIU = TRUE;
	ambientParamIn.ambientType = AMBIENT_TYPE_VIDEO;
	ambientParamIn.enable = enable;
	ambientParamIn.framewidth = pctx_pnl->info.de_width;
	ambientParamIn.frameheight = pctx_pnl->info.de_height;
	ambientParamIn.blockwidth = pctx_pnl->info.de_width / AMBIENT_WIDTH_NUM;
	ambientParamIn.blockheight = pctx_pnl->info.de_height / AMBIENT_HEIGHT_NUM;

	drv_hwreg_render_ambient_set_frameinfo(ambientParamIn, &paramOut);
}


void mtk_video_display_ambient_AUL_init(
	struct mtk_tv_kms_context *pctx)
{
	int fd = 0;
	struct sm_aul_info aul_info_amb;
	__u64 misc = E_SM_AUL_AMB;
	__u8 mem_index = 0;

	memset(&aul_info_amb, 0, sizeof(struct sm_aul_info));
	if (pctx->hw_caps.support_amb_light) {
		if (sm_aul_create_resource(&fd, misc) != E_SM_RET_OK) {
			DRM_ERROR("can not create instance!");
			return;
		}

		pctx->amb_context.amb_aul_instance = fd;
		pctx->amb_context.ambient_light_init = TRUE;

		//2.get memory index
		sm_aul_get_mem_index(fd, &mem_index);
	}

}

int mtk_video_ambientlight_get_data(
	struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ambientlight_data *data)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct sm_aul_data_info data_info;
	struct sm_aul_data_size data_size;
	__u8 mem_index = 0;
	__u8 *amb_data;
	struct mtk_tv_drm_metabuf metabuf;
	int ret = 0;
	int aul_fd = 0;

	if (!mtk_tv_crtc || !data) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, data=%p\n",
			__func__, __LINE__, mtk_tv_crtc, data);
		return -EINVAL;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (!pctx->hw_caps.support_amb_light) {
		DRM_WARN("[%s][%d] Not support ambientlight\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drv_STUB("AMB_DMA_FD",   data->amb_dma_fd);

	if (pctx->amb_context.ambient_light_init == TRUE) {
		aul_fd = pctx->amb_context.amb_aul_instance;
		if (aul_fd <= 0) {
			DRM_ERROR("AMB AUL fd is invalid\n");
			return -EINVAL;
		}
	} else {
		DRM_ERROR("AMB not init ready\n");
		return -EINVAL;
	}

	// convert ION to metabuf
	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_ion(&metabuf, data->amb_dma_fd)) {
		DRM_ERROR("mtk_tv_drm_metabuf_alloc_by_ion(%d) fail", data->amb_dma_fd);
		ret = -EPERM;
		goto EXIT;
	}

	memset(&data_info, 0, sizeof(struct sm_aul_data_info));
	memset(&data_size, 0, sizeof(struct sm_aul_data_size));

	amb_data = kvmalloc(sizeof(__u8)*AUL_AMBIENT_SIZE, GFP_KERNEL);
	if (amb_data == NULL) {
		DRM_ERROR("kmalloc(%u) failed", AUL_AMBIENT_SIZE);
		ret = -ENOMEM;
		goto EXIT;
	}
	//2.get memory index
	sm_aul_get_mem_index(aul_fd, &mem_index);
	// get data size
	data_size.aul_client = E_SM_AUL_AMB;
	sm_aul_get_data_size(aul_fd, &data_size);

	//3.get aul cleint data
	if (data_size.data_size > 0) {
		data_info.aul_client = E_SM_AUL_AMB;
		data_info.data_size = data_size.data_size;
		data_info.mem_index = mem_index;
		data_info.data = amb_data;
		sm_aul_get_data(aul_fd, &data_info);
	}

	memcpy(metabuf.addr, amb_data, sizeof(__u8)*AUL_AMBIENT_SIZE);


	kvfree(amb_data);
EXIT:
	mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}

int mtk_tv_drm_get_ambientlight_data_ioctl(struct drm_device *dev, void *data,
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

	ret = mtk_video_ambientlight_get_data(mtk_tv_crtc,
		(struct drm_mtk_tv_ambientlight_data *)data);

	return ret;
}

