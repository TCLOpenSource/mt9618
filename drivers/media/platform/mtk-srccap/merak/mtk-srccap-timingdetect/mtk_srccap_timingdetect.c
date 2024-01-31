// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <asm-generic/div64.h>

#include "mtk_srccap.h"
#include "mtk_srccap_timingdetect.h"
#include "hwreg_srccap_timingdetect.h"
#include "pixelmonitor.h"
#include "mtk_srccap_common_ca.h"
#include "utpa2_XC.h"

#define SYNC_STABLE_CHECK(x) \
	x == V4L2_TIMINGDETECT_UNSTABLE_SYNC ? "UNSTABLE" : "STABLE"

/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static int timingdetect_get_clk(
	struct device *dev,
	char *s,
	struct clk **clk)
{
	if (dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (s == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (clk == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*clk = devm_clk_get(dev, s);
	if (IS_ERR(*clk)) {
		SRCCAP_MSG_FATAL("unable to retrieve %s\n", s);
		return PTR_ERR(*clk);
	}

	return 0;
}

static void timingdetect_put_clk(
	struct device *dev,
	struct clk *clk)
{
	if (dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	devm_clk_put(dev, clk);
}

static int timingdetect_enable_parent(
	struct mtk_srccap_dev *srccap_dev,
	struct clk *clk,
	char *s)
{
	int ret = 0;
	struct clk *parent = NULL;

	ret = timingdetect_get_clk(srccap_dev->dev, s, &parent);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	ret = clk_set_parent(clk, parent);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	ret = clk_prepare_enable(clk);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	timingdetect_put_clk(srccap_dev->dev, parent);

	return 0;
}

static int timingdetect_map_detblock_reg(
	enum v4l2_srccap_input_source src,
	struct srccap_port_map map[V4L2_SRCCAP_INPUT_SOURCE_NUM],
	enum reg_srccap_timingdetect_source *detblock)
{
	if (detblock == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((src <= V4L2_SRCCAP_INPUT_SOURCE_NONE) || (src >= V4L2_SRCCAP_INPUT_SOURCE_NUM)) {
		SRCCAP_MSG_ERROR("invalid source:%d\n", src);
		return -EINVAL;
	}

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		switch (map[src].data_port) {
		case SRCCAP_INPUT_PORT_DVI0:
		case SRCCAP_INPUT_PORT_HDMI0:
			*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI;
			break;
		case SRCCAP_INPUT_PORT_DVI1:
		case SRCCAP_INPUT_PORT_HDMI1:
			*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI2;
			break;
		case SRCCAP_INPUT_PORT_DVI2:
		case SRCCAP_INPUT_PORT_HDMI2:
			*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI3;
			break;
		case SRCCAP_INPUT_PORT_DVI3:
		case SRCCAP_INPUT_PORT_HDMI3:
			*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI4;
			break;
		default:
			SRCCAP_MSG_FATAL("invalid port:%d\n", map[src].data_port);
			break;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_VD;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_VD;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_VD;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		*detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_VD;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int timingdetect_set_field_hwreg(
	enum reg_srccap_timingdetect_source detblock,
	bool in_field_inv,
	bool ext_field,
	bool out_field_inv)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_input_field_invert(detblock, in_field_inv,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_external_field_en(detblock, ext_field,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_output_field_invert(detblock, out_field_inv,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_set_coast_hwreg(
	enum reg_srccap_timingdetect_source detblock,
	bool coast_pol,
	u8 front_coast,
	u8 back_coast)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_coast_polarity(detblock, coast_pol,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_front_coast(detblock, front_coast,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_back_coast(detblock, back_coast,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_set_sync_hwreg(
	enum reg_srccap_timingdetect_source detblock,
	bool hs_ref_edge,
	bool vs_delay_md,
	bool vdovs_ref_edge)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_hsync_ref_edge(detblock, hs_ref_edge,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_vsync_delay_mode(detblock, vs_delay_md,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_vdo_vsync_ref_edge(detblock, vdovs_ref_edge,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}


	return ret;
}

static int timingdetect_set_vtt_extension_enable(
	enum reg_srccap_timingdetect_source src,
	bool enable)
{
	int ret = 0;
	u8 hw_val = 0;

	if (enable)
		hw_val = SRCCAP_TIMINGDETECT_VTT_EXT_EN_VAL;
	else
		hw_val = SRCCAP_TIMINGDETECT_VTT_EXT_DIS_VAL;

	ret = drv_hwreg_srccap_timingdetect_set_vtt_ext_en(src, hw_val, TRUE, NULL, NULL);

	if (ret)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}

static int timingdetect_set_source_hwreg(
	enum reg_srccap_timingdetect_source detblock,
	enum reg_srccap_timingdetect_sync sync,
	enum reg_srccap_timingdetect_sync_detection sync_detection,
	enum reg_srccap_timingdetect_video_port video_port,
	enum reg_srccap_timingdetect_video_type video_type,
	enum reg_srccap_timingdetect_fdet_src_sel src_sel,
	struct reg_srccap_timingdetect_user_scantype scantype,
	bool de_only,
	bool de_bypass,
	bool dot_based_hsync,
	bool component,
	bool coast_pol,
	bool hs_ref_edge,
	bool vs_delay_md,
	bool direct_de_md,
	bool vdovs_ref_edge,
	bool in_field_inv,
	bool ext_field,
	bool out_field_inv,
	u8 front_coast,
	u8 back_coast,
	u8 vs_tol,
	bool en_ypbpr,
	bool en_input_image_wrap,
	bool vtt_ext_en)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_source(detblock, video_type, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_sync(detblock, sync, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_sync_detection(detblock, sync_detection,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_video_port(detblock, video_port,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_de_only_mode(detblock, de_only, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_de_bypass_mode(detblock, de_bypass,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_dot_based_hsync(detblock, dot_based_hsync,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_adc_source_sel(detblock, component,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_coast_hwreg(detblock, coast_pol, front_coast, back_coast);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_sync_hwreg(detblock, hs_ref_edge, vs_delay_md, vdovs_ref_edge);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_direct_de_mode(detblock, direct_de_md,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_field_hwreg(detblock, in_field_inv, ext_field, out_field_inv);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_ycbcr_en(detblock, en_ypbpr,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_fdet_sync_src_sel(detblock, src_sel,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_input_image_wrap_en(detblock,
		en_input_image_wrap, en_input_image_wrap,
		TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_user_scantype(detblock,
		scantype, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_vsync_tolerance(detblock, vs_tol,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_usr_vpol_vs_width_en(detblock, TRUE,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_vtt_extension_enable(detblock, vtt_ext_en);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_htt_filt_mode(detblock, TRUE, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_htt_filt_patch(detblock, TRUE, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_set_source(
	struct mtk_srccap_dev *srccap_dev,
	enum reg_srccap_timingdetect_source detblock,
	enum v4l2_srccap_input_source src)
{
	int ret = 0;
	enum reg_srccap_timingdetect_video_type video_type =
		REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_ANALOG1;
	enum reg_srccap_timingdetect_sync sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
	enum reg_srccap_timingdetect_sync_detection sync_detection =
		REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
	enum reg_srccap_timingdetect_video_port video_port =
		REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
	enum reg_srccap_timingdetect_fdet_src_sel src_sel =
		REG_SRCCAP_TIMINGDETECT_FDET_SRC_IPVSHSIN_SB;
	struct reg_srccap_timingdetect_user_scantype scantype;
	bool de_only = FALSE, de_bypass = FALSE, dot_based_hsync = FALSE, component = FALSE;
	bool coast_pol = FALSE, hs_ref_edge = FALSE, vs_delay_md = FALSE, direct_de_md = FALSE;
	bool vdovs_ref_edge = FALSE, in_field_inv = FALSE, ext_field = FALSE, out_field_inv = FALSE;
	bool en_ypbpr = FALSE, vtt_ext_en = FALSE;
	u8 front_coast = 0;
	u8 back_coast = 0;
	u8 vs_tol = 0;
	bool en_input_image_wrap = FALSE;

	struct clk *clk = NULL;
	memset(&scantype, 0, sizeof(struct reg_srccap_timingdetect_user_scantype));

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_HDMI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		ext_field = FALSE;
		front_coast = 0;
		back_coast = 0;
		clk = srccap_dev->srccap_info.clk.hdmi_idclk_ck;
		en_ypbpr = FALSE;
		in_field_inv = TRUE;
		out_field_inv = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = SRCCAP_TIMINGDETECT_VTT_TOL;
		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_HDMI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		ext_field = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		clk = srccap_dev->srccap_info.clk.hdmi2_idclk_ck;
		in_field_inv = TRUE;
		out_field_inv = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = SRCCAP_TIMINGDETECT_VTT_TOL;

		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi2_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_HDMI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		ext_field = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		clk = srccap_dev->srccap_info.clk.hdmi3_idclk_ck;
		in_field_inv = TRUE;
		out_field_inv = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = SRCCAP_TIMINGDETECT_VTT_TOL;

		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi3_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_HDMI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		ext_field = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		clk = srccap_dev->srccap_info.clk.hdmi4_idclk_ck;
		in_field_inv = TRUE;
		out_field_inv = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = SRCCAP_TIMINGDETECT_VTT_TOL;

		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi4_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_VIDEO;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_INT_VD_A;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = TRUE;
		in_field_inv = TRUE;
		ext_field = TRUE;
		out_field_inv = TRUE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		en_input_image_wrap = TRUE;
		scantype.en = TRUE;
		scantype.interlaced = TRUE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_VIDEO;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_INT_VD_A;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = TRUE;
		in_field_inv = TRUE;
		ext_field = TRUE;
		out_field_inv = TRUE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		en_input_image_wrap = TRUE;
		scantype.en = TRUE;
		scantype.interlaced = TRUE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_ANALOG1;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_SOG;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SOG;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_VSHS_SEP;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = TRUE;
		coast_pol = TRUE;
		hs_ref_edge = TRUE;
		vs_delay_md = TRUE;
		direct_de_md = TRUE;
		vdovs_ref_edge = TRUE;
		in_field_inv = TRUE;
		ext_field = FALSE;
		out_field_inv = TRUE;
		front_coast = 0xC;
		back_coast = 0xC;
		en_ypbpr = TRUE;
		en_input_image_wrap = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_ANALOG1;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		in_field_inv = TRUE;
		ext_field = FALSE;
		out_field_inv = TRUE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		en_input_image_wrap = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_VIDEO;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_INT_VD_A;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = TRUE;
		in_field_inv = TRUE;
		ext_field = TRUE;
		out_field_inv = TRUE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		en_input_image_wrap = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_VIDEO;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_AUTO;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_INT_VD_A;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = FALSE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = TRUE;
		in_field_inv = TRUE;
		ext_field = TRUE;
		out_field_inv = TRUE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		en_input_image_wrap = TRUE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vtt_ext_en = TRUE;
		vs_tol = 1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_DVI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		in_field_inv = FALSE;
		ext_field = FALSE;
		out_field_inv = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vs_tol = 1;
		vtt_ext_en = TRUE;
		clk = srccap_dev->srccap_info.clk.hdmi_idclk_ck;
		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_DVI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		in_field_inv = FALSE;
		ext_field = FALSE;
		out_field_inv = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vs_tol = 1;
		vtt_ext_en = TRUE;
		clk = srccap_dev->srccap_info.clk.hdmi2_idclk_ck;
		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi2_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_DVI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = FALSE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		in_field_inv = FALSE;
		ext_field = FALSE;
		out_field_inv = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vs_tol = 1;
		vtt_ext_en = TRUE;
		clk = srccap_dev->srccap_info.clk.hdmi3_idclk_ck;
		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi3_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		video_type = REG_SRCCAP_TIMINGDETECT_VIDEO_TYPE_DVI;
		sync = REG_SRCCAP_TIMINGDETECT_SYNC_CSYNC;
		sync_detection = REG_SRCCAP_TIMINGDETECT_SYNC_DETECTION_SEPARATE_SYNC;
		video_port = REG_SRCCAP_TIMINGDETECT_PORT_EXT_8_10_BIT;
		src_sel = REG_SRCCAP_TIMINGDETECT_FDET_SRC_HSVSIN;
		de_only = FALSE;
		de_bypass = TRUE;
		dot_based_hsync = TRUE;
		component = FALSE;
		coast_pol = FALSE;
		hs_ref_edge = TRUE;
		vs_delay_md = FALSE;
		direct_de_md = TRUE;
		vdovs_ref_edge = FALSE;
		in_field_inv = FALSE;
		ext_field = FALSE;
		out_field_inv = FALSE;
		front_coast = 0;
		back_coast = 0;
		en_ypbpr = FALSE;
		scantype.en = FALSE;
		scantype.interlaced = FALSE;
		vs_tol = 1;
		vtt_ext_en = TRUE;
		clk = srccap_dev->srccap_info.clk.hdmi4_idclk_ck;
		ret = timingdetect_enable_parent(srccap_dev, clk,
			"srccap_hdmi4_idclk_ck_ip1_xtal_ck");
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	ret = timingdetect_set_source_hwreg(detblock, sync, sync_detection,
						video_port, video_type, src_sel, scantype,
						de_only, de_bypass, dot_based_hsync,
						component, coast_pol, hs_ref_edge, vs_delay_md,
						direct_de_md, vdovs_ref_edge, in_field_inv,
						ext_field, out_field_inv, front_coast, back_coast,
						vs_tol, en_ypbpr, en_input_image_wrap, vtt_ext_en);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static enum v4l2_srccap_input_source timingdetect_ip1_map_src(
		enum srccap_timingdetect_ip1_type ip1_source)
{
	enum v4l2_srccap_input_source source =
				V4L2_SRCCAP_INPUT_SOURCE_NONE;

	switch (ip1_source) {
	case SRCCAP_TIMINGDETECT_IP1_HDMI0:
		source = V4L2_SRCCAP_INPUT_SOURCE_HDMI;
		break;
	case SRCCAP_TIMINGDETECT_IP1_HDMI1:
		source = V4L2_SRCCAP_INPUT_SOURCE_HDMI2;
		break;
	case SRCCAP_TIMINGDETECT_IP1_HDMI2:
		source = V4L2_SRCCAP_INPUT_SOURCE_HDMI3;
		break;
	case SRCCAP_TIMINGDETECT_IP1_HDMI3:
		source = V4L2_SRCCAP_INPUT_SOURCE_HDMI4;
		break;
	case SRCCAP_TIMINGDETECT_IP1_ADCA:
		source = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
		break;
	default:
		source = V4L2_SRCCAP_INPUT_SOURCE_NONE;
		break;
	}

	return (int)source;
}

static int timingdetect_set_user_scantype(
	enum reg_srccap_timingdetect_source detblock,
	bool force_en, bool interlaced)
{
	int ret = 0;
	struct reg_srccap_timingdetect_user_scantype scantype;

	memset(&scantype, 0, sizeof(struct reg_srccap_timingdetect_user_scantype));

	scantype.en = force_en;
	scantype.interlaced = interlaced;

	ret = drv_hwreg_srccap_timingdetect_set_user_scantype(
		detblock, scantype, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_set_progressive(
	struct mtk_srccap_dev *srccap_dev,
	enum reg_srccap_timingdetect_source detblock,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((vrr_type != SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		|| (hdmi_free_sync == TRUE))
		ret = timingdetect_set_user_scantype(detblock, TRUE, FALSE);
	else
		ret = timingdetect_set_user_scantype(detblock, FALSE, FALSE);

	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
	return ret;
}

static int timingdetect_set_vtt_det_en(
	struct mtk_srccap_dev *srccap_dev,
	enum reg_srccap_timingdetect_source detblock,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync)
{
	int ret = 0;
	bool en = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((vrr_type != SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR) || (hdmi_free_sync == TRUE)
		|| (srccap_dev->timingdetect_info.data.vrr_enforcement == TRUE))
		en = TRUE;

	ret = drv_hwreg_srccap_timingdetect_set_ans_vtt_det(detblock, en, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	if (en == TRUE) {
		ret = mtk_timingdetect_set_auto_no_signal_mute(srccap_dev, FALSE);
		if (ret < 0)
			goto exit;
	}

	return ret;
exit:
	SRCCAP_MSG_RETURN_CHECK(ret);
	return ret;
}

static int timingdetect_set_vrr_related_settings(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_timingdetect_ip1_type ip1_source,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	src = timingdetect_ip1_map_src(ip1_source);
	ret = timingdetect_map_detblock_reg(src,
				srccap_dev->srccap_info.map, &detsrc);

	ret = timingdetect_set_progressive(srccap_dev, detsrc, vrr_type, hdmi_free_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_vtt_det_en(srccap_dev, detsrc, vrr_type, hdmi_free_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_check_polarity(
	u16 status,
	bool hpol,
	bool vpol,
	bool *same_pol)
{
	if (same_pol == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*same_pol = TRUE;

	if (status & SRCCAP_TIMINGDETECT_FLAG_POL_HPVP) {
		if (!(hpol && vpol))
			*same_pol = FALSE;
	}
	if (status & SRCCAP_TIMINGDETECT_FLAG_POL_HPVN) {
		if (!(hpol && !vpol))
			*same_pol = FALSE;
	}
	if (status & SRCCAP_TIMINGDETECT_FLAG_POL_HNVP) {
		if (!(!hpol && vpol))
			*same_pol = FALSE;
	}
	if (status & SRCCAP_TIMINGDETECT_FLAG_POL_HNVN) {
		if (!(!hpol && !vpol))
			*same_pol = FALSE;
	}

	return 0;
}

static int timingdetect_get_vfreq_tolerance(
	u32 vfreqx1000,
	u16 *tolerance)
{
	int ret = 0;

	if (tolerance == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (vfreqx1000 < SRCCAP_TIMINGDETECT_VFREQX1000_25HZ +
		SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_500)
		*tolerance = SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_500;
	else if (vfreqx1000 < SRCCAP_TIMINGDETECT_VFREQX1000_60HZ +
		SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_1000)
		*tolerance = SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_1000;
	else
		*tolerance = SRCCAP_TIMINGDETECT_VFREQ_TOLERANCE_1500;

	return ret;
}

static int timingdetect_vrr_refreshrate_param_cal(
	u16 vend, u16 vstart, u16 hend, u16 hstart, u16 *vde, u16 *hde,
	u32 *htt, u32 *pixel_freq, bool frl, bool yuv420)
{
	if (vde == NULL || hde == NULL || htt == NULL || pixel_freq == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*vde = vend - vstart + 1;
	*hde = hend - hstart + 1;

	if (frl) {
		*htt = *htt * SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER;
		*hde = *hde * SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER;
	}
	if (yuv420) {
		*htt = *htt * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
		*hde = *hde * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
		*pixel_freq = *pixel_freq * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
	}
	return 0;
}

static int timingdetect_match_vd_sampling(
	enum v4l2_ext_avd_videostandardtype videotype,
	struct srccap_timingdetect_vdsampling *vdsampling_table,
	u16 table_entry_cnt,
	u16 *table_index,
	bool *listed)
{
	if (vdsampling_table == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (table_index == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	SRCCAP_MSG_INFO("%s:%u, %s:%d\n",
		"video type", videotype,
		"vdtable_entry_count", table_entry_cnt);

	for (*table_index = 0; (*table_index) < table_entry_cnt; (*table_index)++) {
		SRCCAP_MSG_INFO("[%u], %s:%d\n",
			*table_index, "status", vdsampling_table[*table_index].videotype);

		if (videotype == vdsampling_table[*table_index].videotype)
			break;
	}

	if (*table_index == table_entry_cnt) {
		SRCCAP_MSG_ERROR("video type not supported\n");
		*listed = FALSE;
		return -ERANGE;
	} else {
		*listed = TRUE;
	}

	return 0;
}

static int timingdetect_check_source(
	enum v4l2_srccap_input_source src,
	u16 status,
	bool *same_source)
{
	if (same_source == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (src) {

	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		if (status & SRCCAP_TIMINGDETECT_FLAG_HDMI)
			*same_source = true;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		if (status & SRCCAP_TIMINGDETECT_FLAG_DVI)
			*same_source = true;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		if (status & SRCCAP_TIMINGDETECT_FLAG_YPBPR)
			*same_source = true;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		if (status & SRCCAP_TIMINGDETECT_FLAG_VGA)
			*same_source = true;
		break;
	default:
		SRCCAP_MSG_ERROR("unknown src type\n");
		break;
	}

	return 0;
}

static int timingdetect_match_timing(
	bool hpol,
	bool vpol,
	bool interlaced,
	bool *listed,
	bool vrr,
	u32 hfreqx10,
	u32 vfreqx1000,
	u32 vtt,
	struct srccap_timingdetect_timing *timing_table,
	u16 table_entry_count,
	enum v4l2_srccap_input_source src,
	u16 *table_index)
{
	int ret = 0;
	u16 hfreq_tolerance = 0, vfreq_tolerance = 0;
	bool same_pol = FALSE;
	bool same_source = FALSE;

	if (timing_table == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (table_index == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	SRCCAP_MSG_INFO("%s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
		"table_entry_count", table_entry_count,
		"hpol", hpol,
		"vpol", vpol,
		"interlaced", interlaced,
		"hfreqx10", hfreqx10,
		"vfreqx1000", vfreqx1000,
		"vtt", vtt,
		"vrr", vrr);

	/* find matching timing from index 0 to number of table entries */
	for (*table_index = 0; *table_index < table_entry_count; (*table_index)++) {
		SRCCAP_MSG_INFO("[%u], %s:0x%x, %s:%u, %s:%u, %s:%u, %s:%u,\n",
			*table_index,
			"status", timing_table[*table_index].status,
			"hfreqx10", timing_table[*table_index].hfreqx10,
			"vfreqx1000", timing_table[*table_index].vfreqx1000,
			"vtt", timing_table[*table_index].vtt,
			"src", src);
		/* check hpol and vpol only if polarity is defined in the specific timing */
		ret = timingdetect_check_polarity(timing_table[*table_index].status,
			hpol, vpol, &same_pol);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		if (same_pol == FALSE)
			continue;

		/* check source */
		ret = timingdetect_check_source(src, timing_table[*table_index].status,
			&same_source);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		if (same_source == FALSE)
			continue;

		/* check interlaced */
		if ((timing_table[*table_index].status & SRCCAP_TIMINGDETECT_FLAG_INTERLACED)
			!= (interlaced << SRCCAP_TIMINGDETECT_INTERLACED_BIT))
			continue;

		/* increase hfreq tolerance if hfreqx10 is greater than 1000 */
		if (timing_table[*table_index].hfreqx10 > SRCCAP_TIMINGDETECT_HFREQX10_100)
			hfreq_tolerance = SRCCAP_TIMINGDETECT_HFREQ_TOLERANCE_HIGH;
		else
			hfreq_tolerance = SRCCAP_TIMINGDETECT_HFREQ_TOLERANCE;

		/* check hfreq */
		if ((hfreqx10 > timing_table[*table_index].hfreqx10 + hfreq_tolerance) ||
			(hfreqx10 < timing_table[*table_index].hfreqx10 - hfreq_tolerance))
			continue;

		ret = timingdetect_get_vfreq_tolerance(timing_table[*table_index].vfreqx1000,
			&vfreq_tolerance);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* check vfreq */
		if ((vfreqx1000 > timing_table[*table_index].vfreqx1000 + vfreq_tolerance) ||
			(vfreqx1000 < timing_table[*table_index].vfreqx1000 - vfreq_tolerance))
			continue;

		if (!vrr) {
			/* check vtt */
			if ((vtt > timing_table[*table_index].vtt +
				SRCCAP_TIMINGDETECT_VTT_TOLERANCE) ||
				(vtt < timing_table[*table_index].vtt -
				SRCCAP_TIMINGDETECT_VTT_TOLERANCE))
				continue;
		}

		SRCCAP_MSG_INFO("table index:%u\n", *table_index);
		SRCCAP_MSG_INFO
			("%s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%x, %s:%x\n",
			"hfreqx10", timing_table[*table_index].hfreqx10,
			"vfreqx1000", timing_table[*table_index].vfreqx1000,
			"hstart", timing_table[*table_index].hstart,
			"vstart", timing_table[*table_index].vstart,
			"hde", timing_table[*table_index].hde,
			"vde", timing_table[*table_index].vde,
			"htt", timing_table[*table_index].htt,
			"vtt", timing_table[*table_index].vtt,
			"adcphase", timing_table[*table_index].adcphase,
			"status", timing_table[*table_index].status);
		break;
	}

	if (*table_index == table_entry_count) {
		SRCCAP_MSG_ERROR("timing is not in table\n");
		*listed = FALSE;
	} else {
		*listed = TRUE;
	}

	return 0;
}

static bool timingdetect_check_null_pointer(
	u32 *hpd,
	u32 *vtt,
	u32 *vpd,
	bool *interlaced,
	bool *hpol,
	bool *vpol,
	u16 *hstart,
	u16 *vstart,
	u16 *hend,
	u16 *vend,
	bool *yuv420,
	bool *frl,
	u32 *htt,
	u8 *vs_pulse_width,
	u32 *fdet_vtt,
	u8 *ans_mute_status)
{
	bool ret = FALSE;

	if ((hpd == NULL) || (vtt == NULL) || (vpd == NULL) || (interlaced == NULL)
		|| (hpol == NULL) || (vpol == NULL) || (hstart == NULL) || (vstart == NULL)
		|| (hend == NULL) || (vend == NULL) || (yuv420 == NULL) || (frl == NULL)
		|| (htt == NULL) || (vs_pulse_width == NULL) || (fdet_vtt == NULL)
		|| (ans_mute_status == NULL))
		ret = TRUE;

	return ret;
}

static int timingdetect_get_sync_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	struct srccap_port_map map[V4L2_SRCCAP_INPUT_SOURCE_NUM],
	bool adc_offline_status,
	u32 *hpd,
	u32 *vtt)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCB;
	u16 hpd_u16 = 0, vtt_u16 = 0;
	bool offline_pad_compare = FALSE;

	if ((hpd == NULL) || (vtt == NULL))
		return -ENXIO;

	if (adc_offline_status == FALSE) {
		ret = timingdetect_map_detblock_reg(src, map, &detblock);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		//check if CVBS use same pad with YPBPR
		ret = mtk_adc_compare_offline_mux(srccap_dev, src,
			V4L2_SRCCAP_INPUT_SOURCE_YPBPR, &offline_pad_compare);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		if (offline_pad_compare == TRUE) //use same pad setting with ypbpr
			detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
		break;
	default:
		break;
	}

	ret = drv_hwreg_srccap_timingdetect_get_hperiod(detblock, &hpd_u16);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*hpd = (u32)hpd_u16;

	ret = drv_hwreg_srccap_timingdetect_get_vtotal(detblock, &vtt_u16);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*vtt = (u32)vtt_u16;

	return 0;
}


static int timingdetect_get_fake_atv_timing(
	u32 *hpd,
	u32 *vtt,
	u32 *vpd,
	bool *interlaced,
	bool *hpol,
	bool *vpol,
	u16 *hstart,
	u16 *vstart,
	u16 *hend,
	u16 *vend,
	bool *yuv420,
	bool *frl,
	u32 *htt,
	u8 *vs_pulse_width,
	u32 *fdet_vtt,
	u16 fdet_vtt_bound,
	u32 *pixel_freq,
	u8 *ans_mute_status)
{
	int i=0;

	if (timingdetect_check_null_pointer(hpd, vtt, vpd, interlaced, hpol, vpol, hstart, vstart,
			hend, vend, yuv420, frl, htt, vs_pulse_width, fdet_vtt, ans_mute_status)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}
	*hpd = SRCCAP_TIMINGDETECT_ATV_DEFAULT_HPD;
	*vtt = SRCCAP_TIMINGDETECT_ATV_DEFAULT_VTT;
	*vpd = SRCCAP_TIMINGDETECT_ATV_DEFAULT_VPD;
	*interlaced = SRCCAP_TIMINGDETECT_ATV_DEFAULT_INTERLACED;
	*hpol = 0;
	*vpol = 0;
	*hstart = SRCCAP_TIMINGDETECT_ATV_DEFAULT_HSTART;
	*vstart = SRCCAP_TIMINGDETECT_ATV_DEFAULT_VSTART;
	*hend = 0;
	*vend = 0;
	*yuv420 = 0;
	*frl = 0;
	*htt = SRCCAP_TIMINGDETECT_ATV_DEFAULT_HTT;
	*vs_pulse_width = 1;
	for (i=0; i<fdet_vtt_bound; i++)
		*(fdet_vtt+i) = SRCCAP_TIMINGDETECT_ATV_DEFAULT_VPD;
	*pixel_freq = SRCCAP_TIMINGDETECT_ATV_DEFAULT_PIXEL;
	*ans_mute_status = 0;

	return 0;
}


static int timingdetect_get_timing_info(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	u32 *hpd,
	u32 *vtt,
	u32 *vpd,
	bool *interlaced,
	bool *hpol,
	bool *vpol,
	u16 *hstart,
	u16 *vstart,
	u16 *hend,
	u16 *vend,
	bool *yuv420,
	bool *frl,
	u32 *htt,
	u8 *vs_pulse_width,
	u32 *fdet_vtt,
	u16 fdet_vtt_bound,
	u32 *pixel_freq,
	u8 *ans_mute_status)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	u16 hpd_u16 = 0, vtt_u16 = 0, htt_u16 = 0;
	struct reg_srccap_timingdetect_de_info info;

	if (timingdetect_check_null_pointer(hpd, vtt, vpd, interlaced, hpol, vpol, hstart, vstart,
			hend, vend, yuv420, frl, htt, vs_pulse_width, fdet_vtt, ans_mute_status)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&info, 0, sizeof(struct reg_srccap_timingdetect_de_info));

	ret = timingdetect_map_detblock_reg(src,
		srccap_dev->srccap_info.map, &detblock);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_hperiod(detblock, &hpd_u16);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*hpd = (u32)hpd_u16;

	ret = drv_hwreg_srccap_timingdetect_get_vtotal(detblock, &vtt_u16);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*vtt = (u32)vtt_u16;

	ret = drv_hwreg_srccap_timingdetect_get_vsync2vsync_pix_cnt(detblock, vpd);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_interlace_status(detblock, interlaced);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}


	if (src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI &&
		src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4 &&
		/* form HDMIRX, 0:postive, 1:negative */
		srccap_dev->srccap_info.stub_mode == FALSE) {
		*hpol = (bool)mtk_hdmirx_get_datainfo
			(srccap_dev, E_HDMI_GET_H_POLARITY_BY_SRC);
		*vpol = (bool)mtk_hdmirx_get_datainfo
			(srccap_dev, E_HDMI_GET_V_POLARITY_BY_SRC);

		/* fit the meaning of IP1, 1:postive 0:negative */
		*hpol = !(*hpol);
		*vpol = !(*vpol);

	} else {
		ret = drv_hwreg_srccap_timingdetect_get_hsync_polarity(detblock, hpol);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = drv_hwreg_srccap_timingdetect_get_vsync_polarity(detblock, vpol);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	ret = drv_hwreg_srccap_timingdetect_get_de_info(detblock, &info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_htt(detblock, &htt_u16);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*htt = (u32)htt_u16;

	ret = drv_hwreg_srccap_timingdetect_get_yuv420(detblock, yuv420);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_frl_mode(detblock, frl);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_vs_pulse_width(detblock, vs_pulse_width);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_fdet_vtt(detblock, fdet_vtt, fdet_vtt_bound);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_ans_mute(detblock, ans_mute_status);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	*pixel_freq = mtk_srccap_hdmirx_get_pixel_freq(src);

	*hstart = info.hstart;
	*vstart = info.vstart;
	*hend = info.hend;
	*vend = info.vend;

	return 0;
}

static int timingdetect_check_signal_sync(
	enum v4l2_srccap_input_source src,
	u32 hpd, u32 vtt, bool *has_sync)
{
	if (has_sync == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (SRCCAP_IS_VD_SRC(src)) {
		if (((hpd > 0) && (hpd != SRCCAP_TIMINGDETECT_HPD_MASK))
			&& ((vtt > 0) && (vtt != SRCCAP_TIMINGDETECT_VTT_MASK)))
			*has_sync = TRUE;
		else
			*has_sync = FALSE;
	} else {
		if (((hpd > SRCCAP_TIMINGDETECT_HPD_MIN)
			&& (hpd != SRCCAP_TIMINGDETECT_HPD_MASK))
			&& ((vtt > SRCCAP_TIMINGDETECT_VTT_MIN)
			&& (vtt != SRCCAP_TIMINGDETECT_VTT_MASK)))
			*has_sync = TRUE;
		else
			*has_sync = FALSE;
	}
	return 0;
}

static bool timingdetect_check_last_signal(
	u32 hpd,
	u32 old_hpd,
	u32 vtt,
	u32 old_vtt,
	bool interlaced,
	bool old_interlaced,
	bool hpol,
	bool old_hpol,
	bool vpol,
	bool old_vpol,
	bool bypass_v)
{
	int ret = FALSE;

	if ((hpd < SRCCAP_TIMINGDETECT_HPD_MIN
		|| hpd == SRCCAP_TIMINGDETECT_HPD_MASK)
		&& (old_hpd < SRCCAP_TIMINGDETECT_HPD_MIN
		|| old_hpd == SRCCAP_TIMINGDETECT_HPD_MASK)
		&& ((vtt < SRCCAP_TIMINGDETECT_VTT_MIN)
		|| (vtt == SRCCAP_TIMINGDETECT_VTT_MASK) || (bypass_v == TRUE))
		&& ((old_vtt < SRCCAP_TIMINGDETECT_VTT_MIN)
		|| (old_vtt == SRCCAP_TIMINGDETECT_VTT_MASK) || (bypass_v == TRUE)))
		ret = TRUE;

	return ret;
}

static bool timingdetect_check_last_timing(
	u32 hpd,
	u32 old_hpd,
	u32 vtt,
	u32 old_vtt,
	bool interlaced,
	bool old_interlaced,
	bool hpol,
	bool old_hpol,
	bool vpol,
	bool old_vpol,
	u16 hstart,
	u16 old_hstart,
	u16 vstart,
	u16 old_vstart,
	u16 hend,
	u16 old_hend,
	u16 vend,
	u16 old_vend,
	bool bypass_v)
{
	int ret = FALSE;

	if ((abs((s64)hpd - (s64)old_hpd) < SRCCAP_TIMINGDETECT_HPD_TOL)
		&& ((abs((s64)vtt - (s64)old_vtt) < SRCCAP_TIMINGDETECT_VTT_TOL)
			|| (bypass_v == TRUE))
		&& ((interlaced == old_interlaced)
			|| (bypass_v == TRUE))
		&& (hpol == old_hpol)
		&& (vpol == old_vpol)
		&& (abs((s64)hstart - (s64)old_hstart) < SRCCAP_TIMINGDETECT_HST_TOL)
		&& (abs((s64)vstart - (s64)old_vstart) < SRCCAP_TIMINGDETECT_VST_TOL)
		&& (abs((s64)hend - (s64)old_hend) < SRCCAP_TIMINGDETECT_HEND_TOL)
		&& (abs((s64)vend - (s64)old_vend) < SRCCAP_TIMINGDETECT_VEND_TOL)
		)
		ret = TRUE;

	return ret;
}

static bool timingdetect_check_same_timingvalue(
	u32 hpd,
	u32 old_hpd,
	u32 vtt,
	u32 old_vtt,
	bool interlaced,
	bool old_interlaced,
	bool hpol,
	bool old_hpol,
	bool vpol,
	bool old_vpol,
	u16 hstart,
	u16 old_hstart,
	u16 vstart,
	u16 old_vstart,
	u16 hend,
	u16 old_hend,
	u16 vend,
	u16 old_vend,
	bool bypass_v)
{
	int ret = FALSE;

	if ((hpd == old_hpd)
		&& ((vtt == old_vtt)
			|| (bypass_v == TRUE))
		&& ((interlaced == old_interlaced)
			|| (bypass_v == TRUE))
		&& (hpol == old_hpol)
		&& (vpol == old_vpol)
		&& (hstart == old_hstart)
		&& (vstart == old_vstart)
		&& (hend == old_hend)
		&& (vend == old_vend)
		)
		ret = TRUE;

	return ret;
}

static int timingdetect_compare_timing(
	u32 hpd,
	u32 old_hpd,
	u32 vtt,
	u32 old_vtt,
	bool interlaced,
	bool old_interlaced,
	bool hpol,
	bool old_hpol,
	bool vpol,
	bool old_vpol,
	u16 hstart,
	u16 old_hstart,
	u16 vstart,
	u16 old_vstart,
	u16 hend,
	u16 old_hend,
	u16 vend,
	u16 old_vend,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool vrr_enforcement,
	bool hdmi_free_sync_status,
	bool *same_timing,
	bool *same_timing_value)
{
	int same_stable_sync = 0, same_no_signal = 0;
	bool bypass_v = FALSE;

	if (same_timing == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if ((vrr_type == SRCCAP_TIMINGDETECT_VRR_TYPE_VRR)
		|| (vrr_type == SRCCAP_TIMINGDETECT_VRR_TYPE_CVRR)
		|| vrr_enforcement == TRUE
		|| hdmi_free_sync_status == TRUE)
		bypass_v = TRUE;

	/* check if all timing info are the same as last time */
	same_stable_sync = timingdetect_check_last_timing(
		hpd, old_hpd,
		vtt, old_vtt,
		interlaced, old_interlaced,
		hpol, old_hpol,
		vpol, old_vpol,
		hstart, old_hstart,
		vstart, old_vstart,
		hend, old_hend,
		vend, old_vend,
		bypass_v);

	/* check if there is no signal like last time */
	same_no_signal = timingdetect_check_last_signal(
		hpd, old_hpd,
		vtt, old_vtt,
		interlaced, old_interlaced,
		hpol, old_hpol,
		vpol, old_vpol,
		bypass_v);

	*same_timing_value = timingdetect_check_same_timingvalue(
		hpd, old_hpd,
		vtt, old_vtt,
		interlaced, old_interlaced,
		hpol, old_hpol,
		vpol, old_vpol,
		hstart, old_hstart,
		vstart, old_vstart,
		hend, old_hend,
		vend, old_vend,
		bypass_v);

	if (same_stable_sync)
		*same_timing = TRUE;
	else if (same_no_signal)
		*same_timing = TRUE;
	else
		*same_timing = FALSE;

	return 0;
}

static void timingdetect_set_pixel_monitor_size(
	u16 dev_id,
	struct reg_srccap_timingdetect_window win)
{
	enum pxm_return ret = E_PXM_RET_FAIL;
	struct pxm_size_info info;

	memset(&info, 0, sizeof(struct pxm_size_info));

	if (dev_id == 0)
		info.point = E_PXM_POINT_PRE_IP2_0_INPUT;
	else if (dev_id == 1)
		info.point = E_PXM_POINT_PRE_IP2_1_INPUT;
	else {
		SRCCAP_MSG_ERROR("dev_id(%u) is invalid!\n", dev_id);
		return;
	}
	info.win_id = 0;  // srccap not support time sharing, win id set 0
	info.size.h_size = (__u16)win.w;
	info.size.v_size = (__u16)win.h;

	ret = pxm_set_size(&info);
	if (ret != E_PXM_RET_OK)
		SRCCAP_MSG_ERROR("srccap set pxm size fail(%d)!\n", ret);
}

static enum srccap_timingdetect_vrr_type timingdetect_get_vrr_status(
	enum v4l2_srccap_input_source src,
	struct srccap_port_map map[V4L2_SRCCAP_INPUT_SOURCE_NUM])
{
	int ret = 0;
	enum reg_srccap_timingdetect_vrr_type vrr = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if ((src < V4L2_SRCCAP_INPUT_SOURCE_HDMI)
		|| (src > V4L2_SRCCAP_INPUT_SOURCE_HDMI4))
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;

	ret = timingdetect_map_detblock_reg(src, map, &detblock);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	}

	ret = drv_hwreg_srccap_timingdetect_get_vrr(detblock, &vrr);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	}

	if (vrr == REG_SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	else if (vrr == REG_SRCCAP_TIMINGDETECT_VRR_TYPE_VRR)
		return SRCCAP_TIMINGDETECT_VRR_TYPE_VRR;
	else if (vrr == REG_SRCCAP_TIMINGDETECT_VRR_TYPE_CVRR)
		return SRCCAP_TIMINGDETECT_VRR_TYPE_CVRR;
	else
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
}

static enum srccap_timingdetect_vrr_type timingdetect_get_debounce_vrr_status(
	struct srccap_timingdetect_ip1_src_info *ip1_src_info,
	enum v4l2_srccap_input_source src,
	struct srccap_port_map map[V4L2_SRCCAP_INPUT_SOURCE_NUM])
{
	enum srccap_timingdetect_vrr_type ret_vrr_type = SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	enum srccap_timingdetect_vrr_type prev_vrr_type = SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	//     NVRR  VRR   VVRR
	//NVRR x     x     x
	//VRR  v     x     x
	//CVRR x     x     x
	bool debounce_table[SRCCAP_TIMINGDETECT_VRR_TYPE_CNT][SRCCAP_TIMINGDETECT_VRR_TYPE_CNT] = {
									{FALSE, FALSE, FALSE},
									{TRUE, FALSE, FALSE},
									{FALSE, FALSE, FALSE}};
	if (ip1_src_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;
	}

	prev_vrr_type = ip1_src_info->info.vrr;

	if (prev_vrr_type < SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;

	if (ip1_src_info->info.freesync == TRUE) {
		ip1_src_info->info.vrr_debounce_count = 0;
		return SRCCAP_TIMINGDETECT_VRR_TYPE_VRR;
	}

	ret_vrr_type = timingdetect_get_vrr_status(src, map);

	if (ret_vrr_type < SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		return SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR;

	if (debounce_table[prev_vrr_type][ret_vrr_type]) {
		if (ip1_src_info->info.vrr_debounce_count < SRCCAP_TIMINGDETECT_VRR_DEBOUNCE_CNT) {
			ip1_src_info->info.vrr_debounce_count++;
			ret_vrr_type = prev_vrr_type;
		} else {
			ip1_src_info->info.vrr_debounce_count = 0;
		}
	} else {
		ip1_src_info->info.vrr_debounce_count = 0;
	}

	return ret_vrr_type;

}

static void timingdetect_calculate_freq(
	struct mtk_srccap_dev *srccap_dev,
	u32 hpd, u32 vpd, u32 xtal_clk,
	u32 *hfreqx10, u32 *vfreqx1000)
{
	u64 quotient = 0;
	u64 half_hpd = 0, half_vpd = 0;

	half_hpd = (u64)hpd;
	do_div(half_hpd, SRCCAP_TIMINGDETECT_DIVIDE_BASE_2);
	half_vpd = (u64)vpd;
	do_div(half_vpd, SRCCAP_TIMINGDETECT_DIVIDE_BASE_2);

	if ((hpd != 0) && (hfreqx10 != NULL)) {
		quotient = (xtal_clk * 10UL) + half_hpd; // round hfreqx10
		do_div(quotient, hpd);
		do_div(quotient, 1000UL);
		*hfreqx10 = (u32)quotient;
	}

	if ((vpd != 0) && (vfreqx1000 != NULL) && (srccap_dev != NULL)) {
		if (srccap_dev->srccap_info.cap.hw_version == 1) {
			quotient = (24000000UL * 1000UL) + half_vpd; // round vfreqx1000
			do_div(quotient, vpd);
			*vfreqx1000 = (u32)quotient;
		} else {
			quotient = ((u64)xtal_clk * 1000UL) + half_vpd; // round vfreqx1000
			do_div(quotient, vpd);
			*vfreqx1000 = (u32)quotient;
		}
	}
}

static u32 timingdetect_calculate_refreshrate(u16 v_de, u16 h_period, u32 xtal_clk, u16 h_de,
								u32 htt, u32 pixel_freq, u32 *vtt)
// vrr base rule table need v_de / h_de / htt / pixel_freq (add parameter: h_de, htt)
{
	u16 v_total = 0;
	u8 u8Index = 0;
	bool bFound = false;

	if (pixel_freq == SRCCAP_TIMINGDETECT_PIXEL_FREQ_ERROR || h_de <= 0 || htt <= 0 ||
	v_de <= 0)
		return 0;
	else {
		for (u8Index = 0; u8Index < ARRAY_SIZE(stVrrBaseFreqMappingTbl); u8Index++) {
			if (abs(pixel_freq/SRCCAP_TIMINGDETECT_VRR_DIV_1000 -
				stVrrBaseFreqMappingTbl[u8Index].u32PixelClock) >
							SRCCAP_TIMINGDETECT_VRR_RATE_TOLERANCE)
				continue;
			if (abs(stVrrBaseFreqMappingTbl[u8Index].u16Width - h_de) >
					SRCCAP_TIMINGDETECT_VRR_TIMING_TOLERANCE)
				continue;
			if (abs(stVrrBaseFreqMappingTbl[u8Index].u16Height - v_de) >
					SRCCAP_TIMINGDETECT_VRR_TIMING_TOLERANCE)
				continue;
			if (abs(stVrrBaseFreqMappingTbl[u8Index].u16HTT - htt) >
					SRCCAP_TIMINGDETECT_VRR_TIMING_TOLERANCE)
				continue;

			bFound = true;
			break;
		}
	}
	if (bFound == true) {
		*vtt = stVrrBaseFreqMappingTbl[u8Index].u16VTT;
		return stVrrBaseFreqMappingTbl[u8Index].u16FrameRate *
						SRCCAP_TIMINGDETECT_MULTIPLIER_10;
	}
	else {
		v_total = v_de * SRCCAP_TIMINGDETECT_TIMING_HDMI21_VTT_THRESHOLD /
			SRCCAP_TIMINGDETECT_TIMING_HDMI21_VDE_THRESHOLD;

		if (h_period > 0 && v_total > 0)
			return ((u32)(xtal_clk*SRCCAP_TIMINGDETECT_MULTIPLIER_10/h_period/v_total));
		else
			return 0;
	}
}

static int timingdetect_vrr_para_calc(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync_status,
	bool signal_status_changed,
	bool color_fmt_changed,
	bool vrr_changed,
	bool vrr_enforcement,
	u32 hpd, u32 vpd, bool *vrr, u32 *vfreqx1000,
	u16 vend, u16 vstart, u16 hend, u16 hstart, u16 *vde, u16 *hde,
	u32 *htt, u32 *vtt, u32 *pixel_freq, bool frl, bool yuv420,
	u32 *fdet_vtt)
{
	int ret = 0;
	u32 xtal_clk = 0;
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;

	if ((srccap_dev == NULL) || (vrr == NULL) || (vfreqx1000 == NULL) || (vde == NULL)
		|| (hde == NULL) || (htt == NULL) || (pixel_freq == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	xtal_clk = srccap_dev->srccap_info.cap.xtal_clk;
	src = srccap_dev->srccap_info.src;

	if (((vrr_type != SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		|| (hdmi_free_sync_status == TRUE)
		|| (vrr_enforcement == TRUE))
		&& (src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)
		&& (src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI))
		*vrr = TRUE;

	if (*vrr && (signal_status_changed || color_fmt_changed || vrr_changed)) {
		ret = timingdetect_vrr_refreshrate_param_cal(vend, vstart, hend, hstart,
					vde, hde, htt, pixel_freq, frl, yuv420);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		*vfreqx1000 = timingdetect_calculate_refreshrate(*vde, hpd, xtal_clk, *hde,
			*htt, *pixel_freq, vtt) * SRCCAP_TIMINGDETECT_REFRESH_TO_VFREQ_MULTIPLIER;

		fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_0] =
			SRCCAP_TIMINGDETECT_XTALI_12MX1000 / *vfreqx1000;
		fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_1] =
			SRCCAP_TIMINGDETECT_XTALI_12MX1000 / *vfreqx1000;
		fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_2] =
			SRCCAP_TIMINGDETECT_XTALI_12MX1000 / *vfreqx1000;
		fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_3] =
			SRCCAP_TIMINGDETECT_XTALI_12MX1000 / *vfreqx1000;
	}
	return ret;
}
static void timingdetect_disable_clk(struct clk *clk)
{
	if (__clk_is_enabled(clk))
		clk_disable_unprepare(clk);
}

int mtk_timingdetect_set_auto_nosignal(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool enable)
{
	int ret = 0;
	struct reg_srccap_timingdetect_watchdog dog;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&dog, 0, sizeof(struct reg_srccap_timingdetect_watchdog));

	ret = timingdetect_map_detblock_reg(
		src, srccap_dev->srccap_info.map, &detsrc);
	if (ret)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_main_det_mute_en(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_main_pkt_mute_en(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_hdmi_det_mute_en(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_hdmi_vmute_det_en(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_main_mute_usr_val(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	//set default as false, only enable when ans on
	ret = drv_hwreg_srccap_timingdetect_set_main_mute_usr_en(detsrc, FALSE, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	ret = drv_hwreg_srccap_timingdetect_set_ans_function(detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	return ret;
exit:
	SRCCAP_MSG_RETURN_CHECK(ret);
	return ret;
}

int mtk_timingdetect_set_auto_nosignal_watch_dog(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool enable)
{
	int ret = 0;
	struct reg_srccap_timingdetect_watchdog dog;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&dog, 0, sizeof(struct reg_srccap_timingdetect_watchdog));

	ret = timingdetect_map_detblock_reg(
		src, srccap_dev->srccap_info.map, &detsrc);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	SRCCAP_MSG_INFO("dev_id:%u, src:%d, det:%d, enable:%d\n",
		srccap_dev->dev_id, src, detsrc, enable);

	dog.en = enable;
	ret = drv_hwreg_srccap_timingdetect_set_watchdog(detsrc, dog, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_h_lose_watchdog(
					detsrc, enable, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static int timingdetect_set_auto_no_signal_mute(
	struct mtk_srccap_dev *srccap_dev,
	bool enable)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &detsrc);

	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_set_ans_mute(detsrc, enable, TRUE, NULL, NULL);
	if ((!ret) && (srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE))
		SRCCAP_MSG_INFO("%s Auto no signal.\n", enable?"Enable":"Disable");
	else
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}


static bool timingdetect_check_ans_in_stable(
	struct srccap_timingdetect_ip1_src_info *ip1_src_info,
	u8 ans_mute_status,
	bool same_timing)
{

	if (ip1_src_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	if ((ip1_src_info->status == V4L2_TIMINGDETECT_STABLE_SYNC)
		&& (ans_mute_status > 0)
		&& same_timing) {
		return TRUE;
	}

	return FALSE;
}


static int timingdetect_reset(enum reg_srccap_timingdetect_source detblock)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_reset(detblock, TRUE, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	usleep_range(SRCCAP_TIMINGDETECT_RESET_DELAY_1000, SRCCAP_TIMINGDETECT_RESET_DELAY_1100);

	ret = drv_hwreg_srccap_timingdetect_reset(detblock, FALSE, TRUE, NULL, NULL);
	if (ret < 0)
		goto exit;

	usleep_range(SRCCAP_TIMINGDETECT_RESET_DELAY_1000, SRCCAP_TIMINGDETECT_RESET_DELAY_1100);

	return ret;
exit:
	SRCCAP_MSG_RETURN_CHECK(ret);
	return ret;
}

static bool timingdetect_is_keep_meta_vfreq(
	struct mtk_srccap_dev *srccap_dev,
	u32 vfreqx1000,
	bool vrr_changed,
	bool signal_status_changed,
	bool color_format_changed,
	bool listed)
{
	u32 range = 0, pre_vfreq = 0, tol = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	pre_vfreq = srccap_dev->timingdetect_info.data.v_freqx1000;

	// for stable signal + vrr change case
	if (vrr_changed && !signal_status_changed && !color_format_changed) {
		if (listed) {
			tol = srccap_dev->srccap_info.cap.meta_vfreq_tol;
			if ((pre_vfreq - tol) <= vfreqx1000 && vfreqx1000 <= (pre_vfreq + tol)) {
				SRCCAP_MSG_INFO("vrr_chg:%d, sig_stat_chg:%d,fmt_chg:%d,"
					" cur_vfreq:%u, meta_vfreq:%u, meta_freq_tol:%u\n",
					vrr_changed, signal_status_changed, color_format_changed,
					vfreqx1000, pre_vfreq, tol);
				return TRUE;
			}
		} else {
			range = pre_vfreq * SRCCAP_TIMINGDETECT_HDMI_SPEC_NUMERATOR /
				SRCCAP_TIMINGDETECT_HDMI_SPEC_DENOMINATOR;
			if ((pre_vfreq - range) <= vfreqx1000 &&
				vfreqx1000 <= (pre_vfreq + range)) {
				SRCCAP_MSG_INFO("vrr_chg:%d, sig_stat_chg:%d,fmt_chg:%d,"
					" cur_vfreq:%u, meta_vfreq:%u, range:%u\n",
					vrr_changed, signal_status_changed, color_format_changed,
					vfreqx1000, pre_vfreq, range);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static int timingdetect_obtain_timing_parameter(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_data *data,
	enum srccap_timingdetect_vrr_type vrr_type,
	u8 vs_pulse_width,
	u16 hend,
	u16 vend,
	u16 index,
	u16 hstart,
	u16 vstart,
	u32 *fdet_vtt,
	u32 hpd,
	u32 htt,
	u32 vtt,
	u32 hfreqx10,
	u32 vfreqx1000,
	bool frl,
	bool hpol,
	bool vpol,
	bool yuv420,
	bool interlaced,
	bool listed,
	bool hdmi_free_sync_status,
	u32 pixel_freq,
	bool vrr_changed,
	bool signal_status_changed,
	bool color_format_changed)
{
	struct srccap_timingdetect_timing *timing_table;
	struct srccap_timingdetect_vdsampling *vdsampling_table;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (data == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (fdet_vtt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (srccap_dev->srccap_info.src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		timing_table = srccap_dev->timingdetect_info.timing_table;

		if (yuv420)
			data->h_de = (hend - hstart + 1) * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
		else
			data->h_de = hend - hstart + 1;

		if (frl)
			data->h_de = (data->h_de) * SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER;

		if (vrr_type == SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR && hdmi_free_sync_status != TRUE)
			data->interlaced = interlaced;
		else
			data->interlaced = 0;

		if (data->interlaced)
			data->v_de = vend - vstart;
		else
			data->v_de = vend - vstart + 1;

		if (listed) {
			if (vrr_type == SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR && hdmi_free_sync_status != TRUE) {
				data->v_freqx1000 = timing_table[index].vfreqx1000;
			} else {
				data->v_freqx1000 = vfreqx1000;
			}
			data->h_freqx10 = timing_table[index].hfreqx10;
			data->adc_phase = timing_table[index].adcphase;
			data->ce_timing = ((timing_table[index].status
				& SRCCAP_TIMINGDETECT_FLAG_CE_TIMING) >>
				SRCCAP_TIMINGDETECT_CE_TIMING_BIT);
		} else {
			data->v_freqx1000 = vfreqx1000;
			data->h_freqx10 = hfreqx10;
			data->adc_phase = 0;
			data->ce_timing = FALSE;
		}

		if (timingdetect_is_keep_meta_vfreq(srccap_dev, data->v_freqx1000,
			vrr_changed, signal_status_changed, color_format_changed, listed) == TRUE)
			data->v_freqx1000 =
				srccap_dev->timingdetect_info.data.v_freqx1000;
		data->h_start = hstart;
		data->v_start = vstart;
		data->vrr_type = vrr_type;
		data->h_period = hpd;
		data->h_total = htt;
		data->v_total = vtt;
		data->h_pol = hpol;
		data->v_pol = vpol;
		data->yuv420 = yuv420;
		data->frl = frl;
		data->refresh_rate = timingdetect_calculate_refreshrate(
			data->v_de, data->h_period, srccap_dev->srccap_info.cap.xtal_clk,
					data->h_de, data->h_total, pixel_freq, &vtt);

		data->vs_pulse_width = vs_pulse_width;
		data->fdet_vtt0 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_0];
		data->fdet_vtt1 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_1];
		data->fdet_vtt2 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_2];
		data->fdet_vtt3 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_3];
		srccap_dev->timingdetect_info.org_size_width =
			data->h_de;
		srccap_dev->timingdetect_info.org_size_height =
			data->v_de;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		timing_table = srccap_dev->timingdetect_info.timing_table;

		data->h_start = timing_table[index].hstart;
		data->v_start = timing_table[index].vstart;
		data->h_de = timing_table[index].hde;
		data->v_de = timing_table[index].vde;
		data->h_freqx10 = timing_table[index].hfreqx10;
		data->v_freqx1000 = timing_table[index].vfreqx1000;
		data->interlaced = interlaced;
		data->h_period = hpd;
		data->h_total = timing_table[index].htt;
		data->v_total = timing_table[index].vtt;
		data->h_pol = hpol;
		data->v_pol = vpol;
		data->adc_phase = timing_table[index].adcphase;
		data->yuv420 = yuv420;
		data->frl = frl;
		data->ce_timing = ((timing_table[index].status
			& SRCCAP_TIMINGDETECT_FLAG_CE_TIMING) >> SRCCAP_TIMINGDETECT_CE_TIMING_BIT);
		data->vs_pulse_width = vs_pulse_width;
		data->fdet_vtt0 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_0];
		data->fdet_vtt1 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_1];
		data->fdet_vtt2 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_2];
		data->fdet_vtt3 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_3];
		srccap_dev->timingdetect_info.org_size_width =
			data->h_de;
		srccap_dev->timingdetect_info.org_size_height =
			data->v_de;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		vdsampling_table = srccap_dev->timingdetect_info.vdsampling_table;

		data->h_start = vdsampling_table[index].h_start;
		data->v_start = vdsampling_table[index].v_start;
		data->h_de = vdsampling_table[index].h_de;
		data->v_de = vdsampling_table[index].v_de;
		data->v_freqx1000 = vdsampling_table[index].v_freqx1000;
		data->h_freqx10 = hfreqx10;
		data->interlaced = true;
		data->h_total = vdsampling_table[index].h_total;
		data->h_period = hpd;
		data->v_total = vtt;
		data->h_pol = hpol;
		data->v_pol = vpol;
		data->yuv420 = yuv420;
		data->frl = frl;
		data->vs_pulse_width = vs_pulse_width;
		data->fdet_vtt0 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_0];
		data->fdet_vtt1 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_1];
		data->fdet_vtt2 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_2];
		data->fdet_vtt3 = fdet_vtt[SRCCAP_TIMINGDETECT_FDET_VTT_3];
		srccap_dev->timingdetect_info.org_size_width =
			vdsampling_table[index].org_size_width;
		srccap_dev->timingdetect_info.org_size_height =
			vdsampling_table[index].org_size_height;
		break;
	default:
		break;
	}

	return 0;
}

static int timingdetect_set_hw_version(
	u32 hw_version,
	bool stub)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_hw_version(hw_version, stub);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

static enum srccap_timingdetect_ip1_type timingdetect_src_map_ip1(
		enum v4l2_srccap_input_source source)
{
	enum srccap_timingdetect_ip1_type ip1_source =
			SRCCAP_TIMINGDETECT_IP1_TYPE_MAX;

	switch (source) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI0;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_ADCA;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_VD;
		break;
	default:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_TYPE_MAX;
		break;
	}

	return (int)ip1_source;
}

static void timingdetect_change_signal(
		struct srccap_timingdetect_ip1_src_info *ip1_src_info,
		enum v4l2_timingdetect_status status)
{
	if (!ip1_src_info)
		return;

	if (ip1_src_info->status == status)
		return;

	SRCCAP_MSG_INFO(" %d signal change from %d to %d\n",
				ip1_src_info->ip1_source, ip1_src_info->status, status);

	ip1_src_info->status = status;
}


static int timingdetect_108oi_param_cal(
	u16 vend, u16 vstart, u16 hend, u16 hstart, u16 *vde, u16 *hde,
	u32 *htt, bool frl, bool yuv420, bool interlaced)
{
	if (vde == NULL || hde == NULL || htt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (interlaced)
		*vde = vend - vstart;
	else
		*vde = vend - vstart + 1;
	*hde = hend - hstart + 1;

	if (frl) {
		*htt = *htt * SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER;
		*hde = *hde * SRCCAP_TIMINGDETECT_FRL_8P_MULTIPLIER;
	}
	if (yuv420) {
		*htt = *htt * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
		*hde = *hde * SRCCAP_TIMINGDETECT_YUV420_MULTIPLIER;
	}
	return 0;
}


static int timingdetect_108oi_detect(
		struct mtk_srccap_dev *srccap_dev,
		struct srccap_timingdetect_ip1_src_info *ip1_src_info,
		u16 hde, u16 vde, u32 hfreqx10, u32 vfreqx1000, u32 vtt, bool interlaced)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (ip1_src_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	src = timingdetect_ip1_map_src(ip1_src_info->ip1_source);

	ret = timingdetect_map_detblock_reg(
		src, srccap_dev->srccap_info.map, &detsrc);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	SRCCAP_MSG_INFO("%s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
		"hde", hde,
		"vde", vde,
		"hfreqx10", hfreqx10,
		"vfreqx1000", vfreqx1000,
		"vtt", vtt,
		"interlaced", interlaced);

	if ((hde >= SRCCAP_TIMINGDETECT_108OI_WIDTH_MIN)
		&& (hde <= SRCCAP_TIMINGDETECT_108OI_WIDTH_MAX)
		&& (vde >=  SRCCAP_TIMINGDETECT_108OI_HEIGHT_MIN)
		&& (vde <=  SRCCAP_TIMINGDETECT_108OI_HEIGHT_MAX)
		&& (hfreqx10 >=  SRCCAP_TIMINGDETECT_108OI_HFREQX10_MIN)
		&& (hfreqx10 <=  SRCCAP_TIMINGDETECT_108OI_HFREQX10_MAX)
		&& (vfreqx1000 >=  SRCCAP_TIMINGDETECT_108OI_VFREQX10_MIN)
		&& (vfreqx1000 <=  SRCCAP_TIMINGDETECT_108OI_VFREQX10_MAX)
		&& (vtt >=  SRCCAP_TIMINGDETECT_108OI_VTT_MIN)
		&& (vtt <=  SRCCAP_TIMINGDETECT_108OI_VTT_MAX)
		&& (interlaced == 0)) {
		ret = drv_hwreg_srccap_timingdetect_set_1080i_detection_mode(
			detsrc, TRUE, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ip1_src_info->stable_count = 0;
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);
		ip1_src_info->euro108oi_status = SRCCAP_TIMINGDETECT_108OI_SETTING;
		SRCCAP_MSG_INFO("euro108oi_status = 1\n");


	} else if (ip1_src_info->euro108oi_status == SRCCAP_TIMINGDETECT_108OI_ENABLE) {
		ret = drv_hwreg_srccap_timingdetect_set_fdet_en(detsrc, FALSE, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		ip1_src_info->euro108oi_status = SRCCAP_TIMINGDETECT_108OI_DISABLE;
		SRCCAP_MSG_INFO("euro108oi_status = 0\n");

	}
	return ret;
}

static int timingdetect_check_108oi_setting(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_ip1_src_info *ip1_src_info)
{
	int ret = 0;
	u8 vs_pulse_width = 0;
	u32 xtal_clk = 0;
	u32 hfreqx10 = 0, hpd = 0, vtt = 0, vpd = 0, htt = 0, vfreqx1000 = 0, pixel_freq = 0;
	u16 hstart = 0, vstart = 0, hend = 0, vend = 0, vde = 0, hde = 0;
	bool interlaced = 0, hpol = 0, vpol = 0, yuv420 = 0, frl = 0;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	u8 ans_mute_status = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (ip1_src_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	xtal_clk = srccap_dev->srccap_info.cap.xtal_clk;
	src = timingdetect_ip1_map_src(ip1_src_info->ip1_source);

	if ((src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI
		&& src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)
		|| (src >= V4L2_SRCCAP_INPUT_SOURCE_YPBPR
		&& src <= V4L2_SRCCAP_INPUT_SOURCE_YPBPR3)) {

		/* get hpd, vtt, vpd, interlaced, hpol, vpol, hstart, vstart, hend, and vend */
		ret = timingdetect_get_timing_info(srccap_dev, src, &hpd, &vtt, &vpd, &interlaced,
			&hpol, &vpol, &hstart, &vstart, &hend, &vend, &yuv420, &frl, &htt,
			&vs_pulse_width, fdet_vtt, SRCCAP_FDET_VTT_NUM, &pixel_freq,
			&ans_mute_status);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		timingdetect_calculate_freq(srccap_dev, hpd, vpd, xtal_clk, &hfreqx10, &vfreqx1000);
		ret = timingdetect_108oi_param_cal(vend, vstart, hend, hstart,
				&vde, &hde, &htt, frl, yuv420, interlaced);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = timingdetect_108oi_detect(srccap_dev,  ip1_src_info,
			hde, vde, hfreqx10, vfreqx1000, vtt, interlaced);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
		}
	}
	return ret;
}

static int timingdetect_no_sync_debounce_check(
		struct mtk_srccap_dev *srccap_dev,
		struct srccap_timingdetect_ip1_src_info *ip1_src_info)
{
	int ret = 0;

	if (!ip1_src_info || !srccap_dev) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (ip1_src_info->status == V4L2_TIMINGDETECT_STABLE_SYNC)
		ip1_src_info->no_sync_count = 0;
	else if (ip1_src_info->status == V4L2_TIMINGDETECT_UNSTABLE_SYNC)
		ip1_src_info->no_sync_count++;

	if (ip1_src_info->no_sync_count >= srccap_dev->srccap_info.cap.no_sync_count)
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_NO_SIGNAL);
	else
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);
	return ret;
}

static void timingdetect_ignore_comp_hvstart(
		enum v4l2_srccap_input_source source,
		struct srccap_timingdetect_pre_monitor_info *pre_info,
		struct srccap_timingdetect_ip1_src_info *ip1_src_info)
{

	if (pre_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	if (ip1_src_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	if ((source == V4L2_SRCCAP_INPUT_SOURCE_YPBPR) ||
		(source == V4L2_SRCCAP_INPUT_SOURCE_YPBPR2) ||
		(source == V4L2_SRCCAP_INPUT_SOURCE_YPBPR3)) {
		pre_info->hstart = ip1_src_info->info.hstart;
		pre_info->vstart = ip1_src_info->info.vstart;
	}

}

/* this api just for hdmi/ypbpr... */
static int timingdetect_get_ip1_info(
		struct mtk_srccap_dev *srccap_dev,
		struct srccap_timingdetect_ip1_src_info *ip1_src_info)
{
	int ret = 0;
	bool has_sync = false, same_timing = false, same_timing_value = false;
	enum v4l2_srccap_input_source source = 0;
	enum v4l2_timingdetect_status pre_status = 0;
	struct srccap_timingdetect_pre_monitor_info pre_info;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	u8 ans_mute_status = 0;
	bool ans_stable_raise = FALSE;

	if (!ip1_src_info || !srccap_dev) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	source = timingdetect_ip1_map_src(ip1_src_info->ip1_source);

	/* restore the pre info */
	pre_status = ip1_src_info->status;
	memcpy(&pre_info, &ip1_src_info->info, sizeof(pre_info));

	/* set isog detect mode */
	ret = mtk_srccap_set_isog_detect_mode(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* get the cur info */
	ret = timingdetect_get_timing_info(srccap_dev, source,
				&ip1_src_info->info.hpd,
				&ip1_src_info->info.vtt,
				&ip1_src_info->info.vpd,
				&ip1_src_info->info.interlaced,
				&ip1_src_info->info.hpol,
				&ip1_src_info->info.vpol,
				&ip1_src_info->info.hstart,
				&ip1_src_info->info.vstart,
				&ip1_src_info->info.hend,
				&ip1_src_info->info.vend,
				&ip1_src_info->info.yuv420,
				&ip1_src_info->info.frl,
				&ip1_src_info->info.htt,
				&ip1_src_info->info.vs_pulse_width,
				fdet_vtt,
				SRCCAP_FDET_VTT_NUM,
				&ip1_src_info->info.pixel_freq,
				&ans_mute_status);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* get the has_sync... */
	ret = timingdetect_check_signal_sync(source,
			ip1_src_info->info.hpd, ip1_src_info->info.vtt, &has_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* no sync... RETURN */
	if (!has_sync) {
		ip1_src_info->stable_count = 0;
		ret = timingdetect_no_sync_debounce_check(srccap_dev, ip1_src_info);
		return ret;
	}
	ip1_src_info->no_sync_count = 0;

	//ypbpr dont need to compare hstar/vstart, those data from the timing table.
	timingdetect_ignore_comp_hvstart(source, &pre_info, ip1_src_info);

	ret = timingdetect_compare_timing(
				ip1_src_info->info.hpd, pre_info.hpd,
				ip1_src_info->info.vtt, pre_info.vtt,
				ip1_src_info->info.interlaced, pre_info.interlaced,
				ip1_src_info->info.hpol, pre_info.hpol,
				ip1_src_info->info.vpol, pre_info.vpol,
				ip1_src_info->info.hstart, pre_info.hstart,
				ip1_src_info->info.vstart, pre_info.vstart,
				ip1_src_info->info.hend, pre_info.hend,
				ip1_src_info->info.vend, pre_info.vend,
				ip1_src_info->info.vrr, false,
				ip1_src_info->info.freesync, &same_timing, &same_timing_value);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}


	/* set to unstable if ans raise but no timing change */
	ans_stable_raise = timingdetect_check_ans_in_stable(ip1_src_info, ans_mute_status,
										same_timing);
	if (ans_stable_raise) {
		SRCCAP_MSG_INFO("source=%d stable same_timing=%d but raise ans_mute_status=%d\n",
							source, same_timing, ans_mute_status);
		ip1_src_info->stable_count = 0;
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);
		return ret;
	}

	/* compare the signal stable count */
	if (!same_timing) {
		ip1_src_info->stable_count = 0;
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);

		SRCCAP_MSG_INFO("Src:%d, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
				source,
				"hpd", pre_info.hpd, ip1_src_info->info.hpd,
				"vtt", pre_info.vtt, ip1_src_info->info.vtt,
				"hpol", pre_info.hpol, ip1_src_info->info.hpol,
				"vpol", pre_info.vpol, ip1_src_info->info.vpol);

		SRCCAP_MSG_INFO("Src:%d, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
				source,
				"hst", pre_info.hstart, ip1_src_info->info.hstart,
				"vst", pre_info.vstart, ip1_src_info->info.vstart,
				"hend", pre_info.hend, ip1_src_info->info.hend,
				"vend", pre_info.vend, ip1_src_info->info.vend,
				"interlaced", pre_info.interlaced, ip1_src_info->info.interlaced);

		SRCCAP_MSG_INFO("Src:%d, %s:%d->%d, %s:%u, %s:%u, %s:%u\n",
				source,
				"status", pre_status, ip1_src_info->status,
				"same_timing", same_timing,
				"has_sync", has_sync,
				"count", ip1_src_info->stable_count);
		return ret;
	}
	if (!same_timing_value) {
		SRCCAP_MSG_INFO("Src:%d, %s@%u->%u, %s@%u->%u, %s@%u->%u, %s@%u->%u\n",
				source,
				"hpd", pre_info.hpd, ip1_src_info->info.hpd,
				"vtt", pre_info.vtt, ip1_src_info->info.vtt,
				"hpol", pre_info.hpol, ip1_src_info->info.hpol,
				"vpol", pre_info.vpol, ip1_src_info->info.vpol);

		SRCCAP_MSG_INFO("Src:%d, %s@%u->%u, %s@%u->%u, %s@%u->%u, %s@%u->%u, %s@%u->%u\n",
				source,
				"hst", pre_info.hstart, ip1_src_info->info.hstart,
				"vst", pre_info.vstart, ip1_src_info->info.vstart,
				"hend", pre_info.hend, ip1_src_info->info.hend,
				"vend", pre_info.vend, ip1_src_info->info.vend,
				"interlaced", pre_info.interlaced, ip1_src_info->info.interlaced);

		SRCCAP_MSG_INFO("Src:%d, %s@%d->%d, %s:%u, %s:%u, %s:%u\n",
				source,
				"status", pre_status, ip1_src_info->status,
				"same_timing", same_timing,
				"has_sync", has_sync,
				"count", ip1_src_info->stable_count);
	}

	if (pre_status == V4L2_TIMINGDETECT_NO_SIGNAL) {
		ip1_src_info->stable_count = 0;
		timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);
	} else if (pre_status == V4L2_TIMINGDETECT_UNSTABLE_SYNC) {
		ip1_src_info->stable_count++;
		if (ip1_src_info->stable_count < srccap_dev->srccap_info.cap.stable_count) {
			timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_UNSTABLE_SYNC);
		} else {
			ip1_src_info->stable_count = 0;
			ip1_src_info->no_sync_count = 0;
			timingdetect_change_signal(ip1_src_info, V4L2_TIMINGDETECT_STABLE_SYNC);


			ret = timingdetect_set_auto_no_signal_mute(srccap_dev, FALSE);
			if (ret < 0)
				SRCCAP_MSG_RETURN_CHECK(ret);

			if (ip1_src_info->euro108oi_status == SRCCAP_TIMINGDETECT_108OI_SETTING) {
				ip1_src_info->euro108oi_status = SRCCAP_TIMINGDETECT_108OI_ENABLE;
				SRCCAP_MSG_INFO("euro108oi_status = 2\n");
			} else
				ret = timingdetect_check_108oi_setting(srccap_dev, ip1_src_info);
		}
	}

	if ((pre_status != ip1_src_info->status) ||
		(ip1_src_info->status != V4L2_TIMINGDETECT_STABLE_SYNC)) {
		SRCCAP_MSG_DEBUG("Src:%d, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
				source,
				"hpd", pre_info.hpd, ip1_src_info->info.hpd,
				"vtt", pre_info.vtt, ip1_src_info->info.vtt,
				"hpol", pre_info.hpol, ip1_src_info->info.hpol,
				"vpol", pre_info.vpol, ip1_src_info->info.vpol);

		SRCCAP_MSG_DEBUG("Src:%d, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
				source,
				"hst", pre_info.hstart, ip1_src_info->info.hstart,
				"vst", pre_info.vstart, ip1_src_info->info.vstart,
				"hend", pre_info.hend, ip1_src_info->info.hend,
				"vend", pre_info.vend, ip1_src_info->info.vend,
				"interlaced", pre_info.interlaced, ip1_src_info->info.interlaced);

		SRCCAP_MSG_DEBUG("Src:%d, %s:%d->%d, %s:%u, %s:%u, %s:%u\n",
				source,
				"status", pre_status, ip1_src_info->status,
				"same_timing", same_timing,
				"has_sync", has_sync,
				"count", ip1_src_info->stable_count);
	}

	return ret;
}

/* this api just for adca...
 * the adca hw need more config to work fine.
 */
 /*
static int timingdetect_handle_status_change(
	struct srccap_timingdetect_ip1_src_info *ip1_src_info,
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_timingdetect_status status)
{
	int ret = 0;
	enum v4l2_srccap_input_source source = 0;
	enum srccap_adc_isog_detect_mode isog_mode = SRCCAP_ADC_ISOG_NORMAL_MODE;
	bool powerdown = FALSE;

	source = timingdetect_ip1_map_src(ip1_src_info->ip1_source);

	switch (status) {
	case V4L2_TIMINGDETECT_NO_SIGNAL:
		isog_mode = SRCCAP_ADC_ISOG_STANDBY_MODE;
		powerdown = TRUE;
		break;
	case V4L2_TIMINGDETECT_UNSTABLE_SYNC:
		isog_mode = SRCCAP_ADC_ISOG_NORMAL_MODE;
		powerdown = TRUE;

		// adc free run for pll clk enable
		ret = mtk_adc_set_freerun(srccap_dev, true);
		if (ret < 0)
			goto EXIT;

		break;
	case V4L2_TIMINGDETECT_STABLE_SYNC:
		// adc free run for pll clk disable
		ret = mtk_adc_set_freerun(srccap_dev, false);
		if (ret < 0)
			goto EXIT;

		break;
	default:
		break;
	}

	ret = mtk_adc_set_isog_detect_mode(srccap_dev, source, isog_mode);
	if (ret < 0)
		goto EXIT;

	ret = mtk_adc_set_iclamp_rgb_powerdown(srccap_dev, source, powerdown);
	if (ret < 0)
		goto EXIT;

EXIT:
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}
*/

static void timingdetect_ip1_hdmi_sig_handle(
		struct mtk_srccap_dev *srccap_dev,
		enum srccap_timingdetect_ip1_type ip1_source)
{
	int ret = 0;
	bool found = false;
	enum v4l2_srccap_input_source source = 0;
	struct srccap_timingdetect_ip1_src_info *list_head, *list_node;

	if (!srccap_dev)
		return;

	if (srccap_dev->dev_id != 0)
		return;

	list_head = srccap_dev->timingdetect_info.ip1_src_info;
	list_for_each_entry(list_node, &list_head->node, node) {
		if (list_node->ip1_source == ip1_source) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	source = timingdetect_ip1_map_src(ip1_source);

	if (srccap_dev->srccap_info.map[source].data_port == SRCCAP_INPUT_PORT_NONE)
		return;

	mutex_lock(&list_node->lock);
	ret = mtk_timingdetect_update_vrr_info(srccap_dev, list_node);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
	ret = timingdetect_get_ip1_info(srccap_dev, list_node);
	mutex_unlock(&list_node->lock);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	/* get status from hdmirx again */
	if (srccap_dev->srccap_info.stub_mode == 0 &&
		list_node->status == V4L2_TIMINGDETECT_STABLE_SYNC) {
		if (mtk_srccap_hdmirx_get_HDMIStatus(source) == false) {
			SRCCAP_MSG_INFO("get HDMIStatus fail in source=%d\n", source);
			list_node->status = V4L2_TIMINGDETECT_UNSTABLE_SYNC; // NOTICE!!!
		}
	}
}

static void timingdetect_ip1_adca_sig_handle(
		struct mtk_srccap_dev *srccap_dev,
		enum srccap_timingdetect_ip1_type ip1_source)
{
	int ret = 0;
	bool found = false;
	enum v4l2_srccap_input_source source = 0;
	struct srccap_timingdetect_ip1_src_info *list_head, *list_node;

	if (!srccap_dev)
		return;

	if (srccap_dev->dev_id != 0)
		return;

	list_head = srccap_dev->timingdetect_info.ip1_src_info;
	list_for_each_entry(list_node, &list_head->node, node) {
		if (list_node->ip1_source == ip1_source) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	source = timingdetect_ip1_map_src(ip1_source);

	mutex_lock(&list_node->lock);
	ret = timingdetect_get_ip1_info(srccap_dev, list_node);
	mutex_unlock(&list_node->lock);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
}

static int timingdetect_pre_signal_task(void *param)
{
	enum srccap_timingdetect_ip1_type ip1_source = 0;
	struct mtk_srccap_dev *srccap_dev = (struct mtk_srccap_dev *)param;

	int ret = 0;
	uint8_t ans_mute_status = 0;
	uint32_t polling_time_min = 0;
	uint32_t polling_time_max = 0;
	uint16_t hpd = 0;
	uint16_t vtt = 0;
	bool det_mute = 0;
	bool hdmi_status = 0;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	polling_time_min = srccap_dev->srccap_info.cap.polling_time_min_pre_signal_task;
	polling_time_max = srccap_dev->srccap_info.cap.polling_time_max_pre_signal_task;

	mtk_srccap_realtime_add_thread(srccap_dev,
		SRCCAP_PRE_SIGNAL_TASK, current->pid);

	while (!kthread_should_stop()) {
		if (!srccap_dev)
			goto task_sleep;


		if (srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE) {

			ret = timingdetect_map_detblock_reg(srccap_dev->srccap_info.src,
						srccap_dev->srccap_info.map, &detsrc);
			if (ret) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				goto task_sleep;
			}

			ret = drv_hwreg_srccap_timingdetect_get_ans_mute(detsrc, &ans_mute_status);
			if (ret) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				goto task_sleep;
			}

			if (ans_mute_status != srccap_dev->timingdetect_info.ans_mute_status) {
				ret = drv_hwreg_srccap_timingdetect_get_hperiod(detsrc, &hpd);
				if (ret < 0) {
					SRCCAP_MSG_RETURN_CHECK(ret);
					return ret;
				}

				ret = drv_hwreg_srccap_timingdetect_get_vtotal(detsrc, &vtt);
				if (ret < 0) {
					SRCCAP_MSG_RETURN_CHECK(ret);
					return ret;
				}

				ret = drv_hwreg_srccap_timingdetect_get_det_mute(detsrc, &det_mute);
				if (ret < 0) {
					SRCCAP_MSG_RETURN_CHECK(ret);
					return ret;
				}
				if (SRCCAP_IS_HDMI_SRC(srccap_dev->srccap_info.src))
					hdmi_status = mtk_srccap_hdmirx_get_HDMIStatus(
						srccap_dev->srccap_info.src);
				SRCCAP_MSG_INFO(
				"%s:0x%x, %s:0x%x, %s:0x%x, %s:0x%x, %s:%u, %s:%u, %s:%u\n",
				"detsrc", detsrc,
				"pre_ans_mute_status",
				srccap_dev->timingdetect_info.ans_mute_status,
				"ans_mute_status", ans_mute_status,
				"hdmi_status", hdmi_status,
				"hpd", hpd,
				"vtt", vtt,
				"det_mute", det_mute
				);

				srccap_dev->timingdetect_info.ans_mute_status = ans_mute_status;
				ret = drv_hwreg_srccap_timingdetect_set_main_mute_usr_en(detsrc,
				srccap_dev->timingdetect_info.ans_mute_status, TRUE, NULL, NULL);

				if (ret) {
					SRCCAP_MSG_RETURN_CHECK(ret);
					goto task_sleep;
				}
			}
		}

		for (ip1_source = SRCCAP_TIMINGDETECT_IP1_START;
			ip1_source < SRCCAP_TIMINGDETECT_IP1_TYPE_MAX;
			ip1_source++) {

			if (ip1_source > SRCCAP_TIMINGDETECT_IP1_ADC_END)
				continue;

			if (ip1_source < SRCCAP_TIMINGDETECT_IP1_HDMI_END)
				timingdetect_ip1_hdmi_sig_handle(srccap_dev, ip1_source);
			else if (ip1_source < SRCCAP_TIMINGDETECT_IP1_ADC_END)
				timingdetect_ip1_adca_sig_handle(srccap_dev, ip1_source);
		}

task_sleep:
		usleep_range(polling_time_min, polling_time_max);
	}
	mtk_srccap_realtime_remove_thread(current->pid);
	return 0;
}

static int timingdetect_pre_monitor_init(
		struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	bool found = false;
	enum srccap_timingdetect_ip1_type idx = SRCCAP_TIMINGDETECT_IP1_START;
	struct srccap_timingdetect_ip1_src_info *_node, *_head, *_new = NULL;

	if (srccap_dev == NULL)
		return -EINVAL;

	/* we just need one pre_monitor task for all ip1 */
	if (srccap_dev->dev_id != 0)
		return 0;

	/* the head */
	if (srccap_dev->timingdetect_info.ip1_src_info == NULL) {
		_new = kzalloc(
				sizeof(struct srccap_timingdetect_ip1_src_info), GFP_KERNEL);
		if (!_new)
			return -EINVAL;

		INIT_LIST_HEAD(&_new->node); // must
		srccap_dev->timingdetect_info.ip1_src_info = _new;
	}

	_head = srccap_dev->timingdetect_info.ip1_src_info;
	for (idx = 0; idx < SRCCAP_TIMINGDETECT_IP1_TYPE_MAX; idx++) {
		list_for_each_entry(_node, &_head->node, node) {
			if (_node->ip1_source == idx) {
				found = true;
				break;
			}
		}

		if (found) {
			_node->stable_count = 0;
			_node->no_sync_count = 0;
			_node->status = V4L2_TIMINGDETECT_NO_SIGNAL;
			memset(&_node->info, 0,
				sizeof(struct srccap_timingdetect_pre_monitor_info));
			_node->euro108oi_status = SRCCAP_TIMINGDETECT_108OI_DISABLE;
			continue;
		}

		/* currently, fast source switch just support the hdmi/adca source */
		if (idx >= SRCCAP_TIMINGDETECT_IP1_ADC_END)
			continue;

		_new = kzalloc(
				sizeof(struct srccap_timingdetect_ip1_src_info), GFP_KERNEL);
		if (!_new)
			continue;

		INIT_LIST_HEAD(&_new->node);

		_new->status = V4L2_TIMINGDETECT_NO_SIGNAL;
		_new->ip1_source = idx;

		mutex_init(&_new->lock);

		list_add_tail(&_new->node, &_head->node);
	}

	if (srccap_dev->timingdetect_info.task == NULL) {
		srccap_dev->timingdetect_info.task = kthread_create(
							timingdetect_pre_signal_task,
							srccap_dev,
							"pre_signal_task");

		if (IS_ERR_OR_NULL(srccap_dev->timingdetect_info.task))
			srccap_dev->timingdetect_info.task = NULL;
		else
			wake_up_process(srccap_dev->timingdetect_info.task);
	}
	return ret;
}

static int timingdetect_set_fdet_default_setting(
	enum reg_srccap_timingdetect_source detblock)
{
	int ret = 0;

	ret = drv_hwreg_srccap_timingdetect_set_fdet_vtt_pix_cnt_en(
		detblock, TRUE, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	ret = drv_hwreg_srccap_timingdetect_set_fdet_check_en(
		detblock, TRUE, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	return ret;
}

static  enum v4l2_srccap_input_source timingdetect_get_adca_source(void)
{
	enum v4l2_srccap_input_source adca_src = 0;

	if (SRCCAP_FORCE_SCART)
		adca_src = V4L2_SRCCAP_INPUT_SOURCE_SCART;
	else if (SRCCAP_FORCE_VGA)
		adca_src = V4L2_SRCCAP_INPUT_SOURCE_VGA;
	else
		adca_src = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;

	return adca_src;
}

static  enum v4l2_srccap_input_source timingdetect_get_vd_source(void)
{
	enum v4l2_srccap_input_source vd_src = 0;

	if (SRCCAP_FORCE_SVIDEO)
		vd_src = V4L2_SRCCAP_INPUT_SOURCE_SVIDEO;
	else if (SRCCAP_FORCE_SCART)
		vd_src = V4L2_SRCCAP_INPUT_SOURCE_SCART;
	else
		vd_src = V4L2_SRCCAP_INPUT_SOURCE_CVBS;

	return vd_src;
}

/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
static int mtk_timingdetect_get_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_timingdetect_status *status)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (status == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*status = srccap_dev->timingdetect_info.status;

	return 0;
}

static int mtk_timingdetect_get_timing(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_timingdetect_timing *timing)
{
	u32 hfreqx10 = 0, hpd = 0, vtt = 0, vpd = 0, htt = 0, vfreqx1000 = 0, pixel_freq = 0;
	u16 hstart = 0, vstart = 0, hend = 0, vend = 0;
	bool interlaced = 0, hpol = 0, vpol = 0, yuv420 = 0, frl = 0;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	u32 xtal_clk = 0;
	u8 ans_mute_status = 0;
	u8 vs_pulse_width = 0;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (timing == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	timing->vfreqx1000 = srccap_dev->timingdetect_info.data.v_freqx1000;
	timing->hstart = srccap_dev->timingdetect_info.data.h_start;
	timing->vstart = srccap_dev->timingdetect_info.data.v_start;
	timing->hde = srccap_dev->timingdetect_info.data.h_de;
	timing->vde = srccap_dev->timingdetect_info.data.v_de;
	timing->htt = srccap_dev->timingdetect_info.data.h_total;
	timing->vtt = srccap_dev->timingdetect_info.data.v_total;
	timing->adcphase = srccap_dev->timingdetect_info.data.adc_phase;
	timing->interlaced = srccap_dev->timingdetect_info.data.interlaced;
	timing->ce_timing = srccap_dev->timingdetect_info.data.ce_timing;
	timing->vs_pulse_width = srccap_dev->timingdetect_info.data.vs_pulse_width;
	timing->fdet_vtt0 = srccap_dev->timingdetect_info.data.fdet_vtt0;
	timing->fdet_vtt1 = srccap_dev->timingdetect_info.data.fdet_vtt1;
	timing->fdet_vtt2 = srccap_dev->timingdetect_info.data.fdet_vtt2;
	timing->fdet_vtt3 = srccap_dev->timingdetect_info.data.fdet_vtt3;

	ret = timingdetect_get_timing_info(srccap_dev,
		srccap_dev->srccap_info.src, &hpd, &vtt, &vpd, &interlaced,
		&hpol, &vpol, &hstart, &vstart, &hend, &vend, &yuv420, &frl, &htt,
		&vs_pulse_width, fdet_vtt, SRCCAP_FDET_VTT_NUM, &pixel_freq,
		&ans_mute_status);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	xtal_clk = srccap_dev->srccap_info.cap.xtal_clk;

	timingdetect_calculate_freq(srccap_dev, hpd, vpd,
				xtal_clk,
				&hfreqx10, &vfreqx1000);

	timing->cur_vfreqx1000 = vfreqx1000;

	SRCCAP_MSG_INFO(
		"[CTRL_FLOW]g_timing(%u): %s:%u %s:%u %s:%u %s:%u %s:%u %s:%u %s:%u\n",
		srccap_dev->dev_id,
		"vfreq1000", timing->vfreqx1000,
		"hstart", timing->hstart,
		"vstart", timing->vstart,
		"hde", timing->hde,
		"vde", timing->vde,
		"htt", timing->htt,
		"vtt", timing->vtt);
	SRCCAP_MSG_INFO(
		"%s:%u, %s:%d, %s:%d, %s:%u, %s:0x%x, %s:0x%x\n",
		"adcphase", timing->adcphase,
		"interlaced", timing->interlaced,
		"ce", timing->ce_timing,
		"vs_pulse_width", timing->vs_pulse_width,
		"fdet_vtt0", timing->fdet_vtt0,
		"fdet_vtt1", timing->fdet_vtt1);
	SRCCAP_MSG_INFO(
		"%s:0x%x, %s:0x%x\n",
		"fdet_vtt2", timing->fdet_vtt2,
		"fdet_vtt3", timing->fdet_vtt3);

	SRCCAP_MSG_INFO(
		"[CTRL_FLOW]g_timing(%u):cur timing info: %s:%u %s:%u %s:%u %s:%u %s:%u\n",
		srccap_dev->dev_id,
		"cur_vfreq1000", vfreqx1000,
		"hpd", hpd,
		"vpd", vpd,
		"hfreqx10", hfreqx10,
		"xtal_clk", xtal_clk);
	return 0;
}

static int mtk_timingdetect_force_vrr(struct mtk_srccap_dev *srccap_dev, bool enable)
{
	int ret = 0;
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	enum reg_srccap_timingdetect_source detsrc = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	struct v4l2_ext_hdmi_get_freesync_info freesync_info;
	bool freesync = FALSE;
	bool en = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&freesync_info, 0, sizeof(struct v4l2_ext_hdmi_get_freesync_info));
	srccap_dev->timingdetect_info.data.vrr_enforcement = enable;
	SRCCAP_MSG_INFO("vrr_enforcement: %d", enable);

	src = srccap_dev->srccap_info.src;
	ret = timingdetect_map_detblock_reg(src, srccap_dev->srccap_info.map, &detsrc);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	freesync = mtk_srccap_hdmirx_get_freesync_info(src, &freesync_info);

	if ((srccap_dev->timingdetect_info.data.vrr_enforcement == TRUE)
		|| (srccap_dev->timingdetect_info.data.vrr_type
			!= SRCCAP_TIMINGDETECT_VRR_TYPE_NVRR)
		|| (freesync == TRUE))
		en = TRUE;

	ret = drv_hwreg_srccap_timingdetect_set_ans_vtt_det
		(detsrc, en, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (en == TRUE) {
		ret = drv_hwreg_srccap_timingdetect_set_ans_mute(detsrc, FALSE, TRUE, NULL, NULL);
		if (ret < 0)
			SRCCAP_MSG_RETURN_CHECK(ret);
	}
	return ret;
}

static int _timingdetect_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, timingdetect_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_TIMINGDETECT_STATUS:
	{
		enum v4l2_timingdetect_status status = V4L2_TIMINGDETECT_NO_SIGNAL;

		SRCCAP_MSG_DEBUG("V4L2_CID_TIMINGDETECT_STATUS\n");

		ret = mtk_timingdetect_get_status(srccap_dev, &status);
		ctrl->val = (s32)status;
		break;
	}
	case V4L2_CID_TIMINGDETECT_TIMING:
	{
		SRCCAP_MSG_DEBUG("V4L2_CID_TIMINGDETECT_TIMING\n");

		ret = mtk_timingdetect_get_timing(srccap_dev,
			(struct v4l2_timingdetect_timing *)(ctrl->p_new.p_u8));
		break;
	}
	default:
	{
		ret = -1;
		break;
	}
	}

	return ret;
}

static int _timingdetect_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, timingdetect_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_TIMINGDETECT_VRR_ENFORCEMENT:
	{
		bool vrr = (bool)(ctrl->val);

		SRCCAP_MSG_DEBUG("V4L2_CID_TIMINGDETECT_VRR_ENFORCEMENT\n");

		ret = mtk_timingdetect_force_vrr(srccap_dev, vrr);
		break;
	}
	default:
	{
		ret = -1;
		break;
	}
	}

	return ret;
}

static const struct v4l2_ctrl_ops timingdetect_ctrl_ops = {
	.g_volatile_ctrl = _timingdetect_g_ctrl,
	.s_ctrl = _timingdetect_s_ctrl,
};

static const struct v4l2_ctrl_config timingdetect_ctrl[] = {
	{
		.ops = &timingdetect_ctrl_ops,
		.id = V4L2_CID_TIMINGDETECT_STATUS,
		.name = "Srccap TimingDetect Status",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &timingdetect_ctrl_ops,
		.id = V4L2_CID_TIMINGDETECT_TIMING,
		.name = "Srccap TimingDetect Timing",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_timingdetect_timing)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &timingdetect_ctrl_ops,
		.id = V4L2_CID_TIMINGDETECT_VRR_ENFORCEMENT,
		.name = "Srccap TimingDetect VRR Enforce",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			|V4L2_CTRL_FLAG_VOLATILE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops timingdetect_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops timingdetect_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_timingdetect_init(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	enum v4l2_srccap_input_source vd_src = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	enum v4l2_srccap_input_source adca_src = V4L2_SRCCAP_INPUT_SOURCE_NONE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = timingdetect_set_hw_version(srccap_dev->srccap_info.cap.hw_version, FALSE);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* set a default source for each timingdetect hardware block */
	adca_src = timingdetect_get_adca_source();

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA,
		adca_src);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_ADCB,
		V4L2_SRCCAP_INPUT_SOURCE_YPBPR);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	vd_src = timingdetect_get_vd_source();

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_VD,
		vd_src);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI,
		V4L2_SRCCAP_INPUT_SOURCE_HDMI);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI2,
		V4L2_SRCCAP_INPUT_SOURCE_HDMI2);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI3,
		V4L2_SRCCAP_INPUT_SOURCE_HDMI3);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_set_source(srccap_dev, REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI4,
		V4L2_SRCCAP_INPUT_SOURCE_HDMI4);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* disable auto no signal mute for all timingdetect hardware blocks */
	for (detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
		detblock <= REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI4;
		detblock++) {
		ret = drv_hwreg_srccap_timingdetect_set_ans_mute(detblock, FALSE, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = drv_hwreg_srccap_timingdetect_set_no_signal_gen(detblock,
			FALSE, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* set default value for fdet setting */
		ret = timingdetect_set_fdet_default_setting(detblock);
		if (ret) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	if (srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_4
		|| srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_5
		|| srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_6) {
		for (detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
			detblock <= REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI4;
			detblock++) {
			ret = drv_hwreg_srccap_timingdetect_set_xtali_div(detblock,
				TRUE, TRUE, NULL, NULL);
			if (ret) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}
		}
	}

	ret = timingdetect_pre_monitor_init(srccap_dev);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

int mtk_timingdetect_init_offline(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCB;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = timingdetect_reset(detblock);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

int mtk_timingdetect_init_clock(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct clk *clk = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* set timingdetect xtal clock */
	clk = srccap_dev->srccap_info.clk.ip1_xtal_ck;
	if (srccap_dev->srccap_info.cap.xtal_clk == 12000000)
		ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_ip1_xtal_ck_xtal_12m_ck");
	else if (srccap_dev->srccap_info.cap.xtal_clk == 24000000)
		ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_ip1_xtal_ck_xtal_24m_ck");
	else
		ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_ip1_xtal_ck_xtal_48m_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	/* set adc timingdetect clock */
	clk = srccap_dev->srccap_info.clk.adc_ck;
	ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_adc_ck_adc_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	/* set hdmi timingdetect clock */
	clk = srccap_dev->srccap_info.clk.hdmi_ck;
	ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_hdmi_ck_hdmi_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	/* set hdmi2 timingdetect clock */
	clk = srccap_dev->srccap_info.clk.hdmi2_ck;
	ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_hdmi2_ck_hdmi2_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	/* set hdmi3 timingdetect clock */
	clk = srccap_dev->srccap_info.clk.hdmi3_ck;
	ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_hdmi3_ck_hdmi3_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	/* set hdmi4 timingdetect clock */
	clk = srccap_dev->srccap_info.clk.hdmi4_ck;
	ret = timingdetect_enable_parent(srccap_dev, clk, "srccap_hdmi4_ck_hdmi4_ck");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -ENOENT;
	}

	return 0;
}

int mtk_timingdetect_deinit(struct mtk_srccap_dev *srccap_dev)
{
	struct clk *clk = NULL;
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id == 0) {
		clk = srccap_dev->avd_info.stclock.cmbai2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.cmbao2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.cmbbi2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.cmbbo2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.mcu_mail02vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.mcu_mail12vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.smi2mcu_m2mcu;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.smi2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.vd2x2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.vd_32fsc2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.vd2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.xtal_12m2vd;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2riu;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.xtal_12m2mcu_m2mcu;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.clk_vd_atv_input;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.clk_vd_cvbs_input;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->avd_info.stclock.clk_buf_sel;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.ip1_xtal_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi_idclk_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi2_idclk_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi3_idclk_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi4_idclk_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.adc_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi2_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi3_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.hdmi4_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
		clk = srccap_dev->srccap_info.clk.ipdma0_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
	} else if (srccap_dev->dev_id == 1) {
		clk = srccap_dev->srccap_info.clk.ipdma1_ck;
		timingdetect_put_clk(srccap_dev->dev, clk);
	}

	if (srccap_dev->timingdetect_info.task != NULL) {
		kthread_stop(srccap_dev->timingdetect_info.task);
		put_task_struct(srccap_dev->timingdetect_info.task);
		srccap_dev->timingdetect_info.task = NULL;
	}

	return 0;
}

int mtk_timingdetect_deinit_clock(struct mtk_srccap_dev *srccap_dev)
{
	struct clk *clk = NULL;
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	clk = srccap_dev->srccap_info.clk.ip1_xtal_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.adc_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi2_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi3_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi4_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi_idclk_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi2_idclk_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi3_idclk_ck;
	timingdetect_disable_clk(clk);
	clk = srccap_dev->srccap_info.clk.hdmi4_idclk_ck;
	timingdetect_disable_clk(clk);

	return 0;
}

int mtk_timingdetect_set_capture_win(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_rect cap_win)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	struct reg_srccap_timingdetect_window win;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&win, 0, sizeof(struct reg_srccap_timingdetect_window));

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if ((src >= REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI)
		&& (src <= REG_SRCCAP_TIMINGDETECT_SOURCE_HDMI4)
		&& (srccap_dev->timingdetect_info.data.interlaced == true))
		win.y = cap_win.top + 1;
	else
		win.y = cap_win.top;

	win.x = cap_win.left;
	win.w = cap_win.width;
	win.h = cap_win.height;

	if (srccap_dev->srccap_info.stub_mode)
		ret = drv_hwreg_srccap_timingdetect_set_capture_win(src, win, TRUE, NULL, NULL);
	else {
#if IS_ENABLED(CONFIG_OPTEE)
		TEEC_Session *pstSession = srccap_dev->memout_info.secure_info.pstSession;
		TEEC_Operation op = {0};
		u32 error = 0;

		op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&win;
		op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.size =
				sizeof(struct reg_srccap_timingdetect_window);
		op.params[SRCCAP_CA_SMC_PARAM_IDX_1].value.a = (u32)src;
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
				TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
		ret = mtk_common_ca_teec_invoke_cmd(srccap_dev, pstSession,
				E_XC_OPTEE_SET_IPCAP_WIN, &op, &error);
#else
		ret = drv_hwreg_srccap_timingdetect_set_capture_win(src, win, TRUE, NULL, NULL);
#endif
	}
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	timingdetect_set_pixel_monitor_size(srccap_dev->dev_id, win);

	return 0;
}

int mtk_timingdetect_retrieve_timing(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_timingdetect_vrr_type vrr_type,
	bool hdmi_free_sync_status,
	struct srccap_timingdetect_data *data,
	enum v4l2_timingdetect_status status,
	bool signal_status_changed,
	bool color_fmt_changed,
	bool vrr_changed)
{
	int ret = 0;
	u8 vs_pulse_width = 0;
	u16 table_entry_count = 0, table_index = 0;
	struct srccap_timingdetect_timing *timing_table;
	struct srccap_timingdetect_vdsampling *vdsampling_table;
	u32 xtal_clk = 0;
	u32 hfreqx10 = 0, hpd = 0, vtt = 0, vpd = 0, htt = 0, vfreqx1000 = 0, pixel_freq = 0;
	u16 hstart = 0, vstart = 0, hend = 0, vend = 0, vde = 0, hde = 0;
	bool interlaced = 0, hpol = 0, vpol = 0, yuv420 = 0, frl = 0, vrr = 0, listed = 0;
	bool vrr_enforcement = FALSE;
	enum v4l2_ext_avd_videostandardtype videotype = V4L2_EXT_AVD_STD_PAL_BGHI;
	enum v4l2_srccap_input_source src = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	u8 ans_mute_status = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (data == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	xtal_clk = srccap_dev->srccap_info.cap.xtal_clk;
	src = srccap_dev->srccap_info.src;

	/* get hpd, vtt, vpd, interlaced, hpol, vpol, hstart, vstart, hend, and vend */
	ret = timingdetect_get_timing_info(srccap_dev, src, &hpd, &vtt, &vpd, &interlaced, &hpol,
		&vpol, &hstart, &vstart, &hend, &vend, &yuv420, &frl, &htt, &vs_pulse_width,
		fdet_vtt, SRCCAP_FDET_VTT_NUM, &pixel_freq, &ans_mute_status);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	if ( src == V4L2_SRCCAP_INPUT_SOURCE_ATV &&
		(status == V4L2_TIMINGDETECT_NO_SIGNAL_SCANNING
		|| status == V4L2_TIMINGDETECT_UNSTABLE_SCANNING)) {
		ret = timingdetect_get_fake_atv_timing(&hpd, &vtt,
			&vpd, &interlaced, &hpol, &vpol, &hstart, &vstart, &hend,
			&vend, &yuv420, &frl, &htt, &vs_pulse_width,
			fdet_vtt, SRCCAP_FDET_VTT_NUM, &pixel_freq, &ans_mute_status);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		/* match timing from timing table */
		table_entry_count = srccap_dev->timingdetect_info.table_entry_count;
		timing_table = srccap_dev->timingdetect_info.timing_table;
		vrr_enforcement = srccap_dev->timingdetect_info.data.vrr_enforcement;

		timingdetect_calculate_freq(srccap_dev, hpd, vpd, xtal_clk, &hfreqx10, &vfreqx1000);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = timingdetect_vrr_para_calc(srccap_dev, vrr_type,
			hdmi_free_sync_status, signal_status_changed,
			color_fmt_changed, vrr_changed, vrr_enforcement,
			hpd, vpd, &vrr, &vfreqx1000,
			vend, vstart, hend, hstart,
			&vde, &hde, &htt, &vtt, &pixel_freq, frl, yuv420,
			fdet_vtt);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		SRCCAP_MSG_INFO("(%u): %s=%d, %s=%d, %s=%d, %s=%u, "
			"%s=%u, %s=%u, %s=%u, %s=%u, %s=%u\n",
			srccap_dev->dev_id,
			"vrr_type", vrr_type,
			"vrr_enforcement", vrr_enforcement,
			"hdmi_free_sync_status", hdmi_free_sync_status,
			"xtal_clk", xtal_clk,
			"hpd", hpd,
			"vpd", vpd,
			"vde", vde,
			"hfreqx10", hfreqx10,
			"vfreqx1000", vfreqx1000);

		ret = timingdetect_match_timing(hpol, vpol, interlaced, &listed, vrr, hfreqx10,
			vfreqx1000, vtt, timing_table, table_entry_count, src, &table_index);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		/* match timing from timing table */
		table_entry_count = srccap_dev->timingdetect_info.table_entry_count;
		timing_table = srccap_dev->timingdetect_info.timing_table;

		timingdetect_calculate_freq(srccap_dev, hpd, vpd, xtal_clk, &hfreqx10, &vfreqx1000);

		ret = timingdetect_match_timing(hpol, vpol, interlaced, &listed, vrr, hfreqx10,
			vfreqx1000, vtt, timing_table, table_entry_count, src, &table_index);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		//recheck again, cause the TBL setting does not workable.
		if (listed == FALSE) {

			SRCCAP_MSG_INFO(
				"[CTRL_FLOW]fail retrieve_timin(%u): %s=%u %s=%u\n",
				srccap_dev->dev_id,
				"hpd", hpd,
				"vpd", vpd);

			usleep_range(SRCCAP_RETRIEVE_TIMING_SLEEP_BEGIN,
						SRCCAP_RETRIEVE_TIMING_SLEEP_END);

			ret = timingdetect_get_timing_info(srccap_dev, src, &hpd, &vtt, &vpd,
				&interlaced, &hpol, &vpol, &hstart, &vstart, &hend, &vend, &yuv420,
				&frl, &htt, &vs_pulse_width, fdet_vtt, SRCCAP_FDET_VTT_NUM,
								&pixel_freq, &ans_mute_status);
			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}

			SRCCAP_MSG_INFO(
				"[CTRL_FLOW]get retrieve_timin again(%u): %s=%u %s=%u\n",
				srccap_dev->dev_id,
				"hpd", hpd,
				"vpd", vpd);

			/* match timing from timing table */
			table_entry_count = srccap_dev->timingdetect_info.table_entry_count;
			timing_table = srccap_dev->timingdetect_info.timing_table;

			timingdetect_calculate_freq(srccap_dev, hpd, vpd, xtal_clk,
								&hfreqx10, &vfreqx1000);

			ret = timingdetect_match_timing(hpol, vpol, interlaced, &listed, vrr,
							hfreqx10, vfreqx1000, vtt, timing_table,
							table_entry_count, src, &table_index);
			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}
			if (listed == FALSE) {
				SRCCAP_MSG_INFO("[CTRL_FLOW]not in timing table\n");
				return -ERANGE;
			}
		}

		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		// get vd sampling table
		table_entry_count = srccap_dev->timingdetect_info.vdsampling_table_entry_count;
		vdsampling_table = srccap_dev->timingdetect_info.vdsampling_table;
		videotype = srccap_dev->avd_info.eStableTvsystem;

		if (hpd != 0)
			hfreqx10 = ((xtal_clk * 10) + (hpd / 2)) / hpd / 1000; // round hfreqx10

		ret = timingdetect_match_vd_sampling(videotype, vdsampling_table, table_entry_count,
			&table_index, &listed);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	/* save data */
	ret = timingdetect_obtain_timing_parameter(srccap_dev, data, vrr_type, vs_pulse_width, hend,
		vend, table_index, hstart, vstart, fdet_vtt, hpd, htt, vtt, hfreqx10, vfreqx1000,
		frl, hpol, vpol, yuv420, interlaced, listed, hdmi_free_sync_status, pixel_freq,
		vrr_changed, signal_status_changed, color_fmt_changed);

	return ret;
}
#define PATCH_ALIGN_4 (4)
int mtk_timingdetect_store_timing_info(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_data data)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev->timingdetect_info.data.h_start = data.h_start;
	srccap_dev->timingdetect_info.data.v_start = data.v_start;
	srccap_dev->timingdetect_info.data.h_de = data.h_de;
	srccap_dev->timingdetect_info.data.v_de = data.v_de;
	srccap_dev->timingdetect_info.data.h_freqx10 = data.h_freqx10;
	srccap_dev->timingdetect_info.data.v_freqx1000 = data.v_freqx1000;
	srccap_dev->timingdetect_info.data.interlaced = data.interlaced;
	srccap_dev->timingdetect_info.data.h_period = data.h_period;
	srccap_dev->timingdetect_info.data.h_total = data.h_total;
	srccap_dev->timingdetect_info.data.v_total = data.v_total;
	srccap_dev->timingdetect_info.data.h_pol = data.h_pol;
	srccap_dev->timingdetect_info.data.v_pol = data.v_pol;
	srccap_dev->timingdetect_info.data.adc_phase = data.adc_phase;
	srccap_dev->timingdetect_info.data.ce_timing = data.ce_timing;
	srccap_dev->timingdetect_info.data.yuv420 = data.yuv420;
	srccap_dev->timingdetect_info.data.frl = data.frl;
	srccap_dev->timingdetect_info.data.colorformat = data.colorformat;
	srccap_dev->timingdetect_info.data.vrr_type = data.vrr_type;
	srccap_dev->timingdetect_info.data.refresh_rate = data.refresh_rate;
	srccap_dev->timingdetect_info.data.vs_pulse_width = data.vs_pulse_width;
	srccap_dev->timingdetect_info.data.fdet_vtt0 = data.fdet_vtt0;
	srccap_dev->timingdetect_info.data.fdet_vtt1 = data.fdet_vtt1;
	srccap_dev->timingdetect_info.data.fdet_vtt2 = data.fdet_vtt2;
	srccap_dev->timingdetect_info.data.fdet_vtt3 = data.fdet_vtt3;

	/* patch starts */
	if (srccap_dev->timingdetect_info.data.interlaced)
		srccap_dev->timingdetect_info.data.v_de =
			srccap_dev->timingdetect_info.data.v_de
			/ PATCH_ALIGN_4 * PATCH_ALIGN_4;
	/* patch ends */

	SRCCAP_MSG_INFO(
		"[CTRL_FLOW]s_timing(%u): %s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%d %s=%u\n",
		srccap_dev->dev_id,
		"hstart", srccap_dev->timingdetect_info.data.h_start,
		"vstart", srccap_dev->timingdetect_info.data.v_start,
		"hde", srccap_dev->timingdetect_info.data.h_de,
		"vde", srccap_dev->timingdetect_info.data.v_de,
		"hfreq10", srccap_dev->timingdetect_info.data.h_freqx10,
		"vfreq1000", srccap_dev->timingdetect_info.data.v_freqx1000,
		"interlaced", srccap_dev->timingdetect_info.data.interlaced,
		"hpd", srccap_dev->timingdetect_info.data.h_period);
	SRCCAP_MSG_INFO(
		"%s=%u %s=%u %s=%d %s=%d %s=%u %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d\n",
		"htt", srccap_dev->timingdetect_info.data.h_total,
		"vtt", srccap_dev->timingdetect_info.data.v_total,
		"hpol", srccap_dev->timingdetect_info.data.h_pol,
		"vpol", srccap_dev->timingdetect_info.data.v_pol,
		"adcphase", srccap_dev->timingdetect_info.data.adc_phase,
		"ce", srccap_dev->timingdetect_info.data.ce_timing,
		"yuv420", srccap_dev->timingdetect_info.data.yuv420,
		"frl", srccap_dev->timingdetect_info.data.frl,
		"colorfmt", srccap_dev->timingdetect_info.data.colorformat,
		"vrrtype", srccap_dev->timingdetect_info.data.vrr_type,
		"refreshrate", srccap_dev->timingdetect_info.data.refresh_rate);
	SRCCAP_MSG_INFO(
		"%s=%u %s=0x%x %s=0x%x %s=0x%x %s=0x%x\n",
		"vs_pulse_wid", srccap_dev->timingdetect_info.data.vs_pulse_width,
		"fdet_vtt0", srccap_dev->timingdetect_info.data.fdet_vtt0,
		"fdet_vtt1", srccap_dev->timingdetect_info.data.fdet_vtt1,
		"fdet_vtt2", srccap_dev->timingdetect_info.data.fdet_vtt2,
		"fdet_vtt3", srccap_dev->timingdetect_info.data.fdet_vtt3);

	return 0;
}

int mtk_timingdetect_get_signal_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum v4l2_timingdetect_status *status)
{
	int ret = 0;
	u8 vs_pulse_width = 0;
	u16 dev_id = 0;
	static u64 count[SRCCAP_DEV_NUM];
	static enum v4l2_timingdetect_status old_status[SRCCAP_DEV_NUM];
	static u32 old_hpd[SRCCAP_DEV_NUM], old_vtt[SRCCAP_DEV_NUM];
	static bool old_interlaced[SRCCAP_DEV_NUM];
	static bool old_hpol[SRCCAP_DEV_NUM], old_vpol[SRCCAP_DEV_NUM];
	static u16 old_hstart[SRCCAP_DEV_NUM], old_vstart[SRCCAP_DEV_NUM];
	static u16 old_hend[SRCCAP_DEV_NUM], old_vend[SRCCAP_DEV_NUM];
	u32 hpd = 0, vtt = 0, vpd = 0, htt = 0, pixel_freq = 0;
	bool interlaced = 0, hpol = 0, vpol = 0, yuv420 = 0, frl = 0;
	u16 hstart = 0, vstart = 0, hend = 0, vend = 0;
	bool has_sync = FALSE, same_timing = FALSE, same_timing_value = FALSE;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	u8 ans_mute_status = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (status == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	dev_id = srccap_dev->dev_id;

	/* get hpd, vtt, vpd, interlaced, hpol, vpol, hstart, vstart, hend, and vend */
	ret = timingdetect_get_timing_info(srccap_dev, srccap_dev->srccap_info.src, &hpd, &vtt,
		&vpd, &interlaced, &hpol, &vpol, &hstart, &vstart, &hend, &vend, &yuv420, &frl,
		&htt, &vs_pulse_width, fdet_vtt, SRCCAP_FDET_VTT_NUM, &pixel_freq,
		&ans_mute_status);

	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* check if signal has sync */
	ret = timingdetect_check_signal_sync(src, hpd, vtt, &has_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	/* check if timing is the same */
	ret = timingdetect_compare_timing(hpd, old_hpd[dev_id], vtt, old_vtt[dev_id],
		interlaced, old_interlaced[dev_id],
		hpol, old_hpol[dev_id], vpol, old_vpol[dev_id],
		hstart, old_hstart[dev_id], vstart, old_vstart[dev_id],
		hend, old_hend[dev_id], vend, old_vend[dev_id],
		0, FALSE, FALSE, &same_timing, &same_timing_value);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	switch (old_status[dev_id]) {
	case V4L2_TIMINGDETECT_NO_SIGNAL:
		/* if there's signal, enter unstable sync */
		if (has_sync) {
			count[dev_id] = 0;
			*status = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		/* else do nothing */
		} else
			*status = V4L2_TIMINGDETECT_NO_SIGNAL;
		break;
	case V4L2_TIMINGDETECT_UNSTABLE_SYNC:
		/* if signal is the same, add count */
		if (same_timing) {
			count[dev_id]++;
			/* if count reaches stable count, enter no signal or stable state */
			if (count[dev_id] >= srccap_dev->srccap_info.cap.stable_count) {
				if (has_sync) {
					count[dev_id] = 0;
					*status = V4L2_TIMINGDETECT_STABLE_SYNC;
				} else {
					count[dev_id] = 0;
					*status = V4L2_TIMINGDETECT_NO_SIGNAL;
				}
			/* else do nothing */
			} else
				*status = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		/* else reset count */
		} else {
			count[dev_id] = 0;
			*status = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		}
		break;
	case V4L2_TIMINGDETECT_STABLE_SYNC:
		/* if signal is the same do nothing */
		if (has_sync && same_timing)
			*status = V4L2_TIMINGDETECT_STABLE_SYNC;
		/* else enter unstable state */
		else {
			count[dev_id] = 0;
			*status = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("undefined status\n");
		break;
	}

	SRCCAP_MSG_DEBUG("%s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
		"hpd", old_hpd[dev_id], hpd,
		"vtt", old_vtt[dev_id], vtt,
		"interlaced", old_interlaced[dev_id], interlaced,
		"hpol", old_hpol[dev_id], hpol,
		"vpol", old_vpol[dev_id], vpol);
	SRCCAP_MSG_DEBUG("%s:%u->%u, %s:%u->%u, %s:%u->%u, %s:%u->%u\n",
		"hstart", old_hstart[dev_id], hstart,
		"vstart", old_vstart[dev_id], vstart,
		"hend", old_hend[dev_id], hend,
		"vend", old_vend[dev_id], vend);
	SRCCAP_MSG_DEBUG("%s:%u, %s:%u, %s:%u, %s:%llu, %s:%d->%d\n",
		"dev", dev_id,
		"same_timing", same_timing,
		"has_sync", has_sync,
		"count", count[dev_id],
		"status", old_status[dev_id], *status);

	/* save timing info */
	old_hpd[dev_id] = hpd;
	old_vtt[dev_id] = vtt;
	old_interlaced[dev_id] = interlaced;
	old_hpol[dev_id] = hpol;
	old_vpol[dev_id] = vpol;
	old_hstart[dev_id] = hstart;
	old_vstart[dev_id] = vstart;
	old_hend[dev_id] = hend;
	old_vend[dev_id] = vend;
	old_status[dev_id] = *status;

	return 0;
}

int mtk_timingdetect_streamoff(
	struct mtk_srccap_dev *srccap_dev)
{
	/* keep the latest crop win setting w/o reset.
	int ret = 0;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	struct reg_srccap_timingdetect_window win;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&win, 0, sizeof(struct reg_srccap_timingdetect_window));

	ret = timingdetect_map_detblock_reg(srccap_dev->srccap_info.src, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	win.x = 0;
	win.y = 0;
	win.w = 0;
	win.h = 0;

	ret = drv_hwreg_srccap_timingdetect_set_capture_win(src, win, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	*/

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&srccap_dev->timingdetect_info.cap_win, 0, sizeof(struct v4l2_rect));
	srccap_dev->timingdetect_info.data.vrr_enforcement = FALSE;

	return 0;
}

int mtk_timingdetect_check_sync(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool adc_offline_status,
	bool *has_sync)
{
	u32 hpd = 0;
	u32 vtt = 0;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (src == V4L2_SRCCAP_INPUT_SOURCE_NONE)
		return -EINVAL;

	ret = timingdetect_get_sync_status(srccap_dev, src, srccap_dev->srccap_info.map,
				adc_offline_status, &hpd, &vtt);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = timingdetect_check_signal_sync(src, hpd, vtt, has_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

int mtk_timingdetect_get_field(
	struct mtk_srccap_dev *srccap_dev,
	u8 *field)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source detblock = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &detblock);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_field_type(detblock, field);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

int mtk_timingdetect_set_auto_no_signal_mute(
	struct mtk_srccap_dev *srccap_dev,
	bool enable)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = timingdetect_set_auto_no_signal_mute(srccap_dev, enable);

	return ret;
}

int mtk_timingdetect_steamon(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	bool enable = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE)
		enable = TRUE;

	ret = mtk_timingdetect_set_auto_nosignal(
		srccap_dev,
		srccap_dev->srccap_info.src,
		enable);

	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}

int mtk_timingdetect_set_8p(
	struct mtk_srccap_dev *srccap_dev,
	bool frl)
{
	int ret = 0;
	bool enable_8p = 0;
	enum reg_srccap_timingdetect_source src = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((srccap_dev->srccap_info.src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI) &&
		(srccap_dev->srccap_info.src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)) {

		ret = timingdetect_map_detblock_reg(
			srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		enable_8p = frl;
		SRCCAP_MSG_INFO("frl mode:%s, enable_8p:%s\n", frl?"Yes":"No", enable_8p?"1p":"8p");

		ret = drv_hwreg_srccap_timingdetect_set_8p(enable_8p, src, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}
	return 0;
}

int mtk_timingdetect_update_vrr_info(
		struct mtk_srccap_dev *srccap_dev,
		struct srccap_timingdetect_ip1_src_info *ip1_src_info)
{
	int ret = 0;
	bool has_sync = false;
	enum v4l2_srccap_input_source source = 0;
	struct v4l2_ext_hdmi_get_freesync_info freesync_info;
	u32 pre_vrr = 0;
	bool pre_freesync = 0;

	if (!ip1_src_info || !srccap_dev) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	source = timingdetect_ip1_map_src(ip1_src_info->ip1_source);

	memset(&freesync_info, 0, sizeof(struct v4l2_ext_hdmi_get_freesync_info));

	ret = mtk_timingdetect_check_sync(srccap_dev,
			source, FALSE, &has_sync);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	if (has_sync && (mtk_srccap_hdmirx_get_HDMIStatus(source) != false)) {
		pre_freesync = ip1_src_info->info.freesync;
		pre_vrr = ip1_src_info->info.vrr;

		ip1_src_info->info.freesync =
			mtk_srccap_hdmirx_get_freesync_info(source, &freesync_info);

		ip1_src_info->info.vrr = timingdetect_get_debounce_vrr_status(
				ip1_src_info,
				source,
				srccap_dev->srccap_info.map);

		if (pre_freesync != ip1_src_info->info.freesync
			|| pre_vrr != ip1_src_info->info.vrr) {
			SRCCAP_MSG_INFO("source:%d, freesync:%d->%d, vrr:%d->%d",
				source, pre_freesync, ip1_src_info->info.freesync,
				pre_vrr, ip1_src_info->info.vrr);
			ret = timingdetect_set_vrr_related_settings(
					srccap_dev,
					ip1_src_info->ip1_source,
					ip1_src_info->info.vrr,
					ip1_src_info->info.freesync);
			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}
		}
	}

	return ret;
}

int mtk_timingdetect_set_hw_version(
	struct mtk_srccap_dev *srccap_dev,
	bool stub)
{
	u32 hw_ver = 0;
	int ret = 0;

	hw_ver = srccap_dev->srccap_info.cap.hw_version;

	ret = timingdetect_set_hw_version(hw_ver, stub);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}

int mtk_srccap_register_timingdetect_subdev(struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *subdev_timingdetect,
	struct v4l2_ctrl_handler *timingdetect_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num =
		sizeof(timingdetect_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(timingdetect_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(timingdetect_ctrl_handler,
					&timingdetect_ctrl[ctrl_count], NULL);
	}

	ret = timingdetect_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create timingdetect ctrl\n"
					"handler\n");
		goto exit;
	}
	subdev_timingdetect->ctrl_handler = timingdetect_ctrl_handler;

	v4l2_subdev_init(subdev_timingdetect, &timingdetect_sd_ops);
	subdev_timingdetect->internal_ops = &timingdetect_sd_internal_ops;
	strlcpy(subdev_timingdetect->name, "mtk-timingdetect",
					sizeof(subdev_timingdetect->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_timingdetect);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register timingdetect\n"
					"subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(timingdetect_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_timingdetect_subdev(
			struct v4l2_subdev *subdev_timingdetect)
{
	v4l2_ctrl_handler_free(subdev_timingdetect->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_timingdetect);
}

void mtk_timingdetect_enable_auto_gain_function(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}
	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	ret = drv_hwreg_srccap_timingdetect_set_auto_gain_function(src, TRUE, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

}

void mtk_timingdetect_disable_auto_gain_function(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}
	ret = drv_hwreg_srccap_timingdetect_set_auto_gain_function(src, FALSE, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

}

void mtk_timingdetect_enable_auto_range_function(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	ret = drv_hwreg_srccap_timingdetect_set_auto_range_function(src, TRUE, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
}

void mtk_timingdetect_disable_auto_range_function(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	ret = drv_hwreg_srccap_timingdetect_set_auto_range_function(src, FALSE, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
}

void mtk_timingdetect_set_auto_range_window(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	u16 vcnt = 0, hcnt = 0;
	struct reg_srccap_timingdetect_window auto_range_win;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}
	memset(&auto_range_win, 0, sizeof(struct reg_srccap_timingdetect_window));

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	hcnt = srccap_dev->timingdetect_info.cap_win.width /
			SRCCAP_TIMINGDETECT_DIVIDE_BASE_10;
	vcnt = srccap_dev->timingdetect_info.cap_win.height /
			SRCCAP_TIMINGDETECT_DIVIDE_BASE_10;
	auto_range_win.x = srccap_dev->timingdetect_info.cap_win.left +
			(hcnt / SRCCAP_TIMINGDETECT_DIVIDE_BASE_2);
	auto_range_win.y = srccap_dev->timingdetect_info.cap_win.top +
			(vcnt / SRCCAP_TIMINGDETECT_DIVIDE_BASE_2);
	auto_range_win.w = srccap_dev->timingdetect_info.cap_win.width - hcnt;
	auto_range_win.h = srccap_dev->timingdetect_info.cap_win.height - vcnt;

	ret = drv_hwreg_srccap_timingdetect_set_auto_range_window(
					src, auto_range_win, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);
}

void mtk_timingdetect_get_auto_gain_max_value_status(void *param)
{
	int ret = 0;
	u8 auto_gain_max_value_status = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	ret = drv_hwreg_srccap_timingdetect_get_auto_gain_max_status(
						src, &auto_gain_max_value_status);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	srccap_dev->timingdetect_info.auto_gain_max_value_status = auto_gain_max_value_status;

}

void mtk_timingdetect_get_auto_gain_status(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;
	bool auto_gain_ready_status = 0;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return;
	}

	ret = drv_hwreg_srccap_timingdetect_get_auto_gain_status(src, &auto_gain_ready_status);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	srccap_dev->timingdetect_info.auto_gain_ready_status = auto_gain_ready_status;
}
int mtk_timingdetect_get_vtotal(
	struct mtk_srccap_dev *srccap_dev,
	uint16_t *vtt)
{
	int ret = 0;
	enum reg_srccap_timingdetect_source src = REG_SRCCAP_TIMINGDETECT_SOURCE_ADCA;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (vtt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = timingdetect_map_detblock_reg(
		srccap_dev->srccap_info.src, srccap_dev->srccap_info.map, &src);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_timingdetect_get_vtotal(src, vtt);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;

}

bool mtk_timingdetect_check_if_mapping(
		enum v4l2_srccap_input_source source,
		enum srccap_timingdetect_ip1_type ip1_source)
{
	return (ip1_source == timingdetect_src_map_ip1(source)) ? true : false;
}

bool mtk_timingdetect_check_if_invalid(
		enum srccap_timingdetect_ip1_type ip1_source)
{
	return (ip1_source > SRCCAP_TIMINGDETECT_IP1_TYPE_MAX ||
			ip1_source < SRCCAP_TIMINGDETECT_IP1_START)
			? true : false;
}

int mtk_timingdetect_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size)
{
	int ret = 0;
	int total_string_size = 0;
	bool current_free_sync = 0;
	enum srccap_timingdetect_ip1_type ip1_source = SRCCAP_TIMINGDETECT_IP1_VD_END;
	struct srccap_timingdetect_ip1_src_info *list_head = NULL, *list_node = NULL;
	u32 fdet_vtt[SRCCAP_FDET_VTT_NUM] = {0};
	u16 dev_id = 0;
	struct srccap_timingdetect_pre_monitor_info info;
	struct v4l2_ext_hdmi_get_freesync_info freesync_info;
	char *pre_status = NULL;
	char *current_status = NULL;
	u8 ans_mute_status = 0;

	static u32 fdet_vtt_pre[SRCCAP_FDET_VTT_NUM];
	static struct srccap_timingdetect_pre_monitor_info info_pre[SRCCAP_DEV_NUM];
	static u32 h_freqx10[SRCCAP_DEV_NUM];
	static u32 v_freqx1000[SRCCAP_DEV_NUM];
	static bool free_sync[SRCCAP_DEV_NUM];
	static enum v4l2_timingdetect_status sync_status[SRCCAP_DEV_NUM];

	memset(&freesync_info, 0, sizeof(struct v4l2_ext_hdmi_get_freesync_info));
	memset(&info, 0, sizeof(struct srccap_timingdetect_pre_monitor_info));

	if ((srccap_dev == NULL) || (buf == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id >= SRCCAP_DEV_NUM) {
		SRCCAP_MSG_ERROR("Wrong srccap_dev->dev_id = %d\n", srccap_dev->dev_id);
		return -ENXIO;
	}

	dev_id = srccap_dev->dev_id;

	current_free_sync = mtk_srccap_hdmirx_get_freesync_info(srccap_dev->srccap_info.src,
								&freesync_info);

	ret = timingdetect_get_timing_info(srccap_dev, srccap_dev->srccap_info.src,
				&info.hpd,
				&info.vtt,
				&info.vpd,
				&info.interlaced,
				&info.hpol,
				&info.vpol,
				&info.hstart,
				&info.vstart,
				&info.hend,
				&info.vend,
				&info.yuv420,
				&info.frl,
				&info.htt,
				&info.vs_pulse_width,
				fdet_vtt,
				SRCCAP_FDET_VTT_NUM,
				&info.pixel_freq,
				&ans_mute_status);

	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return 0;
	}

	info.vrr = (u32)timingdetect_get_vrr_status(srccap_dev->srccap_info.src,
							srccap_dev->srccap_info.map);

	/* get current stable info of HDMI or YPBPR*/
	switch (srccap_dev->srccap_info.src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI0;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI1;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_HDMI3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_ADCA;
		break;
	default:
		ip1_source = SRCCAP_TIMINGDETECT_IP1_VD_END;
		break;
	}


	if ((ip1_source != SRCCAP_TIMINGDETECT_IP1_VD_END) &&
		(srccap_dev->timingdetect_info.ip1_src_info != NULL)) {
		list_head = srccap_dev->timingdetect_info.ip1_src_info;
		list_for_each_entry(list_node, &list_head->node, node) {
			if (list_node->ip1_source == ip1_source) {
				memcpy(&info_pre[dev_id], &list_node->info,
					sizeof(struct srccap_timingdetect_pre_monitor_info));
				break;
			}
		}
	}

	if (sync_status[dev_id] > V4L2_TIMINGDETECT_STABLE_SYNC ||
		sync_status[dev_id] < V4L2_TIMINGDETECT_NO_SIGNAL)
		pre_status = "NONE";
	else if (sync_status[dev_id] == V4L2_TIMINGDETECT_NO_SIGNAL)
		pre_status = "NO_SIGNAL";
	else
		pre_status = SYNC_STABLE_CHECK(sync_status[dev_id]);

	if (srccap_dev->timingdetect_info.status > V4L2_TIMINGDETECT_STABLE_SYNC ||
		srccap_dev->timingdetect_info.status < V4L2_TIMINGDETECT_NO_SIGNAL)
		current_status = "NONE";
	else if (srccap_dev->timingdetect_info.status == V4L2_TIMINGDETECT_NO_SIGNAL)
		current_status = "NO_SIGNAL";
	else
		current_status = SYNC_STABLE_CHECK(srccap_dev->timingdetect_info.status);

	/*compare timing info with previous and now */
	total_string_size = snprintf(buf, max_size,
	"Timing Info (Previous:Current):\n"
	"Current Source(dev:%d): %d\n"
	"status : (%s : %s)\n"
	"hpd :	(%d : %d)\n"
	"vtt :	(%d : %d)\n"
	"vpd :	(%d : %d)\n"
	"interlaced :	(%d : %d)\n"
	"hpol:	(%d : %d)\n"
	"vpol:	(%d : %d)\n"
	"hstart:	(%d : %d)\n"
	"vstart:	(%d : %d)\n"
	"hend:	(%d : %d)\n"
	"vend:	(%d : %d)\n"
	"yuv420:	(%d : %d)\n"
	"frl:	(%d : %d)\n"
	"htt:	(%d : %d)\n"
	"vs_pulse_width:	(%d : %d)\n"
	"vrr type:	(%d : %d)\n"
	"h_freqx10:	(%d : %d)\n"
	"v_freqx1000:	(%d : %d)\n"
	"free_sync:	(%d : %d)\n"
	"\n",
	dev_id, srccap_dev->srccap_info.src,
	pre_status, current_status,
	info_pre[dev_id].hpd, info.hpd,
	info_pre[dev_id].vtt, info.vtt,
	info_pre[dev_id].vpd, info.vpd,
	info_pre[dev_id].interlaced, info.interlaced,
	info_pre[dev_id].hpol, info.hpol,
	info_pre[dev_id].vpol, info.vpol,
	info_pre[dev_id].hstart, info.hstart,
	info_pre[dev_id].vstart, info.vstart,
	info_pre[dev_id].hend, info.hend,
	info_pre[dev_id].vend, info.vend,
	info_pre[dev_id].yuv420, info.yuv420,
	info_pre[dev_id].frl, info.frl,
	info_pre[dev_id].htt, info.htt,
	info_pre[dev_id].vs_pulse_width, info.vs_pulse_width,
	info_pre[dev_id].vrr, info.vrr,
	h_freqx10[dev_id], srccap_dev->timingdetect_info.data.h_freqx10,
	v_freqx1000[dev_id], srccap_dev->timingdetect_info.data.v_freqx1000,
	free_sync[dev_id], current_free_sync);


	/* save the previous info */
	if ((ip1_source == SRCCAP_TIMINGDETECT_IP1_VD_END) ||
		(srccap_dev->timingdetect_info.ip1_src_info == NULL))
		memcpy(&info_pre[dev_id], &info,
				sizeof(struct srccap_timingdetect_pre_monitor_info));


	memcpy(fdet_vtt_pre, fdet_vtt, sizeof(u32) * SRCCAP_FDET_VTT_NUM);
	h_freqx10[dev_id] = srccap_dev->timingdetect_info.data.h_freqx10;
	v_freqx1000[dev_id] = srccap_dev->timingdetect_info.data.v_freqx1000;
	free_sync[dev_id] = current_free_sync;
	sync_status[dev_id] = srccap_dev->timingdetect_info.status;



	if (total_string_size < 0 || total_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	return total_string_size;
}

