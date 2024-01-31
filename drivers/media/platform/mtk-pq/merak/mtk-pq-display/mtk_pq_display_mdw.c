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
#include "mtk_iommu_dtv_api.h"
#include "metadata_utility.h"
#include "m_pqu_pq.h"
#include "pqu_msg.h"

//-----------------------------------------------------------------------------
//  Driver Compiler Options
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define POW2_ALIGN(val, align) (((val) + ((align) - 1)) & (~((align) - 1)))
#define FAKE_BUF_WIDTH 1
#define FAKE_BUF_HEIGHT 1
#define MV_BUF_INDEX 0
#define FRC_SHM_INDEX_V4     (0)
#define FRC_SHM_INDEX        (1)

//-----------------------------------------------------------------------------
//  Local Structures
//-----------------------------------------------------------------------------
struct format_info {
	u32 v4l2_fmt;
	u32 meta_fmt;
	u8 plane_num;
	u8 buffer_num;
	u8 bpp[MAX_FPLANE_NUM];
	enum pqu_pq_output_path output_path;
};

struct vgsync_buf {
	uint64_t u64VgsyncBufPhyAddr;
	uint64_t u64VgsyncBufVirAddr;
	u32 u32VgsyncBufSize;
};

struct frc_shm_buf {
	uint64_t u64FrcShmBufPhyAddr;
	uint64_t u64FrcShmBufVirAddr;
	u32 u32FrcShmBufSize;
};

//-----------------------------------------------------------------------------
//  Global Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
struct vgsync_buf VgsyncBufInfo = {0, 0, 0};
struct frc_shm_buf FrcShmBufInfo = {0, 0, 0};

