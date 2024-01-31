// SPDX-License-Identifie_: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Kevin Ren <kevin.ren@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "mtk_pq.h"
#include "mtk_pq_dv.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define PQU_BIT_WIDTH_IDK          (36)
#define IOMMU_DMA_MASK             (34)

#define MAX_4K_W (5001)
#define MIN_4K_W (2096)
#define MAX_4K_H (2681)
#define MIN_4K_H (1280)

#define FPS_61	(61000)
#define FPS_121	121000
#define FPS_145 (145000)

#define IS4K(w, h) \
    ((w > MIN_4K_W) && (w < MAX_4K_W) && \
     (h > MIN_4K_H) && (h < MAX_4K_H))

#define UCM_FOMAT_420		(0)
#define UCM_FOMAT_420_EN	(1)
#define UCM_FHD_WIDTH	(1920)
#define UCM_FHD_HEIGHT	(1080)
#define UCM_0_BITS	(0)
#define UCM_3_BITS	(3)
#define UCM_4_BITS	(4)
#define UCM_6_BITS	(6)
#define UCM_8_BITS	(8)
#define IS_UCM_REFINE_FORMAT(ucm_format, ucm_ch0_bit, ucm_ch1_bit, ucm_ch2_bit) \
			((ucm_format == UCM_FOMAT_420) \
			&& (ucm_ch0_bit == UCM_6_BITS) \
			&& (ucm_ch1_bit == UCM_3_BITS) \
			&& (ucm_ch2_bit == UCM_0_BITS))

#define RBANK_PROT_POSITION	(2)
#define PQ_SCMI_GAME_BUF_MIN	(3)

enum {
	IN_FPS_15 = 0,
	IN_FPS_20,
	IN_FPS_24,
	IN_FPS_25,
	IN_FPS_30,
	IN_FPS_48,
	IN_FPS_50,
	IN_FPS_60,
	IN_FPS_96,
	IN_FPS_100,
	IN_FPS_120,
	IN_FPS_144,
	IN_FPS_192,
	IN_FPS_200,
	IN_FPS_240,
	IN_FPS_288,
	IN_FPS_MAX
};

/* frc table lite */
static const struct frc_table {
	int low;
	int high;
} table_lite[] = {
	[IN_FPS_15] = {
		.low = 14500,
		.high = 15500,
	},
	[IN_FPS_20] = {
		.low = 19500,
		.high = 20500
	},
	[IN_FPS_24] = {
		.low = 23500,
		.high = 24500
	},
	[IN_FPS_25] = {
		.low = 24500,
		.high = 25500
	},
	[IN_FPS_30] = {
		.low = 29500,
		.high = 30500
	},
	[IN_FPS_48] = {
		.low = 47500,
		.high = 48500
	},
	[IN_FPS_50] = {
		.low = 49500,
		.high = 50500
	},
	[IN_FPS_60] = {
		.low = 59500,
		.high = 60500
	},
	[IN_FPS_96] = {
		.low = 95500,
		.high = 96500
	},
	[IN_FPS_100] = {
		.low = 99500,
		.high = 100500
	},
	[IN_FPS_120] = {
		.low = 119500,
		.high = 120500
	},
	[IN_FPS_144] = {
		.low = 143500,
		.high = 144500
	},
	[IN_FPS_192] = {
		.low = 191500,
		.high = 192500
	},
	[IN_FPS_200] = {
		.low = 199500,
		.high = 200500
	},
	[IN_FPS_240] = {
		.low = 239500,
		.high = 240500
	},
	[IN_FPS_288] = {
		.low = 287500,
		.high = 288500
	},
};

