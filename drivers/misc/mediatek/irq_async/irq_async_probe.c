// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author Mark-PK Tsai <mark-pk.tsai@mediatek.com>
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/kprobes.h>

static DEFINE_MUTEX(test_lock);
static DEFINE_MUTEX(virq_lock);
static bool disable_kp;
static bool test;
module_param(disable_kp, bool, 0644);
module_param(test, bool, 0644);


static irqreturn_t dummy_interrupt(int irq, void *dev_id)
{
	static int count;

	if (count == 0) {
		pr_info("dummy-irq: interrupt occurred on IRQ %d\n", irq);
		count++;
	}

	return IRQ_NONE;
}

static void async_probe(void *data)
{
	struct device_node *np = (struct device_node *)data;
	int ret;
	static int saved_virq;
	int virq = of_irq_get(np, 0);

	mutex_lock(&test_lock);
	if (!saved_virq)
		saved_virq = virq;

	mutex_unlock(&test_lock);

	WARN(virq != saved_virq, "virq(%d) != saved virq(%d)\n", virq, saved_virq);
	ret = request_irq(virq, &dummy_interrupt, IRQF_SHARED, "dummy_irq", &virq);
	if (ret)
		pr_err("failed to register irq %d\n", virq);
}

static int test_parallel_probe(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "mediatek,dummy-irq");
	if (!np) {
		pr_warn("no node with compatible: mediatek,dummy-irq\n");
		return -ENODEV;
	}

	pr_info("call of_irq_get on each cpu ...\n");
	on_each_cpu(async_probe, np, 0);
	pr_info("test finish\n");
	return 0;
}

static int __kprobes irq_create_fwspec_mapping_entry(struct kretprobe_instance *p,
						     struct pt_regs *regs)
{
	mutex_lock(&virq_lock);
	return 0;
}

static int __kprobes irq_create_fwspec_mapping_exit(struct kretprobe_instance *p,
						    struct pt_regs *regs)
{
	mutex_unlock(&virq_lock);
	return 0;
}

static struct kretprobe krp_arr[] = {
	{
		.handler	= irq_create_fwspec_mapping_exit,
		.entry_handler  = irq_create_fwspec_mapping_entry,
		.kp.symbol_name = "irq_create_fwspec_mapping"
	}
};

int __init kp_init(void)
{
	int ret, i;

	if (disable_kp) {
		pr_warn("irq_async_probe kp is disabled\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(krp_arr); i++) {
		ret = register_kretprobe(&krp_arr[i]);
		if (ret < 0) {
			pr_err("register_kretprobe returned %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int __init irq_async_probe_init(void)
{
	int ret;

	ret = kp_init();
	if (ret)
		return ret;

	if (test)
		test_parallel_probe();

	return 0;
}

module_init(irq_async_probe_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark-PK Tsai");
MODULE_PARM_DESC(irq, "patch kernel to support IRQ async probe");
MODULE_DESCRIPTION("irqdomain patch code");
