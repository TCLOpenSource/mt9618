// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/dma-buf.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/sysfs.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <drm/mtk_tv_drm.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_panel.h>
#include <drm/drm_self_refresh_helper.h>

#include "mtk_tv_drm_plane.h"
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_gop.h"
#include "mtk_tv_drm_encoder.h"
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_pnlgamma.h"
#include "mtk_tv_drm_pqgamma.h"
#include "mtk_tv_drm_video_frc.h"
#include "mtk_tv_drm_oled.h"
#include "mtk_tv_drm_atrace.h"
#include "mtk_tv_drm_ambient_light.h"

#include "mtk_tv_sm_ml.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "mtk_tv_drm_common_irq.h"
#include "mtk_tv_drm_common_clock.h"
#include "mtk_tv_drm_video_pixelshift.h"
#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_common.h"
#include "mtk_tcon_out_if.h"
#include "pqu_msg.h"
#include "m_pqu_render.h"
#include "pqu_render.h"
#include "ext_command_render_if.h"
#include "mtk_tv_drm_sync.h"
#include "mtk-efuse.h"
#include "mtk_tv_drm_trigger_gen.h"
#include "mtk_tv_drm_global_pq.h"
#include "hwreg_render_common.h"
#include "pnl_cust.h"
#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
#include "mi_realtime.h"
#endif
#endif

#define MTK_DRM_MODEL MTK_DRM_MODEL_KMS
#define DTB_MSK 0xFF
#define MAX_SYSFS_SIZE 255
#define INT8_NEGATIVE 0x80
#define DEC_1000000 (1000000)
#define DEC_1000 (1000)
#define MOD_VER_2 (2)
#define CRYSTAL_CLK_X1000 (24000000000LL)
spinlock_t synclockdebugVTT;

uint32_t mtk_drm_log_level; // DRM_INFO is default disable
module_param_named(log_level, mtk_drm_log_level, int, 0644);

DECLARE_WAIT_QUEUE_HEAD(atomic_commit_tail_wait);
DECLARE_WAIT_QUEUE_HEAD(atomic_commit_tail_wait_Grapcrtc);
unsigned long atomic_commit_tail_entry, atomic_commit_tail_Gcrtc_entry;

#define MTK_DRM_KMS_ATTR_MODE     (0644)
#define DRM_PLANE_SCALE_CAPBILITY (1<<4)
#define OLD_LAYER_INITIAL_VALUE   (0xFF)

static __s64 start_crtc_begin;

struct mtk_tv_drm_debug gMtkDrmDbg;

struct task_struct  *panel_handler_task;

struct mtk_tv_kms_vsync_callback_info {
	struct list_head list;
	struct task_struct *task_worker;
	struct wait_queue_head wait_queue_head;
	unsigned long task_entry;
	const char *thread_name;
	char *mi_rt_domain;
	char *mi_rt_class;
	void *priv_data;
	int (*callback_func)(void *priv_data);
};
static LIST_HEAD(mtk_tv_kms_callback_info_list);

static struct drm_prop_enum_list plane_type_enum_list[] = {
	{.type = MTK_DRM_PLANE_TYPE_GRAPHIC,
		.name = MTK_PLANE_PROP_PLANE_TYPE_GRAPHIC},
	{.type = MTK_DRM_PLANE_TYPE_VIDEO,
		.name = MTK_PLANE_PROP_PLANE_TYPE_VIDEO},
	{.type = MTK_DRM_PLANE_TYPE_PRIMARY,
		.name = MTK_PLANE_PROP_PLANE_TYPE_PRIMARY},
};

static const struct ext_prop_info ext_common_plane_props_def[] = {
	{
		.prop_name = MTK_PLANE_PROP_PLANE_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &plane_type_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(plane_type_enum_list),
		.init_val = 0,
	},
};

static struct drm_prop_enum_list ldm_status_enum_list[] = {
	{.type = MTK_CRTC_LDM_STATUS_INIT,
		.name = MTK_CRTC_PROP_LDM_STATUS_INIT},
	{.type = MTK_CRTC_LDM_STATUS_ENABLE,
		.name = MTK_CRTC_PROP_LDM_STATUS_ENABLE},
	{.type = MTK_CRTC_LDM_STATUS_DISABLE,
		.name = MTK_CRTC_PROP_LDM_STATUS_DISABLE},
	{.type = MTK_CRTC_LDM_STATUS_DEINIT,
		.name = MTK_CRTC_PROP_LDM_STATUS_DEINIT},
};


static struct drm_prop_enum_list video_pixelshift_type_enum_list[] = {
	{.type = VIDEO_CRTC_PIXELSHIFT_PRE_JUSTSCAN,
		.name = MTK_CRTC_PROP_PIXELSHIFT_PRE_JUSTSCAN},
	{.type = VIDEO_CRTC_PIXELSHIFT_PRE_OVERSCAN,
		.name = MTK_CRTC_PROP_PIXELSHIFT_PRE_OVERSCAN},
	{.type = VIDEO_CRTC_PIXELSHIFT_POST_JUSTSCAN,
		.name = MTK_CRTC_PROP_PIXELSHIFT_POST_JUSTSCAN},
};


static struct drm_prop_enum_list video_framesync_mode_enum_list[] = {
	{.type = VIDEO_CRTC_FRAME_SYNC_FREERUN, .name = "framesync_freerun"},
	{.type = VIDEO_CRTC_FRAME_SYNC_VTTV, .name = "framesync_vttv"},
	{.type = VIDEO_CRTC_FRAME_SYNC_FRAMELOCK, .name = "framesync_framelock1to1"},
	{.type = VIDEO_CRTC_FRAME_SYNC_VTTPLL, .name = "framesync_vttpll"},
	{.type = VIDEO_CRTC_FRAME_SYNC_MAX, .name = "framesync_max"},
};

static struct drm_prop_enum_list video_gop_order_enum_list[] = {
	{.type = VIDEO_OSDB0_OSDB1,
		.name = MTK_VIDEO_CRTC_PROP_V_OSDB0_OSDB1},
	{.type = OSDB0_VIDEO_OSDB1,
		.name = MTK_VIDEO_CRTC_PROP_OSDB0_V_OSDB1},
	{.type = OSDB0_OSDB1_VIDEO,
		.name = MTK_VIDEO_CRTC_PROP_OSDB0_OSDB1_V},
};

static struct drm_prop_enum_list video_gop_osdb_loc_enum_list[] = {
	{.type = PQGAMMA_OSDB_LD_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_PQGAMMA_OSDB_LD_PADJ},
	{.type = PQGAMMA_LD_OSDB_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_PQGAMMA_LD_OSDB_PADJ},
	{.type = OSDB_PQGAMMA_LD_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_OSDB_PQGAMMA_LD_PADJ},
	{.type = OSDB_LD_PQGAMMA_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_OSDB_LD_PQGAMMA_PADJ},
	{.type = LD_PQGAMMA_OSDB_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_LD_PQGAMMA_OSDB_PADJ},
	{.type = LD_OSDB_PQGAMMA_PADJ,
		.name = MTK_VIDEO_CRTC_PROP_LD_OSDB_PQGAMMA_PADJ},
	{.type = LOCATION_MAX,
		.name = MTK_VIDEO_CRTC_PROP_LOCATION_MAX},
};

static const struct ext_prop_info ext_crtc_props_def[] = {
	/*****panel related properties*****/
	{
		//get framesync frame id
		.prop_name = MTK_CRTC_PROP_FRAMESYNC_PLANE_ID,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		//set framesync ratio
		.prop_name = MTK_CRTC_PROP_FRAMESYNC_FRC_RATIO,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		// set frame sync mode setting
		.prop_name = MTK_CRTC_PROP_FRAMESYNC_MODE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_framesync_mode_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(video_framesync_mode_enum_list),
		.init_val = 0,
	},
	{
		// low latency
		.prop_name = MTK_CRTC_PROP_LOW_LATENCY_MODE,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	{
		//customize frc table
		.prop_name = MTK_CRTC_PROP_FRC_TABLE,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_DISPLAY_TIMING_INFO,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_FREERUN_TIMING,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_FORCE_FREERUN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_FREERUN_VFREQ,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFFFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_VIDEO_LATENCY,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		// set no signal
		.prop_name = MTK_CRTC_PROP_NO_SIGNAL,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	/****LDM crtc prop******/
	{
		.prop_name = MTK_CRTC_PROP_LDM_STATUS,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &ldm_status_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(ldm_status_enum_list),
		//  .flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		//  .range_info.min = 0,
		//  .range_info.max = 5,
		.init_val = MTK_CRTC_LDM_STATUS_DEINIT,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_LUMA,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_ENABLE,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_STRENGTH,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_DATA,
		.flag = DRM_MODE_PROP_BLOB  | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_DEMO_PATTERN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFFFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_SW_SET_CTRL,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_AUTO_LD,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 3,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_XTendedRange,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 3,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_VRR_SEAMLESS,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_INIT,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_SUSPEND,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,

	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_RESUME,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,

	},
	{
		.prop_name = MTK_CRTC_PROP_LDM_SUSPEND_RESUME_TEST,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,

	},
	/****pixelshift crtc prop******/
	{
		.prop_name = MTK_CRTC_PROP_PIXELSHIFT_ENABLE,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_PIXELSHIFT_OSD_ATTACH,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 1,
	},
	{
		.prop_name = MTK_CRTC_PROP_PIXELSHIFT_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_pixelshift_type_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_pixelshift_type_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_PIXELSHIFT_H,
		.flag = DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = -128,
		.range_info.max = 128,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_PIXELSHIFT_V,
		.flag = DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = -128,
		.range_info.max = 128,
		.init_val = 0,
	},
	// demura property
	{
		.prop_name = MTK_CRTC_PROP_DEMURA_CONFIG,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// pattern property
	{
		.prop_name = MTK_CRTC_PROP_PATTERN_GENERATE,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},

	/****global pq crtc prop******/
	{
		.prop_name = PROP_VIDEO_CRTC_PROP_GLOBAL_PQ,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// video crtc mute color
	{
		.prop_name = MTK_VIDEO_CRTC_PROP_MUTE_COLOR,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// video crtc background color
	{
		.prop_name = MTK_VIDEO_CRTC_PROP_BG_COLOR,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// graphic and video property
	{
		.prop_name = MTK_VIDEO_CRTC_PROP_VG_ORDER,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_gop_order_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_gop_order_enum_list),
		.init_val = 0,
	},
	// graphic osdb location
	{
		.prop_name = MTK_VIDEO_CRTC_PROP_OSDB_LOC,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &video_gop_osdb_loc_enum_list[0],
		.enum_info.enum_length =
			ARRAY_SIZE(video_gop_osdb_loc_enum_list),
		.init_val = 0,
	},
	// video crtc backlight property
	{
		.prop_name = MTK_CRTC_PROP_BACKLIGHT_RANGE_CONFIG,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// video crtc live-tone property
	{
		.prop_name = MTK_CRTC_PROP_LIVETONE_RANGE_CONFIG,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	// video crtc pqmap_node array property
	{
		.prop_name = MTK_CRTC_PROP_PQMAP_NODES_ARRAY,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CRTC_PROP_CFD_CSC_RENDER_IN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
};

static const struct ext_prop_info ext_crtc_graphic_props_def[] = {
	/*****graphic crtc related properties*****/
	{
		.prop_name = MTK_CRTC_PROP_VG_SEPARATE,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
};

static struct drm_prop_enum_list crtc_type_enum_list[] = {
	{.type = CRTC_MAIN_PATH,
		.name = MTK_CRTC_COMMON_MAIN_PATH},
	{.type = CRTC_GRAPHIC_PATH,
		.name = MTK_CRTC_COMMON_GRAPHIC_PATH},
	{.type = CRTC_EXT_VIDEO_PATH,
		.name = MTK_CRTC_COMMON_EXT_VIDEO_PATH},
};

static const struct ext_prop_info ext_crtc_common_props_def[] = {
	/*****common crtc related properties*****/
	{
		.prop_name = MTK_CRTC_COMMON_PROP_CRTC_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &crtc_type_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(crtc_type_enum_list),
		.init_val = 0,
	},
};

static struct drm_prop_enum_list connector_disp_path_enum_list[] = {
	{.type = CONNECTOR_MAIN_PATH,
		.name = MTK_CONNECTOR_COMMON_MAIN_PATH},
	{.type = CONNECTOR_GRAPHIC_PATH,
		.name = MTK_CONNECTOR_COMMON_GRAPHIC_PATH},
	{.type = CONNECTOR_EXT_VIDEO_PATH,
		.name = MTK_CONNECTOR_COMMON_EXT_VIDEO_PATH},
};

/*connector properties*/
static const struct ext_prop_info ext_connector_props_def[] = {
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_DBG_LEVEL,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_OUTPUT,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SWING_LEVEL,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_FORCE_PANEL_DCLK,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFFFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_FORCE_PANEL_HSTART,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_OUTPUT_PATTERN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_MIRROR_STATUS,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SSC_EN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 1,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SSC_FMODULATION,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SSC_RDEVIATION,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
	.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_OVERDRIVER_ENABLE,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_PANEL_SETTING,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_INFO,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_PARSE_OUT_INFO_FROM_DTS,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_VBO_CTRLBITS,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_TX_MUTE_EN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SWING_VREG,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_PRE_EMPHASIS,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_SSC,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_OUTPUT_MODEL,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = E_OUT_MODEL_VG_BLENDED,
		.range_info.max = E_OUT_MODEL_MAX,
		.init_val = E_OUT_MODEL_VG_BLENDED,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_CHECK_DTS_OUTPUT_TIMING,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_PROP_PNL_CHECK_FRAMESYNC_MLOAD,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
};

static const struct ext_prop_info ext_connector_common_props_def[] = {
	/*****common connector related properties*****/
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_CONNECTOR_TYPE,
		.flag = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_ATOMIC,
		.enum_info.enum_list = &connector_disp_path_enum_list[0],
		.enum_info.enum_length = ARRAY_SIZE(connector_disp_path_enum_list),
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_PNL_TX_MUTE_EN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		//.range_info.min = 0,
		//.range_info.max = 0xFFFF,
		//.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_PNL_PIXEL_MUTE_EN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		//.range_info.min = 0,
		//.range_info.max = 0xFFFF,
		//.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_BACKLIGHT_MUTE_EN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
		//.range_info.min = 0,
		//.range_info.max = 0xFFFF,
		//.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_HMIRROR_EN,
		.flag = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC,
		.range_info.min = 0,
		.range_info.max = 0xFFFF,
		.init_val = 0,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_PNL_VBO_CTRLBITS,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
	},
	{
		.prop_name = MTK_CONNECTOR_COMMON_PROP_PNL_GLOBAL_MUTE_EN,
		.flag = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC,
	},
};

static int _mtk_kms_check_array_idx(int idx, int size)
{
	int ret = 0;

	if (idx >= size) {
		DRM_ERROR("[%s, %d] array out of bound\n",
			__func__, __LINE__);
		ret = -1;
	}

	return ret;
}

static ssize_t panel_oled_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_oled_info *oled_info = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;
	oled_info = &pctx_pnl->oled_info;

	ret = snprintf(buf, PAGE_SIZE, "================OLED INFO================\n"
			"support          = %d\n"
			"slave_addr       = 0x%X\n"
			"channel_id       = 0x%X\n"
			"i2c_mode         = 0x%X\n"
			"reg_offset       = 0x%X\n"
			"lumin_gain_addr  = 0x%X\n"
			"temp_addr        = 0x%X\n"
			"offrs_addr       = 0x%X\n"
			"offrs_mask       = 0x%X\n"
			"jb_addr          = 0x%X\n"
			"jb_mask          = 0x%X\n"
			"hdr_addr         = 0x%X\n"
			"hdr_mask         = 0x%X\n"
			"lea_addr         = 0x%X\n"
			"lea_mask         = 0x%X\n"
			"tpc_addr         = 0x%X\n"
			"tpc_mask         = 0x%X\n"
			"cpc_addr         = 0x%X\n"
			"cpc_mask         = 0x%X\n"
			"opt_p0_addr      = 0x%X\n"
			"opt_start_addr   = 0x%X\n"
			"opt_start_mask   = 0x%X\n"
			"onrf_op          = %u\n"
			"onrf_delay       = %u\n"
			"offrs_op         = %u\n"
			"jb_op            = %u\n"
			"jb_on_off        = %u\n"
			"jb_temp_max      = %u\n"
			"jb_temp_min      = %u\n"
			"jb_cooldown      = %u\n"
			"qsm_on           = %u\n"
			"qsm_evdd_off     = %u\n"
			"qsm_off          = %u\n"
			"qsm_evdd_on      = %u\n"
			"mon_delay        = %u\n"
			"================OLED INFO END================\n",
			oled_info->oled_support,
			oled_info->oled_i2c.slave,
			oled_info->oled_i2c.channel_id,
			oled_info->oled_i2c.i2c_mode,
			oled_info->oled_i2c.reg_offset,
			oled_info->oled_i2c.lumin_addr,
			oled_info->oled_i2c.temp_addr,
			oled_info->oled_i2c.offrs_addr,
			oled_info->oled_i2c.offrs_mask,
			oled_info->oled_i2c.jb_addr,
			oled_info->oled_i2c.jb_mask,
			oled_info->oled_i2c.hdr_addr,
			oled_info->oled_i2c.hdr_mask,
			oled_info->oled_i2c.lea_addr,
			oled_info->oled_i2c.lea_mask,
			oled_info->oled_i2c.tpc_addr,
			oled_info->oled_i2c.tpc_mask,
			oled_info->oled_i2c.cpc_addr,
			oled_info->oled_i2c.cpc_mask,
			oled_info->oled_i2c.opt_p0_addr,
			oled_info->oled_i2c.opt_start_addr,
			oled_info->oled_i2c.opt_start_mask,
			oled_info->oled_seq.onrf_op,
			oled_info->oled_seq.onrf_delay,
			oled_info->oled_seq.offrs_op,
			oled_info->oled_seq.jb_op,
			oled_info->oled_seq.jb_on_off,
			oled_info->oled_seq.jb_temp_max,
			oled_info->oled_seq.jb_temp_min,
			oled_info->oled_seq.jb_cooldown,
			oled_info->oled_seq.qsm_on,
			oled_info->oled_seq.qsm_evdd_off,
			oled_info->oled_seq.qsm_off,
			oled_info->oled_seq.qsm_evdd_on,
			oled_info->oled_seq.mon_delay);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_oled_support_show(
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

	DRM_INFO("OLED support: %d\n",
		pctx->panel_priv.oled_info.oled_support);

	ret = snprintf(buf, SYS_DEVICE_SIZE, "%d\n",
		pctx->panel_priv.oled_info.oled_support);

	return ((ret < 0) || (ret > SYS_DEVICE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mtk_drm_capability_OLED_PIXELSHIFT_SUPPORT_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	return sysfs_emit(buf, "support_OLED_pixelshift:%d\n",
			pctx_pnl->oled_info.oled_pixelshift);
}

static ssize_t mtk_drm_OLED_PIXELSHIFT_H_RANGE_MAX_show(
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

	DRM_INFO("i8OLEDPixelshiftHRangeMax:%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMax);

	ret = snprintf(buf, SYS_DEVICE_SIZE, "%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMax);

	return ((ret < 0) || (ret > SYS_DEVICE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mtk_drm_OLED_PIXELSHIFT_H_RANGE_MIN_show(
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

	DRM_INFO("i8OLEDPixelshiftHRangeMin:%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMin);

	ret = snprintf(buf, SYS_DEVICE_SIZE, "%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMin);

	return ((ret < 0) || (ret > SYS_DEVICE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mtk_drm_OLED_PIXELSHIFT_V_RANGE_MAX_show(
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

	DRM_INFO("i8OLEDPixelshiftVRangeMax:%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMax);

	ret = snprintf(buf, SYS_DEVICE_SIZE, "%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMax);

	return ((ret < 0) || (ret > SYS_DEVICE_SIZE)) ? -EINVAL : ret;
}

static ssize_t mtk_drm_OLED_PIXELSHIFT_V_RANGE_MIN_show(
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

	DRM_INFO("i8OLEDPixelshiftVRangeMin:%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMin);

	ret = snprintf(buf, SYS_DEVICE_SIZE, "%d\n",
		pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMin);

	return ((ret < 0) || (ret > SYS_DEVICE_SIZE)) ? -EINVAL : ret;
}

static ssize_t oled_pixelshift_show(
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

	ret = snprintf(buf, PAGE_SIZE, "OLED pixel shift status:\n"
				"		- enable = %d\n"
				"		- attach = %d\n"
				"		- type =b %u\n"
				"		- h = %d\n"
				"		- v = %d\n",
				pctx->pixelshift_priv.bIsOLEDPixelshiftEnable,
				pctx->pixelshift_priv.bIsOLEDPixelshiftOsdAttached,
				pctx->pixelshift_priv.u8OLEDPixleshiftType,
				pctx->pixelshift_priv.i8OLEDPixelshiftHDirection,
				pctx->pixelshift_priv.i8OLEDPixelshiftVDirection);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t oled_pixelshift_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (mtk_video_pixelshift_debug(buf, dev) < 0) {
		DRM_ERROR("[%s, %d]: mtk_video_pixelshift_debug failed!\n",
			__func__, __LINE__);
	}

	return count;
}

static ssize_t help_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "DRM Info log control bit:\n"
				"		- COMMON    = 0x%08lx\n"
				"		- DISPLAY   = 0x%08lx\n"
				"		- PANEL     = 0x%08lx\n"
				"		- CONNECTOR = 0x%08lx\n"
				"		- CRTC      = 0x%08lx\n"
				"		- DRV       = 0x%08lx\n"
				"		- ENCODER   = 0x%08lx\n"
				"		- FB        = 0x%08lx\n"
				"		- GEM       = 0x%08lx\n"
				"		- KMS       = 0x%08lx\n"
				"		- PATTERN   = 0x%08lx\n"
				"		- PLANE     = 0x%08lx\n"
				"		- VIDEO     = 0x%08lx\n"
				"		- GOP       = 0x%08lx\n"
				"		- DEMURA    = 0x%08lx\n"
				"		- PNLGAMMA  = 0x%08lx\n"
				"		- PQMAP     = 0x%08lx\n"
				"		- AUTOGEN   = 0x%08lx\n"
				"		- TVDAC     = 0x%08lx\n"
				"		- METABUF   = 0x%08lx\n"
				"		- FENCE     = 0x%08lx\n"
				"		- PQU_METADATA = 0x%08lx\n"
				"		- OLED      = 0x%08lx\n"
				"		- LD        = 0x%08lx\n"
				"		- GLOBAL_PQ = 0x%08lx\n"
				"		- SCRIPTMGT_ML = 0x%08lx\n",
				MTK_DRM_MODEL_COMMON,
				MTK_DRM_MODEL_DISPLAY,
				MTK_DRM_MODEL_PANEL,
				MTK_DRM_MODEL_CONNECTOR,
				MTK_DRM_MODEL_CRTC,
				MTK_DRM_MODEL_DRV,
				MTK_DRM_MODEL_ENCODER,
				MTK_DRM_MODEL_FB,
				MTK_DRM_MODEL_GEM,
				MTK_DRM_MODEL_KMS,
				MTK_DRM_MODEL_PATTERN,
				MTK_DRM_MODEL_PLANE,
				MTK_DRM_MODEL_VIDEO,
				MTK_DRM_MODEL_GOP,
				MTK_DRM_MODEL_DEMURA,
				MTK_DRM_MODEL_PNLGAMMA,
				MTK_DRM_MODEL_PQMAP,
				MTK_DRM_MODEL_AUTOGEN,
				MTK_DRM_MODEL_TVDAC,
				MTK_DRM_MODEL_METABUF,
				MTK_DRM_MODEL_FENCE,
				MTK_DRM_MODEL_PQU_METADATA,
				MTK_DRM_MODEL_OLED,
				MTK_DRM_MODEL_LD,
				MTK_DRM_MODEL_GLOBAL_PQ,
				MTK_DRM_MODEL_SCRIPTMGT_ML);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t log_level_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "DRM Info log status:\n"
				"		- COMMON    = %d\n"
				"		- DISPLAY   = %d\n"
				"		- PANEL     = %d\n"
				"		- CONNECTOR = %d\n"
				"		- CRTC      = %d\n"
				"		- DRV       = %d\n"
				"		- ENCODER   = %d\n"
				"		- FB        = %d\n"
				"		- GEM       = %d\n"
				"		- KMS       = %d\n"
				"		- PATTERN   = %d\n"
				"		- PLANE     = %d\n"
				"		- VIDEO     = %d\n"
				"		- GOP       = %d\n"
				"		- DEMURA    = %d\n"
				"		- PNLGAMMA  = %d\n"
				"		- PQMAP     = %d\n"
				"		- AUTOGEN   = %d\n"
				"		- TVDAC     = %d\n"
				"		- METABUF   = %d\n"
				"		- FENCE     = %d\n"
				"		- PQU_METADATA = %d\n"
				"		- OLED      = %d\n"
				"		- LD        = %d\n"
				"		- GLOBAL_PQ = %d\n"
				"		- SCRIPTMGT_ML = %d\n",
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_COMMON),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_DISPLAY),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PANEL),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_CONNECTOR),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_CRTC),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_DRV),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_ENCODER),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_FB),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_GEM),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_KMS),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PATTERN),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PLANE),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_VIDEO),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_GOP),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_DEMURA),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PNLGAMMA),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PQMAP),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_AUTOGEN),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_TVDAC),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_METABUF),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_FENCE),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_PQU_METADATA),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_OLED),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_LD),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_GLOBAL_PQ),
				(bool)(mtk_drm_log_level & MTK_DRM_MODEL_SCRIPTMGT_ML));

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t log_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	mtk_drm_log_level = val;
	return count;
}

