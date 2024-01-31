/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_DRM_LD_H_
#define _MTK_DRM_LD_H_

#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_crtc.h"


#define MTK_PLANE_DISPLAY_PIPE (3)

#define DEVICE_MAX_NUM	(0x20UL)
#define MSPI_MAX_NUM (0x08UL)
#define MSPI_MAX_NUM_2 (0x02UL)

#define DMA_INFO_NUM (0x03UL)
#define TWICE	0x02
#define TRIPLE	0x03
#define QUADRUPLE	0x04

#define FOUR_BYTE_BIT_NUM  (0x10UL)

#define TWO_BYTE (0x02UL)
//Panel ini related
#define LDM_COMPATIBLE_STR        "mediatek,mediatek-ldm"
#define LDM_PANEL_INFO_NODE	          "ldm-panel-info"
#define LDM_MISC_INFO_NODE	          "ldm-misc-info"
#define LDM_MSPI_INFO_NODE	          "ldm-mspi-info"
#define LDM_DMA_INFO_NODE	          "ldm-dma-info"
#define LDM_BOOSTING_INFO_NODE	          "ldm-boosting-info"
#define LDM_LED_DEVICE_INFO_NODE      "ldm-led-device-info"
#define LDM_LED_DEVICE_AS3824_INFO_NODE      "ldm-led-device-as3824-info"
#define LDM_LED_DEVICE_NT50585_INFO_NODE      "ldm-led-device-nt50585-info"
#define LDM_LED_DEVICE_IW7039_INFO_NODE      "ldm-led-device-IW7039-info"
#define LDM_LED_DEVICE_MBI6353_INFO_NODE      "ldm-led-device-MBI6353-info"

#define LDM_CUS_SETTING_NODE      "ldm-cus-setting"

#define LD_MAX_HEIGHT (128)
#define LD_MAX_WIDTH (128)

/*
 *###########################
 *#       ldm-panel-info    #
 *###########################
 */
#define PANEL_WIDTH_TAG          "PanelWidth"
#define PANEL_HEIGHT_TAG         "PanelHeight"
#define LED_TYPE_TAG             "LEDType"
#define LDF_WIDTH_TAG            "LDFWidth"
#define LDF_HEIGHT_TAG           "LDFHeight"
#define LED_WIDTH_TAG            "LEDWidth"
#define LED_HEIGHT_TAG           "LEDHeight"
#define LSF_WIDTH_TAG            "LSFWidth"
#define LSF_HEIGHT_TAG           "LSFHeight"
#define EDGE2D_LOCAL_H_TAG      "Edge2D_Local_h"
#define EDGE2D_LOCAL_V_TAG      "Edge2D_Local_v"
#define PANEL_TAG               "PanelHz"
#define MIRROR_PANEL_TAG    "MirrorPanel"
#define SUPPORT_DV_GD       "Support_DV_GD"
/*
 *###########################
 *#       ldm-misc-info    #
 *###########################
 */

#define LDEN_TAG            "LDEn"
#define COMPENSATIONEN_TAG		"Compensation_En"
#define LD_CUS_ALG_MODE_TAG		"LD_cus_alg_mode"
#define OLED_EN_TAG         "OLEDEn"
#define OLED_LSP_EN_TAG     "OLED_LSP_En"
#define OLED_GSP_EN_TAG     "OLED_GSP_En"
#define OLED_HBP_EN_TAG     "OLED_HBP_En"
#define OLED_CRP_EN_TAG     "OLED_CRP_En"
#define OLED_LSPHDR_EN_TAG  "OLED_LSPHDR_En"
#define OLED_YUVHIST_EN_TAG "OLED_Report_En"
#define OLED_LGE_GSR_EN_TAG "OLED_LGE_GSR_En"
#define OLED_LGE_GSR_HBP_EN_TAG "OLED_LGE_GSR_HBP_En"

/*
 *###########################
 *#       ldm-mspi-info    #
 *###########################
 */
