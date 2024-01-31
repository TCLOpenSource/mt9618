/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_PANEL_H_
#define _MTK_TV_DRM_PANEL_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#ifndef DRM_NAME_MAX_NUM
#define DRM_NAME_MAX_NUM 256
#endif

#ifndef __KERNEL__
// include video/display_timing.h here ?
typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;
typedef uint32_t u32;
#endif

/* Enum */

typedef enum {
	E_OUTPUT_RGB,
	E_OUTPUT_YUV444,
	E_OUTPUT_YUV422,
	E_OUTPUT_YUV420,
	E_OUTPUT_ARGB8101010,
	E_OUTPUT_ARGB8888_W_DITHER,
	E_OUTPUT_ARGB8888_W_ROUND,
	E_OUTPUT_ARGB8888_MODE0,
	E_OUTPUT_ARGB8888_MODE1,
	E_PNL_OUTPUT_FORMAT_NUM,
} drm_en_pnl_output_format;

typedef enum {
	E_OUTPUT_MODE_NONE,
	E_8K4K_144HZ,
	E_8K4K_120HZ,
	E_8K4K_60HZ,
	E_8K4K_30HZ,
	E_4K2K_120HZ,
	E_4K2K_60HZ,
	E_4K2K_30HZ,
	E_FHD_120HZ,
	E_FHD_60HZ,
	E_4K2K_144HZ,
	E_4K1K_144HZ,
	E_4K1K_240HZ,
	E_4K1K_120HZ,
	E_OUTPUT_MODE_MAX,
} drm_output_mode;

typedef enum {
	E_LINK_NONE,
	E_LINK_VB1,
	E_LINK_LVDS,
	E_LINK_VB1_TO_HDMITX,
	E_LINK_MINILVDS,
	E_LINK_TTL,
	E_LINK_RESERVED6,
	E_LINK_RESERVED7,
	E_LINK_RESERVED8,
	E_LINK_RESERVED9,
	E_LINK_EXT,
	E_LINK_HSLVDS_1CH = E_LINK_EXT,
	E_LINK_HSLVDS_2CH,
	E_LINK_MINILVDS_1BLK_3PAIR_6BIT,
	E_LINK_MINILVDS_1BLK_6PAIR_6BIT,
	E_LINK_MINILVDS_2BLK_3PAIR_6BIT,
	E_LINK_MINILVDS_2BLK_6PAIR_6BIT,
	E_LINK_MINILVDS_1BLK_3PAIR_8BIT,
	E_LINK_MINILVDS_1BLK_6PAIR_8BIT,
	E_LINK_MINILVDS_2BLK_3PAIR_8BIT,
	E_LINK_MINILVDS_2BLK_6PAIR_8BIT,
	E_LINK_EPI28_8BIT_2PAIR_2KCML,
	E_LINK_EPI28_8BIT_4PAIR_2KCML,
	E_LINK_EPI28_8BIT_6PAIR_2KCML,
	E_LINK_EPI28_8BIT_8PAIR_2KCML,
	E_LINK_EPI28_8BIT_6PAIR_4KCML,
	E_LINK_EPI28_8BIT_8PAIR_4KCML,
	E_LINK_EPI28_8BIT_12PAIR_4KCML,
	E_LINK_EPI24_10BIT_12PAIR_4KCML,
	E_LINK_EPI28_8BIT_2PAIR_2KLVDS,
	E_LINK_EPI28_8BIT_4PAIR_2KLVDS,
	E_LINK_EPI28_8BIT_6PAIR_2KLVDS,
	E_LINK_EPI28_8BIT_8PAIR_2KLVDS,
	E_LINK_EPI28_8BIT_6PAIR_4KLVDS,
	E_LINK_EPI28_8BIT_8PAIR_4KLVDS,
	E_LINK_EPI28_8BIT_12PAIR_4KLVDS,
	E_LINK_EPI24_10BIT_12PAIR_4KLVDS,
	E_LINK_CMPI27_8BIT_6PAIR,
	E_LINK_CMPI27_8BIT_8PAIR,
	E_LINK_CMPI27_8BIT_12PAIR,
	E_LINK_CMPI27_10BIT_8PAIR,
	E_LINK_CMPI27_10BIT_12PAIR,
	E_LINK_USIT_8BIT_6PAIR,
	E_LINK_USIT_8BIT_12PAIR,
	E_LINK_USIT_10BIT_6PAIR,
	E_LINK_USIT_10BIT_12PAIR,
	E_LINK_ISP_8BIT_6PAIR,
	E_LINK_ISP_8BIT_6X2PAIR,
	E_LINK_ISP_8BIT_8PAIR,
	E_LINK_ISP_8BIT_12PAIR,
	E_LINK_ISP_10BIT_6X2PAIR,
	E_LINK_ISP_10BIT_8PAIR,
	E_LINK_ISP_10BIT_12PAIR,
	E_LINK_CHPI_8BIT_6PAIR,
	E_LINK_CHPI_8BIT_6X2PAIR,
	E_LINK_CHPI_8BIT_8PAIR,
	E_LINK_CHPI_8BIT_12PAIR,
	E_LINK_CHPI_10BIT_8PAIR,
	E_LINK_CHPI_10BIT_12PAIR,
	E_LINK_CHPI_10BIT_6PAIR,
	E_LINK_LPLL_VBY1_10BIT_8LANE,
	E_LINK_LPLL_VBY1_10BIT_4LANE,
	E_LINK_LPLL_VBY1_10BIT_2LANE,
	E_LINK_LPLL_VBY1_8BIT_8LANE,
	E_LINK_LPLL_VBY1_8BIT_4LANE,
	E_LINK_LPLL_VBY1_8BIT_2LANE,
	E_LINK_EPI28_8BIT_16PAIR_4KCML,
	E_LINK_EPI24_10BIT_16PAIR_4KCML,    //60
	E_LINK_EPI28_8BIT_16PAIR_4KLVDS,
	E_LINK_EPI24_10BIT_16PAIR_4KLVDS,
	E_LINK_USIT_8BIT_8PAIR,
	E_LINK_USIT_8BIT_16PAIR,
	E_LINK_USIT_10BIT_16PAIR,
	E_LINK_MAX,
} drm_pnl_link_if;

