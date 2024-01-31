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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <uapi/linux/mtk-v4l2-pq.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>

#include "mtk_pq.h"
#include "mtk_pq_hdr.h"
//#include "mtk_pq_tch.h"
//#include "mtk_pq_dolby.h"
#include "mtk_pq_common.h"
#include "mtk_pq_buffer.h"

extern bool bPquEnable;

static bool bDVBin_mmp;
static bool bIsDVBinDone;

// set ctrl
int mtk_hdr_SetHDRMode(struct st_hdr_info *hdr_info, __s32 mode)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "hdr enable mode = %d\n", mode);

	if (!hdr_info)
		return -EFAULT;

	return 0;
}

int mtk_hdr_SetDolbyViewMode(struct st_hdr_info *hdr_info,
	__s32 mode)
{
	if (!hdr_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "dolby view mode = %d\n", mode);

	// TODO by DV owner

	return 0;
}

int mtk_hdr_SetTmoLevel(struct st_hdr_info *hdr_info,
	__s32 level)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "tmo level = %d\n", level);

	if (!hdr_info)
		return -EFAULT;

	return 0;
}

// get ext ctrl
int mtk_hdr_GetHDRType(struct st_hdr_info *hdr_info,
	void *ctrl, struct mtk_pq_device *pq_dev)
{
	struct v4l2_get_hdr_type *get_hdr_type;

	if ((!hdr_info) || (!ctrl) || (!pq_dev))
		return -EFAULT;

	get_hdr_type = (struct v4l2_get_hdr_type *)ctrl;

	get_hdr_type->hdr_type = pq_dev->hdr_type;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "get_hdr_type = %d\n", get_hdr_type->hdr_type);

	// TODO

	return 0;
}

// set ext ctrl
int mtk_hdr_SetDolbyBinMmpStatus(struct st_hdr_info *hdr_info, void *ctrl)
{
	int ret = 0;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	bDVBin_mmp = *((bool *)ctrl);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "[DV CFG] SetDolbyBinMmpStatus = %d\n", bDVBin_mmp);

	return ret;
}

int mtk_hdr_SetDVBinDone(struct st_hdr_info *hdr_info, void *ctrl,
	struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	v4l2_PQ_dv_config_info_t *dv_config_info = NULL;
	struct msg_config_info *config_info_msg = NULL;

	if ((!hdr_info) || (!ctrl) || (!pqdev))
		return -EFAULT;

	config_info_msg = &pqdev->config_info;

	if (!config_info_msg)
		return -EFAULT;

	bIsDVBinDone = *((bool *)ctrl);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"[DV CFG] bIsDVBinDone=%d, bPquEnable=%d, hwmap_config va=0x%llX, pa=0x%llX\n",
		bIsDVBinDone, bPquEnable, pqdev->hwmap_config.va, pqdev->hwmap_config.pa);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"[DV CFG] dv_config va=0x%llX, pa=0x%llX\n",
		pqdev->dv_config.va, pqdev->dv_config.pa);

	if (bIsDVBinDone) {
		dv_config_info = (v4l2_PQ_dv_config_info_t *)pqdev->dv_config.va;
		dv_config_info->bin_info.pa = pqdev->dv_config.pa + DV_CONFIG_BIN_OFFSET;
		dv_config_info->bin_info.va = pqdev->dv_config.va + DV_CONFIG_BIN_OFFSET;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"[DV CFG] dolby bin: va=0x%llx, pa=0x%llx, size=%d\n",
			(__u64)dv_config_info->bin_info.va,
			(__u64)dv_config_info->bin_info.pa,
			dv_config_info->bin_info.size);

		mtk_pq_dv_refresh_dolby_support(dv_config_info, NULL);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"[DV CFG] dolby capability: %08x (en=%d), dv=%d\n",
			dv_config_info->cap_info.capability,
			dv_config_info->cap_info.en,
			mtk_pq_dv_get_dv_support());

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"[DV CFG][DV_BL][PQ DD] u8CurveXNum = %d\n",
			dv_config_info->dv_gd_tbl_info.u8CurveXNum);

		/* update PR cap */
		mtk_pq_dv_update_pr_cap(dv_config_info, pqdev);

		if (bPquEnable) {
			config_info_msg->config = pqdev->hwmap_config.pa;
			config_info_msg->dvconfig = pqdev->dv_config.pa;
		} else {
			config_info_msg->config = pqdev->hwmap_config.va;
			config_info_msg->dvconfig = pqdev->dv_config.va;
		}

		config_info_msg->dvconfigsize = sizeof(v4l2_PQ_dv_config_info_t);
		config_info_msg->dvconfig_en = true;
		config_info_msg->ignore_init_en = true;
		mtk_pq_common_config(config_info_msg, bPquEnable);
		config_info_msg->config = pqdev->hwmap_config.va;
		config_info_msg->dvconfig = pqdev->dv_config.va;
	}

	return ret;
}

