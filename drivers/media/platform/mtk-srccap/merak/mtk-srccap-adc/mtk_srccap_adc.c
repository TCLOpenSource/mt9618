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

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <asm-generic/div64.h>

#include "mtk_srccap.h"
#include "mtk_srccap_adc.h"
#include "hwreg_srccap_adc.h"

/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static enum reg_srccap_adc_pws_type adc_map_pws_reg(
	enum srccap_adc_source_info src)
{
	switch (src) {
	/* Non-ADC source case */
	case SRCCAP_ADC_SOURCE_NONADC:
		return REG_SRCCAP_ADC_PWS_NONE;
	/* Single source case */
	case SRCCAP_ADC_SOURCE_ONLY_CVBS:
		return REG_SRCCAP_ADC_PWS_CVBS;
	case SRCCAP_ADC_SOURCE_ONLY_SVIDEO:
		return REG_SRCCAP_ADC_PWS_SVIDEO;
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
		return REG_SRCCAP_ADC_PWS_YPBPR;
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
		return REG_SRCCAP_ADC_PWS_RGB;
	case SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV:
		return REG_SRCCAP_ADC_PWS_ATV_SSIF;
	case SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV:
		return REG_SRCCAP_ADC_PWS_ATV_VIF;
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
	case SRCCAP_ADC_SOURCE_ONLY_SCART_Y:
		return REG_SRCCAP_ADC_PWS_SCART;
	/* Multi source case */
	case SRCCAP_ADC_SOURCE_MULTI_RGB_ATV:
		return REG_SRCCAP_ADC_PWS_RGB;
	case SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY:
		return REG_SRCCAP_ADC_PWS_SCART_RGB;
	case SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS:
		return REG_SRCCAP_ADC_PWS_CVBS_RGB;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV:
		return REG_SRCCAP_ADC_PWS_YPBPR;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY:
		return REG_SRCCAP_ADC_PWS_SCART_YPBPR;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS:
		return REG_SRCCAP_ADC_PWS_CVBS_YPBPR;
	default:
		SRCCAP_MSG_ERROR("invalid source type\n");
		return REG_SRCCAP_ADC_PWS_NONE;
	}
}

static enum reg_srccap_adc_input_port adc_map_port_reg(
	enum srccap_input_port port)
{
	switch (port) {
	case SRCCAP_INPUT_PORT_NONE:
		return REG_SRCCAP_ADC_INPUT_PORT_NONE;
	case SRCCAP_INPUT_PORT_RGB0_DATA:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB0_DATA;
	case SRCCAP_INPUT_PORT_RGB1_DATA:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB1_DATA;
	case SRCCAP_INPUT_PORT_RGB2_DATA:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB2_DATA;
	case SRCCAP_INPUT_PORT_RGB0_SYNC:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB0_SYNC;
	case SRCCAP_INPUT_PORT_RGB1_SYNC:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB1_SYNC;
	case SRCCAP_INPUT_PORT_RGB2_SYNC:
		return REG_SRCCAP_ADC_INPUT_PORT_RGB2_SYNC;
	case SRCCAP_INPUT_PORT_YCVBS0:
		return REG_SRCCAP_ADC_INPUT_PORT_YCVBS0;
	case SRCCAP_INPUT_PORT_YCVBS1:
		return REG_SRCCAP_ADC_INPUT_PORT_YCVBS1;
	case SRCCAP_INPUT_PORT_YCVBS2:
		return REG_SRCCAP_ADC_INPUT_PORT_YCVBS2;
	case SRCCAP_INPUT_PORT_YCVBS3:
		return REG_SRCCAP_ADC_INPUT_PORT_YCVBS3;
	case SRCCAP_INPUT_PORT_YG0:
		return REG_SRCCAP_ADC_INPUT_PORT_YG0;
	case SRCCAP_INPUT_PORT_YG1:
		return REG_SRCCAP_ADC_INPUT_PORT_YG1;
	case SRCCAP_INPUT_PORT_YG2:
		return REG_SRCCAP_ADC_INPUT_PORT_YG2;
	case SRCCAP_INPUT_PORT_YR0:
		return REG_SRCCAP_ADC_INPUT_PORT_YR0;
	case SRCCAP_INPUT_PORT_YR1:
		return REG_SRCCAP_ADC_INPUT_PORT_YR1;
	case SRCCAP_INPUT_PORT_YR2:
		return REG_SRCCAP_ADC_INPUT_PORT_YR2;
	case SRCCAP_INPUT_PORT_YB0:
		return REG_SRCCAP_ADC_INPUT_PORT_YB0;
	case SRCCAP_INPUT_PORT_YB1:
		return REG_SRCCAP_ADC_INPUT_PORT_YB1;
	case SRCCAP_INPUT_PORT_YB2:
		return REG_SRCCAP_ADC_INPUT_PORT_YB2;
	case SRCCAP_INPUT_PORT_CCVBS0:
		return REG_SRCCAP_ADC_INPUT_PORT_CCVBS0;
	case SRCCAP_INPUT_PORT_CCVBS1:
		return REG_SRCCAP_ADC_INPUT_PORT_CCVBS1;
	case SRCCAP_INPUT_PORT_CCVBS2:
		return REG_SRCCAP_ADC_INPUT_PORT_CCVBS2;
	case SRCCAP_INPUT_PORT_CCVBS3:
		return REG_SRCCAP_ADC_INPUT_PORT_CCVBS3;
	case SRCCAP_INPUT_PORT_CR0:
		return REG_SRCCAP_ADC_INPUT_PORT_CR0;
	case SRCCAP_INPUT_PORT_CR1:
		return REG_SRCCAP_ADC_INPUT_PORT_CR1;
	case SRCCAP_INPUT_PORT_CR2:
		return REG_SRCCAP_ADC_INPUT_PORT_CR2;
	case SRCCAP_INPUT_PORT_CG0:
		return REG_SRCCAP_ADC_INPUT_PORT_CG0;
	case SRCCAP_INPUT_PORT_CG1:
		return REG_SRCCAP_ADC_INPUT_PORT_CG1;
	case SRCCAP_INPUT_PORT_CG2:
		return REG_SRCCAP_ADC_INPUT_PORT_CG2;
	case SRCCAP_INPUT_PORT_CB0:
		return REG_SRCCAP_ADC_INPUT_PORT_CB0;
	case SRCCAP_INPUT_PORT_CB1:
		return REG_SRCCAP_ADC_INPUT_PORT_CB1;
	case SRCCAP_INPUT_PORT_CB2:
		return REG_SRCCAP_ADC_INPUT_PORT_CB2;
	case SRCCAP_INPUT_PORT_DVI0:
		return REG_SRCCAP_ADC_INPUT_PORT_DVI0;
	case SRCCAP_INPUT_PORT_DVI1:
		return REG_SRCCAP_ADC_INPUT_PORT_DVI1;
	case SRCCAP_INPUT_PORT_DVI2:
		return REG_SRCCAP_ADC_INPUT_PORT_DVI2;
	case SRCCAP_INPUT_PORT_DVI3:
		return REG_SRCCAP_ADC_INPUT_PORT_DVI3;
	default:
		SRCCAP_MSG_ERROR("invalid port type\n");
		return REG_SRCCAP_ADC_INPUT_PORT_NONE;
	}
}

static enum reg_srccap_adc_input_source_type adc_map_source_reg(
	enum srccap_adc_source_info src)
{
	switch (src) {
	/* Non-ADC source case */
	case SRCCAP_ADC_SOURCE_NONADC:
		return REG_SRCCAP_ADC_INPUTSOURCE_NONE;
	/* Single source case */
	case SRCCAP_ADC_SOURCE_ONLY_CVBS:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_CVBS;
	case SRCCAP_ADC_SOURCE_ONLY_SVIDEO:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_SVIDEO;
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_YPBPR;
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_RGB;
	case SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_INT_DMD_ATV;
	case SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_EXT_DMD_ATV;
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_SCART_RGB;
	case SRCCAP_ADC_SOURCE_ONLY_SCART_Y:
		return REG_SRCCAP_ADC_INPUTSOURCE_ONLY_SCART_Y;
	/* Multi source case */
	case SRCCAP_ADC_SOURCE_MULTI_RGB_ATV:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_RGB_ATV;
	case SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_RGB_SCART;
	case SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_RGB_CVBS;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_YPBPR_ATV;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_YPBPR_SCART;
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS:
		return REG_SRCCAP_ADC_INPUTSOURCE_MULTI_YPBPR_CVBS;
	default:
		SRCCAP_MSG_ERROR("invalid source type\n");
		return REG_SRCCAP_ADC_INPUTSOURCE_NONE;
	}
}

enum srccap_adc_source_info adc_map_analog_source(
	enum v4l2_srccap_input_source src)
{
	bool is_scart_rgb = 0;

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_NONE:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		return SRCCAP_ADC_SOURCE_NONADC;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		return SRCCAP_ADC_SOURCE_ONLY_CVBS;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		return SRCCAP_ADC_SOURCE_ONLY_SVIDEO;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		return SRCCAP_ADC_SOURCE_ONLY_YPBPR;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		return SRCCAP_ADC_SOURCE_ONLY_RGB;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		return SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV; // need fix decide int/ext ATV
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
	{
		drv_hwreg_srccap_adc_get_scart_type(&is_scart_rgb);
		if (is_scart_rgb)
			return SRCCAP_ADC_SOURCE_ONLY_SCART_RGB;
		else
			return SRCCAP_ADC_SOURCE_ONLY_SCART_Y;
	}
	default:
		SRCCAP_MSG_ERROR("invalid source type\n");
		return SRCCAP_ADC_SOURCE_NONADC;
	}
}

static enum reg_srccap_adc_input_offline_mux adc_map_offline_port_reg(
	enum srccap_input_port port)
{
	switch (port) {
	case SRCCAP_INPUT_PORT_RGB0_DATA:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_G0;
	case SRCCAP_INPUT_PORT_RGB1_DATA:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_G1;
	case SRCCAP_INPUT_PORT_RGB0_SYNC:
		// TODO
	case SRCCAP_INPUT_PORT_YCVBS0:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_CVBS0;
	case SRCCAP_INPUT_PORT_YCVBS1:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_CVBS1;
	case SRCCAP_INPUT_PORT_YG0:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_G0;
	case SRCCAP_INPUT_PORT_YG1:
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_G1;
	default:
		SRCCAP_MSG_ERROR("invalid port type\n");
		return REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_NUM;
	}
}

static int adc_set_hpol(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u16 h_pol = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	h_pol = srccap_dev->timingdetect_info.data.h_pol;

	ret = drv_hwreg_srccap_adc_set_hpol(h_pol, true, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int adc_set_plldiv(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u16 htt = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	htt = srccap_dev->timingdetect_info.data.h_total;

	if ((htt > 3) && (htt < SRCCAP_ADC_MAX_CLK)) {
		ret = drv_hwreg_srccap_adc_set_plldiv((htt - 3), TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	return 0;
}

static int adc_set_plla_reset(void)
{
	int ret = 0;

	/* toggle to reset */
	ret = drv_hwreg_srccap_adc_set_plla_reset(TRUE, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_INFO("[ADC] Set plla reset failed\n");
		return ret;
	}

	// sleep 500us
	usleep_range(SRCCAP_ADC_UDELAY_500, SRCCAP_ADC_UDELAY_510);

	ret = drv_hwreg_srccap_adc_set_plla_reset(FALSE, TRUE, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_INFO("[ADC] toggle plla reset failed\n");

	return ret;
}
static int adc_set_fixed_gain(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_gain fixed_gain;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&fixed_gain, 0, sizeof(struct reg_srccap_adc_rgb_gain));

	SRCCAP_MSG_INFO("[ADC] set fixed gain, R=%u, G=%u, B=%u\n",
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_r,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_g,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_b);
	fixed_gain.r_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_r;
	fixed_gain.g_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_g;
	fixed_gain.b_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_b;
	ret = drv_hwreg_srccap_adc_set_rgb_gain(fixed_gain, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set rgb gain fail\n");
	}

	return ret;
}

static int adc_get_fixed_gain(
	enum v4l2_srccap_input_source adc_input_src,
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_YPBPR) {
		SRCCAP_MSG_INFO("[ADC] get YPBPR fixed gain, R=%u, G=%u, B=%u\n",
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_r,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_g,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_b);

		gain->r_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_r;
		gain->g_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_g;
		gain->b_gain = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_gain_b;
	} else if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_VGA) {
		SRCCAP_MSG_INFO("[ADC] get VGA fixed gain, R=%u, G=%u, B=%u\n",
			srccap_dev->srccap_info.cap.adc_vga_fixed_gain_r,
			srccap_dev->srccap_info.cap.adc_vga_fixed_gain_g,
			srccap_dev->srccap_info.cap.adc_vga_fixed_gain_b);

		gain->r_gain = srccap_dev->srccap_info.cap.adc_vga_fixed_gain_r;
		gain->g_gain = srccap_dev->srccap_info.cap.adc_vga_fixed_gain_g;
		gain->b_gain = srccap_dev->srccap_info.cap.adc_vga_fixed_gain_b;
	} else {
		SRCCAP_MSG_ERROR("Not support input src =%d\n", adc_input_src);
		return -EINVAL;
	}

	return ret;
}

static int adc_set_fixed_offset(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_offset fixed_offset;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&fixed_offset, 0, sizeof(struct reg_srccap_adc_rgb_offset));

	SRCCAP_MSG_INFO("[ADC] set fixed offset, R=%u, G=%u, B=%u\n",
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b);
	fixed_offset.r_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r;
	fixed_offset.g_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g;
	fixed_offset.b_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b;
	ret = drv_hwreg_srccap_adc_set_rgb_offset(fixed_offset, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set fixed offset fail\n");
	}
	return ret;
}

static int adc_get_fixed_offset(
	enum v4l2_srccap_input_source adc_input_src,
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_YPBPR) {
		SRCCAP_MSG_INFO("[ADC] get ypbpr fixed offset, R=%u, G=%u, B=%u\n",
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b);

		offset->r_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r;
		offset->g_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g;
		offset->b_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b;
	} else if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_VGA) {
		SRCCAP_MSG_INFO("[ADC] get vga fixed offset, R=%u, G=%u, B=%u\n",
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g,
			srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b);


		offset->r_offset = srccap_dev->srccap_info.cap.adc_vga_fixed_offset_r;
		offset->g_offset = srccap_dev->srccap_info.cap.adc_vga_fixed_offset_g;
		offset->b_offset = srccap_dev->srccap_info.cap.adc_vga_fixed_offset_b;
	} else {
		SRCCAP_MSG_ERROR("Not support input src =%d\n", adc_input_src);
		return -EINVAL;
	}

	return ret;
}


