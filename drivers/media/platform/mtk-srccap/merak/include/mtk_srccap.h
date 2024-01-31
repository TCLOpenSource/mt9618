/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_H
#define MTK_SRCCAP_H

#include <linux/platform_device.h>
#include <linux/mtk-v4l2-srccap.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include "drvXC_HDMI_if.h"
#include "mtk_srccap_common.h"
#include "mtk_srccap_dscl.h"
#include "mtk_srccap_hdmirx.h"
#include "mtk_srccap_adc.h"
#include "mtk_srccap_avd.h"
#include "mtk_srccap_mux.h"
#include "mtk_srccap_timingdetect.h"
#include "mtk_srccap_memout.h"
#include "mtk_srccap_vbi.h"
#include "mtk_srccap_dv.h"
#include "mtk_srccap_pattern.h"

/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
#define SYMBOL_WEAK __attribute__((weak))
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif
#define SRCCAP_NAME "mtk_srccap"
#define SRCCAP_LABEL "[SRCCAP] "
#define SRCCAP_DEV_NUM (2)
#define SRCCAP_UNSPECIFIED_NODE_NUM (0xFF)
#define SRCCAP_VIDEO_DEV_NUM_BASE (3)
#define SRCCAP_EVENTQ_SIZE (8)
#define SRCCAP_MAX_PLANE_NUM (3)
#define SRCCAP_NSEC_PER_USEC (1000)
#define SRCCAP_USEC_PER_SEC (1000000)
#define SRCCAP_BYTE_SIZE (8)
#define SRCCAP_RIU_OFFSET (0x1C000000)
#define SRCCAP_IRQ_MAX_NUM (16)
#define SRCCAP_DEVICE_CAPS (V4L2_CAP_VIDEO_CAPTURE_MPLANE|\
				V4L2_CAP_VIDEO_M2M_MPLANE|\
				V4L2_CAP_STREAMING)
#define SRCCAP_HDMI_VS_PACKET_TYPE (0x81)
#define SRCCAP_HDMI_VSIF_PACKET_IEEE_OUI_HRD10_PLUS (0x90848B)

#define SRCCAP_DRIVER0_NAME "mtk-srccap0"
#define SRCCAP_DRIVER1_NAME "mtk-srccap1"
#define VBI_BUS_OFFSET      0x20000000
#define COMB_BUS_OFFSET     0x20000000

#define SRCCAP_STRING_SIZE_10 (10)
#define SRCCAP_STRING_SIZE_20 (20)
#define SRCCAP_STRING_SIZE_50 (50)
#define SRCCAP_STRING_SIZE_60 (60)
#define SRCCAP_STRING_SIZE_65535 (65535)
#define SRCCAP_MENULOAD_CMD_COUNT (100)
#define SRCCAP_YUV420_WIDTH_DIVISOR (2)
#define SRCCAP_INTERLACE_HEIGHT_DIVISOR (2)
#define SRCCAP_SHIFT_NUM_24 (24)
#define SRCCAP_SHIFT_NUM_16 (16)
#define SRCCAP_SHIFT_NUM_8 (8)
#define SRCCAP_MAX_VSIF_PACKET_NUMBER (4)
#define SRCCAP_MAX_EMP_PACKET_NUMBER (24)
#define MAX_DRM_PACKET_NUMBER (1)
#define SRCCAP_ML_CMD_SIZE (8)
#define SRCCAP_MAX_INPUT_SOURCE_PORT_NUM (25)
#define SRCCAP_HW_VERSION_4 (4)
#define SRCCAP_HW_VERSION_5 (5)
#define SRCCAP_HW_VERSION_6 (6)
#define SRCCAP_PORT_TYPE_NUM (2)
#define SRCCAP_FDET_VTT_NUM (4)

