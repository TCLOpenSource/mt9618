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
#include <asm-generic/div64.h>

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>

#include <linux/videodev2.h>
#include <uapi/linux/mtk-v4l2-pq.h>
#include "mtk_pq.h"
#include "mtk_pq_common.h"
#include "mtk_pq_buffer.h"

#include "pqu_msg.h"
#include "hwreg_common_riu.h"
#include "hwreg_pq_display_b2r.h"
#include "ext_command_video_if.h"

/* local definition */
#define B2R_MIN_FPS     1000
#define B2R_H_BLK	300
#define B2R_V_BLK	45
#define B2R_MAX_CLK (720000000ull)
#define B2R_FPS_SFT     1000
#define VIDEO_SIZE_FHD_H (1088)
#define HTT_REG_LIMIT (0xfffe)

#define VBLK0_DEF_STR	0x1

#define ALIGN_DOWNTO_2(x)  ((x >> 1) << 1)
#define ALIGN_DOWNTO_4(x)  (((x) >> 2) << 2)
#define ALIGN_UPTO_32(x)  ((((x) + 31) >> 5) << 5)
#define B2R_PLANE_NUM 2
#define B2R_PLANE_SIZE 4
#define B2R_BUFFER_NUM 32
#define B2R_SW_QUEUE_FRAME_NUM	(2)
#define B2R_HW_QUEUE_FRAME_NUM	(3)
#define B2R_BUFFER_CNT		(1)
#define B2R_FRAME_RATE		(100000)
#define B2R_TOTAL_DELAY     (1000000 * 10)
#define B2R_TOTAL_SHITF     (5)
#define B2R_CALC            (10)

/* resource name assign */
static const struct resources_name b2r_res_name = {
	.regulator  = { NULL },
	.clock      = { NULL },
	.reg        = { NULL },
	.interrupt  = { "b2r" }
};

#define _READY_CB_COUNT 8
#define _CB_INC(x) ((x++)&(_READY_CB_COUNT-1))
#define _CB_DEC(x) (x == 0 ? 0 : --x)

static __u64 B2R_CLK = (B2R_MAX_CLK * B2R_FPS_SFT);
static struct msg_b2r_timing_info	_timing_msg_info[_READY_CB_COUNT];
static struct msg_b2r_global_timing_info _global_timing_msg_info[_READY_CB_COUNT];
static int _timing_count;
static int _global_timing_count;

static __u32 u32B2RPixelNum;
static __u32 u32B2Rersion;

struct b2r_ctrl pre_reg_b2r_ctrl;

int _mtk_pq_b2r_chk_ipt_timing(struct v4l2_timing *input_timing_info)
{
	if ((input_timing_info->v_freq < B2R_MIN_FPS)
		|| (input_timing_info->de_width == 0)
		|| (input_timing_info->de_height == 0)) {
		return -EINVAL;
	}

	return 0;
}

int _mtk_pq_b2r_calc_clk(struct v4l2_timing *input_timing_info, __u64 *clock_rate)
{
	__u64 clock_rate_est;
	__u64 htt_est;
	__u64 vtt_est;
	__u64 dividend;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "pixel_per_tik = %x\r\n", u32B2RPixelNum);
	if ((u32B2Rersion == B2R_VER2) || (u32B2Rersion == B2R_VER4)
		|| (u32B2Rersion > B2R_VER6)) {
		/* calculate clock by 1 p */
		htt_est = ALIGN_UPTO_32(input_timing_info->de_width) + B2R_H_BLK;
	} else
		htt_est = (ALIGN_UPTO_32(input_timing_info->de_width) >> u32B2RPixelNum)
				  + B2R_H_BLK;
	vtt_est = ALIGN_UPTO_32(input_timing_info->de_height) + B2R_V_BLK;
	dividend = htt_est * vtt_est * input_timing_info->v_freq;
	do_div(dividend, B2R_FPS_SFT);
	clock_rate_est = dividend;

	if (clock_rate_est > B2R_CLK_720MHZ) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: input timing out of spec\n", __func__);
		return -EINVAL;
	}

	/*set maximum clock rate for TS*/
	if ((input_timing_info->disp_type == V4L2_E_PQ_B2R_DISP_TYPE_TS)
		|| (input_timing_info->disp_type == V4L2_E_PQ_B2R_DISP_TYPE_LGY))
		clock_rate_est = B2R_CLK_720MHZ;

	if (clock_rate_est < B2R_CLK_156MHZ)
		*clock_rate = B2R_CLK_156MHZ;
	else if (clock_rate_est < B2R_CLK_360MHZ)
		*clock_rate = B2R_CLK_360MHZ;
	else if (clock_rate_est < B2R_CLK_630MHZ)
		*clock_rate = B2R_CLK_630MHZ;
	else
		*clock_rate = B2R_CLK_720MHZ;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "%s: clock = %lld\n",
			   __func__, *clock_rate);

	return 0;
}

int _mtk_pq_b2r_set_clk(struct device *dev, __u64 clock_rate)
{
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);
	struct clk *b2r_clk = devm_clk_get(pqdev->dev,
									   "clk_b2r_ckg_dc0");
	struct clk *b2r_lite1_clk = devm_clk_get(pqdev->dev,
								"clk_b2r_ckg_sub1");
	struct clk_hw *target_parent, *target_parent_lite1;
	struct clk *parent, *parent_lite1;
	unsigned int idx = 0;
	int ret = 0;
	int ret_lite1 = 0;

	if (IS_ERR_OR_NULL(b2r_clk) || IS_ERR_OR_NULL(b2r_lite1_clk)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				   "[%s][%d] b2r get ckg failed\n", __func__, __LINE__);

		return -EINVAL;
	}

	switch (clock_rate) {
	case B2R_CLK_156MHZ:
		idx = B2R_CLK_156M_IDX;
		break;
	case B2R_CLK_360MHZ:
		idx = B2R_CLK_360M_IDX;
		break;
	case B2R_CLK_630MHZ:
		if ((u32B2Rersion == B2R_VER0) || (u32B2Rersion == B2R_VER1)
			|| (u32B2Rersion == B2R_VER3) || (u32B2Rersion == B2R_VER5))
			idx = B2R_CLK_630M_IDX;
		else
			idx = B2R_CLK_720M_IDX;
		break;
	case B2R_CLK_720MHZ:
		idx = B2R_CLK_720M_IDX;
		break;
	default:
		idx = B2R_CLK_720M_IDX;
		break;
	}

	target_parent = clk_hw_get_parent_by_index(__clk_get_hw(b2r_clk), idx);
	target_parent_lite1 = clk_hw_get_parent_by_index(__clk_get_hw(b2r_lite1_clk), idx);

	ret = clk_set_parent(b2r_clk, target_parent->clk);
	ret_lite1 = clk_set_parent(b2r_lite1_clk, target_parent_lite1->clk);
	if (ret || ret_lite1) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to set dc0 clock\n");
		return -EINVAL;
	}
	parent = clk_get_parent(b2r_clk);
	parent_lite1 = clk_get_parent(b2r_lite1_clk);

	if (__clk_get_hw(parent) != target_parent)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				   "[%s][%d] parent = %s, target_parent = %s, clock_rate = %lld\n",
				   __func__, __LINE__, __clk_get_name(parent),
				   clk_hw_get_name(target_parent), clock_rate);

	ret = clk_prepare_enable(b2r_clk);
	ret_lite1 = clk_prepare_enable(b2r_lite1_clk);
	if (ret || ret_lite1) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to set dc0 clock\n");
		return -EINVAL;
	}
	devm_clk_put(dev, b2r_clk);
	devm_clk_put(dev, b2r_lite1_clk);

	return 0;
}

