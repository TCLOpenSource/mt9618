/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _UAPI_MTK_TV_DRM_H_
#define _UAPI_MTK_TV_DRM_H_

#include "drm.h"
#include <stdbool.h>

#define MTK_GEM_CREATE_FLAG_IMPORT 0x80000000
#define MTK_GEM_CREATE_FLAG_EXPORT 0x00000000
#define MTK_GEM_CREATE_FLAG_IN_CMA 0x00000001   /* physical continuous */
#define MTK_GEM_CREATE_FLAG_IN_IOMMU 0x00000002 /* physical discrete */
#define MTK_GEM_CREATE_FLAG_IN_LX 0x00000004
#define MTK_GEM_CREATE_FLAG_IN_MMAP 0x00000008

#define MTK_CTRL_BOOTLOGO_CMD_GETINFO 0x0
#define MTK_CTRL_BOOTLOGO_CMD_DISABLE  0x1

/* pixelshift feature: justscan:Hrange=32,Vrange=16;overscan:Hrange=32,Vrange=10 */
#define PIXELSHIFT_H_MAX (32)
#define PIXELSHIFT_V_MAX (16)
#define MTK_MFC_PLANE_ID_MAX (0xFF)

#define CTRLBIT_MAX_NUM (32)
#define VBO_MAX_LANES (64)
#define PLANE_SUPPORT_FENCE_MAX_NUM (5)

#define MTK_TV_DRM_WINDOW_NUM_MAX       (16)
#define MTK_TV_DRM_PQU_METADATA_COUNT   (2)
#define AUL_AMBIENT_SIZE           (1920)  //32*20*3

#define DRM_NAME_MAX_NUM	(256)

#define MTK_PLANE_PROP_PLANE_TYPE   "Plane_type"
    #define MTK_PLANE_PROP_PLANE_TYPE_GRAPHIC   "Plane_type_GRAPHIC"
    #define MTK_PLANE_PROP_PLANE_TYPE_VIDEO "Plane_type_VIDEO"
    #define MTK_PLANE_PROP_PLANE_TYPE_PRIMARY "Plane_type_PRIMARY"
	#define MTK_PLANE_PROP_PLANE_TYPE_EXT_VIDEO "Plane_type_EXT_VIDEO"
#define MTK_PLANE_PROP_HSTRETCH    "H-Stretch"
    #define MTK_PLANE_PROP_HSTRETCH_DUPLICATE    "HSTRETCH-DUPLICATE"
    #define MTK_PLANE_PROP_HSTRETCH_6TAP8PHASE     "6TAP8PHASE"
    #define MTK_PLANE_PROP_HSTRETCH_4TAP256PHASE   "H4TAP256PHASE"
#define MTK_PLANE_PROP_VSTRETCH    "V-Stretch"
    #define MTK_PLANE_PROP_VSTRETCH_DUPLICATE      "DUPLICATE"
    #define MTK_PLANE_PROP_VSTRETCH_2TAP16PHASE    "2TAP16PHASE"
    #define MTK_PLANE_PROP_VSTRETCH_BILINEAR       "BILINEAR"
    #define MTK_PLANE_PROP_VSTRETCH_4TAP256PHASE   "V4TAP256PHASE"
#define MTK_PLANE_PROP_DISABLE    "DISABLE"
#define MTK_PLANE_PROP_ENABLE     "ENABLE"
#define MTK_PLANE_PROP_SUPPORT    "SUPPORT"
#define MTK_PLANE_PROP_UNSUPPORT  "UNSUPPORT"
#define MTK_PLANE_PROP_HMIRROR    "H-Mirror"
#define MTK_PLANE_PROP_VMIRROR    "V-Mirror"
#define MTK_PLANE_PROP_AFBC_FEATURE "AFBC-feature"
#define MTK_PLANE_PROP_AID_NS    "AID-NS"
#define MTK_PLANE_PROP_AID_SDC    "AID-SDC"
#define MTK_PLANE_PROP_AID_S     "AID-S"
#define MTK_PLANE_PROP_AID_CSP  "AID-CSP"
#define MTK_PLANE_PROP_DST_OSDB0    "OP_DST_OSDB0"
#define MTK_PLANE_PROP_DST_OSDB1    "OP_DST_OSDB1"
#define MTK_PLANE_PROP_DST_INVALID    "DST_INVALID"

#define MTK_GOP_PLANE_PROP_BYPASS	"PlaneBypass"

/* property name: GFX_PQ_DATA
 * property object: plane
 * support plane type: gop
 * type: blob
 *
 * This property is used to set pq data of gop plane
 */
#define MTK_GOP_PLANE_PROP_PQ_DATA "GFX_PQ_DATA"
#define GOP_PQ_BUF_ST_IDX_VERSION (2)

/* property name: GFX_ALPHA_MODE
 * property object: plane
 * support plane type: gop
 * type: blob
 *
 * This property is used to set pq data of gop plane
 */
#define MTK_GOP_PLANE_PROP_ALPHA_MODE "GFX_ALPHA_MODE"

/* property name: GFX_AID_TYPE
 * property object: plane
 * support plane type: gop
 * type: enum
 *
 * This property is used to set aid type of gop plane
 */
#define MTK_GOP_PLANE_PROP_AID_TYPE "GFX_AID_TYPE"

/* property name: GFX_BLENDING_DST
 * property object: plane
 * support plane type: gop
 * type: enum
 *
 * This property is used to set gop blending position in VG-blend
 */
#define MTK_GOP_PLANE_PROP_BLENDING_DST "GFX_OP_BLEND_DST"

/* property name: GFX_CSC
 * property object: plane
 * support plane type: gop
 * type: enum
 *
 * This property is used to set gop csc
 */
#define MTK_GOP_PLANE_PROP_CSC "GFX_CSC"

/* property name: PNL_CURLUM
 * property object: plane
 * support plane type: gop
 * type: range
 * value: 0~INT_MAX
 *
 * This property is used to set panel current luminance of gop plane
 */
#define MTK_GOP_PLANE_PROP_PNL_CURLUM	"PNL_CURLUM"

/* property name: VIDEO_GOP_ZORDER
 * property object: crtc
 * type: enum
 * value: enum
 *
 * This property is used to set the zorder of video and gop
 */
#define MTK_VIDEO_CRTC_PROP_VG_ORDER	"VIDEO_OSDB_ZORDER"
#define MTK_VIDEO_CRTC_PROP_V_OSDB0_OSDB1    "VIDEO_OSDB0_OSDB1"
#define MTK_VIDEO_CRTC_PROP_OSDB0_V_OSDB1    "OSDB0_VIDEO_OSDB1"
#define MTK_VIDEO_CRTC_PROP_OSDB0_OSDB1_V   "OSDB0_OSDB1_VIDEO"
enum video_crtc_vg_order_type {
	VIDEO_OSDB0_OSDB1,
	OSDB0_VIDEO_OSDB1,
	OSDB0_OSDB1_VIDEO,
};

/* property name: GOP_OSDB_LOCATION
 * property object: crtc
 * type: enum
 * value: enum
 *
 * This property is used to set the osdb location
 */

#define MTK_VIDEO_CRTC_PROP_OSDB_LOC	"OSDB_LOCATION"
#define MTK_VIDEO_CRTC_PROP_PQGAMMA_OSDB_LD_PADJ	"PQGAMMA_OSDB_LD_PADJ"
#define MTK_VIDEO_CRTC_PROP_PQGAMMA_LD_OSDB_PADJ	"PQGAMMA_LD_OSDB_PADJ"
#define MTK_VIDEO_CRTC_PROP_OSDB_PQGAMMA_LD_PADJ	"OSDB_PQGAMMA_LD_PADJ"
#define MTK_VIDEO_CRTC_PROP_OSDB_LD_PQGAMMA_PADJ	"OSDB_LD_PQGAMMA_PADJ"
#define MTK_VIDEO_CRTC_PROP_LD_PQGAMMA_OSDB_PADJ	"LD_PQGAMMA_OSDB_PADJ"
#define MTK_VIDEO_CRTC_PROP_LD_OSDB_PQGAMMA_PADJ	"LD_OSDB_PQGAMMA_PADJ"
#define MTK_VIDEO_CRTC_PROP_LOCATION_MAX	"LOCATION_MAX"

enum video_crtc_osdb_loc {
	PQGAMMA_OSDB_LD_PADJ,
	PQGAMMA_LD_OSDB_PADJ,
	OSDB_PQGAMMA_LD_PADJ,
	OSDB_LD_PQGAMMA_PADJ,
	LD_PQGAMMA_OSDB_PADJ,
	LD_OSDB_PQGAMMA_PADJ,
	LOCATION_MAX,
};

/* property name: MUTE_SCREEN
 * property object: plane
 * support plane type: video
 * type: bool
 * value: 0 or 1
 *
 * This property is used to set current plane mute or note
 */
#define MTK_VIDEO_PLANE_PROP_MUTE_SCREEN	"MUTE_SCREEN"

/* property name: MUTE_COLOR
 * property object: crtc
 * type: blob
 * value: struct drm_mtk_mute_color
 *
 * This property is used to set the mute color
 */
#define MTK_VIDEO_CRTC_PROP_MUTE_COLOR		"MUTE_COLOR"

#define MTK_VIDEO_PLANE_PROP_SNOW_SCREEN      "SNOW_SCREEN"

/* property name: BACKGROUND_COLOR
 * property object: crtc
 * type: blob
 * value: struct drm_mtk_bg_color
 *
 * This property is used to set the background color
 */
#define MTK_VIDEO_CRTC_PROP_BG_COLOR		"BACKGROUND_COLOR"

/* property name: VIDEO_PLANE_TYPE
 * property object: plane
 * support plane type: video
 * type: ENUM
 * value: reference enum drm_mtk_video_plane_type
 *
 * This property is used to distinguish plane in which buffer
 */
#define PROP_NAME_VIDEO_PLANE_TYPE    "VIDEO_PLANE_TYPE"

/* property name: META_FD
 * property object: plane
 * support plane type: video
 * type: range
 * value: 0~INT_MAX
 *
 * This property is used to pass metadata fd.
 */
#define PROP_NAME_META_FD    "META_FD"

/* property name: BUF_MODE
 * property object: plane
 * support plane type: video
 * type: ENUM
 * value: reference enum drm_mtk_vplane_buf_mode
 *
 * This property is used to determine buffer mode.
 * Only take effect when prop "VIDEO_SRC0_PATH" set to MEMC.
 *
 */
#define PROP_VIDEO_PLANE_PROP_BUF_MODE    "BUF_MODE"

/* property name: DISP_MODE
 * property object: plane
 * support plane type: video
 * type: ENUM
 * value: reference enum vplane_disp_mode_enum_list
 *
 * This property is used to determine disp mode.
 *
 */
#define PROP_VIDEO_PLANE_PROP_DISP_MODE    "DISP_MODE"

/* property name: DISP_WIN_TYPE
 * property object: plane
 * support plane type: video
 * type: ENUM
 * value: reference enum vplane_disp_win_type_enum_list
 *
 * This property is used to determine disp win type.
 *
 */
#define PROP_VIDEO_PLANE_PROP_DISP_WIN_TYPE    "DISP_WIN_TYPE"

