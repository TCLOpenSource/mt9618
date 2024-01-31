/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_TRIGGER_GEN_H_
#define _MTK_TV_DRM_TRIGGER_GEN_H_

#include "../video_pnl/mtk_tv_drm_panel_common.h"
#include "hwreg_render_common_trigger_gen.h"

#include "../mtk_tv_drm_plane.h"
#include "../mtk_tv_drm_drv.h"
#include "../mtk_tv_drm_crtc.h"
#include "../mtk_tv_drm_connector.h"
#include "../mtk_tv_drm_encoder.h"
#include <linux/gpio/consumer.h>
#include <drm/mtk_tv_drm_panel.h>
#include "mtk_tv_drm_kms.h"

#define HWREG_VIDEO_REG_NUM_TRIGGER_GEN (30)
#define HWREG_VIDEO_REG_NUM_PQ_COMMON_TRIGGEN_SET (10)
#define HWREG_VIDEO_REG_NUM_RENDER_COMMON_TRIGGEN_SET (50)
#define HWREG_VIDEO_REG_NUM_TRIGGER_GEN_INIT (30)

typedef enum {
	MTK_TV_SERIES_MERAK_1 = 1,
	MTK_TV_SERIES_MERAK_2,
} mtktv_chip_series;

typedef enum  {
	TRIGGER_GEN_USECASE_HDMI_SCMI_FB_FRC_FB_MEMC_NORMAL = 0,
	TRIGGER_GEN_USECASE_B2R_SCMI_FBL_FRC_FB_MEMC_NORMAL,
	TRIGGER_GEN_USECASE_HDMI_SCMI_FB_FRC_FB_MEMC_LL, //SEC special
	TRIGGER_GEN_USECASE_HDMI_B2R_SCMI_FB_FRC_FBL_LL, //Game mode
	TRIGGER_GEN_USECASE_HDMI_SCMI_FB_FRC_FBL_VRR,
	TRIGGER_GEN_USECASE_SPECIAL,
	TRIGGER_GEN_USECASE_MAX,
} trigger_gen_usecase;

struct ST_VRR_BASE_VFREQ_MAPPING_TBL {
	uint8_t index;
	uint16_t u16Width;
	uint16_t u16Height;
	uint16_t u16FrameRate;
	uint16_t u16HTT;
	uint16_t u16VTT;
	uint32_t u32PixelClock;
};

enum EN_TRIGGER_DEBUG_MODE {
	E_TRIGGER_DEBUG_OFF,
	E_TRIGGER_DEBUG_LEGACY,
	E_TRIGGER_DEBUG_TS,
	E_TRIGGER_DEBUG_MAX,
};

static inline mtktv_chip_series get_mtk_chip_series(uint32_t pnl_ver)
{
	switch (pnl_ver) {
	case DRV_PNL_VERSION0100:
	case DRV_PNL_VERSION0200:
	case DRV_PNL_VERSION0203:
	case DRV_PNL_VERSION0300:
		return MTK_TV_SERIES_MERAK_1;
	case DRV_PNL_VERSION0400:
	case DRV_PNL_VERSION0500:
	case DRV_PNL_VERSION0600:
		return MTK_TV_SERIES_MERAK_2;
	default:
		return MTK_TV_SERIES_MERAK_1;
	}
}

int mtk_trigger_gen_init_render(struct mtk_tv_kms_context *pctx);
int mtk_trigger_gen_setup(
	struct mtk_tv_kms_context *pctx, bool bRIU, bool render_modify);
uint64_t mtk_video_panel_get_FrameLockPhaseDiff(void);
int mtk_trigger_gen_show_param(char *buf);
int mtk_trigger_gen_set_param(struct mtk_tv_kms_context *pctx,
		const char *buf, size_t count);
int mtk_trigger_gen_vsync_callback(void *p __maybe_unused);
int mtk_tv_drm_trigger_gen_init(
	struct mtk_tv_kms_context *pctx, bool render_modify);
void mtk_trigger_gen_set_bypass(bool bypass);

#endif