static int _mtk_pq_cal_buf_size(struct pq_buffer *ppq_buf, enum pqu_buffer_type buf_type,
			struct meta_pq_stream_info stream)
{
	uint16_t ctur_mode = 1, ctur2_mode = 1;
	int ret = 0;

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	switch (buf_type) {
	case PQU_BUF_SCMI:
	case PQU_BUF_UCM:
		ppq_buf->size_ch[PQU_BUF_CH_0] =
			(((stream.width + (ADDRESS_ALIGN - 1)) / ADDRESS_ALIGN)
			* ADDRESS_ALIGN * stream.height * ppq_buf->frame_num
			* ppq_buf->bit[PQU_BUF_CH_0]) / BIT_PER_BYTE;
		ppq_buf->size_ch[PQU_BUF_CH_1] =
			(((stream.width + (ADDRESS_ALIGN - 1)) / ADDRESS_ALIGN)
			* ADDRESS_ALIGN * stream.height * ppq_buf->frame_num
			* ppq_buf->bit[PQU_BUF_CH_1]) / BIT_PER_BYTE;
		ppq_buf->size_ch[PQU_BUF_CH_2] =
			(((stream.width + (ADDRESS_ALIGN - 1)) / ADDRESS_ALIGN)
			* ADDRESS_ALIGN * stream.height * ppq_buf->frame_num
			* ppq_buf->bit[PQU_BUF_CH_2]) / BIT_PER_BYTE;
		break;
	case PQU_BUF_ZNR:
		ppq_buf->size_ch[PQU_BUF_CH_0] =
			((((stream.width + ZNR_BLK_SIZE_CH_0 - 1) / ZNR_BLK_SIZE_CH_0
			+ (ADDRESS_ALIGN - 1)) / ADDRESS_ALIGN) * ADDRESS_ALIGN
			* ((stream.height + ZNR_BLK_SIZE_CH_0 - 1) / ZNR_BLK_SIZE_CH_0)
			* ppq_buf->frame_num * ppq_buf->bit[PQU_BUF_CH_0] + BIT_PER_BYTE - 1)
			/ BIT_PER_BYTE;
		ppq_buf->size_ch[PQU_BUF_CH_1] =
			((((stream.width + ZNR_BLK_SIZE_CH_1 - 1) / ZNR_BLK_SIZE_CH_1
			+ (ADDRESS_ALIGN - 1)) / ADDRESS_ALIGN) * ADDRESS_ALIGN
			* ((stream.height + ZNR_BLK_SIZE_CH_1 - 1) / ZNR_BLK_SIZE_CH_1)
			* ppq_buf->frame_num * ppq_buf->bit[PQU_BUF_CH_1] + BIT_PER_BYTE - 1)
			/ BIT_PER_BYTE;
		ppq_buf->size_ch[PQU_BUF_CH_2] = 0;

		ret = drv_hwreg_pq_display_znr_set_stub(stream.stub);
		ret = drv_hwreg_pq_display_znr_buffer(ppq_buf->size_ch,
				ppq_buf->frame_num, ppq_buf->bit);
		break;
	case PQU_BUF_ABF:
		mtk_display_abf_blk_mode(stream.width, stream.height, &ctur_mode, &ctur2_mode);

		ppq_buf->size_ch[PQU_BUF_CH_0] =
				(stream.width / ctur_mode) * (stream.height / ctur_mode) *
				((ppq_buf->bit[PQU_BUF_CH_0] + BIT_PER_BYTE - 1) / BIT_PER_BYTE)
				* ppq_buf->frame_num;
		ppq_buf->size_ch[PQU_BUF_CH_1] =
				(stream.width / ctur2_mode) *
				(stream.height / ctur2_mode) *
				((ppq_buf->bit[PQU_BUF_CH_1] + BIT_PER_BYTE - 1) / BIT_PER_BYTE)
				* ppq_buf->frame_num;
		ppq_buf->size_ch[PQU_BUF_CH_2] =
				((stream.width / ctur2_mode) + PQ_CTUR2_EXT_BIT) *
				((stream.height / ctur2_mode) + PQ_CTUR2_EXT_BIT) *
				((ppq_buf->bit[PQU_BUF_CH_2] + BIT_PER_BYTE - 1) / BIT_PER_BYTE)
				* ppq_buf->frame_num;
		break;
	case PQU_BUF_MCDI:
		ppq_buf->size_ch[PQU_BUF_CH_0] = MCDI_BUFFER_SIZE0;
		ppq_buf->size_ch[PQU_BUF_CH_1] = MCDI_BUFFER_SIZE1;
		ppq_buf->size_ch[PQU_BUF_CH_2] = 0;
		break;
	case PQU_BUF_DIPDI:
		ppq_buf->size_ch[PQU_BUF_CH_0] = DIPDI_BUFFER_SIZE;
		ppq_buf->size_ch[PQU_BUF_CH_1] = 0;
		ppq_buf->size_ch[PQU_BUF_CH_2] = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int _mtk_pq_cut_reserved_memory(struct mtk_pq_platform_device *pqdev)
{
	enum pqu_buffer_type buf_idx = PQU_BUF_SCMI;
	int iommu_idx = 0, win = 0, buf_win_size = 0;
	uint32_t mmap_size = 0;
	uint64_t mmap_addr = 0;
	struct pq_buffer **ppResBuf = NULL;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	ppResBuf = pqdev->display_dev.pReserveBufTbl;

	if (ppResBuf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
		iommu_idx = mtk_pq_buf_get_iommu_idx(pqdev, buf_idx);

		if (iommu_idx == 0) {
			switch (buf_idx) {
			case PQU_BUF_ZNR:
				mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_ZNR_INDEX,
				&mmap_addr, &mmap_size);

				 mmap_addr -= BUSADDRESS_TO_IPADDRESS_OFFSET;
				break;
			case PQU_BUF_ABF:
				mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_ABF_INDEX,
				&mmap_addr, &mmap_size);

				 mmap_addr -= BUSADDRESS_TO_IPADDRESS_OFFSET;
				break;
			case PQU_BUF_MCDI:
				mtk_pq_get_dedicate_memory_info(pqdev->dev, MMAP_MCDI_INDEX,
				&mmap_addr, &mmap_size);

				mmap_addr -= BUSADDRESS_TO_IPADDRESS_OFFSET;
				break;
			default:
				break;
			}

			buf_win_size = mmap_size / pqdev->xc_win_num;

			for (win = 0; win < pqdev->xc_win_num; win++) {
				if (ppResBuf[win] == NULL) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"revserve table[%d] is not initialized\n", win);
					return -EPERM;
				}

				ppResBuf[win][buf_idx].size = buf_win_size;
				ppResBuf[win][buf_idx].addr = mmap_addr + (win * buf_win_size);
				ppResBuf[win][buf_idx].valid = true;

				STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
					"ppResBuf[%d][%d].size : %u\n",
					win, buf_idx, ppResBuf[win][buf_idx].size);

				STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
					"ppResBuf[%d][%d].addr : 0x%llx\n",
					win, buf_idx, ppResBuf[win][buf_idx].addr);
			}
		}
	}

	return 0;
}

static int _mtk_pq_get_reserved_memory(struct mtk_pq_device *pq_dev,
		enum pqu_buffer_type buf_type, unsigned long long *iova, uint32_t size)
{
	int ret = 0, win = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct pq_buffer **ppResBuf = NULL;

	if (iova == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	win = pq_dev->dev_indx;
	pqdev = dev_get_drvdata(pq_dev->dev);

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	ppResBuf = pqdev->display_dev.pReserveBufTbl;

	if (ppResBuf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppResBuf[win] == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppResBuf[win][buf_type].size < size) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"mmap buf isn't enough : %u < %u\n",
		ppResBuf[win][buf_type].size, size);
		return -EPERM;
	}

	if (ppResBuf[win][buf_type].valid == false) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"mmap buf isn't in mmap : %d\n", buf_type);
		return -EPERM;
	}

	*iova = ppResBuf[win][buf_type].addr;

	return ret;
}

static int _mtk_pq_get_dipdi_frame_num(void)
{
	struct device_node *np;
	int ret = 0;
	uint32_t useDINR = 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,tvpqu");
	if (np != NULL) {
		ret = of_property_read_u32(np, "DipUseDINR", &useDINR);
		if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "Read DTS table failed, name = %s\n",
				"DipUseDINR");
		}
	}
	if (useDINR == 1)
		ret = DIPDI_FRAME;
	else
		ret = 0;
	return ret;
}