int _mtk_pq_b2r_get_power_of(int n)
{

	int ret = 0;

	if (n < 1 || n == 1)
		return 0;

	while (n > 1) {
		if (n & 1 == 1)
			return 0;
		n >>= 1;
		ret++;
	}

	return ret;
}

int _mtk_pq_b2r_gen_timing(struct v4l2_timing *input_timing_info,
						   struct v4l2_b2r_info *b2r_info, __u64 clock_rate)
{
	__u64 htt_est = 0, htt_align = 0;
	__u64 vtt_est;
	__u16 VBlank0;
	__u64 dividend;
	bool need_regen = false;
	uint64_t clk_tmp = 0;
	__u16 pixelsht = _mtk_pq_b2r_get_power_of(u32B2RPixelNum);

	vtt_est = input_timing_info->de_height + B2R_V_BLK;
	if (u32B2Rersion > B2R_VER0)
		dividend = (clock_rate << pixelsht) * B2R_FPS_SFT;
	else
		dividend = clock_rate * B2R_FPS_SFT;
	do_div(dividend, input_timing_info->v_freq);
	do_div(dividend, vtt_est);
	htt_est = dividend;
	need_regen = (u32B2Rersion > B2R_VER0)
		? (htt_est < input_timing_info->de_width)
		: (htt_est < (input_timing_info->de_width >> pixelsht));

	if ((need_regen) || (htt_est > HTT_REG_LIMIT)) {
		input_timing_info->v_total = VIDEO_SIZE_FHD_H + B2R_V_BLK;
		dividend = clock_rate * B2R_FPS_SFT;
		do_div(dividend, input_timing_info->v_freq);
		do_div(dividend, input_timing_info->v_total);
		input_timing_info->h_total = dividend;
	} else {
		input_timing_info->h_total = htt_est;
		input_timing_info->v_total = vtt_est;
	}

	clk_tmp = (uint64_t)input_timing_info->h_total
			* input_timing_info->v_total
			* input_timing_info->v_freq;

	dividend = ((clock_rate * B2R_FPS_SFT) << pixelsht) - clk_tmp;
	do_div(dividend, input_timing_info->v_freq);
	b2r_info->timing_out.Hcnt_FineTune = dividend;

	/* Htt can not be non multiples of pixel number */
	vtt_est = (uint64_t)input_timing_info->v_total;
	if (pixelsht == B2R_2P_SHT) {
		htt_align = (input_timing_info->h_total % u32B2RPixelNum)
			? input_timing_info->v_total : 0;
		input_timing_info->h_total = ALIGN_DOWNTO_2(input_timing_info->h_total);
	} else if (pixelsht == B2R_4P_SHT) {
		htt_align = (input_timing_info->h_total % u32B2RPixelNum) ?
			(uint64_t)(vtt_est * (input_timing_info->h_total % u32B2RPixelNum)) : 0;
		input_timing_info->h_total = ALIGN_DOWNTO_4(input_timing_info->h_total);
	} else {
		htt_align = 0;
	}
	b2r_info->timing_out.Hcnt_FineTune = b2r_info->timing_out.Hcnt_FineTune + htt_align;
	/* unit: 100Hz */
	input_timing_info->h_freq = input_timing_info->v_total *
		input_timing_info->v_freq / B2R_FRAME_RATE;

	memcpy(&b2r_info->timing_in, input_timing_info,
		   sizeof(struct v4l2_timing));
	b2r_info->current_clk = clock_rate;
	b2r_info->timing_out.V_TotalCount = input_timing_info->v_total;
	b2r_info->timing_out.H_TotalCount = input_timing_info->h_total;
	VBlank0 = b2r_info->timing_out.V_TotalCount
			  - input_timing_info->de_height;
	b2r_info->timing_out.VBlank0_Start = VBLK0_DEF_STR;
	b2r_info->timing_out.VBlank0_End = VBlank0 - b2r_info->timing_out.VBlank0_Start;
	b2r_info->timing_out.TopField_VS = b2r_info->timing_out.VBlank0_Start
									   + (VBlank0 >> 1);
	b2r_info->timing_out.TopField_Start = b2r_info->timing_out.TopField_VS;
	b2r_info->timing_out.HActive_Start = b2r_info->timing_out.H_TotalCount
			- (ALIGN_UPTO_32(input_timing_info->de_width) >> u32B2RPixelNum);
	b2r_info->timing_out.HImg_Start = b2r_info->timing_out.HActive_Start;
	b2r_info->timing_out.VImg_Start0 = b2r_info->timing_out.VBlank0_End;

	if (input_timing_info->interlance == V4L2_FIELD_NONE) {
		b2r_info->timing_out.VBlank1_Start = b2r_info->timing_out.VBlank0_Start;
		b2r_info->timing_out.VBlank1_End = b2r_info->timing_out.VBlank0_End;
		b2r_info->timing_out.BottomField_VS = b2r_info->timing_out.TopField_VS;
		b2r_info->timing_out.BottomField_Start = b2r_info->timing_out.TopField_Start;
		b2r_info->timing_out.VImg_Start1 = b2r_info->timing_out.VImg_Start0;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: v_de = 0x%x, h_de = 0x%x, pixelsht = %d\n",
			   __func__,
			   b2r_info->timing_in.de_height,
			   b2r_info->timing_in.de_width,
			   pixelsht);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "interlance = 0x%x, v_freq = %d, h_freq = %d\n",
			   b2r_info->timing_in.interlance,
			   b2r_info->timing_in.v_freq,
			   b2r_info->timing_in.h_freq);

	return 0;
}

int _mtk_pq_b2r_config_reg_timing(struct v4l2_timing *input_timing_info,
	struct v4l2_b2r_info *b2r_info, __u64 clock_rate)
{
	__u64 htt_est;
	__u64 vtt_est;
	__u64 dividend;

	vtt_est = input_timing_info->de_height + B2R_V_BLK;
	dividend = clock_rate * B2R_FPS_SFT;
	do_div(dividend, input_timing_info->v_freq);
	do_div(dividend, vtt_est);
	htt_est = dividend;

	input_timing_info->h_total = htt_est;
	input_timing_info->v_total = vtt_est;
	/* unit: 100Hz */
	input_timing_info->h_freq = input_timing_info->v_total *
			input_timing_info->v_freq / B2R_FRAME_RATE;

	memcpy(&b2r_info->timing_in, input_timing_info,
		   sizeof(struct v4l2_timing));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: v_total = 0x%x, h_total = 0x%x\n",
			   __func__,
			   b2r_info->timing_in.v_total,
			   b2r_info->timing_in.h_total);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "interlance = 0x%x, v_freq = %d, h_freq = %d\n",
			   b2r_info->timing_in.interlance,
			   b2r_info->timing_in.v_freq,
			   b2r_info->timing_in.h_freq);

	return 0;
}

static void _ready_cb_pqu_video_set_b2r_timing_store(void)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_set_b2r_timing_store((void *)&_timing_msg_info[_CB_DEC(_timing_count)]);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_set_b2r_timing_store fail!\n");
}