/* property name: FREEZE
 * property object: plane
 * support plane type: video
 * type: range
 * value: 0~1,  0: disable freeze,  1: enable freeze
 *
 * This property is used to set buffer freeze.
 * Should only be set in legacy mode.
 */
#define PROP_NAME_FREEZE    "FREEZE"
#define MTK_VIDEO_PLANE_PROP_INPUT_VFREQ "DISPLAY_INPUT_VFREQ"
#define MTK_VIDEO_PLANE_PROP_INPUT_SOURCE_VFREQ "INPUT_SOURCE_VFREQ"

/* property name: WINDOW_PQ
 * property object: plane
 * support crtc type: video
 * type: BLOB
 * value: string
 *
 * This property is used to set Window PQ.
 * All Window PQ parameters are expressed in a long string.
 */
#define PROP_VIDEO_CRTC_PROP_WINDOW_PQ    "WINDOW_PQ"

/* property name: GLOBAL_PQ
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: string
 *
 * This property is used to set Global PQ.
 * All Global PQ parameters are expressed in a long string.
 */
#define PROP_VIDEO_CRTC_PROP_GLOBAL_PQ    "GLOBAL_PQ"

/* property name: DEMURA_CONFIG
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_demura_cuc_config
 *
 * This property is used to set crtc-video demura.
 */
#define MTK_CRTC_PROP_DEMURA_CONFIG	"DEMURA_CONFIG"
struct drm_mtk_demura_config {
	__u8  id;
	bool  disable;
	__u32 binary_size;
	__u8  binary_data[];
};

/* property name: PQMAP_NODE_ARRAY
 * property object: plane
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_tv_pqmap_node_array
 *
 * This property is used to set PQmap nodes.
 */
#define MTK_VIDEO_PLANE_PROP_PQMAP_NODES_ARRAY          "PQMAP_NODE_ARRAY"

/* property name: BACKLIGHT_RANGE
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_range_value
 *
 * This property is used to set crtc-video backlight.
 */
#define MTK_CRTC_PROP_BACKLIGHT_RANGE_CONFIG	"BACKLIGHT_RANGE"
struct drm_mtk_range_value {
	bool  valid;
	__u32 max_value;
	__u32 min_value;
	__u32 value;
};

/* property name: LIVETONE_RANGE
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_range_value
 *
 * This property is used to set crtc-video live tone.
 */
#define MTK_CRTC_PROP_LIVETONE_RANGE_CONFIG	"LIVETONE_RANGE"

/* property name: PQMAP_NODE_ARRAY
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_tv_pqmap_node_array
 *
 * This property is used to set PQmap nodes.
 */
#define MTK_CRTC_PROP_PQMAP_NODES_ARRAY         "PQMAP_NODE_ARRAY"

/* property name: CFD_CSC_RENDER_IN
 * property object: crtc
 * support crtc type: video
 * type: BLOB
 * value: struct drm_mtk_cfd_CSC
 *
 * This property is used to set render in CSC settings.
 */
#define MTK_CRTC_PROP_CFD_CSC_RENDER_IN         "CFD_CSC_RENDER_IN"

#define MTK_CRTC_PROP_LDM_STATUS          "LDM_STATUS"
#define MTK_CRTC_PROP_LDM_STATUS_INIT            "LDM_STATUS_INIT"
#define MTK_CRTC_PROP_LDM_STATUS_ENABLE          "LDM_STATUS_ENABLE"
#define MTK_CRTC_PROP_LDM_STATUS_DISABLE         "LDM_STATUS_DISABLE"
#define MTK_CRTC_PROP_LDM_STATUS_DEINIT          "LDM_STATUS_DEINIT"
#define MTK_CRTC_PROP_LDM_LUMA            "LDM_LUMA"
#define MTK_CRTC_PROP_LDM_ENABLE          "LDM_ENABLE"
#define MTK_CRTC_PROP_LDM_STRENGTH        "LDM_STRENGTH"
#define MTK_CRTC_PROP_LDM_DATA            "LDM_DATA"
#define MTK_CRTC_PROP_LDM_DEMO_PATTERN    "LDM_DEMO_PATTERN"
#define MTK_CRTC_PROP_LDM_SW_SET_CTRL		"LDM_SW_SET_CTRL"
#define MTK_CRTC_PROP_LDM_AUTO_LD		"AUTO_LD"
#define MTK_CRTC_PROP_LDM_XTendedRange	"XTendedRange"
#define MTK_CRTC_PROP_LDM_VRR_SEAMLESS	"VRR_SEAMLESS"

#define MTK_CRTC_PROP_LDM_INIT	"LDM_INIT"
#define MTK_CRTC_PROP_LDM_SUSPEND	"LDM_SUSPEND"
#define MTK_CRTC_PROP_LDM_RESUME		"LDM_RESUME"
#define MTK_CRTC_PROP_LDM_SUSPEND_RESUME_TEST		"LDM_SUSPEND_RESUME_TEST"

/****pixelshift crtc prop******/
#define MTK_CRTC_PROP_PIXELSHIFT_ENABLE          "PIXELSHIFT_ENABLE"
#define MTK_CRTC_PROP_PIXELSHIFT_OSD_ATTACH      "PIXELSHIFT_OSD_ATTACH"
#define MTK_CRTC_PROP_PIXELSHIFT_TYPE            "PIXELSHIFT_TYPE"
#define MTK_CRTC_PROP_PIXELSHIFT_PRE_JUSTSCAN    "PIXELSHIFT_PRE_JUSTSCAN"
#define MTK_CRTC_PROP_PIXELSHIFT_PRE_OVERSCAN    "PIXELSHIFT_PRE_OVERSCAN"
#define MTK_CRTC_PROP_PIXELSHIFT_POST_JUSTSCAN   "PIXELSHIFT_POST_JUSTSCAN"
#define MTK_CRTC_PROP_PIXELSHIFT_H               "PIXELSHIFT_H"
#define MTK_CRTC_PROP_PIXELSHIFT_V               "PIXELSHIFT_V"

/****memc plane prop******/
#define MTK_PLANE_PROP_MEMC_LEVEL_MODE         "MEMC_LEVEL_MODE"
#define MTK_VIDEO_PROP_MEMC_LEVEL_OFF   "MEMC_LEVEL_OFF"
#define MTK_VIDEO_PROP_MEMC_LEVEL_LOW   "MEMC_LEVEL_LOW"
#define MTK_VIDEO_PROP_MEMC_LEVEL_MID   "MEMC_LEVEL_MID"
#define MTK_VIDEO_PROP_MEMC_LEVEL_HIGH   "MEMC_LEVEL_HIGH"
#define MTK_PLANE_PROP_MEMC_GAME_MODE     "MEMC_GAME_MODE"
#define MTK_VIDEO_PROP_MEMC_GAME_OFF   "MEMC_GAME_OFF"
#define MTK_VIDEO_PROP_MEMC_GAME_ON   "MEMC_GAME_ON"
#define MTK_PLANE_PROP_MEMC_MISC_TYPE    "MEMC_MISC_TYPE"
#define MTK_VIDEO_PROP_MEMC_MISC_NULL   "MEMC_MISC_NULL"
#define MTK_VIDEO_PROP_MEMC_MISC_INSIDE   "MEMC_MISC_INSIDE"
#define MTK_VIDEO_PROP_MEMC_MISC_INSIDE_60HZ   "MEMC_MISC_INSIDE_60HZ"
#define MTK_PLANE_PROP_MEMC_PATTERN	"MEMC_PATTERN"
#define MTK_VIDEO_PROP_MEMC_NULL_PAT   "MEMC_NULL_PAT"
#define MTK_VIDEO_PROP_MEMC_OPM_PAT   "MEMC_OPM_PAT"
#define MTK_VIDEO_PROP_MEMC_MV_PAT   "MEMC_MV_PAT"
#define MTK_VIDEO_PROP_MEMC_END_PAT   "MEMC_END_PAT"
#define MTK_PLANE_PROP_MEMC_DECODE_IDX_DIFF     "MEMC_DECODE_IDX_DIFF"
#define MTK_PLANE_PROP_MEMC_STATUS    "MEMC_STATUS"
#define MTK_PLANE_PROP_MEMC_TRIG_GEN   "MEMC_TRIG_GEN"
#define MTK_PLANE_PROP_MEMC_RV55_INFO    "MEMC_RV55_INFO"
#define MTK_PLANE_PROP_MEMC_INIT_VALUE_FOR_RV55    "MEMC_INIT_VALUE_FOR_RV55"


#define MTK_VIDEO_PROP_MEMC_MISC_A_NULL   "MEMC_MISC_A_NULL"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE   "MEMC_MISC_A_MEMCINSIDE"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE_60HZ   "MEMC_MISC_A_60HZ"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE_240HZ   "MEMC_MISC_A_240HZ"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE_4K1K_120HZ   "MEMC_MISC_A_4K1K_120HZ"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE_KEEP_OP_4K2K   "MEMC_MISC_A_KEEP_OP_4K2K"
#define MTK_VIDEO_PROP_MEMC_MISC_A_MEMCINSIDE_4K_HALFK_240HZ   "MEMC_MISC_A_4K_HALFK_240HZ"

enum ldm_crtc_status {
	MTK_CRTC_LDM_STATUS_INIT = 1,   /* lDM status init */
	MTK_CRTC_LDM_STATUS_ENABLE = 2, /* LDM status enable */
	MTK_CRTC_LDM_STATUS_DISABLE,    /* LDM status disable */
	MTK_CRTC_LDM_STATUS_DEINIT,     /* LDM status deinit */
};

enum drm_mtk_plane_type {
	MTK_DRM_PLANE_TYPE_GRAPHIC = 0,
	MTK_DRM_PLANE_TYPE_VIDEO,
	MTK_DRM_PLANE_TYPE_PRIMARY,
	MTK_DRM_PLANE_TYPE_MAX,
};

enum drm_mtk_video_plane_type {
	MTK_VPLANE_TYPE_NONE = 0,
	MTK_VPLANE_TYPE_MEMC,
	MTK_VPLANE_TYPE_MULTI_WIN,
	MTK_VPLANE_TYPE_SINGLE_WIN1,
	MTK_VPLANE_TYPE_SINGLE_WIN2,
	MTK_VPLANE_TYPE_MAX,
};

enum drm_mtk_vplane_buf_mode {
	MTK_VPLANE_BUF_MODE_NONE = 0,
	MTK_VPLANE_BUF_MODE_SW,     /* time sharing */
	MTK_VPLANE_BUF_MODE_HW,     /* legacy */
	MTK_VPLANE_BUF_MODE_BYPASSS,
	MTK_VPLANE_BUF_MODE_MAX,
};

enum drm_mtk_vplane_disp_mode {
	MTK_VPLANE_DISP_MODE_NONE = 0,
	MTK_VPLANE_DISP_MODE_SW,
	MTK_VPLANE_DISP_MODE_HW,
	MTK_VPLANE_DISP_MODE_MAX,
};

enum drm_mtk_vplane_disp_win_type {
	MTK_VPLANE_DISP_WIN_TYPE_NONE = 0,
	MTK_VPLANE_DISP_WIN_TYPE_NO_FRC_VGSYNC,
	MTK_VPLANE_DISP_WIN_TYPE_FRC_VGSYNC,
	MTK_VPLANE_DISP_WIN_TYPE_MAX,
};