#define MSPI_CHANNEL_TAG     "MspiChannel"
#define MSPI_MODE_TAG       "MspiMode"
#define TR_START_TAG        "TrStart"
#define TR_END_TAG          "TrEnd"
#define TB_TAG              "TB"
#define TRW_TAG             "TRW"
#define MSPI_CLK_TAG        "MspiClk"
#define BCLKPOLARITY_TAG    "BClkPolarity"
#define BCLKPHASE_TAG       "BClkPhase"
#define MAX_CLK_TAG         "MAXClk"
#define MSPI_BUFFSIZES_TAG  "MspiBuffSizes"
#define W_BIT_CONFIG_TAG    "WBitConfig"
#define R_BIT_CONFIG_TAG    "RBitConfig"

#define W_BIT_CONFIG_NUM       8
#define R_BIT_CONFIG_NUM       8

/*
 *###########################
 *#       ldm-dma-info    #
 *###########################
 */
#define LDMA_CHANNEL_TAG         "LDMAchannel"
#define LDMA_TRIMODE_TAG        "LDMATrimode"
#define LDMA_CHECK_SUM_MODE_TAG "LDMACheckSumMode"
#define MSPI_HEAD_TAG           "MspiHead"
#define DMA_DELAY_TAG           "DMADelay"
#define CMD_LENGTH_TAG          "cmdlength"
#define BL_WIDTH_TAG            "BLWidth"
#define BL_HEIGHT_TAG           "BLHeight"
#define LED_NUM_TAG             "LedNum"
#define DATA_PACK_MODE_TAG      "DataPackMode"
#define DMA_PACK_LENGTH_TAG     "DMAPackLength"
#define DATA_INVERT_TAG         "DataInvert"
#define DMA_BASE_OFFSET_TAG     "DMABaseOffset"
#define EXT_DATA_TAG            "ExtData"
#define EXT_DATA_LENGTH_TAG     "ExtDataLength"
#define BDAC_NUM_TAG            "BDACNum"
#define BDAC_CMD_LENGTH_TAG     "BDACcmdlength"
#define BDAC_HEAD_TAG           "BDACHead"
#define LDM_VSYNC_WIDTH_TAG     "VsyncWidth"

#define LDM_SAME_TRIG_VSYNC_DMA_TAG     "Same_tri_for_Vsync_and_DMA"
#define LDM_FIRST_TRIG_TAG     "First_trig"
#define LDM_PWM0_PERIOD_TAG     "PWM0_Period"
#define LDM_PWM0_DUTY_TAG     "PWM0_Duty"
#define LDM_PWM0_SHIFT_TAG     "PWM0_Shift"
#define LDM_PWM1_PERIOD_TAG     "PWM1_Period"
#define LDM_PWM1_DUTY_TAG     "PWM1_Duty"
#define LDM_PWM1_SHIFT_TAG     "PWM1_Shift"
#define LDM_CLOSED_12BIT_DUTY_TAG		"Closed_12bit_duty_mode"

#define MSPI_HEADER_NUM       8
#define DMA_DELAY_NUM       4
#define EXT_DATA_NUM        16
#define BDAC_HEADER_NUM       8
#define VSYNC_WIDTH_NUM       4
#define MSPI_BIT_MASK_NUM       8
#define LD10K_INIX_NUM 11
#define PWM_PERIO_NUM       2
#define PWM_DUTY_NUM       2
#define PWM_SHIFT_NUM       2


/*
 *###########################
 *#       ldm-boosting-info    #
 *###########################
 */

#define MAX_POWER_RATIO_TAG         "MaxPowerRatio"
#define OPEN_AREA_DUTY_TAG        "OpenAreaDuty_100"
#define OPEN_AREA_DUTY_ANCHOR_POINT_TAG "OpenAreaDuty_AnchorPoints"
#define DECAY_WEI_TAG           "DecayWei"
#define OPEN_AREA_CURRENT_ANCHOR_POINT_TAG           "OpenAreaCurrentAnchorPoints"
#define OPEN_AREA_CURRENT_TAG          "OpenAreaCurrent"

#define INIT_OPEN_AREA_NUM 20
#define DECAY_WEI_NUM 256

/*
 *################################
 *#       ldm-led-device-info    #
 *################################
 */
