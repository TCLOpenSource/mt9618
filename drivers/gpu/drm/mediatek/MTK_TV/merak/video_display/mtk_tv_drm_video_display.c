// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <drm/mtk_tv_drm.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "mtk_tv_drm_plane.h"
#include "mtk_tv_drm_gem.h"
#include "mtk_tv_drm_video_display.h"
#include "mtk_tv_drm_video_frc.h"
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_ambient_light.h"

#include "drv_scriptmgt.h"
#include "mtk_tv_drm_trigger_gen.h"
#include "hwreg_render_common.h"
#include "hwreg_render_video_display.h"
#include "hwreg_render_video_frc.h"
#include "hwreg_render_stub.h"
#include "hwreg_render_video_globalpq.h"

#include "mapi_pq_cfd_if.h"
#include "pqu_msg.h"
#include "m_pqu_pq.h"
#include "ext_command_render_if.h"
#include "pqu_render.h"
#include "mtk-reserved-memory.h"
#include "mtk-efuse.h"

//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_DISPLAY

/*annie*/
#define MEMC_CONFIG (1)

#define DRM_PLANE_SCALE_MIN (1<<4)
#define DRM_PLANE_SCALE_MAX (1<<28)
#define MAX_WINDOW_NUM (16)
#define OLD_LAYER_INITIAL_VALUE 0xFF

#define MEMC_PROP(i) \
		((i >= EXT_VPLANE_PROP_MEMC_LEVEL) && \
			(i <= EXT_VPLANE_PROP_MEMC_INIT_VALUE_FOR_RV55)) \

//Explicitly specify division using u64
#define ALIGN_UPTO_X(w, x)      (div_u64(((uint64_t)(w) + (x - 1)), x) * x)
#define ALIGN_DOWNTO_X(w, x)    (div_u64((uint64_t)w, x) * x)

#define WINDOW_ALPHA_DEFAULT_VALUE 0xFF
#define BLEND_VER0_HSZIE_UNIT 4
#define BITS_PER_BYTE 8
#define MEMC_WINID_DEFAULT 0xFFFF
#define ROUNDING 2
#define BLENDING_LAYER_SRC0 0
#define BLENDING_LAYER_SRC1 1
#define BLENDING_LAYER_SRC2 2
#define BLENDING_LAYER_SRC3 3
#define CFD_DATAFORMAT_DEFAULT 0xFF

/* VG sync */
#define MTK_DRM_TV_FRC_COMPATIBLE_STR "mediatek,mediatek-frc"
#define VGSYNC_MAX_FRAME_CNT (20)
#define VGSYNC_BUF_INDEX (2)
#define FRC_CMD_LENGTH (5)
#define VGSYNC_8_ALIGN (8)
#define IS_INPUT_B2R(x) ((x == META_PQ_INPUTSRC_DTV_DIS) || (x == META_PQ_INPUTSRC_STORAGE_DIS) || \
				(x == META_PQ_INPUTSRC_STORAGE_DIS))
#define _FREQ_70HZ   (70)
#define _FREQ_1KHZ (1000)
#define _120HZ_PANEL(x) ((x) > _FREQ_70HZ)

// color black in RGB and YUV
#define RGB_BLACK_R 0
#define RGB_BLACK_G 0
#define RGB_BLACK_B 0
#define YUV_BLACK_Y 0
#define YUV_BLACK_U 128
#define YUV_BLACK_V 128

#define ENABLE_HW_MODE_VGSYNC_SHM 0
#define FRC_ONE 1
#define FRC_TWO 2
#define RBANK_PROT_POSITION 4

#define DEFAULT_SCL_RATIO 0x100000L

static bool g_bUseRIU = FALSE;
static struct dma_buf *g_meta_db;
static u8 Frame_Domain_CFD_OutputDataFormat = CFD_DATAFORMAT_DEFAULT;

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
static bool g_b_memc_vgsync_on;
static uint8_t g_u8_memc_rw_diff_counter;
static struct mtk_cfd_set_CSC g_st_cfd_last_csc_input = {0};
static struct mtk_cfd_set_CSC g_st_cfd_last_csc_tconin = {0};
static struct mtk_cfd_set_CSC g_st_cfd_last_csc_mainout = {0};
static struct mtk_cfd_set_CSC g_st_cfd_last_csc_deltaout = {0};
static struct mtk_frc_vgsync_buf_info *g_pst_vgsync_buf_info;

static struct m_pq_dv_out_frame_info g_dv_out_frame_info = {
	.stub = false,
	.gd_info = {0},
	.dv_pq_hw_ver = 0,
	.l_11_Data = {0},
	.ret = 0
};

static struct drm_prop_enum_list video_plane_type_enum_list[] = {
	{
		.type = MTK_VPLANE_TYPE_NONE,
		.name = "VIDEO_PLANE_TYPE_NONE"
	},
	{
		.type = MTK_VPLANE_TYPE_MEMC,
		.name = "VIDEO_PLANE_TYPE_MEMC"
	},
	{
		.type = MTK_VPLANE_TYPE_MULTI_WIN,
		.name = "VIDEO_PLANE_TYPE_MULTI_WIN",
	},
	{
		.type = MTK_VPLANE_TYPE_SINGLE_WIN1,
		.name = "VIDEO_PLANE_TYPE_SINGLE_WIN1"
	},
	{
		.type = MTK_VPLANE_TYPE_SINGLE_WIN2,
		.name = "VIDEO_PLANE_TYPE_SINGLE_WIN2"
	},
};

static struct drm_prop_enum_list video_memc_misc_type_enum_list[] = {
	{
		.type = VIDEO_PLANE_MEMC_MISC_NULL,
		.name = MTK_VIDEO_PROP_MEMC_MISC_NULL},
	{
		.type = VIDEO_PLANE_MEMC_MISC_INSIDE,
		.name = MTK_VIDEO_PROP_MEMC_MISC_INSIDE},
	{
		.type = VIDEO_PLANE_MEMC_MISC_INSIDE_60HZ,
		.name = MTK_VIDEO_PROP_MEMC_MISC_INSIDE_60HZ},
};

static struct drm_prop_enum_list video_memc_pattern_enum_list[] = {
	{.type = VIDEO_PLANE_MEMC_NULL_PAT,
		.name = MTK_VIDEO_PROP_MEMC_NULL_PAT},
	{.type = VIDEO_PLANE_MEMC_OPM_PAT,
		.name = MTK_VIDEO_PROP_MEMC_OPM_PAT},
	{.type = VIDEO_PLANE_MEMC_END_PAT,
		.name = MTK_VIDEO_PROP_MEMC_END_PAT},
};

static struct drm_prop_enum_list video_memc_level_enum_list[] = {
	{.type = VIDEO_PLANE_MEMC_LEVEL_OFF,
		.name = MTK_VIDEO_PROP_MEMC_LEVEL_OFF},
	{.type = VIDEO_PLANE_MEMC_LEVEL_LOW,
		.name = MTK_VIDEO_PROP_MEMC_LEVEL_LOW},
	{.type = VIDEO_PLANE_MEMC_LEVEL_MID,
		.name = MTK_VIDEO_PROP_MEMC_LEVEL_MID},
	{.type = VIDEO_PLANE_MEMC_LEVEL_HIGH,
		.name = MTK_VIDEO_PROP_MEMC_LEVEL_HIGH},
};

static struct drm_prop_enum_list video_memc_game_mode_enum_list[] = {
	{.type = VIDEO_PLANE_MEMC_GAME_OFF,
		.name = MTK_VIDEO_PROP_MEMC_GAME_OFF},
	{.type = VIDEO_PLANE_MEMC_GAME_ON,
		.name = MTK_VIDEO_PROP_MEMC_GAME_ON},
};

static struct drm_prop_enum_list vplane_buf_mode_enum_list[] = {
	{
		.type = MTK_VPLANE_BUF_MODE_SW,
		.name = "VPLANE_BUF_MODE_SW"
	},
	{
		.type = MTK_VPLANE_BUF_MODE_HW,
		.name = "VPLANE_BUF_MODE_HW"
	},
	{
		.type = MTK_VPLANE_BUF_MODE_BYPASSS,
		.name = "VPLANE_BUF_MODE_BYPASSS"
	},
};

static struct drm_prop_enum_list vplane_disp_mode_enum_list[] = {
	{
		.type = MTK_VPLANE_DISP_MODE_SW,
		.name = "VPLANE_DISP_MODE_SW"
	},
	{
		.type = MTK_VPLANE_DISP_MODE_HW,
		.name = "VPLANE_DISP_MODE_HW"
	},
};

static struct drm_prop_enum_list vplane_disp_win_type_enum_list[] = {
	{
		.type = MTK_VPLANE_DISP_WIN_TYPE_NONE,
		.name = "DISP_WIN_TYPE_NONE"
	},
	{
		.type = MTK_VPLANE_DISP_WIN_TYPE_NO_FRC_VGSYNC,
		.name = "DISP_WIN_TYPE_NO_VGSYNC"
	},
	{
		.type = MTK_VPLANE_DISP_WIN_TYPE_FRC_VGSYNC,
		.name = "DISP_WIN_TYPE_VGSYNC"
	},
	{
		.type = MTK_VPLANE_DISP_WIN_TYPE_MAX,
		.name = "DISP_WIN_TYPE_MAX"
	},
};

static const struct ext_prop_info ext_video_plane_props_def[] = {
	{
		.prop_name = MTK_VIDEO_PLANE_PROP_MUTE_SCREEN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0x1,
		.init_val = 0x0,
	},
	{
		.prop_name = MTK_VIDEO_PLANE_PROP_SNOW_SCREEN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0x1,
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_NAME_VIDEO_PLANE_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_plane_type_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(video_plane_type_enum_list),
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_NAME_META_FD,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = INT_MAX,
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_NAME_FREEZE,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0x1,
		.init_val = 0x0,
	},
	{
		.prop_name = MTK_VIDEO_PLANE_PROP_INPUT_VFREQ,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = INT_MAX,
		.init_val = 0x0,
	},
	{
		.prop_name = MTK_VIDEO_PLANE_PROP_INPUT_SOURCE_VFREQ,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = INT_MAX,
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_VIDEO_PLANE_PROP_BUF_MODE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &vplane_buf_mode_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(vplane_buf_mode_enum_list),
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_VIDEO_PLANE_PROP_DISP_MODE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &vplane_disp_mode_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(vplane_disp_mode_enum_list),
		.init_val = 0x0,
	},
	{
		.prop_name = PROP_VIDEO_PLANE_PROP_DISP_WIN_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &vplane_disp_win_type_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(vplane_disp_win_type_enum_list),
		.init_val = 0x0,
	},
/*annie add MEMC*/
#if (1)/*(MEMC_CONFIG == 1)*/
	/****MEMC plane prop******/
	{
		.prop_name = MTK_PLANE_PROP_MEMC_LEVEL_MODE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_memc_level_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_memc_level_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_GAME_MODE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_memc_game_mode_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_memc_game_mode_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_PATTERN,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_memc_pattern_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_memc_pattern_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_MISC_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_memc_misc_type_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_memc_misc_type_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_DECODE_IDX_DIFF,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_STATUS,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_TRIG_GEN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0xFF,
		.init_val = 0xBB,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_RV55_INFO,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0xFF,
		.init_val = 0x0,
	},
	{
		.prop_name = MTK_PLANE_PROP_MEMC_INIT_VALUE_FOR_RV55,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0x0,
		.range_info.max = 0xFF,
		.init_val = 0x0,
	},
#endif
	{
		.prop_name = PROP_VIDEO_CRTC_PROP_WINDOW_PQ,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_VIDEO_PLANE_PROP_PQMAP_NODES_ARRAY,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0x0,
	},
};


/* ------- Static function -------*/

static void _mtk_video_display_update_val_if_debug(uint32_t prop_idx,
	struct drm_plane_state *state,
	unsigned int plane_idx, uint64_t *val)
{
	if (gMtkDrmDbg.mute_enable.bEn) {
		if (prop_idx == EXT_VPLANE_PROP_MUTE_SCREEN)
			*val = gMtkDrmDbg.mute_enable.bMuteEn;
	} else if ((gMtkDrmDbg.change_vPlane_type != NULL) &&
				(gMtkDrmDbg.change_vPlane_type->win_en[plane_idx])) {
		if (prop_idx == EXT_VPLANE_PROP_VIDEO_PLANE_TYPE)
			*val = gMtkDrmDbg.change_vPlane_type->vPlane_type[plane_idx];
	} else if ((gMtkDrmDbg.update_disp_size != NULL) &&
				(gMtkDrmDbg.update_disp_size->win_en[plane_idx])) {
		state->crtc_x =
			(int32_t)gMtkDrmDbg.update_disp_size->dispwindow[plane_idx].x;
		state->crtc_y =
			(int32_t)gMtkDrmDbg.update_disp_size->dispwindow[plane_idx].y;
		state->crtc_w =
			gMtkDrmDbg.update_disp_size->dispwindow[plane_idx].w;
		state->crtc_h =
			gMtkDrmDbg.update_disp_size->dispwindow[plane_idx].h;
	}
}

static bool _mtk_video_display_is_variable_updated(
	uint64_t oldVar,
	uint64_t newVar)
{
	bool update = 0;

	if (newVar != oldVar)
		update = 1;

	return update;
}

static uint8_t _mtk_video_display_is_ext_prop_updated(
	struct video_plane_prop *plane_props,
	enum ext_video_plane_prop prop)
{
	uint8_t update = 0;

	if ((prop >= EXT_VPLANE_PROP_MUTE_SCREEN) && (prop < EXT_VPLANE_PROP_MAX))
		update = plane_props->prop_update[prop];

	return update;
}

static void _mtk_video_display_check_support_window(
	enum drm_mtk_video_plane_type old_video_plane_type,
	enum drm_mtk_video_plane_type video_plane_type,
	unsigned int plane_idx, struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	int i = 0;
	static uint16_t memc_windowID = MEMC_WINID_DEFAULT;

	if ((video_plane_type >= MTK_VPLANE_TYPE_NONE) && (video_plane_type < MTK_VPLANE_TYPE_MAX))
		pctx_video->plane_num[video_plane_type]++;
	else
		return;

	if ((old_video_plane_type >= MTK_VPLANE_TYPE_NONE)
		&& (old_video_plane_type < MTK_VPLANE_TYPE_MAX))
		pctx_video->plane_num[old_video_plane_type]--;
	else
		return;

	for (i = 0; i < MTK_VPLANE_TYPE_MAX; i++) {
		DRM_DEBUG_ATOMIC("[%d] plane_type=%d plane_num=%d\n",
			__LINE__, i, pctx_video->plane_num[i]);
	}

	plane_ctx->memc_change_on = MEMC_CHANGE_NONE;
	switch (video_plane_type) {
	case MTK_VPLANE_TYPE_MEMC:
		plane_ctx->memc_change_on = MEMC_CHANGE_ON;
		memc_windowID = plane_idx;
		DRM_INFO("[%s][%d] memc_change_on, memc_windowID:%d.\n", __func__, __LINE__,
			memc_windowID);
		break;
	case MTK_VPLANE_TYPE_NONE:
		plane_ctx->memc_change_on = MEMC_CHANGE_NONE;
		DRM_INFO("[%s][%d] memc_change_none\n",	__func__, __LINE__);
		if (memc_windowID == plane_idx) {
			mtk_video_display_set_frc_freeze(pctx, plane_idx, 0 /*unfreeze*/);
			memc_windowID = MEMC_WINID_DEFAULT;
		}
		break;
	default:
		plane_ctx->memc_change_on = MEMC_CHANGE_OFF;
		DRM_INFO("[%s][%d] memc_change_off\n",	__func__, __LINE__);
		break;
	}

	pctx_video->video_plane_type[plane_idx] = video_plane_type;
}

static void _mtk_video_display_set_mgwdmaEnable(
	struct mtk_video_context *pctx_video, bool suspend)
{
	enum drm_mtk_video_plane_type video_plane_type = 0;
	struct mtk_tv_kms_context *pctx = NULL;

	struct reg_info reg[HWREG_VIDEO_REG_NUM_MGWDMA_EN];
	struct hwregMGWDMAEnIn paramIn;
	struct hwregOut paramOut;
	bool force = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	paramIn.RIU = g_bUseRIU;
	paramIn.planeType = (enum drm_hwreg_video_plane_type) video_plane_type;

	if (pctx->stub)
		force = TRUE;

	if (!suspend) {
		for (video_plane_type = MTK_VPLANE_TYPE_MULTI_WIN;
						video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
						++video_plane_type) {
			paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;
			if (pctx_video->plane_num[video_plane_type] == 0)
				paramIn.enable = false;
			else
				paramIn.enable = true;

			if (force ||
				(paramIn.enable != pctx_video->mgwdmaEnable[video_plane_type])) {
				drv_hwreg_render_display_set_mgwdmaEnable(paramIn, &paramOut);

				pctx_video->mgwdmaEnable[video_plane_type] = paramIn.enable;
			}
		}

		if (!g_bUseRIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}
	} else {
		for (video_plane_type = MTK_VPLANE_TYPE_MULTI_WIN;
				video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
				++video_plane_type) {
			paramIn.RIU = true; //use RIU when it's suspend state.
			paramIn.enable = false;

			if (force ||
				(paramIn.enable != pctx_video->mgwdmaEnable[video_plane_type])) {
				drv_hwreg_render_display_set_mgwdmaEnable(paramIn, &paramOut);

				pctx_video->mgwdmaEnable[video_plane_type] = paramIn.enable;
			}
		}
	}
}

static void _mtk_video_display_check_render_close(
	struct mtk_tv_kms_context *pctx,
	struct mtk_video_context *pctx_video,
	bool *pbrendercloseEn)
{
	uint8_t u8Total_plane_num = 0;
	struct mtk_panel_context *pctx_pnl = NULL;

	if ((pctx_video == NULL) || (pbrendercloseEn == NULL) || (pctx == NULL))
		return;

	pctx_pnl = &pctx->panel_priv;

	u8Total_plane_num = pctx_video->plane_num[MTK_VPLANE_TYPE_MEMC] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN1] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN2];

	DRM_INFO("[%s, %d] u8Total_plane_num=%d",
			__func__, __LINE__, u8Total_plane_num);

	if (u8Total_plane_num == 0)
		*pbrendercloseEn = true;
	else
		*pbrendercloseEn = false;
}

static void _mtk_video_display_set_frcOpmMaskEnable(
	struct mtk_tv_kms_context *pctx)
{
	enum drm_mtk_video_plane_type video_plane_type = 0;
	E_DRV_MFC_UNMASK E_MFC_IPM_UNMASK = E_MFC_UNMASK_OFF;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FRC_OPM_MASK_EN];
	struct hwregFrcOpmMaskEn paramIn;
	struct hwregOut paramOut;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;
	paramIn.RIU = true;

	E_MFC_IPM_UNMASK = mtk_video_frc_get_unmask(pctx_video);
	video_plane_type = MTK_VPLANE_TYPE_MEMC;
	if (pctx_video->plane_num[video_plane_type] == 0)
		paramIn.enable = false;
	else
		paramIn.enable = true;

	if ((E_MFC_IPM_UNMASK == E_MFC_UNMASK_OFF) &&
		(paramIn.enable == false)) {
		//When FRC plane_num = 0, Mask OPM Read
		if (pctx_frc->frcopmMaskEnable == false) {
			drv_hwreg_render_frc_opmMaskEn(paramIn, &paramOut);
			pctx_frc->frcopmMaskEnable = true;
			DRM_INFO("[%s][%d] FRC plane_num = 0, IPM_UNMASK = OFF!!\n",
				__func__, __LINE__);
		}
	}

	if(paramIn.enable == true)
		pctx_frc->frcopmMaskEnable = false;

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_extWinEnable(
	struct mtk_video_context *pctx_video)
{
	enum drm_mtk_video_plane_type video_plane_type = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum DRM_VB_VERSION_E VB_Version = (enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	uint8_t MGW_NUM = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	struct reg_info reg[HWREG_VIDEO_REG_NUM_EXT_WIN_EN];
	struct hwregExtWinEnIn paramIn;
	struct hwregOut paramOut;
	bool force = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	paramIn.RIU = g_bUseRIU;
	paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (pctx->stub)
		force = TRUE;

	for (video_plane_type = MTK_VPLANE_TYPE_MULTI_WIN;
					video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
					++video_plane_type) {
		paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

		if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
			/*VB version 0: MGW single win use multi-bank*/
			/*VB version 1: MGW single win use ext-bank*/
			if ((MGW_NUM > 1) ||
				((MGW_NUM == 1) && (VB_Version == EN_VB_VERSION_TS_V0)))
				paramIn.enable = false;
			else
				paramIn.enable = true;
		} else {
			paramIn.enable = true;
		}

		if (force || (paramIn.enable != pctx_video->extWinEnable[video_plane_type])) {
			drv_hwreg_render_display_set_extWinEnable(paramIn, &paramOut);

			pctx_video->extWinEnable[video_plane_type] = paramIn.enable;
		}
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_mgwdmaWinEnable(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	bool enable)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	enum drm_mtk_video_plane_type old_video_plane_type =
		old_plane_ctx->video_plane_type;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MGWDMA_WIN_EN];
	struct hwregMGWDMAWinEnIn paramIn;
	struct hwregOut paramOut;
	int ret;
	bool bUpdate = FALSE;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	paramIn.RIU = g_bUseRIU;
	paramIn.planeType = (enum drm_hwreg_video_plane_type)
		(enable ? video_plane_type : old_video_plane_type);

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramIn.windowID = plane_idx;

	paramIn.enable = enable;

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_video_plane_type, video_plane_type);

	if (bUpdate) {
		DRM_INFO("[%s][%d]plane_idx:%d, enable:%d, video_plane_type(old, new):(%d, %d)\n",
			__func__, __LINE__,
			plane_idx, paramIn.enable,
			old_video_plane_type, video_plane_type);
	}

	drv_hwreg_render_display_set_mgwdmaWinEnable(paramIn, &paramOut);

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_crop_win(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct mtk_plane_state *state)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	struct hwregCropWinIn paramIn;
	struct hwregVBPreBlendOutSizeIn paramIn_vb_pre;
	struct hwregOut paramOut;
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E)video_hw_version->video_blending;
	uint8_t MGW_NUM = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	struct reg_info reg[HWREG_VIDEO_REG_NUM_CROP_WIN];
	bool force = FALSE;
	bool bUpdate = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregCropWinIn));
	memset(&paramIn_vb_pre, 0, sizeof(struct hwregVBPreBlendOutSizeIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	if (plane_idx >= MAX_WINDOW_NUM) {
		DRM_ERROR("[%s][%d] Invalid plane_idx:%d !! MAX_WINDOW_NUM:%d\n",
			__func__, __LINE__, plane_idx, MAX_WINDOW_NUM);
		return;
	}

	pctx = pctx_video_to_kms(pctx_video);
	pctx_pnl = &(pctx->panel_priv);

	// crop height update at scaling
	plane_ctx->crop_win.h =
		ALIGN_UPTO_X(plane_ctx->crop_win.h, pctx->hw_caps.crop_h_align);

	paramOut.reg = reg;

	paramIn.RIU = g_bUseRIU;
	paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramIn.windowID = plane_idx;

	if (pctx->stub)
		force = TRUE;

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_video_plane_type, video_plane_type);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.x, plane_ctx->crop_win.x);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.y, plane_ctx->crop_win.y);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.w, plane_ctx->crop_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.h, plane_ctx->crop_win.h);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mgw_plane_num, MGW_NUM);

	if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
		(video_plane_type < MTK_VPLANE_TYPE_MAX)) {

		if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
			paramIn.MGWPlaneNum = MGW_NUM;

			if (MGW_NUM == 1) {
				/*VB version 0:*/
				/*MGW single win use ext-bank, crop fill real size*/
				/*VB version 1:*/
				/*MGW single win use multi-bank, crop fill frame size*/
				if (VB_Version == EN_VB_VERSION_TS_V0) {
					paramIn.cropWindow.w = pctx_pnl->info.de_width;
					paramIn.cropWindow.h = pctx_pnl->info.de_height;
				}

				if (VB_Version == EN_VB_VERSION_TS_V1) {
					paramIn.cropWindow.w = plane_ctx->crop_win.w;
					paramIn.cropWindow.h = plane_ctx->crop_win.h;
				}
			} else {
				paramIn.cropWindow.w = pctx_pnl->info.de_width;
				paramIn.cropWindow.h = pctx_pnl->info.de_height;
			}
		} else {
			/*set ext win H_size/V_size*/
			paramIn.cropWindow.w = plane_ctx->crop_win.w;
			paramIn.cropWindow.h = plane_ctx->crop_win.h;
		}
	} else {
		DRM_INFO("[%s][%d] Invalid  HW plane type\n",
			__func__, __LINE__);
	}

	paramIn.cropWindow.x = plane_ctx->crop_win.x;
	paramIn.cropWindow.y = plane_ctx->crop_win.y;

	drv_hwreg_render_display_set_cropWindow(paramIn, &paramOut);

	if (pctx->sw_caps.enable_pqu_ds_size_ctl == 0) {
		if (VB_Version == EN_VB_VERSION_LEGACY_V1) {
			paramIn_vb_pre.RIU = g_bUseRIU;
			paramIn_vb_pre.Hsize = plane_ctx->crop_win.w;

			drv_hwreg_render_display_set_vb_pre_blendOut_size(
				paramIn_vb_pre, &paramOut);
		}
	}

	if (force || bUpdate) {
		DRM_INFO("[%s][%d] plane_idx:%d video_plane_type:%d MGW_NUM:%d\n",
			__func__, __LINE__, plane_idx, video_plane_type, MGW_NUM);
		DRM_INFO("[%s][%d] old_crop_win:[%llu, %llu, %llu, %llu]\n"
			, __func__, __LINE__,
			old_plane_ctx->crop_win.x, old_plane_ctx->crop_win.y,
			old_plane_ctx->crop_win.w, old_plane_ctx->crop_win.h);
		DRM_INFO("[%s][%d] crop_win:[%llu, %llu, %llu, %llu]\n",
			__func__, __LINE__,
			plane_ctx->crop_win.x, plane_ctx->crop_win.y,
			plane_ctx->crop_win.w, plane_ctx->crop_win.h);


		DRM_INFO("[%s][%d]final crop(x,y,w,h):[%llu, %llu, %llu, %llu]\n",
			__func__, __LINE__,
			paramIn.cropWindow.x, paramIn.cropWindow.y,
			paramIn.cropWindow.w, paramIn.cropWindow.h);
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->vgs_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: vgs_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_get_format_bpp(
	struct mtk_video_format_info *video_format_info)
{
	switch (video_format_info->fourcc) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
		video_format_info->bpp[0] = 32;
		video_format_info->bpp[1] = 0;
		video_format_info->bpp[2] = 0;
		break;
	case DRM_FORMAT_RGB565:
		video_format_info->bpp[0] = 16;
		video_format_info->bpp[1] = 0;
		video_format_info->bpp[2] = 0;
		break;
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		video_format_info->bpp[0] = 32;
		video_format_info->bpp[1] = 0;
		video_format_info->bpp[2] = 0;
		break;
	case DRM_FORMAT_YUV444:
		if (video_format_info->modifier && video_format_info->modifier_compressed) {
			//YUV444 8b ce
			video_format_info->bpp[0] = 8;
			video_format_info->bpp[1] = 8;
			video_format_info->bpp[2] = 8;
		} else {
			//undefine format
			video_format_info->bpp[0] = 0;
			video_format_info->bpp[1] = 0;
			video_format_info->bpp[2] = 0;
		}
		break;
	case DRM_FORMAT_YVYU:
		if ((video_format_info->modifier) &&
			(video_format_info->modifier_arrange ==
				(DRM_FORMAT_MOD_MTK_YUV444_VYU & FMT_MODIFIER_ARRANGE_MASK))) {
			if (video_format_info->modifier_10bit) {
				//YUV444 10b
				video_format_info->bpp[0] = 30;
				video_format_info->bpp[1] = 0;
				video_format_info->bpp[2] = 0;
			} else if (video_format_info->modifier_6bit) {
				//undefine format
				video_format_info->bpp[0] = 0;
				video_format_info->bpp[1] = 0;
				video_format_info->bpp[2] = 0;
			} else {
				//YUV444 8b
				video_format_info->bpp[0] = 24;
				video_format_info->bpp[1] = 0;
				video_format_info->bpp[2] = 0;
			}
		} else {
			//YUV422 8b 1pln
			video_format_info->bpp[PLANE_0] = FMT_YUV422_8B_1PLN_BPP0;
			video_format_info->bpp[PLANE_1] = 0;
			video_format_info->bpp[PLANE_2] = 0;
		}
		break;
	case DRM_FORMAT_NV61:
		if (video_format_info->modifier && video_format_info->modifier_10bit) {
			//YUV422 10b
			video_format_info->bpp[0] = 10;
			video_format_info->bpp[1] = 10;
			video_format_info->bpp[2] = 0;
		} else if (video_format_info->modifier &&
			   video_format_info->modifier_6bit &&
			   video_format_info->modifier_compressed) {
			//YUV422 6b ce
			video_format_info->bpp[0] = 6;
			video_format_info->bpp[1] = 6;
			video_format_info->bpp[2] = 0;
		} else if (video_format_info->modifier && video_format_info->modifier_compressed) {
			//YUV422 8b ce
			video_format_info->bpp[0] = 8;
			video_format_info->bpp[1] = 8;
			video_format_info->bpp[2] = 0;
		} else if (!video_format_info->modifier) {
			//YUV422 8b 2pln
			video_format_info->bpp[0] = 8;
			video_format_info->bpp[1] = 8;
			video_format_info->bpp[2] = 0;
		} else {
			//undefine format
			video_format_info->bpp[0] = 0;
			video_format_info->bpp[1] = 0;
			video_format_info->bpp[2] = 0;
		}
		break;
	case DRM_FORMAT_NV21:
		if (video_format_info->modifier && video_format_info->modifier_10bit) {
			//YUV420 10b
			video_format_info->bpp[0] = 10;
			video_format_info->bpp[1] = 5;
			video_format_info->bpp[2] = 0;
		} else if (video_format_info->modifier &&
			   video_format_info->modifier_6bit &&
			   video_format_info->modifier_compressed) {
			//YUV420 6b ce
			video_format_info->bpp[0] = 6;
			video_format_info->bpp[1] = 3;
			video_format_info->bpp[2] = 0;
		} else if (video_format_info->modifier && video_format_info->modifier_compressed) {
			//YUV420 8b ce
			video_format_info->bpp[0] = 8;
			video_format_info->bpp[1] = 4;
			video_format_info->bpp[2] = 0;
		} else if (!video_format_info->modifier) {
			//YUV420 8b
			video_format_info->bpp[0] = 8;
			video_format_info->bpp[1] = 4;
			video_format_info->bpp[2] = 0;
		} else {
			//undefine format
			video_format_info->bpp[0] = 0;
			video_format_info->bpp[1] = 0;
			video_format_info->bpp[2] = 0;
		}
		break;
	default:
		video_format_info->bpp[0] = 0;
		video_format_info->bpp[1] = 0;
		video_format_info->bpp[2] = 0;
		break;
	}
}