static ssize_t atrace_enable_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = snprintf(buf, PAGE_SIZE, "drm_atrace enable:%d\n", gMtkDrmDbg.atrace_enable);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t atrace_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	gMtkDrmDbg.atrace_enable = val;
	return count;
}

static ssize_t panel_v_flip_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	bool vflip = FALSE;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	vflip = (pctx->panel_priv.cus.mirror_mode == E_PNL_MIRROR_V ||
			      pctx->panel_priv.cus.mirror_mode == E_PNL_MIRROR_V_H);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", vflip);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_h_flip_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	bool hflip = FALSE;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	hflip = (pctx->panel_priv.cus.mirror_mode == E_PNL_MIRROR_H ||
			      pctx->panel_priv.cus.mirror_mode == E_PNL_MIRROR_V_H);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", hflip);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t vrr_max_v_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx_pnl->info.vrr_max_v);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t vrr_min_v_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "%d\n",  pctx_pnl->info.vrr_min_v);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t vrr_tolerance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	uint64_t vrr_tolerance = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	/* vrr_tolerance: need refine VRR_TOLERANCE_OUPTUT define in dtso*/
	if (IS_OUTPUT_FRAMERATE_60(pctx_pnl->outputTiming_info.u32OutputVfreq))
		vrr_tolerance = VRR_TOLERANCE_OUPTUT_60;
	else if (IS_OUTPUT_FRAMERATE_120(pctx_pnl->outputTiming_info.u32OutputVfreq))
		vrr_tolerance = VRR_TOLERANCE_OUPTUT_120;
	else if (IS_OUTPUT_FRAMERATE_144(pctx_pnl->outputTiming_info.u32OutputVfreq))
		vrr_tolerance = VRR_TOLERANCE_OUPTUT_144;
	else if (IS_OUTPUT_FRAMERATE_240(pctx_pnl->outputTiming_info.u32OutputVfreq))
		vrr_tolerance = VRR_TOLERANCE_OUPTUT_240;
	else
		vrr_tolerance = 5;


	ret = snprintf(buf, PAGE_SIZE, "%llu\n", vrr_tolerance);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_mod_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_st_mod_status stPnlModStatus = {0};
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mtk_render_get_mod_status(&stPnlModStatus);

	ret = snprintf(buf, PAGE_SIZE, "mod_status:\n"
				"		- vby1_locked(0:locked 1:unlock) = %d\n"
				"		- vby1_htpdn(0:locked 1:unlock) = %d\n"
				"		- vby1_lockn(0:locked 1:unlock) = %d\n"
				"		- vby1_unlockcnt = %d\n"
				"		- outconfig ch0~ch7 = 0x%04x, ch8~ch15= 0x%04x, ch16~ch19= 0x%04x\n",
				stPnlModStatus.vby1_locked,
				stPnlModStatus.vby1_htpdn,
				stPnlModStatus.vby1_lockn,
				stPnlModStatus.vby1_unlockcnt,
				stPnlModStatus.outconfig.outconfig_ch0_ch7,
				stPnlModStatus.outconfig.outconfig_ch8_ch15,
				stPnlModStatus.outconfig.outconfig_ch16_ch19);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_out_model_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->out_model);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t framesync_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->framesync_version);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx     = NULL;
	struct mtk_panel_context  *pctx_pnl = NULL;
	//get something from the data structure
	uint16_t BANK_REG = 0;
	#define BANK_SIZE 0x80
	#define END_OF_LINE 8
	int bnk_reg = 0;
	int len = 0;
	int j = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;
	BANK_REG = pctx_pnl->dump_reg_bank;

	//printf current reg BANK
	len += snprintf(buf + len,
			PAGE_SIZE - len,
			"\n\n[BANK_REG]:0x%04X\n\n",
			BANK_REG);
	//pribrf for first column
	len += snprintf(buf + len,
			PAGE_SIZE - len,
			" [00]");
	//dump 8x8
	for (; bnk_reg < BANK_SIZE; bnk_reg++) {
		if (j == END_OF_LINE) {
			len += snprintf(buf + len,
			PAGE_SIZE - len,
			"\n [%02X]",
			bnk_reg);
			j = 0;
		}
		//Dump Hwreg by call hwreg function
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"  %04X",
				drv_hwreg_render_pnl_get_bank_dump(BANK_REG, bnk_reg));
			j++;
	}
	//DUMP register
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	#undef BANK_SIZE
	#undef END_OF_LINE

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t panel_reg_dump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx     = NULL;
	struct mtk_panel_context  *pctx_pnl = NULL;
	uint32_t val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	err = kstrtou32(buf, 0, &val);
	if (err)
		return err;
	DRM_INFO("[%s, %d] Dump Reg Bank: %d\n", __func__, __LINE__, val);
	pctx_pnl->dump_reg_bank = val;
	return count;
}

static ssize_t panel_TgenVTT_CheckIvsOvs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	#define DEBUG_INPUTVTT 0xA
	#define DEBUG_CURRENTVTT 0x4
	#define DEBUG_DELAYTIME 20
	#define DEBUG_DIFFLINE 100
	uint16_t u16MainVTT = 0;
	uint16_t u16InputVTTDebugValueInput = 0;
	uint16_t u16InputVTTDebugValueCurrent = 0;
	bool bIsInputMainDiffBigthan100 = false;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	u16MainVTT = mtk_tv_drm_panel_get_TgenMainVTT();
	// get Input VTT debug mode
	mtk_tv_drm_panel_set_VTT_debugmode(DEBUG_INPUTVTT);
	mdelay(DEBUG_DELAYTIME);
	u16InputVTTDebugValueInput = mtk_tv_drm_panel_get_VTT_debugValue();
	// get Current VTT debug mode
	mtk_tv_drm_panel_set_VTT_debugmode(DEBUG_CURRENTVTT);
	mdelay(DEBUG_DELAYTIME);
	u16InputVTTDebugValueCurrent = mtk_tv_drm_panel_get_VTT_debugValue();

	if (u16InputVTTDebugValueCurrent >= u16InputVTTDebugValueInput) {
		if ((u16InputVTTDebugValueCurrent - u16InputVTTDebugValueInput) > DEBUG_DIFFLINE)
			bIsInputMainDiffBigthan100 = true;
	} else {
		if ((u16InputVTTDebugValueInput - u16InputVTTDebugValueCurrent) > DEBUG_DIFFLINE)
			bIsInputMainDiffBigthan100 = true;
	}

	#undef DEBUG_DIFFLINE
	#undef DEBUG_DELAYTIME
	#undef DEBUG_CURRENTVTT
	#undef DEBUG_INPUTVTT

	ret = snprintf(buf, PAGE_SIZE,
		"bIsInputMainDiffBigthan100 = %d\n"
		"u16MainVTT = %d\n"
		"u16InputVTTDebugValueInput = %d\n"
		"u16InputVTTDebugValueCurrent = %d\n", bIsInputMainDiffBigthan100,
		u16MainVTT, u16InputVTTDebugValueInput, u16InputVTTDebugValueCurrent);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t frame_sync_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	#define XTAL_CLK 24000000000
	#define HW_SW_DIFF_TH 500 //0.5hz
	struct mtk_tv_kms_context *pctx = NULL;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t input_vfreq = 0;
	int ret = 0;
	st_Vtt_status stVTTInfo = {0};
	uint64_t HW_out_framerate = 0;
	uint64_t HW_in_framerate = 0;
	uint64_t HW_SW_in_framerate_diff = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	crtc_props = pctx->ext_crtc_properties;

	ret = mtk_get_framesync_locked_flag(pctx);
	if (crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID] != 0) {
		ret = mtk_video_get_vttv_input_vfreq(pctx,
			crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID],
			&input_vfreq);
	}

	drv_hwreg_render_pnl_get_Vttv_status(&stVTTInfo);
	if (stVTTInfo.xtail_cnt > 0) {
		HW_out_framerate = XTAL_CLK / stVTTInfo.xtail_cnt; //unit 0.0001 hz
		if (stVTTInfo.Ivs > 0)
			HW_in_framerate = HW_out_framerate * stVTTInfo.Ivs / stVTTInfo.Ovs;
	}
	if (input_vfreq > HW_in_framerate)
		HW_SW_in_framerate_diff = (input_vfreq - HW_in_framerate);
	else
		HW_SW_in_framerate_diff = (HW_in_framerate - input_vfreq);

	ret = snprintf(buf, PAGE_SIZE, "SW frame_sync_status:\n"
				"		- frame_sync_mode = %d\n"
				"		- ivs = %d\n"
				"		- ovs = %d\n"
				"		- input frame rate = %llu\n"
				"		- output frame rate = %d\n"
				"		- output VTotal = %d\n"
				"		- lock flag = %d\n"
				"\nHW frame_sync_status:\n"
				"		- lock En = %d\n"
				"		- lock flag = %d\n"
				"		- ivs = %d\n"
				"		- ovs = %d\n"
				"		- in_frame_rate = %llu\n"
				"		- out_frame_rate = %llu\n"
				"		- in_Vcnt = %d\n"
				"		- out_Vcnt = %d\n"
				"\n		- HW/SW in framerate diff = %llu *0.001 hz\n"
				"		- HW/SW in framerate diff > 0.5hz: %d\n",
				pctx->panel_priv.outputTiming_info.eFrameSyncMode,
				pctx->panel_priv.outputTiming_info.u8FRC_in,
				pctx->panel_priv.outputTiming_info.u8FRC_out,
				input_vfreq,
				pctx->panel_priv.outputTiming_info.u32OutputVfreq,
				pctx->panel_priv.outputTiming_info.u16OutputVTotal,
				pctx->panel_priv.outputTiming_info.locked_flag,
				stVTTInfo.lock_en,
				stVTTInfo.lock_flag,
				stVTTInfo.Ivs,
				stVTTInfo.Ovs,
				HW_in_framerate,
				HW_out_framerate,
				stVTTInfo.Vtt_input,
				stVTTInfo.Vcnt,
				HW_SW_in_framerate_diff,
				(HW_SW_in_framerate_diff > HW_SW_DIFF_TH)?1:0);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_mode_ID_Sel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	uint64_t Output_frame_rate = 0;
	__u16 typ_htt = 0;
	__u16 typ_vtt = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	typ_htt = pctx->panel_priv.info.typ_htt;
	typ_vtt = pctx->panel_priv.info.typ_vtt;

	if ((typ_htt != 0) && (typ_vtt != 0))
		Output_frame_rate =	pctx->panel_priv.info.typ_dclk / typ_htt / typ_vtt;

	ret = snprintf(buf, PAGE_SIZE, "Panel mode ID:\n"
				"		- Mode ID name = %s\n"
				"		- linkIF (0:None, 1:VB1, 2:LVDS) = %d\n"
				"		- VBO byte mode = %d\n"
				"		- div_section = %d\n"
				"		- Hsync Start = %d\n"
				"		- HSync Width = %d\n"
				"		- Hsync Polarity = %d\n"
				"		- Vsync Start = %d\n"
				"		- VSync Width = %d\n"
				"		- VSync Polarity = %d\n"
				"		- Panel HStart = %d\n"
				"		- Panel VStart = %d\n"
				"		- Panel Width = %d\n"
				"		- Panel Height = %d\n"
				"		- Panel HTotal = %d\n"
				"		- Panel MaxVTotal = %d\n"
				"		- Panel VTotal = %d\n"
				"		- Panel MinVTotal = %d\n"
				"		- Panel MaxVTotalPanelProtect = %d\n"
				"		- Panel MinVTotalPanelProtect = %d\n"
				"		- Panel DCLK = %llu\n"
				"		- Output frame rate = %llu\n"
				"		- Output Format = %d\n"
				"		- DTSI File Type = %d\n"
				"		- DTSI File Ver = %d\n"
				"		- SSC En = %d\n"
				"		- SSC Step = %d\n"
				"		- SSC Span = %d\n",
				pctx->panel_priv.info.pnlname,
				pctx->panel_priv.info.linkIF,
				pctx->panel_priv.info.vbo_byte,
				pctx->panel_priv.info.div_section,
				pctx->panel_priv.info.hsync_st,
				pctx->panel_priv.info.hsync_w,
				pctx->panel_priv.info.hsync_pol,
				pctx->panel_priv.info.vsync_st,
				pctx->panel_priv.info.vsync_w,
				pctx->panel_priv.info.vsync_pol,
				pctx->panel_priv.info.de_hstart,
				pctx->panel_priv.info.de_vstart,
				pctx->panel_priv.info.de_width,
				pctx->panel_priv.info.de_height,
				pctx->panel_priv.info.typ_htt,
				pctx->panel_priv.info.max_vtt,
				pctx->panel_priv.info.typ_vtt,
				pctx->panel_priv.info.min_vtt,
				pctx->panel_priv.info.max_vtt_panelprotect,
				pctx->panel_priv.info.min_vtt_panelprotect,
				pctx->panel_priv.info.typ_dclk,
				Output_frame_rate,
				pctx->panel_priv.info.op_format,
				pctx->panel_priv.cus.dtsi_file_type,
				pctx->panel_priv.cus.dtsi_file_ver,
				pctx->panel_priv.stdrmSSC.ssc_setting.ssc_en,
				pctx->panel_priv.stdrmSSC.ssc_setting.ssc_modulation,
				pctx->panel_priv.stdrmSSC.ssc_setting.ssc_deviation);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_mode_ID_deltav_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	uint64_t Output_frame_rate = 0;
	__u16 typ_htt = 0;
	__u16 typ_vtt = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	//panel_priv.extdev_extdev_info
	typ_htt = pctx->panel_priv.extdev_info.typ_htt;
	typ_vtt = pctx->panel_priv.extdev_info.typ_vtt;

	if ((typ_htt != 0) && (typ_vtt != 0))
		Output_frame_rate =	pctx->panel_priv.extdev_info.typ_dclk / typ_htt / typ_vtt;

	ret = snprintf(buf, PAGE_SIZE, "Panel mode ID:\n"
				"		- Mode ID name = %s\n"
				"		- linkIF (0:None, 1:VB1, 2:LVDS) = %d\n"
				"		- VBO byte mode = %d\n"
				"		- div_section = %d\n"
				"		- Hsync Start = %d\n"
				"		- HSync Width = %d\n"
				"		- Hsync Polarity = %d\n"
				"		- Vsync Start = %d\n"
				"		- VSync Width = %d\n"
				"		- VSync Polarity = %d\n"
				"		- Panel HStart = %d\n"
				"		- Panel VStart = %d\n"
				"		- Panel Width = %d\n"
				"		- Panel Height = %d\n"
				"		- Panel HTotal = %d\n"
				"		- Panel MaxVTotal = %d\n"
				"		- Panel VTotal = %d\n"
				"		- Panel MinVTotal = %d\n"
				"		- Panel MaxVTotalPanelProtect = %d\n"
				"		- Panel MinVTotalPanelProtect = %d\n"
				"		- Panel DCLK = %llu\n"
				"		- Output frame rate = %llu\n"
				"		- Output Format = %d\n"
				"		- DTSI File Type = %d\n"
				"		- DTSI File Ver = %d\n",
				pctx->panel_priv.extdev_info.pnlname,
				pctx->panel_priv.extdev_info.linkIF,
				pctx->panel_priv.extdev_info.vbo_byte,
				pctx->panel_priv.extdev_info.div_section,
				pctx->panel_priv.extdev_info.hsync_st,
				pctx->panel_priv.extdev_info.hsync_w,
				pctx->panel_priv.extdev_info.hsync_pol,
				pctx->panel_priv.extdev_info.vsync_st,
				pctx->panel_priv.extdev_info.vsync_w,
				pctx->panel_priv.extdev_info.vsync_pol,
				pctx->panel_priv.extdev_info.de_hstart,
				pctx->panel_priv.extdev_info.de_vstart,
				pctx->panel_priv.extdev_info.de_width,
				pctx->panel_priv.extdev_info.de_height,
				pctx->panel_priv.extdev_info.typ_htt,
				pctx->panel_priv.extdev_info.max_vtt,
				pctx->panel_priv.extdev_info.typ_vtt,
				pctx->panel_priv.extdev_info.min_vtt,
				pctx->panel_priv.extdev_info.max_vtt_panelprotect,
				pctx->panel_priv.extdev_info.min_vtt_panelprotect,
				pctx->panel_priv.extdev_info.typ_dclk,
				Output_frame_rate,
				pctx->panel_priv.extdev_info.op_format,
				pctx->panel_priv.cus.dtsi_file_type,
				pctx->panel_priv.cus.dtsi_file_ver);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t output_frame_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->panel_priv.outputTiming_info.u32OutputVfreq);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_Backlight_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	gpiod_set_value(pctx->panel_priv.gpio_backlight, 1);
	ret = snprintf(buf, PAGE_SIZE, "BL on\n");

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_Backlight_off_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	gpiod_set_value(pctx->panel_priv.gpio_backlight, 0);
	ret = snprintf(buf, PAGE_SIZE, "BL off\n");

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

//pixel_mute_dbg store rgb parameter
static ssize_t pixel_mute_rgb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#define _32BITMAX 0xffff
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	st_drv_pixel_mute_pattern pxl_mute_pat_info = {0};//struct of pixel mute debug info
	static const char _sep[] = " ";
	uint32_t pixel_mute_rgb_set[MTK_DRM_PIXEL_MUTE_CMD_MAX] = {0};

	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	ret = mtk_video_panel_get_all_value_from_string(
			(char *)buf,
			(char *)_sep,
			count,
			pixel_mute_rgb_set);
	if (ret < 0) {
		DRM_ERROR("[%s, %d]: String Parser Failed!\n", __func__, __LINE__);
		return ret;
	}
	//store the data structure  echo
	memcpy(pctx_panel->pixel_mute_rgb_info, pixel_mute_rgb_set, sizeof(pixel_mute_rgb_set));
	//start to trigger the hardware register

	pxl_mute_pat_info.bEn = pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_EN];
	pxl_mute_pat_info.bLatchMode
		= pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_VLATCH];
	pxl_mute_pat_info.u16Red
		= pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_RED];
	pxl_mute_pat_info.u16Green
		= pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_GREEN];
	pxl_mute_pat_info.u16Blue
		= pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_BLUE];
	pxl_mute_pat_info.u8ConnectorID
		= pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_CONNECTOR_TYPE];

	pxl_mute_pat_info.h_start = _32BITMAX;
	pxl_mute_pat_info.h_end   = _32BITMAX;
	pxl_mute_pat_info.v_start = _32BITMAX;
	pxl_mute_pat_info.v_end   = _32BITMAX;

	//bEn: True ,LatchMode: False
	if (pxl_mute_pat_info.bLatchMode == 0 &&  pxl_mute_pat_info.bEn)
		pxl_mute_pat_info.h_start = 0;


	switch (pxl_mute_pat_info.u8ConnectorID) {
	case MTK_DRM_CONNECTOR_TYPE_VIDEO:
		drv_hwreg_render_pnl_set_mod_pattern_video(&pxl_mute_pat_info);
		break;
	case MTK_DRM_CONNECTOR_TYPE_GRAPHIC:
		drv_hwreg_render_pnl_set_mod_pattern_gfx(&pxl_mute_pat_info);
		break;
	case MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO:
		drv_hwreg_render_pnl_set_mod_pattern_deltav(&pxl_mute_pat_info);
		break;
	default:
		ret = -EINVAL;
		break;
	}
#undef _32BITMAX
	return count;
}

//pixel_mute_dbg show pattern
static ssize_t pixel_mute_rgb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#define _32BITMAX 0xffff
	int ret = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	char *video_type = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	//pctx_panel get data
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	if (pctx_panel == NULL) {
		DRM_ERROR("[%s, %d]: pctx_panel Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	//Video type
	switch (pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_CONNECTOR_TYPE]) {
	case MTK_DRM_CONNECTOR_TYPE_VIDEO:
		video_type = "MAIN_VIDEO";
		break;
	case MTK_DRM_CONNECTOR_TYPE_GRAPHIC:
		video_type = "GRAPHIC_VIDEO";
		break;
	case MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO:
		video_type = "EXT_VIDEO";
		break;
	default:
		video_type = "WRONG_TYPE_VIDEO";
		break;
	}

	//copy to user from what stored in pixel_mute_rgb_info
	ret = snprintf(buf, PAGE_SIZE, "pixel_mute_rgb info:\n"
				"		- bEnable     = %d\n"
				"		- bLatchMode  = %d\n"
				"		- u32Red      = %d\n"
				"		- u32Green    = %d\n"
				"		- u32Blue     = %d\n"
				"		- video_type  = %s\n",
				pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_EN],
				pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_VLATCH],
				pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_RED],
				pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_GREEN],
				pctx_panel->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_BLUE],
				video_type);
#undef _32BITMAX
	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

//mod pattern store v_start h_start parameter
static ssize_t mod_pattern_set_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	static const char _sep[] = " ";
	uint32_t mod_ptrn_set_cmd[MTK_DRM_SET_MOD_CMD_MAX] = {};
	st_drv_pixel_mute_pattern pxl_mute_pat_info = {0};//struct of mod pat set  info
	int ret = 0;

	ret = mtk_video_panel_get_all_value_from_string(
			(char *)buf,
			(char *)_sep,
			count,
			mod_ptrn_set_cmd);
	if (ret < 0) {
		DRM_ERROR("[%s, %d]: String Parser Failed!\n", __func__, __LINE__);
		return -EINVAL;
	}
	//pctx_panel get data
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	//store the data structure fro, echo
	memcpy(pctx_panel->mod_ptrn_set_info,
		mod_ptrn_set_cmd,
		sizeof(mod_ptrn_set_cmd));
	//assign commnasd data to store in the structure
	pxl_mute_pat_info.bEn
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_EN];
	pxl_mute_pat_info.bLatchMode
		= 0;	//bLatchMode always disable at mod pat set
	pxl_mute_pat_info.u16Red
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_RED];
	pxl_mute_pat_info.u16Green
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_GREEN];
	pxl_mute_pat_info.u16Blue
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_BLUE];
	pxl_mute_pat_info.u8ConnectorID
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_CONNECTOR_TYPE];
	pxl_mute_pat_info.h_start
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_H_ST];
	pxl_mute_pat_info.h_end
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_H_END];
	pxl_mute_pat_info.v_start
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_V_ST];
	pxl_mute_pat_info.v_end
		= pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_V_END];

	switch (pxl_mute_pat_info.u8ConnectorID) {
	case MTK_DRM_CONNECTOR_TYPE_VIDEO:
		drv_hwreg_render_pnl_set_mod_pattern_video(&pxl_mute_pat_info);
	break;
	case MTK_DRM_CONNECTOR_TYPE_GRAPHIC:
		drv_hwreg_render_pnl_set_mod_pattern_gfx(&pxl_mute_pat_info);
		break;
	case MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO:
		drv_hwreg_render_pnl_set_mod_pattern_deltav(&pxl_mute_pat_info);
		break;
	default:
		ret = -EINVAL;
		break;
	};

	if (ret < 0)
		DRM_ERROR("[%s, %d]Invalid Input!\n", __func__, __LINE__);

	return count;
}

