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
#include "mtk_srccap_dscl.h"
#include "hwreg_srccap_dscl.h"
#include "drv_scriptmgt.h"
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
#include "mtk_srccap_atrace.h"
#endif

/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
static int mtk_dscl_set_scaling_params(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_dscl_info *user_dscl_info)
{
	int ret = 0, index = 0;
	u16 arraysize = 0;
	u16 *node_array = NULL;
	u32 width = 0, height = 0;
	u32 src_width = 0, src_height = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (user_dscl_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	arraysize = user_dscl_info->rm_info.array_size;
	if (arraysize == 0) {
		SRCCAP_MSG_ERROR("array size is 0!\n");
		return -EINVAL;
	}

	node_array = kcalloc(arraysize, sizeof(u16), GFP_KERNEL);
	if (!node_array) {
		SRCCAP_MSG_ERROR("kcalloc failed!\n");
		return -ENOMEM;
	}

	if (copy_from_user((void *)node_array,
		(u16 __user *)user_dscl_info->rm_info.p.node_array, sizeof(u16)*arraysize)) {
		SRCCAP_MSG_ERROR("copy_from_user fail\n");
		kfree(node_array);
		return -EFAULT;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);

	srccap_dev->common_info.rm_info.array_size = arraysize;

	kfree(srccap_dev->common_info.rm_info.p.node_array);
	srccap_dev->common_info.rm_info.p.node_array = NULL;

	srccap_dev->common_info.rm_info.p.node_array = kcalloc(arraysize, sizeof(u16), GFP_KERNEL);
	if (!srccap_dev->common_info.rm_info.p.node_array) {
		SRCCAP_MSG_ERROR("srccap node array kcalloc failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	memcpy(srccap_dev->common_info.rm_info.p.node_array, node_array, (sizeof(u16) * arraysize));

	width = user_dscl_info->dscl_size.output.width;
	height = user_dscl_info->dscl_size.output.height;

	src_width = (srccap_dev->timingdetect_info.data.yuv420 == true) ?
		srccap_dev->timingdetect_info.cap_win.width * SRCCAP_YUV420_WIDTH_DIVISOR :
		srccap_dev->timingdetect_info.cap_win.width;
	src_height = srccap_dev->timingdetect_info.cap_win.height;

	srccap_dev->dscl_info.user_dscl_size.output.width = width;

	srccap_dev->dscl_info.user_dscl_size.output.height = height;

	SRCCAP_MSG_INFO("%s:(%u, %u), %s:(%u, %u), %s:(%u, %u), %s:(%u)\n",
		"inputsize", src_width, src_height,
		"outputsize", srccap_dev->dscl_info.user_dscl_size.output.width,
		srccap_dev->dscl_info.user_dscl_size.output.height,
		"user_outputsize", width, height,
		"array_size", srccap_dev->common_info.rm_info.array_size);

	for (index = 0; index < arraysize; index++)
		SRCCAP_MSG_INFO("node_array[%d]:%u\n",
			index, srccap_dev->common_info.rm_info.p.node_array[index]);

exit:
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);
	kfree(node_array);
	return ret;
}

static int _dscl_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, dscl_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	default:
	{
		ret = -1;
		break;
	}
	}

	return ret;
}