/*
static void _mtk_video_display_get_disp_mode(
	unsigned int plane_inx,
	struct mtk_video_context *pctx_video,
	enum drm_mtk_vplane_disp_mode *disp_mode)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);

	*disp_mode = plane_props->prop_val[EXT_VPLANE_PROP_DISP_MODE];

}
*/

static void _mtk_video_display_update_displywinodw_info(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	struct mtk_video_plane_ctx *old_plane_ctx =
		(pctx_video->old_plane_ctx + plane_idx);
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	uint64_t fixup_x = state->base.crtc_x;
	uint64_t fixup_y = state->base.crtc_y;
	int disp_win_version = video_hw_version->disp_win;
	bool bUpdate = FALSE;

	/* add the panel start if is tgen control blending */
	if (disp_win_version == DISP_WIN_VERSION_TGEN) {
		fixup_x += pctx_pnl->info.de_hstart;
		fixup_y += pctx_pnl->info.de_vstart;
	}

	plane_ctx->disp_win.x =
		ALIGN_DOWNTO_X(fixup_x, pctx->hw_caps.disp_x_align);
	plane_ctx->disp_win.y =
		ALIGN_DOWNTO_X(fixup_y, pctx->hw_caps.disp_y_align);
	plane_ctx->disp_win.w =
		ALIGN_UPTO_X(state->base.crtc_w, pctx->hw_caps.disp_w_align);
	plane_ctx->disp_win.h =
		ALIGN_UPTO_X(state->base.crtc_h, pctx->hw_caps.disp_h_align);
	plane_ctx->disp_win_type =
		plane_props->prop_val[EXT_VPLANE_PROP_DISP_WIN_TYPE];

	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->disp_win.x, plane_ctx->disp_win.x);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->disp_win.y, plane_ctx->disp_win.y);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->disp_win.w, plane_ctx->disp_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->disp_win.h, plane_ctx->disp_win.h);

	if (bUpdate) {
		DRM_INFO("[%s][%d] plane_inx:%d video_plane_type:%d\n",
			__func__, __LINE__, plane_idx, video_plane_type);
		DRM_INFO("[%s][%d] disp win(x,y,w,h):[%d, %d, %d, %d]\n",
			__func__, __LINE__,
			(int)state->base.crtc_x, (int)state->base.crtc_y,
			(int)state->base.crtc_w, (int)state->base.crtc_h);
		DRM_INFO("[%s][%d] disp win w/ align(x,y,w,h):[%d, %d, %d, %d]\n",
			__func__, __LINE__,
			(int)plane_ctx->disp_win.x, (int)plane_ctx->disp_win.y,
			(int)plane_ctx->disp_win.w, (int)plane_ctx->disp_win.h);
	}
}

static void _mtk_video_display_update_cropwinodw_info(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	struct mtk_video_plane_ctx *old_plane_ctx =
		(pctx_video->old_plane_ctx + plane_idx);
	uint64_t crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0;
	bool bUpdate = FALSE;

	crop_x = state->base.src_x >> SHIFT_16_BITS;
	crop_y = state->base.src_y >> SHIFT_16_BITS;
	crop_w = state->base.src_w >> SHIFT_16_BITS;
	crop_h = state->base.src_h >> SHIFT_16_BITS;

	plane_ctx->crop_win.x = ALIGN_DOWNTO_X(crop_x, pctx->hw_caps.crop_x_align_420_422);
	plane_ctx->crop_win.y = ALIGN_DOWNTO_X(crop_y, pctx->hw_caps.crop_y_align_420);
	plane_ctx->crop_win.w = ALIGN_UPTO_X(crop_w, pctx->hw_caps.crop_w_align);
	plane_ctx->crop_win.h = ALIGN_UPTO_X(crop_h, pctx->hw_caps.crop_h_align);

#if (defined ENABLE_HW_MODE_VGSYNC_SHM) && (ENABLE_HW_MODE_VGSYNC_SHM == 1)
	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW) {
		struct mtk_tv_drm_metabuf metabuf;
		struct render_pqu_frame_info *frame_info = NULL;
		uint16_t r_index = 0;
		uint16_t buffer = 0;
		int ret = 0;

		memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));

		if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf,
		MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO)) {
			DRM_ERROR("get mmap %d fail",
				MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO);
		}
		frame_info = metabuf.addr;

		r_index = pctx_video->rbank[plane_idx].r_index;

		buffer = pctx_video->rbank[plane_idx].rbank_idx[r_index];

		if (frame_info->frame_info[buffer].pqu_ready) {
			plane_ctx->crop_win.w = frame_info->frame_info[buffer].disp_width;
			plane_ctx->crop_win.h = frame_info->frame_info[buffer].disp_height;
			frame_info->frame_info[buffer].pqu_ready = FALSE;
			if ((plane_ctx->crop_win.w == 0) || (plane_ctx->crop_win.h == 0))
				DRM_INFO("[%s][%d] size = 0\n", __func__, __LINE__);
		} else {
			DRM_WARN("[%s][%d] pqu not ready\n", __func__, __LINE__);
		}

		ret = mtk_tv_drm_metabuf_free(&metabuf);
		if (ret != 0)
			DRM_ERROR("release mmap %d fail",
				MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO);
	}
#endif
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->crop_win.x, plane_ctx->crop_win.x);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->crop_win.y, plane_ctx->crop_win.y);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->crop_win.w, plane_ctx->crop_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->crop_win.h, plane_ctx->crop_win.h);

	if (bUpdate) {
		DRM_INFO("[%s][%d] crop win(x,y,w,h)(>>16):[%llu, %llu, %llu, %llu]\n",
			__func__, __LINE__, crop_x, crop_y, crop_w, crop_h);

		DRM_INFO("[%s][%d](align) crop(x,y,w,h)(>>16):[%llu, %llu, %llu, %llu]\n",
			__func__, __LINE__,
			plane_ctx->crop_win.x, plane_ctx->crop_win.y,
			plane_ctx->crop_win.w, plane_ctx->crop_win.h);
	}
}

static void _mtk_video_set_fohvsp_scaling_ratio(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	uint64_t result = 0, Phaseresult = 0;
	uint32_t HorIniFactor = 0, VerIniFactor = 0;
	uint32_t HorRatio = 0, VerRatio = 0;
	uint8_t Hscale_enable = 0, Vscale_enable = 0;
	uint8_t IsScalingUp_H = 0, IsScalingUp_V = 0;

	struct hwregScalingIn paramScalingIn;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_SCL];
	int ret;
	bool stub = 0;
	bool bUpdate = FALSE;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get drm video test mode\n");


	memset(reg, 0, sizeof(reg));
	memset(&paramScalingIn, 0, sizeof(struct hwregScalingIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;
	paramScalingIn.RIU = g_bUseRIU;

	bUpdate |= _mtk_video_display_is_variable_updated(
			plane_ctx->fo_hvsp_out_window.w, old_plane_ctx->fo_hvsp_out_window.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
			plane_ctx->fo_hvsp_out_window.h, old_plane_ctx->fo_hvsp_out_window.h);

	if (stub || bUpdate) {
		DRM_INFO("[%s][%d] fo_hvsp input win[w,h]:[%lld, %lld]\n",
			__func__, __LINE__,
			plane_ctx->fo_hvsp_in_window.w, plane_ctx->fo_hvsp_in_window.h);
		DRM_INFO("[%s][%d] fo_hvsp output win[w,h]:[%lld, %lld]\n",
			__func__, __LINE__,
			plane_ctx->fo_hvsp_out_window.w, plane_ctx->fo_hvsp_out_window.h);

		// H scaling ratio
		if (plane_ctx->fo_hvsp_in_window.w != plane_ctx->fo_hvsp_out_window.w) {
			H_ScalingRatio(plane_ctx->fo_hvsp_in_window.w,
				plane_ctx->fo_hvsp_out_window.w);
			HorRatio = (uint32_t)result;
			Hscale_enable = 1;
		} else {
			Hscale_enable = 0;
			HorRatio = SHIFT_20_BITS;
		}
		// H scaling factor
		if (plane_ctx->fo_hvsp_out_window.w > plane_ctx->fo_hvsp_in_window.w) {
			/*Scale Up*/
			H_ScalingUpPhase(plane_ctx->fo_hvsp_in_window.w,
				plane_ctx->fo_hvsp_out_window.w);
			HorIniFactor = (uint32_t)Phaseresult;
			IsScalingUp_H = 1;
		} else if (plane_ctx->fo_hvsp_out_window.w < plane_ctx->fo_hvsp_in_window.w) {
			H_ScalingDownPhase(plane_ctx->fo_hvsp_in_window.w,
				plane_ctx->fo_hvsp_out_window.w);
			HorIniFactor = (uint32_t)Phaseresult;
			IsScalingUp_H = 0;
		} else {
			HorIniFactor = 0;
		}
		DRM_INFO("[%s][%d][FO_HVSP] H Ratio:%x, Initfactor: %x, ScalingUp %d\n",
			__func__, __LINE__, HorRatio, HorIniFactor, IsScalingUp_H);

		// V scaling ratio
		if (plane_ctx->fo_hvsp_in_window.h != plane_ctx->fo_hvsp_out_window.h) {
			V_ScalingRatio(plane_ctx->fo_hvsp_in_window.h,
				plane_ctx->fo_hvsp_out_window.h);
			Vscale_enable = 1;
			VerRatio = (uint32_t)result;
		} else {
			Vscale_enable = 0;
			VerRatio = SHIFT_20_BITS;
		}
		// V scaling factor
		if (plane_ctx->fo_hvsp_out_window.h > plane_ctx->fo_hvsp_in_window.h) {
			/*Scale Up*/
			V_ScalingUpPhase(plane_ctx->fo_hvsp_in_window.h,
				plane_ctx->fo_hvsp_out_window.h);
			VerIniFactor = (uint32_t)Phaseresult;
			IsScalingUp_V = 1;
		} else if (plane_ctx->fo_hvsp_out_window.h < plane_ctx->fo_hvsp_in_window.h) {
			V_ScalingDownPhase(plane_ctx->fo_hvsp_in_window.h,
				plane_ctx->fo_hvsp_out_window.h);
			VerIniFactor = (uint32_t)Phaseresult;
			IsScalingUp_V = 0;
		} else {
			VerIniFactor = 0;
		}
		DRM_INFO("[%s][%d] V Ratio:%x, Inifactor: %x, ScalingUp %d\n",
			__func__, __LINE__, VerRatio, VerIniFactor, IsScalingUp_V);

		// Set param
		if (Hscale_enable && IsScalingUp_H)
			paramScalingIn.H_shift_mode = 1;
		else
			paramScalingIn.H_shift_mode = 0;

		if (Vscale_enable && IsScalingUp_V)
			paramScalingIn.V_shift_mode = 1;
		else
			paramScalingIn.V_shift_mode = 0;

		if (Hscale_enable || Vscale_enable)
			paramScalingIn.HVSP_bypass = 0;
		else
			paramScalingIn.HVSP_bypass = 1;

		paramScalingIn.Hscale_enable = Hscale_enable;
		paramScalingIn.IsScalingUp_H = IsScalingUp_H;
		paramScalingIn.HorIniFactor = HorIniFactor;
		paramScalingIn.HorRatio = HorRatio;
		paramScalingIn.HInputsize = plane_ctx->fo_hvsp_in_window.w;
		paramScalingIn.HOutputsize = plane_ctx->fo_hvsp_out_window.w;
		paramScalingIn.Vscale_enable = Vscale_enable;
		paramScalingIn.IsScalingUp_V = IsScalingUp_V;
		paramScalingIn.VerIniFactor = VerIniFactor;
		paramScalingIn.VerRatio = VerRatio;
		paramScalingIn.VInputsize = plane_ctx->fo_hvsp_in_window.h;
		paramScalingIn.VOutputsize = plane_ctx->fo_hvsp_out_window.h;

		DRM_INFO("[%s][%d][FO_HVSP]Hscale_enable:%d H_shift_mode:%d Scalingup_H:%d\n",
		__func__, __LINE__, paramScalingIn.Hscale_enable, paramScalingIn.H_shift_mode,
		paramScalingIn.IsScalingUp_H);
		DRM_INFO("[%s][%d][FO_HVSP]HorIniFactor:%x HorRatio:%x\n",
		__func__, __LINE__, paramScalingIn.HorIniFactor, paramScalingIn.HorRatio);
		DRM_INFO("[%s][%d][FO_HVSP]InputSize_H:%d OutputSize_H:%d\n",
		__func__, __LINE__, (int)paramScalingIn.HInputsize,
			(int)paramScalingIn.HOutputsize);
		DRM_INFO("[%s][%d][FO_HVSP]Vscale_enable:%d V_shift_mode:%d Scalingup_V:%d\n",
		__func__, __LINE__, paramScalingIn.Vscale_enable, paramScalingIn.V_shift_mode,
		paramScalingIn.IsScalingUp_V);
		DRM_INFO("[%s][%d][FO_HVSP]VerIniFactor:%x VerRatio:%x\n",
		__func__, __LINE__, paramScalingIn.VerIniFactor, paramScalingIn.VerRatio);
		DRM_INFO("[%s][%d][FO_HVSP]InputSize_V:%d OutputSize_V:%d\n",
		__func__, __LINE__, (int)paramScalingIn.VInputsize,
			(int)paramScalingIn.VOutputsize);

		drv_hwreg_render_display_set_fohvsp_scalingratio(paramScalingIn, &paramOut);

		DRM_INFO("[%s][%d][FO_HVSP] regCount:%d\n",
			__func__, __LINE__, paramOut.regCount);

		if (!g_bUseRIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->vgs_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: vgs_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}
	}
}

static void _mtk_video_set_scaling_ratio(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state)
{
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_SCL];
	struct hwregScalingIn paramScalingIn;
	struct hwregThinReadFactorIn paraThinReadFactorIn;
	struct hwregLinebuffersizeIn paraLBsizeIn;
	struct hwregOut paramOut;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	uint8_t MGW_NUM = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	uint64_t result = 0, Phaseresult = 0;
	uint32_t TmpRatio = 0;
	uint32_t TmpFactor = 0;
	uint32_t HorIniFactor = 0, VerIniFactor = 0;
	uint32_t HorRatio = 0, VerRatio = 0;
	uint8_t Hscale_enable = 0, Vscale_enable = 0;
	uint32_t Thinfactor = 0;
	uint64_t tmpheight = 0;
	uint8_t IsScalingUp_H = 0, IsScalingUp_V = 0;
	uint64_t Rem = 0, DivResult = 0;
	int VB_Version = video_hw_version->video_blending;
	bool force = FALSE;
	bool bUpdate = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paraThinReadFactorIn, 0, sizeof(struct hwregThinReadFactorIn));
	memset(&paramScalingIn, 0, sizeof(struct hwregScalingIn));
	memset(&paraLBsizeIn, 0, sizeof(struct hwregLinebuffersizeIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	//pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	paraThinReadFactorIn.RIU = g_bUseRIU;
	paraThinReadFactorIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	paramScalingIn.RIU = g_bUseRIU;
	paramScalingIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramScalingIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramScalingIn.windowID = plane_idx;

	paraLBsizeIn.RIU = g_bUseRIU;
	paraLBsizeIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (pctx->stub)
		force = TRUE;

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_video_plane_type, video_plane_type);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.w, plane_ctx->crop_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->crop_win.h, plane_ctx->crop_win.h);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.w, plane_ctx->disp_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.h, plane_ctx->disp_win.h);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mgw_plane_num, MGW_NUM);

	if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
		(video_plane_type < MTK_VPLANE_TYPE_MAX))  {

		if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
			paraThinReadFactorIn.MGWPlaneNum =
				pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
			paramScalingIn.MGWPlaneNum =
				pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
			paraLBsizeIn.MGWPlaneNum =
				pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
		}
	} else {
		DRM_INFO("[%s][%d] Invalid  HW plane type\n",
				__func__, __LINE__);
	}

	// set line buffer
	if (VB_Version == EN_VB_VERSION_TS_V1) {
		paraLBsizeIn.enable = 1;
		if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
			if (MGW_NUM == 1)
				paraLBsizeIn.Linebuffersize = plane_ctx->disp_win.w;
			else
				paraLBsizeIn.Linebuffersize = pctx_pnl->info.de_width;
		} else {
			paraLBsizeIn.Linebuffersize = plane_ctx->disp_win.w;
		}
	}

	//-----------------------------------------
	// Horizontal
	//-----------------------------------------
	if (plane_ctx->crop_win.w != plane_ctx->disp_win.w) {
		H_ScalingRatio(plane_ctx->crop_win.w, plane_ctx->disp_win.w);
		TmpRatio = (uint32_t)result;
		Hscale_enable = 1;
	} else {
		Hscale_enable = 0;
		TmpRatio = DEFAULT_SCL_RATIO;
	}
	HorRatio = TmpRatio;
	/// Set Phase Factor
	if (plane_ctx->disp_win.w > plane_ctx->crop_win.w) {
		/*Scale Up*/
		H_ScalingUpPhase(plane_ctx->crop_win.w, plane_ctx->disp_win.w);
		TmpFactor = (uint32_t)Phaseresult;
		IsScalingUp_H = 1;
	} else if (plane_ctx->disp_win.w < plane_ctx->crop_win.w) {
		H_ScalingDownPhase(plane_ctx->crop_win.w, plane_ctx->disp_win.w);
		TmpFactor = (uint32_t)Phaseresult;
		IsScalingUp_H = 0;
	} else {
		TmpFactor = 0;
	}
	HorIniFactor = TmpFactor;

	//Vertical

	//Thin Read
	if (plane_ctx->crop_win.h <= plane_ctx->disp_win.h) {
		Thinfactor = 1;
	} else {
		DivResult = div64_u64_rem(plane_ctx->crop_win.h,
			plane_ctx->disp_win.h, &Rem);
		if ((Rem) == 0)
			Thinfactor = DivResult;
		else
			Thinfactor =
				(((div64_u64(plane_ctx->crop_win.h,
				plane_ctx->disp_win.h))+ROUNDING)>>1)<<1;

	}

	//Thinfactor = ((crop_height/disp_height)>>1)<<1;

	tmpheight = div64_u64(plane_ctx->crop_win.h, Thinfactor);
	plane_ctx->crop_win.h = tmpheight;

	if (tmpheight == plane_ctx->disp_win.h) {
		Vscale_enable = 0;
		TmpRatio = DEFAULT_SCL_RATIO;
	} else {
		V_ScalingRatio(tmpheight, plane_ctx->disp_win.h);
		Vscale_enable = 1;
		TmpRatio = (uint32_t)result;
	}
	VerRatio = TmpRatio;
	if (plane_ctx->disp_win.h > tmpheight) {
		/*Scale Up*/
		V_ScalingUpPhase(tmpheight, plane_ctx->disp_win.h);
		TmpFactor = (uint32_t)Phaseresult;
		IsScalingUp_V = 1;
	} else if (plane_ctx->disp_win.h > tmpheight) {
		V_ScalingDownPhase(tmpheight, plane_ctx->disp_win.h);
		TmpFactor = (uint32_t)Phaseresult;
		IsScalingUp_V = 0;
	} else {
		TmpFactor = 0;
	}
	VerIniFactor =	TmpFactor;

	paraThinReadFactorIn.thinReadFactor = Thinfactor;
	if (Hscale_enable && IsScalingUp_H)
		paramScalingIn.H_shift_mode = 1;
	else
		paramScalingIn.H_shift_mode = 0;

	if (Vscale_enable && IsScalingUp_V)
		paramScalingIn.V_shift_mode = 1;
	else
		paramScalingIn.V_shift_mode = 0;

	if (Hscale_enable || Vscale_enable)
		paramScalingIn.HVSP_bypass = 0;
	else
		paramScalingIn.HVSP_bypass = 1;

	paramScalingIn.Hscale_enable = Hscale_enable;
	paramScalingIn.IsScalingUp_H = IsScalingUp_H;
	paramScalingIn.HorIniFactor = HorIniFactor;
	paramScalingIn.HorRatio = HorRatio;
	paramScalingIn.HInputsize = plane_ctx->crop_win.w;
	paramScalingIn.HOutputsize = plane_ctx->disp_win.w;
	paramScalingIn.Vscale_enable = Vscale_enable;
	paramScalingIn.IsScalingUp_V = IsScalingUp_V;
	paramScalingIn.VerIniFactor = VerIniFactor;
	paramScalingIn.VerRatio = VerRatio;
	paramScalingIn.VInputsize = tmpheight;//crop_height[plane_inx];
	paramScalingIn.VOutputsize = plane_ctx->disp_win.h;

	if (VB_Version == EN_VB_VERSION_TS_V1) {
		drv_hwreg_render_display_set_linebuffersize(
			paraLBsizeIn, &paramOut);
	}

	drv_hwreg_render_display_set_thinReadFactor(paraThinReadFactorIn, &paramOut);
	drv_hwreg_render_display_set_scalingratio(paramScalingIn, &paramOut);

	if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
			(video_plane_type < MTK_VPLANE_TYPE_MAX)) {
		pctx_video->scl_inputWin[video_plane_type].w = plane_ctx->crop_win.w;
		pctx_video->scl_inputWin[video_plane_type].h = tmpheight;
		pctx_video->scl_outputWin[video_plane_type].w = plane_ctx->disp_win.w;
		pctx_video->scl_outputWin[video_plane_type].h = plane_ctx->disp_win.h;
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->vgs_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: vgs_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}

	if (force || bUpdate) {
		DRM_INFO("[%s][%d] crop_win[w,h]:[%lld, %lld]\n"
			, __func__, __LINE__,
			plane_ctx->crop_win.w, plane_ctx->crop_win.h);
		DRM_INFO("[%s][%d] disp_win[w,h]:[%lld, %lld]\n"
			, __func__, __LINE__,
			plane_ctx->disp_win.w, plane_ctx->disp_win.h);

		if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
			(video_plane_type < MTK_VPLANE_TYPE_MAX))  {

			if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
				DRM_INFO("[%s][%d] MGW_NUM:%d\n",
				__func__, __LINE__, paraThinReadFactorIn.MGWPlaneNum);
			}
		} else {
			DRM_INFO("[%s][%d] Invalid  HW plane type\n",
					__func__, __LINE__);
		}

		DRM_INFO("[%s][%d] H_ratio:%x, H_initfactor: %x, ScalingUp %d\n"
			, __func__, __LINE__, HorRatio,
			HorIniFactor, IsScalingUp_H);

		DRM_INFO("[%s][%d] Thin Read factor:%d, crop height: %llu\n"
			, __func__, __LINE__, Thinfactor, tmpheight);

		DRM_INFO("[%s][%d] V_ratio:%x, V_inifactor: %x, ScalingUp %d\n"
			, __func__, __LINE__, VerRatio,
			VerIniFactor, IsScalingUp_V);

		DRM_INFO("[%s][%d][paramIn]Hscale_enable:%d H_shift_mode:%d Scalingup_H:%d\n",
		__func__, __LINE__, paramScalingIn.Hscale_enable, paramScalingIn.H_shift_mode,
		paramScalingIn.IsScalingUp_H);
		DRM_INFO("[%s][%d][paramIn]HorIniFactor:%x HorRatio:%x\n",
		__func__, __LINE__, paramScalingIn.HorIniFactor, paramScalingIn.HorRatio);
		DRM_INFO("[%s][%d][paramIn]InputSize_H:%llu OutputSize_H:%llu\n",
		__func__, __LINE__, paramScalingIn.HInputsize, paramScalingIn.HOutputsize);
		DRM_INFO("[%s][%d][paramIn]Vscale_enable:%d V_shift_mode:%d Scalingup_V:%d\n",
		__func__, __LINE__, paramScalingIn.Vscale_enable, paramScalingIn.V_shift_mode,
		paramScalingIn.IsScalingUp_V);
		DRM_INFO("[%s][%d][paramIn]VerIniFactor:%x VerRatio:%x\n",
		__func__, __LINE__, paramScalingIn.VerIniFactor, paramScalingIn.VerRatio);
		DRM_INFO("[%s][%d][paramIn]InputSize_V:%llu OutputSize_V:%llu\n",
		__func__, __LINE__, paramScalingIn.VInputsize, paramScalingIn.VOutputsize);
	}

	// update fo_hvsp scaling ratio
	_mtk_video_set_fohvsp_scaling_ratio(plane_idx, pctx, state);

}

