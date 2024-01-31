/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_ADC_H
#define MTK_SRCCAP_ADC_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/mtk-v4l2-srccap.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
#define ADCTBL_CLK_SET_NUM (50)
#define ADCTBL_CLK_SWEN_NUM (50)

#define ADC_YPBPR_FIXED_PHASE       (0x0005)
#define ADC_CENTER_GAIN (0x1000)
#define ADC_CENTER_OFFSET (0x0800)
#define ADC_GAIN_BIT_CNT (14)
#define SRCCAP_ADC_MAX_CLK (3500)
#define SRCCAP_ADC_UDELAY_500 (500)
#define SRCCAP_ADC_UDELAY_510 (510)
#define SRCCAP_ADC_UDELAY_1000 (1000)
#define SRCCAP_ADC_UDELAY_1100 (1100)

#define SRCCAP_ADC_DOUBLE_SAMPLE_RESOLUTION  (720)

#define SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG (BIT(2))
#define SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG (BIT(1))
#define SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG (BIT(0))
#define SRCCAP_ADC_FLOW_FLAG_R_SUB (BIT(0))
#define SRCCAP_ADC_FLOW_FLAG_R_ADD (BIT(1))
#define SRCCAP_ADC_FLOW_FLAG_R (	\
	SRCCAP_ADC_FLOW_FLAG_R_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_R_ADD)
#define SRCCAP_ADC_FLOW_FLAG_G_SUB (BIT(2))
#define SRCCAP_ADC_FLOW_FLAG_G_ADD (BIT(3))
#define SRCCAP_ADC_FLOW_FLAG_G (	\
	SRCCAP_ADC_FLOW_FLAG_G_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_G_ADD)
#define SRCCAP_ADC_FLOW_FLAG_B_SUB (BIT(4))
#define SRCCAP_ADC_FLOW_FLAG_B_ADD (BIT(5))
#define SRCCAP_ADC_FLOW_FLAG_B (	\
	SRCCAP_ADC_FLOW_FLAG_B_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_B_ADD)
#define SRCCAP_ADC_FLOW_FLAG_MAX (	\
	SRCCAP_ADC_FLOW_FLAG_R_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_R_ADD |	\
	SRCCAP_ADC_FLOW_FLAG_G_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_G_ADD |	\
	SRCCAP_ADC_FLOW_FLAG_B_SUB |	\
	SRCCAP_ADC_FLOW_FLAG_B_ADD)
#define SRCCAP_ADC_AUTO_YUV_GAIN_STEP (1)
#define SRCCAP_ADC_AUTO_YUV_MAX_CNT (30)
#define SRCCAP_ADC_AUTO_GAIN_FINE_TUNE (10)
#define SRCCAP_ADC_MAX_CB_NUM (10)
#define SRCCAP_ADC_MAX_VALUE_STATUS_RETRY_CNT (5)
#define SRCCAP_ADC_AUTO_GAIN_READY_TIMEOUT (1000)
#define SRCCAP_ADC_SRC_0 (0)
#define SRCCAP_ADC_SRC_1 (1)
#define SRCCAP_ADC_DUAL_SRC_CNT (2)
#define SRCCAP_ADC_CHECK_SRC_BOUND_INVAL(src)   (	\
	src < V4L2_SRCCAP_INPUT_SOURCE_NONE ||		\
	src >= V4L2_SRCCAP_INPUT_SOURCE_NUM)

typedef void (*SRCCAP_ADC_INTCB) (void *param);

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */
#define SRCCAP_ADC_IS_VD_SRC(_adc_src) \
	((_adc_src == SRCCAP_ADC_SOURCE_ONLY_CVBS) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_ONLY_SVIDEO) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_ONLY_SCART_RGB) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_ONLY_SCART_Y) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_RGB_ATV) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY) \
	|| (_adc_src == SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS))

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
enum srccap_input_port;
enum srccap_access_type;

enum srccap_adc_source_info {
	// None ADC source
	SRCCAP_ADC_SOURCE_NONADC = 0,

	// Single ADC source
	SRCCAP_ADC_SOURCE_ONLY_CVBS,
	SRCCAP_ADC_SOURCE_ONLY_SVIDEO,
	SRCCAP_ADC_SOURCE_ONLY_YPBPR,
	SRCCAP_ADC_SOURCE_ONLY_RGB,
	SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV,
	SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV,
	SRCCAP_ADC_SOURCE_ONLY_SCART_RGB,
	SRCCAP_ADC_SOURCE_ONLY_SCART_Y,

	SRCCAP_ADC_SOURCE_SINGLE_NUMS,

