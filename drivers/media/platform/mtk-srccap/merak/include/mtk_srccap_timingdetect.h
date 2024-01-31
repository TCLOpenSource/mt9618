/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_TIMINGDETECT_H
#define MTK_SRCCAP_TIMINGDETECT_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/mtk-v4l2-srccap.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
/* signal status */
#define SRCCAP_TIMINGDETECT_HPD_MASK (0x3FFF)
#define SRCCAP_TIMINGDETECT_VTT_MASK (0xFFFF)
#define SRCCAP_TIMINGDETECT_HPD_MIN (10)
#define SRCCAP_TIMINGDETECT_VTT_MIN (200)
#define SRCCAP_TIMINGDETECT_HPD_TOL (5)
#define SRCCAP_TIMINGDETECT_VTT_TOL (9)
#define SRCCAP_TIMINGDETECT_HST_TOL (3)
#define SRCCAP_TIMINGDETECT_HEND_TOL (3)
#define SRCCAP_TIMINGDETECT_VST_TOL (3)
#define SRCCAP_TIMINGDETECT_VEND_TOL (3)
#define SRCCAP_TIMINGDETECT_STABLE_CNT (100)
#define SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER (8)
#define SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER (2)
#define SRCCAP_TIMINGDETECT_INTERLACE_ADDEND (2)
#define SRCCAP_TIMINGDETECT_DIVIDE_BASE_2 (2)
#define SRCCAP_TIMINGDETECT_DIVIDE_BASE_10 (10)
#define SRCCAP_TIMINGDETECT_VRR_DEBOUNCE_CNT (30)
#define SRCCAP_TIMINGDETECT_VRR_TYPE_CNT (3)
#define SRCCAP_TIMINGDETECT_REFRESH_TO_VFREQ_MULTIPLIER (100)
#define SRCCAP_TIMINGDETECT_HDMI_SPEC_NUMERATOR (5)
#define SRCCAP_TIMINGDETECT_HDMI_SPEC_DENOMINATOR (1000)
#define SRCCAP_TIMINGDETECT_MULTIPLIER_10 (10)
#define SRCCAP_TIMINGDETECT_VTT_EXT_DIS_VAL (0x1F)
#define SRCCAP_TIMINGDETECT_VTT_EXT_EN_VAL  (0xFF)

/* timing status */
#define SRCCAP_TIMINGDETECT_FLAG_NULL (0x00)

#define SRCCAP_TIMINGDETECT_POL_HPVP_BIT (0)
#define SRCCAP_TIMINGDETECT_FLAG_POL_HPVP (1 << SRCCAP_TIMINGDETECT_POL_HPVP_BIT)

#define SRCCAP_TIMINGDETECT_POL_HPVN_BIT (1)
#define SRCCAP_TIMINGDETECT_FLAG_POL_HPVN (1 << SRCCAP_TIMINGDETECT_POL_HPVN_BIT)

#define SRCCAP_TIMINGDETECT_POL_HNVP_BIT (2)
#define SRCCAP_TIMINGDETECT_FLAG_POL_HNVP (1 << SRCCAP_TIMINGDETECT_POL_HNVP_BIT)

#define SRCCAP_TIMINGDETECT_POL_HNVN_BIT (3)
#define SRCCAP_TIMINGDETECT_FLAG_POL_HNVN (1 << SRCCAP_TIMINGDETECT_POL_HNVN_BIT)

#define SRCCAP_TIMINGDETECT_INTERLACED_BIT (4)
#define SRCCAP_TIMINGDETECT_FLAG_INTERLACED (1 << SRCCAP_TIMINGDETECT_INTERLACED_BIT)

#define SRCCAP_TIMINGDETECT_CE_TIMING_BIT (5)
#define SRCCAP_TIMINGDETECT_FLAG_CE_TIMING (1 << SRCCAP_TIMINGDETECT_CE_TIMING_BIT)

#define SRCCAP_TIMINGDETECT_YPBPR_BIT (6)
#define SRCCAP_TIMINGDETECT_FLAG_YPBPR (1 << SRCCAP_TIMINGDETECT_YPBPR_BIT)

#define SRCCAP_TIMINGDETECT_HDMI_BIT (7)
#define SRCCAP_TIMINGDETECT_FLAG_HDMI (1 << SRCCAP_TIMINGDETECT_HDMI_BIT)

#define SRCCAP_TIMINGDETECT_DVI_BIT (8)
#define SRCCAP_TIMINGDETECT_FLAG_DVI (1 << SRCCAP_TIMINGDETECT_DVI_BIT)