enum DRM_VB_VERSION_E {
	EN_VB_VERSION_TS_V0 = 0,
	EN_VB_VERSION_TS_V1,
	EN_VB_VERSION_LEGACY_V0,
	EN_VB_VERSION_LEGACY_V1,
	EN_VB_VERSION_MAX,
};

enum drm_mtk_crtc_type {
	MTK_DRM_CRTC_TYPE_VIDEO = 0,
	MTK_DRM_CRTC_TYPE_GRAPHIC,
	MTK_DRM_CRTC_TYPE_EXT_VIDEO,
	MTK_DRM_CRTC_TYPE_MAX,
};

enum drm_mtk_encoder_type {
	MTK_DRM_ENCODER_TYPE_VIDEO = 0,
	MTK_DRM_ENCODER_TYPE_GRAPHIC,
	MTK_DRM_ENCODER_TYPE_EXT_VIDEO,
	MTK_DRM_ENCODER_TYPE_MAX,};

enum drm_mtk_connector_type {
	MTK_DRM_CONNECTOR_TYPE_VIDEO = 0,
	MTK_DRM_CONNECTOR_TYPE_GRAPHIC,
	MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO,
	MTK_DRM_CONNECTOR_TYPE_MAX,
};

/* Define the panel vby1 metadata feature */
enum drm_en_vbo_ctrlbit_feature {
	E_VBO_CTRLBIT_NO_FEATURE,
	E_VBO_CTRLBIT_GLOBAL_FRAMEID,
	E_VBO_CTRLBIT_HTOTAL,
	E_VBO_CTRLBIT_VTOTAL,
	E_VBO_CTRLBIT_HSTART_POS,
	E_VBO_CTRLBIT_VSTART_POS,
	E_VBO_CTRLBIT_CBVF,
	E_VBO_CTRLBIT_NUM,
};

enum DRM_CFD_DATA_FORMAT_E {
	EN_CFD_DATA_FORMAT_RGB = 0,
	EN_CFD_DATA_FORMAT_YUV422,
	EN_CFD_DATA_FORMAT_YUV444,
	EN_CFD_DATA_FORMAT_YUV420,
	EN_CFD_DATA_FORMAT_MAX,
};

enum DRM_CFD_COLOR_RANGE_E {
	EN_CFD_COLOR_RANGE_LIMIT = 0,
	EN_CFD_COLOR_RANGE_FULL,
	EN_CFD_COLOR_RANGE_MAX,
};

enum DRM_CFD_COLOR_SPACE_E {
	EN_CFD_COLOR_SPACE_BT601 = 0,
	EN_CFD_COLOR_SPACE_BT709,
	EN_CFD_COLOR_SPACE_BT2020,
	EN_CFD_COLOR_SPACE_DCI_P3,
	EN_CFD_COLOR_SPACE_ADOBE_RGB,
	EN_CFD_COLOR_SPACE_MAX,
};

/** \brief pixel monitor position */
enum drm_pxm_point {
	E_DRM_PXM_POINT_PRE_IP2_0_INPUT = 0,
	E_DRM_PXM_POINT_PRE_IP2_1_INPUT,
	E_DRM_PXM_POINT_IP2_INPUT,
	E_DRM_PXM_POINT_IP2_HDR_INPUT,
	E_DRM_PXM_POINT_VOP_INPUT,
	E_DRM_PXM_POINT_TCON_AFTER_OSD,
	E_DRM_PXM_POINT_TCON_BEFORE_MOD,
	E_DRM_PXM_POINT_GFX,
	E_DRM_PXM_POINT_TCON_DELTA_PATH,
	E_DRM_PXM_POINT_DV_HW5_SCDV,
	E_DRM_PXM_POINT_DV_HW5_SCTCON,
	E_DRM_PXM_POINT_MAX
};

/* Define the panel vby1 tx mute feature */
typedef struct  {
	bool bEnable;
	__u8 u8ConnectorID; /* 0: Video; 1: Delta Video; 2: GFX */
} drm_st_tx_mute_info;

typedef struct  {
	bool  bEnable;
	bool  bLatchMode; /* bLatch; */
	__u32 u32Red;
	__u32 u32Green;
	__u32 u32Blue;
	__u8 u8ConnectorID; /* 0: Video; 1: Delta Video; 2: GFX */
} drm_st_pixel_mute_info;

typedef struct  {
	bool bEnable;
	__u8 u8ConnectorID; /* 0: Video; 1: Delta Video; 2: GFX */
} drm_st_backlight_mute_info;

typedef struct  {
	bool  bEnable;
	bool  bLatchMode; /* bLatch; */
	__u32 u32Red;
	__u32 u32Green;
	__u32 u32Blue;
} drm_st_global_mute_info;

typedef struct {
	__u16 de_width;
	__u16 de_height;
	__u64 typ_dclk;
	__u32 vfreq;
	__u16 de_hstart;
	__u16 de_vstart;
} drm_st_pnl_mode_timing;

struct drm_mtk_global_mute_ctrl_info {
	drm_st_pnl_mode_timing pre_mode_info;
	drm_st_pnl_mode_timing new_mode_info;
	bool global_mute_enable;
};

struct drm_mtk_panel_info {
	char panel_name[DRM_NAME_MAX_NUM];
	unsigned int panel_inch_size;
};

struct drm_mtk_panel_max_framerate {
	unsigned int typ_max_framerate;
	unsigned int dlg_max_framerate;
};

/*****panel related properties - CRTC*****/
#define MTK_CRTC_PROP_FRAMESYNC_PLANE_ID "DISPLAY_FRAMESYNC_PLANE_ID"
#define MTK_CRTC_PROP_FRAMESYNC_FRC_RATIO "DISPLAY_FRAMESYNC_FRC_RATIO"
#define MTK_CRTC_PROP_FRAMESYNC_MODE "DISPLAY_FRAMESYNC_MODE"
#define MTK_CRTC_PROP_LOW_LATENCY_MODE "DISPLAY_LOWLATENCY_MODE"
#define MTK_CRTC_PROP_FRC_TABLE "DISPLAY_FRC_TABLE"
#define MTK_CRTC_PROP_DISPLAY_TIMING_INFO "DISPLAY_TIMING_INFO"
#define MTK_CRTC_PROP_FREERUN_TIMING    "SET_FREERUN_TIMING"
#define MTK_CRTC_PROP_FORCE_FREERUN    "FORCE_FREERUN"
#define MTK_CRTC_PROP_FREERUN_VFREQ    "FREERUN_VFREQ"
#define MTK_CRTC_PROP_VIDEO_LATENCY    "VIDEO_LATENCY"
#define MTK_CRTC_PROP_PATTERN_GENERATE	"PATTERN_GENERATE"
#define MTK_CRTC_PROP_NO_SIGNAL "NO_SIGNAL"

/*****graphic related properties - CRTC*****/
#define MTK_CRTC_PROP_VG_SEPARATE "GRAPHIC_MODE"

/*****common related properties - CRTC*****/
#define MTK_CRTC_COMMON_PROP_CRTC_TYPE "CRTC_TYPE"
	#define MTK_CRTC_COMMON_MAIN_PATH "MAIN_PATH"
	#define MTK_CRTC_COMMON_GRAPHIC_PATH "GRAPHIC_PATH"
	#define MTK_CRTC_COMMON_EXT_VIDEO_PATH "EXT_VIDEO_PATH"

/*****panel related properties - CONNECTOR*****/
#define MTK_CONNECTOR_PROP_PNL_DBG_LEVEL "PNL_DBG_LEVEL"
#define MTK_CONNECTOR_PROP_PNL_OUTPUT "PNL_OUTPUT"
#define MTK_CONNECTOR_PROP_PNL_SWING_LEVEL "PNL_SWING_LEVEL"
#define MTK_CONNECTOR_PROP_PNL_FORCE_PANEL_DCLK "PNL_FORCE_PANEL_DCLK"
#define MTK_CONNECTOR_PROP_PNL_FORCE_PANEL_HSTART "PNL_FORCE_PANEL_HSTART"
#define MTK_CONNECTOR_PROP_PNL_OUTPUT_PATTERN "PNL_OUTPUT_PATTERN"
#define MTK_CONNECTOR_PROP_PNL_MIRROR_STATUS "PNL_MIRROR_STATUS"
#define MTK_CONNECTOR_PROP_PNL_SSC_EN "PNL_SSC_EN"
#define MTK_CONNECTOR_PROP_PNL_SSC_FMODULATION "PNL_SSC_FMODULATION"
#define MTK_CONNECTOR_PROP_PNL_SSC_RDEVIATION "PNL_SSC_RDEVIATION"
#define MTK_CONNECTOR_PROP_PNL_OVERDRIVER_ENABLE "PNL_OVERDRIVER_ENABLE"
#define MTK_CONNECTOR_PROP_PNL_PANEL_SETTING "PNL_PANEL_SETTING"
#define MTK_CONNECTOR_PROP_PNL_INFO "PNL_INFO"
#define MTK_CONNECTOR_PROP_PNL_PARSE_OUT_INFO_FROM_DTS "PARSE_OUT_INFO_FROM_DTS"
#define MTK_CONNECTOR_PROP_PNL_VBO_CTRLBITS "PNL_VBO_CTRLBITS"
#define MTK_CONNECTOR_PROP_PNL_TX_MUTE_EN "PNL_TX_MUTE_EN"
#define MTK_CONNECTOR_PROP_PNL_SWING_VREG "PNL_SWING_VREG"
#define MTK_CONNECTOR_PROP_PNL_PRE_EMPHASIS "PNL_PRE_EMPHASIS"
#define MTK_CONNECTOR_PROP_PNL_SSC "PNL_SSC"
#define MTK_CONNECTOR_PROP_PNL_OUTPUT_MODEL "OUT_MODEL"
#define MTK_CONNECTOR_PROP_PNL_CHECK_DTS_OUTPUT_TIMING "PNL_CHECK_DTS_OUTPUT_TIMING"
#define MTK_CONNECTOR_PROP_PNL_CHECK_FRAMESYNC_MLOAD "PNL_CHECK_FRAMESYNC_MLOAD"


/*****common related properties - CONNECTOR*****/
#define MTK_CONNECTOR_COMMON_PROP_CONNECTOR_TYPE "CONNECTOR_TYPE"
#define MTK_CONNECTOR_COMMON_MAIN_PATH "MAIN_PATH"
#define MTK_CONNECTOR_COMMON_GRAPHIC_PATH "GRAPHIC_PATH"
#define MTK_CONNECTOR_COMMON_EXT_VIDEO_PATH "EXT_VIDEO_PATH"
#define MTK_CONNECTOR_COMMON_PROP_PNL_TX_MUTE_EN "COMMON_PNL_VBO_TX_MUTE_EN"
#define MTK_CONNECTOR_COMMON_PROP_PNL_PIXEL_MUTE_EN "COMMON_PNL_PIXEL_MUTE_EN"
#define MTK_CONNECTOR_COMMON_PROP_BACKLIGHT_MUTE_EN "COMMON_PNL_BACKLIGHT_MUTE_EN"
#define MTK_CONNECTOR_COMMON_PROP_HMIRROR_EN "COMMON_PNL_HMIRROR_EN"
#define MTK_CONNECTOR_COMMON_PROP_PNL_VBO_CTRLBITS "COMMON_PNL_VBO_CTRLBITS"
#define MTK_CONNECTOR_COMMON_PROP_PNL_GLOBAL_MUTE_EN "COMMON_PNL_GLOBAL_MUTE_EN"

