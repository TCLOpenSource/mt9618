// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-----------------------------------------------------------------------------
// Include Files
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
#include <linux/platform_device.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <asm-generic/div64.h>

#include "mtk_srccap.h"
#include "mtk_srccap_dv.h"
#include "mtk_srccap_dv_dma.h"
#include "mtk_srccap_common_ca.h"
#include "mtk_iommu_dtv_api.h"
#include "utpa2_XC.h"

#include "hwreg_srccap_dv_dma.h"

#include "drv_scriptmgt.h"

//-----------------------------------------------------------------------------
// Driver Capability
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------
#define DV_DMA_ALIGN(val, align) (((val) + ((align) - 1)) & (~((align) - 1)))
#define DV_DMA_ACCESS_MODE_444   (0x7)
#define DV_DMA_ACCESS_MODE_422   (0x6)
#define DV_DMA_PLANE_NUM_1       (1)
#define DV_DMA_PLANE_NUM_2       (2)
#define DV_DMA_PLANE_NUM_3       (3)
#define DV_DMA_IDX_0             (0)
#define DV_DMA_IDX_1             (1)
#define DV_DMA_IDX_2             (2)
#define DV_DMA_BIT_WIDTH_0       (0)
#define DV_DMA_BIT_WIDTH_5       (5)
#define DV_DMA_BIT_WIDTH_6       (6)
#define DV_DMA_BIT_WIDTH_8       (8)
#define DV_DMA_BIT_WIDTH_10      (10)
#define DV_DMA_BIT_WIDTH_12      (12)
#define DV_DMA_BIT_WIDTH_ALPHA_0 (0)
#define DV_DMA_BIT_WIDTH_ALPHA_2 (2)
#define DV_DMA_BIT_WIDTH_ALPHA_8 (8)
#define DV_DMA_CE_0              (0)
#define DV_DMA_CE_1              (1)
#define DV_DMA_CE_2              (2)
#define DV_DMA_UHD_WIDTH         (3840)
#define DV_DMA_UHD_HEIGHT        (2160)
#define DV_DMA_FHD_WIDTH         (1920)
#define DV_DMA_FHD_HEIGHT        (1080)

