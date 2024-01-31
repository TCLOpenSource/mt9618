/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* * Copyright (c) 2020 MediaTek Inc. */

#ifndef _MTK_DRM_KMS_H_
#define _MTK_DRM_KMS_H_

#include <linux/preempt.h>
#include <linux/gfp.h>
#include <drm/drm_util.h>
#include <drm/drm_encoder.h>
#include "mtk_tv_drm_gop.h"
#include "mtk_tv_drm_video_panel.h"
#include "mtk_tv_drm_video.h"
#include "mtk_tv_drm_video_display.h"
#include "mtk_tv_drm_video_frc.h"
#include "mtk_tv_drm_video_pixelshift.h"
#include "mtk_tv_drm_global_pq.h"
#include "mtk_tv_drm_encoder.h"
#include "mtk_tv_drm_ld.h"
#include "mtk_tv_drm_pattern.h"
#include "mtk_tv_drm_demura.h"
#include "mtk_tv_drm_tvdac.h"
#include "mtk_tv_drm_autogen.h"
#include "mtk_tv_drm_pqmap.h"
#include "mtk_tv_drm_pqu_metadata.h"
#include "mtk_tv_drm_oled.h"
#include "mtk_tv_drm_common.h"

#include "hwreg_render_common_trigger_gen.h"
#include "mtk_tv_drm_common_clock.h"


#define HWREG_KMS_REG_NUM_TGEN_VTT (5)


#define MTK_DRM_TV_KMS_COMPATIBLE_STR "MTK-drm-tv-kms"
#define MTK_DRM_DTS_GOP_PLANE_NUM_TAG             "GRAPHIC_PLANE_NUM"
#define MTK_DRM_DTS_GOP_PLANE_INDEX_START_TAG     "GRAPHIC_PLANE_INDEX_START"
// FIXME: increase vsync to 0x42, dma_read to 0x46 for trigger gen temp solution
#ifdef CONFIG_MTK_DISP_BRINGUP
#define MTK_DRM_TRIGGEN_VSDLY_V         (0x42)
#define MTK_DRM_TRIGGEN_DMARDDLY_V      (0x46)
#else
#define MTK_DRM_TRIGGEN_VSDLY_V         (0x1B)
#define MTK_DRM_TRIGGEN_DMARDDLY_V      (0x1F)
#endif
#define MTK_DRM_TRIGGEN_STRDLY_V        (0x1F)
#define MTK_DRM_COMMIT_TAIL_THREAD_PRIORITY        (40)
#define MTK_DRM_ML_DISP_BUF_COUNT       (2)
#define MTK_DRM_ML_DISP_CMD_COUNT       (500)
#define MTK_DRM_ML_DISP_IRQ_BUF_COUNT   (2)
#define MTK_DRM_ML_DISP_IRQ_CMD_COUNT   (500)
#define MTK_DRM_ML_DISP_VGS_BUF_COUNT   (2)
#define MTK_DRM_ML_DISP_VGS_CMD_COUNT   (300)
#define MTK_PQ_GAMMA_LUT_SIZE (256)

#define IS_FRAMESYNC(x)     ((x == VIDEO_CRTC_FRAME_SYNC_VTTV) || \
							(x == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK))
#define MARK_METADATA_LOG	1

#define IS_RANGE_PROP(prop) ((prop)->flags & DRM_MODE_PROP_RANGE)
#define IS_ENUM_PROP(prop) ((prop)->flags & DRM_MODE_PROP_ENUM)
#define IS_BLOB_PROP(prop) ((prop)->flags & DRM_MODE_PROP_BLOB)

extern struct platform_driver mtk_drm_tv_kms_driver;
extern bool bPquEnable; // is pqurv55 enable ? declare at mtk_tv_drm_drv.c.

struct property_enum_info {
	struct drm_prop_enum_list *enum_list;
	int enum_length;
};

struct property_range_info {
	uint64_t min;
	uint64_t max;
};

struct ext_prop_info {
	char *prop_name;
	int flag;
	uint64_t init_val;
	union{
		struct property_enum_info enum_info;
		struct property_range_info range_info;
	};
};

