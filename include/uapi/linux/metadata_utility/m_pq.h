/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __UAPI_MTK_M_PQ_H
#define __UAPI_MTK_M_PQ_H

#include <linux/types.h>
#ifndef __KERNEL__
#include <stdint.h>
#include <stdbool.h>
#endif

/* ============================================================== */
/* ------------------------ Metadata Tag ------------------------- */
/* ============================================================== */

/* ============================================================== */
/* ---------------------- Metadata ENUM ----------------------- */
/* ============================================================== */
enum meta_pq_win_type {
	meta_pq_win_type_main = 0,
	meta_pq_win_type_sub,
	meta_pq_win_type_max,
};

enum meta_pq_source {
	meta_pq_source_ipdma = 0,
	meta_pq_source_b2r,
	meta_pq_source_max,
};

enum meta_pq_mode {
	meta_pq_mode_ts = 0,
	meta_pq_mode_legacy,
	meta_pq_mode_max,
};

enum meta_pq_scene {
	meta_pq_scene_normal = 0,
	meta_pq_scene_game,
	meta_pq_scene_game_crop,
	meta_pq_scene_pc,
	meta_pq_scene_photo,
	meta_pq_scene_force_p,
	meta_pq_scene_hfr,
	meta_pq_scene_game_with_memc,
	meta_pq_scene_3d,
	meta_pq_scene_sliced_game,
	meta_pq_scene_game_with_pc,
	meta_pq_scene_max,
};

enum meta_pq_framerate {
	meta_pq_framerate_24 = 0,
	meta_pq_framerate_25,
	meta_pq_framerate_30,
	meta_pq_framerate_50,
	meta_pq_framerate_60,
	meta_pq_framerate_120,
	meta_pq_framerate_144,
	meta_pq_framerate_240,
	meta_pq_framerate_max,
};

enum meta_pq_quality {
	meta_pq_quality_best = 0,
	meta_pq_quality_full,
	meta_pq_quality_normal,
	meta_pq_quality_lite,
	meta_pq_quality_zfd,
	meta_pq_quality_max,
};

enum meta_pq_idr_input_path {
	META_PQ_PATH_IPDMA_0 = 0,
	META_PQ_PATH_IPDMA_1,
	META_PQ_PATH_IPDMA_MAX,
};

enum meta_pq_colorformat {
	meta_pq_colorformat_rgb = 0,
	meta_pq_colorformat_yuv,
	meta_pq_colorformat_max,
};

enum meta_pq_input_source {
	META_PQ_INPUTSRC_NONE = 0,      /* NULL input */
	META_PQ_INPUTSRC_VGA,           /* 1   VGA input */
	META_PQ_INPUTSRC_TV,            /* 2   TV input */

	META_PQ_INPUTSRC_CVBS,          /* 3   AV 1 */
	META_PQ_INPUTSRC_CVBS2,         /* 4   AV 2 */
	META_PQ_INPUTSRC_CVBS3,         /* 5   AV 3 */
	META_PQ_INPUTSRC_CVBS4,         /* 6   AV 4 */
	META_PQ_INPUTSRC_CVBS5,         /* 7   AV 5 */
	META_PQ_INPUTSRC_CVBS6,         /* 8   AV 6 */
	META_PQ_INPUTSRC_CVBS7,         /* 9   AV 7 */
	META_PQ_INPUTSRC_CVBS8,         /* 10   AV 8 */
	META_PQ_INPUTSRC_CVBS_MAX,      /* 11 AV max */

	META_PQ_INPUTSRC_SVIDEO,        /* 12 S-video 1 */
	META_PQ_INPUTSRC_SVIDEO2,       /* 13 S-video 2 */
	META_PQ_INPUTSRC_SVIDEO3,       /* 14 S-video 3 */
	META_PQ_INPUTSRC_SVIDEO4,       /* 15 S-video 4 */
	META_PQ_INPUTSRC_SVIDEO_MAX,    /* 16 S-video max */

	META_PQ_INPUTSRC_YPBPR,         /* 17 Component 1 */
	META_PQ_INPUTSRC_YPBPR2,        /* 18 Component 2 */
	META_PQ_INPUTSRC_YPBPR3,        /* 19 Component 3 */
	META_PQ_INPUTSRC_YPBPR_MAX,     /* 20 Component max */

	META_PQ_INPUTSRC_SCART,         /* 21 Scart 1 */
	META_PQ_INPUTSRC_SCART2,        /* 22 Scart 2 */
	META_PQ_INPUTSRC_SCART_MAX,     /* 23 Scart max */

	META_PQ_INPUTSRC_HDMI,          /* 24 HDMI 1 */
	META_PQ_INPUTSRC_HDMI2,         /* 25 HDMI 2 */
	META_PQ_INPUTSRC_HDMI3,         /* 26 HDMI 3 */
	META_PQ_INPUTSRC_HDMI4,         /* 27 HDMI 4 */
	META_PQ_INPUTSRC_HDMI_MAX,      /* 28 HDMI max */

	META_PQ_INPUTSRC_DVI,           /* 29 DVI 1 */
	META_PQ_INPUTSRC_DVI2,          /* 30 DVI 2 */
	META_PQ_INPUTSRC_DVI3,          /* 31 DVI 2 */
	META_PQ_INPUTSRC_DVI4,          /* 32 DVI 4 */
	META_PQ_INPUTSRC_DVI_MAX,       /* 33 DVI max */

	META_PQ_INPUTSRC_DTV,           /* 34 DTV */
	META_PQ_INPUTSRC_DTV2,          /* 35 DTV2 */
	META_PQ_INPUTSRC_DTV_MAX,       /* 36 DTV max */

	/* Application source */
	META_PQ_INPUTSRC_STORAGE,       /* 37 Storage */
	META_PQ_INPUTSRC_STORAGE2,      /* 38 Storage2 */
	META_PQ_INPUTSRC_STORAGE_MAX,   /* 39 Storage max */

	/* Support OP capture */
	META_PQ_INPUTSRC_SCALER_OP,     /* 40 scaler OP */

	META_PQ_INPUTSRC_NUM,           /* number of the source */

};

enum meta_ip_window {
	meta_ip_capture = 0,
	meta_ip_ip2_in,
	meta_ip_ip2_out,
	meta_ip_scmi_in,
	meta_ip_scmi_out,
	meta_ip_di,
	meta_ip_aisr_in,
	meta_ip_aisr_out,
	meta_ip_hvsp_in,
	meta_ip_hvsp_out,
	meta_ip_display,
	meta_ip_panel_display,
	meta_ip_max,
};

enum meta_dip_cap_point {
	meta_dip_cap_pqin = 0,
	meta_dip_cap_iphdr,
	meta_dip_cap_prespf,
	meta_dip_cap_hvsp,
	meta_dip_cap_srs,
	meta_dip_cap_vip,
	meta_dip_cap_mdw,
	meta_dip_cap_max,
};