static int _dscl_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, dscl_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_DSCL_SETTING:
	{
		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		SRCCAP_MSG_DEBUG("Set V4L2_CID_DSCL_SETTING\n");

		ret = mtk_dscl_set_scaling_params(srccap_dev,
			(struct v4l2_dscl_info *)(ctrl->p_new.p));
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

static const struct v4l2_ctrl_ops dscl_ctrl_ops = {
	.g_volatile_ctrl = _dscl_g_ctrl,
	.s_ctrl = _dscl_s_ctrl,
};

static const struct v4l2_ctrl_config dscl_ctrl[] = {
	{
		.ops = &dscl_ctrl_ops,
		.id = V4L2_CID_DSCL_SETTING,
		.name = "Srccap DSCL Setting",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_dscl_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops dscl_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops dscl_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_srccap_dscl_set_source(
	struct mtk_srccap_dev *srccap_dev,
	bool frl)
{
	int ret = 0;
	enum reg_srccap_dscl_mode mode = REG_SRCCAP_DSCL_SOURCE_8P_MODE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (!frl)
		mode = REG_SRCCAP_DSCL_SOURCE_1P_MODE;

	SRCCAP_MSG_INFO("frl mode:%s, mode:%s\n", frl?"Yes":"No", mode?"1p":"8p");

	ret = drv_hwreg_srccap_dscl_set_source(srccap_dev->dev_id, mode, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	return 0;
}

int mtk_srccap_register_dscl_subdev(
	struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *subdev_dscl,
	struct v4l2_ctrl_handler *dscl_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(dscl_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(dscl_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(dscl_ctrl_handler,
			&dscl_ctrl[ctrl_count], NULL);
	}

	ret = dscl_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create dscl ctrl handler\n");
		goto exit;
	}
	subdev_dscl->ctrl_handler = dscl_ctrl_handler;

	v4l2_subdev_init(subdev_dscl, &dscl_sd_ops);
	subdev_dscl->internal_ops = &dscl_sd_internal_ops;
	strlcpy(subdev_dscl->name, "mtk-dscl", sizeof(subdev_dscl->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_dscl);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register dscl subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(dscl_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_dscl_subdev(struct v4l2_subdev *subdev_dscl)
{
	v4l2_ctrl_handler_free(subdev_dscl->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_dscl);
}

u32 mtk_dscl_set_ratio_h(u16 src_size, u16 dst_size, bool filter_mode)
{
	u64 ratio = 0;

	if (dst_size <= 1 || src_size <= 1)
		return 0;

	if (filter_mode) {
		ratio = (u64)((src_size) - 1) * 2097152ul;
		do_div(ratio, ((dst_size) - 1));
		ratio = ratio + 1;
		do_div(ratio, SRCCAP_DSCL_AVERAGE);
	} else {
		ratio = ((u64)(dst_size)) * 2097152ul;
		do_div(ratio, src_size);
		ratio = ratio + 1;
		do_div(ratio, SRCCAP_DSCL_AVERAGE);
	}
	SRCCAP_MSG_DEBUG("[SRCCAP]srcSize:%u dstSize:%u ratio:0x%llx filter_mode:%d\n",
		src_size, dst_size, ratio, filter_mode);
	return (u32)ratio;
}

u32 mtk_dscl_set_ratio_v(u16 src_size, u16 dst_size, bool linear_mode)
{
	u64 ratio = 0;

	if (dst_size <= 1 || src_size <= 1)
		return 0;

	if (linear_mode) {
		ratio = ((src_size) - 1) * 1048576ul;
		do_div(ratio, ((dst_size) - 1));
	} else {
		ratio = (dst_size) * 1048576ul;
		do_div(ratio, ((src_size) + 1));
	}

	SRCCAP_MSG_DEBUG("[SRCCAP]srcSize:%u, dstSize:%u, ratio:0x%llx linear_mode:%d\n",
		src_size, dst_size, ratio, linear_mode);
	return (u32)ratio;
}

void mtk_dscl_set_scaling(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_ml_script_info *ml_script_info,
	struct v4l2_dscl_size *dscl_size)
{
	int ret = 0;
	bool h_scaling = false, v_scaling = false;
	bool h_filter_mode = 0;
	bool v_linear_mode = 0;
	u16 count = 0;
	u32 h_ratio = 0, v_ratio = 0;
	struct reg_srccap_dscl_scaling_info dscl_scaling_info = {0};
	struct reg_info reg[SRCCAP_DSCL_REG_NUM] = {{0}};
	bool stub = 0;
	struct sm_ml_direct_write_mem_info ml_direct_write_info = {0};

	if ((srccap_dev == NULL) || (ml_script_info == NULL)
		|| (dscl_size == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}
	SRCCAP_ATRACE_BEGIN("mtk_dscl_set_scaling");

	SRCCAP_MSG_DEBUG("dscl(%u): input(%u,%u), output(%u,%u)\n",
		srccap_dev->dev_id,
		dscl_size->input.width, dscl_size->input.height,
		dscl_size->output.width, dscl_size->output.height);

	h_filter_mode = srccap_dev->dscl_info.h_scalingmode;
	v_linear_mode = srccap_dev->dscl_info.v_scalingmode;

	/* H scaling */
	if (dscl_size->input.width > dscl_size->output.width)
		h_scaling = true;

	if (h_scaling == true) {
		h_ratio = mtk_dscl_set_ratio_h(dscl_size->input.width,
						dscl_size->output.width, h_filter_mode);

		srccap_dev->dscl_info.h_scalingratio = h_ratio;
	}
	/* H scaling end */

	/* V scaling */
	if (dscl_size->input.height > dscl_size->output.height)
		v_scaling = true;

	if (v_scaling == true) {
		v_ratio = mtk_dscl_set_ratio_v(dscl_size->input.height,
						dscl_size->output.height, v_linear_mode);

		srccap_dev->dscl_info.v_scalingratio = v_ratio;
	}

	dscl_scaling_info.h_scaling = h_scaling;
	dscl_scaling_info.v_scaling = v_scaling;
	dscl_scaling_info.h_scaling_ratio = h_ratio;
	dscl_scaling_info.v_scaling_ratio = v_ratio;
	dscl_scaling_info.h_filter_mode = h_filter_mode;
	dscl_scaling_info.v_linear_mode = v_linear_mode;
	dscl_scaling_info.vsd_ipmfetch = dscl_size->output.width;
	dscl_scaling_info.input_size_v = dscl_size->input.height;
	dscl_scaling_info.output_size_v = dscl_size->output.height;

	/* V scaling end */

	SRCCAP_MSG_DEBUG("h: en:%d, ratio:%llx, mode:%d\n",
		dscl_scaling_info.h_scaling,
		dscl_scaling_info.h_scaling_ratio,
		dscl_scaling_info.h_filter_mode);
	SRCCAP_MSG_DEBUG("v: %s:%d, %s:%llx, %s:%d, %s:%u, %s:%u, %s:%u\n",
		"en", dscl_scaling_info.v_scaling,
		"ratio", dscl_scaling_info.v_scaling_ratio,
		"mode", dscl_scaling_info.v_linear_mode,
		"ipm fetch", dscl_scaling_info.vsd_ipmfetch,
		"input_size_v", dscl_scaling_info.input_size_v,
		"output_size_v", dscl_scaling_info.output_size_v);

	ret = drv_hwreg_srccap_dscl_set_scaling(srccap_dev->dev_id,
					reg, &count, false, &dscl_scaling_info);

	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	ret = drv_hwreg_common_get_stub(&stub);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	ml_script_info->dscl_cmd_count = count;
	if (stub == 0) {
		//3.add cmd
		ml_direct_write_info.reg = (struct sm_reg *)&reg;
		ml_direct_write_info.cmd_cnt = count;
		ml_direct_write_info.va_address = ml_script_info->start_addr +
				(ml_script_info->total_cmd_count * SRCCAP_ML_CMD_SIZE);
		ml_direct_write_info.va_max_address = ml_script_info->max_addr;
		ml_direct_write_info.add_32_support = FALSE;

		SRCCAP_MSG_DEBUG("ml_write_info: cmd_count: total:%d, dscl:%d\n",
			ml_script_info->total_cmd_count,
			count);
		SRCCAP_MSG_DEBUG("ml_write_info: va_address:0x%llx,  va_max_address:0x%llx\n",
			ml_direct_write_info.va_address,
			ml_direct_write_info.va_max_address);

		ret = sm_ml_write_cmd(&ml_direct_write_info);
		if (ret != E_SM_RET_OK)
			SRCCAP_MSG_ERROR("sm_ml_write_cmd fail, ret:%d\n", ret);
	}
	ml_script_info->total_cmd_count = ml_script_info->total_cmd_count
					+ ml_script_info->dscl_cmd_count;
	SRCCAP_ATRACE_END("mtk_dscl_set_scaling");

}

int mtk_dscl_streamoff(
	struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&srccap_dev->dscl_info, 0, sizeof(struct srccap_dscl_info));

	return 0;
}

int mtk_dscl_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size)
{
	int total_string_size = 0;
	u16 dev_id = 0;

	if ((srccap_dev == NULL) || (buf == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id >= SRCCAP_DEV_NUM) {
		SRCCAP_MSG_ERROR("Wrong srccap_dev->dev_id = %d\n", srccap_dev->dev_id);
		return -ENXIO;
	}

	dev_id = srccap_dev->dev_id;

	total_string_size = snprintf(buf, max_size,
	"DSCL Info:\n"
	"Current Source(dev:%d):	%d\n"
	"input dscl_size:	WxH = (%d x %d)\n"
	"output dscl_size:	WxH = (%d x %d)\n"
	"user_outputsize:	WxH = (%d x %d)\n"
	"scalingmode:	V:H = (%d x %d)\n"
	"scalingration:	V:H = (%lld x %lld)\n"
	"\n",
	dev_id, srccap_dev->srccap_info.src,
	srccap_dev->dscl_info.dscl_size.input.width,
	srccap_dev->dscl_info.dscl_size.input.height,
	srccap_dev->dscl_info.dscl_size.output.width,
	srccap_dev->dscl_info.dscl_size.output.height,
	srccap_dev->dscl_info.user_dscl_size.output.width,
	srccap_dev->dscl_info.user_dscl_size.output.height,
	srccap_dev->dscl_info.v_scalingmode, srccap_dev->dscl_info.h_scalingmode,
	srccap_dev->dscl_info.v_scalingratio, srccap_dev->dscl_info.h_scalingratio);

	if (total_string_size < 0 || total_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	return total_string_size;

}


