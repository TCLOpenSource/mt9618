/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_VIDEO_PANEL_H_
#define _MTK_TV_DRM_VIDEO_PANEL_H_

#include "mtk_tv_drm_panel_common.h"
#include "hwreg_render_common_trigger_gen.h"
#include "mtk_mod.h"
#include "mtk_tv_drm_oled.h"

#include "../mtk_tv_drm_plane.h"
#include "../mtk_tv_drm_drv.h"
#include "../mtk_tv_drm_crtc.h"
#include "../mtk_tv_drm_connector.h"
#include "../mtk_tv_drm_encoder.h"
#include <linux/gpio/consumer.h>
#include "uapi/mtk_tv_drm_panel.h"
#include "mtk_tv_drm_backlight.h"
#include <linux/jump_label.h>
//-------------------------------------------------------------------------------------------------
// Define & Macro
//-------------------------------------------------------------------------------------------------

#define FILE_PATH_LEN (50)
#define PNL_FRC_TABLE_MAX_INDEX (20)
#define PNL_FRC_TABLE_ARGS_NUM (4)
#define MTK_GAMMA_LUT_SIZE	(1024)
#define PNL_INFO_VERSION             (1)
#define PNL_CUS_SETTING_VERSION      (1)
#define PNL_SWING_CONTROL_VERSION    (1)
#define PNL_PE_CONTROL_VERSION       (1)
#define PNL_LANE_ORDER_VERSION       (1)
#define PNL_OUTPUT_CONFIG_VERSION    (1)
#define PNL_COLOR_INFO_VERSION       (1)

//SSC limitation
#define SSC_MODULATION_LIMIT (100)	//10%
#define SSC_DEVIATION_LIMIT (1000)	//1000kHz

#define SSC_MODULATION_LIMIT_VBY1  (30)  //30KHz
#define SSC_DEVIATION_LIMIT_VBY1 (50)  //0.5%
#define SSC_MODULATION_LIMIT_LVDS  (50)  //50KHz
#define SSC_DEVIATION_LIMIT_LVDS (300)  //3%
#define SSC_MODULATION_LIMIT_EPI15  (100)  //100KHz
#define SSC_DEVIATION_LIMIT_EPI15 (300)  //3%
#define SSC_MODULATION_LIMIT_EPI30  (100)  //100KHz
#define SSC_DEVIATION_LIMIT_EPI30 (100)  //1%
#define SSC_MODULATION_LIMIT_CEDS1  (100)  //100KHz
#define SSC_DEVIATION_LIMIT_CEDS1 (100)  //1%
#define SSC_MODULATION_LIMIT_CEDS2  (50)  //50KHz
#define SSC_DEVIATION_LIMIT_CEDS2 (200)  //2%
#define SSC_MODULATION_LIMIT_CMPI20  (100)  //100KHz
#define SSC_DEVIATION_LIMIT_CMPI20 (200)  //2%
#define SSC_MODULATION_LIMIT_USIT16  (33)  //33KHz
#define SSC_DEVIATION_LIMIT_USIT16 (150)  //1.5%
#define SSC_MODULATION_LIMIT_USIT31  (33)  //33KHz
#define SSC_DEVIATION_LIMIT_USIT31 (50)  //0.5%
#define SSC_MODULATION_LIMIT_CSPI18  (33)  //33KHz
#define SSC_DEVIATION_LIMIT_CSPI18 (50)  //0.5%
#define SSC_MODULATION_LIMIT_ISP18  (90)  //90KHz
#define SSC_DEVIATION_LIMIT_ISP18 (150)  //1.5%
#define SSC_MODULATION_LIMIT_PDI  (33)  //33KHz
#define SSC_DEVIATION_LIMIT_PDI (250)  //2.5%
#define SSC_MODULATION_LIMIT_CHPI  (100)  //100KHz
#define SSC_DEVIATION_LIMIT_CHPI (250)  //2.5%
#define SSC_MODULATION_LIMIT_MINILVDS  (50)  //50KHz
#define SSC_DEVIATION_LIMIT_MINILVDS (300)  //3%





// ST_PNL_INI_PARA version
#define ST_PNL_PARA_VERSION                              (1)

//Panel ini related
//#define PNL_COMPATIBLE_STR        "mediatek-panel"
#define PNL_CUS_SETTING_NODE      "panel-cus-setting"
#define PNL_OUTPUT_SWING_INFO     "output_swing_info"
#define PNL_OUTPUT_PE_NODE        "panel-output-pre-emphasis"
#define PNL_SSC_INFO              "spread_spectrum_info"
#define PNL_LANE_ORDER_NODE       "panel-lane-order"
#define PNL_PKG_LANE_ORDER_NODE   "pkg_lane_order"
#define PNL_OUTPUT_PRE_EMPHASIS   "output_pre_emphasis"
#define PNL_USR_LANE_ORDER_NODE   "usr_lane_order"
#define PNL_OUTPUT_CONFIG_NODE    "panel-output-config"
#define PNL_COLOR_INFO_NODE       "panel-color-info"
#define PNL_FRC_TABLE_NODE        "panel-frc-table-info"
#define PNL_HW_INFO_NODE          "hw-info"
#define PNL_COMBO_INFO_NODE       "ext_graph_combo_info"
#define PNL_DITHER_INFO_NODE      "panel-dither-info"
#define PNL_TCONLESS_INFO_NODE    "tconless_model_info"
#define PNL_TCON_I2C_INFO_NODE    "panel_tcon_i2c_info"
#define PNL_TCON_I2C_SEC_INFO_NODE    "panel_tcon_i2c_sec_info"
#define PNL_TCON_VRR_OD_INFO_NODE "panel_tcon_vrr_od_info"
#define PNL_TCON_VRR_PGA_INFO_NODE "panel_tcon_vrr_pga_info"
#define PNL_OLED_INFO_NODE         "oled_info"
#define PNL_CUST_TCON_INFO_NODE    "tcon_info"
#define PNL_TCON_VRR_HFR_PMIC_I2C_CTRL_NODE	"vrr_hfr_pmic_i2c_ctrl"
#define PNL_TGEN_INFO_NODE        "panel_tgen_info"

/***********
 *MOD setting
 ************/
//define output mode
#define MOD_IN_IF_VIDEO (4)	//means 4p engine input
#define LVDS_MPLL_CLOCK_MHZ	(864)	// For crystal 24Mhz

/***********
 *REG setting
 ************/
#define REG_MAX_INDEX (50)

/***********
 *FRAMESYNC setting
 ************/
