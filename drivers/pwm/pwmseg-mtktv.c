// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author Owen.Tseng <Owen.Tseng@mediatek.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "pwm-mt5896-riu.h"
#include <linux/clk.h>
#include <linux/delay.h>


struct mtk_segpwm_dat {
	unsigned int vsync_p;
	unsigned int period;
	unsigned int duty_cycle;
	unsigned int shift;
	unsigned int sync_times;
	bool enable;
	bool oneshot;
};

struct mtk_segpwm {
	struct pwm_chip chip;
	struct mtk_segpwm_dat *data;
	void __iomem *base_x;
	void __iomem *base_y;
	void __iomem *base;
	void __iomem *base_n;
	void __iomem *base_ckgen;
	void __iomem *base_comb;
	struct clk *clk_gate_xtal;
	struct clk *clk_gate_pwm;
	unsigned int channel;
	unsigned int scanpad_ch;
	unsigned int segdbg;
	unsigned int comb_ch;
	unsigned int pre_syn_t;
	unsigned int comb_event;
	struct mutex pwm_mutex;
	u32 pre_duty;
};

#define MAX_PWM_HW_NUM				4
static struct pwm_chip *_ptr_array[MAX_PWM_HW_NUM];
#define PWM_REG_SHIFT_INDEX		(0x2 << 2)

#define LENSIZE					32
#define REG_0004_PWM			0x04
#define CKEN_ALL_EN				(0x1 << 6)
#define REG_00DC_PWM_SEG		0xDC
#define REG_01CC_PWM_SEG		0x1CC
#define REG_0160_PWM_SEG		0x160
#define REG_0014_PWM_SEG		0x14
#define REG_0010_PWM_SEG		0x10
#define REG_0020_PWM_SEG		0x20
#define REG_0024_PWM_SEG		0x24
#define REG_0028_PWM_SEG		0x28
#define REG_002C_PWM_SEG		0x2C
#define REG_0030_PWM_SEG		0x30
#define REG_0034_PWM_SEG		0x34
#define REG_0038_PWM_SEG		0x38
#define REG_003C_PWM_SEG		0x3C
#define REG_0008_PWM_SEG		0x08

#define SEG_OUT_OFFSET			0x4
#define SEG_VRESET_SET			(0x1 << 0)
#define SEG_VSYNC_SYNC_0		(0x1 << 8)
#define SEG_VSYNC_SYNC_1		(0x1 << 9)
#define VSYNC_PULSE_SEL			(3)
#define V_16384_P		(16384)
#define HALF_DUTY			(50)
#define SEG_EVENT_SET			(1 << 14)
#define SEG_EVENT_EN			(1 << 15)
#define SEG_PULS_BYPASS_SET		(1 << 4)
#define SEG_2_T_V		(2)
#define SEG_4_T_V		(4)
#define SEG_8_T_V		(8)
#define SEG_10_T_V		(10)
#define SEG_16_T_V		(16)
#define FULL_D			(100)
#define SEG_EVENTS_DEFAULT		(2)
#define SEG_PWM01_COMB_REG		(0xE0)
#define SEG_PWM01_COB_SET		(3 << 12)
#define SEG_PWM23_COB_SET		(3 << 10)
#define SEG_PWM01_PORT_SET		0
#define SEG_PWM2_PORT_SEL		(3 << 8)
#define SEG_PWM3_PORT_SEL		(4 << 12)
#define SEG_PWM_OFFSET			(0x40)

#define REG_0048_PWM_SEG		0x48

#define REG_0060_PWM_SEG		0x60
#define REG_0064_PWM_SEG		0x64
#define REG_0068_PWM_SEG		0x68
#define REG_006C_PWM_SEG		0x6C
#define REG_0070_PWM_SEG		0x70
#define REG_0074_PWM_SEG		0x74
#define REG_0078_PWM_SEG		0x78
#define REG_007C_PWM_SEG		0x7C

#define OUT_SEL_SEG01			0x21
#define REG_0050_PWM_SEG		0x50
#define REG_PWM0_VDBEN_SW		0x10 //0x4*4
#define REG_PWM1_VDBEN_SW		0x1C //0x7*4
#define PWM_VDBEN_SW_SET		(1 << 14)
#define SEG_PWM0_OUT			0x0700
#define SEG_PWM1_OUT			0x0106
#define LOW_WORD_MSK			0xFF00
#define HIGH_HALF_WORD_MSK		0xF0FF
#define SEG_PWM0_OUT_SINGLE		0x01
#define SEG_PWM1_OUT_SINGLE		0x20
#define SEG_PWM12_OUT			0x21
#define FRAME_DELAY_MS			20
#define HALF_SEG_BYTE			4
#define BYTE_SHIFT				8
#define CLK0_1_EN				3
#define IDX_2					2
#define IDX_3					3
#define IDX_4					4
#define IDX_5					5
#define V_60					60