enum ext_prop_type {
	E_EXT_PROP_TYPE_COMMON_PLANE = 0,
	E_EXT_PROP_TYPE_CRTC,
	E_EXT_PROP_TYPE_CONNECTOR,
	E_EXT_PROP_TYPE_CRTC_GRAPHIC,
	E_EXT_PROP_TYPE_CRTC_COMMON,
	E_EXT_PROP_TYPE_CONNECTOR_COMMON,
	E_EXT_PROP_TYPE_MAX,
};

enum {
	E_EXT_COMMON_PLANE_PROP_TYPE = 0,
	E_EXT_COMMON_PLANE_PROP_MAX,
};

// need to update kms.c get/set property function after adding new properties
enum ext_video_crtc_prop_type {
	// pnl property
	E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID,
	E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO,
	E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE,
	E_EXT_CRTC_PROP_LOW_LATENCY_MODE,
	E_EXT_CRTC_PROP_CUSTOMIZE_FRC_TABLE,
	E_EXT_CRTC_PROP_PANEL_TIMING,
	E_EXT_CRTC_PROP_SET_FREERUN_TIMING,
	E_EXT_CRTC_PROP_FORCE_FREERUN,
	E_EXT_CRTC_PROP_FREERUN_VFREQ,
	E_EXT_CRTC_PROP_VIDEO_LATENCY,
	E_EXT_CRTC_PROP_NO_SIGNAL,
	// ldm property
	E_EXT_CRTC_PROP_LDM_STATUS,
	E_EXT_CRTC_PROP_LDM_LUMA,
	E_EXT_CRTC_PROP_LDM_ENABLE,
	E_EXT_CRTC_PROP_LDM_STRENGTH,
	E_EXT_CRTC_PROP_LDM_DATA,
	E_EXT_CRTC_PROP_LDM_DEMO_PATTERN,
	E_EXT_CRTC_PROP_LDM_SW_REG_CTRL,
	E_EXT_CRTC_PROP_LDM_AUTO_LD,
	E_EXT_CRTC_PROP_LDM_XTendedRange,
	E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS,
	E_EXT_CRTC_PROP_LDM_INIT,
	E_EXT_CRTC_PROP_LDM_SUSPEND,
	E_EXT_CRTC_PROP_LDM_RESUME,
	E_EXT_CRTC_PROP_LDM_SUSPEND_RESUME_TEST,
	// pixelshift property
	E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE,
	E_EXT_CRTC_PROP_PIXELSHIFT_OSD_ATTACH,
	E_EXT_CRTC_PROP_PIXELSHIFT_TYPE,
	E_EXT_CRTC_PROP_PIXELSHIFT_H,
	E_EXT_CRTC_PROP_PIXELSHIFT_V,
	// demura property
	E_EXT_CRTC_PROP_DEMURA_CONFIG,
	// pattern property
	E_EXT_CRTC_PROP_PATTERN_GENERATE,
	// global pq property
	E_EXT_CRTC_PROP_GLOBAL_PQ,
	// mute color property
	E_EXT_CRTC_PROP_MUTE_COLOR,
	// background color property
	E_EXT_CRTC_PROP_BACKGROUND_COLOR,
	// graphic and video property
	E_EXT_CRTC_PROP_GOP_VIDEO_ORDER,
	E_EXT_CRTC_PROP_GOP_OSDB_LOCATION,
	// backlight property
	E_EXT_CRTC_PROP_BACKLIGHT_RANGE,
	// live-tone prepoery
	E_EXT_CRTC_PROP_LIVETONE_RANGE,
	// pqmap_node array property
	E_EXT_CRTC_PROP_PQMAP_NODE_ARRAY,
	//CFD settings
	E_EXT_CRTC_PROP_CFD_CSC_Render_In,
	// total property
	E_EXT_CRTC_PROP_MAX,
};

enum {
	E_EXT_COMMON_CRTC_PROP_TYPE = 0,
	E_EXT_COMMON_CRTC_PROP_MAX,
};

enum ext_graphic_crtc_prop_type {
	E_EXT_CRTC_GRAPHIC_PROP_MODE,
	E_EXT_CRTC_GRAPHIC_PROP_MAX,
};