//-----------------------------------------------------------------------------
// Enums and Structures
//-----------------------------------------------------------------------------
#define FMT_NUM        (4)
unsigned int fmt_map_dv[FMT_NUM] = {    //refer to fmt_map/mi_pq_cfd_fmt_map in mi_pq_apply_cfd.c
	V4L2_PIX_FMT_RGB_8_8_8_CE,
	V4L2_PIX_FMT_YUV422_Y_UV_8B_CE,
	V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE,
	V4L2_PIX_FMT_YUV420_Y_UV_8B_CE};

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
static int dv_dma_get_ip_fmt(
	enum v4l2_srccap_input_source src,
	enum srccap_dv_descrb_color_format color_format,
	u8 *ip_fmt)
{
	if (ip_fmt == NULL) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "ip_fmt is NULL\n");
		return -EINVAL;
	}

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		*ip_fmt =
			(color_format == SRCCAP_DV_DESCRB_COLOR_FORMAT_RGB) ?
			REG_SRCCAP_DV_DMA_FORMAT_RGB : REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		*ip_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int dv_dma_get_mem_fmt(
	struct v4l2_format *fmt,
	struct srccap_dv_dma_mem_fmt *mem_fmt)
{
	if ((fmt == NULL) || (mem_fmt == NULL)) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"fmt or mem_fmt is NULL\n");
		return -EINVAL;
	}

	switch (fmt->fmt.pix_mp.pixelformat) {
	/* ------------------ RGB format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_RGB_8_8_8_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_3;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_1;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_RGB565:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_5;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_5;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_RGB888:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_RGB101010:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_RGB121212:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGB;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_ARGB8888:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_ARGB;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_8;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_ABGR8888:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_ABGR;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_8;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_RGBA8888:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGBA;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_8;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_BGRA8888:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_BGRA;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_8;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_ARGB2101010:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_ARGB;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_2;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_ABGR2101010:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_ABGR;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_2;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_RGBA;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_2;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_BGRA;
		mem_fmt->argb = TRUE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_2;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	/* ------------------ YUV format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV444_Y_U_V_12B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_3;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_3;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_3;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_3;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_1;
		break;
	/* 3 plane -- uncontiguous planes */
	/* 2 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV422_Y_UV_12B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_NV16:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_6B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_6B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_NV12:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_6B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_8;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_1;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_6B_CE:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_2;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 0;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_6;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_2;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_YUV444_YUV_12B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV444_YUV_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV444;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_444;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_12B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_12;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV422;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	case V4L2_PIX_FMT_YUV420_YUV_10B:
		mem_fmt->num_planes = DV_DMA_PLANE_NUM_1;
		mem_fmt->num_bufs = 1;
		mem_fmt->pack = 1;
		mem_fmt->fmt_conv.dvdma_fmt = REG_SRCCAP_DV_DMA_FORMAT_YUV420;
		mem_fmt->argb = FALSE;
		mem_fmt->access_mode = DV_DMA_ACCESS_MODE_422;
		mem_fmt->bitmode_alpha = DV_DMA_BIT_WIDTH_ALPHA_0;
		mem_fmt->bitmode[DV_DMA_IDX_0] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_1] = DV_DMA_BIT_WIDTH_10;
		mem_fmt->bitmode[DV_DMA_IDX_2] = DV_DMA_BIT_WIDTH_0;
		mem_fmt->ce[DV_DMA_IDX_0] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_1] = DV_DMA_CE_0;
		mem_fmt->ce[DV_DMA_IDX_2] = DV_DMA_CE_0;
		break;
	default:
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
			"invalid pixel format: %d\n", fmt->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	return 0;
}

static int dv_dma_set_mem_fmt(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 dev_id = 0;
	u8 num_planes = 0;
	u8 plane_cnt = 0;
	u32 align = 0;
	u64 offset = 0;
	struct reg_srccap_dv_dma_format_conversion fmt_conv;
	struct reg_srccap_dv_dma_resolution res;

	memset(&fmt_conv, 0, sizeof(struct reg_srccap_dv_dma_format_conversion));
	memset(&res, 0, sizeof(struct reg_srccap_dv_dma_resolution));

	dev_id = srccap_dev->dev_id;

	/* set memory format */
	fmt_conv.ip_fmt = srccap_dev->dv_info.dma_info.mem_fmt.fmt_conv.ip_fmt;
	fmt_conv.dvdma_fmt = srccap_dev->dv_info.dma_info.mem_fmt.fmt_conv.dvdma_fmt;
	fmt_conv.en = (fmt_conv.ip_fmt != fmt_conv.dvdma_fmt) ? TRUE : FALSE;
	ret = drv_hwreg_srccap_dv_dma_set_format(
			dev_id, fmt_conv, TRUE, NULL, NULL);
	ret |= drv_hwreg_srccap_dv_dma_set_pack(
			dev_id, srccap_dev->dv_info.dma_info.mem_fmt.pack,
			TRUE, NULL, NULL);
	ret |= drv_hwreg_srccap_dv_dma_set_argb(
			dev_id, srccap_dev->dv_info.dma_info.mem_fmt.argb,
			TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set bit mode */
	ret = drv_hwreg_srccap_dv_dma_set_bit_mode(
			dev_id, srccap_dev->dv_info.dma_info.mem_fmt.bitmode,
			TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set compression */
	ret = drv_hwreg_srccap_dv_dma_set_compression(
			dev_id, srccap_dev->dv_info.dma_info.mem_fmt.ce,
			TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set resolution */
	res.hsize = srccap_dev->dv_info.dma_info.mem_fmt.dma_width;
	res.vsize = srccap_dev->dv_info.dma_info.mem_fmt.dma_height;
	ret = drv_hwreg_srccap_dv_dma_set_input_resolution(
		dev_id, res, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set line offset */
	ret = drv_hwreg_srccap_dv_dma_set_line_offset(
		dev_id, res.hsize, TRUE, NULL, NULL); /* unit: pixel */
	ret |= drv_hwreg_srccap_dv_dma_set_frame_pitch(
		dev_id, srccap_dev->dv_info.dma_info.mem_fmt.pitch,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set frame number */
	ret = drv_hwreg_srccap_dv_dma_set_frame_num(
		dev_id, srccap_dev->dv_info.dma_info.mem_fmt.frame_num,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set r/w diff */
	ret = drv_hwreg_srccap_dv_dma_set_rw_diff(
		dev_id, srccap_dev->dv_info.dma_info.mem_fmt.rw_diff,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set based address */
	align = srccap_dev->srccap_info.cap.bpw;
	num_planes = srccap_dev->dv_info.dma_info.mem_fmt.num_planes;
	if (num_planes > SRCCAP_DV_MAX_PLANE_NUM)
		num_planes = SRCCAP_DV_MAX_PLANE_NUM;

	offset = 0;
	for (plane_cnt = 0; plane_cnt < num_planes; plane_cnt++) {
		/* unit: n bit align */
		srccap_dev->dv_info.dma_info.buf.addr_mplane[plane_cnt] =
			(srccap_dev->dv_info.dma_info.buf.addr + offset) *
			SRCCAP_BYTE_SIZE;
		do_div(srccap_dev->dv_info.dma_info.buf.addr_mplane[plane_cnt],
			align);
		offset +=
			srccap_dev->dv_info.dma_info.buf.size_mplane[plane_cnt];
	}

	ret = drv_hwreg_srccap_dv_dma_set_base_addr(
		dev_id, srccap_dev->dv_info.dma_info.buf.addr_mplane,
		TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

exit:
	return ret;
}

static int dv_dma_calculate_buf_size(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct srccap_dv_dma_mem_fmt *mem_fmt = NULL;
	u32 align = 0;
	u8 bitwidth[SRCCAP_DV_MAX_PLANE_NUM] = {0};
	u8 plane_cnt = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma set buf size\n");

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	mem_fmt = &srccap_dev->dv_info.dma_info.mem_fmt;

	/* calculate frame pitch and buffer size */
	align = srccap_dev->srccap_info.cap.bpw;
	if (mem_fmt->pack) {
		bitwidth[0] =
			mem_fmt->bitmode[DV_DMA_IDX_0] +
			mem_fmt->bitmode[DV_DMA_IDX_1] +
			mem_fmt->bitmode[DV_DMA_IDX_2];
		if (mem_fmt->argb)
			bitwidth[0] += mem_fmt->bitmode_alpha;
	} else {
		for (plane_cnt = 0; plane_cnt < SRCCAP_DV_MAX_PLANE_NUM; plane_cnt++)
			bitwidth[plane_cnt] = mem_fmt->bitmode[plane_cnt];
	}

	srccap_dev->dv_info.dma_info.buf.size = 0;
	for (plane_cnt = 0; plane_cnt < SRCCAP_DV_MAX_PLANE_NUM; plane_cnt++) {
		/* unit: n bit align */
		mem_fmt->pitch[plane_cnt] =
			DV_DMA_ALIGN((mem_fmt->dma_width * bitwidth[plane_cnt]), align) *
			mem_fmt->dma_height / align;

		/* unit: byte */
		mem_fmt->frame_pitch[plane_cnt] =
			mem_fmt->pitch[plane_cnt] * align / SRCCAP_BYTE_SIZE;

		/* calculate size, unit: byte */
		srccap_dev->dv_info.dma_info.buf.size_mplane[plane_cnt] =
			mem_fmt->frame_num *
			mem_fmt->frame_pitch[plane_cnt];
		srccap_dev->dv_info.dma_info.buf.size +=
			srccap_dev->dv_info.dma_info.buf.size_mplane[plane_cnt];
	}
	if (srccap_dev->dv_info.dma_info.buf.size == 0)
		goto exit;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"width: %u, height: %u, plane num: %d, buffer num: %d,\n",
		mem_fmt->dma_width,
		mem_fmt->dma_height,
		mem_fmt->num_planes,
		mem_fmt->num_bufs);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"pitch: (%u, %u, %u), frame pitch: (%u, %u, %u)\n",
		mem_fmt->pitch[DV_DMA_IDX_0],
		mem_fmt->pitch[DV_DMA_IDX_1],
		mem_fmt->pitch[DV_DMA_IDX_2],
		mem_fmt->frame_pitch[DV_DMA_IDX_0],
		mem_fmt->frame_pitch[DV_DMA_IDX_1],
		mem_fmt->frame_pitch[DV_DMA_IDX_2]);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"frame num: %d, rw diff: %d, access mode: %d,\n",
		mem_fmt->frame_num,
		mem_fmt->rw_diff,
		mem_fmt->access_mode);
	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"memory format: %d, bitmode: (%d, %d, %d, %d), ce: (%d, %d, %d)\n",
		mem_fmt->fmt_conv.dvdma_fmt,
		mem_fmt->bitmode[DV_DMA_IDX_0],
		mem_fmt->bitmode[DV_DMA_IDX_1],
		mem_fmt->bitmode[DV_DMA_IDX_2],
		mem_fmt->bitmode_alpha,
		mem_fmt->ce[DV_DMA_IDX_0],
		mem_fmt->ce[DV_DMA_IDX_1],
		mem_fmt->ce[DV_DMA_IDX_2]);

exit:
	return ret;
}

static int dv_dma_allocate_buf(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (srccap_dev->dv_info.dma_info.buf.valid) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
			"dma buffer exist\n");
		ret = -EINVAL;
		goto exit;
	}

	if (srccap_dev->dv_info.dma_info.buf.size == 0) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
			"dma buffer size = 0\n");
		ret = -EINVAL;
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"mmap addr: %llx, mmap size: %x, required size: %x\n",
		srccap_dev->dv_info.dma_info.mmap_addr,
		srccap_dev->dv_info.dma_info.mmap_size,
		srccap_dev->dv_info.dma_info.buf.size);

	/* NOT an error if buffer allocation fail */
	if (srccap_dev->dv_info.dma_info.buf.size >
		srccap_dev->dv_info.dma_info.mmap_size) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
			"mmap size not enough\n");
		goto exit;
	}

	/* NOT an error if buffer allocation fail */
	if (srccap_dev->dv_info.dma_info.mmap_addr == 0) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
			"mmap addr = 0\n");
		goto exit;
	}

	/* allocate buffer */
	srccap_dev->dv_info.dma_info.buf.addr =
		(srccap_dev->dv_info.dma_info.mmap_addr -
		 SRCCAP_DV_DMA_BUS2IP_ADDR_OFFSET);

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"va: %llx, pa: %llx\n",
		srccap_dev->dv_info.dma_info.buf.va,
		srccap_dev->dv_info.dma_info.buf.addr);

	/* set memory format */
	ret = dv_dma_set_mem_fmt(srccap_dev);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	srccap_dev->dv_info.dma_info.buf.valid = TRUE;

	if (srccap_dev->dv_info.dma_info.buf.valid &&
		srccap_dev->dv_info.dma_info.secure.svp_id_valid) {
		/* lock dma buffer */
		if (srccap_dev->dv_info.dma_info.secure.svp_buf_authed) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "buffer already locked\n");
			goto already_auth;
		}
		ret = mtk_dv_dma_lock_buffer(
				srccap_dev->dv_info.dma_info.secure.svp_id,
				srccap_dev->dv_info.dma_info.buf.addr,
				srccap_dev->dv_info.dma_info.buf.size);
		if (ret) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"lock buffer fail, return: %d\n", ret);
		} else {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_SVP,
				"lock buffer ok\n");
			srccap_dev->dv_info.dma_info.secure.svp_buf_authed = TRUE;
		}
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"allocate dma buffer done\n");