enum meta_pq_pqmap_type {
	META_PQMAP_MAIN,
	META_PQMAP_MAIN_EX,
	META_PQMAP_MAX
};

enum meta_pq_dipmap_type {
	META_DIPMAP_MAIN,
	META_DIPMAP_MAIN_EX,
	META_DIPMAP_MAX
};

#define CFD_ALG_ENUM_DECLATION 1
enum EN_PQ_CFD_DATA_FORMAT {
	E_PQ_CFD_DATA_FORMAT_RGB = 0x00,
	E_PQ_CFD_DATA_FORMAT_YUV422 = 0x01,
	E_PQ_CFD_DATA_FORMAT_YUV444 = 0x02,
	E_PQ_CFD_DATA_FORMAT_YUV420 = 0x03,
	E_PQ_CFD_DATA_FORMAT_RESERVED_START,
};

enum EN_PQ_CFD_COLOR_RANGE {
	E_PQ_CFD_COLOR_RANGE_LIMIT = 0x0,
	E_PQ_CFD_COLOR_RANGE_FULL = 0x1,
	E_PQ_CFD_COLOR_RANGE_RESERVED_START,
};

enum EN_PQ_CFD_HDR_MODE {
	E_PQ_CFD_HDR_MODE_SDR = 0x0,
	E_PQ_CFD_HDR_MODE_DOLBY = 0x1,  /* Dolby */
	E_PQ_CFD_HDR_MODE_HDR10 = 0x2,  /* open HDR */
	E_PQ_CFD_HDR_MODE_HLG = 0x3,    /* Hybrid log gamma */
	E_PQ_CFD_HDR_MODE_TCH = 0x4,    /* TCH */
	E_PQ_CFD_HDR_MODE_HDR10PLUS = 0x5,  /* HDR10plus */
	E_PQ_CFD_HDR_MODE_HDRVIVID = 0x6,  /* HDR VIVID */
	E_PQ_CFD_HDR_MODE_CONTROL_FLAG_RESET = 0x10,    /* flag reset */
	E_PQ_CFD_HDR_MODE_RESERVED_START,
};

enum EN_PQ_CFD_INPUT_SOURCE {
	E_PQ_CFD_INPUT_SOURCE_VGA = 0x00,
	E_PQ_CFD_INPUT_SOURCE_TV = 0x01,
	E_PQ_CFD_INPUT_SOURCE_CVBS = 0x02,
	E_PQ_CFD_INPUT_SOURCE_SVIDEO = 0x03,
	E_PQ_CFD_INPUT_SOURCE_YPBPR = 0x04,
	E_PQ_CFD_INPUT_SOURCE_SCART = 0x05,
	E_PQ_CFD_INPUT_SOURCE_HDMI = 0x06,
	E_PQ_CFD_INPUT_SOURCE_DTV = 0x07,
	E_PQ_CFD_INPUT_SOURCE_DVI = 0x08,
	E_PQ_CFD_INPUT_SOURCE_STORAGE = 0x09,
	E_PQ_CFD_INPUT_SOURCE_KTV = 0x0A,
	E_PQ_CFD_INPUT_SOURCE_JPEG = 0x0B,
	E_PQ_CFD_INPUT_SOURCE_RX = 0x0C,
	E_PQ_CFD_INPUT_SOURCE_RESERVED_START,
	E_PQ_CFD_INPUT_SOURCE_NONE = E_PQ_CFD_INPUT_SOURCE_RESERVED_START,
	E_PQ_CFD_INPUT_SOURCE_GENERAL = 0xFF,
};

enum EN_PQ_CFD_OUTPUT_SOURCE {
	/* include VDEC series */
	E_PQ_CFD_OUTPUT_SOURCE_MM = 0x00,
	E_PQ_CFD_OUTPUT_SOURCE_HDMI = 0x01,
	E_PQ_CFD_OUTPUT_SOURCE_ANALOG = 0x02,
	E_PQ_CFD_OUTPUT_SOURCE_PANEL = 0x03,
	E_PQ_CFD_OUTPUT_SOURCE_ULSA = 0x04,
	E_PQ_CFD_OUTPUT_SOURCE_GENERAL = 0x05,
	E_PQ_CFD_OUTPUT_SOURCE_RESERVED_START,
};

enum EN_PQ_CFD_COLOR_FORMAT {
	E_PQ_CFD_COLOR_FORMAT_RGB_NOTSPECIFIED      = 0x0, /*means RGB, but no specific colorspace*/
	E_PQ_CFD_COLOR_FORMAT_RGB_BT601_625         = 0x1,
	E_PQ_CFD_COLOR_FORMAT_RGB_BT601_525         = 0x2,
	E_PQ_CFD_COLOR_FORMAT_RGB_BT709             = 0x3,
	E_PQ_CFD_COLOR_FORMAT_RGB_BT2020            = 0x4,
	E_PQ_CFD_COLOR_FORMAT_SRGB                  = 0x5,
	E_PQ_CFD_COLOR_FORMAT_ADOBE_RGB             = 0x6,
	E_PQ_CFD_COLOR_FORMAT_YUV_NOTSPECIFIED      = 0x7, /*means RGB, but no specific colorspace*/
	E_PQ_CFD_COLOR_FORMAT_YUV_BT601_625         = 0x8,
	E_PQ_CFD_COLOR_FORMAT_YUV_BT601_525         = 0x9,
	E_PQ_CFD_COLOR_FORMAT_YUV_BT709             = 0xA,
	E_PQ_CFD_COLOR_FORMAT_YUV_BT2020_NCL        = 0xB,
	E_PQ_CFD_COLOR_FORMAT_YUV_BT2020_CL         = 0xC,
	E_PQ_CFD_COLOR_FORMAT_XVYCC_601             = 0xD,
	E_PQ_CFD_COLOR_FORMAT_XVYCC_709             = 0xE,
	E_PQ_CFD_COLOR_FORMAT_SYCC601               = 0xF,
	E_PQ_CFD_COLOR_FORMAT_ADOBE_YCC601          = 0x10,
	E_PQ_CFD_COLOR_FORMAT_DOLBY_HDR_TEMP        = 0x11,
	E_PQ_CFD_COLOR_FORMAT_SYCC709               = 0x12,
	E_PQ_CFD_COLOR_FORMAT_DCIP3_THEATER         = 0x13,
	E_PQ_CFD_COLOR_FORMAT_DCIP3_D65             = 0x14,
	E_PQ_CFD_COLOR_FORMAT_ITURBT_BT2100_ICTCP   = 0x15,
	E_PQ_CFD_COLOR_FORMAT_RESERVED_START,
};