#define LEDDEVICE_TYPE_TAG         "LedDevice_Type"
#define LEDDEVICE_VRR_SUPPORT      "LedDevice_VRR_Support"
#define SPI_BITS_TAG			"SPIbits"
#define SPI_CHANNEL_NUM_TAG	"SpiChannelNum"
#define SPI_CHANNEL_LIST_TAG	"SPIChannelList"
#define DUTY_HEADER_BYTE_TAG	"DutyHeaderByte"
#define DUTY_HEADER_TAG	"DutyHeader"
#define DUTY_PER_DATA_BYTE_TAG	"DutyPerDataByte"
#define DUTY_CHECKSUM_BYTE_TAG	"DutyCheckSumByte"
#define CURRENT_HEADER_BYTE_TAG	"CurrentHeaderByte"
#define CURRENT_HEADER_TAG	"CurrentHeader"
#define CURRENT_PER_DATA_BYTE_TAG	"CurrentPerDataByte"
#define CURRENT_CHECKSUM_BYTE_TAG	 "CurrentCheckSumByte"
#define DEVICE_NUM_TAG	"Device_Num"
#define DUTY_DRAM_FORMAT_TAG	"DutyDramFormat"
#define CURRENT_DRAM_FORMAT_TAG	"CurrentDramFormat"
#define MAPPING_TABLE_HEADER_DUTY_TAG	 "Mapping_Table_Header_Duty"
#define MAPPING_TABLE_DUTY_TAG	 "Mapping_Table_Duty"
#define MAPPING_TABLE_CHECKSUM_DUTY_TAG	 "Mapping_Table_CheckSum_Duty"
#define MAPPING_TABLE_EXTEND_DATA_DUTY_TAG	"Mapping_Table_ExtendData_Duty"

#define MAPPING_TABLE_HEADER_CURRENT_TAG	 "Mapping_Table_Header_Current"
#define MAPPING_TABLE_CURRENT_TAG	 "Mapping_Table_Current"
#define MAPPING_TABLE_CHECKSUM_CURRENT_TAG	 "Mapping_Table_CheckSum_Current"
#define MAPPING_TABLE_EXTEND_DATA_CURRENT_TAG	"Mapping_Table_ExtendData_Current"
#define EXTEND_DATA_BYTE_TAG	"ExtendDataByte"

#define DUTY_HEADER_NUM		(0x10)
#define CURRENT_HEADER_NUM	(0x10)
#define CHECK_SUM_NUM_MAX		(0x10)

#define Reg_0x01_to_0x0B_NUM       0x0B
#define Reg_0x0E_to_0x15_NUM       0x08
#define Reg_0x1D_to_0x1F_NUM       0x03
#define Reg_0x00_to_0x1F_NUM       0x20
#define VDAC_BYTE_NUM        2
#define PLL_MULTI_BYTE_NUM       2

#define MBI6353_MASK_NUM		0x03
#define MBI6353_CFG_NUM		0x10
#define MBI6353_DUTY_NUM		0xC0

/*
 *################################
 *#      ldm-led-device-as3824-info       #
 *################################
 */
#define RSENSE_TAG          "Rsense"
#define AS3824_Reg_0x01_to_0x0B_TAG		"AS3824_Reg_0x01_to_0x0B"
#define AS3824_Reg_0x0E_to_0x15_TAG		"AS3824_Reg_0x0E_to_0x15"
#define AS3824_Reg_0x67_TAG		"AS3824_Reg_0x67"

#define AS3824_PWM_Duty_Init_TAG		"AS3824_PWM_Duty_Init"
#define AS3824_PWM_Phase_MBR_OFF_TAG		"AS3824_PWM_Phase_MBR_OFF"
#define AS3824_PWM_Phase_MBR_ON_TAG		"AS3824_PWM_Phase_MBR_ON"
#define AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF_TAG		"AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF"
#define AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON_TAG		"AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON"
#define AS3824_Reg_0x61_0x62_PLL_multi_TAG		"AS3824_Reg_0x61_0x62_PLL_multi"
#define AS3824_BDAC_High_Limit_TAG		"AS3824_BDAC_High_Limit"
#define AS3824_BDAC_Low_Limit_TAG		"AS3824_BDAC_Low_Limit"

/*
 *################################
 *#      ldm-led-device-nt50585-info        #
 *################################
 */
