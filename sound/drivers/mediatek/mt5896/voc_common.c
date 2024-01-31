// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

//------------------------------------------------------------------------------
//  Include Files
//------------------------------------------------------------------------------
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include "voc_common.h"
#include "voc_common_reg.h"
#include "clk-dtv.h"   //CLK_PM

//------------------------------------------------------------------------------
//  Macros
//------------------------------------------------------------------------------

#define VOICE_TIMER2_COUNT_MAX_L_OFFSET         (0x08)
#define VOICE_TIMER2_COUNT_MAX_H_OFFSET         (0x0C)
#define VOICE_TIMER2_COUNT_CUR_L_OFFSET         (0x10)
#define VOICE_TIMER2_COUNT_CUR_H_OFFSET         (0x14)
#define VOICE_BITS_OF_SHORT                     (16)
//------------------------------------------------------------------------------
//  Variables
//------------------------------------------------------------------------------
void __iomem *timer2_base;
uint32_t timer2_tick_to_us;

//------------------------------------------------------------------------------
//  Function
//------------------------------------------------------------------------------
static uint32_t voc_common_get_tick(void)
{
	uint32_t u32Tick;
	uint32_t count_l1, count_h1;
	uint32_t count_l2, count_h2;

	if (!timer2_base)
		return 0;

	count_l1 = readw(timer2_base + VOICE_TIMER2_COUNT_CUR_L_OFFSET);
	count_h1 = readw(timer2_base + VOICE_TIMER2_COUNT_CUR_H_OFFSET);
	count_l2 = readw(timer2_base + VOICE_TIMER2_COUNT_CUR_L_OFFSET);
	count_h2 = readw(timer2_base + VOICE_TIMER2_COUNT_CUR_H_OFFSET);
	if (count_l2 < count_l1) {
		count_l1 = count_l2;
		count_h1 = count_h2;
	}
	u32Tick = (count_h1 << VOICE_BITS_OF_SHORT) | count_l1;
	return u32Tick;
}

uint64_t voc_common_get_us(void)
{
	uint64_t us = 0;
	uint32_t u32Tick;

	u32Tick = voc_common_get_tick();

	if (timer2_tick_to_us == 0)
		return 0;

	us = u32Tick / timer2_tick_to_us;
	return us;
}

int64_t voc_common_get_sync_time(uint64_t dsp_ts)
{
	struct timespec64 current_time;
	unsigned long save_flags;
	int64_t sync_time = 0;
	uint64_t timer2_us;
	uint64_t dsp_minutes;
	uint64_t dsp_us;
	uint64_t cpu_ts;

	local_irq_save(save_flags);
	timer2_us = voc_common_get_us();
	ktime_get_ts64(&current_time);
	local_irq_restore(save_flags);

	dsp_minutes = dsp_ts / VOICE_US_PER_MINUTE; //us to minute
	dsp_us = dsp_ts - (dsp_minutes * VOICE_US_PER_MINUTE);

	if (timer2_us < dsp_us)
		dsp_minutes++;

	dsp_ts = dsp_minutes * VOICE_US_PER_MINUTE; //minute to us
	dsp_ts += timer2_us;
	dsp_ts *= VOICE_NS_PER_US; //us -> ns

	cpu_ts = current_time.tv_sec * VOICE_NS_PER_SECOND + current_time.tv_nsec;
	sync_time = cpu_ts - dsp_ts;

	return sync_time;
}

void voc_common_clock_set(enum voice_clock clk_bank,
					u32 reg_addr_8bit,
					u16 value,
					u32 start,
					u32 end)
{
	#define VOC_OFFSET_CLKGEN1  0x200
	u32 cpu_addr = 0;

	if (clk_bank == VOC_CLKGEN1)
		cpu_addr += VOC_OFFSET_CLKGEN1;

	cpu_addr += OFFSET8(reg_addr_8bit); //8bit mode to cpu addr

	clock_write(CLK_PM, cpu_addr, value, start, end);
}

int voc_common_dts_init(void)
{
	struct device_node *np;
	uint32_t freq = 0;
	int ret;

	timer2_base = NULL;
	timer2_tick_to_us = 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,mtk-mic-dai");
	if (np) {
		timer2_base = of_iomap(np, VOC_DTS_ID_TIMER2);
		ret = of_property_read_u32(np, "timer2-frequency", &freq);
		if (ret < 0) {
			voc_err("Request property failed, err = %d!!\n",
					ret);
		}
		of_node_put(np);
	}

	if (!timer2_base)
		return -ENODEV;

	timer2_tick_to_us = (freq / VOICE_US_PER_SECOND);

	if (!timer2_tick_to_us)
		return -ENODEV;

	return 0;
}