static inline struct mtk_segpwm *to_mtk_segpwm(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_segpwm, chip);
}

static inline u16 dtvsegpwm_rd(struct mtk_segpwm *mdp, u32 reg)
{
	return mtk_readw_relaxed((mdp->base + reg));
}

static inline u8 dtvsegpwm_rdl(struct mtk_segpwm *mdp, u16 reg)
{
	return dtvsegpwm_rd(mdp, reg)&0xff;
}

static inline void dtvsegpwm_wr(struct mtk_segpwm *mdp, u16 reg, u32 val)
{
	mtk_writew_relaxed((mdp->base + reg), val);
}

static inline void dtvsegpwm_wrl(struct mtk_segpwm *mdp, u16 reg, u8 val)
{
	u16 val16 = dtvsegpwm_rd(mdp, reg)&LOW_WORD_MSK;

	val16 |= val;
	dtvsegpwm_wr(mdp, reg, val16);
}

static void mtk_segpwm_oen(struct mtk_segpwm *mdp, unsigned int data)
{
	u16 bitop_tmp;
	u16 bitop_org;
	u16 reg_tmp;
	u16 reg_org;
	u16 shift_chtmp;

	reg_org = ((mdp->scanpad_ch) >> 1)*SEG_OUT_OFFSET;
	bitop_org = mtk_readw_relaxed(mdp->base_n + REG_01CC_PWM_SEG + reg_org);
	shift_chtmp = mdp->scanpad_ch >> 1;
	/*base_n port_switch*/
	if (data) {
		reg_tmp = ((mdp->channel) >> shift_chtmp)&1;
		bitop_tmp = bitop_org;
		if (reg_tmp) {
			bitop_tmp &= HIGH_HALF_WORD_MSK;
			bitop_tmp |= ((mdp->channel) << BYTE_SHIFT);
		} else {
			bitop_tmp &= LOW_WORD_MSK;
			bitop_tmp |= (mdp->channel);
		}
	} else
		bitop_tmp = bitop_org;

	mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG + reg_org, bitop_tmp);
	/*PWM_SEGx output select*/
	if ((mdp->scanpad_ch) < SEG_OUT_OFFSET) {
		reg_org = (mdp->scanpad_ch)*SEG_OUT_OFFSET;
		bitop_org = mtk_readw_relaxed(mdp->base_y);
		bitop_tmp = ((mdp->channel)+1) << reg_org;
		bitop_tmp |= bitop_org;
		mtk_writew_relaxed(mdp->base_y, bitop_tmp);
	}
	if ((mdp->comb_ch)) {
		bitop_org = OUT_SEL_SEG01;
		mtk_writew_relaxed(mdp->base_y, bitop_org);
	}
}

static void mtk_segpwm_cken(struct mtk_segpwm *mdp)
{
	u16 reg_tmp;

	reg_tmp = mtk_readw_relaxed(mdp->base_x);
	reg_tmp |= (1 << (mdp->channel));
	mtk_writew_relaxed(mdp->base_x, reg_tmp);
	reg_tmp = mtk_readw_relaxed(mdp->base_ckgen + REG_0004_PWM);
	reg_tmp |= CKEN_ALL_EN;
	mtk_writew_relaxed(mdp->base_ckgen + REG_0004_PWM, reg_tmp);

	if (mdp->comb_ch) {
		/*Combine SEGPWM01*/
		reg_tmp = mtk_readw_relaxed(mdp->base_ckgen + SEG_PWM01_COMB_REG);
		reg_tmp |= SEG_PWM01_COB_SET;
		mtk_writew_relaxed(mdp->base_ckgen + SEG_PWM01_COMB_REG, reg_tmp);
		/*seg0/1 clock enable*/
		reg_tmp = mtk_readw_relaxed(mdp->base_x);
		reg_tmp |= CLK0_1_EN;
		mtk_writew_relaxed(mdp->base_x, reg_tmp);
	}
}


