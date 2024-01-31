/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __UAPI_MTK_M_SRCCAP_H__
#define __UAPI_MTK_M_SRCCAP_H__

#include <linux/types.h>
#ifndef __KERNEL__
#include <stdint.h>
#include <stdbool.h>
#endif

/* ============================================================================================== */
/* ---------------------------------------- Metadata Tag ---------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------ Metadata Tag Version ------------------------------------ */
/* ============================================================================================== */
#define META_SRCCAP_FRAME_INFO_VERSION		(1)
#define META_SRCCAP_SVP_INFO_VERSION		(1)
#define META_SRCCAP_COLOR_INFO_VERSION		(1)
#define META_SRCCAP_HDR_PKT_VERSION		(1)
#define META_SRCCAP_HDR_VSIF_PKT_VERSION	(1)
#define META_SRCCAP_HDR_EMP_PKT_VERSION		(1)
#define META_SRCCAP_DV_INFO_VERSION		(2)
#define META_SRCCAP_DIP_POINT_INFO_VERSION		(1)
#define META_SRCCAP_SPD_PKT_VERSION		(1)

#define META_SRCCAP_HDMIRX_AVI_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_GCP_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_SPD_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_VSIF_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_HDR_VERSION		(1)
#define META_SRCCAP_HDMIRX_AUI_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_MPG_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_VS_EMP_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_DSC_EMP_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_VRR_EMP_PKT_VERSION		(1)
#define META_SRCCAP_HDMIRX_DHDR_EMP_PKT_VERSION		(1)

/* ============================================================================================== */
/* ------------------------------------ Metadata Tag Size ------------------------------------ */
/* ============================================================================================== */
#define META_SRCCAP_HDMIRX_AVI_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_GCP_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_SPD_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE		(4)
#define META_SRCCAP_HDMIRX_HDR_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_AUI_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_MPG_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE		(24)
#define META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE		(6)
#define META_SRCCAP_HDMIRX_VRR_EMP_PKT_MAX_SIZE		(1)
#define META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE		(32)

#define META_SRCCAP_HDMIRX_PKT_MAX_SIZE		(31)
#define META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE	META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE

#define META_SRCCAP_BUF_PLANE_MAX_SIZE (3)
/* ============================================================================================== */
/* -------------------------------------- Metadata Content -------------------------------------- */
/* ============================================================================================== */
/* member enum */
enum m_input_source {
	SOURCE_NONE = 0,
	SOURCE_HDMI = 10,	/* 10   HDMI input */
	SOURCE_HDMI2,		/* 11   HDMI2 input */
	SOURCE_HDMI3,		/* 12   HDMI3 input */
	SOURCE_HDMI4,		/* 13   HDMI4 input */
	SOURCE_CVBS = 20,	/* 20   CVBS input */
	SOURCE_CVBS2,		/* 21   CVBS2 input */
	SOURCE_CVBS3,		/* 22   CVBS3 input */
	SOURCE_CVBS4,		/* 23   CVBS4 input */
	SOURCE_CVBS5,		/* 24   CVBS5 input */
	SOURCE_CVBS6,		/* 25   CVBS6 input */
	SOURCE_CVBS7,		/* 26   CVBS7 input */
	SOURCE_CVBS8,		/* 27   CVBS8 input */
	SOURCE_SVIDEO = 30,	/* 30   SVIDEO input */
	SOURCE_SVIDEO2,		/* 31   SVIDEO2 input */
	SOURCE_SVIDEO3,		/* 32   SVIDEO3 input */
	SOURCE_SVIDEO4,		/* 33   SVIDEO4 input */
	SOURCE_YPBPR = 40,	/* 40   YPBPR input */
	SOURCE_YPBPR2,		/* 41   YPBPR2 input */
	SOURCE_YPBPR3,		/* 42   YPBPR3 input */
	SOURCE_VGA = 50,	/* 50   VGA input */
	SOURCE_VGA2,		/* 51   VGA2 input */
	SOURCE_VGA3,		/* 52   VGA3 input */
	SOURCE_ATV = 60,	/* 60   ATV input */
	SOURCE_SCART = 70,	/* 70   SCART input */
	SOURCE_SCART2,		/* 71   SCART2 input */
	SOURCE_DVI = 80,	/* 80   DVI input */
	SOURCE_DVI2,		/* 81   DVI2 input */
	SOURCE_DVI3,		/* 82   DVI3 input */
	SOURCE_DVI4,		/* 83   DVI4 input */
};