#define SRCCAP_TIMINGDETECT_VGA_BIT (9)
#define SRCCAP_TIMINGDETECT_FLAG_VGA (1 << SRCCAP_TIMINGDETECT_VGA_BIT)

#define SRCCAP_TIMINGDETECT_HFREQ_TOLERANCE (15)
#define SRCCAP_TIMINGDETECT_HFREQ_TOLERANCE_HIGH (200)
#define SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_500 (500)
#define SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_1000 (1000)
#define SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_1500 (1500)
#define SRCCAP_TIMINGDETECT_VTT_TOLERANCE (10)

#define SRCCAP_TIMINGDETECT_TIMING_INFO_NUM (10)
#define SRCCAP_VD_SAMPLING_INFO_NUM (9)

#define SRCCAP_TIMINGDETECT_TIMING_HDMI21_VTT_THRESHOLD	(225)
#define SRCCAP_TIMINGDETECT_TIMING_HDMI21_VDE_THRESHOLD	(216)

#define SRCCAP_TIMINGDETECT_CAPWIN_PARA_NUM (2)

#define SRCCAP_TIMINGDETECT_RESET_DELAY_1000 (1000)
#define SRCCAP_TIMINGDETECT_RESET_DELAY_1100 (1100)

#define SRCCAP_TIMINGDETECT_VFREQX1000_24HZ (24000)
#define SRCCAP_TIMINGDETECT_VFREQX1000_25HZ (25000)
#define SRCCAP_TIMINGDETECT_VFREQX1000_60HZ (60000)
#define SRCCAP_TIMINGDETECT_HFREQX10_100 (1000)
#define SRCCAP_TIMINGDETECT_FDET_VTT_0 (0)
#define SRCCAP_TIMINGDETECT_FDET_VTT_1 (1)
#define SRCCAP_TIMINGDETECT_FDET_VTT_2 (2)
#define SRCCAP_TIMINGDETECT_FDET_VTT_3 (3)
#define SRCCAP_TIMINGDETECT_XTALI_12MX1000 (12000000000)

#define SRCCAP_TIMINGDETECT_WAIT_STABLE_DELAY msecs_to_jiffies(400) // 400ms
#define SRCCAP_VTT_CHG_FLAG (1 << 0)
#define SRCCAP_VSYNC_LOST_FLAG (1 << 1)
#define SRCCAP_HTT_CHG_FLAG (1 << 2)
#define SRCCAP_HSYNC_LOST_FLAG (1 << 3)

/* VRR base frame rate */
#define SRCCAP_TIMINGDETECT_VRR_TIMING_TOLERANCE (20)
#define SRCCAP_TIMINGDETECT_VRR_RATE_TOLERANCE (5)
#define SRCCAP_TIMINGDETECT_VRR_DIV_1000 (1000)
#define SRCCAP_TIMINGDETECT_PIXEL_FREQ_ERROR (0xFFFFFFFF)

#define SRCCAP_TIMINGDETECT_108OI_WIDTH_MAX    (1925)
#define SRCCAP_TIMINGDETECT_108OI_WIDTH_MIN    (1915)
#define SRCCAP_TIMINGDETECT_108OI_HEIGHT_MAX   (545)
#define SRCCAP_TIMINGDETECT_108OI_HEIGHT_MIN   (535)
#define SRCCAP_TIMINGDETECT_108OI_HFREQX10_MAX (315)
#define SRCCAP_TIMINGDETECT_108OI_HFREQX10_MIN (305)
#define SRCCAP_TIMINGDETECT_108OI_VFREQX10_MAX (50400)
#define SRCCAP_TIMINGDETECT_108OI_VFREQX10_MIN (49600)
#define SRCCAP_TIMINGDETECT_108OI_VTT_MAX      (630)
#define SRCCAP_TIMINGDETECT_108OI_VTT_MIN      (610)


#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_HPD		(765)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_VPD		(239616)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_VTT		(625)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_HTT		(1702)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_INTERLACED		(1)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_HSTART		(0x1fff)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_VSTART		(0x1ffe)
#define SRCCAP_TIMINGDETECT_ATV_DEFAULT_PIXEL		(0xffffffff)

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
enum srccap_timingdetect_vrr_type {
	SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR = 0,
	SRCCAP_TIMINGDETECT_VRR_TYPE_VRR,
	SRCCAP_TIMINGDETECT_VRR_TYPE_CVRR
};

