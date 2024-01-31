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
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <uapi/linux/mtk_iommu_dtv_api.h>
#include <uapi/linux/metadata_utility/m_vdec.h>
#include <uapi/linux/metadata_utility/m_dip.h>
#include <uapi/linux/mtk_dip.h>
#include <asm-generic/div64.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

#include "mtk_dip_priv.h"
#include "mtk_dip_svc.h"
#include "metadata_utility.h"
#include "mtk_iommu_dtv_api.h"
#include "mapi_pq_cfd_if.h"

#define DIP_ERR(args...)	pr_err("[DIP ERR]" args)
#define DIP_WARNING(args...)	pr_warn("[DIP WARNING]" args)
#define DIP_INFO(args...)	pr_info("[DIP INFO]" args)
#define DIP_DBG_API(args...)	\
do { \
	switch (dev->variant->u16DIPIdx) { \
	case DIP0_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_API) \
			pr_crit("[DIP0 DBG]" args); \
		break; \
	case DIP1_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_API) \
			pr_crit("[DIP1 DBG]" args); \
		break; \
	case DIP2_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_API) \
			pr_crit("[DIP2 DBG]" args); \
		break; \
	case DIPR_BLEND_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_API) \
			pr_crit("[DIPR)BELND DBG]" args); \
		break; \
	default: \
		break; \
	} \
} while (0)

#define DIP_DBG_BUF(args...)	\
do { \
	switch (dev->variant->u16DIPIdx) { \
	case DIP0_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_BUF) \
			pr_crit("[DIP0 DBG]" args); \
		break; \
	case DIP1_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_BUF) \
			pr_crit("[DIP1 DBG]" args); \
		break; \
	case DIP2_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_BUF) \
			pr_crit("[DIP2 DBG]" args); \
		break; \
	case DIPR_BLEND_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_BUF) \
			pr_crit("[DIPR)BELND DBG]" args); \
		break; \
	default: \
		break; \
	} \
} while (0)

#define DIP_DBG_QUEUE(args...)	\
do { \
	switch (dev->variant->u16DIPIdx) { \
	case DIP0_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_QUEUE) \
			pr_crit("[DIP0 DBG]" args); \
		break; \
	case DIP1_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_QUEUE) \
			pr_crit("[DIP1 DBG]" args); \
		break; \
	case DIP2_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_QUEUE) \
			pr_crit("[DIP2 DBG]" args); \
		break; \
	case DIPR_BLEND_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_QUEUE) \
			pr_crit("[DIPR)BELND DBG]" args); \
		break; \
	default: \
		break; \
	} \
} while (0)

#define DIP_DBG_IOCTLTIME(args...)	\
do { \
	switch (dev->variant->u16DIPIdx) { \
	case DIP0_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_IOCTLTIME) \
			pr_crit("[DIP0 DBG]" args); \
		break; \
	case DIP1_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_IOCTLTIME) \
			pr_crit("[DIP1 DBG]" args); \
		break; \
	case DIP2_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_IOCTLTIME) \
			pr_crit("[DIP2 DBG]" args); \
		break; \
	case DIPR_BLEND_WIN_IDX: \
		if (gu32DipLogLevel[dev->variant->u16DIPIdx] & LOG_LEVEL_IOCTLTIME) \
			pr_crit("[DIPR)BELND DBG]" args); \
		break; \
	default: \
		break; \
	} \
} while (0)

#define DIP0_WIN_IDX    (0)
#define DIP1_WIN_IDX    (1)
#define DIP2_WIN_IDX    (2)
#define DIPR_BLEND_WIN_IDX    (3)

#define DMA_INPUT     (1)
#define DMA_OUTPUT    (2)

#define BUF_VA_MODE      (0)
#define BUF_PA_MODE      (1)
#define BUF_IOVA_MODE    (2)

#define DEVICE_CAPS  (V4L2_CAP_VIDEO_CAPTURE_MPLANE|\
						V4L2_CAP_VIDEO_OUTPUT_MPLANE|\
						V4L2_CAP_VIDEO_M2M_MPLANE|\
						V4L2_CAP_STREAMING)

#define DEVICE_DIPR_BLEND_CAPS  (V4L2_CAP_VIDEO_OUTPUT_MPLANE|\
						V4L2_CAP_STREAMING)

#define ATTR_MOD     (0600)

#define fh2ctx(__fh) container_of(__fh, struct dip_ctx, fh)

#define SPT_FMT_CNT(x) ARRAY_SIZE(x)

#define DIP_CAPABILITY_SHOW(DIPItem, DIPItemCap)\
{\
	if (ssize > u8Size) {\
		DIP_ERR("Failed to get" DIPItem "\n");\
	} else {\
		ssize += snprintf(buf + ssize, u8Size - ssize, "%d\n", DIPItemCap);\
	} \
}

#define ALIGN_UPTO_32(x)  ((((x) + 31) >> 5) << 5)
#define ALIGN_UPTO_4(x)  ((((x) + 3) >> 2) << 2)

static struct ST_DIP_DINR_BUF gstDiNrBufInfo = {0};
static struct dip_1rnw_info gst1RNWInfo = {0};

static u32 gu32DipLogLevel[E_DIP_IDX_MAX] =  {[0 ... (E_DIP_IDX_MAX-1)] = LOG_LEVEL_OFF};

struct mtk_vb2_buf {
	enum dma_data_direction   dma_dir;
	unsigned long     size;
	unsigned long     paddr;
	atomic_t          refcount;
	struct device	*dev;
	void	*vaddr;
	struct sg_table	*dma_sgt;
	struct dma_buf_attachment	*db_attach;
};

struct dip_cap_dev gDIPcap_capability[E_DIP_IDX_MAX];

static char *b2r_clocks[E_B2R_CLK_MAX] = {
	"top_subdc",
	"top_smi",
	"top_mfdec",
};

static struct timespec64 StartTmpTime = {0};

static struct dip_fmt stDip0FmtList[] = {
	{
		.name = "ABGR32",
		.fourcc = V4L2_PIX_FMT_ABGR32,
	},
	{
		.name = "ARGB32",
		.fourcc = V4L2_PIX_FMT_ARGB32,
	},
	{
		.name = "ABGR32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR32_SWAP,
	},
	{
		.name = "ARGB32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB32_SWAP,
	},
	{
		.name = "ARGB2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB2101010,
	},
	{
		.name = "ABGR2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR2101010,
	},
	{
		.name = "BGR24",
		.fourcc = V4L2_PIX_FMT_BGR24,
	},
	{
		.name = "RGB565X",
		.fourcc = V4L2_PIX_FMT_RGB565X,
	},
	{
		.name = "YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
	},
	{
		.name = "VYUY",
		.fourcc = V4L2_PIX_FMT_VYUY,
	},
	{
		.name = "YVYU",
		.fourcc = V4L2_PIX_FMT_YVYU,
	},
	{
		.name = "UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
	},
	{
		.name = "YUYV_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_10BIT,
	},
	{
		.name = "YVYU_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YVYU_10BIT,
	},
	{
		.name = "UYVY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_UYVY_10BIT,
	},
	{
		.name = "VYUY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_VYUY_10BIT,
	},
	{
		.name = "NV16",
		.fourcc = V4L2_PIX_FMT_NV16,
	},
	{
		.name = "NV61",
		.fourcc = V4L2_PIX_FMT_NV61,
	},
	{
		.name = "8B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE,
	},
	{
		.name = "10B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE,
	},
	{
		.name = "NV12",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "NV21",
		.fourcc = V4L2_PIX_FMT_NV21,
	},
	{
		.name = "MT21S10L",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10L,
	},
	{
		.name = "YV12",
		.fourcc = V4L2_PIX_FMT_YV12,
	},
};

static struct dip_fmt stDip1FmtList[] = {
	{
		.name = "ABGR32",
		.fourcc = V4L2_PIX_FMT_ABGR32,
	},
	{
		.name = "ARGB32",
		.fourcc = V4L2_PIX_FMT_ARGB32,
	},
	{
		.name = "ABGR32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR32_SWAP,
	},
	{
		.name = "ARGB32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB32_SWAP,
	},
	{
		.name = "ARGB2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB2101010,
	},
	{
		.name = "ABGR2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR2101010,
	},
	{
		.name = "BGR24",
		.fourcc = V4L2_PIX_FMT_BGR24,
	},
	{
		.name = "RGB565X",
		.fourcc = V4L2_PIX_FMT_RGB565X,
	},
	{
		.name = "YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
	},
	{
		.name = "VYUY",
		.fourcc = V4L2_PIX_FMT_VYUY,
	},
	{
		.name = "YVYU",
		.fourcc = V4L2_PIX_FMT_YVYU,
	},
	{
		.name = "UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
	},
	{
		.name = "YUYV_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_10BIT,
	},
	{
		.name = "YVYU_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YVYU_10BIT,
	},
	{
		.name = "UYVY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_UYVY_10BIT,
	},
	{
		.name = "VYUY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_VYUY_10BIT,
	},
	{
		.name = "NV16",
		.fourcc = V4L2_PIX_FMT_NV16,
	},
	{
		.name = "NV61",
		.fourcc = V4L2_PIX_FMT_NV61,
	},
	{
		.name = "8B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE,
	},
	{
		.name = "10B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE,
	},
	{
		.name = "NV12",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "NV21",
		.fourcc = V4L2_PIX_FMT_NV21,
	},
	{
		.name = "MT21S10L",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10L,
	},
	{
		.name = "MT21S",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S,
	},
	{
		.name = "MT21S10T",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10T,
	},
	{
		.name = "MT21S10R",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10R,
	},
	{
		.name = "YV12",
		.fourcc = V4L2_PIX_FMT_YV12,
	},
	{
		.name = "MTK_YUYV_ROTATE",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_ROT,
	},
};

static struct dip_fmt stDip2FmtList[] = {
	{
		.name = "ABGR32",
		.fourcc = V4L2_PIX_FMT_ABGR32,
	},
	{
		.name = "ARGB32",
		.fourcc = V4L2_PIX_FMT_ARGB32,
	},
	{
		.name = "ABGR32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR32_SWAP,
	},
	{
		.name = "ARGB32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB32_SWAP,
	},
	{
		.name = "ARGB2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB2101010,
	},
	{
		.name = "ABGR2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR2101010,
	},
	{
		.name = "BGR24",
		.fourcc = V4L2_PIX_FMT_BGR24,
	},
	{
		.name = "RGB565X",
		.fourcc = V4L2_PIX_FMT_RGB565X,
	},
	{
		.name = "YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
	},
	{
		.name = "VYUY",
		.fourcc = V4L2_PIX_FMT_VYUY,
	},
	{
		.name = "YVYU",
		.fourcc = V4L2_PIX_FMT_YVYU,
	},
	{
		.name = "UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
	},
	{
		.name = "YUYV_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_10BIT,
	},
	{
		.name = "YVYU_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YVYU_10BIT,
	},
	{
		.name = "UYVY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_UYVY_10BIT,
	},
	{
		.name = "VYUY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_VYUY_10BIT,
	},
	{
		.name = "NV16",
		.fourcc = V4L2_PIX_FMT_NV16,
	},
	{
		.name = "NV61",
		.fourcc = V4L2_PIX_FMT_NV61,
	},
	{
		.name = "8B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE,
	},
	{
		.name = "10B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE,
	},
	{
		.name = "NV12",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "NV21",
		.fourcc = V4L2_PIX_FMT_NV21,
	},
	{
		.name = "MT21S10L",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10L,
	},
};

static struct dip_fmt stDiprFmtList[] = {
	{
		.name = "ABGR32",
		.fourcc = V4L2_PIX_FMT_ABGR32,
	},
	{
		.name = "ARGB32",
		.fourcc = V4L2_PIX_FMT_ARGB32,
	},
	{
		.name = "ABGR32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR32_SWAP,
	},
	{
		.name = "ARGB32_SWAP",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB32_SWAP,
	},
	{
		.name = "ARGB2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ARGB2101010,
	},
	{
		.name = "ABGR2101010",
		.fourcc = V4L2_PIX_FMT_DIP_ABGR2101010,
	},
	{
		.name = "BGR24",
		.fourcc = V4L2_PIX_FMT_BGR24,
	},
	{
		.name = "RGB565X",
		.fourcc = V4L2_PIX_FMT_RGB565X,
	},
	{
		.name = "YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
	},
	{
		.name = "VYUY",
		.fourcc = V4L2_PIX_FMT_VYUY,
	},
	{
		.name = "YVYU",
		.fourcc = V4L2_PIX_FMT_YVYU,
	},
	{
		.name = "UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
	},
	{
		.name = "YUYV_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_10BIT,
	},
	{
		.name = "YVYU_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_YVYU_10BIT,
	},
	{
		.name = "UYVY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_UYVY_10BIT,
	},
	{
		.name = "VYUY_10BIT",
		.fourcc = V4L2_PIX_FMT_DIP_VYUY_10BIT,
	},
	{
		.name = "NV16",
		.fourcc = V4L2_PIX_FMT_NV16,
	},
	{
		.name = "NV61",
		.fourcc = V4L2_PIX_FMT_NV61,
	},
	{
		.name = "8B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE,
	},
	{
		.name = "10B_BLK_TILE",
		.fourcc = V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE,
	},
	{
		.name = "NV12",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "NV21",
		.fourcc = V4L2_PIX_FMT_NV21,
	},
	{
		.name = "MT21S10L",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10L,
	},
	{
		.name = "MT21S",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S,
	},
	{
		.name = "MT21S10T",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10T,
	},
	{
		.name = "MT21S10R",
		.fourcc = V4L2_PIX_FMT_DIP_MT21S10R,
	},
	{
		.name = "MT21C",
		.fourcc = V4L2_PIX_FMT_MT21C,
	},
	{
		.name = "MTKVSD8x4",
		.fourcc = V4L2_PIX_FMT_DIP_VSD8X4,
	},
	{
		.name = "MTKVSD8x2",
		.fourcc = V4L2_PIX_FMT_DIP_VSD8X2,
	},
	{
		.name = "CODEC_VSD8x4",
		.fourcc = V4L2_PIX_FMT_CODEC_VSD8X4,
	},
	{
		.name = "CODEC_VSD8x2",
		.fourcc = V4L2_PIX_FMT_CODEC_VSD8X2,
	},
	{
		.name = "MTK_YUYV_ROTATE",
		.fourcc = V4L2_PIX_FMT_DIP_YUYV_ROT,
	},
};

static struct dip_frame def_frame = {
	.width      = DIP_DEFAULT_WIDTH,
	.height     = DIP_DEFAULT_HEIGHT,
	.c_width    = DIP_DEFAULT_WIDTH,
	.c_height   = DIP_DEFAULT_HEIGHT,
	.o_width    = 0,
	.o_height   = 0,
	.fmt        = &stDiprFmtList[0],
	.u32OutWidth      = DIP_DEFAULT_WIDTH,
	.u32OutHeight     = DIP_DEFAULT_HEIGHT,
	.colorspace = V4L2_COLORSPACE_REC709,
};

static int _FindMappingDIP(struct dip_dev *dev, EN_DIP_IDX *penDIPIdx)
{
	int ret = 0;

	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX)
		*penDIPIdx = E_DIP_IDX0;
	else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX)
		*penDIPIdx = E_DIP_IDX1;
	else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX)
		*penDIPIdx = E_DIP_IDX2;
	else if (dev->variant->u16DIPIdx == DIPR_BLEND_WIN_IDX)
		*penDIPIdx = E_DIPR_BLEND_IDX0;
	else {
		ret = -ENODEV;
		v4l2_err(&dev->v4l2_dev, "%s,%d, Invalid DIP number\n",
			__func__, __LINE__);
	}

	return ret;
}

static bool _GetDIPWFmtAlignUnit(struct dip_dev *dev,
					u32 fourcc, u16 *pu16SizeAlign, u16 *pu16PitchAlign)
{
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;
	bool bNeedDiv = true;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
		u16SizeAlign = ARGB32_HSIZE_ALIGN;
		u16PitchAlign = ARGB32_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_BGR24:
		u16SizeAlign = RGB24_HSIZE_ALIGN;
		u16PitchAlign = RGB24_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u16SizeAlign = RGB16_HSIZE_ALIGN;
		u16PitchAlign = RGB16_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
		u16SizeAlign = YUV422_10B_HSIZE_ALIGN;
		u16PitchAlign = YUV422_10B_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u16SizeAlign = YUV422_SEP_HSIZE_ALIGN;
		u16PitchAlign = YUV422_SEP_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
		u16SizeAlign = YUV422_8BLKTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV422_8BLKTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		u16SizeAlign = YUV422_10BLKTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV422_10BLKTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		u16SizeAlign = YUV420_LINEAR_HSIZE_ALIGN;
		u16PitchAlign = YUV420_LINEAR_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10L:
		u16SizeAlign = YUV420_10BLINEAR_HSIZE_ALIGN;
		u16PitchAlign = YUV420_10BLINEAR_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
		u16SizeAlign = YUV420_8BTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV420_W_8BTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		u16SizeAlign = YUV420_10BTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV420_W_10BTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_YV12:
		u16SizeAlign = YV12_HSIZE_ALIGN;
		u16PitchAlign = YV12_PITCH_ALIGN;
		bNeedDiv = false;
		break;
	default:
		break;
	}

	*pu16SizeAlign = u16SizeAlign;
	*pu16PitchAlign = u16PitchAlign;

	if (dev->dipcap_dev.u32MiuBus == MIUBUS_128 && bNeedDiv == true) {
		*pu16SizeAlign = u16SizeAlign/MOD2;
		*pu16PitchAlign = u16PitchAlign/MOD2;
	}

	return true;
}

static bool _GetDIPFmtAlignUnit(struct dip_dev *dev,
					u32 fourcc, u16 *pu16SizeAlign, u16 *pu16PitchAlign)
{
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;
	bool bNeedDiv = true;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
		u16SizeAlign = ARGB32_HSIZE_ALIGN;
		u16PitchAlign = ARGB32_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_BGR24:
		u16SizeAlign = RGB24_HSIZE_ALIGN;
		u16PitchAlign = RGB24_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
		u16SizeAlign = RGB16_HSIZE_ALIGN;
		u16PitchAlign = RGB16_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
		u16SizeAlign = YUV422_10B_HSIZE_ALIGN;
		u16PitchAlign = YUV422_10B_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u16SizeAlign = YUV422_SEP_HSIZE_ALIGN;
		u16PitchAlign = YUV422_SEP_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
		u16SizeAlign = YUV422_8BLKTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV422_8BLKTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		u16SizeAlign = YUV422_10BLKTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV422_10BLKTILE_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		u16SizeAlign = YUV420_LINEAR_HSIZE_ALIGN;
		u16PitchAlign = YUV420_LINEAR_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10L:
		u16SizeAlign = YUV420_10BLINEAR_HSIZE_ALIGN;
		u16PitchAlign = YUV420_10BLINEAR_PITCH_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
		u16SizeAlign = YUV420_8BTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV420_R_8BTILE_PITCH_ALIGN;
		bNeedDiv = false;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		u16SizeAlign = YUV420_10BTILE_HSIZE_ALIGN;
		u16PitchAlign = YUV420_R_10BTILE_PITCH_ALIGN;
		bNeedDiv = false;
		break;
	case V4L2_PIX_FMT_MT21C:
		u16SizeAlign = MT21C_HSIZE_ALIGN;
		u16PitchAlign = MT21C_PITCH_ALIGN;
		bNeedDiv = false;
		break;
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
		if (dev->dipcap_dev.u32B2R) {
			u16SizeAlign = VSD_HSIZE_ALIGN;
			u16PitchAlign = VSD_PITCH_ALIGN;
		} else if (dev->dipcap_dev.u32DiprVSD == 1) {
			u16SizeAlign = DIPR_VSD_HSIZE_ALIGN;
			u16PitchAlign = DIPR_VSD_PITCH_ALIGN;
		} else {
			u16SizeAlign = 0;
			u16PitchAlign = 0;
		}
		bNeedDiv = false;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u16SizeAlign = YUV422_ROT_HSIZE_ALIGN;
		u16PitchAlign = YUV422_ROT_PITCH_ALIGN;
		bNeedDiv = false;
		break;
	default:
		break;
	}

	*pu16SizeAlign = u16SizeAlign;
	*pu16PitchAlign = u16PitchAlign;

	if (dev->dipcap_dev.u32MiuBus == MIUBUS_128 && bNeedDiv == true) {
		*pu16SizeAlign = u16SizeAlign/MOD2;
		*pu16PitchAlign = u16PitchAlign/MOD2;
	}

	return true;
}

static void _GetInFmtHeightAlign(u32 fourcc,
	u32 *pu32WriteAlign, u32 *pu32BufAlign)
{
	u32 u132WriteAlign = 0;
	u32 u32BufAlign = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
		u132WriteAlign = 1;
		u32BufAlign = 1;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u132WriteAlign = YUV422_HEIGHT_ALIGN;
		u32BufAlign = YUV422_HEIGHT_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		u132WriteAlign = DIP_ROT_HV_ALIGN;
		u32BufAlign = DIP_ROT_HV_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		u132WriteAlign = YUV420_HEIGHT_ALIGN;
		u32BufAlign = YUV420TL_HEIGHT_BUF_ALIGN;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_MT21C:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
	case V4L2_PIX_FMT_YV12:
		u132WriteAlign = YUV420_HEIGHT_ALIGN;
		u32BufAlign = YUV420_HEIGHT_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u132WriteAlign = YUV422_ROT_HEIGHT_ALIGN;
		u32BufAlign = YUV422_ROT_HEIGHT_ALIGN;
		break;
	default:
		DIP_ERR("[%s]Error:no match format\n", __func__);
		break;
	}

	*pu32WriteAlign = u132WriteAlign;
	*pu32BufAlign = u32BufAlign;
}

static void _GetOutFmtHeightAlign(struct dip_dev *dev,
	u32 fourcc, u32 *pu32WriteAlign, u32 *pu32BufAlign)
{
	u32 u132WriteAlign = 0;
	u32 u32BufAlign = 0;

	if ((dev == NULL) || (pu32WriteAlign == NULL) || (pu32BufAlign == NULL)) {
		DIP_ERR("[%s]pointer is NULL\n", __func__);
		return;
	}

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
		u132WriteAlign = 1;
		u32BufAlign = 1;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u132WriteAlign = YUV422_HEIGHT_ALIGN;
		u32BufAlign = YUV422_HEIGHT_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		u132WriteAlign = DIP_ROT_HV_ALIGN;
		u32BufAlign = DIP_ROT_HV_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
		if (dev->dipcap_dev.u32HvspInW)
			u132WriteAlign = YUV420_HEIGHT_ALIGN;
		else
			u132WriteAlign = YUV420TL_8B_HEIGHT_WRITE_ALIGN;
		u32BufAlign = YUV420TL_HEIGHT_BUF_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		if (dev->dipcap_dev.u32HvspInW)
			u132WriteAlign = YUV420_HEIGHT_ALIGN;
		else
			u132WriteAlign = YUV420TL_10B_HEIGHT_WRITE_ALIGN;
		u32BufAlign = YUV420TL_HEIGHT_BUF_ALIGN;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_MT21C:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
	case V4L2_PIX_FMT_YV12:
		u132WriteAlign = YUV420_HEIGHT_ALIGN;
		u32BufAlign = YUV420_HEIGHT_ALIGN;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u132WriteAlign = YUV422_ROT_HEIGHT_ALIGN;
		u32BufAlign = YUV422_ROT_HEIGHT_ALIGN;
		break;
	default:
		DIP_ERR("[%s]Error:no match format\n", __func__);
		break;
	}

	*pu32WriteAlign = u132WriteAlign;
	*pu32BufAlign = u32BufAlign;
}

static u32 _GetDIPFmtPlaneCnt(u32 fourcc)
{
	u32 u32NPlanes = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
	case V4L2_PIX_FMT_MT21C:
	case V4L2_PIX_FMT_YV12:
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u32NPlanes = 1;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
		u32NPlanes = 2;
		break;
	default:
		u32NPlanes = 1;
		break;
	}

	return u32NPlanes;
}

static u32 _GetDIPWFmtMaxWidth(struct dip_ctx *ctx, u32 fourcc, EN_DIP_IDX enDIPIdx)
{
	struct dip_dev *dev = ctx->dev;
	u32 u32MaxWidth = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u32MaxWidth = DIP_GNRL_FMT_WIDTH_MAX;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		if (dev->dipcap_dev.u32DipwBKTile == 1)
			u32MaxWidth = DIP_GNRL_FMT_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		if (enDIPIdx == E_DIP_IDX0)
			u32MaxWidth = dev->dipcap_dev.u32YUV420LinearWidthMax;
		else if (enDIPIdx == E_DIP_IDX1)
			u32MaxWidth = dev->dipcap_dev.u32YUV420LinearWidthMax;
		else if (enDIPIdx == E_DIP_IDX2)
			u32MaxWidth = dev->dipcap_dev.u32YUV420LinearWidthMax;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		if (dev->dipcap_dev.u32DipwTile == 1)
			u32MaxWidth = dev->dipcap_dev.TileWidthMax;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_YV12:
		if (dev->dipcap_dev.u32DipwYV12 == 1)
			u32MaxWidth = DIP_FHD_FMT_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		if ((dev->dipcap_dev.u32Rotate &
			(ROT90_CAP_SYS_BIT|ROT270_CAP_SYS_BIT)))
			u32MaxWidth = DIPW_YUV422_ROT_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	default:
		u32MaxWidth = 0;
		break;
	}

	return u32MaxWidth;
}

static u32 _GetDIPRFmtMaxWidth(struct dip_ctx *ctx, u32 fourcc)
{
	struct dip_dev *dev = ctx->dev;
	u32 u32MaxWidth = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		u32MaxWidth = DIP_GNRL_FMT_WIDTH_MAX;
		break;
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		if (dev->dipcap_dev.u32DipwBKTile == 1)
			u32MaxWidth = DIP_GNRL_FMT_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S10L:
		u32MaxWidth = dev->dipcap_dev.u32YUV420LinearWidthMax;
		break;
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		u32MaxWidth = dev->dipcap_dev.TileWidthMax;
		break;
	case V4L2_PIX_FMT_MT21C:
		if (dev->dipcap_dev.u32B2R)
			u32MaxWidth = DIP_B2R_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
		if (dev->dipcap_dev.u32B2R)
			u32MaxWidth = DIP_B2R_WIDTH_MAX;
		else if (dev->dipcap_dev.u32DiprVSD == 1)
			u32MaxWidth = DIP_FHD_FMT_WIDTH_MAX;
		else
			u32MaxWidth = 0;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u32MaxWidth = DIPR_YUV422_ROT_WIDTH_MAX;
		break;
	default:
		u32MaxWidth = 0;
		break;
	}

	return u32MaxWidth;
}

static u32 _GetDIPDiDnrMaxWidth(EN_DIP_IDX enDIPIdx)
{
	u32 u32MaxWidth = 0;

	if (enDIPIdx == E_DIP_IDX0)
		u32MaxWidth = DIP0_DI_IN_WIDTH_MAX;
	else if (enDIPIdx == E_DIP_IDX1)
		u32MaxWidth = DIP1_DI_IN_WIDTH_MAX;
	else if (enDIPIdx == E_DIP_IDX2)
		u32MaxWidth = DIP2_DI_IN_WIDTH_MAX;
	else
		u32MaxWidth = 0;

	return u32MaxWidth;
}

static u16 _GetDIPDiDnrWidthAlign(EN_DIP_IDX enDIPIdx)
{
	u32 u32Align = 0;

	if (enDIPIdx == E_DIP_IDX0)
		u32Align = DIP0_DI_IN_WIDTH_ALIGN;
	else if (enDIPIdx == E_DIP_IDX1)
		u32Align = DIP1_DI_IN_WIDTH_ALIGN;
	else if (enDIPIdx == E_DIP_IDX2)
		u32Align = DIP2_DI_IN_WIDTH_ALIGN;
	else
		u32Align = 0;

	return u32Align;
}

static bool _IsSupportRWSepFmt(struct dip_ctx *ctx, u32 fourcc)
{
	struct dip_dev *dev = ctx->dev;
	bool bRet = false;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		bRet = true;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S10L:
		if (dev->dipcap_dev.u32RWSepVer == DIP_RWSEP_V0_VERSION)
			bRet = false;
		else
			bRet = true;
		break;
	case V4L2_PIX_FMT_MT21C:
	default:
		bRet = false;
		break;
	}

	return bRet;
}

bool _IsYUV420TileFormat(MS_U32 u32fourcc)
{
	bool bRet = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		bRet = true;
		break;
	default:
		bRet = false;
		break;
	}

	return bRet;
}

static u32 _GetDIPRWSepWidthAlign(struct dip_ctx *ctx, u32 fourcc)
{
	struct dip_dev *dev = ctx->dev;
	u32 u32RetAlign = 0;
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;

	if (_IsSupportRWSepFmt(ctx, fourcc) == true) {
		if (_IsYUV420TileFormat(fourcc) == true) {
			u32RetAlign = DIP_RWSEP_420TILE_ALIGN;
		} else {
			_GetDIPFmtAlignUnit(dev, fourcc, &u16SizeAlign, &u16PitchAlign);
			u32RetAlign = u16SizeAlign;
		}
	} else {
		u32RetAlign = 0;
	}

	return u32RetAlign;
}

static bool _GetFmtBpp(u32 fourcc, u32 *pu32Dividend, u32 *pu32Divisor)
{
	u16 u16Dividend = 0;
	u16 u16Divisor = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_DIP_ABGR32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB32_SWAP:
	case V4L2_PIX_FMT_DIP_ARGB2101010:
	case V4L2_PIX_FMT_DIP_ABGR2101010:
		u16Dividend = BPP_4BYTE;
		u16Divisor = BPP_NO_MOD;
		break;
	case V4L2_PIX_FMT_BGR24:
		u16Dividend = BPP_3BYTE;
		u16Divisor = BPP_NO_MOD;
		break;
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		u16Dividend = BPP_2BYTE;
		u16Divisor = BPP_NO_MOD;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_MT21C:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
	case V4L2_PIX_FMT_YV12:
		u16Dividend = BPP_1BYTE;
		u16Divisor = BPP_NO_MOD;
		break;
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		u16Dividend = BPP_YUV422_10B_DIV;
		u16Divisor = BPP_YUV422_10B_MOD;
		break;
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
		u16Dividend = BPP_YUV420_10B_DIV;
		u16Divisor = BPP_YUV420_10B_MOD;
		break;
	default:
		break;
	}
	*pu32Dividend = u16Dividend;
	*pu32Divisor = u16Divisor;

	return true;
}

u32 _GetNPlaneBppDivisor(u32 fourcc)
{
	u32 u32Divisor = 0;

	if ((fourcc == V4L2_PIX_FMT_NV16) ||
		(fourcc == V4L2_PIX_FMT_NV61)) {
		u32Divisor = BPP_NO_MOD;
	} else {
		u32Divisor = BPP_2_MOD;
	}

	return u32Divisor;
}

u32 CalFramePixel(u32 fourcc, u32 size)
{
	u32 u32PixelCnt = 0;
	u32 u32Dividend = 0;
	u32 u32Divisor = 0;

	_GetFmtBpp(fourcc, &u32Dividend, &u32Divisor);
	if ((u32Dividend == 0) || (u32Divisor == 0))
		return 0;

	u32PixelCnt = (size*u32Divisor)/u32Dividend;

	return u32PixelCnt;
}

u32 MapPixelToByte(u32 fourcc, u32 u32Pixel)
{
	u32 u32Byte = 0;
	u32 u32Dividend = 0;
	u32 u32Divisor = 0;

	_GetFmtBpp(fourcc, &u32Dividend, &u32Divisor);
	if ((u32Dividend == 0) || (u32Divisor == 0))
		return 0;

	u32Byte = (u32Pixel*u32Dividend)/u32Divisor;

	return u32Byte;
}

u32 MapFramePixelToByte(u32 fourcc, u32 u32Pixel)
{
	u32 u32Byte = 0;
	u32 u32Dividend = 0;
	u32 u32Divisor = 0;
	u32 u32NPlanes = 0;

	_GetFmtBpp(fourcc, &u32Dividend, &u32Divisor);
	if ((u32Dividend == 0) || (u32Divisor == 0))
		return 0;

	u32Byte = (u32Pixel*u32Dividend)/u32Divisor;

	u32NPlanes = _GetDIPFmtPlaneCnt(fourcc);
	if (u32NPlanes > 1) {
		if ((fourcc == V4L2_PIX_FMT_NV16) ||
			(fourcc == V4L2_PIX_FMT_NV61)) {
			u32Byte = u32Byte*BPP_2BYTE;
		} else {
			u32Byte = (u32Byte*BPP_3BYTE)/BPP_2_MOD;
		}
	}

	return u32Byte;
}

static bool _IsCompressFormat(u32 fourcc)
{
	bool bRet = false;

	switch (fourcc) {
	case V4L2_PIX_FMT_MT21C:
	case DIP_PIX_FMT_DIP_MT21CS:
	case DIP_PIX_FMT_DIP_MT21CS10T:
	case DIP_PIX_FMT_DIP_MT21CS10R:
	case DIP_PIX_FMT_DIP_MT21CS10TJ:
	case DIP_PIX_FMT_DIP_MT21CS10RJ:
		bRet = true;
		break;
	default:
		bRet = false;
		break;
	}

	return bRet;
};

static bool _IsYUVFormat(u32 u32fourcc)
{
	bool bRet = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
	case V4L2_PIX_FMT_MT21C:
	case V4L2_PIX_FMT_DIP_MT21CS:
	case V4L2_PIX_FMT_DIP_MT21CS10T:
	case V4L2_PIX_FMT_DIP_MT21CS10R:
	case V4L2_PIX_FMT_DIP_MT21CS10TJ:
	case V4L2_PIX_FMT_DIP_MT21CS10RJ:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
	case V4L2_PIX_FMT_YV12:
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		bRet = TRUE;
		break;
	default:
		bRet = FALSE;
		break;
	}

	return bRet;
}

static bool _IsM2mCase(EN_DIP_SOURCE enDIPSrc)
{
	if ((enDIPSrc == E_DIP_SOURCE_DIPR)
		|| (enDIPSrc == E_DIP_SOURCE_DIPR_MFDEC)
		|| (enDIPSrc == E_DIP_SOURCE_B2R)) {
		return true;
	} else {
		return false;
	}
};

static bool _IsCapCase(EN_DIP_SOURCE enDIPSrc)
{
	if (_IsM2mCase(enDIPSrc) == true)
		return false;
	else
		return true;
};

enum v4l2_ycbcr_encoding _QueryYcbcrEnc(enum v4l2_colorspace colorspace,
	enum v4l2_ycbcr_encoding YCbCrEnc)
{
	enum v4l2_ycbcr_encoding RetYCbCrEnc = V4L2_YCBCR_ENC_DEFAULT;

	if (YCbCrEnc == V4L2_YCBCR_ENC_DEFAULT)
		RetYCbCrEnc = V4L2_MAP_YCBCR_ENC_DEFAULT(colorspace);
	else
		RetYCbCrEnc = YCbCrEnc;

	return RetYCbCrEnc;
}

enum v4l2_quantization _LookUpQuantization(enum v4l2_colorspace colorspace,
	enum v4l2_ycbcr_encoding ycbcr_enc,
	enum v4l2_quantization quantization,
	u8 is_rgb)
{
	enum v4l2_quantization RetQuan = V4L2_QUANTIZATION_DEFAULT;

	if (quantization == V4L2_QUANTIZATION_DEFAULT) {
		RetQuan = V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb,
				colorspace, ycbcr_enc);
	} else {
		RetQuan = quantization;
	}

	return RetQuan;
}

EN_DIP_CSC_ENC _GetDIPCscEnc(enum v4l2_ycbcr_encoding SrcYCbCrEnc,
	enum v4l2_ycbcr_encoding DstYCbCrEnc)
{
	EN_DIP_CSC_ENC enCscEnc = E_DIPCSC_ENC_BT709;
	int Ret = 0;

	switch (SrcYCbCrEnc) {
	case V4L2_YCBCR_ENC_709:
	case V4L2_YCBCR_ENC_XV709:
		if ((DstYCbCrEnc == V4L2_YCBCR_ENC_709) ||
			(DstYCbCrEnc == V4L2_YCBCR_ENC_XV709)) {
			enCscEnc = E_DIPCSC_ENC_BT709;
		} else {
			Ret = (-1);
		}
		break;
	case V4L2_YCBCR_ENC_BT2020:
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		if ((DstYCbCrEnc == V4L2_YCBCR_ENC_BT2020) ||
			(DstYCbCrEnc == V4L2_YCBCR_ENC_BT2020_CONST_LUM)) {
			enCscEnc = E_DIPCSC_ENC_BT2020;
		} else {
			Ret = (-1);
		}
		break;

	case V4L2_YCBCR_ENC_601:
	case V4L2_YCBCR_ENC_XV601:
		if ((DstYCbCrEnc == V4L2_YCBCR_ENC_601) ||
			(DstYCbCrEnc == V4L2_YCBCR_ENC_XV601)) {
			enCscEnc = E_DIPCSC_ENC_BT601;
		} else {
			Ret = (-1);
		}
		break;
	default:
		Ret = (-1);
		break;
	}

	if (Ret != 0)
		enCscEnc = E_DIPCSC_ENC_MAX;

	return enCscEnc;
}

EN_DIP_CSC_RANGE _CalDIPCscRange(enum v4l2_quantization SrcQuan,
		enum v4l2_quantization DstQuan)
{
	EN_DIP_CSC_RANGE enCscRg = E_DIPCSC_RG_F2F;

	if (SrcQuan == V4L2_QUANTIZATION_FULL_RANGE) {
		if (DstQuan == V4L2_QUANTIZATION_FULL_RANGE)
			enCscRg = E_DIPCSC_RG_F2F;
		else
			enCscRg = E_DIPCSC_RG_F2L;
	} else {
		if (DstQuan == V4L2_QUANTIZATION_FULL_RANGE)
			enCscRg = E_DIPCSC_RG_L2F;
		else
			enCscRg = E_DIPCSC_RG_L2L;
	}

	return enCscRg;
}

EN_DIP_CSC_RANGE _GetDIPCscRange(enum v4l2_ycbcr_encoding SrcYCbCrEnc,
	enum v4l2_ycbcr_encoding DstYCbCrEnc,
	enum v4l2_quantization SrcQuan,
	enum v4l2_quantization DstQuan)
{
	EN_DIP_CSC_RANGE enCscRg = E_DIPCSC_RG_F2F;
	int Ret = 0;

	switch (SrcYCbCrEnc) {
	case V4L2_YCBCR_ENC_709:
		if (DstYCbCrEnc == V4L2_YCBCR_ENC_709) {
			enCscRg = _CalDIPCscRange(SrcQuan, DstQuan);
		} else if (DstYCbCrEnc == V4L2_YCBCR_ENC_XV709) {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				enCscRg = E_DIPCSC_RG_L2L;
			else
				enCscRg = E_DIPCSC_RG_L2F;
		} else {
			Ret = (-1);
		}
		break;
	case V4L2_YCBCR_ENC_XV709:
		if (DstYCbCrEnc == V4L2_YCBCR_ENC_709) {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				enCscRg = E_DIPCSC_RG_L2L;
			else
				enCscRg = E_DIPCSC_RG_L2F;
		} else if (DstYCbCrEnc == V4L2_YCBCR_ENC_XV709) {
			enCscRg = E_DIPCSC_RG_L2L;
		} else {
			Ret = (-1);
		}
		break;
	case V4L2_YCBCR_ENC_BT2020:
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		if ((DstYCbCrEnc == V4L2_YCBCR_ENC_BT2020) ||
			(DstYCbCrEnc == V4L2_YCBCR_ENC_BT2020_CONST_LUM)) {
			enCscRg = _CalDIPCscRange(SrcQuan, DstQuan);
		} else {
			Ret = (-1);
		}
		break;
	case V4L2_YCBCR_ENC_601:
		if (DstYCbCrEnc == V4L2_YCBCR_ENC_601) {
			enCscRg = _CalDIPCscRange(SrcQuan, DstQuan);
		} else if (DstYCbCrEnc == V4L2_YCBCR_ENC_XV601) {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				enCscRg = E_DIPCSC_RG_L2L;
			else
				enCscRg = E_DIPCSC_RG_L2F;
		} else {
			Ret = (-1);
		}
		break;
	case V4L2_YCBCR_ENC_XV601:
		if (DstYCbCrEnc == V4L2_YCBCR_ENC_601) {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				enCscRg = E_DIPCSC_RG_L2L;
			else
				enCscRg = E_DIPCSC_RG_L2F;
		} else if (DstYCbCrEnc == V4L2_YCBCR_ENC_XV601) {
			enCscRg = E_DIPCSC_RG_L2L;
		} else {
			Ret = (-1);
		}
		break;
	default:
		Ret = (-1);
		break;
	}

	if (Ret != 0)
		enCscRg = E_DIPCSC_RG_MAX;

	return enCscRg;
}

enum EN_DIP_CLR_SPACE _GetClrSpaceMapping(__u8 u8ClrColorSpace, __u8 u8ClrMatrixCoef)
{
	enum EN_DIP_CLR_SPACE enClrSpace = E_DIP_CLR_SPACE_BT709;
	enum EN_PQ_CFD_CFIO_CP enCP = (enum EN_PQ_CFD_CFIO_CP)u8ClrColorSpace;
	enum EN_PQ_CFD_CFIO_MC enMC = (enum EN_PQ_CFD_CFIO_MC)u8ClrMatrixCoef;

	switch (enCP) {
	case E_PQ_CFD_CFIO_CP_BT709_SRGB_SYCC:
		if (enMC == E_PQ_CFD_CFIO_MC_BT601625_XVYCC601_SYCC)
			enClrSpace = E_DIP_CLR_SPACE_BT601;
		else
			enClrSpace = E_DIP_CLR_SPACE_BT709;
		break;
	case E_PQ_CFD_CFIO_CP_BT601625:
	case E_PQ_CFD_CFIO_CP_BT601525_SMPTE170M:
		enClrSpace = E_DIP_CLR_SPACE_BT601;
		break;
	case E_PQ_CFD_CFIO_CP_BT2020:
		enClrSpace = E_DIP_CLR_SPACE_BT2020;
		break;
	default:
		enClrSpace = E_DIP_CLR_SPACE_BT709;
		break;
	}

	return enClrSpace;
}

int _GetDIPCscMapping(struct dip_ctx *ctx,
	struct ST_SRC_INFO *pstSrcInfo,
	struct dip_frame *pDstFrm,
	struct ST_CSC *pstCSC)
{
	struct dip_frame *pSrcFrm = NULL;
	enum v4l2_ycbcr_encoding SrcYCbCrEnc = V4L2_YCBCR_ENC_DEFAULT;
	enum v4l2_ycbcr_encoding DstYCbCrEnc = V4L2_YCBCR_ENC_DEFAULT;
	enum v4l2_quantization SrcQuan = V4L2_QUANTIZATION_DEFAULT;
	enum v4l2_quantization DstQuan = V4L2_QUANTIZATION_DEFAULT;
	EN_DIP_CSC_ENC enCscEnc = E_DIPCSC_ENC_BT709;
	EN_DIP_CSC_RANGE enCscRg = E_DIPCSC_RG_F2F;
	u8 IsRGB = 0;
	bool bDstIsYUV = false;

	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -EPERM;
	}
	pSrcFrm = &(ctx->in);

	if (_IsYUVFormat(pDstFrm->fmt->fourcc) == true)
		IsRGB = 0;
	else
		IsRGB = 1;
	DstQuan = _LookUpQuantization(pDstFrm->colorspace, DstYCbCrEnc,
			pDstFrm->quantization, IsRGB);

	if ((_IsM2mCase(ctx->enSource)) == true) {
		SrcYCbCrEnc = _QueryYcbcrEnc(pSrcFrm->colorspace, pSrcFrm->ycbcr_enc);
		DstYCbCrEnc = _QueryYcbcrEnc(pDstFrm->colorspace, pDstFrm->ycbcr_enc);

		if (pstSrcInfo->enSrcClr == E_SRC_COLOR_RGB)
			IsRGB = 1;
		else
			IsRGB = 0;
		SrcQuan = _LookUpQuantization(pSrcFrm->colorspace, SrcYCbCrEnc,
				pSrcFrm->quantization, IsRGB);

		enCscEnc = _GetDIPCscEnc(SrcYCbCrEnc, DstYCbCrEnc);
		if (enCscEnc != E_DIPCSC_ENC_MAX)
			pstCSC->enCscEnc = enCscEnc;
		else
			return (-1);

		enCscRg = _GetDIPCscRange(SrcYCbCrEnc, DstYCbCrEnc, SrcQuan, DstQuan);
		if (enCscRg != E_DIPCSC_RG_MAX)
			pstCSC->enCscRg = enCscRg;
		else
			return (-1);
	} else {
		pstCSC->enCscEnc = (EN_DIP_CSC_ENC)pstSrcInfo->enClrSpace;
		if (pstSrcInfo->enClrRange == E_DIP_CLR_RANGE_FULL) {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				pstCSC->enCscRg = E_DIPCSC_RG_F2L;
			else
				pstCSC->enCscRg = E_DIPCSC_RG_F2F;
		} else {
			if (DstQuan == V4L2_QUANTIZATION_LIM_RANGE)
				pstCSC->enCscRg = E_DIPCSC_RG_L2L;
			else
				pstCSC->enCscRg = E_DIPCSC_RG_L2F;
		}
	}

	bDstIsYUV = _IsYUVFormat(pDstFrm->fmt->fourcc);
	if (pstSrcInfo->enSrcClr == E_SRC_COLOR_RGB) {
		if (bDstIsYUV == true) {
			pstCSC->bEnaR2Y = true;
			pstCSC->bEnaY2R = false;
		} else {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = false;
		}
	} else {
		if (bDstIsYUV == true) {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = false;
		} else {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = true;
		}
	}

	return 0;
}

static void _GetB2RAddrAlign(u32 fourcc, __u32 *pu32RetVal, __u32 *pu32RetValB)
{
	u16 u16LumaFB = 0;
	u16 u16ChromaFB = 0;
	u16 u16LumaBL = 0;
	u16 u16ChromaBL = 0;

	if (_IsCompressFormat(fourcc) == true) {
		u16LumaFB = B2R_LUMA_FB_CMPS_ADR_AL;
		u16ChromaFB = B2R_CHRO_FB_CMPS_ADR_AL;
		u16LumaBL = B2R_LUMA_BL_CMPS_ADR_AL;
		u16ChromaBL = B2R_CHRO_BL_CMPS_ADR_AL;
	} else {
		u16LumaFB = B2R_LUMA_FB_CMPS_ADR_AL;
		u16ChromaFB = B2R_CHRO_FB_CMPS_ADR_AL;
	}
	*pu32RetVal = u16ChromaFB << ChromaFB_BITSHIFT | u16LumaFB;
	*pu32RetValB = u16ChromaBL << ChromaBL_BITSHIFT | u16LumaBL;
}

static int _IsAlign(u32 u32Size, u16 u16AlignUnit)
{
	if (u16AlignUnit == 0) {
		DIP_ERR("[%s]Error:u16AlignUnit is zero\n", __func__);
		return (-1);
	}
	if ((u32Size % u16AlignUnit) == 0)
		return 0;
	else
		return (-1);
}

static int _AlignAdjust(u32 *pu32Width, u16 u16SizeAlign)
{
	u32 u32Width = *pu32Width;
	u32 u16Remainder = 0;

	if (u16SizeAlign == 0) {
		DIP_ERR("[%s]Error:u16SizeAlign is zero\n", __func__);
		return (-1);
	}
	u16Remainder = u32Width%u16SizeAlign;
	if (u16Remainder == 0)
		*pu32Width = u32Width;
	else
		*pu32Width = u32Width + u16SizeAlign - u16Remainder;

	return 0;
}

static bool _PitchConvert(u32 fourcc, u16 *pu16Pitch)
{
	u16 u16ConvertPitch = *pu16Pitch;
	u32 u32Dividend = 0;
	u32 u32Divisor = 0;

	_GetFmtBpp(fourcc, &u32Dividend, &u32Divisor);
	if ((u32Dividend == 0) || (u32Divisor == 0))
		return false;

	//bpp = u32Dividend/u32Divisor;
	u16ConvertPitch = (u16ConvertPitch*u32Divisor)/u32Dividend;

	*pu16Pitch = u16ConvertPitch;

	return true;
}

static bool _IsSdcCapCase(EN_DIP_SOURCE enDIPSrc)
{
	if ((enDIPSrc == E_DIP_SOURCE_SRCCAP_MAIN)
		|| (enDIPSrc == E_DIP_SOURCE_SRCCAP_SUB)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_START)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_HDR)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_SHARPNESS)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_SCALING)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_CHROMA_COMPENSAT)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_BOOST)
		|| (enDIPSrc == E_DIP_SOURCE_PQ_COLOR_MANAGER)
		|| (enDIPSrc == E_DIP_SOURCE_B2R_MAIN)) {
		return true;
	} else {
		return false;
	}
};

static bool _IsVSDFormat(u32 u32fourcc)
{
	bool bRet = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
		bRet = true;
		break;
	default:
		bRet = false;
		break;
	}

	return bRet;
}

static bool _IsB2rFormat(struct dip_ctx *ctx, u32 fourcc)
{
	bool bRet = false;

	if (_IsCompressFormat(fourcc) == true) {
		bRet = true;
	} else if (_IsVSDFormat(fourcc) == true) {
		if (ctx->dev->dipcap_dev.u32B2R)
			bRet = true;
		else
			bRet = false;
	} else {
		bRet = false;
	}

	return bRet;
};

static bool _IsB2rPath(struct dip_ctx *ctx, u32 fourcc)
{
	struct dip_dev *dev = NULL;
	bool bRet = false;

	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return false;
	}
	dev = ctx->dev;
	if (dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return false;
	}

	if (ctx->enSource == E_DIP_SOURCE_B2R)
		bRet = true;
	else if (_IsB2rFormat(ctx, fourcc) == true)
		bRet = true;
	else if ((ctx->u8WorkingAid == E_AID_SDC) &&
		(ctx->dev->dipcap_dev.u32SDCCap == SDC_CAP_B2R))
		bRet = true;
	else
		bRet = false;

	return bRet;
};

static bool _IsSrcBufType(enum v4l2_buf_type type)
{
	if ((type == V4L2_BUF_TYPE_VIDEO_OUTPUT) || (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
		return true;
	else
		return false;
};

static bool _IsDstBufType(enum v4l2_buf_type type)
{
	if ((type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
		return true;
	else
		return false;
};

bool IsYUVFormat(MS_U32 u32fourcc)
{
	bool bRet = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_DIP_MT21S:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
	case V4L2_PIX_FMT_DIP_MT21S10L:
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
	case V4L2_PIX_FMT_DIP_VSD8X4:
	case V4L2_PIX_FMT_DIP_VSD8X2:
	case V4L2_PIX_FMT_CODEC_VSD8X4:
	case V4L2_PIX_FMT_CODEC_VSD8X2:
	case V4L2_PIX_FMT_YV12:
	case V4L2_PIX_FMT_DIP_YUYV_ROT:
		bRet = true;
		break;
	default:
		bRet = false;
		break;
	}

	return bRet;
}

bool IsYUV422Format(MS_U32 u32fourcc)
{
	bool bRet = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
	case V4L2_PIX_FMT_DIP_YUYV_10BIT:
	case V4L2_PIX_FMT_DIP_YVYU_10BIT:
	case V4L2_PIX_FMT_DIP_UYVY_10BIT:
	case V4L2_PIX_FMT_DIP_VYUY_10BIT:
		bRet = true;
		break;
	default:
		bRet = false;
		break;
	}

	return bRet;
}

static bool _FmtIsYuv10bTile(__u32 u32fourcc)
{
	bool ret = false;

	switch (u32fourcc) {
	case V4L2_PIX_FMT_DIP_MT21S10T:
	case V4L2_PIX_FMT_DIP_MT21S10R:
	case V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static int _FillCSCInfo(struct ST_CSC *pstCSC, struct dip_frame *pSrcfrm,
	struct ST_DIP_BLENDING_DISP_INFO *pstBlendDispInfo)
{
	bool bSrcYuv = false, bDstYuv = false;
	EN_DIP_CSC_ENC enCscEnc = E_DIPCSC_ENC_BT709;
	enum v4l2_ycbcr_encoding DstYCbCrEnc = V4L2_YCBCR_ENC_DEFAULT;
	enum v4l2_quantization DstQuan = V4L2_QUANTIZATION_DEFAULT;

	if (!pSrcfrm) {
		DIP_ERR("%s, pSrcfrm is NULL\n", __func__);
		return -EINVAL;
	}

	if (!pSrcfrm) {
		DIP_ERR("%s, pSrcfrm is NULL\n", __func__);
		return -EINVAL;
	}

	if (!pstBlendDispInfo) {
		DIP_ERR("%s, pstBlendDispInfo is NULL\n", __func__);
		return -EINVAL;
	}

	bSrcYuv = IsYUVFormat(pSrcfrm->fmt->fourcc);
	bDstYuv = (pstBlendDispInfo->enDstClrFmt == E_CLR_FMT_RGB) ? false : true;

	if (bSrcYuv) {
		if (bDstYuv) {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = false;
		} else {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = true;
		}
	} else {
		if (bDstYuv) {
			pstCSC->bEnaR2Y = true;
			pstCSC->bEnaY2R = false;
		} else {
			pstCSC->bEnaR2Y = false;
			pstCSC->bEnaY2R = false;
		}
	}

	if (pstBlendDispInfo->enDstClrSpace == E_DIP_COLOR_SPACE_BT601)
		DstYCbCrEnc = V4L2_YCBCR_ENC_601;
	else if (pstBlendDispInfo->enDstClrSpace == E_DIP_COLOR_SPACE_BT709)
		DstYCbCrEnc = V4L2_YCBCR_ENC_709;
	else if (pstBlendDispInfo->enDstClrSpace == E_DIP_COLOR_SPACE_BT2020)
		DstYCbCrEnc = V4L2_YCBCR_ENC_BT2020;
	else
		DstYCbCrEnc = V4L2_YCBCR_ENC_709;

	enCscEnc = _GetDIPCscEnc(pSrcfrm->ycbcr_enc, DstYCbCrEnc);
	if (enCscEnc != E_DIPCSC_ENC_MAX)
		pstCSC->enCscEnc = enCscEnc;
	else
		return -EINVAL;

	if (pstBlendDispInfo->enDstClrRange == E_DIP_COLOR_RANGE_LIMIT)
		DstQuan = V4L2_QUANTIZATION_LIM_RANGE;
	else if (pstBlendDispInfo->enDstClrRange == E_DIP_COLOR_RANGE_FULL)
		DstQuan = V4L2_QUANTIZATION_FULL_RANGE;
	else
		DstQuan = V4L2_QUANTIZATION_FULL_RANGE;

	pstCSC->enCscRg = _CalDIPCscRange(pSrcfrm->quantization, DstQuan);

	return 0;
}

static bool _ValidBufType(enum v4l2_buf_type type)
{
	if ((_IsSrcBufType(type) == true) || (_IsDstBufType(type) == true))
		return true;
	else
		return false;
};

static bool _GetSourceSelect(struct dip_ctx *ctx, EN_DIP_SRC_FROM *penSrcFrom)
{
	bool bRet = true;
	EN_DIP_SOURCE enCapSrc = E_DIP_SOURCE_SRCCAP_MAIN;
	struct dip_dev *dev = NULL;

	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -EINVAL;
	}

	dev = ctx->dev;
	if (dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return false;
	}
	enCapSrc = ctx->enSource;

	if (enCapSrc == E_DIP_SOURCE_SRCCAP_MAIN)
		*penSrcFrom = E_DIP_SRC_FROM_SRCCAP_MAIN;
	else if (enCapSrc == E_DIP_SOURCE_SRCCAP_SUB)
		*penSrcFrom = E_DIP_SRC_FROM_SRCCAP_SUB;
	else if (enCapSrc == E_DIP_SOURCE_PQ_START)
		*penSrcFrom = E_DIP_SRC_FROM_PQ_START;
	else if (enCapSrc == E_DIP_SOURCE_PQ_HDR)
		*penSrcFrom = E_DIP_SRC_FROM_PQ_HDR;
	else if (enCapSrc == E_DIP_SOURCE_PQ_SHARPNESS)
		*penSrcFrom = E_DIP_SRC_FROM_PQ_SHARPNESS;
	else {
		switch (enCapSrc) {
		case E_DIP_SOURCE_PQ_SCALING:
			*penSrcFrom = E_DIP_SRC_FROM_PQ_SCALING;
			break;
		case E_DIP_SOURCE_PQ_CHROMA_COMPENSAT:
			*penSrcFrom = E_DIP_SRC_FROM_PQ_CHROMA_COMPENSAT;
			break;
		case E_DIP_SOURCE_PQ_BOOST:
			*penSrcFrom = E_DIP_SRC_FROM_PQ_BOOST;
			break;
		case E_DIP_SOURCE_PQ_COLOR_MANAGER:
			*penSrcFrom = E_DIP_SRC_FROM_PQ_COLOR_MANAGER;
			break;
		case E_DIP_SOURCE_STREAM_ALL_VIDEO:
			*penSrcFrom = E_DIP_SRC_FROM_STREAM_ALL_VIDEO;
			break;
		case E_DIP_SOURCE_STREAM_ALL_VIDEO_OSDB:
			*penSrcFrom = E_DIP_SRC_FROM_STREAM_ALL_VIDEO_OSDB;
			break;
		case E_DIP_SOURCE_STREAM_POST:
			*penSrcFrom = E_DIP_SRC_FROM_STREAM_POST;
			break;
		case E_DIP_SOURCE_OSD:
			*penSrcFrom = E_DIP_SRC_FROM_OSD;
			break;
		case E_DIP_SOURCE_DIPR:
			if (_IsB2rPath(ctx, ctx->in.fmt->fourcc) == true) {
				if (dev->dipcap_dev.u32B2R == 0) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
						__func__, __LINE__);
					return false;
				}
				*penSrcFrom = E_DIP_SRC_FROM_B2R;
			} else if (gst1RNWInfo.u32WriteFlag != E_DIP_1RNW_NONE) {
				*penSrcFrom = E_DIP_SRC_FROM_DIPR_1RNW;
			} else
				*penSrcFrom = E_DIP_SRC_FROM_DIPR;
			break;
		case E_DIP_SOURCE_B2R:
			*penSrcFrom = E_DIP_SRC_FROM_B2R;
			break;
		case E_DIP_SOURCE_STREAM_ALL_VIDEO_OSDB0:
			*penSrcFrom = E_DIP_SRC_FROM_STREAM_ALL_VIDEO_OSDB0;
			break;
		case E_DIP_SOURCE_GOP0:
			*penSrcFrom = E_DIP_SRC_FROM_GOP0;
			break;
		case E_DIP_SOURCE_B2R_MAIN:
			*penSrcFrom = E_DIP_SRC_FROM_B2R_MAIN;
			break;
		default:
			bRet = false;
		}
	}
	return bRet;
};

static bool _GetSourceInfo(struct dip_ctx *ctx, EN_DIP_SRC_FROM enSrcFrom,
	struct ST_SRC_INFO *pstSrcInfo)
{
	u32 u32fourcc = 0;
	bool bRet = true;
	bool bIsYUV = false;
	bool bIsYUV422 = false;
	struct dip_dev *dev = ctx->dev;

	dev = ctx->dev;
	if (dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return false;
	}

	if (enSrcFrom == E_DIP_SRC_FROM_DIPR || enSrcFrom == E_DIP_SRC_FROM_DIPR_1RNW) {
		u32fourcc = ctx->in.fmt->fourcc;
		if (_IsB2rPath(ctx, u32fourcc) == true) {
			if (dev->dipcap_dev.u32B2R == 0) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
				return false;
			}
			pstSrcInfo->u8PixelNum = ctx->dev->dipcap_dev.u32B2rPixelNum;
			pstSrcInfo->enSrcClr = E_SRC_COLOR_YUV444;
		} else {
			bIsYUV = IsYUVFormat(u32fourcc);
			bIsYUV422 = IsYUV422Format(u32fourcc);
			pstSrcInfo->u8PixelNum = ctx->dev->dipcap_dev.u32DiprPixelNum;
			if (bIsYUV == TRUE) {
				if (bIsYUV422 == TRUE)
					pstSrcInfo->enSrcClr = E_SRC_COLOR_YUV422;
				else
					pstSrcInfo->enSrcClr = E_SRC_COLOR_YUV420;
			} else
				pstSrcInfo->enSrcClr = E_SRC_COLOR_RGB;
		}
		pstSrcInfo->u32Width = ctx->in.width;
		pstSrcInfo->u32Height = ctx->in.height;
		pstSrcInfo->u32DisplayWidth = ctx->in.width;
		pstSrcInfo->u32DisplayHeight = ctx->in.height;
	} else if (enSrcFrom == E_DIP_SRC_FROM_B2R) {
		pstSrcInfo->u8PixelNum = ctx->dev->dipcap_dev.u32B2rPixelNum;
		pstSrcInfo->enSrcClr = E_SRC_COLOR_YUV444;
		pstSrcInfo->u32Width = ctx->in.width;
		pstSrcInfo->u32Height = ctx->in.height;
		pstSrcInfo->u32DisplayWidth = ctx->in.width;
		pstSrcInfo->u32DisplayHeight = ctx->in.height;
	} else {
		pstSrcInfo->u8PixelNum = ctx->stSrcInfo.u8PixelNum;
		pstSrcInfo->enSrcClr = ctx->stSrcInfo.enSrcClr;
		pstSrcInfo->u32Width = ctx->stSrcInfo.u32Width;
		pstSrcInfo->u32Height = ctx->stSrcInfo.u32Height;
		pstSrcInfo->u32DisplayWidth = ctx->stSrcInfo.u32DisplayWidth;
		pstSrcInfo->u32DisplayHeight = ctx->stSrcInfo.u32DisplayHeight;
	}
	pstSrcInfo->enClrRange = ctx->stSrcInfo.enClrRange;
	pstSrcInfo->enClrSpace = ctx->stSrcInfo.enClrSpace;

	return bRet;
}

static int _ClkMappingIndex(u64 u64ClockRate, int *pParentClkIdx)
{
	int ret = 0;

	if (u64ClockRate == DIP_B2R_CLK_720MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_720MHZ;
	else if (u64ClockRate == DIP_B2R_CLK_630MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_630MHZ;
	else if (u64ClockRate == DIP_B2R_CLK_360MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_360MHZ;
	else if (u64ClockRate == DIP_B2R_CLK_312MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_312MHZ;
	else if (u64ClockRate == DIP_B2R_CLK_156MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_156MHZ;
	else if (u64ClockRate == DIP_B2R_CLK_144MHZ)
		*pParentClkIdx = E_TOP_SUBDC_CLK_144MHZ;
	else
		ret = (-1);

	return ret;
}

static int _SetB2rClk(struct dip_dev *dev, bool bEnable, u64 u64ClockRate)
{
	struct clk_hw *MuxClkHw;
	struct clk_hw *ParentClkGHw;
	int ret = 0;
	int ParentClkIdx = E_TOP_SUBDC_CLK_720MHZ;
	int Idx = 0;

	if (bEnable == true) {
		for (Idx = 0; Idx < E_B2R_CLK_MAX; Idx++) {
			ret = clk_enable(dev->B2rClk[Idx]);
			if (ret) {
				DIP_ERR("[%s]Enable clock failed %d\n", __func__, ret);
				return ret;
			}
		}
		_ClkMappingIndex(u64ClockRate, &ParentClkIdx);
		//get change_parent clk
		MuxClkHw = __clk_get_hw(dev->B2rClk[E_B2R_CLK_TOP_SUBDC]);
		ParentClkGHw = clk_hw_get_parent_by_index(MuxClkHw, ParentClkIdx);
		if (IS_ERR(ParentClkGHw)) {
			DIP_ERR("[%s]failed to get parent_clk_hw\n", __func__);
			return (-1);
		}
		//set_parent clk
		ret = clk_set_parent(dev->B2rClk[E_B2R_CLK_TOP_SUBDC], ParentClkGHw->clk);
		if (ret) {
			DIP_ERR("[%s]failed to set parent clock\n", __func__);
			return (-1);
		}
	} else {
		for (Idx = 0; Idx < E_B2R_CLK_MAX; Idx++)
			clk_disable(dev->B2rClk[Idx]);
	}

	return 0;
}

static bool _SetDIPClkParent(struct dip_dev *dev, EN_DIP_SRC_FROM enSrcFrom, bool bEnable)
{
	struct clk_hw *MuxClkHw;
	struct clk_hw *ParentClkGHw;
	struct clk *scip_dip_clk;
	int ret = 0;
	int ParentClkIdx = 0;

	if (enSrcFrom != E_DIP_SRC_FROM_DIPR && enSrcFrom != E_DIP_SRC_FROM_DIPR_1RNW) {
		if (dev == NULL)
			return false;
		scip_dip_clk = dev->scip_dip_clk;
		if (bEnable == true) {
			ret = clk_enable(scip_dip_clk);
			if (ret) {
				DIP_ERR("[%s]Enable clock failed %d\n", __func__, ret);
				return false;
			}
		} else {
			clk_disable(scip_dip_clk);
		}


		switch (enSrcFrom) {
		case E_DIP_SRC_FROM_SRCCAP_MAIN:
			ParentClkIdx = CLK_IDX_0;
			break;
		case E_DIP_SRC_FROM_SRCCAP_SUB:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_1;
			else
				ParentClkIdx = CLK_IDX_0;
			break;
		case E_DIP_SRC_FROM_PQ_START:
		case E_DIP_SRC_FROM_PQ_HDR:
		case E_DIP_SRC_FROM_PQ_SHARPNESS:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_2;
			else
				ParentClkIdx = CLK_IDX_0;
			break;
		case E_DIP_SRC_FROM_PQ_SCALING:
		case E_DIP_SRC_FROM_PQ_CHROMA_COMPENSAT:
		case E_DIP_SRC_FROM_PQ_BOOST:
		case E_DIP_SRC_FROM_PQ_COLOR_MANAGER:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_3;
			else
				ParentClkIdx = CLK_IDX_0;
			break;
		case E_DIP_SRC_FROM_STREAM_ALL_VIDEO:
		case E_DIP_SRC_FROM_STREAM_ALL_VIDEO_OSDB:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_5;
			else if (dev->dipcap_dev.u32ClkVer == CLK_VER_3)
				ParentClkIdx = CLK_IDX_2;
			else
				ParentClkIdx = CLK_IDX_1;
			break;
		case E_DIP_SRC_FROM_STREAM_ALL_VIDEO_OSDB0:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_5;
			else  if (dev->dipcap_dev.u32ClkVer == CLK_VER_3)
				ParentClkIdx = CLK_IDX_2;
			else
				ParentClkIdx = CLK_IDX_1;
			break;
		case E_DIP_SRC_FROM_STREAM_POST:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_5;
			else
				ParentClkIdx = CLK_IDX_2;
			break;
		case E_DIP_SRC_FROM_OSD:
		case E_DIP_SRC_FROM_GOP0:
			if (dev->dipcap_dev.u32ClkVer == 1)
				ParentClkIdx = CLK_IDX_4;
			else if (dev->dipcap_dev.u32ClkVer == CLK_VER_4)
				ParentClkIdx = CLK_IDX_1;
			else
				ParentClkIdx = CLK_IDX_0;
			break;
		case E_DIP_SRC_FROM_B2R:
			ParentClkIdx = CLK_IDX_6;
			break;
		case E_DIP_SRC_FROM_B2R_MAIN:
			ParentClkIdx = CLK_IDX_0;
			break;
		default:
			return false;
		}

		//get change_parent clk
		MuxClkHw = __clk_get_hw(scip_dip_clk);
		ParentClkGHw = clk_hw_get_parent_by_index(MuxClkHw, ParentClkIdx);
		if (IS_ERR(ParentClkGHw) || (ParentClkGHw == NULL)) {
			DIP_ERR("[%s]failed to get parent_clk_hw\n", __func__);
			return false;
		}

		//set_parent clk
		ret = clk_set_parent(scip_dip_clk, ParentClkGHw->clk);
		if (ret) {
			DIP_ERR("[%s]failed to set parent clock\n", __func__);
			return false;
		}
	}

	return true;
}

static void scip_clk_put(struct platform_device *pdev, struct dip_dev *dev)
{
	if (IS_ERR(dev->scip_dip_clk)) {
		dev_err(&pdev->dev, "[%s]scip_dip_clk IS_ERR, scip_dip_clk:%p\n",
			__func__, dev->scip_dip_clk);
	} else {
		clk_unprepare(dev->scip_dip_clk);
		devm_clk_put(dev->dev, dev->scip_dip_clk);
		dev->scip_dip_clk = ERR_PTR(-EINVAL);
	}
}

static void b2r_clk_put(struct platform_device *pdev, struct dip_dev *dev)
{
	int i;

	for (i = 0; i < E_B2R_CLK_MAX; i++) {
		dev_err(&pdev->dev, "[%s]clock Index:%d\n", __func__, i);
		if (IS_ERR(dev->B2rClk[i])) {
			dev_err(&pdev->dev, "[%s]IS_ERR Idx:%d\n", __func__, i);
			continue;
		}
		clk_unprepare(dev->B2rClk[i]);
		devm_clk_put(dev->dev, dev->B2rClk[i]);
		dev->B2rClk[i] = ERR_PTR(-EINVAL);
	}
}

static int b2r_clk_get(struct platform_device *pdev, struct dip_dev *dev)
{
	int i, ret;

	for (i = 0; i < E_B2R_CLK_MAX; i++) {
		dev->B2rClk[i] = devm_clk_get(&pdev->dev, b2r_clocks[i]);
		if (IS_ERR(dev->B2rClk[i])) {
			dev_err(&pdev->dev, "[%s]failed devm_clk_get Idx:%d\n", __func__, i);
			ret = PTR_ERR(dev->B2rClk[i]);
			goto b2rClkErr;
		}
		ret = clk_prepare(dev->B2rClk[i]);
		if (ret < 0) {
			dev_err(&pdev->dev, "[%s]failed clk_prepare Idx:%d\n", __func__, i);
			devm_clk_put(dev->dev, dev->B2rClk[i]);
			dev->B2rClk[i] = ERR_PTR(-EINVAL);
			goto b2rClkErr;
		}
	}
	return 0;
b2rClkErr:
	b2r_clk_put(pdev, dev);
	return -ENXIO;
}

static int _SetIoBase(struct platform_device *pdev, MS_U8 u8IdxDts, MS_U8 u8IdxHwreg)
{
	int ret = 0;
	struct resource *res;
	void __iomem *ioDipBase;
	MS_U64 u64V_Addr = 0;

	if (KDrv_DIP_CheckIOBaseInit(u8IdxHwreg) == FALSE) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, u8IdxDts);
		if (!res) {
			dev_err(&pdev->dev, "failed get IORESOURCE_MEM u8IdxDts %d\n", u8IdxDts);
			ret = -ENXIO;
		} else {
			ioDipBase = devm_ioremap_resource(&pdev->dev, res);
			if (IS_ERR(ioDipBase)) {
				dev_err(&pdev->dev, "failed devm_ioremap_resource\n");
				ret = PTR_ERR(ioDipBase);
			} else {
				u64V_Addr = (MS_U64)ioDipBase;
				KDrv_DIP_SetIOBase(u8IdxHwreg,
						res->start, resource_size(res), u64V_Addr);
			}
		}
	}

	return ret;
}

static struct dip_fmt *_FindDipCapMpFmt(struct dip_dev *dev, __u32 pixelformat)
{
	u32 u32FmtCnt = 0;
	unsigned int i;

	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip0FmtList);
		for (i = 0; i < u32FmtCnt; i++) {
			if (stDip0FmtList[i].fourcc == pixelformat)
				return &stDip0FmtList[i];
		}
	} else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip1FmtList);
		for (i = 0; i < u32FmtCnt; i++) {
			if (stDip1FmtList[i].fourcc == pixelformat)
				return &stDip1FmtList[i];
		}
	} else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip2FmtList);
		for (i = 0; i < u32FmtCnt; i++) {
			if (stDip2FmtList[i].fourcc == pixelformat)
				return &stDip2FmtList[i];
		}
	}

	return NULL;
}

static struct dip_fmt *_FindDipOutMpFmt(__u32 pixelformat)
{
	unsigned int i;

	for (i = 0; i < SPT_FMT_CNT(stDiprFmtList); i++) {
		if (stDiprFmtList[i].fourcc == pixelformat)
			return &stDiprFmtList[i];
	}
	return NULL;
}

static struct dip_frame *_GetDIPFrame(struct dip_ctx *ctx,
						enum v4l2_buf_type type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return &ctx->in;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return &ctx->out;
	default:
		return ERR_PTR(-EINVAL);
	}
}

static u32 _GetDIPBufMode(struct dip_ctx *ctx, enum v4l2_buf_type type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return ctx->u32SrcBufMode;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return ctx->u32DstBufMode;
	default:
		return 0;
	}
}

static int _ValidCrop(struct file *file, void *priv, struct v4l2_selection *pstSel)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *f;

	f = _GetDIPFrame(ctx, pstSel->type);
	if (IS_ERR(f))
		return PTR_ERR(f);

	if (pstSel->r.top < 0 || pstSel->r.left < 0) {
		v4l2_err(&dev->v4l2_dev,
			"doesn't support negative values for top & left\n");
		return -EINVAL;
	}

	return 0;
}

static int _dip_common_map_fd(int fd, void **va, u64 *size)
{
	int ret = 0;
	struct dma_buf *db = NULL;

	if ((size == NULL) || (fd == 0)) {
		DIP_ERR("Invalid input, fd=(%d), size is NULL?(%s)\n",
			fd, (size == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}

	if (*va != NULL) {
		kfree(*va);
		*va = NULL;
	}

	db = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(db)) {
		DIP_ERR("dma_buf_get fail, db=%p\n", db);
		return -EPERM;
	}

	*size = db->size;

	ret = dma_buf_begin_cpu_access(db, DMA_BIDIRECTIONAL);
	if (ret < 0) {
		DIP_ERR("dma_buf_begin_cpu_access fail\n");
		dma_buf_put(db);
		return -EPERM;
	}

	*va = dma_buf_vmap(db);

	if (*va == NULL) {
		dma_buf_end_cpu_access(db, DMA_BIDIRECTIONAL);
		DIP_ERR("dma_buf_vmap fail\n");
		dma_buf_put(db);
		return -EPERM;
	}

	dma_buf_put(db);

	return ret;
}

static int _dip_common_unmap_fd(int fd, void *va)
{
	struct dma_buf *db = NULL;

	if ((va == NULL) || (fd == 0)) {
		DIP_ERR("Invalid input, fd=(%d), va is NULL?(%s)\n",
			fd, (va == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}

	db = dma_buf_get(fd);
	if (db == NULL) {
		DIP_ERR("dma_buf_get fail\n");
		return -1;
	}

	dma_buf_vunmap(db, va);
	dma_buf_end_cpu_access(db, DMA_BIDIRECTIONAL);
	dma_buf_put(db);

	return 0;
}

static int _dip_common_get_metaheader(
	enum EN_DIP_MTDT_TAG meta_tag,
	struct meta_header *meta_header)
{
	int ret = 0;

	if (meta_header == NULL) {
		DIP_ERR("Invalid input, meta_header == NULL\n");
		return -EPERM;
	}

	switch (meta_tag) {
	case E_DIP_MTDT_VDEC_FRM_TAG:
		meta_header->size = sizeof(struct vdec_dd_frm_info);
		meta_header->ver = 0;
		meta_header->tag = MTK_VDEC_DD_FRM_INFO_TAG;
		break;
	case E_DIP_MTDT_VDEC_COMPRESS_TAG:
		meta_header->size = sizeof(struct vdec_dd_compress_info);
		meta_header->ver = 0;
		meta_header->tag = MTK_VDEC_DD_COMPRESS_INFO_TAG;
		break;
	case E_DIP_MTDT_VDEC_COLOR_DESC_TAG:
		meta_header->size = sizeof(struct vdec_dd_color_desc);
		meta_header->ver = 0;
		meta_header->tag = MTK_VDEC_DD_COLOR_DESC_TAG;
		break;
	case E_DIP_MTDT_VDEC_SVP_TAG:
		meta_header->size = sizeof(struct vdec_dd_svp_info);
		meta_header->ver = 0;
		meta_header->tag = MTK_VDEC_DD_SVP_INFO_TAG;
		break;
	case E_DIP_MTDT_DIP_FRAME_INFO_TAG:
		meta_header->size = sizeof(struct meta_dip_frame_info);
		meta_header->ver = 0;
		meta_header->tag = M_DIP_IN_FRAME_INFO_META_TAG;
		break;
	default:
		DIP_ERR("Invalid metadata tag:(%d)\n", meta_tag);
		ret = -EPERM;
		break;
	}

	return ret;
}

int dip_common_read_metadata(int meta_fd,
	enum EN_DIP_MTDT_TAG meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (meta_content == NULL) {
		DIP_ERR("Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _dip_common_map_fd(meta_fd, &va, &size);
	if (ret) {
		DIP_ERR("trans fd to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _dip_common_get_metaheader(meta_tag, &header);
	if (ret) {
		DIP_ERR("get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = query_metadata_header(&buffer, &header);
	if (!meta_ret) {
		DIP_INFO("not found metadata header, tag:%d\n", header.tag);
		ret = -EPERM;
		goto out;
	}

	meta_ret = query_metadata_content(&buffer, &header, meta_content);
	if (!meta_ret) {
		DIP_ERR("query metadata content fail\n");
		ret = -EPERM;
		goto out;
	}

out:
	_dip_common_unmap_fd(meta_fd, va);
	return ret;
}

int _GetCapPtType(EN_DIP_SOURCE enDIPSrc, EN_CAPPT_TYPE *penCappt)
{
	EN_CAPPT_TYPE enCappt = E_CAPPT_VDEC;
	int ret = 0;

	switch (enDIPSrc) {
	case E_DIP_SOURCE_DIPR:
	case E_DIP_SOURCE_B2R:
		enCappt = E_CAPPT_VDEC;
		break;
	case E_DIP_SOURCE_SRCCAP_MAIN:
	case E_DIP_SOURCE_SRCCAP_SUB:
	case E_DIP_SOURCE_B2R_MAIN:
		enCappt = E_CAPPT_SRC;
		break;
	case E_DIP_SOURCE_PQ_START:
	case E_DIP_SOURCE_PQ_HDR:
	case E_DIP_SOURCE_PQ_SHARPNESS:
	case E_DIP_SOURCE_PQ_SCALING:
	case E_DIP_SOURCE_PQ_CHROMA_COMPENSAT:
	case E_DIP_SOURCE_PQ_BOOST:
	case E_DIP_SOURCE_PQ_COLOR_MANAGER:
		enCappt = E_CAPPT_PQ;
		break;
	case E_DIP_SOURCE_STREAM_ALL_VIDEO:
	case E_DIP_SOURCE_STREAM_ALL_VIDEO_OSDB:
	case E_DIP_SOURCE_STREAM_ALL_VIDEO_OSDB0:
	case E_DIP_SOURCE_STREAM_POST:
		enCappt = E_CAPPT_RNDR;
		break;
	case E_DIP_SOURCE_OSD:
	case E_DIP_SOURCE_GOP0:
		enCappt = E_CAPPT_OSD;
		break;
	default:
		enCappt = E_CAPPT_VDEC;
		ret = (-1);
		break;
	}
	*penCappt = enCappt;

	return ret;
}

static int _GetVdecMtdtFormat(struct vdec_dd_frm_info *pvdec_frm_md, u32 *pu32fourcc)
{
	struct fmt_modifier *pmodifier = NULL;

	if (pvdec_frm_md == NULL)
		return -EINVAL;

	if (pu32fourcc == NULL)
		return -EINVAL;

	pmodifier = &(pvdec_frm_md->frm_src_info[0].modifier);

	if (pmodifier->compress == 1) {
		if (pvdec_frm_md->bitdepth == 10) {
			if (pmodifier->jump == 1) {
				if (pmodifier->tile == 1)
					*pu32fourcc = DIP_PIX_FMT_DIP_MT21CS10TJ;
				else
					*pu32fourcc = DIP_PIX_FMT_DIP_MT21CS10RJ;
			} else {
				if (pmodifier->tile == 1)
					*pu32fourcc = DIP_PIX_FMT_DIP_MT21CS10T;
				else
					*pu32fourcc = DIP_PIX_FMT_DIP_MT21CS10R;
			}
		} else {
			*pu32fourcc = DIP_PIX_FMT_DIP_MT21CS;
		}
	} else {
		if (pvdec_frm_md->bitdepth == 10) {
			if (pmodifier->tile == 1)
				*pu32fourcc = DIP_PIX_FMT_DIP_MT21S10T;
			else
				*pu32fourcc = DIP_PIX_FMT_DIP_MT21S10R;
		} else {
			*pu32fourcc = DIP_PIX_FMT_DIP_MT21S;
		}
	}

	return 0;
}

static int GetVdecMetadataInfo(struct dip_ctx *ctx, struct ST_DIP_METADATA *pstMetaData)
{
	struct vdec_dd_compress_info *pvdec_cprs = NULL;
	struct vdec_dd_frm_info *pvdec_frm = NULL;
	struct vdec_dd_svp_info *pvdec_svp = NULL;
	struct ST_DIP_B2R_MTDT *pstB2rMtdt = &(ctx->stB2rMtdt);
	struct dip_dev *dev = ctx->dev;
	int ret = 0;
	int MetaFd = 0;
	unsigned int len = 0;

	MetaFd = pstMetaData->meta_fd;

	len = ALIGN_UPTO_4(sizeof(struct vdec_dd_svp_info));
	pvdec_svp = kzalloc(len, GFP_KERNEL);
	ret = dip_common_read_metadata(MetaFd, E_DIP_MTDT_VDEC_SVP_TAG, pvdec_svp);
	if (ret) {
		ctx->u8aid = pvdec_svp->aid;
		ctx->u32pipeline_id = pvdec_svp->pipeline_id;
	} else {
		ctx->u8aid = 0;
		ctx->u32pipeline_id = 0;
	}
	kfree(pvdec_svp);

	len = ALIGN_UPTO_4(sizeof(struct vdec_dd_compress_info));
	pvdec_cprs = kzalloc(len, GFP_KERNEL);
	ret = dip_common_read_metadata(MetaFd, E_DIP_MTDT_VDEC_COMPRESS_TAG, pvdec_cprs);
	if (ret == 0) {
		if ((pvdec_cprs->flag & VDEC_COMPRESS_FLAG_UNCOMPRESS) == 0) {
			len = ALIGN_UPTO_4(sizeof(struct vdec_dd_frm_info));
			pvdec_frm = kzalloc(len, GFP_KERNEL);
			ret = dip_common_read_metadata(MetaFd, E_DIP_MTDT_VDEC_FRM_TAG, pvdec_frm);
			if (ret) {
				v4l2_err(&dev->v4l2_dev,
					"%s,%d, Read E_DIP_MTDT_VDEC_FRM_TAG fail\n",
					__func__, __LINE__);
				kfree(pvdec_cprs);
				kfree(pvdec_frm);
				return ret;
			}
			ret = _GetVdecMtdtFormat(pvdec_frm, &(pstB2rMtdt->u32fourcc));
			if (ret) {
				v4l2_err(&dev->v4l2_dev,
					"%s,%d, Get Vdec Metadata Format fail\n",
					__func__, __LINE__);
				kfree(pvdec_cprs);
				kfree(pvdec_frm);
				return ret;
			}
			pstB2rMtdt->u32BitlenPitch = pvdec_frm->frm_src_info[0].pitch;
			pstB2rMtdt->u64LumaFbOft = pvdec_frm->luma_data_info[0].offset;
			pstB2rMtdt->u32LumaFbSz = pvdec_frm->luma_data_info[0].size;
			pstB2rMtdt->u64ChromaFbOft = pvdec_frm->chroma_data_info[0].offset;
			pstB2rMtdt->u32ChromaFbSz = pvdec_frm->chroma_data_info[0].size;
			pstB2rMtdt->u64LumaBitlenOft = pvdec_cprs->luma_data_info.offset;
			pstB2rMtdt->u32LumaBitlenSz = pvdec_cprs->luma_data_info.size;
			pstB2rMtdt->u64ChromaBitlenOft =
				pvdec_cprs->chroma_data_info.offset;
			pstB2rMtdt->u32ChromaBitlenSz =
				pvdec_cprs->chroma_data_info.size;
			pstB2rMtdt->u32Enable = 1;
		} else {
			pstB2rMtdt->u32Enable = 0;
		}
		kfree(pvdec_frm);
	} else {
		pstB2rMtdt->u32Enable = 0;
	}
	kfree(pvdec_cprs);

	return ret;
}

static int GetMetadataInfo(struct dip_ctx *ctx, struct ST_DIP_METADATA *pstMetaData)
{
	struct dip_dev *dev = ctx->dev;
	EN_CAPPT_TYPE enCappt = E_CAPPT_VDEC;
	int ret = 0;

	if (pstMetaData->u16Enable) {
		ret = _GetCapPtType(ctx->enSource, &enCappt);
		if (ret)
			return ret;
		if (enCappt == E_CAPPT_VDEC) {
			ret = GetVdecMetadataInfo(ctx, pstMetaData);
		} else {
			v4l2_err(&dev->v4l2_dev, "%s,Not Support enSource:%d metadata\n",
				__func__, ctx->enSource);
		}
	} else {
		ctx->u8aid = 0;
		ctx->u32pipeline_id = 0;
	}

	return ret;
}

int _DipB2rChkSize(u32 u32Width, u32 u32Height)
{
	if ((u32Width < DIP_B2R_WIDTH_ALIGN)
		|| (u32Height == 0)) {
		return -EINVAL;
	}

	return 0;
}

int _DipB2rChkTiming(struct ST_B2R_INFO *pstB2rInfo)
{
	if ((pstB2rInfo->u32Fps < DIP_B2R_MIN_FPS)
		|| (pstB2rInfo->u32DeWidth < DIP_B2R_WIDTH_ALIGN)
		|| (pstB2rInfo->u32DeHeight == 0)) {
		return -EINVAL;
	}

	return 0;
}

int _DipB2rGetClk(struct ST_B2R_INFO *pstB2rInfo)
{
	u64 clock_rate_est;
	u64 htt_est;
	u64 vtt_est;
	u32 u32B2RPixelNum = 0;
	int ret = 0;

	ret = _DipB2rChkTiming(pstB2rInfo);
	if (ret) {
		DIP_ERR("[%s]Check B2R Timing Fail\n", __func__);
		return ret;
	}

	u32B2RPixelNum = pstB2rInfo->u32B2rPixelNum;
	if (u32B2RPixelNum == 0) {
		DIP_ERR("[%s]Invalid B2R Engine Pixel\n", __func__);
		return (-1);
	}

	htt_est = ALIGN_UPTO_32(pstB2rInfo->u32DeWidth/u32B2RPixelNum) + DIP_B2R_HBLK_SZ;
	vtt_est = ALIGN_UPTO_32(pstB2rInfo->u32DeHeight) + DIP_B2R_VBLK_SZ;
	clock_rate_est = (htt_est * vtt_est * pstB2rInfo->u32Fps);

	if (clock_rate_est < DIP_B2R_CLK_144MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_144MHZ;
	} else if (clock_rate_est < DIP_B2R_CLK_156MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_156MHZ;
	} else if (clock_rate_est < DIP_B2R_CLK_312MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_312MHZ;
	} else if (clock_rate_est < DIP_B2R_CLK_360MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_360MHZ;
	} else if (clock_rate_est < DIP_B2R_CLK_630MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_630MHZ;
	} else if (clock_rate_est <= DIP_B2R_CLK_720MHZ) {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_720MHZ;
	} else {
		pstB2rInfo->u64ClockRate = DIP_B2R_CLK_720MHZ;
		DIP_ERR("[%s]Request clock over highest clock\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int _DipB2rGenTiming(struct ST_B2R_INFO *pstB2rInfo,
	struct ST_B2R_TIMING *pstB2rTiming)
{
	u64 u64Htt = 0;
	u64 u64Vtt = 0;
	u32 u32B2RPixelNum = 0;
	u16 u16VBlankSz = 0;
	int ret = 0;

	ret = _DipB2rChkTiming(pstB2rInfo);
	if (ret) {
		DIP_ERR("[%s]Check B2R Timing Fail\n", __func__);
		return ret;
	}

	u32B2RPixelNum = pstB2rInfo->u32B2rPixelNum;
	if (u32B2RPixelNum == 0) {
		DIP_ERR("[%s]Invalid B2R Engine Pixel\n", __func__);
		return (-1);
	}

	u64Vtt = ALIGN_UPTO_32(pstB2rInfo->u32DeHeight) + DIP_B2R_VBLK_SZ;
	u64Htt = pstB2rInfo->u64ClockRate;
	do_div(u64Htt, pstB2rInfo->u32Fps);
	do_div(u64Htt, u64Vtt);

	pstB2rTiming->V_TotalCount = u64Vtt;
	if (pstB2rInfo->u32B2rHttPixelUnit)
		pstB2rTiming->H_TotalCount = u64Htt * u32B2RPixelNum;
	else
		pstB2rTiming->H_TotalCount = u64Htt;
	pstB2rTiming->VBlank0_Start = DIP_B2R_VBLK_STR;
	u16VBlankSz = pstB2rTiming->V_TotalCount - pstB2rInfo->u32DeHeight;
	pstB2rTiming->VBlank0_End = u16VBlankSz - pstB2rTiming->VBlank0_Start;
	pstB2rTiming->TopField_VS = pstB2rTiming->VBlank0_Start
		+ (u16VBlankSz >> 1);
	pstB2rTiming->TopField_Start = pstB2rTiming->TopField_VS;
	pstB2rTiming->HActive_Start = (pstB2rTiming->H_TotalCount
		- (ALIGN_UPTO_32(pstB2rInfo->u32DeWidth) / u32B2RPixelNum));
	pstB2rTiming->HImg_Start = pstB2rTiming->HActive_Start;
	pstB2rTiming->VImg_Start0 = pstB2rTiming->VBlank0_End;

	pstB2rTiming->VBlank1_Start = pstB2rTiming->VBlank0_Start;
	pstB2rTiming->VBlank1_End = pstB2rTiming->VBlank0_End;
	pstB2rTiming->BottomField_Start = pstB2rTiming->TopField_Start;
	pstB2rTiming->BottomField_VS = pstB2rTiming->TopField_VS;
	pstB2rTiming->VImg_Start1 = pstB2rTiming->VImg_Start0;

	return 0;
}

static int _DipGetHvspResource(struct dip_ctx *ctx,
	EN_DIP_SHARE eShareEng, EN_DIP_IDX enDIPIdx)
{
	if (!ctx) {
		DIP_ERR("[%s], ctx is NULL\n", __func__);
		return -ENXIO;
	}

	if (ctx->bHvspResource == false) {
		if (KDrv_DIP_ResourceGet(EN_DIP1_HVSP)) {
			KDrv_DIP_SetDIPShareMode(eShareEng, true, enDIPIdx);
			ctx->bHvspResource = true;
		} else {
			return -EBUSY;
		}
	}

	return 0;
}

static int _DipReleaseHvspResource(struct dip_ctx *ctx,
	EN_DIP_SHARE eShareEng, EN_DIP_IDX enDIPIdx)
{
	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	if (ctx->bHvspResource == true) {
		KDrv_DIP_SetDIPShareMode(eShareEng, false, enDIPIdx);
		KDrv_DIP_ResourceRelease(EN_DIP1_HVSP);
		ctx->bHvspResource = false;
	}

	return 0;
}

static u64 mtk_get_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	struct vb2_queue *vq = NULL;
	struct dip_ctx *ctx = NULL;
	struct dip_dev *dev = NULL;
	struct sg_table *sgt = NULL;
	unsigned long *user_ptr = NULL;
	u32 u32BufMode = 0;
	u64 addr = 0;

	if (vb == NULL) {
		DIP_ERR("%s, vb is NULL\n", __func__);
		return INVALID_ADDR;
	}

	vq = vb->vb2_queue;
	ctx = vq->drv_priv;
	dev = ctx->dev;

	u32BufMode = _GetDIPBufMode(ctx, vq->type);
	if (u32BufMode <= E_BUF_IOVA_MODE) {
		user_ptr = (unsigned long *)vb2_plane_cookie(vb, plane_no);
		if (user_ptr == NULL)
			addr = INVALID_ADDR;
		else
			addr = (u64)*user_ptr;
	} else if (u32BufMode == E_BUF_FRAMEBUF_FD_MODE) {
		sgt = (struct sg_table *)vb2_plane_cookie(vb, plane_no);
		if (!sgt)
			addr = INVALID_ADDR;
		else
			addr = sg_dma_address(sgt->sgl);
	} else {
		addr = INVALID_ADDR;
	}

	return addr;
}

int Dip_GetDMABufInfo(struct dip_ctx *ctx, struct vb2_v4l2_buffer *vbuf,
	enum v4l2_buf_type type)
{
	u16 u16NPlanes = 0, u16Idx = 0;
	u64 size = 0;
	u32 u32BufMode = _GetDIPBufMode(ctx, type);
	struct dip_frame *pFrm = _GetDIPFrame(ctx, type);
	struct dip_dev *dev = ctx->dev;
	struct meta_buffer buffer;
	struct meta_header header;
	struct meta_dip_frame_info stDipFrameInfo;
	struct dip_fmt *fmt = NULL;
	void	*vaddr;

	if (u32BufMode == E_BUF_FRAMEBUF_FD_MODE) {
		u16NPlanes = _GetDIPFmtPlaneCnt(pFrm->fmt->fourcc);
		for (u16Idx = 0; u16Idx < u16NPlanes ; u16Idx++) {
			pFrm->u64PhyAddr[u16Idx] = mtk_get_addr(&vbuf->vb2_buf, u16Idx);
			if (pFrm->u64PhyAddr[u16Idx] == INVALID_ADDR) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, address[%d] invalid\n",
					__func__, __LINE__, u16Idx);
				return -EINVAL;
			}
			pFrm->size[u16Idx] = vb2_plane_size(&vbuf->vb2_buf, u16Idx);
		}
	} else if (u32BufMode == E_BUF_META_FD_MODE) {
		vaddr = vb2_plane_vaddr(&vbuf->vb2_buf, 0);
		size = vb2_plane_size(&vbuf->vb2_buf, 0);
		if (_dip_common_get_metaheader(E_DIP_MTDT_DIP_FRAME_INFO_TAG, &header)) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, get metadata header fail\n",
				__func__, __LINE__);
			return -EPERM;
		}
		buffer.paddr = (unsigned char *)vaddr;
		buffer.size = (unsigned int)size;
		memset(&stDipFrameInfo, 0, sizeof(struct meta_dip_frame_info));
		if (!query_metadata_content(&buffer, &header, (void *)&stDipFrameInfo)) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, query metadata content fail\n",
				__func__, __LINE__);
			return -EPERM;
		}

		pFrm->width = stDipFrameInfo.u32Width;
		pFrm->height = stDipFrameInfo.u32Height;
		pFrm->u32BytesPerLine[0] = stDipFrameInfo.u32Bytesperline;
		ctx->u32Rotation = stDipFrameInfo.u16Rotation;
		fmt = _FindDipOutMpFmt(stDipFrameInfo.u32Fourcc);
		pFrm->fmt = fmt;
		u16NPlanes = _GetDIPFmtPlaneCnt(stDipFrameInfo.u32Fourcc);
		for (u16Idx = 0; u16Idx < u16NPlanes ; u16Idx++) {
			pFrm->size[u16Idx] = stDipFrameInfo.u32Size[u16Idx];
			pFrm->u64PhyAddr[u16Idx] = stDipFrameInfo.u64Addr[u16Idx];
		}
		if (stDipFrameInfo.eColorRange == E_M_DIP_COLOR_RANGE_FULL)
			pFrm->quantization = V4L2_QUANTIZATION_FULL_RANGE;
		else
			pFrm->quantization = V4L2_QUANTIZATION_LIM_RANGE;

		if (stDipFrameInfo.eColorSpace == E_M_DIP_COLOR_SPACE_BT601)
			pFrm->ycbcr_enc = V4L2_YCBCR_ENC_601;
		else if (stDipFrameInfo.eColorSpace == E_M_DIP_COLOR_SPACE_BT709)
			pFrm->ycbcr_enc = V4L2_YCBCR_ENC_709;
		else if (stDipFrameInfo.eColorSpace == E_M_DIP_COLOR_SPACE_BT2020)
			pFrm->ycbcr_enc = V4L2_YCBCR_ENC_BT2020;
		else
			pFrm->ycbcr_enc = V4L2_YCBCR_ENC_709;
	} else {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Current is not DMA buffer mode\n",
				__func__, __LINE__);
		return -EPERM;
	}

	return 0;
}

int _DipSetSrcUdmaLimitRange(struct dip_ctx *ctx, u64 *pu64PhyAddr, bool bEnable)
{
	struct dip_dev *dev = NULL;
	struct dip_frame *pSrcFrm = NULL;
	u16 u16CovertPitch;
	u32 u32NPlanes = 0, u32Idx = 0;
	u32 u32InputByte = 0, u32Height = 0, u32Divisor = 0;
	u32 u32HAlign = 0, u32BufHAlign = 0, u32Size = 0;
	u64 u64MaxAddr = 0, u64MinAddr = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	if (ctx == NULL) {
		DIP_ERR("[%s]ctx is NULL\n", __func__);
		return 0;
	}
	dev = ctx->dev;
	pSrcFrm = &(ctx->in);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (bEnable) {
		_GetInFmtHeightAlign(pSrcFrm->fmt->fourcc, &u32HAlign, &u32BufHAlign);
		if (u32BufHAlign == 0)
			return -1;

		if (pSrcFrm->height % u32BufHAlign)
			u32Height = ((pSrcFrm->height+u32BufHAlign)/u32BufHAlign)*u32BufHAlign;
		else
			u32Height = pSrcFrm->height;

		u16CovertPitch = pSrcFrm->u32BytesPerLine[0];
		_PitchConvert(pSrcFrm->fmt->fourcc, &u16CovertPitch);
		u32InputByte = MapPixelToByte(pSrcFrm->fmt->fourcc,
				u16CovertPitch*u32Height);

		u32NPlanes = _GetDIPFmtPlaneCnt(pSrcFrm->fmt->fourcc);
		u32Divisor = _GetNPlaneBppDivisor(pSrcFrm->fmt->fourcc);

		for (u32Idx = 0; u32Idx < u32NPlanes ; u32Idx++) {
			if (pu64PhyAddr[u32Idx] != NULL) {
				if (u64MinAddr == 0 || u64MinAddr > pu64PhyAddr[u32Idx])
					u64MinAddr = pu64PhyAddr[u32Idx];

				if (u32Idx > 0 && u32Divisor != 0)
					u32Size = u32InputByte/u32Divisor;
				else
					u32Size = u32InputByte;

				if ((pu64PhyAddr[u32Idx] + u32Size) > u64MaxAddr)
					u64MaxAddr = pu64PhyAddr[u32Idx] + u32Size;
			}
		}
		DIP_DBG_BUF("[%s]MinAddr:0x%llx, MaxAddr:0x%llx InputByte:%d\n",
				__func__, u64MinAddr, u64MaxAddr, u32InputByte);
	}
	KDrv_DIP_SetSrcUdmaRange(bEnable, u64MinAddr, u64MaxAddr, enDIPIdx);

	return 0;
}

int _DipSetDstUdmaLimitRange(struct dip_ctx *ctx, u64 *pu64PhyAddr, bool bEnable)
{
	struct dip_dev *dev = NULL;
	struct dip_frame *pDstFrm = NULL;
	u16 u16CovertPitch = 0;
	u32 u32NPlanes = 0, u32Idx = 0;
	u32 u32OutputByte = 0, u32Height = 0, u32Divisor = 0;
	u32 u32HAlign = 0, u32BufHAlign = 0, u32Size = 0;
	u64 u64MaxAddr = 0, u64MinAddr = 0, u64TempAddr;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	if (ctx == NULL) {
		DIP_ERR("[%s]ctx is NULL\n", __func__);
		return 0;
	}
	dev = ctx->dev;
	pDstFrm = &(ctx->out);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (bEnable) {
		_GetOutFmtHeightAlign(dev, pDstFrm->fmt->fourcc, &u32HAlign, &u32BufHAlign);
		if (u32BufHAlign == 0)
			return -1;

		if (pDstFrm->u32OutHeight % u32BufHAlign) {
			u32Height =
				((pDstFrm->u32OutHeight+u32BufHAlign)/u32BufHAlign)*u32BufHAlign;
		} else {
			u32Height = pDstFrm->u32OutHeight;
		}

		u16CovertPitch = pDstFrm->u32BytesPerLine[0];
		_PitchConvert(pDstFrm->fmt->fourcc, &u16CovertPitch);
		u32OutputByte = MapPixelToByte(pDstFrm->fmt->fourcc,
				u16CovertPitch*u32Height);

		u32NPlanes = _GetDIPFmtPlaneCnt(pDstFrm->fmt->fourcc);
		u32Divisor = _GetNPlaneBppDivisor(pDstFrm->fmt->fourcc);

		for (u32Idx = 0; u32Idx < u32NPlanes ; u32Idx++) {
			if (pu64PhyAddr[u32Idx] != NULL) {
				if (u64MinAddr == 0 || u64MinAddr > pu64PhyAddr[u32Idx])
					u64MinAddr = pu64PhyAddr[u32Idx];

				if (u32Idx > 0 && u32Divisor != 0)
					u32Size = u32OutputByte/u32Divisor;
				else
					u32Size = u32OutputByte;

				if ((ctx->u32CapStat & ST_CONT) == 0) {
					u64TempAddr = pu64PhyAddr[u32Idx] + u32Size;
				} else {
					u64TempAddr =
						pu64PhyAddr[u32Idx] + (u32Size*ctx->u8ContiBufCnt);
				}

				if (u64TempAddr > u64MaxAddr)
					u64MaxAddr = u64TempAddr;
			}
		}
		DIP_DBG_BUF("[%s]MinAddr:0x%llx, MaxAddr:0x%llx OutputByte:%d\n",
				__func__, u64MinAddr, u64MaxAddr, u32OutputByte);
	}
	KDrv_DIP_SetDstUdmaRange(bEnable, u64MinAddr, u64MaxAddr, enDIPIdx);

	return 0;
}

int _DipDevRunSetSrcAddr(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *src_vb = NULL;
	struct dip_fmt *Srcfmt = NULL;
	struct ST_B2R_ADDR stB2rAddr;
	struct ST_DIP_B2R_MTDT *pstB2rMtdt = &(ctx->stB2rMtdt);
	u64 pu64SrcPhyAddr[MAX_PLANE_NUM] = {0};
	u32 u32Idx = 0;
	u32 u32NPlanes = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	if (src_vb == NULL)
		return (-1);

	Srcfmt = ctx->in.fmt;
	u32NPlanes = _GetDIPFmtPlaneCnt(Srcfmt->fourcc);
	if (u32NPlanes > MAX_PLANE_NUM) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,NPlanes(%d) too much\n",
				__func__, __LINE__, u32NPlanes);
		return (-1);
	}
	for (u32Idx = 0; u32Idx < u32NPlanes ; u32Idx++) {
		pu64SrcPhyAddr[u32Idx] = mtk_get_addr(&(src_vb->vb2_buf), u32Idx);
		if (pu64SrcPhyAddr[u32Idx] == INVALID_ADDR) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, address[%d] invalid\n",
				__func__, __LINE__, u32Idx);
			return -EINVAL;
		}
		if (ctx->u32SrcBufMode == E_BUF_IOVA_MODE)
			pu64SrcPhyAddr[u32Idx] += IOVA_START_ADDR;
		DIP_DBG_BUF("%s, Addr[%d]:0x%llx\n",
					__func__, u32Idx, pu64SrcPhyAddr[u32Idx]);
	}

	if (ctx->enSource == E_DIP_SOURCE_DIPR) {
		if (_IsB2rPath(ctx, Srcfmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R == 0) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
				return -ENODEV;
			}
			if (_IsCompressFormat(Srcfmt->fourcc) == true) {
				if (pstB2rMtdt->u32Enable == 0) {
					v4l2_err(&dev->v4l2_dev, "%s,%d,Invalid Metadata\n",
						__func__, __LINE__);
					return (-1);
				}
				stB2rAddr.u64LumaFb =
					pu64SrcPhyAddr[0] + pstB2rMtdt->u64LumaFbOft;
				stB2rAddr.u64ChromaFb =
					pu64SrcPhyAddr[0] + pstB2rMtdt->u64ChromaFbOft;
				stB2rAddr.u64LumaBlnFb =
					pu64SrcPhyAddr[0] + pstB2rMtdt->u64LumaBitlenOft;
				stB2rAddr.u64ChromaBlnFb =
					pu64SrcPhyAddr[0] + pstB2rMtdt->u64ChromaBitlenOft;
				stB2rAddr.u8Addrshift = B2R_CMPS_ADDR_OFT;
			} else {
				if (u32NPlanes > PLANE0_NUM)
					stB2rAddr.u64LumaFb = pu64SrcPhyAddr[PLANE0_NUM];
				if (u32NPlanes > PLANE1_NUM)
					stB2rAddr.u64ChromaFb = pu64SrcPhyAddr[PLANE1_NUM];
				if (u32NPlanes > PLANE2_NUM)
					stB2rAddr.u64LumaBlnFb = pu64SrcPhyAddr[PLANE2_NUM];
				if (u32NPlanes > PLANE3_NUM)
					stB2rAddr.u64ChromaBlnFb = pu64SrcPhyAddr[PLANE3_NUM];
				stB2rAddr.u8Addrshift = B2R_UNCMPS_ADDR_OFT;
			}
			KDrv_DIP_B2R_SetAddr(&stB2rAddr);
		} else {
			_DipSetSrcUdmaLimitRange(ctx, pu64SrcPhyAddr, true);
			KDrv_DIP_SetSrcAddr(u32NPlanes, pu64SrcPhyAddr, enDIPIdx);
		}
	} else if (ctx->enSource == E_DIP_SOURCE_B2R) {
		stB2rAddr.u64LumaFb = pu64SrcPhyAddr[0];
		stB2rAddr.u64ChromaFb = pu64SrcPhyAddr[1];
		if (_IsCompressFormat(Srcfmt->fourcc) == true) {
			stB2rAddr.u64LumaBlnFb = pu64SrcPhyAddr[2];
			stB2rAddr.u64ChromaBlnFb = pu64SrcPhyAddr[3];
			stB2rAddr.u8Addrshift = B2R_CMPS_ADDR_OFT;
		} else {
			stB2rAddr.u8Addrshift = B2R_UNCMPS_ADDR_OFT;
		}
		KDrv_DIP_B2R_SetAddr(&stB2rAddr);
		if (ctx->stDiNrInfo.u32OpMode == E_DIP_MODE_DI) {
			if ((src_vb->vb2_buf.index % MOD2) == 0)
				KDrv_DIP_B2R_SetFiled(E_B2R_FIELD_TOP);
			else
				KDrv_DIP_B2R_SetFiled(E_B2R_FIELD_BOTTOM);
		}
	} else {
		v4l2_err(&dev->v4l2_dev, "%s,%d, source %d not support M2M\n",
			__func__, __LINE__, ctx->enSource);
		return -ENODEV;
	}

	return 0;
}

int _DipDevRunSetDstAddr(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *dst_vb = NULL;
	struct dip_fmt *Dstfmt = NULL;
	u64 pu64DstPhyAddr[MAX_PLANE_NUM] = {0};
	u32 u32Idx = 0;
	u32 u32NPlanes = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	dst_vb = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	if (dst_vb == NULL) {
		ctx->u32M2mStat = ST_OFF;
		return (-1);
	}

	Dstfmt = ctx->out.fmt;
	u32NPlanes = _GetDIPFmtPlaneCnt(Dstfmt->fourcc);
	if (u32NPlanes > MAX_PLANE_NUM) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, NPlanes(%d) too much\n",
			__func__, __LINE__, u32NPlanes);
		return (-1);
	}
	for (u32Idx = 0; u32Idx < u32NPlanes ; u32Idx++) {
		pu64DstPhyAddr[u32Idx] = mtk_get_addr(&(dst_vb->vb2_buf), u32Idx);
		if (pu64DstPhyAddr[u32Idx] == INVALID_ADDR) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, address[%d] invalid\n",
				__func__, __LINE__, u32Idx);
			return -EINVAL;
		}
		if (ctx->u32DstBufMode == E_BUF_IOVA_MODE)
			pu64DstPhyAddr[u32Idx] += IOVA_START_ADDR;
		DIP_DBG_BUF("%s, Addr[%d]:0x%llx\n",
					__func__, u32Idx, pu64DstPhyAddr[u32Idx]);
	}
	_DipSetDstUdmaLimitRange(ctx, pu64DstPhyAddr, true);
	KDrv_DIP_SetDstAddr(u32NPlanes, pu64DstPhyAddr, enDIPIdx);

	return 0;
}

static void SaveExcuteStart(void)
{
	ktime_get_real_ts64(&StartTmpTime);
}

int _DipDevRunFire(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *Srcfmt = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	Srcfmt = ctx->in.fmt;
	if (ctx->enSource == E_DIP_SOURCE_DIPR) {
		if (_IsB2rPath(ctx, Srcfmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R) {
				KDrv_DIP_B2R_Ena(true);
				SaveExcuteStart();
				KDrv_DIP_Trigger(enDIPIdx);
			} else {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
				ret = (-1);
			}
		} else if (gst1RNWInfo.u32WriteFlag != E_DIP_1RNW_NONE) {
			gst1RNWInfo.u32Fired |= (1 << enDIPIdx);
			if (gst1RNWInfo.u32Fired == gst1RNWInfo.u32WriteFlag) {
				SaveExcuteStart();
				KDrv_DIP_DIPR_Trigger(enDIPIdx);
				gst1RNWInfo.u32Fired = 0;
			}
		} else {
			SaveExcuteStart();
			KDrv_DIP_DIPR_Trigger(enDIPIdx);
		}
	} else if (ctx->enSource == E_DIP_SOURCE_B2R) {
		if (dev->dipcap_dev.u32B2R) {
			if ((ctx->u32CapStat & ST_CONT) == 0) {
				SaveExcuteStart();
				KDrv_DIP_Trigger(enDIPIdx);
			}
			KDrv_DIP_B2R_Ena(true);
		} else {
			v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
				__func__, __LINE__);
			ret = (-1);
		}
	}

	return 0;
}

int _DipAllocateBuffer(struct dip_ctx *ctx,
		unsigned int buf_tag,
		dma_addr_t *piova,
		size_t size)
{
	struct dip_dev *dipdev = ctx->dev;
	struct device *dev = dipdev->dev;
	int ret = 0;
	unsigned long dma_attrs = 0;
	void *cookie;

	if (buf_tag == 0) {
		v4l2_err(&(dipdev->v4l2_dev), "Invalid buf tag\n");
		return -1;
	}

	if (dma_get_mask(dev) < DMA_BIT_MASK(34)) {
		v4l2_err(&(dipdev->v4l2_dev), "IOMMU isn't registered\n");
		return -1;
	}

	if (!dma_supported(dev, 0)) {
		v4l2_err(&(dipdev->v4l2_dev), "IOMMU is not supported\n");
		return -1;
	}

	dma_attrs = buf_tag << IOMMU_BUF_TAG_SHIFT;
	dma_attrs |= DMA_ATTR_WRITE_COMBINE;
	cookie = dma_alloc_attrs(dev, size,
					piova, GFP_KERNEL,
					dma_attrs);
	if (!cookie) {
		v4l2_err(&(dipdev->v4l2_dev), "failed to allocate %zx byte dma buffer", size);
		return (-1);
	}
	gstDiNrBufInfo.DmaAddr = *piova;
	gstDiNrBufInfo.u32Size = size;
	gstDiNrBufInfo.dma_attrs = dma_attrs;
	gstDiNrBufInfo.virtAddr = cookie;
	ret = dma_mapping_error(dev, *piova);
	if (ret) {
		v4l2_err(&(dipdev->v4l2_dev), "dma_alloc_attrs fail\n");
		return ret;
	}

	return ret;
}

int _GetB2rBWLimitFps(struct dip_ctx *ctx,
	u32 *pu32Fps)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pSrcFrm = &(ctx->in);
	struct dip_frame *pDstFrm = &(ctx->out);
	u32 u32PixelCnt = 0;
	u32 u32InputByte = 0;
	u32 u32OutputByte = 0;
	u32 u32TotalByte = 0;

	if (ctx->u8Bwlimit) {
		if ((pSrcFrm->width*pSrcFrm->height) > (pDstFrm->u32OutWidth*pDstFrm->u32OutHeight))
			u32PixelCnt = pSrcFrm->width*pSrcFrm->height;
		else
			u32PixelCnt = pDstFrm->u32OutWidth*pDstFrm->u32OutHeight;

		u32InputByte = MapFramePixelToByte(pSrcFrm->fmt->fourcc,
			pSrcFrm->width*pSrcFrm->height);
		u32OutputByte = MapFramePixelToByte(pDstFrm->fmt->fourcc,
			pDstFrm->u32OutWidth*pDstFrm->u32OutHeight);
		u32TotalByte = u32InputByte + u32OutputByte;
		if (u32TotalByte == 0) {
			v4l2_err(&dev->v4l2_dev, "[%s]Error:RW data is Zero\n", __func__);
			*pu32Fps = DIP_B2R_DEF_FPS;
			return -EINVAL;
		}
		*pu32Fps = ctx->u64BytePerSec/u32TotalByte;
		if (*pu32Fps > DIP_B2R_DEF_FPS)
			*pu32Fps = DIP_B2R_DEF_FPS;
	} else {
		*pu32Fps = DIP_B2R_DEF_FPS;
	}

	return 0;
}

static int _SetDipInOutClk(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *DipFrm;
	struct ST_B2R_INFO stB2rInfo = {0};
	EN_DIP_SRC_FROM enSrcFrom = E_DIP_SRC_FROM_SRCCAP_MAIN;
	bool bRet = false;
	int ret = 0;

	if ((_IsM2mCase(ctx->enSource)) == true) {
		DipFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (IS_ERR(DipFrm))
			return PTR_ERR(DipFrm);
		if (_IsB2rPath(ctx, DipFrm->fmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R) {
				/* check input parameters */
				stB2rInfo.u32DeWidth = DipFrm->width;
				stB2rInfo.u32DeHeight = DipFrm->height;
				stB2rInfo.u32B2rPixelNum = dev->dipcap_dev.u32B2rPixelNum;
				stB2rInfo.u32B2rHttPixelUnit = dev->dipcap_dev.u32B2rHttPixelUnit;
				ret = _GetB2rBWLimitFps(ctx, &(stB2rInfo.u32Fps));
				if (ret) {
					v4l2_err(&dev->v4l2_dev, "%s, %d, Invalid BW limit Setting ",
						__func__, __LINE__);
					return ret;
				}
				ctx->u32B2rFps = stB2rInfo.u32Fps;
				ret = _DipB2rGetClk(&stB2rInfo);
				if (ret)
					return ret;
				ret = _SetB2rClk(dev, true, stB2rInfo.u64ClockRate);
				if (ret)
					return ret;
			} else {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
				return -ENODEV;
			}
		}
	}

	bRet = _GetSourceSelect(ctx, &enSrcFrom);
	if (bRet == false) {
		v4l2_err(&dev->v4l2_dev, "_GetSourceSelect Fail\n");
		return -EPERM;
	}
	bRet = _SetDIPClkParent(dev, enSrcFrom, true);
	if (bRet == false) {
		v4l2_err(&dev->v4l2_dev, "_SetDIPClkParent Fail\n");
		return -EPERM;
	}

	return 0;
}

int _DipSetSource(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *DipFrm;
	struct ST_DIP_B2R_MTDT *pstB2rMtdt = &(ctx->stB2rMtdt);
	struct ST_B2R_TIMING stB2rTiming = {0};
	struct ST_B2R_INFO stB2rInfo = {0};
	struct vb2_v4l2_buffer *src_vb = NULL;
	struct ST_DIPR_SRC_WIN stDiprSrcWin = {0};
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;
	u32 u32MaxWidth = 0;
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;
	u16 u16CovertPitch = 0;

	DipFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	if (IS_ERR(DipFrm))
		return PTR_ERR(DipFrm);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	DIP_DBG_API("%s, AID Type:%d\n", __func__, ctx->u8aid);

	_GetDIPFmtAlignUnit(dev, DipFrm->fmt->fourcc, &u16SizeAlign, &u16PitchAlign);
	ret = _IsAlign(DipFrm->width, u16SizeAlign);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d]Width(%u) Not Align(%u)\n",
			__func__, __LINE__, DipFrm->width, u16SizeAlign);
		_AlignAdjust(&(DipFrm->width), u16SizeAlign);
	}

	u16PitchAlign = MapPixelToByte(DipFrm->fmt->fourcc, u16PitchAlign);
	ret = _IsAlign(DipFrm->u32BytesPerLine[0], u16PitchAlign);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d]Pitch(%u) Not Align(%u)\n",
			__func__, __LINE__, DipFrm->u32BytesPerLine[0], u16PitchAlign);
		_AlignAdjust(&(DipFrm->u32BytesPerLine[0]), u16PitchAlign);
	}
	u16CovertPitch = DipFrm->u32BytesPerLine[0];
	_PitchConvert(DipFrm->fmt->fourcc, &u16CovertPitch);

	u32MaxWidth = _GetDIPRFmtMaxWidth(ctx, DipFrm->fmt->fourcc);
	if (DipFrm->width > u32MaxWidth) {
		if (dev->dipcap_dev.u32RWSep) {
			if (_IsSupportRWSepFmt(ctx, DipFrm->fmt->fourcc) == false) {
				v4l2_err(&dev->v4l2_dev,
					"%s, %d, Format %s, Out Of Spec Width(%d), Input Max Width(%d)",
					__func__, __LINE__,
					DipFrm->fmt->name, DipFrm->width, u32MaxWidth);
				return (-1);
			}
			if (DipFrm->width > DIP_RWSEP_IN_WIDTH_MAX) {
				v4l2_err(&dev->v4l2_dev,
					"%s, %d, Format %s, Out Of Spec Width(%d), RWSep Input Max Width(%d)",
					__func__, __LINE__,
					DipFrm->fmt->name, DipFrm->width, DIP_RWSEP_IN_WIDTH_MAX);
				return (-1);
			}
		} else {
			v4l2_err(&dev->v4l2_dev,
				"%s, %d, Format %s, Out Of Spec Width(%d), Input Max Width(%d)",
				__func__, __LINE__,
				DipFrm->fmt->name, DipFrm->width, u32MaxWidth);
			return (-1);
		}
	}

	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);

	if (_IsB2rPath(ctx, DipFrm->fmt->fourcc) == true) {
		if (dev->dipcap_dev.u32B2R) {
			ret = _DipB2rChkSize(DipFrm->width, DipFrm->height);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s, %d, InValid B2R Size",
						__func__, __LINE__);
				return (-1);
			}
			KDrv_DIP_B2R_Init();
			/* check input parameters */
			stB2rInfo.u32DeWidth = DipFrm->width;
			stB2rInfo.u32DeHeight = DipFrm->height;
			stB2rInfo.u32B2rPixelNum = dev->dipcap_dev.u32B2rPixelNum;
			stB2rInfo.u32B2rHttPixelUnit = dev->dipcap_dev.u32B2rHttPixelUnit;
			stB2rInfo.u32Fps = ctx->u32B2rFps;
			ret = _DipB2rGetClk(&stB2rInfo);
			if (ret)
				return ret;
			ctx->u64B2RClockRate = stB2rInfo.u64ClockRate;
			ret = _DipB2rGenTiming(&stB2rInfo, &stB2rTiming);
			if (ret)
				return ret;
			KDrv_DIP_B2R_SetTiming(&stB2rTiming);
			if (_IsCompressFormat(DipFrm->fmt->fourcc) == true) {
				if (pstB2rMtdt->u32Enable == 0) {
					v4l2_err(&dev->v4l2_dev, "%s, %d, InValid Metadata data",
							__func__, __LINE__);
					return (-1);
				}
				KDrv_DIP_B2R_SetFmt(pstB2rMtdt->u32fourcc);
				KDrv_DIP_B2R_SetFiled(E_B2R_FIELD_PRO);
				KDrv_DIP_B2R_SetSize(DipFrm->width, DipFrm->height,
						pstB2rMtdt->u32BitlenPitch);
			} else {
				KDrv_DIP_B2R_SetFmt(DipFrm->fmt->fourcc);
				if (ctx->stDiNrInfo.u32OpMode == E_DIP_MODE_DI)
					KDrv_DIP_B2R_SetFiled(E_B2R_FIELD_TOP);
				else
					KDrv_DIP_B2R_SetFiled(E_B2R_FIELD_PRO);
				KDrv_DIP_B2R_SetSize(DipFrm->width,
						DipFrm->height,
						u16CovertPitch);
			}
			KDrv_DIP_SetAID(ctx->u8aid, E_HW_B2R, enDIPIdx);
		} else {
			v4l2_err(&dev->v4l2_dev, "%s, %d, Not Support B2R",
					__func__, __LINE__);
			return (-1);
		}
	} else {
		memset(&stDiprSrcWin, 0, sizeof(stDiprSrcWin));
		stDiprSrcWin.u16Width = DipFrm->width;
		stDiprSrcWin.u16Height = DipFrm->height;
		stDiprSrcWin.u16Pitch = u16CovertPitch;
		stDiprSrcWin.u32fourcc = DipFrm->fmt->fourcc;
		KDrv_DIP_DIPR_SetSrcWin(&stDiprSrcWin, enDIPIdx);
		KDrv_DIP_SetAID(ctx->u8aid, E_HW_DIPR, enDIPIdx);
	}

	return 0;
}

int _DipDiNrSetting(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_DINR *pstDiNrInfo = &(ctx->stDiNrInfo);
	struct ST_OPMODE stOpMode = {0};
	struct dip_frame *DipFrm;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	DipFrm = _GetDIPFrame(ctx, type);
	if (IS_ERR(DipFrm))
		return PTR_ERR(DipFrm);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (ctx->stDiNrInfo.u32OpMode) {
		stOpMode.enOpMode = ctx->stDiNrInfo.u32OpMode;
		stOpMode.u16Width = pstDiNrInfo->u32InputWidth;
		stOpMode.u16Height = pstDiNrInfo->u32InputHeight;
		stOpMode.u64Addr = gstDiNrBufInfo.DmaAddr;
		KDrv_DIP_SetOpMode(&stOpMode, enDIPIdx);
	} else {
		stOpMode.enOpMode = E_DIP_NORMAL;
		KDrv_DIP_SetOpMode(&stOpMode, enDIPIdx);
	}

	return 0;
}

int _DipScalingSetting(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_SCALING *pstScalingDown = &(ctx->stScalingDown);
	struct ST_SCALING *pstScalingHVSP = &(ctx->stScalingHVSP);
	struct ST_RWSEP_INFO *pstRwSep = &(ctx->stRwSep);
	struct dip_frame *DipFrm;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	DipFrm = _GetDIPFrame(ctx, type);
	if (IS_ERR(DipFrm))
		return PTR_ERR(DipFrm);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	KDrv_DIP_SetScaling(pstScalingDown, enDIPIdx);
	if (dev->dipcap_dev.u32USCL) {
		if (!_DipGetHvspResource(ctx, E_DIP_SHARE_DIP1_HVSP, enDIPIdx)) {
			KDrv_DIP_SetHVSP(pstScalingHVSP, enDIPIdx);
			if (pstScalingHVSP->u8HScalingEna == 0 &&
				pstScalingHVSP->u8VScalingEna == 0 &&
				pstScalingHVSP->u32OutputHeight <= pstScalingHVSP->u32InputHeight) {
				_DipReleaseHvspResource(ctx, E_DIP_SHARE_DIP1_HVSP,
					enDIPIdx);
			}
		} else {
			if (pstScalingHVSP->u8HScalingEna == 1 ||
				pstScalingHVSP->u8VScalingEna == 1 ||
				pstScalingHVSP->u32OutputHeight > pstScalingHVSP->u32InputHeight) {
				DIP_ERR("[%s]ERR: src < dst, but no HVSP resource\n",
					__func__);
				return -EBUSY;
			}
		}
	}
	KDrv_DIP_SetRWSeparate(pstRwSep, enDIPIdx);

	return 0;
}

int _DipRotateSetting(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (dev->dipcap_dev.u32Rotate) {
		if (ctx->u32Rotation == ROT90_ANGLE) {
			KDrv_DIP_SetRotation(E_DIP_ROT_90, enDIPIdx);
		} else if (ctx->u32Rotation == ROT270_ANGLE) {
			KDrv_DIP_SetRotation(E_DIP_ROT_270, enDIPIdx);
		} else {
			KDrv_DIP_SetRotation(E_DIP_ROT_0, enDIPIdx);
		}
	}

	return 0;
}

static int _DipHighWayLRSetting(struct dip_ctx *ctx)
{
	struct dip_dev *dev;
	struct ST_HIGHWAY_LR_CTRL stHighWay;

	if (!ctx) {
		DIP_ERR("%s, Null pointer of ctx\n", __func__);
		return -1;
	}

	if (!ctx->dev) {
		DIP_ERR("%s, Null pointer of ctx->dev\n", __func__);
		return -1;
	}

	dev = ctx->dev;

	if (!dev->dipcap_dev.u32CapHighway)
		return 0;

	stHighWay.bHighWayEable = (MS_BOOL)ctx->stHighway.u8HighwayEn;

	switch (ctx->stHighway.enLRMode) {
	case E_DIP_LR_DISABLE:
		stHighWay.enDIPLRMode = E_DIP_LR_MODE_DISABLE;
		break;
	case E_DIP_LR_NORMAL:
		stHighWay.enDIPLRMode = E_DIP_LR_MODE_NORMAL;
		break;
	case E_DIP_LR_W2D2:
		stHighWay.enDIPLRMode = E_DIP_LR_MODE_W2D2;
		break;
	default:
		stHighWay.enDIPLRMode = E_DIP_LR_MODE_DISABLE;
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error enum of LR mode\n",
				__func__, __LINE__);
		break;
	}

	if (!KDrv_DIP_SetHighwayPath(&stHighWay, dev->variant->u16DIPIdx)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Highway path and LR setting\n",
				__func__, __LINE__);
		return -1;
	}

	return 0;
}

static int _DipSetOutputSize(struct dip_ctx *ctx,
	u32 u16CovertPitch)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pDstFrm = NULL;
	struct ST_RES_WIN stResWin = {0};
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	struct ST_FILL_BLACK stFillBlack = {0};
	int ret = 0;
	u32 u32NPlanes = 0;
	u32 u32Idx = 0;
	u32 u32BufSzNeed = 0;
	u32 u32PerPlaneHeight = 0;
	u32 u32WriteHeight = 0;
	bool bFillBlack = false;
	u32 u32OutWidth = 0, u32OutWidthAlign = 0;
	u32 u32OutHeight = 0;
	u16 u16SizeAlign, u16PitchAlign = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	pDstFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (IS_ERR(pDstFrm)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Invalid Frame Type\n",
				__func__, __LINE__);
		return (-1);
	}

	u32OutWidth = pDstFrm->u32OutWidth;
	u32OutHeight = pDstFrm->u32OutHeight;

	if (u32OutWidth > ctx->stScalingHVSP.u32OutputWidth) {
		if (dev->dipcap_dev.u32FillBlack == 1) {
			bFillBlack = true;
		} else {
			if (ctx->stScalingHVSP.u8HScalingEna == 1) {
				_GetDIPWFmtAlignUnit(dev,
					pDstFrm->fmt->fourcc, &u16SizeAlign, &u16PitchAlign);
				u32OutWidthAlign = ctx->stScalingHVSP.u32OutputWidth;
				_AlignAdjust(&u32OutWidthAlign, u16SizeAlign);
				if (u32OutWidth > u32OutWidthAlign) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, output width%d over scaling out%d\n",
						__func__, __LINE__,
						u32OutWidth, ctx->stScalingHVSP.u32OutputWidth);
						u32OutWidth = u32OutWidthAlign;
					ret = (-EINVAL);
				}
			}
		}
	}

	if (u32OutHeight > ctx->stScalingHVSP.u32OutputHeight) {
		if (dev->dipcap_dev.u32FillBlack == 1) {
			bFillBlack = true;
		} else {
			if (ctx->stScalingHVSP.u8VScalingEna == 1) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, output height %d over scaling out %d\n",
						__func__, __LINE__,
						u32OutHeight,
						ctx->stScalingHVSP.u32OutputHeight);
				u32OutHeight = ctx->stScalingHVSP.u32OutputHeight;
				ret = (-EINVAL);
			}
		}
	}

	if (dev->dipcap_dev.u32FillBlack == 1) {
		if (bFillBlack) {
			stFillBlack.bEable = TRUE;
			stFillBlack.u32XStart = 0;
			stFillBlack.u32YStart = 0;
			stFillBlack.u32Width = ctx->stScalingHVSP.u32OutputWidth;
			stFillBlack.u32Height = ctx->stScalingHVSP.u32OutputHeight;
		} else {
			stFillBlack.bEable = FALSE;
		}
		KDrv_DIP_SetFillBlack(&stFillBlack, pDstFrm->fmt->fourcc, enDIPIdx);
	}

	u32NPlanes = _GetDIPFmtPlaneCnt(pDstFrm->fmt->fourcc);
	u32WriteHeight = u32OutHeight;
	for (u32Idx = 0; u32Idx < u32NPlanes; u32Idx++) {
		if (u32Idx == 0)
			u32BufSzNeed = u16CovertPitch*u32OutHeight;
		else
			u32BufSzNeed = (u16CovertPitch*u32OutHeight)>>1;
		if (u32BufSzNeed > pDstFrm->size[u32Idx]) {
			v4l2_err(&dev->v4l2_dev, "[%s,%d]Plane:%d, Output Size(0x%x) is not Enough(0x%x)\n",
				__func__, __LINE__, u32Idx, pDstFrm->size[u32Idx], u32BufSzNeed);
			u32PerPlaneHeight = pDstFrm->size[u32Idx]/u16CovertPitch;
			if (_IsYUV420TileFormat(pDstFrm->fmt->fourcc) == true) {
				u32PerPlaneHeight = u32PerPlaneHeight/YUV420TL_HEIGHT_BUF_ALIGN;
				u32PerPlaneHeight = u32PerPlaneHeight*YUV420TL_HEIGHT_BUF_ALIGN;
			}
			if (u32WriteHeight > u32PerPlaneHeight) {
				v4l2_err(&dev->v4l2_dev, "[%s,%d]Plane:%d, Adjust Output Height:%d to %d\n",
					__func__, __LINE__,
					u32Idx, u32WriteHeight, u32PerPlaneHeight);
				u32WriteHeight = u32PerPlaneHeight;
			}
		}
	}

	if (ctx->u32Rotation == 0) {
		stResWin.u32Width = u32OutWidth;
		stResWin.u32Height = u32WriteHeight;
	} else {
		if (u32WriteHeight == u32OutHeight) {
			if ((dev->dipcap_dev.u32Rotate &
				(ROT90_M2M_SYS_BIT|ROT270_M2M_SYS_BIT))) {
				stResWin.u32Width = u32WriteHeight;
				stResWin.u32Height = u32OutWidth;
			} else {
				stResWin.u32Width = u32OutWidth;
				stResWin.u32Height = u32WriteHeight;
				u16CovertPitch = u32WriteHeight;
			}
		} else {
			v4l2_err(&dev->v4l2_dev, "[%s,%d]Rotate Fail\n",
				__func__, __LINE__);
			ret = (-EINVAL);
		}
	}
	stResWin.u32Pitch = u16CovertPitch;
	KDrv_DIP_SetWriteProperty(&stResWin, pDstFrm->fmt->fourcc, enDIPIdx);

	return ret;
}

int _DipWorkCapFire(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pDstFrm = _GetDIPFrame(ctx, type);
	struct dip_fmt *pDstfmt = pDstFrm->fmt;
	struct vb2_v4l2_buffer *dst_vb = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;
	u64 pu64DstPhyAddr[MAX_PLANE_NUM] = {0};
	u32 u32Idx = 0;
	u32 u32NPlanes = 0;
	u32 u32PixelCnt = 0;

	if (IS_ERR(pDstFrm))
		return PTR_ERR(pDstFrm);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	//set address
	dst_vb = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	u32NPlanes = _GetDIPFmtPlaneCnt(pDstfmt->fourcc);
	if (u32NPlanes > MAX_PLANE_NUM) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, NPlanes(%d) too much\n",
			__func__, __LINE__, u32NPlanes);
		return -1;
	}
	for (u32Idx = 0; u32Idx < u32NPlanes ; u32Idx++) {
		pu64DstPhyAddr[u32Idx] = mtk_get_addr(&(dst_vb->vb2_buf), u32Idx);
		if (pu64DstPhyAddr[u32Idx] == INVALID_ADDR) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, address[%d] invalid\n",
				__func__, __LINE__, u32Idx);
			return -EINVAL;
		}
		if (ctx->u32DstBufMode == E_BUF_IOVA_MODE)
			pu64DstPhyAddr[u32Idx] += IOVA_START_ADDR;
		DIP_DBG_BUF("%s, Addr[%d]:0x%llx\n",
					__func__, u32Idx, pu64DstPhyAddr[u32Idx]);
	}
	_DipSetDstUdmaLimitRange(ctx, pu64DstPhyAddr, true);
	KDrv_DIP_SetDstAddr(u32NPlanes, pu64DstPhyAddr, enDIPIdx);
	if ((ctx->u32CapStat & ST_CONT) == 0) {
		KDrv_DIP_Cont_Ena(false, enDIPIdx);
		KDrv_DIP_FrameCnt(1, 0, enDIPIdx);
		KDrv_DIP_Ena(true, enDIPIdx);
		SaveExcuteStart();
		KDrv_DIP_Trigger(enDIPIdx);
	} else {
		if ((ctx->u32FrcInRatio == ctx->u32FrcOutRatio) ||
			(ctx->u32FrcInRatio == 0) ||
			(ctx->u32FrcOutRatio == 0))
			KDrv_DIP_FrameRateCtrl(false, ctx->u32FrcInRatio,
						ctx->u32FrcOutRatio, enDIPIdx);
		else
			KDrv_DIP_FrameRateCtrl(true, ctx->u32FrcInRatio,
						ctx->u32FrcOutRatio, enDIPIdx);
		KDrv_DIP_Cont_Ena(true, enDIPIdx);
		u32PixelCnt = CalFramePixel(pDstfmt->fourcc, pDstFrm->size[0]);
		KDrv_DIP_FrameCnt(ctx->u8ContiBufCnt, u32PixelCnt, enDIPIdx);
		SaveExcuteStart();
		KDrv_DIP_Ena(true, enDIPIdx);
	}

	return 0;
}

int _DipCheckSdcSize(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pDipFrm = &(ctx->out);
	u32 u32DevelopMode = 0;
	int ret = 0;

	if (dev->efuse_develop_mode) {
		if (dev->sys_develop_mode)
			u32DevelopMode = 1;
		else
			u32DevelopMode = 0;
	} else {
		u32DevelopMode = 0;
	}

	if (u32DevelopMode == 0) {
		if ((pDipFrm->u32OutWidth > SDC_SD_WIDTH) ||
			(pDipFrm->u32OutHeight > SDC_SD_HEIGHT)) {
			v4l2_err(&dev->v4l2_dev,
				"%s, Check SDC ouptut Size fail, (%d x %d) over SD size\n",
				__func__, pDipFrm->u32OutWidth, pDipFrm->u32OutHeight);
			ret = -EPERM;
		}
	}

	return ret;
}

int _DipSecureCheck(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	EN_AID enAid = E_AID_NS;
	int ret = 0;
	u32 u32DevelopMode = 0;

	if (dev->efuse_develop_mode) {
		if (dev->sys_develop_mode)
			u32DevelopMode = 1;
		else
			u32DevelopMode = 0;
	} else {
		u32DevelopMode = 0;
	}

	if (ctx->u8aid == E_AID_NS) {
		if (ctx->u32OutputSecutiry == 0)
			enAid = E_AID_NS;
		else
			enAid = E_AID_S;
	} else if (ctx->u8aid == E_AID_S) {
		enAid = E_AID_S;
		if (ctx->u32OutputSecutiry == 0) {
			if (u32DevelopMode == 0)
				ret = -EPERM;
		}
	} else if (ctx->u8aid == E_AID_SDC) {
		if (ctx->u32OutputSecutiry == 0) {
			if (u32DevelopMode == 1) {
				enAid = E_AID_S;
			} else {
				if (dev->SDCSupport) {
					ret = _DipCheckSdcSize(ctx);
					if (!ret) {
						enAid = E_AID_SDC;
					} else {
						enAid = E_AID_S;
						v4l2_err(&dev->v4l2_dev,
							"%s, _DipCheckSdcSize fail\n", __func__);
					}
				} else {
					v4l2_err(&dev->v4l2_dev, "Not Support Secure Capture\n");
					enAid = E_AID_S;
					ret = -EPERM;
				}
			}
		} else {
			enAid = E_AID_S;
		}
	}
	ctx->u8WorkingAid = enAid;

	return ret;
}

int _DipTeeSdcCapture(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	int ret = 0;

	DIP_DBG_API("%s, SDC Capture Case\n", __func__);

	if (dev->SDCSupport == 0) {
		v4l2_err(&dev->v4l2_dev, "%s, Not Support secure capture\n", __func__);
		return -EPERM;
	}

	ret = mtk_dip_svc_init(ctx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "mtk_dip_svc_init Fail\n");
		return -EPERM;
	}
	ret = mtk_dip_svc_enable(ctx, 1);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "mtk_dip_svc_enable Fail\n");
		return -EPERM;
	}

	return ret;
}

int _DipTeeSdcMem2Mem(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *src_vb = NULL;
	struct dip_fmt *Srcfmt = NULL;
	struct ST_DIP_B2R_MTDT *pstB2rMtdt = &(ctx->stB2rMtdt);
	u64 u64SrcPhyAddr = 0;
	u32 u32Idx = 0;
	u32 u32NPlanes = 0;
	int ret = 0;

	DIP_DBG_API("%s, SDC Memory to Memory Case\n", __func__);

	if (dev->SDCSupport == 0) {
		v4l2_err(&dev->v4l2_dev, "%s, Not Support secure capture\n", __func__);
		return -EPERM;
	}

	ret = mtk_dip_svc_init(ctx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "mtk_dip_svc_init Fail\n");
		return -EPERM;
	}
	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	if (src_vb == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, v4l2_m2m_next_src_buf Fail\n", __func__);
		return (-1);
	}

	Srcfmt = ctx->in.fmt;
	if (_IsCompressFormat(Srcfmt->fourcc) == true) {
		if (pstB2rMtdt->u32Enable == 0) {
			v4l2_err(&dev->v4l2_dev, "%s,%d,Invalid Metadata\n",
				__func__, __LINE__);
			return (-1);
		}
		u64SrcPhyAddr = mtk_get_addr(&(src_vb->vb2_buf), 0);
		if (u64SrcPhyAddr == INVALID_ADDR) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, address invalid\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (ctx->u32SrcBufMode == E_BUF_IOVA_MODE)
			u64SrcPhyAddr += IOVA_START_ADDR;
		DIP_DBG_BUF("%s, compress base Addr:0x%llx\n", __func__, u64SrcPhyAddr);
		ctx->secure_info.u64InputAddr[E_SVC_IN_ADDR_Y] =
			u64SrcPhyAddr + pstB2rMtdt->u64LumaFbOft;
		ctx->secure_info.u64InputAddr[E_SVC_IN_ADDR_C] =
			u64SrcPhyAddr + pstB2rMtdt->u64ChromaFbOft;
		ctx->secure_info.u64InputAddr[E_SVC_IN_ADDR_YBLN] =
			u64SrcPhyAddr + pstB2rMtdt->u64LumaBitlenOft;
		ctx->secure_info.u64InputAddr[E_SVC_IN_ADDR_CBLN] =
			u64SrcPhyAddr + pstB2rMtdt->u64ChromaBitlenOft;
		ctx->secure_info.u32PlaneCnt = E_SVC_IN_ADDR_MAX;
		ctx->secure_info.u32BufCnt = 1;
		ctx->secure_info.u32InputSize[0] =
			pstB2rMtdt->u64ChromaBitlenOft - pstB2rMtdt->u64LumaFbOft +
			pstB2rMtdt->u32ChromaBitlenSz;
		ctx->secure_info.u8InAddrshift = B2R_CMPS_ADDR_OFT;
	} else {
		u32NPlanes = _GetDIPFmtPlaneCnt(Srcfmt->fourcc);
		if (u32NPlanes > MAX_PLANE_NUM) {
			v4l2_err(&dev->v4l2_dev, "%s,%d,NPlanes(%d) too much\n",
					__func__, __LINE__, u32NPlanes);
			return (-1);
		}
		ctx->secure_info.u32PlaneCnt = u32NPlanes;
		ctx->secure_info.u32BufCnt = u32NPlanes;
		for (u32Idx = 0; u32Idx < MAX_PLANE_NUM ; u32Idx++) {
			if (u32Idx < u32NPlanes) {
				u64SrcPhyAddr = mtk_get_addr(&(src_vb->vb2_buf), u32Idx);
				if (u64SrcPhyAddr == INVALID_ADDR) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, address[%d] invalid\n",
						__func__, __LINE__, u32Idx);
					return -EINVAL;
				}
				if (ctx->u32SrcBufMode == E_BUF_IOVA_MODE)
					u64SrcPhyAddr += IOVA_START_ADDR;
			} else {
				u64SrcPhyAddr = 0;
			}
			ctx->secure_info.u64InputAddr[u32Idx] = u64SrcPhyAddr;
			ctx->secure_info.u32InputSize[u32Idx] = ctx->in.size[u32Idx];
			DIP_DBG_BUF("%s, Addr[%d]:0x%llx, size[%d]:0x%x\n",
				__func__, u32Idx, ctx->secure_info.u64InputAddr[u32Idx],
				u32Idx, ctx->secure_info.u32InputSize[u32Idx]);
		}
		ctx->secure_info.u8InAddrshift = B2R_UNCMPS_ADDR_OFT;
	}
	ret = mtk_dip_svc_enable(ctx, 1);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "mtk_dip_svc_enable Fail\n");
		return -EPERM;
	}

	return ret;
}

int _SetCapTsSetting(struct dip_ctx *ctx, EN_DIP_SRC_FROM enSrcFrom, EN_DIP_IDX enDIPIdx)
{
	EN_CAPPT_TYPE enCappt = E_CAPPT_VDEC;
	int ret = 0;

	ret = _GetCapPtType(ctx->enSource, &enCappt);
	if (ret) {
		v4l2_err(&ctx->dev->v4l2_dev, "_GetCapPtType Fail\n");
		return ret;
	}

	if (enCappt == E_CAPPT_PQ)
		KDrv_DIP_SetWinId(ctx->stWinid.u8Enable, ctx->stWinid.u16WinId, enDIPIdx);
	else
		KDrv_DIP_SetWinId(0, 0, enDIPIdx);

	return ret;
}

int _GetDiprBWLimitSetting(struct dip_ctx *ctx,
	u8 *pu8LimitEna,
	u16 *pu16BaseVal,
	u16 *pu16LimitVal)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pSrcFrm = &(ctx->in);
	struct dip_frame *pDstFrm = &(ctx->out);
	u32 u32PixelCnt = 0;
	u32 u32InputByte = 0;
	u32 u32OutputByte = 0;
	u32 u32TotalByte = 0;

	if (ctx->u8Bwlimit) {
		if ((pSrcFrm->width*pSrcFrm->height) > (pDstFrm->u32OutWidth*pDstFrm->u32OutHeight))
			u32PixelCnt = pSrcFrm->width*pSrcFrm->height;
		else
			u32PixelCnt = pDstFrm->u32OutWidth*pDstFrm->u32OutHeight;

		u32InputByte = MapFramePixelToByte(pSrcFrm->fmt->fourcc,
			pSrcFrm->width*pSrcFrm->height);
		u32OutputByte = MapFramePixelToByte(pDstFrm->fmt->fourcc,
			pDstFrm->u32OutWidth*pDstFrm->u32OutHeight);
		u32TotalByte = u32InputByte + u32OutputByte;
		if (u32TotalByte == 0) {
			v4l2_err(&dev->v4l2_dev, "[%s]Error:RW data is Zero\n", __func__);
			*pu8LimitEna = 0;
			*pu16BaseVal = 1;
			*pu16LimitVal = 1;
			return -EINVAL;
		}
		*pu16BaseVal = BW_BASE_VAL;
		*pu16LimitVal = ((ctx->u64BytePerSec*u32PixelCnt*BW_BASE_VAL)
			/u32TotalByte)/DIP_MAX_CLK;
		if (*pu16BaseVal > *pu16LimitVal)
			*pu8LimitEna = 1;
		else
			*pu8LimitEna = 0;
	} else {
		*pu8LimitEna = 0;
		*pu16BaseVal = 1;
		*pu16LimitVal = 1;
	}

	return 0;
}

void _DipReeCaptureFlowControl(struct dip_ctx *ctx, EN_DIP_IDX enDIPIdx, unsigned int type)
{
	int ret = 0;
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *pSrcfmt = ctx->in.fmt;

	if ((_IsCapCase(ctx->enSource)) == true) {
		KDrv_DIP_SetFlowControl(false, 1, 1, enDIPIdx);
		_DipWorkCapFire(ctx, type);
	} else {
		if (_IsB2rPath(ctx, pSrcfmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R == 0) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
				return;
			}
			KDrv_DIP_SetFlowControl(false, 1, 1, enDIPIdx);
			if ((ctx->u32CapStat & ST_CONT) == ST_CONT) {
				ret = _DipDevRunSetDstAddr(ctx);
				if (ret) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Dst buffer\n",
						__func__, __LINE__);
					ctx->u32M2mStat = ST_OFF;
					return;
				}
				KDrv_DIP_Cont_Ena(true, enDIPIdx);
			} else {
				KDrv_DIP_Cont_Ena(false, enDIPIdx);
			}
		} else {
			ret = _GetDiprBWLimitSetting(ctx,
				&ctx->u8FlowCtrlEna,
				&ctx->u16FlowCtrlBase,
				&ctx->u16FlowCtrlLimit);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Error in BW limit setting\n",
						__func__, __LINE__);
			}
			if (ctx->u8FlowCtrlEna) {
				KDrv_DIP_SetFlowControl(TRUE,
					ctx->u16FlowCtrlBase,
					ctx->u16FlowCtrlLimit,
					enDIPIdx);
			} else {
				KDrv_DIP_SetFlowControl(FALSE,
					ctx->u16FlowCtrlBase,
					ctx->u16FlowCtrlLimit,
					enDIPIdx);
			}
			KDrv_DIP_Cont_Ena(true, enDIPIdx);
		}
		KDrv_DIP_FrameRateCtrl(false, 1, 1, enDIPIdx);
		KDrv_DIP_FrameCnt(1, 0, enDIPIdx);
		KDrv_DIP_Ena(true, enDIPIdx);
	}
}

int _DipReeCapture(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *pDstFrm = _GetDIPFrame(ctx, type);
	struct dip_fmt *pDstfmt = pDstFrm->fmt;
	struct ST_SRC_INFO stSrcInfo = {0};
	struct ST_CROP_WIN stCropWin = {0};
	struct ST_MUX_INFO stMuxInfo = {0};
	struct ST_CSC stCSC = {0};
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	EN_DIP_SRC_FROM enSrcFrom = E_DIP_SRC_FROM_MAX;
	int ret = 0;
	u32 u32MaxWidth = 0;
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;
	u16 u16CovertPitch = 0;

	if (IS_ERR(pDstFrm))
		return PTR_ERR(pDstFrm);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	KDrv_DIP_Init(enDIPIdx);

	_GetSourceSelect(ctx, &enSrcFrom);
	_GetSourceInfo(ctx, enSrcFrom, &stSrcInfo);
	KDrv_DIP_SetSourceInfo(&stSrcInfo, enSrcFrom, enDIPIdx);

	ret = _SetCapTsSetting(ctx, enSrcFrom, enDIPIdx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, SetCapTsSetting Fail\n", __func__);
		return ret;
	}
	stMuxInfo.enSrcFrom = enSrcFrom;
	stMuxInfo.u32DipPixelNum = dev->dipcap_dev.u32DipPixelNum;
	stMuxInfo.u32FrontPack = dev->dipcap_dev.u32FrontPack;
	KDrv_DIP_SetMux(&stMuxInfo, enDIPIdx);

	KDrv_DIP_SetAlpha(ctx->alpha, enDIPIdx);

	KDrv_DIP_SetMirror(ctx->HMirror, ctx->VMirror, enDIPIdx);

	stCropWin.u32XStart = pDstFrm->o_width;
	stCropWin.u32YStart = pDstFrm->o_height;
	stCropWin.u32CropWidth = pDstFrm->c_width;
	stCropWin.u32CropHeight = pDstFrm->c_height;
	KDrv_DIP_SetCrop(&stCropWin, enDIPIdx);

	ret = _DipDiNrSetting(ctx, type);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in DI setting\n",
				__func__, __LINE__);
	}

	ret = _DipScalingSetting(ctx, type);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Scaling setting\n",
				__func__, __LINE__);
	}

	ret = _DipRotateSetting(ctx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Rotate setting\n",
				__func__, __LINE__);
	}

	if (_DipHighWayLRSetting(ctx))
		return -EINVAL;

	_GetDIPWFmtAlignUnit(dev, pDstfmt->fourcc, &u16SizeAlign, &u16PitchAlign);
	ret = _IsAlign(pDstFrm->u32OutWidth, u16SizeAlign);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d]Width(%u) Not Align(%u)\n",
			__func__, __LINE__, pDstFrm->u32OutWidth, u16SizeAlign);
		_AlignAdjust(&pDstFrm->u32OutWidth, u16SizeAlign);
	}

	u16PitchAlign = MapPixelToByte(pDstfmt->fourcc, u16PitchAlign);
	ret = _IsAlign(pDstFrm->u32BytesPerLine[0], u16PitchAlign);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d]Pitch(%u) Not Align(%u)\n",
			__func__, __LINE__, pDstFrm->u32BytesPerLine[0], u16PitchAlign);
		_AlignAdjust(&(pDstFrm->u32BytesPerLine[0]), u16PitchAlign);
	}
	u16CovertPitch = pDstFrm->u32BytesPerLine[0];
	_PitchConvert(pDstfmt->fourcc, &u16CovertPitch);

	u32MaxWidth = _GetDIPWFmtMaxWidth(ctx, pDstfmt->fourcc, enDIPIdx);
	if (pDstFrm->u32OutWidth > u32MaxWidth) {
		if ((dev->dipcap_dev.u32RWSep == 0) ||
			(_IsSupportRWSepFmt(ctx, pDstfmt->fourcc) == false)) {
			v4l2_err(&dev->v4l2_dev, "%s, %d, Out Of Spec Width(%d), Output Max Width(%d)",
				__func__, __LINE__, pDstFrm->u32OutWidth, u32MaxWidth);
			return -EINVAL;
		}
	}
	ret = _DipSetOutputSize(ctx, u16CovertPitch);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d]Invalid Output Size:%d, %d, %d\n",
			__func__, __LINE__,
			pDstFrm->u32OutWidth, pDstFrm->u32OutHeight, u16CovertPitch);
		return -EINVAL;
	}

	ret = _GetDIPCscMapping(ctx, &stSrcInfo, pDstFrm, &stCSC);
	if (ret)
		return -EINVAL;
	KDrv_DIP_SetCSC(&stCSC, enDIPIdx);

	_DipReeCaptureFlowControl(ctx, enDIPIdx, type);

	return 0;
}

int _DipWorkSetting(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = ctx->dev;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (ctx->u8WorkingAid > ctx->u8aid)
		KDrv_DIP_SetAID(ctx->u8WorkingAid, E_HW_DIPW, enDIPIdx);
	else
		KDrv_DIP_SetAID(E_AID_NS, E_HW_DIPW, enDIPIdx);

	ret = _DipReeCapture(ctx, type);
	if (ret)
		return ret;

	return 0;
}

static int dip_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct dip_ctx *ctx = vb2_get_drv_priv(vq);
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *f = _GetDIPFrame(ctx, vq->type);
	int idx;
	u32 u32NPlanes = 0, u32BufMode = 0;

	if (IS_ERR(f)) {
		v4l2_err(&dev->v4l2_dev, "[%s]dip_frame IS_ERR\n", __func__);
		return PTR_ERR(f);
	}

	u32NPlanes = _GetDIPFmtPlaneCnt(f->fmt->fourcc);
	if (u32NPlanes == 0) {
		v4l2_err(&dev->v4l2_dev, "[%s]format 0x%x plane count is zero\n",
			__func__, f->fmt->fourcc);
		return -EINVAL;
	}
	*nplanes = u32NPlanes;

	u32BufMode = _GetDIPBufMode(ctx, vq->type);

	for (idx = 0; idx < *nplanes; idx++) {
		if (u32BufMode <= E_BUF_FRAMEBUF_FD_MODE)
			sizes[idx] = f->size[idx];
		else if (u32BufMode == E_BUF_META_FD_MODE)
			sizes[idx] = 1;

		DIP_DBG_BUF("%s, idx:%d, Sizes:0x%x, Plane:%d\n",
			__func__, idx, sizes[idx], *nplanes);
		if (sizes[idx] == 0) {
			v4l2_err(&dev->v4l2_dev, "[%s]plane[%d] size is zero\n", __func__, idx);
			return -EINVAL;
		}
	}

	return 0;
}

static int dip_buf_prepare(struct vb2_buffer *vb)
{
	struct dip_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *f = _GetDIPFrame(ctx, vb->vb2_queue->type);
	unsigned int idx = 0;
	u32 u32NPlanes = 0, u32BufMode = 0;

	if (IS_ERR(f)) {
		v4l2_err(&dev->v4l2_dev, "[%s]dip_frame IS_ERR\n", __func__);
		return PTR_ERR(f);
	}

	u32BufMode = _GetDIPBufMode(ctx, vb->vb2_queue->type);

	if (u32BufMode <= E_BUF_IOVA_MODE) {
		u32NPlanes = _GetDIPFmtPlaneCnt(f->fmt->fourcc);

		for (idx = 0 ; idx < u32NPlanes; idx++) {
			DIP_DBG_BUF("%s, Idx:%d, Fsize:0x%x, Plane:%d\n",
				__func__, idx, f->size[idx], u32NPlanes);
			vb2_set_plane_payload(vb, idx, f->size[idx]);
		}
	}
	return 0;
}

static void dip_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct dip_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

u32 _GetVspOutHeight(u32 fourcc, u32 u32Height)
{
	u32 u32HeightAlign = 0;
	u32 u32RefactHeight = 0;

	if (_IsYUV420TileFormat(fourcc) == true) {
		if (fourcc == V4L2_PIX_FMT_DIP_MT21S)
			u32HeightAlign = YUV420TL_8B_HEIGHT_WRITE_ALIGN;
		else
			u32HeightAlign = YUV420TL_10B_HEIGHT_WRITE_ALIGN;
		if (u32Height % u32HeightAlign)
			u32RefactHeight = (u32Height/u32HeightAlign)*u32HeightAlign;
		else
			u32RefactHeight = u32Height;
	} else {
		u32RefactHeight = u32Height;
	}

	return u32RefactHeight;
}

int _DipPreCalDiScalUpDnFromDipr(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	struct ST_SCALING *pstScalingDown = NULL;
	struct ST_SCALING *pstScalingHVSP = NULL;
	struct ST_DIP_DINR *pstDiNrInfo = NULL;
	struct ST_RWSEP_INFO *pstRwSep = NULL;
	struct dip_frame *pDipFrm = NULL;
	u32 u32MaxInWidth = 0;
	u32 u32MaxOutWidth = 0;
	bool bHVSPScalingDown = false;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;
	MS_U32 u32HvspMaxWidth = 0;
	MS_U32 u32LimitWidth = 0;

	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return (-EPERM);
	}
	if (ctx->dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return (-EPERM);
	}
	dev = ctx->dev;
	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	pstScalingDown = &(ctx->stScalingDown);
	pstScalingHVSP = &(ctx->stScalingHVSP);
	pstDiNrInfo = &(ctx->stDiNrInfo);
	pstRwSep = &(ctx->stRwSep);
	pDipFrm = &(ctx->out);
	u32HvspMaxWidth = dev->dipcap_dev.u32HvspInW;

	u32MaxInWidth = _GetDIPRFmtMaxWidth(ctx, ctx->in.fmt->fourcc);
	u32MaxOutWidth = _GetDIPWFmtMaxWidth(ctx, ctx->out.fmt->fourcc, enDIPIdx);
	//HSD
	if ((dev->dipcap_dev.u32RWSep)
		&& ((ctx->in.width > u32MaxInWidth) || (ctx->out.width > u32MaxOutWidth))) {
		pstScalingDown->u32InputWidth = pDipFrm->c_width;
		pstScalingDown->u32OutputWidth = pDipFrm->c_width;
		pstScalingDown->u8HScalingEna = 0;
	} else {
		pstScalingDown->u32InputWidth = pDipFrm->c_width;
		if ((pDipFrm->c_width == pDipFrm->u32SclOutWidth) &&
			(pDipFrm->c_height == pDipFrm->u32SclOutHeight)) {
			u32LimitWidth = pDipFrm->c_width;
		} else {
			u32LimitWidth = u32HvspMaxWidth;
		}
		if (pDipFrm->c_width > u32LimitWidth)
			pstScalingDown->u32OutputWidth = u32LimitWidth;
		else
			pstScalingDown->u32OutputWidth = pDipFrm->c_width;

		if (pstScalingDown->u32InputWidth != pstScalingDown->u32OutputWidth)
			pstScalingDown->u8HScalingEna = 1;
		else
			pstScalingDown->u8HScalingEna = 0;
	}

	//DI
	pstDiNrInfo->u32InputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32OutputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32InputHeight = pDipFrm->c_height;
	pstDiNrInfo->u32OutputHeight = pDipFrm->c_height;

	//VSD
	pstScalingDown->u32InputHeight = pstDiNrInfo->u32OutputHeight;
	pstScalingDown->u32OutputHeight = pstDiNrInfo->u32OutputHeight;
	pstScalingDown->u8VScalingEna = 0;

	//HSP
	pstScalingHVSP->u32InputWidth = pstDiNrInfo->u32InputWidth;
	pstScalingHVSP->u32OutputWidth = pDipFrm->u32SclOutWidth;
	if (pstScalingHVSP->u32InputWidth != pstScalingHVSP->u32OutputWidth)
		pstScalingHVSP->u8HScalingEna = 1;
	else
		pstScalingHVSP->u8HScalingEna = 0;
	if (pstScalingHVSP->u32InputWidth > pstScalingHVSP->u32OutputWidth)
		bHVSPScalingDown = true;
	else
		bHVSPScalingDown = false;

	//VSP
	pstScalingHVSP->u32InputHeight = pstScalingDown->u32OutputHeight;
	if (pstScalingHVSP->u32InputHeight != pDipFrm->u32SclOutHeight) {
		pstScalingHVSP->u32OutputHeight = pDipFrm->u32SclOutHeight;
		pstScalingHVSP->u8VScalingEna = 1;
	} else {
		pstScalingHVSP->u32OutputHeight =
			_GetVspOutHeight(pDipFrm->fmt->fourcc, pDipFrm->u32SclOutHeight);
		pstScalingHVSP->u8VScalingEna = 0;
	}

	// RWSEP
	if ((dev->dipcap_dev.u32RWSep)
		&& ((ctx->in.width > u32MaxInWidth) || (ctx->out.width > u32MaxOutWidth))) {
		pstRwSep->bEable = true;
		pstRwSep->bHVSPScalingDown = bHVSPScalingDown;
		pstRwSep->u16Times = DIP_RWSEP_TIMES;
	} else {
		pstRwSep->bEable = false;
	}

	return 0;
}

int _DipPreCalDiScalUpDnFromSrc(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_SCALING *pstScalingDown = &(ctx->stScalingDown);
	struct ST_SCALING *pstScalingHVSP = &(ctx->stScalingHVSP);
	struct ST_DIP_DINR *pstDiNrInfo = &(ctx->stDiNrInfo);
	struct ST_RWSEP_INFO *pstRwSep = &(ctx->stRwSep);
	struct dip_frame *pDipFrm = &(ctx->out);
	MS_U32 u32LimitWidth = 0;
	MS_U32 u32HvspMaxWidth = dev->dipcap_dev.u32HvspInW;

	//HSD
	pstScalingDown->u32InputWidth = pDipFrm->c_width;
	if (ctx->stDiNrInfo.u32OpMode) {
		if (u32HvspMaxWidth > DIP1_DI_IN_WIDTH_MAX)
			u32LimitWidth = DIP1_DI_IN_WIDTH_MAX;
		else
			u32LimitWidth = u32HvspMaxWidth;
	} else {
		if ((pDipFrm->c_width == pDipFrm->u32SclOutWidth) &&
			(pDipFrm->c_height == pDipFrm->u32SclOutHeight)) {
			u32LimitWidth = pDipFrm->c_width;
		} else {
			u32LimitWidth = u32HvspMaxWidth;
		}
	}
	if (pDipFrm->c_width > u32LimitWidth)
		pstScalingDown->u32OutputWidth = u32LimitWidth;
	else
		pstScalingDown->u32OutputWidth = pDipFrm->c_width;
	if (pstScalingDown->u32InputWidth != pstScalingDown->u32OutputWidth)
		pstScalingDown->u8HScalingEna = 1;
	else
		pstScalingDown->u8HScalingEna = 0;

	//DI
	pstDiNrInfo->u32InputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32OutputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32InputHeight = pDipFrm->c_height;
	if (ctx->stDiNrInfo.u32OpMode == E_DIP_MODE_DI)
		pstDiNrInfo->u32OutputHeight = pDipFrm->c_height*2;
	else
		pstDiNrInfo->u32OutputHeight = pDipFrm->c_height;

	//VSD
	pstScalingDown->u32InputHeight = pstDiNrInfo->u32OutputHeight;
	pstScalingDown->u32OutputHeight = pstDiNrInfo->u32OutputHeight;
	pstScalingDown->u8VScalingEna = 0;

	//HSP
	pstScalingHVSP->u32InputWidth = pstDiNrInfo->u32OutputWidth;
	pstScalingHVSP->u32OutputWidth = pDipFrm->u32SclOutWidth;
	if (pstScalingHVSP->u32InputWidth != pstScalingHVSP->u32OutputWidth)
		pstScalingHVSP->u8HScalingEna = 1;
	else
		pstScalingHVSP->u8HScalingEna = 0;

	//VSP
	pstScalingHVSP->u32InputHeight = pstScalingDown->u32OutputHeight;
	if (pstScalingHVSP->u32InputHeight != pDipFrm->u32SclOutHeight) {
		pstScalingHVSP->u32OutputHeight = pDipFrm->u32SclOutHeight;
		pstScalingHVSP->u8VScalingEna = 1;
	} else {
		pstScalingHVSP->u32OutputHeight =
			_GetVspOutHeight(pDipFrm->fmt->fourcc, pDipFrm->u32SclOutHeight);
		pstScalingHVSP->u8VScalingEna = 0;
	}

	// RWSEP
	pstRwSep->bEable = false;

	return 0;
}

int _DipPreCalScalDown(struct dip_ctx *ctx)
{
	struct ST_SCALING *pstScalingDown = &(ctx->stScalingDown);
	struct ST_SCALING *pstScalingHVSP = &(ctx->stScalingHVSP);
	struct ST_DIP_DINR *pstDiNrInfo = &(ctx->stDiNrInfo);
	struct ST_RWSEP_INFO *pstRwSep = &(ctx->stRwSep);
	struct dip_frame *pDipFrm = &(ctx->out);

	//HSD
	pstScalingDown->u32InputWidth = pDipFrm->c_width;
	pstScalingDown->u32OutputWidth = pDipFrm->u32SclOutWidth;
	if (pstScalingDown->u32InputWidth != pstScalingDown->u32OutputWidth)
		pstScalingDown->u8HScalingEna = 1;
	else
		pstScalingDown->u8HScalingEna = 0;

	//DI
	pstDiNrInfo->u32InputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32OutputWidth = pstScalingDown->u32OutputWidth;
	pstDiNrInfo->u32InputHeight = pDipFrm->c_height;
	pstDiNrInfo->u32OutputHeight = pDipFrm->c_height;

	//VSD
	pstScalingDown->u32InputHeight = pDipFrm->c_height;
	pstScalingDown->u32OutputHeight = pDipFrm->u32SclOutHeight;
	if (pstScalingDown->u32InputHeight != pstScalingDown->u32OutputHeight)
		pstScalingDown->u8VScalingEna = 1;
	else
		pstScalingDown->u8VScalingEna = 0;

	//HSP
	pstScalingHVSP->u32InputWidth = pstScalingDown->u32OutputWidth;
	pstScalingHVSP->u32OutputWidth = pstScalingDown->u32OutputWidth;
	if (pstScalingHVSP->u32InputWidth != pstScalingHVSP->u32OutputWidth)
		pstScalingHVSP->u8HScalingEna = 1;
	else
		pstScalingHVSP->u8HScalingEna = 0;

	//VSP
	pstScalingHVSP->u32InputHeight = pstScalingDown->u32OutputHeight;
	pstScalingHVSP->u32OutputHeight = pstScalingDown->u32OutputHeight;
	if (pstScalingHVSP->u32InputHeight != pstScalingHVSP->u32OutputHeight)
		pstScalingHVSP->u8VScalingEna = 1;
	else
		pstScalingHVSP->u8VScalingEna = 0;

	// RWSEP
	pstRwSep->bEable = false;

	return 0;
}

int _DipPreCalDiScalUpDnFromVIP(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	struct ST_SCALING *pstScalingDown = NULL;
	struct ST_SCALING *pstScalingHVSP = NULL;
	struct ST_DIP_DINR *pstDiNrInfo = NULL;
	struct dip_frame *pDipFrm = NULL;
	MS_U32 u32InputWidth = 0, u32InputHeight = 0;
	MS_U32 u32OutputWidth = 0, u32OutputHeight = 0;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	pstScalingDown = &(ctx->stScalingDown);
	pstScalingHVSP = &(ctx->stScalingHVSP);
	pstDiNrInfo = &(ctx->stDiNrInfo);
	pDipFrm = &(ctx->out);

	u32InputWidth = pDipFrm->c_width;
	u32InputHeight = pDipFrm->c_height;
	u32OutputWidth = pDipFrm->u32SclOutWidth;
	u32OutputHeight = pDipFrm->u32SclOutHeight;

	if (u32InputWidth >= u32OutputWidth && u32InputHeight >= u32OutputHeight) {
		//HSD
		if (u32InputWidth != u32OutputWidth)
			pstScalingDown->u8HScalingEna = 1;
		else
			pstScalingDown->u8HScalingEna = 0;
		pstScalingDown->u32InputWidth = u32InputWidth;
		pstScalingDown->u32OutputWidth = u32OutputWidth;

		//DI
		pstDiNrInfo->u32InputWidth = pstScalingDown->u32OutputWidth;
		pstDiNrInfo->u32OutputWidth = pstScalingDown->u32OutputWidth;
		pstDiNrInfo->u32InputHeight = u32InputHeight;
		if (ctx->stDiNrInfo.u32OpMode == E_DIP_MODE_DI)
			pstDiNrInfo->u32OutputHeight = (u32InputHeight << 1);
		else
			pstDiNrInfo->u32OutputHeight = u32InputHeight;

		//VSD
		pstScalingDown->u32InputHeight = pstDiNrInfo->u32OutputHeight;
		if (pstScalingDown->u32InputHeight != u32OutputHeight)
			pstScalingDown->u8VScalingEna = 1;
		else
			pstScalingDown->u8VScalingEna = 0;
		pstScalingDown->u32OutputHeight = pDipFrm->u32SclOutHeight;

		//HVSP
		pstScalingHVSP->u8HScalingEna = 0;
		pstScalingHVSP->u8VScalingEna = 0;
		pstScalingHVSP->u32InputWidth = pstScalingDown->u32OutputWidth;
		pstScalingHVSP->u32InputHeight = pstScalingDown->u32OutputHeight;
		pstScalingHVSP->u32OutputWidth = pstScalingDown->u32OutputWidth;
		pstScalingHVSP->u32OutputHeight = pstScalingDown->u32OutputHeight;
	} else {
		_DipPreCalDiScalUpDnFromSrc(ctx);
	}

	return 0;
}

int _DipPreCalSetting(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;

	if ((dev->dipcap_dev.u32USCL) &&
			(dev->dipcap_dev.u32DSCL) &&
			(dev->dipcap_dev.u323DDI)) {
		if ((ctx->enSource == E_DIP_SOURCE_DIPR) &&
				(_IsB2rPath(ctx, ctx->in.fmt->fourcc) == false)) {
			_DipPreCalDiScalUpDnFromDipr(ctx);
		} else {
			if (ctx->enSource == E_DIP_SOURCE_PQ_BOOST &&
				(dev->dipcap_dev.u32Rotate &
				(ROT90_CAP_SYS_BIT|ROT270_CAP_SYS_BIT)))
				_DipPreCalDiScalUpDnFromVIP(ctx);
			else
				_DipPreCalDiScalUpDnFromSrc(ctx);
		}
	} else if (dev->dipcap_dev.u32DSCL) {
		_DipPreCalScalDown(ctx);
	}

	return 0;
}

static int _Check420Tile10bSupportIova(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	int ret = 0;
	bool bIs420tile10b = false;
	u32 u32fourcc = 0;
	struct vb2_v4l2_buffer *vb = NULL;
	u64 u64Addr = 0;

	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctx is NULL\n", __func__);
		return (-EPERM);
	}
	dev = ctx->dev;

	if (dev->dipcap_dev.u32YUV10bTileIova == 0) {
		if (ctx->enSource == E_DIP_SOURCE_DIPR) {
			u32fourcc = ctx->in.fmt->fourcc;
			bIs420tile10b = _FmtIsYuv10bTile(u32fourcc);
			if (bIs420tile10b == true) {
				vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
				if (vb == NULL) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, Src vb is NULL\n",
						__func__, __LINE__);
					return -EINVAL;
				}
				u64Addr = mtk_get_addr(&(vb->vb2_buf), 0);
				if (u64Addr == INVALID_ADDR) {
					v4l2_err(&dev->v4l2_dev, "%s,%d, address invalid\n",
						__func__, __LINE__);
					return -EINVAL;
				}
				if ((ctx->u32SrcBufMode == E_BUF_IOVA_MODE) ||
					(u64Addr >= IOVA_START_ADDR)) {
					v4l2_err(&dev->v4l2_dev,
						"Input format YUV420 10b tile Not Support IOVA\n");
					ret = (-EPERM);
				}
			}
		}

		u32fourcc = ctx->out.fmt->fourcc;
		bIs420tile10b = _FmtIsYuv10bTile(u32fourcc);
		if (bIs420tile10b == true) {
			vb = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
			if (vb == NULL) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Dst vb is NULL\n",
					__func__, __LINE__);
				return -EINVAL;
			}
			u64Addr = mtk_get_addr(&(vb->vb2_buf), 0);
			if (u64Addr == INVALID_ADDR) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, address invalid\n",
					__func__, __LINE__);
				return -EINVAL;
			}
			if ((ctx->u32DstBufMode == E_BUF_IOVA_MODE) ||
				(u64Addr >= IOVA_START_ADDR)) {
				v4l2_err(&dev->v4l2_dev,
					"Output format YUV420 10b tile Not Support IOVA\n");
				ret = (-EPERM);
			}
		}
	}

	return ret;
}

static int _CheckSupportRotate(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	int ret = 0;
	u32 u32Infourcc = 0;
	u32 u32Outfourcc = 0;

	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctx is NULL\n", __func__);
		return (-EPERM);
	}
	dev = ctx->dev;

	if (ctx->u32Rotation) {
		if ((dev->dipcap_dev.u32Rotate &
			(ROT90_M2M_SYS_BIT|ROT270_M2M_SYS_BIT))) {
			u32Infourcc = ctx->in.fmt->fourcc;
			u32Outfourcc = ctx->out.fmt->fourcc;
			if (u32Infourcc == V4L2_PIX_FMT_DIP_YUV422_8B_BLK_TILE) {
				if (!((u32Outfourcc == V4L2_PIX_FMT_YUYV) ||
					(u32Outfourcc == V4L2_PIX_FMT_YVYU) ||
					(u32Outfourcc == V4L2_PIX_FMT_UYVY) ||
					(u32Outfourcc == V4L2_PIX_FMT_VYUY))) {
					v4l2_err(&dev->v4l2_dev,
						"%s, Invalid Rotate Output Format:0x%x\n",
						__func__, u32Outfourcc);
					ret = (-EPERM);
				}
			} else if (u32Infourcc == V4L2_PIX_FMT_DIP_YUV422_10B_BLK_TILE) {
				if (u32Outfourcc != V4L2_PIX_FMT_DIP_MT21S10T) {
					v4l2_err(&dev->v4l2_dev,
						"%s, Invalid Rotate Output Format:0x%x\n",
						__func__, u32Outfourcc);
					ret = (-EPERM);
				}
			} else {
				v4l2_err(&dev->v4l2_dev,
					"%s, Invalid Rotate input format:0x%x\n",
					__func__, u32Infourcc);
				ret = (-EPERM);
			}
		} else if ((dev->dipcap_dev.u32Rotate &
				(ROT90_CAP_SYS_BIT|ROT270_CAP_SYS_BIT))) {
			u32Outfourcc = ctx->out.fmt->fourcc;
			if (u32Outfourcc != V4L2_PIX_FMT_DIP_YUYV_ROT) {
				v4l2_err(&dev->v4l2_dev,
					"%s, Invalid Rotate Output Format:0x%x\n",
					__func__, u32Outfourcc);
				ret = (-EPERM);
			}
		} else {
			v4l2_err(&dev->v4l2_dev, "%s, Not Support Rotate\n", __func__);
			ret = (-EPERM);
		}
	}

	return ret;
}

static int _CheckSupportInOutSz(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	u32 u32MaxInWidth = 0;
	u32 u32MaxOutWidth = 0;
	int ret = 0;

	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctx is NULL\n", __func__);
		return -EPERM;
	}
	if (ctx->dev == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, dev is NULL\n", __func__);
		return -EPERM;
	}
	dev = ctx->dev;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Not Found DIP number\n",
					__func__, __LINE__);
		return ret;
	}

	u32MaxInWidth = _GetDIPRFmtMaxWidth(ctx, ctx->in.fmt->fourcc);
	u32MaxOutWidth = _GetDIPWFmtMaxWidth(ctx, ctx->out.fmt->fourcc, enDIPIdx);

	if ((_IsM2mCase(ctx->enSource)) == true) {
		if ((ctx->in.width > u32MaxInWidth) || (ctx->out.width > u32MaxOutWidth)) {
			if (dev->dipcap_dev.u32RWSep) {
				if (ctx->out.u32SclOutWidth > ctx->out.c_width) {
					v4l2_err(&dev->v4l2_dev, "%s, not support RW Separate\n",
						__func__);
					ret = (-EPERM);
				} else {
					v4l2_err(&dev->v4l2_dev,
						"%s, RW Separate not support scaling up\n",
						__func__);
				}
			}
		} else {
			ret = 0;
		}
	} else {
		ret = 0;
	}

	return ret;
}

static int _CheckHWSupport(struct dip_ctx *ctx)
{
	int ret = 0;

	ret |= _Check420Tile10bSupportIova(ctx);

	ret |= _CheckSupportRotate(ctx);

	ret |= _CheckSupportInOutSz(ctx);

	return ret;
}

static int AllocateDiBufferCheck(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	int ret = 0;
	dma_addr_t iova = 0;
	size_t DiBufSize = 0;

	if (gstDiNrBufInfo.DmaAddr == 0) {
		if (ctx->stDiNrInfo.u32OpMode == E_DIP_MODE_DI) {
			DiBufSize = (ctx->stDiNrInfo.u32InputWidth)*
				(ctx->stDiNrInfo.u32InputHeight)*
				DIP_DIDNR_BPP*
				DIP_DI_CNT;
		} else {
			DiBufSize = (ctx->stDiNrInfo.u32InputWidth)*
				(ctx->stDiNrInfo.u32InputHeight)*
				DIP_DIDNR_BPP*
				DIP_DNR_CNT;
		}
		ret = _DipAllocateBuffer(ctx,
				dev->di_buf_tag,
				&iova,
				DiBufSize);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "Allocate DI Buffer Fail\n");
			return -EPERM;
		}
	}

	return ret;
}

static u64 CalcDiprFullBw(struct dip_frame *pDipDstFrm, struct dip_frame *pDipSrcFrm)
{
	u32 u32InputPixel = 0;
	u32 u32OutputPixel = 0;
	u32 u32PixelCnt = 0;
	u32 u32InputByte = 0;
	u32 u32OutputByte = 0;
	u32 u32TotalByte = 0;
	u64 u64RealBWVal = 0;
	u32 u32Fps = 0;

	u32InputPixel = pDipSrcFrm->width*pDipSrcFrm->height;
	u32OutputPixel = pDipDstFrm->u32OutWidth*pDipDstFrm->u32OutHeight;
	if (u32InputPixel > u32OutputPixel)
		u32PixelCnt = u32InputPixel;
	else
		u32PixelCnt = u32OutputPixel;

	u32InputByte = MapFramePixelToByte(pDipSrcFrm->fmt->fourcc, u32InputPixel);
	u32OutputByte = MapFramePixelToByte(pDipDstFrm->fmt->fourcc, u32OutputPixel);
	u32TotalByte = u32InputByte + u32OutputByte;
	if (u32TotalByte != 0) {
		u32Fps = DIP_MAX_CLK/u32PixelCnt;
		u64RealBWVal = u32Fps*u32TotalByte;
	} else {
		u64RealBWVal = 0;
	}

	return u64RealBWVal;
}

static u64 CalcB2rRealBw(struct dip_frame *pDipDstFrm, struct dip_frame *pDipSrcFrm, u32 u32B2rFps)
{
	u32 u32InputPixel = 0;
	u32 u32OutputPixel = 0;
	u32 u32InputByte = 0;
	u32 u32OutputByte = 0;
	u32 u32TotalByte = 0;
	u64 u64RealBWVal = 0;

	u32InputPixel = pDipDstFrm->width*pDipDstFrm->height;
	u32OutputPixel = pDipDstFrm->u32OutWidth*pDipDstFrm->u32OutHeight;
	u32InputByte = MapFramePixelToByte(pDipSrcFrm->fmt->fourcc, u32InputPixel);
	u32OutputByte = MapFramePixelToByte(pDipDstFrm->fmt->fourcc, u32OutputPixel);
	u32TotalByte = u32InputByte + u32OutputByte;
	u64RealBWVal = (u64)u32TotalByte*u32B2rFps;

	return u64RealBWVal;
}

static void RecordExecute(struct dip_ctx *ctx)
{
	struct dip_dev *Dipdev = NULL;
	struct dip_frame *pDipSrcFrm = NULL;
	struct dip_frame *pDipDstFrm = NULL;
	u8 u8idx = 0;
	u64 u64BWVal = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return;
	}
	Dipdev = ctx->dev;
	if (Dipdev == NULL) {
		DIP_ERR("%s, Dipdev is NULL\n", __func__);
		return;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return;

	if (enDIPIdx != E_DIPR_BLEND_IDX0) {
		pDipDstFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		if (IS_ERR(pDipDstFrm)) {
			DIP_ERR("%s, get output buffer fail\n", __func__);
			return;
		}
	}

	if (Dipdev->exe_log_cur_idx >= EXE_LOG_CNT)
		Dipdev->exe_log_cur_idx = 0;

	u8idx = Dipdev->exe_log_cur_idx;

	if (enDIPIdx != E_DIPR_BLEND_IDX0) {
		if ((_IsM2mCase(ctx->enSource)) == true) {
			pDipSrcFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
			if (IS_ERR(pDipSrcFrm)) {
				DIP_ERR("%s, get input buffer fail\n", __func__);
				return;
			}
			if (_IsB2rPath(ctx, pDipSrcFrm->fmt->fourcc) == true) {
				if (Dipdev->dipcap_dev.u32B2R == 0) {
					v4l2_err(&Dipdev->v4l2_dev, "%s,%d, Not Support B2R\n",
						__func__, __LINE__);
					return;
				}
				Dipdev->exe_log[u8idx].u64RealBWVal =
					CalcB2rRealBw(pDipDstFrm, pDipSrcFrm, ctx->u32B2rFps);
				Dipdev->exe_log[u8idx].u8IsB2RLite = 1;
				Dipdev->exe_log[u8idx].u64B2RClockRate = ctx->u64B2RClockRate;
			} else {
				u64BWVal = CalcDiprFullBw(pDipDstFrm, pDipSrcFrm);
				Dipdev->exe_log[u8idx].u8FlowCtrlEna = ctx->u8FlowCtrlEna;
				Dipdev->exe_log[u8idx].u16FlowCtrlBase = ctx->u16FlowCtrlBase;
				Dipdev->exe_log[u8idx].u16FlowCtrlLimit = ctx->u16FlowCtrlLimit;
				if (ctx->u8FlowCtrlEna != 0) {
					Dipdev->exe_log[u8idx].u64RealBWVal =
						(u64BWVal*ctx->u16FlowCtrlLimit)/
						ctx->u16FlowCtrlBase;
				} else {
					Dipdev->exe_log[u8idx].u64RealBWVal = u64BWVal;
				}
				Dipdev->exe_log[u8idx].u8IsB2RLite = 0;
			}
			Dipdev->exe_log[u8idx].u32InputWidth = pDipSrcFrm->width;
			Dipdev->exe_log[u8idx].u32InputHeight = pDipSrcFrm->height;
			Dipdev->exe_log[u8idx].u32Inputfourcc = pDipSrcFrm->fmt->fourcc;
		} else {
			if (ctx->u32SourceFps != 0) {
				u64BWVal =
					(u64)MapFramePixelToByte(pDipDstFrm->fmt->fourcc,
					pDipDstFrm->width*pDipDstFrm->height)*
					(u64)ctx->u32SourceFps;
				if ((ctx->u32FrcInRatio != 0) || (ctx->u32FrcOutRatio != 0)) {
					if (ctx->u32FrcInRatio != 0)
						Dipdev->exe_log[u8idx].u64RealBWVal =
						((u64)u64BWVal*ctx->u32FrcOutRatio)/
						(u64)ctx->u32FrcInRatio;
					else
						Dipdev->exe_log[u8idx].u64RealBWVal = 0;
				} else {
					Dipdev->exe_log[u8idx].u64RealBWVal = u64BWVal;
				}
			} else {
				//skip because of no fps info
				return;
			}
			Dipdev->exe_log[u8idx].u32InputWidth = ctx->stSrcInfo.u32Width;
			Dipdev->exe_log[u8idx].u32InputHeight = ctx->stSrcInfo.u32Height;
			Dipdev->exe_log[u8idx].u32Inputfourcc = ctx->stSrcInfo.enSrcClr;
		}

		Dipdev->exe_log[u8idx].u8BWLimitEna = ctx->u8Bwlimit;
		Dipdev->exe_log[u8idx].u64BWLimitVal = ctx->u64BytePerSec;
		Dipdev->exe_log[u8idx].u32PqWinTime = ctx->u32PqWinTime;
		Dipdev->exe_log[u8idx].u64PqRemainderBudget = ctx->u64PqRemainderBudget;
		Dipdev->exe_log[u8idx].enSource = ctx->enSource;
		Dipdev->exe_log[u8idx].u32outputWidth = pDipDstFrm->width;
		Dipdev->exe_log[u8idx].u32OutputHeight = pDipDstFrm->height;
		Dipdev->exe_log[u8idx].u32Outputfourcc = pDipDstFrm->fmt->fourcc;
	} else {
		pDipSrcFrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		Dipdev->exe_log[u8idx].u32InputWidth = pDipSrcFrm->width;
		Dipdev->exe_log[u8idx].u32InputHeight = pDipSrcFrm->height;
		Dipdev->exe_log[u8idx].u32Inputfourcc = pDipSrcFrm->fmt->fourcc;
		Dipdev->exe_log[u8idx].u32outputWidth = pDipSrcFrm->u32SclOutWidth;
		Dipdev->exe_log[u8idx].u32OutputHeight = pDipSrcFrm->u32SclOutHeight;
		Dipdev->exe_log[u8idx].u16BlendDispX = ctx->stBlendDispInfo.u16DispX;
		Dipdev->exe_log[u8idx].u16BlendDispY = ctx->stBlendDispInfo.u16DispY;
		Dipdev->exe_log[u8idx].enBlendDstClrFmt = ctx->stBlendDispInfo.enDstClrFmt;
	}

	Dipdev->exe_log[u8idx].u16Rotation = (u16)ctx->u32Rotation;

	ktime_get_real_ts64(&Dipdev->exe_log[u8idx].EndTime);
	Dipdev->exe_log[u8idx].StartTime.tv_sec = StartTmpTime.tv_sec;
	Dipdev->exe_log[u8idx].StartTime.tv_nsec = StartTmpTime.tv_nsec;

	if ((Dipdev->exe_log_cur_idx+1) >= EXE_LOG_CNT)
		Dipdev->exe_log_cur_idx = 0;
	else
		Dipdev->exe_log_cur_idx++;
}

static void FreeDiBufferCheck(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;

	if ((gstDiNrBufInfo.DmaAddr != 0) &&
		(gstDiNrBufInfo.virtAddr != NULL)) {
		dma_free_attrs(dev->dev,
				gstDiNrBufInfo.u32Size,
				gstDiNrBufInfo.virtAddr,
				gstDiNrBufInfo.DmaAddr,
				gstDiNrBufInfo.dma_attrs);
		gstDiNrBufInfo.DmaAddr = 0;
		gstDiNrBufInfo.virtAddr = NULL;
	}
}

int Dipr_SetBlendingSrcInfo(struct dip_ctx *ctx, unsigned int type)
{
	struct dip_dev *dev = NULL;
	struct vb2_v4l2_buffer *src_vb = NULL;
	struct dip_frame *Srcfrm = NULL;
	struct ST_SCALING stScalingHVSP = {0};
	struct ST_CSC stCSC = {0};
	struct ST_SRC_INFO stSrcInfo = {0};
	u32 u32NPlanes = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (!ctx) {
		pr_err("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;

	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	src_vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	if (src_vb == NULL)
		return -ENXIO;

	if (Dip_GetDMABufInfo(ctx, src_vb, type)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,Dip_GetDMABufInfo fail\n",
				__func__, __LINE__);
		return -EPERM;
	}

	_GetSourceInfo(ctx, E_DIP_SRC_FROM_DIPR, &stSrcInfo);
	if (!KDrv_DIP_SetSourceInfo(&stSrcInfo, E_DIP_SRC_FROM_DIPR, E_DIPR_BLEND_IDX0)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, KDrv_DIP_SetSourceInfo fail\n",
				__func__, __LINE__);
		return -EPERM;
	}

	if (_DipSetSource(ctx)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,_DipSetSource fail\n",
				__func__, __LINE__);
		return -EPERM;
	}

	Srcfrm = _GetDIPFrame(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	if (IS_ERR(Srcfrm)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,_GetDIPFrame fail\n",
				__func__, __LINE__);
		return PTR_ERR(Srcfrm);
	}

	if (Srcfrm->width < Srcfrm->u32SclOutWidth || Srcfrm->height < Srcfrm->u32SclOutHeight) {
		if (_DipGetHvspResource(ctx, E_DIP_SHARE_DIP1_HVSP, E_DIPR_BLEND_IDX0)) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, src < dst, but no HVSP resource\n",
				__func__, __LINE__);
			return -EBUSY;
		}
		stScalingHVSP.u32InputWidth = Srcfrm->width;
		stScalingHVSP.u32InputHeight = Srcfrm->height;
		stScalingHVSP.u32OutputWidth = Srcfrm->u32SclOutWidth;
		stScalingHVSP.u32OutputHeight = Srcfrm->u32SclOutHeight;
		if (stScalingHVSP.u32InputWidth != stScalingHVSP.u32OutputWidth)
			stScalingHVSP.u8HScalingEna = true;

		if (stScalingHVSP.u32InputHeight != stScalingHVSP.u32OutputHeight)
			stScalingHVSP.u8VScalingEna = true;

		KDrv_DIP_SetHVSP(&stScalingHVSP, enDIPIdx);
	}

	_FillCSCInfo(&stCSC, Srcfrm, &ctx->stBlendDispInfo);
	KDrv_DIP_SetCSC(&stCSC, E_DIPR_BLEND_IDX0);

	if (_DipRotateSetting(ctx)) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Rotate setting\n",
				__func__, __LINE__);
		return -EPERM;
	}

	u32NPlanes = _GetDIPFmtPlaneCnt(Srcfrm->fmt->fourcc);
	_DipSetSrcUdmaLimitRange(ctx, Srcfrm->u64PhyAddr, true);
	KDrv_DIP_SetSrcAddr(u32NPlanes, Srcfrm->u64PhyAddr, E_DIPR_BLEND_IDX0);

	return 0;
}

int Dipr_SetBlending(struct dip_ctx *ctx, unsigned int type, bool bEnable)
{
	struct ST_DIP_BLEND_INFO stDipBlendInfo;
	struct ST_VB_INFO stVBInfo;
	struct dip_frame *pDipFrm;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	pDipFrm = _GetDIPFrame(ctx, type);

	memset(&stVBInfo, 0, sizeof(struct ST_VB_INFO));
	if (bEnable) {
		stVBInfo.bSrc1En = true;
		stVBInfo.u16Hstart = ctx->stBlendDispInfo.u16DispX;
		stVBInfo.u16Hend = stVBInfo.u16Hstart + pDipFrm->u32SclOutWidth;
		stVBInfo.u16Vstart = ctx->stBlendDispInfo.u16DispY;
		stVBInfo.u16Vend = stVBInfo.u16Vstart + pDipFrm->u32SclOutHeight;
		stVBInfo.u16Src1Alpha = (u16)ctx->alpha;
		stVBInfo.enVbColor = (EN_DIP_SRC_COLOR)ctx->stBlendDispInfo.enDstClrFmt;
		stVBInfo.u16Background_A = ctx->stBlendDispInfo.u16Background_A;
		stVBInfo.u16Background_R = ctx->stBlendDispInfo.u16Background_R;
		stVBInfo.u16Background_G = ctx->stBlendDispInfo.u16Background_G;
		stVBInfo.u16Background_B = ctx->stBlendDispInfo.u16Background_B;
	} else {
		stVBInfo.bSrc1En = false;
	}
	KDrv_DIP_SetVBInfo(&stVBInfo, E_DIPR_BLEND_IDX0);

	memset(&stDipBlendInfo, 0, sizeof(struct ST_DIP_BLEND_INFO));
	stDipBlendInfo.bBlendEnable = bEnable;
	if (bEnable) {
		stDipBlendInfo.u16PIPWidth = pDipFrm->u32SclOutWidth;
		stDipBlendInfo.u16PIPHeight = pDipFrm->u32SclOutHeight;
		stDipBlendInfo.eBlendDst = E_DIP_BLEND_AFTER_VIP;
	} else {
		stDipBlendInfo.u16PIPWidth = 0;
		stDipBlendInfo.u16PIPHeight = 0;
	}

	KDrv_DIP_SetBlending(&stDipBlendInfo, E_DIPR_BLEND_IDX0);

	return 0;
}

static int dip_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct dip_ctx *ctx = NULL;
	struct dip_dev *dev = NULL;
	unsigned long flags = 0;
	int ret = 0;
	struct hwregOut stDipOut;

	ctx = vb2_get_drv_priv(vq);
	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENODATA;
	}
	dev = ctx->dev;
	if (dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENODATA;
	}

	DIP_DBG_API("%s,%d\n", __func__, __LINE__);

	if (_IsDstBufType(vq->type) == true) {
		ret = _CheckHWSupport(ctx);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "[%s]HW Not Support\n", __func__);
			return ret;
		}
		_DipPreCalSetting(ctx);

		if (ctx->stDiNrInfo.u32OpMode) {
			ret = AllocateDiBufferCheck(ctx);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "AllocateDiBufferCheck Fail\n");
				return ret;
			}
		}

		ret = _DipSecureCheck(ctx);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "Check Dip Secure Fail\n");
			goto free_buffer;
		}
		if ((_IsCapCase(ctx->enSource)) == true) {
			if (ctx->u8WorkingAid == E_AID_SDC) {
				if (_IsSdcCapCase(ctx->enSource) == true) {
					ret = _DipTeeSdcCapture(ctx);
					if (ret) {
						v4l2_err(&dev->v4l2_dev,
							"%s, Secure Capture Fail\n", __func__);
						goto free_buffer;
					}
				} else {
					v4l2_err(&dev->v4l2_dev,
						"%s, enSource:%d, Not Support secure capture\n",
						__func__, ctx->enSource);
					ctx->u8WorkingAid = E_AID_S;
					ret = -EPERM;
					goto free_buffer;
				}
			}
		}
		/* bw record */
		SaveExcuteStart();

		ret = _SetDipInOutClk(ctx);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, Error in set DIP I/O Clock\n",
				__func__, __LINE__);
			ret = -EPERM;
			goto free_buffer;
		}

		spin_lock_irqsave(&ctx->dev->ctrl_lock, flags);
		if ((_IsM2mCase(ctx->enSource)) == true) {
			ret = _DipSetSource(ctx);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Src Setting\n",
					__func__, __LINE__);
				spin_unlock_irqrestore(&ctx->dev->ctrl_lock, flags);
				ret = -EPERM;
				goto free_buffer;
			}
		}
		ret = _DipWorkSetting(ctx, vq->type);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Capture Setting\n",
				__func__, __LINE__);
			spin_unlock_irqrestore(&ctx->dev->ctrl_lock, flags);
			goto free_buffer;
		}
		spin_unlock_irqrestore(&ctx->dev->ctrl_lock, flags);
	}

	if (dev->variant->u16DIPIdx == DIPR_BLEND_WIN_IDX) {
		if (_IsSrcBufType(vq->type) == false) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, Not support queue type %d\n",
				__func__, __LINE__, vq->type);
			ret = -EPERM;
			goto free_buffer;
		}

		memset(&stDipOut, 0, sizeof(struct hwregOut));
		ret = Dip_Menuload_CreateRes(ctx, 1, MAX_ML_CMD, E_SM_ML_OP2_SYNC);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_CreateRes");

		ret |= Dip_Menuload_Begin(ctx);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_Begin");

		ret |= Dipr_SetBlendingSrcInfo(ctx, vq->type);
		DIP_CHECK_RETURN(ret, "Dipr_SetBlendingSrcInfo");

		ret |= Dipr_SetBlending(ctx, vq->type, true);
		DIP_CHECK_RETURN(ret, "Dipr_SetBlending");

		ret |= Dip_Menuload_End(ctx, &stDipOut);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_End");

		ret |= Dip_Menuload_Fire(ctx, &stDipOut);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_Fire");
		SaveExcuteStart();
	}

	return ret;

free_buffer:
	if (ctx->stDiNrInfo.u32OpMode)
		FreeDiBufferCheck(ctx);
	return ret;
}

static void dip_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct dip_ctx *ctx = NULL;
	struct dip_dev *dev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	unsigned long flags;
	EN_DIP_SRC_FROM enSrcFrom = E_DIP_SRC_FROM_MAX;
	int ret = 0;
	struct hwregOut stDipOut;

	ctx = vb2_get_drv_priv(vq);
	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctx is NULL\n", __func__);
		return;
	}
	dev = ctx->dev;
	if (dev == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, dev is NULL\n", __func__);
		return;
	}

	DIP_DBG_API("%s,%d\n", __func__, __LINE__);

	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX)
		enDIPIdx = E_DIP_IDX0;
	else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX)
		enDIPIdx = E_DIP_IDX1;
	else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX)
		enDIPIdx = E_DIP_IDX2;
	else if (dev->variant->u16DIPIdx == DIPR_BLEND_WIN_IDX)
		enDIPIdx = E_DIPR_BLEND_IDX0;

	if (_IsDstBufType(vq->type) == true) {
		_GetSourceSelect(ctx, &enSrcFrom);
		_SetDIPClkParent(dev, enSrcFrom, false);
	}
	if (_IsSrcBufType(vq->type) == true) {
		if (_IsB2rPath(ctx, ctx->in.fmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R) {
				_SetB2rClk(dev, false, 0);
			} else {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
			}
		}
	}
	spin_lock_irqsave(&ctx->dev->ctrl_lock, flags);
	/* stop hardware streaming */
	if (_IsDstBufType(vq->type) == true) {
		KDrv_DIP_Ena(false, enDIPIdx);
		_DipSetDstUdmaLimitRange(ctx, NULL, false);
		_DipReleaseHvspResource(ctx, E_DIP_SHARE_DIP1_HVSP,
			enDIPIdx);
	}
	if (_IsSrcBufType(vq->type) == true) {
		if (_IsB2rPath(ctx, ctx->in.fmt->fourcc) == true) {
			if (dev->dipcap_dev.u32B2R) {
				KDrv_DIP_B2R_Ena(false);
			} else {
				v4l2_err(&dev->v4l2_dev, "%s,%d, Not Support B2R\n",
					__func__, __LINE__);
			}
		}
		_DipSetSrcUdmaLimitRange(ctx, NULL, false);
	}
	spin_unlock_irqrestore(&ctx->dev->ctrl_lock, flags);

	if (ctx->stDiNrInfo.u32OpMode)
		FreeDiBufferCheck(ctx);

	if (_IsDstBufType(vq->type) == true) {
		if (ctx->u8WorkingAid == E_AID_SDC)
			mtk_dip_svc_exit(ctx);
	}

	if (enDIPIdx == E_DIPR_BLEND_IDX0) {
		memset(&stDipOut, 0, sizeof(struct hwregOut));
		ret = Dip_Menuload_Begin(ctx);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_Begin");

		_DipReleaseHvspResource(ctx, E_DIP_SHARE_DIP1_HVSP,
			E_DIPR_BLEND_IDX0);

		ret = Dipr_SetBlending(ctx, vq->type, false);
		DIP_CHECK_RETURN(ret, "Dipr_SetBlending");

		ret = Dip_Menuload_End(ctx, &stDipOut);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_End");

		ret = Dip_Menuload_Fire(ctx, &stDipOut);
		DIP_CHECK_RETURN(ret, "Dip_Menuload_Fire");

		Dip_Menuload_ReleaseRes(ctx);
	}
}

static const struct vb2_ops dip_qops = {
	.queue_setup     = dip_queue_setup,
	.buf_prepare     = dip_buf_prepare,
	.buf_queue       = dip_buf_queue,
	.start_streaming = dip_vb2_start_streaming,
	.stop_streaming  = dip_vb2_stop_streaming,
};

static void *mtk_vb2_cookie(void *buf_priv)
{
	struct mtk_vb2_buf *buf = NULL;
	struct dip_dev *Dipdev = NULL;
	struct dip_ctx *ctx = NULL;
	u32 u32BufMode = 0;

	if (!buf_priv)	{
		DIP_ERR("%s, buf_priv is NULL\n", __func__);
		return NULL;
	}
	buf = buf_priv;
	Dipdev = dev_get_drvdata(buf->dev);
	if (!Dipdev)	{
		DIP_ERR("%s, Dipdev is NULL\n", __func__);
		return NULL;
	}

	ctx = Dipdev->ctx;
	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return NULL;
	}

	if (buf->dma_dir == DMA_INPUT)
		u32BufMode = ctx->u32SrcBufMode;
	else
		u32BufMode = ctx->u32DstBufMode;

	if (u32BufMode <= E_BUF_IOVA_MODE)
		return &buf->paddr;
	else if (u32BufMode == E_BUF_FRAMEBUF_FD_MODE)
		return buf->dma_sgt;
	else
		return NULL;
}

static void *mtk_vb2_get_userptr(struct device *dev, unsigned long vaddr,
			unsigned long size, enum dma_data_direction dma_dir)
{
	struct mtk_vb2_buf *buf;
	struct dip_dev *Dipdev = NULL;
	struct dip_ctx *ctx = NULL;

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL)	{
		DIP_ERR("%s, Dipdev is NULL\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	ctx = Dipdev->ctx;
	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dma_dir = dma_dir;
	buf->dev = dev;
	if (dma_dir == DMA_INPUT) {
		if (ctx->u32SrcBufMode == E_BUF_PA_MODE) {
			buf->paddr = vaddr;
		} else {
			buf->paddr = vaddr;
			//fixme
		}
	}
	if (dma_dir == DMA_OUTPUT) {
		if (ctx->u32DstBufMode == E_BUF_PA_MODE) {
			buf->paddr = vaddr;
		} else {
			buf->paddr = vaddr;
			//fixme
		}
	}
	buf->size = size;

	return buf;
}

static void mtk_vb2_put_userptr(void *buf_priv)
{
	struct mtk_vb2_buf *buf = buf_priv;

	kfree(buf);
}

static void *mtk_vb2_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
	unsigned long size, enum dma_data_direction dma_dir)
{
	struct mtk_vb2_buf *buf;
	struct dma_buf_attachment *dba;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	if (dbuf->size < size) {
		DIP_ERR("dbuf->size=0x%zx < size=0x%lx fail\n", dbuf->size, size);
		return ERR_PTR(-EFAULT);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		DIP_ERR("failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = dma_dir;
	buf->size = size;
	buf->db_attach = dba;
	buf->vaddr = NULL;

	return buf;
}

static void *mtk_vb2_vaddr(void *buf_priv)
{
	struct mtk_vb2_buf *buf = NULL;

	if (!buf_priv)	{
		DIP_ERR("%s, buf_priv is NULL\n", __func__);
		return NULL;
	}

	buf = buf_priv;

	return buf->vaddr ? buf->vaddr:NULL;
}

static int mtk_vb2_map_dmabuf(void *buf_priv)
{
	struct mtk_vb2_buf *buf = NULL;
	struct sg_table *sgt;

	if (!buf_priv)	{
		DIP_ERR("%s, buf_priv is NULL\n", __func__);
		return -ENXIO;
	}
	buf = buf_priv;

	if (WARN_ON(!buf->db_attach)) {
		DIP_ERR("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		DIP_ERR("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */

	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	if (IS_ERR(sgt)) {
		DIP_ERR("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	buf->dma_sgt = sgt;
	if (!buf->vaddr)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);

	return 0;
}

static void mtk_vb2_unmap_dmabuf(void *buf_priv)
{
	struct mtk_vb2_buf *buf = NULL;
	struct sg_table *sgt = NULL;

	if (!buf_priv)	{
		DIP_ERR("%s, buf_priv is NULL\n", __func__);
		return;
	}
	buf = buf_priv;
	sgt = buf->dma_sgt;

	if (WARN_ON(!buf->db_attach)) {
		DIP_ERR("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		DIP_ERR("dmabuf buffer is already unpinned\n");
		return;
	}

	if (buf->vaddr) {
		dma_buf_vunmap(buf->db_attach->dmabuf, buf->vaddr);
		buf->vaddr = NULL;
	}
	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

	buf->dma_sgt = NULL;
}

static void mtk_vb2_detach_dmabuf(void *buf_priv)
{
	struct mtk_vb2_buf *buf = NULL;

	if (!buf_priv)	{
		DIP_ERR("%s, buf_priv is NULL\n", __func__);
		return;
	}
	buf = buf_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_sgt))
		mtk_vb2_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

const struct vb2_mem_ops mtk_vb2_alloc_memops = {
	.get_userptr    = mtk_vb2_get_userptr,
	.put_userptr    = mtk_vb2_put_userptr,
	.cookie         = mtk_vb2_cookie,
	.vaddr		= mtk_vb2_vaddr,
	.map_dmabuf = mtk_vb2_map_dmabuf,
	.unmap_dmabuf = mtk_vb2_unmap_dmabuf,
	.attach_dmabuf = mtk_vb2_attach_dmabuf,
	.detach_dmabuf = mtk_vb2_detach_dmabuf,
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
						struct vb2_queue *dst_vq)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	int ret = 0;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->ops = &dip_qops;
	src_vq->mem_ops = &mtk_vb2_alloc_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->dev->mutex;
	src_vq->min_buffers_needed = 1;
	src_vq->dev = ctx->dev->v4l2_dev.dev;
	ret = vb2_queue_init(src_vq);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d], src_vq Init Fail, ret =%d\n",
				__func__, __LINE__, ret);
		return ret;
	}

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &dip_qops;
	dst_vq->mem_ops = &mtk_vb2_alloc_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->dev->mutex;
	dst_vq->min_buffers_needed = 1;
	dst_vq->dev = ctx->dev->v4l2_dev.dev;
	ret = vb2_queue_init(dst_vq);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "[%s,%d], dst_vq Init Fail, ret =%d\n",
				__func__, __LINE__, ret);
		return ret;
	}

	return ret;
}

int _UpdateSourceInfo(struct dip_ctx *ctx, struct ST_DIP_SOURCE_INFO *pstSrcInfo)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_METADATA stMetaData;
	struct mdseal_ioc_data sealdata;
	int ret = 0;
	unsigned int idx = 0;

	DIP_DBG_API("Source Width:%d, Height:%d, Fps:%d, ClrRange:%d, ClrSpace:%d\n",
		pstSrcInfo->u32Width, pstSrcInfo->u32Height, ctx->u32SourceFps,
		ctx->stSrcInfo.enClrRange, ctx->stSrcInfo.enClrSpace);

	DIP_DBG_API("u8IsValidFd:%d, MetaFd:%d, u32PipeLineId:%d, u8IsSecutiry:%d\n",
		pstSrcInfo->u8IsValidFd, pstSrcInfo->MetaFd,
		pstSrcInfo->u32PipeLineId, pstSrcInfo->u8IsSecutiry);

	if ((pstSrcInfo->u32Version == DIP_SOURCE_INFO_VER) &&
		(pstSrcInfo->u32Length == sizeof(struct ST_DIP_SOURCE_INFO))) {
		//General Iinfo
		ctx->stSrcInfo.u8PixelNum = pstSrcInfo->u8PxlEngine;
		ctx->stSrcInfo.enSrcClr = (EN_DIP_SRC_COLOR)pstSrcInfo->enClrFmt;
		ctx->stSrcInfo.u32Width = pstSrcInfo->u32Width;
		ctx->stSrcInfo.u32Height = pstSrcInfo->u32Height;
		ctx->stSrcInfo.u32DisplayWidth = pstSrcInfo->u32Width;
		ctx->stSrcInfo.u32DisplayHeight = pstSrcInfo->u32Height;
		ctx->u32SourceFps = pstSrcInfo->u32Fps;
		ctx->stSrcInfo.enClrRange = (enum EN_DIP_CLR_RANGE)pstSrcInfo->enClrRange;
		ctx->stSrcInfo.enClrSpace =
			(enum EN_DIP_CLR_SPACE)_GetClrSpaceMapping(pstSrcInfo->u8ClrColorSpace,
			 pstSrcInfo->u8ClrMatrixCoef);
		//Secure Info
		ctx->u32pipeline_id = pstSrcInfo->u32PipeLineId;
		if (pstSrcInfo->u8IsValidFd) {
			stMetaData.u16Enable = pstSrcInfo->u8IsValidFd;
			stMetaData.meta_fd = pstSrcInfo->MetaFd;
			stMetaData.enDIPSrc = pstSrcInfo->enDIPSrc;
			GetMetadataInfo(ctx, &stMetaData);
		} else if (pstSrcInfo->u32PipeLineId) {
			sealdata.pipelineID = pstSrcInfo->u32PipeLineId;
			ret = mtkd_seal_getaidtype(&sealdata);
			if (ret == 0) {
				ctx->u8aid = sealdata.enType;
			} else {
				v4l2_err(&dev->v4l2_dev, "[%s][Error]Get AID type fail\n",
					__func__);
				ctx->u8aid = E_AID_NS;
			}
		} else {
			if (pstSrcInfo->u8IsSecutiry)
				ctx->u8aid = E_AID_S;
			else
				ctx->u8aid = E_AID_NS;
		}
		DIP_DBG_API("[%s,%d]source aid:%d\n", __func__, __LINE__, ctx->u8aid);

		//Max Crop Width
		if ((_IsCapCase(ctx->enSource)) == true) {
			if (ctx->stSrcInfo.u8PixelNum > dev->dipcap_dev.u32DipPixelNum)
				idx = (ctx->stSrcInfo.u8PixelNum)/(dev->dipcap_dev.u32DipPixelNum);
			else
				idx = 1;
			pstSrcInfo->u32CropMaxWidth = ctx->stSrcInfo.u32Width/idx;
		} else {
			pstSrcInfo->u32CropMaxWidth = ctx->stSrcInfo.u32Width;
		}
		DIP_DBG_API("%s, Crop Input Max Width:%d\n", __func__, pstSrcInfo->u32CropMaxWidth);
	} else {
		v4l2_err(&dev->v4l2_dev, "%s, Version and Size Mismatch\n",
			__func__);
		ret = (-EINVAL);
	}

	return ret;
}

int dip_SetSourceInfo(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_SOURCE_INFO stSrcInfo;
	int ret = 0;

	if (ptr == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}

	memset(&stSrcInfo, 0, sizeof(struct ST_DIP_SOURCE_INFO));
	ret = copy_from_user(&stSrcInfo, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n",
				__func__);
		return -EFAULT;
	}
	ret = _UpdateSourceInfo(ctx, &stSrcInfo);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, _UpdateSourceInfo error\n", __func__);
		return -EFAULT;
	}
	ret = copy_to_user((void *)ptr, &stSrcInfo, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copy data to user\n",
				__func__);
		return -EFAULT;
	}

	return ret;
}

int dip_SetOutputInfo(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_OUTPUT_INFO stOutInfo;
	int ret = 0;

	if (ptr == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}

	memset(&stOutInfo, 0, sizeof(struct ST_DIP_OUTPUT_INFO));
	ret = copy_from_user(&stOutInfo, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}

	if ((stOutInfo.u32Version == DIP_OUTPUT_INFO_VER) &&
	(stOutInfo.u32Length == sizeof(struct ST_DIP_OUTPUT_INFO))) {
		ctx->u32OutputSecutiry = stOutInfo.u32IsSecutiry;
		ctx->u8ContiBufCnt = stOutInfo.u8ContiBufCnt;
	} else {
		v4l2_err(&dev->v4l2_dev, "%s, Version and Size Mismatch\n", __func__);
		ret = (-EINVAL);
	}

	DIP_DBG_API("[%s,%d]OutputSecutiry:%d, ContiBufCnt:%d\n",
		__func__, __LINE__, ctx->u32OutputSecutiry, ctx->u8ContiBufCnt);

	return ret;
}

int dip_SetFlowControl(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_FLOW_CONTROL stFlowControl;
	int ret = 0;

	if (ptr == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}
	if (size < sizeof(struct ST_DIP_FLOW_CONTROL)) {
		v4l2_err(&dev->v4l2_dev, "%s, size is not enough\n", __func__);
		return -EINVAL;
	}

	stFlowControl.u8Enable = 0;
	stFlowControl.u64BytePerSec = 0;
	ret = copy_from_user(&stFlowControl, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}
	ctx->u8Bwlimit = stFlowControl.u8Enable;
	ctx->u64BytePerSec = stFlowControl.u64BytePerSec;

	return ret;
}

int dip_SetPqBWInfo(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev = ctx->dev;
	struct ST_DIP_PQBW_INFOL stPqBwInfo;
	int ret = 0;

	if (ptr == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}
	if (size < sizeof(struct ST_DIP_FLOW_CONTROL)) {
		v4l2_err(&dev->v4l2_dev, "%s, size is not enough\n", __func__);
		return -EINVAL;
	}

	stPqBwInfo.u32PqWinTime = 0;
	stPqBwInfo.u64PqRemainderBudget = 0;
	ret = copy_from_user(&stPqBwInfo, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}
	ctx->u32PqWinTime = stPqBwInfo.u32PqWinTime;
	ctx->u64PqRemainderBudget = stPqBwInfo.u64PqRemainderBudget;

	return ret;
}

int dip_Set2pHighway_LRWrite(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev;
	struct ST_DIP_HIGHWAY_LR_CTRL *pstHighwayCrtl;
	int ret = 0;

	if (!ctx) {
		DIP_ERR("%s, Null pointer of ctx\n", __func__);
		return -EINVAL;
	}

	if (!ctx->dev) {
		DIP_ERR("%s, Null pointer of ctx->dev\n", __func__);
		return -EINVAL;
	}

	dev = ctx->dev;

	if (!ptr) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}

	if (size < sizeof(struct ST_DIP_HIGHWAY_LR_CTRL)) {
		v4l2_err(&dev->v4l2_dev, "%s, size is not enough\n", __func__);
		return -EINVAL;
	}

	pstHighwayCrtl = &ctx->stHighway;

	ret = copy_from_user(pstHighwayCrtl, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}

	if (!dev->dipcap_dev.u32CapHighway && pstHighwayCrtl->u8HighwayEn) {
		v4l2_err(&dev->v4l2_dev, "%s, not support highway path\n", __func__);
		return -EINVAL;
	}

	return ret;
}

int dip_SetBlend_DispInfo(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev;
	struct ST_DIP_BLENDING_DISP_INFO *pstDipBlendDisp;

	if (!ctx) {
		DIP_ERR("%s, Null pointer of ctx\n", __func__);
		return -EINVAL;
	}

	if (!ctx->dev) {
		DIP_ERR("%s, Null pointer of ctx->dev\n", __func__);
		return -EINVAL;
	}

	dev = ctx->dev;

	if (!ptr) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}

	if (size < sizeof(struct ST_DIP_BLENDING_DISP_INFO)) {
		v4l2_err(&dev->v4l2_dev, "%s, size is not enough\n", __func__);
		return -EINVAL;
	}

	pstDipBlendDisp = &ctx->stBlendDispInfo;

	if (copy_from_user(pstDipBlendDisp, (void *)ptr, size)) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}

	return 0;
}

int dip_Set1RNW(struct dip_ctx *ctx, void __user *ptr, __u32 size)
{
	struct dip_dev *dev;
	struct ST_DIP_1RNW_CTRL st1RNWCtrl;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	if (!ctx) {
		DIP_ERR("%s, Null pointer of ctx\n", __func__);
		return -EINVAL;
	}

	if (!ctx->dev) {
		DIP_ERR("%s, Null pointer of ctx->dev\n", __func__);
		return -EINVAL;
	}

	dev = ctx->dev;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if (!ptr) {
		v4l2_err(&dev->v4l2_dev, "%s, NULL pointer\n", __func__);
		return -EINVAL;
	}

	if (size < sizeof(struct ST_DIP_1RNW_CTRL)) {
		v4l2_err(&dev->v4l2_dev, "%s, size is not enough\n", __func__);
		return -EINVAL;
	}

	memset(&st1RNWCtrl, 0x0, sizeof(struct ST_DIP_1RNW_CTRL));

	ret = copy_from_user(&st1RNWCtrl, (void *)ptr, size);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, error copying data\n", __func__);
		return -EFAULT;
	}

	if (st1RNWCtrl.u32WriteFlag & ~(E_DIP_1RNW_DIP0|E_DIP_1RNW_DIP1|E_DIP_1RNW_DIP2)) {
		v4l2_err(&dev->v4l2_dev, "%s, invalid 1RNW flag = 0x%x\n", __func__,
			st1RNWCtrl.u32WriteFlag);
		return -EINVAL;
	}

	if (!dev->dipcap_dev.u321RNW && (st1RNWCtrl.u32WriteFlag & (1 << enDIPIdx))) {
		v4l2_err(&dev->v4l2_dev, "%s, not support 1RNW\n", __func__);
		return -EINVAL;
	}

	gst1RNWInfo.u32WriteFlag = st1RNWCtrl.u32WriteFlag;
	v4l2_info(&dev->v4l2_dev, "%s, set 1RNW flag = 0x%x\n", __func__, gst1RNWInfo.u32WriteFlag);

	return ret;
}

int _dip_get_hwcap(struct dip_ctx *ctx, struct ST_DIP_CAP_INFO *pstHwCap)
{
	struct dip_dev *dev = ctx->dev;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;
	u16 u16SizeAlign = 0;
	u16 u16PitchAlign = 0;
	u32 u32MaxWidth = 0;

	DIP_DBG_API("%s, cap enum:0x%x\n", __func__, pstHwCap->enDipCap);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	if ((pstHwCap->u32Version == DIP_GET_HWCAP_VER) &&
		(pstHwCap->u32Length == sizeof(struct ST_DIP_CAP_INFO))) {
		switch (pstHwCap->enDipCap) {
		case E_DIP_CAP_ADDR_ALIGN:
			pstHwCap->u32RetVal = DIP_ADDR_ALIGN;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_WIDTH_ALIGN:
			_GetDIPWFmtAlignUnit(dev,
				pstHwCap->u32Fourcc, &u16SizeAlign, &u16PitchAlign);
			pstHwCap->u32RetVal = u16SizeAlign;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_PITCH_ALIGN:
			_GetDIPWFmtAlignUnit(dev,
				pstHwCap->u32Fourcc, &u16SizeAlign, &u16PitchAlign);
			pstHwCap->u32RetVal = u16PitchAlign;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_IN_WIDTH_MAX:
			pstHwCap->u32RetVal = _GetDIPRFmtMaxWidth(ctx, pstHwCap->u32Fourcc);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_IN_WIDTH_MIN:
			pstHwCap->u32RetVal = DIP_IN_WIDTH_MIN;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_OUT_WIDTH_MAX:
			pstHwCap->u32RetVal =
				_GetDIPWFmtMaxWidth(ctx, pstHwCap->u32Fourcc, enDIPIdx);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_OUT_WIDTH_MIN:
			pstHwCap->u32RetVal = DIP_OUT_WIDTH_MIN;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_CROP_W_ALIGN:
			pstHwCap->u32RetVal = DIP_CROP_WIDTH_ALIGN;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_CROP_XPOS_ALIGN:
			pstHwCap->u32RetVal = dev->dipcap_dev.u32CropAlign;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_DI_IN_WIDTH_MAX:
			pstHwCap->u32RetVal = _GetDIPDiDnrMaxWidth(enDIPIdx);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_DI_IN_WIDTH_ALIGN:
			pstHwCap->u32RetVal = _GetDIPDiDnrWidthAlign(enDIPIdx);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_DNR_IN_WIDTH_MAX:
			pstHwCap->u32RetVal = _GetDIPDiDnrMaxWidth(enDIPIdx);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_DNR_IN_WIDTH_ALIGN:
			u32MaxWidth = _GetDIPDiDnrWidthAlign(enDIPIdx);
			pstHwCap->u32RetVal = u32MaxWidth;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_VSD_IN_WIDTH_MAX:
			pstHwCap->u32RetVal = dev->dipcap_dev.u32VsdInW;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_HVSP_IN_WIDTH_MAX:
			pstHwCap->u32RetVal = dev->dipcap_dev.u32HvspInW;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_HMIR_WIDTH_MAX:
			pstHwCap->u32RetVal = dev->dipcap_dev.u32HMirSz;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_RWSEP_WIDTH_ALIGN:
			pstHwCap->u32RetVal = _GetDIPRWSepWidthAlign(ctx, pstHwCap->u32Fourcc);
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_RWSEP_IN_MAX:
			if (dev->dipcap_dev.u32RWSep) {
				pstHwCap->u32RetVal = DIP_RWSEP_IN_WIDTH_MAX;
				if (dev->dipcap_dev.u32RWSepVer == DIP_RWSEP_V0_VERSION)
					pstHwCap->u32RetValB = DIP_RWSEP_V0_IN_HEIGHT_MAX;
				else
					pstHwCap->u32RetValB = DIP_RWSEP_V1_IN_HEIGHT_MAX;
			} else {
				pstHwCap->u32RetVal = 0;
				pstHwCap->u32RetValB = 0;
			}
			break;
		case E_DIP_CAP_RWSEP_OUT_MAX:
			if (dev->dipcap_dev.u32RWSep) {
				pstHwCap->u32RetVal = DIP_RWSEP_OUT_WIDTH_MAX;
				pstHwCap->u32RetValB = DIP_RWSEP_OUT_HEIGHT_MAX;
			} else {
				pstHwCap->u32RetVal = 0;
				pstHwCap->u32RetValB = 0;
			}
			break;
		case E_DIP_CAP_ROT_HV_ALIGN:
			pstHwCap->u32RetVal = DIP_ROT_HV_ALIGN;
			pstHwCap->u32RetValB = DIP_ROT_HV_ALIGN;
			break;
		case E_DIP_CAP_FMT_BPP:
			_GetFmtBpp(pstHwCap->u32Fourcc,
					&(pstHwCap->u32RetVal), &(pstHwCap->u32RetValB));
			break;
		case E_DIP_CAP_INPUT_ALIGN:
			_GetDIPFmtAlignUnit(dev,
				pstHwCap->u32Fourcc, &u16SizeAlign, &u16PitchAlign);
			pstHwCap->u32RetVal = u16SizeAlign;
			pstHwCap->u32RetValB = u16PitchAlign;
			break;
		case E_DIP_CAP_IN_HEIGHT_ALIGN:
			_GetInFmtHeightAlign(pstHwCap->u32Fourcc,
				&pstHwCap->u32RetVal, &pstHwCap->u32RetValB);
			break;
		case E_DIP_CAP_OUT_HEIGHT_ALIGN:
			_GetOutFmtHeightAlign(dev, pstHwCap->u32Fourcc,
				&pstHwCap->u32RetVal, &pstHwCap->u32RetValB);
			break;
		case E_DIP_CAP_HVSP_RATIO_MAX:
			pstHwCap->u32RetVal = DIP_HVSP_RATIO_MAX;
			pstHwCap->u32RetValB = DIP_HVSP_RATIO_MAX;
			break;
		case E_DIP_CAP_B2R_ADDR_ALIGN:
			_GetB2RAddrAlign(pstHwCap->u32Fourcc,
					&(pstHwCap->u32RetVal), &(pstHwCap->u32RetValB));
			break;
		case E_DIP_CAP_B2R_WIDTH_ALIGN:
			pstHwCap->u32RetVal = DIP_B2R_WIDTH_ALIGN;
			pstHwCap->u32RetValB = 0;
			break;
		case E_DIP_CAP_B2R_WIDTH_MAX:
			if (dev->dipcap_dev.u32B2R)
				pstHwCap->u32RetVal = DIP_B2R_WIDTH_MAX;
			else
				pstHwCap->u32RetVal = 0;
			pstHwCap->u32RetValB = 0;
			break;
		default:
			v4l2_err(&dev->v4l2_dev, "%s, not support enum %d\n",
				__func__, pstHwCap->enDipCap);
			ret = (-1);
			break;
		}
	} else {
		v4l2_err(&dev->v4l2_dev, "%s, version(%d)/size(%d) not match\n",
			__func__, pstHwCap->u32Version, pstHwCap->u32Length);
		ret = (-1);
	}
	DIP_DBG_API("%s, cap enum:0x%x, val:%d,%d\n", __func__,
		pstHwCap->enDipCap,
		pstHwCap->u32RetVal, pstHwCap->u32RetValB);

	return ret;
}

static irqreturn_t dipr_blending_isr(void *prv)
{
	struct dip_dev *dev = NULL;
	struct dip_ctx *ctx = NULL;
	struct vb2_v4l2_buffer *src = NULL;

	if (!prv) {
		DIP_ERR("%s, Null pointer of prv\n", __func__);
		return IRQ_NONE;
	}

	dev = prv;
	ctx = dev->ctx;
	if (!ctx) {
		v4l2_err(&dev->v4l2_dev, "%s, Null pointer of ctx\n",
			__func__);
		return IRQ_NONE;
	}

	// Set Src done buffer
	src = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	if (!src) {
		v4l2_err(&dev->v4l2_dev, "%s,NULL Src Buffer\n",
			__func__);
		return IRQ_HANDLED;
	}
	v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);

	//Check next src buffer
	src = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	if (!src)
		return IRQ_HANDLED;

	if (Dipr_SetBlendingSrcInfo(ctx, src->vb2_buf.type)) {
		v4l2_err(&dev->v4l2_dev,
			"%s,Dipr_SetBlendingSrcInfo fail\n", __func__);
		return IRQ_HANDLED;
	}

	if (Dipr_SetBlending(ctx, src->vb2_buf.type, true)) {
		v4l2_err(&dev->v4l2_dev,
			"%s,Dipr_SetBlending fail\n", __func__);
		return IRQ_HANDLED;
	}
	SaveExcuteStart();

	return IRQ_HANDLED;
}

static irqreturn_t dip_isr(int irq, void *prv)
{
	struct dip_dev *dev = NULL;
	struct dip_ctx *ctx = NULL;
	struct vb2_v4l2_buffer *src, *dst;
	struct dip_fmt *Dstfmt = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	u16 u16IrqSts = 0;
	u64 pu64DstPhyAddr[MAX_PLANE_NUM] = {0};
	u32 u32Idx = 0;
	u32 u32NPlanes = 0;
	int ret = 0;

	if (prv == NULL)
		return IRQ_NONE;
	dev = prv;
	ctx = dev->ctx;
	if (ctx == NULL)
		return IRQ_NONE;

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return IRQ_NONE;

	KDrv_DIP_GetIrqStatus(&u16IrqSts, enDIPIdx);
	KDrv_DIP_ClearIrq(u16IrqSts, enDIPIdx);
	if (u16IrqSts != 0) {
		RecordExecute(ctx);
		if (enDIPIdx == E_DIPR_BLEND_IDX0)
			return dipr_blending_isr(prv);

		if (ctx->fh.m2m_ctx->cap_q_ctx.num_rdy == 0) {
			DIP_DBG_QUEUE("%s,%d, rdy buffer is empty\n",
				__func__, __LINE__);
			return IRQ_NONE;
		}
		// Set Dst done buffer
		dst = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		if (dst == NULL) {
			ctx->u32CapStat = ST_OFF;
			v4l2_err(&dev->v4l2_dev, "%s,%d, NULL Dst Buffer\n",
				__func__, __LINE__);
			return IRQ_HANDLED;
		}
		DIP_DBG_QUEUE("%s,%d, rdy:%d, u16IrqSts:0x%x, index:%d\n",
			__func__, __LINE__,
			ctx->fh.m2m_ctx->cap_q_ctx.num_rdy, u16IrqSts,
			dst->vb2_buf.index);

		v4l2_m2m_buf_done(dst, VB2_BUF_STATE_DONE);
		if ((_IsM2mCase(ctx->enSource)) == true) {
			// Set Src done buffer
			src = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
			if (src == NULL) {
				ctx->u32M2mStat = ST_OFF;
				v4l2_err(&dev->v4l2_dev, "%s,%d, NULL Src Buffer\n",
					__func__, __LINE__);
				return IRQ_HANDLED;
			}
			v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);
			v4l2_m2m_job_finish(dev->m2m_dev, ctx->fh.m2m_ctx);

			//Check next buffer
			src = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
			dst = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
			if ((src == NULL) || (dst == NULL)) {
				ctx->u32M2mStat = ST_OFF;
				return IRQ_HANDLED;
			}
			//check StreamOff
			if (ctx->u32M2mStat & ST_ABORT)
				ctx->u32M2mStat = ST_OFF;
		} else if ((_IsCapCase(ctx->enSource)) == true) {
			if ((ctx->u32CapStat & ST_CONT) == 0) {
				dst = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
				if (dst == NULL)
					return IRQ_HANDLED;
				Dstfmt = ctx->out.fmt;
				u32NPlanes = _GetDIPFmtPlaneCnt(Dstfmt->fourcc);
				if (u32NPlanes > MAX_PLANE_NUM)
					return IRQ_HANDLED;
				for (u32Idx = 0 ; u32Idx < u32NPlanes ; u32Idx++) {
					pu64DstPhyAddr[u32Idx] = mtk_get_addr(&(dst->vb2_buf),
						u32Idx);
					if (pu64DstPhyAddr[u32Idx] == INVALID_ADDR) {
						v4l2_err(&dev->v4l2_dev, "%s,%d, addr[%d] invalid\n",
							__func__, __LINE__, u32Idx);
						return IRQ_HANDLED;
					}
					if (ctx->u32DstBufMode == E_BUF_IOVA_MODE)
						pu64DstPhyAddr[u32Idx] += IOVA_START_ADDR;
					DIP_DBG_BUF("%s, Addr[%d]:0x%llx\n",
						__func__, u32Idx, pu64DstPhyAddr[u32Idx]);
				}
				_DipSetDstUdmaLimitRange(ctx, pu64DstPhyAddr, true);
				KDrv_DIP_SetDstAddr(u32NPlanes, pu64DstPhyAddr, enDIPIdx);
				SaveExcuteStart();
				KDrv_DIP_Trigger(enDIPIdx);
			} else {
				SaveExcuteStart();
			}
		}
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}

int dip_request_irq(struct dip_dev *dev, u8 u8Enable)
{
	struct dip_ctx *ctx = NULL;
	int ret = 0;

	ctx = dev->ctx;
	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctx is NULL\n", __func__);
		return -EFAULT;
	}

	DIP_DBG_QUEUE("%s, Enable:%d\n", __func__, u8Enable);

	if (u8Enable) {
		if (ctx->u8ReqIrq == 0) {
			ret = devm_request_irq(dev->dev, dev->irq, dip_isr,
				IRQF_SHARED, dev->v4l2_dev.name, dev);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s, failed to request IRQ %d",
					__func__, dev->irq);
				return ret;
			}
			ctx->u8ReqIrq = 1;
		}
	} else {
		if (ctx->u8ReqIrq) {
			devm_free_irq(dev->dev, dev->irq, dev);
			ctx->u8ReqIrq = 0;
		}
	}

	return ret;
}

static int dip_ioctl_s_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s, Id:0x%x, value:%d\n", __func__, ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		ctx->HMirror = ctrl->value;
		break;
	case V4L2_CID_VFLIP:
		ctx->VMirror = ctrl->value;
		break;
	case V4L2_CID_ALPHA_COMPONENT:
		ctx->alpha = ctrl->value;
		break;
	case V4L2_CID_ROTATE:
		if (ctrl->value == 0) {
			ctx->u32Rotation = ctrl->value;
		} else {
			if ((ctrl->value == ROT90_ANGLE) &&
				((dev->dipcap_dev.u32Rotate & ROT90_M2M_SYS_BIT) ||
				(dev->dipcap_dev.u32Rotate & ROT90_CAP_SYS_BIT))) {
				ctx->u32Rotation = ctrl->value;
			} else if ((ctrl->value == ROT270_ANGLE) &&
				((dev->dipcap_dev.u32Rotate & ROT270_M2M_SYS_BIT) ||
				(dev->dipcap_dev.u32Rotate & ROT270_CAP_SYS_BIT))) {
				ctx->u32Rotation = ctrl->value;
			} else {
				ctx->u32Rotation = ctrl->value;
				v4l2_err(&dev->v4l2_dev, "Not Support Rotate %d", ctrl->value);
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int dip_ioctl_querycap(struct file *file, void *priv,
	struct v4l2_capability *cap)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	DIP_DBG_API("%s\n", __func__);

	strncpy(cap->driver, DIP_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, DIP_NAME, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	if (enDIPIdx != E_DIPR_BLEND_IDX0)
		cap->device_caps = DEVICE_CAPS;
	else
		cap->device_caps = DEVICE_DIPR_BLEND_CAPS;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int dip_ioctl_s_input(struct file *file, void *priv, unsigned int i)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s, Input:%d\n", __func__, i);

	if (i >= E_DIP_SOURCE_MAX) {
		v4l2_err(&dev->v4l2_dev, "%s, %d, Invalid Input enSource %d",
			__func__, __LINE__, ctx->enSource);
		return -EINVAL;
	}

	ctx->enSource = (EN_DIP_SOURCE)i;
	ctx->u32CapStat = (ctx->u32CapStat & (~ST_CONT));

	return 0;
}

static int dip_ioctl_streamon(struct file *file, void *priv, enum v4l2_buf_type type)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	struct vb2_queue *vq;
	int ret = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;


	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	DIP_DBG_API("%s, Type:%d\n", __func__, type);

	if (_ValidBufType(type) == false) {
		v4l2_err(&dev->v4l2_dev, "%s, %d, Invalid Buffer Type %d",
			__func__, __LINE__, type);
		return -EINVAL;
	}

	dev->ctx = ctx;

	if (enDIPIdx != DIPR_BLEND_WIN_IDX) {
		if (_IsDstBufType(type) == true) {
			ret = dip_request_irq(dev, 1);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "failed to request IRQ:%d\n", dev->irq);
				return ret;
			}
		}
		if ((_IsM2mCase(ctx->enSource)) == true) {
			ret = v4l2_m2m_ioctl_streamon(file, priv, type);
		} else {
			vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, type);
			ret = vb2_streamon(vq, type);
		}
	} else {
		ret = dip_request_irq(dev, 1);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "failed to request IRQ:%d\n", dev->irq);
			return ret;
		}
		vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, type);
		ret = vb2_streamon(vq, type);
	}
	return ret;
}

void ClearSrcQueue(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *src;
	u8 u8Idx = 0;
	u8 u8Cnt = 0;

	DIP_DBG_QUEUE("%s,%d, rdy:%d\n",
		__func__, __LINE__, ctx->fh.m2m_ctx->out_q_ctx.num_rdy);

	u8Cnt = ctx->fh.m2m_ctx->out_q_ctx.num_rdy;
	for (u8Idx = 0; u8Idx < u8Cnt ; u8Idx++) {
		src = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		if (src != NULL) {
			v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);
			v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		} else {
			v4l2_err(&dev->v4l2_dev, "%s,%d, NULL Src Buffer\n",
			__func__, __LINE__);
		}
	}
}

void ClearDstQueue(struct dip_ctx *ctx)
{
	struct dip_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *dst;
	u8 u8Idx = 0;
	u8 u8Cnt = 0;

	DIP_DBG_QUEUE("%s,%d, rdy:%d\n",
		__func__, __LINE__, ctx->fh.m2m_ctx->cap_q_ctx.num_rdy);

	u8Cnt = ctx->fh.m2m_ctx->cap_q_ctx.num_rdy;
	for (u8Idx = 0; u8Idx < u8Cnt ; u8Idx++) {
		dst = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		if (dst != NULL) {
			v4l2_m2m_buf_done(dst, VB2_BUF_STATE_DONE);
			v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
		} else {
			v4l2_err(&dev->v4l2_dev, "%s,%d, NULL Dst Buffer\n",
			__func__, __LINE__);
		}
	}
}

static int dip_ioctl_streamoff(struct file *file, void *priv, enum v4l2_buf_type type)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	struct vb2_queue *vq;
	int ret = 0;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	DIP_DBG_API("%s, Type:%d\n", __func__, type);

	if (_ValidBufType(type) == false) {
		v4l2_err(&dev->v4l2_dev, "%s, %d, Invalid Buffer Type %d",
			__func__, __LINE__, type);
		return -EINVAL;
	}

	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	if (_IsDstBufType(type) == true) {
		ret = dip_request_irq(dev, 0);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "failed to free IRQ:%d\n", dev->irq);
			return ret;
		}
		ClearDstQueue(ctx);
	} else {
		ClearSrcQueue(ctx);
	}

	if (enDIPIdx != DIPR_BLEND_WIN_IDX) {
		if ((_IsM2mCase(ctx->enSource)) == true) {
			ret = v4l2_m2m_ioctl_streamoff(file, priv, type);
			ctx->u32M2mStat |= ST_ABORT;
		} else {
			vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, type);
			ctx->u32CapStat = ST_OFF;
			ret = vb2_streamoff(vq, type);
		}
	} else {
		ret = dip_request_irq(dev, 0);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "failed to free IRQ:%d\n", dev->irq);
			return ret;
		}
		vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, type);
		ret = vb2_streamoff(vq, type);
	}

	return ret;
}

static int dip_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	struct dip_ctx *ctx = NULL;
	struct dip_dev *dev = NULL;
	struct v4l2_ext_control *ctrl = NULL;
	struct ST_DIP_MFDEC *pstMfdecSet = NULL;
	struct ST_DIP_MFDEC_INFO stMfdInfo;
	struct ST_DIP_BUF_MODE stBufMode;
	struct ST_DIP_METADATA stMetaData;
	struct ST_DIP_WINID_INFO stWinIdInfo;
	int i = 0;
	int ret = 0;
	u32 u32OpMode = 0;

	ctx = fh2ctx(file->private_data);
	if (ctx == NULL) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -EINVAL;
	}
	dev = ctx->dev;
	if (dev == NULL) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctrls == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctrls is NULL\n", __func__);
		return -EINVAL;
	}
	pstMfdecSet = &(ctx->stMfdecSet);

	DIP_DBG_API("%s, Count:%d\n", __func__, ctrls->count);

	for (i = 0; i < ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		DIP_DBG_API("%s, Id:0x%x\n", __func__, ctrl->id);
		switch (ctrl->id) {
		case V4L2_CID_EXT_DIP_SET_MFDEC:
			ret = copy_from_user(&stMfdInfo, (void *)ctrl->ptr,
							sizeof(struct ST_DIP_MFDEC_INFO));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			pstMfdecSet->u16Ena = stMfdInfo.u16Enable;
			pstMfdecSet->u16Sel = stMfdInfo.u16SelNum;
			pstMfdecSet->u16FBPitch = stMfdInfo.u16FBPitch;
			pstMfdecSet->u16BitlenPitch = stMfdInfo.u16BitlenPitch;
			pstMfdecSet->u16StartX = stMfdInfo.u16StartX;
			pstMfdecSet->u16StartY = stMfdInfo.u16StartY;
			pstMfdecSet->u16HSize = stMfdInfo.u16HSize;
			pstMfdecSet->u16VSize = stMfdInfo.u16VSize;
			pstMfdecSet->u16VP9Mode = stMfdInfo.u16VP9Mode;
			pstMfdecSet->u16HMir = stMfdInfo.u16HMirror;
			pstMfdecSet->u16VMir = stMfdInfo.u16VMirror;
			if (stMfdInfo.enMFDecTileMode == E_DIP_MFEDC_TILE_16_32)
				pstMfdecSet->enTileFmt = E_TILE_FMT_16_32;
			else if (stMfdInfo.enMFDecTileMode == E_DIP_MFEDC_TILE_32_32)
				pstMfdecSet->enTileFmt = E_TILE_FMT_32_32;
			else
				pstMfdecSet->enTileFmt = E_TILE_FMT_32_16;
			pstMfdecSet->enTileFmt = (EN_TILE_FMT)stMfdInfo.enMFDecTileMode;
			pstMfdecSet->u16Bypass = stMfdInfo.u16Bypasse;
			pstMfdecSet->u16SimMode = stMfdInfo.u16SimMode;
			break;
		case V4L2_CID_EXT_DIP_SET_OP_MODE:
			ret = copy_from_user(&u32OpMode, (void *)ctrl->ptr, sizeof(u32));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			if (dev->dipcap_dev.u323DDI) {
				ctx->stDiNrInfo.u32OpMode = u32OpMode;
				if (ctx->stDiNrInfo.u32OpMode == 0) {
					FreeDiBufferCheck(ctx);
				}
			}
			break;
		case V4L2_CID_EXT_DIP_SET_BUF_MODE:
			ret = copy_from_user(&stBufMode, (void *)ctrl->ptr, sizeof(stBufMode));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			if (_IsSrcBufType(stBufMode.u32BufType) == true)
				ctx->u32SrcBufMode = stBufMode.u32BufMode;
			if (_IsDstBufType(stBufMode.u32BufType) == true)
				ctx->u32DstBufMode = stBufMode.u32BufMode;
			break;
		case V4L2_CID_EXT_DIP_METADATA_FD:
			memset(&stMetaData, 0, sizeof(struct ST_DIP_METADATA));
			ret = copy_from_user(&stMetaData, (void *)ctrl->ptr, sizeof(stMetaData));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			ret = GetMetadataInfo(ctx, &stMetaData);
			break;
		case V4L2_CID_EXT_DIP_SET_ONEWIN:
			memset(&stWinIdInfo, 0, sizeof(struct ST_DIP_WINID_INFO));
			ret = copy_from_user(&stWinIdInfo, (void *)ctrl->ptr, sizeof(stWinIdInfo));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			DIP_DBG_API("%s, DIP_SET_ONEWIN, Enable:%d, WinId:%d\n", __func__,
				stWinIdInfo.u8Enable, stWinIdInfo.u16WinId);
			ctx->stWinid.u8Enable = stWinIdInfo.u8Enable;
			ctx->stWinid.u16WinId = stWinIdInfo.u16WinId;
			break;
		case V4L2_CID_EXT_DIP_SOURCE_INFO:
			ret = dip_SetSourceInfo(ctx, ctrl->ptr, sizeof(struct ST_DIP_SOURCE_INFO));
			break;
		case V4L2_CID_EXT_DIP_OUTPUT_INFO:
			ret = dip_SetOutputInfo(ctx, ctrl->ptr, ctrl->size);
			break;
		case V4L2_CID_EXT_DIP_FLOWCONTROL:
			ret = dip_SetFlowControl(ctx, ctrl->ptr, ctrl->size);
			break;
		case V4L2_CID_EXT_DIP_PQBW_INFO:
			ret = dip_SetPqBWInfo(ctx, ctrl->ptr, ctrl->size);
			break;
		case V4L2_CID_EXT_DIP_HIGHWAY_LR_CTRL:
			ret = dip_Set2pHighway_LRWrite(ctx, ctrl->ptr, ctrl->size);
			break;
		case V4L2_CID_EXT_DIP_SET_DISP_INFO:
			ret = dip_SetBlend_DispInfo(ctx, ctrl->ptr, ctrl->size);
			break;
		case V4L2_CID_EXT_DIP_SET_1RNW:
			ret = dip_Set1RNW(ctx, ctrl->ptr, ctrl->size);
			break;
		default:
			v4l2_err(&dev->v4l2_dev, "%s,%d, id=0x%x\n", __func__, __LINE__, ctrl->id);
			ret = (-EINVAL);
			break;
		}
	}

	return ret;
}

static int dip_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	struct dip_ctx *ctx = fh2ctx(file->private_data);
	struct dip_dev *dev = ctx->dev;
	struct v4l2_ext_control *ctrl = NULL;
	struct ST_DIP_CAP_INFO stHwCap;
	int ret = 0;
	int i = 0;

	if (ctrls == NULL) {
		v4l2_err(&dev->v4l2_dev, "%s, ctrls is NULL\n", __func__);
		return -EINVAL;
	}

	DIP_DBG_API("%s, Count:%d\n", __func__, ctrls->count);

	for (i = 0; i < ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		DIP_DBG_API("%s, Id:0x%x\n", __func__, ctrl->id);
		switch (ctrl->id) {
		case V4L2_CID_EXT_DIP_GET_HWCAP:
			memset(&stHwCap, 0, sizeof(struct ST_DIP_CAP_INFO));
			ret = copy_from_user(&stHwCap, (void *)ctrl->ptr, sizeof(stHwCap));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copying data\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			ret = _dip_get_hwcap(ctx, &stHwCap);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, get invalid parameter\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			ret = copy_to_user((void *)ctrl->ptr, &stHwCap, sizeof(stHwCap));
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "%s,id=0x%x, error copy data to user\n",
						__func__, ctrl->id);
				return -EFAULT;
			}
			break;
		default:
			v4l2_err(&dev->v4l2_dev, "%s,%d, id=0x%x\n", __func__, __LINE__, ctrl->id);
			break;
		}
	}

	return ret;
}

static int dip_ioctl_enum_cap_mp_fmt(struct file *file, void *prv, struct v4l2_fmtdesc *f)
{
	struct dip_ctx *ctx = fh2ctx(file->private_data);
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *fmt;
	u32 u32FmtCnt = 0;

	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip0FmtList);
		if (f->index < u32FmtCnt)
			fmt = &stDip0FmtList[f->index];
		else
			return -EINVAL;
	} else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip1FmtList);
		if (f->index < u32FmtCnt)
			fmt = &stDip1FmtList[f->index];
		else
			return -EINVAL;
	} else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX) {
		u32FmtCnt = SPT_FMT_CNT(stDip2FmtList);
		if (f->index < u32FmtCnt)
			fmt = &stDip2FmtList[f->index];
		else
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	f->pixelformat = fmt->fourcc;
	strncpy(f->description, fmt->name, sizeof(f->description) - 1);

	DIP_DBG_API("%s, Name:%s\n", __func__, fmt->name);

	return 0;
}

static int dip_ioctl_enum_out_mp_fmt(struct file *file, void *prv, struct v4l2_fmtdesc *f)
{
	struct dip_ctx *ctx = fh2ctx(file->private_data);
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *fmt;

	if (f->index >= SPT_FMT_CNT(stDiprFmtList))
		return -EINVAL;

	fmt = &stDiprFmtList[f->index];
	f->pixelformat = fmt->fourcc;
	strncpy(f->description, fmt->name, sizeof(f->description) - 1);

	DIP_DBG_API("%s, Name:%s\n", __func__, fmt->name);

	return 0;
}

static int dip_ioctl_g_fmt_mp(struct file *file, void *prv, struct v4l2_format *f)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	struct vb2_queue *vq;
	struct dip_frame *frm;
	unsigned int idx = 0;
	u32 u32NPlanes = 0;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;
	frm = _GetDIPFrame(ctx, f->type);
	if (IS_ERR(frm))
		return PTR_ERR(frm);

	f->fmt.pix.width        = frm->width;
	f->fmt.pix.height       = frm->height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat      = frm->fmt->fourcc;

	u32NPlanes = _GetDIPFmtPlaneCnt(frm->fmt->fourcc);
	for (idx = 0; idx < u32NPlanes; idx++) {
		f->fmt.pix_mp.plane_fmt[idx].sizeimage = frm->size[idx];
		f->fmt.pix_mp.plane_fmt[idx].bytesperline = frm->u32BytesPerLine[idx];
	}

	f->fmt.pix_mp.colorspace = frm->colorspace;
	f->fmt.pix_mp.ycbcr_enc = frm->ycbcr_enc;
	f->fmt.pix_mp.quantization = frm->quantization;

	DIP_DBG_API("%s, Type:%d, Width:%d, Height:%d, Format:0x%x, Plane:%d\n",
		__func__, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes);

	return 0;
}

static int dip_try_cap_mp_fmt(struct file *file, void *prv, struct v4l2_format *f)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *fmt;
	enum v4l2_field *field;

	DIP_DBG_API("%s, Type:%d, Width:%d, Height:%d, Format:0x%x, Plane:%d\n",
		__func__, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes);

	fmt = _FindDipCapMpFmt(dev, f->fmt.pix_mp.pixelformat);
	if (!fmt)
		return -EINVAL;

	field = &f->fmt.pix_mp.field;
	if (*field == V4L2_FIELD_ANY)
		*field = V4L2_FIELD_NONE;
	else if (*field != V4L2_FIELD_NONE)
		return -EINVAL;

	if (f->fmt.pix_mp.width < 1)
		f->fmt.pix_mp.width = 1;
	if (f->fmt.pix_mp.height < 1)
		f->fmt.pix_mp.height = 1;

	return 0;
}

static int dip_try_out_mp_fmt(struct file *file, void *prv, struct v4l2_format *f)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	struct dip_fmt *fmt;
	enum v4l2_field *field;

	DIP_DBG_API("%s, Type:%d, Width:%d, Height:%d, Format:0x%x, Plane:%d\n",
		__func__, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes);

	fmt = _FindDipOutMpFmt(f->fmt.pix_mp.pixelformat);
	if (!fmt)
		return -EINVAL;

	field = &f->fmt.pix_mp.field;
	if (*field == V4L2_FIELD_ANY)
		*field = V4L2_FIELD_NONE;
	else if (*field != V4L2_FIELD_NONE)
		return -EINVAL;

	if (f->fmt.pix_mp.width < 1)
		f->fmt.pix_mp.width = 1;
	if (f->fmt.pix_mp.height < 1)
		f->fmt.pix_mp.height = 1;

	return 0;
}

static int dip_ioctl_s_fmt_cap_mplane(struct file *file, void *prv, struct v4l2_format *f)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	struct vb2_queue *vq;
	struct dip_frame *frm;
	struct dip_fmt *fmt;
	int ret = 0;
	unsigned int idx = 0;
	u32 u32NPlanes = 0;

	DIP_DBG_API("%s, Type:%d, Width:%d, Height:%d, Format:0x%x, Plane:%d\n",
		__func__, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes);

	ret = dip_try_cap_mp_fmt(file, prv, f);
	if (ret)
		return ret;
	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (vb2_is_busy(vq)) {
		v4l2_err(&dev->v4l2_dev, "queue (%d) bust\n", f->type);
		return -EBUSY;
	}
	frm = _GetDIPFrame(ctx, f->type);
	if (IS_ERR(frm))
		return PTR_ERR(frm);
	fmt = _FindDipCapMpFmt(dev, f->fmt.pix_mp.pixelformat);
	if (!fmt)
		return -EINVAL;

	frm->width  = f->fmt.pix_mp.width;
	frm->height = f->fmt.pix_mp.height;

	frm->u32OutWidth  = frm->width;
	frm->u32OutHeight = frm->height;
	frm->fmt    = fmt;
	u32NPlanes = _GetDIPFmtPlaneCnt(fmt->fourcc);
	for (idx = 0; idx < u32NPlanes; idx++) {
		frm->u32BytesPerLine[idx]  = f->fmt.pix_mp.plane_fmt[idx].bytesperline;
		frm->size[idx] = f->fmt.pix_mp.plane_fmt[idx].sizeimage;
		DIP_DBG_API("%s, plane[%d], BytesPerLine:0x%x, size:0x%x\n",
			__func__, idx, frm->u32BytesPerLine[idx], frm->size[idx]);
	}

	if ((_IsCapCase(ctx->enSource)) == true) {
		if (ctx->stSrcInfo.u8PixelNum > dev->dipcap_dev.u32DipPixelNum)
			idx = (ctx->stSrcInfo.u8PixelNum)/(dev->dipcap_dev.u32DipPixelNum);
		else
			idx = 1;
		f->fmt.pix_mp.width = (ctx->stSrcInfo.u32Width)/idx;
		DIP_DBG_API("%s, Crop Input Max Width:%d\n", __func__, f->fmt.pix_mp.width);
	}

	if (f->fmt.pix_mp.colorspace == V4L2_COLORSPACE_DEFAULT)
		frm->colorspace = def_frame.colorspace;
	else
		frm->colorspace = f->fmt.pix_mp.colorspace;
	frm->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
	frm->quantization = f->fmt.pix_mp.quantization;

	return 0;
}

static int dip_ioctl_s_fmt_out_mplane(struct file *file, void *prv, struct v4l2_format *f)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	struct vb2_queue *vq;
	struct dip_frame *frm;
	struct dip_fmt *fmt;
	int ret = 0;
	unsigned int idx = 0;
	u32 u32NPlanes = 0;

	DIP_DBG_API("%s, Type:%d, Width:%d, Height:%d, Format:0x%x, Plane:%d\n",
		__func__, f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes);

	ret = dip_try_out_mp_fmt(file, prv, f);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s, %d, No Support Foramt, type:%d, fourc:0x%x",
			__func__, __LINE__, f->type, f->fmt.pix_mp.pixelformat);
		return ret;
	}

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (vb2_is_busy(vq)) {
		v4l2_err(&dev->v4l2_dev, "queue (%d) bust\n", f->type);
		return -EBUSY;
	}
	frm = _GetDIPFrame(ctx, f->type);
	if (IS_ERR(frm))
		return PTR_ERR(frm);
	fmt = _FindDipOutMpFmt(f->fmt.pix_mp.pixelformat);
	if (!fmt)
		return -EINVAL;

	frm->width  = f->fmt.pix_mp.width;
	frm->height = f->fmt.pix_mp.height;

	frm->u32OutWidth  = frm->width;
	frm->u32OutHeight = frm->height;
	frm->fmt    = fmt;
	u32NPlanes = _GetDIPFmtPlaneCnt(fmt->fourcc);
	for (idx = 0; idx < u32NPlanes; idx++) {
		frm->u32BytesPerLine[idx]  = f->fmt.pix_mp.plane_fmt[idx].bytesperline;
		frm->size[idx] = f->fmt.pix_mp.plane_fmt[idx].sizeimage;
		DIP_DBG_API("%s, plane[%d], BytesPerLine:0x%x, size:0x%x\n",
			__func__, idx, frm->u32BytesPerLine[idx], frm->size[idx]);
	}

	if (f->fmt.pix_mp.colorspace == V4L2_COLORSPACE_DEFAULT)
		frm->colorspace = def_frame.colorspace;
	else
		frm->colorspace = f->fmt.pix_mp.colorspace;
	frm->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
	frm->quantization = f->fmt.pix_mp.quantization;

	return 0;
}

static int dip_ioctl_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *rb)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s, Type:%d, Count:%d\n", __func__, rb->type, rb->count);

	return v4l2_m2m_ioctl_reqbufs(file, priv, rb);
}

static int dip_ioctl_querybuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s\n", __func__);

	return v4l2_m2m_ioctl_querybuf(file, priv, buf);
}

static int dip_ioctl_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct dip_ctx *ctx = NULL;
	struct dip_dev *dev = NULL;
	int ret = 0;

	if (priv == NULL) {
		pr_err("%s, priv is NULL\n", __func__);
		return -EPERM;
	}
	ctx = priv;
	if (ctx->dev == NULL) {
		pr_err("%s, dev is NULL\n", __func__);
		return -EPERM;
	}
	dev = ctx->dev;

	DIP_DBG_API("%s, Type:%d, Index:%d, rdy:%d, Start\n", __func__,
		buf->type, buf->index, ctx->fh.m2m_ctx->cap_q_ctx.num_rdy);

	ret = v4l2_m2m_ioctl_qbuf(file, priv, buf);

	DIP_DBG_API("%s, Type:%d, Index:%d, rdy:%d, End\n", __func__,
		buf->type, buf->index, ctx->fh.m2m_ctx->cap_q_ctx.num_rdy);

	return ret;
}

static int dip_ioctl_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	int ret = 0;

	DIP_DBG_API("%s, Type:%d, rdy:%d, Start\n", __func__,
		buf->type, ctx->fh.m2m_ctx->cap_q_ctx.num_rdy);

	ret = v4l2_m2m_ioctl_dqbuf(file, priv, buf);

	DIP_DBG_API("%s, Type:%d, Index:%d, rdy:%d, End\n", __func__,
		buf->type, buf->index, ctx->fh.m2m_ctx->cap_q_ctx.num_rdy);

	return ret;
}

static int dip_ioctl_g_selection(struct file *file, void *priv, struct v4l2_selection *pstSel)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *f;

	f = _GetDIPFrame(ctx, pstSel->type);
	if (IS_ERR(f))
		return PTR_ERR(f);

	if (pstSel->target == V4L2_SEL_TGT_CROP) {
		pstSel->r.left = f->o_width;
		pstSel->r.top = f->o_height;
		pstSel->r.width = f->c_width;
		pstSel->r.height = f->c_height;
	} else if (pstSel->target == V4L2_SEL_TGT_COMPOSE) {
		pstSel->r.left = 0;
		pstSel->r.top = 0;
		pstSel->r.width = f->u32SclOutWidth;
		pstSel->r.height = f->u32SclOutHeight;
	} else {
		v4l2_err(&dev->v4l2_dev,
			"doesn't support target:0x%X\n", pstSel->target);
		return -EINVAL;
	}

	DIP_DBG_API("%s, Target:%d, Left:%d, Top:%d, Width:%d, Height:%d\n",
		__func__, pstSel->target,
		pstSel->r.left, pstSel->r.top,
		pstSel->r.width, pstSel->r.height);

	return 0;
}

static int dip_ioctl_s_selection(struct file *file, void *priv, struct v4l2_selection *pstSel)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;
	struct dip_frame *f;
	int ret;

	DIP_DBG_API("%s, Target:0x%x, Left:%d, Top:%d, Width:%d, Height:%d\n",
		__func__, pstSel->target,
		pstSel->r.left, pstSel->r.top,
		pstSel->r.width, pstSel->r.height);

	f = _GetDIPFrame(ctx, pstSel->type);
	if (IS_ERR(f))
		return PTR_ERR(f);

	if (pstSel->target == V4L2_SEL_TGT_CROP) {
		ret = _ValidCrop(file, priv, pstSel);
		if (ret)
			return ret;
		f->o_width  = pstSel->r.left;
		f->o_height = pstSel->r.top;
		f->c_width  = pstSel->r.width;
		f->c_height = pstSel->r.height;
	} else if (pstSel->target == V4L2_SEL_TGT_COMPOSE) {
		f->u32SclOutWidth  = pstSel->r.width;
		f->u32SclOutHeight = pstSel->r.height;
	} else {
		v4l2_err(&dev->v4l2_dev,
			"doesn't support target = 0x%X\n", pstSel->target);
		return -EINVAL;
	}

	return 0;
}

static int dip_ioctl_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *a)
{
	struct dip_ctx *ctx = priv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s, Type:%d, InFps:%d, OutFps:%d\n",
		__func__, a->type,
		a->parm.output.timeperframe.denominator,
		a->parm.output.timeperframe.numerator);

	if (_IsDstBufType(a->type) == true) {
		if (a->parm.output.timeperframe.numerator != 0) {
			ctx->u32CapStat |= ST_CONT;
			ctx->u32FrcInRatio = a->parm.output.timeperframe.denominator;
			ctx->u32FrcOutRatio = a->parm.output.timeperframe.numerator;
		} else {
			ctx->u32CapStat = (ctx->u32CapStat & (~ST_CONT));
			ctx->u32FrcInRatio = a->parm.output.timeperframe.denominator;
			ctx->u32FrcOutRatio = a->parm.output.timeperframe.numerator;
		}
	}

	return 0;
}

static void dip_job_abort(void *prv)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;

	DIP_DBG_API("%s,%d\n", __func__, __LINE__);

	v4l2_m2m_job_finish(dev->m2m_dev, ctx->fh.m2m_ctx);
}

static void dip_device_run(void *prv)
{
	struct dip_ctx *ctx = prv;
	struct dip_dev *dev = ctx->dev;
	unsigned long flags;
	int ret = 0;

	DIP_DBG_API("%s,%d\n", __func__, __LINE__);

	if (ctx->u8WorkingAid == E_AID_SDC)
		ret = _DipTeeSdcMem2Mem(ctx);

	spin_lock_irqsave(&ctx->dev->ctrl_lock, flags);

	if (ctx->u8WorkingAid != E_AID_SDC)
		ret = _DipDevRunSetSrcAddr(ctx);

	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Src buffer\n",
			__func__, __LINE__);
		ctx->u32M2mStat = ST_OFF;
	}

	if ((ctx->u32CapStat & ST_CONT) == 0) {
		ret = _DipDevRunSetDstAddr(ctx);
		if (ret) {
			v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Dst buffer\n",
				__func__, __LINE__);
			ctx->u32M2mStat = ST_OFF;
		}
	}
	ret = _DipDevRunFire(ctx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "%s,%d, Error in Fire\n",
			__func__, __LINE__);
		ctx->u32M2mStat = ST_OFF;
	}

	spin_unlock_irqrestore(&dev->ctrl_lock, flags);
}

int Dip_Menuload_CreateRes(struct dip_ctx *ctx, u8 bufcnt, u16 cmd_cnt, enum sm_ml_sync sync_id)
{
	struct dip_dev *dev = NULL;
	struct sm_ml_res sm_res;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	if (!dev) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENXIO;
	}

	sm_res.sync_id = sync_id;
	sm_res.buffer_cnt = bufcnt;
	sm_res.cmd_cnt = cmd_cnt;

	if (sm_ml_create_resource(&ctx->stDipMl.ml_fd, &sm_res) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_create_resource fail\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	ctx->stDipMl.ml_sync_id = sync_id;
	return 0;
}

int Dip_Menuload_ReleaseRes(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	struct sm_ml_info stMlInfo;
	MS_U8 u8MemIndex = 0;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	if (!dev) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENXIO;
	}

	if (sm_ml_get_mem_index(ctx->stDipMl.ml_fd, &u8MemIndex, 0) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_get_mem_index fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	if (sm_ml_get_info(ctx->stDipMl.ml_fd, u8MemIndex, &stMlInfo) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_get_info fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	if (sm_ml_destroy_resource(ctx->stDipMl.ml_fd) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_destroy_resource fail\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	return 0;
}
int Dip_Menuload_Begin(struct dip_ctx *ctx)
{
	struct dip_dev *dev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	if (!dev) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENXIO;
	}

	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	if (KDrv_DIP_SetMLScript_begin(enDIPIdx) == FALSE) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,KDrv_DIP_SetMLScript_begin fail\n",
				__func__, __LINE__);
		return -EACCES;
	}

	return 0;
}

int Dip_Menuload_End(struct dip_ctx *ctx, struct hwregOut *pstDipOut)
{
	struct dip_dev *dev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	if (!dev) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENXIO;
	}

	if (_FindMappingDIP(dev, &enDIPIdx))
		return -ENODEV;

	if (!pstDipOut) {
		DIP_ERR("%s, pstDipOut is NULL\n", __func__);
		return -ENXIO;
	}
	if (KDrv_DIP_SetMLScript_end(enDIPIdx, pstDipOut) == FALSE) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,KDrv_DIP_SetMLScript_end fail\n",
				__func__, __LINE__);
		return -EACCES;
	}

	return 0;
}

int Dip_Menuload_Fire(struct dip_ctx *ctx, struct hwregOut *pstDipOut)
{
	struct dip_dev *dev = NULL;
	struct sm_ml_info stMlInfo;
	struct sm_ml_direct_write_mem_info write_cmd_info;
	struct sm_ml_fire_info fire_info;
	MS_U8 u8MemIndex = 0;

	if (!ctx) {
		DIP_ERR("%s, ctx is NULL\n", __func__);
		return -ENXIO;
	}

	dev = ctx->dev;
	if (!dev) {
		DIP_ERR("%s, dev is NULL\n", __func__);
		return -ENXIO;
	}

	if (sm_ml_get_mem_index(ctx->stDipMl.ml_fd, &u8MemIndex, 0) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_get_mem_index fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	if (sm_ml_get_info(ctx->stDipMl.ml_fd, u8MemIndex, &stMlInfo) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_get_info fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	write_cmd_info.reg = (struct sm_reg *)pstDipOut->reg;
	write_cmd_info.cmd_cnt = (__u16)pstDipOut->regCount;
	write_cmd_info.va_address = stMlInfo.start_va;
	write_cmd_info.va_max_address = stMlInfo.start_va + stMlInfo.mem_size;
	write_cmd_info.add_32_support = FALSE;
	if (sm_ml_write_cmd(&write_cmd_info) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_write_cmd fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	fire_info.cmd_cnt = write_cmd_info.cmd_cnt;
	fire_info.external = FALSE;
	fire_info.mem_index = u8MemIndex;
	if (sm_ml_fire(ctx->stDipMl.ml_fd, &fire_info) != E_SM_RET_OK) {
		v4l2_err(&dev->v4l2_dev, "%s,%d,sm_ml_fire fail\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	return 0;
}

static ssize_t dip_capability_support_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-ENXIO);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-ENXIO);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-ENXIO);
	}

	if (Dipdev->variant->u16DIPIdx > DIPR_BLEND_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-ENXIO);
	}

	DIP_CAPABILITY_SHOW("EngineSupport",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32DipSupport);
	return ssize;
}

static ssize_t dip_capability0_3ddi_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("3DDI", gDIPcap_capability[0].u323DDI);

	return ssize;
}

static ssize_t dip_capability0_dnr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DNR", gDIPcap_capability[0].u32DNR);

	return ssize;
}

static ssize_t dip_capability0_mfdec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("MFDEC", gDIPcap_capability[0].u32MFDEC);

	return ssize;
}

static ssize_t dip_capability0_dscl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DSCL", gDIPcap_capability[0].u32DSCL);

	return ssize;
}

static ssize_t dip_capability_uscl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-ENXIO);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-ENXIO);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-ENXIO);
	}

	if (Dipdev->variant->u16DIPIdx > DIPR_BLEND_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-ENXIO);
	}

	DIP_CAPABILITY_SHOW("USCL",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32USCL);
	return ssize;
}

static ssize_t dip_capability_rotate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-ENXIO);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-ENXIO);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-ENXIO);
	}

	if (Dipdev->variant->u16DIPIdx > DIPR_BLEND_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-ENXIO);
	}

	DIP_CAPABILITY_SHOW("Rotate",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32Rotate);

	return ssize;
}

static ssize_t dip_capability0_hdr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("HDR", gDIPcap_capability[0].u32HDR);

	return ssize;
}

static ssize_t dip_capability0_letterbox_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("LetterBox", gDIPcap_capability[0].u32LetterBox);

	return ssize;
}

static ssize_t dip_capability0_rwsep_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("RWSep", gDIPcap_capability[0].u32RWSep);

	return ssize;
}

static ssize_t dip_capability0_b2r_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("B2R", gDIPcap_capability[0].u32B2R);

	return ssize;
}

static ssize_t dip_capability0_dipr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DIPR", gDIPcap_capability[0].u32DIPR);

	return ssize;
}

static ssize_t dip_capability_diprblending_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-ENXIO);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-ENXIO);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-ENXIO);
	}

	if (Dipdev->variant->u16DIPIdx > DIPR_BLEND_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-ENXIO);
	}

	DIP_CAPABILITY_SHOW("DiprBlending",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32DiprBlending);
	return ssize;
}

static ssize_t dip_capability0_SDCCap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("SDCCap", gDIPcap_capability[DIP0_WIN_IDX].u32SDCCap);

	return ssize;
}

static ssize_t dip_capability_42010bTileIova_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-ENXIO);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-ENXIO);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-ENXIO);
	}

	if (Dipdev->variant->u16DIPIdx > DIPR_BLEND_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-ENXIO);
	}

	DIP_CAPABILITY_SHOW("YUV42010bitTileIova",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32YUV10bTileIova);

	return ssize;
}

static ssize_t dip_capability0_FixY2R_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FixY2R", gDIPcap_capability[DIP0_WIN_IDX].u32FixY2R);

	return ssize;
}

static ssize_t dip_capability0_FillBlack_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FillBlack", gDIPcap_capability[DIP0_WIN_IDX].u32FillBlack);

	return ssize;
}

static ssize_t dip_capability0_CapOsdb0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOsdb0", gDIPcap_capability[DIP0_WIN_IDX].u32CapOsdb0);

	return ssize;
}

static ssize_t dip_capability0_CapOSD_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOSD", gDIPcap_capability[DIP0_WIN_IDX].u32CapOSD);

	return ssize;
}

static ssize_t dip_capability_CapGOP0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("CapGOP0",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32CapGOP0);

	return ssize;
}

static ssize_t dip_capability_FrontPack_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev = dev_get_drvdata(dev);
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("FrontPack",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32FrontPack);

	return ssize;
}

static ssize_t dip_capability_HighWayPath_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);
	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("HighwayPath",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32CapHighway);
	return ssize;
}

static ssize_t dip_capability_LRWrite_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("HighwayPath",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32CapLRWrite);
	return ssize;
}

static ssize_t dip_capability_1RNW_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("1RNW",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u321RNW);
	return ssize;
}

static ssize_t dip_capability_CapB2rMain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("CapB2rMain",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].u32CapB2rMain);
	return ssize;
}

static ssize_t dip_capability_SubWidthMax_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("SubWidthMax",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].TileWidthMax);
	return ssize;
}

static ssize_t dip_capability_SubHeightMax_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev;
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	if (!dev) {
		DIP_ERR("%s Null pointer of dev\n", __func__);
		return (-EPERM);
	}

	Dipdev = dev_get_drvdata(dev);

	if (!Dipdev) {
		DIP_ERR("%s Null pointer of dip device\n", __func__);
		return (-EPERM);
	}

	if (!Dipdev->variant) {
		DIP_ERR("%s Null pointer of dip variant\n", __func__);
		return (-EPERM);
	}

	if (Dipdev->variant->u16DIPIdx > DIP2_WIN_IDX) {
		DIP_ERR("%s Invalid DIP index\n", __func__);
		return (-EPERM);
	}

	DIP_CAPABILITY_SHOW("SubHeightMax",
		gDIPcap_capability[Dipdev->variant->u16DIPIdx].TileHeightMax);
	return ssize;
}

static ssize_t dip_crc_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	MS_U8 u8Enable = 0;
	MS_U8 u8FrmCnt = 0;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	KDrv_DIP_IsEnableCRC(&u8Enable, &u8FrmCnt, enDIPIdx);
	ssize = snprintf(buf, SYS_SIZE, "%u", u8Enable);
	if ((ssize < 0) || (ssize > SYS_SIZE)) {
		DIP_ERR("%s Failed to get CRC enable status\n", __func__);
		return (-EPERM);
	}

	return ssize;
}

static ssize_t dip_crc_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	MS_U8 u8FrmCnt = DIP_CRC_FRMCNT;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	if (strncmp("on", buf, 2) == 0) {
		DIP_INFO("[%s]Enable DIP%d CRC\n", __func__, enDIPIdx);
		KDrv_DIP_EnableCRC(true, u8FrmCnt, enDIPIdx);
	} else if (strncmp("off", buf, 3) == 0) {
		DIP_INFO("[%s]Disable DIP%d CRC\n", __func__, enDIPIdx);
		KDrv_DIP_EnableCRC(false, u8FrmCnt, enDIPIdx);
	}

	return count;
}

static ssize_t dip_crc_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	MS_U32 u32Val = 0;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	KDrv_DIP_GetCrcValue(&u32Val, enDIPIdx);
	DIP_INFO("[%s]Get DIP%d CRC Value:%u\n", __func__, enDIPIdx, u32Val);
	ssize = snprintf(buf, SYS_SIZE, "%u", u32Val);
	if ((ssize < 0) || (ssize > SYS_SIZE)) {
		DIP_ERR("%s Failed to get CRC Value\n", __func__);
		return 0;
	}

	return ssize;
}

static ssize_t dip_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	KDrv_DIP_ShowDevStatus(enDIPIdx);

	return 0;
}

static ssize_t dip_show_loglevel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	uint16_t u16Size = SYS_SIZE;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	ssize +=
		scnprintf(buf + ssize, u16Size - ssize, "%u\n", gu32DipLogLevel[enDIPIdx]);

	return ssize;
}

static ssize_t dip_store_loglevel(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	int err;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	err = kstrtoul(buf, 0, &val);

	if (err)
		return err;

	gu32DipLogLevel[enDIPIdx] = val;

	return count;
}

static ssize_t dip_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	MS_U8 u8Enable = 0;
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	KDrv_DIP_IsEna(&u8Enable, enDIPIdx);
	ssize = snprintf(buf, SYS_SIZE, "%d\n", u8Enable);
	if ((ssize < 0) || (ssize > SYS_SIZE)) {
		DIP_ERR("%s Failed to get enable status\n", __func__);
		return (-EPERM);
	}

	return ssize;
}

static ssize_t dip_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct dip_dev *Dipdev = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (dev == NULL) {
		DIP_ERR("[%s]dev is NULL\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		DIP_ERR("[%s]buf is NULL\n", __func__);
		return 0;
	}

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL) {
		DIP_ERR("[%s]Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	if (strncmp("on", buf, 2) == 0) {
		DIP_INFO("[%s]Enable DIP%d Engine\n", __func__, enDIPIdx);
		KDrv_DIP_Ena(true, enDIPIdx);
	} else if (strncmp("off", buf, 3) == 0) {
		DIP_INFO("[%s]Disable DIP%d Engine\n", __func__, enDIPIdx);
		KDrv_DIP_Ena(false, enDIPIdx);
	}

	return count;
}

static ssize_t dip_capability1_3ddi_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("3DDI", gDIPcap_capability[1].u323DDI);

	return ssize;
}

static ssize_t dip_capability1_dnr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DNR", gDIPcap_capability[1].u32DNR);

	return ssize;
}

static ssize_t dip_capability1_mfdec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("MFDEC", gDIPcap_capability[1].u32MFDEC);

	return ssize;
}

static ssize_t dip_capability1_dscl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DSCL", gDIPcap_capability[1].u32DSCL);

	return ssize;
}

static ssize_t dip_capability1_hdr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	//BIT0: HDR_10
	//BIT1: HDR_TCH
	//BIT2: HDR_HLG
	//BIT3: HDR_10+
	//BIT4: HDR_VIVID
	//BIT5: HDR_DV
	DIP_CAPABILITY_SHOW("HDR", gDIPcap_capability[1].u32HDR);

	return ssize;
}

static ssize_t dip_capability1_letterbox_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("LetterBox", gDIPcap_capability[1].u32LetterBox);

	return ssize;
}

static ssize_t dip_capability1_b2r_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("B2R", gDIPcap_capability[1].u32B2R);

	return ssize;
}

static ssize_t dip_capability1_rwsep_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("RWSep", gDIPcap_capability[1].u32RWSep);

	return ssize;
}

static ssize_t dip_capability1_dipr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DIPR", gDIPcap_capability[1].u32DIPR);

	return ssize;
}

static ssize_t dip_capability1_SDCCap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("SDCCap", gDIPcap_capability[DIP1_WIN_IDX].u32SDCCap);

	return ssize;
}

static ssize_t dip_capability1_FixY2R_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FixY2R", gDIPcap_capability[DIP1_WIN_IDX].u32FixY2R);

	return ssize;
}

static ssize_t dip_capability1_FillBlack_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FillBlack", gDIPcap_capability[DIP1_WIN_IDX].u32FillBlack);

	return ssize;
}

static ssize_t dip_capability1_CapOsdb0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOsdb0", gDIPcap_capability[DIP1_WIN_IDX].u32CapOsdb0);

	return ssize;
}

static ssize_t dip_capability1_CapOSD_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOSD", gDIPcap_capability[DIP1_WIN_IDX].u32CapOSD);

	return ssize;
}

static ssize_t dip_capability2_dscl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DSCL", gDIPcap_capability[2].u32DSCL);

	return ssize;
}

static ssize_t dip_capability2_rwsep_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("RWSep", gDIPcap_capability[2].u32RWSep);

	return ssize;
}

static ssize_t dip_capability2_b2r_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("B2R", gDIPcap_capability[2].u32B2R);

	return ssize;
}

static ssize_t dip_capability2_mfdec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("MFDEC", gDIPcap_capability[2].u32MFDEC);

	return ssize;
}

static ssize_t dip_capability2_3ddi_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("3DDI", gDIPcap_capability[2].u323DDI);

	return ssize;
}

static ssize_t dip_capability2_dnr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DNR", gDIPcap_capability[2].u32DNR);

	return ssize;
}

static ssize_t dip_capability2_hdr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("HDR", gDIPcap_capability[2].u32HDR);

	return ssize;
}

static ssize_t dip_capability2_dipr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("DIPR", gDIPcap_capability[2].u32DIPR);

	return ssize;
}

static ssize_t dip_capability2_letterbox_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = 255;

	DIP_CAPABILITY_SHOW("LetterBox", gDIPcap_capability[2].u32LetterBox);

	return ssize;
}

static ssize_t dip_capability2_SDCCap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("SDCCap", gDIPcap_capability[DIP2_WIN_IDX].u32SDCCap);

	return ssize;
}

static ssize_t dip_capability2_FixY2R_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FixY2R", gDIPcap_capability[DIP2_WIN_IDX].u32FixY2R);

	return ssize;
}

static ssize_t dip_capability2_FillBlack_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("FillBlack", gDIPcap_capability[DIP2_WIN_IDX].u32FillBlack);

	return ssize;
}

static ssize_t dip_capability2_CapOsdb0_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOsdb0", gDIPcap_capability[DIP2_WIN_IDX].u32CapOsdb0);

	return ssize;
}

static ssize_t dip_capability2_CapOSD_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = SYS_SIZE;

	DIP_CAPABILITY_SHOW("CapOSD", gDIPcap_capability[DIP2_WIN_IDX].u32CapOSD);

	return ssize;
}

static ssize_t dip_show_develop_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev = dev_get_drvdata(dev);
	int ssize = 0;
	uint16_t u16Size = SYS_SIZE;
	u32 u32DevelopMode = 0;

	if (Dipdev->efuse_develop_mode) {
		if (Dipdev->sys_develop_mode)
			u32DevelopMode = 1;
		else
			u32DevelopMode = 0;
	} else {
		u32DevelopMode = 0;
	}

	ssize +=
		scnprintf(buf + ssize, u16Size - ssize, "%u\n", u32DevelopMode);

	return ssize;
}

static ssize_t dip_store_develop_mode(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct dip_dev *Dipdev = dev_get_drvdata(dev);
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);

	if (err)
		return err;

	if (Dipdev->efuse_develop_mode) {
		if (val)
			Dipdev->sys_develop_mode = 1;
		else
			Dipdev->sys_develop_mode = 0;
	} else {
		Dipdev->sys_develop_mode = 0;
		if (val)
			dev_err(dev, "MP chip, cant switch to debug chip\n");
	}

	return count;
}

static ssize_t dip_show_exe_record(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dip_dev *Dipdev =  NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	u8 u8LoppIdx = 0;
	u8 u8idx = 0;
	struct dip_fmt *pfmt;
	u64 u64StartTime = 0, u64EndTime = 0;

	Dipdev = dev_get_drvdata(dev);
	if (Dipdev == NULL)	{
		pr_crit("%s, Dipdev is NULL\n", __func__);
		return 0;
	}

	if (_FindMappingDIP(Dipdev, &enDIPIdx))
		return 0;

	for (u8LoppIdx = 0; u8LoppIdx < EXE_LOG_CNT; u8LoppIdx++) {
		if (Dipdev->exe_log_cur_idx) {
			if ((Dipdev->exe_log_cur_idx - 1) >= u8LoppIdx)
				u8idx = Dipdev->exe_log_cur_idx - 1 - u8LoppIdx;
			else
				u8idx = EXE_LOG_CNT + Dipdev->exe_log_cur_idx - 1 - u8LoppIdx;
		} else {
			u8idx = EXE_LOG_CNT - 1 - u8LoppIdx;
		}
		pr_crit("############ DIP%d Last %d Execution ###########\n", enDIPIdx, u8LoppIdx);
		pr_crit("[%d]BW Rate: %llu Bps\n",
			u8LoppIdx,  Dipdev->exe_log[u8idx].u64RealBWVal);

		if (Dipdev->exe_log[u8idx].u8BWLimitEna) {
			pr_crit("[%d]BW Limit Enable\n", u8LoppIdx);
			pr_crit("[%d]BW Limit Size: %llu Bps\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u64BWLimitVal);
			if (_IsM2mCase(Dipdev->exe_log[u8idx].enSource) == true) {
				if (Dipdev->exe_log[u8idx].u8IsB2RLite == 1) {
					pr_crit("[%d]BW Limit/Full Ratio: %llu:%lu\n", u8LoppIdx,
						Dipdev->exe_log[u8idx].u64B2RClockRate/META_UNIT,
						DIP_MAX_CLK/META_UNIT);
					pr_crit("[%d]Clock Current: %llu hz\n", u8LoppIdx,
						Dipdev->exe_log[u8idx].u64B2RClockRate);
				} else {
					pr_crit("[%d]BW Limit/Full Ratio: %d:%d\n", u8LoppIdx,
						Dipdev->exe_log[u8idx].u16FlowCtrlLimit,
						Dipdev->exe_log[u8idx].u16FlowCtrlBase);
					pr_crit("[%d]Current Clock: %llu hz\n", u8LoppIdx,
						(Dipdev->exe_log[u8idx].u64B2RClockRate*
						Dipdev->exe_log[u8idx].u16FlowCtrlLimit)/
						Dipdev->exe_log[u8idx].u16FlowCtrlBase);
				}
				pr_crit("[%d]Clock Max: %lu hz\n", u8LoppIdx, DIP_MAX_CLK);
			} else {
				pr_crit("[%d]BW Limit Disable\n", u8LoppIdx);
				pr_crit("[%d]BW Limit Size: NA\n", u8LoppIdx);
				pr_crit("[%d]BW Limit/Full Ratio: 1:1\n", u8LoppIdx);
				pr_crit("[%d]Clock Current: %lu hz\n", u8LoppIdx, DIP_MAX_CLK);
				pr_crit("[%d]Clock Max: %lu hz\n", u8LoppIdx, DIP_MAX_CLK);
			}
		} else {
			pr_crit("[%d]BW Limit Disable\n", u8LoppIdx);
			pr_crit("[%d]BW Limit Size: NA\n", u8LoppIdx);
			pr_crit("[%d]BW Limit/Full Ratio: 1:1\n", u8LoppIdx);
			pr_crit("[%d]Clock Current: %lu hz\n", u8LoppIdx, DIP_MAX_CLK);
			pr_crit("[%d]Clock Max: %lu hz\n", u8LoppIdx, DIP_MAX_CLK);
		}

		if (enDIPIdx != E_DIPR_BLEND_IDX0) {
			if (_IsM2mCase(Dipdev->exe_log[u8idx].enSource) == true) {
				pr_crit("[%d]Source: M2M\n", u8LoppIdx);
				pr_crit("[%d]Source DRAM Width: %d\n",
					u8LoppIdx, Dipdev->exe_log[u8idx].u32InputWidth);
				pr_crit("[%d]Source DRAM Height: %d\n",
					u8LoppIdx, Dipdev->exe_log[u8idx].u32InputHeight);
				pfmt = _FindDipOutMpFmt(Dipdev->exe_log[u8idx].u32Inputfourcc);
				if (!pfmt)
					pr_crit("[%d]Source DRAM Format: Invalid format\n",
					u8LoppIdx);
				else
					pr_crit("[%d]Source DRAM Format: %s\n",
					u8LoppIdx, pfmt->name);
			} else {
				pr_crit("[%d]Source: V2M, capture point: %d\n",
					u8LoppIdx, Dipdev->exe_log[u8idx].enSource);
				pr_crit("[%d]Source Video Width: %d\n",
					u8LoppIdx, Dipdev->exe_log[u8idx].u32InputWidth);
				pr_crit("[%d]Source Video Height: %d\n",
					u8LoppIdx, Dipdev->exe_log[u8idx].u32InputHeight);
				if (Dipdev->exe_log[u8idx].u32Inputfourcc ==
					E_SRC_COLOR_RGB)
					pr_crit("[%d]Source Video Format: RGB\n", u8LoppIdx);
				else if (Dipdev->exe_log[u8idx].u32Inputfourcc ==
					E_SRC_COLOR_YUV444)
					pr_crit("[%d]Source Video Format: YUV444\n", u8LoppIdx);
				else if (Dipdev->exe_log[u8idx].u32Inputfourcc ==
					E_SRC_COLOR_YUV422)
					pr_crit("[%d]Source Video Format: YUV422\n", u8LoppIdx);
				else if (Dipdev->exe_log[u8idx].u32Inputfourcc ==
					E_SRC_COLOR_YUV420)
					pr_crit("[%d]Source Video Format: YUV420\n", u8LoppIdx);
				else
					pr_crit("[%d]Source Video Format: Invalid\n", u8LoppIdx);
			}
			pr_crit("[%d]Output Width: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32outputWidth);
			pr_crit("[%d]Output Height: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32OutputHeight);
			pfmt = _FindDipCapMpFmt(Dipdev, Dipdev->exe_log[u8idx].u32Outputfourcc);
			if (!pfmt)
				pr_crit("[%d]Output Format: Invalid format\n", u8LoppIdx);
			else
				pr_crit("[%d]Output Format: %s\n", u8LoppIdx, pfmt->name);
		} else {
			pr_crit("[%d]Source DRAM Width: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32InputWidth);
			pr_crit("[%d]Source DRAM Height: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32InputHeight);
			pfmt = _FindDipOutMpFmt(Dipdev->exe_log[u8idx].u32Inputfourcc);
			if (!pfmt)
				pr_crit("[%d]Source DRAM Format: Invalid format\n", u8LoppIdx);
			else
				pr_crit("[%d]Source DRAM Format: %s\n", u8LoppIdx, pfmt->name);

			pr_crit("[%d]blending display x: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u16BlendDispX);
			pr_crit("[%d]blending display y: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u16BlendDispY);
			pr_crit("[%d]blending Output Width: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32outputWidth);
			pr_crit("[%d]blending Output Height: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32OutputHeight);
			pr_crit("[%d]blending dst color format: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].enBlendDstClrFmt);
		}

		pr_crit("[%d]Rotation: %d\n",
			u8LoppIdx, Dipdev->exe_log[u8idx].u16Rotation);

		if (Dipdev->exe_log[u8idx].u32PqWinTime) {
			pr_crit("[%d]PQ Win Time: %d\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u32PqWinTime);
		} else {
			pr_crit("[%d]PQ Win Time: No Information\n", u8LoppIdx);
		}

		if (Dipdev->exe_log[u8idx].u32PqWinTime) {
			pr_crit("[%d]PQ Remainder Budget: %llu\n",
				u8LoppIdx, Dipdev->exe_log[u8idx].u64PqRemainderBudget);
		} else {
			pr_crit("[%d]PQ Remainder Budget: No Information\n", u8LoppIdx);
		}
		pr_crit("[%d]Execute Str Time: %lld.%06ld sec\n", u8LoppIdx,
			Dipdev->exe_log[u8idx].StartTime.tv_sec,
			Dipdev->exe_log[u8idx].StartTime.tv_nsec/NS_TO_US);
		pr_crit("[%d]Execute End Time: %lld.%06ld sec\n", u8LoppIdx,
			Dipdev->exe_log[u8idx].EndTime.tv_sec,
			Dipdev->exe_log[u8idx].EndTime.tv_nsec/NS_TO_US);

		u64StartTime = (Dipdev->exe_log[u8idx].StartTime.tv_sec*S_TO_US)
			+ (Dipdev->exe_log[u8idx].StartTime.tv_nsec/NS_TO_US);
		u64EndTime = (Dipdev->exe_log[u8idx].EndTime.tv_sec*S_TO_US)
			+ (Dipdev->exe_log[u8idx].EndTime.tv_nsec/NS_TO_US);
		pr_crit("[%d]Execute Duration Time: %llu us\n",
			u8LoppIdx, u64EndTime-u64StartTime);
	}

	return 0;
}

static ssize_t dip_capability_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static struct device_attribute dip_cap_attr0[] = {
	__ATTR(dip_capability_support, ATTR_MOD, dip_capability_support_show,
			dip_capability_store),
	__ATTR(dip_capability_dscl, ATTR_MOD, dip_capability0_dscl_show,
			dip_capability_store),
	__ATTR(dip_capability_uscl, ATTR_MOD, dip_capability_uscl_show,
			dip_capability_store),
	__ATTR(dip_capability_rotate, ATTR_MOD, dip_capability_rotate_show,
			dip_capability_store),
	__ATTR(dip_capability_rwsep, ATTR_MOD, dip_capability0_rwsep_show,
			dip_capability_store),
	__ATTR(dip_capability_b2r, ATTR_MOD, dip_capability0_b2r_show,
			dip_capability_store),
	__ATTR(dip_capability_mfdec, ATTR_MOD, dip_capability0_mfdec_show,
			dip_capability_store),
	__ATTR(dip_capability_3ddi, ATTR_MOD, dip_capability0_3ddi_show,
			dip_capability_store),
	__ATTR(dip_capability_dnr, ATTR_MOD, dip_capability0_dnr_show,
			dip_capability_store),
	__ATTR(dip_capability_hdr, ATTR_MOD, dip_capability0_hdr_show,
			dip_capability_store),
	__ATTR(dip_capability_letterbox, ATTR_MOD, dip_capability0_letterbox_show,
			dip_capability_store),
	__ATTR(dip_capability_dipr, ATTR_MOD, dip_capability0_dipr_show,
			dip_capability_store),
	__ATTR(dip_capability_diprblending, ATTR_MOD, dip_capability_diprblending_show,
			dip_capability_store),
	__ATTR(dip_capability_SDCCap, ATTR_MOD, dip_capability0_SDCCap_show,
			dip_capability_store),
	__ATTR(dip_capability_YUV42010bitTileIova, ATTR_MOD, dip_capability_42010bTileIova_show,
			dip_capability_store),
	__ATTR(dip_capability_FixY2R, ATTR_MOD, dip_capability0_FixY2R_show,
			dip_capability_store),
	__ATTR(dip_capability_FillBlack, ATTR_MOD, dip_capability0_FillBlack_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOsdb0, ATTR_MOD, dip_capability0_CapOsdb0_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOSD, ATTR_MOD, dip_capability0_CapOSD_show,
			dip_capability_store),
	__ATTR(dip_crc_enable, ATTR_MOD, dip_crc_enable_show,
			dip_crc_enable_store),
	__ATTR(dip_crc_result, ATTR_MOD, dip_crc_result_show,
			dip_capability_store),
	__ATTR(dip_capability_CapGOP0, ATTR_MOD, dip_capability_CapGOP0_show,
			dip_capability_store),
	__ATTR(dip_debug, ATTR_MOD, dip_debug_show,
			dip_capability_store),
	__ATTR(dip_loglevel, ATTR_MOD, dip_show_loglevel,
			dip_store_loglevel),
	__ATTR(develop_mode, ATTR_MOD, dip_show_develop_mode,
			dip_store_develop_mode),
	__ATTR(dip_capability_FrontPack, ATTR_MOD, dip_capability_FrontPack_show,
			dip_capability_store),
	__ATTR(dip_exe_record, ATTR_MOD, dip_show_exe_record,
			dip_capability_store),
	__ATTR(dip_capability_HighWayPath, ATTR_MOD, dip_capability_HighWayPath_show,
			dip_capability_store),
	__ATTR(dip_capability_LRWrite, ATTR_MOD, dip_capability_LRWrite_show,
			dip_capability_store),
	__ATTR(dip_capability_1RNW, ATTR_MOD, dip_capability_1RNW_show,
			dip_capability_store),
	__ATTR(dip_enable, ATTR_MOD, dip_enable_show,
			dip_enable_store),
	__ATTR(dip_capability_CapB2rMain, ATTR_MOD, dip_capability_CapB2rMain_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_h_max, ATTR_MOD, dip_capability_SubWidthMax_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_v_max, ATTR_MOD, dip_capability_SubHeightMax_show,
			dip_capability_store),
};

static struct device_attribute dip_cap_attr1[] = {
	__ATTR(dip_capability_support, ATTR_MOD, dip_capability_support_show,
			dip_capability_store),
	__ATTR(dip_capability_dscl, ATTR_MOD, dip_capability1_dscl_show,
			dip_capability_store),
	__ATTR(dip_capability_uscl, ATTR_MOD, dip_capability_uscl_show,
			dip_capability_store),
	__ATTR(dip_capability_rotate, ATTR_MOD, dip_capability_rotate_show,
			dip_capability_store),
	__ATTR(dip_capability_rwsep, ATTR_MOD, dip_capability1_rwsep_show,
			dip_capability_store),
	__ATTR(dip_capability_b2r, ATTR_MOD, dip_capability1_b2r_show,
			dip_capability_store),
	__ATTR(dip_capability_mfdec, ATTR_MOD, dip_capability1_mfdec_show,
			dip_capability_store),
	__ATTR(dip_capability_3ddi, ATTR_MOD, dip_capability1_3ddi_show,
			dip_capability_store),
	__ATTR(dip_capability_dnr, ATTR_MOD, dip_capability1_dnr_show,
			dip_capability_store),
	__ATTR(dip_capability_hdr, ATTR_MOD, dip_capability1_hdr_show,
			dip_capability_store),
	__ATTR(dip_capability_letterbox, ATTR_MOD, dip_capability1_letterbox_show,
			dip_capability_store),
	__ATTR(dip_capability_dipr, ATTR_MOD, dip_capability1_dipr_show,
			dip_capability_store),
	__ATTR(dip_capability_diprblending, ATTR_MOD, dip_capability_diprblending_show,
			dip_capability_store),
	__ATTR(dip_capability_SDCCap, ATTR_MOD, dip_capability1_SDCCap_show,
			dip_capability_store),
	__ATTR(dip_capability_YUV42010bitTileIova, ATTR_MOD, dip_capability_42010bTileIova_show,
			dip_capability_store),
	__ATTR(dip_capability_FixY2R, ATTR_MOD, dip_capability1_FixY2R_show,
			dip_capability_store),
	__ATTR(dip_capability_FillBlack, ATTR_MOD, dip_capability1_FillBlack_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOsdb0, ATTR_MOD, dip_capability1_CapOsdb0_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOSD, ATTR_MOD, dip_capability1_CapOSD_show,
			dip_capability_store),
	__ATTR(dip_crc_enable, ATTR_MOD, dip_crc_enable_show,
			dip_crc_enable_store),
	__ATTR(dip_crc_result, ATTR_MOD, dip_crc_result_show,
			dip_capability_store),
	__ATTR(dip_capability_CapGOP0, ATTR_MOD, dip_capability_CapGOP0_show,
			dip_capability_store),
	__ATTR(dip_debug, ATTR_MOD, dip_debug_show,
			dip_capability_store),
	__ATTR(dip_loglevel, ATTR_MOD, dip_show_loglevel,
			dip_store_loglevel),
	__ATTR(develop_mode, ATTR_MOD, dip_show_develop_mode,
			dip_store_develop_mode),
	__ATTR(dip_capability_FrontPack, ATTR_MOD, dip_capability_FrontPack_show,
			dip_capability_store),
	__ATTR(dip_exe_record, ATTR_MOD, dip_show_exe_record,
			dip_capability_store),
	__ATTR(dip_capability_HighWayPath, ATTR_MOD, dip_capability_HighWayPath_show,
			dip_capability_store),
	__ATTR(dip_capability_LRWrite, ATTR_MOD, dip_capability_LRWrite_show,
			dip_capability_store),
	__ATTR(dip_capability_1RNW, ATTR_MOD, dip_capability_1RNW_show,
			dip_capability_store),
	__ATTR(dip_enable, ATTR_MOD, dip_enable_show,
			dip_enable_store),
	__ATTR(dip_capability_CapB2rMain, ATTR_MOD, dip_capability_CapB2rMain_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_h_max, ATTR_MOD, dip_capability_SubWidthMax_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_v_max, ATTR_MOD, dip_capability_SubHeightMax_show,
			dip_capability_store),
};

static struct device_attribute dip_cap_attr2[] = {
	__ATTR(dip_capability_support, ATTR_MOD, dip_capability_support_show,
			dip_capability_store),
	__ATTR(dip_capability_dscl, ATTR_MOD, dip_capability2_dscl_show,
			dip_capability_store),
	__ATTR(dip_capability_uscl, ATTR_MOD, dip_capability_uscl_show,
			dip_capability_store),
	__ATTR(dip_capability_rotate, ATTR_MOD, dip_capability_rotate_show,
			dip_capability_store),
	__ATTR(dip_capability_rwsep, ATTR_MOD, dip_capability2_rwsep_show,
			dip_capability_store),
	__ATTR(dip_capability_b2r, ATTR_MOD, dip_capability2_b2r_show,
			dip_capability_store),
	__ATTR(dip_capability_mfdec, ATTR_MOD, dip_capability2_mfdec_show,
			dip_capability_store),
	__ATTR(dip_capability_3ddi, ATTR_MOD, dip_capability2_3ddi_show,
			dip_capability_store),
	__ATTR(dip_capability_dnr, ATTR_MOD, dip_capability2_dnr_show,
			dip_capability_store),
	__ATTR(dip_capability_hdr, ATTR_MOD, dip_capability2_hdr_show,
			dip_capability_store),
	__ATTR(dip_capability_letterbox, ATTR_MOD, dip_capability2_letterbox_show,
			dip_capability_store),
	__ATTR(dip_capability_dipr, ATTR_MOD, dip_capability2_dipr_show,
			dip_capability_store),
	__ATTR(dip_capability_diprblending, ATTR_MOD, dip_capability_diprblending_show,
			dip_capability_store),
	__ATTR(dip_capability_SDCCap, ATTR_MOD, dip_capability2_SDCCap_show,
			dip_capability_store),
	__ATTR(dip_capability_YUV42010bitTileIova, ATTR_MOD, dip_capability_42010bTileIova_show,
			dip_capability_store),
	__ATTR(dip_capability_FixY2R, ATTR_MOD, dip_capability2_FixY2R_show,
			dip_capability_store),
	__ATTR(dip_capability_FillBlack, ATTR_MOD, dip_capability2_FillBlack_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOsdb0, ATTR_MOD, dip_capability2_CapOsdb0_show,
			dip_capability_store),
	__ATTR(dip_capability_CapOSD, ATTR_MOD, dip_capability2_CapOSD_show,
			dip_capability_store),
	__ATTR(dip_crc_enable, ATTR_MOD, dip_crc_enable_show,
			dip_crc_enable_store),
	__ATTR(dip_crc_result, ATTR_MOD, dip_crc_result_show,
			dip_capability_store),
	__ATTR(dip_capability_CapGOP0, ATTR_MOD, dip_capability_CapGOP0_show,
			dip_capability_store),
	__ATTR(dip_debug, ATTR_MOD, dip_debug_show,
			dip_capability_store),
	__ATTR(dip_loglevel, ATTR_MOD, dip_show_loglevel,
			dip_store_loglevel),
	__ATTR(develop_mode, ATTR_MOD, dip_show_develop_mode,
			dip_store_develop_mode),
	__ATTR(dip_capability_FrontPack, ATTR_MOD, dip_capability_FrontPack_show,
			dip_capability_store),
	__ATTR(dip_exe_record, ATTR_MOD, dip_show_exe_record,
			dip_capability_store),
	__ATTR(dip_capability_HighWayPath, ATTR_MOD, dip_capability_HighWayPath_show,
			dip_capability_store),
	__ATTR(dip_capability_LRWrite, ATTR_MOD, dip_capability_LRWrite_show,
			dip_capability_store),
	__ATTR(dip_capability_1RNW, ATTR_MOD, dip_capability_1RNW_show,
			dip_capability_store),
	__ATTR(dip_enable, ATTR_MOD, dip_enable_show,
			dip_enable_store),
	__ATTR(dip_capability_CapB2rMain, ATTR_MOD, dip_capability_CapB2rMain_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_h_max, ATTR_MOD, dip_capability_SubWidthMax_show,
			dip_capability_store),
	__ATTR(dip_capability_sub_v_max, ATTR_MOD, dip_capability_SubHeightMax_show,
			dip_capability_store),
};

static struct device_attribute dipr_blend_cap_attr2[] = {
	__ATTR(dip_capability_support, ATTR_MOD, dip_capability_support_show,
			dip_capability_store),
	__ATTR(dip_capability_uscl, ATTR_MOD, dip_capability_uscl_show,
			dip_capability_store),
	__ATTR(dip_capability_rotate, ATTR_MOD, dip_capability_rotate_show,
			dip_capability_store),
	__ATTR(dip_capability_YUV42010bitTileIova, ATTR_MOD, dip_capability_42010bTileIova_show,
			dip_capability_store),
	__ATTR(develop_mode, ATTR_MOD, dip_show_develop_mode,
			dip_store_develop_mode),
	__ATTR(dip_loglevel, ATTR_MOD, dip_show_loglevel,
			dip_store_loglevel),
	__ATTR(dip_exe_record, ATTR_MOD, dip_show_exe_record,
			dip_capability_store),
};

static void dip_get_capability(struct dip_dev *dip_dev, struct platform_device *pdev)
{
	if (dip_dev->variant->u16DIPIdx == 0) {
		memset(&gDIPcap_capability[0], 0, sizeof(struct dip_cap_dev));
		gDIPcap_capability[0].u32DipSupport = dip_dev->dipcap_dev.u32DipSupport;
		gDIPcap_capability[0].u32DSCL = dip_dev->dipcap_dev.u32DSCL;
		gDIPcap_capability[0].u32USCL = dip_dev->dipcap_dev.u32USCL;
		gDIPcap_capability[0].u32Rotate = dip_dev->dipcap_dev.u32Rotate;
		gDIPcap_capability[0].u32RWSep = dip_dev->dipcap_dev.u32RWSep;
		gDIPcap_capability[0].u32B2R = dip_dev->dipcap_dev.u32B2R;
		gDIPcap_capability[0].u32MFDEC = dip_dev->dipcap_dev.u32MFDEC;
		gDIPcap_capability[0].u323DDI = dip_dev->dipcap_dev.u323DDI;
		gDIPcap_capability[0].u32DNR = dip_dev->dipcap_dev.u32DNR;
		gDIPcap_capability[0].u32HDR = dip_dev->dipcap_dev.u32HDR;
		gDIPcap_capability[0].u32LetterBox = dip_dev->dipcap_dev.u32LetterBox;
		gDIPcap_capability[0].u32DIPR = dip_dev->dipcap_dev.u32DIPR;
		gDIPcap_capability[0].u32DiprBlending = dip_dev->dipcap_dev.u32DiprBlending;
		gDIPcap_capability[DIP0_WIN_IDX].u32SDCCap = dip_dev->SDCSupport;
		gDIPcap_capability[DIP0_WIN_IDX].u32YUV10bTileIova =
			dip_dev->dipcap_dev.u32YUV10bTileIova;
		gDIPcap_capability[DIP0_WIN_IDX].u32DevelopMode = dip_dev->efuse_develop_mode;
		gDIPcap_capability[DIP0_WIN_IDX].u32YUV42010bTileIova =
			dip_dev->dipcap_dev.u32YUV42010bTileIova;
		gDIPcap_capability[DIP0_WIN_IDX].u32FixY2R = dip_dev->dipcap_dev.u32FixY2R;
		gDIPcap_capability[DIP0_WIN_IDX].u32FillBlack = dip_dev->dipcap_dev.u32FillBlack;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapOsdb0 = dip_dev->dipcap_dev.u32CapOsdb0;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapOSD = dip_dev->dipcap_dev.u32CapOSD;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapGOP0 = dip_dev->dipcap_dev.u32CapGOP0;
		gDIPcap_capability[DIP0_WIN_IDX].u32FrontPack = dip_dev->dipcap_dev.u32FrontPack;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapHighway = dip_dev->dipcap_dev.u32CapHighway;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapLRWrite = dip_dev->dipcap_dev.u32CapLRWrite;
		gDIPcap_capability[DIP0_WIN_IDX].u321RNW = dip_dev->dipcap_dev.u321RNW;
		gDIPcap_capability[DIP0_WIN_IDX].u32CapB2rMain = dip_dev->dipcap_dev.u32CapB2rMain;
		gDIPcap_capability[DIP0_WIN_IDX].TileWidthMax = dip_dev->dipcap_dev.TileWidthMax;
		gDIPcap_capability[DIP0_WIN_IDX].TileHeightMax = dip_dev->dipcap_dev.TileHeightMax;
	} else if (dip_dev->variant->u16DIPIdx == 1) {
		memset(&gDIPcap_capability[1], 0, sizeof(struct dip_cap_dev));
		gDIPcap_capability[1].u32DipSupport = dip_dev->dipcap_dev.u32DipSupport;
		gDIPcap_capability[1].u32DSCL = dip_dev->dipcap_dev.u32DSCL;
		gDIPcap_capability[1].u32USCL = dip_dev->dipcap_dev.u32USCL;
		gDIPcap_capability[1].u32Rotate = dip_dev->dipcap_dev.u32Rotate;
		gDIPcap_capability[1].u32RWSep = dip_dev->dipcap_dev.u32RWSep;
		gDIPcap_capability[1].u32B2R = dip_dev->dipcap_dev.u32B2R;
		gDIPcap_capability[1].u32MFDEC = dip_dev->dipcap_dev.u32MFDEC;
		gDIPcap_capability[1].u323DDI = dip_dev->dipcap_dev.u323DDI;
		gDIPcap_capability[1].u32DNR = dip_dev->dipcap_dev.u32DNR;
		gDIPcap_capability[1].u32HDR = dip_dev->dipcap_dev.u32HDR;
		gDIPcap_capability[1].u32LetterBox = dip_dev->dipcap_dev.u32LetterBox;
		gDIPcap_capability[1].u32DIPR = dip_dev->dipcap_dev.u32DIPR;
		gDIPcap_capability[1].u32DiprBlending = dip_dev->dipcap_dev.u32DiprBlending;
		gDIPcap_capability[DIP1_WIN_IDX].u32SDCCap = dip_dev->SDCSupport;
		gDIPcap_capability[DIP1_WIN_IDX].u32YUV10bTileIova =
			dip_dev->dipcap_dev.u32YUV10bTileIova;
		gDIPcap_capability[DIP1_WIN_IDX].u32DevelopMode = dip_dev->efuse_develop_mode;
		gDIPcap_capability[DIP1_WIN_IDX].u32YUV42010bTileIova =
			dip_dev->dipcap_dev.u32YUV42010bTileIova;
		gDIPcap_capability[DIP1_WIN_IDX].u32FixY2R = dip_dev->dipcap_dev.u32FixY2R;
		gDIPcap_capability[DIP1_WIN_IDX].u32FillBlack = dip_dev->dipcap_dev.u32FillBlack;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapOsdb0 = dip_dev->dipcap_dev.u32CapOsdb0;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapOSD = dip_dev->dipcap_dev.u32CapOSD;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapGOP0 = dip_dev->dipcap_dev.u32CapGOP0;
		gDIPcap_capability[DIP1_WIN_IDX].u32FrontPack = dip_dev->dipcap_dev.u32FrontPack;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapHighway = dip_dev->dipcap_dev.u32CapHighway;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapLRWrite = dip_dev->dipcap_dev.u32CapLRWrite;
		gDIPcap_capability[DIP1_WIN_IDX].u321RNW = dip_dev->dipcap_dev.u321RNW;
		gDIPcap_capability[DIP1_WIN_IDX].u32CapB2rMain = dip_dev->dipcap_dev.u32CapB2rMain;
		gDIPcap_capability[DIP1_WIN_IDX].TileWidthMax = dip_dev->dipcap_dev.TileWidthMax;
		gDIPcap_capability[DIP1_WIN_IDX].TileHeightMax = dip_dev->dipcap_dev.TileHeightMax;
	} else if (dip_dev->variant->u16DIPIdx == 2) {
		memset(&gDIPcap_capability[2], 0, sizeof(struct dip_cap_dev));
		gDIPcap_capability[DIP2_WIN_IDX].u32DipSupport = dip_dev->dipcap_dev.u32DipSupport;
		gDIPcap_capability[2].u32DSCL = dip_dev->dipcap_dev.u32DSCL;
		gDIPcap_capability[2].u32USCL = dip_dev->dipcap_dev.u32USCL;
		gDIPcap_capability[2].u32Rotate = dip_dev->dipcap_dev.u32Rotate;
		gDIPcap_capability[2].u32B2R = dip_dev->dipcap_dev.u32B2R;
		gDIPcap_capability[2].u32MFDEC = dip_dev->dipcap_dev.u32MFDEC;
		gDIPcap_capability[2].u323DDI = dip_dev->dipcap_dev.u323DDI;
		gDIPcap_capability[2].u32DNR = dip_dev->dipcap_dev.u32DNR;
		gDIPcap_capability[2].u32HDR = dip_dev->dipcap_dev.u32HDR;
		gDIPcap_capability[2].u32LetterBox = dip_dev->dipcap_dev.u32LetterBox;
		gDIPcap_capability[2].u32DIPR = dip_dev->dipcap_dev.u32DIPR;
		gDIPcap_capability[2].u32DiprBlending = dip_dev->dipcap_dev.u32DiprBlending;
		gDIPcap_capability[DIP2_WIN_IDX].u32SDCCap = dip_dev->SDCSupport;
		gDIPcap_capability[DIP2_WIN_IDX].u32YUV10bTileIova =
			dip_dev->dipcap_dev.u32YUV10bTileIova;
		gDIPcap_capability[DIP2_WIN_IDX].u32DevelopMode = dip_dev->efuse_develop_mode;
		gDIPcap_capability[DIP2_WIN_IDX].u32YUV42010bTileIova =
			dip_dev->dipcap_dev.u32YUV42010bTileIova;
		gDIPcap_capability[DIP2_WIN_IDX].u32FixY2R = dip_dev->dipcap_dev.u32FixY2R;
		gDIPcap_capability[DIP2_WIN_IDX].u32FillBlack = dip_dev->dipcap_dev.u32FillBlack;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapOsdb0 = dip_dev->dipcap_dev.u32CapOsdb0;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapOSD = dip_dev->dipcap_dev.u32CapOSD;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapGOP0 = dip_dev->dipcap_dev.u32CapGOP0;
		gDIPcap_capability[DIP2_WIN_IDX].u32FrontPack = dip_dev->dipcap_dev.u32FrontPack;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapHighway = dip_dev->dipcap_dev.u32CapHighway;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapLRWrite = dip_dev->dipcap_dev.u32CapLRWrite;
		gDIPcap_capability[DIP2_WIN_IDX].u321RNW = dip_dev->dipcap_dev.u321RNW;
		gDIPcap_capability[DIP2_WIN_IDX].u32CapB2rMain = dip_dev->dipcap_dev.u32CapB2rMain;
		gDIPcap_capability[DIP2_WIN_IDX].TileWidthMax = dip_dev->dipcap_dev.TileWidthMax;
		gDIPcap_capability[DIP2_WIN_IDX].TileHeightMax = dip_dev->dipcap_dev.TileHeightMax;
	} else if (dip_dev->variant->u16DIPIdx == DIPR_BLEND_WIN_IDX) {
		memset(&gDIPcap_capability[DIPR_BLEND_WIN_IDX], 0, sizeof(struct dip_cap_dev));
		gDIPcap_capability[DIPR_BLEND_WIN_IDX].u32DipSupport =
			dip_dev->dipcap_dev.u32DipSupport;
		gDIPcap_capability[DIPR_BLEND_WIN_IDX].u32USCL = dip_dev->dipcap_dev.u32USCL;
		gDIPcap_capability[DIPR_BLEND_WIN_IDX].u32Rotate = dip_dev->dipcap_dev.u32Rotate;
		gDIPcap_capability[DIPR_BLEND_WIN_IDX].u32YUV10bTileIova =
			dip_dev->dipcap_dev.u32YUV10bTileIova;
	} else {
		dev_err(&pdev->dev, "Failed get capability\r\n");
	}
}

static int dip_read_dts_u32(struct device *property_dev,
				struct device_node *node, char *s, u32 *val)
{
	int ret = 0;

	if (node == NULL) {
		DIP_ERR("[%s,%d]node pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (s == NULL) {
		DIP_ERR("[%s,%d]string pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (val == NULL) {
		DIP_ERR("[%s,%d]val pointer is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, s, val);
	if (ret != 0) {
		DIP_ERR("[%s,%d]Failed to get %s\n", __func__, __LINE__, s);
		return -ENOENT;
	}

	return ret;
}

static int dip_parse_cap_dts(struct dip_dev *dip_dev, struct platform_device *pdev)
{
	struct device *property_dev = &pdev->dev;
	struct device_node *bank_node;
	int ret = 0;
	u16 u16DIPIdx;

	if (!dip_dev || !pdev) {
		DIP_ERR("[%s,%d]Null pointer\n", __func__, __LINE__);
		return -ENXIO;
	}

	if (!dip_dev->variant) {
		DIP_ERR("[%s,%d]Null pointer of dip variant\n", __func__, __LINE__);
		return -ENXIO;
	}
	u16DIPIdx = dip_dev->variant->u16DIPIdx;

	bank_node = of_find_node_by_name(property_dev->of_node, "capability");

	if (bank_node == NULL) {
		dev_err(&pdev->dev, "Failed to get banks node\r\n");
		return -EINVAL;
	}

	ret |= dip_read_dts_u32(property_dev, bank_node, "DipSupport",
					&(dip_dev->dipcap_dev.u32DipSupport));

	ret |= dip_read_dts_u32(property_dev, bank_node, "USCL",
					&(dip_dev->dipcap_dev.u32USCL));

	ret |= dip_read_dts_u32(property_dev, bank_node, "Rotate",
					&(dip_dev->dipcap_dev.u32Rotate));

	ret |= dip_read_dts_u32(property_dev, bank_node, "DiprPixelNum",
					&(dip_dev->dipcap_dev.u32DiprPixelNum));

	ret |= dip_read_dts_u32(property_dev, bank_node, "fixed_dd_index",
					&(dip_dev->dipcap_dev.u32MinorNo));

	ret |= dip_read_dts_u32(property_dev, bank_node, "TileWidthMax",
					&(dip_dev->dipcap_dev.TileWidthMax));

	ret |= dip_read_dts_u32(property_dev, bank_node, "TileHeightMax",
					&(dip_dev->dipcap_dev.TileHeightMax));

	ret |= dip_read_dts_u32(property_dev, bank_node, "yuv10bTileIova",
					&(dip_dev->dipcap_dev.u32YUV10bTileIova));

	ret |= dip_read_dts_u32(property_dev, bank_node, "HvspInW",
					&(dip_dev->dipcap_dev.u32HvspInW));

	ret |= dip_read_dts_u32(property_dev, bank_node, "MiuBus",
					&(dip_dev->dipcap_dev.u32MiuBus));

	ret |= dip_read_dts_u32(property_dev, bank_node, "DiprVSD",
					&(dip_dev->dipcap_dev.u32DiprVSD));

	if (u16DIPIdx != DIPR_BLEND_WIN_IDX) {
		ret |= dip_read_dts_u32(property_dev, bank_node, "DSCL",
						&(dip_dev->dipcap_dev.u32DSCL));

		ret |= dip_read_dts_u32(property_dev, bank_node, "RWSep",
						&(dip_dev->dipcap_dev.u32RWSep));

		ret |= dip_read_dts_u32(property_dev, bank_node, "B2R",
						&(dip_dev->dipcap_dev.u32B2R));

		ret |= dip_read_dts_u32(property_dev, bank_node, "MFDEC",
						&(dip_dev->dipcap_dev.u32MFDEC));

		ret |= dip_read_dts_u32(property_dev, bank_node, "3DDI",
						&(dip_dev->dipcap_dev.u323DDI));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DNR",
						&(dip_dev->dipcap_dev.u32DNR));

		ret |= dip_read_dts_u32(property_dev, bank_node, "HDR",
						&(dip_dev->dipcap_dev.u32HDR));

		ret |= dip_read_dts_u32(property_dev, bank_node, "LetterBox",
						&(dip_dev->dipcap_dev.u32LetterBox));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DIPR",
						&(dip_dev->dipcap_dev.u32DIPR));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DiprBlending",
					&(dip_dev->dipcap_dev.u32DiprBlending));

		ret |= dip_read_dts_u32(property_dev, bank_node, "B2rPixelNum",
						&(dip_dev->dipcap_dev.u32B2rPixelNum));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DipPixelNum",
						&(dip_dev->dipcap_dev.u32DipPixelNum));

		ret |= dip_read_dts_u32(property_dev, bank_node, "FrontPack",
						&(dip_dev->dipcap_dev.u32FrontPack));

		ret |= dip_read_dts_u32(property_dev, bank_node, "CropAlign",
						&(dip_dev->dipcap_dev.u32CropAlign));

		ret |= dip_read_dts_u32(property_dev, bank_node, "HMirrorSz",
						&(dip_dev->dipcap_dev.u32HMirSz));

		ret |= dip_read_dts_u32(property_dev, bank_node, "RWSepVer",
						&(dip_dev->dipcap_dev.u32RWSepVer));

		ret |= dip_read_dts_u32(property_dev, bank_node, "VsdInW",
						&(dip_dev->dipcap_dev.u32VsdInW));

		ret |= dip_read_dts_u32(property_dev, bank_node, "B2rHttPixelUnit",
						&(dip_dev->dipcap_dev.u32B2rHttPixelUnit));

		ret |= dip_read_dts_u32(property_dev, bank_node, "SDCCap",
						&(dip_dev->dipcap_dev.u32SDCCap));

		ret |= dip_read_dts_u32(property_dev, bank_node, "MuxClkVer",
						&(dip_dev->dipcap_dev.u32ClkVer));

		ret |= dip_read_dts_u32(property_dev, bank_node, "FixY2R",
						&(dip_dev->dipcap_dev.u32FixY2R));

		ret |= dip_read_dts_u32(property_dev, bank_node, "FillBlack",
						&(dip_dev->dipcap_dev.u32FillBlack));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DipwTile",
						&(dip_dev->dipcap_dev.u32DipwTile));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DipwBKTile",
						&(dip_dev->dipcap_dev.u32DipwBKTile));

		ret |= dip_read_dts_u32(property_dev, bank_node, "DipwYV12",
						&(dip_dev->dipcap_dev.u32DipwYV12));

		ret |= dip_read_dts_u32(property_dev, bank_node, "CapOsdb0",
						&(dip_dev->dipcap_dev.u32CapOsdb0));

		ret |= dip_read_dts_u32(property_dev, bank_node, "CapOSD",
						&(dip_dev->dipcap_dev.u32CapOSD));

		ret |= dip_read_dts_u32(property_dev, bank_node, "2PHighway",
						&(dip_dev->dipcap_dev.u32CapHighway));

		ret |= dip_read_dts_u32(property_dev, bank_node, "LRWrite",
						&(dip_dev->dipcap_dev.u32CapLRWrite));

		ret |= dip_read_dts_u32(property_dev, bank_node, "CapGOP0",
						&(dip_dev->dipcap_dev.u32CapGOP0));

		ret |= dip_read_dts_u32(property_dev, bank_node, "1RNW",
						&(dip_dev->dipcap_dev.u321RNW));

		ret |= dip_read_dts_u32(property_dev, bank_node, "YUV420LinearWidthMax",
						&(dip_dev->dipcap_dev.u32YUV420LinearWidthMax));

		ret |= dip_read_dts_u32(property_dev, bank_node, "CapB2rMain",
						&(dip_dev->dipcap_dev.u32CapB2rMain));
	}

	return ret;
}

static int dip_parse_iommu_dts(struct dip_dev *dip_dev, struct platform_device *pdev)
{
	struct device *property_dev = &pdev->dev;
	struct device_node *bank_node;

	bank_node = of_find_node_by_name(property_dev->of_node, "iommu_buf_tag");

	if (bank_node == NULL) {
		dev_err(&pdev->dev, "Failed to get banks node\r\n");
		return -EINVAL;
	}

	if (of_property_read_u32(bank_node, "di_buf", &(dip_dev->di_buf_tag))) {
		dev_err(&pdev->dev, "Failed to get iommu buf tag\n");
		return -EINVAL;
	}

	return 0;
}

static int dip_set_ioBase(struct dip_dev *dev, struct platform_device *pdev)
{
	int ret = 0;
	MS_U8 u8IdxDts = 0;
	MS_U8 u8IdxHwreg = 0;

	//DIPW
	u8IdxDts = E_DTS_DIP0;
	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX)
		u8IdxHwreg = E_HWREG_DIP0;
	else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX)
		u8IdxHwreg = E_HWREG_DIP1;
	else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX)
		u8IdxHwreg = E_HWREG_DIP2;
	else
		u8IdxHwreg = E_HWREG_DIPR;
	ret = _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
	DIP_CHECK_RETURN(ret, "_SetIoBase");

	if (u8IdxHwreg == E_HWREG_DIPR) {
		u8IdxDts = E_DTS_VB;
		u8IdxHwreg = E_HWREG_VB;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");

		u8IdxDts = E_DTS_ACE;
		u8IdxHwreg = E_HWREG_ACE;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");
	}
	//DIPR
	u8IdxDts = E_DTS_DIPR;
	u8IdxHwreg = E_HWREG_DIPR;
	ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
	DIP_CHECK_RETURN(ret, "_SetIoBase");

	//DIP_TOP
	u8IdxDts = E_DTS_DIPTOP;
	u8IdxHwreg = E_HWREG_DIPTOP;
	ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
	DIP_CHECK_RETURN(ret, "_SetIoBase");

	if (dev->dipcap_dev.u323DDI == 1) {
		//DDI1 bank
		u8IdxDts = E_DTS_DDI1;
		u8IdxHwreg = E_HWREG_DDI1;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");

		//DDNR bank
		u8IdxDts = E_DTS_DDNR;
		u8IdxHwreg = E_HWREG_DDNR;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");
	}

	if (dev->dipcap_dev.u32USCL == 1) {
		//HVSP bank
		u8IdxDts = E_DTS_HVSP;
		u8IdxHwreg = E_HWREG_HVSP;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");
	}

	if (dev->dipcap_dev.u32B2R == 1) {
		//B2R+UFO clk
		ret |= b2r_clk_get(pdev, dev);
		DIP_CHECK_RETURN(ret, "b2r_clk_get");

		//UFO bank
		u8IdxDts = E_DTS_UFO;
		u8IdxHwreg = E_HWREG_UFO;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");

		//B2R bank
		u8IdxDts = E_DTS_B2R;
		u8IdxHwreg = E_HWREG_B2R;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");
	}

	if (dev->dipcap_dev.u32HDR == 1) {
		//XVYCC bank
		u8IdxDts = E_DTS_XVYCC;
		u8IdxHwreg = E_HWREG_XVYCC;
		ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
		DIP_CHECK_RETURN(ret, "_SetIoBase");
	}

	//AID bank
	u8IdxDts = E_DTS_AID;
	u8IdxHwreg = E_HWREG_AID;
	ret |= _SetIoBase(pdev, u8IdxDts, u8IdxHwreg);
	DIP_CHECK_RETURN(ret, "_SetIoBase");

	return ret;
}

static void dip_set_UdmaLevel(struct dip_dev *dev)
{
	struct ST_UDMA_LV stUdmaLv = {0};

	stUdmaLv.u8PreultraMask = 0;
	stUdmaLv.u8UltraMask = 0;
	stUdmaLv.u8UrgentMask = 0;

	if (dev->variant->u16DIPIdx == DIP0_WIN_IDX) {
		stUdmaLv.u8PreultraLv = DIPW0_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DIPW0_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DIPW0_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DIPW0);
	} else if (dev->variant->u16DIPIdx == DIP1_WIN_IDX) {
		stUdmaLv.u8PreultraLv = DIPW1_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DIPW1_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DIPW1_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DIPW1);
	} else if (dev->variant->u16DIPIdx == DIP2_WIN_IDX) {
		stUdmaLv.u8PreultraLv = DIPW2_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DIPW2_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DIPW2_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DIPW2);
	}

	if (dev->dipcap_dev.u32DIPR) {
		stUdmaLv.u8PreultraLv = DIPR_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DIPR_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DIPR_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DIPR);
	}

	if (dev->dipcap_dev.u323DDI) {
		stUdmaLv.u8PreultraLv = DDIW_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DDIW_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DDIW_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DDIW);
		stUdmaLv.u8PreultraLv = DDIR_PREULTRA_VAL;
		stUdmaLv.u8UltraLv = DDIR_ULTRA_VAL;
		stUdmaLv.u8UrgentLv = DDIR_URGENT_VAL;
		KDrv_DIP_SetUDMALevel(&stUdmaLv, E_UDMA_DDIR);
	}
}

int dip_get_efuse(struct dip_dev *dev)
{
	MS_BOOL Ret = FALSE;
	unsigned char GetVal = 0;

	Ret = KDrv_DIP_EFUSE_GetDIDPCI(&GetVal);
	if (!Ret) {
		v4l2_err(&dev->v4l2_dev, "[%s]call KDrv_DIP_EFUSE_GetDIDPCI Fail\n",
			__func__);
		dev->efuse_develop_mode = 0;
	} else {
		if (GetVal == EFUSE_DIP_PCI_TRUE)
			dev->efuse_develop_mode = 0;
		else
			dev->efuse_develop_mode = 1;
	}

	Ret = KDrv_DIP_EFUSE_GetDIDDSC(&GetVal);
	if (!Ret) {
		v4l2_err(&dev->v4l2_dev, "[%s]call KDrv_DIP_EFUSE_GetDIDDSC Fail\n",
			__func__);
		dev->SDCSupport = 0;
	} else {
		if ((GetVal == EFUSE_DIP_SDC_SUPPORT) && (dev->dipcap_dev.u32SDCCap))
			dev->SDCSupport = 1;
		else
			dev->SDCSupport = 0;
	}

	return 0;
}

void dip_create_sysfs(struct dip_dev *dip_dev, struct platform_device *pdev)
{
	int i = 0;

	if (dip_dev->variant->u16DIPIdx == DIP0_WIN_IDX) {
		for (i = 0; i < (sizeof(dip_cap_attr0) / sizeof(struct device_attribute)); i++) {
			if (device_create_file(dip_dev->dev, &dip_cap_attr0[i]) != 0)
				dev_err(&pdev->dev, "Device_create_file_error\n");
		}
	} else if (dip_dev->variant->u16DIPIdx == DIP1_WIN_IDX) {
		for (i = 0; i < (sizeof(dip_cap_attr1) / sizeof(struct device_attribute)); i++) {
			if (device_create_file(dip_dev->dev, &dip_cap_attr1[i]) != 0)
				dev_err(&pdev->dev, "Device_create_file_error\n");
		}
	} else if (dip_dev->variant->u16DIPIdx == DIP2_WIN_IDX) {
		for (i = 0; i < (sizeof(dip_cap_attr2) / sizeof(struct device_attribute)); i++) {
			if (device_create_file(dip_dev->dev, &dip_cap_attr2[i]) != 0)
				dev_err(&pdev->dev, "Device_create_file_error\n");
		}
	} else if (dip_dev->variant->u16DIPIdx == DIPR_BLEND_WIN_IDX) {
		for (i = 0; i < (sizeof(dipr_blend_cap_attr2) /
			sizeof(struct device_attribute)); i++) {
			if (device_create_file(dip_dev->dev, &dipr_blend_cap_attr2[i]) != 0)
				dev_err(&pdev->dev, "Device_create_file_error\n");
		}
	} else {
		dev_err(&pdev->dev, "Failed get capability\r\n");
	}
}

void dip_remove_sysfs(struct dip_dev *dip_dev, struct platform_device *pdev)
{
	u8 u8Idx = 0;
	u8 u8loopSz = 0;

	dev_info(&pdev->dev, "remove DIP:%d sysfs\n", dip_dev->variant->u16DIPIdx);

	if (dip_dev->variant->u16DIPIdx == DIP0_WIN_IDX) {
		u8loopSz = sizeof(dip_cap_attr0)/sizeof(struct device_attribute);
		for (u8Idx = 0; u8Idx < u8loopSz; u8Idx++)
			device_remove_file(dip_dev->dev, &dip_cap_attr0[u8Idx]);
	} else if (dip_dev->variant->u16DIPIdx == DIP1_WIN_IDX) {
		u8loopSz = sizeof(dip_cap_attr1)/sizeof(struct device_attribute);
		for (u8Idx = 0; u8Idx < u8loopSz; u8Idx++)
			device_remove_file(dip_dev->dev, &dip_cap_attr1[u8Idx]);
	} else if (dip_dev->variant->u16DIPIdx == DIP2_WIN_IDX) {
		u8loopSz = sizeof(dip_cap_attr2)/sizeof(struct device_attribute);
		for (u8Idx = 0; u8Idx < u8loopSz; u8Idx++)
			device_remove_file(dip_dev->dev, &dip_cap_attr2[u8Idx]);
	} else {
		dev_err(&pdev->dev, "invalid dip number:%d\n", dip_dev->variant->u16DIPIdx);
	}
}

static int dip_open(struct file *file)
{
	struct dip_dev *dev = NULL;
	struct dip_ctx *ctx = NULL;
	int ret = 0;

	if (file == NULL) {
		pr_err("%s, file is NULL\n", __func__);
		return -ENOENT;
	}
	dev = video_drvdata(file);
	if (dev == NULL) {
		pr_err("%s, dev is NULL\n", __func__);
		return -ENOENT;
	}

	DIP_DBG_API("%s, instance opened\n", __func__);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	/* Set default DIPFormats */
	ctx->in = def_frame;
	ctx->out = def_frame;

	ctx->u8STR = 0;
	ctx->u8SecureSTR = 0;

	if (mutex_lock_interruptible(&dev->mutex)) {
		kfree(ctx);
		return -ERESTARTSYS;
	}
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev, ctx, &queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		mutex_unlock(&dev->mutex);
		kfree(ctx);
		return ret;
	}
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	dev->ctx = ctx;

	ctx->u32SrcBufMode = E_BUF_PA_MODE;
	ctx->u32DstBufMode = E_BUF_PA_MODE;

	mutex_unlock(&dev->mutex);

	return 0;
}

static int dip_release(struct file *file)
{
	struct dip_dev *dev = video_drvdata(file);
	struct dip_ctx *ctx = fh2ctx(file->private_data);
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;
	int ret = 0;

	DIP_DBG_API("%s, instance closed\n", __func__);

	ret = _FindMappingDIP(dev, &enDIPIdx);
	if (ret)
		return ret;

	mutex_lock(&dev->mutex);

	gst1RNWInfo.u32WriteFlag &= ~(1 << enDIPIdx);
	gst1RNWInfo.u32Fired &= ~(1 << enDIPIdx);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	dev->ctx = NULL;
	kfree(ctx);
	mutex_unlock(&dev->mutex);

	return 0;
}

static long dip_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct dip_dev *dev = video_drvdata(file);
	struct timespec64 Time;
	long ret = 0;
	u64 start = 0, end = 0;

	if ((cmd == VIDIOC_QBUF) || (cmd == VIDIOC_DQBUF)) {
		ktime_get_real_ts64(&Time);
		start = (Time.tv_sec*S_TO_US) + (Time.tv_nsec/NS_TO_US);
	}
	if (dev->ctx != NULL) {
		if (dev->ctx->u8STR != 0) {
			v4l2_err(&dev->v4l2_dev, "DIP cant work during STR\n");
			return -EPERM;
		}

		if (dev->ctx->u8SecureSTR != 0) {
			v4l2_err(&dev->v4l2_dev, "DIP STR when Secure, please reopen device\n");
			return -EPERM;
		}
	}

	ret = video_ioctl2(file, cmd, arg);

	if ((cmd == VIDIOC_QBUF) || (cmd == VIDIOC_DQBUF)) {
		ktime_get_real_ts64(&Time);
		end = (Time.tv_sec*S_TO_US) + (Time.tv_nsec/NS_TO_US);
		DIP_DBG_IOCTLTIME("%s, Cmd:0x%X, time:%llu us\n", __func__, cmd, end-start);
	}

	return ret;
}

static const struct v4l2_file_operations dip_fops = {
	.owner      = THIS_MODULE,
	.open       = dip_open,
	.release    = dip_release,
	.poll       = v4l2_m2m_fop_poll,
	.unlocked_ioctl = dip_ioctl,
	.mmap       = v4l2_m2m_fop_mmap,
};

static const struct v4l2_ioctl_ops dip_ioctl_ops = {
	.vidioc_querycap    = dip_ioctl_querycap,

	.vidioc_s_input     = dip_ioctl_s_input,

	.vidioc_enum_fmt_vid_cap    = dip_ioctl_enum_cap_mp_fmt,
	.vidioc_enum_fmt_vid_out    = dip_ioctl_enum_out_mp_fmt,

	.vidioc_g_fmt_vid_cap_mplane = dip_ioctl_g_fmt_mp,
	.vidioc_g_fmt_vid_out_mplane = dip_ioctl_g_fmt_mp,

	.vidioc_s_fmt_vid_cap       = dip_ioctl_s_fmt_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = dip_ioctl_s_fmt_cap_mplane,
	.vidioc_s_fmt_vid_out       = dip_ioctl_s_fmt_out_mplane,
	.vidioc_s_fmt_vid_out_mplane = dip_ioctl_s_fmt_out_mplane,

	.vidioc_reqbufs         = dip_ioctl_reqbufs,
	.vidioc_querybuf        = dip_ioctl_querybuf,
	.vidioc_qbuf            = dip_ioctl_qbuf,
	.vidioc_dqbuf           = dip_ioctl_dqbuf,
	.vidioc_create_bufs     = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf     = v4l2_m2m_ioctl_prepare_buf,

	.vidioc_streamon        = dip_ioctl_streamon,
	.vidioc_streamoff       = dip_ioctl_streamoff,
	.vidioc_s_parm          = dip_ioctl_s_parm,

	.vidioc_s_ctrl          = dip_ioctl_s_ctrl,
	.vidioc_s_ext_ctrls     = dip_ioctl_s_ext_ctrls,
	.vidioc_g_ext_ctrls     = dip_ioctl_g_ext_ctrls,

	.vidioc_g_selection     = dip_ioctl_g_selection,
	.vidioc_s_selection     = dip_ioctl_s_selection,
};

static struct video_device dip_videodev = {
	.name       = DIP_NAME,
	.fops       = &dip_fops,
	.ioctl_ops  = &dip_ioctl_ops,
	.minor      = -1,
	.release    = video_device_release,
	.vfl_dir    = VFL_DIR_M2M,
};

static struct v4l2_m2m_ops dip_m2m_ops = {
	.device_run = dip_device_run,
	.job_abort  = dip_job_abort,
};

static const struct of_device_id mtk_dip_match[];

static int dip_probe(struct platform_device *pdev)
{
	struct dip_dev *dev;
	struct video_device *vfd;
	const struct of_device_id *of_id;
	int ret = 0;
	int loopIndex = 0;
	unsigned int irq_no;
	int nr = 0;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->ctrl_lock);
	mutex_init(&dev->mutex);
	atomic_set(&dev->num_inst, 0);
	init_waitqueue_head(&dev->irq_queue);

	/* get the interrupt */
	irq_no = platform_get_irq(pdev, 0);
	if (irq_no < 0) {
		dev_err(&pdev->dev, "no irq defined\n");
		return -EINVAL;
	}
	dev->irq = irq_no;

	of_id = of_match_node(mtk_dip_match, pdev->dev.of_node);
	if (!of_id) {
		ret = -ENODEV;
		return ret;
	}
	dev->ctx = NULL;
	dev->variant = (struct dip_variant *)of_id->data;

	ret = dip_parse_cap_dts(dev, pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get capability\n");
		return ret;
	}

	if (dev->dipcap_dev.u323DDI) {
		ret = dip_parse_iommu_dts(dev, pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to get iommu tag\n");
			return ret;
		}
	}

	if (dev->variant->u16DIPIdx != DIPR_BLEND_WIN_IDX) {
		//get clcok
		dev->scip_dip_clk = devm_clk_get(&pdev->dev, "scip_dip");
		if (IS_ERR(dev->scip_dip_clk)) {
			dev_err(&pdev->dev, "failed to get scip_dip clock\n");
			return -ENODEV;
		}

		//prepare clk
		ret = clk_prepare(dev->scip_dip_clk);
		if (ret) {
			dev_err(&pdev->dev, "failed to prepare_enable scip_dip_clk\n");
			goto put_scip_dip_clk;
		}
	}

	ret = dip_set_ioBase(dev, pdev);
	if (ret)
		goto unprep_scip_dip_clk;

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		goto unprep_scip_dip_clk;
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto unreg_v4l2_dev;
	}
	*vfd = dip_videodev;
	vfd->lock = &dev->mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	dev->dev = &pdev->dev;
	if (dev->variant->u16DIPIdx != DIPR_BLEND_WIN_IDX)
		vfd->device_caps = DEVICE_CAPS;
	else
		vfd->device_caps = DEVICE_DIPR_BLEND_CAPS;

	if (dev->dipcap_dev.u32MinorNo == FRAMEWK_ASSIGN_NO)
		nr = (-1);
	else
		nr = dev->dipcap_dev.u32MinorNo;
	ret = video_register_device(vfd, VFL_TYPE_GRABBER, nr);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		goto rel_vdev;
	}
	video_set_drvdata(vfd, dev);
	snprintf(vfd->name, sizeof(vfd->name), "%s", dip_videodev.name);
	dev->vfd = vfd;
	v4l2_info(&dev->v4l2_dev, "device registered as /dev/video%d\n", vfd->num);
	platform_set_drvdata(pdev, dev);
	dev->m2m_dev = v4l2_m2m_init(&dip_m2m_ops);
	if (IS_ERR(dev->m2m_dev)) {
		v4l2_err(&dev->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(dev->m2m_dev);
		goto unreg_video_dev;
	}
	for (loopIndex = 0; loopIndex < MAX_PLANE_NUM; loopIndex++)
		def_frame.u32BytesPerLine[loopIndex] = 0;

	dip_create_sysfs(dev, pdev);

	dip_set_UdmaLevel(dev);

	dip_get_efuse(dev);

	dev->sys_develop_mode = dev->efuse_develop_mode;
	dip_get_capability(dev, pdev);

	KDrv_DIP_SetSrcHblanking(DIP_PQ_HBLANKING, E_DIP_SRC_FROM_PQ_START);

	return 0;

unreg_video_dev:
	video_unregister_device(dev->vfd);
rel_vdev:
	video_device_release(vfd);
unreg_v4l2_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
unprep_scip_dip_clk:
	clk_unprepare(dev->scip_dip_clk);
put_scip_dip_clk:
	devm_clk_put(dev->dev, dev->scip_dip_clk);

	return ret;
}

static int dip_remove(struct platform_device *pdev)
{
	struct dip_dev *dev = NULL;

	if (pdev == NULL) {
		DIP_ERR("%s, pdev is NULL\n", __func__);
		return -ENODEV;
	}

	dev = platform_get_drvdata(pdev);
	if (dev == NULL) {
		dev_err(&pdev->dev, "%s, dev is NULL\n", __func__);
		return -ENODATA;
	}

	v4l2_info(&dev->v4l2_dev, "Removing " DIP_NAME);
	scip_clk_put(pdev, dev);
	if (dev->dipcap_dev.u32B2R == 1)
		b2r_clk_put(pdev, dev);
	v4l2_m2m_release(dev->m2m_dev);
	video_unregister_device(dev->vfd);
	v4l2_device_unregister(&dev->v4l2_dev);
	dip_remove_sysfs(dev, pdev);

	return 0;
}

static int dip_suspend(
	struct platform_device *pdev,
	pm_message_t state)
{
	struct dip_dev *dev = NULL;
	struct dip_ctx *ctx = NULL;
	EN_DIP_IDX enDIPIdx = E_DIP_IDX0;

	if (!pdev) {
		pr_crit("[%s]pdev is NULL\n", __func__);
		return 0;
	}
	dev = platform_get_drvdata(pdev);
	if (dev == NULL) {
		pr_crit("[%s]drvdata is NULL\n", __func__);
		return 0;
	}

	if (dev->ctx != NULL) {
		ctx = dev->ctx;
		ctx->u8STR = 1;

		if (_FindMappingDIP(dev, &enDIPIdx))
			return 0;

		KDrv_DIP_Ena(false, enDIPIdx);
		if ((ctx->enSource == E_DIP_SOURCE_DIPR) &&
			(_IsB2rPath(ctx, ctx->in.fmt->fourcc) == true)) {
			if (dev->dipcap_dev.u32B2R)
				KDrv_DIP_B2R_Ena(false);
		}
	}

	return 0;
}

static int dip_resume(struct platform_device *pdev)
{
	struct dip_dev *dev = NULL;
	struct dip_ctx *ctx = NULL;

	if (!pdev) {
		pr_crit("[%s]pdev is NULL\n", __func__);
		return 0;
	}
	dev = platform_get_drvdata(pdev);
	if (dev == NULL) {
		pr_crit("[%s]drvdata is NULL\n", __func__);
		return 0;
	}

	ctx = dev->ctx;
	if (ctx != NULL) {
		ctx->u8STR = 0;
		if (ctx->u8WorkingAid != E_AID_NS)
			ctx->u8SecureSTR = 1;
	}

	return 0;
}

/* the First DIP HW Info */
static struct dip_variant mtk_drvdata_dip0 = {
	.u16DIPIdx = DIP0_WIN_IDX,
};

/* the Second DIP HW Info  */
static struct dip_variant mtk_drvdata_dip1 = {
	.u16DIPIdx = DIP1_WIN_IDX,
};

/* the 3rd DIP HW Info  */
static struct dip_variant mtk_drvdata_dip2 = {
	.u16DIPIdx = DIP2_WIN_IDX,
};

/* the 3rd DIPR blending  */
static struct dip_variant mtk_drvdata_dipr_blend = {
	.u16DIPIdx = DIPR_BLEND_WIN_IDX,
};

static const struct of_device_id mtk_dip_match[] = {
	{
		.compatible = "MTK,DIP0",
		.data = &mtk_drvdata_dip0,
	}, {
		.compatible = "MTK,DIP1",
		.data = &mtk_drvdata_dip1,
	}, {
		.compatible = "mediatek,mt5896-dip0",
		.data = &mtk_drvdata_dip0,
	}, {
		.compatible = "mediatek,mt5896-dip1",
		.data = &mtk_drvdata_dip1,
	}, {
		.compatible = "mediatek,mt5896-dip2",
		.data = &mtk_drvdata_dip2,
	}, {
		.compatible = "mediatek,mtk-dtv-dip0",
		.data = &mtk_drvdata_dip0,
	}, {
		.compatible = "mediatek,mtk-dtv-dip1",
		.data = &mtk_drvdata_dip1,
	}, {
		.compatible = "mediatek,mtk-dtv-dip2",
		.data = &mtk_drvdata_dip2,
	}, {
		.compatible = "mediatek,mtk-dtv-dipr-blend",
		.data = &mtk_drvdata_dipr_blend,
	},
	{},
};

static struct platform_driver dip_pdrv = {
	.probe      = dip_probe,
	.remove     = dip_remove,
	.suspend    = dip_suspend,
	.resume     = dip_resume,
	.driver     = {
		.name = DIP_NAME,
		.of_match_table = mtk_dip_match,
	},
};
module_platform_driver(dip_pdrv);

MODULE_DEVICE_TABLE(of, mtk_dip_match);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK Video Capture Driver");