/* color space */
enum EN_PQ_CFD_CFIO_CP {
	E_PQ_CFD_CFIO_CP_RESERVED0 = 0x0,   /* means RGB, but no specific colorspace */
	E_PQ_CFD_CFIO_CP_BT709_SRGB_SYCC = 0x1,
	E_PQ_CFD_CFIO_CP_UNSPECIFIED = 0x2,
	E_PQ_CFD_CFIO_CP_RESERVED3 = 0x3,
	E_PQ_CFD_CFIO_CP_BT470_6 = 0x4,
	E_PQ_CFD_CFIO_CP_BT601625 = 0x5,
	E_PQ_CFD_CFIO_CP_BT601525_SMPTE170M = 0x6,
	E_PQ_CFD_CFIO_CP_SMPTE240M = 0x7,
	E_PQ_CFD_CFIO_CP_GENERIC_FILM = 0x8,
	E_PQ_CFD_CFIO_CP_BT2020 = 0x9,
	E_PQ_CFD_CFIO_CP_CIEXYZ = 0xA,
	E_PQ_CFD_CFIO_CP_DCIP3_THEATER = 0xB,
	E_PQ_CFD_CFIO_CP_DCIP3_D65 = 0xC,
	/* 13-21: reserved */
	E_PQ_CFD_CFIO_CP_EBU3213 = 0x16,
	/* 23-127: reserved */
	E_PQ_CFD_CFIO_CP_ADOBERGB = 0x80,
	E_PQ_CFD_CFIO_CP_PANEL = 0x81,
	E_PQ_CFD_CFIO_CP_SOURCE = 0x82,
	E_PQ_CFD_CFIO_CP_RESERVED_START,
};

/* Transfer characteristics */
enum EN_PQ_CFD_CFIO_TR {
	E_PQ_CFD_CFIO_TR_RESERVED0 = 0x0,   /* means RGB, but no specific colorspace */
	E_PQ_CFD_CFIO_TR_BT709 = 0x1,
	E_PQ_CFD_CFIO_TR_UNSPECIFIED = 0x2,
	E_PQ_CFD_CFIO_TR_RESERVED3 = 0x3,
	E_PQ_CFD_CFIO_TR_GAMMA2P2 = 0x4,
	E_PQ_CFD_CFIO_TR_GAMMA2P8 = 0x5,
	E_PQ_CFD_CFIO_TR_BT601525_601625 = 0x6,
	E_PQ_CFD_CFIO_TR_SMPTE240M = 0x7,
	E_PQ_CFD_CFIO_TR_LINEAR = 0x8,
	E_PQ_CFD_CFIO_TR_LOG0 = 0x9,
	E_PQ_CFD_CFIO_TR_LOG1 = 0xA,
	E_PQ_CFD_CFIO_TR_XVYCC = 0xB,
	E_PQ_CFD_CFIO_TR_BT1361 = 0xC,
	E_PQ_CFD_CFIO_TR_SRGB_SYCC = 0xD,
	E_PQ_CFD_CFIO_TR_BT2020NCL = 0xE,
	E_PQ_CFD_CFIO_TR_BT2020CL = 0xF,
	E_PQ_CFD_CFIO_TR_SMPTE2084 = 0x10,
	E_PQ_CFD_CFIO_TR_SMPTE428 = 0x11,
	E_PQ_CFD_CFIO_TR_HLG = 0x12,
	E_PQ_CFD_CFIO_TR_BT1886 = 0x13,
	E_PQ_CFD_CFIO_TR_DOLBYMETA = 0x14,
	E_PQ_CFD_CFIO_TR_ADOBERGB = 0x15,
	E_PQ_CFD_CFIO_TR_GAMMA2P6 = 0x16,
	E_PQ_CFD_CFIO_TR_RESERVED_START,
};

/* Matrix coefficient */
enum EN_PQ_CFD_CFIO_MC {
	E_PQ_CFD_CFIO_MC_IDENTITY = 0x0,    /* means RGB, but no specific colorspace */
	E_PQ_CFD_CFIO_MC_BT709_XVYCC709 = 0x1,
	E_PQ_CFD_CFIO_MC_UNSPECIFIED = 0x2,
	E_PQ_CFD_CFIO_MC_RESERVED = 0x3,
	E_PQ_CFD_CFIO_MC_USFCCT47 = 0x4,
	E_PQ_CFD_CFIO_MC_BT601625_XVYCC601_SYCC = 0x5,
	E_PQ_CFD_CFIO_MC_BT601525_SMPTE170M = 0x6,
	E_PQ_CFD_CFIO_MC_SMPTE240M = 0x7,
	E_PQ_CFD_CFIO_MC_YCGCO = 0x8,
	E_PQ_CFD_CFIO_MC_BT2020NCL = 0x9,
	E_PQ_CFD_CFIO_MC_BT2020CL = 0xA,
	E_PQ_CFD_CFIO_MC_YDZDX = 0xB,
	E_PQ_CFD_CFIO_MC_CD_NCL = 0xC,
	E_PQ_CFD_CFIO_MC_CD_CL = 0xD,
	E_PQ_CFD_CFIO_MC_ICTCP = 0xE,
	E_PQ_CFD_CFIO_MC_RESERVED_START,
};

enum EN_PQ_AIPQ_ACTIVITY {
	META_PQ_AIPQ_ACTIVITY_ON = 0x0,
	META_PQ_AIPQ_ACTIVITY_TEMP_OFF,
	META_PQ_AIPQ_ACTIVITY_OFF,
	META_PQ_AIPQ_ACTIVITY_MAX,
};

enum meta_pq_aipq_scene_type {
	META_PQ_AIPQ_SCENE_TYPE_FACE = 0,
	META_PQ_AIPQ_SCENE_TYPE_BLUE,
	META_PQ_AIPQ_SCENE_TYPE_GREEN,
	META_PQ_AIPQ_SCENE_TYPE_FOOD,
	META_PQ_AIPQ_SCENE_TYPE_ARCH,
	META_PQ_AIPQ_SCENE_TYPE_ANIME,
	META_PQ_AIPQ_SCENE_TYPE_SPORT,
	META_PQ_AIPQ_SCENE_TYPE_MOVIE,
	META_PQ_AIPQ_SCENE_TYPE_MAX,
};

enum meta_pq_aipq_model_type {
	META_AIPQ_MODEL_TYPE_NONE = 0,
	META_AIPQ_MODEL_TYPE_MDLA,
	META_AIPQ_MODEL_TYPE_MAX,
};

enum EN_PQ_AIPQ_CONDITION {
	E_PQ_AIPQ_COND_NORMAL = 0x0,
	E_PQ_AIPQ_COND_NO_SIGNAL = 0x01,
	E_PQ_AIPQ_COND_FIRST_RESULT = 0x02,
	E_PQ_AIPQ_COND_RESOLUTION_CHANGE = 0x04,
	E_PQ_AIPQ_COND_AR_CHANGE = 0x08,
	E_PQ_AIPQ_COND_MUTE_VIDEO = 0x10,
	E_PQ_AIPQ_COND_FASTFORWARD_REWIND = 0x20,
	E_PQ_AIPQ_COND_PAUSE = 0x40,
	E_PQ_AIPQ_COND_P_I_CHANGE = 0x80,
	E_PQ_AIPQ_COND_HDR = 0x100,
	E_PQ_AIPQ_COND_SOURCE_CHANGE = 0x200,
	E_PQ_AIPQ_COND_MULTI_WIN = 0x400,
	E_PQ_AIPQ_COND_PHOTO = 0x800,
	E_PQ_AIPQ_COND_UNSUPPORT_FRAME_RATE = 0x1000,
	E_PQ_AIPQ_COND_DIP_STOP = 0x2000,
};

