// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Joe Liu <joe.liu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-direct.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/buffer_head.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/of_address.h>
#include <linux/sched/signal.h>
#include <linux/device.h>
#include <linux/gfp.h>

#include "mtk-cma.h"
#include "cma.h"

#define to_mtkcma_heap(x) container_of(x, struct mtkcma_ion_cma_heap, heap)
#define CMA_RETRY_TIMEOUT_MS 8000
#define NULL_MAPPING_ADDRESS 0xDEADDADD

static LIST_HEAD(mtkcma_ion_cma_heap_device_list);
static DEFINE_MUTEX(mtkcma_ion_cma_heap_device_mutex);
static DEFINE_MUTEX(mtkcma_cpu_bus_base_mutex);

static u32 mtkcma_cpu_bus_base = -1;

struct mtkcma_ion_cma_heap {
	struct ion_heap heap;
	struct cma *cma;
	struct device *owner_device;
	struct list_head list;
};

static inline struct cma *dev_get_cma_off_area(struct device *dev)
{
	if (dev && dev->cma_area)
		return dev->cma_area;
	return NULL;
}

static int count_cma_area_free_page_num(struct cma *counted_cma)
{
	int count_cma_free_page_count       = 0;
	int count_cma_free_start			= 0;
	int count_cma_bitmap_start_zero		= 0;
	int count_cma_bitmap_end_zero		= 0;

	pr_debug("cma_area having ");
	for (;;) {
		count_cma_bitmap_start_zero = find_next_zero_bit(
			counted_cma->bitmap, counted_cma->count,
				count_cma_free_start);

		if (count_cma_bitmap_start_zero >= counted_cma->count)
			break;

		count_cma_free_start = count_cma_bitmap_start_zero + 1;
		count_cma_bitmap_end_zero = find_next_bit(
			counted_cma->bitmap, counted_cma->count,
				count_cma_free_start);

		if (count_cma_bitmap_end_zero >= counted_cma->count) {
			count_cma_free_page_count +=
			(counted_cma->count - count_cma_bitmap_start_zero);
			break;
		}

		count_cma_free_page_count +=
		(count_cma_bitmap_end_zero - count_cma_bitmap_start_zero);

		count_cma_free_start = count_cma_bitmap_end_zero + 1;

		if (count_cma_free_start >= counted_cma->count)
			break;
	}
	pr_debug("%d free pages\n", count_cma_free_page_count);

	return count_cma_free_page_count;
}

static unsigned long cma_bitmap_pages_to_bits(const struct cma *cma,
						unsigned long pages)
{
	return ALIGN(pages, 1UL << cma->order_per_bit) >> cma->order_per_bit;
}

static void cma_clear_bitmap(struct cma *cma, unsigned long pfn,
			unsigned int count)
{
	unsigned long bitmap_no, bitmap_count;

	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	mutex_lock(&cma->lock);
	bitmap_clear(cma->bitmap, bitmap_no, bitmap_count);
	mutex_unlock(&cma->lock);
}