#define NT50585_Reg_0x01_to_0x0B_TAG          "NT50585_Reg_0x01_to_0x0B"
#define NT50585_Reg_0x1D_to_0x1F_TAG		"NT50585_Reg_0x1D_to_0x1F"
#define NT50585_Reg_0x60_TAG		"NT50585_Reg_0x60"
#define NT50585_Reg_0x68_TAG		"NT50585_Reg_0x68"

#define NT50585_PWM_Duty_Init_TAG		"NT50585_PWM_Duty_Init"
#define NT50585_PWM_Phase_MBR_OFF_TAG		"NT50585_PWM_Phase_MBR_OFF"
#define NT50585_PWM_Phase_MBR_ON_TAG		"NT50585_PWM_Phase_MBR_ON"
#define NT50585_Reg_0x14_0x15_IDAC_MBR_OFF_TAG		"NT50585_Reg_0x14_0x15_IDAC_MBR_OFF"
#define NT50585_Reg_0x14_0x15_IDAC_MBR_ON_TAG		"NT50585_Reg_0x14_0x15_IDAC_MBR_ON"
#define NT50585_Reg_0x66_0x67_PLL_multi_TAG		"NT50585_Reg_0x66_0x67_PLL_multi"
#define NT50585_BDAC_High_Limit_TAG		"NT50585_BDAC_High_Limit"
#define NT50585_BDAC_Low_Limit_TAG		"NT50585_BDAC_Low_Limit"

/*
 *################################
 *#       ldm-led-device-IW7039-info        #
 *################################
 */
#define IW7039_Reg_0x000_to_0x01F_TAG          "IW7039_Reg_0x000_to_0x01F"

#define IW7039_PWM_Duty_Init_TAG		"IW7039_PWM_Duty_Init"
#define IW7039_ISET_MBR_OFF_TAG		"IW7039_ISET_MBR_OFF"
#define IW7039_ISET_MBR_ON_TAG		"IW7039_ISET_MBR_ON"
#define IW7039_PWM_Phase_MBR_OFF_TAG		"IW7039_PWM_Phase_MBR_OFF"
#define IW7039_PWM_Phase_MBR_ON_TAG			"IW7039_PWM_Phase_MBR_ON"
#define IW7039_ISET_High_Limit_TAG		"IW7039_ISET_High_Limit"
#define IW7039_ISET_Low_Limit_TAG		"IW7039_ISET_Low_Limit"


/*
 *################################
 *#       ldm-led-device-MBI6353-info        #
 *################################
 */
#define MBI6353_MASK_TAG          "MBI6353_MASK"
#define MBI6353_CFG_TAG          "MBI6353_CFG"
#define MBI6353_CFG_1_TAG          "MBI6353_CFG_1"
#define MBI6353_PWM_Duty_Init_TAG		"MBI6353_PWM_Duty_Init"

/*
 *################################
 *#       ldm-cus-setting        #
 *################################
 */

//=============================================================================
// Type and Structure Declaration
//=============================================================================
enum drm_en_ld_type {
	E_LD_EDGE_TB_TYPE = 0,
	E_LD_EDGE_LR_TYPE = 1,
	E_LD_DIRECT_TYPE  = 2,
	E_LD_LOCAL_TYPE   = 3,
	E_LD_10K_TYPE     = 4,
	E_LD_TYPE_NUM,
	E_LD_TYPE_MAX = E_LD_TYPE_NUM,
};

enum drm_en_ld_device_type {
	E_LD_DEVICE_UNSUPPORT = 0,
	E_LD_DEVICE_AS3824 = 1,
	E_LD_DEVICE_NT50585,
	E_LD_DEVICE_IW7039,
	E_LD_DEVICE_MCU,
	E_LD_DEVICE_CUS,
	E_LD_DEVICE_MBI6353,
	E_LD_DEVICE_TL01,
	E_LD_DEVICE_TL02,
	E_LD_DEVICE_TL01_LPC1114,
	E_LD_DEVICE_TL03,
	E_LD_DEVICE_TL04,
	E_LD_DEVICE_TL05,
	E_LD_DEVICE_TL06,
	E_LD_DEVICE_TL07,

};

enum drm_en_ld_boosting_mode {
	E_BOOSTING_OFF = 0,
	E_BOOSTING_FIXED_CURRENT = 1,
	E_BOOSTING_VARIABLE_CURRENT  = 2,
	E_BOOSTING_NUM,
};

