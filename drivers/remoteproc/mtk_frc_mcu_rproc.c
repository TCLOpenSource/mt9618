// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <asm/barrier.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include "mtk-reserved-memory.h"


#include "remoteproc_internal.h"

#define FRC_R2_REG_RST_BASE                     (0x00DC)
#define FRC_R2_REG_RST_BASE_LOW                 (0x00B4)
#define FRC_R2_REG_STOP                         (0x0080)

#define FRC_R2_VAL_stop                         (0x20)
#define FRC_R2_VAL_rst                          (0x24)
#define FRC_R2_VAL_boot                         (0x27)


struct mtk_frc_mcu {
	bool support_suspend;
	bool ac_ready;
	struct device *dev;
	struct rproc *rproc;
	struct clk *clk;
	void *fw_base, *intg_base, *frcpll;
	u64 phy_base;
	struct regmap  *ckgen_regmap, *frcu_regmap;
	struct list_head clk_init_seq;
	struct list_head setting_init_seq;
};

struct frc_mcu_setting {
	struct list_head list;
	u32 offset, mask, val;
};

#define OFFSET_INDEX	0
#define MASK_INDEX	1
#define VAL_INDEX	2
#define PLL_DELAY	100


s64 d0_frc_mcu_clk_init;
s64 d0_frc_mcu_start;
s64 d0_frc_bin_load;
s64 d0_mtk_frc_mcu_resume;


#define REG_SIZE         0x10
#define MIU_BASE_ADR     0x20000000

#define REG_BIT_MASK	0xFFFF
#define REG_BIT_OFFSET	16

static bool auto_boot;
module_param(auto_boot, bool, 0444);
MODULE_PARM_DESC(auto_boot, "Enable auto boot");

struct frc_rproc_watchdog {
	struct device		*dev;
	struct task_struct	*tsk;
	struct timer_list	timer;
};


static void frc_rproc_watchdog_handler(struct timer_list *t)
{
	struct frc_rproc_watchdog *wd = from_timer(wd, t, timer);

	dev_emerg(wd->dev, "**** frc_rproc timeout ****\n");
	dev_emerg(wd->dev, "d0_mtk_frc_mcu_resume time  = %lld\n", d0_mtk_frc_mcu_resume);
	dev_emerg(wd->dev, "d0_frc_bin_load time  = %lld\n", d0_frc_bin_load);
	dev_emerg(wd->dev, "d0_frc_mcu_start time  = %lld\n", d0_frc_mcu_start);
	dev_emerg(wd->dev, "d0_frc_mcu_clk_init time  = %lld\n", d0_frc_mcu_clk_init);

}


static void frc_rproc_watchdog_set(struct frc_rproc_watchdog *wd, struct device *dev)
{
	struct timer_list *timer = &wd->timer;

	wd->dev = dev;
	wd->tsk = current;

	timer_setup_on_stack(timer, frc_rproc_watchdog_handler, 0);
	/* use same timeout value for both suspend and resume */
	timer->expires = jiffies + HZ * 2;
	add_timer(timer);
}


static void frc_rproc_watchdog_clear(struct frc_rproc_watchdog *wd)
{
	struct timer_list *timer = &wd->timer;

	del_timer_sync(timer);
	destroy_timer_on_stack(timer);
}

static int setup_init_seq(struct device *dev, struct mtk_frc_mcu *frc_mcu,
			   struct device_node *np)
{
	int count, i, rc;
	struct of_phandle_args clkspec;

	count = of_count_phandle_with_args(np, "frc_mcu_setting", "#frc_mcu_bank_map_cells");
	if (count < 0) {
		dev_warn(dev, "frc_mcu_setting not found\n");
		return count;
	}

	for (i = 0; i < count; i++) {
		struct frc_mcu_setting *clk;

		rc = of_parse_phandle_with_args(np, "frc_mcu_setting",
				"#frc_mcu_bank_map_cells", i, &clkspec);
		if (rc < 0) {
			dev_warn(dev,  "frc_mcu-setting parse fail with err %d\n", rc);
			return rc;
		}

		clk = kzalloc(sizeof(struct frc_mcu_setting), GFP_KERNEL);
		if (!clk)
			return -ENOMEM;

		clk->offset = clkspec.args[OFFSET_INDEX];
		clk->mask = clkspec.args[MASK_INDEX];
		clk->val = clkspec.args[VAL_INDEX];

		list_add_tail(&clk->list, &frc_mcu->setting_init_seq);
	}


