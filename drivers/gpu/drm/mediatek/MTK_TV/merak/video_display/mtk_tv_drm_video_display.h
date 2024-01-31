/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_VIDEO_DISPLAY_H_
#define _MTK_TV_DRM_VIDEO_DISPLAY_H_

#include <linux/dma-buf.h>
#include <linux/workqueue.h>

#include "mtk_tv_sm_ml.h"
#include "hwreg_render_video_display.h"
#include "m_pqu_pq.h"
#include "mtk_tv_drm_autogen.h"
#include "mtk_tv_drm_common_irq.h"

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define HWREG_VIDEO_REG_NUM_MGWDMA_EN (10)
#define HWREG_VIDEO_REG_NUM_FRC_OPM_MASK_EN (10)
#define HWREG_VIDEO_REG_NUM_EXT_WIN_EN (10)
#define HWREG_VIDEO_REG_NUM_MGWDMA_WIN_EN (10)
#define HWREG_VIDEO_REG_NUM_CROP_WIN (15)
#define HWREG_VIDEO_REG_NUM_SCL (30)
#define HWREG_VIDEO_REG_NUM_DISP_WIN (20)
#define HWREG_VIDEO_REG_NUM_FB_MEM_FORMAT (10)
#define HWREG_VIDEO_REG_NUM_FB (30)
#define HWREG_VIDEO_REG_NUM_RW_DIFF (10)
#define HWREG_VIDEO_REG_NUM_FREEZE (10)
#define HWREG_VIDEO_REG_NUM_BG_ALPHA (10)
#define HWREG_VIDEO_REG_NUM_WINDOW_ALPHA (20)
#define HWREG_VIDEO_REG_NUM_MUTE (20)
#define HWREG_VIDEO_REG_NUM_VB_OUT_SIZE (30)
#define HWREG_VIDEO_REG_NUM_VB_PRE_INSERT_EN (10)
#define HWREG_VIDEO_REG_NUM_VB_POST_FILL_EN (10)
#define HWREG_VIDEO_REG_NUM_VB_POST_FILL_WIN_EN (10)
#define HWREG_VIDEO_REG_NUM_VB_POST_FILL_WINDOW_RGB (10)
#define HWREG_VIDEO_REG_NUM_HW_MODE (10)
#define HWREG_VIDEO_REG_NUM_VB_LAYER_EN (10)
#define HWREG_VIDEO_REG_NUM_AID (10)
#define HWREG_VIDEO_REG_NUM_VB_MUX (10)
#define HWREG_VIDEO_REG_NUM_FRC_VGSYNC (48)
#define HWREG_VIDEO_REG_NUM_BLENDING_MUX (5)
#define HWREG_VIDEO_REG_NUM_BLENDING_EN (5)
#define HWREG_VIDEO_REG_NUM_MGW_MIU_MASK (5)
#define HWREG_VIDEO_REG_NUM_DV_GD_EN (10)
#define HWREG_VIDEO_REG_NUM_FRC_AID (5)

#define H_ScalingRatio(Input, Output) \
	{ result = ((uint64_t)(Input)) * 2097152ul; do_div(result, (Output)); \
	result += 1; do_div(result, 2); }
#define H_ScalingUpPhase(Input, Output) \
	{ Phaseresult = ((uint64_t)(Input)) * 1048576ul; \
	do_div(Phaseresult, (Output));    Phaseresult += 1048576ul; \
	Phaseresult += 1; do_div(Phaseresult, 2); }
#define H_ScalingDownPhase(Input, Output)\
	{ Phaseresult = ((uint64_t)(Input)) * 1048576ul;\
	do_div(Phaseresult, (Output));  Phaseresult = Phaseresult - 1048576ul;\
	Phaseresult += 1; do_div(Phaseresult, 2); }
#define V_ScalingRatio(Input, Output) \
	{ result = ((uint64_t)(Input)) * 2097152ul; do_div(result, (Output));\
	result += 1; do_div(result, 2); }
#define V_ScalingUpPhase(Input, Output)\
	{ Phaseresult = ((uint64_t)(Input)) * 1048576ul;\
	do_div(Phaseresult, (Output));    Phaseresult += 1048576ul;\
	Phaseresult += 1; do_div(Phaseresult, 2); }