//mod pattern store v_start h_start parameter
static ssize_t mod_pattern_set_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	char *video_type = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	//pctx_panel get data
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	if (pctx_panel == NULL) {
		DRM_ERROR("[%s, %d]: pctx_panel Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	//Video type
	switch (pctx_panel->mod_ptrn_set_info[MTK_DRM_PIXEL_MUTE_CONNECTOR_TYPE]) {
	case MTK_DRM_CONNECTOR_TYPE_VIDEO:
		video_type = "MAIN_VIDEO";
		break;
	case MTK_DRM_CONNECTOR_TYPE_GRAPHIC:
		video_type = "GRAPHIC_VIDEO";
		break;
	case MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO:
		video_type = "EXT_VIDEO";
		break;
	default:
		video_type = "WRONG_TYPE_VIDEO";
		break;
	}

	ret = snprintf(buf, PAGE_SIZE, "mod_pattern_set info:\n"
				"		- bEnable     = %d\n"
				"		- u32Red      = %d\n"
				"		- u32Green    = %d\n"
				"		- u32Blue     = %d\n"
				"		- video_type  = %s\n"
				"		- h_start     = %d\n"
				"		- h_end       = %d\n"
				"		- v_start     = %d\n"
				"		- v_end       = %d\n",
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_EN],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_RED],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_GREEN],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_BLUE],
				video_type,
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_H_ST],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_H_END],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_V_ST],
				pctx_panel->mod_ptrn_set_info[MTK_DRM_SET_MOD_V_END]);
	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t main_video_pixel_mute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	#define PATTERN_RED_VAL (0xFFF)
	int ret = 0;
	drm_st_pixel_mute_info pixel_mute_info = {0};

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pixel_mute_info.bEnable    = true;
	pixel_mute_info.bLatchMode = true;
	pixel_mute_info.u32Red     = PATTERN_RED_VAL;
	pixel_mute_info.u32Green   = 0;
	pixel_mute_info.u32Blue    = 0;
	mtk_render_set_pixel_mute_video(pixel_mute_info);
	mtk_render_set_pixel_mute_deltav(pixel_mute_info);
	mtk_render_set_pixel_mute_gfx(pixel_mute_info);
	#undef PATTERN_RED_VAL
	ret = snprintf(buf, PAGE_SIZE, "main video pixel mute enable\n");

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t main_video_pixel_unmute_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	drm_st_pixel_mute_info pixel_mute_info = {0};
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pixel_mute_info.bEnable = false;
	pixel_mute_info.bLatchMode = true;
	pixel_mute_info.u32Red = 0;
	pixel_mute_info.u32Green = 0;
	pixel_mute_info.u32Blue = 0;
	mtk_render_set_pixel_mute_video(pixel_mute_info);
	mtk_render_set_pixel_mute_deltav(pixel_mute_info);
	mtk_render_set_pixel_mute_gfx(pixel_mute_info);
	ret = snprintf(buf, PAGE_SIZE, "main video pixel mute disable\n");

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t pattern_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_frc_context *pctx_frc = NULL;

	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_frc = &pctx->frc_priv;

	ret = snprintf(buf, PAGE_SIZE, "\n[frc pattern]%d\n", pctx_frc->frc_pattern_sel);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t memc_level_frc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	static const char _sep[] = " ";
	uint16_t TokenCnt;
	uint16_t cmdval[RV55_CMD_LENGTH];
	//int i;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	cmdval[0] = FRC_MB_CMD_SET_MECM_LEVEL;
	TokenCnt = 0;
	mtk_frc_get_all_value_from_string((char *)buf, (char *)_sep, count, cmdval+1, &TokenCnt);
	mtk_video_display_frc_set_rv55_burst_cmd(cmdval, TokenCnt);
	return count;
}

static ssize_t memc_level_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t memc_level[RV55_CMD_LENGTH-1];
	int len = 0;
	int i;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mtk_video_display_frc_get_memc_level(memc_level,
		RV55_CMD_LENGTH-1); //get memc level from json

	#define _CHARNUM 4
	#define DEBUG_INFO_SHOW(title, size, elem) do { \
	len += snprintf(buf + len, PAGE_SIZE - len, "%s |\n", title); \
	len += snprintf(buf + len, PAGE_SIZE - len, "MEMC Level from Json |\n"); \
	for (i = 0; i < (size); i++) { \
		len += snprintf(buf + len, PAGE_SIZE - len, " %8llx |", (uint64_t)(elem)); \
		if ((i%_CHARNUM) == (_CHARNUM-1)) \
			len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} \
	len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} while (0)

	DEBUG_INFO_SHOW("MEMC Level", RV55_CMD_LENGTH-1, memc_level[i]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	#undef DEBUG_INFO_SHOW
	#undef _CHARNUM

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t frc_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	#define MFC_FRAMENUM_MASK(X) ((X) & (0x000F))
	#define MFC_RWDIFF_MASK(X) (((X) & (0x00F0))>>4)
	#define MFC_LEVL_MASK(X) ((X) & (0x000F))
	#define MFC_DEBLUR_MASK(X) (((X) & (0x0F00))>>8)
	#define MFC_DEJUDDER_MASK(X) (((X) & (0xF000))>>12)
	#define MFC_BW_MASK(X) (((X) & (0x0300))>>8)
	#define MFC_GAME_MASK(X) (((X) & (0x0800))>>11)
	#define MFC_GAME_MEMC_MASK(X) (((X) & (0x4000))>>14)

	uint16_t BlendingHsize, BlendingVsize;
	uint64_t TGENOutTimingX1000 = 0, MIVsyncVline = 0;
	int ret = 0;
	struct hwregFrcDebugInfo stFrcDebugInfo = {0};
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_frc_context *pctx_frc = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_frc = &pctx->frc_priv;

	memset(&stFrcDebugInfo, 0, sizeof(struct hwregFrcDebugInfo));

	drv_hwreg_render_frc_get_debug_info(&stFrcDebugInfo);
	BlendingHsize = stFrcDebugInfo.BlendingHEd - stFrcDebugInfo.BlendingHSt + 1;
	BlendingVsize = stFrcDebugInfo.BlendingVEd - stFrcDebugInfo.BlendingVSt + 1;
	if (stFrcDebugInfo.TgenOutTiming != 0)
		TGENOutTimingX1000 = CRYSTAL_CLK_X1000 / stFrcDebugInfo.TgenOutTiming;

	MIVsyncVline = stFrcDebugInfo.MIVsync * (stFrcDebugInfo.MISwHttSize + 1) *
			((uint32_t)stFrcDebugInfo.TgenVtt) * TGENOutTimingX1000 / CRYSTAL_CLK_X1000;


	ret = snprintf(buf, PAGE_SIZE, "		SW frc_status:\n"
				"\n		****  frc miu dump state ****\n"
				"		[IPM ADDR: 0x%X][R_RST:%s][W_RST:%s]\n"
				"		[OPM ADDR: 0x%X][ME_RST:%s][MI_RST:%s]\n"
				"		[IOPM W/R lmt: 0x%X - 0x%X][EN:%d]\n"
				"		[IPM MEDS ADDR: 0x%X][R_RST:%s][W_RST:%s]\n"
				"		[OPM MEDS ADDR: 0x%X]\n"
				"		[IOPM MEDS W/R lmt: 0x%X - 0x%X][EN:%d]\n"
				"\n		****  frc_pipe_line_status ****\n"
				"		- W/R Idx	Ipm = %d, OpmF2 = %d\n"
				"		- IpmSrc	HSize = %d, VSize = %d\n"
				"		- Hvsp		ByapssEn = %d\n"
				"		- HvspSrc	HSize = %d, VSize = %d\n"
				"		- HvspDst	HSize = %d, VSize = %d\n"
				"		- Crop		HEn = %d, VEn = %d\n"
				"		- Crop		HSize = %d, VSize = %d\n"
				"		- Mux2VbSel	(S-Win) = %d (0x%04X)\n"
				"		- FrcWin En	(M-View) = %s, Id =%d\n"
				"		- Blending	Hsize = %d, VSize = %d\n"
				"		- Blending	HSt = %d, HEd = %d\n"
				"		- Blending	VSt = %d, VEd = %d\n"
				"		- LBW F2F3 path = %d"
				"\n		****  frc triggen ****\n"
				"		- TGEN		Out TimingX1000 = %llu\n"
				"		- TGEN		VTT = %d\n"
				"		- TGEN		VDeSt = %d\n"
				"		- MI_SW_HTT En = %d\n"
				"		- MI_SW_HTT Size = %d\n"
				"		- MI_VSync	line = %d, Vline = %llu\n"
				"\n		****  frc_rv55/r2_status ****\n"
				"		- RV55/R2	Ver = %X\n"
				"		- MFC		Level = %d\n"
				"		- MFC		frame num = %d, RWDF = %d\n"
				"		- MFC		Deblur = %d\n"
				"		- MFC		Dejudder = %d\n"
				"		- MFC		BW = %d\n"
				"		- MFC		SW Scene = %s (%d)\n"
				"		- MFC		Game = %d\n"
				"		- MFC		GameMemc = %d\n"
				"		- TGEN		FrameLock = %s\n"
				"		- TGEN		FrameLockRatio = %X\n"
				"		- MFC		In/Out Timing = %X\n"
				"		- MFC		In/Out Panel Timing = %X\n",
				stFrcDebugInfo.IpmAddr,
					(stFrcDebugInfo.IpmRstR == 0) ? "On" : "Off",
					(stFrcDebugInfo.IpmRstW == 0) ? "On" : "Off",
				stFrcDebugInfo.OpmAddr,
					(stFrcDebugInfo.OpmRstMe == 0) ? "On" : "Off",
					(stFrcDebugInfo.OpmRstMi == 0) ? "On" : "Off",
				stFrcDebugInfo.IopmWRLimitSt, stFrcDebugInfo.IopmWRLimitEd,
					stFrcDebugInfo.IopmWRLimitEn,
				stFrcDebugInfo.IpmMedsAddr,
					(stFrcDebugInfo.IpmMedsRstR == 0) ? "On" : "Off",
					(stFrcDebugInfo.IpmMedsRstW == 0) ? "On" : "Off",
				stFrcDebugInfo.OpmMedsAddr,
				stFrcDebugInfo.IopmMedsWRLimitSt, stFrcDebugInfo.IopmMedsWRLimitEd,
					stFrcDebugInfo.IopmMedsWRLimitEn,
				stFrcDebugInfo.IpmWriteIdx, stFrcDebugInfo.OpmF2ReadIdx,
				stFrcDebugInfo.IpmSrcHSize, stFrcDebugInfo.IpmSrcVSize,
				stFrcDebugInfo.HvspBypassEn,
				stFrcDebugInfo.HvspSrcHSize, stFrcDebugInfo.HvspSrcVSize,
				stFrcDebugInfo.HvspDstHSize, stFrcDebugInfo.HvspDstVSize,
				stFrcDebugInfo.CropHEn,	stFrcDebugInfo.CropVEn,
				stFrcDebugInfo.CropHSize, stFrcDebugInfo.CropVSize,
				stFrcDebugInfo.Mux2VbInSel, stFrcDebugInfo.BlendingTopMux,
				(stFrcDebugInfo.FrcWinId ? "Enable" : "NONE"),
					fls(stFrcDebugInfo.FrcWinId)-1,
				BlendingHsize, BlendingVsize,
				stFrcDebugInfo.BlendingHSt, stFrcDebugInfo.BlendingHEd,
				stFrcDebugInfo.BlendingVSt, stFrcDebugInfo.BlendingVEd,
				stFrcDebugInfo.LbwF2F3Path,
				TGENOutTimingX1000,
				stFrcDebugInfo.TgenVtt, stFrcDebugInfo.TgenVDeSt,
				stFrcDebugInfo.MISwHttEn, stFrcDebugInfo.MISwHttSize,
				stFrcDebugInfo.MIVsync, MIVsyncVline,
				stFrcDebugInfo.Rv55Ver,
				MFC_LEVL_MASK(stFrcDebugInfo.MfcLevel),
				MFC_FRAMENUM_MASK(stFrcDebugInfo.MfcFrame),
				MFC_RWDIFF_MASK(stFrcDebugInfo.MfcFrame),
				MFC_DEBLUR_MASK(stFrcDebugInfo.MfcUserLevel),
				MFC_DEJUDDER_MASK(stFrcDebugInfo.MfcUserLevel),
				MFC_BW_MASK(stFrcDebugInfo.MfcBWAndGameMode),
				SenceToString(pctx_frc->frc_scene), pctx_frc->frc_scene,
				MFC_GAME_MASK(stFrcDebugInfo.MfcBWAndGameMode),
				MFC_GAME_MEMC_MASK(stFrcDebugInfo.MfcBWAndGameMode),
				stFrcDebugInfo.TgenFrameLock ? "Lock" : "Not Lock",
				stFrcDebugInfo.TgenLockRatio,
				stFrcDebugInfo.MfcIOTiming,
				stFrcDebugInfo.MfcIOPanelTiming
	);

	#undef MFC_FRAMENUM_MASK
	#undef MFC_RWDIFF_MASK
	#undef MFC_LEVL_MASK
	#undef MFC_DEJUDDER_MASK
	#undef MFC_DEBLUR_MASK
	#undef MFC_BW_MASK
	#undef MFC_GAME_MASK
	#undef MFC_GAME_MEMC_MASK

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t frc_algo_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_video_display_frc_get_algo_info(dev, buf);
	return ret;
}

static uint16_t cmdval[RV55_CMD_LENGTH] = {0};

static ssize_t rv55_command_frc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	static const char _sep[] = " ";
	uint16_t TokenCnt;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_frc_context *pctx_frc = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_frc = &pctx->frc_priv;

	TokenCnt = 0;
	mtk_frc_get_all_value_from_string((char *)buf, (char *)_sep, count, cmdval, &TokenCnt);
	mtk_video_display_frc_set_rv55_cmd(cmdval, TokenCnt, pctx_frc);
	return count;
}

static ssize_t rv55_command_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	int len = 0;
	int i;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_frc = &pctx->frc_priv;

	#define _CHARNUM 4
	#define DEBUG_INFO_SHOW(title, size, elem) do { \
	len += snprintf(buf + len, PAGE_SIZE - len, "%s |\n", title); \
	len += snprintf(buf + len, PAGE_SIZE - len, "0:None 1:Mjc 2:MWin 3:SWin1 4:SWin2 |\n"); \
	for (i = 0; i < (size); i++) { \
		len += snprintf(buf + len, PAGE_SIZE - len, " %8llx |", (uint64_t)(elem)); \
		if ((i%_CHARNUM) == (_CHARNUM-1)) \
			len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} \
	len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} while (0)


	DEBUG_INFO_SHOW("RV55 Cmd", RV55_CMD_LENGTH, cmdval[i]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	#undef DEBUG_INFO_SHOW
	#undef _CHARNUM

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;

}

bool _mtk_frc_get_value_from_string(char *buf, char *name,
	unsigned int len, int *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		DRM_ERROR("Pointer is NULL!\n");
		return false;
	}

	*value = 0;
	cmd = buf;

	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0) {
			if (kstrtoint(tmp_value, 0, value)) {
				DRM_ERROR("kstrtoint fail!\n");
				return find;
			}
			find = true;
			DRM_ERROR("name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	DRM_ERROR("name(%s) was not found!\n", name);

	return find;
}

static ssize_t pattern_frc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int value = 0;
	int type = 0;
	bool find = false;
	bool pat_enable;
	int pat_type;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	find = _mtk_frc_get_value_from_string((char *)buf, "enable", count, &value);
	if (!find) {
		DRM_ERROR("Cmdline format error, enable should be set, please echo help!\n");
		return -EINVAL;
	}
	pat_enable = value;

	find = _mtk_frc_get_value_from_string((char *)buf, "type", count, &type);
	if (!find) {
		DRM_ERROR("Cmdline format error, type should be set, please echo help!\n");
		return -EINVAL;
	}
	pat_type = type;


	mtk_video_display_frc_set_pattern(pctx, pat_type, pat_enable);
	return count;
}

static ssize_t isany_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	int len = 0;
	uint32_t i = 0;
	int memc_winId = 0;
	bool bIsAnyFrc = false;
	bool bIsAnyFrcFreeze = false;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;

	for (i = 0 ; i < MAX_WINDOW_NUM ; i++) {
		if (pctx_video->video_plane_type[i] == MTK_VPLANE_TYPE_MEMC) {
			memc_winId = i;
			bIsAnyFrc = true;
			bIsAnyFrcFreeze = (pctx_video->plane_ctx + i)->memc_freeze;
		}
	}

	if (bIsAnyFrc) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"[IsAnyFrc]There is a FRC win Id = %d.\n", memc_winId);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"[IsAnyFrc]Current FRC freeze status = %d.\n\n", bIsAnyFrcFreeze);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"[IsAnyFrc]There is no FRC window currently.\n\n");
	}

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t rwdiff_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	pctx_frc = &pctx->frc_priv;

	ret = snprintf(buf, PAGE_SIZE, "\n[frc rwdiff]=%d\n", pctx_frc->frc_rwdiff);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t rwdiff_frc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	pctx_frc = &pctx->frc_priv;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx_frc->frc_user_rwdiff = val;
	return count;
}


static ssize_t path_frc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	int len = 0;
	int i;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;

	#define _CHARNUM 4
	#define DEBUG_INFO_SHOW(title, size, elem) do { \
	len += snprintf(buf + len, PAGE_SIZE - len, "%s |\n", title); \
	len += snprintf(buf + len, PAGE_SIZE - len, "0:None 1:Mjc 2:MWin 3:SWin1 4:SWin2 |\n"); \
	for (i = 0; i < (size); i++) { \
		len += snprintf(buf + len, PAGE_SIZE - len, " %8llx |", (uint64_t)(elem)); \
		if ((i%_CHARNUM) == (_CHARNUM-1)) \
			len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} \
	len += snprintf(buf + len, PAGE_SIZE - len, "\n"); \
	} while (0)


	DEBUG_INFO_SHOW("frc path", MAX_WINDOW_NUM, pctx_video->video_plane_type[i]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	#undef DEBUG_INFO_SHOW
	#undef _CHARNUM

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t phase_diff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t get_phase_diff_data = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (ret) {
		DRM_ERROR("[%s, %d]: set vttv phase diff error\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_tv_drm_get_phase_diff_us(&get_phase_diff_data);
	ret = snprintf(buf, PAGE_SIZE, "\n Phase Diff Val = %u\n",
		get_phase_diff_data);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t force_freerun_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "\nforce free run = %d\n",
		pctx_pnl->cus.force_free_run_en);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t force_freerun_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx_pnl->cus.force_free_run_en = val&1;
	_mtk_video_set_framesync_mode(pctx);

	return count;
}

static ssize_t panel_debug_log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "\n panel_debug_log = %llx\n",
		pctx_pnl->gu64panel_debug_log);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_debug_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx_pnl->gu64panel_debug_log = val;

	return count;
}

static ssize_t panel_tg_ML_SupportValue_setting_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	#define TRIGGERGEN_CRYSTAL 24000000
	#define ML_PERFORMANCEx1000_MEARK1 43
	#define ML_PERFORMANCEx1000_MEARK2 66
	#define DEBUG_SYSFS_ML 0
	uint16_t au16TgMLSetting[TRIGGER_GEN_DOMAINSEL_MAX];
	uint16_t au16TgVSSetting[TRIGGER_GEN_DOMAINSEL_MAX];
	uint16_t au16TgSWHttSetting[TRIGGER_GEN_DOMAINSEL_MAX];
	uint32_t au32TgMLHaveUsx1000[TRIGGER_GEN_DOMAINSEL_MAX];
	uint32_t au32TgMLSupportValue[TRIGGER_GEN_DOMAINSEL_MAX];
	char chDoaminName[TRIGGER_GEN_DOMAINSEL_MAX][20] = {
			{"SRC0"}, {"SRC1"}, {"B2R"},
			{"IP"}, {"OP1"}, {"OP2"},
			{"DISP"}, {"FCC1"}, {"FRC2"},
			{"TGEN"}, {"B2RLINE1"}, {"B2RLINE2"},
			{"FRC INPUT"}, {"FRC STAGE"}, {"FRC ME"},
			{"FRC MI"}
		};
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl;
	enum en_trigger_gen_doaminsel enCount
		= TRIGGER_GEN_DOMAINSEL_SRC0;
	uint16_t u16ML_Perforancex1000 = 1;
	mtktv_chip_series series;
	int len = 0;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;
	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	if (series == MTK_TV_SERIES_MERAK_1)
		u16ML_Perforancex1000 = ML_PERFORMANCEx1000_MEARK1;
	else
		u16ML_Perforancex1000 = ML_PERFORMANCEx1000_MEARK2;

	for (enCount = TRIGGER_GEN_DOMAINSEL_SRC0;
		enCount < TRIGGER_GEN_DOMAINSEL_MAX; enCount++) {

		ret = drv_hwreg_render_common_get_tg_setting(
			enCount,
			&au16TgMLSetting[enCount],
			&au16TgVSSetting[enCount],
			&au16TgSWHttSetting[enCount]);

		if (au16TgMLSetting[enCount] <= au16TgVSSetting[enCount]) {
			au32TgMLHaveUsx1000[enCount] =
				(uint32_t)(DEC_1000000*DEC_1000
				*(uint64_t)(au16TgVSSetting[enCount]
				- au16TgMLSetting[enCount])
				*au16TgSWHttSetting[enCount]/TRIGGERGEN_CRYSTAL);
			au32TgMLSupportValue[enCount] =
				au32TgMLHaveUsx1000[enCount]
				/u16ML_Perforancex1000;
		} else {
			au32TgMLHaveUsx1000[enCount] = 0;
			au32TgMLSupportValue[enCount] = 0;
		}

		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"  [%s]\n",
				chDoaminName[enCount]);
		#if DEBUG_SYSFS_ML
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"ML=%X  ",
				au16TgMLSetting[enCount]);
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"VS=%X  ",
				au16TgVSSetting[enCount]);
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"SW HTT=%X  ",
				au16TgSWHttSetting[enCount]);
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"u16ML_Perforancex1000=%X\n",
				u16ML_Perforancex1000);
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"MLHaveUsx1000=  %04d\n",
				au32TgMLHaveUsx1000[enCount]);
		#endif
		len += snprintf(buf + len,
				PAGE_SIZE - len,
				"Support_ML_Value=  %04d\n",
				au32TgMLSupportValue[enCount]);
	}

	#undef DEBUG_SYSFS_ML
	#undef ML_PERFORMANCEx1000_MEARK2
	#undef ML_PERFORMANCEx1000_MEARK1
	#undef TRIGGERGEN_CRYSTAL

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t panel_tg_debug_setting_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "\n panel_tg_debug_setting = %llx\n",
		pctx_pnl->gu64panel_debug_log);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_tg_debug_setting_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	unsigned long val;
	int err;
	mtktv_chip_series series;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;
	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	if (series == MTK_TV_SERIES_MERAK_1) {
		err = kstrtoul(buf, 0, &val);
		if (err)
			return err;
		pctx_pnl->gu8Triggergendebugsetting = val;
	} else if (series == MTK_TV_SERIES_MERAK_2) {
		err = mtk_trigger_gen_set_param(pctx, buf, count);
		if (err)
			return err;
	} else {
		pr_err("invalid chip series\n");
		return -EINVAL;
	}

	mtk_tv_drm_set_trigger_gen_debug(pctx);

	return count;
}

static ssize_t Disable_modeID_change_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf, PAGE_SIZE, "\nDisable_modeID_change = %d\n",
		pctx_pnl->disable_ModeID_change);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t Disable_modeID_change_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx_pnl->disable_ModeID_change = val&1;
	return count;
}

static ssize_t Tgen_vtt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	ret = snprintf(buf,
			PAGE_SIZE,
			"\n[%s %d]Tgen_vtt = %u\n",
			__func__,
			__LINE__,
			pctx_pnl->Tgen_vtt);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t  Tgen_vtt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_KMS_REG_NUM_TGEN_VTT];
	struct hwregOut paramOut;
	unsigned long val;
	int err;
	uint16_t outputVTT;// changed volume to VTT

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_pnl = &pctx->panel_priv;

	//paramOut.reg = reg + RegCount;
	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pctx_pnl->Tgen_vtt = val;
	outputVTT = pctx_pnl->Tgen_vtt;
	//h23[15:0] = output VTT
	DRM_INFO("[%s, %d] Set VTT HWREG\n", __func__, __LINE__);
	DRM_INFO("OutputVTT:[%d]\n", outputVTT);
	drv_hwreg_render_pnl_set_vtt(
			&paramOut,
			true,
			outputVTT);
	return count;
}

static ssize_t support_render_qmap_version_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	count += snprintf(buf + count, u8MaxSize - count,
		"Qmap_Version=%d\n", pctx->ld_priv.u8RENDER_QMAP_Version);

	return ((count < 0) || (count > u8MaxSize)) ? -EINVAL : count;
}

static ssize_t support_render_ldm_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;
	__u8 u8LDMSupport;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	if (pctx->ld_priv.u8LDMSupport > 0)
		u8LDMSupport = 1;
	else
		u8LDMSupport = 0;

	count = snprintf(buf, u8MaxSize,
		"%d\n", u8LDMSupport);
	return ((count < 0) || (count > u8MaxSize)) ? -EINVAL : count;
}

