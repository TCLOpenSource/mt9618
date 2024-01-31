/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_VIDEO_H_
#define _MTK_TV_DRM_VIDEO_H_

//#include "sti_msos.h"
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_video_panel.h"
#include <drm/mtk_tv_drm_panel.h>



/* define*/
#define HWREG_VIDEO_REG_NUM_BG_RGB (5)
#define HWREG_VIDEO_REG_NUM_WINDOW_RGB (5)
#define HWREG_VIDEO_REG_NUM_FACTORY_MODE (5)


//Board ini related
#define MTK_DRM_DTS_BOARD_INFO_NODE "board-info"
//MMAP related
#define MTK_DRM_DTS_MMAP_INFO_NODE "mmap-info"

typedef enum {
	///< TTL  type
	LINK_TTL,
	///< LVDS type
	LINK_LVDS,
	///< RSDS type
	LINK_RSDS,
	///< TCON
	LINK_MINILVDS,
	///< Analog TCON
	LINK_ANALOG_MINILVDS,
	///< Digital TCON
	LINK_DIGITAL_MINILVDS,
	///< Ursa (TTL output to Ursa)
	LINK_MFC,
	///< DAC output
	LINK_DAC_I,
	///< DAC output
	LINK_DAC_P,
	///< For PDP(Vsync use Manually MODE)
	LINK_PDPLVDS,
	/// EXT LPLL TYPE
	LINK_EXT,
	/// 10
	LINK_EPI34_8P = LINK_EXT,
	/// 11
	LINK_EPI28_8P,
	/// 12
	LINK_EPI34_6P,
	/// 13
	LINK_EPI28_6P,

	/// replace this with LINK_MINILVDS
	///LINK_MINILVDS_6P_2L,
	/// 14
	LINK_MINILVDS_5P_2L,
	/// 15
	LINK_MINILVDS_4P_2L,
	/// 16
	LINK_MINILVDS_3P_2L,
	/// 17
	LINK_MINILVDS_6P_1L,
	/// 18
	LINK_MINILVDS_5P_1L,
	/// 19
	LINK_MINILVDS_4P_1L,
	/// 20
	LINK_MINILVDS_3P_1L,

	/// 21
	LINK_HS_LVDS,
	/// 22
	LINK_HF_LVDS,

	/// 23
	LINK_TTL_TCON,
	//  2 channel, 3 pair, 8 bits ///24
	LINK_MINILVDS_2CH_3P_8BIT,
	//  2 channel, 4 pair, 8 bits ///25
	LINK_MINILVDS_2CH_4P_8BIT,
	// 2 channel, 5 pair, 8 bits ///26
	LINK_MINILVDS_2CH_5P_8BIT,
	// 2 channel, 6 pair, 8 bits ///27
	LINK_MINILVDS_2CH_6P_8BIT,

	// 1 channel, 3 pair, 8 bits ///28
	LINK_MINILVDS_1CH_3P_8BIT,
	// 1 channel, 4 pair, 8 bits ///29
	LINK_MINILVDS_1CH_4P_8BIT,
	// 1 channel, 5 pair, 8 bits ///30
	LINK_MINILVDS_1CH_5P_8BIT,
	// 1 channel, 6 pair, 8 bits ///31
	LINK_MINILVDS_1CH_6P_8BIT,

	// 2 channel, 3 pari, 6 bits ///32
	LINK_MINILVDS_2CH_3P_6BIT,
	// 2 channel, 4 pari, 6 bits ///33
	LINK_MINILVDS_2CH_4P_6BIT,
	// 2 channel, 5 pari, 6 bits ///34
	LINK_MINILVDS_2CH_5P_6BIT,
	//  2 channel, 6 pari, 6 bits ///35
	LINK_MINILVDS_2CH_6P_6BIT,

	// 1 channel, 3 pair, 6 bits ///36
	LINK_MINILVDS_1CH_3P_6BIT,
	// 1 channel, 4 pair, 6 bits ///37
	LINK_MINILVDS_1CH_4P_6BIT,
	// 1 channel, 5 pair, 6 bits ///38
	LINK_MINILVDS_1CH_5P_6BIT,
	// 1 channel, 6 pair, 6 bits ///39
	LINK_MINILVDS_1CH_6P_6BIT,
	//   HDMI Bypass Mode///40
	LINK_HDMI_BYPASS_MODE,

	/// 41
	LINK_EPI34_2P,
	/// 42
	LINK_EPI34_4P,
	/// 43
	LINK_EPI28_2P,
	/// 44
	LINK_EPI28_4P,

	///45
	LINK_VBY1_10BIT_4LANE,
	///46
	LINK_VBY1_10BIT_2LANE,
	///47
	LINK_VBY1_10BIT_1LANE,
	///48
	LINK_VBY1_8BIT_4LANE,
	///49
	LINK_VBY1_8BIT_2LANE,
	///50
	LINK_VBY1_8BIT_1LANE,

	///51
	LINK_VBY1_10BIT_8LANE,
	///52
	LINK_VBY1_8BIT_8LANE,

	///53
	LINK_EPI28_12P,

	//54
	LINK_HS_LVDS_2CH_BYPASS_MODE,
	//55
	LINK_VBY1_8BIT_4LANE_BYPASS_MODE,
	//56
	LINK_VBY1_10BIT_4LANE_BYPASS_MODE,
	///57
	LINK_EPI24_12P,
	///58
	LINK_VBY1_10BIT_16LANE,
	///59
	LINK_VBY1_8BIT_16LANE,
	///60
	LINK_USI_T_8BIT_12P,
	///61
	LINK_USI_T_10BIT_12P,
	///62
	LINK_ISP_8BIT_12P,
	///63
	LINK_ISP_8BIT_6P_D,

	///64
	LINK_ISP_8BIT_8P,
	///65
	LINK_ISP_10BIT_12P,
	///66
	LINK_ISP_10BIT_6P_D,
	///67
	LINK_ISP_10BIT_8P,
	///68
	LINK_EPI24_16P,
	///69
	LINK_EPI28_16P,
	///70
	LINK_EPI28_6P_EPI3G,
	///71
	LINK_EPI28_8P_EPI3G,

	///72
	LINK_CMPI24_10BIT_12P,
	///73
	LINK_CMPI27_8BIT_12P,
	///74
	LINK_CMPI27_8BIT_8P,
	///75
	LINK_CMPI24_10BIT_8P,
	///76
	LINK_USII_8BIT_12X1_4K,
	///77
	LINK_USII_8BIT_6X1_D_4K,
	///78
	LINK_USII_8BIT_8X1_4K,
	///79
	LINK_USII_8BIT_6X1_4K,
	///80
	LINK_USII_8BIT_6X2_4K,
	///81
	LINK_USII_10BIT_12X1_4K,
	///82
	LINK_USII_10BIT_8X1_4K,

	///83
	LINK_EPI24_8P,
	///84
	LINK_USI_T_8BIT_8P,
	///85
	LINK_USI_T_10BIT_8P,
	///86
	LINK_MINILVDS_4CH_6P_8BIT,
	///87
	LINK_EPI28_12P_EPI3G,
	///88
	LINK_EPI28_16P_EPI3G,

	///89
	LINK_MIPI_DSI_RGB888_4lane,
	///90
	LINK_MIPI_DSI_RGB565_4lane,
	///91
	LINK_MIPI_DSI_RGB666_4lane,
	///92
	LINK_MIPI_DSI_RGB888_3lane,
	///93
	LINK_MIPI_DSI_RGB565_3lane,
	///94
	LINK_MIPI_DSI_RGB666_3lane,
	///95
	LINK_MIPI_DSI_RGB888_2lane,
	///96
	LINK_MIPI_DSI_RGB565_2lane,
	///97
	LINK_MIPI_DSI_RGB666_2lane,
	///98
	LINK_MIPI_DSI_RGB888_1lane,
	///99
	LINK_MIPI_DSI_RGB565_1lane,
	///100
	LINK_MIPI_DSI_RGB666_1lane,

	///101
	LINK_NUM,
} drm_en_pnl_link_type; //APIPNL_LINK_TYPE;

