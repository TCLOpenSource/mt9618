/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_PQU_WRAPPER_H_
#define _MTK_TV_DRM_PQU_WRAPPER_H_
#include "pqu_msg.h"
#include "ext_command_client_api.h"
#include "ext_command_render_if.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
#define MTK_TV_DRM_PQU_WRAPPER(								\
		msg_id,		/* The value of @enum PQU_MSG_SEND_ID */		\
		param_ptr_lite,	/* The parameter pointer of @msg_id on PQU-list */	\
		param_ptr_rv55)	/* The parameter pointer of @msg_id on PQU-rv55 */	\
	mtk_tv_drm_pqu_wrapper_##msg_id(param_ptr_lite, sizeof(*(param_ptr_lite)),		\
					param_ptr_rv55, sizeof(*(param_ptr_rv55)))

#define PQU_WRAPPER_HEADER(msg_id)	\
	int mtk_tv_drm_pqu_wrapper_##msg_id(void *, uint32_t, void *, uint32_t)

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
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_PQ_FIRE);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_PQSHAREMEM_INFO);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_FRAMEINFO_SHAREMEM);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_BACKLIGHT);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_DEMURA);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_BE_SHAREMEM_INFO);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_BE_SETTING);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_ALGO_CTX_INIT);
PQU_WRAPPER_HEADER(PQU_MSG_SEND_RENDER_OS_WRAPPER_INIT);

#endif //_MTK_TV_DRM_PQU_WRAPPER_H_
