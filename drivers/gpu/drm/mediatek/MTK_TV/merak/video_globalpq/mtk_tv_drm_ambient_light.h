/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_AMBLIGHT_H_
#define _MTK_TV_DRM_AMBLIGHT_H_

#include <linux/dma-buf.h>

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
void mtk_video_display_set_ambient_framesize(
	struct mtk_video_context *pctx_video, bool enable);

void mtk_video_display_ambient_AUL_init(
	struct mtk_tv_kms_context *pctx);

int mtk_video_ambientlight_get_data(
	struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ambientlight_data *curve);

int mtk_tv_drm_get_ambientlight_data_ioctl(struct drm_device *dev, void *data,
						struct drm_file *file_priv);

#endif