static void mtk_segpwm_vsync(struct mtk_segpwm *mdp, unsigned int data)
{
	u16 reg_tmp;

	reg_tmp = dtvsegpwm_rd(mdp, REG_0014_PWM_SEG);
	if (!data) {
		reg_tmp |= SEG_VRESET_SET;
		reg_tmp &= ~SEG_VSYNC_SYNC_0;
		reg_tmp &= ~SEG_VSYNC_SYNC_1;
	} else {
		/*reset by n times Vsync*/
		reg_tmp |= SEG_VRESET_SET;
		reg_tmp |= SEG_VSYNC_SYNC_0;
		reg_tmp &= ~SEG_VSYNC_SYNC_1;
		reg_tmp |= (data << HALF_SEG_BYTE);
	}
	dtvsegpwm_wr(mdp, REG_0014_PWM_SEG, reg_tmp);
	reg_tmp = dtvsegpwm_rd(mdp, REG_0010_PWM_SEG);
	reg_tmp |= VSYNC_PULSE_SEL;
	reg_tmp |= SEG_PULS_BYPASS_SET;
	dtvsegpwm_wr(mdp, REG_0010_PWM_SEG, reg_tmp);
	if (mdp->comb_ch) {
		reg_tmp = mtk_readw_relaxed(mdp->base_comb + REG_0010_PWM_SEG);
		reg_tmp |= VSYNC_PULSE_SEL;
		reg_tmp |= SEG_PULS_BYPASS_SET;
		mtk_writew_relaxed(mdp->base_comb + REG_0010_PWM_SEG, reg_tmp);

		reg_tmp = mtk_readw_relaxed(mdp->base_comb + REG_0050_PWM_SEG);
		reg_tmp |= VSYNC_PULSE_SEL;
		reg_tmp |= SEG_PULS_BYPASS_SET;
		mtk_writew_relaxed(mdp->base_comb + REG_0050_PWM_SEG, reg_tmp);
	}
}

static void mtk_seg_event2_switch(struct mtk_segpwm *mdp, u8 duty_t)
{
	u32 position = 0;
	u32 counter = 0;
	u32 counter_tmp = 0;
	u8 vsync_t = 0;
	u16 ctrl = 0;

	vsync_t = SEG_2_T_V;
	if (mdp->comb_event) {
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, 0);

		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, 0);
		if (mdp->channel == 0) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM0_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM0_OUT);
		} else if (mdp->channel == 1) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM1_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM1_OUT);
		}
		mdp->comb_event = 0;
	} else {
		dtvsegpwm_wr(mdp, REG_0028_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_0030_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_0034_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_0038_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_003C_PWM_SEG, 0);
	}
	counter = (mdp->data->shift)*(V_16384_P/(HALF_DUTY*vsync_t));
	if (counter) {
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		position |= counter;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		counter_tmp = counter + (mdp->pre_duty*((V_16384_P)/(HALF_DUTY*vsync_t)));
		counter += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		mdp->pre_duty = duty_t;
		if ((counter_tmp >= V_16384_P) && (counter < V_16384_P)) {
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= V_16384_P - 1;
			position |= SEG_EVENT_EN;
			dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mdelay(FRAME_DELAY_MS);
		} else {
			if (counter >= V_16384_P)
				counter -= V_16384_P;
			position = 0;
			position |= counter;
			position &= ~SEG_EVENT_SET;//0
			position |= SEG_EVENT_EN;
			dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
		}
	} else {
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
	}
}