enum drm_mtk_hmirror {
	MTK_DRM_HMIRROR_DISABLE = 0,
	MTK_DRM_HMIRROR_ENABLE,
};

enum drm_mtk_vmirror {
	MTK_DRM_VMIRROR_DISABLE = 0,
	MTK_DRM_VMIRROR_ENABLE,
};

enum drm_mtk_hstretch {
	MTK_DRM_HSTRETCH_DUPLICATE = 0,
	MTK_DRM_HSTRETCH_6TAP8PHASE,
	MTK_DRM_HSTRETCH_4TAP256PHASE,
};

enum drm_mtk_vstretch {
	MTK_DRM_VSTRETCH_2TAP16PHASE = 0,
	MTK_DRM_VSTRETCH_DUPLICATE,
	MTK_DRM_VSTRETCH_BILINEAR,
	MTK_DRM_VSTRETCH_4TAP,
};

enum drm_mtk_plane_aid_type {
	MTK_DRM_AID_TYPE_NS = 0,
	MTK_DRM_AID_TYPE_SDC,
	MTK_DRM_AID_TYPE_S,
	MTK_DRM_AID_TYPE_CSP,
};

enum drm_mtk_plane_blending_dst {
	MTK_DRM_DST_OSDB0 = 0,
	MTK_DRM_DST_OSDB1,
	MTK_DRM_DST_INVALID,
};

enum drm_mtk_plane_csc {
	MTK_DRM_CSC_Y2R_709_F2F = 0,
	MTK_DRM_CSC_Y2R_709_F2L,
	MTK_DRM_CSC_Y2R_709_L2F,
	MTK_DRM_CSC_Y2R_709_L2L,
	MTK_DRM_CSC_Y2R_601_F2F,
	MTK_DRM_CSC_Y2R_601_F2L,
	MTK_DRM_CSC_Y2R_601_L2F,
	MTK_DRM_CSC_Y2R_601_L2L,
	MTK_DRM_CSC_R2Y_709_F2F,
	MTK_DRM_CSC_R2Y_709_F2L,
	MTK_DRM_CSC_R2Y_709_L2F,
	MTK_DRM_CSC_R2Y_709_L2L,
	MTK_DRM_CSC_R2Y_601_F2F,
	MTK_DRM_CSC_R2Y_601_F2L,
	MTK_DRM_CSC_R2Y_601_L2F,
	MTK_DRM_CSC_R2Y_601_L2L,
	MTK_DRM_CSC_NONE,
};

enum drm_mtk_plane_bypass {
	MTK_DRM_PLANE_BYPASS_DISABLE = 0,
	MTK_DRM_PLANE_BYPASS_ENABLE,
};

enum drm_mtk_support_feature {
	MTK_DRM_SUPPORT_FEATURE_YES = 0,
	MTK_DRM_SUPPORT_FEATURE_NO,
};

enum video_crtc_pixelshift_type {
	VIDEO_CRTC_PIXELSHIFT_PRE_JUSTSCAN,  /* pixelshift pre-demura justscan */
	VIDEO_CRTC_PIXELSHIFT_PRE_OVERSCAN,  /* pixelshift pre-demura overscan */
	VIDEO_CRTC_PIXELSHIFT_POST_JUSTSCAN, /* pixelshift pose-demura justscan */
};

enum video_crtc_memc_a_type {
	VIDEO_CRTC_MISC_A_NULL,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE_60HZ,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE_240HZ,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE_4K1K_120HZ,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE_KEEP_OP_4K2K,
	VIDEO_CRTC_MISC_A_MEMC_INSIDE_4K_HALFK_240HZ,
};

enum video_crtc_memc_caps {
	VIDEO_CRTC_MEMC_CAPS_NOT_SUPPORTED, /* chip caps */
	VIDEO_CRTC_MEMC_CAPS_SUPPORTED,     /* chip caps */
};

enum video_plane_memc_pattern {
	VIDEO_PLANE_MEMC_NULL_PAT,
	VIDEO_PLANE_MEMC_OPM_PAT,
	VIDEO_PLANE_MEMC_MV_PAT,
	VIDEO_PLANE_MEMC_END_PAT,
};

enum video_plane_memc_level {
	VIDEO_PLANE_MEMC_LEVEL_OFF,
	VIDEO_PLANE_MEMC_LEVEL_LOW,
	VIDEO_PLANE_MEMC_LEVEL_MID,
	VIDEO_PLANE_MEMC_LEVEL_HIGH,
};

enum video_plane_memc_game_mode {
	VIDEO_PLANE_MEMC_GAME_OFF,
	VIDEO_PLANE_MEMC_GAME_ON,
};

enum video_plane_memc_misc_type {
	VIDEO_PLANE_MEMC_MISC_NULL,
	VIDEO_PLANE_MEMC_MISC_INSIDE,
	VIDEO_PLANE_MEMC_MISC_INSIDE_60HZ,
};

enum video_crtc_frame_sync_mode {
	VIDEO_CRTC_FRAME_SYNC_FREERUN = 0,
	VIDEO_CRTC_FRAME_SYNC_VTTV,
	VIDEO_CRTC_FRAME_SYNC_FRAMELOCK,
	VIDEO_CRTC_FRAME_SYNC_VTTPLL,
	VIDEO_CRTC_FRAME_SYNC_MAX,
};

enum crtc_type {
	CRTC_MAIN_PATH,
	CRTC_GRAPHIC_PATH,
	CRTC_EXT_VIDEO_PATH,
};

enum connector_type {
	CONNECTOR_MAIN_PATH,
	CONNECTOR_GRAPHIC_PATH,
	CONNECTOR_EXT_VIDEO_PATH,
};

enum connector_out_type {
	OUT_MODEL_VG_BLENDED,
	OUT_MODEL_VG_SEPARATED,
	OUT_MODEL_VG_SEPARATED_W_EXTDEV,
	OUT_MODEL_ONLY_EXTDEV,
	OUT_MODEL_MAX,
};

enum drm_en_pnl_output_format {
	E_DRM_OUTPUT_RGB,
	E_DRM_OUTPUT_YUV444,
	E_DRM_OUTPUT_YUV422,
	E_DRM_OUTPUT_YUV420,
	E_DRM_OUTPUT_ARGB8101010,
	E_DRM_OUTPUT_ARGB8888_W_DITHER,
	E_DRM_OUTPUT_ARGB8888_W_ROUND,
	E_DRM_OUTPUT_ARGB8888_MODE0,
	E_DRM_OUTPUT_ARGB8888_MODE1,
	E_DRM_PNL_OUTPUT_FORMAT_NUM,
};

enum DRM_VIDEO_MEM_MODE_E {
	EN_VIDEO_MEM_MODE_SW,
	EN_VIDEO_MEM_MODE_HW,
	EN_VIDEO_MEM_MODE_MAX,
};

enum DRM_VIDEO_FACTORY_MODE_E {
	EN_VIDEO_FACTORY_MODE_ON_WB,
	EN_VIDEO_FACTORY_MODE_OFF,
	EN_VIDEO_FACTORY_MODE_MAX,
};

enum drm_video_data_range {
	E_VIDEO_DATA_RANGE_LIMIT = 0x0,
	E_VIDEO_DATA_RANGE_FULL = 0x1,
	E_VIDEO_DATA_RANGE_RANGE_MAX,
};

struct drm_mtk_tv_dac {
	bool enable;
};

struct drm_mtk_tv_common_stub {
	bool stub;
	bool pnlgamma_stubmode;
	bool b2r_src_stubmode;
	bool vtt2fpll_stubmode;
};

struct drm_mtk_tv_pnlgamma_enable {
	bool enable;
};

struct drm_mtk_tv_pnlgamma_gainoffset {
	__u16 pnlgamma_offset_r;
	__u16 pnlgamma_offset_g;
	__u16 pnlgamma_offset_b;
	__u16 pnlgamma_gain_r;
	__u16 pnlgamma_gain_g;
	__u16 pnlgamma_gain_b;
};

struct drm_mtk_tv_pnlgamma_gainoffset_enable {
	bool enable;
};

struct drm_mtk_tv_pqgamma_curve {
	__u16 curve_r[256];
	__u16 curve_g[256];
	__u16 curve_b[256];
	__u16 size;
};

struct drm_mtk_tv_pqgamma_enable {
	bool enable;
};

struct drm_mtk_tv_pqgamma_gainoffset {
	__u16 pqgamma_offset_r;
	__u16 pqgamma_offset_g;
	__u16 pqgamma_offset_b;
	__u16 pqgamma_gain_r;
	__u16 pqgamma_gain_g;
	__u16 pqgamma_gain_b;
	bool  pregainoffset;
};

struct drm_mtk_tv_pqgamma_gainoffset_enable {
	bool enable;
	bool pregainoffset;
};

struct drm_mtk_tv_pqgamma_maxvalue {
	__u16 pqgamma_max_r;
	__u16 pqgamma_max_g;
	__u16 pqgamma_max_b;
};

struct drm_mtk_tv_pqgamma_maxvalue_enable {
	bool enable;
};

#define MTK_DRM_PQMAP_NUM_MAX 2
struct drm_mtk_tv_pqmap_param {
	__u32 pim_size[MTK_DRM_PQMAP_NUM_MAX];
	__u32 pim_dma_fd[MTK_DRM_PQMAP_NUM_MAX];
};
struct drm_mtk_tv_pqmap_node_array {
	__u16 node_num;
	__u16 qmap_num[MTK_DRM_PQMAP_NUM_MAX];
	__u16 node_array[];
};

struct drm_mtk_tv_gem_create {
	__u32 u32version;
	__u32 u32length;
	struct drm_mode_create_dumb drm_dumb;
	__u64 u64gem_dma_addr;
};

struct drm_mtk_tv_bootlogo_ctrl {
	__u32 u32version;
	__u32 u32length;
	__u8  u8CmdType;
	__u8  u8GopNum;
};

enum drm_mtk_tv_graphic_pattern_move_dir {
	MTK_DRM_GRAPHIC_PATTERN_MOVE_TO_RIGHT,
	MTK_DRM_GRAPHIC_PATTERN_MOVE_TO_LEFT,
};

struct drm_mtk_tv_graphic_testpattern {
	__u8  PatEnable;
	__u8  HwAutoMode;
	__u32 DisWidth;
	__u32 DisHeight;
	__u32 ColorbarW;
	__u32 ColorbarH;
	__u8  EnColorbarMove;
	enum drm_mtk_tv_graphic_pattern_move_dir MoveDir;
	__u16 MoveSpeed;
};

struct drm_mtk_tv_graphic_pq_setting {
	__u32  u32version;
	__u32  u32buf_cfd_ml_size;
	__u64  u64buf_cfd_ml_addr;
	__u32  u32buf_cfd_adl_size;
	__u64  u64buf_cfd_adl_addr;
	__u32  u32buf_pq_ml_size;
	__u64  u64buf_pq_ml_addr;
	__u32  u32GopIdx;
};

