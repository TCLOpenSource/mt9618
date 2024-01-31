/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_PQU_AGENT_H_
#define _MTK_TV_DRM_PQU_AGENT_H_
#include <drm/drm_device.h>
#include "pqu_render.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
typedef void (*mtk_tv_pqu_render_reply_cb)(
		void *priv_data,
		struct pqu_render_agent_param *reply_param);

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------
extern bool bPquEnable; // is pqurv55 enable ? declare at mtk_tv_drm_drv.c.

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_pqu_agent_no_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input);

int mtk_tv_drm_pqu_agent_sync_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input,
	struct pqu_render_agent_param *output);

int mtk_tv_drm_pqu_agent_async_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input,
	mtk_tv_pqu_render_reply_cb callback,
	void *priv_data);

int mtk_tv_drm_pqu_get_dv_pwm_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

#endif //_MTK_TV_DRM_PQU_WRAPPER_H_