#define SRCCAP_UDELAY_1000 (1000)
#define SRCCAP_UDELAY_1100 (1100)
#define SRCCAP_TIMING_VTOTAL_UNITS (1000)
#define SRCCAP_BANK_REFRESH_MDELAY_60 (60)
#define SRCCAP_U8_MAX (0xFF)
#define SRCCAP_24FPS_FRAME_TIME (42)
#define SRCCAP_TWO_VSYNC (2)
#define SRCCAP_THREE_VSYNC (3)
#define LOG_LEVEL_MASK		(0xFFFF)
#define LOG_LEVEL_OFF		(0)
#define LOG_LEVEL_NORMAL	(1)
#define LOG_LEVEL_FATAL		(2)
#define LOG_LEVEL_ERROR		(3)
#define LOG_LEVEL_WARNING	(4)
#define LOG_LEVEL_INFO		(5)
#define LOG_LEVEL_DEBUG		(6)

#define FMM_VSIF_PKT_TYPE (0x01)
#define FMM_VSIF_VERSION (0x01)
#define FMM_VSIF_LENGTH (0x05)
#define FMM_VSIF_PAYLOAD_CONTENT_TYPE (0x01)
#define FMM_VSIF_PAYLOAD_CONTENT_SUBTYPE (0x00)
#define FMM_VSIF_IEEE_OUI_LENGTH (0x03)
#define FMM_VSIF_IEEE_OUI_VALUE (0xFBBB1A)
#define FMM_AVI_ITC_VALUE (0x01)
#define FMM_AVI_CN10_VALUE (0x02)

#define	ALG_CAPABILITY_SIZE (2)
#define SRCCAP_SHM_QUE_NUM (32)

#define	SRC_DETECT_MAX_NUM (6)
#define	SRC_DETECT_GPIO_OFFSET (9)
#define	SRC_DETECT_EN_OFFSET (63)

#define SRCCAP_USLEEP_RANGE_TOLERANCE (100)
#define SRCCAP_TASK_SLEEP_4000 (4000) // use in usleep for 4ms
#define SRCCAP_TASK_SLEEP_4100 (4000 + SRCCAP_USLEEP_RANGE_TOLERANCE)
#define SRCCAP_RETRIEVE_TIMING_SLEEP_BEGIN (40000) // using in usleep as 40ms
#define SRCCAP_RETRIEVE_TIMING_SLEEP_END (40000 + SRCCAP_USLEEP_RANGE_TOLERANCE) //40.1 ms
#define SRCCAP_VRR_SRC_NUM (4) //HDMI Port 0~3
#define SRCCAP_IS_HDMI_SRC(src) (src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI && \
	src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)
#define SRCCAP_IS_DVI_SRC(src) (src >= V4L2_SRCCAP_INPUT_SOURCE_DVI && \
	src <= V4L2_SRCCAP_INPUT_SOURCE_DVI4)

#define SRCCAP_FORCE_SINGLE_BUFFER_MODE (0)

#define SRCCAP_FORCE_SVIDEO (0)
#define SRCCAP_FORCE_SCART (0)
#define SRCCAP_FORCE_VGA (0)
/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
extern int atrace_enable_srccap;
#endif
extern int log_level;

extern int boottime_print(const char *s);