static void _mtk_video_display_set_disp_win(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct mtk_plane_state *state)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum DRM_VB_VERSION_E VB_version = 0;
	uint8_t MGW_NUM = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_DISP_WIN];
	struct hwregDispWinIn paramDispWinIn;
	struct hwregPreInsertImageSizeIn paramPreInsertImageSizeIn;
	struct hwregPreInsertFrameSizeIn paramPreInsertFrameSizeIn;
	struct hwregBlendOutSizeIn paramBlendOutSizeIn;
	struct hwregVBPreBlendOutSizeIn paramVBPreBlendOutIn;
	struct hwregOut paramOut;
	bool force = FALSE;
	bool bUpdate = FALSE;
	int ret;
	uint16_t disp_h_end_limit = 0, disp_v_end_limit = 0;

	int disp_win_version = 0;

	memset(reg, 0, sizeof(reg));
	memset(&paramDispWinIn, 0, sizeof(struct hwregDispWinIn));
	memset(&paramPreInsertImageSizeIn, 0, sizeof(paramPreInsertImageSizeIn));
	memset(&paramPreInsertFrameSizeIn, 0, sizeof(paramPreInsertFrameSizeIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	pctx = pctx_video_to_kms(pctx_video);
	pctx_pnl = &(pctx->panel_priv);

	if (plane_idx >= MAX_WINDOW_NUM) {
		DRM_ERROR("[%s][%d] Invalid plane_idx:%d !! MAX_WINDOW_NUM:%d\n",
			__func__, __LINE__, plane_idx, MAX_WINDOW_NUM);
		return;
	}

	disp_win_version = video_hw_version->disp_win;
	VB_version = (enum DRM_VB_VERSION_E) video_hw_version->video_blending;

	paramOut.reg = reg;

	paramDispWinIn.RIU = g_bUseRIU;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramDispWinIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramDispWinIn.windowID = plane_idx;

	paramPreInsertImageSizeIn.RIU = g_bUseRIU;
	paramPreInsertImageSizeIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	paramPreInsertFrameSizeIn.RIU = g_bUseRIU;
	paramPreInsertFrameSizeIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (pctx->stub)
		force = TRUE;

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.x, plane_ctx->disp_win.x);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.y, plane_ctx->disp_win.y);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.w, plane_ctx->disp_win.w);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->disp_win.h, plane_ctx->disp_win.h);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_video_plane_type, video_plane_type);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mgw_plane_num, MGW_NUM);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode);

	/* bypass setting if disp win not chaged */
	if (force || bUpdate) {
		DRM_INFO("[%s][%d] plane_idx:%d video_plane_type:%d\n",
			__func__, __LINE__, plane_idx, video_plane_type);

		DRM_INFO("[%s][%d] old_disp_win:[%d, %d, %d, %d]\n"
			, __func__, __LINE__,
			(int)old_plane_ctx->disp_win.x, (int)old_plane_ctx->disp_win.y,
			(int)old_plane_ctx->disp_win.w, (int)old_plane_ctx->disp_win.h);
		DRM_INFO("[%s][%d] disp_win:[%d, %d, %d, %d]\n",
			__func__, __LINE__,
			(int)plane_ctx->disp_win.x, (int)plane_ctx->disp_win.y,
			(int)plane_ctx->disp_win.w, (int)plane_ctx->disp_win.h);

		if (disp_win_version == DISP_WIN_VERSION_VB) {
			disp_h_end_limit = pctx_pnl->info.de_width - 1;
			disp_v_end_limit = pctx_pnl->info.de_height - 1;
		}

		if (disp_win_version == DISP_WIN_VERSION_TGEN) {
			disp_h_end_limit = pctx_pnl->info.de_hstart + pctx_pnl->info.de_width - 1;
			disp_v_end_limit = pctx_pnl->info.de_vstart + pctx_pnl->info.de_height - 1;
		}

		DRM_INFO("[%s][%d] disp_h_end_limit:%d disp_v_end_limit:%d\n"
			, __func__, __LINE__,
			(int)disp_h_end_limit, (int)disp_v_end_limit);

		paramDispWinIn.dispWindowRect.h_start = plane_ctx->disp_win.x;

		if (plane_ctx->disp_win.x + plane_ctx->disp_win.w >= 1) {
			paramDispWinIn.dispWindowRect.h_end =
				plane_ctx->disp_win.x + plane_ctx->disp_win.w - 1;

			if (paramDispWinIn.dispWindowRect.h_end > disp_h_end_limit) {
				DRM_INFO("[%s][%d] plane_idx:%d plane_type:%d over support rect!\n",
					__func__, __LINE__, plane_idx, video_plane_type);

				DRM_INFO("[%s][%d] Refine disp h_end from %d to %d\n",
					__func__, __LINE__,
					(int)paramDispWinIn.dispWindowRect.h_end,
					(int)disp_h_end_limit);

				paramDispWinIn.dispWindowRect.h_end = disp_h_end_limit;

				drv_STUB("disp_win_refine_case_h", 1);
			}
		}

		paramDispWinIn.dispWindowRect.v_start = plane_ctx->disp_win.y;

		if (plane_ctx->disp_win.y + plane_ctx->disp_win.h >= 1) {
			paramDispWinIn.dispWindowRect.v_end =
				plane_ctx->disp_win.y + plane_ctx->disp_win.h - 1;

			if (paramDispWinIn.dispWindowRect.v_end > disp_v_end_limit) {
				DRM_INFO("[%s][%d] plane_idx:%d plane_type:%d over support rect!\n",
					__func__, __LINE__, plane_idx, video_plane_type);
				DRM_INFO("[%s][%d] Refine disp v_end from %d to %d\n",
					__func__, __LINE__,
					(int)paramDispWinIn.dispWindowRect.v_end,
					(int)disp_v_end_limit);

				paramDispWinIn.dispWindowRect.v_end = disp_v_end_limit;

				drv_STUB("disp_win_refine_case_v", 1);
			}
		}

		DRM_INFO("[%s][%d][paramIn]winID:%d,disp rect(h_s,h_e,v_s,v_e):[%d, %d, %d, %d]\n",
			__func__, __LINE__, paramDispWinIn.windowID,
			(int)paramDispWinIn.dispWindowRect.h_start,
			(int)paramDispWinIn.dispWindowRect.h_end,
			(int)paramDispWinIn.dispWindowRect.v_start,
			(int)paramDispWinIn.dispWindowRect.v_end);

		//change render setting to pqu using DS
		if (pctx->sw_caps.enable_pqu_ds_size_ctl == 0)
			drv_hwreg_render_display_set_dispWindow(paramDispWinIn, &paramOut);

		/* only use ext (single win) need to set pre insert image size */
		if (disp_win_version == DISP_WIN_VERSION_VB) {
			if ((video_plane_type != MTK_VPLANE_TYPE_MULTI_WIN) ||
				(MGW_NUM == 1)) {
				paramPreInsertImageSizeIn.imageSizeRect.h_start =
					paramDispWinIn.dispWindowRect.h_start;
				paramPreInsertImageSizeIn.imageSizeRect.h_end =
					paramDispWinIn.dispWindowRect.h_end;
				if (VB_version == EN_VB_VERSION_TS_V0) {
					paramPreInsertImageSizeIn.imageSizeRect.v_start = 0;
					paramPreInsertImageSizeIn.imageSizeRect.v_end =
						paramDispWinIn.dispWindowRect.v_end -
						paramDispWinIn.dispWindowRect.v_start;
						//dispwindow[plane_inx].h - 1;
				} else {
					paramPreInsertImageSizeIn.imageSizeRect.v_start =
						paramDispWinIn.dispWindowRect.v_start;
					paramPreInsertImageSizeIn.imageSizeRect.v_end =
						paramDispWinIn.dispWindowRect.v_end;
				}

				drv_hwreg_render_display_set_preInsertImageSize(
					paramPreInsertImageSizeIn, &paramOut);

				if (VB_version == EN_VB_VERSION_TS_V0) {
					paramPreInsertFrameSizeIn.htt = pctx_pnl->info.de_width - 1;
					paramPreInsertFrameSizeIn.vtt = plane_ctx->disp_win.h - 1;

					drv_hwreg_render_display_set_preInsertFrameSize(
						paramPreInsertFrameSizeIn, &paramOut);
				}
			}
		}

		/* for legacy mode, setup blend_out size to disp_wind_w */
		if (IS_VB_LEGACY(VB_version)) {
			memset(&paramBlendOutSizeIn, 0, sizeof(paramBlendOutSizeIn));
			paramBlendOutSizeIn.RIU = g_bUseRIU;
			paramBlendOutSizeIn.Hsize = plane_ctx->disp_win.w;

			//change render setting to pqu using DS
			if (pctx->sw_caps.enable_pqu_ds_size_ctl == 0) {
				drv_hwreg_render_display_set_blendOut_size(
					paramBlendOutSizeIn, &paramOut);

				if (VB_version == EN_VB_VERSION_LEGACY_V1) {
					memset(&paramVBPreBlendOutIn, 0,
						sizeof(paramVBPreBlendOutIn));
					paramVBPreBlendOutIn.RIU = g_bUseRIU;
					paramVBPreBlendOutIn.Hsize = plane_ctx->disp_win.w;
					drv_hwreg_render_display_set_vb_pre_blendOut_size(
						paramVBPreBlendOutIn, &paramOut);
				}
			}
		}

		if (!g_bUseRIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->vgs_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: vgs_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}
	}
}

static void _mtk_video_display_update_frame_offset(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct drm_framebuffer *fb,
	uint32_t *frame_offset)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_format_info video_format_info;
	struct drm_format_name_buf format_name;
	uint16_t byte_per_word = 0;
	uint16_t render_p_engine = 0;
	uint16_t line_offset = 0;
	uint32_t tmpframeOffset = 0;
	int i = 0;

	pctx = pctx_video_to_kms(pctx_video);
	byte_per_word = pctx->hw_caps.byte_per_word;
	render_p_engine = pctx->hw_caps.render_p_engine;
	line_offset = (fb->width + (render_p_engine - 1)) & ~(render_p_engine - 1);
	memset(&video_format_info, 0, sizeof(struct mtk_video_format_info));

	/*frameOffset formula:*/
	/*frameOffset = (align_256bit[align_4pixel[h_size]*bpp] / 8 / 32 )*[v_size]*/

	/*Get bpp for plane 0*/
	video_format_info.fourcc = fb->format->format;
	if (((fb->modifier >> 56) & 0xF) == DRM_FORMAT_MOD_VENDOR_MTK) {
		video_format_info.modifier = fb->modifier & 0x00ffffffffffffffULL;
		video_format_info.modifier_arrange = video_format_info.modifier & 0xff;
		video_format_info.modifier_compressed = (video_format_info.modifier & 0x100) >> 8;
		video_format_info.modifier_6bit = (video_format_info.modifier & 0x200) >> 9;
		video_format_info.modifier_10bit = (video_format_info.modifier & 0x400) >> 10;
	}

	DRM_INFO("[%s][%d] %s%s %s(0x%08x) %s%llu %s0x%x %s%d %s%d %s%d\n",
		__func__, __LINE__,
		"fourcc:", drm_get_format_name(fb->format->format, &format_name),
		"fourcc:", video_format_info.fourcc,
		"modifier:", video_format_info.modifier,
		"modifier_arrange:", video_format_info.modifier_arrange,
		"modifier_compressed:", video_format_info.modifier_compressed,
		"modifier_6bit:", video_format_info.modifier_6bit,
		"modifier_10bit:", video_format_info.modifier_10bit);


	_mtk_video_display_get_format_bpp(&video_format_info);

	for (i = 0 ; i < MTK_PLANE_DISPLAY_PIPE ; i++) {

		DRM_INFO("[%s][%d] bpp[%d]:%d\n", __func__, __LINE__, i, video_format_info.bpp[i]);

		tmpframeOffset = line_offset * video_format_info.bpp[i];

		tmpframeOffset =
			(tmpframeOffset + (byte_per_word * BITS_PER_BYTE - 1))
			& ~(byte_per_word * BITS_PER_BYTE - 1);

		if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW) {
			frame_offset[i] =
				(tmpframeOffset / BITS_PER_BYTE / byte_per_word) * fb->height;
		} else if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_SW) {
			frame_offset[i] = 0;
		} else {
			DRM_INFO("[%s][%d] no support mem mode:%d\n",
				__func__, __LINE__, plane_ctx->mem_hw_mode);
		}

	}
}

static void _mtk_video_display_set_fb_memory_format(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct drm_framebuffer *fb)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	struct drm_format_name_buf format_name;
	struct hwregMemConfigIn paramIn;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FB_MEM_FORMAT];

	struct hwregOut paramOut;
	bool force = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregMemConfigIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	paramIn.RIU = g_bUseRIU;
	paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramIn.windowID = plane_idx;

	paramIn.fourcc = fb->format->format;

	if (pctx->stub)
		force = TRUE;

	if (((fb->modifier >> 56) & 0xF) == DRM_FORMAT_MOD_VENDOR_MTK) {
		uint64_t modifier = fb->modifier & 0x00ffffffffffffffULL;

		paramIn.modifier = 1;
		paramIn.modifier_arrange = modifier & 0xff;
		paramIn.modifier_compressed = (modifier & 0x100) >> 8;
		paramIn.modifier_6bit = (modifier & 0x200) >> 9;
		paramIn.modifier_10bit = (modifier & 0x400) >> 10;
	}

	if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
			(video_plane_type < MTK_VPLANE_TYPE_MAX))  {

		if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
			paramIn.MGWPlaneNum = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];

			DRM_INFO("[%s][%d] MGW_NUM:%d\n",
				__func__, __LINE__, paramIn.MGWPlaneNum);
		}
	} else {
		DRM_INFO("[%s][%d] Invalid  HW plane type\n",
			__func__, __LINE__);
	}

	DRM_INFO("[%s][%d][paramIn] %s%s %s(0x%08x) %s%d %s0x%x %s%d %s%d %s%d\n",
		__func__, __LINE__,
		"fourcc:", drm_get_format_name(fb->format->format, &format_name),
		"fourcc:", paramIn.fourcc,
		"modifier:", paramIn.modifier,
		"modifier_arrange:", paramIn.modifier_arrange,
		"modifier_compressed:", paramIn.modifier_compressed,
		"modifier_6bit:", paramIn.modifier_6bit,
		"modifier_10bit:", paramIn.modifier_10bit);

	if ((video_plane_type > MTK_VPLANE_TYPE_NONE) &&
		(video_plane_type < MTK_VPLANE_TYPE_MAX))
		drv_hwreg_render_display_set_memConfig(paramIn, &paramOut);

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_fb(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct drm_framebuffer *fb)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_format_info video_format_info;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	uint16_t byte_per_word = 0;
	uint16_t render_p_engine = 0;
	uint8_t MGW_plane_num = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t plane_num_per_frame = fb->format->num_planes;
	int i = 0;
	struct drm_gem_object *gem;
	struct mtk_drm_gem_obj *mtk_gem;
	unsigned int offsets[4] = {0};
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FB];
	struct hwregFrameSizeIn paramFrameSizeIn;
	struct hwregLineOffsetIn paramLineOffsetIn;
	struct hwregFrameOffsetIn paramFrameOffsetIn;
	struct hwregBaseAddrIn paramBaseAddrIn;
	struct hwregOut paramOut;
	bool bUpdate = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramFrameSizeIn, 0, sizeof(struct hwregFrameSizeIn));
	memset(&paramLineOffsetIn, 0, sizeof(struct hwregLineOffsetIn));
	memset(&paramFrameOffsetIn, 0, sizeof(struct hwregFrameOffsetIn));
	memset(&paramBaseAddrIn, 0, sizeof(struct hwregBaseAddrIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(&video_format_info, 0, sizeof(struct mtk_video_format_info));

	pctx = pctx_video_to_kms(pctx_video);
	pctx_pnl = &(pctx->panel_priv);
	render_p_engine = pctx->hw_caps.render_p_engine;
	byte_per_word = pctx->hw_caps.byte_per_word;

	if (plane_idx >= MAX_WINDOW_NUM) {
		DRM_ERROR("[%s][%d] Invalid plane_idx:%d !! MAX_WINDOW_NUM:%d\n",
			__func__, __LINE__, plane_idx, MAX_WINDOW_NUM);
		return;
	}

	paramOut.reg = reg;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW) {
		paramLineOffsetIn.windowID = plane_ctx->hw_mode_win_id;
		paramFrameOffsetIn.windowID = plane_ctx->hw_mode_win_id;
		paramBaseAddrIn.windowID = plane_ctx->hw_mode_win_id;
	} else {
		paramLineOffsetIn.windowID = plane_idx;
		paramFrameOffsetIn.windowID = plane_idx;
		paramBaseAddrIn.windowID = plane_idx;
	}

	paramFrameSizeIn.RIU = g_bUseRIU;
	paramFrameSizeIn.hSize = pctx_pnl->info.de_width;
	paramFrameSizeIn.vSize = pctx_pnl->info.de_height;

	paramLineOffsetIn.RIU = g_bUseRIU;
	paramLineOffsetIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;
	paramLineOffsetIn.lineOffset =
		(fb->width + (render_p_engine - 1)) & ~(render_p_engine - 1);

	paramFrameOffsetIn.RIU = g_bUseRIU;
	paramFrameOffsetIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;
	paramFrameOffsetIn.separateEn = (plane_num_per_frame > 1) ? true : false;

	paramBaseAddrIn.RIU = g_bUseRIU;
	paramBaseAddrIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
		paramLineOffsetIn.MGWPlaneNum = MGW_plane_num;
		paramFrameOffsetIn.MGWPlaneNum = MGW_plane_num;
		paramBaseAddrIn.MGWPlaneNum = MGW_plane_num;
	}

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mgw_plane_num, MGW_plane_num);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_video_plane_type, video_plane_type);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->frame_buf_width, fb->width);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->frame_buf_height, fb->height);

	if (bUpdate || (plane_ctx->disp_mode_info.disp_mode_update)) {
		DRM_INFO("[%s][%d] plane_idx:%d video_plane_type:%d\n",
			__func__, __LINE__, plane_idx, video_plane_type);

		DRM_INFO("[%s][%d] fb_width:%d fb_height:%d\n",
			__func__, __LINE__, fb->width, fb->height);

		/*=========================Set frame size===============================*/
		DRM_INFO("[%s][%d][paramIn] frame_size(%d, %d)\n",
			__func__, __LINE__,
			paramFrameSizeIn.hSize,
			paramFrameSizeIn.vSize);

		drv_hwreg_render_display_set_frameSize(
			paramFrameSizeIn, &paramOut);

		/*=========================Set line offset===============================*/
		DRM_INFO("[%s][%d][paramIn] MGWNum:%d lineOffset:%d\n",
			__func__, __LINE__,
			paramLineOffsetIn.MGWPlaneNum,
			paramLineOffsetIn.lineOffset);

		for (i = 0 ; i < MTK_PLANE_DISPLAY_PIPE ; i++) {
			DRM_INFO("[%s][%d][paramIn] frameOffset[%d]:0x%x\n",
				__func__, __LINE__, i,
				paramFrameOffsetIn.frameOffset[i]);
		}
	}

	drv_hwreg_render_display_set_lineOffset(
		paramLineOffsetIn, &paramOut);

	/*=========================Set frame offset===============================*/
	_mtk_video_display_update_frame_offset(
		plane_idx, pctx_video, fb,
		paramFrameOffsetIn.frameOffset);

	drv_hwreg_render_display_set_frameOffset(
		paramFrameOffsetIn, &paramOut);

	plane_ctx->frame_buf_width = fb->width;
	plane_ctx->frame_buf_height = fb->height;
	if (video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) {
		plane_ctx->mgw_plane_num =
			pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	}

	bUpdate = FALSE;
	for (i = 0; i < plane_num_per_frame; i++) {
		gem = fb->obj[0];
		mtk_gem = to_mtk_gem_obj(gem);
		offsets[i] = fb->offsets[i];
		plane_ctx->base_addr[i] = mtk_gem->dma_addr + offsets[i];

		bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->base_addr[i], plane_ctx->base_addr[i]);

		if (bUpdate)
			DRM_INFO("[%s][%d]fb_plane:%d addr:0x%llx offset:0x%x\n",
				__func__, __LINE__, i, plane_ctx->base_addr[i], offsets[i]);
	}

	paramBaseAddrIn.addr.plane0 =
		div64_u64(plane_ctx->base_addr[0], byte_per_word);
	paramBaseAddrIn.addr.plane1 =
		div64_u64(plane_ctx->base_addr[1], byte_per_word);
	paramBaseAddrIn.addr.plane2 =
		div64_u64(plane_ctx->base_addr[MTK_PLANE_DISPLAY_PIPE-1], byte_per_word);

	drv_hwreg_render_display_set_baseAddr(
		paramBaseAddrIn, &paramOut);

	if (bUpdate) {
		DRM_INFO("[%s][%d] plane_num_per_frame:%d\n",
			__func__, __LINE__, plane_num_per_frame);
		DRM_INFO("[%s][%d]mtk_gem->dma_addr: 0x%llx\n",
			__func__, __LINE__, mtk_gem->dma_addr);

		DRM_INFO("[%s][%d][paramIn]plane0_addr:0x%x plane1_addr:0x%x plane2_addr:0x%x\n",
			__func__, __LINE__,
			paramBaseAddrIn.addr.plane0,
			paramBaseAddrIn.addr.plane1,
			paramBaseAddrIn.addr.plane2);
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
	//========================================================================//
}

static int _mtk_video_display_update_rw_diff_value(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state,
	uint8_t *rwdiff,
	uint8_t *protectVal)
{
	int ret = 0;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	uint8_t windowNum = 0;

	windowNum = pctx->video_priv.plane_num[MTK_VPLANE_TYPE_MEMC] +
		pctx->video_priv.plane_num[MTK_VPLANE_TYPE_MULTI_WIN] +
		pctx->video_priv.plane_num[MTK_VPLANE_TYPE_SINGLE_WIN1] +
		pctx->video_priv.plane_num[MTK_VPLANE_TYPE_SINGLE_WIN2];

	if (pctx->stub)
		pctx_pnl->outputTiming_info.locked_flag = true;

	if ((windowNum == 1) &&
		(pctx_pnl->outputTiming_info.locked_flag == true) &&
		((uint16_t)state->base.crtc_w == pctx_pnl->info.de_width &&
			(uint16_t)state->base.crtc_h == pctx_pnl->info.de_height) &&
		((pctx_pnl->outputTiming_info.eFrameSyncMode == //vtt-var
			VIDEO_CRTC_FRAME_SYNC_VTTV &&
			pctx_pnl->outputTiming_info.u8FRC_in == FRC_ONE &&
				(pctx_pnl->outputTiming_info.u8FRC_out == FRC_ONE ||
				pctx_pnl->outputTiming_info.u8FRC_out == FRC_TWO)) ||
		(pctx_pnl->outputTiming_info.eFrameSyncMode == //vrr
			VIDEO_CRTC_FRAME_SYNC_FRAMELOCK &&
			pctx_pnl->outputTiming_info.u8FRC_in == FRC_ONE &&
			(pctx_pnl->outputTiming_info.u8FRC_out == FRC_ONE ||
			pctx_pnl->outputTiming_info.u8FRC_out == FRC_TWO)))) {
		*rwdiff = 0;
		//enable DMA wbank protect
		switch (pctx_pnl->outputTiming_info.eFrameSyncMode) {
		case VIDEO_CRTC_FRAME_SYNC_VTTV:
			if (pctx_pnl->outputTiming_info.u8FRC_in == FRC_ONE) {
				if (pctx_pnl->outputTiming_info.u8FRC_out == FRC_TWO) {
					*protectVal =
					(((state->base.src_h) >> SHIFT_16_BITS)
					/ RBANK_PROT_POSITION)
					/ MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_DIV;
				} else {
					*protectVal = 0;
				}
			} else {
				*protectVal = 0;
			}

			if (*protectVal > MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_MAX)
				*protectVal = MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_MAX;
			break;
		case VIDEO_CRTC_FRAME_SYNC_FRAMELOCK:
			*protectVal = 0;
			break;
		default:
			DRM_ERROR("[%s][%d] invalid framesync mode!!\n",
				__func__, __LINE__);
			ret = 1;
		}
	} else {
		*rwdiff = 1;
		*protectVal = 0;
	}

	return ret;
}

static void _mtk_video_display_set_rw_diff(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	uint8_t rwdiff = 0;
	uint16_t CurrentRWdiff = 0;
	uint8_t protectVal = 0;
	int ret_upd = 0;

	struct reg_info reg[HWREG_VIDEO_REG_NUM_RW_DIFF];
	struct hwregProtectValIn paramProtectValIn;
	struct hwregRwDiffIn paramRwDiffIn;
	struct hwregOut paramOut;
	bool force = FALSE;
	bool bUpdate = FALSE;
	int ret;

	memset(reg, 0, sizeof(reg));
	memset(&paramProtectValIn, 0, sizeof(struct hwregProtectValIn));
	memset(&paramRwDiffIn, 0, sizeof(struct hwregRwDiffIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		paramRwDiffIn.windowID = plane_ctx->hw_mode_win_id;
	else
		paramRwDiffIn.windowID = plane_idx;

	ret_upd = _mtk_video_display_update_rw_diff_value(
		paramRwDiffIn.windowID, pctx, state, &rwdiff, &protectVal);

	if (ret_upd != 0) {//error case
		DRM_INFO("[%s][%d][paramIn] ret_upd:%d\n", __func__, __LINE__, ret_upd);
		rwdiff = 1;
		protectVal = 0;
	}

	if (pctx->stub)
		force = TRUE;

	if ((gMtkDrmDbg.set_rw_diff != NULL) &&
		(gMtkDrmDbg.set_rw_diff->win_en[plane_idx])) {
		rwdiff = gMtkDrmDbg.set_rw_diff->rw_diff[plane_idx];
	}

	drv_hwreg_render_display_get_rwDiff(plane_idx, &CurrentRWdiff);
	plane_ctx->rwdiff = (uint8_t)CurrentRWdiff;

	bUpdate |= _mtk_video_display_is_variable_updated(
		plane_ctx->rwdiff, rwdiff);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->protectVal, protectVal);

	if (force || bUpdate) {
		DRM_INFO("[%s][%d] framesync_mode: %d, locked_flag: %d, de(%d, %d)\n",
			__func__, __LINE__,
			pctx_pnl->outputTiming_info.eFrameSyncMode,
			pctx_pnl->outputTiming_info.locked_flag,
			pctx_pnl->info.de_width, pctx_pnl->info.de_height);

		DRM_INFO("[%s][%d] u8FRC_in: %d, u8FRC_out: %d\n",
			__func__, __LINE__, pctx_pnl->outputTiming_info.u8FRC_in,
			pctx_pnl->outputTiming_info.u8FRC_out);

		DRM_INFO("[%s][%d] rwdiff(old, new): (%d, %d)\n",
			__func__, __LINE__, plane_ctx->rwdiff, rwdiff);
		DRM_INFO("[%s][%d] protectVal(old, new): (%d, %d)\n",
			__func__, __LINE__, plane_ctx->protectVal, protectVal);

		paramOut.reg = reg;

		paramProtectValIn.RIU = g_bUseRIU;
		paramProtectValIn.protectVal = protectVal;
		DRM_INFO("[%s][%d][paramIn] RIU:%d protectVal:%d\n",
			__func__, __LINE__,
			paramProtectValIn.RIU, paramProtectValIn.protectVal);
		drv_hwreg_render_display_set_rbankRefProtect(
			paramProtectValIn, &paramOut);

		paramRwDiffIn.RIU = g_bUseRIU;
		paramRwDiffIn.rwDiff = rwdiff;
		DRM_INFO("[%s][%d][paramIn] RIU:%d windowID:%d, rwDiff:%d\n",
			__func__, __LINE__,
			paramRwDiffIn.RIU, paramRwDiffIn.windowID, paramRwDiffIn.rwDiff);
		drv_hwreg_render_display_set_rwDiff(
			paramRwDiffIn, &paramOut);

		plane_ctx->rwdiff = rwdiff;
		plane_ctx->protectVal = protectVal;

		if (!g_bUseRIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}
	}
}

static void _mtk_video_display_set_freeze(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = NULL;
	struct video_plane_prop *plane_props = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct mtk_video_plane_ctx *old_plane_ctx = NULL;
	enum VIDEO_FREEZE_VERSION_E freeze_version = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FREEZE];
	struct hwregFreezeIn paramIn;
	struct hwregOut paramOut;
	enum drm_mtk_video_plane_type video_plane_type;
	int ret = 0;
	uint8_t sw_auto_freeze_en = 0;
	uint8_t user_freeze_en = 0;
	bool freeze_en = false;
	bool bUpdate = FALSE;
	static uint16_t memc_windowID = MEMC_WINID_DEFAULT;

	if (!pctx)
		return;

	pctx_video = &pctx->video_priv;
	plane_props = (pctx_video->video_plane_properties + plane_idx);
	plane_ctx = (pctx_video->plane_ctx + plane_idx);
	old_plane_ctx = pctx_video->old_plane_ctx + plane_idx;
	video_plane_type = plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	freeze_version = pctx_video->video_hw_ver.video_freeze;

	/* get sw auto freeze */
	if (freeze_version != EN_VIDEO_FREEZE_VERSION_INHWREG)
		drv_hwreg_render_display_get_swAutoFreeze(&sw_auto_freeze_en);

	/* get user freeze */
	user_freeze_en = plane_props->prop_val[EXT_VPLANE_PROP_FREEZE];

	if (sw_auto_freeze_en || user_freeze_en)
		freeze_en = true;

	plane_ctx->freeze = freeze_en;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregFreezeIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	/* use RIU to set frreze/unfreeze, so that it can be effective immediately at bk_upd*/
	paramIn.RIU = TRUE;

	paramIn.enable = plane_ctx->freeze;

	bUpdate |= (_mtk_video_display_is_variable_updated(
		old_plane_ctx->freeze, plane_ctx->freeze));
	bUpdate	|= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode);

	if (bUpdate) {
		if ((video_plane_type >= MTK_VPLANE_TYPE_MULTI_WIN) &&
			(video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2)) {
			if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
				paramIn.windowID = plane_ctx->hw_mode_win_id;
			else
				paramIn.windowID = plane_idx;

			DRM_INFO("[%s][%d] plane_idx:%d freeze:%d freeze_changed:%d\n",
				__func__, __LINE__,
				plane_idx, plane_ctx->freeze, bUpdate);

			drv_hwreg_render_display_set_freeze(paramIn, &paramOut);
		}

		if ((plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE]
			== MTK_VPLANE_TYPE_MEMC) && (paramIn.enable == 1)) {
			DRM_INFO("[MEMC][%s][%d] memc_windowID:%d --> %d.\n", __func__, __LINE__,
				memc_windowID, paramIn.windowID);
			memc_windowID = paramIn.windowID;
		}

		if (memc_windowID == paramIn.windowID) {
			mtk_video_display_set_frc_freeze(pctx, plane_idx, paramIn.enable);
			if (paramIn.enable == 0) {
				memc_windowID = MEMC_WINID_DEFAULT;
			}
		}

		if (!paramIn.RIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}
	}
}

static void _mtk_video_display_set_BackgroundAlpha(
	struct mtk_tv_kms_context *pctx)
{
	enum drm_mtk_video_plane_type plane_type = MTK_VPLANE_TYPE_NONE;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_BG_ALPHA];
	struct hwregPostFillBgARGBIn paramIn;
	struct hwregOut paramOut;

	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	// set background alpha to @WINDOW_ALPHA_DEFAULT_VALUE
	memset(&paramIn, 0, sizeof(struct hwregPostFillBgARGBIn));
	paramIn.RIU = g_bUseRIU;
	paramIn.bgARGB.alpha = WINDOW_ALPHA_DEFAULT_VALUE;
	for (plane_type = MTK_VPLANE_TYPE_MEMC;
		plane_type < MTK_VPLANE_TYPE_SINGLE_WIN2;
		++plane_type) {
		memset(reg, 0, sizeof(reg));
		paramOut.regCount = 0;
		paramIn.planeType = (enum drm_hwreg_video_plane_type)plane_type;
		drv_hwreg_render_display_set_postFillBgAlpha(paramIn, &paramOut);
	}

}

static void _mtk_video_display_set_windowAlpha(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum drm_mtk_video_plane_type plane_type = MTK_VPLANE_TYPE_NONE;
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_WINDOW_ALPHA];
	struct hwregPostFillWindowARGBIn paramPostFillWindowARGBIn;
	struct hwregVBPrePreInsertARGBIn paramIn_vb_pre;
	struct hwregOut paramOut;
	bool bRIU = TRUE;

	memset(&paramOut, 0, sizeof(paramOut));
	paramOut.reg = reg;

	/* For VB*/
	// set window alpha to @WINDOW_ALPHA_DEFAULT_VALUE
	memset(&paramPostFillWindowARGBIn, 0, sizeof(struct hwregPostFillWindowARGBIn));
	paramPostFillWindowARGBIn.RIU = bRIU;
	paramPostFillWindowARGBIn.windowARGB.alpha = WINDOW_ALPHA_DEFAULT_VALUE;
	for (plane_type = MTK_VPLANE_TYPE_MEMC;
		plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
		++plane_type) {
		memset(reg, 0, sizeof(reg));
		paramOut.regCount = 0;
		paramPostFillWindowARGBIn.planeType = (enum drm_hwreg_video_plane_type)plane_type;
		drv_hwreg_render_display_set_postFillWindowAlpha(
			paramPostFillWindowARGBIn, &paramOut);
	}

	/* For VB_PRE */
	memset(&paramIn_vb_pre, 0, sizeof(struct hwregVBPrePreInsertARGBIn));
	if (VB_Version == EN_VB_VERSION_LEGACY_V1) {
		paramIn_vb_pre.RIU = bRIU;
		paramIn_vb_pre.windowARGB.alpha = WINDOW_ALPHA_DEFAULT_VALUE;
		drv_hwreg_render_display_set_vb_pre_preInsertAlpha(
			paramIn_vb_pre,
			&paramOut);
	}

}