//-----------------------------------------------------------------------------
//  Debug Functions
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------
#define FMT_RGB565_BPP0 16
#define FMT_RGB444_6B_BPP0 18
#define FMT_RGB444_8B_BPP0 24
#define FMT_YUV444_6B_BPP0 18
#define FMT_YUV444_8B_BPP0 24
#define FMT_YUV422_6B_BPP0 12
#define FMT_YUV422_8B_BPP0 16
#define FMT_YUV420_6B_BPP0 9
#define FMT_YUV420_8B_BPP0 12
#define BPP_1ST 0
#define BPP_2ND 1
#define BPP_3RD 2
static int _get_fmt_info(u32 pixfmt_v4l2, struct format_info *fmt_info)
{
	if (WARN_ON(!fmt_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, fmt_info=0x%llx\n", (uint64_t)fmt_info);
		return -EINVAL;
	}

	switch (pixfmt_v4l2) {
	/* ------------------ RGB format ------------------ */
	case V4L2_PIX_FMT_RGB565:
		fmt_info->meta_fmt = MEM_FMT_RGB565;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_RGB565_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_ARGB8888:
		fmt_info->meta_fmt = MEM_FMT_ARGB8888;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_ABGR8888:
		fmt_info->meta_fmt = MEM_FMT_ABGR8888;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_RGBA8888:
		fmt_info->meta_fmt = MEM_FMT_RGBA8888;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_BGRA8888:
		fmt_info->meta_fmt = MEM_FMT_BGRA8888;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_ARGB2101010:
		fmt_info->meta_fmt = MEM_FMT_ARGB2101010;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_ABGR2101010:
		fmt_info->meta_fmt = MEM_FMT_ABGR2101010;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		fmt_info->meta_fmt = MEM_FMT_RGBA1010102;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		fmt_info->meta_fmt = MEM_FMT_BGRA1010102;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 32;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	/* ------------------ YUV format ------------------ */
	/* 1 plane */
	case V4L2_PIX_FMT_YUV444_VYU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_VYU_10B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 30;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV444_VYU_8B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_VYU_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 24;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YVYU:
		fmt_info->meta_fmt = MEM_FMT_YUV422_YVYU_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = 16;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	/* 2 planes */
	case V4L2_PIX_FMT_YUV422_Y_VU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_10B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 10;
		fmt_info->bpp[1] = 10;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_NV61:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 8;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV422_Y_VU_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_8B_CE;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 8;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV422_Y_VU_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV422_Y_VU_6B_CE;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 6;
		fmt_info->bpp[1] = 6;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_10B:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_10B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 10;
		fmt_info->bpp[1] = 5;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_NV12:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_UV_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 4;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_NV21:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 4;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_8B_CE;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 4;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	case V4L2_PIX_FMT_YUV420_Y_VU_6B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV420_Y_VU_6B_CE;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 2;
		fmt_info->bpp[0] = 6;
		fmt_info->bpp[1] = 3;
		fmt_info->bpp[2] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;
	/* 3 planes */
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE:
		fmt_info->meta_fmt = MEM_FMT_YUV444_Y_U_V_8B_CE;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 3;
		fmt_info->bpp[0] = 8;
		fmt_info->bpp[1] = 8;
		fmt_info->bpp[2] = 8;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_MDW;
		break;

	case V4L2_PIX_FMT_RGB444_6B:
		fmt_info->meta_fmt = MEM_FMT_RGB444_6B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_RGB444_6B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_RGB444_8B:
		fmt_info->meta_fmt = MEM_FMT_RGB444_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_RGB444_8B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV444_YUV_6B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_YUV_6B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV444_6B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV444_YUV_8B:
		fmt_info->meta_fmt = MEM_FMT_YUV444_YUV_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV444_8B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_6B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_YUV_6B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV422_6B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_8B:
		fmt_info->meta_fmt = MEM_FMT_YUV422_YUV_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV422_8B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV420_YUV_6B:
		fmt_info->meta_fmt = MEM_FMT_YUV420_YUV_6B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV420_6B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
		break;
	case V4L2_PIX_FMT_YUV420_YUV_8B:
		fmt_info->meta_fmt = MEM_FMT_YUV420_YUV_8B;
		fmt_info->buffer_num = 1;
		fmt_info->plane_num = 1;
		fmt_info->bpp[0] = FMT_YUV420_8B_BPP0;
		fmt_info->bpp[1] = 0;
		fmt_info->bpp[BPP_3RD] = 0;
		fmt_info->output_path = PQ_OUTPUT_PATH_WITH_BUF_FRC;
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

//-----------------------------------------------------------------------------
//  Global Functions
//-----------------------------------------------------------------------------
int mtk_pq_display_mdw_read_dts(
	struct device_node *display_node,
	struct pq_display_mdw_device *mdw_dev)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;
	struct device_node *mdw_node;

	if (display_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "ptr np is NULL\n");
		return -EINVAL;
	}

	mdw_node = of_find_node_by_name(display_node, "mdw");

	// read mdw v align
	name = "mdw_v_align";
	ret = of_property_read_u32(mdw_node, name, &tmp_u32);
	if (ret != 0x0)
		goto Fail;
	mdw_dev->v_align = (u8)tmp_u32;

	// read mdw h align
	name = "mdw_h_align";
	ret = of_property_read_u32(mdw_node, name, &tmp_u32);
	if (ret != 0x0)
		goto Fail;
	mdw_dev->h_align = (u8)tmp_u32;

	return 0;

Fail:
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
	return ret;
}

#if (1)//(MEMC_CONFIG == 1)
#define IP_FRC_VER1    1
#define IP_FRC_VER1_E3 3

#define _8K_H 7680
#define _8K_V 4320
#define _8K_H_ALIGN 1024
#define FRAMES_CNT 10
#define MEDS_H 4096
#define MEDS_V 2160
#define MEDS_BPP 6
#define _8K_4KMI_H 4096
#define _8K_4KMI_V 2160


#define IP_FRC_VER2 2
#define IP_FRC_VER4 4
#define IP_FRC_VER5 5
#define IP_FRC_VER6 6
#define _4K_H 3840
#define _4K_V 2160
#define _4K_H_ALIGN 1024
#define MEDS_H_4K 1920
#define MEDS_V_4K 1080
#define MEDS_BPP_4K 6
#define MV_BUFFER_SIZE 0x19999A
#define _4K_4KMI_H 0
#define _4K_4KMI_V 0
#define INVALID_BUF_INDEX 0xFF
#define IOMMU_IDX_FRC_PQ     (7)
#define IOMMU_OFFSET_FRC_PQ  (24)
#define INVALID_IOMMU_ADDR   (0)


#define MTK_DRM_TV_FRC_COMPATIBLE_STR "mediatek,mediatek-frc"

static int get_memory_info(u32 *addr)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;

	// find memory_info node in dts
	target_memory_np = of_find_node_by_name(NULL, "memory_info");
	if (!target_memory_np)
		return -ENODEV;

	// query cpu_emi0_base value from memory_info node
	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);
	if (p != NULL) {
		*addr = be32_to_cpup(p);
		of_node_put(target_memory_np);
		p = NULL;
	} else {
		pr_err("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}

int _mtk_pq_display_frc_read_dts_frc_shm_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	struct of_mmap_info_data of_mmap_info;
	u32 CPU_base_adr = 0;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	ret = get_memory_info(&CPU_base_adr);
	if (ret)
		ret = -EINVAL;

	if ((frc_dev->frc_ip_version == IP_FRC_VER4) ||
	(frc_dev->frc_ip_version == IP_FRC_VER5) ||
	(frc_dev->frc_ip_version == IP_FRC_VER6)) {
		ret = of_mtk_get_reserved_memory_info_by_idx(frc_node, FRC_SHM_INDEX_V4,
			&of_mmap_info);
	} else {
		ret = of_mtk_get_reserved_memory_info_by_idx(frc_node, FRC_SHM_INDEX,
			&of_mmap_info);
	}

	if (ret != 0x0) {
		FrcShmBufInfo.u64FrcShmBufPhyAddr = 0;
		FrcShmBufInfo.u64FrcShmBufVirAddr = 0;
		FrcShmBufInfo.u32FrcShmBufSize = 0;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Get frc_shm_adr failed, ret = %d.\n", ret);
		return ret;
	}

	FrcShmBufInfo.u64FrcShmBufPhyAddr =
		(uint64_t)of_mmap_info.start_bus_address;
	FrcShmBufInfo.u32FrcShmBufSize =
		(uint32_t)of_mmap_info.buffer_size;

	//Allocate memory, only support non-cache.
	FrcShmBufInfo.u64FrcShmBufVirAddr =
		(unsigned long)ioremap_wc(
			of_mmap_info.start_bus_address, of_mmap_info.buffer_size);

	if (FrcShmBufInfo.u64FrcShmBufVirAddr == 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nmtk mmap failed to frc shm vaddr\n");
		FrcShmBufInfo.u64FrcShmBufPhyAddr = 0;
		FrcShmBufInfo.u64FrcShmBufVirAddr = 0;
		FrcShmBufInfo.u32FrcShmBufSize = 0;
		return -ENXIO;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_shm_bufaddr=%llx, size=%zu\n",
	FrcShmBufInfo.u64FrcShmBufPhyAddr, (size_t)FrcShmBufInfo.u32FrcShmBufSize);

	return 0;
}

int _mtk_pq_display_frc_read_dts_frc_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	name = "FRC_IP_VERSION";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_ip_version = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_ip_version=%d\n", frc_dev->frc_ip_version);

	// read frc width
	name = "frc_width";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_width = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_width=%d\n", frc_dev->frc_width);

	// read frc height
	name = "frc_height";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_height = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_height=%d\n", frc_dev->frc_height);

	// read frc h align
	name = "frc_h_align";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_h_align = (uint16_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_h_align=%d\n", frc_dev->frc_h_align);

	// read frc frame cnt
	name = "frc_frame_cnt";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_frame_cnt = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_frame_cnt=%d\n", frc_dev->frc_frame_cnt);

	name = "frc_max_frame_cnt";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_max_frame_cnt = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_max_frame_cnt=%d\n", frc_dev->frc_max_frame_cnt);

	return 0;
}

int _mtk_pq_display_frc_read_dts_meds_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	name = "frc_meds_width";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_meds_width = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_meds_width=%d\n", frc_dev->frc_meds_width);

	/* read frc meds height */
	name = "frc_meds_height";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_meds_height = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_meds_height=%d\n", frc_dev->frc_meds_height);

	name = "frc_meds_bpp";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_meds_bpp = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_meds_bpp=%d\n", frc_dev->frc_meds_bpp);

	return 0;
}

int _mtk_pq_display_frc_read_dts_iommu_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	name = "frc_iommu_idx_frc_pq";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_iommu_idx_frc_pq = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nFRC_IOMMU_IDX_FRC_PQ=%d\n",
		frc_dev->frc_iommu_idx_frc_pq);

	name = "frc_buf_iommu_offset";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_buf_iommu_offset = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nFRC_BUF_IOMMU_OFFSET=%d\n",
		frc_dev->frc_buf_iommu_offset);

	name = "frc_iommu_frc_pq_size";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_iommu_frc_pq_size = tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nFRC_IOMMU_FRC_PQ_SIZE=0x%x\n",
		frc_dev->frc_iommu_frc_pq_size);

	return 0;
}