enum srccap_timingdetect_ip1_type {
	SRCCAP_TIMINGDETECT_IP1_START,
	SRCCAP_TIMINGDETECT_IP1_HDMI0 = SRCCAP_TIMINGDETECT_IP1_START,
	SRCCAP_TIMINGDETECT_IP1_HDMI1,
	SRCCAP_TIMINGDETECT_IP1_HDMI2,
	SRCCAP_TIMINGDETECT_IP1_HDMI3,
	SRCCAP_TIMINGDETECT_IP1_HDMI_END,
	SRCCAP_TIMINGDETECT_IP1_ADCA = SRCCAP_TIMINGDETECT_IP1_HDMI_END,
	SRCCAP_TIMINGDETECT_IP1_ADCB, // connect anything, so it's the adc end.
	SRCCAP_TIMINGDETECT_IP1_ADC_END = SRCCAP_TIMINGDETECT_IP1_ADCB,
	SRCCAP_TIMINGDETECT_IP1_VD = SRCCAP_TIMINGDETECT_IP1_ADC_END,
	SRCCAP_TIMINGDETECT_IP1_VD_END,
	SRCCAP_TIMINGDETECT_IP1_TYPE_MAX = SRCCAP_TIMINGDETECT_IP1_VD_END,
};

enum srccap_timingdetect_108oi_status {
	SRCCAP_TIMINGDETECT_108OI_DISABLE = 0,
	SRCCAP_TIMINGDETECT_108OI_SETTING,
	SRCCAP_TIMINGDETECT_108OI_ENABLE
};

/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct mtk_srccap_dev;

struct srccap_timingdetect_timing {
	u32 hfreqx10; // unit: kHz
	u32 vfreqx1000; // unit: Hz
	u16 hstart;
	u16 vstart;
	u16 hde;
	u16 vde;
	u16 htt;
	u16 vtt;
	u16 adcphase;
	u16 status;
};

struct srccap_timingdetect_data {
	u16 h_start;
	u16 v_start;
	u16 h_de;		/* width */
	u16 v_de;		/* height */
	u32 h_freqx10;
	u32 v_freqx1000;	/* frameratex1000 */
	bool interlaced;
	u16 h_period;
	u16 h_total;
	u16 v_total;
	bool h_pol;		/* hsync polarity */
	bool v_pol;		/* vsync polarity */
	u16 adc_phase;
	bool ce_timing;
	bool yuv420;
	bool frl;
	bool vrr_enforcement;
	enum srccap_timingdetect_vrr_type vrr_type;
	u32 refresh_rate;
	u8 colorformat;
	u8 vs_pulse_width;
	u32 fdet_vtt0;
	u32 fdet_vtt1;
	u32 fdet_vtt2;
	u32 fdet_vtt3;
};

struct srccap_timingdetect_vdsampling {
	enum v4l2_ext_avd_videostandardtype videotype;
	u16 v_start;
	u16 h_start;
	u16 h_de;
	u16 v_de;
	u16 h_total;
	u32 v_freqx1000;
	u16 org_size_width;
	u16 org_size_height;
};

struct srccap_timingdetect_auto_gain_max_value_status {
	bool b_status;
	bool g_status;
	bool r_status;
};

/* just used for pre_monitor...
 * since the above struct does not mapping the all member we need.
 */
struct srccap_timingdetect_pre_monitor_info {
	bool interlaced;

	bool yuv420;
	bool frl;

	bool vpol;
	bool hpol;
	bool freesync;

	u32 hpd;
	u32 vtt;
	u32 vpd;
	u32 htt;

	u32 vrr;

	u16 hstart;
	u16 vstart;
	u16 hend;
	u16 vend;

	u32 pixel_freq;

	u8 vrr_debounce_count;
	u8 vs_pulse_width;
};

struct srccap_timingdetect_ip1_src_info {
	struct list_head node;

	/* lock for pre_monitor_task & signal_task */
	struct mutex lock;

	u32 stable_count;
	u32 no_sync_count;
	enum srccap_timingdetect_ip1_type ip1_source;
	enum v4l2_timingdetect_status status;
	struct srccap_timingdetect_pre_monitor_info info;
	enum srccap_timingdetect_108oi_status euro108oi_status;
};

struct srccap_timingdetect_fv_crop {
	u16 CropWinX;
	u16 CropWinY;
	u16 CropWinWidth;
	u16 CropWinHeight;
	bool bEnable;
};