already_auth:
exit:
	return ret;
}

static int dv_dma_try_allocate_buf(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if ((srccap_dev->dv_info.dma_info.buf.available) &&
		(!srccap_dev->dv_info.dma_info.buf.valid) &&
		(srccap_dev->dv_info.descrb_info.common.interface !=
			SRCCAP_DV_DESCRB_INTERFACE_NONE))
		ret = dv_dma_allocate_buf(srccap_dev);

exit:
	return ret;
}

static int dv_dma_determine_dma_res(int hw_ver, struct srccap_dv_dma_mem_fmt *mem_fmt)
{
	int ret = 0;

	if (mem_fmt == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if ((mem_fmt->src_width > DV_DMA_UHD_WIDTH) ||
		(mem_fmt->src_height > DV_DMA_UHD_HEIGHT)) {
		mem_fmt->dma_width = (mem_fmt->src_width / SRCCAP_DV_UHD_SD_RATIO);
		mem_fmt->dma_height = (mem_fmt->src_height / SRCCAP_DV_UHD_SD_RATIO);
	} else if ((mem_fmt->src_width > DV_DMA_FHD_WIDTH) ||
		(mem_fmt->src_height > DV_DMA_FHD_HEIGHT)) {
		mem_fmt->dma_width = (mem_fmt->src_width / SRCCAP_DV_FHD_SD_RATIO);
		mem_fmt->dma_height = (mem_fmt->src_height / SRCCAP_DV_FHD_SD_RATIO);
	} else {
		mem_fmt->dma_width = mem_fmt->src_width;
		mem_fmt->dma_height = mem_fmt->src_height;
	}

	if ((g_dv_pr_level & SRCCAP_DV_PR_LEVEL_PRE_SD_OFF)
			== SRCCAP_DV_PR_LEVEL_PRE_SD_OFF) {
		mem_fmt->dma_width = mem_fmt->src_width;
		mem_fmt->dma_height = mem_fmt->src_height;
	}

	if ((g_dv_pr_level & SRCCAP_DV_PR_LEVEL_PRE_SD_FORCE)
			== SRCCAP_DV_PR_LEVEL_PRE_SD_FORCE) {
		mem_fmt->dma_width = mem_fmt->src_width / SRCCAP_DV_FHD_SD_RATIO;
		mem_fmt->dma_height = mem_fmt->src_height / SRCCAP_DV_FHD_SD_RATIO;
	}

exit:
	return ret;
}

static int dv_dma_free_buf(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	u8 dev_id = 0;

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	dev_id = srccap_dev->dev_id;

	if (srccap_dev->dv_info.dma_info.buf.valid &&
		srccap_dev->dv_info.dma_info.secure.svp_id_valid) {
		/* unlock dma buffer */
		if (!srccap_dev->dv_info.dma_info.secure.svp_buf_authed) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "buffer already unlocked\n");
			goto already_unauth;
		}
		ret = mtk_dv_dma_unlock_buffer(
				srccap_dev->dv_info.dma_info.buf.addr,
				srccap_dev->dv_info.dma_info.buf.size);
		if (ret) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"unlock buffer fail, return: %d\n", ret);
		} else {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_SVP,
				"unlock buffer ok\n");
			srccap_dev->dv_info.dma_info.secure.svp_buf_authed = FALSE;
		}
	}

	if (srccap_dev->dv_info.dma_info.buf.valid)
		srccap_dev->dv_info.dma_info.buf.valid = FALSE;

	/* set access mode */
	ret = drv_hwreg_srccap_dv_dma_set_access(
		dev_id, 0, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

already_unauth:
exit:
	return ret;
}