enum m_memout_path {
	PATH_IPDMA_0 = 0,
	PATH_IPDMA_1,
};

enum m_color_format {
	FORMAT_RGB = 0,
	FORMAT_YUV420,
	FORMAT_YUV422,
	FORMAT_YUV444,
};

enum m_vrr_type {
	TYPE_NVRR = 0,
	TYPE_VRR,
	TYPE_CVRR,
};

enum m_freesync_type {
	TYPE_NFREESYNC = 0,
	TYPE_FREESYNC,
	TYPE_FREESYNC_PREMIUM,
	TYPE_FREESYNC_PREMIUM_PRO,
};

enum m_hdmi_color_format_change_ID {
	M_HDMI_COLOR_SRC_TIMING,                   // timing_monitor_task
	M_HDMI_COLOR_DV_DESCRB,                    // mtk_dv_descrb_set_mute_and_freeze
	M_HDMI_COLOR_ID_RESERVED,
};

enum m_hdmi_color_format {
	M_HDMI_COLOR_RGB,	/* HDMI RGB 444 Color Format */
	M_HDMI_COLOR_YUV_422,	/* HDMI YUV 422 Color Format */
	M_HDMI_COLOR_YUV_444,	/* HDMI YUV 444 Color Format */
	M_HDMI_COLOR_YUV_420,	/* HDMI YUV 420 Color Format */
	M_HDMI_COLOR_RESERVED,	/* Reserve */
	M_HDMI_COLOR_DEFAULT = M_HDMI_COLOR_RGB,	/* Default setting */
	M_HDMI_COLOR_UNKNOWN = 7,	/* Unknown Color Format */
};

enum m_hdmi_color_range {
	M_HDMI_COLOR_RANGE_DEFAULT,
	M_HDMI_COLOR_RANGE_LIMIT,   /* HDMI RGB Limited Range (16-235) */
	M_HDMI_COLOR_RANGE_FULL,    /* HDMI Full Range (0-255) */
	M_HDMI_COLOR_RANGE_RESERVED
};

enum m_hdmi_colorimetry_format {
	M_HDMI_COLORIMETRY_NO_DATA = 0,
	M_HDMI_COLORIMETRY_SMPTE_170M,
	M_HDMI_COLORIMETRY_ITU_R_BT_709,
	M_HDMI_COLORIMETRY_EXTENDED = 3,

	M_HDMI_EXT_COLORIMETRY_XVYCC601 = 0x8,		/* xvycc 601 */
	M_HDMI_EXT_COLORIMETRY_XVYCC709,		/* xvycc 709 */
	M_HDMI_EXT_COLORIMETRY_SYCC601,			/* sYCC 601 */
	M_HDMI_EXT_COLORIMETRY_ADOBEYCC601,		/* Adobe YCC 601 */
	M_HDMI_EXT_COLORIMETRY_ADOBERGB,		/* Adobe RGB */
	M_HDMI_EXT_COLORIMETRY_BT2020YcCbcCrc,		/* ITU-F BT.2020 YcCbcCrc */
	M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr = 0xe,	/* ITU-R BT.2020 RGB or YCbCr */
	M_HDMI_EXT_COLORIMETRY_ADDITIONAL = 0xf,	/* ITU-R BT.2020 RGB or YCbCr */

	M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_D65 = 0x10,
	M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_THEATER = 0x11,
	M_HDMI_EXT_COLORIMETRY_DEFAULT = M_HDMI_EXT_COLORIMETRY_XVYCC601,

	M_HDMI_COLORMETRY_UNKNOWN = 0xffff,		/* Unknown */
};