int mtk_hdr_SetDolby3DLut(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_dolby_3dlut dolby_3dlut;
	__u8 *pu8_3dlut = NULL;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	memset(&dolby_3dlut, 0, sizeof(struct v4l2_dolby_3dlut));
	memcpy(&dolby_3dlut, ctrl, sizeof(struct v4l2_dolby_3dlut));
	pu8_3dlut = kzalloc(dolby_3dlut.size, GFP_KERNEL);
	if (pu8_3dlut == NULL)
		return -EFAULT;

	if (copy_from_user((void *)pu8_3dlut,
		(__u8 __user *)dolby_3dlut.p.pu8_data, dolby_3dlut.size)) {
		kfree(pu8_3dlut);
		return -EFAULT;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "Size = %u\n", dolby_3dlut.size);

	//todo by dolby owner
	kfree(pu8_3dlut);

	return 0;
}

int mtk_hdr_SetCUSColorRange(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_cus_color_range *cus_color_range;
	__u8 u8ColorRangeType;
	__u8 u8WhiteLevel;
	__u8 u8BlackLevel;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	cus_color_range = (struct v4l2_cus_color_range *)ctrl;
	u8ColorRangeType = cus_color_range->cus_type;
	u8WhiteLevel = cus_color_range->ultral_white_level;
	u8BlackLevel = cus_color_range->ultral_black_level;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "cus_type = %u, ultral_black_level = %u, ultral_white_level = %u\n",
			   u8ColorRangeType, u8WhiteLevel, u8BlackLevel);

	return 0;
}

int mtk_hdr_SetCustomerSetting(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_cus_ip_setting cus_ip_setting;
	__u8 *pu8_setting = NULL;

	memset(&cus_ip_setting, 0, sizeof(struct v4l2_cus_ip_setting));
	memcpy(&cus_ip_setting, ctrl, sizeof(struct v4l2_cus_ip_setting));

	if (cus_ip_setting.ip >= V4L2_CUS_IP_MAX) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "customer setting error\n");
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "CustomerIp = %u\n", cus_ip_setting.ip);

	pu8_setting = kzalloc(cus_ip_setting.param_size, GFP_KERNEL);
	if (pu8_setting == NULL)
		return -EFAULT;

	if (copy_from_user((void *)pu8_setting,
		(__u8 __user *)cus_ip_setting.p.pu8_param,
		cus_ip_setting.param_size)) {
		kfree(pu8_setting);
		return -EFAULT;
	}

	// TODO
	kfree(pu8_setting);

	return 0;
}

int mtk_hdr_SetHDRAttr(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_hdr_attr_info *hdr_attr_info;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	hdr_attr_info = (struct v4l2_hdr_attr_info *)ctrl;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "hdr_type=%u,b_output_max_luminace=%u,output_max_luminace=%u,b_output_tr=%u,enOutputTR=%u\n",
			   hdr_attr_info->hdr_type,
			   hdr_attr_info->b_output_max_luminace,
			   hdr_attr_info->output_max_luminace,
			   hdr_attr_info->b_output_tr,
			   hdr_attr_info->output_tr);

	// TODO by TCH owner

	return 0;
}

int mtk_hdr_SetSeamlessStatus(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_seamless_status *seamless_status;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	seamless_status = (struct v4l2_seamless_status *)ctrl;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "b_hdr_changed = %u, b_cfd_updated = %u, b_mute_done = %u, b_reset_flag = %u\n",
			   seamless_status->b_hdr_changed,
			   seamless_status->b_cfd_updated,
			   seamless_status->b_mute_done,
			   seamless_status->b_reset_flag);

	// TODO

	return 0;
}