#define FRAMESYNC_VTTV_NORMAL_PHASE_DIFF (500)
#define FRAMESYNC_VTTV_SHORTEST_LOW_LATENCY_PHASE_DIFF (55)
#define FRAMESYNC_VTTV_LOW_LATENCY_PHASE_DIFF (100)
#define FRAMESYNC_VTTV_PHASE_DIFF_DIV_1000 (1000)
#define FRAMESYNC_VTTV_FRAME_COUNT (1)
#define FRAMESYNC_VTTV_THRESHOLD (3)
#define FRAMESYNC_VTTV_DIFF_LIMIT (0x7fff)
#define FRAMESYNC_VTTV_LOCKED_LIMIT (9)
#define FRAMESYNC_VTTV_LOCKKEEPSEQUENCETH (3)
#define FRAMESYNC_VTTV_LOCK_VTT_UPPER (0xFFFF)
#define FRAMESYNC_VTTV_LOCK_VTT_LOWER (0)
#define FRAMESYNC_VRR_LOCK_VTT_UPPER (0xFFFF)
#define FRAMESYNC_VRR_LOCK_VTT_LOWER (0)
#define FRAMESYNC_VRR_DEPOSIT_COUNT (1)
#define FRAMESYNC_VRR_STATE_SW_MODE (4)
#define FRAMESYNC_VRR_CHASE_TOTAL_DIFF_RANGE (2)
#define FRAMESYNC_VRR_CHASE_PHASE_TARGET (2)
#define FRAMESYNC_VRR_CHASE_PHASE_ALMOST (2)
#define FRAMESYNC_RECOV_LOCKKEEPSEQUENCETH (3)
#define FRAMESYNC_RECOV_DIFF_LIMIT (0x7fff)

#define UHD_8K_W   (7680)
#define UHD_8K_H   (4320)
#define UHD_4K_W   (3840)
#define UHD_4K_H   (2160)
#define FHD_2K_W   (1920)
#define FHD_2K_H   (1080)

/*
 *###################################
 *#  Panel backlight related params.#
 *###################################
 */

#define PERIOD_PWM_TAG         "u32PeriodPWM"
#define DUTY_PWM_TAG           "u32DutyPWM"
#define DIV_PWM_TAG            "u32DivPWM"
#define SHIFT_PWM_TAG          "u32ShiftPWM"
#define POL_PWM_TAG            "bPolPWM"
#define MAX_PWM_VALUE_TAG      "u32MaxPWMvalue"
#define TYP_PWM_VAL_TAG        "u16TypPWMvalue"
#define MIN_PWM_VALUE_TAG      "u32MinPWMvalue"
#define PWM_PORT_TAG           "u16PWMPort"
#define PWM_PORT_MAX_NUM       (16)
#define PWM_VSYNC_ENABLE_TAG   "bBakclightFreq2Vfreq"
#define PERIOD_MULTI_TAG       "u32PwmPeriodMulti"


/*
 *###########################
 *#    panel-cus-setting    #
 *###########################
 */
#define DTSI_FILE_TYPE      "DTSIFileType"
#define DTSI_FILE_VER     "DTSIFileVer"
#define GAMMA_EN_TAG      "PanelGamma_Enable"
#define GAMMA_BIN_TAG     "PanelGamme_BinPath"
#define TCON_EN_TAG       "TCON_Enable"
#define TCON_BIN_TAG      "TCON_BinPath"
#define MIRROR_TAG        "PanelMirrorMode"
#define OUTPUT_FORMAT_TAG "PanelOutputFormat"
#define FS_MODE_TAG       "Framesync_Mode"
#define OD_EN_TAG         "OverDrive_Enable"
#define OD_ADDR_TAG       "OverDrive_Buf_Addr"
#define OD_BUF_SIZE_TAG   "OverDrive_Buf_Size"
#define VCC_BL_CUSCTRL_TAG "VCC_BL_CusCtrl"
#define VCC_OFFON_DELAY_TAG "VCC_OffOn_Delay"
#define SCDISP_PATH_SEL_TAG         "SCDISP_PATH_SEL"
#define FORCE_FREE_RUN_EN_TAG	"Force_FreeRun_Enable"
#define GPIO_BL_TAG       "GPIO_Backlight"
#define GPIO_VCC_TAG      "GPIO_VCC"
#define M_DELTA_TAG       "M_delta"
#define COLOR_PRIMA_TAG   "CustomerColorPrimaries"
#define SOURCE_WX_TAG     "SourceWx"
#define SOURCE_WY_TAG     "SourceWy"
#define VG_DISP_TAG       "VIDEO_GFX_DISPLAY_MODE"
#define OSD_HEIGHT_TAG    "OSD_Height"
#define OSD_WIDTH_TAG     "OSD_Width"
#define OSD_HS_START_TAG  "OSD_HsyncStart"
#define OSD_HS_END_TAG    "OSD_HsyncEnd"
#define OSD_HDE_START_TAG "OSD_HDEStart"
#define OSD_HDE_END_TAG   "OSD_HDEEnd"
#define OSD_HTOTAL_TAG    "OSD_HTotal"
#define OSD_VS_START_TAG  "OSD_VsyncStart"
#define OSD_VS_END_TAG    "OSD_VsyncEnd"
#define OSD_VDE_START_TAG "OSD_VDEStart"
#define OSD_VDE_END_TAG   "OSD_VDEEnd"
#define OSD_VTOTAL_TAG    "OSD_VTotal"
#define GLOBAL_MUTE_BACKLIGHT_IGNORE_TAG "Global_Mute_Backlight_Ignore"
#define GLOBAL_MUTE_DELAY_TAG "Global_Mute_Delay"
#define GLOBAL_UNMUTE_DELAY_TAG "Global_Unmute_Delay"
#define PANEL_NAME_TAG    "Panel_Name"
#define PWM_TO_BL_DELAY_TAG   "PWM_to_BL_delay"
#define BL_TO_PWM_DELAY_TAG   "BL_to_PWM_delay"
#define PMIC_SEQ_CUSCTRL_TAG   "PMIC_Seq_CusCtrl"
#define PWR_ON_TO_PMIC_DLY_TAG   "PowerOn_to_PMIC_delay"

/*
 *#################################
 *#    panel cus setting(tcon)    #
 *#################################
 */
#define OVERDIRVE_ENABLE_TAG    "OverDrive_Enable"
#define PANEL_INDEX_TAG         "PanelIndex"
#define PANEL_ID_STRING_TAG     "PanelIDString"
#define INCH_SIZE_TAG           "InchSize"
#define CHASSIS_INDEX_TAG       "ChassisIndex"
#define VCOM_PATTERN_TAG        "VCOM_Pattern"
#define VCOM_TYPE_TAG           "VCOM_Type"
#define SPREAD_PERMILLAGE_TAG   "SpreadPermillage"
#define SPREAD_FREQ_TAG         "SpreadFreq"
#define OCELL_DEMURA_IDX_TAG    "Ocell_Demura_IDX"
#define TCON_INDEX_TAG          "Tcon_Index"
#define CURRENT_MAX_TAG         "CURRENT_MAX"
#define BL_CUR_FREQ_TAG         "BL_CUR_FREQ"
#define BL_PWM_FREQ_TAG         "BL_PWM_FREQ"
#define BL_WAIT_LOGO_TAG        "BL_WAIT_LOGO"
#define BL_CONTROL_IF_TAG       "BL_CONTROL_IF"
#define RENDER_IN_COLOR_FORMAT_TAG      "RENDER_IN_COLOR_FORMAT"
#define RENDER_IN_COLOR_RANGE_TAG       "RENDER_IN_COLOR_RANGE"
#define RENDER_IN_COLOR_SPACE_TAG       "RENDER_IN_COLOR_SPACE"
#define GAME_DIRECT_FR_MODE_TAG "GameDirectFrameRateMode"
#define DLG_ENABLE_TAG "DLG_Enable"
#define DLG_GPIO_CUS_CTRL_TAG "DLG_GPIO_CusCtrl"
#define DLG_I2C_CUS_CTRL_TAG "DLG_I2C_CusCtrl"
#define DLG_I2C_SEC_CUS_CTRL_TAG "DLG_I2C_Sec_CusCtrl"
#define DLG_ACON_MEM_ENABLE_TAG "DLG_ACOn_Mem_Enable"
#define CTRL_BIT_EN_TAG		"CTRL_BIT_Enable"//meta data