enum m_hdmi_pixel_repetition {
	M_HDMI_PIXEL_REPETITION_1X = 0,
	M_HDMI_PIXEL_REPETITION_2X,
	M_HDMI_PIXEL_REPETITION_3X,
	M_HDMI_PIXEL_REPETITION_4X,
	M_HDMI_PIXEL_REPETITION_5X,
	M_HDMI_PIXEL_REPETITION_6X,
	M_HDMI_PIXEL_REPETITION_7X,
	M_HDMI_PIXEL_REPETITION_8X,
	M_HDMI_PIXEL_REPETITION_9X,
	M_HDMI_PIXEL_REPETITION_10X,
	M_HDMI_PIXEL_REPETITION_RESERVED,
};

enum m_hdmi_ar_type {
	/* Active Format Aspect Ratio - AFAR */
	/* M_HDMI_AF_AR_Not_Present   = -1, */
	/* IF0[11..8] AFD not present, or AFD not yet found */
	M_HDMI_AF_AR_Reserve_0  = 0x00, /* IF0[11..8] 0000, Reserved */
	M_HDMI_AF_AR_Reserve_1  = 0x01, /* IF0[11..8] 0001, Reserved */
	M_HDMI_AF_AR_16x9_Top = 0x02,	/* IF0[11..8] 0010, box 16:9 (top). */
	M_HDMI_AF_AR_14x9_Top = 0x03,	/* IF0[11..8] 0011, box 14:9 (top). */
	M_HDMI_AF_AR_GT_16x9 = 0x04,	/* IF0[11..8] 0100, box >16:9 (centre) */
	M_HDMI_AF_AR_Reserve_5  = 0x05,	/* IF0[11..8] 0101, Reserved */
	M_HDMI_AF_AR_Reserve_6  = 0x06,	/* IF0[11..8] 0110, Reserved */
	M_HDMI_AF_AR_Reserve_7  = 0x07,	/* IF0[11..8] 0111, Reserved */
	M_HDMI_AF_AR_SAME   = 0x08,	/* IF0[11..8] 1000, same as picture */
	M_HDMI_AF_AR_4x3_C  = 0x09,	/* IF0[11..8] 1001, 4:3 Center */
	M_HDMI_AF_AR_16x9_C = 0x0A,	/* IF0[11..8] 1010, 16:9 Center */
	M_HDMI_AF_AR_14x9_C = 0x0B,	/* IF0[11..8] 1011, 14:9 Center */
	M_HDMI_AF_AR_Reserve_12   = 0x0C,	/* IF0[11..8] 1100, Reserved. */
	M_HDMI_AF_AR_4x3_with_14x9_C  = 0x0D,
				/* IF0[11..8] 1101, 4:3 with shoot and protect 14:9 centre. */
	M_HDMI_AF_AR_16x9_with_14x9_C = 0x0E,
				/* IF0[11..8] 1110, 16:9 with shoot and protect 14:9 centre. */
	M_HDMI_AF_AR_16x9_with_4x3_C = 0x0F,
				/* IF0[11..8] 1111, 16:9 with shoot and protect 4:3 centre. */
	/* Picture Aspect Ratio - PAR */
	M_HDMI_Pic_AR_NODATA = 0x00,	 /* IF0[13..12] 00 */
	M_HDMI_Pic_AR_4x3	= 0x10,	 /* IF0[13..12] 01, 4:3 */
	M_HDMI_Pic_AR_16x9   = 0x20,	 /* IF0[13..12] 10, 16:9 */
	M_HDMI_Pic_AR_RSV	= 0x30,	 /* IF0[13..12] 11, reserved */
};

enum m_hdmi_color_depth {
	M_HDMI_COLOR_DEPTH_6_BIT = 0,
	M_HDMI_COLOR_DEPTH_8_BIT = 1,
	M_HDMI_COLOR_DEPTH_10_BIT = 2,
	M_HDMI_COLOR_DEPTH_12_BIT = 3,
	M_HDMI_COLOR_DEPTH_16_BIT = 4,
	M_HDMI_COLOR_DEPTH_UNKNOWN = 5,
};