static void mtk_seg_event4_switch(struct mtk_segpwm *mdp, u8 duty_t)
{
	u32 position = 0;
	u32 counter = 0;
	u32 counter_tmp = 0;
	u8 vsync_t = 0;
	u16 ctrl = 0;

	vsync_t = SEG_4_T_V;
	if (mdp->comb_event) {
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, 0);

		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, 0);
		if (mdp->channel == 0) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM0_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM0_OUT);
		} else if (mdp->channel == 1) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM1_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM1_OUT);
		}
		mdp->comb_event = 0;
	} else {
		dtvsegpwm_wr(mdp, REG_0030_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_0034_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_0038_PWM_SEG, 0);
		dtvsegpwm_wr(mdp, REG_003C_PWM_SEG, 0);
	}
	counter = ((V_16384_P >> 1)*(mdp->data->shift))/FULL_D;
	if (counter) {
		position = 0;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		counter = (duty_t + (mdp->data->shift))*(V_16384_P >> 1);
		counter /= FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position &= ~SEG_EVENT_SET;//0
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
		counter = ((V_16384_P >> 1)*(mdp->data->shift))/FULL_D + (V_16384_P >> 1);
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0028_PWM_SEG, position);
		counter_tmp = (V_16384_P >> 1) + ((V_16384_P >> 1)*(mdp->pre_duty +
					  (mdp->data->shift)))/FULL_D;
		counter = (V_16384_P >> 1) + ((V_16384_P >> 1)*(duty_t +
				  (mdp->data->shift)))/FULL_D;
		mdp->pre_duty = duty_t;
		if ((counter_tmp >= V_16384_P) && (counter < V_16384_P)) {
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= V_16384_P - 1;
			position |= SEG_EVENT_EN;
			dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mdelay(FRAME_DELAY_MS);
		} else {
			if (counter >= V_16384_P)
				counter -= V_16384_P;
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= counter;
			position |= SEG_EVENT_EN;
			dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
	} else {
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
		position += (FULL_D - duty_t)*
					((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		dtvsegpwm_wr(mdp, REG_0028_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, position);
	}
}

static void mtk_seg_event8_switch(struct mtk_segpwm *mdp, u8 duty_t)
{
	u32 position = 0;
	u32 counter = 0;
	u32 counter_tmp = 0;
	u8 vsync_t = 0;
	u16 ctrl = 0;

	vsync_t = SEG_8_T_V;
	if (mdp->comb_event) {
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, 0);

		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, 0);
		mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, 0);
		if (mdp->channel == 0) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM0_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM0_OUT);
		} else if (mdp->channel == 1) {
			mtk_writew_relaxed(mdp->base_y, SEG_PWM1_OUT_SINGLE);
			mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, SEG_PWM1_OUT);
		}
		mdp->comb_event = 0;
	}
	counter = ((V_16384_P/(SEG_8_T_V >> 1))*(mdp->data->shift))/FULL_D;
	if (counter) {
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		position |= counter;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		counter_tmp = counter + (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position &= ~SEG_EVENT_SET;//0
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0028_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position &= ~SEG_EVENT_SET;//0
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0030_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position &= ~SEG_EVENT_SET;//0
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0034_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0038_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		mdp->pre_duty = duty_t;
		if ((counter_tmp >= V_16384_P) && (counter < V_16384_P)) {
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= V_16384_P - 1;
			position |= SEG_EVENT_EN;
			mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mdelay(FRAME_DELAY_MS);
		} else {
			if (counter >= V_16384_P)
				counter -= V_16384_P;
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= counter;
			position |= SEG_EVENT_EN;
			dtvsegpwm_wr(mdp, REG_003C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
	} else {
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		dtvsegpwm_wr(mdp, REG_0020_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_0024_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		dtvsegpwm_wr(mdp, REG_0028_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_002C_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		dtvsegpwm_wr(mdp, REG_0030_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_0034_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		dtvsegpwm_wr(mdp, REG_0038_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		dtvsegpwm_wr(mdp, REG_003C_PWM_SEG, position);
	}
}

static void mtk_seg_event10_switch(struct mtk_segpwm *mdp, u8 duty_t)
{
	u32 position = 0;
	u32 counter = 0;
	u32 counter_tmp = 0;
	u8 vsync_t = 0;
	u8 vsync_tmp = 0;
	u16 ctrl = 0;

	mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, 0);
	mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, 0);
	mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, 0);
	mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, 0);
	mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, 0);
	mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, 0);
	vsync_tmp = SEG_10_T_V;
	vsync_t = SEG_10_T_V >> 1;
	counter = ((V_16384_P/vsync_t)*(mdp->data->shift))/FULL_D;
	mtk_writew_relaxed(mdp->base_y, SEG_PWM12_OUT);
	if (counter) {
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		position |= counter;
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, position);
		counter_tmp = counter + (mdp->pre_duty*(V_16384_P/vsync_t))/FULL_D;
		counter += (duty_t*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position &= ~SEG_EVENT_SET;//0
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*(V_16384_P/vsync_t))/FULL_D;
		counter += ((FULL_D - duty_t)*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*(V_16384_P/vsync_t))/FULL_D;
		counter += (duty_t*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position &= ~SEG_EVENT_SET;//0
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/vsync_t))/FULL_D;
		counter += ((FULL_D - duty_t)*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*(V_16384_P/vsync_t))/FULL_D;
		counter += (duty_t*(V_16384_P/vsync_t))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*(V_16384_P/vsync_t))/FULL_D;
		counter += ((FULL_D - duty_t)*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*(V_16384_P/vsync_t))/FULL_D;
		counter += (duty_t*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, position);

		/*SEG PWM1*/
		counter_tmp += ((FULL_D - mdp->pre_duty)*(V_16384_P/vsync_t))/FULL_D;
		counter += ((FULL_D - duty_t)*(V_16384_P/vsync_t))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*(V_16384_P/vsync_t))/FULL_D;
		counter += (duty_t*(V_16384_P/vsync_t))/FULL_D;
		mdp->pre_duty = duty_t;
		if ((counter_tmp >= V_16384_P) && (counter < V_16384_P)) {
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= V_16384_P - 1;
			position |= SEG_EVENT_EN;
			mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mdelay(FRAME_DELAY_MS);
		} else {
			if (counter >= V_16384_P)
				counter -= V_16384_P;
			position = 0;
			position |= counter;
			position |= SEG_EVENT_EN;
			position &= ~SEG_EVENT_SET;//0
			mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, position);
		}
	} else {
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, position);

		/*SEG PWM1*/
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_tmp));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, position);
	}
}

