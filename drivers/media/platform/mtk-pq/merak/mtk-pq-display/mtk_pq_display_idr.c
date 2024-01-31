// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-----------------------------------------------------------------------------
//  Include Files
//-----------------------------------------------------------------------------

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>


#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <linux/videodev2.h>
#include <linux/dma-buf.h>
#include <uapi/linux/mtk-v4l2-pq.h>

#include "mtk_pq.h"

//-----------------------------------------------------------------------------
//  Driver Compiler Options
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define PQ_IDR_FIELD_DEFAULT (0xFF)
#define Debug_idx_max (6)
#define IDR_SW_QUEUE_FRAME_NUM	(2)
#define IDR_HW_QUEUE_FRAME_NUM	(4)
//-----------------------------------------------------------------------------
//  Local Structurs
//-----------------------------------------------------------------------------
struct format_info {
	u32 v4l2_fmt;
	u32 meta_fmt;
	u8 yuv_mode;
	u8 plane_num;
	u8 buffer_num;
};

//-----------------------------------------------------------------------------
//  Global Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Global Structurs
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Debug Functions
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------
static bool _is_yuv_422(u32 meta_fmt)
{
	if ((meta_fmt == MEM_FMT_YUV422_Y_UV_12B) ||
		(meta_fmt == MEM_FMT_YUV422_Y_UV_10B) ||
		(meta_fmt == MEM_FMT_YUV422_Y_VU_10B) ||
		(meta_fmt == MEM_FMT_YUV422_Y_UV_8B) ||
		(meta_fmt == MEM_FMT_YUV422_Y_VU_8B) ||
		(meta_fmt == MEM_FMT_YUV422_Y_UV_8B_CE) ||
		(meta_fmt == MEM_FMT_YUV422_Y_VU_8B_CE) ||
		(meta_fmt == MEM_FMT_YUV422_Y_UV_8B_6B_CE) ||
		(meta_fmt == MEM_FMT_YUV422_Y_UV_6B_CE) ||
		(meta_fmt == MEM_FMT_YUV422_Y_VU_6B_CE) ||
		(meta_fmt == MEM_FMT_YUV422_YVYU_8B))
		return TRUE;
	else
		return FALSE;
}

static bool _is_yuv_420(u32 meta_fmt)
{
	if ((meta_fmt == MEM_FMT_YUV420_Y_UV_10B) ||
		(meta_fmt == MEM_FMT_YUV420_Y_VU_10B) ||
		(meta_fmt == MEM_FMT_YUV420_Y_UV_8B) ||
		(meta_fmt == MEM_FMT_YUV420_Y_VU_8B) ||
		(meta_fmt == MEM_FMT_YUV420_Y_UV_8B_CE) ||
		(meta_fmt == MEM_FMT_YUV420_Y_VU_8B_CE) ||
		(meta_fmt == MEM_FMT_YUV420_Y_UV_8B_6B_CE) ||
		(meta_fmt == MEM_FMT_YUV420_Y_UV_6B_CE) ||
		(meta_fmt == MEM_FMT_YUV420_Y_VU_6B_CE))
		return TRUE;
	else
		return FALSE;
}

