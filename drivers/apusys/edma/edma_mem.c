// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
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

#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/ktime.h>
#include <comm_driver.h>
#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "edma_queue.h"
#include "edma_api.h"
#include "edma_dbgfs.h"

#define	EDMA_EXT_MODE_SIZE		0x60
#define MTK_IOVA_START_ADDR 0x200000000

static inline void edma_fill_kmem(u32 iova, u32 size, u8 iommu, struct comm_kmem *mem)
{
	if (mem) {
		memset(mem, 0, sizeof(*mem));

		mem->iova = iova;
		mem->size = size;

		if (iommu == 1) {
			mem->iova |= MTK_IOVA_START_ADDR;
			mem->mem_type = APU_COMM_MEM_DRAM_DMA;
		} else {
			mem->mem_type = APU_COMM_MEM_VLM;
		}
	}
}

void *edma_map_buf(void *user, u32 iova, u32 size, u8 iommu)
{
	struct comm_kmem mem;
	struct comm_user *u = (struct comm_user *)user;

	if (!IS_ALIGNED(iova, 4))
		return NULL;

	edma_fill_kmem(iova, size, iommu, &mem);

	if (iommu == 0) {
		if (comm_util_get_cb()->query_mem(&mem, COMM_QUERY_BY_IOVA, NULL))
			return NULL;

		return (void *)(uintptr_t)mem.kva;
	}

	if (!u) {
		if (comm_util_get_cb()->query_mem(&mem, COMM_QUERY_BY_IOVA, NULL))
			return NULL;
	} else {
		if (comm_util_get_cb()->get_user_mem(user, &mem))
			return NULL;
	}

	return (void *)(uintptr_t)mem.kva;
}

void edma_unmap_buf(void *user, u32 iova, u32 size, u8 iommu)
{
	struct comm_kmem mem;
	struct comm_user *u = (struct comm_user *)user;

	if ((iommu == 0) || (!u))
		return;

	if (!IS_ALIGNED(iova, 4))
		return;

	edma_fill_kmem(iova, size, iommu, &mem);

	if (comm_util_get_cb()->put_user_mem(u, &mem))
		return;
}

static int edma_check_user_cb(struct comm_kmem *mm)
{

	if (mm) {
		if (mm->tgid == current->tgid)
			return 0;
	}

	return -EFAULT;
}

static int edma_mem_to_user(struct comm_user **user,
			u32 iova,
			u32 size,
			u8 iommu,
			u32 ioctl)
{
	struct comm_kmem mem;
	struct comm_user *u;
	int ret;

	/* bypass comm_user check if req come from apusys */
	if (!ioctl) {
		*user = NULL;
		return 0;
	}

	edma_fill_kmem(iova, size, iommu, &mem);
	ret = comm_util_get_cb()->search_user_mem_foreach(&u,
							&mem,
							COMM_CMP_IOVA,
							edma_check_user_cb);
	if (ret) {
		LOG_DBG("%s(): fail, iova:%x size:%d\n",
			__func__, iova, size);
		return -EFAULT;
	}

	*user = u;

	return 0;
}

int edma_map_ext_mode_subcmd(void *user, void *kva, u32 count, u32 iommu)
{
	u32 *p;
	void *res;
	u32 src_size, dst_size;
	u32 src_mva, dst_mva;
	int i, exit_count;

	if (!user)
		return 0;

	// check subcmd on ioctl ext_mode path
	for (i = 0; i < count; i++) {
		p = (u32 *)(uintptr_t)(kva + EDMA_EXT_MODE_SIZE * i);
		src_size = ((p[0] & 0xffff) + 1) * ((p[0] >> 16 & 0xFFFF) + 1) * (p[1] + 1);
		dst_size = ((p[6] & 0xffff) + 1) * ((p[6] >> 16 & 0xFFFF) + 1) * (p[1] + 1);
		src_mva = p[11];
		dst_mva = p[13];

		if (src_mva == dst_mva)
			goto check_fail;

		// [TODO] check size of src/dst buffer size
		res = edma_map_buf(user, src_mva, src_size, iommu);
		if (res == NULL)
			goto check_fail;

		res = edma_map_buf(user, dst_mva, dst_size, iommu);
		if (res == NULL) {
			edma_unmap_buf(user, src_mva, src_size, iommu);
			goto check_fail;
		}

	}

	return 0;

check_fail:
	LOG_DBG("%s(): subcmd[%x] fail, src:%x dst:%x\n",
		__func__, i, src_mva, dst_mva);

	exit_count = i;
	for (i = 0; i < exit_count; i++) {
		p = (u32 *)(uintptr_t)(kva + EDMA_EXT_MODE_SIZE * i);
		src_size = ((p[0] & 0xffff) + 1) * ((p[0] >> 16 & 0xFFFF) + 1) * (p[1] + 1);
		dst_size = ((p[6] & 0xffff) + 1) * ((p[6] >> 16 & 0xFFFF) + 1) * (p[1] + 1);
		src_mva = p[11];
		dst_mva = p[13];

		edma_unmap_buf(user, src_mva, src_size, iommu);
		edma_unmap_buf(user, dst_mva, dst_size, iommu);
	}

	return -EFAULT;
}

