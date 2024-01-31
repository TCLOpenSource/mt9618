// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/trace.h>
#include <linux/platform_device.h>

#define DUMMY_OFFSET0	0x22
#define DUMMY_OFFSET1	0x24
#define DUMMY_OFFSET2	0x26
#define DUMMY_OFFSET3	0x28
#define DUMMY_OFFSET4	0x2a

#define SBOOT_DUMMY_OFFSET0 0x0
#define SBOOT_DUMMY_OFFSET1 0x1
#define SBOOT_DUMMY_OFFSET2 0x2
#define SBOOT_DUMMY_OFFSET3 0x3
#define SBOOT_DUMMY_OFFSET4 0x4

#undef pr_fmt
#define pr_fmt(fmt) "BOOTTIME: " fmt

#define BANK_TO_ADDR(_bank) ((_bank) << 9)
#define timestamp_readl(_base, _offset) readl((_base) + ((_offset) << 2))

struct uboot_timestamp {
	char *desp;
	unsigned int offset;
};

struct mtk_timestamp {
	struct device *dev;
	void __iomem *dummy_bank0;
	void __iomem *dummy_bank1;
};

static struct trace_array *instance;
static bool boottime_enable;
module_param(boottime_enable, bool, 0644);

extern int trace_array_printk(struct trace_array *tr, unsigned long ip,
		const char *fmt, ...);
extern struct trace_array *trace_array_create(const char *name);
extern int trace_array_init_printk(struct trace_array *tr);
extern int trace_array_destroy(struct trace_array *tr);

int boottime_print(const char *s)
{
	if (!instance)
		return 0;

	return trace_array_printk(instance, 0, s);
}
EXPORT_SYMBOL(boottime_print);

static int mtk_boottime_suspend(struct device *dev)
{
	return 0;
}

static int mtk_boottime_resume(struct device *dev)
{
	struct mtk_timestamp *pdata = dev_get_drvdata(dev);
	int i;
	uint32_t rom_latency;
	struct uboot_timestamp stamp[] = {
		{
			.desp   = "Resume: ROM end",
			.offset = SBOOT_DUMMY_OFFSET0,
		},
		{
			.desp	= "Resume: DRAM start",
			.offset	= SBOOT_DUMMY_OFFSET1,
		},
		{	.desp	= "Resume: DRAM end",
			.offset	= SBOOT_DUMMY_OFFSET2,
		},
		{
			.desp	= "Resume: Sboot end",
			.offset = SBOOT_DUMMY_OFFSET3,
		},
		{
			.desp   = "Resume: ATF end",
			.offset = SBOOT_DUMMY_OFFSET4,
		},

	};

	rom_latency = timestamp_readl(pdata->dummy_bank1, stamp[0].offset);
	trace_array_printk(instance, 0, "%s at [%d ms]\n", stamp[0].desp, rom_latency);

	for (i = 1; i < ARRAY_SIZE(stamp); ++i) {
		uint32_t tmp = timestamp_readl(pdata->dummy_bank1, stamp[i].offset);

		tmp += rom_latency;

		pr_debug("%s at [%d]\n", stamp[i].desp, tmp);
		trace_array_printk(instance, 0, "%s at [%d ms]\n",
				stamp[i].desp, tmp);
	}

	return 0;
}

static int mtk_boottime_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	struct resource *res;
	struct mtk_timestamp *pdata;
	struct uboot_timestamp stamp[] = {
	// how about sboot ?
	{
		.desp   = "show logo start",
		.offset = DUMMY_OFFSET0,
	},
	{
		.desp   = "show logo end",
		.offset = DUMMY_OFFSET1,
	},
	{
		.desp   = "start kernel",
		.offset = DUMMY_OFFSET2,
	},
	};

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	platform_set_drvdata(pdev, pdata);
	pdata->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res)
		return -EINVAL;

	pdata->dummy_bank0 = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pdata->dummy_bank0)) {
		dev_err(&pdev->dev, "err dummy_bank0=%#lx\n",
			(unsigned long)pdata->dummy_bank0);
		return PTR_ERR(pdata->dummy_bank0);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;
	pdata->dummy_bank1 = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pdata->dummy_bank1)) {
		dev_err(&pdev->dev, "err dummy_bank1=%#lx\n",
			(unsigned long)pdata->dummy_bank1);
		return PTR_ERR(pdata->dummy_bank1);
	}


	instance = trace_array_create("boottime");
	if (IS_ERR_OR_NULL(instance)) {
		pr_err("create boottime instance failed\n");
		return -ENODEV;
	}

	ret = trace_array_init_printk(instance);
	if (ret) {
		pr_err("init boottime instance failed with %d\n", ret);
		goto out;
	}

	pr_info("create boottime instance success\n");

	for (i = 0; i < ARRAY_SIZE(stamp); ++i) {
		uint32_t tmp = timestamp_readl(pdata->dummy_bank0, stamp[i].offset);

		pr_debug("%s at [%d]\n", stamp[i].desp, tmp);
		trace_array_printk(instance, 0, "%s at [%d ms]\n",
		stamp[i].desp, tmp);
	}

	return 0;
out:
	trace_array_destroy(instance);
	return ret;

}
static int mtk_boottime_remove(struct platform_device *pdev)
{
	return trace_array_destroy(instance);
}

static const struct dev_pm_ops mtk_boottime_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mtk_boottime_suspend, mtk_boottime_resume)
};

static const struct of_device_id mtk_boottime_device_ids[] = {
	{ .compatible = "mtk_boottime", },
	{},
};

static struct platform_driver mtk_boottime_driver = {
	.probe	= mtk_boottime_probe,
	.remove	= mtk_boottime_remove,
	.driver = {
		.name   = "mtk_boottime",
		.of_match_table = mtk_boottime_device_ids,
		.owner  = THIS_MODULE,
		.pm	= &mtk_boottime_pm_ops,
	}
};

static __init int mtk_boottime_init(void)
{
	if (!boottime_enable) {
		pr_info("mtk-boottime: Disable boottime function\n");
		return 0;
	}

	return platform_driver_register(&mtk_boottime_driver);
}

static __exit void mtk_boottime_exit(void)
{
	platform_driver_unregister(&mtk_boottime_driver);
}

module_init(mtk_boottime_init);
module_exit(mtk_boottime_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK boottime record instance");