static int _mtk_pq_get_hwmap_info(struct pq_buffer *ppq_buf, enum pqu_buffer_type buf_type,
		hwmap_swbit *swbit)
{
	struct mtk_pq_dv_status dv_status;

	if (ppq_buf == NULL || swbit == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	memset(&dv_status, 0, sizeof(struct mtk_pq_dv_status));

	switch (buf_type) {
	case PQU_BUF_SCMI:
		mtk_pq_dv_get_status(&dv_status);
		if (dv_status.idk_enable == TRUE) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQU 36 bit mode due to IDK dump\n");
			ppq_buf->bit[PQU_BUF_CH_0] = PQU_BIT_WIDTH_IDK;
			ppq_buf->bit[PQU_BUF_CH_1] = 0;
			ppq_buf->bit[PQU_BUF_CH_2] = 0;
		} else {
			ppq_buf->bit[PQU_BUF_CH_0] = swbit->scmi_ch0_bit;
			ppq_buf->bit[PQU_BUF_CH_1] = swbit->scmi_ch1_bit;
			ppq_buf->bit[PQU_BUF_CH_2] = swbit->scmi_ch2_bit;
		}
		ppq_buf->frame_num = swbit->scmi_frame;
		ppq_buf->frame_diff = swbit->scmi_delay; // scmi frame diff is same as frame delay
		ppq_buf->frame_delay = swbit->scmi_delay;
		ppq_buf->frame_format = swbit->scmi_format;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[SCMI] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	case PQU_BUF_ZNR:
		ppq_buf->bit[PQU_BUF_CH_0] = swbit->znr_ch0_bit;
		ppq_buf->bit[PQU_BUF_CH_1] = swbit->znr_ch1_bit;
		ppq_buf->bit[PQU_BUF_CH_2] = 0;
		ppq_buf->frame_num = swbit->znr_frame;
		ppq_buf->frame_diff = 0;
		ppq_buf->frame_delay = 0;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[ZNR] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	case PQU_BUF_UCM:
		ppq_buf->bit[PQU_BUF_CH_0] = swbit->ucm_ch0_bit;
		ppq_buf->bit[PQU_BUF_CH_1] = swbit->ucm_ch1_bit;
		ppq_buf->bit[PQU_BUF_CH_2] = swbit->ucm_ch2_bit;
		ppq_buf->frame_num = swbit->ucm_frame;
		ppq_buf->frame_diff = swbit->ucm_delay; // ucm frame diff is same as frame delay
		ppq_buf->frame_delay = swbit->ucm_delay;
		ppq_buf->frame_format = swbit->ucm_format;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[UCM] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	case PQU_BUF_ABF:
		ppq_buf->bit[PQU_BUF_CH_0] = swbit->abf_ch0_bit;
		ppq_buf->bit[PQU_BUF_CH_1] = swbit->abf_ch1_bit;
		ppq_buf->bit[PQU_BUF_CH_2] = swbit->abf_ch2_bit;
		ppq_buf->frame_num = swbit->abf_frame;
		ppq_buf->frame_diff = 0;
		ppq_buf->frame_delay = 0;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[ABF] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	case PQU_BUF_MCDI:
		ppq_buf->bit[PQU_BUF_CH_0] = 0;
		ppq_buf->bit[PQU_BUF_CH_1] = 0;
		ppq_buf->bit[PQU_BUF_CH_2] = 0;
		ppq_buf->frame_num = swbit->mcdi_frame;
		ppq_buf->frame_diff = 0;
		ppq_buf->frame_delay = 0;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[MCDI] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	case PQU_BUF_DIPDI:
		ppq_buf->bit[PQU_BUF_CH_0] = 0;
		ppq_buf->bit[PQU_BUF_CH_1] = 0;
		ppq_buf->bit[PQU_BUF_CH_2] = 0;
		ppq_buf->frame_num = _mtk_pq_get_dipdi_frame_num();
		ppq_buf->frame_diff = 0;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"[DIPDI] %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u\n",
			"bit[0]", ppq_buf->bit[PQU_BUF_CH_0],
			"bit[1]", ppq_buf->bit[PQU_BUF_CH_1],
			"bit[2]", ppq_buf->bit[PQU_BUF_CH_2],
			"frame_num", ppq_buf->frame_num,
			"frame_diff", ppq_buf->frame_diff,
			"frame_delay", ppq_buf->frame_delay);
		break;
	default:
		break;
	}

	return 0;
}

static int _mtk_pq_allocate_buf(struct mtk_pq_device *pq_dev, enum pqu_buffer_type buf_type,
			struct pq_buffer *ppq_buf, struct meta_pq_stream_info stream_pq)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	unsigned int buf_iommu_idx = 0;
	unsigned long iommu_attrs = 0;
	unsigned long long iova = 0, va = 0;
	int ret = 0;
	uint32_t size = 0;
	struct device *dev = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	dev = pq_dev->dev;
	pqdev = dev_get_drvdata(pq_dev->dev);
	buf_iommu_idx = mtk_pq_buf_get_iommu_idx(pqdev, buf_type);

	if (dma_get_mask(dev) < DMA_BIT_MASK(34)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "IOMMU isn't registered\n");
		return -1;
	}

	if (!dma_supported(dev, 0)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"IOMMU is not supported\n");
		return -1;
	}

	ret = _mtk_pq_cal_buf_size(ppq_buf, buf_type, stream_pq);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_cal_buf_size Failed, ret = %d\n", ret);
		return ret;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
		"%s%x, %s%u, %s%u, %s%u\n",
		"buf_type : ", buf_type,
		"size_ch[PQU_BUF_CH_0] : ", ppq_buf->size_ch[PQU_BUF_CH_0],
		"size_ch[PQU_BUF_CH_1] : ", ppq_buf->size_ch[PQU_BUF_CH_1],
		"size_ch[PQU_BUF_CH_2] : ", ppq_buf->size_ch[PQU_BUF_CH_2]);

	size = ppq_buf->size_ch[PQU_BUF_CH_0] + ppq_buf->size_ch[PQU_BUF_CH_1]
							+ ppq_buf->size_ch[PQU_BUF_CH_2];

	if (size > 0) {
		if (buf_iommu_idx != 0) {
			// According to iommu requires the size to align 4K.
			size = PAGE_ALIGN(size);

			// set buf tag
			iommu_attrs |= buf_iommu_idx;

			// if mapping uncache
			iommu_attrs |= DMA_ATTR_WRITE_COMBINE;

			if (size == 0) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"PQ buffer alloc size = 0\n");
				return -1;
			}

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
				"buf_type : %d, buf_iommu_idx : %x, size = %u\n",
				buf_type, buf_iommu_idx, size);

			va = (unsigned long long)dma_alloc_attrs(dev,
					size,
					&iova,
					GFP_KERNEL,
					iommu_attrs);
			ret = dma_mapping_error(dev, iova);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
					"dma_alloc_attrs fail\n");
				return ret;
			}
		} else {
			ret = _mtk_pq_get_reserved_memory(pq_dev, buf_type, &iova, size);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_get_reserved_memory Failed, ret = %d\n", ret);
				return ret;
			}
		}

		ppq_buf->valid = true;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"buf = {va : %llx, addr : %llx, size = %u}\n", va, iova, size);
	}

	ppq_buf->va = va;
	ppq_buf->addr = iova;
	ppq_buf->size = size;

	return 0;
}

static int _mtk_pq_release_buf(struct mtk_pq_device *pq_dev,
	enum pqu_buffer_type buf_type, struct pq_buffer *ppq_buf)
{
	struct device *dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;
	unsigned int buf_iommu_idx = 0;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	dev = pq_dev->dev;
	pqdev = dev_get_drvdata(pq_dev->dev);
	buf_iommu_idx = mtk_pq_buf_get_iommu_idx(pqdev, buf_type);