static void _mtk_video_display_set_CfdSetting(
	struct mtk_tv_kms_context *pctx)
{
	static struct ST_PQ_CFD_SET_INPUT_CSC_PARAMS st_cfd_set_csc_in = {0};
	static struct ST_PQ_CFD_SET_CUS_CSC_INFO st_cfd_set_csc_out = {0};

	if (pctx == NULL) {
		DRM_ERROR("[%s][%d] NULL ptr pctx\n", __func__, __LINE__);
		return;
	}

	st_cfd_set_csc_in.u8InputDataFormat = E_PQ_CFD_DATA_FORMAT_YUV444;
	st_cfd_set_csc_in.u8InputRange = E_PQ_CFD_COLOR_RANGE_FULL;
	st_cfd_set_csc_in.u8InputColorSpace = 0;

	if ((pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0400) ||
		(pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0500)) { // 5876, 5879
		// For 5876 and 5879, the position of VOP2 3x3 is fixed behind MEMC Y2R,
		// Thus, MEMC Y2R should be disabled or bypassed.
		// That is, input color info should be the same as the output color info.
		st_cfd_set_csc_in.u8OutputDataFormat = st_cfd_set_csc_in.u8InputDataFormat;
		st_cfd_set_csc_in.u8OutputRange = st_cfd_set_csc_in.u8InputRange;
		st_cfd_set_csc_in.u8OutputColorSpace = st_cfd_set_csc_in.u8InputColorSpace;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MEMC;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_out);
	} else {
		st_cfd_set_csc_in.u8OutputDataFormat = E_PQ_CFD_DATA_FORMAT_RGB;
		st_cfd_set_csc_in.u8OutputRange = E_PQ_CFD_COLOR_RANGE_FULL;
		st_cfd_set_csc_in.u8OutputColorSpace = 0;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MGW;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_out);
		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA0;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_out);
		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA1;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_out);
		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MEMC;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_out);
	}

	st_cfd_set_csc_in.u8InputDataFormat = E_PQ_CFD_DATA_FORMAT_RGB;
	st_cfd_set_csc_in.u8InputRange = E_PQ_CFD_COLOR_RANGE_FULL;
	st_cfd_set_csc_in.u8InputColorSpace = 0;

	st_cfd_set_csc_in.u8OutputDataFormat = E_PQ_CFD_DATA_FORMAT_RGB;
	st_cfd_set_csc_in.u8OutputRange = E_PQ_CFD_COLOR_RANGE_FULL;
	st_cfd_set_csc_in.u8OutputColorSpace = 0;
}

static int __set_osdb_mute(struct mtk_video_context *pctx_video,
		bool mute_en, bool use_riu)
{
	int ret = 0;
	struct hwregVideoMuteIn MuteOSDBParamIn = { };
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MUTE] = { };
	struct hwregOut paramOut = { };

	if (!pctx_video)
		return -EINVAL;

	paramOut.reg = reg;

	// mute param
	MuteOSDBParamIn.RIU = use_riu;
	MuteOSDBParamIn.muteEn = mute_en;

	drv_hwreg_render_display_set_video_mute(MuteOSDBParamIn, &paramOut);

	if (!use_riu) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}

	return ret;
}

static int __set_vb_mute(struct mtk_video_context *pctx_video,
		unsigned int plane_idx,
		bool mute_en, bool use_riu)
{
	struct video_plane_prop *plane_props;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct mtk_video_plane_ctx *old_plane_ctx = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MUTE] = { };
	struct hwregPostFillMuteScreenIn MuteVBParamIn = { };
	struct hwregAutoNoSignalIn AutoNoSignalParamIn = { };
	struct hwregOut paramOut = { };
	enum drm_mtk_video_plane_type video_plane_type;
	enum drm_mtk_video_plane_type old_video_plane_type;
	bool bmuteSWModeEn = FALSE;
	bool old_mute_en = FALSE;
	bool plane_type_changed = FALSE;
	bool mute_changed = FALSE;
	bool mem_mode_changed = FALSE;
	int ret = 0;

	if (!pctx_video)
		return -EINVAL;

	plane_ctx = pctx_video->plane_ctx + plane_idx;
	plane_props = (pctx_video->video_plane_properties + plane_idx);
	video_plane_type = plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	old_video_plane_type = plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	old_plane_ctx = pctx_video->old_plane_ctx + plane_idx;
	old_mute_en = plane_props->old_prop_val[EXT_VPLANE_PROP_MUTE_SCREEN];

	plane_type_changed =
		(_mtk_video_display_is_variable_updated(old_video_plane_type, video_plane_type));

	mute_changed =
		(_mtk_video_display_is_variable_updated(old_mute_en, mute_en));

	mem_mode_changed = (_mtk_video_display_is_variable_updated(
		old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode));

	paramOut.reg = reg;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW) {
		bmuteSWModeEn = FALSE;
		MuteVBParamIn.windowID = plane_ctx->hw_mode_win_id;
	} else {
		bmuteSWModeEn = TRUE;
		MuteVBParamIn.windowID = plane_idx;
	}

	if ((old_video_plane_type != MTK_VPLANE_TYPE_NONE) && (plane_type_changed)) {
		MuteVBParamIn.RIU = use_riu;
		MuteVBParamIn.planeType =
			(enum drm_hwreg_video_plane_type)old_video_plane_type;
		MuteVBParamIn.muteEn = !mute_en;
		MuteVBParamIn.muteSWModeEn = bmuteSWModeEn;
		drv_hwreg_render_display_set_postFillMuteScreen(MuteVBParamIn, &paramOut);
	}

	if (old_mute_en && mem_mode_changed) {
		MuteVBParamIn.RIU = use_riu;
		MuteVBParamIn.planeType =
			(enum drm_hwreg_video_plane_type)old_video_plane_type;
		MuteVBParamIn.muteEn = !old_mute_en;
		MuteVBParamIn.muteSWModeEn = !old_plane_ctx->mem_hw_mode;
		drv_hwreg_render_display_set_postFillMuteScreen(MuteVBParamIn, &paramOut);
	}

	/* Mute parameter */
	MuteVBParamIn.RIU = use_riu;
	MuteVBParamIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;
	MuteVBParamIn.muteEn = mute_en;
	MuteVBParamIn.muteSWModeEn = bmuteSWModeEn;

	if (mute_changed || plane_type_changed || mem_mode_changed) {
		DRM_INFO("[%s][%d][paramIn]winID:%d, planeType:%d, muteEn:%d muteSWModeEn:%d\n",
			__func__, __LINE__,
			MuteVBParamIn.windowID, MuteVBParamIn.planeType,
			MuteVBParamIn.muteEn, MuteVBParamIn.muteSWModeEn);
	}

	/* Auto no signal parameter*/
	AutoNoSignalParamIn.RIU = use_riu;
	AutoNoSignalParamIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		AutoNoSignalParamIn.bEnable = TRUE;
	else
		AutoNoSignalParamIn.bEnable = FALSE;

	drv_hwreg_render_display_set_auto_no_signal(AutoNoSignalParamIn, &paramOut);
	drv_hwreg_render_display_set_postFillMuteScreen(MuteVBParamIn, &paramOut);

	if (!use_riu) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}

	return ret;
}

static void _mtk_video_display_set_mute(unsigned int plane_idx,
	struct mtk_video_context *pctx_video)
{
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct mtk_video_plane_ctx *old_plane_ctx = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	struct video_plane_prop *plane_props = NULL;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;
	int ret = 0;
	uint8_t sw_auto_mute_en = 0;
	uint8_t user_mute_en = 0;
	bool mute_en = false;
	bool mute_changed = false;

	if (!pctx_video)
		return;

	plane_props = pctx_video->video_plane_properties + plane_idx;
	plane_ctx = (pctx_video->plane_ctx + plane_idx);
	old_plane_ctx = pctx_video->old_plane_ctx + plane_idx;

	video_hw_version = &pctx_video->video_hw_ver;
	video_mute_hw_ver = video_hw_version->video_mute;

	/* get sw auto mute */
	/* Legacy chip let HWREG judge SW mute*/
	if (video_mute_hw_ver != EN_VIDEO_MUTE_VERSION_OSDB)
		drv_hwreg_render_display_get_swAutoMute(&sw_auto_mute_en);

	/* get user mute */
	user_mute_en = plane_props->prop_val[EXT_VPLANE_PROP_MUTE_SCREEN];

	if (sw_auto_mute_en || user_mute_en)
		mute_en = true;

	plane_ctx->mute = mute_en;

	mute_changed = (_mtk_video_display_is_variable_updated(
		old_plane_ctx->mute, plane_ctx->mute));

	if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_OSDB) {
		ret = __set_osdb_mute(pctx_video, plane_ctx->mute, true);
		if (ret) {
			DRM_ERROR("set osdb mute failed with %d\n", ret);
			return;
		}
	} else if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_VB) {
		ret = __set_vb_mute(pctx_video, plane_idx, plane_ctx->mute, g_bUseRIU);
		if (ret) {
			DRM_ERROR("set vb mute failed with %d\n", ret);
			return;
		}
	} else {
		DRM_WARN("[%s][%d] not support video_mute_hw_ver:%d\n",
			__func__, __LINE__, video_mute_hw_ver);
	}

	if (mute_changed) {
		if (plane_ctx->mute) {
			trace_printk("mute video\n");
			DRM_INFO("mute video in update plane\n");
		} else {
			trace_printk("unmute video\n");
			DRM_INFO("unmute video in update plane\n");
		}

		if (!plane_ctx->mute)
			boottime_print("MTK render unmute [end]\n");
	}
}

static void _mtk_video_display_set_blend_size(struct mtk_tv_kms_context *pctx, bool binit)
{
	struct mtk_panel_context *pctx_pnl = &(pctx->panel_priv);
	struct mtk_video_context *pctx_video = &(pctx->video_priv);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	struct hwregBlendOutSizeIn paramBlendOutSizeIn;
	struct hwregPreInsertFrameSizeIn paramPreInsertFrameSizeIn;
	struct hwregPostFillFrameSizeIn paramPostFillFrameSizeIn;
	struct hwregOut paramOut;
	enum drm_mtk_video_plane_type plane_type = MTK_VPLANE_TYPE_NONE;
	enum DRM_VB_VERSION_E VB_version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	int disp_win_version = video_hw_version->disp_win;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_OUT_SIZE];

	static uint16_t oldPanelDE_W, oldPanelDE_H;
	bool bRIU = g_bUseRIU;
	bool bUpdate = FALSE;
	int ret;

	memset(&paramOut, 0, sizeof(paramOut));
	paramOut.reg = reg;

	memset(reg, 0, sizeof(reg));

	if (binit)
		bRIU = TRUE;
	else
		bRIU = FALSE;

	bUpdate |= _mtk_video_display_is_variable_updated(
			oldPanelDE_W, pctx_pnl->info.de_width);
	bUpdate |= _mtk_video_display_is_variable_updated(
			oldPanelDE_H, pctx_pnl->info.de_height);

	if (binit || bUpdate) {
		DRM_INFO("[%s][%d][old] panel_DE_old:(%d, %d), panel_DE_cur:(%d, %d)",
			__func__, __LINE__,
			oldPanelDE_W, oldPanelDE_H,
			pctx_pnl->info.de_width, pctx_pnl->info.de_height);

		/*
		 * set de_size to blend_out
		 * for legacy mode, move blend_out setup to set_disp_win
		 */
		if (IS_VB_TS(VB_version)) {
			memset(&paramBlendOutSizeIn, 0, sizeof(paramBlendOutSizeIn));
			paramBlendOutSizeIn.RIU = bRIU;
			if (VB_version == EN_VB_VERSION_TS_V0)
				paramBlendOutSizeIn.Hsize =
					pctx_pnl->info.de_width / BLEND_VER0_HSZIE_UNIT;
			else
				paramBlendOutSizeIn.Hsize = pctx_pnl->info.de_width;

			drv_hwreg_render_display_set_blendOut_size(paramBlendOutSizeIn, &paramOut);
		}

		if (disp_win_version == DISP_WIN_VERSION_VB) {
			/* set panel de_size to pre insert frame size */
			memset(&paramPreInsertFrameSizeIn, 0, sizeof(paramPreInsertFrameSizeIn));
			paramPreInsertFrameSizeIn.RIU = bRIU;
			paramPreInsertFrameSizeIn.htt = pctx_pnl->info.de_width - 1;
			paramPreInsertFrameSizeIn.vtt = pctx_pnl->info.de_height - 1;

			for (plane_type = MTK_VPLANE_TYPE_MEMC;
				plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2; ++plane_type) {
				paramPreInsertFrameSizeIn.planeType =
					(enum drm_hwreg_video_plane_type)plane_type;

				drv_hwreg_render_display_set_preInsertFrameSize(
					paramPreInsertFrameSizeIn, &paramOut);
			}

			/* set panel de_size to post fill frame size */
			memset(&paramPostFillFrameSizeIn, 0, sizeof(paramPostFillFrameSizeIn));
			paramPostFillFrameSizeIn.RIU = bRIU;
			paramPostFillFrameSizeIn.htt = pctx_pnl->info.de_width - 1;
			paramPostFillFrameSizeIn.vtt = pctx_pnl->info.de_height - 1;

			for (plane_type = MTK_VPLANE_TYPE_MEMC;
				plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2; ++plane_type) {
				paramPostFillFrameSizeIn.planeType =
					(enum drm_hwreg_video_plane_type)plane_type;

				drv_hwreg_render_display_set_postFillFrameSize(
					paramPostFillFrameSizeIn, &paramOut);

			}
		}

		if (!bRIU) {
			ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)reg);
			if (ret) {
				DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
					__func__, __LINE__, ret);
			}
		}

		oldPanelDE_W = pctx_pnl->info.de_width;
		oldPanelDE_H = pctx_pnl->info.de_height;
	}
}

static void _mtk_video_display_set_enable_pre_insert(
	struct mtk_video_context *pctx_video)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	struct hwregPreInsertEnableIn paramIn;
	struct hwregOut paramOut;
	enum drm_mtk_video_plane_type video_plane_type = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_PRE_INSERT_EN];
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	bool force = FALSE;
	int ret;

	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(paramOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	/* enable pre insert */
	paramIn.RIU = g_bUseRIU;

	if (pctx->stub)
		force = TRUE;

	for (video_plane_type = MTK_VPLANE_TYPE_MEMC;
					video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
					++video_plane_type) {
		paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

		if (pctx_video->plane_num[video_plane_type] == 1) {
			/*VB version 0: MGW single win use ext-bank*/
			/*VB version 1: MGW single win use multi-bank*/
			if ((paramIn.planeType == VPLANE_TYPE_MULTI_WIN) &&
				(VB_Version == EN_VB_VERSION_TS_V0)) {
				paramIn.enable = false;
			} else {
				paramIn.enable = true;
			}
		} else
			paramIn.enable = false;

		if (force || (paramIn.enable != pctx_video->preinsertEnable[video_plane_type])) {
			drv_hwreg_render_display_set_preInsertEnable(paramIn, &paramOut);

			pctx_video->preinsertEnable[video_plane_type] = paramIn.enable;
		}
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_enable_post_fill(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct hwregPostFillEnableIn paramIn;
	struct hwregOut paramOut;
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_POST_FILL_EN];
	enum drm_mtk_video_plane_type video_plane_type = 0;
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	bool enable = false;
	bool force = FALSE;
	int ret;

	memset(&paramIn, 0, sizeof(paramIn));
	memset(&paramOut, 0, sizeof(paramOut));

	paramOut.reg = reg;

	/* enable post fill */
	paramIn.RIU = g_bUseRIU;

	if (pctx->stub)
		force = TRUE;

	for (video_plane_type = MTK_VPLANE_TYPE_MEMC;
					video_plane_type <= MTK_VPLANE_TYPE_SINGLE_WIN2;
					++video_plane_type) {
		paramIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

		if (pctx_video->plane_num[video_plane_type] >= 1) {
			if (paramIn.planeType == VPLANE_TYPE_MULTI_WIN) {
				if (VB_Version == EN_VB_VERSION_TS_V0)
					enable = false;
				else
					enable = true;
			} else
				enable = true;
		}
		paramIn.enable = enable;

		if (force || (paramIn.enable != pctx_video->postfillEnable[video_plane_type])) {
			drv_hwreg_render_display_set_postFillEnable(paramIn, &paramOut);

			pctx_video->postfillEnable[video_plane_type] = paramIn.enable;
		}
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_post_fill_win_enable(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	bool enable)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		old_plane_ctx->video_plane_type;
	uint16_t oldPostFillWinEn = old_plane_ctx->post_fill_win_en;
	struct hwregPostFillWinEnableIn postFillWinEnIn;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_POST_FILL_WIN_EN];
	struct mtk_tv_kms_context *pctx = NULL;
	bool force = FALSE;
	bool bUpdate = FALSE;
	int ret;

	memset(&postFillWinEnIn, 0, sizeof(postFillWinEnIn));
	memset(&paramOut, 0, sizeof(paramOut));

	pctx = pctx_video_to_kms(pctx_video);

	paramOut.reg = reg;

	if (pctx->stub)
		force = TRUE;

	bUpdate |= _mtk_video_display_is_variable_updated(
			oldPostFillWinEn, enable);
	bUpdate |= _mtk_video_display_is_variable_updated(
			old_video_plane_type, video_plane_type);
	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode);

	if (force || bUpdate) {
		DRM_INFO("[%s][%d] video_plane_type: (%d, %d)\n",
			__func__, __LINE__, old_video_plane_type, video_plane_type);
		DRM_INFO("[%s][%d] postFillWinEn: (%d, %d)\n",
			__func__, __LINE__, oldPostFillWinEn, enable);

		if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
			postFillWinEnIn.windowID = plane_ctx->hw_mode_win_id;
		else
			postFillWinEnIn.windowID = plane_idx;

		postFillWinEnIn.RIU = g_bUseRIU;
		postFillWinEnIn.planeType = (enum drm_hwreg_video_plane_type)
					(!enable ? video_plane_type : old_video_plane_type);
		postFillWinEnIn.enable = !enable;
		drv_hwreg_render_display_set_postFillWinEnable(postFillWinEnIn, &paramOut);

		/* enable post window mask */
		postFillWinEnIn.RIU = g_bUseRIU;
		postFillWinEnIn.planeType = (enum drm_hwreg_video_plane_type)
					(enable ? video_plane_type : old_video_plane_type);
		postFillWinEnIn.enable = enable;
		drv_hwreg_render_display_set_postFillWinEnable(postFillWinEnIn, &paramOut);

		plane_ctx->post_fill_win_en = enable;
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_set_videoblending_en(
	struct mtk_video_context *pctx_video)
{
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_BLENDING_EN];
	struct hwregVBPreEnableIn paramIn_vb_pre;
	struct hwregVBEnableIn paramIn_vb;
	struct hwregOut paramOut;
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;

	memset(&paramIn_vb_pre, 0, sizeof(paramIn_vb_pre));
	memset(&paramIn_vb, 0, sizeof(paramIn_vb));
	memset(&paramOut, 0, sizeof(paramOut));

	paramOut.reg = reg;

	paramIn_vb.RIU = TRUE;
	paramIn_vb.enable = TRUE;

	/* init vb */
	drv_hwreg_render_display_set_vb_enable(paramIn_vb, &paramOut);

	/* init vb pre */
	if (VB_Version == EN_VB_VERSION_LEGACY_V1) {
		paramIn_vb_pre.RIU = TRUE;
		paramIn_vb_pre.enable = TRUE;
		drv_hwreg_render_display_set_vb_pre_enable(
			paramIn_vb_pre,
			&paramOut);
	}
}

static void _mtk_video_display_set_HW_mode(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	struct hwregMemSWmodeIn memSWmodeIn;
	struct hwregExtWinAutoEnIn extWinAutoEnIn;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_HW_MODE];
	bool extWinAutoEn = 0;
	bool bUpdate = FALSE;
	int ret;

	memset(&memSWmodeIn, 0, sizeof(memSWmodeIn));
	memset(&extWinAutoEnIn, 0, sizeof(extWinAutoEnIn));
	memset(&paramOut, 0, sizeof(paramOut));

	paramOut.reg = reg;

	bUpdate |= _mtk_video_display_is_variable_updated(
		old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode);

	if (bUpdate) {
		DRM_INFO("[%s][%d]mem_hw_mode:%d, old_mem_hw_mode:%d\n",
			__func__, __LINE__,
			plane_ctx->mem_hw_mode, old_plane_ctx->mem_hw_mode);
	}

	if (plane_ctx->mem_hw_mode) {
		/*hw mode, see multi bank*/
		extWinAutoEn = TRUE;
	} else {
		/*sw mode, see ext bank*/
		extWinAutoEn = FALSE;
	}

	memSWmodeIn.RIU = g_bUseRIU;
	memSWmodeIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;

	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		memSWmodeIn.windowID = plane_ctx->hw_mode_win_id;
	else
		memSWmodeIn.windowID = plane_idx;

	memSWmodeIn.bSWmode = !plane_ctx->mem_hw_mode;

	extWinAutoEnIn.RIU = g_bUseRIU;
	extWinAutoEnIn.planeType = (enum drm_hwreg_video_plane_type)video_plane_type;
	extWinAutoEnIn.enable = extWinAutoEn;

	drv_hwreg_render_display_set_mem_sw_mode(memSWmodeIn, &paramOut);
	drv_hwreg_render_display_set_extwin_auto_en(extWinAutoEnIn, &paramOut);

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static void _mtk_video_display_update_HW_mode(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video
)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	struct mtk_video_plane_type_ctx *plane_type_ctx;
	struct mtk_video_plane_type_ctx *old_plane_type_ctx;
	enum drm_mtk_vplane_buf_mode buf_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_BUF_MODE];
	enum drm_mtk_video_plane_type video_plane_type;

	bool plane_type_changed =
		plane_props->prop_update[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];

	video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	plane_type_ctx =
		pctx_video->plane_type_ctx + video_plane_type;
	old_plane_type_ctx =
		pctx_video->old_plane_type_ctx + video_plane_type;
	plane_type_ctx->disp_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_DISP_MODE];

	if ((_mtk_video_display_is_variable_updated(
		plane_type_ctx->disp_mode, old_plane_type_ctx->disp_mode)) || plane_type_changed){
		plane_ctx->disp_mode_info.disp_mode_update = TRUE;
		plane_ctx->disp_mode_info.disp_mode = plane_type_ctx->disp_mode;
		DRM_INFO("[%s][%d] disp_mode changed, disp_mode:%d\n",
			__func__, __LINE__, plane_type_ctx->disp_mode);
	} else {
		plane_ctx->disp_mode_info.disp_mode_update = FALSE;
	}

	if (plane_ctx->disp_mode_info.disp_mode_update) {
		if (plane_type_ctx->disp_mode == MTK_VPLANE_DISP_MODE_HW)
			plane_ctx->mem_hw_mode = TRUE;
		else if (plane_type_ctx->disp_mode == MTK_VPLANE_DISP_MODE_SW)
			plane_ctx->mem_hw_mode = FALSE;
		else
			DRM_INFO("[%s, %d]: Not HWorSW mode state\n", __func__, __LINE__);
	} else if ((_mtk_video_display_is_ext_prop_updated(plane_props,
		EXT_VPLANE_PROP_BUF_MODE)) || plane_type_changed) {
		DRM_INFO("[%s][%d] buf_mode changed, buf_mode:%d\n",
			__func__, __LINE__, buf_mode);
		if (buf_mode == MTK_VPLANE_BUF_MODE_HW)
			plane_ctx->mem_hw_mode = TRUE;
		else if (buf_mode == MTK_VPLANE_BUF_MODE_SW)
			plane_ctx->mem_hw_mode = FALSE;
		else
			DRM_INFO("[%s, %d]:Not HWorSW mode state\n", __func__, __LINE__);
	}

	/* hw mode win id always use 0, align with pq mdw win id */
	if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
		plane_ctx->hw_mode_win_id = 0;

}

static int _mtk_video_display_set_blending_layer_enable(
	struct mtk_video_context *pctx_video,
	bool bRIU,
	enum drm_mtk_video_plane_type plane_type,
	uint8_t layer,
	bool layer_enable)
{
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_LAYER_EN];
	struct hwregLayerControlIn paramIn;
	struct hwregOut paramOut;
	int ret = 0;

	memset(&paramIn, 0, sizeof(struct hwregLayerControlIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	paramIn.RIU = bRIU;
	paramIn.layerEn = layer_enable;
	paramIn.layer = layer;
	switch (plane_type) {
	case MTK_VPLANE_TYPE_NONE:
	case MTK_VPLANE_TYPE_MEMC:
		paramIn.srcIndex = BLENDING_LAYER_SRC0;
		break;
	case MTK_VPLANE_TYPE_MULTI_WIN:
		paramIn.srcIndex = BLENDING_LAYER_SRC1;
		break;
	case MTK_VPLANE_TYPE_SINGLE_WIN1:
		paramIn.srcIndex = BLENDING_LAYER_SRC2;
		break;
	case MTK_VPLANE_TYPE_SINGLE_WIN2:
		paramIn.srcIndex = BLENDING_LAYER_SRC3;
		break;
	default:
		break;
	}
	DRM_INFO("[%s][%d][paramIn] RIU=%d, layerEn=%d, srcIndex=%d, layer=%d\n",
		__func__, __LINE__, paramIn.RIU, paramIn.layerEn, paramIn.srcIndex,
		paramIn.layer);

	drv_hwreg_render_display_set_layerControl(paramIn, &paramOut);

	/* add ml cmd if not riu */
	if (!paramIn.RIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
		paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}

	return ret;
}

static void _mtk_video_display_set_vb_pre_layer_enable(
	struct mtk_video_context *pctx_video,
	bool bRIU)
{
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_LAYER_EN];
	struct hwregVBPreLayerControlIn paramIn;
	struct hwregOut paramOut;
	int ret;

	memset(&paramIn, 0, sizeof(struct hwregVBPreLayerControlIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	// DIPR SRC control in capture DD
	paramIn.src = VB_PRE_SRC_MAIN_SCALER;
	paramIn.RIU = bRIU;
	paramIn.layerEn = TRUE;

	drv_hwreg_render_display_set_vb_pre_layerControl(paramIn, &paramOut);

	// add ml cmd if not riu
	if (!paramIn.RIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
		paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
}

static int __mtk_video_display_set_zpos_vb_legacy(
	struct mtk_video_context *pctx_video,
	enum drm_mtk_video_plane_type plane_type,
	bool bRIU)
{
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum DRM_VB_VERSION_E VB_Version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	int ret = 0;

	/*src 0: xc main path (MTK_VPLANE_TYPE_NONE & MTK_VPLANE_TYPE_MEMC)*/
	/*src 1: dipr*/
	/*not support src1:dipr, src0 always use layer 0 now*/

	ret = _mtk_video_display_set_blending_layer_enable(
		pctx_video, bRIU, plane_type, 0, true);

	if (VB_Version == EN_VB_VERSION_LEGACY_V1)
		_mtk_video_display_set_vb_pre_layer_enable(pctx_video, bRIU);

	return ret;
}

static int __mtk_video_display_set_zpos_vb_ts(
	struct mtk_video_context *pctx_video,
	bool bRIU)
{
	struct mtk_tv_kms_context *pctx = NULL;
	enum drm_mtk_video_plane_type plane_type = 0;
	uint8_t plane_count[MTK_VPLANE_TYPE_MAX];
	uint8_t layer[MTK_VPLANE_TYPE_MAX];
	uint8_t used_layer[MTK_VPLANE_TYPE_MAX];
	bool layer_en[MTK_VPLANE_TYPE_MAX];
	int index = 0;
	int ret = 0;
	bool force = FALSE;

	memset(plane_count, 0, sizeof(plane_count));
	memset(layer, 0, sizeof(layer));
	memset(layer_en, 0, sizeof(layer_en));
	memset(used_layer, 0, sizeof(used_layer));

	pctx = pctx_video_to_kms(pctx_video);

	/* prepare enable layer args first */
	for (plane_type = 0; plane_type < MTK_VPLANE_TYPE_MAX; ++plane_type) {
		/*
		 * BYPASS mode patch: if not BYPASS mode, should not use MTK_VPLANE_TYPE_NONE.
		 * Should distiguish it using HW/SW/BYPASS property.
		 */
		if (plane_type == MTK_VPLANE_TYPE_NONE)
			continue;

		/* get plane count to check this src is disable or not */
		plane_count[plane_type] = pctx_video->plane_num[plane_type];

		if (plane_count[plane_type] > 0) {
			layer_en[plane_type] = true;
			layer[plane_type] = pctx_video->zpos_layer[plane_type];
			used_layer[layer[plane_type]] = plane_type;
		}
	}

	/*
	 *DRM_INFO("[%s][%d] used_layer=[%d, %d, %d, %d]\n",
	 *	__func__, __LINE__,
	 *	used_layer[0], used_layer[1], used_layer[2], used_layer[3]);
	 */

	/* prepare disable layer args */
	for (plane_type = 0; plane_type < MTK_VPLANE_TYPE_MAX; ++plane_type) {

		/*
		 * BYPASS mode patch: if not BYPASS mode, should not use MTK_VPLANE_TYPE_NONE.
		 * Should distiguish it using HW/SW/BYPASS property.
		 */
		if (plane_type == MTK_VPLANE_TYPE_NONE)
			continue;

		if (plane_count[plane_type] == 0) {
			/* this plane type is unused */
			layer_en[plane_type] = false;

			/* for hw limitation, need to set disable layer to unused plane */
			for (index = 0; index < NUMBER_4; ++index) {
				if (used_layer[index] == 0) {
					layer[plane_type] = index;
					used_layer[index] = plane_type;
					break;
				}
			}
		}
	}

	/* set zpos */
	for (plane_type = 0; plane_type < MTK_VPLANE_TYPE_MAX; ++plane_type) {

		/*
		 * BYPASS mode patch: if not BYPASS mode, should not use MTK_VPLANE_TYPE_NONE.
		 * Should distiguish it using HW/SW/BYPASS property.
		 */
		if (plane_type == MTK_VPLANE_TYPE_NONE) {
#ifdef CONFIG_MTK_DISP_BRINGUP
			DRM_INFO("[%s][%d] plane type is MTK_VPLANE_TYPE_NONE\n",
				__func__, __LINE__);

			ret = _mtk_video_display_set_blending_layer_enable(
				pctx_video, bRIU, plane_type, 0, true);

#endif
			continue;
		}

		/* check if update, call hwreg */
		if (force ||
			(_mtk_video_display_is_variable_updated(
				pctx_video->old_layer[plane_type], layer[plane_type]) ||
				_mtk_video_display_is_variable_updated(
				pctx_video->old_layer_en[plane_type], layer_en[plane_type]) ||
				_mtk_video_display_is_variable_updated(
				pctx_video->old_used_layer[layer[plane_type]],
				used_layer[layer[plane_type]]))) {

			DRM_INFO("[%s][%d] %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
				__func__, __LINE__,
				"plane_type=", plane_type,
				"plane_count=", plane_count[plane_type],
				"old_layer_en=", pctx_video->old_layer_en[plane_type],
				"layer_en=", layer_en[plane_type],
				"old_layer=", pctx_video->old_layer[plane_type],
				"layer=", layer[plane_type]);

			ret = _mtk_video_display_set_blending_layer_enable(
				pctx_video, bRIU,

				plane_type, layer[plane_type], layer_en[plane_type]);

		}
	}

	/* update old val */
	memcpy(pctx_video->old_layer, layer, sizeof(layer));
	memcpy(pctx_video->old_used_layer, used_layer, sizeof(used_layer));
	memcpy(pctx_video->old_layer_en, layer_en, sizeof(layer_en));

	return ret;
}

static void _mtk_video_display_set_zpos(
	struct mtk_video_context *pctx_video,
	bool binit)
{
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum DRM_VB_VERSION_E VB_Version = 0;
	int ret = 0;
	bool bRIU = FALSE;

	video_hw_version = &pctx_video->video_hw_ver;
	VB_Version = (enum DRM_VB_VERSION_E) video_hw_version->video_blending;

	if (binit)
		bRIU = TRUE;
	else
		bRIU = FALSE;

	if (IS_VB_LEGACY(VB_Version)) {
		ret = __mtk_video_display_set_zpos_vb_legacy(
			pctx_video, MTK_VPLANE_TYPE_NONE, bRIU);

		if (ret) {
			DRM_ERROR("set zpos (vb_legacy) failed with %d\n", ret);
			return;
		}
	} else {
		ret = __mtk_video_display_set_zpos_vb_ts(
			pctx_video, bRIU);

		if (ret) {
			DRM_ERROR("set zpos (vb_ts) failed with %d\n", ret);
			return;
		}
	}
}

static void _mtk_video_display_clean_video_context
	(struct device *dev,
	struct mtk_video_context *video_pctx)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_tv_drm_crtc *crtc = &pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO];

	if (video_pctx) {
		devm_kfree(dev, video_pctx->video_plane_properties);
		video_pctx->video_plane_properties = NULL;
		devm_kfree(dev, video_pctx->plane_ctx);
		video_pctx->plane_ctx = NULL;
		devm_kfree(dev, video_pctx->old_plane_ctx);
		video_pctx->old_plane_ctx = NULL;
	}
	devm_kfree(dev, crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]);
	crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO] = NULL;
	devm_kfree(dev, crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY]);
	crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY] = NULL;
}