static ssize_t support_render_ldm_global_dimming_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;
	__u16 u16LEDWidth = 0;
	__u16 u16LEDHeight = 0;
	__u8 u8LD_Type = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	u16LEDWidth = pctx->ld_priv.ld_panel_info.u16LEDWidth;
	u16LEDHeight = pctx->ld_priv.ld_panel_info.u16LEDHeight;
	if ((u16LEDWidth == 1) && (u16LEDHeight == 1))
		u8LD_Type = 1;
	else
		u8LD_Type = 0;

	count = snprintf(buf, u8MaxSize, "%d\n", u8LD_Type);

	return ((count < 0) || (count > u8MaxSize)) ? -EINVAL : count;
}

static ssize_t support_render_ldm_vrr_support_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	count = snprintf(buf, u8MaxSize,
		"%d\n", pctx->ld_priv.ld_led_device_info.bLedDevice_VRR_Support);

	return ((count < 0) || (count > u8MaxSize)) ? -EINVAL : count;
}

static ssize_t support_render_ldm_support_dv_gd_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int count = 0;
	__u8 u8MaxSize = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	count = snprintf(buf, u8MaxSize,
		"%d\n", pctx->ld_priv.ld_panel_info.bSupport_DV_GD);

	return ((count < 0) || (count > u8MaxSize)) ? -EINVAL : count;
}

static ssize_t bw_efficiency_rate_mgw_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* hard code retuen 1 first*/
	/* need calc real ER_MGW by difffernt DDR type*/
	return snprintf(buf, PAGE_SIZE, "%u\n", 1);
}

static ssize_t bw_efficiency_rate_frcop_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* hard code retuen 1 first*/
	/* need calc real ER_FRCOP by difffernt DDR type*/
	return snprintf(buf, PAGE_SIZE, "%u\n", 1);
}

static ssize_t dither_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_st_dither_reg stdrmDITHERReg = {0};
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mtk_render_get_panel_dither_reg(&stdrmDITHERReg);

	ret = snprintf(buf, PAGE_SIZE, "dither register(Bank 0xA324 16bit addr):\n"
				"		- h'3F[0]  dither_frc_on        = %d\n"
				"		- h'3F[3]  dither_noise_disable = %d\n"
				"		- h'3F[4]  dither_tail_cut      = %d\n"
				"		- h'40[8]  dither_box_freeze    = %d\n"
				"		- h'3F[2]  dither_dith_bit      = %d\n"
				"		- h'3F[11] dither_12bit_bypass  = %d\n"
				"		- h'3F[15] dither_is_12bit      = %d\n",
				stdrmDITHERReg.dither_frc_on,
				stdrmDITHERReg.dither_noise_disable,
				stdrmDITHERReg.dither_tail_cut,
				stdrmDITHERReg.dither_box_freeze,
				stdrmDITHERReg.dither_dith_bit,
				stdrmDITHERReg.dither_12bit_bypass,
				stdrmDITHERReg.dither_is_12bit);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t set_rw_diff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t i = 0, len = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "rw_diff |");
	for (i = 0; i < MAX_WINDOW_NUM; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, " %3d |",
			gMtkDrmDbg.set_rw_diff->rw_diff[i]);

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t set_rw_diff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *video = NULL;
	uint64_t rw_diff = 0, windowID = 0;
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	video = &pctx->video_priv;

	ret = mtk_render_common_parser_cmd_int(buf, "window", &windowID);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	ret = mtk_render_common_parser_cmd_int(buf, "rw_diff", &rw_diff);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	if (((video->video_plane_properties + windowID)->
		prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE]) != MTK_VPLANE_TYPE_NONE) {
		gMtkDrmDbg.set_rw_diff->win_en[windowID] = true;
		gMtkDrmDbg.set_rw_diff->rw_diff[windowID] =
			(uint8_t)rw_diff;
	} else
		DRM_ERROR("[%s] the windowID: %llu is not exist.\n", __func__, windowID);

EXIT:
	return count;
}

static ssize_t update_disp_size_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *video = NULL;
	struct window updateInfo;
	uint64_t windowID = 0;
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	video = &pctx->video_priv;

	ret = mtk_render_common_parser_cmd_int(buf, "window", &windowID);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	ret = mtk_render_common_parser_cmd_int(buf, "x", &updateInfo.x);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "y", &updateInfo.y);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "width", &updateInfo.w);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}
	ret = mtk_render_common_parser_cmd_int(buf, "height", &updateInfo.h);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	if (((video->video_plane_properties + windowID)->
		prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE]) != MTK_VPLANE_TYPE_NONE) {
		gMtkDrmDbg.update_disp_size->win_en[windowID] = true;
		gMtkDrmDbg.update_disp_size->dispwindow[windowID].x =
			updateInfo.x;
		gMtkDrmDbg.update_disp_size->dispwindow[windowID].y =
			updateInfo.y;
		gMtkDrmDbg.update_disp_size->dispwindow[windowID].w =
			updateInfo.w;
		gMtkDrmDbg.update_disp_size->dispwindow[windowID].h =
			updateInfo.h;
	} else
		DRM_ERROR("[%s] the windowID: %llu is not exist.\n", __func__, windowID);

EXIT:
	return count;
}

static ssize_t change_vPlane_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *video = NULL;
	uint64_t video_plane_type = 0, windowID = 0;
	int ret;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	video = &pctx->video_priv;

	ret = mtk_render_common_parser_cmd_int(buf, "window", &windowID);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	ret = mtk_render_common_parser_cmd_int(buf, "video_plane_type", &video_plane_type);
	if (ret) {
		DRM_ERROR("[%s, %d]: fail parser red, ret=%d\n", __func__, __LINE__, ret);
		goto EXIT;
	}

	if (((video->video_plane_properties + windowID)->
		prop_val[EXT_VPLANE_PROP_VIDEO_PLANE_TYPE]) != MTK_VPLANE_TYPE_NONE) {
		gMtkDrmDbg.change_vPlane_type->win_en[windowID] = true;
		gMtkDrmDbg.change_vPlane_type->vPlane_type[windowID] =
			(uint8_t)video_plane_type;
	} else
		DRM_ERROR("[%s] the windowID: %llu is not exist.\n", __func__, windowID);

EXIT:
	return count;
}

static ssize_t tcon_status_show(
				struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	char *pchInfo = NULL;
	int len;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	// allocate info buffer
	pchInfo = kvmalloc(sizeof(char)*TCON_INFO_LEN, GFP_KERNEL);
	if (!pchInfo)
		return -ENOMEM;

	memset(pchInfo, '\0', sizeof(char)*TCON_INFO_LEN);

	pctx = dev_get_drvdata(dev);

	if (pctx)
		mtk_tcon_show_info(&pctx->panel_priv, pchInfo, TCON_INFO_LEN);
	else
		DRM_ERROR("[%s, %d]: pctx is NULL!\n", __func__, __LINE__);

	len = sysfs_emit(buf, "%s\n", pchInfo);
	kvfree(pchInfo);

	return len;
}

static ssize_t tcon_mode_store(
				struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	struct mtk_tv_kms_context *pctx;
	unsigned long val;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Pointer pctx is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err) {
		pr_crit("Tcon mode store failed!\n");
		pr_crit("Please set tcon mode within 0 ~ %d\n", EN_TCON_MODE_MAX-1);
		return err;
	}

	if (val >= EN_TCON_MODE_MAX)
		pr_crit("Please set tcon mode within 0 ~ %d\n", EN_TCON_MODE_MAX-1);
	else {
		set_tcon_mode((EN_TCON_MODE)val);
		pctx->panel_priv.out_upd_type = E_MODE_RESET_TYPE;

		/* power off */
		mtk_tcon_enable(&(pctx->panel_priv), false);

		/* re-do tcon init */
		mtk_tcon_init(&(pctx->panel_priv));

		/* power on */
		mtk_tcon_enable(&(pctx->panel_priv), true);
		pctx->panel_priv.out_upd_type = E_NO_UPDATE_TYPE;

		pr_crit("Tcon mode has been set to %lu\n", val);
	}

	return count;
}

static ssize_t tcon_debug_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int value = 0;
	int type = 0;
	bool find = false;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	/* parser value */
	find = _mtk_frc_get_value_from_string((char *)buf, "value", count, &value);
	if (!find) {
		DRM_ERROR("Cmdline format error, value should be set, please echo help!\n");
		return -EINVAL;
	}

	/* parser type */
	find = _mtk_frc_get_value_from_string((char *)buf, "type", count, &type);
	if (!find) {
		DRM_ERROR("Cmdline format error, type should be set, please echo help!\n");
		return -EINVAL;
	}

	if (pctx)
		mtk_tcon_set_cmd(&pctx->panel_priv, type, (uint16_t)value);
	else {
		DRM_ERROR("[%s, %d]: pctx is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	return count;
}

static ssize_t swing_level_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	int len = 0;
	uint16_t SwingIndex = 0;
	struct drm_st_out_swing_level stSwing;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	mtk_render_get_swing_vreg(&stSwing, pctx_panel->cus.tcon_en);
	for (SwingIndex = 0; SwingIndex < pctx_panel->out_lane_info.ctrl_lanes; SwingIndex++)
		len += snprintf(buf + len,
		PAGE_SIZE - len,
		"	Lane %2d swing_level = %x\n",
		SwingIndex,
		stSwing.swing_level[SwingIndex]);

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t swing_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	unsigned long val;
	int err;
	struct drm_st_out_swing_level stSwing;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	stSwing.usr_swing_level = 1;
	stSwing.common_swing = 1;
	stSwing.ctrl_lanes = pctx_panel->out_lane_info.ctrl_lanes;

	err = kstrtoul(buf, 0, &val);
	if (err) {
		pr_crit("swing_level store failed!\n");
		pr_crit("please set swing within 0xf(0x0~0xf)\n");
		return err;
	}

	if (val > 15)
		pr_crit("please set swing within 0xf(0x0~0xf)\n");
	else {
		stSwing.swing_level[0] = val;
		mtk_render_set_swing_vreg(&stSwing, pctx_panel->cus.tcon_en);
		pr_crit("swing_level has been set to %lx\n", val);
	}

	return count;
}

static ssize_t pe_level_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	int len = 0;
	uint16_t PEIndex = 0;
	struct drm_st_out_pe_level stPE;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	mtk_render_get_pre_emphasis(&stPE);
	for (PEIndex = 0; PEIndex < pctx_panel->out_lane_info.ctrl_lanes; PEIndex++)
		len += snprintf(buf + len,
			PAGE_SIZE - len,
			"	Lane %2d PE_level = %x\n",
			PEIndex,
			stPE.pe_level[PEIndex]);

	return ((len < 0) || (len > PAGE_SIZE)) ? -EINVAL : len;
}

static ssize_t pe_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	unsigned long val;
	int err;

	struct drm_st_out_pe_level stPE;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;

	stPE.usr_pe_level = 1;
	stPE.common_pe = 1;
	stPE.ctrl_lanes = pctx_panel->out_lane_info.ctrl_lanes;

	err = kstrtoul(buf, 0, &val);
	if (err) {
		pr_crit("PE_level_store failed!\n");
		pr_crit("please set PE_level within 0x1f(0x0~0x1f)\n");
		return err;
	}

	if (val > 31)
		pr_crit("please set PE_level within 0xf(0x0~0x1f)\n");
	else {
		stPE.pe_level[0] = val;
		mtk_render_set_pre_emphasis(&stPE);
		pr_crit("PE_level is set to %lx\n", val);
	}

	return count;
}

static ssize_t ssc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct drm_st_out_ssc stSSC;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mtk_render_get_ssc(&stSSC);

	ret = snprintf(buf, PAGE_SIZE, "SSC status:\n"
			"	ssc_on         = %d\n"
			"	ssc_modulation = %d\n"
			"	ssc_deviation  = %d\n",
			stSSC.ssc_en,
			stSSC.ssc_modulation,
			stSSC.ssc_deviation);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ssc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_panel_context *pctx_panel = NULL;
	int err;
	int err1;
	unsigned long val_e, val_m, val_d;
	char *enable_s, *modulation_s, *deviation_s, *buf1;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	pctx_panel = &pctx->panel_priv;
	buf1 = (char *)buf;

	enable_s = strsep(&buf1, " =\n\r");
	err = kstrtoul(enable_s, 0, &val_e);
	if (err) {
		pr_crit("Cmdline format error,SSC_store failed!\n");
		return err;
	}

	switch (val_e) {
	case 0:
		pctx_panel->stdrmSSC.ssc_setting.ssc_en = val_e;
		mtk_render_set_ssc(pctx_panel);
		pr_crit("SSC has been set up!\n");
	break;
	case 1:
		pctx_panel->stdrmSSC.ssc_setting.ssc_en = val_e;
		modulation_s = strsep(&buf1, " =\n\r");
		err = kstrtoul(modulation_s, 0, &val_m);
		deviation_s = strsep(&buf1, " =\n\r");
		err1 = kstrtoul(deviation_s, 0, &val_d);
		if (err || err1) {
			pr_crit("Cmdline format error,SSC_store failed!\n");
			if (err)
				return err;
			else
				return err1;
		} else {
			if (val_m > 1000)
				pr_crit("please set SSC_emodulation within 1000Khz");
			else if (val_d > 5000)
				pr_crit("please set SSC_deviation within 5000");
			else if ((val_m <= 1000) &&  (val_d <= 5000)) {
				// modulation: 1Khz for property, 0.1Khz in render DD
				pctx_panel->stdrmSSC.ssc_setting.ssc_modulation = val_m * 10;
				// deviation: 0.01% for property, 0.01% in render DD
				pctx_panel->stdrmSSC.ssc_setting.ssc_deviation = val_d;
				mtk_render_set_ssc(pctx_panel);
				pr_crit("SSC has been set up!\n");
			}
		}
	break;
	default:
		pr_crit("please set SSC_enable 0 or 1\n");
	break;
	}

	return count;
}

static ssize_t panel_CRC_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	uint16_t CRC_val = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	CRC_val = drv_hwreg_render_pnl_Get_video1_CRC_value();

	ret = snprintf(buf, PAGE_SIZE, "\n panel_CRC = 0x%x\n",
		CRC_val);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t pqgamma_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_enable = val;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t pqgamma_pregainoffset_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_gainoffset_enbale = val;
	pqu_gamma_data->pqgamma.pqgamma_pregainoffset = TRUE;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_gainoffset_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_gainoffset_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t pqgamma_postgainoffset_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_gainoffset_enbale = val;
	pqu_gamma_data->pqgamma.pqgamma_pregainoffset = FALSE;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_gainoffset_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_gainoffset_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t pqgamma_maxvalue_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pqgamma.pqgamma_maxvalue_enable = val;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pqgamma_maxvalue_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pqgamma_maxvalue_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t pnlgamma_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pnlgamma.pnlgamma_enable = val;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pnlgamma_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pnlgamma_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t pnlgamma_gainoffset_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data = NULL;
	unsigned long val;
	int ret;
	int err;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	DRM_INFO("[%s][%d]: cmd: '%s'", __func__, __LINE__, buf);

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pnlgamma.pnlgamma_gainoffset_enbale = val;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pnlgamma_gainoffset_enable = TRUE;
	updateInfo.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pnlgamma_gainoffset_enable = TRUE;
	pqupdate.pnlgamma_stub_mode = pctx->pnlgamma_stubmode;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	return count;
}

static ssize_t tg_param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	return mtk_trigger_gen_show_param(buf);
}

static ssize_t ambilight_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (mtk_tv_drm_ambilight_debug(buf, dev) < 0) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_ambilight_debug failed!\n",
			__func__, __LINE__);
	}

	return count;
}


static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(log_level);
static DEVICE_ATTR_RW(pattern_frc);
static DEVICE_ATTR_RW(rwdiff_frc);
static DEVICE_ATTR_RO(frc_status);
static DEVICE_ATTR_RW(rv55_command_frc);
static DEVICE_ATTR_RW(memc_level_frc);
static DEVICE_ATTR_RW(panel_debug_log);
static DEVICE_ATTR_RW(panel_tg_debug_setting);
static DEVICE_ATTR_RO(path_frc);
static DEVICE_ATTR_RO(isany_frc);
static DEVICE_ATTR_RO(panel_v_flip);
static DEVICE_ATTR_RO(panel_h_flip);
static DEVICE_ATTR_RO(vrr_max_v);
static DEVICE_ATTR_RO(vrr_min_v);
static DEVICE_ATTR_RO(vrr_tolerance);
static DEVICE_ATTR_RO(panel_oled_status);
static DEVICE_ATTR_RO(panel_oled_support);
static DEVICE_ATTR_RO(mtk_drm_capability_OLED_PIXELSHIFT_SUPPORT);
static DEVICE_ATTR_RO(mtk_drm_OLED_PIXELSHIFT_H_RANGE_MAX);
static DEVICE_ATTR_RO(mtk_drm_OLED_PIXELSHIFT_H_RANGE_MIN);
static DEVICE_ATTR_RO(mtk_drm_OLED_PIXELSHIFT_V_RANGE_MAX);
static DEVICE_ATTR_RO(mtk_drm_OLED_PIXELSHIFT_V_RANGE_MIN);
static DEVICE_ATTR_RW(oled_pixelshift);
static DEVICE_ATTR_RO(panel_mod_status);
static DEVICE_ATTR_RO(panel_out_model);
static DEVICE_ATTR_RO(framesync_version);
static DEVICE_ATTR_RO(frame_sync_status);
static DEVICE_ATTR_RO(panel_mode_ID_Sel);
static DEVICE_ATTR_RO(panel_mode_ID_deltav);
static DEVICE_ATTR_RO(panel_Backlight_on);
static DEVICE_ATTR_RO(panel_Backlight_off);
static DEVICE_ATTR_RO(output_frame_rate);
static DEVICE_ATTR_RW(pixel_mute_rgb);
static DEVICE_ATTR_RW(mod_pattern_set);
static DEVICE_ATTR_RO(main_video_pixel_mute);
static DEVICE_ATTR_RO(main_video_pixel_unmute);
static DEVICE_ATTR_RW(panel_reg_dump);
static DEVICE_ATTR_RO(panel_TgenVTT_CheckIvsOvs);
static DEVICE_ATTR_RW(force_freerun);
static DEVICE_ATTR_RO(panel_tg_ML_SupportValue_setting);
static DEVICE_ATTR_RW(Disable_modeID_change);
static DEVICE_ATTR_RW(Tgen_vtt);
static DEVICE_ATTR_RO(dither_reg);
static DEVICE_ATTR_RO(tcon_status);
static DEVICE_ATTR_WO(tcon_mode);
static DEVICE_ATTR_WO(tcon_debug_cmd);
static DEVICE_ATTR_RW(swing_level);
static DEVICE_ATTR_RW(pe_level);
static DEVICE_ATTR_RW(ssc);
static DEVICE_ATTR_RO(support_render_qmap_version);
static DEVICE_ATTR_RO(support_render_ldm_support);
static DEVICE_ATTR_RO(support_render_ldm_global_dimming_support);
static DEVICE_ATTR_RO(support_render_ldm_vrr_support);
static DEVICE_ATTR_RO(support_render_ldm_support_dv_gd);
static DEVICE_ATTR_RW(atrace_enable);
static DEVICE_ATTR_WO(change_vPlane_type);
static DEVICE_ATTR_WO(update_disp_size);
static DEVICE_ATTR_RW(set_rw_diff);
static DEVICE_ATTR_RO(panel_CRC);
static DEVICE_ATTR_WO(pqgamma_enable);
static DEVICE_ATTR_WO(pqgamma_pregainoffset_enable);
static DEVICE_ATTR_WO(pqgamma_postgainoffset_enable);
static DEVICE_ATTR_WO(pqgamma_maxvalue_enable);
static DEVICE_ATTR_WO(pnlgamma_enable);
static DEVICE_ATTR_WO(pnlgamma_gainoffset_enable);
static DEVICE_ATTR_RO(bw_efficiency_rate_mgw);
static DEVICE_ATTR_RO(bw_efficiency_rate_frcop);
static DEVICE_ATTR_RO(tg_param);
static DEVICE_ATTR_RO(phase_diff);
static DEVICE_ATTR_RO(frc_algo_status);
static DEVICE_ATTR_WO(ambilight);
static DEVICE_ATTR_RW(mtk_tv_drm_backlight_sysfs);

static struct attribute *mtk_tv_drm_kms_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_log_level.attr,
	&dev_attr_pattern_frc.attr,
	&dev_attr_rwdiff_frc.attr,
	&dev_attr_frc_status.attr,
	&dev_attr_rv55_command_frc.attr,
	&dev_attr_memc_level_frc.attr,
	&dev_attr_path_frc.attr,
	&dev_attr_isany_frc.attr,
	&dev_attr_framesync_version.attr,
	&dev_attr_panel_oled_status.attr,
	&dev_attr_panel_oled_support.attr,
	&dev_attr_mtk_drm_capability_OLED_PIXELSHIFT_SUPPORT.attr,
	&dev_attr_mtk_drm_OLED_PIXELSHIFT_H_RANGE_MAX.attr,
	&dev_attr_mtk_drm_OLED_PIXELSHIFT_H_RANGE_MIN.attr,
	&dev_attr_mtk_drm_OLED_PIXELSHIFT_V_RANGE_MAX.attr,
	&dev_attr_mtk_drm_OLED_PIXELSHIFT_V_RANGE_MIN.attr,
	&dev_attr_oled_pixelshift.attr,
	&dev_attr_panel_mod_status.attr,
	&dev_attr_panel_out_model.attr,
	&dev_attr_frame_sync_status.attr,
	&dev_attr_panel_mode_ID_Sel.attr,
	&dev_attr_panel_mode_ID_deltav.attr,
	&dev_attr_output_frame_rate.attr,
	&dev_attr_panel_Backlight_on.attr,
	&dev_attr_panel_Backlight_off.attr,
	&dev_attr_pixel_mute_rgb.attr,
	&dev_attr_mod_pattern_set.attr,
	&dev_attr_main_video_pixel_mute.attr,
	&dev_attr_main_video_pixel_unmute.attr,
	&dev_attr_panel_TgenVTT_CheckIvsOvs.attr,
	&dev_attr_panel_reg_dump.attr,
	&dev_attr_force_freerun.attr,
	&dev_attr_panel_tg_ML_SupportValue_setting.attr,
	&dev_attr_Disable_modeID_change.attr,
	&dev_attr_Tgen_vtt.attr,
	&dev_attr_dither_reg.attr,
	&dev_attr_tcon_status.attr,
	&dev_attr_tcon_mode.attr,
	&dev_attr_tcon_debug_cmd.attr,
	&dev_attr_swing_level.attr,
	&dev_attr_pe_level.attr,
	&dev_attr_ssc.attr,
	&dev_attr_panel_debug_log.attr,
	&dev_attr_panel_tg_debug_setting.attr,
	&dev_attr_atrace_enable.attr,
	&dev_attr_change_vPlane_type.attr,
	&dev_attr_update_disp_size.attr,
	&dev_attr_set_rw_diff.attr,
	&dev_attr_panel_CRC.attr,
	&dev_attr_pqgamma_enable.attr,
	&dev_attr_pqgamma_pregainoffset_enable.attr,
	&dev_attr_pqgamma_postgainoffset_enable.attr,
	&dev_attr_pqgamma_maxvalue_enable.attr,
	&dev_attr_pnlgamma_enable.attr,
	&dev_attr_pnlgamma_gainoffset_enable.attr,
	&dev_attr_tg_param.attr,
	&dev_attr_phase_diff.attr,
	&dev_attr_frc_algo_status.attr,
	&dev_attr_ambilight.attr,
	NULL,
};

static struct attribute *mtk_tv_drm_kms_attrs_panel[] = {
	&dev_attr_panel_v_flip.attr,
	&dev_attr_panel_h_flip.attr,
	&dev_attr_vrr_max_v.attr,
	&dev_attr_vrr_min_v.attr,
	&dev_attr_vrr_tolerance.attr,
	&dev_attr_mtk_tv_drm_backlight_sysfs.attr,
	NULL
};

static struct attribute *mtk_tv_drm_kms_attrs_capability[] = {
	&dev_attr_support_render_qmap_version.attr,
	&dev_attr_support_render_ldm_support.attr,
	&dev_attr_support_render_ldm_global_dimming_support.attr,
	&dev_attr_support_render_ldm_vrr_support.attr,
	&dev_attr_support_render_ldm_support_dv_gd.attr,
	&dev_attr_bw_efficiency_rate_mgw.attr,
	&dev_attr_bw_efficiency_rate_frcop.attr,
	NULL
};

static const struct attribute_group mtk_tv_drm_kms_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_kms_attrs
};

static const struct attribute_group mtk_tv_drm_kms_attr_panel_group = {
	.name = "panel",
	.attrs = mtk_tv_drm_kms_attrs_panel
};

static const struct attribute_group mtk_tv_drm_kms_attr_capability_group = {
	.name = "capability",
	.attrs = mtk_tv_drm_kms_attrs_capability
};

