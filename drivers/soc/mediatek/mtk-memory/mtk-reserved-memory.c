// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Joe Liu <joe.liu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>

#include "mtk-reserved-memory.h"
#include "mtk-cma.h"
#include "mtk-null-memory-dmaheap.h"

int root_addr_cells = 2;
int root_size_cells = 2;

struct device *mtk_mmap_device;

static const struct of_device_id mtk_mmap_device_driver_of_ids[] = {
	{.compatible = "mediatek,tv-reserved-memory",},
	{},
};

u64 mtk_mem_next_cell(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}
EXPORT_SYMBOL(mtk_mem_next_cell);

/*
 * of_mtk_get_reserved_memory_info_by_idx
 * - Get the mmap info by the memory-region index of a device_node
 *
 * @np: device node which needs to query its reserved memory info
 * @idx: reserved memory idx
 * @of_mmap_info: Returned node info
 *
 * Use the given device_node and index to search the mmap_info device_node.
 * All property of this found child device node will be stored in of_mmap_info.
 */
int of_mtk_get_reserved_memory_info_by_idx(struct device_node *np, int idx,
	struct of_mmap_info_data *of_mmap_info)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p, *endp = NULL;

	if (np == NULL) {
		pr_err("np is NULL\n");
		return -ENXIO;
	}

	if (of_mmap_info == NULL) {
		pr_err("np %s is @ %p, of_mmap_info is NULL\n",
			np->name, (void *)np);
		return -ENXIO;
	}

	target_memory_np = of_parse_phandle(np, "memory-region", idx);
	if (!target_memory_np)
		return -ENODEV;

	p = (__be32 *)of_get_property(target_memory_np, "layer", &len);
	if (p != NULL) {
		of_mmap_info->layer = be32_to_cpup(p);
		p = NULL;
	}

	p = (__be32 *)of_get_property(target_memory_np, "miu", &len);
	if (p != NULL) {
		of_mmap_info->miu = be32_to_cpup(p);
		p = NULL;
	}

	p = (__be32 *)of_get_property(target_memory_np, "is_cache", &len);
	if (p != NULL) {
		of_mmap_info->is_cache = be32_to_cpup(p);
		p = NULL;
	}

	p = (__be32 *)of_get_property(target_memory_np, "cma_id", &len);
	if (p != NULL) {
		of_mmap_info->cma_id = be32_to_cpup(p);
		p = NULL;
	}

	p = (__be32 *)of_get_property(target_memory_np, "reg", &len);
	if (p != NULL) {
		endp = p + (len / sizeof(__be32));

		while ((endp - p) >=
			(root_addr_cells + root_size_cells)) {
			of_mmap_info->start_bus_address =
				mtk_mem_next_cell(root_addr_cells, (const __be32 **)&p);
			of_mmap_info->buffer_size =
				mtk_mem_next_cell(root_size_cells, (const __be32 **)&p);
		}
	}

	of_node_put(target_memory_np);
	return 0;
}
EXPORT_SYMBOL(of_mtk_get_reserved_memory_info_by_idx);

int mtk_ion_alloc(size_t len, unsigned int heap_id_mask,
	unsigned int flags)
{
	int fd;
	struct dma_buf *dmabuf;

	dmabuf = ion_alloc(len, heap_id_mask, flags);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}
EXPORT_SYMBOL(mtk_ion_alloc);

int ion_test(struct device *dev, unsigned long alloc_offset,
			unsigned long size, unsigned int flags)
{
	struct ion_heap_data *data;
	unsigned int i = 0;
	unsigned int cma_heap_id = -1;
	int ret = 0;
	int fd = -1;
	struct ion_allocation_data cma_alloc_data = {0};
	size_t heap_cnt = 0;
	size_t kmalloc_size = 0;

	heap_cnt = ion_query_heaps_kernel(NULL, heap_cnt);
	kmalloc_size = sizeof(struct ion_heap_data) * heap_cnt;
	data = (struct ion_heap_data *)
		kmalloc(kmalloc_size, GFP_KERNEL);
	if (!data) {
		pr_emerg("Function = %s, no data!\n", __PRETTY_FUNCTION__);
		return -ENOMEM;
	}
	heap_cnt = ion_query_heaps_kernel(data, heap_cnt);

	pr_info("\n\ncurrently, query heap cnt is %zu\n\n", heap_cnt);

	for (i = 0; i < heap_cnt; i++) {
		pr_info("data[%d] name %s\n", i, data[i].name);
		ret = strncmp(data[i].name, dev_name(dev), MAX_HEAP_NAME);
		if (ret == 0) {
			cma_heap_id = data[i].heap_id;
			pr_info("    get cma_heap_id %u\n", cma_heap_id);
		}
	}
	if (cma_heap_id == -1) {
		pr_emerg("Function = %s, get cma_heap_id failed\n",
			__PRETTY_FUNCTION__);
		goto invalid_cma_heap_id;
	} else
		pr_info("get ion test cma_heap_id %d OK\n", cma_heap_id);

	cma_alloc_data.len = size;
	cma_alloc_data.heap_id_mask = 1 << cma_heap_id;
	fd = mtk_ion_alloc(size, 1 << cma_heap_id,
		((alloc_offset >> PAGE_SHIFT) << PAGE_SHIFT) | flags);

	pr_info("[ION] get fd %d\n", fd);

invalid_cma_heap_id:
	kfree(data);
	return fd;
}
EXPORT_SYMBOL(ion_test);