/* panel related enum and structure */
typedef enum {
	MTK_DRM_VIDEO_LANTENCY_TYPE_FRAME_LOCK,
	MTK_DRM_VIDEO_LANTENCY_TYPE_MAX,
} drm_en_video_latency_type;

/// Define aspect ratio
typedef enum {
	///< set aspect ratio to 4 : 3
	E_PNL_ASPECT_RATIO_4_3 = 0,
	///< set aspect ratio to 16 : 9
	E_PNL_ASPECT_RATIO_WIDE,
	///< resvered for other aspect ratio other than 4:3/ 16:9
	E_PNL_ASPECT_RATIO_OTHER,
} drm_en_pnl_aspect_ratio; //E_PNL_ASPECT_RATIO;

/// Define TI bit mode
typedef enum {
	TI_10BIT_MODE = 0,
	TI_8BIT_MODE = 2,
	TI_6BIT_MODE = 3,
} drm_en_pnl_tibitmode; //APIPNL_TIBITMODE;

/// Define panel output format bit mode
typedef enum {
	// default is 10bit, because 8bit panel can use 10bit config and
	// 8bit config.
	OUTPUT_10BIT_MODE = 0,
	// but 10bit panel(like PDP panel) can only use 10bit config.
	OUTPUT_6BIT_MODE = 1,
	// and some PDA panel is 6bit.
	OUTPUT_8BIT_MODE = 2,
} drm_en_pnl_outputformat_bitmode; // APIPNL_OUTPUTFORMAT_BITMODE;

/// Define which panel output timing change mode is used to change VFreq
/// for same panel
typedef enum {
	///<change output DClk to change Vfreq.
	E_PNL_CHG_DCLK   = 0,
	///<change H total to change Vfreq.
	E_PNL_CHG_HTOTAL = 1,
	///<change V total to change Vfreq.
	E_PNL_CHG_VTOTAL = 2,
} drm_en_pnl_out_timing_mode; //APIPNL_OUT_TIMING_MODE;

typedef enum {
	E_PNL_MOD_PECURRENT_SETTING,
	E_PNL_CONTROL_OUT_SWING,
	E_PNL_UPDATE_INI_PARA,
	E_PNL_CONTROL_OUT_SWING_CHANNEL,
	E_PNL_MIPI_UPDATE_PNL_TIMING,
	E_PNL_MIPI_DCS_COMMAND,
	E_PNL_SETTING_CMD_NUM,
	E_PNL_SETTING_CMD_MAX = E_PNL_SETTING_CMD_NUM,
} drm_en_pnl_setting;

typedef enum {
	E_VBO_NO_LINK = 0,
	E_VBO_3BYTE_MODE = 3,
	E_VBO_4BYTE_MODE = 4,
	E_VBO_5BYTE_MODE = 5,
	E_VBO_MODE_MAX,
} drm_en_vbo_bytemode;

typedef enum {
	E_PNL_MIRROR_NONE = 0,
	E_PNL_MIRROR_V,
	E_PNL_MIRROR_H,
	E_PNL_MIRROR_V_H,
	E_PNL_MIRROR_TYPE_MAX
} drm_en_pnl_mirror_type;