/*
 *#################################
 *#    tconless model setting     #
 *#################################
 */
#define HIGT_FRAME_RATE_BIN_PATH        "higt_frame_rate_bin_path"
#define HIGT_PIXEL_CLOCK_BIN_PATH       "higt_pixel_clock_bin_path"
#define GAME_MODE_BIN_PATH              "game_mode_bin_path"
#define HIGT_FRAME_RATE_GAMMA_BIN_PATH  "higt_frame_rate_gamma_bin_path"
#define HIGT_PIXEL_CLOCK_GAMMA_BIN_PATH "higt_pixel_clock_gamma_bin_path"
#define GAME_MODE_GAMMA_BIN_PATH        "game_mode_gamma_bin_path"
#define POWER_SEQ_ON_BIN_PATH           "power_seq_on_bin_path"
#define POWER_SEQ_OFF_BIN_PATH          "power_seq_off_bin_path"

/*
 *###########################
 *#       swing level       #
 *###########################
 */

#define SWING_USER_DEF       "swing_usr_define"
#define SWING_COMMON         "common_swing"
#define SWING_CTRL_LANE      "swing_ctrl_lane"
#define SWING_LEVEL          "swing_level"

/*
 *###########################
 *#       SSC      #
 *###########################
 */

#define SSC_EN		"ssc_ctrl_en"
#define SSC_MODULATION	"ssc_modulation"
#define SSC_DEVIATION	"ssc_deviation"
#define SSC_PROTECT_BY_USER_EN	"ssc_protect_by_user_en"
#define SSC_MODULATION_PROTECT_VALUE	"ssc_modulation_protect_value"
#define SSC_DEVIATION_PROTECT_VALUE	"ssc_deviation_protect_value"


/*
 *###########################
 *#   pre-emphasis level    #
 *###########################
 */
#define PE_USER_DEF       "pe_usr_define"
#define PE_COMMON         "common_pe"
#define PE_CTRL_LANE      "pe_ctrl_lane"
#define PE_LEVEL          "pe_level"
#define PE_EN_TAG         "PE_Level_Enable"
#define PE_CHS_TAG        "PE_Control_Channels"
#define PE_CH3_0_TAG      "PE_Level_CH3to0"
#define PE_CH7_4_TAG      "PE_Level_CH7to4"
#define PE_CH11_8_TAG     "PE_Level_CH11to8"
#define PE_CH15_12_TAG    "PE_Level_CH15to12"
#define PE_CH19_16_TAG    "PE_Level_CH19to16"
#define PE_CH23_20_TAG    "PE_Level_CH23to20"
#define PE_CH27_24_TAG    "PE_Level_CH27to24"
#define PE_CH31_28_TAG    "PE_Level_CH31to28"

/*
 *###########################
 *#   output lane order     #
 *###########################
 */
#define LANE_CH3_0_TAG    "MOD_LANE_ORDER_CH3to0"
#define LANE_CH7_4_TAG    "MOD_LANE_ORDER_CH7to4"
#define LANE_CH11_8_TAG   "MOD_LANE_ORDER_CH11to8"
#define LANE_CH15_12_TAG  "MOD_LANE_ORDER_CH15to12"
#define LANE_CH19_16_TAG  "MOD_LANE_ORDER_CH19to16"
#define LANE_CH23_20_TAG  "MOD_LANE_ORDER_CH23to20"
#define LANE_CH27_24_TAG  "MOD_LANE_ORDER_CH27to24"
#define LANE_CH31_28_TAG  "MOD_LANE_ORDER_CH31to28"

/*
 *###########################
 *#      output config      #
 *###########################
 */
#define OUTCFG_CH3_0_TAG      "MOD_OUTPUT_CONFIG_CH3to0"
#define OUTCFG_CH7_4_TAG      "MOD_OUTPUT_CONFIG_CH7to4"
#define OUTCFG_CH11_8_TAG     "MOD_OUTPUT_CONFIG_CH11to8"
#define OUTCFG_CH15_12_TAG    "MOD_OUTPUT_CONFIG_CH15to12"
#define OUTCFG_CH19_16_TAG    "MOD_OUTPUT_CONFIG_CH19to16"
#define OUTCFG_CH23_20_TAG    "MOD_OUTPUT_CONFIG_CH23to20"
#define OUTCFG_CH27_24_TAG    "MOD_OUTPUT_CONFIG_CH27to24"
#define OUTCFG_CH31_28_TAG    "MOD_OUTPUT_CONFIG_CH31to28"

/*
 *###########################
 *#    output lane oder   #
 *###########################
 */
#define SUPPORT_LANES        "support_lanes"
#define LAYOUT_ORDER         "layout_order"
#define USR_DEFINE           "usr_define"
#define CTRL_LANES           "ctrl_lanes"
#define LANEORDER            "laneorder"

/*
 *###########################
 *#   panel-color-info      #
 *###########################
 */
#define COLOR_FORMAT_TAG  "COLOR_FORAMT"
#define RX_TAG            "Rx"
#define RY_TAG            "Ry"
#define GX_TAG            "Gx"
#define GY_TAG            "Gy"
#define BX_TAG            "Bx"
#define BY_TAG            "By"
#define WX_TAG            "Wx"
#define WY_TAG            "Wy"
#define MAX_LUMI_TAG      "MaxLuminance"
#define MED_LUMI_TAG      "MedLuminance"
#define MIN_LUMI_TAG      "MinLuminance"
#define LINEAR_RGB_TAG    "LinearRGB"
#define HDR_NITS_TAG      "HDRPanelNits"
#define FRC_TABLE_TAG     "FRC_TABLE"

/*
 *###########################
 *#        hw-info          #
 *###########################
 */
#define TGEN_VERSION					"tgen_version"
#define LPLL_VERSION					"lpll_version"
#define MOD_VERSION					"mod_version"
#define PNL_LIB_VER					"pnl_lib_version"
#define RCON_ENABLE						"rcon_enable"
#define RCON_MAX						"rcon_max"
#define RCON_MIN						"rcon_min"
#define RCON_VALUE						"rcon_value"
#define BIASCON_SINGLE_MAX				"biascon_single_max"
#define BIASCON_SINGLE_MIN				"biascon_single_min"
#define BIASCON_SINGLE_VALUE			"biascon_single_value"
#define BIASCON_BIASCON_DOUBLE_MAX		"biascon_double_max"
#define BIASCON_BIASCON_DOUBLE_MIN		"biascon_double_min"
#define BIASCON_BIASCON_DOUBLE_VALUE	"biascon_double_value"
#define RINT_ENABLE						"rint_enable"
#define RINT_MAX						"rint_max"
#define RINT_MIN						"rint_min"
#define RINT_VALUE						"rint_value"