struct drm_mtk_tv_graphic_alpha_mode {
	__u32  u32version;
	bool bEnPixelAlpha;
	__u8  u8ConstantAlphaVal;
	bool bEnMultiAlpha;
};

struct drm_mtk_tv_graphic_path_mode {
	__u8  VGsync_mode;
	__u32  GVreqDivRatio;
};

struct property_blob_memc_status {
	__u8 u8MemcEnabled;
	__u8 u8MemcLevelStatus;
};

struct drm_mtk_tv_gfx_pqudata {
	__u32 RetValue;
};

struct drm_st_ctrlbit_item {
	bool bEnable;   /* is this ctrlbit active default 0 */
	__u8 u8LaneID;  /* 1~16 (to specify the lane), 0: copy to all lane(for 8KBE) */
	__u8 u8Lsb;     /* the least significant bit of ctrlbit, support 0~23 */
	__u8 u8Msb;     /* the most significant bit of ctrlbit, support 0~23 */
	__u32 u32Value;
	/* the value if specific ctrlbit. if feature was specified, this value will be ignored. */
	enum drm_en_vbo_ctrlbit_feature enFeature; /* feature_name */
};


struct drm_st_ctrlbits {
	__u8 u8ConnectorID; /* 0: Video; 1: Delta Video; 2: GFX */
	__u8 u8CopyType;    /* 0: normal(for Delta Video, GFX) ; 1: copy all lane(for Video) */
	struct drm_st_ctrlbit_item ctrlbit_item[CTRLBIT_MAX_NUM]; /* ctrl items */
};

struct drm_st_pnl_frc_ratio_info {
	__u8 u8FRC_in;  /* ivs */
	__u8 u8FRC_out; /* ovs */
};

enum drm_mtk_source_type {
	MTK_SOURCE_TYPE_HDMI,
	MTK_SOURCE_TYPE_VGA,
	MTK_SOURCE_TYPE_ATV,
	MTK_SOURCE_TYPE_YPBPR,
	MTK_SOURCE_TYPE_CVBS,
	MTK_SOURCE_TYPE_DTV,
	MTK_SOURCE_TYPE_MM,
	MTK_SOURCE_TYPE_PHOTO,
	MTK_SOURCE_TYPE_NUM,
};

struct drm_mtk_tv_trigger_gen_info {
	__u16 Vde_start;
	__u16 Vde;
	__u16 Vtotal;
	__u8 Vs_pulse_width;
	__u32 Input_Vfreqx1000;
	__u32 fdet_vtt0;
	__u32 fdet_vtt1;
	bool IsSCMIFBL;
	bool IsFrcMdgwFBL;
	bool IsLowLatency;
	bool IsVrr;
	bool IsPmodeEn;
	bool LegacyMode;
	bool IsForceP;
	bool IsB2Rsource;
	enum drm_mtk_source_type eSourceType;
	__u8 u8Ivs;
	__u8 u8Ovs;
	bool IsGamingMJCOn;
	bool ip_path;
	bool op_path;
};

enum drm_mtk_pattern_position {
	MTK_PATTERN_POSITION_OPDRAM,
	MTK_PATTERN_POSITION_IPBLEND,
	MTK_PATTERN_POSITION_MULTIWIN,
	MTK_PATTERN_POSITION_TCON,
	MTK_PATTERN_POSITION_GFX,
	MTK_PATTERN_POSITION_MAX,
};

enum drm_mtk_pattern_type {
	MTK_PATTERN_PURE_COLOR,
	MTK_PATTERN_PURE_COLOR_BAR,
	MTK_PATTERN_RAMP,
	MTK_PATTERN_PIP_WINDOW,
	MTK_PATTERN_CROSSHATCH,
	MTK_PATTERN_RADOM,
	MTK_PATTERN_CHESSBOARD,
	MTK_PATTERN_MAX,
};

/* Panel pattern gen type */
enum drm_mtk_panel_pattern_type {
	MTK_PNL_PATTERN_PURE_COLOR,
	MTK_PNL_PATTERN_RAMP,
	MTK_PNL_PATTERN_PIP_WINDOW,
	MTK_PNL_PATTERN_CROSSHATCH,
	MTK_PNL_PATTERN_MAX,
};

struct drm_mtk_pattern_color {
	__u16 red;
	__u16 green;
	__u16 blue;
};

struct drm_mtk_pattern_pure_color {
	struct drm_mtk_pattern_color color;
};

struct drm_mtk_pattern_ramp {
	struct drm_mtk_pattern_color color_begin;
	struct drm_mtk_pattern_color color_end;
	__u8  vertical;
	__u16 level;
};

struct drm_mtk_pattern_pip_window {
	struct drm_mtk_pattern_color background_color;
	struct drm_mtk_pattern_color window_color;
	__u16 window_x;
	__u16 window_y;
	__u16 window_width;
	__u16 window_height;
};

struct drm_mtk_pattern_crosshatch {
	struct drm_mtk_pattern_color background_color;
	struct drm_mtk_pattern_color line_color;
	__u16 line_start_x;
	__u16 line_start_y;
	__u16 line_interval_h;
	__u16 line_interval_v;
	__u16 line_width_v;
	__u16 line_width_h;
};

struct drm_mtk_pattern_config {
	enum drm_mtk_pattern_type pattern_type;
	enum drm_mtk_pattern_position pattern_position;
	union {
		struct drm_mtk_pattern_pure_color pure_color;
		struct drm_mtk_pattern_ramp ramp;
		struct drm_mtk_pattern_pip_window pip_window;
		struct drm_mtk_pattern_crosshatch crosshatch;
	} u;
};

enum drm_video_ldm_sw_set_ctrl_type {
	E_DRM_LDM_SW_LDC_BYPASS = 0,
	E_DRM_LDM_SW_LDC_PATH_SEL,
	E_DRM_LDM_SW_LDC_XIU2AHB_SEL0,
	E_DRM_LDM_SW_LDC_XIU2AHB_SEL1,
	E_DRM_LDM_SW_RESET_LDC_RST,
	E_DRM_LDM_SW_RESET_LDC_MRST,
	E_DRM_LDM_SW_RESET_LDC_AHB,
	E_DRM_LDM_SW_RESET_LDC_AHB_WP,
	E_DRM_LDM_SW_SET_LD_SUPPORT,
	E_DRM_LDM_SW_SET_CUS_PROFILE,
	E_DRM_LDM_SW_SET_TMON_DEBUG,

};

struct DrmSWSetCtrlIn {
	bool RIU;
	enum drm_video_ldm_sw_set_ctrl_type enDrmLDMSwSetCtrlType;
	__u8 u8Value;

};

struct DrmLdmPath {
	char aCusPath[64];
	char aCusPathU[64];
};

enum drm_ldm_led_check_type {
	E_LDM_LED_CHECK_SINGLE = 0,
	E_LDM_LED_CHECK_H = 1,
	E_LDM_LED_CHECK_V,
	E_LDM_LED_CHECK_MARQUEE,
	E_LDM_LED_CHECK_LEFT_RIGHT,
	E_LDM_LED_CHECK_MAX,
};

struct drm_mtk_tv_ldm_led_check {
	bool bEn;
	enum drm_ldm_led_check_type enldm_led_check_type;
	__u16 u16LEDNum;

};

struct drm_mtk_tv_ldm_black_insert_enable {
	bool bEn;
};

enum drm_ldm_pq_param_type {
	E_LD_Param_strength = 0,
	E_LD_Param_GammaIn,
	E_LD_Param_GammaOut,
	E_LD_Param_Compensation_blend,
	E_LD_Param_SF_1,
	E_LD_Param_SF_2,
	E_LD_Param_SF_3,
	E_LD_Param_SF_4,
	E_LD_Param_SF_5,
	E_LD_Param_TF_Up,
	E_LD_Param_TF_Dn,
	E_LD_Param_TF_Low_TH,
	E_LD_Param_TF_High_TH,
	E_LD_Param_TF_MaxSpd,
	E_LD_Param_BL_Alpha,
	E_LD_Param_CUS_UI_VIDEO_Pictype,
	E_LD_Param_MAX,
};

struct drm_mtk_tv_ldm_pq_param {
	enum drm_ldm_pq_param_type e_ldm_pq_param_type;
	__u8 u8value;
};

struct drm_mtk_tv_ldm_VCOM_enable {
	bool bEn;
};


struct drm_st_out_swing_level {
	bool usr_swing_level;
	bool common_swing;
	__u32 ctrl_lanes;
	__u32 swing_level[VBO_MAX_LANES];
};

struct drm_st_out_pe_level {
	bool usr_pe_level;
	bool common_pe;
	__u32 ctrl_lanes;
	__u32 pe_level[VBO_MAX_LANES];
};

struct drm_st_out_ssc {
	bool ssc_en;
	__u32 ssc_modulation;
	__u32 ssc_deviation;
};

struct drm_st_out_ssc_info {
	struct drm_st_out_ssc ssc_setting;
	bool ssc_protect_by_user_en;
	__u32 ssc_modulation_protect_value;
	__u32 ssc_deviation_protect_value;
};

struct drm_st_mod_outconfig {
	__u16 outconfig_ch0_ch7;
	__u16 outconfig_ch8_ch15;
	__u16 outconfig_ch16_ch19;
};

struct drm_st_vtt_info {
	bool lock;
	bool lock_en;
	__u32 input_vtt;
	__u32 output_vtt;
};

struct drm_st_mod_freeswap {
	__u16 freeswap_ch0_ch1;
	__u16 freeswap_ch2_ch3;
	__u16 freeswap_ch4_ch5;
	__u16 freeswap_ch6_ch7;
	__u16 freeswap_ch8_ch9;
	__u16 freeswap_ch10_ch11;
	__u16 freeswap_ch12_ch13;
	__u16 freeswap_ch14_ch15;
	__u16 freeswap_ch16_ch17;
	__u16 freeswap_ch18_ch19;
	__u16 freeswap_ch20_ch21;
	__u16 freeswap_ch22_ch23;
	__u16 freeswap_ch24_ch25;
	__u16 freeswap_ch26_ch27;
	__u16 freeswap_ch28_ch29;
	__u16 freeswap_ch30_ch31;
};

struct drm_st_mod_status {
	bool vby1_locked;
	__u16 vby1_unlockcnt;
	bool vby1_htpdn;
	bool vby1_lockn;
	struct drm_st_mod_outconfig outconfig;
	struct drm_st_mod_freeswap freeswap;
};

struct drm_mtk_tv_video_delay {
	__u16 sw_delay_ms;
	__u32 memc_pre_input_vfreq;
	__u16 memc_delay_ms;
	__u16 memc_rw_diff;
	__u16 mdgw_delay_ms[16];
	__u16 mdgw_rw_diff[16];
};

enum drm_mtk_memc_mode {
	E_MEMC_OFF,
	E_MEMC_LOW,
	E_MEMC_MIDDLE,
	E_MEMC_HIGH,
	E_MEMC_LBW = 0x0F,
};