static void mtk_seg_event16_switch(struct mtk_segpwm *mdp, u8 duty_t)
{
	u32 position = 0;
	u32 counter = 0;
	u32 counter_tmp = 0;
	u8 vsync_t = 0;
	u16 ctrl = 0;

	vsync_t = SEG_16_T_V;
	/*Combine port for pwmseg0*/
	mtk_writew_relaxed(mdp->base_n + REG_01CC_PWM_SEG, 0);
	counter = ((V_16384_P/(SEG_16_T_V >> 1))*(mdp->data->shift))/FULL_D;
	mtk_writew_relaxed(mdp->base_y, SEG_PWM12_OUT);
	if (counter) {
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		position |= counter;
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, position);
		counter_tmp = counter + (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position &= ~SEG_EVENT_SET;//0
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position &= ~SEG_EVENT_SET;//0
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter > V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, position);

		/*SEG PWM1*/
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= SEG_EVENT_SET;//1
		position |= counter;
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, position);
		counter_tmp += ((FULL_D - mdp->pre_duty)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += ((FULL_D - duty_t)*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		if (counter >= V_16384_P)
			counter -= V_16384_P;
		position = 0;
		position |= counter;
		position |= SEG_EVENT_EN;
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, position);
		counter_tmp += (mdp->pre_duty*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		counter += (duty_t*((V_16384_P)/(vsync_t >> 1)))/FULL_D;
		mdp->pre_duty = duty_t;
		if ((counter_tmp >= V_16384_P) && (counter < V_16384_P)) {
			position = 0;
			position &= ~SEG_EVENT_SET;//0
			position |= V_16384_P - 1;
			position |= SEG_EVENT_EN;
			mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mdelay(FRAME_DELAY_MS);
		} else {
			if (counter >= V_16384_P)
				counter -= V_16384_P;
			position = 0;
			position |= counter;
			position |= SEG_EVENT_EN;
			position &= ~SEG_EVENT_SET;//0
			mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, position);
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
	} else {
		position |= SEG_EVENT_SET;//1
		position |= SEG_EVENT_EN;
		mtk_writew_relaxed(mdp->base_comb + REG_0020_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0024_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0028_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_002C_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0030_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0034_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0038_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_003C_PWM_SEG, position);

		/*SEG PWM1*/
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0060_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0064_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0068_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_006C_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0070_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_0074_PWM_SEG, position);
		position += (FULL_D - duty_t)*((V_16384_P)/(HALF_DUTY*vsync_t));
		position |= SEG_EVENT_SET;//1
		mtk_writew_relaxed(mdp->base_comb + REG_0078_PWM_SEG, position);
		position += duty_t*((V_16384_P)/(HALF_DUTY*vsync_t));
		position &= ~SEG_EVENT_SET;//0
		mtk_writew_relaxed(mdp->base_comb + REG_007C_PWM_SEG, position);
	}
}

