// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Joy Huang <joy-mi.huang@mediatek.com>
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kallsyms.h>
#include "clk-dtv.h"

static int util_log;
module_param(util_log, int, 0644);
MODULE_PARM_DESC(util_log, "clk util log\n");

#define CLK_UTIL_USE_INFO(fmt, args...)   do { \
	if (util_log > 0)  \
		pr_crit("[%s] " fmt,  \
			__func__, ## args);    \
} while (0)


struct mtkdtv_clk_ops {
	mtkdtv_clock_write write_fn;
	mtkdtv_clock_read read_fn;
};

static struct mtkdtv_clk_ops dtv_clk_ops;

int mtk_dtvclk_register(mtkdtv_clock_write write_fn, mtkdtv_clock_read read_fn)
{
	if (dtv_clk_ops.read_fn || dtv_clk_ops.write_fn)
		return -EBUSY;

	dtv_clk_ops.write_fn = write_fn;
	dtv_clk_ops.read_fn = read_fn;

	return 0;
}
EXPORT_SYMBOL(mtk_dtvclk_register);

/*
 * dtv_bank_type : CLK_PM, CLK_NONPM
 * reg_addr : clock address
 * start : clock start bit
 * end : clock end bit
 * ret : clock reg value or negtive if error
 */
int clock_read(enum dtv_regbank_type bank_type, unsigned int reg_addr,
	unsigned int start, unsigned int end)
{

	if (!dtv_clk_ops.read_fn)
		return -ENODEV;

	CLK_UTIL_USE_INFO("read 0x%x, 0x%x 0x%x\n", reg_addr, start, end);

	return dtv_clk_ops.read_fn(bank_type, reg_addr, start, end);

}
EXPORT_SYMBOL(clock_read);

/*
 * dtv_bank_type : CLK_PM, CLK_NONPM
 * reg_addr : clock address
 * value : write value
 * start : clock start bit
 * end : clock end bit
 * ret : clock write value or negtive if error
 */
int clock_write(enum dtv_regbank_type bank_type, unsigned int reg_addr, u16 value,
		unsigned int start, unsigned int end)
{
	if (!dtv_clk_ops.read_fn)
		return -ENODEV;

	CLK_UTIL_USE_INFO("write %pS: addr: %x %x %x %x\n",
		__builtin_return_address(0), reg_addr, value, start, end);

	return dtv_clk_ops.write_fn(bank_type, reg_addr, value, start, end);

}
EXPORT_SYMBOL(clock_write);

static int __init mtk_dtvclk_init(void)
{
	return 0;
}
module_init(mtk_dtvclk_init);
MODULE_DESCRIPTION("Driver for dtvclk interface");
MODULE_LICENSE("GPL v2");