struct srccap_timingdetect_info {
	enum v4l2_timingdetect_status status;
	u16 table_entry_count;
	struct srccap_timingdetect_timing *timing_table;
	struct v4l2_rect cap_win;

	/* for signal pre-monitor.
	 * @ktime: which start from set_input/signal_unstable,
	 *			end at signal stable;
	 * @task: the task for monitor all signal status in ip1,
	 *			its just for dev_id 0 since we just need one task.
	 * @ip1_src_info: each ip1 has its own src_info,
	 *			but currently we just support hdmi0/1/2/3 & adca.
	 */
	ktime_t ktime;
	struct task_struct *task;
	struct srccap_timingdetect_ip1_src_info *ip1_src_info;

	struct srccap_timingdetect_data data;
	struct srccap_timingdetect_vdsampling *vdsampling_table;
	struct srccap_timingdetect_fv_crop fv_crop;
	u16 vdsampling_table_entry_count;
	u8 auto_gain_max_value_status;
	bool auto_gain_ready_status;
	u8 ans_mute_status;
	u16 org_size_width;
	u16 org_size_height;
};

struct ST_VRR_BASE_VFREQ_MAPPING_TBL {
	u8 index;
	u16 u16Width;
	u16 u16Height;
	u16 u16FrameRate;
	u16 u16HTT;
	u16 u16VTT;
	u32 u32PixelClock;
};

