/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_PANEL_COMMON_H_
#define _MTK_TV_DRM_PANEL_COMMON_H_

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <drm/drmP.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <video/display_timing.h>
#include <video/videomode.h>
#include <drm/mtk_tv_drm.h>
#include "hwreg_render_video_pnl.h"
#include "common/hwreg_common_riu.h"
#include <drm/mtk_tv_drm_panel.h>

//-------------------------------------------------------------------------------------------------
// Define & Macro
//-------------------------------------------------------------------------------------------------
#define PNL_OUTPUT_MAX_LANES    64
#define EXTDEV_NODE			"ext_video_out"
#define GFX_NODE			"graphic_out"

#define PNL_MOD_NODE              "mod"
#define PNL_MOD_REG               "reg"
#define PNL_LPLL_NODE             "lpll"
#define PNL_LPLL_REG              "reg"

#define IS_OUT_8K4K(width, height) (((width <= 8000) && (width >= 7500)) && \
							((height <= 4500) && (height >= 4000)))
#define IS_OUT_4K2K(width, height) (((width <= 4000) && (width >= 3750)) && \
							((height <= 2250) && (height >= 2000)))
#define IS_OUT_2K1K(width, height) (((width <= 2000) && (width >= 1875)) && \
							((height <= 1125) && (height >= 1000)))
#define IS_OUT_4K1K(width, height) (((width <= 4000) && (width >= 3750)) && \
							((height <= 1125) && (height >= 1000)))

#define IS_VFREQ_60HZ_GROUP(vfreq) ((vfreq < 65) && (vfreq > 45))
#define IS_VFREQ_120HZ_GROUP(vfreq) ((vfreq < 125) && (vfreq > 90))
#define IS_VFREQ_144HZ_GROUP(vfreq) ((vfreq < 149) && (vfreq > 139))
#define IS_VFREQ_240HZ_GROUP(vfreq) ((vfreq < 260) && (vfreq > 180))

/*
 *###########################
 *#       panel-info        #
 *###########################
 */
#define NAME_TAG		"panel_name"
#define LINK_TYPE_TAG		"link_type"
#define VBO_BYTE_TAG		"vbo_byte_mode"
#define DIV_SECTION_TAG		"div_section"
#define ON_TIMING_1_TAG		"on_timing1"
#define ON_TIMING_2_TAG		"on_timing2"
#define OFF_TIMING_1_TAG	"off_timing1"
#define OFF_TIMING_2_TAG	"off_timing2"
#define HSYNC_ST_TAG		"hsync_start"
#define HSYNC_WIDTH_TAG		"hsync_width"
#define HSYNC_POL_TAG		"hsync_polarity"
#define VSYNC_ST_TAG		"vsync_start"
#define VSYNC_WIDTH_TAG		"vsync_width"
#define VSYNC_POL_TAG		"vsync_polarity"
#define HSTART_TAG		"de_hstart"
#define VSTART_TAG		"de_vstart"
#define WIDTH_TAG		"resolution_width"
#define HEIGHT_TAG		"resolution_height"
#define MAX_HTOTAL_TAG		"max_h_total"
#define HTOTAL_TAG		"typ_h_total"
#define MIN_HTOTAL_TAG		"min_h_total"
#define MAX_VTOTAL_TAG		"max_v_total"
#define VTOTAL_TAG		"typ_v_total"
#define MIN_VTOTAL_TAG		"min_v_total"
#define MAX_VTOTAL_PANELPROTECT_TAG		"max_v_total_panelprotect"
#define MIN_VTOTAL_PANELPROTECT_TAG		"min_v_total_panelprotect"
#define TYP_CLK_H_TAG           "typ_clk_high"
#define TYP_CLK_L_TAG           "typ_clk_low"
#define OUTPUT_FMT_TAG		"output_format"
#define COUTPUT_FMT_TAG		"content_output_format"
#define VRR_EN_TAG             "vrr_en"
#define VRR_MIN_TAG            "min_vrr_framerate"
#define VRR_MAX_TAG            "max_vrr_framerate"
#define GRAPHIC_MIXER1_OUTMODE_TAG	"graphic_mixer1_outmode"
#define GRAPHIC_MIXER2_OUTMODE_TAG	"graphic_mixer2_outmode"
#define OUTPUT_COLOR_RANGE_TAG		"output_color_range"
#define OUTPUT_COLOR_SPACE_TAG		"output_color_space"
#define HPC_MODE_TAG		"hpc_mode"
#define GAME_DIRECT_FR_GROUP_TAG	"GameDirectFrameRateGroup"
#define DLG_ON_TAG	"dlg_on"

//------------------------------------------------------------------------------
// Enum
//------------------------------------------------------------------------------
typedef enum {
	E_NO_UPDATE_TYPE,
	E_MODE_RESET_TYPE,
	E_MODE_CHANGE_TYPE,
	E_MODE_RESUME_TYPE,
	E_OUTPUT_TYPE_MAX,
} drm_update_output_timing_type;

//-------------------------------------------------------------------------------------------------
// Structure
//-------------------------------------------------------------------------------------------------
typedef struct {
	uint32_t sup_lanes;
	uint8_t def_layout[PNL_OUTPUT_MAX_LANES];
	bool usr_defined;
	uint32_t ctrl_lanes;
	uint8_t lane_order[PNL_OUTPUT_MAX_LANES];
} drm_st_out_lane_order;

struct mtk_drm_panel_context {
	drm_st_pnl_info pnlInfo;
	drm_mod_cfg cfg;
	drm_update_output_timing_type out_upd_type;
	struct drm_panel drmPanel;
	struct device *dev;
	__u8 u8TimingsNum;
	drm_st_out_lane_order out_lane_info;
	bool lane_duplicate_en;
	struct list_head panelInfoList;
};

//-------------------------------------------------------------------------------------------------
// Function
//-------------------------------------------------------------------------------------------------
__u32 get_dts_u32(struct device_node *np, const char *name);
__u32 get_dts_u32_index(struct device_node *np, const char *name, int index);
__u32 get_dts_u32_elems(struct device_node *np, const char *name);
__u32 get_dts_u32_array(struct device_node *np, const char *name, uint32_t *array, int size);
void init_mod_cfg(drm_mod_cfg *pCfg);
__u32 setDisplayTiming(drm_st_pnl_info *pStPnlInfo);
void set_out_mod_cfg(drm_st_pnl_info *src_info, drm_mod_cfg *dst_cfg);
void dump_mod_cfg(drm_mod_cfg *pCfg);
void dump_panel_info(drm_st_pnl_info *pStPnlInfo);
void panel_common_enable(void);
void panel_common_disable(void);
void panel_init_clk_V1(bool bEn);
void panel_init_clk_V2(bool bEn);
void panel_init_clk_V3(bool bEn);
void panel_init_clk_V4(bool bEn);
void panel_init_clk_V5(bool bEn);
void panel_init_clk_V6(bool bEn);

int _parse_panel_info(struct device_node *np,
		drm_st_pnl_info *currentPanelInfo,
		struct list_head *panelInfoList);
int _copyPanelInfo(struct drm_crtc_state *crtc_state,
			drm_st_pnl_info *pCurrentInfo,
			struct list_head *pInfoList);
int _panelAddModes(struct drm_panel *panel,
			struct list_head *pInfoList,
			int *modeCouct);
bool _isSameModeAndPanelInfo(struct drm_display_mode *mode,
			drm_st_pnl_info *pPanelInfo);

#endif