static void mtk_tv_kms_enable(
	struct mtk_tv_drm_crtc *mtk_tv_crtc)
{
}

static void mtk_tv_kms_disable(
	struct mtk_tv_drm_crtc *mtk_tv_crtc)
{
}

static int mtk_tv_kms_enable_vblank(
	struct mtk_tv_drm_crtc *mtk_tv_crtc)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	int ret = 0;

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_video = &pctx->video_priv;

	if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		ret = mtk_gop_enable_vblank(mtk_tv_crtc);
	} else if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		ret = mtk_common_IRQ_set_unmask(
			mtk_tv_crtc, MTK_VIDEO_IRQTYPE_HK,
			MTK_VIDEO_SW_IRQ_TRIG_DISP, pctx_video->video_hw_ver.irq);
	} else if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_EXT_VIDEO) {
		//ret = mtk_common_IRQ_set_unmask(
		//	crtc, MTK_VIDEO_IRQTYPE_HK, MTK_VIDEO_SW_IRQ_TRIG_DISP);
	} else {
		DRM_ERROR("[%s, %d]: Not support crtc type\n",
			__func__, __LINE__);
	}

	return ret;
}

static int mtk_tv_kms_gamma_set(
	struct drm_crtc *crtc,
	uint16_t *r,
	uint16_t *g,
	uint16_t *b,
	uint32_t size,
	struct drm_modeset_acquire_ctx *ctx)
{
	int ret = 0;

	ret = mtk_video_panel_gamma_setting(crtc, r, g, b, size);

	return ret;
}

static int mtk_tv_kms_gamma_enable(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pnlgamma_enable *pnlgamma_enable)
{
	int ret = 0;

	ret = mtk_video_panel_gamma_enable(crtc, pnlgamma_enable);

	return ret;
}

static int mtk_tv_kms_gamma_gainoffset(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pnlgamma_gainoffset *pnl_gainoffset)
{
	int ret = 0;

	ret = mtk_video_panel_gamma_gainoffset(crtc, pnl_gainoffset);

	return ret;
}

static int mtk_tv_kms_gamma_gainoffset_enable(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pnlgamma_gainoffset_enable *pnl_gainoffsetenable)
{
	int ret = 0;

	ret = mtk_video_panel_gamma_gainoffset_enable(crtc, pnl_gainoffsetenable);

	return ret;
}

static int mtk_tv_pqgamma_set(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_curve *curve)
{
	int ret = 0;

	if (curve->size != MTK_PQ_GAMMA_LUT_SIZE)
		return -EINVAL;
	ret = mtk_video_pqgamma_set(crtc, curve);

	return ret;
}

static int mtk_tv_pqgamma_enable(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_enable *pqgamma_enable)
{
	int ret = 0;

	ret = mtk_video_pqgamma_enable(crtc, pqgamma_enable);

	return ret;
}

static int mtk_tv_pqgamma_gainoffset(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_gainoffset *pq_gainoffset)
{
	int ret = 0;
	ret = mtk_video_pqgamma_gainoffset(crtc, pq_gainoffset);

	return ret;
}

static int mtk_tv_pqgamma_gainoffset_enable(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_gainoffset_enable *pq_gainoffsetenable)
{
	int ret = 0;

	ret = mtk_video_pqgamma_gainoffset_enable(crtc, pq_gainoffsetenable);

	return ret;
}

static int mtk_tv_pqgamma_set_maxvalue(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_maxvalue *pq_maxvalue)
{
	int ret = 0;

	ret = mtk_video_pqgamma_set_maxvalue(crtc, pq_maxvalue);

	return ret;
}

static int mtk_tv_pqgamma_maxvalue_enable(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_pqgamma_maxvalue_enable *pq_maxvalueenable)
{
	int ret = 0;

	ret = mtk_video_pqgamma_maxvalue_enable(crtc, pq_maxvalueenable);

	return ret;
}



static void mtk_tv_kms_update_plane(
	struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC)
		mtk_gop_update_plane(state);
	else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO) {
		mtk_drm_atrace_begin("mtk_update_plane_VIDEO");
		mtk_video_display_update_plane(state);
		mtk_drm_atrace_end("mtk_update_plane_VIDEO");
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY) {
		mtk_gop_update_plane(state);
	} else {
		DRM_ERROR("[%s, %d]: unknown plane type %d\n",
			__func__, __LINE__, mplane->mtk_plane_type);
	}
}

static void mtk_tv_kms_disable_plane(
	struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC)
		mtk_gop_disable_plane(state);
	else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO)
		mtk_video_display_disable_plane(state);
	else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY) {
		mtk_gop_disable_plane(state);
	} else {
		DRM_ERROR("[%s, %d]: unknown plane type %d\n",
			__func__, __LINE__, mplane->mtk_plane_type);
	}
}

static void mtk_tv_kms_disable_vblank(
	struct mtk_tv_drm_crtc *mtk_tv_crtc)
{
	if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		mtk_gop_disable_vblank(mtk_tv_crtc);
	} else if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		//mtk_common_IRQ_set_mask(
		//	crtc, MTK_VIDEO_IRQTYPE_HK, MTK_VIDEO_SW_IRQ_TRIG_DISP, _gu32IRQ_Version);
	} else if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_EXT_VIDEO) {
		//mtk_common_IRQ_set_mask(
		//	crtc, MTK_VIDEO_IRQTYPE_HK, MTK_VIDEO_SW_IRQ_TRIG_DISP);
	} else {
		DRM_ERROR("[%s, %d]: Not support crtc type\n",
			__func__, __LINE__);
	}
}

static void mtk_video_atomic_crtc_begin(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct ext_crtc_prop_info *crtc_prop = NULL;
	struct mtk_video_context *pctx_video = NULL;

	if (!crtc || !old_crtc_state) {
		DRM_ERROR("[%s, %d]: null ptr! crtc=0x%p, old_crtc_state=0x%p\n",
			__func__, __LINE__, crtc, old_crtc_state);
		return;
	}

	pctx = (struct mtk_tv_kms_context *)
	crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	crtc_prop = pctx->ext_crtc_properties;
	pctx_video = &pctx->video_priv;

	//2. sm ml prepare resource
	if (mtk_tv_sm_ml_get_mem_index(&pctx_video->disp_ml))
		DRM_WARN("[%s, %d]: disp_ml get mem index fail", __func__, __LINE__);
	if (mtk_tv_sm_ml_get_mem_index(&pctx_video->vgs_ml))
		DRM_WARN("[%s, %d]: vgs_ml get mem index fail", __func__, __LINE__);

	/* video display crtc begin */
	/* mtk_video_display_atomic_crtc_begin(pctx_video); */
}

static void mtk_video_atomic_crtc_flush(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct ext_crtc_prop_info *crtc_prop = NULL;
	struct mtk_video_context *pctx_video = NULL;

	DRM_INFO("[%s][%d] atiomic flush start!\n", __func__, __LINE__);

	if (!crtc || !old_crtc_state) {
		DRM_ERROR("[%s, %d]: null ptr! crtc=0x%p, old_crtc_state=0x%p\n",
			__func__, __LINE__, crtc, old_crtc_state);
		return;
	}

	pctx = (struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	crtc_prop = pctx->ext_crtc_properties;
	pctx_video = &pctx->video_priv;

	mtk_video_update_crtc(crtc);

	/* video display crtc flush */
	mtk_video_display_atomic_crtc_flush(pctx_video);
	mtk_video_pixelshift_atomic_crtc_flush(crtc);

	if (pctx_video->disp_ml.cmd_cnt != 0) {
		if (mtk_tv_sm_ml_fire(&pctx_video->disp_ml))
			DRM_WARN("[%s, %d]: disp ml fire fail", __func__, __LINE__);
		mtk_video_panel_update_framesync_state(pctx);
	}

	if (mtk_tv_sm_ml_fire(&pctx_video->vgs_ml))
		DRM_ERROR("[%s, %d]: vgs ml fire fail", __func__, __LINE__);

	DRM_INFO("[%s][%d] atiomic flush end!\n", __func__, __LINE__);
}

static int mtk_commit_tail(void *data)
{
#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	MI_U32 u32Ret = MI_OK;

	u32Ret = MI_REALTIME_AddThread((MI_S8 *)"Domain_Render_DD",
			(MI_S8 *)"Class_KMS_Commit_tail_Video",
			(MI_S32)current->pid, NULL);

	if (u32Ret != MI_OK) {
		DRM_ERROR("[%s][%d] Realtime AddThread failed!\n", __func__, __LINE__);
	}
#endif
#endif

	while (!kthread_should_stop()) {
		const struct drm_mode_config_helper_funcs *funcs;
		struct drm_crtc_state *new_crtc_state;
		struct drm_crtc *crtc;
		ktime_t start;
		s64 commit_time_ms;
		unsigned int i, new_self_refresh_mask = 0;
		struct mtk_tv_kms_context *pctx = NULL;
		struct drm_atomic_state *old_state = NULL;
		struct drm_device *dev = NULL;

		wait_event_interruptible(atomic_commit_tail_wait, atomic_commit_tail_entry);
		atomic_commit_tail_entry = 0;

		mtk_drm_atrace_begin("mtk_commit_tail");
		pctx = (struct mtk_tv_kms_context *)(data);
		old_state = pctx->tvstate[MTK_DRM_CRTC_TYPE_VIDEO];
		dev = old_state->dev;

		DRM_INFO("[%s, %d] old_state=0x%p\n", __func__, __LINE__, old_state);

		funcs = dev->mode_config.helper_private;

		// * We're measuring the _entire_ commit, so the time will vary depending
		// * on how many fences and objects are involved. For the purposes of self
		// * refresh, this is desirable since it'll give us an idea of how
		// * congested things are. This will inform our decision on how often we
		// * should enter self refresh after idle.
		// *
		// * These times will be averaged out in the self refresh helpers to avoid
		// * overreacting over one outlier frame
		start = ktime_get();

		drm_atomic_helper_wait_for_fences(dev, old_state, false);

		drm_atomic_helper_wait_for_dependencies(old_state);

		// * We cannot safely access new_crtc_state after
		// * drm_atomic_helper_commit_hw_done() so figure out which crtc's have
		// * self-refresh active beforehand:
		for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i)
			if (new_crtc_state->self_refresh_active)
				new_self_refresh_mask |= BIT(i);

		if (funcs && funcs->atomic_commit_tail)
			funcs->atomic_commit_tail(old_state);
		else {
			mtk_drm_atrace_begin("helper_commit_tail");
			drm_atomic_helper_commit_tail(old_state);
			mtk_drm_atrace_end("helper_commit_tail");
		}

		commit_time_ms = ktime_ms_delta(ktime_get(), start);

		DRM_INFO("[%s, %d] commit_time_ms=%lld\n",
			__func__, __LINE__, commit_time_ms);

		if (commit_time_ms > 0)
			drm_self_refresh_helper_update_avg_times(old_state,
							 (unsigned long)commit_time_ms,
							 new_self_refresh_mask);

		drm_atomic_helper_commit_cleanup_done(old_state);

		drm_atomic_state_put(old_state);
		mtk_drm_atrace_end("mtk_commit_tail");

	}

#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	u32Ret = MI_REALTIME_RemoveThread((MI_S8 *)"Domain_Render_DD", (MI_S32)current->pid);
	if (u32Ret != MI_OK)
		DRM_ERROR("[%s][%d] Realtime RemoveThread failed!\n", __func__, __LINE__);
#endif
#endif

	return 0;
}

static int mtk_commit_tail_gCrtc(void *data)
{
#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	MI_U32 u32Ret = MI_OK;

	u32Ret = MI_REALTIME_AddThread((MI_S8 *)"Domain_Render_DD",
			(MI_S8 *)"Class_KMS_Commit_tail_Graphic",
			(MI_S32)current->pid, NULL);

	if (u32Ret != MI_OK)
		DRM_ERROR("[%s][%d] Realtime AddThread failed!\n", __func__, __LINE__);
#endif
#endif

	while (!kthread_should_stop()) {
		const struct drm_mode_config_helper_funcs *funcs;
		struct drm_crtc_state *new_crtc_state;
		struct drm_crtc *crtc;
		ktime_t start;
		s64 commit_time_ms;
		unsigned int i, new_self_refresh_mask = 0;
		struct mtk_tv_kms_context *pctx = NULL;
		struct drm_atomic_state *old_state = NULL;
		struct drm_device *dev = NULL;

		wait_event_interruptible(atomic_commit_tail_wait_Grapcrtc,
					atomic_commit_tail_Gcrtc_entry);
		atomic_commit_tail_Gcrtc_entry = 0;

		pctx = (struct mtk_tv_kms_context *)(data);
		old_state = pctx->tvstate[MTK_DRM_CRTC_TYPE_GRAPHIC];
		dev = old_state->dev;

		DRM_INFO("[%s, %d] old_state=0x%p\n", __func__, __LINE__, old_state);

		funcs = dev->mode_config.helper_private;

		// * We're measuring the _entire_ commit, so the time will vary depending
		// * on how many fences and objects are involved. For the purposes of self
		// * refresh, this is desirable since it'll give us an idea of how
		// * congested things are. This will inform our decision on how often we
		// * should enter self refresh after idle.
		// *
		// * These times will be averaged out in the self refresh helpers to avoid
		// * overreacting over one outlier frame
		start = ktime_get();

		drm_atomic_helper_wait_for_fences(dev, old_state, false);

		drm_atomic_helper_wait_for_dependencies(old_state);

		// * We cannot safely access new_crtc_state after
		// * drm_atomic_helper_commit_hw_done() so figure out which crtc's have
		// * self-refresh active beforehand:
		for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i)
			if (new_crtc_state->self_refresh_active)
				new_self_refresh_mask |= BIT(i);

		if (funcs && funcs->atomic_commit_tail)
			funcs->atomic_commit_tail(old_state);
		else
			drm_atomic_helper_commit_tail(old_state);

		commit_time_ms = ktime_ms_delta(ktime_get(), start);

		DRM_INFO("[%s, %d] commit_time_ms=%lld\n",
			__func__, __LINE__, commit_time_ms);

		if (commit_time_ms > 0)
			drm_self_refresh_helper_update_avg_times(old_state,
							 (unsigned long)commit_time_ms,
							 new_self_refresh_mask);

		drm_atomic_helper_commit_cleanup_done(old_state);

		drm_atomic_state_put(old_state);

	}

#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	u32Ret = MI_REALTIME_RemoveThread((MI_S8 *)"Domain_Render_DD", (MI_S32)current->pid);
	if (u32Ret != MI_OK)
		DRM_ERROR("[%s][%d] Realtime RemoveThread failed!\n", __func__, __LINE__);
#endif
#endif

	return 0;
}

int mtk_tv_drm_atomic_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock)
{
	int ret;

	DRM_INFO("[%s][%d] nonblock:%d\n", __func__, __LINE__, nonblock);

	start_crtc_begin = ktime_get();

	if (!nonblock) { //block
		ret = drm_atomic_helper_commit(dev, state, false);
	} else { // nonblock
		int i;
		struct drm_crtc *crtc = NULL;
		struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
		struct drm_crtc_state *new_crtc_state;
		struct mtk_drm_plane *mplane = NULL;
		struct mtk_tv_kms_context *pctx = NULL;

		drm_for_each_crtc(crtc, dev) {
			mtk_tv_crtc = to_mtk_tv_crtc(crtc);
			mplane = mtk_tv_crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO];
			pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
			break;
		}

		if (state->async_update) {
			ret = drm_atomic_helper_prepare_planes(dev, state);
			if (ret) {
				DRM_ERROR("[%s][%d] drm_atomic_helper_prepare_planes fail (%d)\n",
					__func__, __LINE__, ret);
				return ret;
			}

			drm_atomic_helper_async_commit(dev, state);
			drm_atomic_helper_cleanup_planes(dev, state);
			return 0;
		}

		ret = drm_atomic_helper_setup_commit(state, nonblock);
		if (ret) {
			mtk_drm_atrace_int("drm_setup_commit_FuncRet", ret);
			DRM_ERROR("[%s][%d] drm_atomic_helper_setup_commit fail (%d)\n",
				__func__, __LINE__, ret);
			return ret;
		}
		//INIT_WORK(&state->commit_work, commit_work);

		ret = drm_atomic_helper_prepare_planes(dev, state);
		if (ret) {
			DRM_ERROR("[%s][%d] drm_atomic_helper_prepare_planes fail (%d)\n",
				__func__, __LINE__, ret);
			return ret;
		}

		/*
		 *if (!nonblock) {
		 *	ret = drm_atomic_helper_wait_for_fences(dev, state, true);
		 *	if (ret)
		 *		goto err;
		 *}
		 */

		/*
		 * This is the point of no return - everything below never fails except
		 * when the hw goes bonghits. Which means we can commit the new state on
		 * the software side now.
		 */

		ret = drm_atomic_helper_swap_state(state, true);
		if (ret) {
			DRM_ERROR("[%s][%d] drm_atomic_helper_swap_state fail (%d)\n",
				__func__, __LINE__, ret);
			goto err;
		}

		/*
		 * Everything below can be run asynchronously without the need to grab
		 * any modeset locks at all under one condition: It must be guaranteed
		 * that the asynchronous work has either been cancelled (if the driver
		 * supports it, which at least requires that the framebuffers get
		 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
		 * before the new state gets committed on the software side with
		 * drm_atomic_helper_swap_state().
		 *
		 * This scheme allows new atomic state updates to be prepared and
		 * checked in parallel to the asynchronous completion of the previous
		 * update. Which is important since compositors need to figure out the
		 * composition of the next frame right after having submitted the
		 * current layout.
		 *
		 * NOTE: Commit work has multiple phases, first hardware commit, then
		 * cleanup. We want them to overlap, hence need system_unbound_wq to
		 * make sure work items don't artificially stall on each another.
		 */

		drm_atomic_state_get(state);

		/*
		 *if (nonblock)
		 *	queue_work(system_unbound_wq, &state->commit_work);
		 *else
		 *	commit_tail(state);
		 */

		for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
			mtk_tv_crtc = to_mtk_tv_crtc(crtc);
			if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
				pctx->tvstate[MTK_DRM_CRTC_TYPE_GRAPHIC] = state;
				DRM_INFO("[%s, %d] state=0x%p\n", __func__, __LINE__, state);
				atomic_commit_tail_Gcrtc_entry = 1;
				wake_up_interruptible(&atomic_commit_tail_wait_Grapcrtc);
				break;
			}

			if (mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO ||
				mtk_tv_crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_EXT_VIDEO) {
				pctx->tvstate[MTK_DRM_CRTC_TYPE_VIDEO] = state;
				DRM_INFO("[%s, %d] state=0x%p\n", __func__, __LINE__, state);
				atomic_commit_tail_entry = 1;
				wake_up_interruptible(&atomic_commit_tail_wait);
				break;
			}
		}

		return 0;

err:
		drm_atomic_helper_cleanup_planes(dev, state);

		return ret;

	}

	return ret;
}

static int mtk_tv_kms_atomic_set_crtc_common_property(
	struct mtk_tv_drm_crtc *mcrtc,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;
	uint32_t i;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mcrtc->crtc_private;
	struct ext_common_crtc_prop_info *crtc_common_prop =
		(pctx->ext_common_crtc_properties + mcrtc->mtk_crtc_type);

	for (i = 0; i < E_EXT_COMMON_CRTC_PROP_MAX; i++) {
		if (property == pctx->drm_common_crtc_prop[i]) {
			crtc_common_prop->prop_val[i] = val;
			crtc_common_prop->prop_update[i] = 1;
			ret = 0x0;
			break;
		}
	}

	return ret;
}

static int mtk_tv_kms_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_set_crtc_common_property(crtc, property, val);
	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		ret = mtk_video_atomic_set_crtc_property(
			crtc, state, property, val);
		if (ret)
			mtk_drm_atrace_int("drm_video_set_crtc_property_FuncRet", ret);
	} else if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		ret = mtk_gop_atomic_set_crtc_property(crtc, state, property, val);
	}

	return ret;
}

static int mtk_tv_kms_atomic_get_crtc_common_property(struct mtk_tv_drm_crtc *mcrtc,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;
	uint32_t i;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mcrtc->crtc_private;
	struct ext_common_crtc_prop_info *crtc_common_prop =
		pctx->ext_common_crtc_properties + mcrtc->mtk_crtc_type;

	for (i = 0; i < E_EXT_COMMON_CRTC_PROP_MAX; i++) {
		if (property == pctx->drm_common_crtc_prop[i]) {
			*val = crtc_common_prop->prop_val[i];
			ret = 0x0;
			break;
		}
	}

	return ret;
}

static int mtk_tv_kms_atomic_get_crtc_property(struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_get_crtc_common_property(crtc,
		property,
		val);

	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		ret = mtk_video_atomic_get_crtc_property(
			crtc, state, property, val);
	} else if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		ret = mtk_gop_atomic_get_crtc_property(crtc, state, property, val);
	}

	return ret;
}



static int mtk_tv_kms_atomic_set_plane_common_property(
	struct mtk_drm_plane *mplane,
	struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;
	uint32_t i;
	unsigned int plane_inx = mplane->base.index;

	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mplane->crtc_private;
	struct ext_common_plane_prop_info *plane_prop =
		pctx->ext_common_plane_properties + plane_inx;

	for (i = 0; i < E_EXT_COMMON_PLANE_PROP_MAX; i++) {
		if (property == pctx->drm_common_plane_prop[i]) {
			plane_prop->prop_val[i] = val;
			ret = 0x0;
			break;
		}
	}

	return ret;
}

static int mtk_tv_kms_atomic_get_plane_common_property(
	struct mtk_drm_plane *mplane,
	const struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;
	uint32_t i;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mplane->crtc_private;
	unsigned int plane_inx = mplane->base.index;
	struct ext_common_plane_prop_info *plane_prop =
		pctx->ext_common_plane_properties + plane_inx;

	for (i = 0; i < E_EXT_COMMON_PLANE_PROP_MAX; i++) {
		if (property == pctx->drm_common_plane_prop[i]) {
			*val = plane_prop->prop_val[i];
			ret = 0x0;
			break;
		}
	}

	return ret;
}

static int mtk_tv_kms_atomic_set_plane_property(
	struct mtk_drm_plane *mplane,
	struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_set_plane_common_property(
		mplane,
		state,
		property,
		val);
	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC) {
		ret = mtk_gop_atomic_set_plane_property(
			mplane,
			state,
			property,
			val);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO) {
		ret = mtk_video_display_atomic_set_plane_property(
			mplane,
			state,
			property,
			val);
		if (ret)
			mtk_drm_atrace_int("drm_video_set_plane_property_FuncRet", ret);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY) {
		//no use
		ret = 0;
	} else
		DRM_ERROR("[%s, %d]: unknown plane type %d\n",
			__func__, __LINE__, mplane->mtk_plane_type);

	return ret;
}

static int mtk_tv_kms_atomic_get_plane_property(
	struct mtk_drm_plane *mplane,
	const struct drm_plane_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_get_plane_common_property(
		mplane,
		state,
		property,
		val);
	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC) {
		ret = mtk_gop_atomic_get_plane_property(
			mplane,
			state,
			property,
			val);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO) {
		ret = mtk_video_display_atomic_get_plane_property(
			mplane,
			state,
			property,
			val);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY) {
		//no use
		ret = 0;
	} else
		DRM_ERROR("[%s, %d]: unknown plane type %d\n",
			__func__, __LINE__, mplane->mtk_plane_type);

	return ret;
}

void mtk_tv_kms_atomic_state_clear(struct drm_atomic_state *old_state)
{
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct mtk_drm_plane *mplane = NULL;
	int i;

	/* clear plane status */
	for_each_oldnew_plane_in_state(old_state, plane, old_plane_state, new_plane_state, i) {
		mplane = to_mtk_plane(plane);

		switch (mplane->mtk_plane_type) {
		case MTK_DRM_PLANE_TYPE_GRAPHIC:
			break;
		case MTK_DRM_PLANE_TYPE_VIDEO:
			mtk_video_display_clear_plane_status(to_mtk_plane_state(plane->state));
			break;
		case MTK_DRM_PLANE_TYPE_PRIMARY:
			break;
		default:
			DRM_ERROR("[%s, %d]: unknown plane type %d\n",
				__func__, __LINE__, mplane->mtk_plane_type);
			break;
		}
	}

	/* clear drm atomic status */
	drm_atomic_state_default_clear(old_state);
}

static int mtk_tv_kms_bootlogo_ctrl(
	struct mtk_tv_drm_crtc *crtc,
	unsigned int cmd,
	unsigned int *gop)
{
	int ret = 0;

	ret = mtk_gop_bootlogo_ctrl(crtc, cmd, gop);

	return ret;
}