/* Structure */

#ifndef __KERNEL__
// include video/display_timing.h here ?
enum display_flags {
	DISPLAY_FLAGS_HSYNC_LOW         = 1 << 0,
	DISPLAY_FLAGS_HSYNC_HIGH        = 1 << 1,
	DISPLAY_FLAGS_VSYNC_LOW         = 1 << 2,
	DISPLAY_FLAGS_VSYNC_HIGH        = 1 << 3,

	/* data enable flag */
	DISPLAY_FLAGS_DE_LOW            = 1 << 4,
	DISPLAY_FLAGS_DE_HIGH           = 1 << 5,
	/* drive data on pos. edge */
	DISPLAY_FLAGS_PIXDATA_POSEDGE   = 1 << 6,
	/* drive data on neg. edge */
	DISPLAY_FLAGS_PIXDATA_NEGEDGE   = 1 << 7,
	DISPLAY_FLAGS_INTERLACED        = 1 << 8,
	DISPLAY_FLAGS_DOUBLESCAN        = 1 << 9,
	DISPLAY_FLAGS_DOUBLECLK         = 1 << 10,
	/* drive sync on pos. edge */
	DISPLAY_FLAGS_SYNC_POSEDGE      = 1 << 11,
	/* drive sync on neg. edge */
	DISPLAY_FLAGS_SYNC_NEGEDGE      = 1 << 12,
};

struct timing_entry {
	u32 min;
	u32 typ;
	u32 max;
};

struct display_timing {
	struct timing_entry pixelclock;

	struct timing_entry hactive;            /* hor. active video */
	struct timing_entry hfront_porch;       /* hor. front porch */
	struct timing_entry hback_porch;        /* hor. back porch */
	struct timing_entry hsync_len;          /* hor. sync len */

	struct timing_entry vactive;            /* ver. active video */
	struct timing_entry vfront_porch;       /* ver. front porch */
	struct timing_entry vback_porch;        /* ver. back porch */
	struct timing_entry vsync_len;          /* ver. sync len */

	enum display_flags flags;               /* display flags */
};

struct list_head {
	struct list_head *next, *prev;
};

#endif

typedef struct {
	drm_pnl_link_if linkIF;
	__u8 lanes;
	__u8 div_sec;
	drm_output_mode timing;
	drm_en_pnl_output_format format;
} drm_mod_cfg;

/// Define the set timing information
typedef struct {
	///<high accurate input V frequency
	__u32 u32HighAccurateInputVFreq;
	///<input V frequency
	__u16 u16InputVFreq;
	///<input vertical total
	__u16 u16InputVTotal;
	///<MVOP source
	bool bMVOPSrc;
	///<whether it's fast frame lock case
	bool bFastFrameLock;
	///<whether it's interlace
	bool bInterlace;
} drm_st_mtk_tv_timing_info;

typedef struct {
	///<Version of current structure.
	///Please always set to "MIRROR_INFO_VERSION" as input
	__u32 u32Version;
	///<Length of this structure,
	///u32MirrorInfo_Length=sizeof(ST_APIPNL_MIRRORINFO)
	__u32 u32Length;
	drm_en_pnl_mirror_type eMirrorType;   ///< Mirror type
	bool bEnable;
} drm_st_pnl_mirror_info;

typedef struct {
	bool bEnable;  //Enable output pattern
	__u16 u16Red;  //Red channel value
	__u16 u16Green;  //Green channel value
	__u16 u16Blue;     //Blue channel value
} drm_st_pnl_output_pattern;

typedef struct {
	//< Version of current structure. Please always set to
	//< "ST_XC_VIDEO_LATENCY_INFO" as input
	__u32 u32Version;
	//< Length of this structure, u16Length=sizeof(ST_XC_VIDEO_LATENCY_INFO)
	__u16 u16Length;

	// latency type
	drm_en_video_latency_type enType;
	// frame lock or not
	bool bIsFrameLock;
	// leading or lagging
	bool bIsLeading;
	// video latency (ns)
	__u32  u32VideoLatency;
	// input frame rate x 10 (ex. 24FPS = 240)
	__u16  u16InputFrameRate;
} drm_st_mtk_tv_video_latency_info;