static int mtk_segpwm_config(struct pwm_chip *chip,
							struct pwm_device *segpwm,
						int duty_t, int period_t)
{
	struct mtk_segpwm *mdp = to_mtk_segpwm(chip);
	u8 vsync_t = 0;
	u16 ctrl = 0;

	if (!mdp->data->oneshot) {
		mtk_segpwm_oen(mdp, 1);
		mtk_segpwm_vsync(mdp, 0);
		mdp->data->oneshot = 1;
	}

	if (!mdp->data->vsync_p)
		return 0;

	if (mdp->segdbg)
		mdp->data->sync_times = SEG_EVENTS_DEFAULT*(period_t / mdp->data->vsync_p);

	mutex_lock(&mdp->pwm_mutex);
	ctrl = mtk_readw_relaxed(mdp->base_comb + REG_PWM0_VDBEN_SW);
	ctrl &= ~PWM_VDBEN_SW_SET;
	if (((mdp->data->duty_cycle) != duty_t) || ((mdp->pre_syn_t) != mdp->data->sync_times)) {
		vsync_t = mdp->data->sync_times;
		mdp->pre_syn_t = vsync_t;
		if (vsync_t == SEG_2_T_V) {
			/*HW Mode SW VDBEN Set 0*/
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mtk_seg_event2_switch(mdp, duty_t);
			/*HW Mode SW VDBEN Set 0*/
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}

		if (vsync_t == SEG_4_T_V) {
			/*HW Mode SW VDBEN Set 0*/
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mtk_seg_event4_switch(mdp, duty_t);
			/*HW Mode SW VDBEN Set 0*/
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}

		if (vsync_t == SEG_8_T_V) {
			/*HW Mode SW VDBEN Set 0*/
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mtk_seg_event8_switch(mdp, duty_t);
			/*HW Mode SW VDBEN Set 0*/
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
		if (vsync_t == SEG_10_T_V) {
			/*HW Mode SW VDBEN Set 0*/
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mtk_seg_event10_switch(mdp, duty_t);
			mtk_writew_relaxed(mdp->base_comb + REG_0008_PWM_SEG, 1);
			mtk_writew_relaxed(mdp->base_comb + REG_0048_PWM_SEG, 1);
			mdp->comb_event = 1;
			/*HW Mode SW VDBEN Set 0*/
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
		if (vsync_t == SEG_16_T_V) {
			/*HW Mode SW VDBEN Set 0*/
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
			mtk_seg_event16_switch(mdp, duty_t);
			mtk_writew_relaxed(mdp->base_comb + REG_0008_PWM_SEG, 1);
			mtk_writew_relaxed(mdp->base_comb + REG_0048_PWM_SEG, 1);
			mdp->comb_event = 1;
			/*HW Mode SW VDBEN Set 0*/
			ctrl |= PWM_VDBEN_SW_SET;
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM0_VDBEN_SW, ctrl);
			mtk_writew_relaxed(mdp->base_ckgen + REG_PWM1_VDBEN_SW, ctrl);
		}
		dtvsegpwm_wr(mdp, REG_0008_PWM_SEG, 1);
		mdp->data->duty_cycle = duty_t;
	}
	mutex_unlock(&mdp->pwm_mutex);
	return 0;
}

static int mtk_segpwm_enable(struct pwm_chip *chip, struct pwm_device *segpwm)
{
	struct mtk_segpwm *mdp = to_mtk_segpwm(chip);

	mdp->data->enable = 1;
	mtk_segpwm_oen(mdp, 1);
	return 0;
}

static void mtk_segpwm_disable(struct pwm_chip *chip, struct pwm_device *segpwm)
{
	struct mtk_segpwm *mdp = to_mtk_segpwm(chip);

	mdp->data->enable = 0;
	mtk_segpwm_oen(mdp, 0);
}

static void mtk_segpwm_get_state(struct pwm_chip *chip,
							struct pwm_device *segpwm,
							struct pwm_state *state)
{
	struct mtk_segpwm *mdp = to_mtk_segpwm(chip);

	state->period = mdp->data->period;
	state->duty_cycle = mdp->data->duty_cycle;
	state->polarity = 0;
	state->enabled = mdp->data->enable;
}

static const struct pwm_ops mtk_segpwm_ops = {
	.config = mtk_segpwm_config,
	.enable = mtk_segpwm_enable,
	.disable = mtk_segpwm_disable,
	.get_state = mtk_segpwm_get_state,
	.owner = THIS_MODULE,
};

static ssize_t shift_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);

	return scnprintf(buf, LENSIZE, "%d\n", mdp->data->shift);
}

static ssize_t shift_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &val);

	mdp->data->shift = val;
	return ret ? : size;
}
static DEVICE_ATTR_RW(shift);


static ssize_t segdbg_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);

	return scnprintf(buf, LENSIZE, "%d\n", mdp->segdbg);
}

static ssize_t segdbg_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &val);

	mdp->segdbg = val;
	return ret ? : size;
}
static DEVICE_ATTR_RW(segdbg);