enum m_dv_dma_status {
	M_DV_DMA_STATUS_DISABLE = 0,
	M_DV_DMA_STATUS_ENABLE_FB,
	M_DV_DMA_STATUS_ENABLE_FBL,
	M_DV_DMA_STATUS_MAX
};

enum m_dv_interface {
	M_DV_INTERFACE_NONE = 0,
	M_DV_INTERFACE_SINK_LED,
	M_DV_INTERFACE_SOURCE_LED_RGB,
	M_DV_INTERFACE_SOURCE_LED_YUV,
	M_DV_INTERFACE_DRM_SOURCE_LED_RGB,
	M_DV_INTERFACE_DRM_SOURCE_LED_YUV,
	M_DV_INTERFACE_FORM_1,
	M_DV_INTERFACE_FORM_2_RGB,
	M_DV_INTERFACE_FORM_2_YUV,
	M_DV_INTERFACE_MAX
};

enum m_dv_crc_state {
	M_DV_CRC_STATE_OK = 0,
	M_DV_CRC_STATE_FAIL_TRANSITION,	/* freeze */
	M_DV_CRC_STATE_FAIL_STABLE,	/* mute */

	M_DV_CRC_STATE_MAX
};

/* m_pqu_pq.h */
#ifndef E_COMPONENT3_TAG_START
#define E_COMPONENT3_TAG_START 0x00030000
enum EN_COMPONENT3_TAG { /* range 0x00030000 ~ 0x0003FFFF */
	M_PQ_DISPLAY_MDW_CTRL_META_TAG = E_COMPONENT3_TAG_START,
	M_PQ_DISPLAY_IDR_CTRL_META_TAG,
	M_PQ_DISPLAY_B2R_CTRL_META_TAG,
	M_PQ_COMMON_STREAM_INFO_TAG,
	M_PQ_COMMON_DEBUG_INFO_TAG,
	M_PQ_COMMON_QBUF_INFO_TAG,
	M_PQ_DISPLAY_FLOW_CTRL_META_TAG,
	M_PQ_PQMAP_SETTING_META_TAG,
	M_PQ_QUEUE_EXT_INFO_TAG,
	M_PQ_DISPLAY_NON_LINEAR_SCALING_TAG,
	M_PQ_DV_HDMI_INFO_TAG,
	M_PQ_DV_DEBUG_INFO_TAG,
	M_PQ_PQPARAM_TAG,
	M_PQ_DISPLAY_WM_META_TAG,
	M_PQ_BBD_INFO_TAG,
	M_PQ_DV_INFO_TAG,
	M_PQ_DV_OUTPUT_FRAME_INFO_TAG,
	M_PQ_DV_CORE_PARAM_TAG,
	M_PQ_DISPLAY_FRC_SCALING_TAG,
	M_PQ_THERMAL_INFO_TAG,
	M_PQ_AIPQ_FRAME_INFO_META_TAG,
	M_PQ_B2R_FILM_GRAIN_TAG,
	M_PQ_DIPMAP_SETTING_META_TAG,
	M_PQ_CUSTOMER1_TAG,
	M_PQ_FRM_STATISTIC_TAG,
	M_PQ_FORCEP_INFO_TAG,
	M_PQ_CALMAN_INFO_TAG,
	M_PQ_CFD_SHM_GAMMA_INFO_TAG,
	M_PQ_CFD_SHM_3DLUT_INFO_TAG,
	M_PQ_CFD_SHM_HDR_CONF_INFO_TAG,
	M_PQ_CFD_SHM_SDR_PIC1,
	M_PQ_CFD_SHM_HDR_PIC1,
	M_PQ_CFD_SHM_HLG_PIC1,
	M_PQ_CFD_SHM_WIN0,
	M_PQ_CFD_SHM_WIN1,
	M_PQ_CFD_SHM_WIN2,
	M_PQ_CFD_SHM_WIN3,
	M_PQ_CFD_SHM_WIN4,
	M_PQ_CFD_SHM_WIN5,
	M_PQ_CFD_SHM_WIN6,
	M_PQ_CFD_SHM_WIN7,
	M_PQ_CFD_SHM_WIN8,
	M_PQ_CFD_SHM_WIN9,
	M_PQ_CFD_SHM_WIN10,
	M_PQ_CFD_SHM_WIN11,
	M_PQ_CFD_SHM_WIN12,
	M_PQ_CFD_SHM_WIN13,
	M_PQ_CFD_SHM_WIN14,
	M_PQ_CFD_SHM_WIN15,
	E_COMPONENT3_TAG_MAX,
};
#endif