static void mtk_tv_kms_atomic_crtc_begin(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{
	mtk_drm_atrace_begin("atomic_crtc_begin");
	if (mtk_gop_crtc_begin(crtc, old_crtc_state)) {
		DRM_ERROR("[GOP][%s, %d]mtk_gop_crtc_begin fail\n",
			__func__, __LINE__);
	}

	if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO)
		mtk_video_atomic_crtc_begin(crtc, old_crtc_state);
	mtk_drm_atrace_end("atomic_crtc_begin");
}

static void mtk_tv_kms_atomic_crtc_flush(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{
	mtk_drm_atrace_begin("atomic_crtc_flush");
	if (mtk_gop_crtc_flush(crtc, old_crtc_state)) {
		DRM_ERROR("[GOP][%s, %d]: mtk_gop_crtc_flush fail\n",
			__func__, __LINE__);
	}

	if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO)
		mtk_video_atomic_crtc_flush(crtc, old_crtc_state);

	mtk_tv_kms_CRTC_active_handler(crtc, old_crtc_state);
	mtk_drm_atrace_end("atomic_crtc_flush");
}

static int mtk_tv_kms_check_plane(struct drm_plane_state *plane_state,
				const struct drm_crtc_state *crtc_state,
				struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	int ret = 0;

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC) {
		ret = drm_atomic_helper_check_plane_state(plane_state, crtc_state,
						   DRM_PLANE_SCALE_CAPBILITY,
						   DRM_PLANE_HELPER_NO_SCALING,
						true, true);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO) {
		ret = mtk_video_display_check_plane(plane_state, crtc_state, state);
		if (ret)
			mtk_drm_atrace_int("drm_video_check_plane_FuncRet", ret);
	} else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY) {
		ret = drm_atomic_helper_check_plane_state(plane_state, crtc_state,
						   DRM_PLANE_SCALE_CAPBILITY,
						   DRM_PLANE_HELPER_NO_SCALING,
						true, true);
	} else {
		DRM_ERROR("[%s, %d]: unknown plane type %d\n",
			__func__, __LINE__, mplane->mtk_plane_type);
		ret = -EINVAL;
	}
	return ret;
}

int mtk_tv_kms_get_fence(struct mtk_tv_drm_crtc *mcrtc,
	struct drm_mtk_tv_fence_info *fenceInfo)
{
	int ret = 0;
	int timeline_val = 0;
	uint32_t gopIdx = 0;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mcrtc->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct drm_plane *plane = NULL;
	struct mtk_drm_plane *mplane = NULL;
	uint64_t fence_value = 0;

	drm_for_each_plane(plane, mcrtc->base.dev) {
		mplane = to_mtk_plane(plane);
		if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC) {
			gopIdx = mplane->gop_index;
			if (fenceInfo->bResetTimeLine[gopIdx]) {
				timeline_val = pctx_gop->gop_sync_timeline[gopIdx]->value;
				mtk_tv_sync_timeline_inc(pctx_gop->gop_sync_timeline[gopIdx],
					INT_MAX - timeline_val);
				pctx_gop->gop_sync_timeline[gopIdx]->value = 0;
				pctx_gop->mfence[gopIdx].fence_seqno = 0;
				continue;
			}
			if (fenceInfo->bCreateFence[gopIdx]) {
				fence_value = pctx_gop->mfence[gopIdx].fence_seqno + 1;
				if (fence_value > INT_MAX)
					pctx_gop->mfence[gopIdx].fence_seqno = 0;
				else
					pctx_gop->mfence[gopIdx].fence_seqno = fence_value;

				if (mtk_tv_sync_fence_create(pctx_gop->gop_sync_timeline[gopIdx],
					&pctx_gop->mfence[gopIdx])) {
					pctx_gop->mfence[gopIdx].fence = MTK_TV_INVALID_FENCE_FD;
					ret |= -ENOMEM;
				}

				fenceInfo->FenceFd[gopIdx] = pctx_gop->mfence[gopIdx].fence;
				DRM_INFO("[%s][%d]gop:%d, fence fd:%d, val:%d\n",
					__func__, __LINE__,
					gopIdx,
					pctx_gop->mfence[gopIdx].fence,
					pctx_gop->mfence[gopIdx].fence_seqno);
			}
		}
	}

	return ret;
}

int mtk_tv_kms_timeline_inc(struct mtk_tv_drm_crtc *mcrtc,
	uint32_t planeIdx)
{
	struct mtk_tv_kms_context *pctx;
	struct mtk_gop_context *pctx_gop;

	if (planeIdx >= MAX_GOP_PLANE_NUM) {
		DRM_ERROR("[%s, %d]: Invalid plane idx %d\n",
			__func__, __LINE__, planeIdx);
		return -EINVAL;
	}

	if (mcrtc == NULL) {
		DRM_ERROR("[%s, %d]: Null pointer of mcrtc\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mcrtc->crtc_private;

	if (pctx == NULL) {
		DRM_ERROR("[%s, %d]: Null pointer of pctx\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	pctx_gop = &pctx->gop_priv;

	if (pctx_gop == NULL) {
		DRM_ERROR("[%s, %d]: Null pointer of ctx gop\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (pctx_gop->gop_sync_timeline[planeIdx] == NULL) {
		DRM_ERROR("[%s, %d]: Null pointer of timeline\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	mtk_tv_sync_timeline_inc(pctx_gop->gop_sync_timeline[planeIdx], 1);

	return 0;
}

int mtk_tv_set_graphic_pq_buf(struct mtk_tv_drm_crtc *mcrtc,
	struct drm_mtk_tv_graphic_pq_setting *PqBufInfo)
{
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)mcrtc->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;

	return mtk_gop_set_pq_buf(pctx_gop, PqBufInfo);
}

int mtk_tv_get_graphic_roi(struct drm_mtk_tv_graphic_roi_info *RoiInfo)
{
	return mtk_gop_get_roi(RoiInfo);
}

int mtk_tv_set_graphic_roi(uint32_t threshold)
{
	return mtk_gop_set_roi(threshold);
}

static int mtk_tv_kms_get_gamma_enable(struct drm_mtk_tv_pnlgamma_enable *pnlgamma_enable)
{
	int ret = 0;

	ret = mtk_video_panel_get_gamma_enable(pnlgamma_enable);

	return ret;
}

static const struct mtk_tv_drm_crtc_ops mtk_tv_kms_crtc_ops = {
	.enable = mtk_tv_kms_enable,
	.disable = mtk_tv_kms_disable,
	.enable_vblank = mtk_tv_kms_enable_vblank,
	.disable_vblank = mtk_tv_kms_disable_vblank,
	.update_plane = mtk_tv_kms_update_plane,
	.disable_plane = mtk_tv_kms_disable_plane,
	.atomic_set_crtc_property = mtk_tv_kms_atomic_set_crtc_property,
	.atomic_get_crtc_property = mtk_tv_kms_atomic_get_crtc_property,
	.atomic_set_plane_property = mtk_tv_kms_atomic_set_plane_property,
	.atomic_get_plane_property = mtk_tv_kms_atomic_get_plane_property,
	.bootlogo_ctrl = mtk_tv_kms_bootlogo_ctrl,
	.atomic_crtc_begin = mtk_tv_kms_atomic_crtc_begin,
	.atomic_crtc_flush = mtk_tv_kms_atomic_crtc_flush,
	.gamma_set = mtk_tv_kms_gamma_set,
	.check_plane = mtk_tv_kms_check_plane,
	.graphic_set_testpattern = mtk_gop_set_testpattern,
	.set_stub_mode = mtk_render_common_set_stub_mode,
	.start_gfx_pqudata = mtk_gop_start_gfx_pqudata,
	.stop_gfx_pqudata = mtk_gop_stop_gfx_pqudata,
	.pnlgamma_enable = mtk_tv_kms_gamma_enable,
	.pnlgamma_gainoffset = mtk_tv_kms_gamma_gainoffset,
	.pnlgamma_gainoffset_enable = mtk_tv_kms_gamma_gainoffset_enable,
	.get_fence = mtk_tv_kms_get_fence,
	.pqgamma_curve = mtk_tv_pqgamma_set,
	.pqgamma_enable = mtk_tv_pqgamma_enable,
	.pqgamma_gainoffset = mtk_tv_pqgamma_gainoffset,
	.pqgamma_gainoffset_enable = mtk_tv_pqgamma_gainoffset_enable,
	.pqgamma_maxvalue = mtk_tv_pqgamma_set_maxvalue,
	.pqgamma_maxvalue_enable = mtk_tv_pqgamma_maxvalue_enable,
	.timeline_inc = mtk_tv_kms_timeline_inc,
	.set_graphic_pq_buf = mtk_tv_set_graphic_pq_buf,
	.get_graphic_roi = mtk_tv_get_graphic_roi,
	.set_graphic_roi = mtk_tv_set_graphic_roi,
	.pnlgamma_get_enable = mtk_tv_kms_get_gamma_enable,
	.ldm_set_led_check = mtk_ldm_set_led_check,
	.ldm_set_black_insert_enable = mtk_ldm_set_black_insert,
	.ldm_set_pq_param = mtk_ldm_set_pq_param,
	.ldm_set_VCOM_enable = mtk_ldm_set_ldm_VCOM_enable,
};

/*Connector*/
static int mtk_tv_kms_atomic_get_connector_common_property(
	struct mtk_tv_drm_connector *connector,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;
	uint32_t i;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)connector->connector_private;
	struct ext_common_connector_prop_info *connector_prop =
		pctx->ext_common_connector_properties + connector->mtk_connector_type;

	for (i = 0; i < E_EXT_CONNECTOR_COMMON_PROP_MAX; i++) {
		if (property == pctx->drm_common_connector_prop[i]) {
			*val = connector_prop->prop_val[i];
			ret = 0;
			break;
		}
	}

	return ret;
}

static int mtk_tv_kms_atomic_set_connector_common_property(
	struct mtk_tv_drm_connector *connector,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;
	uint32_t i;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)connector->connector_private;
	struct ext_common_connector_prop_info *connector_prop =
		pctx->ext_common_connector_properties + connector->mtk_connector_type;
	struct drm_property_blob *property_blob;

	for (i = 0; i < E_EXT_CONNECTOR_COMMON_PROP_MAX; i++) {
		if (property == pctx->drm_common_connector_prop[i]) {
			connector_prop->prop_val[i] = val;
			ret = 0;
			break;
		}
	}

	switch (i) {
	case E_EXT_CONNECTOR_COMMON_PROP_PNL_VBO_TX_MUTE_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_VBO_TX_MUTE_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);

		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown tx_mute_info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_tx_mute_info param;

			memset(&param, 0, sizeof(drm_st_tx_mute_info));
			memcpy(&param, property_blob->data, sizeof(drm_st_tx_mute_info));

			//ret = mtk_render_set_tx_mute(param);
			ret = mtk_render_set_tx_mute_common(&pctx->panel_priv, param,
			connector->mtk_connector_type);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set TX mute return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;
	case E_EXT_CONNECTOR_COMMON_PROP_PNL_PIXEL_MUTE_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_VBO_TX_MUTE_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);

		property_blob = drm_property_lookup_blob(property->dev, val);

		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown drm_st_pixel_mute_info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_pixel_mute_info param;

			memset(&param, 0, sizeof(drm_st_pixel_mute_info));
			memcpy(&param, property_blob->data, sizeof(drm_st_pixel_mute_info));

			//ret = mtk_render_set_tx_mute(param);
			if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_VIDEO)
				ret = mtk_render_set_pixel_mute_video(param);

			if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200 ||
				pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0203 ||
				pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0400 ||
				pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0500) {
				// not support
				ret = 0;
			} else {
				// only m6 series supported
				if (connector->mtk_connector_type ==
					MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO)
					ret = mtk_render_set_pixel_mute_deltav(param);
			}

			if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_GRAPHIC)
				ret = mtk_render_set_pixel_mute_gfx(param);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set TX mute return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;

	case E_EXT_CONNECTOR_COMMON_PROP_BACKLIGHT_MUTE_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_COMMON_PROP_BACKLIGHT_MUTE_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown drm_st_backlight_mute_info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_backlight_mute_info param;

			memset(&param, 0, sizeof(drm_st_backlight_mute_info));
			memcpy(&param, property_blob->data, sizeof(drm_st_backlight_mute_info));

			ret = mtk_render_set_backlight_mute(&pctx->panel_priv, param);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set Backlight mute return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;
	case E_EXT_CONNECTOR_COMMON_PROP_HMIRROR_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_COMMON_PROP_HMIRROR_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		pctx->panel_priv.cus.hmirror_en = (bool)val;
		ret = mtk_render_set_output_hmirror_enable(&pctx->panel_priv);

		DRM_INFO("[%s][%d] set Hmirror return = %d!!\n",
			__func__, __LINE__, ret);
		break;
	case E_EXT_CONNECTOR_COMMON_PROP_PNL_VBO_CTRLBIT:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_COMMON_PROP_PNL_VBO_CTRLBIT = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);
		if ((property_blob != NULL) && (val != 0)) {
			struct drm_st_ctrlbits stCtrlbits;

			memset(&stCtrlbits, 0, sizeof(struct drm_st_ctrlbits));
			memcpy(&stCtrlbits, property_blob->data, sizeof(struct drm_st_ctrlbits));
			ret = mtk_render_set_vbo_ctrlbit(&pctx->panel_priv, &stCtrlbits);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_VBO_CTRLBIT = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
			ret = -EINVAL;
		}
		break;
	case E_EXT_CONNECTOR_COMMON_PROP_PNL_GLOBAL_MUTE_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_COMMON_PROP_PNL_GLOBAL_MUTE_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);

		property_blob = drm_property_lookup_blob(property->dev, val);

		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown drm_st_global_mute_info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_global_mute_info param;

			memset(&param, 0, sizeof(drm_st_global_mute_info));
			memcpy(&param, property_blob->data, sizeof(drm_st_global_mute_info));

			ret = mtk_render_set_global_mute(&pctx->panel_priv,
				connector->connector_private, param, connector->mtk_connector_type);

			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set global mute return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;
	default:
		ret = -EINVAL;
		break;

	}
	return ret;
}

static int mtk_tv_kms_atomic_set_connector_property(
	struct mtk_tv_drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_set_connector_common_property(
		connector,
		property,
		val);

	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_VIDEO) {
		ret = mtk_video_atomic_set_connector_property(
			connector,
			state,
			property,
			val);
	} else if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_GRAPHIC) {
		return 0;
	}

	return ret;
}

static int mtk_tv_kms_atomic_get_connector_property(
	struct mtk_tv_drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;

	ret = mtk_tv_kms_atomic_get_connector_common_property(
			connector,
			property,
			val);

	if (ret == 0) {
		// found in common property list, no need to find in other list
		return ret;
	}

	if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_VIDEO) {
		ret = mtk_video_atomic_get_connector_property(
			connector,
			state,
			property,
			val);
	} else if (connector->mtk_connector_type == MTK_DRM_CONNECTOR_TYPE_GRAPHIC) {
		return ret;
	}

	return ret;
}

static const struct mtk_tv_drm_connector_ops mtk_tv_kms_connector_ops = {
	.atomic_set_connector_property =
		mtk_tv_kms_atomic_set_connector_property,
	.atomic_get_connector_property =
		mtk_tv_kms_atomic_get_connector_property,
};

static int _mtk_drm_kms_parse_dts(struct mtk_tv_kms_context *pctx)
{
	struct device_node *np;
	int u32Tmp = DTB_MSK;
	int ret = 0, ret_tmp = 0;
	const char *name;

	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_KMS_COMPATIBLE_STR);
	if (np != NULL) {
		ret = of_irq_get(np, 0);
		if (ret < 0) {
			DRM_ERROR("[%s, %d]: of_irq_get failed\r\n",
				__func__, __LINE__);
			return ret;
		}
		pctx->irq.video_irq_num = ret;

		ret = of_irq_get(np, 1);
		if (ret < 0) {
			DRM_ERROR("[%s, %d]: of_irq_get failed\r\n",
				__func__, __LINE__);
			return ret;
		}
		pctx->irq.graphic_irq_vgsep_num = ret;

		ret = of_irq_get(np, 2);
		if (ret < 0) {
			DRM_ERROR("[%s, %d]: of_irq_get failed\r\n",
				__func__, __LINE__);
			return ret;
		}
		pctx->irq.mod_detect_irq_num = ret;

		name = "video_irq_affinity";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->irq.video_irq_affinity = (u32Tmp < num_online_cpus()) ? u32Tmp : 0;

		name = "PRIMARY_PLANE_NUM";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_num[MTK_DRM_PLANE_TYPE_PRIMARY] = u32Tmp;

		name = "PRIMARY_PLANE_INDEX_START";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_index_start[MTK_DRM_PLANE_TYPE_PRIMARY] = u32Tmp;

		name = "VIDEO_PLANE_NUM";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_num[MTK_DRM_PLANE_TYPE_VIDEO] = u32Tmp;

		name = "VIDEO_PLANE_INDEX_START";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_index_start[MTK_DRM_PLANE_TYPE_VIDEO] = u32Tmp;

		name = "VIDEO_OLED_PIXELSHIFT_H_RANGE_MAX";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		if (u32Tmp < INT8_NEGATIVE)
			pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMax = (int8_t)u32Tmp;
		else
			pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMax =
				-(int8_t)(~u32Tmp&0xFF)-1;


		name = "VIDEO_OLED_PIXELSHIFT_H_RANGE_MIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		if (u32Tmp < INT8_NEGATIVE)
			pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMin = (int8_t)u32Tmp;
		else
			pctx->pixelshift_priv.i8OLEDPixelshiftHRangeMin =
				-(int8_t)(~u32Tmp&0xFF)-1;


		name = "VIDEO_OLED_PIXELSHIFT_V_RANGE_MAX";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		if (u32Tmp < INT8_NEGATIVE)
			pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMax = (int8_t)u32Tmp;
		else
			pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMax =
				-(int8_t)(~u32Tmp&0xFF)-1;


		name = "VIDEO_OLED_PIXELSHIFT_V_RANGE_MIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		if (u32Tmp < INT8_NEGATIVE)
			pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMin = (int8_t)u32Tmp;
		else
			pctx->pixelshift_priv.i8OLEDPixelshiftVRangeMin =
				-(int8_t)(~u32Tmp&0xFF)-1;

		name = "reg_num";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->video_priv.reg_num = u32Tmp;

		name = "GRAPHIC_PLANE_NUM";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_num[MTK_DRM_PLANE_TYPE_GRAPHIC] = u32Tmp;

		name = "GRAPHIC_PLANE_INDEX_START";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->plane_index_start[MTK_DRM_PLANE_TYPE_GRAPHIC] = u32Tmp;

		name = "framesync_version";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->framesync_version = u32Tmp;

		name = "Clk_Version";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->clk_version = u32Tmp;

		name = "chip_series ";
		ret = of_property_read_u32(np, name, &u32Tmp);
		ret &= ret_tmp;
		pctx->chip_series  = u32Tmp;
	}

	return ret;
}

static int _mtk_drm_kms_read_efuse(struct mtk_tv_kms_context *pctx)
{
	uint32_t value = 0;
	int ret = 0;

	if (pctx == NULL) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = drv_hwreg_render_common_get_efuse(E_EFUSE_IDX_111, &value);
	if (ret != 0) {
		DRM_ERROR("hwreg get efuse(E_EFUSE_IDX_111) fail, ret = %d", ret);
		value = 0;
		goto RET;
	}
	pctx->hw_caps.support_live_tone = value;

	ret = drv_hwreg_render_common_get_efuse(E_EFUSE_IDX_34, &value);
	if (ret != 0) {
		DRM_ERROR("hwreg get efuse(E_EFUSE_IDX_34) fail, ret = %d", ret);
		value = 0;
		goto RET;
	}
	pctx->hw_caps.support_amb_light = value;

	ret = drv_hwreg_render_common_get_efuse(E_EFUSE_IDX_112, &value);
	if (ret != 0) {
		DRM_ERROR("hwreg get efuse(E_EFUSE_IDX_112) fail, ret = %d", ret);
		value = 0;
		goto RET;
	}
	pctx->hw_caps.support_i_view = value;

	ret = drv_hwreg_render_common_get_efuse(E_EFUSE_IDX_113, &value);
	if (ret != 0) {
		DRM_ERROR("hwreg get efuse(E_EFUSE_IDX_113) fail, ret = %d", ret);
		value = 0;
		goto RET;
	}
	pctx->hw_caps.support_i_view_pip = value;

	return ret;

RET:
	return ret;
}

static void mtk_tv_drm_crtc_finish_page_flip(struct mtk_tv_drm_crtc *mcrtc)
{
	struct drm_crtc *crtc = &mcrtc->base;
	unsigned long flags, flags_mcrtc;

	spin_lock_irqsave(&mcrtc->event_lock, flags_mcrtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	drm_crtc_send_vblank_event(crtc, mcrtc->event);
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

	mcrtc->event = NULL;
	mcrtc->pending_needs_vblank = FALSE;
	spin_unlock_irqrestore(&mcrtc->event_lock, flags_mcrtc);
}

static irqreturn_t mtk_tv_kms_irq_top_handler(int irq, void *dev_id)
{
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)dev_id;
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	bool IRQstatus = 0;

	mtk_common_IRQ_get_status(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO],
		MTK_VIDEO_IRQTYPE_HK,
		MTK_VIDEO_SW_IRQ_TRIG_DISP,
		&IRQstatus,
		pctx_video->video_hw_ver.irq);

	if (IRQstatus == TRUE) {
		mtk_common_IRQ_set_clr(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO],
			MTK_VIDEO_IRQTYPE_HK,
			MTK_VIDEO_SW_IRQ_TRIG_DISP,
			pctx_video->video_hw_ver.irq);

		pctx->irq.video_irq_status[MTK_VIDEO_SW_IRQ_TRIG_DISP] = TRUE;
		pctx_video->sw_delay = ktime_ms_delta(ktime_get(), start_crtc_begin);

		return IRQ_WAKE_THREAD;
	} else {
		return IRQ_NONE;
	}
}

int mtk_tv_panel_debugvttvinfo_print(
	struct mtk_tv_kms_context *pctx, uint16_t u16Idex)
{
	#define VTTV_INFO_NUM 25
	uint16_t u16VttDebugInfo[VTTV_INFO_NUM] = {0};
	unsigned long flags;
	struct st_Vtt_info stVtt_info;
	struct mtk_panel_context *pctx_pnl = &(pctx->panel_priv);

	if (((pctx_pnl->gu64panel_debug_log) & (0x01<<E_PNL_VTTV_INFO_DBGLOG))
		!= (0x01<<E_PNL_VTTV_INFO_DBGLOG))
		return 0;

	spin_lock_irqsave(&synclockdebugVTT, flags);
	drv_hwreg_render_pnl_get_vttv_debuginfo(&stVtt_info);
	spin_unlock_irqrestore(&synclockdebugVTT, flags);

	u16VttDebugInfo[0] = stVtt_info.u16value3;
	u16VttDebugInfo[1] = stVtt_info.u16value4&0xFF;
	u16VttDebugInfo[2] = (stVtt_info.u16value4>>8)&0xFF;
	u16VttDebugInfo[3] = stVtt_info.u16value5;
	u16VttDebugInfo[4] = stVtt_info.u16value6;
	u16VttDebugInfo[5] = stVtt_info.u16value7&0x80;
	u16VttDebugInfo[6] = stVtt_info.u16value8;
	u16VttDebugInfo[7] = stVtt_info.u16value9;
	u16VttDebugInfo[8] = stVtt_info.u16value10;
	u16VttDebugInfo[9] = stVtt_info.u16value11;
	u16VttDebugInfo[10] = stVtt_info.u16value12;
	u16VttDebugInfo[11] = stVtt_info.u16value13;
	u16VttDebugInfo[12] = stVtt_info.u16value14;
	u16VttDebugInfo[13] = stVtt_info.u16value15;
	u16VttDebugInfo[14] = stVtt_info.u16value16;
	u16VttDebugInfo[15] = stVtt_info.u16value17;
	u16VttDebugInfo[16] = stVtt_info.u16value18;
	u16VttDebugInfo[17] = stVtt_info.u16value19;
	u16VttDebugInfo[18] = stVtt_info.u16value20;
	u16VttDebugInfo[19] = stVtt_info.u16value21;
	u16VttDebugInfo[20] = stVtt_info.u16value0;
	u16VttDebugInfo[21] = stVtt_info.u16value1;
	u16VttDebugInfo[22] = stVtt_info.u16value22;
	u16VttDebugInfo[23] = u16Idex;
	u16VttDebugInfo[24] = stVtt_info.u16value2;