static int _get_fmt_info(u32 pixfmt_v4l2, struct format_info *fmt_info)
{
	if (WARN_ON(!fmt_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, fmt_info=0x%lx\n", (unsigned long)fmt_info);
		return -EINVAL;
	}

	switch (pixfmt_v4l2) {
	/* ------------------ RGB format ------------------ */
	/* 1 plane */
	case V4L2_PIX_FMT_RGB565:
		fmt_info->meta_fmt = MEM_FMT_RGB565;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_RGB888:
		fmt_info->meta_fmt = MEM_FMT_RGB888;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_RGB101010:
		fmt_info->meta_fmt = MEM_FMT_RGB101010;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_RGB121212:
		fmt_info->meta_fmt = MEM_FMT_RGB121212;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_ARGB8888:
		fmt_info->meta_fmt = MEM_FMT_ARGB8888;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_ABGR8888:
		fmt_info->meta_fmt = MEM_FMT_ABGR8888;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_RGBA8888:
		fmt_info->meta_fmt = MEM_FMT_RGBA8888;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_BGRA8888:
		fmt_info->meta_fmt = MEM_FMT_BGRA8888;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_ARGB2101010:
		fmt_info->meta_fmt = MEM_FMT_ARGB2101010;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_ABGR2101010:
		fmt_info->meta_fmt = MEM_FMT_ABGR2101010;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		fmt_info->meta_fmt = MEM_FMT_RGBA1010102;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		fmt_info->meta_fmt = MEM_FMT_BGRA1010102;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	/* 3 plane */
	case V4L2_PIX_FMT_RGB_8_8_8_CE:
		fmt_info->meta_fmt = MEM_FMT_RGB_8_8_8_CE;
		fmt_info->plane_num = 3;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 0;
		break;
	/* ------------------ YUV format ------------------ */
	/* 1 plane */
	case V4L2_PIX_FMT_YUV444_VYU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_VYU_10B;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV444_VYU_8B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_VYU_8B;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YVYU:
		fmt_info->meta_fmt = MEM_FMT_YUV422_YVYU_8B;
		fmt_info->plane_num = 1;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	/* 2 plane */
	case V4L2_PIX_FMT_YUV422_Y_UV_12B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_12B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_10B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_VU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_10B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_NV16:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_8B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_NV61:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_8B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_8B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_VU_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_8B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_8B_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_UV_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV422_Y_VU_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_10B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_10B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_NV12:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_8B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_NV21:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_8B;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_8B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_8B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_8B_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_6B_CE;
		fmt_info->plane_num = 2;
		fmt_info->buffer_num = 1;
		fmt_info->yuv_mode = 1;
		break;
	/* TODO: multi buffer */
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"unknown fmt = %d!!!\n", pixfmt_v4l2);
		return -EINVAL;

	}

	fmt_info->v4l2_fmt = pixfmt_v4l2;

	return 0;
}

static int _decide_duplicate_out(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf,
	struct meta_srccap_frame_info *meta_srccap,
	struct m_pq_display_idr_duplicate_out *duplicate_out)
{
	struct mtk_pq_platform_device *pq_pdev = NULL;
	struct pq_display_idr_info *idr_info = NULL;
	bool same_frame = FALSE, same_field = FALSE;
	unsigned int win_idx = pq_dev->dev_indx;

	if (WARN_ON(!pq_dev) || WARN_ON(!pq_buf)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, pq_dev=0x%lx, pq_buf=0x%lx\n",
			(unsigned long)pq_dev, (unsigned long)pq_buf);
		return -ENXIO;
	}

	if (duplicate_out == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "duplicate_out is NULL!\n");
		return -EINVAL;
	}

	if (meta_srccap == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "meta_srccap is NULL!\n");
		return -EINVAL;
	}

	pq_pdev = dev_get_drvdata(pq_dev->dev);
	idr_info = &pq_dev->display_info.idr;

	if ((idr_info->input_mode == MTK_PQ_INPUT_MODE_SW) ||
		(idr_info->input_mode == MTK_PQ_INPUT_MODE_BYPASS))
		duplicate_out->user_config = TRUE;
	else
		duplicate_out->user_config = FALSE;

	if (!idr_info->first_frame) {
		if (idr_info->input_mode == MTK_PQ_INPUT_MODE_SW) {
			if (pq_pdev->pqcaps.u32Idr_SwMode_Support)
				same_frame =
					(pq_buf->vb.vb2_buf.planes[0].m.fd == idr_info->frame_fd);
			else
				same_frame = (meta_srccap->w_index == idr_info->index);

			if (meta_srccap->interlaced)
				same_field = (meta_srccap->field == idr_info->field);
			else
				same_field = FALSE;

			if (same_frame || same_field)
				duplicate_out->duplicate = TRUE;
			else
				duplicate_out->duplicate = FALSE;
		} else
			duplicate_out->duplicate = FALSE;
	} else {
		duplicate_out->duplicate = FALSE;
		idr_info->first_frame = FALSE;
	}

	pqdev->pq_devs[win_idx]->pq_debug.src_dup_idx[pqdev->pq_devs[win_idx]->pq_debug.idr_idx] =
									meta_srccap->w_index;
	pqdev->pq_devs[win_idx]->pq_debug.idr_dup_idx[pqdev->pq_devs[win_idx]->pq_debug.idr_idx] =
										idr_info->index;
	pqdev->pq_devs[win_idx]->pq_debug.src_dup_field[pqdev->pq_devs[win_idx]->pq_debug.idr_idx] =
										meta_srccap->field;
	pqdev->pq_devs[win_idx]->pq_debug.idr_dup_field[pqdev->pq_devs[win_idx]->pq_debug.idr_idx] =
										idr_info->field;
	pqdev->pq_devs[win_idx]->pq_debug.idr_idx++;
	if (pqdev->pq_devs[pq_dev->dev_indx]->pq_debug.idr_idx == Debug_idx_max)
		pqdev->pq_devs[pq_dev->dev_indx]->pq_debug.idr_idx = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
		"idr duplicate info(%d): fd:(%d, %d) w_index:(%u, %u) field:(%u, %u) duplicate=%d interlaced=%d same_frame=%d same_field=%d\n",
		win_idx, pq_buf->vb.vb2_buf.planes[0].m.fd, idr_info->frame_fd,
		meta_srccap->w_index, idr_info->index,
		meta_srccap->field, idr_info->field,
		duplicate_out->duplicate, meta_srccap->interlaced, same_frame, same_field);

	return 0;
}