/*
 *###########################
 *#   ext-graph-combo-info  #
 *###########################
 */
 #define GRAPH_VBO_BYTE_MODE	"graph_vbo_byte_mode"

/*
 *###########################
 *#   panel-dither-info     #
 *###########################
 */
#define DITHER_INFO         "panel-dither-info"
#define DITHER_DEPTH        "dither_out_depth"
#define DITHER_PATTERN      "dither_pattern_type"
#define DITHER_CAPABILITY   "dither_capability"

/*
 *#################################
 *#    panel tcon i2c setting     #
 *#################################
 */
#define TCON_I2C_BUS_TAG              "tcon_i2c_bus"
#define TCON_I2C_MODE_TAG             "tcon_i2c_mode"
#define TCON_I2C_DEV_ADDR_TAG         "tcon_i2c_dev_addr"
#define TCON_I2C_REG_OFFSET_TAG       "tcon_i2c_reg_offst"
#define TCON_I2C_INFO_LENGTH_TAG      "tcon_i2c_info_length"
#define TCON_I2C_INFO_ROW_TAG         "tcon_i2c_info_row"
#define TCON_I2C_INFO_COLUMN_NUM_TAG  "tcon_i2c_info_column_num"
#define TCON_I2C_INFO_DATA_TAG        "tcon_i2c_info_data"

#define TCON_I2C_SEC_BUS_TAG              "tcon_i2c_sec_bus"
#define TCON_I2C_SEC_MODE_TAG             "tcon_i2c_sec_mode"
#define TCON_I2C_SEC_DEV_ADDR_TAG         "tcon_i2c_sec_dev_addr"
#define TCON_I2C_SEC_REG_OFFSET_TAG       "tcon_i2c_sec_reg_offst"
#define TCON_I2C_SEC_INFO_LENGTH_TAG      "tcon_i2c_sec_info_length"
#define TCON_I2C_SEC_INFO_ROW_TAG         "tcon_i2c_sec_info_row"
#define TCON_I2C_SEC_INFO_COLUMN_NUM_TAG  "tcon_i2c_sec_info_column_num"
#define TCON_I2C_SEC_INFO_DATA_TAG        "tcon_i2c_sec_info_data"

/*
 *#################################
 *#   panel tcon vrr od/pga setting   #
 *#################################
 */

#define VRR_OD_TABLE_NUM_MAX		(10)
#define TOTAL_FREQ_TABLE			(150)
#define VRR_OD_EN_TAG               "vrr_od_enable"
#define VRR_OD_TAB_LIST_TAG         "vrr_od_table_list"
#define VRR_OD_TAB_EN_TAG           "vrr_od_table_en"
#define VRR_OD_TAB_IDX_TAG          "vrr_od_table_idx"
#define VRR_OD_LO_BOUND_TAG         "vrr_od_lo_bound"
#define VRR_OD_HI_BOUND_TAG         "vrr_od_hi_bound"
#define VRR_OD_FILE_TAG             "vrr_od_file"

#define VRR_PGA_EN_TAG              "vrr_pga_enable"
#define VRR_PGA_TAB_LIST_TAG        "vrr_pga_table_list"
#define VRR_PGA_TAB_EN_TAG          "vrr_pga_table_en"
#define VRR_PGA_TAB_IDX_TAG         "vrr_pga_table_idx"
#define VRR_PGA_LO_BOUND_TAG        "vrr_pga_lo_bound"
#define VRR_PGA_HI_BOUND_TAG        "vrr_pga_hi_bound"
#define VRR_PGA_FILE_TAG            "vrr_pga_file"

/*
 *#################################
 *#    panel cust tcon setting    #
 *#################################
 */
#define TCON_CUST_SUPPORT_I2C_WP_TAG       "bSupportTconI2cWP"
#define TCON_CUST_GPIO_TCON_I2C_WP_ON_TAG  "ucGPIO_TCON_I2C_WP_ON"
#define TCON_CUST_GPIO_TCON_I2C_WP_OFF_TAG "ucGPIO_TCON_I2C_WP_OFF"

/*
 *###########################
 *#     panel_tgen_info     #
 *###########################
 */
#define PNL_TGEN_MAX_VTOTAL     "panel_tgen_max_vtotal"
#define PNL_TGEN_MIN_VTOTAL     "panel_tgen_min_vtotal"

/* WakeUp reason. */
#define PM_WK_NONE          (0x00)  /* No wake up event.  */
#define PM_WK_IR            (0x01)
#define PM_WK_DVI0          (0x02)
#define PM_WK_DVI1          (0x12)
#define PM_WK_DVI2          (0x03)
#define PM_WK_DVI3          (0x13)
#define PM_WK_CEC           (0x04)
#define PM_WK_CEC_PORT1     (0x14)  /* Legacy. */
#define PM_WK_CEC_PORT2     (0x24)  /* Legacy. */
#define PM_WK_CEC_PORT3     (0x34)  /* Legacy. */
#define PM_WK_SAR           (0x05)
#define PM_WK_ESYNC         (0x06)  /* Legacy. */
#define PM_WK_SYNC          (0x07)
#define PM_WK_RTC           (0x08)
#define PM_WK_RTC2          (0x09)  /* Legacy. */
#define PM_WK_AVLINK        (0x0A)  /* Legacy. */
#define PM_WK_VOICE         (0x1A)
#define PM_WK_VAD           (0x2A)
#define PM_WK_UART          (0x0B)
#define PM_WK_GPIO          (0x0C)  /* Legacy. */
#define PM_WK_GPIO_WOWLAN   (0x1C)
#define PM_WK_GPIO_WOBT     (0x2C)
#define PM_WK_GPIO_WOEWBS   (0x3C)  /* Legacy. */
#define PM_WK_GPIO_WOEMER   (0x4C)  /* ATSC for EWBS and AEAS. */
#define PM_WK_GPIO_WOMUTE   (0x5C)
#define PM_WK_MHL           (0x0D)
#define PM_WK_WOL           (0x0E)
#define PM_WK_WOC           (0x0F)

#define HWREG_VIDEO_REG_NUM_PANEL (30)


#define VRR_TOLERANCE_OUPTUT_60  (5)
#define VRR_TOLERANCE_OUPTUT_120 (5)
#define VRR_TOLERANCE_OUPTUT_144 (10)
#define VRR_TOLERANCE_OUPTUT_240 (5)
#define IS_OUTPUT_FRAMERATE_60(x)      ((x > 55000 ) && (x <= 65000 ))
#define IS_OUTPUT_FRAMERATE_120(x)     ((x > 65000 ) && (x <= 125000))
#define IS_OUTPUT_FRAMERATE_144(x)     ((x > 125000) && (x <= 160000))
#define IS_OUTPUT_FRAMERATE_240(x)     ((x > 160000) && (x <= 300000))

#define DEFAULT_OD_MODE_TYPE (27) //E_HALPNL_OD_MODE_RGB_888_HSCALING

//------------------------------------------------------------------------------
// Enum
//------------------------------------------------------------------------------

/// Define Mirror Type
typedef enum {
	E_PNL_GAMMA_TABLE_FROM_BIN,
	E_PNL_GAMMA_TABLE_FROM_REG,
	E_PNL_GAMMA_TABLE_TYPE_MAX,
} drm_en_pnl_PanelGammaTableType;