struct page *dma_alloc_at_from_contiguous(struct cma *cma, unsigned long count,
					unsigned int align, phys_addr_t at_addr)
{
	unsigned long mask, pfn, pageno, start = 0;
	struct page *page = NULL;
	int ret;
	size_t i;
	unsigned long timeout;
	unsigned long start_pfn = __phys_to_pfn(at_addr);

	pr_debug("start_pfn is 0x%lX, count is %lu\n",
			start_pfn, count);

	if (!cma || !cma->count || !count || (count > cma->count))
		return NULL;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	mask = (1 << align) - 1;

	if (start_pfn && start_pfn < cma->base_pfn)
		return NULL;
	start = start_pfn ? start_pfn - cma->base_pfn : start;

	pr_debug("%s(alloc %lu pages, at_addr %pap, start pfn 0x%lX)\n",
		__func__, count, &at_addr, start_pfn);
	pr_debug("Function = %s, Line = %d, find bit_map from 0x%lX\n",
		__PRETTY_FUNCTION__, __LINE__, start);
	pr_debug("[%s] Before %s \033[m", current->comm,
		__PRETTY_FUNCTION__);
	mutex_lock(&cma->lock);
	count_cma_area_free_page_num(cma);
	mutex_unlock(&cma->lock);

	mutex_lock(&cma->lock);
	timeout = jiffies + msecs_to_jiffies(CMA_RETRY_TIMEOUT_MS);

	pageno = bitmap_find_next_zero_area(cma->bitmap, cma->count,
					    start, count, mask);

	/* we want to force the allocation start pfn */
	if (pageno >= cma->count || (start_pfn && start != pageno)) {
		mutex_unlock(&cma->lock);
		goto alloc_finished;
	}
	bitmap_set(cma->bitmap, pageno, count);
	/*
	 * It's safe to drop the lock here. We've marked this region for
	 * our exclusive use. If the migration fails we will take the
	 * lock again and unmark it.
	 */
	mutex_unlock(&cma->lock);

	pfn = cma->base_pfn + pageno;
retry:
	ret = alloc_contig_range(pfn, pfn + count,
					MIGRATE_CMA, GFP_KERNEL);
	if (ret == 0) {
		page = pfn_to_page(pfn);
	} else if (fatal_signal_pending(current)) {
		pr_err("Function = %s, cannot get cma_memory, get fatal_signal_pending\n",
			__PRETTY_FUNCTION__);
		pr_err("Function = %s, ret is %d\n",
			__PRETTY_FUNCTION__, ret);
		cma_clear_bitmap(cma, pfn, count);
	} else if (start_pfn && time_before(jiffies, timeout)) {
		pr_err("Function = %s, cannot get cma_memory, step_1\n",
					__PRETTY_FUNCTION__);
		pr_err("Function = %s, start_pfn is 0x%lX\n",
					__PRETTY_FUNCTION__, start_pfn);
		cond_resched();
		invalidate_bh_lrus();
		goto retry;
	} else if (ret != -EBUSY || start_pfn) {
		pr_err("Function = %s, cannot get cma_memory, step_2\n",
					__PRETTY_FUNCTION__);
		pr_err("Function = %s, ret is %d\n",
					__PRETTY_FUNCTION__, ret);
		cma_clear_bitmap(cma, pfn, count);
	} else {
		pr_err("Function = %s, cannot get cma_memory, step_3\n",
					__PRETTY_FUNCTION__);
		pr_err("Function = %s, ret is %d\n",
					__PRETTY_FUNCTION__, ret);
		cma_clear_bitmap(cma, pfn, count);
	}
alloc_finished:
	/*
	 * CMA can allocate multiple page blocks, which results in different
	 * blocks being marked with different tags. Reset the tags to ignore
	 * those page blocks.
	 */
	if (page) {
		for (i = 0; i < count; i++)
			page_kasan_tag_reset(page + i);
	}

	pr_debug("%s(): returned %p\n", __func__, page);

	pr_debug("[%s] After %s ",
		current->comm, __PRETTY_FUNCTION__);
	mutex_lock(&cma->lock);
	count_cma_area_free_page_num(cma);
	mutex_unlock(&cma->lock);

	return page;
}
EXPORT_SYMBOL(dma_alloc_at_from_contiguous);