enum meta_pq_frame_buffer_mode {
	META_PQ_FRAME_BUFFER_NONE = 0,
	META_PQ_FRAME_BUFFER_FBL,
	META_PQ_FRAME_BUFFER_RFBL,
	META_PQ_FRAME_BUFFER_FB,
	META_PQ_FRAME_BUFFER_MAX
};

/* m_pqu_cfd.h */
#ifndef E_COMPONENT1_TAG_START
#define E_COMPONENT1_TAG_START 0x00010000
enum EN_COMPONENT1_TAG { /* range 0x00010000 ~ 0x0001FFFF */
	M_PQ_CFD_INPUT_FORMAT_META_TAG = E_COMPONENT1_TAG_START,
	M_PQ_CFD_OUTPUT_FORMAT_META_TAG,
	META_CFD_TEST_RULELIST_TAG,
	M_PQ_CFD_INPUT_TMO_TAG,
	E_COMPONENT1_TAG_MAX,
};
#endif

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
	M_PQ_LOCALHSY_TAG,
	M_PQ_LOCALVAC_TAG,
	M_PQ_VSYNC_INFO_TAG,
	E_COMPONENT3_TAG_MAX,
};
#endif

/* m_pq.h */
#ifndef E_COMPONENT5_TAG_START
#define E_COMPONENT5_TAG_START 0x00050000
enum EN_COMPONENT5_TAG { /* range 0x00050000 ~ 0x0005FFFF */
	PQ_DISP_SVP_META_TAG = E_COMPONENT5_TAG_START,
	META_PQ_OUTPUT_FRAME_INFO_TAG,
	MTK_PQ_SH_FRM_INFO_TAG,
	META_PQ_DISPLAY_FLOW_INFO_TAG,
	META_PQ_DISPLAY_IDR_CTRL_META_TAG,
	META_PQ_STREAM_INTERNAL_INFO_TAG,
	PQ_DISP_STREAM_META_TAG,
	META_PQ_INPUT_QUEUE_EXT_INFO_TAG,
	META_PQ_OUTPUT_QUEUE_EXT_INFO_TAG,
	META_PQ_FRAMESYNC_INFO_TAG,
	META_PQ_OUTPUT_RENDER_INFO_TAG,
	META_PQ_DISPLAY_WM_INFO_TAG,
	META_PQ_BBD_INFO_TAG,
	META_PQ_PQPARAM_TAG,
	META_PQ_CUSTOMER1_TAG,
	META_PQ_FORCEP_INFO_TAG,
	META_PQ_THERMAL_INFO_TAG,
	META_PQ_LOCALHSY_TAG,
	META_PQ_LOCALVAC_TAG,
	META_PQ_VSYNC_INFO_TAG,
	E_COMPONENT5_TAG_MAX,
};
#endif

#ifndef E_COMPONENT13_TAG_START
#define E_COMPONENT13_TAG_START 0x000D0000
enum EN_COMPONENT13_TAG { /* range 0x000D0000 ~ 0x000DFFFF */
	META_PQ_CFD_SHM_INFO_TAG = E_COMPONENT13_TAG_START,
	META_PQ_CFD_SHM_GAMMA_INFO_TAG,
	META_PQ_CFD_SHM_3DLUT_INFO_TAG,
	META_PQ_CFD_SHM_HDR_CONF_INFO_TAG,
	META_PQ_CFD_SHM_SDR_PIC1,
	META_PQ_CFD_SHM_HDR_PIC1,
	META_PQ_CFD_SHM_HLG_PIC1,
	META_PQ_CFD_SHM_WIN0,
	META_PQ_CFD_SHM_WIN1,
	META_PQ_CFD_SHM_WIN2,
	META_PQ_CFD_SHM_WIN3,
	META_PQ_CFD_SHM_WIN4,
	META_PQ_CFD_SHM_WIN5,
	META_PQ_CFD_SHM_WIN6,
	META_PQ_CFD_SHM_WIN7,
	META_PQ_CFD_SHM_WIN8,
	META_PQ_CFD_SHM_WIN9,
	META_PQ_CFD_SHM_WIN10,
	META_PQ_CFD_SHM_WIN11,
	META_PQ_CFD_SHM_WIN12,
	META_PQ_CFD_SHM_WIN13,
	META_PQ_CFD_SHM_WIN14,
	META_PQ_CFD_SHM_WIN15,
	E_COMPONENT13_TAG_MAX,
};
#endif

enum EN_PQ_CFD_STATE_CURVE {
	M_PQ_CFD_STATE_CURVE_OETF,			   // pre1D
	M_PQ_CFD_STATE_CURVE_EOTF,			   // post1D
	M_PQ_CFD_STATE_CURVE_3DLUT,			  // 3dlut
	M_PQ_CFD_STATE_CURVE_3x3,				// matrix
	M_PQ_CFD_STATE_CURVE_TMO,				// tmo
	M_PQ_CFD_STATE_CURVE_HDR_EOTF,		   // hdr_eotf
	M_PQ_CFD_STATE_CURVE_HDR_OOTF,		   // hdr_ootf
	M_PQ_CFD_STATE_CURVE_HDR_OETF,		   // hdr_oetf
};
enum EN_PQ_VAC_SAT_BY_TYPE {
	M_PQ_VAC_SAT_BY_HUE,
	M_PQ_VAC_SAT_BY_POS,
	M_PQ_VAC_SAT_BY_MAX,
};

enum EN_PQ_VAC_POS_RANGE_TYPE {
	M_PQ_VAC_POS_RANGE_1024,
	M_PQ_VAC_POS_RANGE_2048,
	M_PQ_VAC_POS_RANGE_4096,
	M_PQ_VAC_POS_RANGE_8192,
	M_PQ_VAC_POS_RANGE_MAX,
};

/* ============================================================== */
/* ---------------------- Metadata Struct ----------------------- */
/* ============================================================== */

#define META_PQ_WIN_NUM_MAX (16)

struct meta_pq_window {
	__u16 x;
	__u16 y;
	__u16 width;
	__u16 height;
	__u16 w_align;
	__u16 h_align;
};

struct meta_dip_window {
	uint16_t width;
	uint16_t height;
	uint8_t color_fmt; /* refer EN_PQ_CFD_DATA_FORMAT */
	uint8_t p_engine;
	uint8_t data_range; /*refer enum EN_PQ_CFD_COLOR_RANGE*/
	uint8_t u8ColorSpace; //enum EN_PQ_CFD_CFIO_CP
	uint8_t u8TransferCharacter; //enum EN_PQ_CFD_CFIO_TR
	uint8_t u8MatrixCoef; //enum EN_PQ_CFD_CFIO_MC
};