void _get_color_fmt_info_from_meta(
	struct m_pq_display_idr_ctrl  *meta_idr,
	struct meta_srccap_frame_info *meta_srccap,
	struct format_info *fmt_info,
	enum mtk_pq_input_mode input_mode,
	bool *is_yuv420,
	bool *is_yuv422)
{
	int plane_cnt = 0;

	if ((meta_idr == NULL) || (meta_srccap == NULL)
		|| (fmt_info == NULL))
		return;

	if (input_mode == MTK_PQ_INPUT_MODE_SW) {
		meta_idr->color_fmt_info.fmt =
			(enum m_pqu_idr_format) meta_srccap->color_fmt_info.fmt;
		meta_idr->color_fmt_info.argb = meta_srccap->color_fmt_info.argb;
		meta_idr->color_fmt_info.pack = meta_srccap->color_fmt_info.pack;
		meta_idr->color_fmt_info.num_planes =
			meta_srccap->color_fmt_info.num_planes;

		for (plane_cnt = 0; plane_cnt < MAX_FPLANE_NUM; plane_cnt++) {
			meta_idr->color_fmt_info.bitmode[plane_cnt]
				= meta_srccap->color_fmt_info.bitmode[plane_cnt];
			meta_idr->color_fmt_info.ce[plane_cnt]
				= meta_srccap->color_fmt_info.ce[plane_cnt];
		}

		if ((meta_srccap->color_fmt_info.fmt >= M_MEMOUT_FORMAT_YUV420) &&
			(meta_srccap->color_fmt_info.fmt <= M_MEMOUT_FORMAT_YUV444))
			meta_idr->yuv_mode = TRUE;
		else
			meta_idr->yuv_mode = FALSE;

		*is_yuv420 = (meta_idr->color_fmt_info.fmt == M_IDR_FORMAT_YUV420) ?
				TRUE : FALSE;
		*is_yuv422 = (meta_idr->color_fmt_info.fmt == M_IDR_FORMAT_YUV422) ?
				TRUE : FALSE;
	} else {
		meta_idr->yuv_mode =  fmt_info->yuv_mode;
		*is_yuv420 = _is_yuv_420(fmt_info->meta_fmt);
		*is_yuv422 = _is_yuv_422(fmt_info->meta_fmt);
	}
}