static int mtkcma_ion_cma_allocate(struct ion_heap *heap,
			struct ion_buffer *buffer,
			unsigned long len, unsigned long flags)
{
	struct mtkcma_ion_cma_heap *cma_heap = to_mtkcma_heap(heap);
	struct sg_table *table;
	struct page *pages;
	phys_addr_t start_addr;
	int ret;
	phys_addr_t end_addr;
	u64 miup_miu_addr;
	u64 miup_miu_size;
	bool do_miu_protect;
	unsigned long size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);

	start_addr = cma_heap->cma->base_pfn + (flags >> PAGE_SHIFT);
	start_addr = start_addr << PAGE_SHIFT;
	end_addr = start_addr + (nr_pages << PAGE_SHIFT);

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	if (strstr(cma_get_name(cma_heap->cma), "default_cma") != NULL) {
		pages = cma_alloc(cma_heap->cma, nr_pages, 0, 1);
		do_miu_protect = false;
	} else {
		pages = dma_alloc_at_from_contiguous(cma_heap->cma,
					nr_pages, 0, start_addr);
		do_miu_protect = true;
	}

	if (!pages) {
		pr_emerg("Function = %s, %s no page\n",
					__PRETTY_FUNCTION__, heap->name);
		return -ENOMEM;
	}

	if (PageHighMem(pages)) {
		unsigned long nr_clear_pages = nr_pages;
		struct page *page = pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(pages), 0, size);
	}

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_mem;

	sg_set_page(table->sgl, pages, size, 0);

	buffer->priv_virt = pages;
	buffer->sg_table = table;

	/* remove miu protect with miu_address*/
	if (do_miu_protect) {
		miup_miu_addr = (u64)(page_to_phys(pages) - mtkcma_cpu_bus_base);
		miup_miu_size = (u64)(nr_pages << PAGE_SHIFT);
		mtk_miup_req_kprot(miup_miu_addr, miup_miu_size);
	}

	return 0;

free_mem:
	kfree(table);
err:
	cma_release(cma_heap->cma, pages, nr_pages);
	return -ENOMEM;
}

