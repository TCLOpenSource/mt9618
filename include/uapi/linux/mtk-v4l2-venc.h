/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __UAPI_MTK_V4L2_VENC_H__
#define __UAPI_MTK_V4L2_VENC_H__

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include "mtk-v4l2-vcodec.h"

#define V4L2_EVENT_PRIVATE_START				0x08000000

#define V4L2_EVENT_MTK_VCODEC_START	 (V4L2_EVENT_PRIVATE_START + 0x00002000)
#define V4L2_EVENT_MTK_VENC_ERROR	 (V4L2_EVENT_MTK_VCODEC_START + 3)

/*  Mediatek control IDs */
#define V4L2_CID_MPEG_MTK_UFO_MODE \
		(V4L2_CID_MPEG_MTK_BASE+5)
#define V4L2_CID_MPEG_MTK_ENCODE_SCENARIO \
		(V4L2_CID_MPEG_MTK_BASE+6)
#define V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE \
		(V4L2_CID_MPEG_MTK_BASE+8)
#define V4L2_CID_MPEG_MTK_ENCODE_RFS_ON \
		(V4L2_CID_MPEG_MTK_BASE+9)
#define V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE \
		(V4L2_CID_MPEG_MTK_BASE+10)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP \
		(V4L2_CID_MPEG_MTK_BASE+12)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_ON \
		(V4L2_CID_MPEG_MTK_BASE+13)
#define V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE \
		(V4L2_CID_MPEG_MTK_BASE+14)
#define V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE \
		(V4L2_CID_MPEG_MTK_BASE+15)
#define V4L2_CID_MPEG_MTK_MAX_WIDTH \
		(V4L2_CID_MPEG_MTK_BASE+16)
#define V4L2_CID_MPEG_MTK_MAX_HEIGHT \
		(V4L2_CID_MPEG_MTK_BASE+17)
#define V4L2_CID_MPEG_MTK_VIDEO_QUALITY \
		(V4L2_CID_MPEG_MTK_BASE+18)
#define V4L2_CID_MPEG_MTK_HIER_P_LAYER_COUNT \
		(V4L2_CID_MPEG_MTK_BASE+19)
#define V4L2_CID_MPEG_MTK_AVERAGE_QP \
		(V4L2_CID_MPEG_MTK_BASE+20)

struct v4l2_venc_color_desc {
	__u32   color_description;
	__u32   color_primaries;
	__u32   transform_character;
	__u32   matrix_coeffs;
	__u32   display_primaries_x[3];
	__u32   display_primaries_y[3];
	__u32   white_point_x;
	__u32   white_point_y;
	__u32   max_display_mastering_luminance;
	__u32   min_display_mastering_luminance;
	__u32   max_content_light_level;
	__u32   max_pic_light_level;
	__u32   full_range;
	__u32   hdr_type;
} __attribute__((packed));

#endif	// #ifndef __UAPI_MTK_V4L2_VENC_H__