void _get_crop_info_from_meta(s32 *crop_left, s32 *crop_top,
	u32 *crop_width, u32 *crop_height,
	struct meta_srccap_frame_info *meta_srccap_ptr,
	struct meta_pq_display_flow_info *pq_meta_ptr)
{
	if ((crop_left == NULL) || (crop_top == NULL)
		|| (crop_width == NULL) || (crop_height == NULL)
		|| (meta_srccap_ptr == NULL) || (pq_meta_ptr == NULL)) {
		PQ_CHECK_NULLPTR();
		return;
	}

	*crop_left = pq_meta_ptr->idrcrop.x;
	*crop_top = pq_meta_ptr->idrcrop.y;

	if ((pq_meta_ptr->idrcrop.width > meta_srccap_ptr->scaled_width) ||
		(pq_meta_ptr->idrcrop.width == 0))
		*crop_width = meta_srccap_ptr->scaled_width;
	else
		*crop_width = pq_meta_ptr->idrcrop.width;

	if ((pq_meta_ptr->idrcrop.height > meta_srccap_ptr->scaled_height) ||
		(pq_meta_ptr->idrcrop.height == 0))
		*crop_height = meta_srccap_ptr->scaled_height;
	else
		*crop_height = pq_meta_ptr->idrcrop.height;
}
//-----------------------------------------------------------------------------
//  Global Functions
//-----------------------------------------------------------------------------
int mtk_pq_display_idr_queue_setup(struct vb2_queue *vq,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_pq_ctx *pq_ctx = vb2_get_drv_priv(vq);
	struct mtk_pq_device *pq_dev = pq_ctx->pq_dev;
	struct pq_display_idr_info *idr = &pq_dev->display_info.idr;
	int i = 0;

	/* video buffers + meta buffer */
	*num_planes = idr->buffer_num + 1;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
			"[idr] num_planes=%d\n", *num_planes);

	for (i = 0; i < idr->plane_num; ++i) {
		sizes[i] = 1;	// TODO
		alloc_devs[i] = pq_dev->dev;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
		"out size[%d]=%d, alloc_devs[%d]=%ld\n",
		i, sizes[i], i, (unsigned long)alloc_devs[i]);
	}

	/* give meta fd min size */
	sizes[*num_planes - 1] = 1;
	alloc_devs[*num_planes - 1] = pq_dev->dev;

	return 0;
}

int mtk_pq_display_idr_queue_setup_info(struct mtk_pq_device *pq_dev,
	void *bufferctrl)
{
	int ret = 0;
	struct v4l2_pq_g_buffer_info bufferInfo;
	struct pq_display_idr_info *idr = &pq_dev->display_info.idr;
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
	bufferInfo.plane_num = idr->buffer_num + 1;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
			"[idr] plane_num=%d\n", bufferInfo.plane_num);

	for (i = 0; i < idr->plane_num; ++i) {
		bufferInfo.size[i] = 1;	// TODO

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
			"out size[%d]=%d\n", i, bufferInfo.size[i]);
	}

	/* give meta fd min size */
	bufferInfo.size[bufferInfo.plane_num - 1] = 1;

	/* update buffer_num */
	//bufferInfo.buffer_num = pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.num_planes;
	if (idr->input_mode == MTK_PQ_INPUT_MODE_SW)
		bufferInfo.buffer_num = IDR_SW_QUEUE_FRAME_NUM;
	else
		bufferInfo.buffer_num = IDR_HW_QUEUE_FRAME_NUM;

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

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_IDR,
		"[idr] buf_num=%u, dque_count=%u, extra=%u, frame_diff=%u\n",
		bufferInfo.buffer_num, bufferInfo.dque_count,
		pq_dev->extra_frame, frame_diff);

	/* update attri */
	if (idr->buffer_num == 1)
		bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_CONTINUOUS;
	else
		bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_DISCRETE;

	/* update bufferctrl */
	memcpy(bufferctrl, &bufferInfo, sizeof(struct v4l2_pq_g_buffer_info));

	return 0;
}


int mtk_pq_display_idr_set_pix_format(
	struct v4l2_format *fmt,
	struct pq_display_idr_info *idr_info)
{
	if (WARN_ON(!fmt) || WARN_ON(!idr_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, fmt=0x%lx, idr_info=0x%lx\n",
			(unsigned long)fmt, (unsigned long)idr_info);
		return -EINVAL;
	}