void edma_unmap_ext_mode_subcmd(void *user, void *kva, u32 count, u32 iommu)
{
	u32 *p;
	u32 src_mva, dst_mva;
	int i;

	if (!user)
		return;

	for (i = 0; i < count; i++) {
		p = (u32 *)(uintptr_t)(kva + EDMA_EXT_MODE_SIZE * i);
		src_mva = ((p[0] & 0xffff) + 1) * ((p[0] >> 16 & 0xFFFF) + 1) * (p[1] + 1);
		dst_mva = ((p[6] & 0xffff) + 1) * ((p[6] >> 16 & 0xFFFF) + 1) * (p[1] + 1);

		edma_unmap_buf(user, p[11], src_mva, iommu);
		edma_unmap_buf(user, p[13], dst_mva, iommu);
	}
}

int edma_map_ext_mode(struct edma_request *req, u32 ioctl)
{
	struct comm_user *u;
	int ret;

	if (req->ext_reg_addr && req->ext_count) {
		ret = edma_mem_to_user(&u, req->ext_reg_addr,
				(req->ext_count) * EDMA_EXT_MODE_SIZE,
				req->desp_iommu_en,
				ioctl);
		if (ret)
			return ret;

		req->data = edma_map_buf(u,
				req->ext_reg_addr,
				(req->ext_count) * EDMA_EXT_MODE_SIZE,
				req->desp_iommu_en);
		if (req->data == NULL)
			return -EINVAL;

		if (edma_map_ext_mode_subcmd(u,
					req->data,
					req->ext_count,
					req->desp_iommu_en)) {
			edma_unmap_buf(u,
				req->ext_reg_addr,
				(req->ext_count) * EDMA_EXT_MODE_SIZE,
				req->desp_iommu_en);
			return -EINVAL;

		}
	} else
		return -EINVAL;

	return 0;
}

void edma_unmap_ext_mode(struct edma_request *req, u32 ioctl)
{
	struct comm_user *u;
	int ret;

	if (req->ext_reg_addr && req->ext_count) {
		ret = edma_mem_to_user(&u, req->ext_reg_addr,
				(req->ext_count) * EDMA_EXT_MODE_SIZE,
				req->desp_iommu_en,
				ioctl);
		if (ret)
			return;

		if (req->data == NULL)
			return;

		edma_unmap_ext_mode_subcmd(u,
					req->data,
					req->ext_count,
					req->desp_iommu_en);
		edma_unmap_buf(u,
				req->ext_reg_addr,
				(req->ext_count) * EDMA_EXT_MODE_SIZE,
				req->desp_iommu_en);
	}
}

#define CHECK_SRC	BIT(0)
#define CHECK_DST	BIT(1)
#define CHECK_SRC_UV	BIT(2)
#define CHECK_DST_UV	BIT(3)

int edma_map_internal_mode(struct edma_request *req, u32 flags)
{
	void *kva;
	struct comm_user *u;
	int ret;

	ret = edma_mem_to_user(&u,
			req->desp.dst_addr,
			req->desp.dst_tile_channel * req->desp.dst_tile_width,
			req->desp_iommu_en, 1);
	if (ret)
		return ret;

	if (flags & CHECK_SRC) {
		kva = edma_map_buf(u,
				req->desp.src_addr,
				req->desp.src_tile_channel * req->desp.src_tile_width,
				req->desp_iommu_en);
		if (kva == NULL)
			goto unmap_src_addr;
	}

	if (flags & CHECK_DST) {
		kva = edma_map_buf(u,
				req->desp.dst_addr,
				req->desp.dst_tile_channel * req->desp.dst_tile_width,
				req->desp_iommu_en);
		if (kva == NULL)
			goto unmap_dst_addr;
	}

	if (flags & CHECK_SRC_UV) {
		kva = edma_map_buf(u,
				req->desp.src_uv_addr,
				req->desp.src_tile_channel * req->desp.src_tile_width,
				req->desp_iommu_en);
		if (kva == NULL)
			goto unmap_src_uv_addr;
	}

	if (flags & CHECK_DST_UV) {
		kva = edma_map_buf(u,
				req->desp.dst_uv_addr,
				req->desp.dst_tile_channel * req->desp.dst_tile_width,
				req->desp_iommu_en);
		if (kva == NULL)
			goto unmap_dst_uv_addr;
	}

	return 0;

unmap_dst_uv_addr:
	if (flags & CHECK_DST_UV) {
		edma_unmap_buf(u,
			req->desp.dst_uv_addr,
			req->desp.dst_tile_channel * req->desp.dst_tile_width,
			req->desp_iommu_en);

	}
unmap_src_uv_addr:
	if (flags & CHECK_SRC_UV) {
		edma_unmap_buf(u,
			req->desp.src_uv_addr,
			req->desp.src_tile_channel * req->desp.src_tile_width,
			req->desp_iommu_en);

	}
unmap_dst_addr:
	if (flags & CHECK_DST) {
		edma_unmap_buf(u,
			req->desp.dst_addr,
			req->desp.dst_tile_channel * req->desp.dst_tile_width,
			req->desp_iommu_en);

	}
unmap_src_addr:
	if (flags & CHECK_SRC) {
		edma_unmap_buf(u,
			req->desp.src_addr,
			req->desp.src_tile_channel * req->desp.src_tile_width,
			req->desp_iommu_en);
	}

	return -EINVAL;
}

