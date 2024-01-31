// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/dma-direct.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_address.h>
#include <linux/genalloc.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/sizes.h>
#include <asm/page.h>

#include "mtk_zram_genpool.h"

#define ZRAM_PAGE_ALIGN	(1 << 12)

struct mtk_zram_carv {
	struct dma_heap *heap;
	struct gen_pool *pool;
	phys_addr_t pa;
	size_t size;
};

struct zram_carveout_node {
	struct list_head list;
	struct page *page;
	unsigned long len;
};

static struct mtk_zram_carv *glb_carv;

bool is_carv_exist(void)
{
	return !!glb_carv;
}
EXPORT_SYMBOL(is_carv_exist);

bool is_carv_range(struct page *page)
{
	unsigned long ba;

	if (!glb_carv)
		return false;

	WARN_ON(!glb_carv->pa || !page);

	ba = (unsigned long)page_to_phys(page);
	return (ba >= glb_carv->pa && ba < (glb_carv->pa + glb_carv->size));
}
EXPORT_SYMBOL(is_carv_range);

void free_carv_page(struct page *page)
{
	if (!glb_carv)
		return;
	if (is_carv_range(page))
		gen_pool_free(glb_carv->pool, page_to_phys(page), PAGE_SIZE);
}
EXPORT_SYMBOL(free_carv_page);

struct page *alloc_carv_page(void)
{
	phys_addr_t pa;

	if (!glb_carv)
		return NULL;

	pa = gen_pool_alloc(glb_carv->pool, ZRAM_PAGE_ALIGN);
	if (!pa) {
		pr_warn("%s: carveout mem not enough\n", __func__);
		return NULL;
	}
	if (offset_in_page(pa)) {
		pr_err("%s: pa is not page aligned\n", __func__);
		gen_pool_free(glb_carv->pool, pa, ZRAM_PAGE_ALIGN);
		return NULL;
	}

	return phys_to_page(pa);

}
EXPORT_SYMBOL(alloc_carv_page);

static const struct of_device_id mtk_zram_carveout_of_ids[] = {
	{.compatible = "mediatek,dtv-zram-carveout",},
	{ }
};

static int zram_carveout_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct mtk_zram_carv *carv;
	struct resource r;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		// it is ok for no carveout memory
		dev_err(dev, "No memory-region specified\n");
		return 0;
	}

	ret = of_address_to_resource(np, 0, &r);
	if (ret) {
		// it is ok for no carveout memory
		dev_err(dev, "No memory address assigned to the region\n");
		return 0;
	}

	carv = devm_kzalloc(dev, sizeof(struct mtk_zram_carv), GFP_KERNEL);
	if (!carv)
		return -ENOMEM;

	dev_info(dev, "reserved mem pa:%pa+%lx\n",
			&r.start, (unsigned long)resource_size(&r));

	carv->pa = r.start;
	carv->size = (size_t)resource_size(&r);
	carv->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carv->pool) {
		dev_err(dev, "gen pool failed\n");
		return -ENOMEM;
	}

	ret = gen_pool_add(carv->pool, carv->pa, carv->size, -1);
	if (ret) {
		dev_err(dev, "gen_pool_add failed with %d\n", ret);
		goto out;
	}

	platform_set_drvdata(pdev, carv);

	glb_carv = carv;

	return 0;
out:
	gen_pool_destroy(carv->pool);
	return ret;
}

static int zram_carveout_remove(struct platform_device *pdev)
{
	struct mtk_zram_carv *carv =
		platform_get_drvdata(pdev);

	if (!carv)
		return -EINVAL;

	gen_pool_destroy(carv->pool);
	glb_carv = NULL;

	return 0;
}

static struct platform_driver zram_genpool_driver = {
	.probe = zram_carveout_probe,
	.remove = zram_carveout_remove,
	.driver = {
		.name = "mtk-zram-carveout",
		.of_match_table = mtk_zram_carveout_of_ids,
	}
};

module_platform_driver(zram_genpool_driver);
MODULE_LICENSE("GPL");