	/* IDR handle, save to share mem */
	idr_info->width = fmt->fmt.pix.width;
	idr_info->height = fmt->fmt.pix.height;
	idr_info->mem_fmt = fmt->fmt.pix.pixelformat;
	idr_info->plane_num = 1;

	return 0;
}

int mtk_pq_display_idr_set_pix_format_mplane(
	struct v4l2_format *fmt,
	struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct format_info fmt_info;

	if (WARN_ON(!fmt) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, fmt=0x%lx, idr_info=0x%lx\n",
			(unsigned long)fmt, (unsigned long)pq_dev);
		return -EINVAL;
	}

	memset(&fmt_info, 0, sizeof(fmt_info));

	/* IDR handle, save to share mem */
	pq_dev->display_info.idr.width = fmt->fmt.pix_mp.width;
	pq_dev->display_info.idr.height = fmt->fmt.pix_mp.height;
	pq_dev->display_info.idr.mem_fmt = fmt->fmt.pix_mp.pixelformat;

	/* get fmt info */
	ret = _get_fmt_info(
		fmt->fmt.pix_mp.pixelformat,
		&fmt_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret = %d\n", ret);
		return ret;
	}

	pq_dev->display_info.idr.plane_num = fmt_info.plane_num;
	pq_dev->display_info.idr.buffer_num = fmt_info.buffer_num;

	/* update fmt to user */
	fmt->fmt.pix_mp.num_planes = pq_dev->display_info.idr.buffer_num;

	/* save fmt  */
	memcpy(&pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp,
		&fmt->fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));

	return 0;
}

int mtk_pq_display_idr_set_pix_format_mplane_info(
	struct mtk_pq_device *pq_dev,
	void *bufferctrl)
{
	struct v4l2_pq_s_buffer_info bufferInfo;
	int ret = 0;
	struct format_info fmt_info;

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_s_buffer_info));
	memcpy(&bufferInfo, bufferctrl, sizeof(struct v4l2_pq_s_buffer_info));


	if (WARN_ON(!bufferctrl) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, bufferctrl=0x%lx, pq_dev=0x%lx\n",
			(unsigned long)bufferctrl, (unsigned long)pq_dev);
		return -EINVAL;
	}

	memset(&fmt_info, 0, sizeof(fmt_info));

	/* IDR handle, save to share mem */
	pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.width = bufferInfo.width;
	pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.height = bufferInfo.height;

	/* get fmt info */
	ret = _get_fmt_info(
		pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.pixelformat,
		&fmt_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret = %d\n", ret);
		return ret;
	}

	pq_dev->display_info.idr.width = bufferInfo.width;
	pq_dev->display_info.idr.height = bufferInfo.height;
	pq_dev->display_info.idr.mem_fmt
		= pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.pixelformat;
	pq_dev->display_info.idr.plane_num = fmt_info.plane_num;
	pq_dev->display_info.idr.buffer_num = fmt_info.buffer_num;

	/* update fmt to user */
	pq_dev->pix_format_info.v4l2_fmt_pix_idr_mp.num_planes
		= pq_dev->display_info.idr.buffer_num;

	return 0;
}