	/* clear valid flag first to avoid racing condition */
	ppq_buf->valid = false;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
		"buf_type : %d, buf_iommu_idx  %u, size : %u\n",
		buf_type, buf_iommu_idx, ppq_buf->size);

	if (buf_iommu_idx != 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"va : %llx, addr : %llx\n", ppq_buf->va, ppq_buf->addr);

		dma_free_attrs(dev, ppq_buf->size, (void *)ppq_buf->va,
			ppq_buf->addr, buf_iommu_idx);
	}

	return 0;
}

static int _mtk_pq_allocate_frc_pq_buf(struct mtk_frc_device *frc_dev,
	struct pq_buffer_frc_pq *ppq_buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	unsigned int frc_pq_buf_iommu_idx = 0;
	unsigned long iommu_attrs = 0;
	unsigned long long iova = 0, va = 0;
	struct device *dev = NULL;
	uint32_t size = 0x0;
	int ret = 0;

	if (frc_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	dev = frc_dev->dev;

	if (dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pqdev = dev_get_drvdata(frc_dev->dev);

	frc_pq_buf_iommu_idx = (pqdev->display_dev.frc_dev.frc_iommu_idx_frc_pq <<
		pqdev->display_dev.frc_dev.frc_buf_iommu_offset);
	size = (pqdev->display_dev.frc_dev.frc_iommu_frc_pq_size);

	if (dma_get_mask(dev) < DMA_BIT_MASK(IOMMU_DMA_MASK)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "IOMMU isn't registered\n");
		return -1;
	}

	if (!dma_supported(dev, 0)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"IOMMU is not supported\n");
		return -1;
	}

	if (size > 0) {
		if (frc_pq_buf_iommu_idx != 0) {
			// According to iommu requires the size to align 4K.
			size = PAGE_ALIGN(size);

			// set buf tag
			iommu_attrs |= frc_pq_buf_iommu_idx;

			// if mapping uncache
			iommu_attrs |= DMA_ATTR_WRITE_COMBINE;

			if (size == 0) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"FRC PQ buffer alloc size = 0\n");
				return -1;
			}

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
				" frc_pq_buf_iommu_idx : %x, size = %u\n",
				frc_pq_buf_iommu_idx, size);

			va = (unsigned long long)dma_alloc_attrs(dev,
					size,
					&iova,
					GFP_KERNEL,
					iommu_attrs);

			ret = dma_mapping_error(dev, iova);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
					"dma_alloc_attrs fail\n");
				return ret;
			}
		}

		ppq_buf->valid = true;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"buf = {va : %llx, addr : %llx, size = %u}\n", va, iova, size);
		pqdev->display_dev.frc_dev.frc_iommu_frc_pq_adr = (uint64_t)iova;
	}

	ppq_buf->va = va;
	ppq_buf->addr = iova;
	ppq_buf->size = size;

	return 0;
}

static int _mtk_pq_release_frc_pq_buf(struct mtk_frc_device *frc_dev,
		struct pq_buffer_frc_pq *ppq_buf)
{
	struct device *dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;
	unsigned int frc_pq_buf_iommu_idx = 0;

	if (frc_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	dev = frc_dev->dev;

	if (dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pqdev = dev_get_drvdata(frc_dev->dev);
	frc_pq_buf_iommu_idx = (pqdev->display_dev.frc_dev.frc_iommu_idx_frc_pq	<<
		pqdev->display_dev.frc_dev.frc_buf_iommu_offset);

	ppq_buf->valid = false;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
		"frc_pq_buf_iommu_idx  %u, size : %u\n",
		 frc_pq_buf_iommu_idx, ppq_buf->size);

	if (frc_pq_buf_iommu_idx != 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"va : %llx, addr : %llx\n", ppq_buf->va, ppq_buf->addr);

		dma_free_attrs(dev, ppq_buf->size, (void *)ppq_buf->va,
			ppq_buf->addr, frc_pq_buf_iommu_idx);
	}

	return 0;
}

static int _mtk_pq_buffer_debug_swbit(struct mtk_pq_device *pq_dev,
	hwmap_swbit *swbit)
{
	/*fill debug info*/
	pq_dev->pq_debug.swbit.scmi_ch0_bit = swbit->scmi_ch0_bit;
	pq_dev->pq_debug.swbit.scmi_ch1_bit = swbit->scmi_ch1_bit;
	pq_dev->pq_debug.swbit.scmi_ch2_bit = swbit->scmi_ch2_bit;
	pq_dev->pq_debug.swbit.scmi_comp_bit = swbit->scmi_comp_bit;
	pq_dev->pq_debug.swbit.scmi_format = swbit->scmi_format;
	pq_dev->pq_debug.swbit.scmi_frame = swbit->scmi_frame;
	pq_dev->pq_debug.swbit.scmi_delay = swbit->scmi_delay;

	pq_dev->pq_debug.swbit.ucm_ch0_bit = swbit->ucm_ch0_bit;
	pq_dev->pq_debug.swbit.ucm_ch1_bit = swbit->ucm_ch1_bit;
	pq_dev->pq_debug.swbit.ucm_ch2_bit = swbit->ucm_ch2_bit;
	pq_dev->pq_debug.swbit.ucm_format = swbit->ucm_format;
	pq_dev->pq_debug.swbit.ucm_frame = swbit->ucm_frame;
	pq_dev->pq_debug.swbit.ucm_delay = swbit->ucm_delay;
	pq_dev->pq_debug.swbit.ucm_diff = swbit->ucm_diff;

	pq_dev->pq_debug.swbit.znr_aul_en = swbit->znr_aul_en;
	pq_dev->pq_debug.swbit.znr_frame = swbit->znr_frame;
	pq_dev->pq_debug.swbit.znr_ch0_bit = swbit->znr_ch0_bit;
	pq_dev->pq_debug.swbit.znr_ch1_bit = swbit->znr_ch1_bit;

	pq_dev->pq_debug.swbit.spf_aul_en = swbit->spf_aul_en;

	pq_dev->pq_debug.swbit.abf_frame = swbit->abf_frame;
	pq_dev->pq_debug.swbit.abf_ch0_bit = swbit->abf_ch0_bit;
	pq_dev->pq_debug.swbit.abf_ch1_bit = swbit->abf_ch1_bit;
	pq_dev->pq_debug.swbit.abf_ch2_bit = swbit->abf_ch2_bit;

	pq_dev->pq_debug.swbit.mcdi_frame = swbit->mcdi_frame;

	pq_dev->pq_debug.swbit.dnr_func_on = swbit->dnr_func_on;
	pq_dev->pq_debug.swbit.dnr_motion_on = swbit->dnr_motion_on;
	pq_dev->pq_debug.swbit.znr_func_on = swbit->znr_func_on;
	pq_dev->pq_debug.swbit.pnr_motion_on = swbit->pnr_motion_on;
	pq_dev->pq_debug.swbit.di_func_on = swbit->di_func_on;
	pq_dev->pq_debug.swbit.di_mode = swbit->di_mode;
	pq_dev->pq_debug.swbit.spf_func_on = swbit->spf_func_on;
	pq_dev->pq_debug.swbit.abf_func_on = swbit->abf_func_on;

	return 0;
}

