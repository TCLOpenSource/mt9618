/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */


#ifndef _MTK_TCON_LINEOD_H_
#define _MTK_TCON_LINEOD_H_

#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_common.h"

bool mtk_tcon_lineod_setting(struct mtk_panel_context *pCon);
int mtk_tcon_lineod_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val);
int mtk_tcon_lineod_get_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize);
#endif