enum {
	E_EXT_CONNECTOR_PROP_PNL_DBG_LEVEL,
	E_EXT_CONNECTOR_PROP_PNL_OUTPUT,
	E_EXT_CONNECTOR_PROP_PNL_SWING_LEVEL,
	E_EXT_CONNECTOR_PROP_PNL_FORCE_PANEL_DCLK,
	E_EXT_CONNECTOR_PROP_PNL_PNL_FORCE_PANEL_HSTART,
	E_EXT_CONNECTOR_PROP_PNL_PNL_OUTPUT_PATTERN,
	E_EXT_CONNECTOR_PROP_PNL_MIRROR_STATUS,
	E_EXT_CONNECTOR_PROP_PNL_SSC_EN,
	E_EXT_CONNECTOR_PROP_PNL_SSC_FMODULATION,
	E_EXT_CONNECTOR_PROP_PNL_SSC_RDEVIATION,
	E_EXT_CONNECTOR_PROP_PNL_OVERDRIVER_ENABLE,
	E_EXT_CONNECTOR_PROP_PNL_PANEL_SETTING,
	E_EXT_CONNECTOR_PROP_PNL_PANEL_INFO,
	E_EXT_CONNECTOR_PROP_PARSE_OUT_INFO_FROM_DTS,
	E_EXT_CONNECTOR_PROP_PNL_VBO_CTRLBIT,
	E_EXT_CONNECTOR_PROP_PNL_VBO_TX_MUTE_EN,
	E_EXT_CONNECTOR_PROP_PNL_SWING_VREG,
	E_EXT_CONNECTOR_PROP_PNL_PRE_EMPHASIS,
	E_EXT_CONNECTOR_PROP_PNL_SSC,
	E_EXT_CONNECTOR_PROP_PNL_OUT_MODEL,
	E_EXT_CONNECTOR_PROP_PNL_CHECK_DTS_OUTPUT_TIMING,
	E_EXT_CONNECTOR_PROP_PNL_FRAMESYNC_MLOAD,
	E_EXT_CONNECTOR_PROP_MAX,
};

enum {
	E_EXT_CONNECTOR_COMMON_PROP_TYPE,
	E_EXT_CONNECTOR_COMMON_PROP_PNL_VBO_TX_MUTE_EN,
	E_EXT_CONNECTOR_COMMON_PROP_PNL_PIXEL_MUTE_EN,
	E_EXT_CONNECTOR_COMMON_PROP_BACKLIGHT_MUTE_EN,
	E_EXT_CONNECTOR_COMMON_PROP_HMIRROR_EN,
	E_EXT_CONNECTOR_COMMON_PROP_PNL_VBO_CTRLBIT,
	E_EXT_CONNECTOR_COMMON_PROP_PNL_GLOBAL_MUTE_EN,
	E_EXT_CONNECTOR_COMMON_PROP_MAX,
};

struct ext_common_plane_prop_info {
	uint64_t prop_val[E_EXT_COMMON_PLANE_PROP_MAX];
};

struct ext_crtc_prop_info {
	uint64_t prop_val[E_EXT_CRTC_PROP_MAX];
	uint8_t prop_update[E_EXT_CRTC_PROP_MAX];
};

struct ext_crtc_graphic_prop_info {
	uint64_t prop_val[E_EXT_CRTC_GRAPHIC_PROP_MAX];
	uint8_t prop_update[E_EXT_CRTC_GRAPHIC_PROP_MAX];
};

struct ext_common_crtc_prop_info {
	uint64_t prop_val[E_EXT_COMMON_CRTC_PROP_MAX];
	uint8_t prop_update[E_EXT_COMMON_CRTC_PROP_MAX];
};

struct ext_connector_prop_info {
	uint64_t prop_val[E_EXT_CONNECTOR_PROP_MAX];
};

struct ext_common_connector_prop_info {
	uint64_t prop_val[E_EXT_CONNECTOR_COMMON_PROP_MAX];
};