	DRM_INFO("[VTTV_DEBUG] %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x,"
		"%4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x, %4x,"
		"%4x, %4x, %4x\n",
		u16VttDebugInfo[0],
		u16VttDebugInfo[1],
		u16VttDebugInfo[2],
		u16VttDebugInfo[3],
		u16VttDebugInfo[4],
		u16VttDebugInfo[5],
		u16VttDebugInfo[6],
		u16VttDebugInfo[7],
		u16VttDebugInfo[8],
		u16VttDebugInfo[9],
		u16VttDebugInfo[10],
		u16VttDebugInfo[11],
		u16VttDebugInfo[12],
		u16VttDebugInfo[13],
		u16VttDebugInfo[14],
		u16VttDebugInfo[15],
		u16VttDebugInfo[16],
		u16VttDebugInfo[17],
		u16VttDebugInfo[18],
		u16VttDebugInfo[19],
		u16VttDebugInfo[20],
		u16VttDebugInfo[21],
		u16VttDebugInfo[22],
		u16VttDebugInfo[23],
		u16VttDebugInfo[24]);

	if (stVtt_info.u16value0 < stVtt_info.u16value23)
		DRM_INFO("[VTTV_DEBUG] ERROR VTT lower than VTT min happen in %d\n", u16Idex);

	#undef VTTV_INFO_NUM

	return 0;
}

static irqreturn_t mtk_tv_kms_irq_bottom_handler(int irq, void *dev_id)
{
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)dev_id;
	struct mtk_tv_kms_vsync_callback_info *info;
	struct list_head *list_node, *tmp_node;

	/* check MTK_VIDEO_SW_IRQ_TRIG_DISP */
	if (pctx->irq.video_irq_status[MTK_VIDEO_SW_IRQ_TRIG_DISP] == TRUE) {
		if (pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].pending_needs_vblank == true) {
			mtk_tv_drm_crtc_finish_page_flip(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO]);
			//pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].pending_needs_vblank = false;
		}

		if (pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].pending_needs_vblank == true) {
			mtk_tv_drm_crtc_finish_page_flip(&pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO]);
			//pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].pending_needs_vblank = false;
		}

		drm_crtc_handle_vblank(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].base);
		drm_crtc_handle_vblank(&pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].base);

		/* MTK_VIDEO_SW_IRQ_TRIG_DISP handled, clear status in pctx */
		pctx->irq.video_irq_status[MTK_VIDEO_SW_IRQ_TRIG_DISP] = FALSE;

		list_for_each_safe(list_node, tmp_node, &mtk_tv_kms_callback_info_list) {
			info = list_entry(list_node, struct mtk_tv_kms_vsync_callback_info, list);
			WRITE_ONCE(info->task_entry, 1);
			wake_up_interruptible(&info->wait_queue_head);
		}
		mtk_tv_panel_debugvttvinfo_print(pctx, 4);
	} else if (pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].active_change) {
		if (pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].pending_needs_vblank == true) {
			mtk_tv_drm_crtc_finish_page_flip(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO]);
			//pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].pending_needs_vblank = false;
		}
		pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].active_change = 0;
	} else if (pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].active_change) {
		if (pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].pending_needs_vblank == true) {
			mtk_tv_drm_crtc_finish_page_flip(&pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO]);
			//pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].pending_needs_vblank = false;
		}
		pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].active_change = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t mtk_tv_graphic_path_irq_handler(int irq, void *dev_id)
{
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)dev_id;
	//struct mtk_gop_context *pctx_gop = &pctx->gop_priv;

	mtk_gop_disable_vblank(&pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC]);

	drm_crtc_handle_vblank(&pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].base);

	if (pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].pending_needs_vblank == true) {
		mtk_tv_drm_crtc_finish_page_flip(&pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC]);
		//pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].pending_needs_vblank = false;
	}
	mtk_gop_enable_vblank(&pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC]);

	return IRQ_HANDLED;
}

static irqreturn_t mtk_tv_mod_detect_irq_top_handler(int irq, void *dev_id)
{
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)dev_id;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	bool IRQstatus_V1_VSYNC = 0;
	bool IRQstatus_V1_VDE_POS = 0;
	bool IRQstatus_V1_VDE_NEG = 0;
	uint32_t fps_x100 = 0;

	// read MOD ISR status
	IRQstatus_V1_VSYNC = mtk_tv_drm_GetMODInterrupt(E_MOD_IRQ_V1_VSYNC_INTR);
	IRQstatus_V1_VDE_POS = mtk_tv_drm_GetMODInterrupt(E_MOD_IRQ_V1_VDE_POS_INTR);
	IRQstatus_V1_VDE_NEG = mtk_tv_drm_GetMODInterrupt(E_MOD_IRQ_V1_VDE_NEG_INTR);

	if (IRQstatus_V1_VSYNC == TRUE) {
		// this Interrupt happened, clear it
		mtk_tv_drm_ClearMODInterrupt(E_MOD_IRQ_V1_VSYNC_INTR);

		if (pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0500) {
			//Only TCONless panel supports LC toggle function
			if (pctx_pnl->cus.tcon_en && mtk_tv_drm_Is_Support_TCON_LC_SecToggle())
				mtk_tv_drm_TCON_LC_SecToggle_Control(pctx_pnl);
			if (mtk_tv_drm_Is_Support_PatDetectFunc())
				mtk_tv_drm_TCON_PatDetectFunc_Control();
		}

		return IRQ_WAKE_THREAD;
	} else if (IRQstatus_V1_VDE_POS == TRUE) {
		// this Interrupt happened, clear it
		mtk_tv_drm_ClearMODInterrupt(E_MOD_IRQ_V1_VDE_POS_INTR);

		mtk_tv_drm_Cust_VDF_VDE_START_Handle();
		return IRQ_WAKE_THREAD;
	} else if (IRQstatus_V1_VDE_NEG == TRUE) {
		// this Interrupt happened, clear it
		mtk_tv_drm_ClearMODInterrupt(E_MOD_IRQ_V1_VDE_NEG_INTR);

		fps_x100 = mtk_tv_drm_get_fpsx100_value();
		mtk_tv_drm_Cust_VDF_VDE_END_Handle((void *)&fps_x100);
		return IRQ_WAKE_THREAD;
	} else {
		return IRQ_NONE;
	}
}

static irqreturn_t mtk_tv_mod_detect_irq_bottom_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static void clean_tv_kms_context(
	struct device *dev,
	struct mtk_tv_kms_context *pctx)
{
	if (pctx && dev) {
		devm_kfree(dev, pctx->gop_priv.gop_plane_properties);
		pctx->gop_priv.gop_plane_properties = NULL;

		devm_kfree(dev, pctx->ext_common_plane_properties);
		pctx->ext_common_plane_properties = NULL;
		devm_kfree(dev, pctx->ext_crtc_properties);
		pctx->ext_crtc_properties = NULL;
		devm_kfree(dev, pctx->ext_crtc_graphic_properties);
		pctx->ext_crtc_graphic_properties = NULL;
		devm_kfree(dev, pctx->ext_common_crtc_properties);
		pctx->ext_common_crtc_properties = NULL;
		devm_kfree(dev, pctx->ext_connector_properties);
		pctx->ext_connector_properties = NULL;
		devm_kfree(dev, pctx->ext_common_connector_properties);
		pctx->ext_common_connector_properties = NULL;

		devm_kfree(dev, pctx);
	}
}

struct drm_property *mtk_tv_kms_create_property_instance(
	struct drm_device *drm_dev,
	const struct ext_prop_info *ext_prop)
{
	struct drm_property *prop = NULL;

	if (ext_prop->flag & DRM_MODE_PROP_ENUM) {
		prop = drm_property_create_enum(
			drm_dev,
			ext_prop->flag,
			ext_prop->prop_name,
			ext_prop->enum_info.enum_list,
			ext_prop->enum_info.enum_length);

		if (prop == NULL) {
			DRM_ERROR("[%s, %d]: create ext prop fail!!\n",
				__func__, __LINE__);
			return NULL;
		}
	} else if (ext_prop->flag & DRM_MODE_PROP_RANGE) {
		prop = drm_property_create_range(
			drm_dev,
			ext_prop->flag,
			ext_prop->prop_name,
			ext_prop->range_info.min,
			ext_prop->range_info.max);

		if (prop == NULL) {
			DRM_ERROR("[%s, %d]: create ext prop fail!!\n",
				__func__, __LINE__);
			return NULL;
		}
	} else if (ext_prop->flag & DRM_MODE_PROP_SIGNED_RANGE) {
		prop = drm_property_create_signed_range(
			drm_dev,
			ext_prop->flag,
			ext_prop->prop_name,
			(U642I64)(ext_prop->range_info.min),
			(U642I64)(ext_prop->range_info.max));
		if (prop == NULL) {
			DRM_ERROR("[%s, %d]: create ext prop fail!!\n",
				__func__, __LINE__);
			return NULL;
		}
	} else if (ext_prop->flag & DRM_MODE_PROP_BLOB) {
		prop = drm_property_create(drm_dev,
		DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
		ext_prop->prop_name, 0);
		if (prop == NULL) {
			DRM_ERROR("[%s, %d]: create ext blob prop fail!!\n",
				__func__, __LINE__);
			return NULL;
		}
	} else {
		DRM_ERROR("[%s, %d]: unknown prop flag 0x%x !!\n",
			__func__, __LINE__, ext_prop->flag);
		return NULL;
	}

	return prop;
}

int mtk_tv_kms_create_ext_props(
	struct mtk_tv_kms_context *pctx,
	enum ext_prop_type prop_type)
{
	struct drm_property *prop = NULL;
	const struct ext_prop_info *prop_def = NULL;
	int extend_prop_count = 0;
	const struct ext_prop_info *ext_prop = NULL;
	struct drm_device *drm_dev = pctx->drm_dev;
	uint32_t i, plane_count = 0, connector_count = 0, crtc_count = 0;
	struct mtk_drm_plane *mplane = NULL;
	struct drm_mode_object *obj = NULL;
	struct drm_plane *plane = NULL;
	unsigned int zpos_min = 0;
	unsigned int zpos_max = 0;
	uint64_t init_val;
	struct drm_connector *connector;
	enum drm_mtk_connector_type mtk_connector_type = MTK_DRM_CONNECTOR_TYPE_MAX;
	struct drm_connector_list_iter conn_iter;
	struct mtk_tv_drm_connector *mconnector;
	struct drm_crtc *crtc;
	struct mtk_tv_drm_crtc *mtk_tv_crtc;

	struct drm_property_blob *propBlob;
	//struct drm_property *propMemcStatus = NULL;
	struct property_blob_memc_status stMemcStatus;

	memset(&stMemcStatus, 0, sizeof(struct property_blob_memc_status));

	if (prop_type == E_EXT_PROP_TYPE_COMMON_PLANE) {
		prop_def = ext_common_plane_props_def;
		extend_prop_count = ARRAY_SIZE(ext_common_plane_props_def);
	} else if (prop_type == E_EXT_PROP_TYPE_CRTC) {
		prop_def = ext_crtc_props_def;
		extend_prop_count = ARRAY_SIZE(ext_crtc_props_def);
	} else if (prop_type == E_EXT_PROP_TYPE_CONNECTOR) {
		prop_def = ext_connector_props_def;
		extend_prop_count = ARRAY_SIZE(ext_connector_props_def);
	} else if (prop_type == E_EXT_PROP_TYPE_CRTC_GRAPHIC) {
		prop_def = ext_crtc_graphic_props_def;
		extend_prop_count = ARRAY_SIZE(ext_crtc_graphic_props_def);
	} else if (prop_type == E_EXT_PROP_TYPE_CRTC_COMMON) {
		prop_def = ext_crtc_common_props_def;
		extend_prop_count = ARRAY_SIZE(ext_crtc_common_props_def);
	} else if (prop_type == E_EXT_PROP_TYPE_CONNECTOR_COMMON) {
		prop_def = ext_connector_common_props_def;
		extend_prop_count = ARRAY_SIZE(ext_connector_common_props_def);
	} else {
		DRM_ERROR("[%s, %d]: unknown ext_prop_type %d!!\n",
			__func__, __LINE__, prop_type);
	}
	// create extend common plane properties
	for (i = 0; i < extend_prop_count; i++) {
		ext_prop = &prop_def[i];
		prop = mtk_tv_kms_create_property_instance(drm_dev,
			ext_prop);

		if (prop == NULL)
			return -ENOMEM;
		// attach created props & add created props to context
		if (prop_type == E_EXT_PROP_TYPE_COMMON_PLANE) {
			drm_for_each_plane(plane, drm_dev) {
				mplane = to_mtk_plane(plane);
				obj = &(mplane->base.base);

			// attach property by init value
			if (strcmp(prop->name, MTK_PLANE_PROP_PLANE_TYPE) == 0)
				init_val = mplane->mtk_plane_type;

			else
				init_val = ext_prop->init_val;

			drm_object_attach_property(
				obj,
				prop,
				init_val);
			(pctx->ext_common_plane_properties + plane_count)
				->prop_val[i] =	init_val;
			pctx->drm_common_plane_prop[i] = prop;
			++plane_count;

			// create zpos property
			if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_VIDEO)
				zpos_max = zpos_min + pctx->video_priv.videoPlaneType_TypeNum - 1;
			else if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC)
				zpos_max = zpos_min + pctx->plane_num[mplane->mtk_plane_type] - 1;

			drm_plane_create_zpos_property(plane,
				mplane->zpos,
				zpos_min,
				zpos_max);
			}
		} else if (prop_type == E_EXT_PROP_TYPE_CRTC) {
			obj = &(pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].base.base);

			// attach property by init value
			drm_object_attach_property(
				obj,
				prop,
				ext_prop->init_val);

			if (_mtk_kms_check_array_idx(i, E_EXT_CRTC_PROP_MAX))
				return -ENOMEM;

			pctx->drm_crtc_prop[i] =
				prop;
			pctx->ext_crtc_properties->prop_val[i] =
				ext_prop->init_val;
		} else if (prop_type == E_EXT_PROP_TYPE_CRTC_GRAPHIC) {
			obj = &(pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].base.base);
			drm_object_attach_property(obj, prop, ext_prop->init_val);

			if (_mtk_kms_check_array_idx(i, E_EXT_CRTC_GRAPHIC_PROP_MAX))
				return -ENOMEM;

			pctx->drm_crtc_graphic_prop[i] = prop;
			pctx->ext_crtc_graphic_properties->prop_val[i] = ext_prop->init_val;
		} else if (prop_type == E_EXT_PROP_TYPE_CRTC_COMMON) {
			pctx->drm_common_crtc_prop[i] = prop;
			drm_for_each_crtc(crtc, drm_dev) {
				mtk_tv_crtc = to_mtk_tv_crtc(crtc);
				obj = &(pctx->crtc[crtc_count].base.base);
				if (strcmp(prop->name, MTK_CRTC_COMMON_PROP_CRTC_TYPE) == 0) {
					drm_object_attach_property(obj, prop,
								mtk_tv_crtc->mtk_crtc_type);
					(pctx->ext_common_crtc_properties +
					crtc_count)->prop_val[i] = mtk_tv_crtc->mtk_crtc_type;
					++crtc_count;
				} else {
					drm_object_attach_property(obj, prop,
									ext_prop->init_val);
					(pctx->ext_common_crtc_properties +
					crtc_count)->prop_val[i] = ext_prop->init_val;
					++crtc_count;
				}
			}
			crtc_count = 0;
		} else if (prop_type == E_EXT_PROP_TYPE_CONNECTOR) {
			obj = &(pctx->connector[MTK_DRM_CONNECTOR_TYPE_VIDEO].base.base);
			drm_object_attach_property(obj,
				prop,
				ext_prop->init_val);

			if (_mtk_kms_check_array_idx(i, E_EXT_CONNECTOR_PROP_MAX))
				return -ENOMEM;

			pctx->drm_connector_prop[i] =
				prop;
			pctx->ext_connector_properties->prop_val[i] =
				ext_prop->init_val;
		} else if (prop_type == E_EXT_PROP_TYPE_CONNECTOR_COMMON) {
			pctx->drm_common_connector_prop[i] = prop;
			drm_connector_list_iter_begin(drm_dev, &conn_iter);
			drm_for_each_connector_iter(connector, &conn_iter) {
				mconnector = to_mtk_tv_connector(connector);
				obj = &(mconnector->base.base);
				if (strcmp(prop->name,
					MTK_CONNECTOR_COMMON_PROP_CONNECTOR_TYPE) == 0) {
					mtk_connector_type = mconnector->mtk_connector_type;
					(pctx->ext_common_connector_properties + connector_count)
					->prop_val[i] = mtk_connector_type;
					++connector_count;
					drm_object_attach_property(obj, prop, mtk_connector_type);
				} else {
					drm_object_attach_property(obj, prop,
									ext_prop->init_val);
					(pctx->ext_common_connector_properties +
					connector_count)->prop_val[i] = ext_prop->init_val;
					++connector_count;
				}
			}
			connector_count = 0;
			drm_connector_list_iter_end(&conn_iter);

		} else {
			DRM_ERROR("[%s, %d]: unknown ext_prop_type %d!!\n",
				__func__, __LINE__, prop_type);
		}
	}
	//for creat blob
	if (prop_type == E_EXT_PROP_TYPE_CRTC) {
		propBlob = drm_property_create_blob(
			pctx->drm_dev,
			sizeof(struct property_blob_memc_status),
			&stMemcStatus);
		if (propBlob == NULL) {
			DRM_ERROR("[%s, %d]: drm_property_create_blob fail!!\n",
				__func__, __LINE__);
			goto NOMEM;
		} else {
#if (0)
			if (propMemcStatus != NULL) {
				for (i = 0; i < E_EXT_CRTC_PROP_MAX; i++) {
					if (propMemcStatus ==
						pctx->drm_crtc_prop[i]) {
						pctx->ext_crtc_properties
							->prop_val[i] =
							propBlob->base.id;
						break;
					}
				}
			}
#endif
		}
	}

	return 0;

NOMEM:
	return -ENOMEM;
}

int mtk_tv_kms_CRTC_active_handler(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{
	struct drm_crtc drm_crtc_base = crtc->base;
	struct drm_crtc_state *cur_state = drm_crtc_base.state;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)crtc->crtc_private;
	struct drm_panel *pDrmPanel = NULL;

	pDrmPanel = pctx->connector[0].base.panel;

	if (cur_state->active_changed) {
		DRM_INFO("[%s][%d] active:%d, active_change: %d\n", __func__, __LINE__,
			cur_state->active, cur_state->active_changed);
		if (cur_state->active == 0) {
			if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
				crtc->active_change = 1;
				//pDrmPanel->funcs->disable(pDrmPanel); //disable BL
				//pDrmPanel->funcs->unprepare(pDrmPanel); //disable VCC
			} else if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_EXT_VIDEO) {
				crtc->active_change = 1;
			} else if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
				if (pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].pending_needs_vblank
					== true) {
					mtk_tv_drm_crtc_finish_page_flip(
						&pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC]);
					pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].pending_needs_vblank
						= false;
				}
			}
		} else {
			if (crtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
				//pDrmPanel->funcs->prepare(pDrmPanel); //enable VCC
				//pDrmPanel->funcs->enable(pDrmPanel); // enable BL
			}
		}
	} else {
	}
	return 0;
}

static int mtk_tv_kms_vsync_callback_handle_task(void *data)
{
	struct mtk_tv_kms_vsync_callback_info *info = NULL;
	bool mi_rt_enable = false;
	int ret;

	if (data == NULL) {
		DRM_ERROR("[%s][%d] Invalid input", __func__, __LINE__);
		return -EINVAL;
	}
	info = (struct mtk_tv_kms_vsync_callback_info *)data;
	mi_rt_enable = (info->mi_rt_domain != NULL) && (info->mi_rt_class != NULL);

	if (mi_rt_enable) {
#if defined(__KERNEL__) && defined(ANDROID)
		ret = MI_REALTIME_AddThread((MI_S8 *)info->mi_rt_domain,
			(MI_S8 *)info->mi_rt_class,
			(MI_S32)current->pid, NULL);
		if (ret != MI_OK) {
			DRM_ERROR("[%s][%d] mi rt add thread %s/%s failed, ret = 0x%x\n",
				__func__, __LINE__, info->mi_rt_domain, info->mi_rt_class, ret);
		}
#endif
	}

	while (!kthread_should_stop()) {
		wait_event_interruptible(info->wait_queue_head, READ_ONCE(info->task_entry));
		WRITE_ONCE(info->task_entry, 0);
		ret = info->callback_func(info->priv_data);
	}

	if (mi_rt_enable) {
#if defined(__KERNEL__) && defined(ANDROID)
		ret = MI_REALTIME_RemoveThread((MI_S8 *)info->mi_rt_domain,
			(MI_S32)current->pid);
		if (ret != MI_OK) {
			DRM_ERROR("[%s][%d] mi rt remove thread %s/%s failed, ret = 0x%x\n",
				__func__, __LINE__, info->mi_rt_domain, info->mi_rt_class, ret);
		}
#endif
	}
	kvfree(info->thread_name);
	kvfree(info->mi_rt_domain);
	kvfree(info->mi_rt_class);
	kvfree(info);
	return 0;
}