typedef enum {
	E_VG_COMB_MODE,
	E_VG_SEP_MODE,
	E_VG_SEP_W_DELTAV_MODE,
	E_VIDEO_GFX_DISP_MODE_MAX
} drm_en_video_gfx_disp_mode;

typedef enum {
	E_PNL_BFI_DBGLOG = 0,
	E_PNL_VTT_VALUE_DBGLOG,
	E_PNL_VTT_AVG_DBGLOG,
	E_PNL_VTTV_INFO_DBGLOG,
	E_PNL_DBGLOG_MAX
} drm_pnl_debug_log;

/// Define the panel gamma bin entry
typedef enum {
	///< Indicate PNL Gamma is 256 entrise
	E_PNL_PNLGAMMA_256_ENTRIES = 0,
	///< Indicate PNL Gamma is 1024 entries
	E_PNL_PNLGAMMA_1024_ENTRIES,
	///< Indicate PNL Gamma is MAX entries
	E_PNL_PNLGAMMA_MAX_ENTRIES
} drm_en_pnl_Gamma_Entries;

/// Define the panel gamma precision type
typedef enum {
	///< Gamma Type of 10bit
	E_PNL_GAMMA_10BIT = 0,
	///< Gamma Type of 12bit
	E_PNL_GAMMA_12BIT,
	///< The library can support all mapping mode
	E_PNL_GAMMA_ALL
} drm_en_pnl_Gamma_Type;

//Define Framesync state
enum drm_en_pnl_framesync_state {
	E_PNL_FRAMESYNC_STATE_PROP_IN = 0,
	E_PNL_FRAMESYNC_STATE_PROP_FIRE,
	E_PNL_FRAMESYNC_STATE_IRQ_IN,
	E_PNL_FRAMESYNC_STATE_IRQ_FIRE,
	E_PNL_FRAMESYNC_STATE_MAX,
};

/*Define the pixel mute command*/
enum drm_mtk_pixel_mute_dbg_cmd {
	MTK_DRM_PIXEL_MUTE_EN = 0,
	MTK_DRM_PIXEL_MUTE_VLATCH,	// 1
	MTK_DRM_PIXEL_MUTE_RED,		// 2
	MTK_DRM_PIXEL_MUTE_GREEN,	// 3
	MTK_DRM_PIXEL_MUTE_BLUE,	// 4
	MTK_DRM_PIXEL_MUTE_CONNECTOR_TYPE,// 5
	MTK_DRM_PIXEL_MUTE_BUFFER_1,	// 6
	MTK_DRM_PIXEL_MUTE_CMD_MAX,	// 7
};

//mod_size_range
enum drm_mtk_set_mod_pattern_cmd {
	MTK_DRM_SET_MOD_EN = 0,
	MTK_DRM_SET_MOD_RED,		// 1
	MTK_DRM_SET_MOD_GREEN,		// 2
	MTK_DRM_SET_MOD_BLUE,		// 3
	MTK_DRM_SET_MOD_H_ST,		// 4
	MTK_DRM_SET_MOD_H_END,		// 5
	MTK_DRM_SET_MOD_V_ST,		// 6
	MTK_DRM_SET_MOD_V_END,		// 7
	MTK_DRM_SET_MOD_CONNECTOR_TYPE,	// 8
	MTK_DRM_SET_MOD_BUFFER_1,
	MTK_DRM_SET_MOD_BUFFER_2,
	MTK_DRM_SET_MOD_BUFFER_3,
	MTK_DRM_SET_MOD_CMD_MAX,
};

enum drm_en_pnl_change_mode_id_state {
	E_MODE_ID_CHANGE_DONE = 0,
	E_MODE_ID_CHANGE_START,
	E_MODE_ID_CHANGE_INIT,
};

enum drm_en_pnl_freq_group {
	E_PNL_FREQ_GROUP1 = 0,
	E_PNL_FREQ_GROUP2,
	E_PNL_FREQ_GROUP3,
	E_PNL_FREQ_GROUP4,
	E_PNL_FREQ_GROUP_UNDEFINED,
};

enum en_pnl_scdisp_path_sel {
	E_PNL_SCDISP_PATH_PQGA_OSDB_LD,
	E_PNL_SCDISP_PATH_PQGA_LD_OSDB,
	E_PNL_SCDISP_PATH_OSDB_PQGA_LD,
	E_PNL_SCDISP_PATH_OSDB_LD_PQGA,
	E_PNL_SCDISP_PATH_LD_PQGA_OSDB,
	E_PNL_SCDISP_PATH_LD_OSDB_PQGA,
	E_PNL_SCDISP_PATH_MAX,
};

//-------------------------------------------------------------------------------------------------
// Structure
//-------------------------------------------------------------------------------------------------
extern struct platform_driver mtk_tv_drm_panel_driver;
extern struct platform_driver mtk_tv_drm_gfx_panel_driver;
extern struct platform_driver mtk_tv_drm_extdev_panel_driver;

struct drm_st_tcon_i2c_info {
	__u8 i2c_bus;
	__u8 i2c_mode;
	__u8 i2c_dev_addr;
	__u8 i2c_reg_offst;
	__u8 i2c_info_length;
	__u8 i2c_info_row;
	__u8 *i2c_info_column_num;
	__u8 *i2c_info_data;
};

struct drm_st_i2c_cmd_info {
	__u8 i2c_cmd_bus;
	__u8 i2c_cmd_dev_addr;
	__u8 i2c_cmd_reg_offst;
	__u8 i2c_cmd_length;
	__u8 *i2c_cmd_data;
};

typedef struct {
	bool	bEnable;
	__u16	u16Index;
	__u16	u16FreqLowBound;
	__u16	u16FreqHighBound;
	const char *s8VRR_OD_table_Name;
} drm_st_VRR_OD_Setting;

typedef struct {
	__u8	u8VRR_OD_table_Total;
	drm_st_VRR_OD_Setting	stVRR_OD_Table[VRR_OD_TABLE_NUM_MAX];
} drm_st_VRR_OD_info;

typedef struct {
	bool	bEnable;
	__u16	u16Index;
	__u16	u16FreqLowBound;
	__u16	u16FreqHighBound;
	const char *s8VRR_PGA_table_Name;
} drm_st_VRR_PGA_Setting;

typedef struct {
	__u8	u8VRR_PGA_table_Total;
	drm_st_VRR_PGA_Setting	stVRR_PGA_Table[VRR_OD_TABLE_NUM_MAX];
} drm_st_VRR_PGA_info;