#define V_ScalingDownPhase(Input, Output)\
	{ Phaseresult = ((uint64_t)(Input)) * 1048576ul;\
	do_div(Phaseresult, (Output));  Phaseresult = Phaseresult - 1048576ul;\
	Phaseresult += 1; do_div(Phaseresult, 2); }


#define MTK_PLANE_DISPLAY_PIPE (3)
#define MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_LINE (32)
#define MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_DIV (8)
#define MTK_VIDEO_DISPLAY_RWDIFF_PROTECT_MAX (0xFF)
#define SHIFT_16_BITS  (16)
#define SHIFT_20_BITS  (0x100000L)
#define MAX_WINDOW_NUM (16)

#define PLANE_0 (0)
#define PLANE_1 (1)
#define PLANE_2 (2)
#define FMT_MODIFIER_ARRANGE_MASK (0xFF)
#define FMT_YUV422_8B_1PLN_BPP0 (16)

#define IS_VB_TS(x) ((x == EN_VB_VERSION_TS_V0) || (x == EN_VB_VERSION_TS_V1))
#define IS_VB_LEGACY(x) ((x == EN_VB_VERSION_LEGACY_V0) || (x == EN_VB_VERSION_LEGACY_V1))

#define pctx_video_to_kms(x)	container_of(x, struct mtk_tv_kms_context, video_priv)
#define STORE_RBANK_NUM (10)

#define NUMBER_4 (4)

extern bool bPquEnable;
//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

enum ext_video_plane_prop {
	EXT_VPLANE_PROP_MUTE_SCREEN,
	EXT_VPLANE_PROP_SNOW_SCREEN,
	EXT_VPLANE_PROP_VIDEO_PLANE_TYPE,
	EXT_VPLANE_PROP_META_FD,
	EXT_VPLANE_PROP_FREEZE,
	EXT_VPLANE_PROP_INPUT_VFREQ,
	EXT_VPLANE_PROP_INPUT_SOURCE_VFREQ,
	EXT_VPLANE_PROP_BUF_MODE,
	EXT_VPLANE_PROP_DISP_MODE,
	EXT_VPLANE_PROP_DISP_WIN_TYPE,
	// memc property
	EXT_VPLANE_PROP_MEMC_LEVEL, /*15*/
	EXT_VPLANE_PROP_MEMC_GAME_MODE,
	EXT_VPLANE_PROP_MEMC_PATTERN,
	EXT_VPLANE_PROP_MEMC_MISC_TYPE,
	EXT_VPLANE_PROP_MEMC_DECODE_IDX_DIFF,
	EXT_VPLANE_PROP_MEMC_STATUS,
	EXT_VPLANE_PROP_MEMC_TRIG_GEN,
	EXT_VPLANE_PROP_MEMC_RV55_INFO,
	EXT_VPLANE_PROP_MEMC_INIT_VALUE_FOR_RV55,

	EXT_VPLANE_PROP_WINDOW_PQ,

	// pqmap
	EXT_VPLANE_PROP_PQMAP_NODE_ARRAY,

	EXT_VPLANE_PROP_MAX,
};

/**/
enum mtk_video_plane_memc_change {
	MEMC_CHANGE_NONE,
	MEMC_CHANGE_OFF,
	MEMC_CHANGE_ON,
};

enum VIDEO_BLENDING_MUX_E {
	EN_VIDEO_BLENDING_MUX_VOP = 0,
	EN_VIDEO_BLENDING_MUX_MEMC = 1,
	EN_VIDEO_BLENDING_MUX_MAX,
};

enum VIDEO_MUTE_VERSION_E {
	EN_VIDEO_MUTE_VERSION_VB = 0,
	EN_VIDEO_MUTE_VERSION_OSDB,
	EN_VIDEO_MUTE_VERSION_MAX,
};

enum VIDEO_FREEZE_VERSION_E {
	EN_VIDEO_FREEZE_VERSION_INRENDER = 0,
	EN_VIDEO_FREEZE_VERSION_INHWREG,
	EN_VIDEO_FREEZE_VERSION_MAX,
};

enum EN_CAPABILITY_CONTENT_TYPE {
	E_CAP_VERSION = 0,
	E_CAP_ID,