int mtk_tv_kms_register_vsync_callback(struct mtk_tv_kms_vsync_callback_param *param)
{
	struct list_head *list_node, *tmp_node;
	struct mtk_tv_kms_vsync_callback_info *info = NULL;
	struct cpumask cpu_mask = {0};
	size_t name_size = 0;

	if (param == NULL)
		return -EINVAL;

	// check input param
	if (param->thread_name == NULL) {
		DRM_ERROR("[%s:%d] The thread_name is NULL", __func__, __LINE__);
		return -EINVAL;
	}
	name_size = strlen(param->thread_name) + 1;
	list_for_each_safe(list_node, tmp_node, &mtk_tv_kms_callback_info_list) {
		info = list_entry(list_node, struct mtk_tv_kms_vsync_callback_info, list);
		if (strncmp(param->thread_name, info->thread_name, name_size) == 0) {
			DRM_INFO("[%s:%d] Thread '%s' is registered",
				__func__, __LINE__, param->thread_name);
			return 0;
		}
	}

	// setup callback info
	info = kvmalloc(sizeof(struct mtk_tv_kms_vsync_callback_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	memset(info, 0, sizeof(struct mtk_tv_kms_vsync_callback_info));
	INIT_LIST_HEAD(&info->list);
	init_waitqueue_head(&info->wait_queue_head);
	info->task_entry    = 0;
	info->priv_data     = param->priv_data;
	info->callback_func = param->callback_func;
	info->thread_name   = kstrdup(param->thread_name, GFP_KERNEL);
	info->mi_rt_domain  = kstrdup(param->mi_rt_domain_name, GFP_KERNEL);
	info->mi_rt_class   = kstrdup(param->mi_rt_class_name, GFP_KERNEL);
	info->task_worker   = kthread_create(mtk_tv_kms_vsync_callback_handle_task,
						info, "%s", info->thread_name);
	// add info into callback list
	list_add_tail(&info->list, &mtk_tv_kms_callback_info_list);

	// setup kthread's executable CPU exclude CPU 0
	cpumask_copy(&cpu_mask, cpu_online_mask);
	cpumask_clear_cpu(0, &cpu_mask);
	set_cpus_allowed_ptr(info->task_worker, &cpu_mask);

	// wake up kthread
	wake_up_process(info->task_worker);

	return 0;
}

static int mtk_tv_drm_ML_init(struct mtk_video_context *pctx_video)
{
	int ret = 0;

	// 1. display ml
	if (mtk_tv_sm_ml_init(&pctx_video->disp_ml, E_SM_ML_DISP_SYNC, E_SM_ML_UID_DISP_VIDEO,
			MTK_DRM_ML_DISP_BUF_COUNT, MTK_DRM_ML_DISP_CMD_COUNT)) {
		DRM_ERROR("[%s][%d] disp_ml init fail", __func__, __LINE__);
		return -EINVAL;
	}

	// 2. display irq ml
	if (mtk_tv_sm_ml_init(&pctx_video->disp_irq_ml, E_SM_ML_DISP_SYNC, E_SM_ML_UID_DISP_IRQ,
			MTK_DRM_ML_DISP_IRQ_BUF_COUNT, MTK_DRM_ML_DISP_IRQ_CMD_COUNT)) {
		DRM_ERROR("[%s][%d] disp_irq_ml init fail", __func__, __LINE__);
		return -EINVAL;
	}

	// 3. vgs ml
	ret = mtk_tv_sm_ml_init(&pctx_video->vgs_ml, E_SM_ML_VGS_SYNC, E_SM_ML_UID_VGS,
				MTK_DRM_ML_DISP_VGS_BUF_COUNT, MTK_DRM_ML_DISP_VGS_CMD_COUNT);
	// when E_SM_ML_VGS_SYNC does not support, use E_SM_ML_DISP_SYNC to keep flow going
	if (ret == -ENODEV) {
		ret = mtk_tv_sm_ml_init(&pctx_video->vgs_ml, E_SM_ML_DISP_SYNC,
				E_SM_ML_UID_DISP_VIDEO, MTK_DRM_ML_DISP_VGS_BUF_COUNT,
				MTK_DRM_ML_DISP_VGS_CMD_COUNT);
	}
	if (ret) {
		DRM_ERROR("[%s][%d] vgs_ml init fail", __func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static void mtk_tv_drm_gamma_init(void)
{
	struct mtk_tv_drm_metabuf metabuf = {0};

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return;
	}
	memset(metabuf.addr, 0, metabuf.size);
	mtk_tv_drm_metabuf_free(&metabuf);
}

static void mtk_tv_drm_send_frameinfo_sharememtopqu(void)
{
	struct mtk_tv_drm_metabuf metabuf = {0};
	struct msg_render_frameinfo_sharemem msg_sharemem = {0};
	struct pqu_render_frameinfo_sharemem pqu_sharemem = {0};

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf,
		MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_FRAMEINFO);
		return;
	}
	memset(metabuf.addr, 0, metabuf.size);

	msg_sharemem.address = (uintptr_t)(metabuf.addr);
	msg_sharemem.size = sizeof(struct render_pqu_frame_info);

	pqu_sharemem.address = metabuf.mmap_info.bus_addr;
	pqu_sharemem.size = sizeof(struct render_pqu_frame_info);

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_FRAMEINFO_SHAREMEM,
		&msg_sharemem, &pqu_sharemem);

	mtk_tv_drm_metabuf_free(&metabuf);
}

static void _mtk_drm_debug_init(bool isInit)
{
	if (isInit) {
		memset(&gMtkDrmDbg, 0, sizeof(struct mtk_tv_drm_debug));
		gMtkDrmDbg.change_vPlane_type =
			kzalloc(sizeof(struct cmd_change_vPlane_type), GFP_KERNEL);
		gMtkDrmDbg.update_disp_size =
			kzalloc(sizeof(struct cmd_update_disp_size), GFP_KERNEL);
		gMtkDrmDbg.set_rw_diff =
			kzalloc(sizeof(struct cmd_set_rw_diff), GFP_KERNEL);
	} else {
		kfree(gMtkDrmDbg.change_vPlane_type);
		kfree(gMtkDrmDbg.update_disp_size);
		kfree(gMtkDrmDbg.set_rw_diff);
	}
}

static int mtk_tv_kms_bind(
	struct device *dev,
	struct device *master,
	void *data)
{
#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	MI_U32 u32Ret = 0;
#endif
#endif
	int ret = 0;
	uint32_t i = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_device *drm_dev = NULL;
	struct mtk_drm_plane *primary_plane = NULL;
	struct mtk_drm_plane *cursor_plane = NULL;
	int plane_type = 0;
	struct mtk_video_context *pctx_video = NULL;
	struct task_struct  *crtc0_commit_tail_worker;
	struct task_struct  *crtc1_commit_tail_worker;
	struct sched_param crtc0_param, crtc1_param, panel_handler_param;
	struct cpumask crtc0_task_cmask, crtc1_task_cmask, panel_handler_cmask;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pctx = dev_get_drvdata(dev);
	pctx_video = &pctx->video_priv;
	memset(&pctx_video->old_layer, OLD_LAYER_INITIAL_VALUE,
		sizeof(uint8_t)*MTK_VPLANE_TYPE_MAX);

	pctx_pnl = &pctx->panel_priv;

	if (!data) {
		DRM_ERROR("[%s, %d]: drm_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	drm_dev = data;

	pctx->drm_dev = drm_dev;
	pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC].crtc_private = pctx;
	pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].crtc_private = pctx;
	pctx->crtc[MTK_DRM_CRTC_TYPE_EXT_VIDEO].crtc_private = pctx;

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_TCON;

	ret = _mtk_drm_kms_read_efuse(pctx);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_kms_read_efuse\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = _mtk_drm_kms_parse_dts(pctx);
	if (ret != 0) {
		DRM_ERROR("[%s, %d]: readDeviceTree2Context failed.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_drm_common_clk_parse_dts(dev, &pctx->clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_clk_parse_dts\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_video_display_parse_dts(pctx);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to mtk_video_display_parse_dts\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_tv_drm_global_pq_parse_dts(pctx);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to mtk_tv_drm_globa_pq_parse_dts\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_drm_common_set_xc_clock(dev, &pctx->clk, TRUE);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_common_set_xc_clock failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	pctx->total_plane_num = 0;
	for (plane_type = 0;
		plane_type < MTK_DRM_PLANE_TYPE_MAX;
		plane_type++) {

		pctx->total_plane_num += pctx->plane_num[plane_type];
	}

	pctx->ext_common_plane_properties =
		devm_kzalloc(dev,
		sizeof(struct ext_common_plane_prop_info) *
			pctx->total_plane_num, GFP_KERNEL);
	if (pctx->ext_common_plane_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_common_plane_properties,
		0,
		sizeof(struct ext_common_plane_prop_info) *
			pctx->total_plane_num);

	pctx->ext_crtc_properties = devm_kzalloc(
		dev,
		sizeof(struct ext_crtc_prop_info),
		GFP_KERNEL);

	if (pctx->ext_crtc_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_crtc_properties, 0, sizeof(struct ext_crtc_prop_info));

	pctx->ext_crtc_graphic_properties = devm_kzalloc(
		dev,
		sizeof(struct ext_crtc_graphic_prop_info),
		GFP_KERNEL);
	if (pctx->ext_crtc_graphic_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_crtc_graphic_properties,
		0,
		sizeof(struct ext_crtc_graphic_prop_info));

	pctx->ext_common_crtc_properties = devm_kzalloc(
		dev,
		sizeof(struct ext_common_crtc_prop_info) * MTK_DRM_CRTC_TYPE_MAX,
		GFP_KERNEL);
	if (pctx->ext_common_crtc_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_common_crtc_properties,
		0,
		sizeof(struct ext_common_crtc_prop_info));

	pctx->ext_connector_properties = devm_kzalloc(
		dev,
		sizeof(struct ext_connector_prop_info),
		GFP_KERNEL);
	if (pctx->ext_connector_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_connector_properties,
		0,
		sizeof(struct ext_connector_prop_info));

	pctx->ext_common_connector_properties = devm_kzalloc(
		dev,
		sizeof(struct ext_common_connector_prop_info) * MTK_DRM_CONNECTOR_TYPE_MAX,
		GFP_KERNEL);
	if (pctx->ext_common_connector_properties == NULL) {
		DRM_ERROR("[%s, %d]: devm_kzalloc failed.\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx->ext_common_connector_properties,
		0,
		sizeof(struct ext_common_connector_prop_info));

	ret = mtk_tv_drm_ML_init(pctx_video);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_video_drm_ML_init  failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_drm_autogen_init(&pctx->autogen_priv);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_autogen_init failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_drm_pqu_metadata_init(&pctx->pqu_metadata_priv);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_pqu_metadata_init failed.\n", __func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_drm_global_pq_init(&pctx->global_pq_priv);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_global_pq_init failed.\n", __func__, __LINE__);
		goto ERR;
	}

	set_panel_context(pctx);

	ret = mtk_tv_drm_demura_init(&pctx->demura_priv, pctx_pnl);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_demura_init failed.\n", __func__, __LINE__);
		goto ERR;
	}

	ret = mtk_video_display_init(dev, master, data, &primary_plane, NULL);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_video_display_init  failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_gop_init(dev, master, data, NULL, &cursor_plane);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_gop_init failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_mod_clk_init(dev);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_mod_clk_init failed.\n",
			__func__, __LINE__);
		return ret;
	}

#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	ret = mtk_tvdac_init(dev);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tvdac_init  failed.\n",
			__func__, __LINE__);
		goto ERR;
	}
#endif

	ret = mtk_ldm_init(dev);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_ldm_init failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	/* trigger gen info lock */
	spin_lock_init(&pctx->tg_lock);
	pctx->trigger_gen_info.IsGamingMJCOn = false;
	spin_lock_init(&synclockdebugVTT);

	ret = mtk_tv_drm_InitMODInterrupt();
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_InitMODInterrupt  failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_COMMON_PLANE);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = request_threaded_irq(
		pctx->irq.video_irq_num,
		mtk_tv_kms_irq_top_handler,
		mtk_tv_kms_irq_bottom_handler,
		IRQF_SHARED | IRQF_ONESHOT,
		"DRM_MTK_VIDEO",
		pctx);

	if (ret) {
		DRM_ERROR("[%s, %d]: irq request failed.\n",
			__func__, __LINE__);
		goto ERR;
	}
	irq_set_affinity_hint(pctx->irq.video_irq_num, cpumask_of(pctx->irq.video_irq_affinity));

	ret = devm_request_irq(
		dev,
		pctx->irq.graphic_irq_vgsep_num,
		mtk_tv_graphic_path_irq_handler,
		IRQF_SHARED,
		"DRM_MTK_GRAPHIC",
		pctx);
	if (ret) {
		DRM_ERROR("[%s, %d]: irq request failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = request_threaded_irq(
		pctx->irq.mod_detect_irq_num,
		mtk_tv_mod_detect_irq_top_handler,
		mtk_tv_mod_detect_irq_bottom_handler,
		IRQF_SHARED | IRQF_ONESHOT,
		"DRM_MTK_MOD_DETECT",
		pctx);

	if (ret) {
		DRM_ERROR("[%s, %d]: irq request failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	for (i = 0; i < MTK_DRM_CRTC_TYPE_MAX; i++) {
		ret = mtk_tv_drm_crtc_create(
		drm_dev,
		&pctx->crtc[i],
		&primary_plane->base,
		&cursor_plane->base,
		&mtk_tv_kms_crtc_ops);
		if (i == MTK_DRM_CRTC_TYPE_VIDEO)
			pctx->crtc[i].mtk_crtc_type = MTK_DRM_CRTC_TYPE_VIDEO;
		else if (i == MTK_DRM_CRTC_TYPE_GRAPHIC)
			pctx->crtc[i].mtk_crtc_type = MTK_DRM_CRTC_TYPE_GRAPHIC;
		else if (i == MTK_DRM_CRTC_TYPE_EXT_VIDEO)
			pctx->crtc[i].mtk_crtc_type = MTK_DRM_CRTC_TYPE_EXT_VIDEO;
	}

	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: crtc create failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	// create extend crtc properties
	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_CRTC);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_CRTC_GRAPHIC);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_CRTC_COMMON);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	for (i = 0; i < MTK_DRM_ENCODER_TYPE_MAX; i++) {
		if (mtk_tv_drm_encoder_create(
			drm_dev,
			&pctx->encoder[i],
			pctx->crtc) != 0) {

			DRM_ERROR("[%s, %d]: mtk_tv_drm_encoder_create fail.\n",
				__func__, __LINE__);
			goto ERR;
		}
	}


	for (i = 0; i < MTK_DRM_CONNECTOR_TYPE_MAX; i++) {
		if (i == MTK_DRM_CONNECTOR_TYPE_VIDEO) {
			pctx->connector[i].mtk_connector_type = MTK_DRM_CONNECTOR_TYPE_VIDEO;
			if ( mtk_tv_drm_connector_create(
				pctx->drm_dev,
				&pctx->connector[i],
				&pctx->encoder[i],
				&mtk_tv_kms_connector_ops) != 0) {
				DRM_ERROR("[%s, %d]: mtk_tv_drm_connector_create fail.\n",
					__func__, __LINE__);
				goto ERR;
			}
		} else if (i == MTK_DRM_CONNECTOR_TYPE_GRAPHIC) {
			pctx->connector[i].mtk_connector_type = MTK_DRM_CONNECTOR_TYPE_GRAPHIC;
			if ( mtk_tv_drm_connector_create(
				pctx->drm_dev,
				&pctx->connector[i],
				&pctx->encoder[i],
				&mtk_tv_kms_connector_ops) != 0) {
				DRM_ERROR("[%s, %d]: mtk_tv_drm_connector_create fail.\n",
					__func__, __LINE__);
				goto ERR;
			}
		} else if (i == MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO) {
			pctx->connector[i].mtk_connector_type = MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO;
			if ( mtk_tv_drm_connector_create(
				pctx->drm_dev,
				&pctx->connector[i],
				&pctx->encoder[i],
				&mtk_tv_kms_connector_ops) != 0) {
				DRM_ERROR("[%s, %d]: mtk_tv_drm_connector_create fail.\n",
					__func__, __LINE__);
				goto ERR;
			}
		}
	}
	//if ( mtk_tv_drm_connector_create(
		//pctx->drm_dev,
		//&pctx->connector,
		//&pctx->encoder,
		//&mtk_tv_kms_connector_ops) != 0) {
		//DRM_ERROR("[%s, %d]: mtk_tv_drm_connector_create fail.\n",
			//__func__, __LINE__);
		//goto ERR;
	//}

	// create extend connector properties
	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_CONNECTOR);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	ret = mtk_tv_kms_create_ext_props(pctx, E_EXT_PROP_TYPE_CONNECTOR_COMMON);
	if (ret) {
		DRM_ERROR("[%s, %d]: kms create ext prop failed\n",
			__func__, __LINE__);
		goto ERR;
	}

	//for pixelshift
	ret = mtk_video_pixelshift_init(dev);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_video_pixelshift_init failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	mtk_video_display_set_panel_color_format(pctx_pnl);

	_mtk_drm_debug_init(true);
	mtk_tv_drm_gamma_init();
	mtk_tv_drm_send_frameinfo_sharememtopqu();
	mtk_tv_drm_trigger_gen_init(pctx, false);

#if defined(__KERNEL__) /* Linux Kernel */
#if (defined ANDROID)
	/*create Realtime Framework Domain*/
	// Please use E_MI_REALTIME_NORMAL, do not modify.
	u32Ret = MI_REALTIME_Init(E_MI_REALTIME_NORMAL);
	if (u32Ret != E_MI_REALTIME_RET_OK) {
		DRM_ERROR("%s: %d MI_REALTIME_Init failed. ret=%u\n",
			__func__, __LINE__, u32Ret);
	}

	// create domain
	u32Ret = MI_REALTIME_CreateDomain((MI_S8 *)"Domain_Render_DD");
	if (u32Ret != MI_OK) {
		DRM_ERROR("%s: %d MI_REALTIME_CreateDomain failed. ret=%u\n",
			__func__, __LINE__, u32Ret);
	}
#endif
#endif

	/*create commit_tail_thread*/
	crtc0_commit_tail_worker = kthread_create(mtk_commit_tail, pctx,
						"crtc0_thread");
	crtc0_param.sched_priority = MTK_DRM_COMMIT_TAIL_THREAD_PRIORITY;
	sched_setscheduler(crtc0_commit_tail_worker, SCHED_RR, &crtc0_param);

	cpumask_copy(&crtc0_task_cmask, cpu_online_mask);
	cpumask_clear_cpu(0, &crtc0_task_cmask);
	set_cpus_allowed_ptr(crtc0_commit_tail_worker, &crtc0_task_cmask);
	wake_up_process(crtc0_commit_tail_worker);

	//create commit_tail_thread for graphic crtc
	if (pctx->out_model != E_OUT_MODEL_VG_BLENDED) {
		crtc1_commit_tail_worker = kthread_create(mtk_commit_tail_gCrtc, pctx,
							"crtc1_thread");
		crtc1_param.sched_priority = MTK_DRM_COMMIT_TAIL_THREAD_PRIORITY;
		sched_setscheduler(crtc1_commit_tail_worker, SCHED_RR, &crtc1_param);
		cpumask_copy(&crtc1_task_cmask, cpu_online_mask);
		cpumask_clear_cpu(0, &crtc1_task_cmask);
		set_cpus_allowed_ptr(crtc1_commit_tail_worker, &crtc1_task_cmask);
		wake_up_process(crtc1_commit_tail_worker);
	}

	//create panel task handler thread
	panel_handler_task = kthread_create(mtk_tv_drm_panel_task_handler, pctx,
						"panel_task_thread");
	panel_handler_param.sched_priority = MTK_DRM_COMMIT_TAIL_THREAD_PRIORITY;
	sched_setscheduler(panel_handler_task, SCHED_RR, &panel_handler_param);

	cpumask_copy(&panel_handler_cmask, cpu_online_mask);
	cpumask_clear_cpu(0, &panel_handler_cmask);
	set_cpus_allowed_ptr(panel_handler_task, &panel_handler_cmask);
	wake_up_process(panel_handler_task);

	return 0;

ERR:
	clean_tv_kms_context(dev, pctx);
	return ret;
}

static void mtk_tv_kms_unbind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_tv_kms_context *pctx = NULL;

	pctx = dev_get_drvdata(dev);

	mtk_video_display_unbind(dev);

	//stop panel task handler thread
	kthread_stop(panel_handler_task);
}

static const struct component_ops mtk_tv_kms_component_ops = {
	.bind	= mtk_tv_kms_bind,
	.unbind = mtk_tv_kms_unbind,
};

static const struct of_device_id mtk_drm_tv_kms_of_ids[] = {
	{ .compatible = "MTK-drm-tv-kms", },
	{},
};

static int mtk_drm_tv_kms_suspend(
	struct platform_device *dev,
	pm_message_t state)
{
	int ret = 0;
	struct device *pdev = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	bool IRQmask = 0;
	enum sm_return_type type = E_SM_RET_FAIL;

	if (!dev) {
		DRM_ERROR("[%s, %d]: platform_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pdev = &dev->dev;
	pctx = dev_get_drvdata(pdev);
	pctx_video = &pctx->video_priv;

	ret = mtk_common_IRQ_get_mask(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO],
		MTK_VIDEO_IRQTYPE_HK,
		MTK_VIDEO_SW_IRQ_TRIG_DISP,
		&IRQmask,
		pctx_video->video_hw_ver.irq);

	pctx->irq.irq_mask = IRQmask;
	ret |= mtk_drm_gop_suspend(dev, state);
	ret |= mtk_common_irq_suspend();
	ret |= mtk_video_display_suspend(dev, state);
#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	ret |= mtk_tvdac_cvbso_suspend(dev);
#endif
	ret |= mtk_ldm_suspend(pdev);
	ret |= mtk_tv_drm_demura_suspend(&pctx->demura_priv);
	ret |= mtk_tv_drm_pqu_metadata_suspend(&pctx->pqu_metadata_priv);
	ret |= mtk_tv_drm_autogen_suspend(&pctx->autogen_priv);
	ret |= mtk_tv_drm_metabuf_suspend();
	ret |= mtk_video_pixelshift_suspend(dev);
	//destroy the AUL instance
	if (pctx->amb_context.ambient_light_init) {
		type = sm_aul_destroy_resource(pctx->amb_context.amb_aul_instance);
		DRM_INFO("[%s][%d] Distory aul resource %d!!\n", __func__, __LINE__
			, type);
	}

	return ret;
}

static int mtk_drm_tv_kms_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_video_context *pctx_video = &pctx->video_priv;

	irq_set_affinity_hint(pctx->irq.video_irq_num, cpumask_of(pctx->irq.video_irq_affinity));

	// First, resume basic utility
	ret = mtk_drm_common_set_xc_clock(dev, &pctx->clk, TRUE);
	ret |= mtk_common_irq_resume();
	ret |= mtk_tv_drm_InitMODInterrupt();
	ret |= mtk_tv_drm_metabuf_resume();
	ret |= mtk_tv_drm_autogen_resume(&pctx->autogen_priv);
	ret |= mtk_tv_drm_pqu_metadata_resume(&pctx->pqu_metadata_priv);

	// Second, resume drm component
	ret |= mtk_ldm_resume(dev); //need to turn on backlight first
	ret |= mtk_drm_gop_resume(pdev);
	ret |= mtk_video_display_resume(pdev);
#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	ret |= mtk_tvdac_cvbso_resume(pdev);
#endif

	// third, resume drm pq
	ret |= mtk_video_pqgamma_resume(pctx);
	ret |= mtk_video_panel_gamma_resume(pctx);
	ret |= mtk_tv_drm_demura_resume(&pctx->demura_priv);
	ret |= mtk_video_pixelshift_resume(pdev);

	if (pctx->irq.irq_mask == 1) {
		ret |= mtk_common_IRQ_set_mask(
			NULL, MTK_VIDEO_IRQTYPE_HK, MTK_VIDEO_SW_IRQ_TRIG_DISP,
			pctx_video->video_hw_ver.irq);
	} else {
		ret |= mtk_common_IRQ_set_unmask(
			NULL, MTK_VIDEO_IRQTYPE_HK, MTK_VIDEO_SW_IRQ_TRIG_DISP,
			pctx_video->video_hw_ver.irq);
	}

	mtk_tv_drm_send_frameinfo_sharememtopqu();

	mtk_render_set_tcon_pattern_en(false);
	ret |= mtk_drm_pattern_resume(&pctx->pattern_info);
	mtk_video_display_set_ambient_framesize(pctx_video, TRUE);
	mtk_video_display_ambient_AUL_init(pctx);

	return ret;
}

static void mtk_tv_drm_kms_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_tv_drm_kms_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%d]Fail to create sysfs files: %d\n", __LINE__, ret);
	}
	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_tv_drm_kms_attr_panel_group);
	if (ret) {
		dev_err(&pdev->dev, "[%d]Fail to create panel sysfs files: %d\n", __LINE__, ret);
	}
	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_tv_drm_kms_attr_capability_group);
	if (ret) {
		dev_err(&pdev->dev, "[%d]Fail to create capability sysfs files: %d\n",
			__LINE__, ret);
	}
}

static void mtk_tv_drm_kms_remove_sysfs(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove device attribute files");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_tv_drm_kms_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &mtk_tv_drm_kms_attr_panel_group);
}

static int mtk_drm_tv_kms_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_tv_kms_context *pctx;
	int ret;

	pctx = devm_kzalloc(dev, sizeof(*pctx), GFP_KERNEL);
	if (!pctx) {
		ret = -ENOMEM;
		goto ERR;
	}
	memset(pctx, 0x0, sizeof(struct mtk_tv_kms_context));

	pctx->dev = dev;

	platform_set_drvdata(pdev, pctx);

	ret = component_add(dev, &mtk_tv_kms_component_ops);
	if (ret) {
		DRM_ERROR("[%s, %d]: component_add fail\n",
			__func__, __LINE__);
		goto ERR;
	}

	bPquEnable |= of_property_read_bool(dev->of_node, "rv55_boot");

	mtk_tv_drm_kms_create_sysfs(pdev);
	mtk_drm_common_clk_create_sysfs(pdev);
	mtk_video_display_create_sysfs(pdev);
	mtk_tv_drm_global_pq_create_sysfs(pdev);
	mtk_drm_ldm_create_sysfs(pdev);
	mtk_drm_pattern_create_sysfs(pdev);
	mtk_tv_drm_metabuf_init();
	return 0;
ERR:
	if (pctx != NULL)
		devm_kfree(dev, pctx);
	return ret;
}

static int mtk_drm_tv_kms_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_video_context *pctx_video = &pctx->video_priv;

	mtk_tv_drm_kms_remove_sysfs(pdev);
	mtk_drm_common_clk_remove_sysfs(pdev);
	mtk_tv_drm_global_pq_remove_sysfs(pdev);
	mtk_drm_ldm_remove_sysfs(pdev);

	if (pctx) {
		//destroy gop ml resource
		if (mtk_gop_ml_destroy_resource(&pctx->gop_priv)) {
			DRM_ERROR("[%s][%d] mtk_gop_ml_destroy_resource fail",
			__func__, __LINE__);
		}

		clean_gop_context(dev, &pctx->gop_priv);

		//5.destroy resource
		if (mtk_tv_sm_ml_deinit(&pctx_video->disp_ml))
			DRM_ERROR("[%s][%d] disp_ml destroy fail", __func__, __LINE__);
		if (mtk_tv_sm_ml_deinit(&pctx_video->disp_irq_ml))
			DRM_ERROR("[%s][%d] disp_irq_ml destroy fail", __func__, __LINE__);
		if (mtk_tv_sm_ml_deinit(&pctx_video->vgs_ml))
			DRM_ERROR("[%s][%d] vgs ml destroy fail", __func__, __LINE__);
	}

	_mtk_drm_debug_init(false);
	return 0;
}

struct platform_driver mtk_drm_tv_kms_driver = {
	.probe = mtk_drm_tv_kms_probe,
	.remove = mtk_drm_tv_kms_remove,
	.suspend = mtk_drm_tv_kms_suspend,
	.resume = mtk_drm_tv_kms_resume,
	.driver = {
		.name = "mediatek-drm-tv-kms",
		.of_match_table = mtk_drm_tv_kms_of_ids,
	},
};