typedef struct {
	__u16 version;
	__u16 length;
	__u16 dtsi_file_type;
	__u16 dtsi_file_ver;
	bool pgamma_en;
	const char *pgamma_path;
	bool tcon_en;
	const char *tcon_path;
	drm_en_pnl_mirror_type mirror_mode;
	bool hmirror_en;
	drm_en_pnl_output_format op_format;
	bool fs_mode;
	bool od_en;
	__u64 od_buf_addr;
	__u32 od_buf_size;
	__u16 vcc_bl_cusctrl;
	__u32 vcc_offon_delay;
	enum en_pnl_scdisp_path_sel escdisp_path_sel;
	bool force_free_run_en;//force free run mode
	__u16 gpio_bl;
	__u16 gpio_vcc;
	__u16 m_del;
	__u16 cus_color_prim;
	__u16 src_wx;
	__u16 src_wy;
	drm_en_video_gfx_disp_mode disp_mode;
	__u16 osd_height;
	__u16 osd_width;
	__u16 osd_hs_st;
	__u16 osd_hs_end;
	__u16 osd_hde_st;
	__u16 osd_hde_end;
	__u16 osd_htt;
	__u16 osd_vs_st;
	__u16 osd_vs_end;
	__u16 osd_vde_st;
	__u16 osd_vde_end;
	__u16 osd_vtt;
	bool ctrl_bit_en;//meta data

	// backlight
	__u16 pwmport_num;
	__u32 pwmport[PWM_PORT_MAX_NUM];
	__u32 period_pwm[PWM_PORT_MAX_NUM];
	__u32 duty_pwm[PWM_PORT_MAX_NUM];
	__u32 shift_pwm[PWM_PORT_MAX_NUM];
	__u32 div_pwm[PWM_PORT_MAX_NUM];
	__u32 pol_pwm[PWM_PORT_MAX_NUM];
	__u32 max_pwm_val[PWM_PORT_MAX_NUM];
	__u32 min_pwm_val[PWM_PORT_MAX_NUM];
	__u32 pwm_period_multi[PWM_PORT_MAX_NUM];
	__u32 pwm_vsync_enable[PWM_PORT_MAX_NUM];
	const char *panel_id_str;
	bool overdrive_en;
	__u32 panel_index;
	__u32 inch_size;
	__u32 chassis_index;
	__u32 vcom_pattern;
	__u32 vcom_type;
	__u32 spread_permillage;
	__u32 spread_freq;
	__u32 ocell_demura_idx;
	__u32 tcon_index;
	__u32 current_max;
	__u32 backlight_curr_freq;
	__u32 backlight_pwm_freq;
	__u32 backlight_wait_logo;
	__u32 backlight_control_if;
	__u16 render_in_color_format;
	__u16 render_in_color_range;
	__u16 render_in_color_space;
	__u32 game_direct_framerate_mode;
	struct drm_st_tcon_i2c_info *tcon_i2c_info;
	struct drm_st_tcon_i2c_info *tcon_i2c_sec_info;
	bool dlg_en;
	bool dlg_gpio_CustCtrl;
	bool dlg_i2c_CustCtrl;
	bool dlg_i2c_sec_CustCtrl;
	bool i2c_wp_CustCtrl;
	struct gpio_desc *i2c_wp_gpio;
	__u32 i2c_wp_off;
	__u32 i2c_wp_on;
	bool dlg_acon_mem_enable;
	__u32 global_mute_delay;
	__u32 global_unmute_delay;
	__u32 global_mute_backlight_ignore;
	char panel_name[DRM_NAME_MAX_NUM];
	__u16 pwm_to_bl_delay;
	__u16 bl_to_pwm_delay;
} drm_st_pnl_cus_setting;

typedef struct {
	const char *higt_frame_rate_bin_path;
	const char *higt_pixel_clock_bin_path;
	const char *game_mode_bin_path;
	const char *higt_frame_rate_gamma_bin_path;
	const char *higt_pixel_clock_gamma_bin_path;
	const char *game_mode_gamma_bin_path;
	const char *power_seq_on_bin_path;
	const char *power_seq_off_bin_path;
} drm_st_tconless_model_info;

typedef struct {
	__u16 version;
	__u16 length;
	bool en;
	__u16 ctrl_chs;
	__u32 ch3_0;
	__u32 ch7_4;
	__u32 ch11_8;
	__u32 ch15_12;
	__u32 ch19_16;
	__u32 ch23_20;
	__u32 ch27_24;
	__u32 ch31_28;
} drm_st_output_ch_info;

typedef struct {
	__u16  version;
	__u16  length;
	drm_en_pnl_output_format format;
	__u16 rx;
	__u16 ry;
	__u16 gx;
	__u16 gy;
	__u16 bx;
	__u16 by;
	__u16 wx;
	__u16 wy;
	__u8 maxlum;
	__u8 medlum;
	__u8 minlum;
	bool linear_rgb;
	__u16 hdrnits;
} drm_st_pnl_color_info;

struct drm_st_pnl_frc_table_info {
	__u16 u16LowerBound;
	__u16 u16HigherBound;
	__u8 u8FRC_in;      //ivs
	__u8 u8FRC_out;     //ovs
};

struct drm_st_output_timing_info {
	__u32 u32OutputVfreq;
	__u16 u16OutputVTotal;
	enum video_crtc_frame_sync_mode eFrameSyncMode;
	__u8 u8FRC_in;      //ivs
	__u8 u8FRC_out;     //ovs
	bool locked_flag;
	bool AutoForceFreeRun;
	bool FrameSyncModeChgBypass;
	enum drm_en_pnl_framesync_state eFrameSyncState;
};

typedef struct {
	__u32 pnl_lib_version;
	bool rcon_enable;
	__u32 rcon_max;
	__u32 rcon_min;
	__u32 rcon_value;
	__u32 biascon_single_max;
	__u32 biascon_single_min;
	__u32 biascon_single_value;
	__u32 biascon_double_max;
	__u32 biascon_double_min;
	__u32 biascon_double_value;
	bool rint_enable;
	__u32 rint_max;
	__u32 rint_min;
	__u32 rint_value;
} drm_st_hw_info;

typedef struct {
	__u32 graph_vbo_byte_mode;
} drm_st_ext_graph_combo_info;

struct drm_st_dither_info {
	drm_en_dither_pattern dither_pattern;
	drm_en_dither_depth dither_depth;
	__u32 dither_capability;
};

struct drm_st_dither_reg {
	bool dither_frc_on;        //BANK:0xA324_3F[0]
	bool dither_noise_disable; //BANK:0xA324_3F[3]
	bool dither_tail_cut;      //BANK:0xA324_3F[4]
	bool dither_box_freeze;    //BANK:0xA324_40[8]
	bool dither_dith_bit;      //BANK:0xA324_3F[2]
	bool dither_12bit_bypass;  //BANK:0xA324_3F[11]
	bool dither_is_12bit;      //BANK:0xA324_3F[15]
};

enum drm_pnl_ssc_link_if_sel {
	E_LINK_SSC_NONE,
	E_LINK_SSC_VBY1,
	E_LINK_SSC_LVDS,
	E_LINK_SSC_EPI15,
	E_LINK_SSC_EPI30,
	E_LINK_SSC_CEDS1,
	E_LINK_SSC_CEDS2,
	E_LINK_SSC_CMPI20,
	E_LINK_SSC_USIT16,
	E_LINK_SSC_USIT31,
	E_LINK_SSC_CSPI18,
	E_LINK_SSC_ISP18,
	E_LINK_SSC_PDI,
	E_LINK_SSC_CHPI,
	E_LINK_SSC_MINILVDS,
	E_LINK_SSC_MAX,
};