	count = of_count_phandle_with_args(np, "frc_mcu_clk_setting", "#frc_mcu_clk_map_cells");
	if (count < 0) {
		dev_warn(dev, "frc_mcu_clk_setting not found\n");
		return count;
	}

	for (i = 0; i < count; i++) {
		struct frc_mcu_setting *clk;

		rc = of_parse_phandle_with_args(np, "frc_mcu_clk_setting",
				"#frc_mcu_clk_map_cells", i, &clkspec);
		if (rc < 0) {
			dev_warn(dev,  "frc_mcu-setting parse fail with err %d\n", rc);
			return rc;
		}

		clk = kzalloc(sizeof(struct frc_mcu_setting), GFP_KERNEL);
		if (!clk)
			return -ENOMEM;

		clk->offset = clkspec.args[OFFSET_INDEX];
		clk->mask = clkspec.args[MASK_INDEX];
		clk->val = clkspec.args[VAL_INDEX];

		list_add_tail(&clk->list, &frc_mcu->clk_init_seq);
	}

	return 0;
}

static void frc_mcu_clk_init(struct device *dev, struct mtk_frc_mcu *frc_mcu)
{
	struct frc_mcu_setting *clk, *setting;
	ktime_t start_time;
	ktime_t end_time;

	start_time = ktime_get();

	if (list_empty(&frc_mcu->clk_init_seq)) {
		dev_warn(dev, "clk_init_seq is not set\n");
		return;
	}

	list_for_each_entry(clk, &frc_mcu->clk_init_seq, list) {

		regmap_update_bits_base(frc_mcu->ckgen_regmap, clk->offset, clk->mask,
					clk->val, NULL, false, true);
	}

	if (list_empty(&frc_mcu->setting_init_seq)) {
		dev_warn(dev, "setting_init_seq is not set\n");
		return;
	}

	list_for_each_entry(setting, &frc_mcu->setting_init_seq, list) {

		regmap_update_bits_base(frc_mcu->frcu_regmap, setting->offset, setting->mask,
					setting->val, NULL, false, true);
	}
	end_time = ktime_get();
	d0_frc_mcu_clk_init = (s64)ktime_to_ns(ktime_sub(end_time, start_time));
}


static int frc_mcu_start(struct rproc *rproc)
{
	struct mtk_frc_mcu *frc_mcu = rproc->priv;
	u32 emi_offset;
	ktime_t start_time;
	ktime_t end_time;

	start_time = ktime_get();

	emi_offset = frc_mcu->phy_base - MIU_BASE_ADR;
	if (frc_mcu->ac_ready != 1) {

		regmap_update_bits_base(frc_mcu->frcu_regmap, FRC_R2_REG_RST_BASE<<1,
					0xFFFF, emi_offset & REG_BIT_MASK, NULL, false, true);
		regmap_update_bits_base(frc_mcu->frcu_regmap, FRC_R2_REG_RST_BASE_LOW<<1,
					0xFFFF, emi_offset >> REG_BIT_OFFSET, NULL, false, true);

		regmap_update_bits_base(frc_mcu->frcu_regmap, FRC_R2_REG_STOP<<1,
					0x00FF, FRC_R2_VAL_rst, NULL, false, true);
		regmap_update_bits_base(frc_mcu->frcu_regmap, FRC_R2_REG_STOP<<1,
					0x00FF, FRC_R2_VAL_boot, NULL, false, true);

		frc_mcu->ac_ready = 1;
	}

	end_time = ktime_get();
	d0_frc_mcu_start = (s64)ktime_to_ns(ktime_sub(end_time, start_time));

	return 0;
}

static int frc_mcu_stop(struct rproc *rproc)
{
	struct mtk_frc_mcu *frc_mcu = rproc->priv;
	//to do
	/* pll disable */
	regmap_update_bits_base(frc_mcu->frcu_regmap, FRC_R2_REG_STOP<<1,
					0x00FF, FRC_R2_VAL_stop, NULL, false, true);

	//writew(0, frc_mcu->intg_base);
	return 0;
}