int mtk_pq_display_idr_streamon(struct pq_display_idr_info *idr_info, unsigned int win_id)
{
	pqdev->pq_devs[win_id]->pq_debug.idr_idx = 0;
	if (WARN_ON(!idr_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, idr_info=0x%lx\n", (unsigned long)idr_info);
		return -EINVAL;
	}

	/* define default crop size if not set */
	if ((idr_info->crop.left == 0)
		&& (idr_info->crop.top == 0)
		&& (idr_info->crop.width == 0)
		&& (idr_info->crop.height == 0)) {
		idr_info->crop.width = idr_info->width;
		idr_info->crop.height = idr_info->height;
	}

	/* update initial value*/
	idr_info->first_frame = TRUE;

	return 0;
}

int mtk_pq_display_idr_streamoff(struct pq_display_idr_info *idr_info)
{
	if (WARN_ON(!idr_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, idr_info=0x%lx\n", (unsigned long)idr_info);
		return -EINVAL;
	}

	memset(idr_info->fbuf, 0, sizeof(idr_info->fbuf));
	memset(&idr_info->crop, 0, sizeof(idr_info->crop));
	idr_info->mem_fmt = 0;
	idr_info->width = 0;
	idr_info->height = 0;
	idr_info->index = 0;
	idr_info->plane_num = 0;
	idr_info->buffer_num = 0;

	return 0;
}

int mtk_pq_display_idr_s_crop(
	struct pq_display_idr_info *idr_info,
	const struct v4l2_selection *s)
{
	if (WARN_ON(!idr_info) || WARN_ON(!s)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, idr_info=0x%lx, selection=0x%lx\n",
			(unsigned long)idr_info, (unsigned long)s);
		return -EINVAL;
	}

	idr_info->crop.left = s->r.left;
	idr_info->crop.top = s->r.top;
	idr_info->crop.width = s->r.width;
	idr_info->crop.height = s->r.height;

	return 0;
}

int mtk_pq_display_idr_g_crop(
	struct pq_display_idr_info *idr_info,
	struct v4l2_selection *s)
{
	if (WARN_ON(!idr_info) || WARN_ON(!s)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, idr_info=0x%lx, selection=0x%lx\n",
			(unsigned long)idr_info, (unsigned long)s);
		return -EINVAL;
	}

	s->r.left = idr_info->crop.left;
	s->r.top = idr_info->crop.top;
	s->r.width = idr_info->crop.width;
	s->r.height = idr_info->crop.height;

	return 0;
}