static int adc_set_fixed_phase(void)
{
	int ret = 0;
	u16 adcpllmod = 0;

	SRCCAP_MSG_INFO("[ADC] set fixed phase = %u\n", ADC_YPBPR_FIXED_PHASE);

	ret = drv_hwreg_srccap_adc_get_adcpllmod(&adcpllmod);
	if (ret) {
		SRCCAP_MSG_ERROR("Get adcpllmod failed\n");
		return ret;
	}

	ret = drv_hwreg_srccap_adc_set_phase(ADC_YPBPR_FIXED_PHASE, adcpllmod, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set fixed phase fail\n");
		return ret;
	}

	return ret;
}

static int adc_get_fixed_phase(
	enum v4l2_srccap_input_source adc_input_src,
	u16 *phase,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (phase == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_YPBPR) {
		SRCCAP_MSG_INFO("[ADC] get YPBPR fixed phase = %u\n", ADC_YPBPR_FIXED_PHASE);
		*phase = ADC_YPBPR_FIXED_PHASE;

	} else if (adc_input_src == V4L2_SRCCAP_INPUT_SOURCE_VGA) {
		SRCCAP_MSG_INFO("[ADC] get VGA fixed phase = %u\n", ADC_YPBPR_FIXED_PHASE);
		*phase = ADC_YPBPR_FIXED_PHASE;
	} else {
		SRCCAP_MSG_ERROR("Not support input src =%d\n", adc_input_src);
		return -EINVAL;
	}

	return ret;
}


static bool adc_check_same_port(
	enum srccap_input_port port1,
	enum srccap_input_port port2)
{
	bool result = FALSE;

	if ((port1 == port2)
		&& (port1 != SRCCAP_INPUT_PORT_NONE)
		&& (port2 != SRCCAP_INPUT_PORT_NONE))
		return TRUE;

	switch (port1) {
	case SRCCAP_INPUT_PORT_RGB0_DATA:
		if (port2 == SRCCAP_INPUT_PORT_YG0)
			result = TRUE;
		break;
	case SRCCAP_INPUT_PORT_RGB1_DATA:
		if (port2 == SRCCAP_INPUT_PORT_YG1)
			result = TRUE;
		break;
	case SRCCAP_INPUT_PORT_RGB2_DATA:
		if (port2 == SRCCAP_INPUT_PORT_YG2)
			result = TRUE;
		break;
	case SRCCAP_INPUT_PORT_YG0:
		if (port2 == SRCCAP_INPUT_PORT_RGB0_DATA)
			result = TRUE;
		break;
	case SRCCAP_INPUT_PORT_YG1:
		if (port2 == SRCCAP_INPUT_PORT_RGB1_DATA)
			result = TRUE;
		break;
	case SRCCAP_INPUT_PORT_YG2:
		if (port2 == SRCCAP_INPUT_PORT_RGB2_DATA)
			result = TRUE;
		break;
	default:
		break;
	}

	return result;
}

static int adc_check_port(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_adc_source *source,
	enum srccap_input_port *port_container,
	u8 *port_cnt)
{
	int i = 0;
	int j = 0;
	u8 cnt = 0;
	enum srccap_input_port *port = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (source == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (port_container == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (port_cnt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	SRCCAP_MSG_INFO("source count = %u\n", source->src_count);

	port = kzalloc((sizeof(enum srccap_input_port) * source->src_count * 2), GFP_KERNEL);
	if (!port) {
		SRCCAP_MSG_ERROR("kzalloc fail\n");
		return -ENOMEM;
	}

	// Get data & sync port used by each source
	for (i = 0; i < source->src_count; i++) {
		port[i*2] = srccap_dev->srccap_info.map[source->p.adc_input_src[i]].data_port;
		port[i*2+1] = srccap_dev->srccap_info.map[source->p.adc_input_src[i]].sync_port;
		SRCCAP_MSG_INFO("adc_input_src[%d] = %u\n", i, source->p.adc_input_src[i]);
		SRCCAP_MSG_INFO("port[%d](data) = %u\n", i*2, port[i*2]);
		SRCCAP_MSG_INFO("port[%d](sync) = %u\n", i*2+1, port[i*2+1]);
	}

	// Check if multi-source data/sync used same port
	for (i = 0; i < source->src_count * 2; i++) {
		if (port[i] != 0) {
			for (j = i+1; j < source->src_count * 2; j++) {
				if (adc_check_same_port(port[i], port[j])) {
					SRCCAP_MSG_ERROR(
						"Conflict pad occur! pad[%d]= pad[%d]= %u\n",
						i, j, port[i]);
					kfree(port);
					return -EPERM;
				}
			}
			port_container[cnt] = port[i];
			cnt++;
		}
	}

	SRCCAP_MSG_INFO("adc check port pass! src_count:%d\n", source->src_count);
	*port_cnt = cnt;
	kfree(port);
	return 0;
}

static int adc_split_src(
	enum srccap_adc_source_info adc_src,
	enum v4l2_srccap_input_source *v4l2_src0,
	enum v4l2_srccap_input_source *v4l2_src1)
{
	if (v4l2_src0 == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (v4l2_src1 == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((adc_src < SRCCAP_ADC_SOURCE_NONADC) || (adc_src >= SRCCAP_ADC_SOURCE_ALL_NUMS)) {
		SRCCAP_MSG_ERROR("Invalid adc source : %d\n", adc_src);
		return -EINVAL;
	}

	if (adc_src == SRCCAP_ADC_SOURCE_NONADC) {
		*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_NONE;
		*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_NONE;
	} else {
		switch (adc_src) {
		case SRCCAP_ADC_SOURCE_ONLY_CVBS:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_CVBS;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_CVBS2;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_SVIDEO:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_SVIDEO;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_YPBPR2;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_RGB:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_VGA;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_VGA2;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_SCART;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_SCART2;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV:
		case SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_ATV;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_NONE;
			break;
		case SRCCAP_ADC_SOURCE_ONLY_SCART_Y:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_SCART;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_SCART2;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_RGB_ATV:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_VGA;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_ATV;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_VGA;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_SCART;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_VGA;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_CVBS;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_ATV;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_SCART;
			break;
		case SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS:
			*v4l2_src0 = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
			*v4l2_src1 = V4L2_SRCCAP_INPUT_SOURCE_CVBS;
			break;
		default:
			SRCCAP_MSG_ERROR("unsupport adc source : %d\n", adc_src);
			return -EPERM;
		}
	}

	return 0;
}


static int adc_merge_dual_src(
	enum srccap_adc_source_info src0,
	enum srccap_adc_source_info src1,
	enum srccap_adc_source_info *result)
{
	enum srccap_adc_source_info temp_src = SRCCAP_ADC_SOURCE_NONADC;
	static int _adc_dual_src_table[][SRCCAP_ADC_SOURCE_SINGLE_NUMS] = {
		/*  NON   CVBS   SVDO  YPBPR    VGA  ATV_I  ATV_E   SC_R   SC_Y */
		{  true,  true,  true,  true,  true,  true,  true,  true,  true,}, /*NONADC*/
		{  true, false, false,  true,  true,  true, false, false, false,}, /*CVBS*/
		{  true, false, false, false, false, false, false, false, false,}, /*SVIDEO*/
		{  true,  true, false, false, false,  true,  true, false,  true,}, /*YPBPR*/
		{  true,  true, false, false, false,  true,  true, false,  true,}, /*VGA*/
		{  true,  true, false,  true,  true,  true,  true, false,  true,}, /*INTATV*/
		{  true, false, false,  true,  true,  true, false, false, false,}, /*EXTATV*/
		{  true, false, false, false, false, false, false, false, false,}, /*SCARTR*/
		{  true, false, false,  true,  true,  true, false, false, false,}, /*SCARTY*/
	};

	if (result == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if ((src0 < SRCCAP_ADC_SOURCE_NONADC) || (src0 >= SRCCAP_ADC_SOURCE_SINGLE_NUMS)) {
		SRCCAP_MSG_ERROR("Invalid source 0: %d\n", src0);
		return -EINVAL;
	}
	if ((src1 < SRCCAP_ADC_SOURCE_NONADC) || (src1 >= SRCCAP_ADC_SOURCE_SINGLE_NUMS)) {
		SRCCAP_MSG_ERROR("Invalid source 1: %d\n", src1);
		return -EINVAL;
	}

	SRCCAP_MSG_INFO("[ADC] src0=%d, src1=%d\n", src0, src1);

	// sort first to reduce complexity
	if (src0 > src1) {
		temp_src = src0;
		src0 = src1;
		src1 = temp_src;
	}

	if (_adc_dual_src_table[src0][src1] == true) {
		if (src0 == SRCCAP_ADC_SOURCE_NONADC) {
			*result = src1;
			goto SUCCESS;
		} else if (src1 == SRCCAP_ADC_SOURCE_NONADC) {
			*result = src0;
			goto SUCCESS;
		} else {
			switch (src0) {
			case SRCCAP_ADC_SOURCE_ONLY_CVBS:
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_YPBPR) {
					*result = SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS;
					goto SUCCESS;
				}
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_RGB) {
					*result = SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS;
					goto SUCCESS;
				}
				break;
			case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV) {
					*result = SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV;
					goto SUCCESS;
				}
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV) {
					*result = SRCCAP_ADC_SOURCE_MULTI_YPBPR_ATV;
					goto SUCCESS;
				}
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_SCART_Y) {
					*result = SRCCAP_ADC_SOURCE_MULTI_YPBPR_SCARTY;
					goto SUCCESS;
				}
				break;
			case SRCCAP_ADC_SOURCE_ONLY_RGB:
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_INT_DMD_ATV) {
					*result = SRCCAP_ADC_SOURCE_MULTI_RGB_ATV;
					goto SUCCESS;
				}
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV) {
					*result = SRCCAP_ADC_SOURCE_MULTI_RGB_ATV;
					goto SUCCESS;
				}
				if (src1 == SRCCAP_ADC_SOURCE_ONLY_SCART_Y) {
					*result = SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY;
					goto SUCCESS;
				}
				break;
			default:
				break;
			}
		}
	}

	SRCCAP_MSG_ERROR("Not support multi source case!, src0= (%d), src1= (%d)\n", src0, src1);
	return -EPERM;

SUCCESS:
	SRCCAP_MSG_INFO("[ADC] source decide = (%d)\n", *result);
	return 0;
}

static int adc_get_sog_mux_sel(
	enum srccap_input_port data_port,
	enum srccap_adc_sog_online_mux *sog_mux)
{
	if (sog_mux == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (data_port == SRCCAP_INPUT_PORT_RGB0_DATA) {
		*sog_mux = SRCCAP_SOG_ONLINE_PADA_GIN0P;

		goto result;
	} else if (data_port == SRCCAP_INPUT_PORT_RGB1_DATA) {
		*sog_mux = SRCCAP_SOG_ONLINE_PADA_GIN1P;

		goto result;
	} else if (data_port == SRCCAP_INPUT_PORT_RGB2_DATA) {
		*sog_mux = SRCCAP_SOG_ONLINE_PADA_GIN2P;

		goto result;
	} else {
		*sog_mux = SRCCAP_SOG_ONLINE_PADA_GND;
		goto result;
	}

result:
	SRCCAP_MSG_INFO("[ADC] sog sel: %u\n", *sog_mux);
	return 0;
}

static bool adc_check_source_port_conflict(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src0,
	enum v4l2_srccap_input_source src1)
{
	enum srccap_input_port src0_data_port = 0, src0_sync_port = 0,
		src1_data_port = 0, src1_sync_port = 0;
	bool result = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	src0_data_port = srccap_dev->srccap_info.map[src0].data_port;
	src0_sync_port = srccap_dev->srccap_info.map[src0].sync_port;
	src1_data_port = srccap_dev->srccap_info.map[src1].data_port;
	src1_sync_port = srccap_dev->srccap_info.map[src1].sync_port;