struct meta_pq_display_rect {
	uint32_t left;
	uint32_t top;
	uint32_t width;
	uint32_t height;
};

struct meta_pq_wm_config {
	bool bWm_en;
};

struct meta_pq_wm_result {
	__u32 u32Version;
	__u32 u32Length;
	__u16 u16Wm_En;
	__u16 u16Wm_decoded[30];
	__u8 u8Wm_DetectStatus;
};

struct meta_pq_aipq_condition {
	enum EN_PQ_AIPQ_CONDITION eCondition;
	__u32 u8FrameId;
	enum EN_PQ_AIPQ_ACTIVITY eActivity;
};

struct meta_pq_aipq_obj_result {
	/* position result, format 1.15, 0~0x8000 */
	__u32 x;
	__u32 y;
	__u32 w;
	__u32 h;

	/* confidence result, format 1.15, 0~0x8000 */
	__u32 u32ConfidenceObj;
	__u32 u32ConfidenceFront; /* rop 0 */
	__u32 u32ConfidenceRight; /* rop 1 */
	__u32 u32ConfidenceLeft; /* rop 2 */
};

struct meta_pq_idr_frame_info {
	__u16 y;
	__u16 height;
	__u16 v_total;
	__u8 vs_pulse_width;
	__u32 fdet_vtt0;
	__u32 fdet_vtt1;
	__u32 fdet_vtt2;
	__u32 fdet_vtt3;
};

struct meta_panel_window {
	__u32 x;
	__u32 y;
	__u32 width;
	__u32 height;
};

#define META_PQ_AIPQ_OBJ_RESULT_MAX_CNT (30)
struct meta_pq_aipq_object_info {
	__u8 u8FrameId;
	__u32 u32ResultCnt;
	enum meta_pq_aipq_model_type model_type;
	struct meta_pq_aipq_obj_result obj_result[META_PQ_AIPQ_OBJ_RESULT_MAX_CNT];
};

struct meta_pq_aipq_scene_info {
	__u8 u8FrameId;
	__u16 u16Confidence[META_PQ_AIPQ_SCENE_TYPE_MAX]; /*scene confidence, format 16.0, 0~10000*/
};

#define META_PQ_AIPQ_TEXT_RESULT_MAX_CNT (60)
struct meta_pq_aipq_text_info {
	__u8 u8_frame_id; /* maybe delay. if = 0xff, means model is disabled */
	__u32 u32_result_cnt;
	enum meta_pq_aipq_model_type model_type;
	struct meta_pq_aipq_obj_result obj_result[META_PQ_AIPQ_TEXT_RESULT_MAX_CNT];
};

#define META_PQ_AIPQ_DEPTH_RESULT_MAX_CNT (32)
struct meta_pq_aipq_depth_info {
	__u8 u8_frame_id;
	__u8 u8_depth_value[META_PQ_AIPQ_DEPTH_RESULT_MAX_CNT][META_PQ_AIPQ_DEPTH_RESULT_MAX_CNT];
};

/* ============================================================== */
/* ---------------------- Metadata Content ----------------------- */
/* ============================================================== */
#define META_PQ_DISP_SVP_VERSION (1)
struct meta_pq_disp_svp {
	__u8 aid;
	__u32 pipelineid;
};

/*
 * Version 1: Init structure
 */
#define META_PQ_OUTPUT_FRAME_INFO_VERSION (1)
struct meta_pq_output_frame_info {
	uint32_t width;
	uint32_t height;
	struct meta_pq_window capture;
	struct meta_pq_window crop;
	struct meta_pq_window display;
	bool nonlinear;
	uint64_t fd_offset[3];
	int32_t pq_frame_id;
	uint16_t hdr_type;
	uint8_t pq_scene;
};

#define META_PQ_FRAMESYNC_INFO_VERSION (1)
struct meta_pq_framesync_info {
	enum meta_pq_input_source input_source;
	bool bInterlace;
};

struct meta_pq_pqrm_win_info {
	__u8 u8PqID;
	__u32 u32WinTime;
};
struct meta_pq_pqrm_info {
	__u32 u32RemainBWBudget;
	__u8 u8ActiveWinNum;
	bool bIsPQHWSet;
	struct meta_pq_pqrm_win_info win_info[META_PQ_WIN_NUM_MAX];
};


#define META_PQ_OUTPUT_RENDER_INFO_VERSION (1)
struct meta_pq_output_render_info {
	struct meta_pq_window capture;
	struct meta_pq_window crop;
	struct meta_pq_window displayRange;
	struct meta_pq_window displayArea;
	struct meta_pq_window displayBase;
	struct meta_pq_window displayWin;
	__u8 u8DotByDotType;
	__u8 u8SignalStable;
	__u32 u32RefreshRate;
	__u8 u8Aid;
	__u32 u32Pipelineid;
	__u8 u8MuteAction;
	__u8 u8PqMuteAction;
	__u64 u64Pts;
	__u32 u32OriginalInputFps;
	struct meta_pq_pqrm_info pqrminfo;
	bool bReportFrameStamp;
	struct meta_pq_idr_frame_info idrinfo;
	enum meta_pq_frame_buffer_mode eFrameBufferMode;
	bool ip_path;
	bool op_path;
};

#define META_PQ_STREAM_INTERNAL_INFO_VERSION (1)
struct meta_pq_stream_internal_info {
	uint64_t file;
	uint64_t pq_dev;
};

#define META_PQ_STREAM_DEBUG_INFO_VERSION (1)
struct meta_pq_stream_debug_info {
	bool cmdq_timeout;
};

#define META_PQ_STREAM_INFO_VERSION (1)
struct meta_pq_stream_info {
	int width;
	int height;
	bool interlace;
	bool adaptive_stream;
	bool low_latency;
	enum meta_pq_source pq_source;
	enum meta_pq_mode pq_mode;
	enum meta_pq_scene pq_scene;
	enum meta_pq_framerate pq_framerate;
	enum meta_pq_quality pq_quality;
	enum meta_pq_colorformat pq_colorformat;
	bool stub;
	enum meta_pq_idr_input_path pq_idr_input_path;
	__u8 scenario_idx;
	bool bForceP;
	bool bPureVideoPath;

	/* path select*/
	bool ip_path;
	bool op_path;
	bool bvdec_statistic_en;
	int input_fps;
	int display_height;
	int display_width;
	int output_fps;
	int output_height;
	int output_width;
	struct meta_panel_window panel_window;
};