unsigned long mtk_get_nullmem_buffer_info(int share_fd)
{
	unsigned long buffer_pfn;
	struct dma_buf *dmabuf;

	dmabuf = dma_buf_get(share_fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		pr_emerg("Function = %s, no dma_buf\n",
			__PRETTY_FUNCTION__);
		return -EINVAL;
	}

	if (is_nullmem_dma_heap(dmabuf)) {
		dma_buf_put(dmabuf);
		buffer_pfn = dma_get_nullmem_buffer_info(share_fd);
	} else {
		dma_buf_put(dmabuf);
		buffer_pfn = ion_get_mtkcma_buffer_info(share_fd);
	}

	return buffer_pfn;
}
EXPORT_SYMBOL(mtk_get_nullmem_buffer_info);

static int mtk_mmap_device_driver_probe(struct platform_device *pdev)
{
	void *va_ptr;
	int ret = -1;
	dma_addr_t dma_alloc_offset = 0;
	struct device_node *device_node_with_cma = NULL;
	struct device *device_with_cma = NULL;
	__be32 *p = NULL;
	u32 pool_index = 0;
	uint32_t len = 0;

	mtk_mmap_device = &pdev->dev;
	pr_info("\n\n[MTK-MEMORY] probe start !!\n");
	pr_info("    platform_device is @ %p\n",
				(void *)pdev);

	if (pdev->name)
		pr_info("    platform_device name is %s\n",
			pdev->name);

	pr_info("    platform_device->device name is %s\n",
		dev_name(mtk_mmap_device));

	/* initialize nullmem heap */
#if IS_ENABLED(CONFIG_ION)
	ret = mtk_nullmemory_heap_create();
#endif
#if IS_ENABLED(CONFIG_DMABUF_HEAPS_SYSTEM)
	ret = mtk_nullmemory_dmaheap_create();
#endif
	if (ret < 0) {
		pr_emerg("mtk nullmem failed to create heap\n");
		return ret;
	}

	pr_info("\n\nauto create mtkcma ion heap... start\n");
	for_each_node_with_property(device_node_with_cma, "cmas") {
		if (!of_device_is_available(device_node_with_cma)) {
			pr_emerg("device_node %s is not available\n",
				device_node_with_cma->name);
			continue;
		}

		device_with_cma = bus_find_device_by_of_node(&platform_bus_type,
			device_node_with_cma);
		if (!device_with_cma) {
			pr_emerg("device_node %s has no device\n",
				device_node_with_cma->name);
			continue;
		}

		p = (__be32 *)of_get_property(device_node_with_cma, "cmas", &len);
		if (p != NULL) {
			pool_index = be32_to_cpup(p);
			pr_info("for device_node %s, use pool_index %u\n",
				device_node_with_cma->name, pool_index);
		} else {
			pr_emerg("can not get cma pool index for device_node %s\n",
				device_node_with_cma->name);
			continue;
		}

		/* do mtkcma_presetting_v2() */
		mtkcma_presetting_v2(device_with_cma, (int)pool_index);

		/* due to of_find_device_by_node() */
		put_device(device_with_cma);

		/* due to for_each_node_with_property() */
		of_node_put(device_node_with_cma);
	}
	pr_info("auto create mtkcma ion heap... finish\n\n");

	/* do dma_alloc & dma_free
	 * here we use default_cma, so we can not use specific address
	 */
	va_ptr = dma_alloc_attrs(mtk_mmap_device, PAGE_SIZE, &dma_alloc_offset,
		GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
	if (IS_ERR(va_ptr))
		pr_emerg("test dma_ops failed, va_ptr is %p, err\n", va_ptr);
	else if (!(va_ptr) && !PageHighMem(phys_to_page(dma_alloc_offset)))
		pr_emerg("test dma_ops failed, va_ptr is %p, err\n", va_ptr);
	else {
		/* do mtkcma_dma_sync_single_for_cpu() */
		dma_sync_single_range_for_cpu(mtk_mmap_device, dma_alloc_offset, 0,
			PAGE_SIZE, DMA_BIDIRECTIONAL);

		/* do mtkcma_dma_sync_single_for_device() */
		dma_sync_single_range_for_device(mtk_mmap_device, dma_alloc_offset, 0,
			PAGE_SIZE, DMA_BIDIRECTIONAL);

		dma_free_attrs(mtk_mmap_device, PAGE_SIZE, va_ptr, dma_alloc_offset,
			DMA_ATTR_NO_KERNEL_MAPPING);
	}

	return 0;
}

static int mtk_mmap_device_driver_remove(struct platform_device *pdev)
{
	pr_emerg("mtk_mmap_device_driver_exit\n");
	return 0;
}

static struct platform_driver mtk_mmap_device_driver = {
	.probe = mtk_mmap_device_driver_probe,
	.remove = mtk_mmap_device_driver_remove,
	.driver = {
		.name = "mtk-mmap-test",
		.of_match_table = mtk_mmap_device_driver_of_ids,
	}
};

static int __init mtk_mmap_device_init(void)
{
	return platform_driver_register(&mtk_mmap_device_driver);
}

static void __exit mtk_mmap_device_exit(void)
{
	platform_driver_unregister(&mtk_mmap_device_driver);
}

module_init(mtk_mmap_device_init);
module_exit(mtk_mmap_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mtk");
MODULE_DESCRIPTION("mtk mmap driver");