static int _mtk_video_display_create_plane_props(struct mtk_tv_kms_context *pctx)
{
	struct drm_property *prop;
	int extend_prop_count = ARRAY_SIZE(ext_video_plane_props_def);
	const struct ext_prop_info *ext_prop;
	struct drm_device *drm_dev = pctx->drm_dev;
	int i;
	struct mtk_video_context *pctx_video = &pctx->video_priv;

#if (1) //(MEMC_CONFIG == 1)
	struct drm_property *propMemcStatus = NULL;
	struct property_blob_memc_status stMemcStatus;

	memset(&stMemcStatus, 0, sizeof(struct property_blob_memc_status));
#endif
	// create extend common plane properties
	for (i = 0; i < extend_prop_count; i++) {
		ext_prop = &ext_video_plane_props_def[i];
		if (ext_prop->flag & DRM_MODE_PROP_ENUM) {
			prop = drm_property_create_enum(
				drm_dev,
				ext_prop->flag,
				ext_prop->prop_name,
				ext_prop->enum_info.enum_list,
				ext_prop->enum_info.enum_length);
			if (prop == NULL) {
				DRM_ERROR("[%s][%d]create enum fail!!\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
		} else if (ext_prop->flag & DRM_MODE_PROP_RANGE) {
			prop = drm_property_create_range(
				drm_dev,
				ext_prop->flag,
				ext_prop->prop_name,
				ext_prop->range_info.min,
				ext_prop->range_info.max);
			if (prop == NULL) {
				DRM_ERROR("[%s][%d]create range fail!!\n",
					__func__, __LINE__);
				return -ENOMEM;
			}

#if (1) //(MEMC_CONFIG == 1)
			//save memc status prop
			if (strcmp(ext_prop->prop_name,
				MTK_PLANE_PROP_MEMC_STATUS) == 0) {
				propMemcStatus = prop;
			}
#endif
		} else if (ext_prop->flag & DRM_MODE_PROP_BLOB) {
			prop = drm_property_create(drm_dev,
				ext_prop->flag,
				ext_prop->prop_name,
				0);
			if (prop == NULL) {
				DRM_ERROR("[%s, %d]: create blob prop fail!!\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
		} else {
			DRM_ERROR("[%s][%d]unknown prop flag 0x%x !!\n",
				__func__, __LINE__, ext_prop->flag);
			return -EINVAL;
		}

		// add created props to context
		pctx_video->drm_video_plane_prop[i] = prop;
	}

	return 0;
}

static void _mtk_video_display_attach_plane_props(
	struct mtk_tv_kms_context *pctx,
	struct mtk_drm_plane *mplane)
{
	int extend_prop_count = ARRAY_SIZE(ext_video_plane_props_def);
	struct drm_plane *plane_base = &mplane->base;
	struct drm_mode_object *plane_obj = &plane_base->base;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	int plane_inx = mplane->video_index;
	const struct ext_prop_info *ext_prop;
	int i;

#if (1)//(MEMC_CONFIG == 1)
	struct property_blob_memc_status stMemcStatus;

	memset(&stMemcStatus, 0, sizeof(struct property_blob_memc_status));
#endif
	for (i = 0; i < extend_prop_count; i++) {
		ext_prop = &ext_video_plane_props_def[i];
		drm_object_attach_property(plane_obj,
			pctx_video->drm_video_plane_prop[i],
			ext_prop->init_val);
		(pctx_video->video_plane_properties + plane_inx)->prop_val[i] =
			ext_prop->init_val;
	}

}

static void _mtk_video_display_set_dv_metadata_info(
	struct dma_buf *meta_db,
	struct m_pq_dv_out_frame_info *dv_out_frame_info)

{
	memset(&g_dv_out_frame_info, 0, sizeof(struct m_pq_dv_out_frame_info));
	g_meta_db = NULL;
	if (meta_db == NULL || dv_out_frame_info == NULL) {
		DRM_INFO("[DV][GD][%s][%d] dv metadata info is NULL\n", __func__, __LINE__);
		return;
	}
	memcpy(&g_dv_out_frame_info, dv_out_frame_info, sizeof(struct m_pq_dv_out_frame_info));
	g_meta_db = meta_db;
}

static void _mtk_video_display_get_dv_metadata_info(
	struct dma_buf **meta_db,
	struct m_pq_dv_out_frame_info *dv_out_frame_info)

{
	if (g_meta_db == NULL) {
		DRM_INFO("[DV][GD][%s][%d] dv metadata info is NULL\n",
			__func__, __LINE__);
		return;
	}

	*meta_db = g_meta_db;
	memcpy(dv_out_frame_info, &g_dv_out_frame_info, sizeof(struct m_pq_dv_out_frame_info));
}

static void _mtk_video_display_update_dv_gd(uint16_t gd)
{
	struct msg_dv_gd_info msg_info;
	struct pqu_render_gd_param gd_param;
	struct msg_gd_done_info msg;
	static struct m_pq_dv_out_frame_info dv_out_frame_info;
	struct dma_buf *meta_db = NULL;
	int meta_ret = 0;

	DRM_INFO("[DV][GD][%s][%d] set dv gd = %d\n", __func__, __LINE__, gd);
	if (bPquEnable) {
		memset(&gd_param, 0, sizeof(struct pqu_render_gd_param));
		memset(&msg, 0, sizeof(struct msg_gd_done_info));
		gd_param.gd = gd;
		pqu_render_gd((const struct pqu_render_gd_param *)&gd_param, &msg);


		/* ltp test for rv55 */
		memset(&dv_out_frame_info, 0, sizeof(struct m_pq_dv_out_frame_info));
		_mtk_video_display_get_dv_metadata_info((struct dma_buf **)&meta_db,
			&dv_out_frame_info);

		if (dv_out_frame_info.stub == true) {
			dv_out_frame_info.ret = msg.ret;
			meta_ret = mtk_render_common_write_metadata(meta_db,
					RENDER_METATAG_DV_OUTPUT_FRAME_INFO,
					(void *)&dv_out_frame_info);
			if (meta_ret) {
				DRM_INFO("[DV][GD][%s][%d] write dv metadata failed\n",
					__func__, __LINE__);
			}
		}
	} else {
		memset(&msg_info, 0, sizeof(struct msg_dv_gd_info));
		msg_info.gd = gd;
		pqu_msg_send(PQU_MSG_SEND_DOLBY_GD, (void *)&msg_info);
	}
}

static void _mtk_video_display_create_dv_workqueue(struct mtk_video_context *pctx_video)
{
	/* check input */
	if (pctx_video == NULL) {
		DRM_ERROR("[DV][GD][%s][%d] Invalid input, pctx_video = %p!!\n",
			__func__, __LINE__, pctx_video);
		return;
	}

	pctx_video->dv_gd_wq.wq = create_workqueue("DV_GD_WQ");
	if (IS_ERR(pctx_video->dv_gd_wq.wq)) {
		DRM_ERROR("[DV][GD][%s][%d] create DV GD workqueue fail, errno = %ld\n",
			__func__, __LINE__,
			PTR_ERR(pctx_video->dv_gd_wq.wq));
		return;
	}
	memset(&(pctx_video->dv_gd_wq.gd_info), 0, sizeof(struct dv_gd_info));

	DRM_INFO("[DV][GD][%s][%d] create DV GD workqueue done\n", __func__, __LINE__);
}

static void _mtk_video_display_destroy_dv_workqueue(struct mtk_video_context *pctx_video)
{
	/* check input */
	if (pctx_video == NULL) {
		DRM_ERROR("[DV][GD][%s][%d]Invalid input, pctx_video = %p!!\n",
			__func__, __LINE__, pctx_video);
		return;
	}

	DRM_INFO("[DV][GD][%s][%d] destroy DV GD workqueue\n", __func__, __LINE__);
	cancel_delayed_work(&(pctx_video->dv_gd_wq.dwq));
	flush_workqueue(pctx_video->dv_gd_wq.wq);
	destroy_workqueue(pctx_video->dv_gd_wq.wq);
}

static void _mtk_video_display_handle_dv_delay_workqueue(struct work_struct *work)
{
	struct mtk_video_plane_dv_gd_workqueue *dv_gd_wq = NULL;
	struct dv_gd_info *gd_info = NULL;

	/* check input */
	if (work == NULL) {
		DRM_ERROR("[DV][GD][%s][%d]Invalid input, work = %p!!\n",
			__func__, __LINE__, work);
		return;
	}

	dv_gd_wq = container_of(work, struct mtk_video_plane_dv_gd_workqueue, dwq.work);

	gd_info = &(dv_gd_wq->gd_info);

	_mtk_video_display_update_dv_gd(gd_info->gd);

	kfree(dv_gd_wq);
}

static void _mtk_video_display_set_dv_global_dimming(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct dma_buf *meta_db)
{
	int meta_ret = 0;
	struct m_pq_dv_out_frame_info dv_out_frame_info = {0};
	struct dv_gd_info *gd_info = NULL;
	struct mtk_video_plane_dv_gd_workqueue *priv_wq = NULL;
	int wq_size = 0;
	unsigned long delay_jiffies = 0;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct hwreg_render_display_dv_gd_info dummy_gd_info = {0};
	struct reg_info reg[HWREG_VIDEO_REG_NUM_DV_GD_EN];
	struct hwregOut paramOut = {0};

	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(&dummy_gd_info, 0, sizeof(struct hwreg_render_display_dv_gd_info));
	paramOut.reg = reg;

	/* input */
	if (pctx == NULL) {
#if (!MARK_METADATA_LOG)
		DRM_INFO("[DV][GD][%s][%d] disable dolby GD\n", __func__, __LINE__);
#endif
		/* reset dolby global dimming dummy enable */
		dummy_gd_info.enable = false;
		drv_hwreg_render_display_set_dv_gd(&dummy_gd_info, &paramOut);
		return;
	}

	pctx_video = &(pctx->video_priv);
	pctx_frc = &(pctx->frc_priv);
	if (pctx_video == NULL || pctx_frc == NULL) {
		DRM_ERROR("[DV][GD][%s][%d] Invalid input, pctx_video=%p, pctx_frc=%p!!\n",
			__func__, __LINE__, pctx_video, pctx_frc);
		return;
	}

	plane_ctx = (pctx_video->plane_ctx + plane_idx);

	memset(&dv_out_frame_info, 0, sizeof(struct m_pq_dv_out_frame_info));

	/* get dolby global dimming from dummy reg */
	drv_hwreg_render_display_get_dv_gd(&dummy_gd_info);

	/* if not bypass frc path then set frc_latency */
	if (plane_ctx->memc_change_on == MEMC_CHANGE_ON) {
		/* need add frc latency if use ML method */
		if (dummy_gd_info.need_frc == TRUE)
			dummy_gd_info.frc_latency = pctx_frc->frc_latency;
		else
			dummy_gd_info.frc_latency = 0;
	} else {
		dummy_gd_info.frc_latency = 0;
	}

	DRM_INFO("[DV][GD] frc_latency=%d memc_change_on=%d, need_frc=%d\n",
		pctx_frc->frc_latency,
		plane_ctx->memc_change_on,
		dummy_gd_info.need_frc);

	/* update frc latency for HW mode */
	if ((dummy_gd_info.stub == false) &&
		(plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)) {
		DRM_INFO("[DV][DUMMY_GD] gd=%d, valid=%d, delay=%d, stub=%d, enable=%d\n",
			dummy_gd_info.gd, dummy_gd_info.valid, dummy_gd_info.delay,
			dummy_gd_info.stub, dummy_gd_info.enable);

		/* update dolby global dimming dummy with frc_latency */
		drv_hwreg_render_display_set_dv_gd(&dummy_gd_info, &paramOut);
		return;
	}


	/* below SW mode only */
	if (meta_db == NULL) {
		DRM_ERROR("[DV][GD][%s][%d] Invalid input meta_db!!\n",
			__func__, __LINE__);
		return;
	}

	/* get dolby global dimming */
	meta_ret = mtk_render_common_read_metadata(meta_db,
		RENDER_METATAG_DV_OUTPUT_FRAME_INFO, (void *)&dv_out_frame_info);
	if (meta_ret) {
		DRM_INFO("[DV][GD][%s][%d] metadata do not has dv gd tag\n",
			__func__, __LINE__);
		return;
	}

	/* ltp test */
	if (dv_out_frame_info.stub == true) {
		_mtk_video_display_set_dv_metadata_info(meta_db, &dv_out_frame_info);
	}

	DRM_INFO("[DV][GD][%s][%d] meta dv gd:%d, valid=%d, delay=%d\n",
		__func__, __LINE__,
		(int)dv_out_frame_info.gd_info.gd,
		dv_out_frame_info.gd_info.valid,
		dv_out_frame_info.gd_info.delay);

	gd_info = &(pctx_video->dv_gd_wq.gd_info);

	/* check GD support */
	if (dv_out_frame_info.gd_info.valid == FALSE) {
		gd_info->valid = FALSE;
		/* invalid gd test*/
		if (dv_out_frame_info.stub == true) {
			dv_out_frame_info.ret = 0;
			meta_ret = mtk_render_common_write_metadata(meta_db,
					RENDER_METATAG_DV_OUTPUT_FRAME_INFO,
					(void *)&dv_out_frame_info);
			if (meta_ret) {
				DRM_INFO("[DV][GD][%s][%d] write dv metadata failed\n",
					__func__, __LINE__);
			}
		}
		return;
	}

	/* check GD change */
	if (gd_info->valid == TRUE && gd_info->gd == dv_out_frame_info.gd_info.gd)
		return;

	/* update gd info */
	memcpy(gd_info, &(dv_out_frame_info.gd_info), sizeof(struct dv_gd_info));

	/* GD delay */
	if (gd_info->delay) {
		wq_size = sizeof(struct mtk_video_plane_dv_gd_workqueue);
		priv_wq = kmalloc(wq_size, GFP_KERNEL);
		if (priv_wq == NULL) {
			DRM_ERROR("[DV][GD][%s][%d]alloc priv_wq fail, size=%d, ret=%p\n",
				__func__, __LINE__,
				wq_size,
				priv_wq);

			/* update gd immediately */
			_mtk_video_display_update_dv_gd(gd_info->gd);

			return;
		}
		memcpy(priv_wq, &(pctx_video->dv_gd_wq), wq_size);

		delay_jiffies = msecs_to_jiffies(gd_info->delay);

		INIT_DELAYED_WORK(&(priv_wq->dwq), _mtk_video_display_handle_dv_delay_workqueue);

		queue_delayed_work(priv_wq->wq, &(priv_wq->dwq), delay_jiffies);
	} else {
		/* update gd without delay */
		_mtk_video_display_update_dv_gd(gd_info->gd);
	}
}

static void _mtk_video_display_set_aid(
	struct mtk_video_context *pctx_video,
	unsigned int plane_idx,
	struct dma_buf *meta_db)
{
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_AID];
	struct hwregAidTableIn paramAidTableIn;
	struct hwregOut paramOut;
	struct meta_pq_disp_svp svpInfo;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_video_plane_type old_video_plane_type =
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	bool bUpdate = FALSE;
	int meta_ret = 0;
	int ret;

	memset(&svpInfo, 0, sizeof(struct meta_pq_disp_svp));
	memset(reg, 0, sizeof(reg));
	memset(&paramAidTableIn, 0, sizeof(struct hwregAidTableIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	meta_ret = meta_ret | mtk_render_common_read_metadata(meta_db,
		RENDER_METATAG_SVP_INFO, (void *)&svpInfo);

	if (meta_ret) {
		DRM_INFO("[%s][%d] metadata do not has svp info tag\n",
			__func__, __LINE__);
	} else {

		bUpdate |= _mtk_video_display_is_variable_updated(
				old_video_plane_type, video_plane_type);
		bUpdate |= _mtk_video_display_is_variable_updated(
				old_plane_ctx->access_id, svpInfo.aid);
		bUpdate |= _mtk_video_display_is_variable_updated(
			old_plane_ctx->mem_hw_mode, plane_ctx->mem_hw_mode);

		paramOut.reg = reg;

		paramAidTableIn.RIU = g_bUseRIU;
		paramAidTableIn.planeType =
			(enum drm_hwreg_video_plane_type)video_plane_type;

		if (plane_ctx->mem_hw_mode == EN_VIDEO_MEM_MODE_HW)
			paramAidTableIn.windowID = plane_ctx->hw_mode_win_id;
		else
			paramAidTableIn.windowID = plane_idx;

		paramAidTableIn.access_id = svpInfo.aid;

		if (bUpdate) {
			DRM_INFO("[%s][%d] plane_type:%d, winID:%d, aid:%d pipeline id:%d\n",
				__func__, __LINE__,
				video_plane_type, plane_idx, svpInfo.aid, svpInfo.pipelineid);

			drv_hwreg_render_display_set_aidtable(paramAidTableIn, &paramOut);

			plane_ctx->access_id = svpInfo.aid;

			if (!g_bUseRIU) {
				ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
					paramOut.regCount, (struct sm_reg *)reg);
				if (ret) {
					DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
						__func__, __LINE__, ret);
				}
			}
		}

	}
}

static void _mtk_video_display_set_window_pq(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct mtk_plane_state *state)
{
	struct hwregSetWindowPQIn paramSetWindowPQIn;
	struct video_plane_prop *plane_props = NULL;

	if (pctx_video == NULL || state == NULL) {
		DRM_ERROR("[%s][%d]Invalid input, pctx_video = %p, state = %p!!\n",
			__func__, __LINE__, pctx_video, state);
		return;
	}
	plane_props = pctx_video->video_plane_properties + plane_idx;
	memset(&paramSetWindowPQIn, 0, sizeof(paramSetWindowPQIn));

	// get window pq string
	paramSetWindowPQIn.windowID = plane_idx;
	paramSetWindowPQIn.windowPQ = (char *)plane_props->prop_val[EXT_VPLANE_PROP_WINDOW_PQ];

	// for stub mode test
	drv_hwreg_render_display_set_window_pq(paramSetWindowPQIn);
}

static inline int _mtk_video_display_check_blob_prop_size(
	uint32_t propy_idx,
	void *buf,
	uint32_t size)
{
	int ret = 0; // default we don't need to check BLOB

	switch (propy_idx) {
	case EXT_VPLANE_PROP_PQMAP_NODE_ARRAY:
		ret = mtk_tv_drm_pqmap_check_buffer(buf, size);
		break;
	default:
		break;
	}
	return ret;
}

static void _mtk_video_display_set_blending_mux(
	struct mtk_video_context *pctx_video,
	bool bRIU)
{
	struct video_plane_prop *plane_props = NULL;
	struct hwregVop2memcMuxSelectIn paramVop2memcMuxSelectIn;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_BLENDING_MUX];
	enum drm_mtk_video_plane_type video_plane_type = MTK_VPLANE_TYPE_NONE;
	enum VIDEO_BLENDING_MUX_E muxSel = EN_VIDEO_BLENDING_MUX_VOP;
	unsigned int plane_idx = 0;

	memset(&paramVop2memcMuxSelectIn, 0, sizeof(struct hwregVop2memcMuxSelectIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(reg, 0, sizeof(reg));

	while (plane_idx < MAX_WINDOW_NUM) {
		plane_props = (pctx_video->video_plane_properties + plane_idx);
		video_plane_type =
			plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
		if (video_plane_type == MTK_VPLANE_TYPE_MEMC)
			muxSel = EN_VIDEO_BLENDING_MUX_MEMC;
		plane_idx++;
	}

	paramOut.reg = reg;
	paramVop2memcMuxSelectIn.muxSel =
		((muxSel == EN_VIDEO_BLENDING_MUX_MEMC)?1:0);
	paramVop2memcMuxSelectIn.RIU = bRIU;

	drv_hwreg_render_display_set_vop2memcMuxSelect(
		paramVop2memcMuxSelectIn, &paramOut);

	if (bRIU == FALSE)
		mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);

}

static void _mtk_video_update_old_plane_context(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video)
{
	struct mtk_video_plane_ctx *plane_ctx =
		pctx_video->plane_ctx + plane_idx;
	struct mtk_video_plane_ctx *old_plane_ctx =
		pctx_video->old_plane_ctx + plane_idx;

	old_plane_ctx->crop_win.x = plane_ctx->crop_win.x;
	old_plane_ctx->crop_win.y = plane_ctx->crop_win.y;
	old_plane_ctx->crop_win.w = plane_ctx->crop_win.w;
	old_plane_ctx->crop_win.h = plane_ctx->crop_win.h;
	old_plane_ctx->disp_win.x = plane_ctx->disp_win.x;
	old_plane_ctx->disp_win.y = plane_ctx->disp_win.y;
	old_plane_ctx->disp_win.w = plane_ctx->disp_win.w;
	old_plane_ctx->disp_win.h = plane_ctx->disp_win.h;
	old_plane_ctx->fo_hvsp_in_window.x = plane_ctx->fo_hvsp_in_window.x;
	old_plane_ctx->fo_hvsp_in_window.y = plane_ctx->fo_hvsp_in_window.y;
	old_plane_ctx->fo_hvsp_in_window.w = plane_ctx->fo_hvsp_in_window.w;
	old_plane_ctx->fo_hvsp_in_window.h = plane_ctx->fo_hvsp_in_window.h;
	old_plane_ctx->frame_buf_width = plane_ctx->frame_buf_width;
	old_plane_ctx->frame_buf_height = plane_ctx->frame_buf_height;
	old_plane_ctx->post_fill_win_en = plane_ctx->post_fill_win_en;
	old_plane_ctx->rwdiff = plane_ctx->rwdiff;
	old_plane_ctx->protectVal = plane_ctx->protectVal;
	old_plane_ctx->access_id = plane_ctx->access_id;
	old_plane_ctx->mgw_plane_num = pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
	old_plane_ctx->mem_hw_mode = plane_ctx->mem_hw_mode;
	old_plane_ctx->disp_win_type = plane_ctx->disp_win_type;
	old_plane_ctx->mute = plane_ctx->mute;
	old_plane_ctx->freeze = plane_ctx->freeze;
	old_plane_ctx->video_plane_type = plane_ctx->video_plane_type;
	memcpy(&old_plane_ctx->base_addr, plane_ctx->base_addr, sizeof(plane_ctx->base_addr));
}

static void _mtk_video_update_old_plane_type_context(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video)
{
	struct video_plane_prop *plane_props;
	struct mtk_video_plane_type_ctx *plane_type_ctx;
	struct mtk_video_plane_type_ctx *old_plane_type_ctx;
	enum drm_mtk_video_plane_type video_plane_type;

	plane_props = pctx_video->video_plane_properties + plane_idx;
	video_plane_type = plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	plane_type_ctx = pctx_video->plane_type_ctx + video_plane_type;
	old_plane_type_ctx = pctx_video->old_plane_type_ctx + video_plane_type;

	old_plane_type_ctx->disp_mode = plane_type_ctx->disp_mode;
}

static void _mtk_video_display_update_trigger_gen_framesync_Info(
	unsigned int plane_inx, struct mtk_tv_kms_context *pctx,
	enum drm_mtk_vplane_disp_mode disp_mode,
	struct dma_buf *meta_db)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct meta_pq_framesync_info framesyncInfo;
	struct mtk_panel_context *pctx_pnl = &(pctx->panel_priv);
	uint64_t Input_VFreqx1000 = 0;
	int meta_ret = 0;
	unsigned long flags;
	mtktv_chip_series s_chipseries = MTK_TV_SERIES_MERAK_1;

	memset(&framesyncInfo, 0, sizeof(struct meta_pq_framesync_info));

	meta_ret = mtk_render_common_read_metadata(meta_db,
		RENDER_METATAG_FRAMESYNC_INFO, (void *)&framesyncInfo);

	if (meta_ret == 0) {
		memcpy(&plane_ctx->framesyncInfo,
			&framesyncInfo,
			sizeof(struct meta_pq_framesync_info));
	}

	s_chipseries = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	/*
	 * sometimes got the previous input fps in vrr case form property
	 * because of metadata latency.
	 * the right input fps but bypass by vrr bypass method,
	 * ignore update input fps if vrr enable
	 */
	if (!pctx->trigger_gen_info.IsVrr) {
		if (crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID] != 0) {
			mtk_video_get_vttv_input_vfreq(pctx,
				crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID],
				&Input_VFreqx1000);
			spin_lock_irqsave(&pctx->tg_lock, flags);
			pctx->trigger_gen_info.Input_Vfreqx1000 = Input_VFreqx1000;
			spin_unlock_irqrestore(&pctx->tg_lock, flags);
		}
	}

	spin_lock_irqsave(&pctx->tg_lock, flags);
	pctx->trigger_gen_info.IsPmodeEn = (plane_ctx->framesyncInfo.bInterlace) ? FALSE : TRUE;
	pctx->trigger_gen_info.IsB2Rsource = IS_INPUT_B2R(plane_ctx->framesyncInfo.input_source);

	/* merak2.0 only support legacy mode */
	if (s_chipseries == MTK_TV_SERIES_MERAK_2)
		pctx->trigger_gen_info.LegacyMode = true;

	spin_unlock_irqrestore(&pctx->tg_lock, flags);
}