static void mtkcma_ion_cma_free(struct ion_buffer *buffer)
{
	u64 miup_miu_addr;
	u64 miup_miu_size;
	bool do_miu_protect;
	struct mtkcma_ion_cma_heap *cma_heap = to_mtkcma_heap(buffer->heap);
	struct page *pages = buffer->priv_virt;
	unsigned long nr_pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;

	if (strstr(cma_get_name(cma_heap->cma), "default_cma") != NULL)
		do_miu_protect = false;
	else
		do_miu_protect = true;

	/* add miu protect with miu_address*/
	if (do_miu_protect) {
		miup_miu_addr = (u64)(page_to_phys(pages) - mtkcma_cpu_bus_base);
		miup_miu_size = (u64)(nr_pages << PAGE_SHIFT);
		mtk_miup_rel_kprot(miup_miu_addr, miup_miu_size);
	}

	cma_release(cma_heap->cma, pages, nr_pages);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static struct ion_heap_ops mtkcma_ion_cma_ops = {
	.allocate = mtkcma_ion_cma_allocate,
	.free = mtkcma_ion_cma_free,
};

static void *mtkcma_dma_alloc(struct device *dev, size_t size,
			dma_addr_t *dma_handle,
			gfp_t flags, unsigned long attrs)
{
	struct page *pages;
	void *ret;
	phys_addr_t start_addr, end_addr;
	struct cma *cma;
	u64 miup_miu_addr;
	u64 miup_miu_size;
	unsigned long nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;
	bool do_miu_protect;

	cma = dev_get_cma_off_area(dev);
	if (!cma) {
		pr_emerg("%s(%d): get cma failed!\n", __func__, __LINE__);
		return ERR_PTR(-EFAULT);
	}

	start_addr = cma->base_pfn + (*dma_handle >> PAGE_SHIFT);
	start_addr = start_addr << PAGE_SHIFT;
	end_addr = start_addr + (nr_pages << PAGE_SHIFT);

	pr_info("dma: allocation by device: %s\n", dev_name(dev));

	if (attrs & 1 << IOMMU_CMA_ALLOC_BIT ||
		strstr(cma_get_name(cma), "default_cma") != NULL) {
		pages = cma_alloc(cma, nr_pages, 0, 1);
		do_miu_protect = false;
	}
	else {
		pr_info("from %pap to %pap\n", &start_addr, &end_addr);
		pages = dma_alloc_at_from_contiguous(cma, nr_pages, 0, start_addr);
		do_miu_protect = true;
	}

	if (!pages) {
		pr_emerg("Function = %s, no page\n", __PRETTY_FUNCTION__);
		*dma_handle = 0;
		return ERR_PTR(-ENOMEM);
	}

	*dma_handle = phys_to_dma(dev, page_to_phys(pages));
	if (!PageHighMem(pages))
		ret = page_address(pages);
	else
		ret = (void *)NULL_MAPPING_ADDRESS;
	pr_info("result: *dma_handle is 0x%llX\n", *dma_handle);
	pr_info("result: ret_va is %p\n", ret);

	/* remove miu protect with miu_address */
	if (do_miu_protect) {
		miup_miu_addr = (u64)(page_to_phys(pages) - mtkcma_cpu_bus_base);
		miup_miu_size = (u64)(nr_pages << PAGE_SHIFT);
		mtk_miup_req_kprot(miup_miu_addr, miup_miu_size);
	}

	return ret;
}

static void mtkcma_dma_free(struct device *dev, size_t size,
			void *vaddr, dma_addr_t dma_handle,
			unsigned long attrs)
{
	u64 miup_miu_addr;
	u64 miup_miu_size;
	phys_addr_t cma_phys_addr;
	unsigned long pfn;
	struct cma *cma;
	struct page *pages;
	bool do_miu_protect;

	pr_info("dma: free by device: %s\n", dev_name(dev));
	pr_info("from 0x%llX to 0x%llX\n", (u64)dma_handle, (u64)(dma_handle + size));
	pr_info("va is %p\n", vaddr);

	cma_phys_addr = dma_to_phys(dev, dma_handle);
	pfn = __phys_to_pfn(cma_phys_addr);
	cma = dev_get_cma_off_area(dev);
	pages = virt_to_page(phys_to_virt(cma_phys_addr));

	if (!cma || !pages) {
		pr_emerg("can not get cma_area or can not get freed_page, do not free\n");
		return;
	}

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return;

	if (strstr(cma_get_name(cma), "default_cma") != NULL)
		do_miu_protect = false;
	else
		do_miu_protect = true;

	/* add miu protect with miu_address*/
	if (do_miu_protect) {
		miup_miu_addr = (u64)(cma_phys_addr - mtkcma_cpu_bus_base);
		miup_miu_size = (u64)(PAGE_ALIGN(size));
		mtk_miup_rel_kprot(miup_miu_addr, miup_miu_size);
	}

	cma_release(cma, pages, PAGE_ALIGN(size) >> PAGE_SHIFT);
}

static void mtkcma_dma_sync_single_for_cpu(struct device *dev, dma_addr_t bus_addr,
				size_t size, enum dma_data_direction dir)
{
	dma_direct_sync_single_for_cpu(dev, bus_addr, size, dir);
}

static void mtkcma_dma_sync_single_for_device(struct device *dev,
				dma_addr_t bus_addr, size_t size, enum dma_data_direction dir)
{
	dma_direct_sync_single_for_device(dev, bus_addr, size, dir);
}

static const struct dma_map_ops mtkcma_dma_ops = {
	.alloc = mtkcma_dma_alloc,
	.free = mtkcma_dma_free,
	.sync_single_for_cpu = mtkcma_dma_sync_single_for_cpu,
	.sync_single_for_device = mtkcma_dma_sync_single_for_device,
};

static int mtk_cma_device_initialize(struct device *cma_device)
{
	struct ion_heap *heap;
	struct mtkcma_ion_cma_heap *cma_heap;
	struct cma *cma;

	if (!cma_device->cma_area) {
		pr_emerg("Function = %s, cma_device has no cma_area\n",
				__PRETTY_FUNCTION__);
		return -EINVAL;
	}

	pr_info("        for: device %s, cma: %s\n",
			dev_name(cma_device), cma_device->cma_area->name);
	cma = cma_device->cma_area;
	show_cma_info(cma);

	/* copy from __ion_add_cma_heaps(cma) */
	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return -ENOMEM;

	cma_heap->owner_device = cma_device;
	cma_heap->heap.ops = &mtkcma_ion_cma_ops;

	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->cma = cma;
	cma_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	heap = &cma_heap->heap;

	heap->name = dev_name(cma_device);

	pr_info("        add mtkcma ion_heap with name %s\n", heap->name);
	ion_device_add_heap(heap);

	set_dma_ops(cma_device, &mtkcma_dma_ops);

	cma_device->coherent_dma_mask = (phys_addr_t)~0;

	mutex_lock(&mtkcma_ion_cma_heap_device_mutex);
	list_add(&cma_heap->list, &mtkcma_ion_cma_heap_device_list);
	mutex_unlock(&mtkcma_ion_cma_heap_device_mutex);

	return 0;
}

static int mtkcma_get_bus_address_info(u32 *addr)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;

	target_memory_np = of_find_node_by_name(NULL, "memory_info");
	if (!target_memory_np)
		return -ENODEV;

	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);
	if (p != NULL) {
		if (*addr == -1)
			pr_info("        cpu_emi0_base is 0x%x\n", be32_to_cpup(p));
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

unsigned long ion_get_mtkcma_buffer_info(int share_fd)
{
	struct dma_buf *show_info_dma_buf;
	struct ion_buffer *buffer;
	struct ion_heap *heap;
	unsigned long buffer_pfn;

	 // do fget, so file->f_count++
	show_info_dma_buf = dma_buf_get(share_fd);
	if (IS_ERR_OR_NULL(show_info_dma_buf)) {
		pr_emerg("Function = %s, no dma_buf\n",
			__PRETTY_FUNCTION__);

		return -EINVAL;
	}
	buffer = show_info_dma_buf->priv;
	heap = buffer->heap;

	if (heap->type != ION_HEAP_TYPE_CUSTOM) {
		pr_emerg("Function = %s, not mtk heap\n",
			__PRETTY_FUNCTION__);
		dma_buf_put(show_info_dma_buf);

		return -EINVAL;
	}

	buffer_pfn = page_to_pfn((struct page *)buffer->priv_virt);

	dma_buf_put(show_info_dma_buf); // release dma_buf, so file->f_count--

	return buffer_pfn;
}
EXPORT_SYMBOL(ion_get_mtkcma_buffer_info);

void show_cma_info(struct cma *show_cma)
{
	pr_info("        cma_base_pfn is 0x%lX\n",
			show_cma->base_pfn);
	pr_info("        cma_page_count is 0x%lX\n",
			show_cma->count);
	pr_info("        cma_name is %s\n",
			show_cma->name);

}
EXPORT_SYMBOL(show_cma_info);

int mtkcma_presetting(struct device *dev, int pool_index)
{
	pr_emerg("please use mtkcma_presetting_v2, instead");
	return 0;
}
EXPORT_SYMBOL(mtkcma_presetting);

int mtkcma_presetting_v2(struct device *dev, int pool_index)
{
	int ret;
	struct mtkcma_ion_cma_heap *cma_heap, *tmp;

	mutex_lock(&mtkcma_ion_cma_heap_device_mutex);
	list_for_each_entry_safe(cma_heap, tmp, &mtkcma_ion_cma_heap_device_list, list) {
		if (cma_heap->owner_device == dev) {
			pr_emerg("%s is already registered, pass it\n", dev_name(dev));
			mutex_unlock(&mtkcma_ion_cma_heap_device_mutex);
			return 0;
		}
	}
	mutex_unlock(&mtkcma_ion_cma_heap_device_mutex);

	// this will set dev->cma_area
	ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, pool_index);
	if (ret) {
		pr_emerg("of_reserved_mem_device_init_by_idx error!!\n");
		return ret;
	}

	pr_info("    Start mtk_cma_device_initialize\n");
	ret = mtk_cma_device_initialize(dev);
	mutex_lock(&mtkcma_cpu_bus_base_mutex);
	mtkcma_get_bus_address_info(&mtkcma_cpu_bus_base);
	mutex_unlock(&mtkcma_cpu_bus_base_mutex);
	pr_info("    End mtk_cma_device_initialize, ret is %d\n",
				ret);

	return ret;
}
EXPORT_SYMBOL(mtkcma_presetting_v2);

MODULE_AUTHOR("MTK");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtk-memory");