	E_CAPMAX
};

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct rbank_info {
	uint8_t rbank_idx[STORE_RBANK_NUM];
	uint8_t r_index;
};

struct mtk_video_plane_type_num {
	uint8_t MEMC_num;
	uint8_t MGW_num;
	uint8_t DMA1_num;
	uint8_t DMA2_num;
};

struct video_plane_prop {
	uint64_t prop_val[EXT_VPLANE_PROP_MAX];//new prop value
	uint64_t old_prop_val[EXT_VPLANE_PROP_MAX];
	uint8_t prop_update[EXT_VPLANE_PROP_MAX];
};

struct disp_mode_info {
	enum drm_mtk_vplane_disp_mode disp_mode;
	bool disp_mode_update;
};

enum meta_pq_input_source_dis {
	META_PQ_INPUTSRC_NONE_DIS = 0,      ///<NULL input
	META_PQ_INPUTSRC_VGA_DIS,           ///<1   VGA input
	META_PQ_INPUTSRC_TV_DIS,            ///<2   TV input

	META_PQ_INPUTSRC_CVBS_DIS,          ///<3   AV 1
	META_PQ_INPUTSRC_CVBS2_DIS,         ///<4   AV 2
	META_PQ_INPUTSRC_CVBS3_DIS,         ///<5   AV 3
	META_PQ_INPUTSRC_CVBS4_DIS,         ///<6   AV 4
	META_PQ_INPUTSRC_CVBS5_DIS,         ///<7   AV 5
	META_PQ_INPUTSRC_CVBS6_DIS,         ///<8   AV 6
	META_PQ_INPUTSRC_CVBS7_DIS,         ///<9   AV 7
	META_PQ_INPUTSRC_CVBS8_DIS,         ///<10   AV 8
	META_PQ_INPUTSRC_CVBS_DIS_MAX,      ///<11 AV max

	META_PQ_INPUTSRC_SVIDEO_DIS,        ///<12 S-video 1
	META_PQ_INPUTSRC_SVIDEO2_DIS,       ///<13 S-video 2
	META_PQ_INPUTSRC_SVIDEO3_DIS,       ///<14 S-video 3
	META_PQ_INPUTSRC_SVIDEO4_DIS,       ///<15 S-video 4
	META_PQ_INPUTSRC_SVIDEO_DIS_MAX,    ///<16 S-video max

	META_PQ_INPUTSRC_YPBPR_DIS,         ///<17 Component 1
	META_PQ_INPUTSRC_YPBPR2_DIS,        ///<18 Component 2
	META_PQ_INPUTSRC_YPBPR3_DIS,        ///<19 Component 3
	META_PQ_INPUTSRC_YPBPR_DIS_MAX,     ///<20 Component max

	META_PQ_INPUTSRC_SCART_DIS,         ///<21 Scart 1
	META_PQ_INPUTSRC_SCART2_DIS,        ///<22 Scart 2
	META_PQ_INPUTSRC_SCART_DIS_MAX,     ///<23 Scart max

	META_PQ_INPUTSRC_HDMI_DIS,          ///<24 HDMI 1
	META_PQ_INPUTSRC_HDMI2_DIS,         ///<25 HDMI 2
	META_PQ_INPUTSRC_HDMI3_DIS,         ///<26 HDMI 3
	META_PQ_INPUTSRC_HDMI4_DIS,         ///<27 HDMI 4
	META_PQ_INPUTSRC_HDMI_DIS_MAX,      ///<28 HDMI max

	META_PQ_INPUTSRC_DVI_DIS,           ///<29 DVI 1
	META_PQ_INPUTSRC_DVI2_DIS,          ///<30 DVI 2
	META_PQ_INPUTSRC_DVI3_DIS,          ///<31 DVI 2
	META_PQ_INPUTSRC_DVI4_DIS,          ///<32 DVI 4
	META_PQ_INPUTSRC_DVI_DIS_MAX,       ///<33 DVI max

	META_PQ_INPUTSRC_DTV_DIS,           ///<34 DTV
	META_PQ_INPUTSRC_DTV2_DIS,          ///<35 DTV2
	META_PQ_INPUTSRC_DTV_DIS_MAX,       ///<36 DTV max