int mtk_pq_display_idr_qbuf(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf)
{
	int ret = 0;
	struct mtk_pq_platform_device *pq_pdev = NULL;
	static struct m_pq_display_idr_ctrl meta_idr[MAX_DEVICE_NUM];
	struct meta_pq_display_flow_info *pq_meta_ptr = NULL;
	struct meta_srccap_frame_info *meta_srccap_ptr = NULL;
	struct format_info fmt_info;
	struct pq_display_idr_info *idr_info = NULL;
	struct meta_buffer meta_buf;
	u16 dev_id = 0;
	s32 crop_left = 0, crop_top = 0;
	u32 crop_width = 0, crop_height = 0;
	bool is_yuv420 = false, is_yuv422 = false;
	int plane_cnt = 0;


	if (WARN_ON(!pq_dev) || WARN_ON(!pq_buf)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, pq_dev=0x%lx, pq_buf=0x%lx\n",
			(unsigned long)pq_dev, (unsigned long)pq_buf);
		return -EINVAL;
	}

	pq_pdev = dev_get_drvdata(pq_dev->dev);
	idr_info = &pq_dev->display_info.idr;



	memset(&fmt_info, 0, sizeof(fmt_info));
	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	/* meta buffer handle */
	meta_buf.paddr = pq_buf->meta_buf.va;
	meta_buf.size = pq_buf->meta_buf.size;

	/* read meta from srccap */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_SRCCAP_FRAME_INFO, (void *)&meta_srccap_ptr);
	if (ret || meta_srccap_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_SRCCAP_FRAME_INFO Failed, ret = %d\n", ret);
		return ret;
	}

	dev_id = meta_srccap_ptr->dev_id;

	if (dev_id >= MAX_DEVICE_NUM) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"win_id=%d is over MAX_DEVICE_NUM=%d\n", dev_id, MAX_DEVICE_NUM);
		return -EINVAL;
	}
	memset(&meta_idr[dev_id], 0, sizeof(struct m_pq_display_idr_ctrl));



	/* mapping pixfmt v4l2 -> meta & get format info */
	ret = _get_fmt_info(idr_info->mem_fmt, &fmt_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret=%d\n", ret);
		return ret;
	}

	/* read meta from display flow info */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_DISPLAY_FLOW_INFO, (void *)&pq_meta_ptr);
	if (ret || pq_meta_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_DISPLAY_FLOW_INFO Failed, ret = %d\n", ret);
		return ret;
	}

	/* get meta info from share mem */
	memcpy(meta_idr[dev_id].fbuf, idr_info->fbuf,
		sizeof(meta_idr[dev_id].fbuf));
	meta_idr[dev_id].mem_fmt = fmt_info.meta_fmt;

	_get_color_fmt_info_from_meta(
		&meta_idr[dev_id],
		meta_srccap_ptr,
		&fmt_info,
		pq_dev->display_info.idr.input_mode,
		&is_yuv420,
		&is_yuv422);

	meta_idr[dev_id].width = idr_info->width;
	meta_idr[dev_id].height = idr_info->height;
	meta_idr[dev_id].index = meta_srccap_ptr->w_index;

	_get_crop_info_from_meta(&crop_left, &crop_top, &crop_width, &crop_height,
		meta_srccap_ptr, pq_meta_ptr);

	crop_height = PQ_ALIGN(crop_height, PQ_Y_ALIGN);

	if (meta_srccap_ptr->interlaced == TRUE) {
		crop_top /= PQ_DIVIDE_BASE_2;
		crop_height /= PQ_DIVIDE_BASE_2;
	}

	// x needs to align 2 for yuv 422/420
	if (is_yuv420 || is_yuv422) {
		meta_idr[dev_id].crop.left = PQ_ALIGN(crop_left, PQ_X_ALIGN);
		crop_width = PQ_ALIGN(crop_width, PQ_X_ALIGN);
	} else
		meta_idr[dev_id].crop.left = crop_left;

	// y needs to align 2 for yuv 420
	if (is_yuv420)
		meta_idr[dev_id].crop.top = PQ_ALIGN(crop_top, PQ_Y_ALIGN);
	else
		meta_idr[dev_id].crop.top = crop_top;

	meta_idr[dev_id].crop.width = PQ_ALIGN(crop_width, pq_pdev->display_dev.pre_hvsp_align);
	meta_idr[dev_id].crop.w_align = pq_pdev->display_dev.pre_hvsp_align;

	//Temp for align pq
	//if (_is_yuv_420(fmt_info.meta_fmt))
		meta_idr[dev_id].crop.height = PQ_ALIGN(crop_height, PQ_Y_ALIGN);
	//else
	//	meta_idr.crop.height = crop_height;

	if (meta_srccap_ptr->path == PATH_IPDMA_0)
		meta_idr[dev_id].path = PQU_PATH_IPDMA_0;
	else
		meta_idr[dev_id].path = PQU_PATH_IPDMA_1;

	meta_idr[dev_id].bypass_ipdma = meta_srccap_ptr->bypass_ipdma;
	meta_idr[dev_id].v_flip = false;	//TODO
#if IS_ENABLED(CONFIG_OPTEE)
	meta_idr[dev_id].aid = pq_dev->display_info.secure_info.aid;
