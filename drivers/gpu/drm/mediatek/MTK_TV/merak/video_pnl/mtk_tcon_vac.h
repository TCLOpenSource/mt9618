/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 * MediaTek Panel driver
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_TCON_VAC_H_
#define _MTK_TCON_VAC_H_

#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_common.h"

#define SUPPORT_VAC_256                     1
#define ENABLE_VAC_AUTODOWNLOAD            (TRUE)

#define UFO_XC_AUTO_DOWNLOAD

bool mtk_tcon_vac_setting(void);
bool mtk_tcon_vac_reg_setting(uint8_t *pu8TconTab, uint16_t u16RegisterCount);
int mtk_tcon_vac_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val);
int mtk_tcon_vac_get_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize);
#endif