//TCON Pattern Detect Function
typedef enum
{
    E_PNL_TCON_PDF_MODE_CSOT_4K1K_HSR = BIT(7),  //defined by dummy resigter
    E_PNL_TCON_PDF_MODE_BOE_4K1K_HSR  = BIT(5),
    E_PNL_TCON_PDF_MODE_HKC           = BIT(4),
    E_PNL_TCON_PDF_MODE_AUO_4K1K_HSR  = BIT(1),
}EN_PNL_TCON_PDF_MODE;

struct drm_st_pnl_vrr_hfr_pmic_i2c_ctrl_info {
	bool SupportVrrHfrTconPmicI2cSet;
	__u8 VRR_HFR_PMIC_I2C_PRE_DELAY;
	__u8 VRR_HFR_PMIC_I2C_SLAVE_ADDR;
	__u8 VRR_HFR_ON_PMIC_I2C_ADDR_SIZE;
	__u8 *VRR_HFR_ON_PMIC_I2C_ADDR;
	__u8 VRR_HFR_ON_PMIC_I2C_DATA_SIZE;
	__u8 *VRR_HFR_ON_PMIC_I2C_DATA;
	__u8 VRR_HFR_OFF_PMIC_I2C_ADDR_SIZE;
	__u8 *VRR_HFR_OFF_PMIC_I2C_ADDR;
	__u8 VRR_HFR_OFF_PMIC_I2C_DATA_SIZE;
	__u8 *VRR_HFR_OFF_PMIC_I2C_DATA;
};

struct drm_st_pnl_tgen_info {
	__u16 pnl_tgen_max_vtotal;
	__u16 pnl_tgen_min_vtotal;
};

struct mtk_panel_context {
	//struct drm_property * drm_video_plane_prop[E_VIDEO_PLANE_PROP_MAX];
	//struct video_plane_prop * video_plane_properties;

	drm_st_pnl_info info;
	drm_st_pnl_info extdev_info;
	drm_st_pnl_info gfx_info;
	drm_st_pnl_cus_setting cus;
	drm_st_output_ch_info swing;//?????Do we need to remove this one?
	drm_st_output_ch_info pe;
	drm_st_output_ch_info lane_order;
	drm_st_output_ch_info op_cfg;
	drm_st_pnl_color_info color_info;
	drm_mod_cfg out_cfg_test; //out cfg for test
	drm_mod_cfg v_cfg;
	drm_st_out_lane_order out_lane_info;
	struct drm_st_pnl_frc_table_info frc_info[PNL_FRC_TABLE_MAX_INDEX];
	struct drm_st_output_timing_info outputTiming_info;
	struct drm_st_pnl_frc_ratio_info frcRatio_info;
	drm_update_output_timing_type out_upd_type;
	struct drm_panel drmPanel;
	struct device *dev;
	__u8 u8TimingsNum;
	struct drm_st_out_pe_level stdrmPE;
	struct drm_st_out_ssc_info stdrmSSC;
	struct drm_st_out_swing_level stdrmSwing;
	struct list_head panelInfoList;
	struct gpio_desc *gpio_vcc;
	struct gpio_desc *gpio_backlight;
	__u8 u8controlbit_gfid;
	drm_st_hw_info hw_info;
	drm_st_ext_graph_combo_info ext_grpah_combo_info;
	bool disable_ModeID_change;
	bool bVby1LocknProtectEn;
	bool bBFIEn;
	uint32_t gu32frameID;
	uint64_t gu64panel_debug_log;
	uint8_t gu8Triggergendebugsetting; // 0:debug off, 1:force Legacy, 2:force TS
	struct clk *st_mod_clk_en[E_MOD_V1_EN_NUM];
	struct clk *st_mod_clk_V2_en[E_MOD_V2_EN_NUM];
	struct clk *st_mod_clk_V3_en[E_MOD_V3_EN_NUM];
	uint32_t Tgen_vtt; //Add for Tgen VTT
	uint32_t dump_reg_bank; //Autolog DBG to dump reg
	uint32_t pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_CMD_MAX]; //pixel_mute rgb data info
	uint32_t mod_ptrn_set_info[MTK_DRM_SET_MOD_CMD_MAX]; //mod pattern set data info
	struct drm_st_dither_info dither_info;
	drm_st_tconless_model_info tconless_model_info;
	struct mtk_oled_info oled_info;
	bool lane_duplicate_en;
	struct gpio_desc *gpio_dlg;
	bool vrr_od_En;
	drm_st_VRR_OD_info *VRR_OD_info;
	bool vrr_pga_En;
	drm_st_VRR_PGA_info *VRR_PGA_info;
	struct gpio_desc *gpio_lc_toggle;
	struct drm_st_pnl_vrr_hfr_pmic_i2c_ctrl_info vrr_hfr_pmic_i2c_ctrl;
	struct drm_st_pnl_tgen_info tgen_info;
};

typedef struct {
	/// Struct version
	uint32_t u32Version;
	/// Sturct length
	uint32_t u32Length;
	/// IN: select Get gamma value from
	drm_en_pnl_PanelGammaTableType enPnlGammaTbl;
	/// IN: Bin file address
	uint8_t *pu8GammaTbl;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy0;
#endif
	/// IN: PNL Gamma bin size
	uint32_t  u32TblSize;
	/// IN/OUT: R channel 0 data
	uint16_t *pu16RChannel0;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy1;
#endif
	/// IN/OUT: G channel 0 data
	uint16_t *pu16GChannel0;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDumm2;
#endif
	/// IN/OUT: B channel 0 data
	uint16_t *pu16BChannel0;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy3;
#endif
	/// IN/OUT: W channel 0 data
	uint16_t *pu16WChannel0;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy4;
#endif
	/// IN/OUT: R channel 1 data
	uint16_t *pu16RChannel1;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy5;
#endif
	/// IN/OUT: G channel 1 data
	uint16_t *pu16GChannel1;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy6;
#endif
	/// IN/OUT: B channel 1 data
	uint16_t *pu16BChannel1;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy7;
#endif
	/// IN/OUT: W channel 1 data
	uint16_t *pu16WChannel1;
#if !defined(__aarch64__)
	/// Dummy parameter
	void *pDummy8;
#endif
	/// IN: the index of the data want to get
	uint16_t u16Index;
	/// IN: every channel data number
	uint16_t u16Size;
	/// OUT: Support PNL gamma Channel 1
	uint8_t bSupportChannel1;
	// OUT: PNL gamma bin entries
	drm_en_pnl_Gamma_Entries enGammaEntries;
	// OUT: PNL gamma bin type
	drm_en_pnl_Gamma_Type enGammaType;
	// OUT: PNL gamma bin 2D table number
	uint16_t u162DTableNum;
	// OUT: PNL gamma bin 3D table number
	uint16_t u163DTableNum;
} drm_st_pnl_gammatbl;

//-------------------------------------------------------------------------------------------------
// Function
//-------------------------------------------------------------------------------------------------
struct mtk_tv_kms_context;

void mtk_render_output_en(
	struct mtk_panel_context *pctx, bool bEn);
int mtk_render_set_output_hmirror_enable(
	struct mtk_panel_context *pctx);
bool mtk_render_cfg_connector(
	struct mtk_panel_context *pctx);
bool mtk_render_cfg_crtc(
	struct mtk_panel_context *pctx);
