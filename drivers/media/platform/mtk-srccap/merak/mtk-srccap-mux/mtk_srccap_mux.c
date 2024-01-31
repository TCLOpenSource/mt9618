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

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <asm-generic/div64.h>

#include "mtk_srccap.h"
#include "mtk_srccap_mux.h"
#include "hwreg_srccap_mux.h"
#include "mtk_srccap_common_ca.h"
#include "utpa2_XC.h"

#define SRCCAP_NOT_USED_FUN 0
/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static int srccap_mux_map_hdmi_mux_src(
	enum srccap_input_port port,
	enum reg_srccap_mux_source *mux_src)
{
	if (mux_src == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (port) {
	case SRCCAP_INPUT_PORT_DVI0:
	case SRCCAP_INPUT_PORT_HDMI0:
		*mux_src = REG_SRCCAP_MUX_SOURCE_HDMI;
		break;
	case SRCCAP_INPUT_PORT_DVI1:
	case SRCCAP_INPUT_PORT_HDMI1:
		*mux_src = REG_SRCCAP_MUX_SOURCE_HDMI2;
		break;
	case SRCCAP_INPUT_PORT_DVI2:
	case SRCCAP_INPUT_PORT_HDMI2:
		*mux_src = REG_SRCCAP_MUX_SOURCE_HDMI3;
		break;
	case SRCCAP_INPUT_PORT_DVI3:
	case SRCCAP_INPUT_PORT_HDMI3:
		*mux_src = REG_SRCCAP_MUX_SOURCE_HDMI4;
		break;
	default:
		SRCCAP_MSG_ERROR("invalid hdmi port: (%d)\n", port);
		return -EINVAL;
	}

	return 0;
}

/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
#if SRCCAP_NOT_USED_FUN
static int _mux_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, mux_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int _mux_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, mux_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	default:
		ret = -1;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops mux_ctrl_ops = {
	.g_volatile_ctrl = _mux_g_ctrl,
	.s_ctrl = _mux_s_ctrl,
};
#endif

static const struct v4l2_ctrl_config mux_ctrl[] = {
};

/* subdev operations */
static const struct v4l2_subdev_ops mux_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops mux_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_srccap_mux_set_source(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum reg_srccap_mux_source mux_src;
#if IS_ENABLED(CONFIG_OPTEE)
	TEEC_Session * pstSession = NULL;
	TEEC_Operation op = {0};
	u32 error = 0;
#endif

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (srccap_dev->srccap_info.src) {
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		mux_src = REG_SRCCAP_MUX_SOURCE_ADCA;
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
		mux_src = REG_SRCCAP_MUX_SOURCE_VD;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		ret = srccap_mux_map_hdmi_mux_src(
			srccap_dev->srccap_info.map[srccap_dev->srccap_info.src].data_port,
			&mux_src);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		break;
	default:
		SRCCAP_MSG_ERROR("invalid source: (%d)\n", srccap_dev->srccap_info.src);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_OPTEE)
	pstSession = srccap_dev->memout_info.secure_info.pstSession;
	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].value.a = (u32)srccap_dev->dev_id;
	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].value.b = (u32)mux_src;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
		TEEC_NONE, TEEC_NONE, TEEC_NONE);
	ret = mtk_common_ca_teec_invoke_cmd(srccap_dev, pstSession,
		E_XC_OPTEE_SET_MUX_SOURCE, &op, &error);
#else
	ret = drv_hwreg_srccap_mux_set_source(srccap_dev->dev_id, mux_src, true, NULL, NULL);
#endif

	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

int mtk_srccap_mux_init(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = drv_hwreg_srccap_mux_set_trig_gen_vfde_en(TRUE, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return ret;
}

int mtk_srccap_register_mux_subdev(struct v4l2_device *v4l2_dev,
			struct v4l2_subdev *subdev_mux,
			struct v4l2_ctrl_handler *mux_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(mux_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(mux_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(mux_ctrl_handler, &mux_ctrl[ctrl_count],
									NULL);
	}

	ret = mux_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create mux ctrl handler\n");
		goto exit;
	}
	subdev_mux->ctrl_handler = mux_ctrl_handler;

	v4l2_subdev_init(subdev_mux, &mux_sd_ops);
	subdev_mux->internal_ops = &mux_sd_internal_ops;
	strlcpy(subdev_mux->name, "mtk-mux", sizeof(subdev_mux->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_mux);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register mux subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(mux_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_mux_subdev(struct v4l2_subdev *subdev_mux)
{
	v4l2_ctrl_handler_free(subdev_mux->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_mux);
}