int mtk_pq_get_dedicate_memory_info(struct device *dev, int pool_index,
	__u64 *pbus_address, __u32 *pbus_size)
{
	struct of_mmap_info_data of_mmap_info;

	if (dev == NULL || pbus_address == NULL || pbus_size == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "NULL pointer\n");
		return -EINVAL;
	}

	memset(&of_mmap_info, 0, sizeof(struct of_mmap_info_data));
	of_mtk_get_reserved_memory_info_by_idx(dev->of_node, pool_index, &of_mmap_info);
	*pbus_address = of_mmap_info.start_bus_address;
	*pbus_size = of_mmap_info.buffer_size;

	if (*pbus_address == 0 || *pbus_size == 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid bus address\n");
		return -EINVAL;
	}
	return 0;
}

int mtk_pq_get_dedicate_memory_cacheable(int pool_index, bool *pcacheable)
{
	if (pcacheable == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "NULL pointer\n");
		return -EINVAL;
	}

	switch (pool_index) {
	case MMAP_MCDI_INDEX:
	case MMAP_ABF_INDEX:
	case MMAP_ZNR_INDEX:
	case MMAP_PQMAP_INDEX:
	case MMAP_CONTROLMAP_INDEX:
	case MMAP_DV_PYR_INDEX:
	case MMAP_DV_CONFIG_INDEX:
	case MMAP_STREAM_METADATA_INDEX:
	case MMAP_METADATA_INDEX:
	case MMAP_CFD_BUF_INDEX:
		*pcacheable = true;
		break;
	case MMAP_PQPARAM_INDEX:
		*pcacheable = false;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid pool_index %d\n", pool_index);
		return -EINVAL;
	}

	return 0;
}

int mtk_pq_buf_get_iommu_idx(struct mtk_pq_platform_device *pqdev, enum pqu_buffer_type buf_type)
{
	int idx = 0;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	switch (buf_type) {
	case PQU_BUF_SCMI:
		idx = pqdev->display_dev.pq_iommu_idx_scmi;
		break;
	case PQU_BUF_UCM:
		idx = pqdev->display_dev.pq_iommu_idx_ucm;
		break;
	case PQU_BUF_DIPDI:
		idx = pqdev->display_dev.pq_iommu_idx_dip;
		break;
	default:
		/* buffer type does not support iommu */
		break;
	}

	return (idx << pqdev->display_dev.buf_iommu_offset);
}

int mtk_pq_buffer_reserve_buf_init(struct mtk_pq_platform_device *pqdev)
{
	uint8_t idx = 0;
	struct pq_buffer **ppResBuf = NULL;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (pqdev->display_dev.pReserveBufTbl != NULL)
		mtk_pq_buffer_reserve_buf_exit(pqdev);

	pqdev->display_dev.pReserveBufTbl
		= (struct pq_buffer **)malloc(pqdev->xc_win_num * sizeof(struct pq_buffer *));

	ppResBuf = pqdev->display_dev.pReserveBufTbl;

	if (ppResBuf != NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"pq reserve buffer allocate succeesful : %llx\n", (uint64_t)ppResBuf);

		memset(ppResBuf, 0, pqdev->xc_win_num * sizeof(struct pq_buffer *));

		for (idx = 0; idx < pqdev->xc_win_num; idx++) {
			ppResBuf[idx] =
				(struct pq_buffer *)malloc(PQU_BUF_MAX * sizeof(struct pq_buffer));

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
				"pq[%d] reserve buffer allocate succeesful : %llx\n",
				idx, (uint64_t)ppResBuf[idx]);

			if (ppResBuf[idx] == NULL) {
				mtk_pq_buffer_reserve_buf_exit(pqdev);

				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"pq[%d] reserve buffer allocate failed!\n", idx);
				return -EPERM;
			}
		}

		_mtk_pq_cut_reserved_memory(pqdev);

	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"pq reserve buffer allocate failed!\n");
		return -EPERM;
	}

	return 0;
}

int mtk_pq_buffer_reserve_buf_exit(struct mtk_pq_platform_device *pqdev)
{
	uint8_t idx = 0;
	struct pq_buffer **ppResBuf = NULL;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	ppResBuf = pqdev->display_dev.pReserveBufTbl;

	if (ppResBuf != NULL) {
		for (idx = 0; idx < pqdev->xc_win_num; idx++) {
			if (ppResBuf[idx] != NULL) {
				free(ppResBuf[idx]);
				ppResBuf[idx] = NULL;
			}
		}

		free(ppResBuf);
		ppResBuf = NULL;
	}

	return 0;
}

int mtk_pq_buffer_buf_init(struct mtk_pq_device *pq_dev)
{
	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	memset(pq_dev->display_info.BufferTable, 0,
			PQU_BUF_MAX*sizeof(struct pq_buffer));

	return 0;
}

int mtk_pq_buffer_buf_exit(struct mtk_pq_device *pq_dev)
{
	enum pqu_buffer_type idx = PQU_BUF_SCMI;
	struct pq_buffer *ppq_buf = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	for (idx = PQU_BUF_SCMI; idx < PQU_BUF_MAX; idx++) {
		ppq_buf = &(pq_dev->display_info.BufferTable[idx]);

		if (ppq_buf->valid == true)
			_mtk_pq_release_buf(pq_dev, idx, ppq_buf);
	}

	memset(&(pq_dev->display_info.BufferTable), 0, PQU_BUF_MAX*sizeof(struct pq_buffer));

	return 0;
}

static inline bool _check_is_framelock(uint32_t input_fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(table_lite); ++i) {
		if (input_fps >= table_lite[i].low &&
			input_fps <= table_lite[i].high)
			return true;
	}

	return false;
}

