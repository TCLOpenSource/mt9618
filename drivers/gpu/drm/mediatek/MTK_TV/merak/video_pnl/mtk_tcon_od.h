/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 * MediaTek tcon overdrive driver
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_TCON_OD_H_
#define _MTK_TCON_OD_H_

#define UFO_XC_AUTO_DOWNLOAD
#define SUPPORT_OVERDRIVE                   1
#define ENABLE_OD_AUTODOWNLOAD             (TRUE)

#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_common.h"

enum EN_TCON_OD_VAL_TYPE {
	E_TCON_OD_VAL_FORCE_DISABLE = 0,
	E_TCON_OD_VAL_FORCE_ENABLE,
	E_TCON_OD_VAL_OFF,
	E_TCON_OD_VAL_ON,
	E_TCON_OD_VAL_MAX
};

extern int mtk_miu2gmc_mask(u32 miu2gmc_id, u32 port_id, bool enable);

bool mtk_tcon_od_setting(struct mtk_panel_context *pCon, uint32_t u32OdModeType);
bool mtk_tcon_vrr_od_setting(struct mtk_panel_context *pCon,
	unsigned char uTableIdx, bool bApply, uint32_t u32OdModeType);
int mtk_tcon_od_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType,
	const uint16_t u16Val, uint32_t u32OdModeType);
int mtk_tcon_od_get_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize);
#endif