	if (adc_check_same_port(src0_data_port, src1_data_port)
		|| adc_check_same_port(src0_data_port, src1_sync_port)
		|| adc_check_same_port(src0_sync_port, src1_data_port)
		|| adc_check_same_port(src0_sync_port, src1_sync_port))
		result = TRUE;
	else
		result = FALSE;

	return result;
}

static int adc_set_sog_online_mux(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src[],
	u32 src_cnt)
{
	int ret = 0;
	int index = 0;
	bool port_conflict_with_ypbpr = FALSE;
	enum srccap_input_port ypbpr_data_port = SRCCAP_INPUT_PORT_NONE;
	enum srccap_adc_sog_online_mux sog_mux = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	for (index = 0; index < src_cnt; index++)
		if (SRCCAP_IS_VD_SRC(src[index]) && adc_check_source_port_conflict(srccap_dev,
			src[index], V4L2_SRCCAP_INPUT_SOURCE_YPBPR))
			port_conflict_with_ypbpr = TRUE;

	if (port_conflict_with_ypbpr) {
		ret = drv_hwreg_srccap_adc_set_sog_online_mux(REG_SRCCAP_ADC_SOG_ONLINE_GND,
		TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_ERROR("set sog online mux fail\n");
			return ret;
		}
	} else {
		ypbpr_data_port =
			srccap_dev->srccap_info.map[V4L2_SRCCAP_INPUT_SOURCE_YPBPR].data_port;
		ret = adc_get_sog_mux_sel(ypbpr_data_port, &sog_mux);
		if (ret) {
			SRCCAP_MSG_ERROR("adc_get_sog_mux_sel fail!\n");
			return ret;
		}

		ret = drv_hwreg_srccap_adc_set_sog_online_mux(
				(enum reg_srccap_adc_sog_online_mux)sog_mux, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_ERROR("set sog online mux fail\n");
			return ret;
		}
	}

	return ret;
}

int adc_set_src_setting(
	struct v4l2_adc_source *source,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	int idx = 0;
	u8 port_cnt = 0;
	enum v4l2_srccap_input_source src[] = {
		V4L2_SRCCAP_INPUT_SOURCE_NONE, V4L2_SRCCAP_INPUT_SOURCE_NONE};
	enum srccap_adc_source_info adc_src[] = {
		SRCCAP_ADC_SOURCE_NONADC, SRCCAP_ADC_SOURCE_NONADC};
	enum srccap_input_port *port_used = NULL;
	enum reg_srccap_adc_input_port reg_port = 0;
	enum reg_srccap_adc_input_source_type reg_src_type = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (source == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* check src count */
	if ((source->src_count > srccap_dev->srccap_info.cap.ipdma_cnt &&
		source->src_count > srccap_dev->srccap_info.cap.adc_multi_src_max_num)
		|| source->src_count <= 0) {
		SRCCAP_MSG_ERROR("Invalid src_count: %u, ipdma_cnt: %u, adc max num: %u\n",
			source->src_count,
			srccap_dev->srccap_info.cap.ipdma_cnt,
			srccap_dev->srccap_info.cap.adc_multi_src_max_num);
		return -EINVAL;
	}

	/* check adc port capability */
	port_used = kzalloc((sizeof(enum srccap_input_port)
		* srccap_dev->srccap_info.cap.adc_multi_src_max_num
		* SRCCAP_PORT_TYPE_NUM), GFP_KERNEL);
	if (!port_used) {
		SRCCAP_MSG_ERROR("kzalloc fail\n");
		return -ENOMEM;
	}
	ret = adc_check_port(srccap_dev, source, port_used, &port_cnt);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_check_port fail!\n");
		goto out;
	}

	/* check multi-src hw capability, only support dual sources for now */
	for (idx = 0; idx < source->src_count; idx++) {
		adc_src[idx] = adc_map_analog_source(source->p.adc_input_src[idx]);
		src[idx] = source->p.adc_input_src[idx];

		if (SRCCAP_ADC_CHECK_SRC_BOUND_INVAL(src[idx])) {
			ret = -EINVAL;
			SRCCAP_MSG_ERROR("invalid source!\n");
			goto out;
		}
	}

	/* check index for array: srccap_dev->srccap_info.map[], just in case when src_count=1 */
	if (SRCCAP_ADC_CHECK_SRC_BOUND_INVAL((u32)src[SRCCAP_ADC_SRC_0]) ||
		SRCCAP_ADC_CHECK_SRC_BOUND_INVAL((u32)src[SRCCAP_ADC_SRC_1])) {
		ret = -EINVAL;
		SRCCAP_MSG_ERROR("invalid source!\n");
		goto out;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
	ret = adc_merge_dual_src(adc_src[SRCCAP_ADC_SRC_0], adc_src[SRCCAP_ADC_SRC_1],
							&srccap_dev->adc_info.adc_src);

	if (ret) {
		SRCCAP_MSG_ERROR("adc_merge_dual_src fail!\n");
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
		goto out;
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);

	/* set mux table */
	for (idx = 0; idx < port_cnt; idx++) {
		reg_port = adc_map_port_reg(port_used[idx]);
		SRCCAP_MSG_INFO("[ADC] set mux table idx: (%d)\n", reg_port);
		if (reg_port != REG_SRCCAP_ADC_INPUT_PORT_NONE) {
			ret = drv_hwreg_srccap_adc_set_mux(reg_port);
			if (ret) {
				SRCCAP_MSG_ERROR("load mux table fail\n");
				goto out;
			}
		}
	}

	/* set SOG online mux for FSS */
	ret = adc_set_sog_online_mux(srccap_dev, src, source->src_count);
	if (ret) {
		SRCCAP_MSG_ERROR("set sog online mux failed!\n");
		goto out;
	}

	/* set source table */
	reg_src_type = adc_map_source_reg(srccap_dev->adc_info.adc_src);
	SRCCAP_MSG_INFO("[ADC] set source table idx: (%d)\n", reg_src_type);
	ret = drv_hwreg_srccap_adc_set_source(reg_src_type);
	if (ret) {
		SRCCAP_MSG_ERROR("load source table fail\n");
		goto out;
	}

out:
	kfree(port_used);
	return ret;
}

static int adc_get_clk(
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
		return -ENXIO;
	}
	if (clk == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	*clk = devm_clk_get(dev, s);
	if (IS_ERR(*clk)) {
		SRCCAP_MSG_FATAL("unable to retrieve %s\n", s);
		return PTR_ERR(*clk);
	}

	return 0;
}

static void adc_put_clk(
	struct device *dev,
	struct clk *clk)
{
	if (dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}
	if (clk == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	devm_clk_put(dev, clk);
}

static int adc_toggle_swen(
	struct mtk_srccap_dev *srccap_dev,
	char *clk_name,
	char *enable)
{
	int ret = 0;
	struct clk *target_clk = NULL;
	struct clk_hw *c_hw = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (clk_name == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (enable == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = adc_get_clk(srccap_dev->dev, clk_name, &target_clk);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	c_hw = __clk_get_hw(target_clk);

	if (strcmp(enable, "ON") == 0) {
		if ((!__clk_is_enabled(target_clk)) ||
			(!clk_hw_is_prepared(c_hw))) {
			ret = clk_prepare_enable(target_clk);
			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				adc_put_clk(srccap_dev->dev, target_clk);
				return ret;
			}
		}
	}
	if (strcmp(enable, "OFF") == 0) {
		if ((__clk_is_enabled(target_clk)) && (clk_hw_is_prepared(c_hw)))
			clk_disable_unprepare(target_clk);
	}

	adc_put_clk(srccap_dev->dev, target_clk);
	return 0;
}

static int adc_set_clk_swen(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 idx = 0;
	u16 count = 0;
	enum reg_srccap_adc_input_source_type src_type;
	struct reg_srccap_adc_clk_info *clk_info = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	src_type = adc_map_source_reg(srccap_dev->adc_info.adc_src);
	clk_info = (struct reg_srccap_adc_clk_info *)
			vzalloc(sizeof(struct reg_srccap_adc_clk_info) * ADCTBL_CLK_SWEN_NUM);
	if (clk_info == NULL) {
		SRCCAP_MSG_ERROR("vzalloc fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = drv_hwreg_srccap_adc_get_clk_swen(src_type, ADCTBL_CLK_SWEN_NUM, clk_info, &count);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg get clk swen fail\n");
		ret = -EPERM;
		goto exit;
	}

	SRCCAP_MSG_INFO("[ADC] CLK SWEN nums from ADC table is: %u\n", count);
	for (idx = 0; idx < count; idx++) {
		SRCCAP_MSG_INFO("[ADC] SWEN[%2u]: Name= %-40s, enable=%s\n",
			idx, clk_info[idx].clk_name, clk_info[idx].clk_data);

		ret = adc_toggle_swen(srccap_dev,
				clk_info[idx].clk_name,
				clk_info[idx].clk_data);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			SRCCAP_MSG_ERROR("adc_toggle_swen fail\n");
			goto exit;
		}
	}

	//Bellow clk swens are ctrled in driver, not from table
	ret = adc_toggle_swen(srccap_dev, "adctbl_swen_dig_2162adc", "ON");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto exit;
	}

exit:
	if (clk_info != NULL)
		vfree(clk_info);

	return ret;
}

static int adc_enable_parent(
	struct mtk_srccap_dev *srccap_dev,
	char *clk_name,
	char *parent_name)
{
	int ret = 0;
	struct clk *target_clk = NULL;
	struct clk *parent = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (clk_name == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (parent_name == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = adc_get_clk(srccap_dev->dev, clk_name, &target_clk);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto out3;
	}

	ret = adc_get_clk(srccap_dev->dev, parent_name, &parent);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto out2;
	}
	ret = clk_prepare_enable(target_clk);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto out1;
	}
	ret = clk_set_parent(target_clk, parent);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto out1;
	}

out1:
	adc_put_clk(srccap_dev->dev, parent);
out2:
	adc_put_clk(srccap_dev->dev, target_clk);
out3:
	return ret;
}

static int adc_set_clk(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 idx = 0;
	u16 count = 0;
	enum reg_srccap_adc_input_source_type src_type;
	struct reg_srccap_adc_clk_info *clk_info = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	src_type = adc_map_source_reg(srccap_dev->adc_info.adc_src);
	clk_info = (struct reg_srccap_adc_clk_info *)
			vzalloc(sizeof(struct reg_srccap_adc_clk_info) * ADCTBL_CLK_SET_NUM);
	if (clk_info == NULL) {
		SRCCAP_MSG_ERROR("vzalloc fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = drv_hwreg_srccap_adc_get_clk_setting(src_type, ADCTBL_CLK_SET_NUM, clk_info, &count);
	if (ret) {
		SRCCAP_MSG_ERROR("get table clk setting fail\n");
		ret = -EPERM;
		goto exit;
	}

	SRCCAP_MSG_INFO("[ADC] CLK nums from ADC table = %u\n", count);
	for (idx = 0; idx < count; idx++) {
		SRCCAP_MSG_INFO("[ADC] CLK[%2u]: Name= %-35s, parent=%s\n",
			idx, clk_info[idx].clk_name, clk_info[idx].clk_data);

		ret = adc_enable_parent(srccap_dev,
				clk_info[idx].clk_name,
				clk_info[idx].clk_data);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			SRCCAP_MSG_ERROR("adc_enable_parent fail\n");
			goto exit;
		}
	}

exit:
	if (clk_info != NULL)
		vfree(clk_info);

	return ret;
}

static int adc_disable_clk_swen(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 idx = 0;
	u16 count = 0;
	enum reg_srccap_adc_input_source_type src_type = REG_SRCCAP_ADC_INPUTSOURCE_OFF;
	struct reg_srccap_adc_clk_info *clk_info = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	clk_info = (struct reg_srccap_adc_clk_info *)
			vzalloc(sizeof(struct reg_srccap_adc_clk_info) * ADCTBL_CLK_SWEN_NUM);
	if (clk_info == NULL) {
		SRCCAP_MSG_ERROR("vzalloc fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = drv_hwreg_srccap_adc_get_clk_swen(src_type, ADCTBL_CLK_SWEN_NUM, clk_info, &count);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg get clk swen fail\n");
		ret = -EPERM;
		goto exit;
	}

	SRCCAP_MSG_INFO("[ADC] CLK SWEN nums from ADC table is: %u\n", count);
	for (idx = 0; idx < count; idx++) {
		SRCCAP_MSG_INFO("[ADC] SWEN[%2u]: Name= %-40s, enable=%s\n",
			idx, clk_info[idx].clk_name, clk_info[idx].clk_data);

		ret = adc_toggle_swen(srccap_dev,
				clk_info[idx].clk_name,
				clk_info[idx].clk_data);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			SRCCAP_MSG_ERROR("adc_toggle_swen fail\n");
			goto exit;
		}
	}

	//Bellow clk swens are ctrled in driver, not from table
	ret = adc_toggle_swen(srccap_dev, "adctbl_swen_dig_2162adc", "OFF");
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto exit;
	}

exit:
	if (clk_info != NULL)
		vfree(clk_info);

	return ret;
}