	// Application source
	META_PQ_INPUTSRC_STORAGE_DIS,       ///<37 Storage
	META_PQ_INPUTSRC_STORAGE2_DIS,      ///<38 Storage2
	META_PQ_INPUTSRC_STORAGE_DIS_MAX,   ///<39 Storage max

	// Support OP capture
	META_PQ_INPUTSRC_SCALER_OP_DIS,     ///<40 scaler OP

	META_PQ_INPUTSRC_DIS_NUM,           ///<number of the source

};

struct meta_pq_framesync_info_dis {
	enum meta_pq_input_source_dis input_source;
	bool bInterlace;
};

struct meta_pq_idr_frame_info_dis {
	__u16 y;
	__u16 height;
	__u16 v_total;
	__u8 vs_pulse_width;
	__u32 fdet_vtt0;
	__u32 fdet_vtt1;
	__u32 fdet_vtt2;
	__u32 fdet_vtt3;
};

struct mtk_video_plane_ctx {
	struct dma_buf *meta_db;
	void           *meta_va;
	struct meta_pq_framesync_info_dis framesyncInfo;
	struct meta_pq_idr_frame_info_dis idrinfo;
	struct disp_mode_info disp_mode_info;
	struct window crop_win;
	struct window disp_win;
	struct window fo_hvsp_in_window;
	struct window fo_hvsp_out_window;
	dma_addr_t base_addr[MTK_PLANE_DISPLAY_PIPE];
	uint32_t frame_buf_width;
	uint32_t frame_buf_height;
	uint16_t post_fill_win_en;
	uint8_t rwdiff;
	uint8_t protectVal;
	uint8_t access_id;
	uint8_t scmifbmode;
	uint8_t mgw_plane_num;
	uint8_t mem_hw_mode;
	uint8_t hw_mode_win_id;
	uint8_t disp_win_type;
	bool mute;
	bool freeze;
	enum drm_mtk_video_plane_type video_plane_type;
	/*memc*/
	enum mtk_video_plane_memc_change memc_change_on;
	bool memc_freeze;
};

struct mtk_video_plane_type_ctx {
	uint8_t disp_mode;
};

struct mtk_video_plane_dv_gd_workqueue {
	struct workqueue_struct *wq;
	struct delayed_work dwq;
	struct dv_gd_info gd_info;
};

struct mtk_video_hw_version {
	u8 mgw;
	u8 video_blending;
	u8 video_mute;
	u8 disp_win;
	u8 irq;
	u8 pattern;
	u8 video_freeze;
};

struct mtk_video_irq {
	int graphic_irq_vgsep_num;
	int video_irq_num;
	int video_irq_affinity;
	int mod_detect_irq_num;
	bool video_irq_status[MTK_VIDEO_IRQEVENT_MAX];
	bool irq_mask;
};

struct mtk_video_context {
	struct drm_property *drm_video_plane_prop[EXT_VPLANE_PROP_MAX];
	struct video_plane_prop *video_plane_properties;
	struct mtk_video_plane_ctx *plane_ctx;
	struct mtk_video_plane_ctx *old_plane_ctx;
	struct mtk_video_plane_type_ctx *plane_type_ctx;
	struct mtk_video_plane_type_ctx *old_plane_type_ctx;
	struct mtk_sm_ml_context disp_ml;
	struct mtk_sm_ml_context disp_irq_ml;
	struct mtk_sm_ml_context vgs_ml;
	struct mtk_video_plane_dv_gd_workqueue dv_gd_wq;
	struct window scl_inputWin[MTK_VPLANE_TYPE_MAX];
	struct window scl_outputWin[MTK_VPLANE_TYPE_MAX];
	struct rbank_info rbank[MAX_WINDOW_NUM];
	struct mtk_video_hw_version video_hw_ver;
	enum drm_mtk_video_plane_type video_plane_type[MAX_WINDOW_NUM];
	uint8_t plane_num[MTK_VPLANE_TYPE_MAX];
	uint8_t videoPlaneType_TypeNum;
	uint16_t reg_num;
	uint8_t zpos_layer[MTK_VPLANE_TYPE_MAX];
	bool mgwdmaEnable[MTK_VPLANE_TYPE_MAX];
	bool extWinEnable[MTK_VPLANE_TYPE_MAX];
	bool preinsertEnable[MTK_VPLANE_TYPE_MAX];
	bool postfillEnable[MTK_VPLANE_TYPE_MAX];
	uint8_t old_layer[MTK_VPLANE_TYPE_MAX];
	uint8_t old_used_layer[MTK_VPLANE_TYPE_MAX];
	bool old_layer_en[MTK_VPLANE_TYPE_MAX];