	// Multi ADC source
	SRCCAP_ADC_SOURCE_MULTI_RGB_ATV,
	SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY,
	SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS,
	SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV,
	SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY,
	SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS,

	SRCCAP_ADC_SOURCE_ALL_NUMS,
};

enum srccap_adc_isog_detect_mode {
	SRCCAP_ADC_ISOG_NORMAL_MODE = 0,
	SRCCAP_ADC_ISOG_STANDBY_MODE,
	SRCCAP_ADC_ISOG_MODE_NUMS,
};

enum srccap_adc_sog_online_mux {
	SRCCAP_SOG_ONLINE_PADA_GIN0P,
	SRCCAP_SOG_ONLINE_PADA_GIN1P,
	SRCCAP_SOG_ONLINE_PADA_GIN2P,
	SRCCAP_SOG_ONLINE_PADA_GND,
};

enum srccap_adc_callback_stage {
	SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_GAIN_FUNCTION = 0,
	SRCCAP_ADC_CB_STAGE_DISABLE_AUTO_GAIN_FUNCTION,
	SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_RANGE_FUNCTION,
	SRCCAP_ADC_CB_STAGE_DISABLE_AUTO_RANGE_FUNCTION,
	SRCCAP_ADC_CB_STAGE_SET_AUTO_RANGE_WINDOW,
	SRCCAP_ADC_CB_STAGE_GET_AUTO_GAIN_MAX_VALUE_STATUS,
	SRCCAP_ADC_CB_STAGE_GET_AUTO_GAIN_STATUS,
	SRCCAP_ADC_CB_STAGE_MAX,
};

/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct mtk_srccap_dev;

struct srccap_adc_cb_info {
	void *param;
	SRCCAP_ADC_INTCB cb;
};

struct srccap_adc_info {
	enum srccap_adc_source_info adc_src;
	enum v4l2_adc_auto_calibration_mode auto_calibration_mode;
	struct srccap_adc_cb_info
		cb_info[SRCCAP_ADC_CB_STAGE_MAX][SRCCAP_ADC_MAX_CB_NUM];
};

struct srccap_adc_filter_info {
	u8 blpf_bwp;
	u8 blpf_bwm;
	u8 glpf_bwp;
	u8 glpf_bwm;
	u8 rlpf_bwp;
	u8 rlpf_bwm;
	u8 test_vdclp_str;
	bool test_adc_clkmode;
	bool test_vdlpf_y_out;
	bool test_vdlpf_c_out;
};

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
int mtk_adc_enable_offline_sog(
	struct mtk_srccap_dev *srccap_dev,
	bool enable);
int mtk_adc_set_offline_mux(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src);
int mtk_adc_check_offline_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool *offline);
int mtk_adc_check_offline_port_conflict(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool *conflict,
	enum v4l2_srccap_input_source *conflicted_src);
int mtk_adc_set_freerun(
	struct mtk_srccap_dev *srccap_dev,
	bool enable);
int mtk_adc_set_mode(
	struct mtk_srccap_dev *srccap_dev);
int mtk_adc_load_cal_tbl(
	struct mtk_srccap_dev *srccap_dev);
int mtk_adc_set_source(
	struct v4l2_adc_source *source,
	struct mtk_srccap_dev *srccap_dev);
int mtk_adc_set_isog_detect_mode(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum srccap_adc_isog_detect_mode mode);
int mtk_adc_set_iclamp_rgb_powerdown(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool powerdown);
int mtk_adc_init(
	struct mtk_srccap_dev *srccap_dev);
int mtk_adc_deinit(
	struct mtk_srccap_dev *srccap_dev);
int mtk_adc_set_port(
	struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_register_adc_subdev(
	struct v4l2_device *srccap_dev,
	struct v4l2_subdev *subdev_adc,
	struct v4l2_ctrl_handler *adc_ctrl_handler);
void mtk_srccap_unregister_adc_subdev(
	struct v4l2_subdev *subdev_adc);
int mtk_adc_register_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_adc_callback_stage stage,
	SRCCAP_ADC_INTCB intcb,
	void *param);
int mtk_adc_unregister_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_adc_callback_stage stage,
	SRCCAP_ADC_INTCB intcb,
	void *param);
int mtk_adc_set_auto_gain_offset_by_string(
	struct device *dev,
	const char *buf);
int mtk_adc_compare_offline_mux(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum v4l2_srccap_input_source target,
	bool *compare_result);
enum srccap_adc_source_info adc_map_analog_source(
	enum v4l2_srccap_input_source src);

#endif  // MTK_SRCCAP_ADC_H