int _mtk_pq_display_frc_read_dts_frc_mvbuff_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;
	struct of_mmap_info_data of_mmap_info;
	u32 CPU_base_adr = 0;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	ret = get_memory_info(&CPU_base_adr);
	if (ret)
		ret = -EINVAL;

	if ((frc_dev->frc_ip_version == IP_FRC_VER1) ||
	(frc_dev->frc_ip_version == IP_FRC_VER1_E3)) {
		name = "frc mvbff address";
		ret = of_mtk_get_reserved_memory_info_by_idx(frc_node, MV_BUF_INDEX, &of_mmap_info);
		if (ret != 0x0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
			return ret;
		}
		frc_dev->frc_mvbff_adr = (uint64_t)of_mmap_info.start_bus_address -
									(uint64_t)CPU_base_adr;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_mvbff_adr=%llx\n",
		frc_dev->frc_mvbff_adr);
	}

	name = "frc_mvbuff_size";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_mvbff_size = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_mvbuff_size=0x%x\n", frc_dev->frc_mvbff_size);

	return 0;
}

int _mtk_pq_display_frc_read_dts_4kmi_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	name = "frc_4kmi_width";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_4kmi_width = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_4kmi_width=%d\n", frc_dev->frc_4kmi_width);

	name = "frc_4kmi_height";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_4kmi_height = (uint32_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_4kmi_height=%d\n", frc_dev->frc_4kmi_height);

	return 0;
}

int _mtk_pq_display_frc_read_dts_VGsync_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name;
	uint8_t u8frc_vgsync_buf_index = 0;
	struct of_mmap_info_data of_mmap_info;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	if (frc_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_node is NULL\n");
		return -EINVAL;
	}

	name = "frc_vgsync_buf_index";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	u8frc_vgsync_buf_index = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_vgsync_buf_index=%d\n", u8frc_vgsync_buf_index);

	if (u8frc_vgsync_buf_index != INVALID_BUF_INDEX) {
		name = "frc_vgsync_bufaddr";
		ret = of_mtk_get_reserved_memory_info_by_idx(frc_node,
			u8frc_vgsync_buf_index, &of_mmap_info);
		if (ret != 0x0) {
			VgsyncBufInfo.u64VgsyncBufPhyAddr = 0;
			VgsyncBufInfo.u64VgsyncBufVirAddr = 0;
			VgsyncBufInfo.u32VgsyncBufSize = 0;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
			return ret;
		}
		VgsyncBufInfo.u64VgsyncBufPhyAddr =
			(uint64_t)of_mmap_info.start_bus_address;
		VgsyncBufInfo.u32VgsyncBufSize =
			(uint32_t)of_mmap_info.buffer_size;

		//Allocate memory, only support non-cache.
		VgsyncBufInfo.u64VgsyncBufVirAddr =
			(unsigned long)ioremap_wc(
				of_mmap_info.start_bus_address, of_mmap_info.buffer_size);
		if (VgsyncBufInfo.u64VgsyncBufVirAddr == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nmtk mmap failed to frc shm vaddr\n");
			VgsyncBufInfo.u64VgsyncBufPhyAddr = 0;
			VgsyncBufInfo.u64VgsyncBufVirAddr = 0;
			VgsyncBufInfo.u32VgsyncBufSize = 0;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
			return -ENXIO;
		}

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nfrc_vgsync_bufaddr=%llx, size=%zu\n",
		VgsyncBufInfo.u64VgsyncBufPhyAddr, (size_t)VgsyncBufInfo.u32VgsyncBufSize);
	}

	return 0;
}

int _mtk_pq_display_frc_read_dts_photo_info(
	struct pq_display_frc_device *frc_dev, struct device_node *frc_node)
{
	int ret = 0;
	u32 tmp_u32 = 0;
	const char *name = NULL;

	if ((frc_dev == NULL) || (frc_node == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pointer is NULL\n");
		return -EINVAL;
	}

	name = "frc_photo_enable";
	ret = of_property_read_u32(frc_node, name, &tmp_u32);
	if (ret != 0x0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
		return ret;
	}
	frc_dev->frc_photo_enable = (uint8_t)tmp_u32;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nFRC_PHOTO_ENABLE=%d\n",
		frc_dev->frc_photo_enable);

	if (frc_dev->frc_photo_enable == true) {
		name = "frc_photo_frame_cnt";
		ret = of_property_read_u32(frc_node, name, &tmp_u32);
		if (ret != 0x0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "read DTS failed, name = %s\n", name);
			return ret;
		}
		frc_dev->frc_photo_frame_cnt = (uint8_t)tmp_u32;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "\nFRC_PHOTO_FRAME_COUNT=%d\n",
			frc_dev->frc_photo_frame_cnt);
	}

	return ret;
}

int mtk_pq_display_frc_read_dts(
	struct pq_display_frc_device *frc_dev)
{
	int ret = 0;
	struct device_node *frc_node;

	if (frc_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "frc_dev is NULL\n");
		return -EINVAL;
	}

	frc_node = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_FRC_COMPATIBLE_STR);

	/* frc info */
	ret = _mtk_pq_display_frc_read_dts_frc_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* read meds info */
	ret = _mtk_pq_display_frc_read_dts_meds_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* frc mvbuff info */
	ret = _mtk_pq_display_frc_read_dts_frc_mvbuff_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* 4K MI info */
	ret = _mtk_pq_display_frc_read_dts_4kmi_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* VGsync info */
	ret = _mtk_pq_display_frc_read_dts_VGsync_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* read iommu info */
	ret = _mtk_pq_display_frc_read_dts_iommu_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* read photo info */
	ret = _mtk_pq_display_frc_read_dts_photo_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	/* read frc shm info */
	ret = _mtk_pq_display_frc_read_dts_frc_shm_info(frc_dev, frc_node);
	if (ret != 0x0)
		goto Fail;

	return 0;

