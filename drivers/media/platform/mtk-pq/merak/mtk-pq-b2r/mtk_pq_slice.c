// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "mtk_pq.h"
#include "mtk_pq_slice.h"

#include <linux/dma-resv.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence.h>

#include <linux/platform_device.h>

#define SLICE_FENCE_NAME_LEN (32)

static const struct dma_fence_ops mtk_pq_slice_fence_ops;
static __u8 g_slice_dbg_mode;

struct mtk_pq_slice_fence {
	struct dma_fence fence;
	spinlock_t lock;
	char timeline_name[SLICE_FENCE_NAME_LEN];
};

static struct mtk_pq_slice_fence *_mtk_pq_slice_to_mtk_fence(struct dma_fence *fence)
{
	if (fence->ops != &mtk_pq_slice_fence_ops) {
		WARN_ON(1);
		return NULL;
	}
	return container_of(fence, struct mtk_pq_slice_fence, fence);
}

static const char *_mtk_pq_slice_fence_get_driver_name(struct dma_fence *fence)
{
	return "mtk-pq-slice-fence";
}

static const char *_mtk_pq_slice_fence_get_timeline_name(struct dma_fence *fence)
{
	struct mtk_pq_slice_fence *mtk_fence = _mtk_pq_slice_to_mtk_fence(fence);

	return mtk_fence->timeline_name;
}

static void _mtk_pq_slice_fence_release(struct dma_fence *f)
{
	struct mtk_pq_slice_fence *mtk_fence = _mtk_pq_slice_to_mtk_fence(f);

	/* Unconditionally signal the fence. The process is getting
	 * terminated.
	 */
	if (WARN_ON(!mtk_fence))
		return; /* Not an mtk_pq_slice_fence */

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "release fence %s\n", mtk_fence->timeline_name);
	kfree_rcu(f, rcu);
}

static const struct dma_fence_ops mtk_pq_slice_fence_ops = {
	.get_driver_name = _mtk_pq_slice_fence_get_driver_name,
	.get_timeline_name = _mtk_pq_slice_fence_get_timeline_name,
	.release = _mtk_pq_slice_fence_release,
};

static bool _mtk_is_pq_fence(struct dma_fence *fence)
{
	return fence->ops == &mtk_pq_slice_fence_ops;
}

void mtk_pq_slice_fence_signal(struct dma_fence *fence, int index)
{
	struct dma_fence_array *fence_array;
	struct dma_fence *slice_fence;

	if (!fence) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "fence is NULL\n");
		return;
	}

	fence_array = to_dma_fence_array(fence);
	if (!fence_array) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "fence %s is not fence array\n",
			fence->ops->get_timeline_name(fence));
		return;
	}

	if (index >= fence_array->num_fences) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "max slice done count %d already reached\n",
			fence_array->num_fences);
		return;
	}

	slice_fence = fence_array->fences[index];
	if (!_mtk_is_pq_fence(slice_fence)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "fence %s at index %d is not a mtk_pq fence\n",
			slice_fence->ops->get_timeline_name(slice_fence), index);
		return;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "signal index %d fence %s\n",
		index, slice_fence->ops->get_timeline_name(slice_fence));
	dma_fence_signal(slice_fence);
}

struct dma_fence *mtk_pq_slice_create_fence(int fence_count)
{
	struct dma_fence_array *fence_array = NULL;
	struct dma_fence **fences = NULL;
	u64 context;
	int i, n;

	// no need to use fence
	if (fence_count <= 0)
		goto CREATE_FENCE_FAIL;

	fences = kcalloc(fence_count, sizeof(*fences), GFP_KERNEL);
	if (fences == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "alloc %d fences fail\n", fence_count);
		goto CREATE_FENCE_FAIL;
	} else {
		for (i = 0; i < fence_count; i++)
			fences[i] = NULL;
	}
	context = dma_fence_context_alloc(1);
	for (i = 0; i < fence_count; i++) {
		struct mtk_pq_slice_fence *mtk_fence;

		mtk_fence = kzalloc(sizeof(*mtk_fence), GFP_KERNEL);
		if (mtk_fence == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "alloc fence index %d fail\n", i);
			goto CREATE_FENCE_FAIL;
		}
		spin_lock_init(&mtk_fence->lock);
		n = snprintf(mtk_fence->timeline_name, sizeof(mtk_fence->timeline_name),
			"mtk_pq_slice_fence%lld-%d", context, i);
		if (!(n > -1 && n < sizeof(mtk_fence->timeline_name))) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf fence name fail: %d\n", n);
			kfree(mtk_fence);
			goto CREATE_FENCE_FAIL;
		}
		dma_fence_init(&mtk_fence->fence, &mtk_pq_slice_fence_ops,
			&mtk_fence->lock, context, 0);
		fences[i] = &mtk_fence->fence;
	}

	fence_array = dma_fence_array_create(fence_count, fences, context, 0, false);
	if (fence_array == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "create fences array fail\n");
		goto CREATE_FENCE_FAIL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "create fence%lld slice count %d\n", context, fence_count);

	return &fence_array->base;

CREATE_FENCE_FAIL:

	if (fences) {
		for (i = 0; i < fence_count; i++) {
			struct mtk_pq_slice_fence *mtk_fence =
				container_of(fences[i], struct mtk_pq_slice_fence, fence);

			dma_fence_free(fences[i]);

			kfree(mtk_fence);
		}
		kfree(fences);
	}

	return NULL;
}

void mtk_pq_slice_set_dbg_mode(__u8 slice_dbg_mode)
{
	g_slice_dbg_mode = slice_dbg_mode;
}

__u8 mtk_pq_slice_get_dbg_mode(void)
{
	return g_slice_dbg_mode;
}

