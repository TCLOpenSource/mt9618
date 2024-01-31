/* SPDX-License-Identifier: GPL-2.0*/
#ifndef __EDMA_MEM_H__
#define __EDMA_MEM_H__

#include <linux/interrupt.h>
#include "edma_dbgfs.h"
#include "edma_cmd_hnd.h"

void *edma_map_buf(void *user, u32 iova, u32 size, u8 iommu);
void edma_unmap_buf(void *user, u32 iova, u32 size, u8 iommu);
int edma_lock_buf(struct edma_request *req, u32 ioctl);
void edma_unlock_buf(struct edma_request *req, u32 ioctl);

#endif