static inline bool _check_is_in_out_1on1(uint32_t input_fps, uint32_t output_fps)
{
	int st, end;

	/* check 1:1
	 * for 4k2k60, the possible 1:1 input fps case is 48~60
	 * for 4k2k120/144, the possible 1:1 input fps case is 96~144
	 * for 4k1k240/288, the possible 1:1 input fps case is 192~288
	 */
	if (output_fps <= FPS_61) {
		st = IN_FPS_48;
		end = IN_FPS_60;
	} else if (output_fps <= FPS_145) {
		st = IN_FPS_96;
		end = IN_FPS_144;
	} else {
		st = IN_FPS_192;
		end = IN_FPS_288;
	}

	for (; st <= end; ++st) {
		if (input_fps >= table_lite[st].low && input_fps <= table_lite[st].high)
			return true;
	}

	return false;
}


static inline bool _check_is_in_out_1on2(uint32_t input_fps, uint32_t output_fps)
{
	int st, end;

	/* check 1:2
	 * for 4k2k60, the possible 1:1 input fps case is 25, 30
	 * for 4k2k120, the possible 1:1 input fps case is 50, 60
	 */
	if (output_fps <= FPS_61) {
		st = IN_FPS_25;
		end = IN_FPS_30;
	} else if (output_fps <= FPS_121) {
		st = IN_FPS_50;
		end = IN_FPS_60;
	} else {
		return false;
	}

	for (; st <= end; ++st) {
		if (input_fps >= table_lite[st].low && input_fps <= table_lite[st].high)
			return true;
	}

	return false;
}

static inline bool _full_screen(struct meta_pq_stream_info *stream_pq)
{
	if (stream_pq->display_width == 0 || stream_pq->display_height == 0)
		return true;

	// disp size = out size, full screen
	return (stream_pq->display_width == stream_pq->output_width &&
		stream_pq->display_height == stream_pq->output_height);
}

static int mtk_pq_buffer_hwmap_fixup(struct mtk_pq_device *pq_dev,
	hwmap_swbit *swbit)
{
	bool need_fixup = false; // determin scmi opm r/w diff + 1
	struct meta_pq_stream_info *stream_pq;
	uint32_t buffer_mode;
	uint16_t protectVal;
	bool scene_game = false;
	uint32_t pqout_fps;

	if (!pq_dev || !swbit) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	stream_pq = &(pq_dev->stream_meta);
	buffer_mode = drv_hwreg_common_config_get_buffer_mode((uint32_t)stream_pq->scenario_idx);
	pq_dev->scmi_opm_diff_fixup = false;
	scene_game = (stream_pq->pq_scene == meta_pq_scene_game ||
			stream_pq->pq_scene == meta_pq_scene_game_with_pc ||
			stream_pq->pq_scene == meta_pq_scene_game_with_memc);

	/* it should be someting wrong if no in/out fps */
	if (stream_pq->input_fps == 0 || stream_pq->output_fps == 0)
		return -EINVAL;

	/* do nothing if froce P */
	if (stream_pq->pq_scene == meta_pq_scene_force_p || stream_pq->bForceP)
		return 0;

	/*
	 * pre condition: scmi should be FB && rwdiff = 0
	 * assume scmi_delay == scmi_frame
	 */
	if (buffer_mode != hwmap_buffer_mode_fb || swbit->scmi_delay != 0)
		return 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
		"scenario_idx:%d, buf:%d, diff:%d, src:%d, in(%dx%d-%d), out(%dx%d-%d), interlace:%d, scene:%d, framelock:%d, frc:%s\n",
		(int)stream_pq->scenario_idx,
		(int)buffer_mode,
		(int)swbit->scmi_delay,
		(int)stream_pq->pq_source,
		stream_pq->display_width,
		stream_pq->display_height,
		stream_pq->input_fps,
		stream_pq->output_width,
		stream_pq->output_height,
		stream_pq->output_fps,
		(int)stream_pq->interlace,
		(int)stream_pq->pq_scene,
		(int)_check_is_framelock(stream_pq->input_fps),
		pq_dev->common_info.output_mode == MTK_PQ_OUTPUT_MODE_BYPASS ? "FBL" : "FB");

	/* following cases the scmi opm rwdiff should be patched by SW */
	do {
		/*
		 * FRC FB case
		 *	pq trig select input, so that pq out = pq in
		 * 1. framelock free run
		 * 2. in:out is not 1:1
		 * 3. in:out is 1:1 and non-game mode
		 * 4. i-mode and game mode
		 */
		if (pq_dev->common_info.output_mode != MTK_PQ_OUTPUT_MODE_BYPASS) {
			pqout_fps = stream_pq->input_fps;

			if (!_check_is_framelock(stream_pq->input_fps)) {
				need_fixup = true;
				break;
			}

			if (!_check_is_in_out_1on1(stream_pq->input_fps, pqout_fps)) {
				need_fixup = true;
				break;
			}

			if (_check_is_in_out_1on1(stream_pq->input_fps, pqout_fps) &&
				!scene_game) {
				need_fixup = true;
				break;
			}

			if (stream_pq->interlace && scene_game) {
				need_fixup = true;
				break;
			}
		} else {
			/*
			 * FRC FBL case
			 *	pq trig select TGEN, so that pq out = render out
			 * 1. 1:2 and game mode: diff=0.5 (vrr 1:1, diff=1)
			 * 2. in:out is not 1:1, 1:2
			 */
			pqout_fps = stream_pq->output_fps;

			if (_check_is_in_out_1on2(stream_pq->input_fps, pqout_fps) &&
				scene_game) {
				protectVal = stream_pq->height / RBANK_PROT_POSITION;
				need_fixup = true;
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "protect: %d, scmi_in_h: %d.\n",
					protectVal,
					stream_pq->height);
				break;
			}

			if (!_check_is_in_out_1on1(stream_pq->input_fps, pqout_fps)) {
				need_fixup = true;
				break;
			}
		}

		/* hdmi + non fullscreen */
		if (stream_pq->pq_source == meta_pq_source_ipdma &&
			!_full_screen(stream_pq)) {
			need_fixup = true;
			break;
		}
		/* hdmi + game crop */
		if (stream_pq->pq_source == meta_pq_source_ipdma &&
			stream_pq->pq_scene == meta_pq_scene_game_crop) {
			need_fixup = true;
			break;
		}
	} while (0);

	if (need_fixup) {
		pq_dev->scmi_opm_diff_fixup = true;	// to notify pqu
		pq_dev->scmi_protect_line = protectVal;
		if (protectVal == 0) {
			swbit->scmi_delay = 1;
			swbit->scmi_frame = (swbit->scmi_frame < PQ_SCMI_GAME_BUF_MIN) ? \
				PQ_SCMI_GAME_BUF_MIN : swbit->scmi_frame;
		}

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "fixup scmi opm diff, protect size: %d.\n",
			pq_dev->scmi_protect_line);
	}

	return 0;
}