int _mtk_pq_b2r_set_timing(struct device *dev,  __u8 win,
	struct v4l2_timing *input_timing_info,
	struct v4l2_b2r_info *b2r_info, struct b2r_input_timing *cur_timing)
{
	int ret = 0;
	__u64 clock_rate = 0;
	struct b2r_timing_info reg_timing;
	struct hwregOut regouts;
	struct msg_b2r_timing_info timing_info;
	struct b2r_reg_timing reg_timing_info = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "timing info: de_w=%d de_h=%d fps=%d win=%d\n",
				   input_timing_info->de_width,
				   input_timing_info->de_height,
				   input_timing_info->v_freq,
				   win);

	/* check input parameters */
	if (_mtk_pq_b2r_chk_ipt_timing(input_timing_info) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_check_input_timing failed!\n", __func__);
		return -EPERM;
	}
	/* calculate clock level */
	if (_mtk_pq_b2r_calc_clk(input_timing_info, &clock_rate) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_calc_clk failed!\n", __func__);
		return -EPERM;
	}
	/* set clock level */
	if (_mtk_pq_b2r_set_clk(dev, clock_rate) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_set_clk failed!\n", __func__);
		return -EPERM;
	}
	/* generate timing */
	if (_mtk_pq_b2r_gen_timing(input_timing_info, b2r_info, clock_rate) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_calc_clk failed!\n", __func__);
		return -EPERM;
	}

	/* assign to hwreg */
	memcpy(&reg_timing, &b2r_info->timing_out,
		   sizeof(struct b2r_timing_info));
	/* fps check hw value */
	mtk_b2r_get_hw_gtgen(win, &reg_timing_info);
	if ((cur_timing->de_width == input_timing_info->de_width)
		&& (cur_timing->de_height == input_timing_info->de_height)
		&& (reg_timing_info.vfreq == input_timing_info->v_freq)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			"timing not change: w=%d, h=%d, fps = %d, not regenerate timing!\n",
			input_timing_info->de_width,
			input_timing_info->de_height,
			input_timing_info->v_freq);
		return 0;
	}
	drv_hwreg_pq_display_b2r_set_timing(TRUE, &regouts, B2R_REG_DEV0, reg_timing);
	drv_hwreg_pq_display_b2r_set_timing(TRUE, &regouts, B2R_REG_DEV2, reg_timing);

	/* for global timing */
	memset(&timing_info, 0, sizeof(struct msg_b2r_timing_info));
	//send to pqu
	timing_info.win_id = win;
	timing_info.de_width = input_timing_info->de_width;
	timing_info.de_height = input_timing_info->de_height;
	timing_info.v_freq = input_timing_info->v_freq;
	timing_info.fld_type = input_timing_info->interlance;

	if (bPquEnable) {
		ret = pqu_video_set_b2r_timing_store((void *)&timing_info);
		if (ret == -ENODEV) {
			memcpy(&_timing_msg_info[_CB_INC(_timing_count)],
				   &timing_info,
				   sizeof(struct msg_b2r_timing_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "pqu_video_set_b2r_timing_store register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
					_ready_cb_pqu_video_set_b2r_timing_store);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "pqu_video_set_b2r_timing_store fail (ret=%d)\n", ret);
		}
	} else
		pqu_msg_send(PQU_MSG_SEND_B2R_TIMING_STORE, (void *)&timing_info);

	cur_timing->de_width = input_timing_info->de_width;
	cur_timing->de_height = input_timing_info->de_height;
	cur_timing->vfreq = input_timing_info->v_freq;

	return 0;
}

int _mtk_pq_b2r_transform_fourcc(struct v4l2_pix_format_mplane *input_fmt,
								 struct b2r_reg_fmt_info *reg_fmt)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s 4cc = 0x%x\n", __func__, input_fmt->pixelformat);

	switch (input_fmt->pixelformat) {
	case V4L2_PIX_FMT_MT21S:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_8Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CS:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_8Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CSA:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_8Bit;
		reg_fmt->fmt.lsb_raster = 1;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21S10T:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21S10R:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 1;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21S10TJ:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 1;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21S10RJ:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 1;
		reg_fmt->fmt.comp_jump = 1;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CS10T:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CS10R:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 1;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CS10TJ:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 1;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_MT21CS10RJ:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 1;
		reg_fmt->fmt.bit_depth = B2R_BD_10Bit;
		reg_fmt->fmt.lsb_raster = 1;
		reg_fmt->fmt.comp_jump = 1;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_YUYV:
		reg_fmt->fmt.yuv422 = 1;
		reg_fmt->fmt.rgb = B2R_RGB_NONE;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_8Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	case V4L2_PIX_FMT_ARGB32:
		reg_fmt->fmt.yuv422 = 0;
		reg_fmt->fmt.rgb = B2R_RGB_888;
		reg_fmt->fmt.compress = 0;
		reg_fmt->fmt.bit_depth = B2R_BD_8Bit;
		reg_fmt->fmt.lsb_raster = 0;
		reg_fmt->fmt.comp_jump = 0;
		reg_fmt->fmt.vsd = B2R_VSD_NONE;
		break;
	/* Fixme: lack of vsd */
	default:
		ret = -EPERM;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
		"%s: not support pixelformat 0x%x.\n",
		__func__,
		input_fmt->pixelformat);
		break;
	}

	return ret;
}

int _mtk_pq_b2r_config_size(struct v4l2_pix_format_mplane *input_fmt,
							struct b2r_reg_size_info *reg_size)
{
	int ret = 0;

	//rotate, alignment
	reg_size->ufo_x = 0;
	reg_size->ufo_y = 0;
	reg_size->ufo_width = input_fmt->width >> 4;
	reg_size->ufo_height = input_fmt->height;
	reg_size->b2r_width = input_fmt->width;
	reg_size->b2r_height = input_fmt->height;

	if (input_fmt->pixelformat == V4L2_PIX_FMT_YUYV)
		reg_size->ufo_pitch = input_fmt->plane_fmt[0].bytesperline >> 1;
	else
		reg_size->ufo_pitch = input_fmt->plane_fmt[0].bytesperline >> 2;

	return ret;
}

int _mtk_pq_b2r_config_interlace(struct v4l2_pix_format_mplane *input_fmt,
								 struct b2r_reg_fmt_info *reg_fmt)
{
	int ret = 0;

	switch (input_fmt->field) {
	case V4L2_FIELD_NONE:
		reg_fmt->scantype.pro_frame = B2R_INT_PRO;
		reg_fmt->scantype.field = 0;
		break;
	case V4L2_FIELD_INTERLACED:
		/* interlace frame (majority:top) */
		reg_fmt->scantype.pro_frame = B2R_INT_Frame;
		reg_fmt->scantype.field = 0;
		break;
	case V4L2_FIELD_ALTERNATE:
		/* interlace field */
		reg_fmt->scantype.pro_frame = B2R_INT_Field;
		reg_fmt->scantype.field = 0;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: not support v4l2 field = 0x%x.\n",
				   __func__, input_fmt->field);
		break;
	}

	return ret;
}

int _mtk_pq_b2r_set_fmt(struct v4l2_format *format, struct v4l2_b2r_info *b2r_info)
{
	struct b2r_reg_fmt_info reg_fmt;
	struct b2r_reg_size_info reg_size;
	struct v4l2_pix_format_mplane pix_fmt;
	struct hwregOut regouts;
	enum b2r_reg_dev dev_id = B2R_REG_DEV0;

	if (format->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: set format type is 0x%x, not for b2r.\n",
				   __func__, format->type);
		return -EPERM;
	}
	memset(&reg_fmt, 0, sizeof(struct b2r_reg_fmt_info));
	memset(&reg_size, 0, sizeof(struct b2r_reg_size_info));
	memset(&pix_fmt, 0, sizeof(struct v4l2_pix_format_mplane));
	memcpy(&pix_fmt, &format->fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));

	/* decode fourcc to reg format */
	if (_mtk_pq_b2r_transform_fourcc(&pix_fmt, &reg_fmt) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: failed!\n", __func__);
		return -EPERM;
	}

	/* config reg size */
	if (_mtk_pq_b2r_config_size(&pix_fmt, &reg_size) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: failed!\n", __func__);
		return -EPERM;
	}

	/* config reg interlace */
	if (_mtk_pq_b2r_config_interlace(&pix_fmt, &reg_fmt) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: failed!\n", __func__);
		return -EPERM;
	}

	drv_hwreg_pq_display_b2r_set_fmt(TRUE, &regouts, &regouts, dev_id, reg_fmt);
	drv_hwreg_pq_display_b2r_set_size(TRUE, &regouts, &regouts, dev_id, reg_size);

	return 0;
}