/* m_srccap.h */
#ifndef E_COMPONENT7_TAG_START
#define E_COMPONENT7_TAG_START 0x00070000
enum EN_COMPONENT7_TAG { /* range 0x00070000 ~ 0x0007FFFF */
	META_SRCCAP_FRAME_INFO_TAG = E_COMPONENT7_TAG_START,
	META_SRCCAP_SVP_INFO_TAG,
	META_SRCCAP_COLOR_INFO_TAG,
	META_SRCCAP_HDR_PKT_TAG,
	META_SRCCAP_HDR_VSIF_PKT_TAG,
	META_SRCCAP_HDR_EMP_PKT_TAG,
	META_SRCCAP_DIP_POINT_INFO_TAG,
	META_SRCCAP_SPD_PKT_TAG,
	META_SRCCAP_HDMIRX_AVI_PKT_TAG,
	META_SRCCAP_HDMIRX_GCP_PKT_TAG,
	META_SRCCAP_HDMIRX_SPD_PKT_TAG,
	META_SRCCAP_HDMIRX_VSIF_PKT_TAG,
	META_SRCCAP_HDMIRX_HDR_TAG,
	META_SRCCAP_HDMIRX_AUI_PKT_TAG,
	META_SRCCAP_HDMIRX_MPG_PKT_TAG,
	META_SRCCAP_HDMIRX_VS_EMP_PKT_TAG,
	META_SRCCAP_HDMIRX_DSC_EMP_PKT_TAG,
	META_SRCCAP_HDMIRX_VRR_EMP_PKT_TAG,
	META_SRCCAP_HDMIRX_DHDR_EMP_PKT_TAG,
	E_COMPONENT7_TAG_MAX,
};
#endif

enum srccap_swmode_buf_type {
	SRCCAP_SW_SINGLE_BUFFER = 0,
	SRCCAP_SW_MULTI_BUFFER,
	SRCCAP_SW_MULTI_BUFFER_MAX,
};

enum srccap_avd_video_standard_type {
	SRCCAP_AVD_STD_PAL_BGHI	= 0x00, // Video standard PAL BGHI
	SRCCAP_AVD_STD_NTSC_M	  = 0x01, // Video standard NTSC M
	SRCCAP_AVD_STD_SECAM	   = 0x02, // Video standard SECAM
	SRCCAP_AVD_STD_NTSC_44	 = 0x03, // Video standard NTSC 44
	SRCCAP_AVD_STD_PAL_M	   = 0x04, // Video standard PAL M
	SRCCAP_AVD_STD_PAL_N	   = 0x05, // Video standard PAL N
	SRCCAP_AVD_STD_PAL_60	  = 0x06, // Video standard PAL 60
	SRCCAP_AVD_STD_NOTSTANDARD = 0x07, // NOT Video standard
	SRCCAP_AVD_STD_AUTO		= 0x08, // Video standard AUTO
	SRCCAP_AVD_STD_MAX				 // Max Number
};

/* member struct */
struct m_time_val {
	__u64 tv_sec;
	__u64 tv_usec;
};

enum m_memout_format {
	M_MEMOUT_FORMAT_YUV420 = 0,
	M_MEMOUT_FORMAT_YUV422 = 2,
	M_MEMOUT_FORMAT_YUV444 = 4,
	M_MEMOUT_FORMAT_RGB = 11,
	M_MEMOUT_FORMAT_ARGB = 12,
	M_MEMOUT_FORMAT_ABGR = 13,
	M_MEMOUT_FORMAT_RGBA = 14,
	M_MEMOUT_FORMAT_BGRA = 15,
};