int mtk_pq_buffer_get_hwmap_info(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	enum pqu_buffer_type buf_idx = PQU_BUF_SCMI;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct pq_buffer *pBufferTable = NULL;
	struct meta_pq_stream_info *stream_pq = NULL;
	hwmap_swbit swbit, swbit_p, swbit_i;
	struct mtk_pq_caps *pqcaps;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	pBufferTable = pq_dev->display_info.BufferTable;
	stream_pq = &(pq_dev->stream_meta);
	pqcaps = &pqdev->pqcaps;

	memset(&swbit, 0, sizeof(hwmap_swbit));
	memset(&swbit_p, 0, sizeof(hwmap_swbit));
	memset(&swbit_i, 0, sizeof(hwmap_swbit));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "stream.width: %d, stream.height: %d\n",
						stream_pq->width, stream_pq->height);

	drv_hwreg_common_config_load_tab_by_src(
		stream_pq->scenario_idx, PQ_BIN_IP_ALL, pqdev->config_info.u8Config_Version,
		false, NULL, NULL, NULL, &swbit, NULL, NULL);

	if ((stream_pq->pq_scene == meta_pq_scene_force_p) && (stream_pq->bForceP == true)) {
		uint8_t p_idx = 0, i_idx = 0;

		p_idx = drv_hwreg_common_get_pi_idx(FORCEP_LEVEL_P_BEST);
		i_idx = drv_hwreg_common_get_pi_idx(FORCEP_LEVEL_I_BEST);

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "p_idx:%d, i_idx:%d\n", p_idx, i_idx);

		drv_hwreg_common_config_load_tab_by_src(
			p_idx, PQ_BIN_IP_ALL, pqdev->config_info.u8Config_Version,
			false, NULL, NULL, NULL, &swbit_p, NULL, NULL);
		drv_hwreg_common_config_load_tab_by_src(
			i_idx, PQ_BIN_IP_ALL, pqdev->config_info.u8Config_Version,
			false, NULL, NULL, NULL, &swbit_i, NULL, NULL);

		swbit.scmi_ch0_bit = MAX(swbit_p.scmi_ch0_bit, swbit_i.scmi_ch0_bit);
		swbit.scmi_ch1_bit = MAX(swbit_p.scmi_ch1_bit, swbit_i.scmi_ch1_bit);
		swbit.scmi_ch2_bit = MAX(swbit_p.scmi_ch2_bit, swbit_i.scmi_ch2_bit);
		swbit.scmi_frame = MAX(swbit_p.scmi_frame, swbit_i.scmi_frame);

		swbit.ucm_ch0_bit = MAX(swbit_p.ucm_ch0_bit, swbit_i.ucm_ch0_bit);
		swbit.ucm_ch1_bit = MAX(swbit_p.ucm_ch1_bit, swbit_i.ucm_ch1_bit);
		swbit.ucm_ch2_bit = MAX(swbit_p.ucm_ch2_bit, swbit_i.ucm_ch2_bit);
		swbit.ucm_frame = MAX(swbit_p.ucm_frame, swbit_i.ucm_frame);

		swbit.znr_ch0_bit = MAX(swbit_p.znr_ch0_bit, swbit_i.znr_ch0_bit);
		swbit.znr_ch1_bit = MAX(swbit_p.znr_ch1_bit, swbit_i.znr_ch1_bit);
		swbit.znr_frame = MAX(swbit_p.znr_frame, swbit_i.znr_frame);

		swbit.abf_ch0_bit = MAX(swbit_p.abf_ch0_bit, swbit_i.abf_ch0_bit);
		swbit.abf_ch1_bit = MAX(swbit_p.abf_ch1_bit, swbit_i.abf_ch1_bit);
		swbit.abf_ch2_bit = MAX(swbit_p.abf_ch2_bit, swbit_i.abf_ch2_bit);
		swbit.abf_frame = MAX(swbit_p.abf_frame, swbit_i.abf_frame);

		swbit.mcdi_frame = MAX(swbit_p.mcdi_frame, swbit_i.mcdi_frame);
	}

	/* only legacy mode need this */
	pq_dev->scmi_protect_line = 0;
	if (pqcaps->u32Support_TS == 0)
		mtk_pq_buffer_hwmap_fixup(pq_dev, &swbit);

	_mtk_pq_buffer_debug_swbit(pq_dev, &swbit);
	for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
		struct pq_buffer *ppq_buf = &(pBufferTable[buf_idx]);

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "buf_idx, %d\n", buf_idx);

		ret = _mtk_pq_get_hwmap_info(ppq_buf, buf_idx, &swbit);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_get_hwmap_info Failed, ret = %d\n", ret);
			return ret;
		}
	}

	return ret;
}

int mtk_pq_buffer_allocate(struct mtk_pq_device *pq_dev)
{
	enum pqu_buffer_type buf_idx = PQU_BUF_SCMI;
	enum pqu_buffer_channel ch_idx = PQU_BUF_CH_0;
	struct pq_buffer *pBufferTable = NULL;
	struct meta_pq_stream_info *stream_pq = NULL;
	int ret = 0;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pBufferTable = pq_dev->display_info.BufferTable;
	stream_pq = &(pq_dev->stream_meta);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "stream.width: %d, stream.height: %d\n",
						stream_pq->width, stream_pq->height);

	for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
		struct pq_buffer *ppq_buf = &(pBufferTable[buf_idx]);
		if (pq_dev->path != V4L2_PQ_PATH_DIP && buf_idx == PQU_BUF_DIPDI)
			continue;
		if (ppq_buf->frame_num != 0) {
			if (ppq_buf->valid == true)
				_mtk_pq_release_buf(pq_dev, buf_idx, ppq_buf);

			ret = _mtk_pq_allocate_buf(pq_dev, buf_idx, ppq_buf, *stream_pq);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_allocate_buf Failed, ret = %d\n", ret);
				return ret;
			}

			for (ch_idx = PQU_BUF_CH_0; ch_idx < PQU_BUF_CH_MAX; ch_idx++) {
				if (ch_idx == PQU_BUF_CH_0)
					ppq_buf->addr_ch[ch_idx] = ppq_buf->addr;
				else
					ppq_buf->addr_ch[ch_idx] =
						ppq_buf->addr_ch[ch_idx-1] +
						ppq_buf->size_ch[ch_idx-1];
			}

			if ((buf_idx == PQU_BUF_SCMI) || (buf_idx == PQU_BUF_UCM))
				ppq_buf->sec_buf = true;

		} else {
			if (ppq_buf->valid == true)
				_mtk_pq_release_buf(pq_dev, buf_idx, ppq_buf);
		}
	}

	return ret;
}