static void _ready_cb_pqu_video_set_b2r_glb_timing(void)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_set_b2r_glb_timing(&_global_timing_msg_info[_CB_DEC(_global_timing_count)]);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_set_b2r_glb_timing fail!\n");
}

int _mtk_pq_b2r_set_global_timing(struct b2r_glb_timing *timing_info,
								  struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;
	struct msg_b2r_global_timing_info glb_timing_info;

	memset(&glb_timing_info, 0, sizeof(struct msg_b2r_global_timing_info));
	//send to pqu
	glb_timing_info.win_id = timing_info->win_id;
	glb_timing_info.enable = timing_info->enable;
	b2r_info->global_timing.win_id = timing_info->win_id;
	b2r_info->global_timing.enable = timing_info->enable;
	if (bPquEnable) {
		ret = pqu_video_set_b2r_glb_timing((void *)&glb_timing_info);
		if (ret == -ENODEV) {
			memcpy(&_global_timing_msg_info[_CB_INC(_global_timing_count)],
				   &glb_timing_info,
				   sizeof(struct msg_b2r_global_timing_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "pqu_video_set_b2r_glb_timing register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
					_ready_cb_pqu_video_set_b2r_glb_timing);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "pqu_video_set_b2r_glb_timing fail (ret=%d)\n", ret);
		}
	} else
		pqu_msg_send(PQU_MSG_SEND_GLOBAL_TIMING, (void *)&glb_timing_info);

	return 0;
}

int mtk_pq_b2r_init(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct hwregOut regouts;
	struct b2r_init_info init_info;
	struct mtk_pq_platform_device *pqdev;

	/* pws */
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);

	/* enable sw en */
	/* b2r main */
	spin_lock(&pqdev->b2r_dev.b2r_slock);
	if (u32B2Rersion != B2R_VER6) {
		pqdev->b2r_dev.b2r_swen_dc02scip = devm_clk_get(pqdev->dev,
												"clk_b2r_dc0");
		if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_dc02scip)) {
			if (IS_ERR(pqdev->b2r_dev.b2r_swen_dc02scip)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"[%s][%d] b2r_swen_dc02scip init fail\n", __func__, __LINE__);
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"[%s][%d] b2r_swen_dc02scip NULL\n", __func__, __LINE__);
			}
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		ret = clk_prepare_enable(pqdev->b2r_dev.b2r_swen_dc02scip);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to enable dc0 clock\n");
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_dc02scip);
	}

	if ((u32B2Rersion != B2R_VER2) && (u32B2Rersion != B2R_VER6)) {
		/* b2r lite1 */
		pqdev->b2r_dev.b2r_swen_sub1_dc02scip = devm_clk_get(pqdev->dev,
												"clk_b2r_sub1");
		if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_sub1_dc02scip)) {
			if (IS_ERR(pqdev->b2r_dev.b2r_swen_sub1_dc02scip)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"[%s][%d] b2r_swen_sub1_dc02scip init fail\n",
				__func__, __LINE__);
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"[%s][%d] b2r_swen_sub1_dc02scip NULL\n", __func__, __LINE__);
			}
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		ret = clk_prepare_enable(pqdev->b2r_dev.b2r_swen_sub1_dc02scip);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to enable sub1_dc0 clock\n");
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_sub1_dc02scip);
	}

	/* film grain */
	if (u32B2Rersion != B2R_VER6) {
		pqdev->b2r_dev.b2r_swen_sram2scip = devm_clk_get(pqdev->dev,
												"clk_b2r_sram");
		if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_sram2scip)) {
			if (IS_ERR(pqdev->b2r_dev.b2r_swen_sram2scip)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			    "[%s][%d] b2r_swen_sram2scip init fail\n", __func__, __LINE__);
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"[%s][%d] b2r_swen_sram2scip NULL\n", __func__, __LINE__);
			}
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		ret = clk_prepare_enable(pqdev->b2r_dev.b2r_swen_sram2scip);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to enable sram clock\n");
			spin_unlock(&pqdev->b2r_dev.b2r_slock);
			return -EINVAL;
		}
		devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_sram2scip);
	}

	pqdev->b2r_dev.win_count++;
	spin_unlock(&pqdev->b2r_dev.b2r_slock);
	init_info.regbase = 0;
	init_info.b2r_version = u32B2Rersion;
	drv_hwreg_pq_display_b2r_init(TRUE, &regouts, B2R_REG_DEV0, init_info);
	drv_hwreg_pq_display_b2r_init(TRUE, &regouts, B2R_REG_DEV2, init_info);


	return ret;
}

int mtk_pq_b2r_exit(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev;

	/* pws */
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);

	spin_lock(&pqdev->b2r_dev.b2r_slock);
	if (pqdev->b2r_dev.win_count == 1) {
		/* disable sw en */
		if (u32B2Rersion != B2R_VER6) {
			pqdev->b2r_dev.b2r_swen_dc02scip =
			devm_clk_get(pqdev->dev, "clk_b2r_dc0");
			if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_dc02scip)) {
				if (IS_ERR(pqdev->b2r_dev.b2r_swen_dc02scip)) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_dc02scip init fail\n",
							   __func__, __LINE__);
				} else {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_dc02scip NULL\n",
							   __func__, __LINE__);
				}
				ret = -EINVAL;
			} else {
				if (__clk_is_enabled(pqdev->b2r_dev.b2r_swen_dc02scip))
					clk_disable_unprepare(pqdev->b2r_dev.b2r_swen_dc02scip);
				devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_dc02scip);
			}
		}

		if ((u32B2Rersion != B2R_VER2) && (u32B2Rersion != B2R_VER6)) {
			pqdev->b2r_dev.b2r_swen_sub1_dc02scip = devm_clk_get(pqdev->dev,
													"clk_b2r_sub1");
			if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_sub1_dc02scip)) {
				if (IS_ERR(pqdev->b2r_dev.b2r_swen_sub1_dc02scip)) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_sub1_dc02scip init fail\n",
							   __func__, __LINE__);
				} else {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_sub1_dc02scip NULL\n",
							   __func__, __LINE__);
				}
				ret = -EINVAL;
			} else {
				if (__clk_is_enabled(pqdev->b2r_dev.b2r_swen_sub1_dc02scip))
					clk_disable_unprepare(
						pqdev->b2r_dev.b2r_swen_sub1_dc02scip);
				devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_sub1_dc02scip);
			}
		}

		if (u32B2Rersion != B2R_VER6) {
			pqdev->b2r_dev.b2r_swen_sram2scip = devm_clk_get(pqdev->dev,
													"clk_b2r_sram");
			if (IS_ERR_OR_NULL(pqdev->b2r_dev.b2r_swen_sram2scip)) {
				if (IS_ERR(pqdev->b2r_dev.b2r_swen_sram2scip)) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_sram2scip init fail\n",
							   __func__, __LINE__);
				} else {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
							   "[%s][%d] b2r_swen_sram2scip NULL\n",
							   __func__, __LINE__);
				}
				ret = -EINVAL;
			} else {
				if (__clk_is_enabled(pqdev->b2r_dev.b2r_swen_sram2scip))
					clk_disable_unprepare(pqdev->b2r_dev.b2r_swen_sram2scip);
				devm_clk_put(pqdev->dev, pqdev->b2r_dev.b2r_swen_sram2scip);
			}
		}
		memset(&pqdev->b2r_dev.cur_timing, 0, sizeof(struct b2r_input_timing));
	}

	if (pqdev->b2r_dev.win_count > 0)
		pqdev->b2r_dev.win_count--;

	spin_unlock(&pqdev->b2r_dev.b2r_slock);

	return ret;
}