static int adc_disable_clk(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 idx = 0;
	u16 count = 0;
	enum reg_srccap_adc_input_source_type src_type = REG_SRCCAP_ADC_INPUTSOURCE_OFF;
	struct reg_srccap_adc_clk_info *clk_info = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	clk_info = (struct reg_srccap_adc_clk_info *)
			vzalloc(sizeof(struct reg_srccap_adc_clk_info) * ADCTBL_CLK_SET_NUM);
	if (clk_info == NULL) {
		SRCCAP_MSG_ERROR("vzalloc fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = drv_hwreg_srccap_adc_get_clk_setting(src_type, ADCTBL_CLK_SET_NUM, clk_info, &count);
	if (ret) {
		SRCCAP_MSG_ERROR("get table clk setting fail\n");
		ret = -EPERM;
		goto exit;
	}

	SRCCAP_MSG_INFO("[ADC] CLK nums from ADC table = %u\n", count);
	for (idx = 0; idx < count; idx++) {
		SRCCAP_MSG_INFO("[ADC] CLK[%2u]: Name= %-35s, parent=%s\n",
			idx, clk_info[idx].clk_name, clk_info[idx].clk_data);

		ret = adc_enable_parent(srccap_dev,
				clk_info[idx].clk_name,
				clk_info[idx].clk_data);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			SRCCAP_MSG_ERROR("adc_enable_parent fail\n");
			goto exit;
		}
	}

exit:
	if (clk_info != NULL)
		vfree(clk_info);

	return ret;
}

static int adc_set_ckg_iclamp_rgb(
	struct mtk_srccap_dev *srccap_dev,
	u16 iclamp_clk_rate)
{
	int ret = 0;
	struct reg_srccap_adc_clk_info clk_info;

	memset(&clk_info, 0, sizeof(struct reg_srccap_adc_clk_info));

	strncpy(clk_info.clk_name, "adctbl_iclamp_rgb_int_ck", CLK_NAME_MAX_LENGTH);

	if (iclamp_clk_rate == 0) {
		strncpy(clk_info.clk_data, "adctbl_prnt_clkd_atop_buf_ck", CLK_DATA_MAX_LENGTH);
		ret = adc_enable_parent(srccap_dev,
				clk_info.clk_name,
				clk_info.clk_data);
		if (ret < 0)
			SRCCAP_MSG_INFO("[ADC] set iclamp_rgb clk error!\n");
	} else {
		strncpy(clk_info.clk_data, "adctbl_prnt_clkd_2iclamp_s_buf_ck",
			CLK_DATA_MAX_LENGTH);
		ret = adc_enable_parent(srccap_dev,
				clk_info.clk_name,
				clk_info.clk_data);
		if (ret < 0)
			SRCCAP_MSG_INFO("[ADC] set iclamp_rgb clk error\n");
	}

	return ret;
}

static bool adc_get_value_from_string(char *buf, char *name,
	unsigned int len, int *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		SRCCAP_MSG_ERROR("Pointer is NULL!\n");
		return false;
	}
	SRCCAP_MSG_DEBUG("enter\n");
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
				SRCCAP_MSG_ERROR("kstrtoint fail!\n");
				return find;
			}
			find = true;
			SRCCAP_MSG_DEBUG(
				"name = %s, value = %d\n", name, *value);
			SRCCAP_MSG_DEBUG("exit\n");
			return find;
		}
	}

	SRCCAP_MSG_DEBUG("name(%s) was not found!\n", name);
	SRCCAP_MSG_DEBUG("exit\n");

	return find;
}

static int adc_set_offset(
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_offset hwreg_offset;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&hwreg_offset, 0, sizeof(struct reg_srccap_adc_rgb_offset));

	hwreg_offset.r_offset = offset->r_offset;
	hwreg_offset.g_offset = offset->g_offset;
	hwreg_offset.b_offset = offset->b_offset;
	ret = drv_hwreg_srccap_adc_set_rgb_offset(hwreg_offset, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg set rgb offset fail\n");
		return ret;
	}

	SRCCAP_MSG_INFO("RGB offset:(%u, %u,%u)\n",
		hwreg_offset.r_offset,
		hwreg_offset.g_offset,
		hwreg_offset.b_offset);
	return ret;
}

static int adc_get_offset(
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_offset hwreg_offset;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	memset(&hwreg_offset, 0, sizeof(struct reg_srccap_adc_rgb_offset));

	ret = drv_hwreg_srccap_adc_get_rgb_offset(&hwreg_offset);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg get offset fail\n");
		return ret;
	}
	offset->r_offset = hwreg_offset.r_offset;
	offset->g_offset = hwreg_offset.g_offset;
	offset->b_offset = hwreg_offset.b_offset;

	return ret;
}

static int adc_set_gain(
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_gain hwreg_gain;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&hwreg_gain, 0, sizeof(struct reg_srccap_adc_rgb_gain));

	hwreg_gain.r_gain = gain->r_gain;
	hwreg_gain.g_gain = gain->g_gain;
	hwreg_gain.b_gain = gain->b_gain;
	ret = drv_hwreg_srccap_adc_set_rgb_gain(hwreg_gain, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set rgb gain fail\n");
		return ret;
	}
	SRCCAP_MSG_INFO("RGB gain:(%x, %x,%x)\n",
		hwreg_gain.r_gain,
		hwreg_gain.g_gain,
		hwreg_gain.b_gain);
	return ret;
}

static int adc_get_gain(
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_adc_rgb_gain hwreg_gain;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	memset(&hwreg_gain, 0, sizeof(struct reg_srccap_adc_rgb_gain));

	ret = drv_hwreg_srccap_adc_get_rgb_gain(&hwreg_gain);
	if (ret) {
		SRCCAP_MSG_ERROR("get gain fail\n");
		return ret;
	}
	gain->r_gain = hwreg_gain.r_gain;
	gain->g_gain = hwreg_gain.g_gain;
	gain->b_gain = hwreg_gain.b_gain;

	return ret;
}

static int adc_execute_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_adc_callback_stage stage)
{
	int ret = 0;
	int num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_ADC_CB_STAGE_MAX) ||
		(stage < SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_GAIN_FUNCTION)) {
		SRCCAP_MSG_ERROR("failed to execute callback: %d, (max %d)\n", stage,
		SRCCAP_ADC_CB_STAGE_MAX);
		return -EINVAL;
	}

	for (num = 0; num < SRCCAP_ADC_CB_STAGE_MAX; num++) {
		SRCCAP_ADC_INTCB cb =
			srccap_dev->adc_info.cb_info[stage][num].cb;
		void *param =
			srccap_dev->adc_info.cb_info[stage][num].param;
		if (cb != NULL) {
			SRCCAP_MSG_DEBUG("execute callback: stage:%d, num:%d\n", stage, num);
			cb(param);
		}
	}

	return ret;
}

static void adc_init_external_auto_tune_gain_offset(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_adc_gain *gain,
	struct v4l2_adc_offset *offset)
{
	int ret = 0;

	SRCCAP_MSG_INFO("[ADC] init gain, R=%u, G=%u, B=%u\n",
		gain->r_gain,
		gain->g_gain,
		gain->b_gain);
	gain->r_gain = (1 << ADC_GAIN_BIT_CNT) >> 1;
	gain->g_gain = (1 << ADC_GAIN_BIT_CNT) >> 1;
	gain->b_gain = (1 << ADC_GAIN_BIT_CNT) >> 1;

	ret = adc_set_gain(gain, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("set rgb gain fail, ret:%d\n", ret);
		return;
	}

	SRCCAP_MSG_INFO("[ADC] init offset, R=%u, G=%u, B=%u\n",
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g,
		srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b);
	offset->r_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_r;
	offset->g_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_g;
	offset->b_offset = srccap_dev->srccap_info.cap.adc_ypbpr_fixed_offset_b;
	ret = adc_set_offset(offset, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("set fixed offset fail, ret:%d\n", ret);
		return;
	}

}

static void adc_check_auto_gain_max_value_status(
	struct mtk_srccap_dev *srccap_dev,
	u8 *atg_status)
{
	u16 retry_count = 0;
	u8 hwreg_atg_status = 0;

	 // Try 5 times to make sure gain values overflow indeed
	for (retry_count = 0; retry_count < SRCCAP_ADC_MAX_VALUE_STATUS_RETRY_CNT;
		retry_count++) {
		adc_execute_callback(srccap_dev,
			SRCCAP_ADC_CB_STAGE_GET_AUTO_GAIN_MAX_VALUE_STATUS);
		hwreg_atg_status = srccap_dev->timingdetect_info.auto_gain_max_value_status;

		if (!(hwreg_atg_status & SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG))
			*atg_status = *atg_status
				& ~SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG;

		if (!(hwreg_atg_status & SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG))
			*atg_status = *atg_status
				& ~SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG;

		if (!(hwreg_atg_status & SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG))
			*atg_status = *atg_status
				& ~SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG;

		mdelay(SRCCAP_THREE_VSYNC * SRCCAP_24FPS_FRAME_TIME);

		if (hwreg_atg_status == 0)
			break;
	}

	SRCCAP_MSG_INFO("[auto gain]atg_status:%x\n", *atg_status);
}

static int adc_get_filter(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_adc_filter_info *adc_filter_info)
{
	int ret = 0;
	struct reg_srccap_adc_filter_info reg_adc_filter_info;

	if (adc_filter_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&reg_adc_filter_info, 0, sizeof(struct reg_srccap_adc_filter_info));
	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = drv_hwreg_srccap_adc_get_filter(&reg_adc_filter_info);
	if (ret) {
		SRCCAP_MSG_ERROR("drv_hwreg_srccap_adc_get_filter fail\n");
		return ret;
	}

	adc_filter_info->blpf_bwp = reg_adc_filter_info.blpf_bwp;
	adc_filter_info->blpf_bwm = reg_adc_filter_info.blpf_bwm;
	adc_filter_info->glpf_bwp = reg_adc_filter_info.glpf_bwp;
	adc_filter_info->glpf_bwm = reg_adc_filter_info.glpf_bwm;
	adc_filter_info->rlpf_bwp = reg_adc_filter_info.rlpf_bwp;
	adc_filter_info->rlpf_bwm = reg_adc_filter_info.rlpf_bwm;
	adc_filter_info->test_vdclp_str = reg_adc_filter_info.test_vdclp_str;
	adc_filter_info->test_adc_clkmode = reg_adc_filter_info.test_adc_clkmode;
	adc_filter_info->test_vdlpf_y_out = reg_adc_filter_info.test_vdlpf_y_out;
	adc_filter_info->test_vdlpf_c_out = reg_adc_filter_info.test_vdlpf_c_out;

	SRCCAP_MSG_DEBUG("filter info:r(%x, %x),g(%x, %x),b(%x, %x)\n",
		adc_filter_info->rlpf_bwp, adc_filter_info->rlpf_bwm,
		adc_filter_info->glpf_bwp, adc_filter_info->glpf_bwm,
		adc_filter_info->blpf_bwp, adc_filter_info->blpf_bwm);
	SRCCAP_MSG_DEBUG("vdclp_str:%x, adc_clkmode:%d, vdlpf_y_out:%d, vdlpf_c_out:%d\n",
		adc_filter_info->test_vdclp_str,
		adc_filter_info->test_adc_clkmode,
		adc_filter_info->test_vdlpf_y_out,
		adc_filter_info->test_vdlpf_c_out);
	return 0;
}

static int adc_set_filter(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_adc_filter_info *adc_filter_info)
{
	int ret = 0;
	struct reg_srccap_adc_filter_info reg_adc_filter_info;

	if (adc_filter_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	memset(&reg_adc_filter_info, 0, sizeof(struct reg_srccap_adc_filter_info));
	reg_adc_filter_info.blpf_bwp = adc_filter_info->blpf_bwp;
	reg_adc_filter_info.blpf_bwm =  adc_filter_info->blpf_bwm;
	reg_adc_filter_info.glpf_bwp =  adc_filter_info->glpf_bwp;
	reg_adc_filter_info.glpf_bwm =  adc_filter_info->glpf_bwm;
	reg_adc_filter_info.rlpf_bwp =  adc_filter_info->rlpf_bwp;
	reg_adc_filter_info.rlpf_bwm =  adc_filter_info->rlpf_bwm;
	reg_adc_filter_info.test_vdclp_str =  adc_filter_info->test_vdclp_str;
	reg_adc_filter_info.test_adc_clkmode =  adc_filter_info->test_adc_clkmode;
	reg_adc_filter_info.test_vdlpf_y_out =  adc_filter_info->test_vdlpf_y_out;
	reg_adc_filter_info.test_vdlpf_c_out =  adc_filter_info->test_vdlpf_c_out;
	ret = drv_hwreg_srccap_adc_set_filter(&reg_adc_filter_info, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set filter fail\n");
		return ret;
	}

	return 0;
}

static int adc_set_filter_value_max(struct mtk_srccap_dev *srccap_dev)
{
	struct srccap_adc_filter_info adc_filter_info;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&adc_filter_info, 0, sizeof(struct srccap_adc_filter_info));

	adc_filter_info.blpf_bwp = SRCCAP_U8_MAX;
	adc_filter_info.blpf_bwm = SRCCAP_U8_MAX;
	adc_filter_info.glpf_bwp = SRCCAP_U8_MAX;
	adc_filter_info.glpf_bwm = SRCCAP_U8_MAX;
	adc_filter_info.rlpf_bwp = SRCCAP_U8_MAX;
	adc_filter_info.rlpf_bwm = SRCCAP_U8_MAX;
	adc_filter_info.test_vdclp_str = SRCCAP_U8_MAX;
	adc_filter_info.test_adc_clkmode = TRUE;
	adc_filter_info.test_vdlpf_y_out = TRUE;
	adc_filter_info.test_vdlpf_c_out = TRUE;

	ret = adc_set_filter(srccap_dev, &adc_filter_info);
	if (ret) {
		SRCCAP_MSG_ERROR("set filter fail\n");
		return ret;
	}

	return 0;
}