int mtk_pq_buffer_release(struct mtk_pq_device *pq_dev)
{
	enum pqu_buffer_type buf_idx = PQU_BUF_SCMI;
	struct pq_buffer *pBufferTable = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pBufferTable = pq_dev->display_info.BufferTable;

	for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
		struct pq_buffer *ppq_buf = &(pBufferTable[buf_idx]);

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "buf_idx, %d\n", buf_idx);

		if (ppq_buf->valid == true) {
			if ((buf_idx == PQU_BUF_SCMI) || (buf_idx == PQU_BUF_UCM))
				ppq_buf->sec_buf = false;

			_mtk_pq_release_buf(pq_dev, buf_idx, ppq_buf);
		}
	}

	return 0;
}

int mtk_get_pq_buffer(struct mtk_pq_device *pq_dev, enum pqu_buffer_type buftype,
	struct pq_buffer *ppq_buf)
{
	int ret = 1;
	struct pq_buffer *pBufferTable = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pBufferTable = pq_dev->display_info.BufferTable;

	if (buftype >= PQU_BUF_MAX) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"buftype is not valid : %d\n", buftype);
		return -EPERM;
	}

	ppq_buf->valid = pBufferTable[buftype].valid;
	ppq_buf->size = pBufferTable[buftype].size;
	ppq_buf->frame_num = pBufferTable[buftype].frame_num;
	ppq_buf->frame_format = pBufferTable[buftype].frame_format;
	ppq_buf->frame_diff = pBufferTable[buftype].frame_diff;
	ppq_buf->frame_delay = pBufferTable[buftype].frame_delay;
	ppq_buf->va = pBufferTable[buftype].va;
	ppq_buf->addr = pBufferTable[buftype].addr;
	memcpy(ppq_buf->addr_ch, pBufferTable[buftype].addr_ch, sizeof(__u64) * PQU_BUF_CH_MAX);
	memcpy(ppq_buf->size_ch, pBufferTable[buftype].size_ch, sizeof(__u32) * PQU_BUF_CH_MAX);
	memcpy(ppq_buf->bit, pBufferTable[buftype].bit, sizeof(__u8) * PQU_BUF_CH_MAX);

	if (pq_dev->log_cd == 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
			"valid : %d, size : %u va : %llx, addr : %llx\n",
			ppq_buf->valid, ppq_buf->size, ppq_buf->va, ppq_buf->addr);
	}
	return ret;
}

int mtk_pq_get_cal_buffer_size(struct mtk_pq_device *pq_dev, enum pqu_buffer_type buf_type,
	struct pq_buffer *ppq_buf)
{
	int ret = 0;
	struct meta_pq_stream_info *stream_pq = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (ppq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	stream_pq = &(pq_dev->stream_meta);
	ret = _mtk_pq_cal_buf_size(ppq_buf, buf_type, *stream_pq);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		"_mtk_pq_cal_buf_size Failed, ret = %d\n", ret);
		return ret;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER, "buf_type = %d, size = %u",
		buf_type, ppq_buf->size);

	return ret;
}

int mtk_pq_buffer_get_extra_frame(struct mtk_pq_device *pq_dev, uint8_t *extra_frame)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	enum meta_pq_source pq_source = meta_pq_source_ipdma;
	bool interlace = false;
	bool low_latency = false;
	__u8 frame_delay = 0;
	__u8 tmp_extra = 0;
	__u8 vdec_extra = 0;
	bool bvdec_statistic_en = false;
	enum EN_PQ_CFD_HDR_MODE hdr_mode = E_PQ_CFD_HDR_MODE_SDR;

	if (pq_dev == NULL || extra_frame == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pq_source = pq_dev->stream_meta.pq_source;
	if (pq_source == meta_pq_source_b2r) {
		bvdec_statistic_en = pq_dev->stream_meta.bvdec_statistic_en;
		low_latency = pq_dev->stream_meta.low_latency;
		if (low_latency == false && bvdec_statistic_en == true) {

			frame_delay =
				pq_dev->display_info.BufferTable[PQU_BUF_SCMI].frame_delay +
				pq_dev->display_info.BufferTable[PQU_BUF_UCM].frame_delay;

			interlace = pq_dev->stream_meta.interlace;
			if (interlace == true) // i mode
				vdec_extra = pqdev->display_dev.extraframe_i;
			else
				vdec_extra = pqdev->display_dev.extraframe_p;

			if (frame_delay >= vdec_extra)
				tmp_extra = 0;
			else
				tmp_extra = vdec_extra - frame_delay;

			if (pq_dev->stream_meta.pq_scene == meta_pq_scene_force_p)
				tmp_extra = MAX(pqdev->display_dev.extraframe_i,
						pqdev->display_dev.extraframe_p);

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
				"VDEC histogram case %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
				"Window = ", pq_dev->dev_indx,
				"pq_source = ", pq_source,
				"low_latency = ", low_latency,
				"interlance = ", interlace,
				"vdec_extra = ", vdec_extra,
				"frame_delay = ", frame_delay,
				"bvdec_statistic_en = ", bvdec_statistic_en);
		}
	}

	hdr_mode = (enum EN_PQ_CFD_HDR_MODE)pq_dev->hdr_type;
	if (hdr_mode == E_PQ_CFD_HDR_MODE_DOLBY) {
		if (tmp_extra == 0)
			tmp_extra++;
	}

	*extra_frame = tmp_extra;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_BUFFER,
		"%s%d, %s%d, %s%d\n",
		"Window = ", pq_dev->dev_indx,
		"hdr_mode = ", hdr_mode,
		"extra_frame = ", *extra_frame);

	return 0;
}

int mtk_pq_buffer_frc_pq_allocate(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	struct pq_buffer_frc_pq *pBuffer_frc_pq = NULL;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pBuffer_frc_pq = &(pqdev->frc_dev.frc_pq_buffer);
	pqdev->frc_dev.dev = pqdev->dev;

	if (pBuffer_frc_pq->valid == true)
		return 0;

	ret = _mtk_pq_allocate_frc_pq_buf(&(pqdev->frc_dev), pBuffer_frc_pq);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_allocate_frc_pq_buf Failed, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

int mtk_pq_buffer_frc_pq_release(struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	struct pq_buffer_frc_pq *pBuffer_frc_pq = NULL;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	pBuffer_frc_pq = &(pqdev->frc_dev.frc_pq_buffer);
	pqdev->frc_dev.dev = pqdev->dev;

	if (pBuffer_frc_pq->valid == true) {
		ret = _mtk_pq_release_frc_pq_buf(&(pqdev->frc_dev), pBuffer_frc_pq);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_release_frc_pq_buf Failed, ret = %d\n", ret);
			return ret;
		}
	}
	return 0;
}

