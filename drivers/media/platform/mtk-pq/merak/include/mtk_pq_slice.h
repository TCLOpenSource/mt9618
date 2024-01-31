/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_SLICE_H
#define _MTK_PQ_SLICE_H

#include <linux/dma-fence.h>

#define PQ_SLICE_MODE_TEST (1)
#define PQ_SLICE_MODE_SKIP (2)

struct dma_fence *mtk_pq_slice_create_fence(int fence_count);
void mtk_pq_slice_fence_signal(struct dma_fence *fence, int index);
void mtk_pq_slice_set_dbg_mode(__u8 slice_dbg_mode);
__u8 mtk_pq_slice_get_dbg_mode(void);

#endif