Fail:
	switch (frc_dev->frc_ip_version) {
	case IP_FRC_VER2:
	case IP_FRC_VER4:
	case IP_FRC_VER5:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"so frc read DTS hard code:3840 2160 1920 1080\n");
		frc_dev->frc_width = _4K_H;
		frc_dev->frc_height = _4K_V;
		frc_dev->frc_h_align = _4K_H_ALIGN;
		frc_dev->frc_frame_cnt = FRAMES_CNT;
		frc_dev->frc_meds_width = MEDS_H_4K;
		frc_dev->frc_meds_height = MEDS_V_4K;
		frc_dev->frc_meds_bpp = MEDS_BPP_4K;
		frc_dev->frc_mvbff_size = MV_BUFFER_SIZE;
		frc_dev->frc_4kmi_width = _4K_4KMI_H;
		frc_dev->frc_4kmi_height = _4K_4KMI_V;
		frc_dev->frc_max_frame_cnt = FRAMES_CNT;
		frc_dev->frc_iommu_idx_frc_pq = IOMMU_IDX_FRC_PQ;
		frc_dev->frc_buf_iommu_offset = IOMMU_OFFSET_FRC_PQ;
		frc_dev->frc_iommu_frc_pq_adr = INVALID_IOMMU_ADDR;
		break;
	case IP_FRC_VER1:
	case IP_FRC_VER1_E3:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"so frc read DTS hard code:7680 4320 4096 2160\n");
		frc_dev->frc_width = _8K_H;
		frc_dev->frc_height = _8K_V;
		frc_dev->frc_h_align = _8K_H_ALIGN;
		frc_dev->frc_frame_cnt = FRAMES_CNT;
		frc_dev->frc_meds_width = MEDS_H;
		frc_dev->frc_meds_height = MEDS_V;
		frc_dev->frc_meds_bpp = MEDS_BPP;
		frc_dev->frc_mvbff_size = 0;
		frc_dev->frc_4kmi_width = _8K_4KMI_H;
		frc_dev->frc_4kmi_height = _8K_4KMI_V;
		frc_dev->frc_max_frame_cnt = FRAMES_CNT;
		frc_dev->frc_iommu_idx_frc_pq = IOMMU_IDX_FRC_PQ;
		frc_dev->frc_buf_iommu_offset = IOMMU_OFFSET_FRC_PQ;
		frc_dev->frc_iommu_frc_pq_adr = INVALID_IOMMU_ADDR;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"read DTS failed, frc_ip_version = %d\n",
			frc_dev->frc_ip_version);
		return -EINVAL;
	}
	ret = 0;

	return ret;
}
#endif

int mtk_pq_display_mdw_queue_setup(struct vb2_queue *vq,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_pq_ctx *pq_ctx = vb2_get_drv_priv(vq);
	struct mtk_pq_device *pq_dev = pq_ctx->pq_dev;
	struct pq_display_mdw_info *mdw = &pq_dev->display_info.mdw;
	int i = 0;
	int x = 1;

	/* video buffers + meta buffer */
	*num_planes = mdw->buffer_num + 1;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[mdw] num_planes=%d\n", *num_planes);

	sizes[0] = 0;

	for (i = 0; i < mdw->plane_num; ++i) {
		if (mdw->buffer_num == 1) {
			/* contiguous buffer case, add all buffer size in one */
//			sizes[0] += mdw->plane_size[i];
			sizes[0] += x;
		} else {
			/* non contiguous buffer case */
//			sizes[i] = mdw->plane_size[i];
			sizes[i] = x;
		}
		alloc_devs[i] = pq_dev->dev;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[mdw] size[%d]=%d, alloc_devs[%d]=%llx\n", i, sizes[i], i,
			(uint64_t)alloc_devs[i]);
	}

	/* give meta fd min size */
	sizes[*num_planes - 1] = 1;
	alloc_devs[*num_planes - 1] = pq_dev->dev;

	return 0;
}

int _mtk_pq_display_frc_get_frame_count(struct mtk_pq_device *pq_dev,
	struct display_device *disp_dev, uint8_t *u8frc_framecount)
{
	if (WARN_ON(!pq_dev) || WARN_ON(!disp_dev) || WARN_ON(!u8frc_framecount)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, pq_dev=0x%llx, disp_dev=0x%llx, framecount:0x%llx\n",
			(uint64_t)pq_dev, (uint64_t)disp_dev, (uint64_t)u8frc_framecount);
		return -EINVAL;
	}

	if (IS_INPUT_B2R(pq_dev->common_info.input_source)) {
		if ((pq_dev->stream_meta.pq_scene == meta_pq_scene_photo) &&
			(disp_dev->frc_dev.frc_photo_enable == true))
			*u8frc_framecount = disp_dev->frc_dev.frc_photo_frame_cnt;
		else
			*u8frc_framecount = disp_dev->frc_dev.frc_frame_cnt;
	} else {
		*u8frc_framecount = disp_dev->frc_dev.frc_max_frame_cnt;
	}

	return 0;
}