static struct ST_VRR_BASE_VFREQ_MAPPING_TBL
	stVrrBaseFreqMappingTbl[] = {
	//idx  width height FR   HTT   VTT  CLK
	//640x480
	{0,  640, 480,  48,   720, 495, 17},
	{1,  640, 480,  50,   800, 493, 20},
	{2,  640, 480,  60,   800, 525, 25},
	{3,  640, 480,  100,   800, 504, 40},
	{4,  640, 480,  120,   800, 509, 49},
	//720x576
	{5,  720, 576,  48,   800, 591, 23},
	{6,  720, 576,  50,   800, 591, 24},
	{7,  720, 576,  50,   864, 625, 27},
	{8,  720, 576,  60,   800, 593, 28},
	{9,  720, 576,  100,   800, 604, 48},
	{10,  720, 576,  120,   800, 610, 59},
	//
	{11, 1024, 768,  60, 1344, 806, 65},
	//1280x720
	{12,  1280, 720,  48,   2500, 750, 90},   //1280x720@48
	{13,  1280, 720,  50,   1980, 750, 74},   //1280x720@50
	{14,  1280, 720,  60,   1650, 750, 74},   //1280x720@59.94/60
	{15,  1280, 720,  100,  1980, 750, 148},  //1280x720@100
	{16,  1280, 720,  120,  1650, 750, 148},  //1280x720@119.88/120
	//2560x1080
	{17,  2560, 1080,  48,  3750, 1100, 198},
	{18,  2560, 1080,  50,  3300, 1125, 185},
	{19,  2560, 1080,  60,  3000, 1100, 198},
	{20,  2560, 1080,  100, 2970, 1250, 371},
	{21,  2560, 1080,  120, 3300, 1250, 495},
	//1920x1080
	{22, 1920, 1080,  48,  2750, 1125, 148},
	{23, 1920, 1080,  50,  2640, 1125, 148},
	{24, 1920, 1080,  60,  2200, 1125, 148},
	{25, 1920, 1080,  100, 2640, 1125, 297},
	{26, 1920, 1080,  120, 2200, 1125, 297},
	//4096x2160
	{27, 4096, 2160,  48,  5500, 2250, 594},
	{28, 4096, 2160,  50,  5280, 2250, 594},
	{29, 4096, 2160,  60,  4400, 2250, 594},
	{30, 4096, 2160,  100, 5280, 2250, 1188},
	{31, 4096, 2160,  120, 4400, 2250, 1188},
	//3840x2160
	{32, 3840, 2160,  48,  5500, 2250, 594},
	{33, 3840, 2160,  50,  5280, 2250, 594},
	{34, 3840, 2160,  60,  4400, 2250, 594},
	{35, 3840, 2160,  100, 5280, 2250, 1188},
	{36, 3840, 2160,  120, 4400, 2250, 1188},

	{37, 3840, 2160,  144, 3920, 2314, 1306},
	{38, 3840, 2160,  144, 4000, 2314, 1332},
	{39, 3840, 2160,  144, 4032, 2314, 1343},
	{40, 3840, 2160,  144, 4400, 2250, 1426},
	{41, 3840, 2160,  144, 5408, 2350, 1830},
	//PC timing
	{42, 2560, 1440,  60,  2720, 1481, 241},
	{43, 2560, 1440,  120, 2720, 1525, 497},
	//1920x1080 144, 2560x1080 144
	{44, 1920, 1080,  144, 2672, 1177, 452},
	{45, 1920, 1080,  144, 2112, 1157, 352},
	{46, 1920, 1080,  144, 2080, 1157, 346},
	{47, 1920, 1080,  144, 2200, 1125, 356},
	{48, 1920, 1080,  144, 2000, 1157, 333},
	{49, 2560, 1440,  144, 3584, 1568, 809},
	{50, 2560, 1440,  144, 2752, 1543, 612},
	{51, 2560, 1440,  144, 2720, 1543, 604}, //AMD 6900 PC timing
	{52, 2560, 1440,  144, 2640, 1543, 586},
	{53, 2560, 1440,  144, 2720, 1525, 596}, // nv3080-TI PC timing
	//FHD 240
	{54, 1920, 1080,  240, 2200, 1125, 594},
	{55, 1920, 1080,  240, 2080, 1157, 578},
	//7680x4320
	{56, 7680, 4320,  48,  11000, 4450, 2350},
	{57, 7680, 4320,  50,  10560, 4450, 2350},
	{58, 7680, 4320,  60,  8800, 4450, 2350},
	{59, 7680, 4320,  100, 10560, 4450, 4699},
	{60, 7680, 4320,  120, 8800, 4450, 4699},
	{61, 3840, 1080,  120, 4000, 1111, 533},
	{62, 3840, 1600,  60, 5248, 1658, 520},
};
/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
int mtk_timingdetect_init(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_init_offline(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_init_clock(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_deinit(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_deinit_clock(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_set_capture_win(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_rect cap_win);
int mtk_timingdetect_retrieve_timing(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync_status,
	struct srccap_timingdetect_data *data,
	enum v4l2_timingdetect_status status,
	bool signal_status_changed,
	bool color_fmt_changed,
	bool vrr_changed);
int mtk_timingdetect_store_timing_info(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_data data);
int mtk_timingdetect_get_signal_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum v4l2_timingdetect_status *status);
int mtk_timingdetect_streamoff(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_check_sync(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool adc_offline_status,
	bool *has_sync);
int mtk_timingdetect_get_field(
	struct mtk_srccap_dev *srccap_dev,
	u8 *field);
int mtk_timingdetect_set_auto_no_signal_mute(
	struct mtk_srccap_dev *srccap_dev,
	bool enable);
int mtk_timingdetect_steamon(
	struct mtk_srccap_dev *srccap_dev);
int mtk_timingdetect_set_8p(
	struct mtk_srccap_dev *srccap_dev,
	bool frl);
int mtk_timingdetect_set_hw_version(
	struct mtk_srccap_dev *srccap_dev,
	bool stub);
int mtk_srccap_register_timingdetect_subdev(
	struct v4l2_device *srccap_dev,
	struct v4l2_subdev *subdev_timingdetect,
	struct v4l2_ctrl_handler *timingdetect_ctrl_handler);
void mtk_srccap_unregister_timingdetect_subdev(
	struct v4l2_subdev *subdev_timingdetect);
void mtk_timingdetect_enable_auto_gain_function(void *param);
void mtk_timingdetect_disable_auto_gain_function(void *param);
void mtk_timingdetect_enable_auto_range_function(void *param);
void mtk_timingdetect_disable_auto_range_function(void *param);
void mtk_timingdetect_set_auto_range_window(void *param);
void mtk_timingdetect_get_auto_gain_max_value_status(void *param);
void mtk_timingdetect_get_auto_gain_status(void *param);
int mtk_timingdetect_get_vtotal(
	struct mtk_srccap_dev *srccap_dev,
	uint16_t *vtt);
bool mtk_timingdetect_check_if_mapping(
		enum v4l2_srccap_input_source source,
		enum srccap_timingdetect_ip1_type ip1_source);
bool mtk_timingdetect_check_if_invalid(
		enum srccap_timingdetect_ip1_type ip1_source);
int mtk_timingdetect_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size);
int mtk_timingdetect_update_vrr_info(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_ip1_src_info *ip1_src_info);
int mtk_timingdetect_set_auto_nosignal(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool enable);
int mtk_timingdetect_set_auto_nosignal_watch_dog(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool enable);
#endif  // MTK_SRCCAP_TIMINGDETECT_H