struct m_memout_frame_capturing_info {
	enum m_memout_format fmt;
	__u8 bitmode[META_SRCCAP_BUF_PLANE_MAX_SIZE];
	__u8 ce[META_SRCCAP_BUF_PLANE_MAX_SIZE];
	bool argb;
	bool pack;
	__u8 num_planes;
};

/* metadata struct */
struct meta_srccap_frame_info {
	__u32 frame_id;
	enum m_input_source source;
	enum m_memout_path path;
	bool bypass_ipdma;
	__u8 w_index;
	enum m_color_format format;
	__u16 x;
	__u16 y;
	__u16 width;
	__u16 height;
	__u16 scaled_width;
	__u16 scaled_height;
	__u32 frameratex1000;
	bool interlaced;
	__u16 h_total;
	__u16 v_total;
	__u8 pixels_aligned;
	__u64 timestamp;
	bool mute;
	enum m_vrr_type vrr_type;
	struct m_time_val ktimes;
	__u32 refresh_rate;
	enum m_dv_crc_state dv_crc_mute;
	bool dv_game_mode;
	__u32 frame_pitch[META_SRCCAP_BUF_PLANE_MAX_SIZE];
	__u32 hoffset;
	__u8 field; /* 1:top 2:bottom */
	bool isFMM;
	enum srccap_swmode_buf_type sw_buffer_mode; /* 0: single 1:multi */
	__u8 vs_pulse_width;
	__u32 fdet_vtt0;
	__u32 fdet_vtt1;
	__u32 fdet_vtt2;
	__u32 fdet_vtt3;
	__u16 org_size_width;
	__u16 org_size_height;
	enum srccap_avd_video_standard_type vd_video_standard_type;
	__u16 crop_x;
	__u16 crop_y;
	__u16 crop_width;
	__u16 crop_height;
	struct m_memout_frame_capturing_info color_fmt_info;
	__u8 buf_cnt_id;
	__u16 dev_id;
	enum m_input_source input_port;
};

struct meta_srccap_svp_info {
	__u8 aid;
	__u32 pipelineid;
};

struct meta_srccap_color_info {
	__u8 u8DataFormat; /*refer enum EN_PQ_CFD_DATA_FORMAT define in m_pq.h*/
	__u8 u8DataRange; /*refer enum EN_PQ_CFD_COLOR_RANGE define in m_pq.h*/
	__u8 u8BitDepth;
	__u8 u8ColorPrimaries; /*refer enum EN_PQ_CFD_CFIO_CP define in m_pq.h*/
	__u8 u8TransferCharacteristics; /*refer enum EN_PQ_CFD_CFIO_TR define in m_pq.h*/
	__u8 u8MatrixCoefficients; /*refer enum EN_PQ_CFD_CFIO_MC define in m_pq.h*/
	__u8 u8HdrType; /*refer enum EN_PQ_CFD_HDR_MODE define in m_pq.h*/
	__u32 u32SamplingMode;
	__u8 u8Source_Format;
	__u8 u8IPDMA_Format;
} __attribute__((packed));

struct meta_srccap_hdr_pkt {
	__u16 u16Version;
	__u16 u16Size;
	__u8 u8EOTF;
	__u8 u8Static_Metadata_ID;
	__u16 u16Display_Primaries_X[3];
	__u16 u16Display_Primaries_Y[3];
	__u16 u16White_Point_X;
	__u16 u16White_Point_Y;
	__u16 u16Max_Display_Mastering_Luminance;
	__u16 u16Min_Display_Mastering_Luminance;
	__u16 u16Maximum_Content_Light_Level;
	__u16 u16Maximum_Frame_Average_Light_Level;
	__u8 u8Version;
	__u8 u8Length;
} __attribute__((packed));