static void *frc_mcu_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct mtk_frc_mcu *frc_mcu = rproc->priv;

	return frc_mcu->fw_base + da;
}

static int frc_bin_load(struct rproc *rproc, const struct firmware *fw)
{
	struct mtk_frc_mcu *frc_mcu = rproc->priv;
	void *ptr = ioremap_wc(frc_mcu->phy_base, fw->size);
	ktime_t start_time;
	ktime_t end_time;

	start_time = ktime_get();

	memcpy(ptr, fw->data, fw->size);
	iounmap(ptr);

	end_time = ktime_get();
	d0_frc_bin_load = (s64)ktime_to_ns(ktime_sub(end_time, start_time));
	return 0;
}


/* kick a virtqueue */
static void frc_mcu_kick(struct rproc *rproc, int vqid)
{
	//to do

}

static const struct rproc_ops frc_mcu_ops = {
	.start		= frc_mcu_start,
	.stop		= frc_mcu_stop,
	.kick		= frc_mcu_kick,
	.da_to_va	= frc_mcu_da_to_va,
	.load		= frc_bin_load
};

static int mtk_frc_mcu_check_mem_region(struct device *dev, struct device_node *np)
{
	int ret;
	struct of_mmap_info_data dma, fwbuf;

	/* dma mem */
	ret = of_mtk_get_reserved_memory_info_by_idx(np, 0, &dma);
	if (ret)
		return ret;

	/* firmware buffer */
	ret = of_mtk_get_reserved_memory_info_by_idx(np, 1, &fwbuf);
	if (ret)
		return ret;

	return 0;
}

static int mtk_frc_mcu_parse_firmware(struct platform_device *pdev, const char **firmware)
{
	int ret;
	struct device_node *mmap_np = NULL;
	const char *mmap_name =  NULL;

	mmap_np = of_find_node_by_name(NULL, "mmap_info");
	if (mmap_np == NULL)
		return 0;

	mmap_np = of_find_node_by_name(mmap_np, "MMAP_PROPERTY");
	if (mmap_np == NULL)
		return 0;


	ret = of_property_read_string_index(mmap_np, "mmap_filename",
						0, &mmap_name);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	ret = of_property_read_string_index(pdev->dev.of_node, mmap_name,
						0, firmware);
	if (ret < 0 && ret != -EINVAL)
		return ret;


	if (*firmware == NULL) {
		ret = of_property_read_string_index(pdev->dev.of_node, "firmware-name",
							0, firmware);
		if (ret < 0 && ret != -EINVAL)
			return ret;
	}

	return ret;

}

static int mtk_frc_mcu_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *syscon_np;
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_frc_mcu *frc_mcu = rproc->priv;
	struct resource *res;
	struct of_mmap_info_data mmap_info;


	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intg");
	frc_mcu->intg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(frc_mcu->intg_base)) {
		dev_warn(dev, "no intg base\n");
		return PTR_ERR(frc_mcu->intg_base);
	}


	/* TODO: Remove this after memcu clk init seq ready */
	frc_mcu->support_suspend = of_property_read_bool(np,
					"mediatek,frc_mcu-support-suspend");

	syscon_np = of_parse_phandle(np, "frc_mcu_clk_bank", 0);
	if (!syscon_np) {
		dev_warn(dev, "no mtk,frc_mcu_clk_bank node\n");
		return -ENODEV;
	}
	frc_mcu->ckgen_regmap = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(frc_mcu->ckgen_regmap))
		return PTR_ERR(frc_mcu->ckgen_regmap);

	syscon_np = of_parse_phandle(np, "frc_mcu_bank", 0);
	if (!syscon_np) {
		dev_warn(dev, "no mtk,frc_mcu_bank node\n");
		return -ENODEV;
	}
	frc_mcu->frcu_regmap = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(frc_mcu->frcu_regmap))
		return PTR_ERR(frc_mcu->frcu_regmap);


	ret = setup_init_seq(dev, frc_mcu, np);
	if (ret)
		return ret;

	/* check memory region, share dma region should be after fw mem */
	ret = mtk_frc_mcu_check_mem_region(dev, np);
	if (ret)
		return ret;

	ret = of_reserved_mem_device_init(dev);
	if (ret)
		return ret;

	ret = of_mtk_get_reserved_memory_info_by_idx(np, 1, &mmap_info);
	if (ret)
		return ret;

	frc_mcu->fw_base = devm_ioremap_wc(dev, mmap_info.start_bus_address, mmap_info.buffer_size);
	frc_mcu->phy_base = mmap_info.start_bus_address;

	if (IS_ERR(frc_mcu->fw_base)) {
		of_reserved_mem_device_release(dev);
		return PTR_ERR(frc_mcu->fw_base);
	}
	frc_mcu->ac_ready = 0;

	return 0;
}