static void _mtk_video_display_update_trigger_gen_idr_Info(
	unsigned int plane_inx,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	struct meta_pq_output_render_info memc_output_render_info;
	unsigned long flags;

	memset(&memc_output_render_info, 0, sizeof(struct meta_pq_output_render_info));

	if (mtk_render_common_read_metadata(plane_ctx->meta_db,
		RENDER_METATAG_META_PQ_OUTPUT_RENDER_INFO_TAG,
		&memc_output_render_info) == 0) {

		//DRM_ERROR("[%s][%d] Read metadata RENDER_METATAG_PQDD_DISPLAY_INFO fail",
		//__func__, __LINE__);
		memcpy(&plane_ctx->idrinfo,
			&memc_output_render_info.idrinfo,
			sizeof(struct meta_pq_idr_frame_info));
		plane_ctx->scmifbmode = (uint8_t)memc_output_render_info.eFrameBufferMode;

		spin_lock_irqsave(&pctx->tg_lock, flags);
		pctx->trigger_gen_info.ip_path = memc_output_render_info.ip_path;
		pctx->trigger_gen_info.op_path = memc_output_render_info.op_path;
		spin_unlock_irqrestore(&pctx->tg_lock, flags);

		if (plane_ctx->memc_change_on == MEMC_CHANGE_ON)
			pctx_frc->u64frcpts = memc_output_render_info.u64Pts;
	}

}

static void _mtk_video_display_update_pqdd_output_frame_Info(
	unsigned int plane_inx,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	struct meta_pq_output_frame_info pqdd_output_frame_info;

	memset(&pqdd_output_frame_info, 0, sizeof(struct meta_pq_output_frame_info));

	if (mtk_render_common_read_metadata(plane_ctx->meta_db,
		RENDER_METATAG_PQDD_OUTPUT_FRAME_INFO,
		&pqdd_output_frame_info) == 0) {

		pctx_frc->frc_scene = pqdd_output_frame_info.pq_scene;
	}
}

static void _mtk_video_display_dv_gd_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param)
{
	struct msg_gd_done_info *msg;
	struct dma_buf *meta_db = NULL;
	struct m_pq_dv_out_frame_info dv_out_frame_info;
	int meta_ret = 0;

	if (!param) {
		DRM_ERROR("iPointer is NULL!\n");
		return;
	}

	memset(&dv_out_frame_info, 0, sizeof(struct m_pq_dv_out_frame_info));

	_mtk_video_display_get_dv_metadata_info((struct dma_buf **)&meta_db, &dv_out_frame_info);

	msg = (struct msg_gd_done_info *)param;

	dv_out_frame_info.ret = msg->ret;

	if (dv_out_frame_info.stub == true) {
		meta_ret = mtk_render_common_write_metadata(meta_db,
			RENDER_METATAG_DV_OUTPUT_FRAME_INFO,
			(void *)&dv_out_frame_info);
		if (meta_ret) {
			DRM_INFO("[DV][GD][%s][%d] write dv metadata failed\n",
				__func__, __LINE__);
		}
	}
}

static void _mtk_video_display_vgsync_calc_shm_info(
	uint16_t *pdispwin_w, uint16_t *pdispwin_h)
{

	// Error handle for invalid SHM info (RV55 or PQ host function abnormal)
	if ((g_pst_vgsync_buf_info->u8CurBufIdx >= VGSYNC_MAX_FRAME_CNT) ||
		(g_pst_vgsync_buf_info->u8CurBufIdx >= g_pst_vgsync_buf_info->u8BufCnt) ||
		((g_pst_vgsync_buf_info->u8CurBufIdx == 0) &&
		 (g_pst_vgsync_buf_info->u8BufCnt == 0)) ||
	(g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].width == 0) ||
	(g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].height == 0)) {
		DRM_INFO("[%s][%d] Error handle dst win[%d,%d]\n",
			__func__, __LINE__, *pdispwin_w, *pdispwin_h);

		g_pst_vgsync_buf_info->u8CurBufIdx = 0;
		g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].width =
			*pdispwin_w;
		g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].height =
			*pdispwin_h;
	}

	// Error handle for RV55 can't update SHM info (Stuck after VGS enable)
	g_pst_vgsync_buf_info->stCurWinInfo.width =
		g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].width;
	g_pst_vgsync_buf_info->stCurWinInfo.height =
		g_pst_vgsync_buf_info->astWinInfo[g_pst_vgsync_buf_info->u8CurBufIdx].height;

	// If 8 align, should disable VG sync & enable MEMC
	if ((g_pst_vgsync_buf_info->stCurWinInfo.width / VGSYNC_8_ALIGN) ==
		(*pdispwin_w / VGSYNC_8_ALIGN) + 1)
		*pdispwin_w = g_pst_vgsync_buf_info->stCurWinInfo.width;

	if ((g_pst_vgsync_buf_info->stCurWinInfo.height / VGSYNC_8_ALIGN) ==
		(*pdispwin_h / VGSYNC_8_ALIGN) + 1)
		*pdispwin_h = g_pst_vgsync_buf_info->stCurWinInfo.height;
}

static bool _mtk_video_display_check_vgsync_status(
	unsigned int plane_idx,
	struct mtk_video_context *pctx_video,
	struct mtk_frc_context *pctx_frc,
	struct mtk_panel_context *pctx_pnl)
{
	bool bStreamOn = false;
	int  ret = 0;
	uint8_t u8CurRwDiff = 0;
	uint16_t DispWinAlign_w   = 0;
	uint16_t DispWinAlign_h   = 0;
	uint16_t FrcOutWinAlign_w = 0;
	uint16_t FrcOutWinAlign_h = 0;
	uint32_t u32CmdStatus     = 0;
	uint16_t au16FrcCmd_Normal[FRC_CMD_LENGTH] = {
			FRC_MB_CMD_SET_MFC_MODE, 0x1, 0x0, 0x0, 0x0};
	uint16_t au16FrcCmd_LvOff[FRC_CMD_LENGTH] = {
			FRC_MB_CMD_SET_MFC_MODE, 0x0, 0x0, 0x0, 0x0};
	struct mtk_video_plane_ctx *plane_ctx =
		(pctx_video->plane_ctx + plane_idx);
	struct hwregFrcVgScale stVgsyncScaleParam;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FRC_VGSYNC];

	memset(reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	memset(&stVgsyncScaleParam, 0, sizeof(struct hwregFrcVgScale));

	if (pctx_frc->frc_vgsync_shm_virAdr == 0) {
		// Platform doesn't support VG sync.
		return false;
	}
	g_pst_vgsync_buf_info =
		(struct mtk_frc_vgsync_buf_info *)pctx_frc->frc_vgsync_shm_virAdr;

	paramOut.reg     = reg;
	DispWinAlign_w   = ALIGN_UPTO_X(plane_ctx->disp_win.w, FRC_TWO);
	DispWinAlign_h   = ALIGN_UPTO_X(plane_ctx->disp_win.h, FRC_TWO);
	FrcOutWinAlign_w =
		ALIGN_UPTO_X(g_pst_vgsync_buf_info->stCurWinInfo.width,  FRC_TWO);
	FrcOutWinAlign_h =
		ALIGN_UPTO_X(g_pst_vgsync_buf_info->stCurWinInfo.height, FRC_TWO);

	_mtk_video_display_vgsync_calc_shm_info(&DispWinAlign_w, &DispWinAlign_h);

	DRM_INFO("[%s][%d] Plane:%d, lbw:%d, frame id:%d,"
		"src:[%d,%d],dst:[%llu,%llu], align to src:[%d,%d] dst:[%d,%d]\n",
		__func__, __LINE__,
		plane_idx,
		g_pst_vgsync_buf_info->eLbwPath,
		g_pst_vgsync_buf_info->u8CurBufIdx,
		g_pst_vgsync_buf_info->stCurWinInfo.width,
		g_pst_vgsync_buf_info->stCurWinInfo.height,
		plane_ctx->disp_win.w, plane_ctx->disp_win.h,
		FrcOutWinAlign_w, FrcOutWinAlign_h,
		DispWinAlign_w,   DispWinAlign_h);

	//Check TS Domain is available or not
	if (mtk_video_frc_get_unmask(pctx_video) == E_MFC_UNMASK_OFF) {
		DRM_INFO("[%s, %d] IPM has been mask. Plane:%d, dispwin:[%d,%d]\n",
			__func__, __LINE__, plane_idx, DispWinAlign_w, DispWinAlign_h);
		bStreamOn = false;
	} else {
		bStreamOn = true;
	}

	stVgsyncScaleParam.RIU      = g_bUseRIU;
	stVgsyncScaleParam.eLbwPath = g_pst_vgsync_buf_info->eLbwPath;
	stVgsyncScaleParam.DstHSize = DispWinAlign_w;
	stVgsyncScaleParam.DstVSize = DispWinAlign_h;
	stVgsyncScaleParam.SrcHSize = FrcOutWinAlign_w;
	stVgsyncScaleParam.SrcVSize = FrcOutWinAlign_h;

	if (bStreamOn &&
		(!g_b_memc_vgsync_on) &&
		((FrcOutWinAlign_w != DispWinAlign_w) ||
		 (FrcOutWinAlign_h != DispWinAlign_h))) {

		// Set MEMC level to none
		u32CmdStatus =
			mtk_video_display_frc_set_rv55_cmd(au16FrcCmd_LvOff, FRC_CMD_LENGTH,
			pctx_frc);
		DRM_INFO("[%s,%d] VG sync start, disable MEMC, ret:%d\n",
			__func__, __LINE__, u32CmdStatus);
		g_b_memc_vgsync_on = true;
	}

	if (bStreamOn &&
		g_b_memc_vgsync_on &&
		((FrcOutWinAlign_w != DispWinAlign_w) ||
		 (FrcOutWinAlign_h != DispWinAlign_h))) {

		g_u8_memc_rw_diff_counter = g_pst_vgsync_buf_info->u8CurBufIdx;

		DRM_INFO("[%s,%d] VG sync on: frame[%d], lbw:%d, src win[%d,%d] dst win[%d,%d]",
			__func__, __LINE__,
			g_pst_vgsync_buf_info->u8CurBufIdx, g_pst_vgsync_buf_info->eLbwPath,
			FrcOutWinAlign_w, FrcOutWinAlign_h, DispWinAlign_w, DispWinAlign_h);

		// Set VG sync cofig
		stVgsyncScaleParam.bEnable = true;
		drv_hwreg_render_frc_set_vgsync_scale_param(stVgsyncScaleParam, &paramOut);
	} else {
		// Set non-VGsync config
		stVgsyncScaleParam.bEnable = false;
		drv_hwreg_render_frc_set_vgsync_scale_param(stVgsyncScaleParam, &paramOut);
	}

	if (!g_bUseRIU) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret)
			DRM_ERROR("[%s, %d] disp_ml append cmd fail(%d)", __func__, __LINE__, ret);
	}

	if (g_b_memc_vgsync_on &&
		((!bStreamOn) || (
		(FrcOutWinAlign_w == DispWinAlign_w) &&
		(FrcOutWinAlign_h == DispWinAlign_h)))) {

		u8CurRwDiff =
			 g_pst_vgsync_buf_info->u8CurBufIdx > g_u8_memc_rw_diff_counter ?
			 g_pst_vgsync_buf_info->u8CurBufIdx - g_u8_memc_rw_diff_counter :
			(g_pst_vgsync_buf_info->u8CurBufIdx + g_pst_vgsync_buf_info->u8BufCnt) -
			 g_u8_memc_rw_diff_counter;

		if (u8CurRwDiff < pctx_frc->frc_rwdiff) {
			DRM_INFO("[%s,%d] VG sync end... count[%d/%d] to enable MEMC.\n",
				__func__, __LINE__, u8CurRwDiff, pctx_frc->frc_rwdiff);
			return true;
		}

		u32CmdStatus =
			mtk_video_display_frc_set_rv55_cmd(au16FrcCmd_Normal, FRC_CMD_LENGTH,
			pctx_frc);
		DRM_INFO("[%s,%d] VG sync end, enable MEMC, ret:%d\n",
			__func__, __LINE__, u32CmdStatus);
		g_b_memc_vgsync_on = false;
	}

	return true;
}

static void _mtk_video_display_set_frc_path(
	unsigned int plane_idx,
	struct mtk_tv_kms_context *pctx,
	struct mtk_plane_state *state,
	enum drm_mtk_vplane_buf_mode buf_mode)
{
	struct mtk_video_context   *pctx_video = &pctx->video_priv;
	struct mtk_frc_context     *pctx_frc   = &pctx->frc_priv;
	struct mtk_panel_context   *pctx_pnl   = &pctx->panel_priv;
	struct mtk_video_plane_ctx *plane_ctx  = (pctx_video->plane_ctx + plane_idx);
	E_DRV_MFC_UNMASK E_MFC_IPM_UNMASK = E_MFC_UNMASK_OFF;
	bool bFrcPathOn = FALSE;

	E_MFC_IPM_UNMASK = mtk_video_frc_get_unmask(pctx_video);
	bFrcPathOn = (plane_ctx->memc_change_on == MEMC_CHANGE_ON) ? TRUE : FALSE;

	if (bFrcPathOn == TRUE) {

		// Check MEMC VG sync status
		_mtk_video_display_check_vgsync_status(plane_idx,
										       pctx_video,
										       pctx_frc,
										       pctx_pnl);
		// Set MEMC_OPM path On
		mtk_video_display_set_frc_opm_path(state, true);

	} else if (buf_mode == MTK_VPLANE_BUF_MODE_BYPASSS) {

		// Set MEMC_OPM path Off
		mtk_video_display_set_frc_opm_path(state, false);

	} else if (E_MFC_IPM_UNMASK == E_MFC_UNMASK_ON) {

		DRM_INFO("[%s][%d]IPM Unmask! FRC Path On Early!!\n", __func__, __LINE__);

		// Set MEMC_OPM path On Early
		mtk_video_display_set_frc_opm_path(state, true);

	} else {

		// Set MEMC_OPM path Off
		mtk_video_display_set_frc_opm_path(state, false);

	}
}

static int _mtk_video_display_parse_version_dts(
	struct mtk_tv_kms_context *pctx)
{
	int ret = 0;
	u32 value = 0;
	const char *name;
	struct device_node *node;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_hw_version *video_hw_version = &pctx_video->video_hw_ver;

	node = of_find_compatible_node(NULL, NULL, "MTK-drm-tv-kms");
	if (!node) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		return -EINVAL;
	}

	name = "MGW_VERSION";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->mgw = value;

	name = "blend_hw_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->video_blending = value;

	name = "video_mute_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->video_mute = value;

	name = "disp_win_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->disp_win = value;

	name = "IRQ_Version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->irq = value;

	name = "PATTERN_HW_VER";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->pattern = value;

	name = "video_freeze_version";
	ret = of_property_read_u32(node, name, &value);
	if (ret != 0)
		goto RET;
	video_hw_version->video_freeze = value;

	return 0;

RET:
	return ret;
}

static int _mtk_video_display_parse_capability_dts(
	struct mtk_tv_kms_context *pctx)
{
	int ret = 0, ret_tmp = 0;
	u32 value = 0;
	const char *name;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "MTK-drm-tv-kms");
	if (!node) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* hw caps */

	name = "VIDEO_MEMC_NUM";
	ret_tmp = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.memc_num = value;

	name = "VIDEO_MGW_NUM";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.mgw_num = value;

	name = "VIDEO_DMA_SCL_NUM";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.dma_num = value;

	name = "VIDEO_WINDOW_NUM";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.window_num = value;

	name = "VIDEO_PANEL_GAMMA_BIT_NUM";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.panel_gamma_bit = value;

	name = "VIDEO_OD";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_OD = value;

	name = "VIDEO_OLED_DEMURA_IN";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_OLED_demura_in = value;

	name = "VIDEO_OLED_BOOSTING";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_OLED_boosting = value;

	name = "hwmode_support_planetype";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_hwmode_planetype = value;

	name = "swmode_support_planetype";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_swmode_planetype = value;

	name = "bypassmode_support_planetype";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_bypassmode_planetype = value;

	name = "support_h_scl_down";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_h_scl_down = value;

	name = "support_render_scaling";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.support_render_scaling = value;

	name = "render_p_engine";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.render_p_engine = value;

	name = "byte_per_word";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.byte_per_word = value;

	name = "disp_x_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.disp_x_align = value;

	name = "disp_y_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.disp_y_align = value;

	name = "disp_w_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.disp_w_align = value;

	name = "disp_h_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.disp_h_align = value;

	name = "crop_x_align_420_422";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_x_align_420_422 = value;

	name = "crop_x_align_444";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_x_align_444 = value;

	name = "crop_y_align_420";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_y_align_420 = value;

	name = "crop_y_align_444_422";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_y_align_444_422 = value;

	name = "crop_w_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_w_align = value;

	name = "crop_h_align";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->hw_caps.crop_h_align = value;

	/* sw caps */
	name = "enable_expect_pqout_size";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->sw_caps.enable_expect_pqout_size = value;

	name = "enable_pqu_ds_size_ctl";
	ret = of_property_read_u32(node, name, &value);
	ret &= ret_tmp;
	pctx->sw_caps.enable_pqu_ds_size_ctl = value;

	return ret;
}

static void _mtk_video_display_set_mute_init(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;
	struct hwregOut paramOut = { };
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MUTE] = { };
	struct hwregAutoNoSignalIn AutoNoSignalParamIn = { };

	if (!pctx)
		return;

	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;
	video_mute_hw_ver = video_hw_version->video_mute;

	/* do nothing in m6/m6l */
	if (video_mute_hw_ver != EN_VIDEO_MUTE_VERSION_OSDB)
		return;

	paramOut.reg = reg;

	/* merak 2.0, set mute in the beginning */
	drv_hwreg_render_display_mute_init(true, &paramOut);
	__set_osdb_mute(pctx_video, true, true);

	// auto no signal enable once, use riu no need to ml fire
	AutoNoSignalParamIn.RIU = TRUE;
	AutoNoSignalParamIn.planeType = VPLANE_TYPE_NONE;
	AutoNoSignalParamIn.bEnable = TRUE;
	drv_hwreg_render_display_set_auto_no_signal(AutoNoSignalParamIn, &paramOut);
}

/* ------- Global function -------*/
int mtk_video_display_parse_dts(struct mtk_tv_kms_context *pctx)
{
	int ret = 0;

	ret = _mtk_video_display_parse_version_dts(pctx);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] video_display_parse_version_dts fail!\n",
			__func__, __LINE__);
		goto RET;
	}

	ret = _mtk_video_display_parse_capability_dts(pctx);
	if (ret != 0) {
		DRM_ERROR("[%s, %d] video_display_parse_capability_dts fail!\n",
			__func__, __LINE__);
		goto RET;
	}

	return 0;

RET:
	return ret;
}

int mtk_video_display_init(
	struct device *dev,
	struct device *master,
	void *data,
	struct mtk_drm_plane **primary_plane,
	struct mtk_drm_plane **cursor_plane)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_tv_drm_crtc *crtc = &pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO];
	struct mtk_tv_drm_connector *connector = &pctx->connector[MTK_DRM_CONNECTOR_TYPE_VIDEO];
	struct mtk_tv_drm_connector *ext_video_connector =
		&pctx->connector[MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO];
	struct mtk_drm_plane *mplane_video = NULL;
	struct mtk_drm_plane *mplane_primary = NULL;
	struct drm_device *drm_dev = data;
	enum drm_plane_type drmPlaneType;
	unsigned int plane_idx;
	struct video_plane_prop *plane_props;
	struct mtk_video_plane_ctx *plane_ctx;
	struct mtk_video_plane_type_ctx *plane_type_ctx;
	unsigned int primary_plane_num = pctx->plane_num[MTK_DRM_PLANE_TYPE_PRIMARY];
	unsigned int video_plane_num =
		pctx->plane_num[MTK_DRM_PLANE_TYPE_VIDEO];
	unsigned int video_zpos_base = 0;
	unsigned int primary_zpos_base = 0;

	pctx->drm_dev = drm_dev;
	pctx_video->videoPlaneType_TypeNum = MTK_VPLANE_TYPE_MAX - 1;

	DRM_INFO("[%s][%d] plane start: [primary]:%d [video]:%d\n",
			__func__, __LINE__,
			pctx->plane_index_start[MTK_DRM_PLANE_TYPE_PRIMARY],
			pctx->plane_index_start[MTK_DRM_PLANE_TYPE_VIDEO]);

	DRM_INFO("[%s][%d] plane num: [primary]:%d [video]:%d\n",
			__func__, __LINE__,
			primary_plane_num,
			video_plane_num);

	mplane_video =
		devm_kzalloc(dev, sizeof(struct mtk_drm_plane) * video_plane_num, GFP_KERNEL);
	if (mplane_video == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}

	crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO] = mplane_video;
	crtc->plane_count[MTK_DRM_PLANE_TYPE_VIDEO] = video_plane_num;

	mplane_primary =
		devm_kzalloc(dev, sizeof(struct mtk_drm_plane) * primary_plane_num, GFP_KERNEL);
	if (mplane_primary == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}

	crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY] = mplane_primary;
	crtc->plane_count[MTK_DRM_PLANE_TYPE_PRIMARY] = primary_plane_num;

	plane_props =
		devm_kzalloc(dev, sizeof(struct video_plane_prop) * video_plane_num, GFP_KERNEL);
	if (plane_props == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}
	memset(plane_props, 0, sizeof(struct video_plane_prop) * video_plane_num);
	pctx_video->video_plane_properties = plane_props;

	plane_ctx =
		devm_kzalloc(dev, sizeof(struct mtk_video_plane_ctx) * video_plane_num, GFP_KERNEL);
	if (plane_ctx == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}
	memset(plane_ctx, 0, sizeof(struct mtk_video_plane_ctx) * video_plane_num);
	pctx_video->plane_ctx = plane_ctx;

	plane_ctx =
		devm_kzalloc(dev, sizeof(struct mtk_video_plane_ctx) * video_plane_num, GFP_KERNEL);
	if (plane_ctx == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}
	memset(plane_ctx, 0, sizeof(struct mtk_video_plane_ctx) * video_plane_num);
	pctx_video->old_plane_ctx = plane_ctx;

	plane_type_ctx =
		devm_kzalloc(dev,
			sizeof(struct mtk_video_plane_type_ctx) * MTK_VPLANE_TYPE_MAX, GFP_KERNEL);
	if (plane_type_ctx == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}
	memset(plane_type_ctx, 0, sizeof(struct mtk_video_plane_type_ctx) * MTK_VPLANE_TYPE_MAX);
	pctx_video->plane_type_ctx = plane_type_ctx;

	plane_type_ctx =
		devm_kzalloc(dev,
			sizeof(struct mtk_video_plane_type_ctx) * MTK_VPLANE_TYPE_MAX, GFP_KERNEL);
	if (plane_type_ctx == NULL) {
		DRM_INFO("devm_kzalloc failed.\n");
		ret = -ENOMEM;
		goto ERR;
	}
	memset(plane_type_ctx, 0, sizeof(struct mtk_video_plane_type_ctx) * MTK_VPLANE_TYPE_MAX);
	pctx_video->old_plane_type_ctx = plane_type_ctx;

	if (_mtk_video_display_create_plane_props(pctx) < 0) {
		DRM_INFO("[%s][%d]mtk_drm_video_create_plane_props fail!!\n",
			__func__, __LINE__);
		goto ERR;
	}

	connector->connector_private = pctx;
	ext_video_connector->connector_private = pctx;

	for (plane_idx = 0; plane_idx < primary_plane_num; plane_idx++) {
		drmPlaneType = DRM_PLANE_TYPE_PRIMARY;

		*primary_plane = &crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx];

		ret = mtk_plane_init(drm_dev,
			&crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].base,
			MTK_PLANE_DISPLAY_PIPE,
			drmPlaneType);

		if (ret != 0x0) {
			DRM_INFO("mtk_plane_init (primary plane) failed.\n");
			goto ERR;
		}

		crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].crtc_base =
			&crtc->base;
		crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].crtc_private =
			pctx;
		crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].zpos =
			primary_zpos_base;
		crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].primary_index =
			plane_idx;
		crtc->plane[MTK_DRM_PLANE_TYPE_PRIMARY][plane_idx].mtk_plane_type =
			MTK_DRM_PLANE_TYPE_PRIMARY;
	}

	for (plane_idx = 0; plane_idx < video_plane_num; plane_idx++) {
		drmPlaneType = DRM_PLANE_TYPE_OVERLAY;

		ret = mtk_plane_init(drm_dev,
			&crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].base,
			MTK_PLANE_DISPLAY_PIPE,
			drmPlaneType);

		if (ret != 0x0) {
			DRM_INFO("mtk_plane_init (video plane) failed.\n");
			goto ERR;
		}

		crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].crtc_base =
			&crtc->base;
		crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].crtc_private =
			pctx;
		crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].zpos =
			video_zpos_base;
		crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].video_index =
			plane_idx;
		crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx].mtk_plane_type =
			MTK_DRM_PLANE_TYPE_VIDEO;

		_mtk_video_display_attach_plane_props(pctx,
			&crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO][plane_idx]);
	}

	/* init blend out size */
	_mtk_video_display_set_blend_size(pctx, TRUE);

	/* init window alpha */
	_mtk_video_display_set_windowAlpha(pctx);

	/* init background alpha */
	_mtk_video_display_set_BackgroundAlpha(pctx);

	/*set video hw version*/
	drv_hwreg_render_display_set_mgw_version(pctx_video->video_hw_ver.mgw);
	drv_hwreg_render_display_set_vb_version(pctx_video->video_hw_ver.video_blending);
	drv_hwreg_render_display_set_mute_version(pctx_video->video_hw_ver.video_mute);
	drv_hwreg_render_ambient_set_version(pctx->globalpq_hw_ver.ambient_version);
	drv_hwreg_render_pnl_set_disp_win_version(pctx_video->video_hw_ver.disp_win);

#if (1)//(MEMC_CONFIG == 1)
	mtk_video_display_frc_init(dev, pctx);
	DRM_INFO("[%s][%d] mtk video frc init success!!\n", __func__, __LINE__);
	g_b_memc_vgsync_on = false;
#endif
	_mtk_video_display_set_CfdSetting(pctx);

	_mtk_video_display_set_videoblending_en(pctx_video);

	_mtk_video_display_create_dv_workqueue(pctx_video);

	// register dv gd callback
	if (!bPquEnable)
		pqu_msg_register_notify_func(PQU_MSG_REPLY_GD, _mtk_video_display_dv_gd_cb);

	mtk_video_display_set_ambient_framesize(pctx_video, TRUE);
	mtk_video_display_ambient_AUL_init(pctx);

	/* init video mute setting */
	_mtk_video_display_set_mute_init(pctx);

	DRM_INFO("[%s][%d] mtk video init success!!\n", __func__, __LINE__);

	return 0;

ERR:
	_mtk_video_display_clean_video_context(dev, pctx_video);
	return ret;
}

int mtk_video_display_unbind(struct device *dev)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	unsigned int video_plane_num = 0;

	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;

	video_plane_num = pctx->plane_num[MTK_DRM_PLANE_TYPE_VIDEO];

	_mtk_video_display_destroy_dv_workqueue(pctx_video);

	// unregister dv gd callback
	if (!bPquEnable)
		pqu_msg_deregister_notify_func(PQU_MSG_REPLY_GD);

	return 0;
}

void mtk_video_display_update_plane(struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct drm_framebuffer *fb = state->base.fb;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	unsigned int plane_idx = mplane->video_index;
	struct video_plane_prop *plane_props = (pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_idx);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	enum drm_mtk_vplane_disp_mode disp_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_DISP_MODE];
	enum drm_mtk_vplane_buf_mode buf_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_BUF_MODE];
	int VB_version = video_hw_version->video_blending;
	uint8_t index = 0;

	DRM_INFO("[%s][%d] update plane:%d start !\n", __func__, __LINE__, plane_idx);

	plane_ctx->video_plane_type = video_plane_type;

	if (plane_idx >= MAX_WINDOW_NUM) {
		DRM_ERROR("[%s][%d] Invalid plane_idx:%d !! MAX_WINDOW_NUM:%d\n",
			__func__, __LINE__, plane_idx, MAX_WINDOW_NUM);
		return;
	}

	/* check mem mode settings */
	_mtk_video_display_update_HW_mode(plane_idx, pctx_video);

	// set aid
	_mtk_video_display_set_aid(pctx_video, plane_idx, plane_ctx->meta_db);

	// update trigger gen framesync Info
	_mtk_video_display_update_trigger_gen_framesync_Info(
		plane_idx, pctx, disp_mode, plane_ctx->meta_db);

	// set dolby global dimming
	_mtk_video_display_set_dv_global_dimming(plane_idx, pctx, plane_ctx->meta_db);

	// handle window pq
	_mtk_video_display_set_window_pq(plane_idx, pctx_video, state);

	// check video plane type
	_mtk_video_display_set_mgwdmaWinEnable(plane_idx, pctx_video, FALSE);
	_mtk_video_display_set_mgwdmaWinEnable(plane_idx, pctx_video, TRUE);

	// update display window info
	_mtk_video_display_update_displywinodw_info(plane_idx, pctx, state);

	// update crop window info
	_mtk_video_display_update_cropwinodw_info(plane_idx, pctx, state);

	_mtk_video_display_set_HW_mode(plane_idx, pctx_video);

	// put this module at the first to check and update the V size
	// update scaling ratio
	_mtk_video_set_scaling_ratio(plane_idx, pctx, state);

	// check display window
	_mtk_video_display_set_disp_win(plane_idx, pctx_video, state);

	// check crop window
	_mtk_video_display_set_crop_win(plane_idx, pctx_video, state);

	// check fb memory format settins
	_mtk_video_display_set_fb_memory_format(plane_idx, pctx_video, fb);

	// check rw diff
	_mtk_video_display_set_rw_diff(plane_idx, pctx, state);

	// check freeze
	_mtk_video_display_set_freeze(plane_idx, pctx);

	// check fb settins
	_mtk_video_display_set_fb(plane_idx, pctx_video, fb);

	// update output frame info
	_mtk_video_display_update_pqdd_output_frame_Info(plane_idx, pctx);

	// update trigger gen idr Info
	_mtk_video_display_update_trigger_gen_idr_Info(plane_idx, pctx);

	// set MEMC_OPM path
	_mtk_video_display_set_frc_path(plane_idx, pctx, state, buf_mode);

	_mtk_video_display_set_blending_mux(pctx_video, g_bUseRIU);

	// check mute
	_mtk_video_display_set_mute(plane_idx, pctx_video);

	// check zpos
	if ((video_plane_type >= MTK_VPLANE_TYPE_NONE) && (video_plane_type < MTK_VPLANE_TYPE_MAX))
		pctx_video->zpos_layer[video_plane_type] = state->base.zpos;

	// blending post fill win enable

	if (IS_VB_TS(VB_version))
		_mtk_video_display_set_post_fill_win_enable(plane_idx, pctx_video, true);

	if (_mtk_video_display_is_ext_prop_updated(plane_props, EXT_VPLANE_PROP_PQMAP_NODE_ARRAY)) {
		mtk_tv_drm_pqmap_update(&pctx->pqmap_priv, &pctx->video_priv.disp_ml,
					(struct drm_mtk_tv_pqmap_node_array *)
					plane_props->prop_val[EXT_VPLANE_PROP_PQMAP_NODE_ARRAY]);
	}

	// store the window setting into pqu_metadata
	mtk_tv_drm_pqu_metadata_add_window_setting(&pctx->pqu_metadata_priv, plane_idx, state);

	//store old property value for extend property
	//this need be located in the end of function
	_mtk_video_update_old_plane_context(plane_idx, pctx_video);
	_mtk_video_update_old_plane_type_context(plane_idx, pctx_video);

	for (index = 0; index < EXT_VPLANE_PROP_MAX; index++) {
		plane_props->old_prop_val[index] = plane_props->prop_val[index];
		plane_props->prop_update[index] = 0;
	}
	if (plane_ctx->meta_db) {
		if (plane_ctx->meta_va != NULL) {
			dma_buf_vunmap(plane_ctx->meta_db, plane_ctx->meta_va);
			plane_ctx->meta_va = NULL;
		}
		dma_buf_put(plane_ctx->meta_db);
		plane_ctx->meta_db = NULL;
	}
	DRM_INFO("[%s][%d] update plane:%d end !\n", __func__, __LINE__, plane_idx);
}