#define SRCCAP_LOG(_dbgLevel, _fmt, _args...) \
	do { \
		switch (_dbgLevel) { \
		case LOG_LEVEL_FATAL: \
			if ((log_level & LOG_LEVEL_MASK) == LOG_LEVEL_NORMAL)  \
				pr_crit(SRCCAP_LABEL "[fatal] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			else if ((log_level & LOG_LEVEL_MASK) >= LOG_LEVEL_FATAL) \
				pr_emerg(SRCCAP_LABEL "[fatal] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			break; \
		case LOG_LEVEL_ERROR: \
			if ((log_level & LOG_LEVEL_MASK) == LOG_LEVEL_NORMAL)  \
				pr_err(SRCCAP_LABEL "[error] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			else if ((log_level & LOG_LEVEL_MASK) >= LOG_LEVEL_ERROR) \
				pr_emerg(SRCCAP_LABEL "[error] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			break; \
		case LOG_LEVEL_WARNING: \
			if ((log_level & LOG_LEVEL_MASK) == LOG_LEVEL_NORMAL)  \
				pr_warn(SRCCAP_LABEL "[warning] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			else if ((log_level & LOG_LEVEL_MASK) >= LOG_LEVEL_WARNING) \
				pr_emerg(SRCCAP_LABEL "[warning] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			break; \
		case LOG_LEVEL_INFO: \
			if ((log_level & LOG_LEVEL_MASK) == LOG_LEVEL_NORMAL)  \
				pr_info(SRCCAP_LABEL "[info] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			else if ((log_level & LOG_LEVEL_MASK) >= LOG_LEVEL_INFO) \
				pr_emerg(SRCCAP_LABEL "[info] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			break; \
		case LOG_LEVEL_DEBUG: \
			if ((log_level & LOG_LEVEL_MASK) == LOG_LEVEL_NORMAL)  \
				pr_debug(SRCCAP_LABEL "[debug] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			else if ((log_level & LOG_LEVEL_MASK) >= LOG_LEVEL_DEBUG) \
				pr_emerg(SRCCAP_LABEL "[debug] [%s,%5d]"_fmt, \
					__func__, __LINE__, ##_args); \
			break; \
		default: \
			break; \
		} \
	} while (0)

#define SRCCAP_MSG_DEBUG(_format, _args...) \
			SRCCAP_LOG(LOG_LEVEL_DEBUG, _format, ##_args)
#define SRCCAP_MSG_INFO(_format, _args...) \
			SRCCAP_LOG(LOG_LEVEL_INFO, _format, ##_args)
#define SRCCAP_MSG_WARNING(_format, _args...) \
			SRCCAP_LOG(LOG_LEVEL_WARNING, _format, ##_args)
#define SRCCAP_MSG_ERROR(_format, _args...) \
			SRCCAP_LOG(LOG_LEVEL_ERROR, _format, ##_args)
#define SRCCAP_MSG_FATAL(_format, _args...) \
			SRCCAP_LOG(LOG_LEVEL_FATAL, _format, ##_args)

#define SRCCAP_MSG_POINTER_CHECK() \
	do { \
		pr_err(SRCCAP_LABEL"[%s,%d] pointer is NULL\n", __func__, __LINE__); \
	} while (0)

#define SRCCAP_MSG_RETURN_CHECK(_result) \
	do { \
		if (_result < 0) \
			SRCCAP_MSG_ERROR("error return is %d\n", (_result)); \
		else \
			SRCCAP_MSG_INFO("info return is %d\n", (_result)); \
	} while (0)



#define SRCCAP_GET_JIFFIES_FOR_N_MS(_ms, _jiffies) \
	do { \
		if ((((_ms) * HZ / 1000) == 0) || (((_ms) * HZ % 1000) > 0)) \
			*(_jiffies) = ((_ms) * HZ / 1000) + 1; \
		else \
			*(_jiffies) = ((_ms) * HZ / 1000); \
	} while (0)

#define SRCCAP_GET_TIMESTAMP(_tv) \
	do { \
		struct timespec ts; \
		ktime_get_ts(&ts); \
		((struct timeval *)(_tv))->tv_sec = ts.tv_sec; \
		((struct timeval *)(_tv))->tv_usec = ts.tv_nsec / SRCCAP_NSEC_PER_USEC; \
	} while (0)

#define SRCCAP_GET_LAST_BUF_ENTRY(_head, _entry) \
	do { \
		(_entry) = NULL; \
		if (((_head) == NULL) || ((_head)->next == NULL)) { \
			SRCCAP_MSG_POINTER_CHECK(); \
			break; \
		} \
		if ((_head)->next == (_head)) { \
			SRCCAP_MSG_ERROR("No entries in queue!\n"); \
			break; \
		} \
		(_entry) = list_first_entry((_head), \
			struct srccap_memout_buffer_entry, list); \
	} while (0)

#define SRCCAP_IS_VD_SRC(_src) \
	(((_src >= V4L2_SRCCAP_INPUT_SOURCE_CVBS) && (_src <= V4L2_SRCCAP_INPUT_SOURCE_CVBS8)) \
	|| ((_src >= V4L2_SRCCAP_INPUT_SOURCE_SVIDEO) \
		&& (_src <= V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4)) \
	|| (_src == V4L2_SRCCAP_INPUT_SOURCE_ATV) \
	|| ((_src >= V4L2_SRCCAP_INPUT_SOURCE_SCART) && (_src <= V4L2_SRCCAP_INPUT_SOURCE_SCART2)))

#define MALLOC(size)	((size >= PAGE_SIZE) ? vmalloc(size) : kmalloc(size, GFP_KERNEL))
#define FREE_MEM(ptr, size)			\
	do {						\
		if (size >= PAGE_SIZE)	\
			vfree(ptr);			\
		else					\
			kfree(ptr);			\
	} while (0)

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
enum srccap_access_type {
	SRCCAP_SET = 0,
	SRCCAP_GET,
};

enum srccap_input_port {
	SRCCAP_INPUT_PORT_NONE = 0,
	SRCCAP_INPUT_PORT_RGB0_DATA = 10,
	SRCCAP_INPUT_PORT_RGB1_DATA,
	SRCCAP_INPUT_PORT_RGB2_DATA,
	SRCCAP_INPUT_PORT_RGB3_DATA,
	SRCCAP_INPUT_PORT_RGB4_DATA,
	SRCCAP_INPUT_PORT_RGB0_SYNC = 30,
	SRCCAP_INPUT_PORT_RGB1_SYNC,
	SRCCAP_INPUT_PORT_RGB2_SYNC,
	SRCCAP_INPUT_PORT_RGB3_SYNC,
	SRCCAP_INPUT_PORT_RGB4_SYNC,
	SRCCAP_INPUT_PORT_YCVBS0 = 50,
	SRCCAP_INPUT_PORT_YCVBS1,
	SRCCAP_INPUT_PORT_YCVBS2,
	SRCCAP_INPUT_PORT_YCVBS3,
	SRCCAP_INPUT_PORT_YCVBS4,
	SRCCAP_INPUT_PORT_YCVBS5,
	SRCCAP_INPUT_PORT_YCVBS6,
	SRCCAP_INPUT_PORT_YCVBS7,
	SRCCAP_INPUT_PORT_YG0 = 70,
	SRCCAP_INPUT_PORT_YG1,
	SRCCAP_INPUT_PORT_YG2,
	SRCCAP_INPUT_PORT_YR0,
	SRCCAP_INPUT_PORT_YR1,
	SRCCAP_INPUT_PORT_YR2,
	SRCCAP_INPUT_PORT_YB0,
	SRCCAP_INPUT_PORT_YB1,
	SRCCAP_INPUT_PORT_YB2,
	SRCCAP_INPUT_PORT_CCVBS0 = 90,
	SRCCAP_INPUT_PORT_CCVBS1,
	SRCCAP_INPUT_PORT_CCVBS2,
	SRCCAP_INPUT_PORT_CCVBS3,
	SRCCAP_INPUT_PORT_CCVBS4,
	SRCCAP_INPUT_PORT_CCVBS5,
	SRCCAP_INPUT_PORT_CCVBS6,
	SRCCAP_INPUT_PORT_CCVBS7,
	SRCCAP_INPUT_PORT_CR0 = 110,
	SRCCAP_INPUT_PORT_CR1,
	SRCCAP_INPUT_PORT_CR2,
	SRCCAP_INPUT_PORT_CG0,
	SRCCAP_INPUT_PORT_CG1,
	SRCCAP_INPUT_PORT_CG2,
	SRCCAP_INPUT_PORT_CB0,
	SRCCAP_INPUT_PORT_CB1,
	SRCCAP_INPUT_PORT_CB2,
	SRCCAP_INPUT_PORT_DVI0 = 130,
	SRCCAP_INPUT_PORT_DVI1,
	SRCCAP_INPUT_PORT_DVI2,
	SRCCAP_INPUT_PORT_DVI3,
	SRCCAP_INPUT_PORT_HDMI0 = 140,
	SRCCAP_INPUT_PORT_HDMI1,
	SRCCAP_INPUT_PORT_HDMI2,
	SRCCAP_INPUT_PORT_HDMI3,
	SRCCAP_INPUT_PORT_NUM,
};

/* refer to mtk_srccap_test.h: enum srccap_stub_mode */
enum srccap_stub_mode {
	SRCCAP_STUB_MODE_OFF = 0,
	SRCCAP_STUB_MODE_ON_COMMON,
	SRCCAP_STUB_MODE_DV = 10,
	SRCCAP_STUB_MODE_DV_VSIF_H14B,		// 11
	SRCCAP_STUB_MODE_DV_VSIF_STD,		// 12
	SRCCAP_STUB_MODE_DV_VSIF_LL_RGB,	// 13
	SRCCAP_STUB_MODE_DV_VSIF_LL_YUV,	// 14
	SRCCAP_STUB_MODE_DV_EMP_FORM1,		// 15
	SRCCAP_STUB_MODE_DV_EMP_FORM2_RGB,	// 16
	SRCCAP_STUB_MODE_DV_EMP_FORM2_YUV,	// 17
	SRCCAP_STUB_MODE_DV_DRM_LL_RGB,		// 18
	SRCCAP_STUB_MODE_DV_DRM_LL_YUV,		// 19
	SRCCAP_STUB_MODE_DV_VSIF_NEG,		// 20
	SRCCAP_STUB_MODE_DV_EMP_NEG,		// 21
	SRCCAP_STUB_MODE_DV_FORCE_IRQ,		// 22
	SRCCAP_STUB_MODE_DV_MAX,
	SRCCAP_STUB_MODE_CFD_SDR_RGB_RGB = 30,
	SRCCAP_STUB_MODE_CFD_SDR_YUV_YUV,
	SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV,
	SRCCAP_STUB_MODE_CFD_HLG_YUV_RGB,
	SRCCAP_STUB_MODE_CFD_LEGACY_RGB_YUV,
	SRCCAP_STUB_MODE_CFD_HDR_VIVID_HDR,
	SRCCAP_STUB_MODE_CFD_HDR_VIVID_HLG,
	SRCCAP_STUB_MODE_CFD_LEGACY_RGB_YUV422,
	SRCCAP_STUB_MODE_CFD_LEGACY_RGB_YUV420,
	SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV422,
	SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV420,
	SRCCAP_STUB_MODE_CFD_MAX,
	SRCCAP_STUB_MODE_NUM,
};

enum srccap_realtime_thread {
	SRCCAP_TIMING_MONITOR_TASK = 0,
	SRCCAP_BUFFER_HADLING_TASK,
	SRCCAP_SYNC_MONITOR_TASK,
	SRCCAP_PRE_SIGNAL_TASK,
};

enum srccap_report_info_module {
	SRCCAP_REPORT_INFO_ALL = 0x0,
	SRCCAP_REPORT_INFO_MEMOUT = 0x1,
	SRCCAP_REPORT_INFO_TIMINGDETECT = 0x2,
	SRCCAP_REPORT_INFO_DSCL = 0x4,
	SRCCAP_REPORT_INFO_COMMON = 0x8,
	SRCCAP_REPORT_INFO_NUM = 0xFF,
};


/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct srccap_clock {
	/* device 0 only */
	struct clk *ip1_xtal_ck;
	struct clk *hdmi_idclk_ck;
	struct clk *hdmi2_idclk_ck;
	struct clk *hdmi3_idclk_ck;
	struct clk *hdmi4_idclk_ck;
	struct clk *adc_ck;
	struct clk *hdmi_ck;
	struct clk *hdmi2_ck;
	struct clk *hdmi3_ck;
	struct clk *hdmi4_ck;
	struct clk *ipdma0_ck;
	/* device 1 only */
	struct clk *ipdma1_ck;
};

struct srccap_cap_color_fmt {
	u32 yuv_single_win_recommended_format;
	u32 yuv_multi_win_recommended_format;
	u32 rgb_single_win_recommended_format;
	u32 rgb_multi_win_recommended_format;
	u32 *color_fmt_support;
	u32 support_cnt;
};

struct srccap_capability {
	/* external use */
	u8 ipdma_cnt;
	u8 hdmi_cnt;
	u8 cvbs_cnt;
	u8 svideo_cnt;
	u8 ypbpr_cnt;
	u8 vga_cnt;
	u8 atv_cnt;
	u8 pcb_hdmi_cnt;
	u8 pcb_cvbs_cnt;
	u8 pcb_svideo_cnt;
	u8 pcb_ypbpr_cnt;
	u8 pcb_vga_cnt;
	u8 pcb_atv_cnt;
	u8 dscl_scaling_cap;
	u32 hw_timestamp_max_value; /* in us */
	bool dither_support;
	u32 adc_multi_src_max_num;
	struct srccap_cap_color_fmt color_fmt;
	bool pre_ip2_src1_support_hdmi;
	bool support_ts;
	bool support_dq_event;
	/* GPIO */
	bool src_standby_detect_en;
	u32 src_gpio_num[SRC_DETECT_MAX_NUM];
	/* internal use */
	u32 hw_version;
	u32 stable_count;
	u32 no_sync_count;
	u32 adc_vga_fixed_gain_r;
	u32 adc_vga_fixed_gain_g;
	u32 adc_vga_fixed_gain_b;
	u32 adc_ypbpr_fixed_gain_r;
	u32 adc_ypbpr_fixed_gain_g;
	u32 adc_ypbpr_fixed_gain_b;
	u32 adc_vga_fixed_offset_r;
	u32 adc_vga_fixed_offset_g;
	u32 adc_vga_fixed_offset_b;
	u32 adc_ypbpr_fixed_offset_r;
	u32 adc_ypbpr_fixed_offset_g;
	u32 adc_ypbpr_fixed_offset_b;
	u32 xtal_clk;
	u32 bpw;
	u32 u32IRQ_Version;
	u32 crop_align;
	u32 u32DV_Srccap_HWVersion;
	u32 u32HDMI_Srccap_HWVersion;
	u32 u32AlgCap[ALG_CAPABILITY_SIZE];
	u32 polling_time;
	u32 polling_time_sync_monitor_task;
	u32 polling_time_min_pre_signal_task;
	u32 polling_time_max_pre_signal_task;
	u32 meta_vfreq_tol;
};


struct srccap_port_map {
	enum srccap_input_port data_port;
	enum srccap_input_port sync_port;
};

struct srccap_event_status {
	bool signalstatus_chg;
	bool resolution_chg;
	bool framerate_chg;
	bool scantype_chg;
	bool colorformat_chg;
	bool dqbuf_chg;
	bool sync_status_event[V4L2_SRCCAP_INPUT_SOURCE_NUM];
	bool vsync;
};

struct srccap_ml_info {
	int fd_dscl;
	int fd_memout;
	int fd_dvdma;
};

struct srccap_waitqueue_list {
	wait_queue_head_t waitq_buffer;
	unsigned long buffer_done;
};

struct srccap_work_list {
};

struct srccap_workqueue_list {
};

struct srccap_mutex_list {
	struct mutex mutex_buffer_q;
	struct mutex mutex_irq_info;
	struct mutex mutex_pqmap;
	struct mutex mutex_timing_monitor;
	struct mutex mutex_sync_monitor;
	struct mutex mutex_online_source;
	struct mutex mutex_adc_cb_info;
	struct mutex mutex_vb2_buffer_done;
	struct mutex mutex_memout_cb_info;
	struct mutex mutex_ml;
	struct mutex mutex_macro_version;
	struct mutex mutex_cgms;
};

struct srccap_spinlock_list {
	spinlock_t spinlock_buffer_handling_task;
};

struct srccap_ml_script_info {
	int instance;
	u8 mem_index;
	u64 start_addr;
	u64 max_addr;
	u32 auto_gen_cmd_count;
	u32 mem_size;
	u32 dscl_cmd_count;
	u32 memout_capture_size_cmd_count;
	u32 memout_memory_addr_cmd_count;
	u32 total_cmd_count;
};

struct srccap_info {
	enum v4l2_srccap_input_source src;
	bool stub_mode;
	struct srccap_event_status sts;
	struct srccap_clock clk;
	struct srccap_capability cap;
	struct srccap_port_map map[V4L2_SRCCAP_INPUT_SOURCE_NUM];
	struct task_struct *timing_monitor_task;
	struct task_struct *sync_monitor_task;
	struct task_struct *buffer_handling_task;
	struct srccap_ml_info ml_info;
	struct srccap_waitqueue_list waitq_list;
	struct srccap_work_list work_list;
	struct srccap_workqueue_list workq_list;
	struct srccap_mutex_list mutex_list;
	struct srccap_spinlock_list spinlock_list;
	bool sync_status[V4L2_SRCCAP_INPUT_SOURCE_NUM];
};

struct srccap_fmm_info {
	bool isFMM;
	bool isFirstFrameDone;
};

struct srccap_report_info_status {
	u8 report_info_module;
};


struct mtk_srccap_dev {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *mdev;
	struct video_device *vdev;
	u16 dev_id;
	bool streaming;
	struct kobject *mtkdbg_kobj;

	struct v4l2_subdev subdev_adc;
	struct v4l2_subdev subdev_avd;
	struct v4l2_subdev subdev_common;
	struct v4l2_subdev subdev_dscl;
	struct v4l2_subdev subdev_hdmirx;
	struct v4l2_subdev subdev_memout;
	struct v4l2_subdev subdev_mux;
	struct v4l2_subdev subdev_timingdetect;
	struct v4l2_subdev subdev_vbi;

	struct v4l2_ctrl_handler srccap_ctrl_handler;
	struct v4l2_ctrl_handler adc_ctrl_handler;
	struct v4l2_ctrl_handler avd_ctrl_handler;
	struct v4l2_ctrl_handler common_ctrl_handler;
	struct v4l2_ctrl_handler dscl_ctrl_handler;
	struct v4l2_ctrl_handler hdmirx_ctrl_handler;
	struct v4l2_ctrl_handler memout_ctrl_handler;
	struct v4l2_ctrl_handler mux_ctrl_handler;
	struct v4l2_ctrl_handler timingdetect_ctrl_handler;
	struct v4l2_ctrl_handler vbi_ctrl_handler;

	struct srccap_info srccap_info;
	struct srccap_adc_info adc_info;
	struct srccap_avd_info avd_info;
	struct srccap_common_info common_info;
	struct srccap_dscl_info dscl_info;
	struct srccap_hdmirx_info hdmirx_info;
	struct srccap_memout_info memout_info;
	struct srccap_mux_info mux_info;
	struct srccap_timingdetect_info timingdetect_info;
	struct srccap_vbi_info vbi_info;
	struct srccap_dv_info dv_info;
	struct srccap_fmm_info fmm_info;

	struct srccap_pattern_status pattern_status;
	struct srccap_pattern_info pattern_info;

	struct mutex mutex;
	struct clk *clk;
	spinlock_t lock;

	int irq_vbi;

	enum v4l2_buf_type src_buf_type;
	enum v4l2_buf_type dst_buf_type;
	enum srccap_swmode_buf_type swmode_buf_type;

	struct srccap_report_info_status src_report_info;

	unsigned int va_size[SRCCAP_SHM_QUE_NUM];
	void *va_addr[SRCCAP_SHM_QUE_NUM];

};

struct mtk_srccap_ctx {
	struct v4l2_fh fh;
	struct mtk_srccap_dev *srccap_dev;
};

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
int mtk_srccap_set_isog_detect_mode(struct mtk_srccap_dev *srccap_dev);
void mtk_srccap_set_dscl(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_memout_buffer_entry *entry);
void mtk_srccap_realtime_add_thread(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_realtime_thread realtime_thread,
	s32 pid);
void mtk_srccap_realtime_remove_thread(s32 pid);
#endif  // MTK_SRCCAP_H