static int dv_dma_write_ml_commands(
	struct reg_info reg[],
	u16 count,
	struct srccap_ml_script_info *ml_script_info)
{
	int ret = 0;
	enum sm_return_type ml_ret = 0;
	struct sm_ml_direct_write_mem_info ml_direct_write_info;

	if (ml_script_info == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memset(&ml_direct_write_info, 0, sizeof(struct sm_ml_direct_write_mem_info));

	ml_direct_write_info.reg = (struct sm_reg *)reg;
	ml_direct_write_info.cmd_cnt = count;
	ml_direct_write_info.va_address = ml_script_info->start_addr
			+ (ml_script_info->total_cmd_count * SRCCAP_ML_CMD_SIZE);
	ml_direct_write_info.va_max_address = ml_script_info->max_addr;
	ml_direct_write_info.add_32_support = FALSE;

	ml_ret = sm_ml_write_cmd(&ml_direct_write_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_write_cmd fail, return: %d\n", ml_ret);
	}

	ml_script_info->total_cmd_count += count;

exit:
	return 0;
}

static int dv_dma_fire_ml_commands(
	int ml_fd,
	u8 ml_mem_index,
	struct srccap_ml_script_info *ml_script_info)
{
	int ret = 0;
	enum sm_return_type ml_ret = 0;
	struct sm_ml_fire_info ml_fire_info;