int _mtk_video_set_customize_frc_table(
	struct mtk_panel_context *pctx_pnl,
	struct drm_st_pnl_frc_table_info *customizeFRCTableInfo);
int _mtk_video_set_framesync_mode(
	struct mtk_tv_kms_context *pctx);
int _mtk_video_set_low_latency_mode(
	struct mtk_tv_kms_context *pctx,
	uint64_t bLowLatencyEn);
int mtk_get_framesync_locked_flag(
	struct mtk_tv_kms_context *pctx);
bool mtk_video_panel_get_framesync_mode_en(
	struct mtk_tv_kms_context *pctx);
int mtk_video_panel_update_framesync_state(
	struct mtk_tv_kms_context *pctx);

int mtk_tv_drm_check_out_mod_cfg(
	struct mtk_panel_context *pctx_pnl,
	drm_parse_dts_info *src_info);

int mtk_tv_drm_check_out_lane_cfg(
	struct mtk_panel_context *pctx_pnl);

int mtk_render_set_vbo_ctrlbit(
	struct mtk_panel_context *pctx_pnl,
	struct drm_st_ctrlbits *pctrlbits);

int mtk_render_set_tx_mute(
	struct mtk_panel_context *pctx_pnl,
	drm_st_tx_mute_info tx_mute_info);

int mtk_render_set_tx_mute_common(
	struct mtk_panel_context *pctx_pnl,
	drm_st_tx_mute_info tx_mute_info,
	uint8_t u8connector_id);

int mtk_tv_drm_get_phase_diff_us(uint32_t *get_phase_diff_val);

char *strtok_video_panel(char *s, const char *ct);

int mtk_video_panel_get_all_value_from_string(
	char *buf,
	char *delim,
	unsigned int len,
	uint32_t *value);

int mtk_render_set_pixel_mute_video(
	drm_st_pixel_mute_info pixel_mute_info);

int mtk_render_set_gop_pattern_en(bool bEn);
int mtk_render_set_multiwin_pattern_en(bool bEn);
int mtk_render_set_pafrc_post_pattern_en(bool bEn);
int mtk_render_set_sec_pattern_en(bool bEn);
int mtk_render_set_tcon_pattern_en(bool bEn);
int mtk_render_set_tcon_blackpattern(bool bEn);
void mtk_render_pnl_set_BFI_En(struct mtk_panel_context *pctx_pnl, bool bEn);
int mtk_render_set_pixel_mute_deltav(
	drm_st_pixel_mute_info pixel_mute_info);

int mtk_render_set_pixel_mute_gfx(
	drm_st_pixel_mute_info pixel_mute_info);

int mtk_render_set_backlight_mute(
	struct mtk_panel_context *pctx,
	drm_st_backlight_mute_info backlight_mute_info);

int mtk_render_set_swing_vreg(
	struct drm_st_out_swing_level *stSwing, bool tcon_en);

int mtk_render_get_swing_vreg(
	struct drm_st_out_swing_level *stSwing, bool tcon_en);

int mtk_render_set_pre_emphasis(
	struct drm_st_out_pe_level *stPE);

int mtk_render_get_pre_emphasis(
	struct drm_st_out_pe_level *stPE);

int mtk_render_set_ssc(
	struct mtk_panel_context *pctx_pnl);

int mtk_render_get_ssc(
	struct drm_st_out_ssc *stSSC);

int mtk_render_get_mod_status(
	struct drm_st_mod_status *stdrmMod);

void mtk_render_pnl_checklockprotect(bool *bVby1LocknProtectEn);

int mtk_mod_clk_init(struct device *dev);

void mtk_render_pnl_set_Bfi_handle(struct mtk_panel_context *pctx_pnl, uint8_t u8PlaneNum);

void mtk_video_display_set_panel_color_format(struct mtk_panel_context *pctx);

void mtk_tv_drm_panel_set_VTT_debugmode(uint8_t u8DebugSelVal);

uint16_t mtk_tv_drm_panel_get_VTT_debugValue(void);

uint16_t mtk_tv_drm_panel_get_TgenMainVTT(void);

int mtk_tv_drm_panel_checkDTSPara(drm_st_pnl_info *pStPnlInfo);

int mtk_tv_drm_get_fps_value_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_video_get_outputinfo_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_set_oled_offrs_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_set_oled_jb_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_set_trigger_gen_debug(
	struct mtk_tv_kms_context *pctx);

int mtk_tv_drm_set_trigger_gen_reset_to_default(
	struct mtk_tv_kms_context *pctx,
	bool bResetToDefaultEn,
	bool render_modify);

int mtk_tv_drm_set_trigger_gen_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_get_phase_diff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_get_ssc_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_check_video_hbproch_protect(drm_parse_dts_info *data,
	st_drv_parse_out_info_from_dts_ans *sthbproch_protect_ans);

int mtk_video_get_vttv_input_vfreq(
	struct mtk_tv_kms_context *pctx,
	uint64_t plane_id,
	uint64_t *input_vfreq);

int mtk_render_get_Vtt_info(
	struct drm_st_vtt_info *stdrmVTTInfo);

int mtk_render_set_panel_dither(
	struct drm_st_dither_info *stdrmDITHERInfo);

int mtk_render_get_panel_dither_reg(
	struct drm_st_dither_reg *stdrmDITHERReg);

int mtk_video_panel_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val,
	int prop_index);

int mtk_video_panel_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val,
	int prop_index);

void mtk_render_set_pnl_init_done(
	bool bEn);

bool mtk_render_get_pnl_init_done(
	void);

int mtk_tv_drm_set_change_modeid_state_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_get_change_modeid_state_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_get_global_mute_ctrl_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_InitMODInterrupt(void);
bool mtk_tv_drm_GetMODInterrupt(en_drv_pnl_mod_irq_bit_map eModIrqType);
void mtk_tv_drm_ClearMODInterrupt(en_drv_pnl_mod_irq_bit_map eModIrqType);

bool mtk_tv_drm_Is_Support_TCON_LC_SecToggle(void);
void mtk_tv_drm_TCON_LC_SecToggle_Control(struct mtk_panel_context *pctx_pnl);
int mtk_render_set_global_mute(struct mtk_panel_context *pctx_pnl,
	struct mtk_tv_kms_context *pctx_kms,
	drm_st_global_mute_info global_mute_info,
	uint8_t u8connector_id);
int mtk_tv_drm_set_vac_enable_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
int mtk_tv_drm_set_dispout_pw_onoff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
int mtk_tv_drm_get_panel_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
int mtk_tv_drm_set_demura_onoff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
void mtk_panel_set_max_framerate(__u32 u32TypMaxFramerate, __u32 u32DlgMaxFramerate);
struct drm_mtk_panel_max_framerate mtk_panel_get_max_framerate(void);
int mtk_tv_drm_get_panel_max_framerate_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
int mtk_tv_drm_panel_task_handler(void *data);
bool mtk_tv_drm_Is_Support_PatDetectFunc(void);
void mtk_tv_drm_TCON_PatDetectFunc_Control(void);
uint32_t mtk_tv_drm_get_fpsx100_value(void);
int mtk_tv_drm_get_blon_status_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
int mtk_tv_drm_get_pnl_timing_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);
#endif