int mtk_pq_display_mdw_queue_setup_info(struct mtk_pq_device *pq_dev,
	void *bufferctrl)
{
	struct v4l2_pq_g_buffer_info bufferInfo;
	struct mtk_pq_platform_device *pq_pdev = NULL;
	struct display_device *disp_dev = NULL;
	struct pq_display_mdw_info *mdw = &pq_dev->display_info.mdw;
	struct pq_display_frc_device *frc_dev = NULL;
	int i = 0;
	int ret = 0;
	uint8_t u8frc_framecount = 0;

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_g_buffer_info));

	if (WARN_ON(!bufferctrl) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, bufferctrl=0x%llx, pq_dev=0x%llx\n", (uint64_t)bufferctrl,
			(uint64_t)pq_dev);
		return -EINVAL;
	}

	pq_pdev = dev_get_drvdata(pq_dev->dev);
	disp_dev = &pq_pdev->display_dev;
	frc_dev = &disp_dev->frc_dev;

	/* video buffers + meta buffer */
	bufferInfo.plane_num = mdw->buffer_num + 1;

	bufferInfo.size[0] = 0;
	if (mdw->output_mode == MTK_PQ_OUTPUT_MODE_BYPASS) {
		mdw->output_path = PQ_OUTPUT_PATH_WITHOUT_BUF;
	}

	for (i = 0; i < mdw->plane_num; ++i) {
		/* mdw width align info to C2 */
		bufferInfo.width_align_pitch[i] = mdw->width_align_pitch[i];
		if (mdw->buffer_num == 1) {
			/* contiguous buffer case, add all buffer size in one */
			bufferInfo.size[0] += mdw->plane_size[i];
		} else {
			/* non contiguous buffer case */
			bufferInfo.size[i] = mdw->plane_size[i];
		}
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[mdw] size[%d]=%d\n", i, bufferInfo.size[i]);
	}

	mdw->frame_size = bufferInfo.size[0];

	/* give meta fd min size */
	bufferInfo.size[bufferInfo.plane_num - 1] = 1;

	ret = mtk_pq_buffer_get_extra_frame(pq_dev, &pq_dev->extra_frame);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Failed to mtk_pq_buffer_get_extra_frame\n");
		return ret;
	}

	/* update buffer_num */
	if (mdw->output_path == PQ_OUTPUT_PATH_WITH_BUF_FRC) {
		/* from frc dtsi frame count */
		ret = _mtk_pq_display_frc_get_frame_count(pq_dev, disp_dev, &u8frc_framecount);
		if (ret < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_display_frc_get_frame_count fail, ret = %d\n", ret);
			return ret;
		}
		bufferInfo.buffer_num = u8frc_framecount;
	} else if (mdw->output_path == PQ_OUTPUT_PATH_WITH_BUF_MDW) {
		if (pq_dev->attr_mdw_buf_num > 0)
			bufferInfo.buffer_num = pq_dev->attr_mdw_buf_num;
		else {
			if (mdw->output_mode == MTK_PQ_OUTPUT_MODE_SW)
				bufferInfo.buffer_num = MDW_SW_QUEUE_FRAME_NUM;
			else if (mdw->output_mode == MTK_PQ_OUTPUT_MODE_HW)
				bufferInfo.buffer_num = MDW_HW_QUEUE_FRAME_NUM;
		}

		bufferInfo.buffer_num += pq_dev->extra_frame;

	} else if (mdw->output_path == PQ_OUTPUT_PATH_WITHOUT_BUF) {
		bufferInfo.buffer_num = MDW_HW_QUEUE_FRAME_NUM;

		bufferInfo.buffer_num += pq_dev->extra_frame;
	}

	/* update attri */
	if (mdw->output_path == PQ_OUTPUT_PATH_WITH_BUF_FRC)
		bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_CONTINUOUS;
	else if (mdw->output_path == PQ_OUTPUT_PATH_WITH_BUF_MDW) {
		if (mdw->output_mode == MTK_PQ_OUTPUT_MODE_SW)
			bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_DISCRETE;
		else if (mdw->output_mode == MTK_PQ_OUTPUT_MODE_HW)
			bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_CONTINUOUS;
	} else if (mdw->output_path == PQ_OUTPUT_PATH_WITHOUT_BUF) {
		bufferInfo.buf_attri = V4L2_PQ_BUF_ATTRI_CONTINUOUS;
	}

	/* update attri */
	if (mdw->output_path == PQ_OUTPUT_PATH_WITH_BUF_FRC) {
		bufferInfo.width = mdw->width;
		bufferInfo.height = mdw->height;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY,
			"[frc] num_planes=[%d], width[%d], height[%d]\n",
			bufferInfo.plane_num, bufferInfo.width, bufferInfo.height);
	} else {
		bufferInfo.width = mdw->width;
		bufferInfo.height = mdw->height;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY,
			"[mdw] num_planes=[%d], width[%d], height[%d]\n",
			bufferInfo.plane_num, bufferInfo.width, bufferInfo.height);
	}

	/* update bufferctrl */
	memcpy(bufferctrl, &bufferInfo, sizeof(struct v4l2_pq_g_buffer_info));

	return 0;
}

int mtk_pq_display_mdw_set_vflip(struct pq_display_mdw_info *mdw_info, bool enable)
{
	if (WARN_ON(!mdw_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "ptr is NULL, mdw_info=0x%llx\n",
			(uint64_t)mdw_info);
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW, "pq display mdw set vflip = %d\n", enable);

	mdw_info->vflip = enable;

	return 0;
}

int mtk_pq_display_mdw_set_pix_format(
	struct v4l2_format *fmt,
	struct pq_display_mdw_info *mdw_info)
{
	return 0;
}
#define FRAMES_CNT_DIV 8
int mtk_pq_display_mdw_set_pix_format_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt)
{
	struct format_info fmt_info;
	int ret = 0;

	struct pq_display_mdw_info *mdw_info = NULL;

	if (WARN_ON(!fmt) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, fmt=0x%llx, pq_dev=0x%llx\n", (uint64_t)fmt,
			(uint64_t)pq_dev);
		return -EINVAL;
	}

	mdw_info = &pq_dev->display_info.mdw;

	memset(&fmt_info, 0, sizeof(fmt_info));
	memset(mdw_info->plane_size, 0, sizeof(mdw_info->plane_size));

	/* get pixel format info */
	ret = _get_fmt_info(
		fmt->fmt.pix_mp.pixelformat,
		&fmt_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret = %d\n", ret);
		return ret;
	}

	/* fill info to share mem */
	mdw_info->width = fmt->fmt.pix_mp.width;	//TODO: get from meta
	mdw_info->height = fmt->fmt.pix_mp.height;	//TODO: get from meta
	mdw_info->mem_fmt = fmt->fmt.pix_mp.pixelformat;
	mdw_info->plane_num = fmt_info.plane_num;
	mdw_info->buffer_num = fmt_info.buffer_num;
	mdw_info->output_path = fmt_info.output_path;

	if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_BYPASS)
		mdw_info->output_path = PQ_OUTPUT_PATH_WITHOUT_BUF;

	/* update fmt to user */
	fmt->fmt.pix_mp.num_planes = mdw_info->buffer_num;

	/* save fmt  */
	memcpy(&pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp,
		&fmt->fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));

	return 0;
}