	if (ml_script_info == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memset(&ml_fire_info, 0, sizeof(struct sm_ml_fire_info));

	ml_fire_info.cmd_cnt = ml_script_info->total_cmd_count;
	ml_fire_info.external = FALSE;
	ml_fire_info.mem_index = ml_mem_index;

	if (ml_fire_info.cmd_cnt == 0) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
			"ml cmd count = 0\n");
		goto exit;
	}

	ml_ret = sm_ml_fire(ml_fd, &ml_fire_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_fire fail, return: %d\n", ml_ret);
	}

exit:
	return 0;
}

static int dv_dma_enable_access(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum sm_return_type ml_ret = E_SM_RET_FAIL;
	struct sm_ml_info ml_info;
	struct srccap_ml_script_info ml_script_info;
	struct reg_srccap_dv_dma_force_w_setting setting;
	struct reg_info reg[SRCCAP_DV_REG_NUM] = {{0}};
	struct srccap_memout_buffer_entry *entry = NULL;
	struct list_head *head = NULL;
	u8 ml_mem_index = 0;
	u8 access_mode = 0;
	u16 count = 0;

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* check dma status */
	ret = dv_dma_try_allocate_buf(srccap_dev);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	if (!srccap_dev->dv_info.dma_info.buf.valid)
		goto exit;

	/* get write index */
	head = &(srccap_dev->memout_info.frame_q.inputq_head);
	SRCCAP_GET_LAST_BUF_ENTRY(head, entry);
	if (entry == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* setup menuload */
	memset(&ml_info, 0, sizeof(struct sm_ml_info));
	memset(&ml_script_info, 0, sizeof(struct srccap_ml_script_info));
	memset(&setting, 0, sizeof(struct reg_srccap_dv_dma_force_w_setting));

	ml_ret = sm_ml_get_mem_index(srccap_dev->srccap_info.ml_info.fd_dvdma,
		&ml_mem_index, FALSE);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_get_mem_index fail, return: %d\n", ml_ret);
		goto exit;
	}

	ml_script_info.instance = srccap_dev->srccap_info.ml_info.fd_dvdma;
	ml_script_info.mem_index = ml_mem_index;

	ml_ret = sm_ml_get_info(ml_script_info.instance, ml_script_info.mem_index, &ml_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_get_info fail, return: %d\n", ml_ret);
		goto exit;
	}

	ml_script_info.start_addr = (u64)ml_info.start_va;
	ml_script_info.max_addr = (u64)((u8 *)ml_info.start_va + ml_info.mem_size);
	ml_script_info.mem_size = ml_info.mem_size;

	/* force write index */
	count = 0;
	setting.index = entry->buf_cnt_id;
	setting.mode = 0;
	setting.en = TRUE;
	ret = drv_hwreg_srccap_dv_dma_force_w_index(srccap_dev->dev_id,
			&setting, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	ret = dv_dma_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* set access mode */
	count = 0;
	access_mode = srccap_dev->dv_info.dma_info.mem_fmt.access_mode;
	ret = drv_hwreg_srccap_dv_dma_set_access(srccap_dev->dev_id,
			access_mode, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	ret = dv_dma_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* fire menuload */
	ret = dv_dma_fire_ml_commands(srccap_dev->srccap_info.ml_info.fd_dvdma,
			ml_mem_index, &ml_script_info);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	srccap_dev->dv_info.dma_info.dma_enable = TRUE;

exit:
	return ret;
}

static int dv_dma_disable_access(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum sm_return_type ml_ret = E_SM_RET_FAIL;
	struct sm_ml_info ml_info;
	struct srccap_ml_script_info ml_script_info;
	struct reg_info reg[SRCCAP_DV_REG_NUM] = {{0}};
	u8 ml_mem_index = 0;
	u16 count = 0;

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (srccap_dev->dv_info.dma_info.dma_enable == FALSE)
		goto exit;

	/* setup menuload */
	memset(&ml_info, 0, sizeof(struct sm_ml_info));
	memset(&ml_script_info, 0, sizeof(struct srccap_ml_script_info));

	ml_ret = sm_ml_get_mem_index(srccap_dev->srccap_info.ml_info.fd_dvdma,
		&ml_mem_index, FALSE);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_get_mem_index fail, return: %d\n", ml_ret);
		goto exit;
	}

	ml_script_info.instance = srccap_dev->srccap_info.ml_info.fd_dvdma;
	ml_script_info.mem_index = ml_mem_index;

	ml_ret = sm_ml_get_info(ml_script_info.instance, ml_script_info.mem_index, &ml_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"sm_ml_get_info fail, return: %d\n", ml_ret);
		goto exit;
	}

	ml_script_info.start_addr = (u64)ml_info.start_va;
	ml_script_info.max_addr = (u64)((u8 *)ml_info.start_va + ml_info.mem_size);
	ml_script_info.mem_size = ml_info.mem_size;

	/* set access mode */
	count = 0;
	ret = drv_hwreg_srccap_dv_dma_set_access(srccap_dev->dev_id,
			FALSE, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	ret = dv_dma_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* fire menuload commands */
	ret = dv_dma_fire_ml_commands(srccap_dev->srccap_info.ml_info.fd_dvdma,
			ml_mem_index, &ml_script_info);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	srccap_dev->dv_info.dma_info.dma_enable = FALSE;

exit:
	return ret;

}

//-----------------------------------------------------------------------------
// Global Functions
//-----------------------------------------------------------------------------
int mtk_dv_dma_init(
	struct srccap_dv_init_in *in,
	struct srccap_dv_init_out *out)
{
	int ret = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma init\n");

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	in->dev->dv_info.dma_info.mmap_size =
		in->mmap_size;
	in->dev->dv_info.dma_info.mmap_addr =
		in->mmap_addr;
	in->dev->dv_info.dma_info.buf.available = FALSE;
	in->dev->dv_info.dma_info.buf.valid = FALSE;
	in->dev->dv_info.dma_info.secure.svp_en = FALSE;
	in->dev->dv_info.dma_info.secure.svp_id_valid = FALSE;
	in->dev->dv_info.dma_info.secure.svp_buf_authed = FALSE;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma init done\n");

exit:
	return ret;
}

int mtk_dv_dma_set_fmt_mplane(
	struct srccap_dv_set_fmt_mplane_in *in,
	struct srccap_dv_set_fmt_mplane_out *out)
{
	int ret = 0;
	enum srccap_dv_descrb_color_format color_format;
	struct srccap_dv_dma_mem_fmt *mem_fmt;
	u32 hw_ver = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma set format\n");

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	color_format = in->dev->dv_info.descrb_info.pkt_info.color_format;
	mem_fmt = &in->dev->dv_info.dma_info.mem_fmt;
	hw_ver = in->dev->srccap_info.cap.u32DV_Srccap_HWVersion;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"color format: %d, pixel format: %x\n",
		color_format,
		in->fmt->fmt.pix_mp.pixelformat);

	/* get memory format */
	ret = dv_dma_get_mem_fmt(in->fmt, mem_fmt);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* get input format */
	ret = dv_dma_get_ip_fmt(
			in->dev->srccap_info.src,
			color_format,
			&(mem_fmt->fmt_conv.ip_fmt));
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* get frame number and rw diff */
	mem_fmt->frame_num = in->frame_num;
	mem_fmt->rw_diff = in->rw_diff;

	/* determine dma resolution */
	mem_fmt->src_width = in->fmt->fmt.pix_mp.width;
	mem_fmt->src_height = in->fmt->fmt.pix_mp.height;

	ret = dv_dma_determine_dma_res(hw_ver, mem_fmt);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* calculate frame pitch and buffer size */
	ret = dv_dma_calculate_buf_size(in->dev);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma set format done\n");

exit:
	return ret;
}