enum drm_en_ld_cus_alg_mode {
	E_CUS_OFF = 0,
	E_CUS_LD_AFTER = 1,
	E_CUS_LD_ALL  = 2,
	E_CUS_GD_ALL,
	E_CUS_MAX = E_CUS_GD_ALL,
};

enum drm_en_ld_ver {
	E_LD_VER1 = 1,
	E_LD_VER2 = 2,
	E_LD_VER3,
	E_LD_VER4,
	E_LD_VER5,
	E_LD_VER6,
	E_LD_VER_NUM,
	E_LD_VER_MAX = E_LD_VER_NUM,
};

struct drm_st_ld_panel_info {
	__u16 u16PanelWidth;
	__u16 u16PanelHeight;
	enum drm_en_ld_type eLEDType;
	__u16 u16LDFWidth;
	__u16 u16LDFHeight;
	__u16 u16LEDWidth;
	__u16 u16LEDHeight;
	__u8 u8LSFWidth;
	__u8 u8LSFHeight;
	__u8 u8Edge2D_Local_h;
	__u8 u8Edge2D_Local_v;
	__u8 u8PanelHz;
	__u8 u8MirrorPanel;
	__u8 u8BacklightVer;
	__u16 u16RefWidth;
	__u16 u16RefHeight;
	s8 s8XsIni[LD10K_INIX_NUM];
	bool bSupport_DV_GD;
};

struct drm_st_ld_misc_info {
	bool bLDEn;
	bool bCompensationEn;
	enum drm_en_ld_cus_alg_mode eLD_cus_alg_mode;
	bool bOLEDEn;
	bool bOLED_LSP_En;
	bool bOLED_GSP_En;
	bool bOLED_HBP_En;
	bool bOLED_CRP_En;
	bool bOLED_LSPHDR_En;
	bool bOLED_YUVHist_En;
	bool bOLED_LGE_GSR_En;
	bool bOLED_LGE_GSR_HBP_En;
};

struct drm_st_ld_mspi_info {
	__u8 u8MspiChannel;
	__u8 u8MspiMode;
	__u8 u8TrStart;
	__u8 u8TrEnd;
	__u8 u8TB;
	__u8 u8TRW;
	__u32 u32MspiClk;
	__u16 BClkPolarity;
	__u16 BClkPhase;
	__u32 u32MAXClk;
	__u8 u8MspiBuffSizes;
	__u8 u8WBitConfig[W_BIT_CONFIG_NUM];
	__u8 u8RBitConfig[R_BIT_CONFIG_NUM];
};

struct drm_st_ld_dma_info {
	__u8 u8LDMAchannel;
	__u8 u8LDMATrimode;
	__u8 u8LDMACheckSumMode;
	__u16 u16MspiHead[MSPI_HEADER_NUM];
	__u16 u16DMADelay[DMA_DELAY_NUM];
	__u8 u8cmdlength;
	__u8 u8BLWidth;
	__u8 u8BLHeight;
	__u16 u16LedNum;
	__u8 u8DataPackMode;
	__u16 u16DMAPackLength;
	__u8 u8DataInvert;
	__u32 u32DMABaseOffset;
	__u8 u8ExtData[EXT_DATA_NUM];
	__u8 u8ExtDataLength;
	__u16 u16BDACNum;
	__u8 u8BDACcmdlength;
	__u16 u16BDACHead[BDAC_HEADER_NUM];
	__u16 u16VsyncWidth[VSYNC_WIDTH_NUM];
	__u8 u8Same_tri_for_Vsync_and_DMA;
	__u16 u16First_trig;
	__u16 u16PWM0_Period[PWM_PERIO_NUM];
	__u16 u16PWM0_Duty[PWM_DUTY_NUM];
	__u16 u16PWM0_Shift[PWM_SHIFT_NUM];
	__u16 u16PWM1_Period[PWM_PERIO_NUM];
	__u16 u16PWM1_Duty[PWM_DUTY_NUM];
	__u16 u16PWM1_Shift[PWM_SHIFT_NUM];
	bool bClosed_12bit_duty_mode;
};