static ssize_t vsync_p_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);

	return scnprintf(buf, LENSIZE, "%d\n", mdp->data->vsync_p);
}

static ssize_t vsync_p_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(child);
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &val);

	mdp->data->vsync_p = val;
	return ret ? : size;
}
static DEVICE_ATTR_RW(vsync_p);


static struct attribute *pwm_extend_attrs[] = {
	&dev_attr_shift.attr,
	&dev_attr_vsync_p.attr,
	&dev_attr_segdbg.attr,
	NULL,
};
static const struct attribute_group pwm_extend_attr_group = {
	.name = "pwm_extend",
	.attrs = pwm_extend_attrs
};


int mtk_segpwm_set_attr(u8 channel, struct mtk_segpwm_dat *scandat)
{
	struct pwm_chip *pwmchip = _ptr_array[channel];
	struct mtk_segpwm *mdp = to_mtk_segpwm(pwmchip);

	mdp->data->shift = scandat->shift;
	mdp->data->enable = scandat->enable;
	mdp->data->sync_times = scandat->sync_times;
	mtk_segpwm_config(pwmchip, NULL, scandat->duty_cycle, scandat->period);
	//mdp->data->duty_cycle = scandat->duty_cycle;
	mdp->data->period = scandat->period;
	return 0;
}
EXPORT_SYMBOL(mtk_segpwm_set_attr);


static int mtk_segpwm_probe(struct platform_device *pdev)
{
	struct mtk_segpwm *mdp;
	int ret;
	struct device_node *np = NULL;
	struct mtk_segpwm_dat *scandat;
	int err = -ENODEV;

	mdp = devm_kzalloc(&pdev->dev, sizeof(*mdp), GFP_KERNEL);
	scandat = devm_kzalloc(&pdev->dev, sizeof(*scandat), GFP_KERNEL);
	np = pdev->dev.of_node;

	if ((!mdp) || (!scandat))
		return -ENOMEM;

	if (!np) {
		dev_info(&pdev->dev, "Failed to find node\n");
		ret = -EINVAL;
		return ret;
	}

	memset(scandat, 0, sizeof(*scandat));
	mdp->data = scandat;
	mdp->chip.dev = &pdev->dev;
	mdp->chip.ops = &mtk_segpwm_ops;
	/*It will return error without this setting*/
	mdp->chip.npwm = 1;
	mdp->chip.base = -1;

	err = of_property_read_u32(pdev->dev.of_node,
				"channel", &mdp->channel);
	if (err)
		dev_info(&pdev->dev, "could not get channel resource\n");
	else
		mdp->scanpad_ch = mdp->channel;


	err = of_property_read_u32(pdev->dev.of_node,
				"vsync-freq", &mdp->data->vsync_p);
	if (err)
		dev_info(&pdev->dev, "could not get vsync-freq resource\n");

	err = of_property_read_u32(pdev->dev.of_node,
				"comb-ch", &mdp->comb_ch);
	if (err)
		dev_info(&pdev->dev, "could not get comb-ch resource\n");

	mdp->base_x = of_iomap(np, 0);
	mdp->base_y = of_iomap(np, 1);
	mdp->base = of_iomap(np, IDX_2);
	mdp->base_n = of_iomap(np, IDX_3);
	mdp->base_ckgen = of_iomap(np, IDX_4);
	mdp->base_comb = of_iomap(np, IDX_5);

	if (!mdp->base)
		return -EINVAL;
	mtk_segpwm_cken(mdp);
	mdp->clk_gate_xtal = devm_clk_get(&pdev->dev, "xtal_12m2pwm");
	if (IS_ERR_OR_NULL(mdp->clk_gate_xtal)) {
		if (IS_ERR(mdp->clk_gate_xtal))
			dev_info(&pdev->dev, "err mdp->clk_gate_xtal=%#lx\n",
				(unsigned long)mdp->clk_gate_xtal);
		else
			dev_info(&pdev->dev, "err mdp->clk_gate_xtal = NULL\n");
	}
	ret = clk_prepare_enable(mdp->clk_gate_xtal);
	if (ret)
		dev_info(&pdev->dev, "Failed to enable xtal_12m2pwm: %d\n", ret);

	mdp->clk_gate_pwm = devm_clk_get(&pdev->dev, "pwm_ck");
	if (IS_ERR_OR_NULL(mdp->clk_gate_pwm)) {
		if (IS_ERR(mdp->clk_gate_pwm))
			dev_info(&pdev->dev, "err mdp->clk_gate_pwm=%#lx\n",
				(unsigned long)mdp->clk_gate_pwm);
		else
			dev_info(&pdev->dev, "err mdp->clk_gate_pwm = NULL\n");
	}
	ret = clk_prepare_enable(mdp->clk_gate_pwm);
	if (ret)
		dev_info(&pdev->dev, "Failed to enable pwm_ck: %d\n", ret);

	ret = pwmchip_add(&mdp->chip);

	if (mdp->channel < MAX_PWM_HW_NUM)
		_ptr_array[mdp->channel] = &mdp->chip;

	if (ret < 0)
		dev_err(&pdev->dev, "segpwmchip_add() failed: %d\n", ret);

	platform_set_drvdata(pdev, mdp);

	ret = sysfs_create_group(&pdev->dev.kobj, &pwm_extend_attr_group);
	if (ret < 0)
		dev_err(&pdev->dev, "sysfs_create_group failed: %d\n", ret);

	mutex_init(&mdp->pwm_mutex);
	return ret;
}