	uint16_t sw_delay;
};


struct mtk_video_format_info {
	uint32_t fourcc;
	uint64_t modifier;
	uint8_t modifier_arrange;
	uint8_t modifier_compressed;
	uint8_t modifier_6bit;
	uint8_t modifier_10bit;
	uint8_t bpp[3];
};
struct mtk_cfd_set_CSC {
	uint8_t u8Data_Format;    //EN_PQ_CFD_DATAFORMAT
	uint8_t u8Range;         //EN_PQ_CFD_RANGE
	uint8_t u8ColorSpace;    //EN_PQ_CFD_SET_COLORSPACE
};

struct mtk_video_dbg_info {
	struct window *crop_win;
	struct window *disp_win;
	enum drm_mtk_video_plane_type planeType[MAX_WINDOW_NUM];
	uint16_t rwdiff[MAX_WINDOW_NUM];
	uint8_t mute_en[MAX_WINDOW_NUM];
	uint8_t postFillWin_en[MAX_WINDOW_NUM];
};

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
struct mtk_tv_kms_context;

int mtk_video_display_parse_dts(
	struct mtk_tv_kms_context *pctx);
int mtk_video_display_init(
	struct device *dev,
	struct device *master,
	void *data,
	struct mtk_drm_plane **primary_plane,
	struct mtk_drm_plane **cursor_plane);
int mtk_video_display_unbind(
	struct device *dev);
void mtk_video_display_update_plane(
	struct mtk_plane_state *state);
void mtk_video_display_disable_plane(
	struct mtk_plane_state *state);
void mtk_video_display_atomic_crtc_flush(
	struct mtk_video_context *pctx_video);
void mtk_video_display_clear_plane_status(
	struct mtk_plane_state *state);
int mtk_video_display_atomic_set_plane_property(
	struct mtk_drm_plane *mplane,
	struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t val);
int mtk_video_display_atomic_get_plane_property(
	struct mtk_drm_plane *mplane,
	const struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t *val);
int mtk_video_display_check_plane(
	struct drm_plane_state *plane_state,
	const struct drm_crtc_state *crtc_state,
	struct mtk_plane_state *state);
void mtk_video_display_get_rbank_IRQ(
	unsigned int plane_inx,
	struct mtk_video_context *pctx_video);
int mtk_video_display_suspend(
	struct platform_device *pdev,
	pm_message_t state);
int mtk_video_display_resume(
	struct platform_device *pdev);
int mtk_video_display_mute_resume(struct mtk_tv_kms_context *pctx);
void mtk_video_display_create_sysfs(
	struct platform_device *pdev);
void mtk_video_display_remove_sysfs(
	struct platform_device *pdev);

int mtk_video_display_SetCFD_Setting(
	uint32_t pnl_ver,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_input,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_mainout,
	const struct mtk_cfd_set_CSC *pst_cfd_set_csc_deltaout);

struct mtk_cfd_set_CSC *mtk_video_display_GetCFD_RenderInCsc_Setting(void);

struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_tcon_in_csc_setting(void);
struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_rendor_main_out_csc_setting(void);
struct mtk_cfd_set_CSC *mtk_video_display_cfd_get_render_delta_out_csc_setting(void);


int mtk_video_display_Cfd_GetFormat(
	u8 *OutputDataFormat);

int mtk_video_display_SetCFD_RenderinCsc_Setting(
	struct mtk_tv_kms_context *pctx,
	void *buffer_addr);

int mtk_video_display_set_defaultBackgroundColor(
	struct mtk_video_context *pctx_video);

int mtk_video_display_get_blendingColorFormat_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_video_display_set_hse_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

#endif