int mtk_dv_dma_reqbufs(
	struct srccap_dv_reqbufs_in *in,
	struct srccap_dv_reqbufs_out *out)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct srccap_dv_dma_mem_fmt *mem_fmt = NULL;

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	srccap_dev = in->dev;
	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	mem_fmt = &srccap_dev->dv_info.dma_info.mem_fmt;

	/* update frame number */
	mem_fmt->frame_num = in->frame_num;

	/* calculate frame pitch and buffer size */
	ret = dv_dma_calculate_buf_size(srccap_dev);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

exit:
	return ret;
}

int mtk_dv_dma_streamon(
	struct srccap_dv_streamon_in *in,
	struct srccap_dv_streamon_out *out)
{
	int ret = 0;
	u32 svp_id = 0;

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	in->dev->dv_info.dma_info.buf.available = in->dma_available;

	in->dev->dv_info.dma_info.secure.svp_en = in->svp_en;
	in->dev->dv_info.dma_info.secure.svp_id_valid = FALSE;
	in->dev->dv_info.dma_info.secure.svp_id = 0;

	/* hdmi case create pipeline */
	if (in->dev->dv_info.dma_info.secure.svp_en) {
		ret = mtk_dv_dma_create_pipeline(in->dev, &svp_id);
		if (ret) {
			in->dev->dv_info.dma_info.secure.svp_id_valid = FALSE;
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"create pipeline fail, return: %d\n", ret);
		} else {
			in->dev->dv_info.dma_info.secure.svp_id_valid = TRUE;
			in->dev->dv_info.dma_info.secure.svp_id = svp_id;
		}
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_SVP,
		"svp en: %d, valid: %d, id: %u\n",
		in->dev->dv_info.dma_info.secure.svp_en,
		in->dev->dv_info.dma_info.secure.svp_id_valid,
		in->dev->dv_info.dma_info.secure.svp_id);