typedef enum {
	/// BIN
	E_PNL_FILE_TCON_BIN_PATH,
	E_PNL_FILE_PANELGAMMA_BIN_PATH,
	E_PNL_FILE_MIPI_DCS_BIN_PATH,
	/// The max support number of paths
	E_PNL_FILE_PATH_MAX,
} drm_en_pnl_file_path;

typedef enum {
	E_APIPNL_TCON_TAB_TYPE_GENERAL,
	E_APIPNL_TCON_TAB_TYPE_GPIO,
	E_APIPNL_TCON_TAB_TYPE_SCALER,
	E_APIPNL_TCON_TAB_TYPE_MOD,
	E_APIPNL_TCON_TAB_TYPE_GAMMA,
	E_APIPNL_TCON_TAB_TYPE_POWER_SEQUENCE_ON,
	E_APIPNL_TCON_TAB_TYPE_POWER_SEQUENCE_OFF,
	E_APIPNL_TCON_TAB_TYPE_PANEL_INFO,
	E_APIPNL_TCON_TAB_TYPE_OVERDRIVER,
	E_APIPNL_TCON_TAB_TYPE_PCID,
	E_APIPNL_TCON_TAB_TYPE_PATCH,
	E_APIPNL_TCON_TAB_TYPE_LINE_OD_TABLE,
	E_APIPNL_TCON_TAB_TYPE_LINE_OD_REG,
	E_APIPNL_TCON_TAB_TYPE_EVA_REG,
} drm_en_pnl_tcon_tab_type;


