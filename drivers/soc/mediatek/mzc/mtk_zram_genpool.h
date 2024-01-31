/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_ZRAM_DMAHEAP_H___
#define __MTK_ZRAM_DMAHEAP_H___

extern bool is_carv_exist(void);
extern bool is_carv_range(struct page *page);
extern void free_carv_page(struct page *page);
extern struct page *alloc_carv_page(void);
#endif