struct meta_srccap_spd_pkt {
	bool native_color_space_active;
	bool brightness_control_active;
	bool ld_disable_control;
	bool sRGB_EOTF_active;
	bool BT709_EOTF_active;
	bool gamma_22_EOTF_active;
	bool gamma_26_EOTF_active;
	bool PQ_EOTF_active;
	__u32 brightness_control;
	bool bRet;
	enum m_freesync_type freesync_type;
};

struct meta_srccap_hdr_vsif_pkt {
	__u16 u16Version;	/* Version. */
	__u16 u16Size;		/* Structure size. */
	__u8 bValid;		/* if vsif info frame is valid */
	__u8 u8VSIFTypeCode;
	__u8 u8VSIFVersion;
	__u8 u8Length;
	__u32 u32IEEECode;
	__u8 u8ApplicationVersion;
	__u8 u8TargetSystemDisplayMaxLuminance;
	__u8 u8AverageMaxRGB;
	__u8 au8DistributionValues[9];
	__u16 u16KneePointX;
	__u16 u16KneePointY;
	__u8 u8NumBezierCurveAnchors;
	__u8 au8BezierCurveAnchors[9];
	__u8 u8GraphicsOverlayFlag;
	__u8 u8NoDelayFlag;
	__u8 u8SourceSideTmoFlag;
	__u8 u8AutoLowLatency;
} __attribute__((packed));

struct meta_srccap_hdr_emp_pkt {
	__u32 u32Version;
	__u8 bValid;
	__u8 bSync;
	__u8 bVFR;
	__u8 bAFR;
	__u8 u8DSType;
	__u8 u8OrganizationID;
	__u16 u16DataSetTag;
	__u16 u16DataSetLength;
	__u8 u8ItuTT35CountryCode; /* 0xB5 as HDR10+ SEI defined */
	__u16 u16ItuTT35TerminalProviderCode; /*0x003C as HDR10+ SEI defined*/
	__u16 u16ItuTT35TerminalProviderOrientedCode; /*0x0001 as HDR10+ SEI defined*/
	__u8 u8ApplicationIdentifier; /* 4 as HDR10+ SEI defined */
	__u8 u8ApplicationVersion; /* 1 as HDR10+ SEI defined */
	__u8 u8NumWindows;/* 1 as HDR10+ SEI defined */
	__u8 u8TargetedSystemDisplayMaximumLuminance;
	__u8 bTargetSystemDisplayActualPeakLuminanceFlag; /*0 as HDR10+ SEI defined*/
	__u32 au32Maxscl[3];
	__u32 au32Average_maxrgb[3];
	__u8 u8NumDistributions; /* 9 as HDR10+ SEI defined */
	__u8 au8DistributionValues[9];
	__u8 u8FractionBrightPixels;
	__u8 bMasteringDisplayActualPeakLuminanceFlag;
	__u8 bToneMappingFlag;
	__u8 u8KneePointX;
	__u8 u8KneePointY;
	__u8 u8NumBezierCurveAnchors;
	__u8 au8BezierCurveAnchors[9];
	__u8 u8ColorSaturationMappingFlag; /*0 as HDR10+ SEI defined*/
} __attribute__((packed));

struct meta_srccap_dip_point_info {
	__u16 width;
	__u16 height;
	enum m_color_format color_format;
	__u32 crop_align;
} __attribute__((packed));

struct meta_srccap_hdmi_pkt {
	__u8 u8Size;
	__u8 u8Data[META_SRCCAP_HDMIRX_PKT_MAX_SIZE*META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE];
};

struct hdmi_vsif_packet_info {
	__u8 u8hb0;       /* packet type */
	__u8 u8version;
	__u8 u5length : 5;
	__u8 u3rsv : 3;
	__u8 u8crc;
	__u8 au8ieee[3];
	__u8 au8payload[24];
} __attribute__((packed));

struct hdmi_hdr_packet_info {
	__u16 u16Version;
	__u16 u16Size;
	__u8 u8EOTF;
	__u8 u8Static_Metadata_ID;
	__u16 u16Display_Primaries_X[3];
	__u16 u16Display_Primaries_Y[3];
	__u16 u16White_Point_X;
	__u16 u16White_Point_Y;
	__u16 u16Max_Display_Mastering_Luminance;
	__u16 u16Min_Display_Mastering_Luminance;
	__u16 u16Maximum_Content_Light_Level;
	__u16 u16Maximum_Frame_Average_Light_Level;
	__u8 u8Version;
	__u8 u8Length;
} __attribute__((packed));