#define META_PQ_DISPLAY_FLOW_INFO_VERSION (1)
struct meta_pq_display_flow_info {
	struct meta_pq_window content;
	struct meta_pq_window capture;
	struct meta_pq_window crop;
	struct meta_pq_window idrcrop;
	struct meta_pq_window display;
	struct meta_pq_window displayArea;
	struct meta_pq_window displayRange;
	struct meta_pq_window displayBase;
	struct meta_pq_window realDisplaySize;

	struct meta_pq_window ip_win[meta_ip_max];
	bool aisr_enable;
	bool vip_path;
	__u16 aisr_ui_level;

	/* output data */
	uint8_t win_id;
	struct meta_dip_window dip_win[meta_dip_cap_max];
	struct meta_pq_window outcrop;
	struct meta_pq_window outdisplay;

	/* win info */
	enum meta_pq_win_type pq_win_type;
	bool multiple_window;
	/*mwe activewin */
	struct meta_pq_window mwe_active_win[meta_ip_max];
};

#define META_PQ_AIPQ_FRAME_INFO_VERSION (1)
struct meta_pq_aipq_frame_info {
	struct meta_pq_aipq_condition aipq_condition;
	struct meta_pq_aipq_object_info aipq_obj_info;
	struct meta_pq_aipq_scene_info aipq_scene_info;
	struct meta_pq_aipq_text_info aipq_text_info;
	struct meta_pq_aipq_depth_info aipq_depth_info;
};

#define META_PQ_FORCEP_INFO_VERSION (1)
struct meta_pq_forcep_info {
	bool PIChange;
	__u64 PICounter;
	bool source_interlace;
	enum meta_pq_quality source_level;
	bool target_interlace;
	enum meta_pq_quality target_level;
};

#define META_PQ_INPUT_QUEUE_EXT_INFO_VERSION (2)
struct meta_pq_input_queue_ext_info {
	__u64 u64Pts;
	__u64 u64UniIdx;
	__u64 u64ExtUniIdx;
	__u64 u64TimeStamp;
	__u64 u64RenderTimeNs;
	__u8 u8BufferValid;
	__u32 u32BufferSlot;
	__u32 u32GenerationIndex;
	__u32 u32StreamUniqueId;
	__u8 u8RepeatStatus;
	__u8 u8FrcMode;
	__u8 u8Interlace;
	__u32 u32InputFps;
	__u32 u32OriginalInputFps;
	bool bEOS;
	__u8 u8MuteAction;
	__u8 u8PqMuteAction;
	__u8 u8SignalStable;
	__u8 u8DotByDotType;
	__u32 u32RefreshRate;
	__u8 u8FastForwardFlag;
	bool bReportFrameStamp;
	bool bBypassAvsync;
	struct meta_pq_pqrm_info pqrminfo;
	__u32 u32HdrApplyType;
	__u32 u32QueueInputIndex;
	struct meta_pq_idr_frame_info idrinfo;
	bool bMuteChange;
	bool bFdMaskBypass;
	bool bTriggerInfiniteGen;
	bool bEnableBbd;
	bool bPerFrameMode;
};

#define META_PQ_OUTPUT_QUEUE_EXT_INFO_VERSION (2)
struct meta_pq_output_queue_ext_info {
	__u64 u64Pts;
	__u64 u64UniIdx;
	__u64 u64ExtUniIdx;
	__u64 u64TimeStamp;
	__u64 u64RenderTimeNs;
	__u8 u8BufferValid;
	__u32 u32BufferSlot;
	__u32 u32GenerationIndex;
	__u32 u32StreamUniqueId;
	__u8 u8RepeatStatus;
	__u8 u8FrcMode;
	__u8 u8Interlace;
	__u32 u32InputFps;
	__u32 u32OriginalInputFps;
	bool bEOS;
	__u8 u8MuteAction;
	__u8 u8PqMuteAction;
	__u8 u8SignalStable;
	__u8 u8DotByDotType;
	__u32 u32RefreshRate;
	bool bReportFrameStamp;
	bool bBypassAvsync;
	__u32 u32HdrApplyType;
	__u32 u32QueueInputIndex;
	__u32 u32QueueOutputIndex;
	struct meta_pq_pqrm_info pqrminfo;
	struct meta_pq_wm_result wm_result;
	struct meta_pq_idr_frame_info idrinfo;
	bool bMuteChange;
	bool bFdMaskBypass;
};

#define META_PQ_DISPLAY_IDR_CTRL_VERSION (1)
struct meta_pq_display_idr_ctrl {
	struct meta_pq_display_rect crop;
	uint32_t mem_fmt;
	uint32_t width;
	uint32_t height;
	uint16_t index;		/* force read index */
	uint64_t vb;		/* ptr of struct vb2_buffer, use in callback */
	enum meta_pq_idr_input_path path;
	bool bypass_ipdma;
	bool v_flip;
	uint8_t aid;		/* access id */
};

#define M_PQ_CFD_INPUT_FORMAT_VERSION (0)
struct m_pq_cfd_input_format {
	uint8_t source; /*refer enum EN_PQ_CFD_INPUT_SOURCE*/

	uint8_t data_format; /*refer enum EN_PQ_CFD_DATA_FORMAT*/
	uint8_t bit_depth; /*6/8/10/12*/
	uint8_t data_range; /*refer enum EN_PQ_CFD_COLOR_RANGE*/
	uint8_t colour_primaries; /*refer enum EN_PQ_CFD_CFIO_CP*/
	uint8_t transfer_characteristics; /*refer enum EN_PQ_CFD_CFIO_TR*/
	uint8_t matrix_coefficients; /*refer enum EN_PQ_CFD_CFIO_MC*/

	uint8_t hdr_mode; /*refer enum EN_PQ_CFD_HDR_MODE*/
	uint32_t u32SamplingMode;
	uint8_t u8Source_Format;
	uint8_t u8IPDMA_Format;
};

#define M_PQ_CFD_OUTPUT_FORMAT_VERSION (0)
struct m_pq_cfd_output_format {
	uint8_t source; /*refer enum EN_PQ_CFD_OUTPUT_SOURCE*/

	uint8_t data_format; /*refer enum EN_PQ_CFD_DATA_FORMAT*/
	uint8_t bit_depth; /*6/8/10/12*/
	uint8_t data_range; /*refer enum EN_PQ_CFD_COLOR_RANGE*/
	uint8_t colour_primaries; /*refer enum EN_PQ_CFD_CFIO_CP*/
	uint8_t transfer_characteristics; /*refer enum EN_PQ_CFD_CFIO_TR*/
	uint8_t matrix_coefficients; /*refer enum EN_PQ_CFD_CFIO_MC*/
};

#define M_PQ_CFD_TMO_CUS_VERSION (0)
struct m_pq_cfd_tmocus {
	bool tmocus_manual_mode;
	uint16_t u16TmoSamplesize;
	uint32_t u32TmoSource[256];
	uint32_t u32TmoTarget[256];
};

#define META_PQ_PQMAP_RULE_VERSION (1)
#define META_PQ_PQMAP_NODE_SIZE (1024)