#endif
	meta_idr[dev_id].buf_ctrl_mode =
				(enum pqu_idr_buf_ctrl_mode)pq_dev->display_info.idr.input_mode;
	for (plane_cnt = 0; plane_cnt < MAX_FPLANE_NUM; plane_cnt++)
		meta_idr[dev_id].frame_pitch[plane_cnt]
			= meta_srccap_ptr->frame_pitch[plane_cnt];

	meta_idr[dev_id].line_offset = meta_srccap_ptr->hoffset;
	meta_idr[dev_id].sw_mode_supported = pq_pdev->pqcaps.u32Idr_SwMode_Support;
	meta_idr[dev_id].sw_buffer_mode =
				(enum pqu_idr_buf_sw_mode)meta_srccap_ptr->sw_buffer_mode;
	meta_idr[dev_id].field = meta_srccap_ptr->field;
	meta_idr[dev_id].interlaced = meta_srccap_ptr->interlaced;
	meta_idr[dev_id].source = (u16)meta_srccap_ptr->input_port;
	ret = _decide_duplicate_out(pq_dev, pq_buf, meta_srccap_ptr,
					&meta_idr[dev_id].duplicate_out);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret=%d\n", ret);
		return ret;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_IDR,
		"%s(%d) %s=%llu.%llu, %s0x%x, %s%d %d, %s%d %d, %s%d, %s[%d,%d,%d,%d], "
		"%s%d, %s%d, %s%d, %s%d, %s%u, %s%u, %s%u, %s%u, %s%d, %s%d, %s%d, %s%d, "
		"%s%d, %s%d, %s[%d,%d,%d], %s[%d,%d,%d], %s%d, %s%d, %s%d, %s%u, %s%u, %s%u\n",
		"idr w meta:", dev_id,
		"ktimes:", meta_srccap_ptr->ktimes.tv_sec, meta_srccap_ptr->ktimes.tv_usec,
		"fmt = ", meta_idr[dev_id].mem_fmt,
		"w = ", meta_idr[dev_id].width, meta_srccap_ptr->width,
		"h = ", meta_idr[dev_id].height, meta_srccap_ptr->height,
		"index = ", meta_idr[dev_id].index,
		"crop = ", meta_idr[dev_id].crop.left, meta_idr[dev_id].crop.top,
		meta_idr[dev_id].crop.width, meta_idr[dev_id].crop.height,
		"align =", meta_idr[dev_id].crop.w_align,
		"path = ", meta_idr[dev_id].path,
		"bypass_ipdma = ", meta_idr[dev_id].bypass_ipdma,
		"vflip = ", meta_idr[dev_id].v_flip,
		"aid = ", meta_idr[dev_id].aid,
		"buf_ctrl_mode = ", meta_idr[dev_id].buf_ctrl_mode,
		"frame_pitch = ", meta_idr[dev_id].frame_pitch[0],
		"line_offset = ", meta_idr[dev_id].line_offset,
		"duplicate_out.user_config = ", meta_idr[dev_id].duplicate_out.user_config,
		"duplicate_out.duplicate = ", meta_idr[dev_id].duplicate_out.duplicate,
		"interlaced = ", meta_idr[dev_id].interlaced,
		"fmt = ", meta_idr[dev_id].color_fmt_info.fmt,
		"argb = ", meta_idr[dev_id].color_fmt_info.argb,
		"pack = ", meta_idr[dev_id].color_fmt_info.pack,
		"bitmode = ", meta_idr[dev_id].color_fmt_info.bitmode[IDR_BUF_FPLANE_NUM_0],
		meta_idr[dev_id].color_fmt_info.bitmode[IDR_BUF_FPLANE_NUM_1],
		meta_idr[dev_id].color_fmt_info.bitmode[IDR_BUF_FPLANE_NUM_2],
		"ce = ", meta_idr[dev_id].color_fmt_info.ce[IDR_BUF_FPLANE_NUM_0],
		meta_idr[dev_id].color_fmt_info.ce[IDR_BUF_FPLANE_NUM_1],
		meta_idr[dev_id].color_fmt_info.ce[IDR_BUF_FPLANE_NUM_2],
		"yuv_mode = ", meta_idr[dev_id].yuv_mode,
		"old yuv_mode = ", fmt_info.yuv_mode,
		"num_planes = ", meta_idr[dev_id].color_fmt_info.num_planes,
		"frame_id = ", meta_srccap_ptr->frame_id,
		"buf_cnt_id = ", meta_srccap_ptr->buf_cnt_id,
		"input_port = ", meta_srccap_ptr->input_port);

	/* set meta buf info */
	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_DISP_IDR_CTRL, &meta_idr[dev_id]);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"EN_PQ_METATAG_DISP_IDR_CTRL fail, ret=%d\n", ret);
		return ret;
	}

	/*store shm from meta*/
	/*for video calc use*/
	pq_dev->display_info.idr.inputHTT = meta_srccap_ptr->h_total;
	pq_dev->display_info.idr.inputVTT = meta_srccap_ptr->v_total;

	idr_info->index = meta_srccap_ptr->w_index;
	idr_info->frame_fd = pq_buf->vb.vb2_buf.planes[0].m.fd;
	idr_info->field = meta_srccap_ptr->field;

	return 0;
}