typedef struct {
	__u16 version;
	__u16 length;
	char pnlname[DRM_NAME_MAX_NUM];
	drm_pnl_link_if linkIF;
	drm_en_vbo_bytemode vbo_byte;
	__u16 div_section;
	/*
	 * Board related setting
	 *
	 * <  A/B channel swap
	 * bool swap_port;
	 * <  PANEL_SWAP_ODD_ML
	 * bool swapodd_ml;
	 * <  PANEL_SWAP_EVEN_ML
	 * bool swapeven_ml;
	 * <  PANEL_SWAP_ODD_RB
	 * bool swapodd_rb;
	 * <  PANEL_SWAP_EVEN_RB
	 * bool swapeven_rb;
	 * <  PANEL_SWAP_LVDS_POL, for differential P/N swap
	 * bool swaplvds_pol;
	 * <  PANEL_SWAP_LVDS_CH, for pair swap
	 * bool swaplvds_ch;
	 * <  PANEL_PDP_10BIT ,for pair swap
	 * bool pdp_10bit;
	 * <  PANEL_LVDS_TI_MODE
	 * bool lvds_ti_mode;
	 */

	/*
	 * panel on/off timing
	 */
	/* <  time between panel & data while turn on power */
	__u16 ontiming_1;
	/* <  time between data & back light while turn on power */
	__u16 ontiming_2;
	/* <  time between back light & data while turn off power */
	__u16 offtiming_1;
	/* <  time between data & panel while turn off power */
	__u16 offtiming_2;

	/*
	 * panel timing spec. sync relate
	 */
	__u16 hsync_st;
	__u8 hsync_w;
	__u8 hsync_pol;
	__u16 vsync_st;
	__u8 vsync_w;
	__u8 vsync_pol;

	/*
	 * DE related
	 */
	__u16 de_hstart;
	__u16 de_vstart;
	__u16 de_width;
	__u16 de_height;

	/*
	 * DClk related
	 */
	__u16 max_htt;
	__u16 typ_htt;
	__u16 min_htt;
	__u16 max_vtt;
	__u16 typ_vtt;
	__u16 min_vtt;
	__u16 max_vtt_panelprotect;
	__u16 min_vtt_panelprotect;
	__u64 max_dclk;
	__u64 typ_dclk;
	__u64 min_dclk;

	drm_en_pnl_output_format op_format;
	drm_en_pnl_output_format cop_format;
	/*
	 *
	 * Board related params
	 *
	 *  If a board ( like BD_MST064C_D01A_S ) swap LVDS TX polarity
	 *    : This polarity swap value =
	 *      (LVDS_PN_SWAP_H<<8) | LVDS_PN_SWAP_L from board define,
	 *  Otherwise
	 *    : The value shall set to 0.
	 */
	/*
	 * __u16 lvds_txswap;
	 * bool swapodd_rg;
	 * bool swapeven_rg;
	 * bool swapodd_gb;
	 * bool swapeven_gb;
	 * bool noise_dith;
	 */

	/*
	 * Others
	 */
	/* HDMI2.1 VRR */
	bool vrr_en;
	__u16 vrr_max_v;
	__u16 vrr_min_v;

	__u32 graphic_mixer1_out_prealpha;
	__u32 graphic_mixer2_out_prealpha;

	struct display_timing displayTiming;
	struct list_head list;

	__u16 op_color_range;
	__u16 op_color_space;

	bool hpc_mode_en;
	__u16 game_direct_fr_group;

	bool dlg_on;
	bool default_sel;
} drm_st_pnl_info;

typedef struct {
	__u16 u16PnlHstart;	//get panel horizontal start
	__u16 u16PnlVstart;	//get panel vertical start
	__u16 u16PnlWidth;	//get panel width
	__u16 u16PnlHeight;	//get panel height
	__u16 u16PnlHtotal;	//get panel horizontal total
	__u16 u16PnlVtotal;	//get panel vertical total
	__u16 u16PnlHsyncWitth;	//get panel horizontal sync width
	__u16 u16PnlHsyncBackPorch;	//get panel horizontal sync back porch
	__u16 u16PnlVsyncBackPorch;	//get panel vertical sync back porch
	__u16 u16PnlDefVfreq;	//get default vfreq
	__u16 u16PnlLPLLMode;	//get panel LPLL mode
	__u16 u16PnlLPLLType;	//get panel LPLL type
	__u16 u16PnlMinSET;	//get panel minima SET value
	__u16 u16PnlMaxSET;	//get panel maxma SET value
} drm_pnl_info;

typedef struct  {
	uint8_t hsync_w;
	uint16_t de_hstart;
	uint8_t lane_numbers;
	drm_en_pnl_output_format op_format;
} st_hprotect_info;

typedef struct {
	drm_st_pnl_info src_info;
	drm_mod_cfg out_cfg;
	bool v_path_en;
	bool extdev_path_en;
	bool gfx_path_en;
	__u16 output_model_ref;
	st_hprotect_info video_hprotect_info;
	st_hprotect_info delta_hprotect_info;
	st_hprotect_info gfx_hprotect_info;
} drm_parse_dts_info;

#endif