struct meta_pq_pqmap_rule_settings {
	uint16_t au16Nodes[META_PQ_PQMAP_NODE_SIZE];
	uint16_t au16NodesNum[META_PQMAP_MAX];
};

#define META_PQ_DIPMAP_RULE_VERSION (1)
#define META_PQ_DIPMAP_NODE_SIZE (1024)

struct meta_pq_dipmap_rule_settings {
	uint16_t au16Nodes[META_PQ_DIPMAP_NODE_SIZE];
	uint16_t au16NodesNum[META_DIPMAP_MAX];
};

#define META_PQ_PQPARAM_VERSION (1)
#define META_PQ_PQPARAM_NUM (768)//3072(3K)/4 = 768
struct meta_pq_pqparam {
	__u32  data[META_PQ_PQPARAM_NUM];
};

#define M_LOCAL_HSY_SIZE_HUE_BY_HUE   (36)
#define M_LOCAL_HSY_SIZE_HUE_BY_SAT   (36*9)
#define M_LOCAL_HSY_SIZE_HUE_BY_LUMA  (36*9)
#define M_LOCAL_HSY_SIZE_SAT_BY_HUE   (36*9)
#define M_LOCAL_HSY_SIZE_SAT_BY_LUMA  (36*9)
#define M_LOCAL_HSY_SIZE_LUMA_BY_LUMA (36*9)
#define M_LOCAL_HSY_SIZE_LUMA_BY_SAT  (36*9)
#define M_LOCAL_HSY_TABLE_SIZE        (36*9)

#define META_PQ_LOCALHSY_VERSION (1)
struct meta_pq_localhsy {
	__s8 s8HuebyHueLut[M_LOCAL_HSY_SIZE_HUE_BY_HUE];          //36
	__u16 u16HuebySatLut[M_LOCAL_HSY_SIZE_HUE_BY_SAT];       //36*9
	__u16 u16HuebyLumaLut[M_LOCAL_HSY_SIZE_HUE_BY_LUMA];      //36*9
	__u16 u16SatbyHueCurrentLut[M_LOCAL_HSY_SIZE_SAT_BY_HUE];//36*9
	__u16 u16SatbyHueMinLut[M_LOCAL_HSY_SIZE_SAT_BY_LUMA];    //36*9
	__u8 u8SatbyLumaLut[M_LOCAL_HSY_SIZE_LUMA_BY_LUMA];        //36*9
	__s16 s16LumabySatLut[M_LOCAL_HSY_SIZE_LUMA_BY_SAT];      //36*9
	__s16 s16LumabyLumaLut[M_LOCAL_HSY_TABLE_SIZE];     //36*9
};

#define M_HSYVAC_SAT_BY_POS_LEN     (9)
#define M_HSYVAC_SAT_BY_HUE_LEN     (7)

#define META_PQ_LOCALVAC_VERSION (1)
struct meta_pq_localvac {
	bool en[M_PQ_VAC_SAT_BY_MAX];
	enum EN_PQ_VAC_POS_RANGE_TYPE type;
	__u8 u8SatbyHueLut[M_HSYVAC_SAT_BY_HUE_LEN];
	__u8 u8SatbyPosLut[M_HSYVAC_SAT_BY_POS_LEN];
};

struct meta_upgci_set_conf_hdr_cmd {
	uint32_t EOTF;
	uint16_t RedX;
	uint16_t RedY;
	uint16_t GreenX;
	uint16_t GreenY;
	uint16_t BlueX;
	uint16_t BlueY;
	uint16_t WhiteX;
	uint16_t WhiteY;
	uint16_t MinLuma;
	uint32_t MaxLuma;
	uint32_t MaxCLL;
	uint32_t MaxFall;
};

struct meta_cfd_hdr_conf {
	struct meta_upgci_set_conf_hdr_cmd data;
	uint32_t u32Size;
	uint32_t u32State;
};

struct meta_cfd_tmo_normal_code {
	uint8_t u8SourceEncodedType;
	uint8_t u8SourceShift;
	uint8_t u8TargetEncodedType;
	uint8_t u8TargetShift;
	uint16_t u16PanelMaxNits;
	uint16_t u16TmoSamplesize;
	uint32_t u32TmoSource[256]; // 256
	uint32_t u32TmoTarget[256]; // 256
};

struct meta_cfd_tmo_normal_nits {
	uint16_t u16TmoSamplesize;
	uint32_t u32TmoSource[256]; // 256
	uint32_t u32TmoTarget[256]; // 256
};

struct meta_cfd_curve_tmo {
	bool bTmocusManualMode;
	uint16_t u16TmoSamplesize;
	uint32_t u32TmoSource[256]; // 256
	uint32_t u32TmoTarget[256]; // 256
	uint32_t u32State;
	struct meta_cfd_tmo_normal_code stNormalCode;
	struct meta_cfd_tmo_normal_nits stNormalNits;
};

struct meta_cfd_curve_3dlut {
	uint16_t u32Entries[4913*3]; // 4913*3
	uint32_t u32Size;
	uint32_t u32State; // m_pq_cfd_curve_state
	uint32_t enable;
};

struct meta_cfd_curve_common {
	uint32_t u32Entries[1024]; // eo, oe, oo all max 1024
	uint32_t u32Size;
	uint32_t u32State; // m_pq_cfd_curve_state
};

struct meta_cfd_curve_matrix {
	__s32 u32Entries[9]; // 9
	uint32_t u32Size;
	uint32_t u32State; // m_pq_cfd_curve_state
};

struct meta_cfd_curve_hdr {
	struct meta_cfd_curve_common eotf;
	struct meta_cfd_curve_common ootf;
	struct meta_cfd_curve_common oetf;
};

struct meta_cfd_calman_curve {
	struct meta_cfd_curve_hdr hdr_curve;
	struct meta_cfd_curve_common eotf;
	struct meta_cfd_curve_common oetf;
	struct meta_cfd_curve_matrix matrix;
	struct meta_cfd_curve_3dlut rgb_3dlut;
};

struct meta_cfd_curve_hdr_tone_mapping {
	struct meta_cfd_curve_hdr hdr;
	struct meta_cfd_curve_tmo tmo;
};

struct meta_cfd_window_curve {
	struct meta_cfd_curve_hdr_tone_mapping hdr_curve;
	uint32_t u32CfdHdrCurveMode; // 0: hdr eo, oe, oo; 1: tmo curve
	struct meta_cfd_curve_common eotf;
	struct meta_cfd_curve_common oetf;
	struct meta_cfd_curve_matrix matrix;
	struct meta_cfd_curve_3dlut rgb_3dlut;
	uint32_t u32CurveState[8]; // EN_PQ_CFD_STATE_CURVE
};

struct meta_ulmpi_read_post1d_cmd {
	uint32_t Size;
	uint16_t PackedData[3072];
};

struct meta_ulmpi_read_pre1d_cmd {
	uint32_t Size;
	uint16_t PackedData[1024];
};