int mtk_pq_b2r_set_pix_format_in_mplane(struct v4l2_format *format,
	struct v4l2_b2r_info *b2r_info)
{
	struct v4l2_format input_format;
	int ret = 0;

	if ((!format) || (!b2r_info)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&input_format, 0, sizeof(struct v4l2_format));
	memcpy(&input_format, format, sizeof(struct v4l2_format));
	memcpy(&b2r_info->pix_fmt_in, &format->fmt.pix_mp,
		   sizeof(struct v4l2_pix_format_mplane));

	if (_mtk_pq_b2r_set_fmt(&input_format, b2r_info) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: failed!\n", __func__);
		ret = -EPERM;
		goto ERR;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "[B2R][DBG][%s] width = %d, height = %d\n",
			   __func__, b2r_info->pix_fmt_in.width,
			   b2r_info->pix_fmt_in.height);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "bytesperline = %d, field = %d, pixelformat = 0x%x\n",
			   b2r_info->pix_fmt_in.plane_fmt[0].bytesperline,
			   b2r_info->pix_fmt_in.field,
			   format->fmt.pix_mp.pixelformat);

ERR:
	return ret;

}

int mtk_pq_display_b2r_set_pix_format_mplane_info(struct mtk_pq_device *pq_dev,
		void *bufferctrl)
{
	struct v4l2_pq_s_buffer_info bufferInfo;
	struct v4l2_format input_format;

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_s_buffer_info));
	memcpy(&bufferInfo, bufferctrl, sizeof(struct v4l2_pq_s_buffer_info));

	if (WARN_ON(!bufferctrl) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				   "ptr is NULL, bufferctrl or pq_dev is invalid\n");
		return -EINVAL;
	}

	memset(&input_format, 0, sizeof(struct v4l2_format));
	input_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	pq_dev->b2r_info.pix_fmt_in.width = bufferInfo.width;
	pq_dev->b2r_info.pix_fmt_in.height = bufferInfo.height;

	/* update fmt infor from user */
	memcpy(&input_format.fmt.pix_mp, &pq_dev->b2r_info.pix_fmt_in,
		   sizeof(struct v4l2_pix_format_mplane));

	if (_mtk_pq_b2r_set_fmt(&input_format, &pq_dev->b2r_info) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: failed!\n", __func__);
		return -EPERM;
	}

	/* update fmt to user */
	pq_dev->b2r_info.buffer_num = B2R_BUFFER_NUM;
	pq_dev->b2r_info.plane_num = B2R_PLANE_NUM; /* y and uv */
	pq_dev->b2r_info.pix_fmt_in.num_planes = pq_dev->b2r_info.plane_num;

	return 0;
}

int mtk_pq_display_b2r_queue_setup(struct vb2_queue *vq,
			   unsigned int *num_buffers, unsigned int *num_planes,
			   unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_pq_ctx *pq_ctx = vb2_get_drv_priv(vq);
	struct mtk_pq_device *pq_dev = pq_ctx->pq_dev;
	//struct v4l2_b2r_info *b2r = &pq_dev->b2r_info;
	int i = 0;

	/* video buffers + meta buffer */
	*num_planes = B2R_PLANE_NUM + 1; /* y and uv */

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "[b2r] num_planes=%d\n", *num_planes);

	for (i = 0; i < B2R_PLANE_NUM; ++i) {
		sizes[i] = B2R_PLANE_SIZE;	// TODO
		alloc_devs[i] = pq_dev->dev;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "out size[%d]=%d\n", i, sizes[i]);
	}

	/* give meta fd min size */
	sizes[*num_planes - 1] = 1;
	alloc_devs[*num_planes - 1] = pq_dev->dev;

	return 0;
}

int mtk_pq_display_b2r_queue_setup_info(struct mtk_pq_device *pq_dev,
	void *bufferctrl)
{
	int ret = 0;
	struct v4l2_pq_g_buffer_info bufferInfo;
	struct v4l2_b2r_info *b2r = &pq_dev->b2r_info;
	int i = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	uint8_t frame_diff = 0;

	pqdev = dev_get_drvdata(pq_dev->dev);

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_g_buffer_info));

	/* video buffers + meta buffer */
	bufferInfo.plane_num = B2R_PLANE_NUM + 1;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "[b2r] plane_num=%d\n", bufferInfo.plane_num);

	for (i = 0; i < B2R_PLANE_NUM; ++i) {
		bufferInfo.size[i] = B2R_PLANE_SIZE;	/* TODO */
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			"out size[%d]=%d\n", i, bufferInfo.size[i]);
	}

	/* give meta fd min size */
	bufferInfo.size[bufferInfo.plane_num - 1] = 1;

	if (b2r->timing_in.disp_type == V4L2_E_PQ_B2R_DISP_TYPE_TS)
		bufferInfo.buffer_num = B2R_SW_QUEUE_FRAME_NUM;
	else
		bufferInfo.buffer_num = B2R_HW_QUEUE_FRAME_NUM;

	frame_diff = pq_dev->display_info.BufferTable[PQU_BUF_SCMI].frame_delay +
		pq_dev->display_info.BufferTable[PQU_BUF_UCM].frame_delay;

	ret = mtk_pq_buffer_get_extra_frame(pq_dev, &pq_dev->extra_frame);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Failed to mtk_pq_buffer_get_extra_frame\n");
		return ret;
	}
	bufferInfo.dque_count = frame_diff + pq_dev->extra_frame + 1;
	bufferInfo.buffer_num += pq_dev->extra_frame;

	/* PQU limit frame number is FRAME_BUFFER_SIZE */
	if (bufferInfo.buffer_num > FRAME_BUFFER_SIZE)
		bufferInfo.buffer_num = FRAME_BUFFER_SIZE;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
		"[b2r] buf_num=%u, dque_count=%u, extra=%u, frame_diff=%u\n",
		bufferInfo.buffer_num, bufferInfo.dque_count,
		pq_dev->extra_frame, frame_diff);

	bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_DISCRETE;
	/* update bufferctrl */
	memcpy(bufferctrl, &bufferInfo, sizeof(struct v4l2_pq_g_buffer_info));

	return 0;
}

int mtk_pq_b2r_set_flip(__u8 win, __s32 enable, struct v4l2_b2r_info *b2r_info,
	enum v4l2_b2r_flip flip_type)
{
	int ret = 0;

	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: type = %d, enable =%d\n",
			   __func__, flip_type, enable);

	switch (flip_type) {
	case B2R_HFLIP:
		b2r_info->b2r_stat.hflip = enable;
		break;
	case B2R_VFLIP:
		b2r_info->b2r_stat.vflip = enable;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "flip type not supported by b2r!\n");
		break;
	}


ERR:
	return ret;

}

int mtk_b2r_set_ext_timing(struct device *dev, __u8 win, void *ctrl,
						   struct v4l2_b2r_info *b2r_info)
{
	struct v4l2_timing input_timing_info;
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(dev);