int mtk_pq_display_mdw_set_pix_format_mplane_info(
	struct mtk_pq_device *pq_dev,
	void *bufferctrl)
{

	struct v4l2_pq_s_buffer_info bufferInfo;
	struct format_info fmt_info;
	int ret = 0;
	uint32_t v_aligned = 0;
	uint32_t h_aligned_p = 0;
	uint32_t h_aligned_w[MAX_FPLANE_NUM];
	struct mtk_pq_platform_device *pq_pdev = NULL;
	struct display_device *disp_dev = NULL;
	struct pq_display_mdw_device *mdw_dev = NULL;
	struct pq_display_frc_device *frc_dev = NULL;
	struct pq_display_mdw_info *mdw_info = NULL;
	int i = 0;
	uint8_t u8frc_framecount = 0;

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_s_buffer_info));
	memcpy(&bufferInfo, bufferctrl, sizeof(struct v4l2_pq_s_buffer_info));

	if (WARN_ON(!bufferctrl) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, bufferctrl=0x%llx, pq_dev=0x%llx\n", (uint64_t)bufferctrl,
			(uint64_t)pq_dev);
		return -EINVAL;
	}

	pq_pdev = dev_get_drvdata(pq_dev->dev);
	disp_dev = &pq_pdev->display_dev;
	mdw_dev = &disp_dev->mdw_dev;
	frc_dev  = &disp_dev->frc_dev;
	mdw_info = &pq_dev->display_info.mdw;

	memset(&fmt_info, 0, sizeof(fmt_info));
	memset(h_aligned_w, 0, sizeof(h_aligned_w));
	memset(mdw_info->plane_size, 0, sizeof(mdw_info->plane_size));

	pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.width = bufferInfo.width;
	pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.height = bufferInfo.height;

	/* get pixel format info */
	ret = _get_fmt_info(
		pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.pixelformat,
		&fmt_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret = %d\n", ret);
		return ret;
	}

	/* fill info to share mem */
	mdw_info->width
	= pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.width;	//TODO: get from meta
	mdw_info->height
		= pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.height;   //TODO: get from meta
	mdw_info->mem_fmt = pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.pixelformat;
	mdw_info->plane_num = fmt_info.plane_num;
	mdw_info->buffer_num = fmt_info.buffer_num;
	mdw_info->output_path = fmt_info.output_path;

	if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_BYPASS)
		mdw_info->output_path = PQ_OUTPUT_PATH_WITHOUT_BUF;

	/* update fmt to user */
	pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.num_planes = mdw_info->buffer_num;

	if (mdw_info->output_path == PQ_OUTPUT_PATH_WITH_BUF_FRC) {
		for (i = 0; i < mdw_info->plane_num; ++i) {
			/* set frc Buffer Size */
			mdw_info->width = disp_dev->frc_dev.frc_width;
			mdw_info->height = disp_dev->frc_dev.frc_height;

			/* h size align by word */
			if (disp_dev->frc_dev.frc_h_align == 0) {
				h_aligned_w[i] = disp_dev->frc_dev.frc_width;
			} else {
				h_aligned_w[i] = POW2_ALIGN(
				disp_dev->frc_dev.frc_width, disp_dev->frc_dev.frc_h_align);
			}
			/* calc size in byte */
			mdw_info->frc_ipm_offset = (h_aligned_w[i] * disp_dev->frc_dev.frc_height *
			fmt_info.bpp[i]) / FRAMES_CNT_DIV;// * disp_dev->frc_dev.frc_frame_cnt;

			/*meds*/
			mdw_info->frc_meds_offset = (disp_dev->frc_dev.frc_meds_width *
			disp_dev->frc_dev.frc_meds_height * disp_dev->frc_dev.frc_meds_bpp) /
			FRAMES_CNT_DIV; // * disp_dev->frc_dev.frc_frame_cnt;

			mdw_info->frc_4kmi_offset = (disp_dev->frc_dev.frc_4kmi_width *
								disp_dev->frc_dev.frc_4kmi_height *
								fmt_info.bpp[i]) / FRAMES_CNT_DIV;


			mdw_info->plane_size[i] = (mdw_info->frc_ipm_offset)+
				(mdw_info->frc_meds_offset)+
				(mdw_info->frc_4kmi_offset)+
				(disp_dev->frc_dev.frc_mvbff_size);
		}

		ret = _mtk_pq_display_frc_get_frame_count(pq_dev, disp_dev, &u8frc_framecount);
		if (ret < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_display_frc_get_frame_count fail, ret = %d\n", ret);
			return ret;
		}

		mdw_info->frc_framecount = u8frc_framecount;

		// frame offset need to transfer to word
		mdw_info->frame_offset[0] = mdw_info->plane_size[0] / disp_dev->bpw;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[FRC]h=[%d],w=[%d],h_align=[%d],fmt=[0x%x], bpw=[%d],buffer_num=[%d], plane_num=[%d],frame_cnt[%d]\n",
			mdw_info->height, mdw_info->width, disp_dev->frc_dev.frc_h_align,
			mdw_info->mem_fmt, disp_dev->bpw, mdw_info->buffer_num, mdw_info->plane_num,
			disp_dev->frc_dev.frc_frame_cnt);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[FRC]ipm : h_aligned_w[%d], height[%d], bpp[%d]\n",
			h_aligned_w[0], disp_dev->frc_dev.frc_height, fmt_info.bpp[0]);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[FRC]4kmi : 4kmi_width[%d], 4kmi_height[%d], bpp[%d]\n",
			disp_dev->frc_dev.frc_4kmi_width, disp_dev->frc_dev.frc_4kmi_height,
			fmt_info.bpp[0]);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[FRC]meds : meds_width[%d], meds_height[%d], meds_bpp[%d]\n",
			disp_dev->frc_dev.frc_meds_width, disp_dev->frc_dev.frc_meds_height,
			disp_dev->frc_dev.frc_meds_bpp);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"[FRC]plane_size=[0x%x] : ipm[0x%x] + 4kmi[0x%x] + meds[0x%x] + mvbff[0x%x]\n",
			mdw_info->plane_size[0], mdw_info->frc_ipm_offset,
			mdw_info->frc_4kmi_offset, mdw_info->frc_meds_offset,
			disp_dev->frc_dev.frc_mvbff_size);
	} else if (mdw_info->output_path == PQ_OUTPUT_PATH_WITHOUT_BUF) {
		v_aligned = POW2_ALIGN(FAKE_BUF_HEIGHT, mdw_dev->v_align);
		// h size align by pixel
		h_aligned_p = POW2_ALIGN(FAKE_BUF_WIDTH, mdw_dev->h_align);
		mdw_info->line_offset = h_aligned_p;
		for (i = 0; i < mdw_info->plane_num; ++i) {
			// h size align by word
			h_aligned_w[i] = POW2_ALIGN(
				mdw_info->line_offset * fmt_info.bpp[i],
				disp_dev->bpw * BIT_PER_BYTE);
			// calc size in byte
			mdw_info->plane_size[i] = (v_aligned * h_aligned_w[i]) / BIT_PER_BYTE;

			if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_HW)
				// frame offset need to transfer to word
				mdw_info->frame_offset[i] = mdw_info->plane_size[i] / disp_dev->bpw;
			else
				mdw_info->frame_offset[i] = 0;
		}
		if (pq_dev->attr_mdw_buf_num > 0)
			mdw_info->mdw_framecount = pq_dev->attr_mdw_buf_num;
		else
			mdw_info->mdw_framecount = MDW_HW_FRAME_NUM;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"h=%d, w=%d, v_align=%d, h_align=%d, v_aligned=%d, h_aligned_p=%d, fmt=0x%x, bpw=%d, buffer_num=%d, plane_num=%d, frame_offset=0x%llx\n",
			mdw_info->height, mdw_info->width, mdw_dev->v_align, mdw_dev->h_align,
			v_aligned, h_aligned_p, mdw_info->mem_fmt, disp_dev->bpw,
			mdw_info->buffer_num, mdw_info->plane_num,
			(uint64_t)mdw_info->frame_offset);
	} else {
		/* calculate frame offset */
		// v size align by pixel
		v_aligned = POW2_ALIGN(pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.height,
			mdw_dev->v_align);
		// h size align by pixel
		h_aligned_p = POW2_ALIGN(pq_dev->pix_format_info.v4l2_fmt_pix_mdw_mp.width,
			mdw_dev->h_align);
		mdw_info->line_offset = h_aligned_p;
		for (i = 0; i < mdw_info->plane_num; ++i) {
			// h size align by word
			h_aligned_w[i] = POW2_ALIGN(
				mdw_info->line_offset * fmt_info.bpp[i],
				disp_dev->bpw * BIT_PER_BYTE);
			// calc size in byte
			mdw_info->plane_size[i] = (v_aligned * h_aligned_w[i]) / BIT_PER_BYTE;

			if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_HW)
				// frame offset need to transfer to word
				mdw_info->frame_offset[i] = mdw_info->plane_size[i] / disp_dev->bpw;
			else
				mdw_info->frame_offset[i] = 0;
		}

		if (pq_dev->attr_mdw_buf_num > 0) {
			mdw_info->mdw_framecount = pq_dev->attr_mdw_buf_num;
		} else {
			if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_SW)
				mdw_info->mdw_framecount = MDW_SW_FRAME_NUM;
			else if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_HW)
				mdw_info->mdw_framecount = MDW_HW_FRAME_NUM;
			else
				mdw_info->mdw_framecount = 0;
		}
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
			"h=%d, w=%d, v_align=%d, h_align=%d, v_aligned=%d, h_aligned_p=%d, fmt=0x%x, bpw=%d, buffer_num=%d, plane_num=%d, frame_offset=0x%llx\n",
			mdw_info->height, mdw_info->width, mdw_dev->v_align, mdw_dev->h_align,
			v_aligned, h_aligned_p, mdw_info->mem_fmt, disp_dev->bpw,
			mdw_info->buffer_num, mdw_info->plane_num,
			(uint64_t)mdw_info->frame_offset);
	}
	for (i = 0; i < mdw_info->plane_num; ++i)
		mdw_info->width_align_pitch[i] = h_aligned_w[i] / BIT_PER_BYTE;

	for (i = 0; i < MAX_FPLANE_NUM; ++i) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
		"plane[%d]: h_aligned_w=%d, bpp=%d, plane_size=0x%x\n",
		i, h_aligned_w[i], fmt_info.bpp[i], mdw_info->plane_size[i]);
	}

	return 0;
}