enum drm_mtk_mfc_mode {
	E_MEMC_MFC_MODE_NONE = 0,
	E_MEMC_MFC_MODE_1X,
	E_MEMC_MFC_MODE_2X,
	E_MEMC_MFC_MODE_4X,
	E_MEMC_MFC_MODE_REPEAT,
	E_MEMC_MFC_MODE_SINGLE_NORMAL_8K,
	E_MEMC_MFC_MODE_SINGLE_LOW_8K,
	E_MEMC_MFC_MODE_MULTI_8K,
	E_MEMC_MFC_MODE_REPEAT_AUTO,
	E_MEMC_MFC_MODE_MAX,
};

struct drm_mtk_tv_video_frc_mode {
	enum drm_mtk_memc_mode video_memc_mode;
	enum drm_mtk_mfc_mode video_frc_mode;
};

struct drm_mtk_tv_video_fb_num {
	enum drm_mtk_vplane_buf_mode mdgw_fb_mode;
	__u16 memc_fb_num;
	__u16 mdgw_fb_num;
};

struct drm_mtk_tv_output_info {
	__u8 p_engine;
	enum drm_en_pnl_output_format color_fmt;
	__u8 data_format;	// EN_PQ_CFD_DATA_FORMAT
	enum drm_video_data_range data_range;	// EN_PQ_CFD_RANGE
	__u8 transfer_characteristics;	// EN_PQ_CFD_CFIO_TR
	__u8 matrix_coefficient;	// EN_PQ_CFD_CFIO_MC
	__u8 color_primary;	// EN_PQ_CFD_CFIO_CP
	__u16 width;
	__u16 height;
};

struct drm_mtk_tv_dip_capture_info {
	struct drm_mtk_tv_output_info video; //P9
	struct drm_mtk_tv_output_info video_osdb; // P10
	struct drm_mtk_tv_output_info video_post; // P11
};

struct drm_mtk_tv_fence_info {
	bool bCreateFence[PLANE_SUPPORT_FENCE_MAX_NUM];
	int FenceFd[PLANE_SUPPORT_FENCE_MAX_NUM];
	bool bResetTimeLine[PLANE_SUPPORT_FENCE_MAX_NUM];
};

struct drm_mtk_tv_pqu_window_setting {
	__u32 mute;
	__u32 pq_id;
	__u32 frame_id;
	__u32 window_x;
	__u32 window_y;
	__u32 window_z;
	__u32 window_width;
	__u32 window_height;
};

struct drm_mtk_tv_pqu_metadata {
	__u32 magic;
	__u32 global_frame_id;
	__u32 window_num;
	struct drm_mtk_tv_pqu_window_setting window_setting[MTK_TV_DRM_WINDOW_NUM_MAX];
};

struct drm_mtk_tv_graphic_roi_info {
	__u64 RetCntOSDB0;
	__u64 RetCntOSDB1;
};

struct drm_mtk_tv_oled_temperature {
	__u16 temp1;
	__u16 temp2;
};

struct drm_mtk_tv_oled_callback {
	void *cbFunc;
};

struct drm_mtk_mute_color {
	__u16 red_Cr;
	__u16 green_Y;
	__u16 blue_Cb;
};

struct drm_mtk_bg_color {
	__u16 red_Cr;
	__u16 green_Y;
	__u16 blue_Cb;
};

struct drm_mtk_tv_video_extra_latency {
	enum drm_mtk_video_plane_type planetype;
	__u16 video_latency_ms;
	__u16 real_video_latency_ms;
};

struct drm_mtk_video_expect_pq_out_size {
	enum DRM_VIDEO_MEM_MODE_E mem_mode;//set
	__u16 width;//get
	__u16 height;//get
};

struct drm_mtk_video_factory_mode {
	enum DRM_VIDEO_FACTORY_MODE_E factory_mode;
};

struct drm_mtk_tv_ambientlight_data {
	__u32 amb_dma_fd;
};

/**************************************************************************************************/
/* MTK TV DRM backlight IOCTL parameters
 *   Index: DRM_IOCTL_MTK_TV_BACKLIGHT_CONTROL
 *   Param: struct drm_mtk_tv_backight_control_param
 *   Desc:
 *     Driver according 'enum drm_mtk_tv_backight_control_action' to control the USER backlight.
 */
enum drm_mtk_tv_backight_control_action {
	/* Render DD uses _Interpolation_ to calculate the pwm duty.
	 * User provides the min, max and value in @drm_mtk_range_value.
	 * Render DD uses PWM min, PWM max and user input to interpolate the PWM duty.
	 */
	/* Set/Get pwm parameter to ALL port. Ignore @port_num.
	 * In multi-pwm environment, Render DD replies the first pwm's parameter.
	 */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_VALUE_ALL,	/* struct drm_mtk_range_value value_all */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_VALUE_ALL,	/* struct drm_mtk_range_value value_all */

	/* Set/Get pwm parameter one by one */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_VALUE,		/* struct drm_mtk_range_value value */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_VALUE,		/* struct drm_mtk_range_value value */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_VSYNC_ENABLE,	/* bool vsync_enable */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_VSYNC_ENABLE,	/* bool vsync_enable */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_PERIOD_MULTI,	/* __u32 period_multi */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_PERIOD_MULTI,	/* __u32 period_multi */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_VRR_ENABLE,	/* bool vrr_enable */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_VRR_ENABLE,	/* bool vrr_enable */
	E_MTK_TV_BACKLIGHT_CONTROL_SET_PERIOD_SHIFT,	/* __u32 period_shift */
	E_MTK_TV_BACKLIGHT_CONTROL_GET_PERIOD_SHIFT,	/* __u32 period_shift */
};
#define MTK_BACKLIGHT_PORT_NUM_MAX			16
struct drm_mtk_tv_backight_control_param {
	enum drm_mtk_tv_backight_control_action action;
	__u8 port_num;
	union {
		struct drm_mtk_range_value value_all;
		struct drm_mtk_range_value value[MTK_BACKLIGHT_PORT_NUM_MAX];
		bool vsync_enable[MTK_BACKLIGHT_PORT_NUM_MAX];
		__u32 period_multi[MTK_BACKLIGHT_PORT_NUM_MAX];
		bool vrr_enable;
		__u32 period_shift[MTK_BACKLIGHT_PORT_NUM_MAX]; /* Percentage of period: 0 ~ 100 */
	};
};
/**************************************************************************************************/

struct drm_mtk_cfd_CSC {
	enum DRM_CFD_DATA_FORMAT_E data_format;
	enum DRM_CFD_COLOR_RANGE_E color_range;
	enum DRM_CFD_COLOR_SPACE_E color_space;
};

enum drm_mtk_oled_event {
	E_OLED_EVENT_PNL_ON,
	E_OLED_EVENT_PNL_OFF,
	E_OLED_EVENT_JB_DONE,
	E_OLED_EVENT_JB_FAIL,
	E_OLED_EVENT_OFFRS_DONE,
	E_OLED_EVENT_OFFRS_FAIL,
	E_OLED_EVENT_ERR,
	E_OLED_EVENT_EVDD,
	E_OLED_EVENT_STILL_TRIG,
	E_OLED_EVENT_STILL_CANCEL,
	E_OLED_EVENT_NUM,
};

struct drm_mtk_tv_vidwin_rect {
	__u16 u16X;
	__u16 u16Y;
	__u16 u16Width;
	__u16 u16Height;
};

struct drm_mtk_tv_pixels_report {
	__u32 u32RcrMin;
	__u32 u32RcrMax;
	__u32 u32GYMin;
	__u32 u32GYMax;
	__u32 u32BcbMin;
	__u32 u32BcbMax;
	__u32 u32RCrSum;
	__u32 u32GYSum;
	__u32 u32BcbSum;
};

struct drm_mtk_tv_ambilight_data {
	struct drm_mtk_tv_vidwin_rect vidwin_rect;
	struct drm_mtk_tv_pixels_report pixels_report;
};


/** \brief pixel monitor report size */
struct drm_mtk_pxm_get_report_size {
	/** Set Pixel Monitor position */
	enum drm_pxm_point point;
	/** Set Pixel Monitor window id(ex:0,main window,1:sub window) */
	__u8 win_id;
	/** Set Cusor report (0:disable, 1:enable) */
	__u8 report_en;
	/** Set Cusor display (0:disable, 1:enable) */
	__u8 display_en;
	/** Set Cusor red value ([0:4095]) */
	__u16 display_r;
	/** Set Cusor green value ([0:4095]) */
	__u16 display_g;
	/** Set Cusor blue value ([0:4095]) */
	__u16 display_b;
	/** Set Cusor alpha value ([0:255]) */
	__u16 display_alpha;
	/** Set Cusor horizontal position value (ex: 4K panel [0:3840]) */
	__u16 pos_x;
	/** Set Cusor vertical position value (ex: 4K panel [0:2160]) */
	__u16 pos_y;
	/** Set One Frame per microsecond value (ex: 60Hz Video is 16.67ms = 16670us) */
	__u64 frame_us;
	/** Get Pixel report red value */
	__u16 r;
	/** Get Pixel report green value */
	__u16 g;
	/** Get Pixel report blue value */
	__u16 b;
	/** Get Pixel report alpha value */
	__u16 alpha;
};

struct drm_mtk_tv_set_hse_enable {
	bool enable;
};

struct drm_mtk_tv_dv_pwm_info {
	__u8 pdim;
	__u8 adim;
};

/**************************************************************************************************/
/* MTK TV DRM local dimming IOCTL parameters
 *   Index: DRM_IOCTL_MTK_TV_CTL_LOCAL_DIMMING
 *   Param: struct drm_mtk_tv_ldm_param
 *   Desc:
 *     Driver according 'enum drm_mtk_tv_ldm_action' to control the local dimming.
 */
enum drm_mtk_tv_ldm_block_num_type {
	E_MTK_TV_LDM_BLOCK_NUM_TYPE_LDF,
	E_MTK_TV_LDM_BLOCK_NUM_TYPE_LED,
};
struct drm_mtk_tv_ldm_block_num {
	enum drm_mtk_tv_ldm_block_num_type num_type;
	__u16 width;
	__u16 height;
};

enum drm_mtk_tv_ldm_block_data_type {
	E_MTK_TV_LDM_BLOCK_TYPE_LDF_MAX,
	E_MTK_TV_LDM_BLOCK_TYPE_LDF_AVG,
	E_MTK_TV_LDM_BLOCK_TYPE_LED,
};
struct drm_mtk_tv_ldm_block_data {
	enum drm_mtk_tv_ldm_block_data_type data_type;
	__u32 data_dma_fd;
	__u64 data_dma_addr;
};

struct drm_mtk_tv_ldm_qmap_nodes {
	__u16 qmap_num[MTK_DRM_PQMAP_NUM_MAX];
	__u32 qmap_dma_fd;
};

enum drm_mtk_tv_ldm_action {
	/* LDM block */
	E_MTK_TV_LDM_ACTION_GET_BLOCK_NUM,	/* struct drm_mtk_tv_ldm_block_num  */
	E_MTK_TV_LDM_ACTION_GET_BLOCK_DATA,	/* struct drm_mtk_tv_ldm_block_data */

	/* LDM duty */
	E_MTK_TV_LDM_ACTION_SET_DUTY,		/* __u8 duty */
	E_MTK_TV_LDM_ACTION_GET_DUTY,		/* __u8 duty */