static int adc_wait_auto_gain_ready(
	struct mtk_srccap_dev *srccap_dev)
{
	u16 timeout = 0;

	do {
		adc_execute_callback(srccap_dev,
			SRCCAP_ADC_CB_STAGE_GET_AUTO_GAIN_STATUS);
		timeout++;
	} while ((srccap_dev->timingdetect_info.auto_gain_ready_status == FALSE)
			&& (timeout < SRCCAP_ADC_AUTO_GAIN_READY_TIMEOUT));

	return 0;
}

static void adc_tune_gain_value(
	u8 atg_status,
	u16 gain_delta,
	u8 *flow_flag,
	struct v4l2_adc_gain *gain)
{
	// Red - Pr
	if (atg_status & SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG) {
		gain->r_gain -= gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_R_SUB;
		SRCCAP_MSG_INFO("[auto gain]R-\n");
	} else {
		gain->r_gain += gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_R_ADD;
		SRCCAP_MSG_INFO("[auto gain]R+\n");
	}

	// Green - Y
	if (atg_status & SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG) {
		gain->g_gain -= gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_G_SUB;
		SRCCAP_MSG_INFO("[auto gain]G-\n");
	} else {
		gain->g_gain += gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_G_ADD;
		SRCCAP_MSG_INFO("[auto gain]G+\n");
	}

	// Blue - Pb
	if (atg_status & SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG) {
		gain->b_gain -= gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_B_SUB;
		SRCCAP_MSG_INFO("[auto gain]B-\n");
	} else {
		gain->b_gain += gain_delta;
		*flow_flag |= SRCCAP_ADC_FLOW_FLAG_B_ADD;
		SRCCAP_MSG_INFO("[auto gain]B+\n");
	}
	SRCCAP_MSG_INFO("[auto gain]flow_flag:%x, gain(%x, %x, %x)\n",
		*flow_flag,
		gain->r_gain, gain->g_gain, gain->b_gain);
}

static void adc_decrease_gain_value(struct v4l2_adc_gain *gain)
{
	if (gain->r_gain > SRCCAP_ADC_AUTO_GAIN_FINE_TUNE)
		gain->r_gain -= SRCCAP_ADC_AUTO_GAIN_FINE_TUNE;
	else
		gain->r_gain = 0;

	if (gain->g_gain > SRCCAP_ADC_AUTO_GAIN_FINE_TUNE)
		gain->g_gain -= SRCCAP_ADC_AUTO_GAIN_FINE_TUNE;
	else
		gain->g_gain = 0;

	if (gain->b_gain > SRCCAP_ADC_AUTO_GAIN_FINE_TUNE)
		gain->b_gain -= SRCCAP_ADC_AUTO_GAIN_FINE_TUNE;
	else
		gain->b_gain = 0;
	SRCCAP_MSG_INFO("[auto gain]gain(%x, %x, %x)\n",
		gain->r_gain, gain->g_gain,
		gain->b_gain);
}

static void adc_finetune_gain_value(
	u8 atg_status,
	u8 *flow_flag,
	struct v4l2_adc_gain *gain)
{
	// Red - Pr
	if ((*flow_flag & SRCCAP_ADC_FLOW_FLAG_R) != SRCCAP_ADC_FLOW_FLAG_R) {
		if (atg_status & SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG) {
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_R_SUB;
			if ((*flow_flag & SRCCAP_ADC_FLOW_FLAG_R_ADD)
					!= SRCCAP_ADC_FLOW_FLAG_R_ADD) {
				gain->r_gain -= SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
				SRCCAP_MSG_INFO("[auto gain]R-\n");
			}
		} else {
			gain->r_gain += SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_R_ADD;
			SRCCAP_MSG_INFO("[auto gain]R+\n");
		}
	}

	// Green - Y
	if ((*flow_flag & (SRCCAP_ADC_FLOW_FLAG_G)) != (SRCCAP_ADC_FLOW_FLAG_G)) {
		if (atg_status & SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG) {
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_G_SUB;
			if ((*flow_flag & SRCCAP_ADC_FLOW_FLAG_G_ADD)
					!= SRCCAP_ADC_FLOW_FLAG_G_ADD) {
				gain->g_gain -= SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
				SRCCAP_MSG_INFO("[auto gain]G-\n");
			}
		} else {
			gain->g_gain += SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_G_ADD;
			SRCCAP_MSG_INFO("[auto gain]G+\n");
		}
	}

	// Blue - Pb
	if ((*flow_flag & SRCCAP_ADC_FLOW_FLAG_B) != SRCCAP_ADC_FLOW_FLAG_B) {
		if (atg_status & SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG) {
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_B_SUB;
			if ((*flow_flag & SRCCAP_ADC_FLOW_FLAG_B_ADD)
					!= SRCCAP_ADC_FLOW_FLAG_B_ADD) {
				gain->b_gain -= SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
				SRCCAP_MSG_INFO("[auto gain]B-\n");
			}
		} else {
			gain->b_gain += SRCCAP_ADC_AUTO_YUV_GAIN_STEP;
			*flow_flag |= SRCCAP_ADC_FLOW_FLAG_B_ADD;
			SRCCAP_MSG_INFO("[auto gain]B+\n");
		}
	}
	SRCCAP_MSG_INFO("[auto gain]flow_flag:%x, gain(%x, %x, %x)\n",
		*flow_flag,
		gain->r_gain, gain->g_gain,
		gain->b_gain);
}

static void adc_finetune_gain(
	struct mtk_srccap_dev *srccap_dev,
	u8 *flow_flag,
	struct v4l2_adc_gain *gain,
	struct v4l2_adc_offset *offset)
{
	u16 gain_delta =  (1 << ADC_GAIN_BIT_CNT) - 1;
	u16 i = 0;
	u8 atg_status = 0;

	SRCCAP_MSG_INFO("[auto gain]fine tune, gain_delta:%x\n", gain_delta);
	for (i = 0; i < gain_delta; i++) {
		SRCCAP_MSG_INFO("[auto gain]i :%d, RGB gain:(%x, %x,%x)\n",
			i,
			gain->r_gain,
			gain->g_gain,
			gain->b_gain);
		adc_set_gain(gain, srccap_dev);
		adc_set_offset(offset, srccap_dev);
		mdelay(SRCCAP_THREE_VSYNC * SRCCAP_24FPS_FRAME_TIME);
		adc_wait_auto_gain_ready(srccap_dev);

		atg_status = SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG |
			SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG |
			SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG;
		adc_check_auto_gain_max_value_status(srccap_dev, &atg_status);
		adc_finetune_gain_value(atg_status, flow_flag, gain);
		SRCCAP_MSG_INFO("*flow_flag:%x, i:%d\n", *flow_flag, i);
		if (*flow_flag == SRCCAP_ADC_FLOW_FLAG_MAX
			|| i > SRCCAP_ADC_AUTO_YUV_MAX_CNT)
			break;
	}
}

static int adc_set_external_auto_tune_ypbpr_gain(
		struct mtk_srccap_dev *srccap_dev,
		u8 *flow_flag,
		struct v4l2_adc_gain *gain_tmp)
{
	u16 gain_delta =  (1 << ADC_GAIN_BIT_CNT) >> 1;
	struct srccap_adc_filter_info hwreg_adc_filter_info;
	u8 atg_status = 0;
	int ret = 0;
	struct v4l2_adc_offset offset_tmp;

	if ((srccap_dev == NULL) || (flow_flag == NULL)
		 || (gain_tmp == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&offset_tmp, 0, sizeof(struct v4l2_adc_offset));
	memset(&hwreg_adc_filter_info, 0, sizeof(struct srccap_adc_filter_info));


	adc_get_filter(srccap_dev, &hwreg_adc_filter_info);
	adc_set_filter_value_max(srccap_dev);

	adc_init_external_auto_tune_gain_offset(srccap_dev, gain_tmp, &offset_tmp);

	SRCCAP_MSG_INFO("[auto gain]gain_delta:%x, gain(%x,%x,%x)\n",
		gain_delta,
		gain_tmp->r_gain,
		gain_tmp->g_gain,
		gain_tmp->b_gain);
	while (1) {
		gain_delta >>= 1;
		if (gain_delta == 0)
			break;
		SRCCAP_MSG_INFO("[auto gain]gain_delta:%x\n", gain_delta);
		adc_set_gain(gain_tmp, srccap_dev);
		adc_set_offset(&offset_tmp, srccap_dev);
		mdelay(SRCCAP_THREE_VSYNC * SRCCAP_24FPS_FRAME_TIME);
		adc_wait_auto_gain_ready(srccap_dev);

		atg_status = SRCCAP_ADC_AUTO_GAIN_R_PR_OVER_MAX_FLAG |
			SRCCAP_ADC_AUTO_GAIN_G_Y_OVER_MAX_FLAG |
			SRCCAP_ADC_AUTO_GAIN_B_PB_OVER_MAX_FLAG;
		adc_check_auto_gain_max_value_status(srccap_dev, &atg_status);

		adc_tune_gain_value(atg_status, gain_delta, flow_flag, gain_tmp);
	}
	*flow_flag = 0;
	adc_decrease_gain_value(gain_tmp);
	adc_finetune_gain(srccap_dev, flow_flag, gain_tmp, &offset_tmp);

	adc_set_filter(srccap_dev, &hwreg_adc_filter_info);
	return ret;
}

static void adc_set_calibrartion_window(
	struct mtk_srccap_dev *srccap_dev,
	bool enable)
{

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	SRCCAP_MSG_DEBUG("[auto gain]enable:%d\n", enable);
	if (enable == TRUE) {
		adc_execute_callback(srccap_dev,
			SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_RANGE_FUNCTION);
		adc_execute_callback(srccap_dev,
			SRCCAP_ADC_CB_STAGE_SET_AUTO_RANGE_WINDOW);

	} else
		adc_execute_callback(srccap_dev,
			SRCCAP_ADC_CB_STAGE_DISABLE_AUTO_RANGE_FUNCTION);
}

static int adc_perform_external_auto_calibration(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_adc_auto_tune_type auto_tune_type,
	u8 *flow_flag,
	struct v4l2_adc_gain *gain_tmp)
{
	int ret = 0;

	if ((srccap_dev == NULL) || (flow_flag == NULL)
		 || (gain_tmp == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	adc_set_calibrartion_window(srccap_dev, TRUE);
	adc_execute_callback(srccap_dev,
		SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_GAIN_FUNCTION);

	mdelay(SRCCAP_THREE_VSYNC * SRCCAP_24FPS_FRAME_TIME);

	switch (auto_tune_type) {
	case V4L2_ADC_AUTO_TUNE_YUV_COLOR:
		SRCCAP_MSG_DEBUG("[auto gain]V4L2_ADC_AUTO_TUNE_YUV_COLOR\n");
		ret = mtk_adc_load_cal_tbl(srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto exit;
		}

		ret = adc_set_external_auto_tune_ypbpr_gain(srccap_dev,
							flow_flag, gain_tmp);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto exit;
		}

		ret = adc_set_fixed_offset(srccap_dev);
		if (ret) {
			SRCCAP_MSG_ERROR("set adc fixed offset fail:%d\n", ret);
			goto exit;
		}
		break;

	default:
		SRCCAP_MSG_DEBUG("unsupport auto_tune_type:%d\n", auto_tune_type);
		break;
	}

exit:
	adc_execute_callback(srccap_dev,
		SRCCAP_ADC_CB_STAGE_DISABLE_AUTO_GAIN_FUNCTION);
	adc_set_calibrartion_window(srccap_dev, FALSE);

	return ret;
}

static int adc_perform_auto_calibration(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_adc_auto_tune_type auto_tune_type,
	u8 *flow_flag,
	struct v4l2_adc_gain *gain_tmp)
{
	int ret = 0;

	if (srccap_dev->adc_info.auto_calibration_mode == V4L2_ADC_HW_CALIBRATION) {
		SRCCAP_MSG_DEBUG("[auto gain]HW_CALIBRATION\n");
		ret = adc_set_fixed_gain(srccap_dev);
		if (ret) {
			SRCCAP_MSG_ERROR("set adc fixed gain fail:%d\n", ret);
			return ret;
		}

		ret = adc_set_fixed_offset(srccap_dev);
		if (ret) {
			SRCCAP_MSG_ERROR("set adc fixed offset fail:%d\n", ret);
			return ret;
		}
	} else {
		SRCCAP_MSG_DEBUG("[auto gain]SW_CALIBRATION\n");
		ret = adc_perform_external_auto_calibration(srccap_dev, auto_tune_type,
								flow_flag, gain_tmp);
		if (ret) {
			SRCCAP_MSG_ERROR("adc_perform_external_auto_calibration fail:%d\n", ret);
			return ret;
		}
	}
	return ret;
}

static int adc_check_auto_gain_offset_result(
	struct mtk_srccap_dev *srccap_dev,
	u8 flow_flag,
	struct v4l2_adc_gain *orig_gain,
	struct v4l2_adc_gain *tuned_gain,
	enum v4l2_adc_auto_tune_type auto_tune_type)
{
	int ret = 0;