/// Define Panel MISC control index
/// please enum use BIT0 = 0x01, BIT1 = 0x02, BIT2 = 0x04, BIT3 = 0x08,
/// BIT4 = 0x10,
typedef enum {
	E_APIPNL_MISC_CTRL_OFF   = 0x0000,
	E_APIPNL_MISC_MFC_ENABLE = 0x0001,
	E_APIPNL_MISC_SKIP_CALIBRATION = 0x0002,
	E_APIPNL_MISC_GET_OUTPUT_CONFIG = 0x0004,
	E_APIPNL_MISC_SKIP_ICONVALUE = 0x0008,

	// bit 4
	E_APIPNL_MISC_MFC_MCP    = 0x0010,
	// bit 5
	E_APIPNL_MISC_MFC_ABChannel = 0x0020,
	// bit 6
	E_APIPNL_MISC_MFC_ACChannel = 0x0040,
	// bit 7, for 60Hz Panel
	E_APIPNL_MISC_MFC_ENABLE_60HZ = 0x0080,
	// bit 8, for 240Hz Panel
	E_APIPNL_MISC_MFC_ENABLE_240HZ = 0x0100,
	// bit 9, for 4k2K 60Hz Panel
	E_APIPNL_MISC_4K2K_ENABLE_60HZ = 0x0200,
	// bit 10, for T3D control
	E_APIPNL_MISC_SKIP_T3D_CONTROL = 0x0400,
	// bit 11, enable pixel shift
	E_APIPNL_MISC_PIXELSHIFT_ENABLE = 0x0800,
	// enable manual V sync control
	E_APIPNL_MISC_ENABLE_MANUAL_VSYNC_CTRL = 0x8000,
} drm_en_pnl_misc;
//----------------------------------------------------------------------------
//MApi_PNL_Setting enum of cmd
//----------------------------------------------------------------------------
typedef enum {
	E_PNL_OUT_MODEL_VG_BLENDED,
	E_PNL_OUT_MODEL_VG_SEPARATED,
	E_PNL_OUT_MODEL_VG_SEPARATED_W_EXTDEV,
	E_PNL_OUT_MODEL_ONLY_EXTDEV,
	E_PNL_OUT_MODEL_MAX,
} drm_en_pnl_output_model;

typedef struct {
	__u32 u32OutputCFG0_7;  //setup output config channel 00~07
	__u32 u32OutputCFG8_15;  //setup output config channel 08~15
	__u32 u32OutputCFG16_21;  //setup output config channel 16~21
} drm_st_mod_output_config_user;

typedef struct {
	/// Structure version
	__u32 u32Version;
	///<  PANEL_MAX_DCLK. Reserved for future using.
	__u32 u32PanelMaxDCLK;
	///<  LPLL_0F[23:0], PANEL_DCLK ,{0x3100_10[7:0], 0x3100_0F[15:0]}
	__u32 u32PanelDCLK;
	///<  PANEL_MIN_DCLK. Reserved for future using.
	__u32 u32PanelMinDCLK;
} drm_st_pnl_init_para_setting;

//OD setting for get info from board ini
typedef struct {
	__u8 u8ODEnable;
	__u32 u32OD_MSB_Addr;
	__u8 u32OD_MSB_Size;
} drm_st_pnl_od_control;

typedef struct DLL_PACKED {
	/// Structure version
	__u32 u32Version;
	///<  PANEL_MAX_DCLK. Reserved for future using.
	__u32 u32PanelMaxDCLK;
	///<  LPLL_0F[23:0], PANEL_DCLK ,{0x3100_10[7:0], 0x3100_0F[15:0]}
	__u32 u32PanelDCLK;
	///<  PANEL_MIN_DCLK. Reserved for future using.
	__u32 u32PanelMinDCLK;
} drm_st_pnl_ini_para;

/*End of panel related enum and structure*/

int mtk_video_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val);

int mtk_video_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val);

int mtk_video_atomic_set_connector_property(
	struct mtk_tv_drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t val);

int mtk_video_atomic_get_connector_property(
	struct mtk_tv_drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t *val);

void mtk_video_update_crtc(
	struct mtk_tv_drm_crtc *crtc);

drm_en_pnl_output_model mtk_tv_drm_set_output_model(
	bool v_path_en,
	bool extdev_path_en,
	bool gfx_path_en);

void set_panel_context(
	struct mtk_tv_kms_context *pctx);

int mtk_tv_drm_video_get_delay_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_get_fb_num_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_set_frc_mode_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_set_extra_video_latency_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_set_live_tone_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv);

int mtk_tv_drm_video_get_expect_pq_out_size_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_get_pxm_report_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_set_factory_mode_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_get_frc_algo_version_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

#endif