	/* LDM vsync frequency:
	 *   If freq = 0, it means disable.
	 *   If freq > 0, it means enable and valid vsync freq.
	 */
	E_MTK_TV_LDM_ACTION_SET_VSYNC_FREQ,	/* __u32 vsync_freq */
	E_MTK_TV_LDM_ACTION_GET_VSYNC_FREQ,	/* __u32 vsync_freq */

	/* LDM qmap */
	E_MTK_TV_LDM_ACTION_SET_QMAP,		/* struct drm_mtk_tv_ldm_qmap_nodes */

	/* LDM status */
	E_MTK_TV_LDM_ACTION_SET_STATUS,		/* int status */
	E_MTK_TV_LDM_ACTION_GET_STATUS,		/* int status */
	/* LDM boost en */
	E_MTK_TV_LDM_ACTION_SET_BOOST_EN,	/* bool boost_en */
};
struct drm_mtk_tv_ldm_param {
	enum drm_mtk_tv_ldm_action action;
	union {
		struct drm_mtk_tv_ldm_block_num block_num_info;
		struct drm_mtk_tv_ldm_block_data block_data_info;
		struct drm_mtk_tv_ldm_qmap_nodes qmap_nodes;
		int status;
		__u32 vsync_freq;
		__u8 duty;
		bool boost_en;
	};
};
/**************************************************************************************************/

#define DRM_MTK_TV_GEM_CREATE   (0x00)
#define DRM_MTK_TV_CTRL_BOOTLOGO   (0x01)
#define DRM_MTK_TV_SET_GRAPHIC_TESTPATTERN   (0x02)
#define DRM_MTK_TV_SET_STUB_MODE (0x03)
#define DRM_MTK_TV_START_GFX_PQUDATA (0x04)
#define DRM_MTK_TV_STOP_GFX_PQUDATA (0x05)
#define DRM_MTK_TV_SET_PNLGAMMA_ENABLE (0x06)
#define DRM_MTK_TV_SET_PNLGAMMA_GAINOFFSET (0x07)
#define DRM_MTK_TV_SET_PNLGAMMA_GAINOFFSET_ENABLE (0x08)
#define DRM_MTK_TV_GET_FPS_VALUE (0x09)
#define DRM_MTK_TV_SET_CVBSO_MODE (0x0A)
#define DRM_MTK_TV_SET_PQMAP_INFO (0x0B)
#define DRM_MTK_TV_GET_VIDEO_DELAY (0x0C)
#define DRM_MTK_TV_GET_OUTPUT_INFO (0x0D)
#define DRM_MTK_TV_GET_FENCE (0x0E)
#define DRM_MTK_TV_SET_PQGAMMA_CURVE (0x0F)
#define DRM_MTK_TV_ENABLE_PQGAMMA (0x10)
#define DRM_MTK_TV_SET_PQGAMMA_GAINOFFSET (0x11)
#define DRM_MTK_TV_ENABLE_PQGAMMA_GAINOFFSET (0x12)
#define DRM_MTK_TV_SET_PQGAMMA_MAXVALUE (0x13)
#define DRM_MTK_TV_ENABLE_PQGAMMA_MAXVLAUE (0x14)
#define DRM_MTK_TV_GET_PQU_METADATA (0x15)
#define DRM_MTK_TV_TIMELINE_INC (0x16)
#define DRM_MTK_TV_SET_GRAPHIC_PQ_BUF (0x17)
#define DRM_MTK_TV_GET_GRAPHIC_ROI (0x18)
#define DRM_MTK_TV_GET_VIDEO_FB_NUM (0x19)
#define DRM_MTK_TV_SET_GRAPHIC_ROI (0x1A)
#define DRM_MTK_TV_SET_OLED_CALLBACK (0x1B)
#define DRM_MTK_TV_GET_OLED_TEMPERATURE (0x1C)
#define DRM_MTK_TV_SET_OLED_OFFRS (0x1D)
#define DRM_MTK_TV_SET_OLED_JB (0x1E)
#define DRM_MTK_TV_SET_OLED_HDR (0x1F)
#define DRM_MTK_TV_GET_PNLGAMMA_ENABLE (0x20)
#define DRM_MTK_TV_CTL_LOCAL_DIMMING (0x21)
#define DRM_MTK_TV_SET_VIDEO_EXTRA_LATENCY (0x22)
#define DRM_MTK_TV_SET_TRIGGER_GEN (0x23)
#define DRM_MTK_TV_SET_VIDEO_FRC_MODE (0x24)
#define DRM_MTK_TV_GET_VIDEO_EXPECT_PQ_OUT_SIZE (0x25)
#define DRM_MTK_TV_GET_VIDEO_BLENDING_COLOR_FORMAT (0x26)
#define DRM_MTK_TV_GET_AMBIENTLIGHT_DATA (0x27)
#define DRM_MTK_TV_SET_LIVE_TONE (0x28)
#define DRM_MTK_TV_GET_PHASE_DIFF (0x29)
#define DRM_MTK_TV_GET_PNL_SSC_INFO (0x2A)
#define DRM_MTK_TV_SET_LDM_LED_CHECK (0x2B)
#define DRM_MTK_TV_SET_LDM_BLACK_INSERT_ENABLE (0x2C)
#define DRM_MTK_TV_BACKLIGHT_CONTROL (0x2D)
#define DRM_MTK_TV_GET_OLED_EVENT (0x2E)
#define DRM_MTK_TV_SET_LDM_PQ_PARAM (0x2F)
#define DRM_MTK_TV_SET_CHANGE_MODEID_STATE (0x30)
#define DRM_MTK_TV_GET_CHANGE_MODEID_STATE (0x31)
#define DRM_MTK_TV_GET_GLOBAL_MUTE_CTRL (0x32)
#define DRM_MTK_TV_GET_VIDEO_PXM_REPORT_SIZE (0x33)
#define DRM_MTK_TV_SET_VAC_ENABLE (0x34)
#define DRM_MTK_TV_SET_VIDEO_HSE (0x35)
#define DRM_MTK_TV_GET_AMBILIGHT_DATA (0x36)
#define DRM_MTK_TV_SET_DISPOUT_POWER_ONOFF (0x37)
#define DRM_MTK_TV_GET_PANEL_INFO (0x38)
#define DRM_MTK_TV_SET_DEMURA_ONOFF (0x39)
#define DRM_MTK_TV_GET_PANEL_MAX_FRAMERATE (0x3A)
#define DRM_MTK_TV_SET_FACTORY_MODE (0x3B)
#define DRM_MTK_TV_SET_LDM_VCOM_ENABLE (0x3C)
#define DRM_MTK_TV_GET_BL_ON_STATE (0x3D)
#define DRM_MTK_TV_GET_DV_PWM_INFO (0x3E)
#define DRM_MTK_TV_GET_FRC_ALGO_VERSION (0x3F)
#define DRM_MTK_TV_GET_PANEL_TIMING_INFO (0x40)

#define DRM_IOCTL_MTK_TV_GEM_CREATE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GEM_CREATE, struct drm_mtk_tv_gem_create)
#define DRM_IOCTL_MTK_TV_CTRL_BOOTLOGO    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_CTRL_BOOTLOGO, struct drm_mtk_tv_bootlogo_ctrl)
#define DRM_IOCTL_MTK_TV_SET_GRAPHIC_TESTPATTERN    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_GRAPHIC_TESTPATTERN, struct drm_mtk_tv_graphic_testpattern)
#define DRM_IOCTL_MTK_TV_SET_STUB_MODE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_STUB_MODE, struct drm_mtk_tv_common_stub)
#define DRM_IOCTL_MTK_TV_START_GFX_PQUDATA    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_START_GFX_PQUDATA, struct drm_mtk_tv_gfx_pqudata)
#define DRM_IOCTL_MTK_TV_STOP_GFX_PQUDATA    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_STOP_GFX_PQUDATA, struct drm_mtk_tv_gfx_pqudata)
#define DRM_IOCTL_MTK_TV_SET_PNLGAMMA_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PNLGAMMA_ENABLE, struct drm_mtk_tv_pnlgamma_enable)
#define DRM_IOCTL_MTK_TV_SET_PNLGAMMA_GAINOFFSET    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PNLGAMMA_GAINOFFSET, struct drm_mtk_tv_pnlgamma_gainoffset)
#define DRM_IOCTL_MTK_TV_SET_PNLGAMMA_GAINOFFSET_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PNLGAMMA_GAINOFFSET_ENABLE,\
		struct drm_mtk_tv_pnlgamma_gainoffset_enable)