static int mtk_segpwm_remove(struct platform_device *pdev)
{
	struct mtk_segpwm *mdp = platform_get_drvdata(pdev);

	if (!(pdev->name) ||
		strcmp(pdev->name, "mediatek,mt5896-segpwm") || pdev->id != 0)
		return -1;

	pwmchip_remove(&mdp->chip);
	clk_unprepare(mdp->clk_gate_xtal);
	clk_unprepare(mdp->clk_gate_pwm);

	pdev->dev.platform_data = NULL;
	return 0;
}
#ifdef CONFIG_PM
static int mtk_dtv_pwmscan_suspend(struct device *pdev)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(pdev);

	if (!mdp)
		return -EINVAL;
	mdp->data->oneshot = 0;
	if (!IS_ERR_OR_NULL(mdp->clk_gate_xtal))
		clk_disable_unprepare(mdp->clk_gate_xtal);

	if (!IS_ERR_OR_NULL(mdp->clk_gate_pwm))
		clk_disable_unprepare(mdp->clk_gate_pwm);

	return 0;
}

static int mtk_dtv_pwmscan_resume(struct device *pdev)
{
	struct mtk_segpwm *mdp = dev_get_drvdata(pdev);
	int ret = 0;

	if (!mdp)
		return -EINVAL;

	if (!IS_ERR_OR_NULL(mdp->clk_gate_xtal)) {
		ret = clk_prepare_enable(mdp->clk_gate_xtal);
		if (ret)
			dev_info(pdev, "resume failed to enable xtal_12m2pwm: %d\n", ret);
	}
	if (!IS_ERR_OR_NULL(mdp->clk_gate_pwm)) {
		ret = clk_prepare_enable(mdp->clk_gate_pwm);
		if (ret)
			dev_info(pdev, "resume failed to enable pwm_ck: %d\n", ret);
	}
	mtk_segpwm_cken(mdp);
	mdp->data->vsync_p = V_60;
	return 0;
}
#endif

#if defined(CONFIG_OF)
static const struct of_device_id mtksegpwm_of_device_ids[] = {
		{.compatible = "mediatek,mt5896-segpwm" },
		{},
};
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops pwm_pwmscan_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mtk_dtv_pwmscan_suspend, mtk_dtv_pwmscan_resume)
};
#endif
static struct platform_driver mtk_segpwm_driver = {
	.probe		= mtk_segpwm_probe,
	.remove		= mtk_segpwm_remove,
	.driver		= {
#if defined(CONFIG_OF)
	.of_match_table = mtksegpwm_of_device_ids,
#endif
#ifdef CONFIG_PM
	.pm	= &pwm_pwmscan_pm_ops,
#endif
	.name   = "mediatek,mt5896-segpwm",
	.owner  = THIS_MODULE,
}
};

static int __init mtk_segpwm_init_module(void)
{
	int retval = 0;

	retval = platform_driver_register(&mtk_segpwm_driver);

	return retval;
}

static void __exit mtk_segpwm_exit_module(void)
{
	platform_driver_unregister(&mtk_segpwm_driver);
}

module_init(mtk_segpwm_init_module);
module_exit(mtk_segpwm_exit_module);


MODULE_AUTHOR("Owen.Tseng@mediatek.com");
MODULE_DESCRIPTION("segpwm driver");
MODULE_LICENSE("GPL");