	if ((!ctrl) || (!b2r_info)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&input_timing_info, 0, sizeof(struct v4l2_timing));
	memcpy(&input_timing_info, ctrl, sizeof(struct v4l2_timing));
	memcpy(&b2r_info->timing_in, &input_timing_info,
		   sizeof(struct v4l2_timing));
	b2r_info->input_format = input_timing_info.pixelformat;

	if (_mtk_pq_b2r_set_timing(dev, win, &input_timing_info, b2r_info,
							   &(pqdev->b2r_dev.cur_timing)) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_dump_timing failed!\n", __func__);
		ret = -EPERM;
		goto ERR;
	}

ERR:
	return ret;
}

int mtk_b2r_get_ext_timing(__u8 win,
	void *ctrl,
	struct v4l2_b2r_info *b2r_info)
{
	struct v4l2_timing input_timing_info;
	int ret = 0;

	if ((!ctrl) || (!b2r_info)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&input_timing_info, 0, sizeof(struct v4l2_timing));
	memcpy(&input_timing_info, &b2r_info->timing_in,
		   sizeof(struct v4l2_timing));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: v_total = 0x%x, h_total = 0x%x\n",
			   __func__,
			   input_timing_info.v_total,
			   input_timing_info.h_total);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "interlance = 0x%x, v_freq = %d, h_freq = %d\n",
			   input_timing_info.interlance,
			   input_timing_info.v_freq,
			   input_timing_info.h_freq);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "de_width = 0x%x, de_height = 0x%x\n",
			   input_timing_info.de_width,
			   input_timing_info.de_height);

	memcpy(ctrl, &input_timing_info, sizeof(struct v4l2_timing));

ERR:
	return ret;
}

int mtk_pq_b2r_set_forcep(__u8 win,
	bool enable,
	struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;

	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "%s: enable =%d\n", __func__, enable);
	b2r_info->b2r_stat.forcep = enable;

ERR:
	return ret;

}

int mtk_pq_b2r_set_delaytime(__u8 win, __u32 fps,
							 struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;

	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "%s: fps =%d\n", __func__, fps);

	b2r_info->delaytime_fps = fps;

ERR:
	return ret;

}

int mtk_pq_b2r_get_delaytime(__u8 win, __u16 *b2r_delay,
							 struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;

	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}

	if (b2r_info->delaytime_fps > 0)
		*b2r_delay = (((B2R_TOTAL_DELAY /
						b2r_info->delaytime_fps) + B2R_TOTAL_SHITF) /
						B2R_CALC);
	else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "%s: warning fps =%d\n",
				   __func__, b2r_info->delaytime_fps);
		*b2r_delay = 0;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "%s: delaytime =%d\n", __func__,
			   *b2r_delay);

ERR:
	return ret;

}

int mtk_pq_b2r_set_pattern(__u8 win, enum v4l2_ext_pq_pattern pat_type,
	    struct v4l2_b2r_pattern *b2r_pat_para, struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;
	struct b2r_reg_dmapat_info b2r_pat;
	struct hwregOut regouts;
	enum b2r_reg_dev dev_id = B2R_REG_DEV0;

	memset(&b2r_pat, 0, sizeof(struct b2r_reg_dmapat_info));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "%s: pattern type =%x\n",
		__func__, pat_type);
	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}

	switch (pat_type) {
	case V4L2_EXT_PQ_B2R_PAT_FRAMECOLOR:
		b2r_pat.type = B2R_PAT_Purecolor;
		b2r_pat.yuv_value.pat_y = b2r_pat_para->ydata;
		b2r_pat.yuv_value.pat_u = b2r_pat_para->cbdata;
		b2r_pat.yuv_value.pat_v = b2r_pat_para->crdata;
		break;
	default:
		ret = -1;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "b2r not support pattern type\n");
		break;
	}

	if (b2r_pat_para->enable == FALSE)
		b2r_pat.type = B2R_PAT_None;

	drv_hwreg_pq_display_b2r_set_pattern(TRUE,
		&regouts, dev_id, b2r_pat);

ERR:
	return ret;
}

int mtk_b2r_parse_dts(struct b2r_device *b2r_dev,
					  struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	struct device *property_dev = &pdev->dev;
	__u32 u32Tmp = 0;
	char *name;
	struct hwregOut regouts;
	struct b2r_init_info init_info;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	np = of_find_node_by_name(property_dev->of_node, "mtk-b2r");

	if (np == NULL) {
		PQ_MSG_ERROR("Failed to get mtk-b2r node\r\n");
		return -EINVAL;
	}

	name = "Pixel_Shift";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->pix_num = (u8)u32Tmp;
	u32B2RPixelNum = b2r_dev->pix_num;

	PQ_MSG_INFO("Pixel_Shift = %x\r\n", u32B2RPixelNum);

	name = "B2R_VERSION";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->b2r_ver = u32Tmp;
	u32B2Rersion = b2r_dev->b2r_ver;

	PQ_MSG_INFO("B2R_VERSION = %x\r\n", b2r_dev->b2r_ver);

	init_info.regbase = 0;
	init_info.b2r_version = u32B2Rersion;
	drv_hwreg_pq_display_b2r_init(TRUE, &regouts, B2R_REG_DEV0, init_info);
	drv_hwreg_pq_display_b2r_init(TRUE, &regouts, B2R_REG_DEV2, init_info);

	name = "B2R_H_MAX";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->h_max_size = u32Tmp;

	PQ_MSG_INFO("B2R_H_MAX = %x\r\n", b2r_dev->h_max_size);

	name = "B2R_V_MAX";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->v_max_size = u32Tmp;

	PQ_MSG_INFO("B2R_V_MAX = %x\r\n", b2r_dev->v_max_size);

	name = "B2R_422_H_MAX";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->h_422_max_size = u32Tmp;

	PQ_MSG_INFO("B2R_422_H_MAX = 0x%x\r\n", b2r_dev->h_422_max_size);

	name = "B2R_Rotate_H";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_support_h = u32Tmp;

	PQ_MSG_INFO("B2R_Rotate_H = 0x%x\r\n", b2r_dev->rotate_support_h);

	name = "B2R_Rotate_V";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_support_v = u32Tmp;

	PQ_MSG_INFO("B2R_ROTATE_V = 0x%x\r\n", b2r_dev->rotate_support_v);

	name = "B2R_Rotate_Interlace";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_interlace = u32Tmp;

	PQ_MSG_INFO("B2R_Rotate_Interlace = 0x%x\r\n", b2r_dev->rotate_interlace);

	name = "B2R_Rotate_Vsd";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_support_vsd = u32Tmp;

	PQ_MSG_INFO("B2R_Rotate_Vsd = 0x%x\r\n", b2r_dev->rotate_support_vsd);

	name = "B2R_VSD_Rotate_H";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_vsd_support_h = u32Tmp;

	PQ_MSG_INFO("B2R_VSD_Rotate_H = 0x%x\r\n", b2r_dev->rotate_vsd_support_h);

	name = "B2R_VSD_Rotate_V";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	b2r_dev->rotate_vsd_support_v = u32Tmp;

	PQ_MSG_INFO("B2R_VSD_Rotate_V = 0x%x\r\n", b2r_dev->rotate_vsd_support_v);

	name = "B2R_MAX_CLK";
	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0)
		goto Fail;
	B2R_CLK = u32Tmp;
	B2R_CLK *= B2R_FPS_SFT;

	PQ_MSG_INFO("B2R_MAX_CLK = %d, B2R_CLK = %lld\r\n", u32Tmp, B2R_CLK);

	of_node_put(np);

	return ret;