	if ((srccap_dev == NULL) || (orig_gain == NULL)
		|| (tuned_gain == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->adc_info.auto_calibration_mode == V4L2_ADC_SW_CALIBRATION) {
		switch (auto_tune_type) {
		case V4L2_ADC_AUTO_TUNE_YUV_COLOR:
			SRCCAP_MSG_DEBUG(
				"[auto gain]V4L2_ADC_AUTO_TUNE_YUV_COLOR\n");

			if (flow_flag == SRCCAP_ADC_FLOW_FLAG_MAX) {
				SRCCAP_MSG_DEBUG(
					"[auto gain]success: RGB gain:(%x,%x,%x)\n",
					tuned_gain->r_gain,
					tuned_gain->g_gain,
					tuned_gain->b_gain);
				adc_set_gain(tuned_gain, srccap_dev);
			} else {
				SRCCAP_MSG_DEBUG("[auto gain]fail, flow_flag:%x\n",
									flow_flag);
				// Restore original setting.
				adc_set_gain(orig_gain, srccap_dev);
			}
			break;
		default:
			SRCCAP_MSG_DEBUG("unsupport auto_tune_type:%d\n", auto_tune_type);
			break;
		}
	}
	return ret;
}

/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
static int mtk_adc_set_gain(
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_set_gain(gain, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("set rgb gain fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_gain(
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_get_gain(gain, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("get gain fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_center_gain(
	u16 *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	*gain = ADC_CENTER_GAIN;

	return ret;
}


static int mtk_adc_set_offset(
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_set_offset(offset, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_set_offset fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_offset(
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_get_offset(offset, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg get offset fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_center_offset(
	u16 *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	*offset = ADC_CENTER_OFFSET;

	return ret;
}

static int mtk_adc_set_phase(
	u16 *phase,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u16 adcpllmod = 0;

	if (phase == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = drv_hwreg_srccap_adc_get_adcpllmod(&adcpllmod);
	if (ret) {
		SRCCAP_MSG_ERROR("get adcpllmod fail\n");
		return ret;
	}

	ret = drv_hwreg_srccap_adc_set_phase(*phase, adcpllmod, true, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("set phase fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_phase(
	u16 *phase,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u16 adcpllmod = 0;

	if (phase == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = drv_hwreg_srccap_adc_get_adcpllmod(&adcpllmod);
	if (ret) {
		SRCCAP_MSG_ERROR("get adcpllmod fail\n");
		return ret;
	}

	ret = drv_hwreg_srccap_adc_get_phase(phase, adcpllmod);
	if (ret) {
		SRCCAP_MSG_ERROR("get phase fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_set_auto_gain_offset(
	enum v4l2_adc_auto_tune_type auto_tune_type,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 flow_flag = 0;
	struct v4l2_adc_gain gain_tmp, hwreg_gain;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&gain_tmp, 0, sizeof(struct v4l2_adc_gain));
	memset(&hwreg_gain, 0, sizeof(struct v4l2_adc_gain));

	adc_get_gain(&hwreg_gain, srccap_dev);
	ret = adc_perform_auto_calibration(srccap_dev, auto_tune_type, &flow_flag, &gain_tmp);
	if (ret < 0) {
		SRCCAP_MSG_ERROR("adc_perform_auto_calibration fail\n");
		return ret;
	}

	ret = adc_check_auto_gain_offset_result(srccap_dev, flow_flag, &hwreg_gain,
							&gain_tmp, auto_tune_type);
	if (ret < 0) {
		SRCCAP_MSG_ERROR("adc_check_auto_gain_offset_result fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_set_auto_calibration_mode(
	enum v4l2_adc_auto_calibration_mode auto_calibration_mode,
	struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev->adc_info.auto_calibration_mode = auto_calibration_mode;

	SRCCAP_MSG_INFO("[auto_gain]auto_calibration_mode:%d\n",
		srccap_dev->adc_info.auto_calibration_mode);

	return 0;
}

static int mtk_adc_get_fixed_gain(
	enum v4l2_srccap_input_source adc_input_src,
	struct v4l2_adc_gain *gain,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (gain == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_get_fixed_gain(adc_input_src, gain, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("get gain fail\n");
		return ret;
	}

	return ret;
}


static int mtk_adc_get_fixed_offset(
	enum v4l2_srccap_input_source adc_input_src,
	struct v4l2_adc_offset *offset,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (offset == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_get_fixed_offset(adc_input_src, offset, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg get offset fail\n");
		return ret;
	}

	return ret;
}

static int mtk_adc_get_fixed_phase(
	enum v4l2_srccap_input_source adc_input_src,
	u16 *phase,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (phase == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_get_fixed_phase(adc_input_src, phase, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("get adcpllmod fail\n");
		return ret;
	}

	return ret;
}


static int _adc_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, adc_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_ADC_GAIN:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_GAIN\n");
		ret = mtk_adc_get_gain(
		(struct v4l2_adc_gain *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_OFFSET\n");
		ret = mtk_adc_get_offset(
		(struct v4l2_adc_offset *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_CENTER_GAIN:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_GAIN\n");
		ret = mtk_adc_get_center_gain((__u16 *)&(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_CENTER_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_OFFSET\n");
		ret = mtk_adc_get_center_offset((__u16 *)&(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_PHASE:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_PHASE\n");
		ret = mtk_adc_get_phase((__u16 *)&(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_IS_SCARTRGB:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_IS_SCARTRGB\n");
		ret = drv_hwreg_srccap_adc_get_scart_type((bool *)&(ctrl->val));
		break;
	}
	case V4L2_CID_ADC_YPBPR_FIXED_GAIN:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_GAIN\n");
		ret = mtk_adc_get_fixed_gain(V4L2_SRCCAP_INPUT_SOURCE_YPBPR,
		(struct v4l2_adc_gain *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_YPBPR_FIXED_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_OFFSET\n");
		ret = mtk_adc_get_fixed_offset(V4L2_SRCCAP_INPUT_SOURCE_YPBPR,
		(struct v4l2_adc_offset *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_YPBPR_FIXED_PHASE:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_PHASE\n");
		ret = mtk_adc_get_fixed_phase(V4L2_SRCCAP_INPUT_SOURCE_YPBPR,
		(__u16 *)&(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_VGA_FIXED_GAIN:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_GAIN\n");
		ret = mtk_adc_get_fixed_gain(V4L2_SRCCAP_INPUT_SOURCE_VGA,
		(struct v4l2_adc_gain *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_VGA_FIXED_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_OFFSET\n");
		ret = mtk_adc_get_fixed_offset(V4L2_SRCCAP_INPUT_SOURCE_VGA,
		(struct v4l2_adc_offset *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_VGA_FIXED_PHASE:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_ADC_FIXED_PHASE\n");
		ret = mtk_adc_get_fixed_phase(V4L2_SRCCAP_INPUT_SOURCE_VGA,
		(__u16 *)&(ctrl->val), srccap_dev);
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

static int _adc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, adc_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_ADC_SOURCE:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_SOURCE\n");
		ret = mtk_adc_set_source(
		(struct v4l2_adc_source *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_GAIN:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_GAIN\n");
		ret = mtk_adc_set_gain(
		(struct v4l2_adc_gain *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_OFFSET\n");
		ret = mtk_adc_set_offset(
		(struct v4l2_adc_offset *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_ADC_PHASE:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_PHASE\n");
		ret = mtk_adc_set_phase((__u16 *)&(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_AUTO_GAIN_OFFSET:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_AUTO_GAIN_OFFSET\n");
		ret = mtk_adc_set_auto_gain_offset(
			(enum v4l2_adc_auto_tune_type)(ctrl->val), srccap_dev);
		break;
	}
	case V4L2_CID_ADC_AUTO_GEOMETRY:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_AUTO_GEOMETRY\n");
		break;
	}
	case V4L2_CID_ADC_AUTO_CALIBRATION_MODE:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_ADC_AUTO_CALIBRATION_MODE\n");
		ret = mtk_adc_set_auto_calibration_mode(
			(enum v4l2_adc_auto_calibration_mode)(ctrl->val), srccap_dev);
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

static const struct v4l2_ctrl_ops adc_ctrl_ops = {
	.g_volatile_ctrl = _adc_g_ctrl,
	.s_ctrl = _adc_s_ctrl,
};

static const struct v4l2_ctrl_config adc_ctrl[] = {
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_SOURCE,
		.name = "Srccap ADC Set Source",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_source)},
		.flags =
		V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_GAIN,
		.name = "Srccap ADC Gain",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_gain)},
		.flags =
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_OFFSET,
		.name = "Srccap ADC Offset",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_offset)},
		.flags =
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_PHASE,
		.name = "Srccap ADC Phase",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_IS_SCARTRGB,
		.name = "Srccap ADC Is ScartRGB",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = false,
		.max = true,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_AUTO_GAIN_OFFSET,
		.name = "Srccap ADC Set Auto Gain Offset",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_AUTO_CALIBRATION_MODE,
		.name = "Srccap ADC Set Auto Calibration mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_CENTER_GAIN,
		.name = "Srccap ADC center gain",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_CENTER_OFFSET,
		.name = "Srccap ADC center offset",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.flags =
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_YPBPR_FIXED_GAIN,
		.name = "Srccap ADC YPBPR FIXED Gain",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_gain)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_YPBPR_FIXED_OFFSET,
		.name = "Srccap ADC YPBPR FIXED Offset",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_offset)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_YPBPR_FIXED_PHASE,
		.name = "Srccap ADC YPBPR FIXED Phase",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_VGA_FIXED_GAIN,
		.name = "Srccap ADC VGA FIXED Gain",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_gain)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_VGA_FIXED_OFFSET,
		.name = "Srccap ADC VGA FIXED Offset",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.dims = {sizeof(struct v4l2_adc_offset)},
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &adc_ctrl_ops,
		.id = V4L2_CID_ADC_VGA_FIXED_PHASE,
		.name = "Srccap ADC VGA FIXED Phase",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops adc_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops adc_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_adc_enable_offline_sog(
	struct mtk_srccap_dev *srccap_dev,
	bool enable)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = drv_hwreg_srccap_adc_enable_offline_sog(enable, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_ERROR("hwreg adc enable offline mux fail\n");
		return ret;
	}

	return ret;
}

int mtk_adc_set_offline_mux(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src)
{
	int ret = 0;
	enum srccap_input_port offline_port = 0;
	enum reg_srccap_adc_input_offline_mux offline_src = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return -EPERM;

	switch (src) {
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
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		if (srccap_dev->srccap_info.map[src].sync_port != 0)
			offline_port = srccap_dev->srccap_info.map[src].sync_port;
		else
			offline_port = srccap_dev->srccap_info.map[src].data_port;

		offline_src = adc_map_offline_port_reg(offline_port);
		if (offline_src == REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_NUM)
			return -EINVAL;

		ret = drv_hwreg_srccap_adc_set_offline_sog_mux(
			offline_src, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set offline mux fail\n");
			return ret;
		}

		usleep_range(SRCCAP_ADC_UDELAY_1000, SRCCAP_ADC_UDELAY_1100); // sleep 1ms

		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		//TODO
		break;
	default:
		return -EPERM;
	}

	return ret;
}

int mtk_adc_compare_offline_mux(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum v4l2_srccap_input_source target,
	bool *compare_result)
{
	int ret = 0;
	enum srccap_input_port offline_src_port = 0;
	enum srccap_input_port offline_target_port = 0;
	enum reg_srccap_adc_input_offline_mux offline_src = 0;
	enum reg_srccap_adc_input_offline_mux offline_target = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (compare_result == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}


	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return -EPERM;

	switch (src) {
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
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		if (srccap_dev->srccap_info.map[src].sync_port != 0)
			offline_src_port = srccap_dev->srccap_info.map[src].sync_port;
		else
			offline_src_port = srccap_dev->srccap_info.map[src].data_port;

		offline_src = adc_map_offline_port_reg(offline_src_port);
		if (offline_src == REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_NUM)
			return -EINVAL;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		*compare_result = FALSE;
		SRCCAP_MSG_ERROR("not support src = %d\n", src);
		return ret;
	default:
		return -EPERM;
	}


	switch (target) {
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		if (srccap_dev->srccap_info.map[target].sync_port != 0)
			offline_target_port = srccap_dev->srccap_info.map[target].sync_port;
		else
			offline_target_port = srccap_dev->srccap_info.map[target].data_port;

		if (offline_target_port == SRCCAP_INPUT_PORT_NONE) {
			*compare_result = FALSE;
			return 0;
		}

		offline_target = adc_map_offline_port_reg(offline_target_port);
		if (offline_target == REG_SRCCAP_ADC_INPUT_OFFLINE_MUX_NUM)
			return -EINVAL;
		break;
	default:
		SRCCAP_MSG_ERROR("not support target = %d\n", target);
		ret = -EINVAL;
		break;
	}

	if (offline_src == offline_target)
		*compare_result = TRUE;
	else
		*compare_result = FALSE;

	return ret;
}



int mtk_adc_check_offline_status(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool *offline)
{
	int ret = 0;
	int port_index = 0;
	bool online = FALSE;
	enum v4l2_srccap_input_source online_src[SRCCAP_DEV_NUM] = {0};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (offline == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
	ret = adc_split_src(srccap_dev->adc_info.adc_src, &online_src[0], &online_src[1]);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_split_src fail!\n");
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
		return ret;
	}

	for (port_index = 0; port_index < SRCCAP_DEV_NUM; port_index++) {
		if (online_src[port_index] < V4L2_SRCCAP_INPUT_SOURCE_NONE
			|| online_src[port_index] >= V4L2_SRCCAP_INPUT_SOURCE_NUM) {
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
			return -EINVAL;
		}

		if (src == online_src[port_index]) {
			online = TRUE;
			break;
		}
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);

	*offline = !online;

	return 0;
}

int mtk_adc_check_offline_port_conflict(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool *conflict,
	enum v4l2_srccap_input_source *conflicted_src)
{
	int ret = 0;
	int port_index = 0;
	enum srccap_input_port offline_port = 0;
	enum v4l2_srccap_input_source online_src[SRCCAP_DEV_NUM] = {0};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (conflict == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (conflicted_src == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (src < V4L2_SRCCAP_INPUT_SOURCE_NONE || src >= V4L2_SRCCAP_INPUT_SOURCE_NUM)
		return -EINVAL;

	if (srccap_dev->srccap_info.map[src].sync_port != 0)
		offline_port = srccap_dev->srccap_info.map[src].sync_port;
	else
		offline_port = srccap_dev->srccap_info.map[src].data_port;

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
	ret = adc_split_src(srccap_dev->adc_info.adc_src, &online_src[0], &online_src[1]);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_split_src fail!\n");
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
		return ret;
	}

	for (port_index = 0; port_index < SRCCAP_DEV_NUM; port_index++) {
		if (online_src[port_index] < V4L2_SRCCAP_INPUT_SOURCE_NONE
			|| online_src[port_index] >= V4L2_SRCCAP_INPUT_SOURCE_NUM) {
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);
			return -EINVAL;
		}

		if (adc_check_same_port(
				offline_port,
				srccap_dev->srccap_info.map[online_src[port_index]].data_port)
			|| adc_check_same_port(
				offline_port,
				srccap_dev->srccap_info.map[online_src[port_index]].sync_port)) {
			*conflict = TRUE;
			*conflicted_src = online_src[port_index];
			break;
		}
		*conflict = FALSE;
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_online_source);

	return 0;
}

int mtk_adc_set_freerun(
	struct mtk_srccap_dev *srccap_dev,
	bool enable)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	SRCCAP_MSG_INFO("adc set freerun: %s\n", enable?("ENABLE"):("DISABLE"));

	ret = drv_hwreg_srccap_adc_set_freerun(enable);
	if (ret) {
		SRCCAP_MSG_ERROR("load adc freerun table fail\n");
		return ret;
	}

	return ret;
}

int mtk_adc_set_mode(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u64 pix_clock = 0;
	u32 hfreqx10 = 0;
	u16 htt = 0;
	u16 iclamp_clk_rate = 0;
	u16 phase = 0;
	enum srccap_adc_source_info adc_src;
	bool iclamp_delay_div_two_enable = false;
	bool adc_double_sampling_enable = false;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	adc_src = adc_map_analog_source(srccap_dev->srccap_info.src);
	hfreqx10 = srccap_dev->timingdetect_info.data.h_freqx10;
	htt = srccap_dev->timingdetect_info.data.h_total;

	/* set mode */
	pix_clock = (u64)hfreqx10 * (u64)htt;
	do_div(pix_clock, 10000UL); // unit: 1000000Hz

	SRCCAP_MSG_INFO("adc set mode: %llu\n", pix_clock);

	if ((adc_double_sampling_enable == false) &&
		(srccap_dev->timingdetect_info.data.h_de == SRCCAP_ADC_DOUBLE_SAMPLE_RESOLUTION)) {
		iclamp_delay_div_two_enable = true;
		SRCCAP_MSG_INFO("iclamp_delay_div_two_enable:%d\n",
						iclamp_delay_div_two_enable);
	}

	switch (adc_src) {
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
		ret = drv_hwreg_srccap_adc_set_mode(REG_SRCCAP_ADC_SETMODETYPE_YUV, (u16)pix_clock,
			&iclamp_clk_rate, iclamp_delay_div_two_enable);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set adc mode fail\n");
			return ret;
		}
		break;
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
		ret = drv_hwreg_srccap_adc_set_mode(REG_SRCCAP_ADC_SETMODETYPE_RGB, (u16)pix_clock,
			&iclamp_clk_rate, iclamp_delay_div_two_enable);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set adc mode fail\n");
			return ret;
		}
		break;
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
		ret = drv_hwreg_srccap_adc_set_mode(REG_SRCCAP_ADC_SETMODETYPE_YUV_Y,
			(u16)pix_clock, &iclamp_clk_rate, iclamp_delay_div_two_enable);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set adc mode fail\n");
			return ret;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid source:%d\n", adc_src);
		break;
	}

	if ((adc_src == SRCCAP_ADC_SOURCE_ONLY_YPBPR)
		|| (adc_src == SRCCAP_ADC_SOURCE_ONLY_RGB)
		|| (adc_src == SRCCAP_ADC_SOURCE_ONLY_SCART_RGB)) {
		/* set plldiv */
		ret = adc_set_plldiv(srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
		/* set hpol */
		ret = adc_set_hpol(srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* set ckg_iclamp_rgb */
		ret = adc_set_ckg_iclamp_rgb(srccap_dev, iclamp_clk_rate);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* Toggle PLLA reset */
		ret = adc_set_plla_reset();
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* toggle phase */
		ret = mtk_adc_get_phase(&phase, srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = mtk_adc_set_phase(&phase, srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	/* stop ISOG reset to prevent garbage on G channel */
	ret = mtk_adc_set_isog_detect_mode(srccap_dev,
			srccap_dev->srccap_info.src, SRCCAP_ADC_ISOG_NORMAL_MODE);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

int mtk_adc_load_cal_tbl(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum srccap_adc_source_info adc_src = SRCCAP_ADC_SOURCE_NONADC;
	bool use_hw_cal = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	adc_src = adc_map_analog_source(srccap_dev->srccap_info.src);

	SRCCAP_MSG_INFO("adc load calibration table: source=[%d]\n", adc_src);

	use_hw_cal = (srccap_dev->adc_info.auto_calibration_mode
			== V4L2_ADC_HW_CALIBRATION) ? TRUE : FALSE;

	switch (adc_src) {
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
		ret = drv_hwreg_srccap_adc_set_rgb_cal(use_hw_cal, TRUE);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set rgb cal fail\n");
			return ret;
		}
		break;
	case SRCCAP_ADC_SOURCE_ONLY_CVBS:
	case SRCCAP_ADC_SOURCE_ONLY_EXT_DMD_ATV:
	case SRCCAP_ADC_SOURCE_ONLY_SCART_Y:
		ret = drv_hwreg_srccap_adc_set_av_cal();
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set av cal fail\n");
			return ret;
		}
		break;
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
	case SRCCAP_ADC_SOURCE_MULTI_RGB_CVBS:
	case SRCCAP_ADC_SOURCE_MULTI_RGB_SCARTY:
	case SRCCAP_ADC_SOURCE_MULTI_YPBPR_CVBS:
		ret = drv_hwreg_srccap_adc_set_rgb_cal(use_hw_cal, TRUE);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set rgb cal fail\n");
			return ret;
		}

		ret = drv_hwreg_srccap_adc_set_av_cal();
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set av cal fail\n");
			return ret;
		}
		break;
	case SRCCAP_ADC_SOURCE_ONLY_SVIDEO:
		ret = drv_hwreg_srccap_adc_set_sv_cal();
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg set sv cal fail\n");
			return ret;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid source:%d\n", adc_src);
		break;
	}

	return ret;
}

int mtk_adc_set_source(
	struct v4l2_adc_source *user_source,
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct v4l2_adc_source source;
	enum v4l2_srccap_input_source *input = NULL;
	enum reg_srccap_adc_pws_type reg_pws_type = 0;

	if (user_source == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return -EPERM;

#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	/* handle user pointer */
	input = kzalloc((sizeof(enum v4l2_srccap_input_source)
		* user_source->src_count), GFP_KERNEL);
	if (!input) {
		SRCCAP_MSG_ERROR("kzalloc fail\n");
		return -ENOMEM;
	}
	if (copy_from_user((void *)input, (__u8 __user *)user_source->p.adc_input_src,
		sizeof(enum v4l2_srccap_input_source) * user_source->src_count)) {
		SRCCAP_MSG_ERROR("copy_from_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	memset(&source, 0, sizeof(struct v4l2_adc_source));
	source.src_count = user_source->src_count;
	source.p.adc_input_src = input;

	if (SRCCAP_FORCE_SVIDEO) {
		source.p.adc_input_src[0] = V4L2_SRCCAP_INPUT_SOURCE_SVIDEO;
		source.p.adc_input_src[1] = V4L2_SRCCAP_INPUT_SOURCE_NONE;
		SRCCAP_MSG_INFO("input src: svideo\n");
	}

	if (SRCCAP_FORCE_SCART) {
		source.p.adc_input_src[0] = V4L2_SRCCAP_INPUT_SOURCE_SCART;
		source.p.adc_input_src[1] = V4L2_SRCCAP_INPUT_SOURCE_NONE;
		SRCCAP_MSG_INFO("input src: scart\n");
	}

	if (SRCCAP_FORCE_VGA) {
		source.p.adc_input_src[0] = V4L2_SRCCAP_INPUT_SOURCE_VGA;
		source.p.adc_input_src[1] = V4L2_SRCCAP_INPUT_SOURCE_NONE;
		SRCCAP_MSG_INFO("input src: VGA\n");
	}

	//src0=adc src1!=source_none, means src0 already init the adc, no need to do again.
	if ((adc_map_analog_source(source.p.adc_input_src[0]) != SRCCAP_ADC_SOURCE_NONADC) &&
		(source.p.adc_input_src[1] != V4L2_SRCCAP_INPUT_SOURCE_NONE)) {
		kfree(input);
		return ret;
	}

	/* set source setting */
	ret = adc_set_src_setting(&source, srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_set_src_setting fail\n");
		goto out;
	}

	/* set clk swen */
	ret = adc_set_clk_swen(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_set_clk_swen fail\n");
		goto out;
	}
	/* set clk setting */
	ret = adc_set_clk(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc_set_clk fail\n");
		goto out;
	}
	/* set pws setting */
	reg_pws_type = adc_map_pws_reg(srccap_dev->adc_info.adc_src);
	SRCCAP_MSG_INFO("[ADC] adc set pws table idx: (%d)\n", reg_pws_type);
	ret = drv_hwreg_srccap_adc_set_pws(reg_pws_type);
	if (ret) {
		SRCCAP_MSG_ERROR("load pws table fail\n");
		goto out;
	}

	/* adc free run for pll clk enable */
	ret = mtk_adc_set_freerun(srccap_dev, true);
	if (ret) {
		SRCCAP_MSG_ERROR("load adc freerun_en table fail\n");
		goto out;
	}

out:
	kfree(input);
#endif
	return ret;
}

int mtk_adc_set_isog_detect_mode(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	enum srccap_adc_isog_detect_mode mode)
{
	int ret = 0;
	bool enable = false;
	enum srccap_adc_source_info adc_src = SRCCAP_ADC_SOURCE_NONADC;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if ((mode < SRCCAP_ADC_ISOG_NORMAL_MODE) || (mode >= SRCCAP_ADC_ISOG_MODE_NUMS)) {
		SRCCAP_MSG_ERROR("Invalid isog detect mode %d\n", mode);
		return -EINVAL;
	}
	adc_src = adc_map_analog_source(src);

	switch (adc_src) {
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
		ret = drv_hwreg_srccap_adc_get_isog_enable(&enable);
		if (ret) {
			SRCCAP_MSG_ERROR("hwreg get isog enable fail\n");
			goto out;
		}

		if (enable) {
			switch (mode) {
			case SRCCAP_ADC_ISOG_NORMAL_MODE:
				drv_hwreg_srccap_adc_set_isog_reset_width(0, true, NULL, NULL);
				break;
			case SRCCAP_ADC_ISOG_STANDBY_MODE:
				drv_hwreg_srccap_adc_set_isog_reset_width(1, true, NULL, NULL);
				break;
			default:
				SRCCAP_MSG_ERROR("Invalid isog detect mode %d\n", mode);
				goto out;
			}
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid source:%d\n", adc_src);
		break;
	}
out:
	return ret;
}

int mtk_adc_set_iclamp_rgb_powerdown(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_srccap_input_source src,
	bool powerdown)
{
	int ret = 0;
	enum srccap_adc_source_info adc_src = SRCCAP_ADC_SOURCE_NONADC;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	adc_src = adc_map_analog_source(src);

	switch (adc_src) {
	case SRCCAP_ADC_SOURCE_ONLY_YPBPR:
	case SRCCAP_ADC_SOURCE_ONLY_RGB:
	case SRCCAP_ADC_SOURCE_ONLY_SCART_RGB:
		ret = drv_hwreg_srccap_adc_set_iclamp_rgb_powerdown(powerdown, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_ERROR("hwreg set iclamp rgb power down fail\n");
			goto out;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid source:%d\n", adc_src);
		break;
	}
out:
	return ret;
}

int mtk_adc_init(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u32 hw_version = 0;
	enum reg_srccap_adc_input_port reg_port = REG_SRCCAP_ADC_INPUT_PORT_NONE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	memset(&(srccap_dev->adc_info), 0, sizeof(struct srccap_adc_info));
	srccap_dev->adc_info.adc_src = SRCCAP_ADC_SOURCE_NONADC;
	hw_version = srccap_dev->srccap_info.cap.hw_version;

	/* set hw version*/
	ret = drv_hwreg_srccap_adc_set_hw_version(hw_version);

	/* set default swen */
	ret = adc_set_clk_swen(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc set init clock swen fail\n");
		goto out;
	}

	/* set default clk setting */
	ret = adc_set_clk(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc set init clk fail\n");
		goto out;
	}

	/* load pws table */
	ret = drv_hwreg_srccap_adc_set_pws(adc_map_pws_reg(srccap_dev->adc_info.adc_src));
	if (ret) {
		SRCCAP_MSG_ERROR("load pws table fail\n");
		goto out;
	}
	usleep_range(100, 101); //need to delay 100us for HW SOG cal

	/* set isog detect mode */
	ret = mtk_adc_set_isog_detect_mode(
		srccap_dev, srccap_dev->srccap_info.src, SRCCAP_ADC_ISOG_STANDBY_MODE);
	if (ret) {
		SRCCAP_MSG_ERROR("set adc isog_detect_mode fail\n");
		goto out;
	}

	/* load init table */
	ret = drv_hwreg_srccap_adc_init();
	if (ret) {
		SRCCAP_MSG_ERROR("load adc init table fail\n");
		goto out;
	}

	/* adc free run for pll clk enable */
	ret = mtk_adc_set_freerun(srccap_dev, true);
	if (ret) {
		SRCCAP_MSG_ERROR("load adc freerun_en table fail\n");
		goto out;
	}

	/* set fixed gain/offset/phase */
	ret = adc_set_fixed_gain(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("set adc fixed gain fail\n");
		goto out;
	}

	ret = adc_set_fixed_offset(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("set adc fixed offset fail\n");
		goto out;
	}

	ret = adc_set_fixed_phase();
	if (ret) {
		SRCCAP_MSG_ERROR("set adc fixed phase fail\n");
		goto out;
	}

	/* set default adc mux for ypbpr/cvbs */
	if (srccap_dev->srccap_info.cap.ypbpr_cnt != 0) {
		reg_port = adc_map_port_reg(
			srccap_dev->srccap_info.map[V4L2_SRCCAP_INPUT_SOURCE_YPBPR].data_port);
		if (reg_port != REG_SRCCAP_ADC_INPUT_PORT_NONE) {
			ret = drv_hwreg_srccap_adc_set_mux(reg_port);
			if (ret) {
				SRCCAP_MSG_ERROR("load mux table fail\n");
				goto out;
			}
		}
	}

	ret = drv_hwreg_srccap_adc_set_source(REG_SRCCAP_ADC_INPUTSOURCE_NONE);
	if (ret) {
		SRCCAP_MSG_ERROR("load source table fail\n");
		goto out;
	}

out:
	return ret;
}

int mtk_adc_deinit(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	ret = adc_disable_clk_swen(srccap_dev);

	if (ret) {
		SRCCAP_MSG_ERROR("adc set default off table fail\n");
		return ret;
	}

	ret = adc_disable_clk(srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("adc set disable unprepare clk fail\n");
		return ret;
	}

	return ret;
}

int mtk_adc_set_port(struct mtk_srccap_dev *srccap_dev)
{
	int i = 0;
	u32 hdmi_cnt = 0;
	u32 cvbs_cnt = 0;
	u32 svideo_cnt = 0;
	u32 ypbpr_cnt = 0;
	u32 vga_cnt = 0;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* Only dev0 ctrl ADC HW */
	if (srccap_dev->dev_id != 0)
		return 0;

	hdmi_cnt = srccap_dev->srccap_info.cap.hdmi_cnt;
	cvbs_cnt = srccap_dev->srccap_info.cap.cvbs_cnt;
	svideo_cnt = srccap_dev->srccap_info.cap.svideo_cnt;
	ypbpr_cnt = srccap_dev->srccap_info.cap.ypbpr_cnt;
	vga_cnt = srccap_dev->srccap_info.cap.vga_cnt;

	if (srccap_dev->srccap_info.cap.hdmi_cnt > 0)
		for (i = V4L2_SRCCAP_INPUT_SOURCE_HDMI;
			i < (V4L2_SRCCAP_INPUT_SOURCE_HDMI + hdmi_cnt);
			i++) {
			/* to be implemented */
			pr_info("src:%d data_port:%d\n",
				i, srccap_dev->srccap_info.map[i].data_port);
			pr_info("src:%d sync_port:%d\n",
				i, srccap_dev->srccap_info.map[i].sync_port);
		}
	if (srccap_dev->srccap_info.cap.cvbs_cnt > 0)
		for (i = V4L2_SRCCAP_INPUT_SOURCE_CVBS;
			i < (V4L2_SRCCAP_INPUT_SOURCE_CVBS + cvbs_cnt);
			i++) {
			/* to be implemented */
			pr_info("src:%d data_port:%d\n",
				i, srccap_dev->srccap_info.map[i].data_port);
			pr_info("src:%d sync_port:%d\n",
				i, srccap_dev->srccap_info.map[i].sync_port);
		}
	if (srccap_dev->srccap_info.cap.svideo_cnt > 0)
		for (i = V4L2_SRCCAP_INPUT_SOURCE_SVIDEO;
			i < (V4L2_SRCCAP_INPUT_SOURCE_SVIDEO + svideo_cnt);
			i++) {
			/* to be implemented */
			pr_info("src:%d data_port:%d\n",
				i, srccap_dev->srccap_info.map[i].data_port);
			pr_info("src:%d sync_port:%d\n",
				i, srccap_dev->srccap_info.map[i].sync_port);
		}
	if (srccap_dev->srccap_info.cap.ypbpr_cnt > 0)
		for (i = V4L2_SRCCAP_INPUT_SOURCE_YPBPR;
			i < (V4L2_SRCCAP_INPUT_SOURCE_YPBPR + ypbpr_cnt);
			i++) {
			/* to be implemented */
			pr_info("src:%d data_port:%d\n",
				i, srccap_dev->srccap_info.map[i].data_port);
			pr_info("src:%d sync_port:%d\n",
				i, srccap_dev->srccap_info.map[i].sync_port);
		}
	if (srccap_dev->srccap_info.cap.vga_cnt > 0)
		for (i = V4L2_SRCCAP_INPUT_SOURCE_VGA;
			i < (V4L2_SRCCAP_INPUT_SOURCE_VGA + vga_cnt);
			i++) {
			/* to be implemented */
			pr_info("src:%d data_port:%d\n",
				i, srccap_dev->srccap_info.map[i].data_port);
			pr_info("src:%d sync_port:%d\n",
				i, srccap_dev->srccap_info.map[i].sync_port);
		}

	return ret;
}

int mtk_srccap_register_adc_subdev(
	struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *subdev_adc,
	struct v4l2_ctrl_handler *adc_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(adc_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(adc_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(adc_ctrl_handler, &adc_ctrl[ctrl_count],
									NULL);
	}

	ret = adc_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create adc ctrl handler\n");
		goto exit;
	}
	subdev_adc->ctrl_handler = adc_ctrl_handler;

	v4l2_subdev_init(subdev_adc, &adc_sd_ops);
	subdev_adc->internal_ops = &adc_sd_internal_ops;
	strlcpy(subdev_adc->name, "mtk-adc", sizeof(subdev_adc->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_adc);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register adc subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(adc_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_adc_subdev(struct v4l2_subdev *subdev_adc)
{
	v4l2_ctrl_handler_free(subdev_adc->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_adc);
}

int mtk_adc_register_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_adc_callback_stage stage,
	SRCCAP_ADC_INTCB intcb,
	void *param)
{
	int num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_ADC_CB_STAGE_MAX) ||
		(stage < SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_GAIN_FUNCTION)) {
		SRCCAP_MSG_ERROR("failed to register callback: %d, (max %d)\n", stage,
		SRCCAP_ADC_CB_STAGE_MAX);
		return -EINVAL;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_adc_cb_info);
	for (num = 0; num < SRCCAP_ADC_CB_STAGE_MAX; num++) {
		if (srccap_dev->adc_info.cb_info[stage][num].cb == NULL) {
			SRCCAP_MSG_DEBUG("register callback stage:%d, num:%d\n", stage, num);
			srccap_dev->adc_info.cb_info[stage][num].param = param;
			srccap_dev->adc_info.cb_info[stage][num].cb = intcb;
			break;
		}
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_adc_cb_info);

	return 0;
}

int mtk_adc_unregister_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_adc_callback_stage stage,
	SRCCAP_ADC_INTCB intcb,
	void *param)
{
	int num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_ADC_CB_STAGE_MAX) ||
		(stage < SRCCAP_ADC_CB_STAGE_ENABLE_AUTO_GAIN_FUNCTION)) {
		SRCCAP_MSG_ERROR("failed to unregister callback: %d, (max %d)\n", stage,
		SRCCAP_ADC_CB_STAGE_MAX);
		return -EINVAL;
	}
	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_adc_cb_info);
	for (num = 0; num < SRCCAP_ADC_CB_STAGE_MAX; num++) {
		if (srccap_dev->adc_info.cb_info[stage][num].cb == intcb) {
			SRCCAP_MSG_DEBUG("unregister callback  stage:%d, num:%d\n", stage, num);
			srccap_dev->adc_info.cb_info[stage][num].param = NULL;
			srccap_dev->adc_info.cb_info[stage][num].cb = NULL;
			break;
		}
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_adc_cb_info);

	return 0;
}

int mtk_adc_set_auto_gain_offset_by_string(
	struct device *dev,
	const char *buf)
{
	int ret = 0;
	char *cmd = NULL;
	bool find = false;
	int len = 0, value = 0, forceAutoGainOffset = 0;
	enum v4l2_adc_auto_tune_type auto_tune_type = 0;
	enum v4l2_adc_auto_calibration_mode auto_calibration_mode = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct v4l2_adc_gain gain_tmp, hwreg_gain;
	u8 flow_flag = 0;

	if ((buf == NULL) || (dev == NULL)) {
		SRCCAP_MSG_ERROR("Pointer is NULL!\n");
		return -EINVAL;
	}

	memset(&gain_tmp, 0, sizeof(struct v4l2_adc_gain));
	memset(&hwreg_gain, 0, sizeof(struct v4l2_adc_gain));

	srccap_dev = dev_get_drvdata(dev);
	if (!srccap_dev) {
		SRCCAP_MSG_ERROR("srccap_dev is NULL!\n");
		return -EINVAL;
	}

	cmd = vmalloc(sizeof(char) * 0x1000);
	if (cmd == NULL) {
		SRCCAP_MSG_ERROR("vmalloc cmd fail!\n");
		return -EINVAL;
	}

	memset(cmd, 0, sizeof(char) * 0x1000);


	len = strlen(buf);

	ret = snprintf(cmd, len + 1, "%s", buf);
	if ((ret < 0) || (cmd == NULL)) {
		ret = -EINVAL;
		SRCCAP_MSG_ERROR("snprintf error\n");
		goto exit;
	}

	find = adc_get_value_from_string(cmd, "calibrationMode", len, &value);
	if (!find) {
		SRCCAP_MSG_ERROR(
			"Cmdline format error, enable should be set, please echo help!\n");
		goto exit;
	}
	auto_calibration_mode = value;

	find = adc_get_value_from_string(cmd, "autoTuneType", len, &value);
	if (!find) {
		SRCCAP_MSG_ERROR(
			"Cmdline format error, enable should be set, please echo help!\n");
		goto exit;
	}
	auto_tune_type = value;

	find = adc_get_value_from_string(cmd, "forceAutoGainOffset", len, &value);
	if (!find) {
		SRCCAP_MSG_ERROR(
			"Cmdline format error, enable should be set, please echo help!\n");
		goto exit;
	}
	forceAutoGainOffset = value;

	SRCCAP_MSG_DEBUG("[auto gain]calibrationMode:%d\n", auto_calibration_mode);
	ret = mtk_adc_set_auto_calibration_mode(auto_calibration_mode, srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_ERROR(
			"mtk_adc_set_auto_calibration_mode fail\n");
		goto exit;
	}

	SRCCAP_MSG_DEBUG("[auto gain]autoTuneType:%d\n", auto_tune_type);

	adc_get_gain(&hwreg_gain, srccap_dev);
	ret = adc_perform_auto_calibration(srccap_dev, auto_tune_type, &flow_flag, &gain_tmp);
	if (ret < 0) {
		SRCCAP_MSG_ERROR(
			"adc_perform_auto_calibrationadc_perform_auto_calibration fail\n");
		goto exit;
	}

	SRCCAP_MSG_DEBUG("[auto gain]forceAutoGainOffset:%d, gain:(%x,%x,%x)\n",
		forceAutoGainOffset, gain_tmp.r_gain, gain_tmp.g_gain, gain_tmp.b_gain);
	if (forceAutoGainOffset)
		adc_set_gain(&gain_tmp, srccap_dev);
	else
		adc_check_auto_gain_offset_result(srccap_dev, flow_flag,
						&hwreg_gain, &gain_tmp, auto_tune_type);

exit:
	vfree(cmd);
	return ret;
}