#define DRM_IOCTL_MTK_TV_GET_FPS_VALUE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_FPS_VALUE, struct drm_mtk_range_value)
#define DRM_IOCTL_MTK_TV_SET_CVBSO_MODE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_CVBSO_MODE, struct drm_mtk_tv_dac)
#define DRM_IOCTL_MTK_TV_SET_PQMAP_INFO    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PQMAP_INFO, struct drm_mtk_tv_pqmap_param)
#define DRM_IOCTL_MTK_TV_GET_VIDEO_DELAY    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_VIDEO_DELAY, struct drm_mtk_tv_video_delay)
#define DRM_IOCTL_MTK_TV_GET_OUTPUT_INFO    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_OUTPUT_INFO, struct drm_mtk_tv_dip_capture_info)
#define DRM_IOCTL_MTK_TV_GET_FENCE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_FENCE, struct drm_mtk_tv_fence_info)
#define DRM_IOCTL_MTK_TV_SET_PQGAMMA_CURVE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PQGAMMA_CURVE, struct drm_mtk_tv_pqgamma_curve)
#define DRM_IOCTL_MTK_TV_ENABLE_PQGAMMA    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_ENABLE_PQGAMMA, struct drm_mtk_tv_pqgamma_enable)
#define DRM_IOCTL_MTK_TV_SET_PQGAMMA_GAINOFFSET    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PQGAMMA_GAINOFFSET, struct drm_mtk_tv_pqgamma_gainoffset)
#define DRM_IOCTL_MTK_TV_ENABLE_PQGAMMA_GAINOFFSET    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_ENABLE_PQGAMMA_GAINOFFSET, struct drm_mtk_tv_pqgamma_gainoffset_enable)
#define DRM_IOCTL_MTK_TV_SET_PQGAMMA_MAXVALUE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_PQGAMMA_MAXVALUE, struct drm_mtk_tv_pqgamma_maxvalue)
#define DRM_IOCTL_MTK_TV_ENABLE_PQGAMMA_MAXVALUE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_ENABLE_PQGAMMA_MAXVLAUE, struct drm_mtk_tv_pqgamma_maxvalue_enable)
#define DRM_IOCTL_MTK_TV_GET_PQU_METADATA   DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PQU_METADATA, struct drm_mtk_tv_pqu_metadata)
#define DRM_IOCTL_MTK_TV_TIMELINE_INC    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_TIMELINE_INC, int)
#define DRM_IOCTL_MTK_TV_SET_GRAPHIC_PQ_BUF    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_GRAPHIC_PQ_BUF, struct drm_mtk_tv_graphic_pq_setting)
#define DRM_IOCTL_MTK_TV_GET_GRAPHIC_ROI    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_GRAPHIC_ROI, struct drm_mtk_tv_graphic_roi_info)
#define DRM_IOCTL_MTK_TV_GET_VIDEO_FB_NUM    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_VIDEO_FB_NUM, struct drm_mtk_tv_video_fb_num)
#define DRM_IOCTL_MTK_TV_SET_GRAPHIC_ROI    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_GRAPHIC_ROI, __u32)
#define DRM_IOCTL_MTK_TV_SET_OLED_CALLBACK   DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_OLED_CALLBACK, struct drm_mtk_tv_oled_callback)
#define DRM_IOCTL_MTK_TV_GET_OLED_TEMPERATURE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_OLED_TEMPERATURE, struct drm_mtk_tv_oled_temperature)
#define DRM_IOCTL_MTK_TV_SET_OLED_OFFRS    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_OLED_OFFRS, bool)
#define DRM_IOCTL_MTK_TV_SET_OLED_JB    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_OLED_JB, bool)
#define DRM_IOCTL_MTK_TV_SET_OLED_HDR    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_OLED_HDR, bool)
#define DRM_IOCTL_MTK_TV_GET_PNLGAMMA_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PNLGAMMA_ENABLE, struct drm_mtk_tv_pnlgamma_enable)
#define DRM_IOCTL_MTK_TV_CTL_LOCAL_DIMMING  DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_CTL_LOCAL_DIMMING, struct drm_mtk_tv_ldm_param)
#define DRM_IOCTL_MTK_TV_SET_VIDEO_EXTRA_LATENCY    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_VIDEO_EXTRA_LATENCY, struct drm_mtk_tv_video_extra_latency)
#define DRM_IOCTL_MTK_TV_SET_TRIGGER_GEN    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_TRIGGER_GEN, struct drm_mtk_tv_trigger_gen_info)
#define DRM_IOCTL_MTK_TV_SET_VIDEO_FRC_MODE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_VIDEO_FRC_MODE, struct drm_mtk_tv_video_frc_mode)
#define DRM_IOCTL_MTK_TV_GET_VIDEO_EXPECT_PQ_OUT_SIZE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_VIDEO_EXPECT_PQ_OUT_SIZE, struct drm_mtk_video_expect_pq_out_size)
#define DRM_IOCTL_MTK_TV_GET_VIDEO_BLENDING_COLOR_FORMAT    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_VIDEO_BLENDING_COLOR_FORMAT, uint32_t)
#define DRM_IOCTL_MTK_TV_GET_AMBIENTLIGHT_DATA     DRM_IOW(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_AMBIENTLIGHT_DATA, struct drm_mtk_tv_ambientlight_data)
#define DRM_IOCTL_MTK_TV_SET_LIVE_TONE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_LIVE_TONE, struct drm_mtk_range_value)
#define DRM_IOCTL_MTK_TV_GET_PHASE_DIFF    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PHASE_DIFF, uint32_t)
#define DRM_IOCTL_MTK_TV_GET_PNL_SSC_INFO    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PNL_SSC_INFO, struct drm_st_out_ssc)
#define DRM_IOCTL_MTK_TV_SET_LDM_LED_CHECK    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_LDM_LED_CHECK, struct drm_mtk_tv_ldm_led_check)
#define DRM_IOCTL_MTK_TV_SET_LDM_BLACK_INSERT_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_LDM_BLACK_INSERT_ENABLE, struct drm_mtk_tv_ldm_black_insert_enable)
#define DRM_IOCTL_MTK_TV_BACKLIGHT_CONTROL    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_BACKLIGHT_CONTROL, struct drm_mtk_tv_backight_control_param)
#define DRM_IOCTL_MTK_TV_GET_OLED_EVENT     DRM_IOR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_OLED_EVENT, int)
#define DRM_IOCTL_MTK_TV_SET_LDM_PQ_PARAM    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_LDM_PQ_PARAM, struct drm_mtk_tv_ldm_pq_param)
#define DRM_IOCTL_MTK_TV_SET_CHANGE_MODEID_STATE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_CHANGE_MODEID_STATE, uint8_t)
#define DRM_IOCTL_MTK_TV_GET_CHANGE_MODEID_STATE    DRM_IOR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_CHANGE_MODEID_STATE, uint8_t)
#define DRM_IOCTL_MTK_TV_GET_GLOBAL_MUTE_CTRL    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_GLOBAL_MUTE_CTRL, struct drm_mtk_global_mute_ctrl_info)
#define DRM_IOCTL_MTK_TV_GET_VIDEO_PXM_REPORT_SIZE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_VIDEO_PXM_REPORT_SIZE, struct drm_mtk_pxm_get_report_size)
#define DRM_IOCTL_MTK_TV_SET_VAC_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_VAC_ENABLE, bool)
#define DRM_IOCTL_MTK_TV_SET_VIDEO_HSE DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_VIDEO_HSE, struct drm_mtk_tv_set_hse_enable)
#define DRM_IOCTL_MTK_TV_GET_AMBILIGHT_DATA     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_AMBILIGHT_DATA, struct drm_mtk_tv_ambilight_data)
#define DRM_IOCTL_MTK_TV_SET_LDM_VCOM_ENABLE    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_LDM_VCOM_ENABLE, struct drm_mtk_tv_ldm_VCOM_enable)
#define DRM_IOCTL_MTK_TV_SET_DISPOUT_POWER_ONOFF DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_DISPOUT_POWER_ONOFF, bool)
#define DRM_IOCTL_MTK_TV_GET_PANEL_INFO DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PANEL_INFO, struct drm_mtk_panel_info)
#define DRM_IOCTL_MTK_TV_SET_DEMURA_ONOFF DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_DEMURA_ONOFF, bool)
#define DRM_IOCTL_MTK_TV_GET_PANEL_MAX_FRAMERATE     DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PANEL_MAX_FRAMERATE, struct drm_mtk_panel_max_framerate)
#define DRM_IOCTL_MTK_TV_SET_FACTORY_MODE DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_SET_FACTORY_MODE, struct drm_mtk_video_factory_mode)
#define DRM_IOCTL_MTK_TV_GET_BL_ON_STATE DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_BL_ON_STATE, uint8_t)
#define DRM_IOCTL_MTK_TV_GET_DV_PWM_INFO DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_DV_PWM_INFO, struct drm_mtk_tv_dv_pwm_info)
#define DRM_IOCTL_MTK_TV_GET_FRC_ALGO_VERSION    DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_FRC_ALGO_VERSION, uint32_t)
#define DRM_IOCTL_MTK_TV_GET_PANEL_TIMING_INFO DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MTK_TV_GET_PANEL_TIMING_INFO, drm_st_pnl_mode_timing)

#define DRM_MTK_TV_FB_MODIFIER_NUM	4

#ifndef DRM_FORMAT_MOD_ARM_AFBC
#define DRM_FORMAT_MOD_VENDOR_ARM     0x08
/*
 * Arm Framebuffer Compression (AFBC) modifiers
 *
 * AFBC is a proprietary lossless image compression protocol and format.
 * It provides fine-grained random access and minimizes the amount of data
 * transferred between IP blocks.
 *
 * AFBC has several features which may be supported and/or used, which are
 * represented using bits in the modifier. Not all combinations are valid,
 * and different devices or use-cases may support different combinations.
 */
#define DRM_FORMAT_MOD_ARM_AFBC(__afbc_mode) fourcc_mod_code(ARM, __afbc_mode)

/*
 * AFBC superblock size
 *
 * Indicates the superblock size(s) used for the AFBC buffer. The buffer
 * size (in pixels) must be aligned to a multiple of the superblock size.
 * Four lowest significant bits(LSBs) are reserved for block size.
 */
#define AFBC_FORMAT_MOD_BLOCK_SIZE_MASK      0xf
#define AFBC_FORMAT_MOD_BLOCK_SIZE_16x16     (1ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8      (2ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_64x4      (3ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 (4ULL)

/*
 * AFBC lossless colorspace transform
 *
 * Indicates that the buffer makes use of the AFBC lossless colorspace
 * transform.
 */
#define AFBC_FORMAT_MOD_YTR     (1ULL <<  4)

/*
 * AFBC block-split
 *
 * Indicates that the payload of each superblock is split. The second
 * half of the payload is positioned at a predefined offset from the start
 * of the superblock payload.
 */
#define AFBC_FORMAT_MOD_SPLIT   (1ULL <<  5)

/*
 * AFBC sparse layout
 *
 * This flag indicates that the payload of each superblock must be stored at a
 * predefined position relative to the other superblocks in the same AFBC
 * buffer. This order is the same order used by the header buffer. In this mode
 * each superblock is given the same amount of space as an uncompressed
 * superblock of the particular format would require, rounding up to the next
 * multiple of 128 bytes in size.
 */
#define AFBC_FORMAT_MOD_SPARSE  (1ULL <<  6)

/*
 * AFBC copy-block restrict
 *
 * Buffers with this flag must obey the copy-block restriction. The restriction
 * is such that there are no copy-blocks referring across the border of 8x8
 * blocks. For the subsampled data the 8x8 limitation is also subsampled.
 */
#define AFBC_FORMAT_MOD_CBR     (1ULL <<  7)

/*
 * AFBC tiled layout
 *
 * The tiled layout groups superblocks in 8x8 or 4x4 tiles, where all
 * superblocks inside a tile are stored together in memory. 8x8 tiles are used
 * for pixel formats up to and including 32 bpp while 4x4 tiles are used for
 * larger bpp formats. The order between the tiles is scan line.
 * When the tiled layout is used, the buffer size (in pixels) must be aligned
 * to the tile size.
 */
#define AFBC_FORMAT_MOD_TILED   (1ULL <<  8)

/*
 * AFBC solid color blocks
 *
 * Indicates that the buffer makes use of solid-color blocks, whereby bandwidth
 * can be reduced if a whole superblock is a single color.
 */
#define AFBC_FORMAT_MOD_SC      (1ULL <<  9)

#endif /* DRM_FORMAT_MOD_ARM_AFBC */

#ifndef DRM_FORMAT_MOD_VENDOR_QCOM
#define DRM_FORMAT_MOD_VENDOR_QCOM    0x05
#endif

#ifndef DRM_FORMAT_MOD_QCOM_COMPRESSED
#define DRM_FORMAT_MOD_QCOM_COMPRESSED	fourcc_mod_code(QCOM, 1)
#endif

/*
 * MTK defined modifier
 *
 * BIT 0 ~ 7	: Used to define extended format arrangement
 * BIT 8	: The format is compressed
 * BIT 9	: The format is modified to 6 bit per pixel
 * BIT 10	: The format is modified to 10 bit per pixel
 * BIT 11	: The format is modified to legacy
 */
#define DRM_FORMAT_MOD_VENDOR_MTK	(0x09)

/* extended arrangment */
#define DRM_FORMAT_MOD_MTK_YUV444_VYU	fourcc_mod_code(MTK, 0x01)

/* extended modifier */
#define DRM_FORMAT_MOD_MTK_COMPRESSED	fourcc_mod_code(MTK, 0x100)
#define DRM_FORMAT_MOD_MTK_6BIT		fourcc_mod_code(MTK, 0x200)
#define DRM_FORMAT_MOD_MTK_10BIT	fourcc_mod_code(MTK, 0x400)
#define DRM_FORMAT_MOD_MTK_MOTION	fourcc_mod_code(MTK, 0x1000)
#define DRM_FORMAT_MOD_MTK_LEGACY	fourcc_mod_code(MTK, 0x2000)

#endif