Fail:
	if (np != NULL)
		of_node_put(np);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "[%s, %d]: read DTS failed, name = %s\n",
			   __func__, __LINE__, name);
	return ret;
}

int mtk_b2r_set_ext_global_timing(__u8 win, bool enable, struct v4l2_b2r_info *b2r_info)
{
	struct b2r_glb_timing glb_timing_info;
	int ret = 0;

	if (!b2r_info) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&glb_timing_info, 0, sizeof(struct b2r_glb_timing));
	glb_timing_info.win_id = win;
	glb_timing_info.enable = enable;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: win_id = %d, enable = %d\n", __func__, win, enable);

	if (_mtk_pq_b2r_set_global_timing(&glb_timing_info, b2r_info) < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
				   "%s: _mtk_pq_b2r_dump_timing failed!\n", __func__);
		ret = -EPERM;
		goto ERR;
	}

ERR:
	return ret;
}

int mtk_b2r_get_ext_global_timing(__u8 win,
	bool *enable,
	struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;

	if ((!enable) || (!b2r_info)) {
		ret = -EINVAL;
		goto ERR;
	}

	*enable = (__s32)b2r_info->global_timing.enable;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: global timing win id = 0x%x, enable = %d\n",
			   __func__, win,
			   *enable);

ERR:
	return ret;
}

int mtk_b2r_get_ext_cap(__u8 win, void *ctrl, struct b2r_device *b2r_dev)
{
	struct v4l2_b2r_cap b2r_cap;
	int ret = 0;

	if ((!ctrl) || (!b2r_dev)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&b2r_cap, 0, sizeof(struct v4l2_b2r_cap));
	b2r_cap.h_max_420 = b2r_dev->h_max_size;
	b2r_cap.h_max_422 = b2r_dev->h_422_max_size;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: 420 h max = %d, 422 h max = %d\n",
			   __func__,
			   b2r_cap.h_max_420,
			   b2r_cap.h_max_422);

	memcpy(ctrl, &b2r_cap, sizeof(struct v4l2_b2r_cap));

ERR:
	return ret;
}

int _mtk_pq_b2r_get_max_fps(__u8 win, void *ctrl, struct b2r_device *b2r_dev)
{
	__u64 htt_est = 0;
	__u64 vtt_est = 0;
	struct v4l2_b2r_max_fps b2r_max_fps;
	int ret = 0;
	__u64 dividend;

	if ((!ctrl) || (!b2r_dev)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&b2r_max_fps, 0, sizeof(struct v4l2_b2r_max_fps));
	memcpy(&b2r_max_fps, ctrl, sizeof(struct v4l2_b2r_max_fps));

	b2r_max_fps.get_max_fps_w = b2r_dev->get_max_fps_w;
	b2r_max_fps.get_max_fps_h = b2r_dev->get_max_fps_h;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "pixel_per_tik = %x\r\n", u32B2RPixelNum);

	if ((u32B2Rersion == B2R_VER0) || (u32B2Rersion == B2R_VER1)
		|| (u32B2Rersion == B2R_VER3) || (u32B2Rersion == B2R_VER5)) {
		htt_est = (ALIGN_UPTO_32(b2r_max_fps.get_max_fps_w) >> u32B2RPixelNum)
		 + B2R_H_BLK;
	} else {
		htt_est = ALIGN_UPTO_32(b2r_max_fps.get_max_fps_w) + B2R_H_BLK;
	}
	vtt_est = ALIGN_UPTO_32(b2r_max_fps.get_max_fps_h) + B2R_V_BLK;
	dividend = B2R_CLK;
	if ((htt_est > 0) && (vtt_est > 0)) {
		do_div(dividend, htt_est);
		do_div(dividend, vtt_est);
		b2r_max_fps.max_fps = dividend;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "max_fps = %d  get_max_fps_w = %d get_max_fps_h = %d\n",
			   b2r_max_fps.max_fps,
			   b2r_max_fps.get_max_fps_w,
			   b2r_max_fps.get_max_fps_h);

	memcpy(ctrl, &b2r_max_fps, sizeof(struct v4l2_b2r_max_fps));

ERR:
	return ret;
}

int _mtk_pq_b2r_set_max_fps(__u8 win, void *ctrl, struct b2r_device *b2r_dev)
{
	struct v4l2_b2r_max_fps b2r_max_fps;
	int ret = 0;

	if ((!ctrl) || (!b2r_dev)) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&b2r_max_fps, 0, sizeof(struct v4l2_b2r_max_fps));
	memcpy(&b2r_max_fps, ctrl, sizeof(struct v4l2_b2r_max_fps));

	b2r_dev->get_max_fps_w = b2r_max_fps.get_max_fps_w;
	b2r_dev->get_max_fps_h = b2r_max_fps.get_max_fps_h;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
			   "%s: get_max_fps_w = %d, get_max_fps_h = %d\n",
			   __func__,
			   b2r_max_fps.get_max_fps_w,
			   b2r_max_fps.get_max_fps_h);

	memcpy(ctrl, &b2r_max_fps, sizeof(struct v4l2_b2r_max_fps));

ERR:
	return ret;
}

int _mtk_pq_b2r_get_rb_switch(__u8 win, bool *enable, struct v4l2_b2r_info *b2r_info)
{
	int ret = 0;

	if ((!enable) || (!b2r_info)) {
		ret = -EINVAL;
		goto ERR;
	}

	if (b2r_info->input_format == V4L2_PIX_FMT_ABGR32)
		*enable = (__s32)TRUE;
	else
		*enable = (__s32)FALSE;

ERR:
	return ret;
}

int mtk_b2r_get_hw_gtgen(__u8 win, struct b2r_reg_timing *reg_timing_info)
{
	struct b2r_timing_info timing_info;
	int ret = 0;
	__u64 gtegn_clk = B2R_CLK_360MHZ;
	__u64 fps = 0;

	if (!reg_timing_info) {
		ret = -EINVAL;
		goto ERR;
	}
	memset(&timing_info, 0, sizeof(struct b2r_timing_info));
	memset(reg_timing_info, 0, sizeof(struct b2r_reg_timing));

	drv_hwreg_pq_display_b2r_get_wtgen(&timing_info);
	if ((timing_info.H_TotalCount > 0) && (timing_info.V_TotalCount > 0)) {
		fps = ((gtegn_clk * B2R_FPS_SFT) << u32B2RPixelNum) / ((timing_info.H_TotalCount *
		timing_info.V_TotalCount) + timing_info.Hcnt_FineTune);
	}
	reg_timing_info->H_TotalCount = timing_info.H_TotalCount;
	reg_timing_info->V_TotalCount = timing_info.V_TotalCount;
	reg_timing_info->Hcnt_FineTune = timing_info.Hcnt_FineTune;
	reg_timing_info->vfreq = fps;

ERR:
	return ret;
}

int mtk_b2r_get_hw_addr(__u8 win, struct b2r_ctrl *reg_b2r_ctrl)
{
	struct b2r_reg_addr_info addr_info;
	int ret = 0;

	if (!reg_b2r_ctrl) {
		ret = -EINVAL;
		goto ERR;
	}

	memset(&addr_info, 0, sizeof(struct b2r_reg_addr_info));
	memset(reg_b2r_ctrl, 0, sizeof(struct b2r_ctrl));

	drv_hwreg_pq_display_b2r_get_addr(&addr_info, B2R_REG_DEV0);
	reg_b2r_ctrl->main.luma_fb = addr_info.luma_fb;
	reg_b2r_ctrl->main.chroma_fb = addr_info.chroma_fb;
	reg_b2r_ctrl->main.luma_bln_fb = addr_info.luma_bln_fb;
	reg_b2r_ctrl->main.chroma_bln_fb = addr_info.chroma_bln_fb;
	drv_hwreg_pq_display_b2r_get_addr(&addr_info, B2R_REG_DEV2);
	reg_b2r_ctrl->sub1.luma_fb = addr_info.luma_fb;
	reg_b2r_ctrl->sub1.chroma_fb = addr_info.chroma_fb;
	reg_b2r_ctrl->sub1.luma_bln_fb = addr_info.luma_bln_fb;
	reg_b2r_ctrl->sub1.chroma_bln_fb = addr_info.chroma_bln_fb;

ERR:
	return ret;
}