void mtk_video_display_disable_plane(struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_frc_context *pctx_frc = &pctx->frc_priv;
	unsigned int plane_idx = mplane->video_index;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_idx);
	struct mtk_video_hw_version *video_hw_version =
		&pctx_video->video_hw_ver;
	enum DRM_VB_VERSION_E VB_version =
		(enum DRM_VB_VERSION_E) video_hw_version->video_blending;
	uint8_t index = 0;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;
	bool brendercloseEn = 0;

	DRM_INFO("[%s][%d]disable plane:%d\n",
		__func__, __LINE__, plane_idx);

	video_mute_hw_ver = video_hw_version->video_mute;

	_mtk_video_display_set_mgwdmaWinEnable(plane_idx, pctx_video, 0);

	if (IS_VB_TS(VB_version)) {
		_mtk_video_display_set_post_fill_win_enable(plane_idx, pctx_video, false);
	} else {
		// patch for memc garbage while close window
		if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_OSDB) {
			__set_osdb_mute(pctx_video, true, true);
			DRM_INFO("mute video in close plane\n");
		}
	}

	//reset frc aid
	mtk_video_display_frc_set_reset_aid(pctx);

	// reset dolby global dimming
	_mtk_video_display_set_dv_global_dimming(plane_idx, NULL, NULL);

	pctx_frc->bIspre_memc_en_Status[plane_idx] = FALSE;
	//store old property value for extend property
	//this need be located in the end of function
	for (index = 0; index < EXT_VPLANE_PROP_MAX; index++) {
		plane_props->old_prop_val[index] = plane_props->prop_val[index];
		plane_props->prop_update[index] = 0;
	}

	_mtk_video_display_check_render_close(pctx, pctx_video, &brendercloseEn);
	mtk_tv_drm_set_trigger_gen_reset_to_default(pctx, brendercloseEn, false);
}

void mtk_video_display_atomic_crtc_flush(struct mtk_video_context *pctx_video)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum DRM_VB_VERSION_E VB_version = 0;

	if (!pctx_video) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_video=0x%p\n",
			__func__, __LINE__, pctx_video);
		return;
	}

	pctx = pctx_video_to_kms(pctx_video);
	video_hw_version = &pctx_video->video_hw_ver;
	VB_version = (enum DRM_VB_VERSION_E) video_hw_version->video_blending;

	/* set ext enable */
	_mtk_video_display_set_extWinEnable(pctx_video);

	/* set mgw/dma enable */
	_mtk_video_display_set_mgwdmaEnable(pctx_video, FALSE);

	/* set frc enable */
	_mtk_video_display_set_frcOpmMaskEnable(pctx);

	/* set pre insert enable */
	if (IS_VB_TS(VB_version))
		_mtk_video_display_set_enable_pre_insert(pctx_video);

	/* set post fill enable */
	if (IS_VB_TS(VB_version))
		_mtk_video_display_set_enable_post_fill(pctx);

	/* set zpos */
	_mtk_video_display_set_zpos(pctx_video, FALSE);

	// send the window setting to pqu from pqu_metadata
	mtk_tv_drm_pqu_metadata_fire_window_setting(&pctx->pqu_metadata_priv);

	/* set blend size*/
	_mtk_video_display_set_blend_size(pctx, FALSE);
}

void mtk_video_display_clear_plane_status(struct mtk_plane_state *state)
{

}

int mtk_video_display_atomic_set_plane_property(
	struct mtk_drm_plane *mplane,
	struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t val)
{
	unsigned int plane_inx = mplane->video_index;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct video_plane_prop *plane_props = (pctx_video->video_plane_properties + plane_inx);
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	struct drm_property_blob *property_blob = NULL;
	void *property_buffer = NULL;
	struct dma_buf *db = NULL;
	int ret = -EINVAL;
	unsigned int i = 0;

	for (i = 0; i < EXT_VPLANE_PROP_MAX; i++) {
		if (property == pctx_video->drm_video_plane_prop[i]) {
			_mtk_video_display_update_val_if_debug(i, state, plane_inx, &val);
			ret = 0;
			break;
		}
	}
	if (ret != 0) {
		DRM_ERROR("[%s, %d]: unknown PLANE property %s!!\n",
			__func__, __LINE__, property->name);
		goto EXIT;
	}

	if (i == EXT_VPLANE_PROP_META_FD) {
		plane_props->old_prop_val[i] = plane_props->prop_val[i];
		plane_props->prop_val[i] = val;
		plane_props->prop_update[i] = (plane_props->old_prop_val[i] != val);
		if (plane_ctx->meta_db) { // last commit not completion
			DRM_WARN("[%s][%d] last meta_db is not NULL\n", __func__, __LINE__);
			goto EXIT;
		}
		db = dma_buf_get((int)val);
		if (IS_ERR_OR_NULL(db)) {
			plane_ctx->meta_db = NULL;
			DRM_ERROR("[%s][%d] dma_buf_get fail, fd=%lld, errno = %ld\n",
				__func__, __LINE__, val, PTR_ERR(db));
			return PTR_ERR(db);
		}
		plane_ctx->meta_db = db;
		plane_ctx->meta_va = dma_buf_vmap(db);
		DRM_INFO("[%s][%d] dma_buf_get done, fd=%lld, ptr=0x%p\n",
			__func__, __LINE__, val, db);
	} else if (i == EXT_VPLANE_PROP_VIDEO_PLANE_TYPE) {
		plane_props->old_prop_val[i] = plane_props->prop_val[i];
		plane_props->prop_val[i] = val;
		plane_props->prop_update[i] = (plane_props->old_prop_val[i] != val);
		if (_mtk_video_display_is_ext_prop_updated(plane_props, i) == 0)
			goto EXIT;

		DRM_INFO("[%s, %d]: plane_inx:%u set property %s, value:%llu\n",
			__func__, __LINE__, plane_inx, property->name, val);

		_mtk_video_display_check_support_window(
			plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE],
			plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE],
			plane_inx, pctx);

		if (!mtk_video_display_check_frc_freeze(
			(plane_ctx->memc_change_on == MEMC_CHANGE_ON), plane_ctx->memc_freeze)) {
			DRM_INFO("[%s][%d] mtk_video_display_check_frc_freeze failed !!\n",
				__func__, __LINE__);
		}
	} else if (MEMC_PROP(i)) {
		plane_props->old_prop_val[i] = plane_props->prop_val[i];
		plane_props->prop_val[i] = val;
		plane_props->prop_update[i] = (plane_props->old_prop_val[i] != val);
		if (_mtk_video_display_is_ext_prop_updated(plane_props, i) == 0)
			goto EXIT;

		mtk_video_display_set_frc_property(plane_props, i);
		DRM_INFO("[%s][%d] plane_inx=%u, set memc_prop=0x%x\n",
			__func__, __LINE__, plane_inx, i);

	} else if (IS_BLOB_PROP(property)) {
		if (plane_props->prop_update[i] == 1) // last commit not completion
			goto EXIT;

		kfree((void *)plane_props->old_prop_val[i]); // free old buffer
		plane_props->old_prop_val[i] = 0; // reset old buffer
		plane_props->prop_val[i] = 0; // reset new buffer

		property_blob = drm_property_lookup_blob(pctx->drm_dev, val);
		if (property_blob == NULL) {
			DRM_ERROR("[%s][%d] blob id %llu is NULL!!", __func__, __LINE__, val);
			ret = -EINVAL;
			goto EXIT;
		}
		if (_mtk_video_display_check_blob_prop_size(
				i, property_blob->data, property_blob->length)) {
			DRM_ERROR("[%s][%d] prop %s check buffer fail",
				__func__, __LINE__, property->name);
			drm_property_blob_put(property_blob);
			ret = -EINVAL;
			goto EXIT;
		}
		property_buffer = kmalloc(property_blob->length, GFP_KERNEL);
		if (property_buffer == NULL) {
			DRM_ERROR("[%s][%d] prop %s alloc buffer fail, size = %zd",
				__func__, __LINE__, property->name, property_blob->length);
			drm_property_blob_put(property_blob);
			ret = -ENOMEM;
			goto EXIT;
		}
		memcpy(property_buffer, property_blob->data, property_blob->length);
		plane_props->prop_val[i] = (uint64_t)property_buffer; // assign new buffer
		plane_props->prop_update[i] = 1;
		drm_property_blob_put(property_blob);
	} else {
		plane_props->old_prop_val[i] = plane_props->prop_val[i];
		plane_props->prop_val[i] = val;
		plane_props->prop_update[i] = (plane_props->old_prop_val[i] != val);

		if (plane_props->prop_update[i]) {
			DRM_INFO("[%s, %d]: plane_inx:%u set property %s, value:%llu\n",
				__func__, __LINE__, plane_inx, property->name, val);
		}

	}
EXIT:
	return ret;
}

int mtk_video_display_atomic_get_plane_property(
	struct mtk_drm_plane *mplane,
	const struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;
	int i;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	unsigned int plane_inx = mplane->video_index;
	struct video_plane_prop *plane_props = (pctx_video->video_plane_properties + plane_inx);

	for (i = 0; i < EXT_VPLANE_PROP_MAX; i++) {
		if (property == pctx_video->drm_video_plane_prop[i]) {
			*val = plane_props->prop_val[i];
			ret = 0x0;
			break;
		}
	}

	/*&&(plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE] == MTK_VPLANE_TYPE_MEMC)*/
	if (MEMC_PROP(i) &&
	(plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE] == MTK_VPLANE_TYPE_MEMC)) {
		mtk_video_display_get_frc_property(plane_props, i);
		*val = plane_props->prop_val[i];
	}

	return ret;
}

int mtk_video_display_check_plane(
	struct drm_plane_state *plane_state,
	const struct drm_crtc_state *crtc_state,
	struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	unsigned int plane_inx = mplane->video_index;
	struct video_plane_prop *plane_props =
		(pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_video_plane_type video_plane_type =
		plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
	int i, j, ret = 0;
	uint64_t disp_width = plane_state->crtc_w;
	uint64_t disp_height = plane_state->crtc_h;
	uint64_t src_width = (plane_state->src_w) >> 16;
	uint64_t src_height = (plane_state->src_h) >> 16;
	uint8_t total_plane = 0;

	DRM_INFO("[%s][%d] video_plane_type:%d[src_w,src_h,disp_w,disp_h]:[%lld,%lld,%lld,%lld]\n"
			, __func__, __LINE__, video_plane_type,
			src_width, src_height, disp_width, disp_height);
	// check scaling cability
	ret = drm_atomic_helper_check_plane_state(plane_state, crtc_state,
						DRM_PLANE_SCALE_MIN,
						DRM_PLANE_SCALE_MAX,
						true, true);
	// check video plane type update
	if (_mtk_video_display_is_ext_prop_updated(plane_props,
		EXT_VPLANE_PROP_VIDEO_PLANE_TYPE)) {
		//mtk_video_check_support_window(plane_props, pctx_video);
		if ((pctx_video->plane_num[MTK_VPLANE_TYPE_MEMC] > 1) ||
		(pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN] > 16) ||
		(pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN1] > 1) ||
		(pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN2] > 1)) {
			DRM_ERROR("[%s][%d] over HW support number, %d, %d, %d, %d",
				__func__, __LINE__,
				pctx_video->plane_num[MTK_VPLANE_TYPE_MEMC],
				pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN],
				pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN1],
				pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN2]);
			goto error;
		}
	}
	if ((video_plane_type == MTK_VPLANE_TYPE_MULTI_WIN) &&
		(pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN] > 1) &&
		((src_width != disp_width) || (src_height != disp_height))) {
		DRM_ERROR("[%s][%d] Multi MGW (%d) not support scaling",
			__func__, __LINE__, pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN]);
		goto error;
	}
	return ret;

error:
	total_plane = pctx_video->plane_num[MTK_VPLANE_TYPE_MEMC] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN1] +
		pctx_video->plane_num[MTK_VPLANE_TYPE_SINGLE_WIN2];

	for (j = 0; j < total_plane ; j++) {

		plane_props = (pctx_video->video_plane_properties + j);

		for (i = 0; i < EXT_VPLANE_PROP_MAX; i++) {
			if ((i == EXT_VPLANE_PROP_VIDEO_PLANE_TYPE) &&
				(_mtk_video_display_is_ext_prop_updated(plane_props, i))) {
				_mtk_video_display_check_support_window(
					plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE],
					plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE],
					j, pctx);
			}
			plane_props->prop_val[i] = plane_props->old_prop_val[i];
		}
	}
	return -EINVAL;
}

void mtk_video_display_get_rbank_IRQ(
	unsigned int plane_inx,
	struct mtk_video_context *pctx_video)
{
	struct video_plane_prop *plane_props = (pctx_video->video_plane_properties + plane_inx);
	enum drm_mtk_vplane_disp_mode disp_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_DISP_MODE];
	enum drm_mtk_vplane_buf_mode buf_mode =
		plane_props->prop_val[EXT_VPLANE_PROP_BUF_MODE];

	if ((disp_mode == MTK_VPLANE_DISP_MODE_HW) ||
		(buf_mode == MTK_VPLANE_BUF_MODE_HW)) {
		uint16_t CurrentRbank = 0;
		uint16_t i = 0;

		i = pctx_video->rbank[plane_inx].r_index;
		drv_hwreg_render_display_get_rbankidx(plane_inx, &CurrentRbank);

		pctx_video->rbank[plane_inx].rbank_idx[i] = CurrentRbank;

		if (i+1 >= STORE_RBANK_NUM)
			pctx_video->rbank[plane_inx].r_index = 0;
		else
			pctx_video->rbank[plane_inx].r_index = i+1;
	}
}

int mtk_video_display_suspend(
	struct platform_device *pdev,
	pm_message_t state)
{
	struct device *dev = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MGW_MIU_MASK];

	if (!pdev) {
		DRM_ERROR("[%s, %d]: platform_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	DRM_INFO("\033[1;34m [%s][%d] \033[m\n", __func__, __LINE__);

	memset(&paramOut, 0, sizeof(paramOut));
	paramOut.reg = reg;

	memset(reg, 0, sizeof(reg));

	dev = &pdev->dev;
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;

	/* Disable frc */
	mtk_video_display_frc_suspend(dev, pctx);

	_mtk_video_display_set_mgwdmaEnable(pctx_video, TRUE);

	drv_hwreg_render_display_set_mgw_miu_mask(TRUE, &paramOut);
	return 0;
}

int mtk_video_display_mute_resume(struct mtk_tv_kms_context *pctx)
{
	if (!pctx)
		return -ENODEV;

	_mtk_video_display_set_mute_init(pctx);
	return 0;
}

int mtk_video_display_resume(
	struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct video_plane_prop *plane_props = NULL;
	int i = 0;
	struct hwregOut paramOut;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_MGW_MIU_MASK];

	if (!pdev) {
		DRM_ERROR("[%s, %d]: platform_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	DRM_INFO("\033[1;34m [%s][%d] \033[m\n", __func__, __LINE__);

	memset(&paramOut, 0, sizeof(paramOut));
	paramOut.reg = reg;

	memset(reg, 0, sizeof(reg));

	dev = &pdev->dev;
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;

	for (i = 0; i < MTK_VPLANE_TYPE_MAX; i++) {
		pctx_video->mgwdmaEnable[i] = 0;
		pctx_video->extWinEnable[i] = 0;
		pctx_video->preinsertEnable[i] = 0;
		pctx_video->postfillEnable[i] = 0;
		pctx_video->old_layer[i] = OLD_LAYER_INITIAL_VALUE;
		pctx_video->old_layer_en[i] = 0;
	}

	for (i = 0 ; i < MAX_WINDOW_NUM; i++) {
		plane_ctx = (pctx_video->plane_ctx+i);

		plane_ctx->rwdiff = 0;
		plane_ctx->protectVal = 0;

		plane_props = (pctx_video->video_plane_properties + i);
		plane_props->old_prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE] = MTK_VPLANE_TYPE_NONE;
		plane_props->old_prop_val[EXT_VPLANE_PROP_BUF_MODE] = MTK_VPLANE_BUF_MODE_NONE;
		plane_props->old_prop_val[EXT_VPLANE_PROP_DISP_MODE] = MTK_VPLANE_DISP_MODE_NONE;

		if (plane_ctx->meta_db != NULL) {
			DRM_INFO("[%s][%d] put meta_db=0x%p\n",
				__func__, __LINE__, plane_ctx->meta_db);
			if (plane_ctx->meta_va != NULL) {
				dma_buf_vunmap(plane_ctx->meta_db, plane_ctx->meta_va);
				plane_ctx->meta_va = NULL;
			}
			dma_buf_put(plane_ctx->meta_db);
			plane_ctx->meta_db = NULL;
		}
	}

	_mtk_video_display_set_zpos(pctx_video, TRUE);

	/* init blend out size */
	_mtk_video_display_set_blend_size(pctx, TRUE);

	/* init window alpha */
	_mtk_video_display_set_windowAlpha(pctx);

	/* init background alpha */
	_mtk_video_display_set_BackgroundAlpha(pctx);

	/* init frc */
	mtk_video_display_frc_resume(dev, pctx);

	/* init cfd setting */

	mtk_video_display_SetCFD_Setting(
		pctx->panel_priv.hw_info.pnl_lib_version,
		&g_st_cfd_last_csc_input,
		&g_st_cfd_last_csc_mainout,
		&g_st_cfd_last_csc_deltaout);

	drv_hwreg_render_display_set_mgw_miu_mask(FALSE, &paramOut);

	return 0;
}

/* sysfs store */

static ssize_t enable_expect_pqout_size_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	unsigned long val = 0;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx->sw_caps.enable_expect_pqout_size = val;

	return count;
}

static ssize_t enable_pqu_ds_size_ctl_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	unsigned long val = 0;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx->sw_caps.enable_pqu_ds_size_ctl = val;

	return count;
}

/* sysfs show */

static ssize_t debug_info_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *video = NULL;
	struct mtk_video_dbg_info win_dbg_info;
	int len = 0;
	int i;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	video = &pctx->video_priv;
	memset(&win_dbg_info, 0, sizeof(win_dbg_info));

	win_dbg_info.crop_win = vmalloc(sizeof(struct window) * MAX_WINDOW_NUM);
	win_dbg_info.disp_win = vmalloc(sizeof(struct window) * MAX_WINDOW_NUM);

	for (i = 0; i < MAX_WINDOW_NUM; i++) {
		win_dbg_info.planeType[i] = (video->video_plane_properties + i)->
				prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE];
		drv_hwreg_render_display_get_rwDiff(i, &win_dbg_info.rwdiff[i]);
		drv_hwreg_render_display_get_dispWindow(i, &win_dbg_info.disp_win[i]);
		drv_hwreg_render_display_get_cropWindow(
				win_dbg_info.planeType[i],
				video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN],
				i, &win_dbg_info.crop_win[i]);

		if ((win_dbg_info.planeType[i] == MTK_VPLANE_TYPE_MULTI_WIN) &&
			(video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN] > 1)) {
			// w/h of cropWin is the same as dispWin
			win_dbg_info.crop_win[i].w = win_dbg_info.disp_win[i].w;
			win_dbg_info.crop_win[i].h = win_dbg_info.disp_win[i].h;
		}

		drv_hwreg_render_display_get_postFillWinEnable(
				win_dbg_info.planeType[i], i, &win_dbg_info.postFillWin_en[i]);
		drv_hwreg_render_display_get_postFillMuteScreen(
				win_dbg_info.planeType[i], i, &win_dbg_info.mute_en[i]);
	}

#define SHOW_WIN_INFO(title, elem) do { \
	len += snprintf(buf + len, PAGE_SIZE - len, "%s |", title); \
	for (i = 0; i < MAX_WINDOW_NUM; i++) { \
		len += snprintf(buf + len, PAGE_SIZE - len, " %5lld |", (uint64_t)(elem)); \
	} \
	len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
} while (0)
	SHOW_WIN_INFO("     Video window      ", i);
	SHOW_WIN_INFO(" crop_x (param)        ", video->old_plane_ctx[i].crop_win.x);
	SHOW_WIN_INFO(" crop_x (reg)          ", win_dbg_info.crop_win[i].x);
	SHOW_WIN_INFO(" crop_y (param)        ", video->old_plane_ctx[i].crop_win.y);
	SHOW_WIN_INFO(" crop_y (reg)          ", win_dbg_info.crop_win[i].y);
	SHOW_WIN_INFO(" crop_w (param)        ", video->old_plane_ctx[i].crop_win.w);
	SHOW_WIN_INFO(" crop_w (reg)          ", win_dbg_info.crop_win[i].w);
	SHOW_WIN_INFO(" crop_h (param)        ", video->old_plane_ctx[i].crop_win.h);
	SHOW_WIN_INFO(" crop_h (reg)          ", win_dbg_info.crop_win[i].h);
	SHOW_WIN_INFO(" disp_x (param)        ", video->old_plane_ctx[i].disp_win.x);
	SHOW_WIN_INFO(" disp_x (reg)          ", win_dbg_info.disp_win[i].x);
	SHOW_WIN_INFO(" disp_y (param)        ", video->old_plane_ctx[i].disp_win.y);
	SHOW_WIN_INFO(" disp_y (reg)          ", win_dbg_info.disp_win[i].y);
	SHOW_WIN_INFO(" disp_w (param)        ", video->old_plane_ctx[i].disp_win.w);
	SHOW_WIN_INFO(" disp_w (reg)          ", win_dbg_info.disp_win[i].w);
	SHOW_WIN_INFO(" disp_h (param)        ", video->old_plane_ctx[i].disp_win.h);
	SHOW_WIN_INFO(" disp_h (reg)          ", win_dbg_info.disp_win[i].h);
	SHOW_WIN_INFO(" rwdiff (param)        ", video->plane_ctx[i].rwdiff);
	SHOW_WIN_INFO(" rwdiff (reg)          ", win_dbg_info.rwdiff[i]);
	SHOW_WIN_INFO(" PostFillWin_en (param)", video->old_plane_ctx[i].post_fill_win_en);
	SHOW_WIN_INFO(" PostFillWin_en (reg)  ", win_dbg_info.postFillWin_en[i]);
	SHOW_WIN_INFO(" mute_en (reg)         ", win_dbg_info.mute_en[i]);
#undef SHOW_WIN_INFO

	len += snprintf(buf + len, PAGE_SIZE - len, " video_plane_type       |");
	for (i = 0; i < MAX_WINDOW_NUM; i++) {
		switch (win_dbg_info.planeType[i]) {
		case MTK_VPLANE_TYPE_NONE:
			len += snprintf(buf + len, PAGE_SIZE - len, "  NONE |");
		break;
		case MTK_VPLANE_TYPE_MEMC:
			len += snprintf(buf + len, PAGE_SIZE - len, "  MEMC |");
		break;
		case MTK_VPLANE_TYPE_MULTI_WIN:
			len += snprintf(buf + len, PAGE_SIZE - len, "   MGW |");
		break;
		case MTK_VPLANE_TYPE_SINGLE_WIN1:
			len += snprintf(buf + len, PAGE_SIZE - len, "  DMA0 |");
		break;
		case MTK_VPLANE_TYPE_SINGLE_WIN2:
			len += snprintf(buf + len, PAGE_SIZE - len, "  DMA1 |");
		break;
		default:
			len += snprintf(buf + len, PAGE_SIZE - len, "  NONE |");
		}
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n\n");

	vfree(win_dbg_info.crop_win);
	vfree(win_dbg_info.disp_win);

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t debug_info_plane_type_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *video = NULL;
	int i;
	int len = 0;
	struct mtk_tv_kms_video_plane_dbgInfo dbgInfo;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	video = &pctx->video_priv;
	memset(&dbgInfo, 0, sizeof(dbgInfo));
	dbgInfo.scl_inputWin = vmalloc(sizeof(struct window) * MTK_VPLANE_TYPE_MAX);
	dbgInfo.scl_outputWin = vmalloc(sizeof(struct window) * MTK_VPLANE_TYPE_MAX);

	for (i = 0; i < MTK_VPLANE_TYPE_MAX; i++) {
		drv_hwreg_render_display_get_scaling_info(
			i, &dbgInfo.hvspbypass[i],
			&dbgInfo.scl_inputWin[i], &dbgInfo.scl_outputWin[i]);
		drv_hwreg_render_display_get_layerControl(
			i, &dbgInfo.zpos[i], &dbgInfo.layerEn[i]);

		dbgInfo.mgwPlaneNum = video->plane_num[MTK_VPLANE_TYPE_MULTI_WIN];
		drv_hwreg_render_display_get_thinReadFactor(
			i, dbgInfo.mgwPlaneNum, &dbgInfo.thinReadFactor[i]);
		drv_hwreg_render_display_get_preInsertEnable(
			i, &dbgInfo.preInsert_en[i]);
		drv_hwreg_render_display_get_preInsertImageSize(
			i, &dbgInfo.preInsert_win[i]);
		dbgInfo.preInsert_w[i] =
			((dbgInfo.preInsert_win[i].h_end) -
			(dbgInfo.preInsert_win[i].h_start)) + 1;
		dbgInfo.preInsert_h[i] =
			((dbgInfo.preInsert_win[i].v_end) -
			(dbgInfo.preInsert_win[i].v_start)) + 1;
		drv_hwreg_render_display_get_preInsertFrameSize(
			i, &dbgInfo.preInsert_htt[i], &dbgInfo.preInsert_vtt[i]);
		drv_hwreg_render_display_get_postFillEnable(
			i, &dbgInfo.postFill_en[i]);
		drv_hwreg_render_display_get_postFillFrameSize(
			i, &dbgInfo.postFill_htt[i], &dbgInfo.postFill_vtt[i]);
		drv_hwreg_render_display_get_postFillWindowARGB(
			i, &dbgInfo.postFill_argb[i]);
	}

#define SHOW_VPLANE_INFO(title, elem) do {							\
	len += snprintf(buf + len, PAGE_SIZE - len, "%s |", title);				\
	for (i = 0; i < MTK_VPLANE_TYPE_MAX; i++) {						\
		len += snprintf(buf + len, PAGE_SIZE - len, " %5lld |", (uint64_t)(elem));	\
	}											\
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");					\
} while (0)
	len += snprintf(buf + len, PAGE_SIZE - len,
			 "    Video Plane type    |   vop |  memc |   MGW |  DMA0 |  DMA1 |\n");
	SHOW_VPLANE_INFO(" zpos (param)          ", video->zpos_layer[i]);
	SHOW_VPLANE_INFO(" zpos (reg)            ", dbgInfo.zpos[i]);
	SHOW_VPLANE_INFO(" layer_en (param)      ", video->old_layer_en[i]);
	SHOW_VPLANE_INFO(" layer_en (reg)        ", dbgInfo.layerEn[i]);
	SHOW_VPLANE_INFO(" hvsp_bypass           ", dbgInfo.hvspbypass[i]);
	SHOW_VPLANE_INFO(" ThinReadFactor        ", dbgInfo.thinReadFactor[i]);
	SHOW_VPLANE_INFO(" PreInsert_en          ", dbgInfo.preInsert_en[i]);
	SHOW_VPLANE_INFO(" PreInsert_h_sta       ", dbgInfo.preInsert_win[i].h_start);
	SHOW_VPLANE_INFO(" PreInsert_h_end       ", dbgInfo.preInsert_win[i].h_end);
	SHOW_VPLANE_INFO(" PreInsert_v_sta       ", dbgInfo.preInsert_win[i].v_start);
	SHOW_VPLANE_INFO(" PreInsert_v_end       ", dbgInfo.preInsert_win[i].v_end);
	SHOW_VPLANE_INFO(" PreInsert_w           ", dbgInfo.preInsert_w[i]);
	SHOW_VPLANE_INFO(" PreInsert_h           ", dbgInfo.preInsert_h[i]);
	SHOW_VPLANE_INFO(" PreInsert_htt         ", dbgInfo.preInsert_htt[i]);
	SHOW_VPLANE_INFO(" PreInsert_vtt         ", dbgInfo.preInsert_vtt[i]);
	SHOW_VPLANE_INFO(" PostFill_en           ", dbgInfo.postFill_en[i]);
	SHOW_VPLANE_INFO(" PostFill_htt          ", dbgInfo.postFill_htt[i]);
	SHOW_VPLANE_INFO(" PostFill_vtt          ", dbgInfo.postFill_vtt[i]);
	SHOW_VPLANE_INFO(" PostFill_alpha        ", dbgInfo.postFill_argb[i].alpha);
	SHOW_VPLANE_INFO(" PostFill_red          ", dbgInfo.postFill_argb[i].R);
	SHOW_VPLANE_INFO(" PostFill_green        ", dbgInfo.postFill_argb[i].G);
	SHOW_VPLANE_INFO(" PostFill_blue         ", dbgInfo.postFill_argb[i].B);
	SHOW_VPLANE_INFO(" scl_input_w (param)   ", video->scl_inputWin[i].w);
	SHOW_VPLANE_INFO(" scl_input_w (reg)     ", dbgInfo.scl_inputWin[i].w);
	SHOW_VPLANE_INFO(" scl_input_h (param)   ", video->scl_inputWin[i].h);
	SHOW_VPLANE_INFO(" scl_input_h (reg)     ", dbgInfo.scl_inputWin[i].h);
	SHOW_VPLANE_INFO(" scl_output_w (param)  ", video->scl_outputWin[i].w);
	SHOW_VPLANE_INFO(" scl_output_w (reg)    ", dbgInfo.scl_outputWin[i].w);
	SHOW_VPLANE_INFO(" scl_output_h (param)  ", video->scl_outputWin[i].h);
	SHOW_VPLANE_INFO(" scl_output_h (reg)    ", dbgInfo.scl_outputWin[i].h);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
#undef SHOW_VPLANE_INFO

	vfree(dbgInfo.scl_inputWin);
	vfree(dbgInfo.scl_outputWin);

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t mute_color_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ARGB color = { };
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;
	video_mute_hw_ver = video_hw_version->video_mute;

	if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_VB)
		// all planes have same mute color
		drv_hwreg_render_display_get_postFillWindowARGB(MTK_VPLANE_TYPE_MULTI_WIN, &color);
	else if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_OSDB)
		drv_hwreg_render_display_get_mute_color(&color);
	else
		DRM_ERROR("[%s, %d]: invalid mute version\n", __func__, __LINE__);

	return sysfs_emit(buf,
			"Mute Color:\nRed   = 0x%llx\nGreen = 0x%llx\nBlue  = 0x%llx\n",
			color.R, color.G, color.B);
}