struct drm_st_ld_boosting_info {
	__u8 u8MaxPowerRatio;
	__u16 u16OpenAreaDuty_100[INIT_OPEN_AREA_NUM];
	__u16 u16OpenAreaDuty_AnchorPoints[INIT_OPEN_AREA_NUM];
	__u16 u16DecayWei[DECAY_WEI_NUM];
	__u16 u16OpenAreaCurrentAnchorPoints[INIT_OPEN_AREA_NUM];
	__u16 u16OpenAreaCurrent[INIT_OPEN_AREA_NUM];
};

struct drm_st_ld_led_device_info {
	enum drm_en_ld_device_type eLEDDeviceType;
	bool bLedDevice_VRR_Support;
	__u8 u8SPIbits;
	__u8 u8DutyHeader[DUTY_HEADER_NUM];
	__u8 u8CurrentHeader[CURRENT_HEADER_NUM];
	__u8 u8SpiChannelList[LD_MAX_WIDTH * LD_MAX_HEIGHT * 2];
	__u8 u8SpiChannelNum;
	__u8 u8DutyHeaderByte;
	__u8 u8DutyPerDataByte;
	__u8 u8DutyCheckSumByte;
	__u8 u8CurrentHeaderByte;
	__u8 u8CurrentPerDataByte;
	__u8 u8CurrentCheckSumByte;
	__u8 u8Device_Num[MSPI_MAX_NUM];
	__u16 u16DutyDramFormat[DMA_INFO_NUM * MSPI_MAX_NUM];
	__u16 u16CurrentDramFormat[DMA_INFO_NUM * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_Header_Duty[DUTY_HEADER_NUM * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_Duty[LD_MAX_WIDTH * LD_MAX_HEIGHT * 2];
	__u16 u16Mapping_Table_CheckSum_Duty[CHECK_SUM_NUM_MAX * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_ExtendData_Duty[DEVICE_MAX_NUM * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_Header_Current[CURRENT_HEADER_NUM * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_Current[LD_MAX_WIDTH * LD_MAX_HEIGHT * 2];
	__u16 u16Mapping_Table_CheckSum_Current[CHECK_SUM_NUM_MAX * MSPI_MAX_NUM];
	__u16 u16Mapping_Table_ExtendData_Current[DEVICE_MAX_NUM * MSPI_MAX_NUM];
	__u16 u16ExtendDataByte[MSPI_MAX_NUM];
};

struct drm_st_ld_led_device_as3824_info {
	__u16 u16Rsense;
	__u8 u8AS3824_Reg_0x01_to_0x0B[Reg_0x01_to_0x0B_NUM];
	__u8 u8AS3824_Reg_0x0E_to_0x15[Reg_0x0E_to_0x15_NUM];
	__u8 u8AS3824_Reg_0x67;
	__u16 u16AS3824_PWM_Duty_Init;
	__u16 u16AS3824_PWM_Phase_MBR_OFF[LD_MAX_HEIGHT];
	__u16 u16AS3824_PWM_Phase_MBR_ON[LD_MAX_HEIGHT];
	__u8 u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF[VDAC_BYTE_NUM];
	__u8 u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON[VDAC_BYTE_NUM];
	__u8 u8AS3824_Reg_0x61_0x62_PLL_multi[PLL_MULTI_BYTE_NUM];
	__u8 u8AS3824_BDAC_High_Limit;
	__u8 u8AS3824_BDAC_Low_Limit;
};

struct drm_st_ld_led_device_nt50585_info {
	__u8 u8NT50585_Reg_0x01_to_0x0B[Reg_0x01_to_0x0B_NUM];
	__u8 u8NT50585_Reg_0x1D_to_0x1F[Reg_0x1D_to_0x1F_NUM];
	__u8 u8NT50585_Reg_0x60;
	__u8 u8NT50585_Reg_0x68;
	__u16 u16NT50585_PWM_Duty_Init;
	__u16 u16NT50585_PWM_Phase_MBR_OFF[LD_MAX_HEIGHT];
	__u16 u16NT50585_PWM_Phase_MBR_ON[LD_MAX_HEIGHT];
	__u8 u8NT50585_Reg_0x14_0x15_IDAC_MBR_OFF[VDAC_BYTE_NUM];
	__u8 u8NT50585_Reg_0x14_0x15_IDAC_MBR_ON[VDAC_BYTE_NUM];
	__u8 u8NT50585_Reg_0x66_0x67_PLL_multi[PLL_MULTI_BYTE_NUM];
	__u16 u16NT50585_BDAC_High_Limit;
	__u16 u16NT50585_BDAC_Low_Limit;
};

struct drm_st_ld_led_device_iw7039_info {
	__u16 u16IW7039_Reg_0x000_to_0x01F[Reg_0x00_to_0x1F_NUM];
	__u16 u16IW7039_PWM_Duty_Init;
	__u16 u16IW7039_ISET_MBR_OFF;
	__u16 u16IW7039_ISET_MBR_ON;
	__u16 u16IW7039_PWM_Phase_MBR_OFF[LD_MAX_HEIGHT];
	__u16 u16IW7039_PWM_Phase_MBR_ON[LD_MAX_HEIGHT];
	__u16 u16IW7039_ISET_High_Limit;
	__u16 u16IW7039_ISET_Low_Limit;
};

struct drm_st_ld_led_device_mbi6353_info {
	__u8 u8MBI6353_Mask[MBI6353_MASK_NUM * 2];
	__u8 u8MBI6353_Config[MBI6353_CFG_NUM * 2];
	__u8 u8MBI6353_Config_1[MBI6353_CFG_NUM * 2];
	__u16 u16MBI6353_PWM_Duty_Init;
};

struct drm_st_ld_qmap_info {
	uint16_t node_num[MTK_DRM_PQMAP_NUM_MAX];
	uint32_t buf_size;
	void *node_buf;
};

struct mtk_ld_context {
	__u32 u8LDMSupport;
	__u64 u64LDM_phyAddress;
	__u32 u32LDM_mmapsize;
	__u32 u32Cpu_emi0_base;
	__u8 u8LDM_Version;
	__u8 u8RENDER_QMAP_Version;
	bool bLDM_uboot;
	bool bDMA_SPI_Test;
	struct drm_st_ld_panel_info ld_panel_info;
	struct drm_st_ld_misc_info ld_misc_info;
	struct drm_st_ld_mspi_info ld_mspi_info;
	struct drm_st_ld_dma_info  ld_dma_info;
	struct drm_st_ld_boosting_info  ld_boosting_info;
	struct drm_st_ld_led_device_info ld_led_device_info;
	struct drm_st_ld_led_device_as3824_info  ld_led_device_as3824_info;
	struct drm_st_ld_led_device_nt50585_info ld_led_device_nt50585_info;
	struct drm_st_ld_led_device_iw7039_info ld_led_device_iw7039_info;
	struct drm_st_ld_led_device_mbi6353_info ld_led_device_mbi6353_info;
	struct drm_st_ld_qmap_info ld_qmap_info;
};

int mtk_ld_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val,
	int prop_index);

int mtk_ld_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val,
	int prop_index);

int readDTB2LDMPrivate(struct mtk_ld_context *pctx);

int mtk_ldm_init(struct device *dev);
int mtk_ldm_set_led_check(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_led_check *ldm_led_check);

int mtk_tv_drm_set_ldm_led_check_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv);


int mtk_ldm_set_black_insert(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_black_insert_enable *ldm_black_insert_en);

int mtk_tv_drm_set_ldm_black_insert_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv);

int mtk_ldm_set_pq_param(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_pq_param *ldm_pq_param);

int mtk_tv_drm_set_ldm_pq_param_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv);

int mtk_ldm_set_ldm_VCOM_enable(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_VCOM_enable *ldm_VCOM_en);

int mtk_tv_drm_set_ldm_VCOM_enable_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv);




int mtk_ldm_suspend(struct device *dev);
int mtk_ldm_resume(struct device *dev);
int mtk_ldm_re_init(void);
uint32_t mtk_ldm_get_support_type(void);
int mtk_ldm_set_backlight_off(bool bEn);

void mtk_drm_ldm_create_sysfs(struct platform_device *pdev);

void mtk_drm_ldm_remove_sysfs(struct platform_device *pdev);

int mtk_tv_drm_ldm_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv);

#endif