void mtk_b2r_set_reg_ctrl(struct b2r_ctrl *reg_b2r_ctrl)
{
	if (!reg_b2r_ctrl)
		return;

	pre_reg_b2r_ctrl.main.luma_fb = reg_b2r_ctrl->main.luma_fb;
	pre_reg_b2r_ctrl.main.chroma_fb = reg_b2r_ctrl->main.chroma_fb;
	pre_reg_b2r_ctrl.sub1.luma_fb = reg_b2r_ctrl->sub1.luma_fb;
	pre_reg_b2r_ctrl.sub1.chroma_fb = reg_b2r_ctrl->sub1.chroma_fb;
}

struct b2r_enable mtk_b2r_get_enable(struct b2r_ctrl *reg_b2r_ctrl)
{
	struct b2r_enable b2r_en;

	b2r_en.main = FALSE;
	b2r_en.sub1 = FALSE;

	if (!reg_b2r_ctrl)
		goto ERR;

	if ((reg_b2r_ctrl->main.luma_fb != 0x0) && (reg_b2r_ctrl->main.chroma_fb != 0x0)) {
		if ((reg_b2r_ctrl->main.luma_fb != reg_b2r_ctrl->main.chroma_fb)
		&& (reg_b2r_ctrl->main.luma_fb != pre_reg_b2r_ctrl.main.luma_fb)) {
			b2r_en.main = TRUE;
		}
	}

	if ((reg_b2r_ctrl->sub1.luma_fb != 0x0) && (reg_b2r_ctrl->sub1.chroma_fb != 0x0)) {
		if ((reg_b2r_ctrl->sub1.luma_fb != reg_b2r_ctrl->sub1.chroma_fb)
		&& (reg_b2r_ctrl->sub1.luma_fb != pre_reg_b2r_ctrl.sub1.luma_fb)) {
			b2r_en.sub1 = TRUE;
		}
	}

ERR:
	return b2r_en;
}

static int _b2r_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev = container_of(ctrl->handler,
	struct mtk_pq_device, b2r_ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_B2R_TIMING:
		ret = mtk_b2r_get_ext_timing(pq_dev->dev_indx,
				(void *)ctrl->p_new.p_u8, &pq_dev->b2r_info);
		break;
	case V4L2_CID_B2R_GLOBAL_TIMING:
		ret = mtk_b2r_get_ext_global_timing(pq_dev->dev_indx,
			(bool *)&ctrl->val,
			&pq_dev->b2r_info);
		break;
	case V4L2_CID_B2R_GET_CAP:
		ret = mtk_b2r_get_ext_cap(pq_dev->dev_indx,
				(void *)ctrl->p_new.p_u8, &pq_dev->b2r_dev);
		break;
	case V4L2_CID_B2R_GET_MAX_FPS:
		ret = _mtk_pq_b2r_get_max_fps(pq_dev->dev_indx,
				(void *)ctrl->p_new.p_u8, &pq_dev->b2r_dev);
		break;
	case V4L2_CID_B2R_GET_RB_SWAP:
		ret = _mtk_pq_b2r_get_rb_switch(pq_dev->dev_indx,
				(bool *)&ctrl->val, &pq_dev->b2r_info);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int _b2r_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev = container_of(ctrl->handler,
	struct mtk_pq_device, b2r_ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_B2R_TIMING:
		ret = mtk_b2r_set_ext_timing(pq_dev->dev,
		pq_dev->dev_indx,
		(void *)ctrl->p_new.p_u8,
		&pq_dev->b2r_info);
		break;
	case V4L2_CID_B2R_GLOBAL_TIMING:
		ret = mtk_b2r_set_ext_global_timing(
		pq_dev->dev_indx,
		(bool)ctrl->val,
		&pq_dev->b2r_info);
		break;
	case V4L2_CID_B2R_GET_MAX_FPS:
		ret = _mtk_pq_b2r_set_max_fps(pq_dev->dev_indx,
		(void *)ctrl->p_new.p_u8, &pq_dev->b2r_dev);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

int mtk_pq_b2r_subdev_init(struct device *dev,
		struct b2r_device *b2r)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	int ret = 0;
	const struct resources_name *b2r_res = &b2r_res_name;

	/* Interrupt */
	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
		b2r_res->interrupt[0]);
	if (!res) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R, "b2r missing IRQ\n");
		return -EINVAL;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_B2R,
		"get b2r irq numver = %llu\n", (unsigned long long)res->start);
	b2r->irq = res->start;

	disable_irq(b2r->irq);

	return ret;
}

static const struct v4l2_ctrl_ops b2r_ctrl_ops = {
	.g_volatile_ctrl = _b2r_g_ctrl,
	.s_ctrl = _b2r_s_ctrl,
};

static const struct v4l2_ctrl_config b2r_ctrl[] = {
	{
		.ops = &b2r_ctrl_ops,
		.id = V4L2_CID_B2R_TIMING,
		.name = "input timing ext info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_timing)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &b2r_ctrl_ops,
		.id = V4L2_CID_B2R_GLOBAL_TIMING,
		.name = "set b2r global timing",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &b2r_ctrl_ops,
		.id = V4L2_CID_B2R_GET_CAP,
		.name = "get b2r cap",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_b2r_cap)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &b2r_ctrl_ops,
		.id = V4L2_CID_B2R_GET_MAX_FPS,
		.name = "get b2r max fps",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_b2r_max_fps)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &b2r_ctrl_ops,
		.id = V4L2_CID_B2R_GET_RB_SWAP,
		.name = "get r/b swap info",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},

};

/* subdev operations */
static const struct v4l2_subdev_ops b2r_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops b2r_sd_internal_ops = {
};

int mtk_pq_register_b2r_subdev(struct v4l2_device *pq_dev,
		struct v4l2_subdev *subdev_b2r,
		struct v4l2_ctrl_handler *b2r_ctrl_handler)
{
	int ret = 0;
	__u32 ctrl_count;
	__u32 ctrl_num = sizeof(b2r_ctrl) / sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(b2r_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(b2r_ctrl_handler,
							 &b2r_ctrl[ctrl_count], NULL);
	}
	ret = b2r_ctrl_handler->error;
	if (ret) {
		v4l2_err(pq_dev, "failed to create b2r ctrl handler\n");
		goto exit;
	}
	subdev_b2r->ctrl_handler = b2r_ctrl_handler;

	v4l2_subdev_init(subdev_b2r, &b2r_sd_ops);
	subdev_b2r->internal_ops = &b2r_sd_internal_ops;
	strlcpy(subdev_b2r->name, "mtk-b2r", sizeof(subdev_b2r->name));

	ret = v4l2_device_register_subdev(pq_dev, subdev_b2r);
	if (ret) {
		v4l2_err(pq_dev, "failed to register b2r subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(b2r_ctrl_handler);
	return ret;
}

void mtk_pq_unregister_b2r_subdev(struct v4l2_subdev *subdev_b2r)
{
	v4l2_ctrl_handler_free(subdev_b2r->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_b2r);
}