static ssize_t mute_enable_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "mute debug, mute enable:%d\n",
			gMtkDrmDbg.mute_enable.bMuteEn);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mute_enable_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	gMtkDrmDbg.mute_enable.bEn = TRUE;
	gMtkDrmDbg.mute_enable.bMuteEn = val;
	return count;
}

static ssize_t mute_color_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_POST_FILL_WINDOW_RGB];
	struct hwregPostFillWindowARGBIn paramIn;
	struct hwregOut paramOut;
	uint64_t R_Cr = 0, G_Y = 0, B_Cb = 0;
	int ret, i;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;

	if (!dev || !buf) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;
	video_mute_hw_ver = video_hw_version->video_mute;

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	ret = mtk_render_common_parser_cmd_int(buf, "r", &R_Cr);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "g", &G_Y);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser green, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "b", &B_Cb);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser blue, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	DRM_INFO("[%s][%d]:R_V:0x%llx,G_Y:0x%llx,B_U:0x%llx", __func__, __LINE__, R_Cr, G_Y, B_Cb);

	if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_VB) {
		for (i = 0; i < MTK_VPLANE_TYPE_MAX; ++i) {
			memset(&paramIn, 0, sizeof(struct hwregPostFillWindowARGBIn));
			memset(&paramOut, 0, sizeof(struct hwregOut));
			paramIn.RIU = true;
			paramIn.planeType = i;
			paramIn.windowARGB.R = R_Cr;
			paramIn.windowARGB.G = G_Y;
			paramIn.windowARGB.B = B_Cb;
			paramOut.reg = reg;
			drv_hwreg_render_display_set_postFillWindowRGB(paramIn, &paramOut);
		}
	} else if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_OSDB) {
		memset(&paramIn, 0, sizeof(struct hwregPostFillWindowARGBIn));
		memset(&paramOut, 0, sizeof(struct hwregOut));
		paramIn.RIU = true;
		paramIn.planeType = 0;	// don't care
		paramIn.windowARGB.R = R_Cr;
		paramIn.windowARGB.G = G_Y;
		paramIn.windowARGB.B = B_Cb;
		paramOut.reg = reg;
		drv_hwreg_render_display_set_mute_color(paramIn, &paramOut);
	} else {
		DRM_ERROR("[%s, %d]: invalid mute version\n", __func__, __LINE__);
		return -EINVAL;
	}
EXIT:
	return count;
}

static ssize_t default_background_color_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	// not implement yet
	return 0;
}

static ssize_t default_background_color_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_POST_FILL_WINDOW_RGB];
	struct hwregDefaultBackgroundRGBIn paramIn;
	struct hwregOut paramOut;
	uint64_t R_Cr = 0, G_Y = 0, B_Cb = 0;
	int ret = 0;

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	ret = mtk_render_common_parser_cmd_int(buf, "r", &R_Cr);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "g", &G_Y);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser green, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "b", &B_Cb);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser blue, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	DRM_INFO("[%s][%d]:R_V:0x%llx,G_Y:0x%llx,B_U:0x%llx", __func__, __LINE__, R_Cr, G_Y, B_Cb);

	memset(&paramIn, 0, sizeof(struct hwregDefaultBackgroundRGBIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	paramIn.RIU = true;
	paramIn.RGB.R = R_Cr;
	paramIn.RGB.G = G_Y;
	paramIn.RGB.B = B_Cb;

	drv_hwreg_render_display_set_defaultBackgroundRGB(paramIn, &paramOut);

EXIT:
	return count;
}

static ssize_t vb_color_space_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	uint8_t OutputDataFormat = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_video_display_Cfd_GetFormat(&OutputDataFormat);

	if (ret == 0)
		ret = snprintf(buf, PAGE_SIZE, "%d\n", OutputDataFormat);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mgw_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->mgw);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t vb_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->video_blending);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mute_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->video_mute);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t disp_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->disp_win);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t irq_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->irq);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t pattern_version_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_version = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	video_hw_version = &pctx_video->video_hw_ver;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", video_hw_version->pattern);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t enable_expect_pqout_size_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->sw_caps.enable_expect_pqout_size);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t enable_pqu_ds_size_ctl_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->sw_caps.enable_pqu_ds_size_ctl);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t dma_num_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->hw_caps.dma_num);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mgw_num_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->hw_caps.mgw_num);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t memc_num_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.memc_num);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t window_num_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->hw_caps.window_num);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_gamma_bit_num_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->hw_caps.panel_gamma_bit);


	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_od_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->hw_caps.support_OD);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_oled_demura_in_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret  = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.support_OLED_demura_in);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_oled_boosting_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			pctx->hw_caps.support_OLED_boosting);


	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_hwmode_planetype_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.support_hwmode_planetype);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_swmode_planetype_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			pctx->hw_caps.support_swmode_planetype);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_bypassmode_planetype_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.support_bypassmode_planetype);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_h_scl_down_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		pctx->hw_caps.support_h_scl_down);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_render_scaling_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.support_render_scaling);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_live_tone_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.support_live_tone);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_i_view_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.support_i_view);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t support_i_view_pip_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.support_i_view_pip);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t render_p_engine_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.render_p_engine);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t byte_per_word_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.byte_per_word);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t disp_x_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.disp_x_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t disp_y_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.disp_y_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t disp_w_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.disp_w_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t disp_h_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.disp_h_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_x_align_420_422_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_x_align_420_422);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_x_align_444_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_x_align_444);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_y_align_420_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_y_align_420);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_y_align_444_422_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_y_align_444_422);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_w_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_w_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t crop_h_align_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%u\n",
		pctx->hw_caps.crop_h_align);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static DEVICE_ATTR_RO(debug_info);
static DEVICE_ATTR_RO(debug_info_plane_type);
static DEVICE_ATTR_RW(mute_enable);
static DEVICE_ATTR_RW(mute_color);
static DEVICE_ATTR_RW(default_background_color);
static DEVICE_ATTR_RO(vb_color_space);
static DEVICE_ATTR_RW(enable_expect_pqout_size);
static DEVICE_ATTR_RW(enable_pqu_ds_size_ctl);
static DEVICE_ATTR_RO(mgw_version);
static DEVICE_ATTR_RO(vb_version);
static DEVICE_ATTR_RO(mute_version);
static DEVICE_ATTR_RO(disp_version);
static DEVICE_ATTR_RO(irq_version);
static DEVICE_ATTR_RO(pattern_version);
static DEVICE_ATTR_RO(dma_num);
static DEVICE_ATTR_RO(mgw_num);
static DEVICE_ATTR_RO(memc_num);
static DEVICE_ATTR_RO(window_num);
static DEVICE_ATTR_RO(panel_gamma_bit_num);
static DEVICE_ATTR_RO(support_od);
static DEVICE_ATTR_RO(support_oled_demura_in);
static DEVICE_ATTR_RO(support_oled_boosting);
static DEVICE_ATTR_RO(support_hwmode_planetype);
static DEVICE_ATTR_RO(support_swmode_planetype);
static DEVICE_ATTR_RO(support_bypassmode_planetype);
static DEVICE_ATTR_RO(support_h_scl_down);
static DEVICE_ATTR_RO(support_render_scaling);
static DEVICE_ATTR_RO(support_live_tone);
static DEVICE_ATTR_RO(support_i_view);
static DEVICE_ATTR_RO(support_i_view_pip);
static DEVICE_ATTR_RO(render_p_engine);
static DEVICE_ATTR_RO(byte_per_word);
static DEVICE_ATTR_RO(disp_x_align);
static DEVICE_ATTR_RO(disp_y_align);
static DEVICE_ATTR_RO(disp_w_align);
static DEVICE_ATTR_RO(disp_h_align);
static DEVICE_ATTR_RO(crop_x_align_420_422);
static DEVICE_ATTR_RO(crop_x_align_444);
static DEVICE_ATTR_RO(crop_y_align_420);
static DEVICE_ATTR_RO(crop_y_align_444_422);
static DEVICE_ATTR_RO(crop_w_align);
static DEVICE_ATTR_RO(crop_h_align);

static struct attribute *mtk_drm_video_display_attrs[] = {
	&dev_attr_debug_info.attr,
	&dev_attr_debug_info_plane_type.attr,
	&dev_attr_mute_enable.attr,
	&dev_attr_mute_color.attr,
	&dev_attr_default_background_color.attr,
	&dev_attr_vb_color_space.attr,
	NULL
};

static struct attribute *mtk_drm_video_display_attrs_version[] = {
	&dev_attr_mgw_version.attr,
	&dev_attr_vb_version.attr,
	&dev_attr_mute_version.attr,
	&dev_attr_disp_version.attr,
	&dev_attr_irq_version.attr,
	&dev_attr_pattern_version.attr,
	NULL
};

static struct attribute *mtk_drm_video_display_attrs_capability[] = {
	&dev_attr_dma_num.attr,
	&dev_attr_mgw_num.attr,
	&dev_attr_memc_num.attr,
	&dev_attr_window_num.attr,
	&dev_attr_panel_gamma_bit_num.attr,
	&dev_attr_support_od.attr,
	&dev_attr_support_oled_demura_in.attr,
	&dev_attr_support_oled_boosting.attr,
	&dev_attr_support_hwmode_planetype.attr,
	&dev_attr_support_swmode_planetype.attr,
	&dev_attr_support_bypassmode_planetype.attr,
	&dev_attr_support_h_scl_down.attr,
	&dev_attr_support_render_scaling.attr,
	&dev_attr_enable_expect_pqout_size.attr,
	&dev_attr_enable_pqu_ds_size_ctl.attr,
	&dev_attr_support_live_tone.attr,
	&dev_attr_support_i_view.attr,
	&dev_attr_support_i_view_pip.attr,
	&dev_attr_render_p_engine.attr,
	&dev_attr_byte_per_word.attr,
	&dev_attr_disp_x_align.attr,
	&dev_attr_disp_y_align.attr,
	&dev_attr_disp_w_align.attr,
	&dev_attr_disp_h_align.attr,
	&dev_attr_crop_x_align_420_422.attr,
	&dev_attr_crop_x_align_444.attr,
	&dev_attr_crop_y_align_420.attr,
	&dev_attr_crop_y_align_444_422.attr,
	&dev_attr_crop_w_align.attr,
	&dev_attr_crop_h_align.attr,
	NULL
};

static const struct attribute_group mtk_drm_video_display_attr_group = {
	.name = "video_display",
	.attrs = mtk_drm_video_display_attrs
};

static const struct attribute_group mtk_drm_video_display_attrs_version_group = {
	.name = "version",
	.attrs = mtk_drm_video_display_attrs_version
};

static const struct attribute_group mtk_drm_video_display_capability_group = {
	.name = "capability",
	.attrs = mtk_drm_video_display_attrs_capability
};

void mtk_video_display_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_drm_video_display_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_drm_video_display_attrs_version_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}

	ret = sysfs_update_group(&pdev->dev.kobj, &mtk_drm_video_display_capability_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}
}

void mtk_video_display_remove_sysfs(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove device attribute files");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_drm_video_display_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &mtk_drm_video_display_attrs_version_group);
	sysfs_remove_group(&pdev->dev.kobj, &mtk_drm_video_display_capability_group);
}

int mtk_video_display_SetCFD_Setting(
	uint32_t pnl_ver,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_input,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_mainout,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_deltaout)
{
	static struct ST_PQ_CFD_SET_INPUT_CSC_PARAMS st_cfd_set_csc_in = {0};
	static struct ST_PQ_CFD_SET_CUS_CSC_INFO st_cfd_set_csc_info = {0};

	if (pst_cfd_set_csc_input == NULL
		|| pst_cfd_set_csc_mainout == NULL) {
		DRM_ERROR("[%s, %d]: ptx or CSC in/main_out is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	DRM_INFO("[%s, %d] pnl_ver=%d in(%d, %d, %d) out(%d, %d, %d) delta(%d, %d, %d)\n",
		__func__, __LINE__, pnl_ver,
		pst_cfd_set_csc_input->u8Data_Format, pst_cfd_set_csc_input->u8Range,
		pst_cfd_set_csc_input->u8ColorSpace, pst_cfd_set_csc_mainout->u8Data_Format,
		pst_cfd_set_csc_mainout->u8Range, pst_cfd_set_csc_mainout->u8ColorSpace,
		pst_cfd_set_csc_deltaout->u8Data_Format, pst_cfd_set_csc_deltaout->u8Range,
		pst_cfd_set_csc_deltaout->u8ColorSpace);

	if (pnl_ver == DRV_PNL_VERSION0100
		|| pnl_ver == DRV_PNL_VERSION0300) {  //M6 E2 ,M6 E3
			//frame domain in csc
		if (pst_cfd_set_csc_deltaout == NULL) {
			DRM_ERROR("[%s, %d]: pst_cfd_set_csc_deltaout csc setting is NULL.\n",
			__func__, __LINE__);
			return -ENODEV;
		}

		/* FrameDomain_SetInputCsc */
		st_cfd_set_csc_in.u8OutputDataFormat = E_PQ_CFD_DATA_FORMAT_RGB;
		st_cfd_set_csc_in.u8OutputRange = E_PQ_CFD_COLOR_RANGE_FULL;
		st_cfd_set_csc_in.u8OutputColorSpace = 0;

		st_cfd_set_csc_in.u8InputDataFormat =
			pst_cfd_set_csc_input->u8Data_Format;
		st_cfd_set_csc_in.u8InputRange =
			pst_cfd_set_csc_input->u8Range;
		st_cfd_set_csc_in.u8InputColorSpace =
			pst_cfd_set_csc_input->u8ColorSpace;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MGW;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA0;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA1;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MEMC;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

	DRM_INFO("[%s, %d] MApi_PQ_CFD_FrameDomain_SetInputCsc in(%d, %d, %d) out(%d, %d, %d)\n",
		__func__, __LINE__, st_cfd_set_csc_info.u8InputDataFormat,
		st_cfd_set_csc_info.u8InputRange, st_cfd_set_csc_info.u8InputColorSpace,
		st_cfd_set_csc_info.u8OutputDataFormat, st_cfd_set_csc_info.u8OutputRange,
		st_cfd_set_csc_info.u8OutputColorSpace);

		/* FrameDomain_SetOutputCsc */
		Frame_Domain_CFD_OutputDataFormat =
			st_cfd_set_csc_in.u8OutputDataFormat;
		//frame domain out csc
		st_cfd_set_csc_in.u8InputDataFormat = E_PQ_CFD_DATA_FORMAT_RGB;
		st_cfd_set_csc_in.u8InputRange = E_PQ_CFD_COLOR_RANGE_FULL;
		st_cfd_set_csc_in.u8InputColorSpace = 0;

		st_cfd_set_csc_in.u8OutputDataFormat =
			pst_cfd_set_csc_mainout->u8Data_Format;
		st_cfd_set_csc_in.u8OutputRange =
			pst_cfd_set_csc_mainout->u8Range;
		st_cfd_set_csc_in.u8OutputColorSpace =
			pst_cfd_set_csc_mainout->u8ColorSpace;
		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_TCON_MAIN_OUT;

		MApi_PQ_CFD_FrameDomain_SetOutputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8OutputDataFormat =
			pst_cfd_set_csc_deltaout->u8Data_Format;
		st_cfd_set_csc_in.u8OutputRange =
			pst_cfd_set_csc_deltaout->u8Range;
		st_cfd_set_csc_in.u8OutputColorSpace =
			pst_cfd_set_csc_deltaout->u8ColorSpace;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_TCON_DELTA_OUT;
		MApi_PQ_CFD_FrameDomain_SetOutputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

	DRM_INFO("[%s, %d] MApi_PQ_CFD_FrameDomain_SetOutputCsc in(%d, %d, %d) out(%d, %d, %d)\n",
		__func__, __LINE__, st_cfd_set_csc_info.u8InputDataFormat,
		st_cfd_set_csc_info.u8InputRange, st_cfd_set_csc_info.u8InputColorSpace,
		st_cfd_set_csc_info.u8OutputDataFormat, st_cfd_set_csc_info.u8OutputRange,
		st_cfd_set_csc_info.u8OutputColorSpace);

		memcpy(&g_st_cfd_last_csc_deltaout, pst_cfd_set_csc_deltaout,
			sizeof(struct mtk_cfd_set_CSC));

	} else if (pnl_ver == DRV_PNL_VERSION0200
			|| pnl_ver == DRV_PNL_VERSION0203) { //M6L ,M6L E3

		/* FrameDomain_SetInputCsc */
		st_cfd_set_csc_in.u8InputDataFormat =
			pst_cfd_set_csc_input->u8Data_Format;
		st_cfd_set_csc_in.u8InputRange =
			pst_cfd_set_csc_input->u8Range;
		st_cfd_set_csc_in.u8InputColorSpace =
			pst_cfd_set_csc_input->u8ColorSpace;

		st_cfd_set_csc_in.u8OutputDataFormat =
			pst_cfd_set_csc_mainout->u8Data_Format;
		st_cfd_set_csc_in.u8OutputRange =
			pst_cfd_set_csc_mainout->u8Range;
		st_cfd_set_csc_in.u8OutputColorSpace =
			pst_cfd_set_csc_mainout->u8ColorSpace;

		Frame_Domain_CFD_OutputDataFormat =
			st_cfd_set_csc_in.u8OutputDataFormat;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MGW;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA0;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_DMA1;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MEMC;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

	DRM_INFO("[%s, %d] MApi_PQ_CFD_FrameDomain_SetInputCsc in(%d, %d, %d) out(%d, %d, %d)\n",
		__func__, __LINE__, st_cfd_set_csc_info.u8InputDataFormat,
		st_cfd_set_csc_info.u8InputRange, st_cfd_set_csc_info.u8InputColorSpace,
		st_cfd_set_csc_info.u8OutputDataFormat, st_cfd_set_csc_info.u8OutputRange,
		st_cfd_set_csc_info.u8OutputColorSpace);

	} else if (pnl_ver == DRV_PNL_VERSION0400
			|| pnl_ver == DRV_PNL_VERSION0500) { // 5876, 5879
		// For 5876 and 5879, the position of VOP2 3x3 is fixed behind MEMC Y2R,
		// Thus, MEMC Y2R should be disabled or bypassed.
		// That is, input color info should be the same as the output color info.

		/* FrameDomain_SetInputCsc only MEMC */
		st_cfd_set_csc_in.u8InputDataFormat =
			pst_cfd_set_csc_input->u8Data_Format;
		st_cfd_set_csc_in.u8InputRange =
			pst_cfd_set_csc_input->u8Range;
		st_cfd_set_csc_in.u8InputColorSpace =
			pst_cfd_set_csc_input->u8ColorSpace;

		st_cfd_set_csc_in.u8OutputDataFormat =
			pst_cfd_set_csc_input->u8Data_Format;
		st_cfd_set_csc_in.u8OutputRange =
			pst_cfd_set_csc_input->u8Range;
		st_cfd_set_csc_in.u8OutputColorSpace =
			pst_cfd_set_csc_input->u8ColorSpace;

		st_cfd_set_csc_in.u8WindowType = E_PQ_CFD_CSC_MEMC;
		MApi_PQ_CFD_FrameDomain_SetInputCsc(
			&st_cfd_set_csc_in, &st_cfd_set_csc_in, &st_cfd_set_csc_info);

	DRM_INFO("[%s, %d] MApi_PQ_CFD_FrameDomain_SetInputCsc in(%d, %d, %d) out(%d, %d, %d)\n",
		__func__, __LINE__, st_cfd_set_csc_info.u8InputDataFormat,
		st_cfd_set_csc_info.u8InputRange, st_cfd_set_csc_info.u8InputColorSpace,
		st_cfd_set_csc_info.u8OutputDataFormat, st_cfd_set_csc_info.u8OutputRange,
		st_cfd_set_csc_info.u8OutputColorSpace);

	} else {
		DRM_INFO("[%s][%d]: chip not support\n'", __func__, __LINE__);
		return -ENODEV;
	}
	memcpy(&g_st_cfd_last_csc_input, pst_cfd_set_csc_input, sizeof(struct mtk_cfd_set_CSC));
	memcpy(&g_st_cfd_last_csc_mainout, pst_cfd_set_csc_mainout, sizeof(struct mtk_cfd_set_CSC));
	g_st_cfd_last_csc_tconin.u8ColorSpace = st_cfd_set_csc_in.u8OutputColorSpace;
	g_st_cfd_last_csc_tconin.u8Data_Format = st_cfd_set_csc_in.u8OutputDataFormat;
	g_st_cfd_last_csc_tconin.u8Range = pst_cfd_set_csc_input->u8Range;

	return 0;
}

int mtk_video_display_Cfd_GetFormat(
	u8 *OutputDataFormat)
{
	if (!OutputDataFormat) {
		DRM_ERROR("[%s, %d]: OutputDataFormat addr is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	OutputDataFormat = &Frame_Domain_CFD_OutputDataFormat;

	return 0;
}

struct mtk_cfd_set_CSC *mtk_video_display_GetCFD_RenderInCsc_Setting(void)
{
	if (Frame_Domain_CFD_OutputDataFormat == CFD_DATAFORMAT_DEFAULT)
		return NULL;
	return &g_st_cfd_last_csc_input;
}

struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_tcon_in_csc_setting(void)
{
	if (Frame_Domain_CFD_OutputDataFormat == CFD_DATAFORMAT_DEFAULT)
		return NULL;
	return &g_st_cfd_last_csc_tconin;
}

struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_rendor_main_out_csc_setting(void)
{
	if (Frame_Domain_CFD_OutputDataFormat == CFD_DATAFORMAT_DEFAULT)
		return NULL;
	return &g_st_cfd_last_csc_mainout;
}

struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_render_delta_out_csc_setting(void)
{
	if (Frame_Domain_CFD_OutputDataFormat == CFD_DATAFORMAT_DEFAULT)
		return NULL;
	return &g_st_cfd_last_csc_deltaout;
}

int mtk_video_display_SetCFD_RenderinCsc_Setting(
	struct mtk_tv_kms_context *pctx,
	void *buffer_addr)
{
	struct drm_mtk_cfd_CSC *pst_cfd_set_csc_input = (struct drm_mtk_cfd_CSC *)buffer_addr;
	struct mtk_cfd_set_CSC st_cfd_csc_render_in;
	uint32_t pnl_ver = 0;

	if ((pctx == NULL) || (pst_cfd_set_csc_input == NULL)) {
		DRM_ERROR("[%s][%d] Invalid input", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&st_cfd_csc_render_in, 0, sizeof(struct mtk_cfd_set_CSC));

	pnl_ver = pctx->panel_priv.hw_info.pnl_lib_version;
	st_cfd_csc_render_in.u8Data_Format = (uint8_t) pst_cfd_set_csc_input->data_format;
	st_cfd_csc_render_in.u8ColorSpace = (uint8_t) pst_cfd_set_csc_input->color_space;
	st_cfd_csc_render_in.u8Range = (uint8_t) pst_cfd_set_csc_input->color_range;

	return mtk_video_display_SetCFD_Setting(
		pnl_ver,
		&st_cfd_csc_render_in,
		&g_st_cfd_last_csc_mainout,
		&g_st_cfd_last_csc_deltaout);
}

int mtk_video_display_set_defaultBackgroundColor(
	struct mtk_video_context *pctx_video)
{
	struct reg_info reg[HWREG_VIDEO_REG_NUM_VB_POST_FILL_WINDOW_RGB];
	struct hwregDefaultBackgroundRGBIn paramIn;
	struct hwregOut paramOut;
	bool riu = (pctx_video == NULL || mtk_tv_sm_ml_is_valid(&pctx_video->disp_ml) == false);
	int ret;

	if (Frame_Domain_CFD_OutputDataFormat >= E_PQ_CFD_DATA_FORMAT_RESERVED_START) {
		DRM_ERROR("[%s, %d]: format 0x%x is invalid.\n",
			__func__, __LINE__, Frame_Domain_CFD_OutputDataFormat);
		return -EINVAL;
	}

	memset(&paramIn, 0, sizeof(struct hwregDefaultBackgroundRGBIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramIn.RIU = riu;
	paramOut.reg = reg;
	if (Frame_Domain_CFD_OutputDataFormat == E_PQ_CFD_DATA_FORMAT_RGB) {
		paramIn.RGB.R = RGB_BLACK_R;
		paramIn.RGB.G = RGB_BLACK_G;
		paramIn.RGB.B = RGB_BLACK_B;
	} else { // YUV
		paramIn.RGB.G = YUV_BLACK_Y;
		paramIn.RGB.B = YUV_BLACK_U;
		paramIn.RGB.R = YUV_BLACK_V;
	}
	ret = drv_hwreg_render_display_set_defaultBackgroundRGB(paramIn, &paramOut);
	if (ret == 0 && riu == false) {
		ret = mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			paramOut.regCount, (struct sm_reg *)reg);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml append cmd fail (%d)",
				__func__, __LINE__, ret);
		}
	}
	DRM_INFO("[%s, %d]: riu=%d, format=0x%x, regCount=%d\n", __func__, __LINE__,
		riu, Frame_Domain_CFD_OutputDataFormat, paramOut.regCount);
	return 0;
}

int mtk_video_display_get_blendingColorFormat_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv)
{
	uint32_t *format = (uint32_t *)data;

	if (format == NULL) {
		DRM_ERROR("[%s, %d]: param is NULL.\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (Frame_Domain_CFD_OutputDataFormat >= E_PQ_CFD_DATA_FORMAT_RESERVED_START) {
		DRM_ERROR("[%s, %d]: format 0x%x is invalid.\n",
			__func__, __LINE__, Frame_Domain_CFD_OutputDataFormat);
		return -EINVAL;
	}
	*format = (uint32_t)Frame_Domain_CFD_OutputDataFormat;
	return 0;
}

int mtk_video_display_set_hse_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	struct drm_mtk_tv_set_hse_enable *hse_enable = (struct drm_mtk_tv_set_hse_enable *)data;

	if (!drm_dev || !data || !file_priv)
		return -EINVAL;

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_frc = &pctx->frc_priv;

	if (pctx_frc->frc_as_hse) {
		struct mtk_drm_plane *plane = 0;
		struct mtk_video_context *pctx_video = NULL;
		struct video_plane_prop *plane_props = NULL;
		unsigned int plane_idx = 0;
		size_t i = 0;

		for (i = 0; i < ARRAY_SIZE(mtk_tv_crtc->plane); ++i) {
			plane = mtk_tv_crtc->plane[i];
			if (plane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO) {
				plane_idx = (unsigned int) plane->video_index;
				break;
			}
		}
		pctx_video = &pctx->video_priv;
		plane_props = (pctx_video->video_plane_properties + plane_idx);
		if (plane_props->prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE]
			!= MTK_VPLANE_TYPE_MEMC) {
			DRM_ERROR("[%s, %d]: plane type is not MEMC.\n", __func__, __LINE__);
			return -EINVAL;
		}
		mtk_video_display_set_frc_freeze(pctx, plane_idx, hse_enable->enable);
	}

	return 0;
}