struct meta_ulmpi_read_3d_cmd {
	uint32_t Size;
	uint8_t PackedData[4913*6];
};

struct meta_ulmpi_read_matrix_cmd {
	uint32_t Size;
	int32_t PackedData[9];
};

#define META_DISP_NON_LINEAR_SCALING_VERSION (0)
struct meta_disp_non_linear_scaling {
	__u8 u8HNL_En;
	__u16 u16SourceRatio;
	__u16 u16TargetRatio;
};

#define META_PQ_DISPLAY_WM_INFO_VERSION (1)
struct meta_pq_display_wm_info {
	struct meta_pq_wm_config wm_config;
	struct meta_pq_wm_result wm_result;
};

/* refer to m_pq_dv_info */
#define META_PQ_DV_INFO_VERSION (2)
#define META_PQ_DV_PYR_NUM      (7)
/* refer to struct m_pq_dv_pyr */
struct meta_pq_dv_pyr {
	bool  valid;
	__u8  frame_num;
	__u8  rw_diff;
	__u32 frame_pitch[META_PQ_DV_PYR_NUM];
	__u64 addr[META_PQ_DV_PYR_NUM];
	__u32 width[META_PQ_DV_PYR_NUM];
	__u32 height[META_PQ_DV_PYR_NUM];
};

/* refer to struct ST_DV_AMBIENT_INFO */
struct meta_pq_dv_ambient {
	__u32 u32Version;
	__u16 u16Length;

	bool bIsModeValid;
	__u32 u32Mode;
	bool bIsFrontLuxValid;
	signed long long s64FrontLux;
	bool  bIsRearLumValid;
	signed long long s64RearLum;
	bool bIsWhiteXYValid;
	__u32 u32WhiteX;
	__u32 u32WhiteY;

	bool bIsLsEnabledValid;
	bool bLsEnabled;
};

/* refer to struct m_pq_dv_pr_ctrl */
struct meta_pq_dv_pr_ctrl {
	bool en;
	__u32 fe_in_width;  /* input width of front end */
	__u32 fe_in_height; /* input height of front end */
	__u32 c1_in_width;  /* input width of core1 */
	__u32 c1_in_height; /* input height of core1 */
};

/* struct m_pq_dv_info */
struct meta_pq_dv_info {
	struct meta_pq_dv_pyr pyr;
	struct meta_pq_dv_pr_ctrl pr_ctrl;
	struct meta_pq_dv_ambient ambient;
};

/*
 * Version 1 : AISR
 */
#define META_PQ_THERMAL_INFO_VERSION (2)
/* brief PQ thermal info.
 *
 * EN_PQU_METATAG_PQU_THERMAL_INFO
 */
enum EN_PQ_THERMAL_STATE_AISR {
	E_PQ_THERMAL_STATE_AISR_ON,
	E_PQ_THERMAL_STATE_AISR_KEEP,
	E_PQ_THERMAL_STATE_AISR_HALF,
	E_PQ_THERMAL_STATE_AISR_OFF,
};

enum EN_PQ_ALGO_THERMAL_STATE_AISR {
	E_PQ_ALGO_THERMAL_AISR_ON = 0,
	E_PQ_ALGO_THERMAL_AISR_OFF,
	E_PQ_ALGO_THERMAL_AISR_HALF,
	E_PQ_ALGO_THERMAL_AISR_PROCESS,
	E_PQ_ALGO_THERMAL_AISR_NONE,
	E_PQ_ALGO_THERMAL_AISR_MAX
};

struct meta_pq_thermal {
	enum EN_PQ_THERMAL_STATE_AISR thermal_state_aisr;
	enum EN_PQ_ALGO_THERMAL_STATE_AISR thermal_algo_state_aisr;
};

#define META_PQ_BBD_INFO_VERSION (2)
struct meta_pq_bbd_info {
	__u8 u8Validity;
	__u16 u16LeftOuterPos;
	__u16 u16RightOuterPos;
	__u16 u16LeftInnerPos;
	__u16 u16LeftInnerConf;
	__u16 u16RightInnerPos;
	__u16 u16RightInnerConf;
	__u16 u16CurTopContentY;
	__u16 u16CurBotContentY;
};

#define META_PQ_DV_OUT_FRAME_INFO_VERSION (3)
/* refer to struct dv_gd_info & v4l2_dv_gd_info & MI_DISP_dv_gd_info */
struct meta_pq_dv_gd_info {
	bool valid;
	__u16 gd;
	__u16 delay;
	bool gd_gray_out;
};

/* refer to struct m_pq_dv_l11_info & v4l2_dv_l11_info & MI_DISP_dv_l11_info */
struct meta_pq_dv_l11_info {
	__u8 u8l11Avail;
	__u8 u8WPValid;
	__u8 u8ContentType;
	__u8 u8WhitePoint;
	__u8 u8L11Byte2;
	__u8 u8L11Byte3;

	/* test */
	__u8 u8Stub;
};

/* refer to m_pqu_pq.h   :   m_pq_dv_out_frame_info     */
/*          m_pq.h       :   meta_pq_dv_out_frame_info  */
/*          mi_pq.h      :   MI_PQ_DV_FrameInfo_t       */
/*          mtk-v4l2-pq.h:   v4l2_dv_out_frame_info     */
/** output frame information */
struct meta_pq_dv_out_frame_info {
	bool stub;
	struct meta_pq_dv_gd_info gd_info;
	__u32 dv_pq_hw_ver;
	struct meta_pq_dv_l11_info l_11_Data;
	__u32 ret;
	__u8 u8PictureMode;
	/* lux/lum calculated and reported from DV alg */
	__u16 u16DVFrontLux;        //u16DVFrontLux = s64FrontLux * u16DVFrontLuxScale
	__u16 u16DVFrontLuxScale;
	__u16 u16DVRearLum;         //u16DVRearLum = s64RearLum * u16DVRearLumScale
	__u16 u16DVRearLumScale;
	/* ambient got from user application */
	__s64 s64FrontLux;
	__s64 s64RearLum;
	__u32 u32WhiteX;
	__u32 u32WhiteY;
};

/* refer to m_pq_vsync_info */
#define META_PQ_VSYNC_INFO_VERSION (1)
struct meta_pq_vsync_info {
	__u64 vsync_timestamp;
	__u64 vsync_duration;
};

/* refer to m_pqu_pq.h   :   m_pq_dv_in_frame_info        */
/*          m_pq.h       :   meta_pq_dv_in_frame_info     */
/*          mi_pq.h      :   MI_PQ_DV_InFrameInfo_t       */
/*          mtk-v4l2-pq.h:   v4l2_dv_in_frame_info        */
/** input frame information from user application and debug*/
struct meta_pq_dv_in_frame_info {
	bool ambientDebugEn;
	__u32 u32AmbientMode;
	__s64 s64FrontLux;
	__s64 s64RearLum;
};

#endif