struct mtk_hw_capability {
	u8 memc_num;
	u8 mgw_num;
	u8 dma_num;
	u8 window_num;
	u8  panel_gamma_bit;
	u8  support_OD;
	u8  support_OLED_demura_in;
	u8  support_OLED_boosting;
	u8  support_hwmode_planetype;
	u8  support_swmode_planetype;
	u8  support_bypassmode_planetype;
	u8  support_h_scl_down;
	u8  support_render_scaling;
	u8  support_live_tone;
	u8  support_amb_light;
	u8  support_i_view;
	u8  support_i_view_pip;
	u8  render_p_engine;
	u8  byte_per_word;
	u8  disp_x_align;
	u8  disp_y_align;
	u8  disp_w_align;
	u8  disp_h_align;
	u8  crop_x_align_420_422;
	u8  crop_x_align_444;
	u8  crop_y_align_420;
	u8  crop_y_align_444_422;
	u8  crop_w_align;
	u8  crop_h_align;

};

struct mtk_sw_capability {
	u8  enable_expect_pqout_size;
	u8  enable_pqu_ds_size_ctl;
};

enum OUTPUT_MODEL {
	E_OUT_MODEL_VG_BLENDED,
	E_OUT_MODEL_VG_SEPARATED,
	E_OUT_MODEL_VG_SEPARATED_W_EXTDEV,
	E_OUT_MODEL_ONLY_EXTDEV,
	E_OUT_MODEL_MAX,
};

enum RESUME_PATTERN_SEL {
	E_RESUME_PATTERN_SEL_DEFAULT,
	E_RESUME_PATTERN_SEL_MOD,
	E_RESUME_PATTERN_SEL_TCON,
	E_RESUME_PATTERN_SEL_GOP,
	E_RESUME_PATTERN_SEL_MULTIWIN,
	E_RESUME_PATTERN_SEL_PQ_GAMMA,
	E_RESUME_PATTERN_SEL_PAFRC_POST,
	E_RESUME_PATTERN_SEL_SEC,
	E_RESUME_PATTERN_SEL_MAX,
};

struct mtk_tv_kms_context {
	struct device *dev;
	struct drm_device *drm_dev;
	struct mtk_tv_drm_crtc crtc[MTK_DRM_CRTC_TYPE_MAX];
	struct drm_encoder encoder[MTK_DRM_ENCODER_TYPE_MAX];
	struct mtk_tv_drm_connector connector[MTK_DRM_CONNECTOR_TYPE_MAX];
	struct mtk_video_irq irq;
	int plane_num[MTK_DRM_PLANE_TYPE_MAX];
	int plane_index_start[MTK_DRM_PLANE_TYPE_MAX];
	int total_plane_num;
	struct drm_atomic_state *tvstate[MTK_DRM_CRTC_TYPE_MAX];
	struct drm_property *drm_common_plane_prop[E_EXT_COMMON_PLANE_PROP_MAX];
	struct ext_common_plane_prop_info *ext_common_plane_properties;
	struct drm_property *drm_crtc_prop[E_EXT_CRTC_PROP_MAX];
	struct ext_crtc_prop_info *ext_crtc_properties;
	struct drm_property *drm_crtc_graphic_prop[E_EXT_CRTC_GRAPHIC_PROP_MAX];
	struct ext_crtc_graphic_prop_info *ext_crtc_graphic_properties;
	struct drm_property *drm_common_crtc_prop[E_EXT_COMMON_CRTC_PROP_MAX];
	struct ext_common_crtc_prop_info *ext_common_crtc_properties;
	struct drm_property *drm_connector_prop[E_EXT_CONNECTOR_PROP_MAX];
	struct ext_connector_prop_info *ext_connector_properties;
	struct drm_property *drm_common_connector_prop[E_EXT_CONNECTOR_COMMON_PROP_MAX];
	struct ext_common_connector_prop_info *ext_common_connector_properties;
	struct mtk_gop_context gop_priv;
	struct mtk_video_context video_priv;
	struct mtk_frc_context frc_priv;
	struct mtk_pqmap_context pqmap_priv;
	struct mtk_autogen_context autogen_priv;
	struct mtk_pqu_metadata_context pqu_metadata_priv;
	struct mtk_ld_context ld_priv;
	struct mtk_global_pq_context global_pq_priv;
	struct mtk_panel_context panel_priv;
	struct mtk_demura_context demura_priv;
	struct mtk_tvdac_context tvdac_priv;
	struct mtk_pixelshift_context pixelshift_priv;
	struct mtk_hw_capability hw_caps;
	struct mtk_sw_capability sw_caps;
	struct drm_pattern_status pattern_status;
	struct drm_pattern_info pattern_info;
	/* trigger gen relate */
	struct drm_mtk_tv_trigger_gen_info trigger_gen_info;
	spinlock_t tg_lock;
	struct mtk_drm_clk clk;
	struct mtk_global_pq_hw_version globalpq_hw_ver;
	struct mtk_global_pq_amb_context amb_context;
	enum OUTPUT_MODEL out_model;
	enum RESUME_PATTERN_SEL eResumePatternselect;
	bool stub;
	bool pnlgamma_stubmode;
	bool b2r_src_stubmode;
	int framesync_version;
	int clk_version;
	int chip_series ;
};