int mtk_pq_display_mdw_streamoff(struct pq_display_mdw_info *mdw_info)
{
	if (WARN_ON(!mdw_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, mdw_info=0x%llx\n", (uint64_t)mdw_info);
		return -EINVAL;
	}

	memset(mdw_info, 0, sizeof(struct pq_display_mdw_info));

	return 0;
}

int mtk_pq_display_mdw_qbuf(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf)
{
	struct m_pq_display_mdw_ctrl meta_mdw;
	struct meta_pq_output_frame_info meta_pqdd_frm;
	struct meta_buffer meta_buf;
	struct vb2_buffer *vb = &pq_buf->vb.vb2_buf;
	int plane_num = 0;
	int i = 0;
	int ret = 0;
	struct format_info fmt_info;
	uint64_t addr[MAX_FPLANE_NUM];
	uint64_t offset[MAX_FPLANE_NUM];
	struct mtk_pq_platform_device *pq_pdev = NULL;
	struct display_device *disp_dev = NULL;
	struct pq_display_mdw_device *mdw_dev = NULL;
	struct pq_display_mdw_info *mdw_info = NULL;
	struct pq_display_frc_device *frc_dev = NULL;
	u16 win_id = 0;

	if (WARN_ON(!pq_dev) || WARN_ON(!pq_buf)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, pq_dev=0x%llx, vb=0x%llx\n",
			(uint64_t)pq_dev, (uint64_t)pq_buf);
		return -EINVAL;
	}

	if (vb->memory != V4L2_MEMORY_DMABUF) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "unsupport memory type %d\n", vb->memory);
		return -EINVAL;
	}

	memset(&meta_mdw, 0, sizeof(struct m_pq_display_mdw_ctrl));
	memset(&fmt_info, 0, sizeof(fmt_info));
	memset(addr, 0, sizeof(addr));
	memset(offset, 0, sizeof(offset));
	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	pq_pdev = dev_get_drvdata(pq_dev->dev);
	disp_dev = &pq_pdev->display_dev;
	mdw_dev = &disp_dev->mdw_dev;
	mdw_info = &pq_dev->display_info.mdw;

	win_id = pq_dev->dev_indx;
	plane_num = vb->num_planes;
	frc_dev  = &disp_dev->frc_dev;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
		"cap, plane_num=%d\n", plane_num);

	if (mdw_info->plane_num > mdw_info->buffer_num) {
		/* contiguous fd: separate by plane size */
		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
			"cap, frame_fd=%d\n", vb->planes[0].m.fd);

		for (i = 0; i < mdw_info->plane_num; ++i) {
			if ((i + 1) < mdw_info->plane_num)
				offset[i + 1] += (offset[i] +
					((uint64_t)mdw_info->mdw_framecount *
						mdw_info->plane_size[i]));

			STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
				"cap, i=%d, offset=0x%llx\n", i, offset[i]);
		}
	} else {
		/* non contiguous fd: get addr individually */
		for (i = 0; i < mdw_info->buffer_num; ++i) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_MDW,
				"cap, frame_fd[%d]=%d\n", i, vb->planes[i].m.fd);
		}
	}

	/* meta buffer handle */
	meta_buf.paddr = pq_buf->meta_buf.va;
	meta_buf.size = pq_buf->meta_buf.size;

	/* mapping pixfmt v4l2 -> meta & get format info */
	ret = _get_fmt_info(
		mdw_info->mem_fmt,
		&fmt_info);

	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_get_fmt_info fail, ret=%d\n", ret);
		return ret;
	}

	/* get meta info from share mem */
	meta_mdw.plane_num = mdw_info->plane_num;
	memcpy(meta_mdw.plane_offset, offset, sizeof(meta_mdw.plane_offset));
	meta_mdw.mem_fmt = fmt_info.meta_fmt;
	meta_mdw.width = mdw_info->width;
	meta_mdw.height = mdw_info->height;
	meta_mdw.vflip = mdw_info->vflip;
	meta_mdw.line_offset = mdw_info->line_offset;
	memcpy(meta_mdw.frame_offset, mdw_info->frame_offset, sizeof(meta_mdw.frame_offset));
	meta_mdw.bpw = disp_dev->bpw;
	meta_mdw.output_mode = (enum pqu_pq_output_mode)mdw_info->output_mode;
	meta_mdw.mdw_framecount = mdw_info->mdw_framecount;
	meta_mdw.frc_ipversion = frc_dev->frc_ip_version;
	if (mdw_info->output_mode == MTK_PQ_OUTPUT_MODE_BYPASS)
		meta_mdw.output_path = PQ_OUTPUT_PATH_WITHOUT_BUF;
	else
		meta_mdw.output_path = mdw_info->output_path;

	meta_mdw.frame_size = mdw_info->frame_size;

	meta_mdw.frc_vgsyncbuf_phy_addr = VgsyncBufInfo.u64VgsyncBufPhyAddr;
	meta_mdw.frc_vgsyncbuf_vir_addr = VgsyncBufInfo.u64VgsyncBufVirAddr;
	meta_mdw.frc_vgsyncbuf_size = VgsyncBufInfo.u32VgsyncBufSize;

	meta_mdw.frc_shmbuf_phy_addr = FrcShmBufInfo.u64FrcShmBufPhyAddr;
	meta_mdw.frc_shmbuf_vir_addr = FrcShmBufInfo.u64FrcShmBufVirAddr;
	meta_mdw.frc_shmbuf_size = FrcShmBufInfo.u32FrcShmBufSize;

	if (mdw_info->output_path == PQ_OUTPUT_PATH_WITH_BUF_FRC) {
		if (frc_dev->frc_iommu_frc_pq_size > 0)
			meta_mdw.frc_mvbff_addr = disp_dev->frc_dev.frc_iommu_frc_pq_adr;
		else
			meta_mdw.frc_mvbff_addr = disp_dev->frc_dev.frc_mvbff_adr;

#if IS_ENABLED(CONFIG_OPTEE)
		meta_mdw.frc_aid_enable = pq_dev->display_info.secure_info.svp_enable;
		meta_mdw.frc_aid_enable |= pq_dev->display_info.secure_info.force_svp;
		meta_mdw.frc_aid = pq_dev->display_info.secure_info.aid;
#endif
		meta_mdw.frc_meds_addr = addr[0] + mdw_info->frc_ipm_offset;
		meta_mdw.frc_framecount = mdw_info->frc_framecount;
		meta_mdw.frc_ipm_offset = mdw_info->frc_ipm_offset;
		meta_mdw.frc_meds_offset = mdw_info->frc_meds_offset;
		meta_mdw.frc_output_height = pq_dev->stream_meta.output_height;
		meta_mdw.frc_output_width = pq_dev->stream_meta.output_width;
		meta_mdw.frc_output_fps = pq_dev->stream_meta.output_fps;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
		"%s %s0x%x, %s%d, %s%d, %s%d, %s%d, %s0x%x, %s0x%x, %s0x%x, %s%d, %s%d, %s%d, %s%d\n",
		"mdw w meta:",
		"fmt = ", meta_mdw.mem_fmt,
		"w = ", meta_mdw.width,
		"h = ", meta_mdw.height,
		"vflip = ", meta_mdw.vflip,
		"line_offset = ", meta_mdw.line_offset,
		"frame_offset[0] = ", meta_mdw.frame_offset[0],
		"frame_offset[1] = ", meta_mdw.frame_offset[1],
		"frame_offset[2] = ", meta_mdw.frame_offset[MAX_FPLANE_NUM-1],
		"output_path = ", meta_mdw.output_path,
		"output_mode = ", meta_mdw.output_mode,
		"bpw = ", meta_mdw.bpw,
		"mdw_framecount = ", meta_mdw.mdw_framecount);

	for (i = 0; i < MAX_FPLANE_NUM; ++i) {
		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
			"plane_offset[%d] = 0x%llx\n", i, meta_mdw.plane_offset[i]);
	}

	/* set meta buf info */
	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_DISP_MDW_CTRL, &meta_mdw);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_write_metadata_addr fail, ret=%d\n", ret);
		return ret;
	}

	/* write meta pq->render */
	memset(&meta_pqdd_frm, 0, sizeof(struct meta_pq_output_frame_info));
	meta_pqdd_frm.width = meta_mdw.width;
	meta_pqdd_frm.height = meta_mdw.height;
	meta_pqdd_frm.pq_scene = pq_dev->stream_meta.pq_scene;
	for (i = 0; i < MAX_FPLANE_NUM; ++i)
		meta_pqdd_frm.fd_offset[i] = offset[i];
	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_OUTPUT_FRAME_INFO, &meta_pqdd_frm);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_write_metadata_addr Failed, ret = %d\n", ret);
		return ret;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_MDW,
		"%s %s%d, %s%d, %s0x%llx, %s0x%llx, %s0x%llx\n",
		"pqdd frm info w meta:",
		"w = ", meta_pqdd_frm.width,
		"h = ", meta_pqdd_frm.height,
		"fd_offset[0] = ", meta_pqdd_frm.fd_offset[0],
		"fd_offset[1] = ", meta_pqdd_frm.fd_offset[1],
		"fd_offset[2] = ", meta_pqdd_frm.fd_offset[2]);

	return 0;
}