static int __maybe_unused mtk_frc_mcu_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_frc_mcu *frc_mcu = rproc->priv;

	if (!frc_mcu->support_suspend) {
		dev_warn(dev, "device doesn't support supsend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	frc_mcu_stop(rproc);

	rproc_boot(rproc);

	rproc_shutdown(rproc);



	return 0;
}

static int __maybe_unused mtk_frc_mcu_resume(struct device *dev)
{
	int ret;
	ktime_t start_time_resume;
	ktime_t end_time_resume;
	struct frc_rproc_watchdog wd;

	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_frc_mcu *frc_mcu = rproc->priv;
	start_time_resume = ktime_get();

	if (!frc_mcu->support_suspend) {
		dev_warn(dev, "device doesn't support supsend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	frc_rproc_watchdog_set(&wd, dev);

	frc_mcu_clk_init(dev, frc_mcu);
	udelay(PLL_DELAY);

	frc_mcu->ac_ready = 0;

	ret = frc_mcu_start(rproc);

	end_time_resume = ktime_get();
	d0_mtk_frc_mcu_resume = (s64)ktime_to_ns(ktime_sub(start_time_resume, end_time_resume));
	frc_rproc_watchdog_clear(&wd);
	return ret;
}

static const struct dev_pm_ops mtk_frc_mcu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_frc_mcu_suspend, mtk_frc_mcu_resume)
};




static int mtk_frc_mcu_rproc_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct mtk_frc_mcu *frc_mcu;
	struct rproc *rproc;
	const char *firmware =  NULL;

	ret = mtk_frc_mcu_parse_firmware(pdev, &firmware);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	rproc = rproc_alloc(dev, np->name, &frc_mcu_ops,
				firmware, sizeof(*frc_mcu));

	if (!rproc)
		return -ENOMEM;


	rproc->auto_boot = of_property_read_bool(np, "mediatek,auto-boot");
	if (!rproc->auto_boot)
		rproc->auto_boot = auto_boot;

	rproc->has_iommu = false;



	frc_mcu = (struct mtk_frc_mcu *)rproc->priv;
	frc_mcu->rproc = rproc;
	frc_mcu->dev = dev;
	INIT_LIST_HEAD(&frc_mcu->clk_init_seq);
	INIT_LIST_HEAD(&frc_mcu->setting_init_seq);

	platform_set_drvdata(pdev, rproc);
	ret = mtk_frc_mcu_parse_dt(pdev);
	if (ret)
		return ret;


	device_enable_async_suspend(dev);

	frc_mcu_clk_init(dev, frc_mcu);
	udelay(PLL_DELAY);


	ret = rproc_add(rproc);
	if (ret)
		return ret;


	return 0;
}

static int mtk_frc_mcu_rproc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rproc *rproc = platform_get_drvdata(pdev);

	of_reserved_mem_device_release(dev);
	rproc_del(rproc);
	rproc_free(rproc);
	return 0;
}

static const struct of_device_id mtk_frc_mcu_of_match[] = {
	{ .compatible = "mediatek,frc-mcu"},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_frc_mcu_of_match);

static struct platform_driver mtk_frc_mcu_rproc = {
	.probe = mtk_frc_mcu_rproc_probe,
	.remove = mtk_frc_mcu_rproc_remove,
	.driver = {
		.name = "mtk-frc-mcu",
		.pm = &mtk_frc_mcu_pm_ops,
		.of_match_table = of_match_ptr(mtk_frc_mcu_of_match),
	},
};

module_platform_driver(mtk_frc_mcu_rproc);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Allen-HW.Hsu <addy.hsu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek FRC_MCU Remoteproce driver");