exit:
	return ret;
}

int mtk_dv_dma_streamoff(
	struct srccap_dv_streamoff_in *in,
	struct srccap_dv_streamoff_out *out)
{
	int ret = 0;

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	ret = dv_dma_free_buf(in->dev);
	if (ret < 0) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	/* hdmi case release pipeline */
	if (in->dev->dv_info.dma_info.secure.svp_id_valid) {
		ret = mtk_dv_dma_destroy_pipeline(in->dev,
				in->dev->dv_info.dma_info.secure.svp_id);
		if (ret)
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_SVP,
				"destroy pipeline fail, return: %d\n", ret);
		/* auto pipeline destroy return fail that cause stream off abnormally */
		ret = 0;
		in->dev->dv_info.dma_info.secure.svp_id_valid = FALSE;
	}

exit:
	return ret;
}

int mtk_dv_dma_qbuf(
	struct srccap_dv_qbuf_in *in,
	struct srccap_dv_qbuf_out *out)
{
	int ret = 0;

	if ((in == NULL) || (out == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

exit:
	return ret;
}

int mtk_dv_dma_dqbuf(
	struct srccap_dv_dqbuf_in *in,
	struct srccap_dv_dqbuf_out *out)
{
	int ret = 0;
	struct srccap_memout_buffer_entry *entry = NULL;
	struct list_head *head = NULL;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma dequeue buffer\n");

	if ((in == NULL) || (out == NULL) || (in->dev == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	/* get write index */
	head = &(in->dev->memout_info.frame_q.outputq_head);
	SRCCAP_GET_LAST_BUF_ENTRY(head, entry);
	if (entry == NULL) {
		/* dma not available */
		//SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		in->dev->dv_info.dma_info.w_index = SRCCAP_MEMOUT_MAX_W_INDEX;
		dv_dma_disable_access(in->dev);
	} else
		in->dev->dv_info.dma_info.w_index = entry->buf_cnt_id;

	/* get dma status */
	if ((in->dev->dv_info.dma_info.buf.valid) &&
		(in->dev->dv_info.dma_info.w_index != SRCCAP_MEMOUT_MAX_W_INDEX))
		in->dev->dv_info.dma_info.dma_status = SRCCAP_DV_DMA_STATUS_ENABLE_FB;
	else
		in->dev->dv_info.dma_info.dma_status = SRCCAP_DV_DMA_STATUS_DISABLE;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_PR,
		"write index: %d, dma status: %d\n",
		in->dev->dv_info.dma_info.w_index,
		in->dev->dv_info.dma_info.dma_status);

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma dequeue buffer done\n");

exit:
	return ret;
}

void mtk_dv_dma_enable_w_dma_cb(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"dma enable write dma callback\n");

	if (param == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = dv_dma_enable_access(srccap_dev);
	if (ret) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"dma enable write dma callback done\n");

exit:
	return;
}

void mtk_dv_dma_disable_w_dma_cb(void *param)
{
	int ret = 0;
	struct mtk_srccap_dev *srccap_dev = NULL;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"dma disable write dma callback\n");

	if (param == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	ret = dv_dma_disable_access(srccap_dev);
	if (ret) {
		SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"dma disable write dma callback done\n");

exit:
	return;
}

int mtk_dv_dma_create_pipeline(struct mtk_srccap_dev *srccap_dev, u32 *dv_ppl_id)
{
	int ret = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Session *pstSession = NULL;
	TEEC_Operation op = {0};
	u32 error = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma create svp pipeline\n");

	if ((srccap_dev == NULL) || (dv_ppl_id == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));

	optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
	optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
	optee_handler.enAID = srccap_dev->memout_info.secure_info.aid;
	pstSession = srccap_dev->memout_info.secure_info.pstSession;

	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	ret = mtk_common_ca_teec_invoke_cmd(srccap_dev, pstSession,
			(u32)E_XC_OPTEE_CREATE_DV_PIPELINE, &op, &error);
	if (ret) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "smc call CREATE_DV_PIPELINE fail\n");
		return ret;
	}