int mtk_hdr_SetCusColorMetry(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_cus_colori_metry *cus_colori_metry;
	bool bCusCP;
	__u16 u16WhitePointX;
	__u16 u16WhitePointY;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	cus_colori_metry = (struct v4l2_cus_colori_metry *)ctrl;
	bCusCP = cus_colori_metry->b_customer_color_primaries;
	u16WhitePointX = cus_colori_metry->white_point_x;
	u16WhitePointY = cus_colori_metry->white_point_y;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "b_customer_CP = %u, white_point_x = %u, white_point_y = %u\n",
			   bCusCP, u16WhitePointX, u16WhitePointY);

	return 0;
}

int mtk_hdr_SetDolbyGDInfo(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_dolby_gd_info *dolby_gd_info;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	dolby_gd_info = (struct v4l2_dolby_gd_info *)ctrl;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "b_global_dimming = %u, mm_delay_frame = %d, hdmi_delay_frame = %d\n",
			   dolby_gd_info->b_global_dimming, dolby_gd_info->mm_delay_frame,
			   dolby_gd_info->hdmi_delay_frame);

	// TODO by DV owner

	return 0;
}

// get ext ctrl
int mtk_hdr_GetSeamlessStatus(struct st_hdr_info *hdr_info,
	void *ctrl)
{
	struct v4l2_seamless_status *seamless_status;

	if ((!hdr_info) || (!ctrl))
		return -EFAULT;

	seamless_status = (struct v4l2_seamless_status *)ctrl;
	seamless_status->b_hdr_changed = false;
	seamless_status->b_cfd_updated = false;
	seamless_status->b_mute_done = false;
	seamless_status->b_reset_flag = false;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "b_hdr_changed = %u, b_cfd_updated = %u, b_mute_done = %u, b_reset_flag = %u\n",
			   seamless_status->b_hdr_changed, seamless_status->b_cfd_updated,
			   seamless_status->b_mute_done, seamless_status->b_reset_flag);

	// TODO

	return 0;
}


// set input
int mtk_hdr_SetInputSource(struct st_hdr_info *hdr_info,
	__u8 u8Input)
{
	if (!hdr_info)
		return -EFAULT;

	return 0;
}

// set format
int mtk_hdr_SetPixFmtIn(struct st_hdr_info *hdr_info,
	struct v4l2_pix_format_mplane *pix)
{
	if ((!pix) || (!hdr_info))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "pixel format = %u, color space = %u, quantization = %u, transfer function = %u\n",
			   pix->pixelformat, pix->colorspace, pix->quantization, pix->xfer_func);

	return 0;
}


int mtk_hdr_SetPixFmtOut(struct st_hdr_info *hdr_info,
	struct v4l2_pix_format_mplane *pix)
{
	if ((!pix) || (!hdr_info))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
			   "pixel format = %u, color space = %u, quantization = %u, transfer function = %u\n",
			   pix->pixelformat, pix->colorspace, pix->quantization, pix->xfer_func);

	return 0;
}

// stream on
int mtk_hdr_StreamOn(struct st_hdr_info *hdr_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "HDR stream on\n");

	return 0;
}

// stream off
int mtk_hdr_StreamOff(struct st_hdr_info *hdr_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "HDR stream off\n");

	return 0;
}

int mtk_hdr_Open(struct st_hdr_info *hdr_info)
{
	if (!hdr_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "HDR open\n");

	return 0;
}

int mtk_hdr_Close(struct st_hdr_info *hdr_info)
{
	if (!hdr_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "HDR close\n");
	//todo
	return 0;
}

int mtk_hdr_SubscribeEvent(struct st_hdr_info *hdr_info,
	__u32 event_type)
{
	if (!hdr_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "subscribe event = %u\n", event_type);
	//todo
	return 0;
}

int mtk_hdr_UnsubscribeEvent(struct st_hdr_info *hdr_info,
	__u32 event_type)
{
	if (!hdr_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "unsubscribe event = %u\n", event_type);
	//todo
	return 0;
}

bool mtk_hdr_GetDVBinMmpStatus(void)
{
	return bDVBin_mmp;
}

bool mtk_hdr_GetDVBinDone(void)
{
	return bIsDVBinDone;
}
