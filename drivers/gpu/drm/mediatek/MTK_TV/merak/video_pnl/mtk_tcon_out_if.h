/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */


#ifndef _MTK_TCON_OUT_IF_H_
#define _MTK_TCON_OUT_IF_H_

#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_common.h"

#define TCON_INFO_LEN	(2048)

typedef enum {
	E_TCON_PROP_MODULE_EN = 0,
	E_TCON_PROP_SSC_EN,
	E_TCON_PROP_SSC_FMODULATION,
	E_TCON_PROP_SSC_PERCENTAGE,
	E_TCON_PROP_SSC_BIN_CTRL,
	E_TCON_PROP_VCOM_SEL,
	E_TCON_PROP_DEMURA_EN,
	E_TCON_PROP_MAX
} drm_en_tcon_property_type;

int mtk_tcon_get_lanes_num_by_linktype(
			drm_pnl_link_if enLinktype, uint16_t *pu16Lanes);
bool mtk_tcon_update_panel_info(drm_st_pnl_info *pInfo);
int mtk_tcon_show_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize);
int mtk_tcon_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val);
bool mtk_tcon_preinit(struct mtk_panel_context *pCon);
bool mtk_tcon_init(struct mtk_panel_context *pCon);
bool mtk_tcon_deinit(struct mtk_panel_context *pCon);
bool mtk_tcon_enable(struct mtk_panel_context *pCon, bool bEn);
bool mtk_tcon_is_mode_change(struct mtk_panel_context *pCon);
bool mtk_tcon_is_enable(void);
int mtk_tcon_vac_enable(bool bEn);
int mtk_tcon_get_property(drm_en_tcon_property_type enType, uint32_t *pu32Val);
int mtk_tcon_set_pq_sharemem_context(struct mtk_panel_context *pCon);
bool mtk_vrr_tcon_init(struct mtk_panel_context *pCon, uint32_t u32OdModeType);
bool mtk_vrr_pnlgamma_init(struct mtk_panel_context *pCon);
void mtk_tcon_store_isp_cmd_gamma(void);
#endif