	*dv_ppl_id = optee_handler.u32DVPipelineID;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO,
		"dma create dv pipeline done, ppl id = 0x%x\n", *dv_ppl_id);

exit:
	return ret;
}

int mtk_dv_dma_destroy_pipeline(struct mtk_srccap_dev *srccap_dev, u32 dv_ppl_id)
{
	int ret = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Session *pstSession = NULL;
	TEEC_Operation op = {0};
	u32 error = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma destroy svp pipeline\n");

	if (srccap_dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));

	optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
	optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
	optee_handler.u32DVPipelineID = dv_ppl_id;
	optee_handler.enAID = srccap_dev->memout_info.secure_info.aid;
	pstSession = srccap_dev->memout_info.secure_info.pstSession;

	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
	op.params[SRCCAP_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	ret = mtk_common_ca_teec_invoke_cmd(srccap_dev, pstSession,
			(u32)E_XC_OPTEE_DESTROY_DV_PIPELINE, &op, &error);
	if (ret) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "smc call DESTROY_DV_PIPELINE fail\n");
		return ret;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma destroy dv pipeline done\n");

exit:
	return ret;
}

int mtk_dv_dma_lock_buffer(u32 dv_ppl_id, unsigned long long buf_pa, unsigned int buf_size)
{
	int ret = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma lock buffer, pa=0x%llx, size=%u\n",
		buf_pa, buf_size);

	if ((!buf_pa) || (!buf_size)) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "Received invalid buffer!\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = mtkd_iommu_buffer_authorize((SRCCAP_DV_BUF_TAG << SRCCAP_DV_BUF_TAG_SHIFT),
			buf_pa, buf_size, dv_ppl_id);
	if (ret) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "mtkd_iommu_buffer_authorize fail\n");
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma lock buffer done\n");

exit:
	return ret;
}

int mtk_dv_dma_unlock_buffer(unsigned long long buf_pa, unsigned int buf_size)
{
	int ret = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma lock buffer, pa=0x%llx, size=%u\n",
		buf_pa, buf_size);

	if ((!buf_pa) || (!buf_size)) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR, "Received invalid buffer!\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = mtkd_iommu_buffer_unauthorize(buf_pa);
	if (ret) {
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"mtkd_iommu_buffer_unauthorize fail\n");
		goto exit;
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "dma unlock buffer done\n");

exit:
	return ret;
}

//-----------------------------------------------------------------------------
// Debug Functions
//-----------------------------------------------------------------------------