struct hdmi_emp_packet_info {
	__u8 u8hb0;
	__u8 u6rsv : 6;
	__u8 b_last : 1;
	__u8 b_first : 1;
	__u8 u8seq_index;
	union {
		struct {
			__u8 b_rsv: 1;
			__u8 b_sync : 1;
			__u8 b_vfr : 1;
			__u8 b_afr : 1;
			__u8 b_ds_type : 2;
			__u8 b_end : 1;
			__u8 b_new : 1;
			__u8 u8pb1;
			__u8 u8organ_id;
			__u8 u8data_set_tag_msb;
			__u8 u8data_set_tag_lsb;
			__u8 u8data_set_len_msb;
			__u8 u8data_set_len_lsb;
			__u8 au8md_first[21];
		};
		struct {
			__u8 au8md[28];
		};
	};
} __attribute__((packed));


struct hdmi_avi_packet_info {
	__u16 u16StructVersion;
	__u16 u16StructSize;
	__u8 u8Version;
	__u8 u8Length;
	__u8 u8S10Value;
	__u8 u8B10Value;
	__u8 u8A0Value;
	__u8 u8Y210Value;
	__u8 u8R3210Value;
	__u8 u8M10Value;
	__u8 u8C10Value;
	__u8 u8SC10Value;
	__u8 u8Q10Value;
	__u8 u8EC210Value;
	__u8 u8ITCValue;
	__u8 u8VICValue;
	__u8 u8PR3210Value;
	__u8 u8CN10Value;
	__u8 u8YQ10Value;
	__u8 u8ACE3210Value;
	enum m_hdmi_color_format enColorForamt;
	enum m_hdmi_color_range enColorRange;
	enum m_hdmi_colorimetry_format enColormetry;
	enum m_hdmi_pixel_repetition enPixelRepetition;
	enum m_hdmi_ar_type enAspectRatio;
	enum m_hdmi_ar_type enActiveFormatAspectRatio;
} __attribute__((packed));

struct hdmi_gc_packet_info {
	__u16 u16StructVersion;	/* Version. */
	__u16 u16StructSize;	/* Structure size. */
	__u8 u8ControlAVMute;
	__u8 u8DefaultPhase;
	__u8 u8CDValue;
	__u8 u8PPValue;
	__u8 u8LastPPValue;
	__u8 u8PreLastPPValue;
	enum m_hdmi_color_depth enColorDepth;
} __attribute__((packed));

struct hdmi_packet_info {
	unsigned char hb[3];
	unsigned char pb[28];
}  __attribute__((packed));

struct meta_srccap_dv_common {  /*refer  to  m_pq_dv_hdmi_common*/
	__u8 path;
	__u32 dv_src_hw_version;
	bool hdmi_422_pack_en;
};

struct meta_srccap_dv_descrb {  /*refer to m_pq_dv_hdmi_descrb*/
	enum m_dv_interface interface;
};

struct meta_srccap_dv_dma {  /*refer to m_pq_dv_hdmi_dma*/
	enum m_dv_dma_status status;
	__u64 addr[3];
	__u8  w_index;
	__u32 width;
	__u32 height;
	bool  svp_id_valid;
	__u32 svp_id;
};

struct meta_srccap_dv_meta {  /*refer to m_pq_dv_hdmi_meta*/
	__u8  data[1024];
	__u32 data_length;
};

struct meta_srccap_dv_info {  /*refer to m_pq_dv_hdmi_info*/
	__u16 version;
	struct meta_srccap_dv_common common;
	struct meta_srccap_dv_descrb descrb;
	struct meta_srccap_dv_dma    dma;
	struct meta_srccap_dv_meta   meta;
};


#endif /* __UAPI_MTK_M_SRCCAP_H__*/