struct mtk_tv_kms_video_plane_dbgInfo {
	uint8_t  hvspbypass[MTK_VPLANE_TYPE_MAX];
	struct  window *scl_inputWin;
	struct  window *scl_outputWin;
	uint8_t  zpos[MTK_VPLANE_TYPE_MAX];
	uint8_t  layerEn[MTK_VPLANE_TYPE_MAX];
	uint8_t  thinReadFactor[MTK_VPLANE_TYPE_MAX];
	uint8_t  mgwPlaneNum;
	uint8_t  preInsert_en[MTK_VPLANE_TYPE_MAX];
	struct  windowRect preInsert_win[MTK_VPLANE_TYPE_MAX];
	uint64_t preInsert_w[MTK_VPLANE_TYPE_MAX];
	uint64_t preInsert_h[MTK_VPLANE_TYPE_MAX];
	uint16_t preInsert_htt[MTK_VPLANE_TYPE_MAX];
	uint16_t preInsert_vtt[MTK_VPLANE_TYPE_MAX];
	uint8_t  postFill_en[MTK_VPLANE_TYPE_MAX];
	uint16_t postFill_htt[MTK_VPLANE_TYPE_MAX];
	uint16_t postFill_vtt[MTK_VPLANE_TYPE_MAX];
	struct ARGB postFill_argb[MTK_VPLANE_TYPE_MAX];
};

struct cmd_mute_enable {
	bool bEn;
	bool bMuteEn;
};

struct cmd_change_vPlane_type {
	bool win_en[MAX_WINDOW_NUM];
	uint8_t vPlane_type[MAX_WINDOW_NUM];
};

struct cmd_update_disp_size {
	bool win_en[MAX_WINDOW_NUM];
	struct window dispwindow[MAX_WINDOW_NUM];
};

struct cmd_set_rw_diff {
	bool win_en[MAX_WINDOW_NUM];
	uint8_t rw_diff[MAX_WINDOW_NUM];
};

struct mtk_tv_drm_debug {
	uint8_t atrace_enable;
	struct cmd_mute_enable mute_enable;
	struct cmd_change_vPlane_type *change_vPlane_type;
	struct cmd_update_disp_size *update_disp_size;
	struct cmd_set_rw_diff *set_rw_diff;
};

struct mtk_tv_kms_vsync_callback_param {
	const char *thread_name;
	char *mi_rt_domain_name; /* set NULL if not rt task */
	char *mi_rt_class_name;  /* set NULL if not rt task */
	void *priv_data;
	int (*callback_func)(void *priv_data);
};

static inline gfp_t _get_alloc_method(void)
{
	if (drm_can_sleep())
		return GFP_KERNEL;
	else
		return GFP_ATOMIC;
}

extern struct mtk_tv_drm_debug gMtkDrmDbg;

void mtk_tv_kms_atomic_state_clear(struct drm_atomic_state *old_state);
int mtk_tv_drm_atomic_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock);
int mtk_tv_kms_CRTC_active_handler(struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state);
int mtk_tv_panel_debugvttvinfo_print(
	struct mtk_tv_kms_context *pctx, uint16_t u16Idex);
int mtk_tv_kms_register_vsync_callback(struct mtk_tv_kms_vsync_callback_param *param);

#endif