void edma_unmap_internal_mode(struct edma_request *req, u32 flags)
{
	struct comm_user *u;
	int ret;

	ret = edma_mem_to_user(&u,
			req->desp.src_addr,
			req->desp.src_tile_channel * req->desp.src_tile_width,
			req->desp_iommu_en, 1);
	if (ret)
		return;

	if (flags & CHECK_DST_UV) {
		edma_unmap_buf(u,
			req->desp.dst_uv_addr,
			req->desp.dst_tile_channel * req->desp.dst_tile_width,
			req->desp_iommu_en);

	}

	if (flags & CHECK_SRC_UV) {
		edma_unmap_buf(u,
			req->desp.src_uv_addr,
			req->desp.src_tile_channel * req->desp.src_tile_width,
			req->desp_iommu_en);

	}

	if (flags & CHECK_DST) {
		edma_unmap_buf(u,
			req->desp.dst_addr,
			req->desp.dst_tile_channel * req->desp.dst_tile_width,
			req->desp_iommu_en);

	}

	if (flags & CHECK_SRC) {
		edma_unmap_buf(u,
			req->desp.src_addr,
			req->desp.src_tile_channel * req->desp.src_tile_width,
			req->desp_iommu_en);
	}
}

int edma_lock_buf(struct edma_request *req, u32 ioctl)
{
	int ret = 0;

	switch (req->cmd) {
	case EDMA_PROC_EXT_MODE:
		ret = edma_map_ext_mode(req, ioctl);
		break;
	case EDMA_PROC_FILL:
		ret = edma_map_internal_mode(req, CHECK_DST);
		break;
	case EDMA_PROC_NORMAL:
		ret = edma_map_internal_mode(req, CHECK_DST | CHECK_SRC);
		break;
	case EDMA_PROC_FORMAT:
	case EDMA_PROC_COMPRESS:
	case EDMA_PROC_DECOMPRESS:
		ret = edma_map_internal_mode(req, CHECK_DST | CHECK_SRC |
						CHECK_DST_UV | CHECK_SRC_UV);
		break;
	case EDMA_PROC_RAW:
		ret = edma_map_internal_mode(req, CHECK_DST | CHECK_SRC | CHECK_SRC_UV);
		break;
	default:
		LOG_DBG("%s: bad cmd %x\n", __func__, req->cmd);
		ret = -EINVAL;
	}

	if (ret) {
		LOG_DBG("%s():fail, (%#x/%#x/%#x/%#x/%#x)\n", __func__,
							req->cmd,
							req->ext_reg_addr,
							req->ext_count,
							req->buf_iommu_en,
							req->desp_iommu_en);
	}

	return ret;
}

void edma_unlock_buf(struct edma_request *req, u32 ioctl)
{
	switch (req->cmd) {
	case EDMA_PROC_EXT_MODE:
		edma_unmap_ext_mode(req, ioctl);
		break;
	case EDMA_PROC_FILL:
		edma_unmap_internal_mode(req, CHECK_DST);
		break;
	case EDMA_PROC_NORMAL:
		edma_unmap_internal_mode(req, CHECK_DST | CHECK_SRC);
		break;
	case EDMA_PROC_FORMAT:
	case EDMA_PROC_COMPRESS:
	case EDMA_PROC_DECOMPRESS:
		edma_unmap_internal_mode(req, CHECK_DST | CHECK_SRC |
						CHECK_DST_UV | CHECK_SRC_UV);
		break;
	case EDMA_PROC_RAW:
		edma_unmap_internal_mode(req, CHECK_DST | CHECK_SRC | CHECK_SRC_UV);
		break;
	default:
		LOG_DBG("%s: bad cmd %x\n", __func__, req->cmd);
	}
}
