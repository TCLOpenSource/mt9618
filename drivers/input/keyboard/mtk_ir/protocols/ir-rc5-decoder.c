// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author MediaTek <MediaTek@mediatek.com>
 */

#include <linux/bitrev.h>
#include <linux/module.h>
#include "../ir_core.h"
#include "../ir_common.h"


static u8 u8InitFlag_rc5 = FALSE;
static u32 speed;
static u8 pre_toggle;
static unsigned long  KeyTime;
static IR_RC5_Spec_t rc5;

#define RC5_STR_2		2
#define RC5_STR_3		3
#define RC5_STR_6		6
#define RC5_STR_16		16
#define RC5_STR_8		8
#define RC5_STR_28		28
#define RC5_STR_255		255
#define RC5_STR_0x0003		0x0003
#define RC5_STR_0x007C0		0x007C0
#define RC5_STR_0x00800		0x00800
#define RC5_STR_0x01000		0x01000
#define RC5_STR_0x40		0x40
#define RC5_STR_0x03		0x03
#define RC5_STR_130		130
#define RC5_STR_0x7		0x7
#define RC5_STR_0xF		0xF
#define RC5_STR_225		255
#define RC5_STR_0x0003F		0x0003F

static u32 databits_to_data(u64 data_bits, u8 bits, u8 reverse)
{
	u32 ret_data = 0;
	u8 i = 0;
	u8 number = bits>>1;

	for (i = 1; i < number+1; i++) {
		ret_data <<= 1;
		if (reverse) {
			if (((data_bits>>(bits-i*RC5_STR_2))&0x03) == 1)
				ret_data |= 0x01;
		} else {
			if (((data_bits>>(bits-i*RC5_STR_2))&0x03) == RC5_STR_2)
				ret_data |= 0x01;
		}
	}
	if (bits%2) {
		ret_data <<= 1;
		if (reverse) {
			if ((data_bits&0x01) == 0)
				ret_data |= 0x01;
		} else {
			if (data_bits&0x01)
				ret_data |= 0x01;
		}
	}
	return ret_data;
}
/* 0 and 1: return 0 and 1 ; 2: on going */
static int searc_support_ir(struct mtk_ir_dev *dev, u8 u8Addr, u8 u8Toggle, u8 u8Cmd)
{
	u8 i = 0;
	IR_RC5_Spec_t *data = &rc5;
	struct ir_scancode *sc = &dev->raw->this_sc;
	struct ir_scancode *prevsc = &dev->raw->prev_sc;

	for (i = 0; i < dev->support_num; i++) {
		if ((u8Addr == dev->support_ir[i].u32HeadCode)
		&& (dev->support_ir[i].eIRType == IR_TYPE_RC5)
		&& (dev->support_ir[i].u8Enable == TRUE)) {
#ifdef CONFIG_MTK_TV_MIRC_INPUT_DEVICE
			sc->scancode = (u8Addr<<RC5_STR_8) | u8Cmd;
			sc->scancode_protocol = (1<<IR_TYPE_RC5);
			speed = dev->support_ir[i].u32IRSpeed;
			dev->map_num = dev->support_ir[i].u32HeadCode;
#else
			sc->scancode = (u8Cmd<<RC5_STR_8) | 0x00;
			sc->scancode |= (0x01UL << RC5_STR_28);
			sc->scancode_protocol = (1<<IR_TYPE_RCA);
			speed = dev->support_ir[i].u32IRSpeed;
#endif
			IRDBG_INFO("[RC5] TIME =%ld\n", (MIRC_Get_System_Time()-KeyTime));
			KeyTime = MIRC_Get_System_Time();
			data->eStatus = STATE_INACTIVE;
			if ((prevsc->scancode_protocol == (1 << IR_TYPE_RC5))
			&& (prevsc->scancode == sc->scancode)
			&& ((MIRC_Get_System_Time()-KeyTime) < RC5_STR_225)) {
				if (pre_toggle == u8Toggle) {
					if (((speed != 0) && (data->u8RepeatTimes >= (speed - 1)))
						|| ((speed == 0)
						&& (data->u8RepeatTimes >= dev->speed))) {
#ifdef CONFIG_MTK_TV_MIRC_INPUT_DEVICE
						dev->raw->u8RepeatFlag = 1;
#else
						sc->scancode |= 0x01;//repeat
#endif
						return 1;
					}
					data->u8RepeatTimes++;
					IRDBG_INFO("[RCA] repeattimes =%d\n", data->u8RepeatTimes);
					return 0;
					}
				}
			pre_toggle = u8Toggle;
			dev->raw->u8RepeatFlag = 0;
			data->u8RepeatTimes = 0;
			return 1;
		}
	}

	return RC5_STR_2;
}
/**
 * ir_rc5_decode() - Decode one RCA pulse or space
 * @dev:	   the struct rc_dev descriptor of the device
 * @duration:  the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int ir_rc5_decode(struct mtk_ir_dev *dev, struct ir_raw_data ev)
{
	IR_RC5_Spec_t *data = &rc5;

	u8 u8Addr = 0;
	u8 u8Toggle = 0;
	u8 u8Cmd = 0;
	u32 u32Data;
	int ret = 0;

	if (!(dev->enabled_protocols & (1<<IR_TYPE_RC5)))
		return -EINVAL;

	switch (data->eStatus) {
	case STATE_INACTIVE:
		if (!ev.pulse)
			break;
		//match RC5 start bit1  1
		if (eq_margin(ev.duration, RC5_BIT_MIN_LWB, RC5_BIT_MIN_UPB)) {
			data->u8BitCount = 0;
			data->u64DataBits = 0;
			data->eStatus = STATE_BIT_DATA;
			return 0;
		} else if (eq_margin(ev.duration, RC5_BIT_MAX_LWB, RC5_BIT_MAX_UPB)) {
			//match RC5 start bit1 with bit2  0_High
			data->u8BitCount = 1;
			data->u64DataBits = 1;
			data->eStatus = STATE_BIT_DATA;
			return 0;
		}
		//else
		break;

	case STATE_BIT_DATA:
		if (eq_margin(ev.duration, RC5_BIT_MIN_LWB, RC5_BIT_MIN_UPB)) {
			data->u8BitCount++;
			data->u64DataBits <<= 1;
			if (!ev.pulse)
				data->u64DataBits |= 1;
		} else if (eq_margin(ev.duration, RC5_BIT_MAX_LWB, RC5_BIT_MAX_UPB)) {
			data->u8BitCount += RC5_STR_2;
			data->u64DataBits <<= RC5_STR_2;
			if (!ev.pulse)
				data->u64DataBits |= RC5_STR_3;
		} else
			goto exit_loop;

		if (data->u8BitCount >= (RC5_NBITS*RC5_STR_2 - RC5_STR_3)) {
			//IRDBG_INFO("[RC5] data->u64DataBits = %llx\n",data->u64DataBits);
			u32Data = databits_to_data(data->u64DataBits, data->u8BitCount, 0);
			//IRDBG_INFO("[RC5] u32Data = %x\n",u32Data);

			u8Cmd	= (u32Data & RC5_STR_0x0003F) >> 0;
			u8Addr   = (u32Data & RC5_STR_0x007C0) >> RC5_STR_6;
			u8Toggle = (u32Data & RC5_STR_0x00800) ? 1 : 0;
			u8Cmd   += (u32Data & RC5_STR_0x01000) ? 0 : RC5_STR_0x40;

			IRDBG_INFO("[RC5] u8Addr = %x u8Cmd = %x u8Toggle = %x\n",
				    u8Addr, u8Cmd, u8Toggle);

			ret = searc_support_ir(dev, u8Addr, u8Toggle, u8Cmd);
			if (ret == 0 || ret == 1)
				return ret;
		} else
			data->eStatus = STATE_BIT_DATA;
		return 0;
	default:
		break;
	}

exit_loop:
	data->eStatus = STATE_INACTIVE;
	return -EINVAL;
}

static struct ir_decoder_handler rc5_handler = {
	.protocols  = (1<<IR_TYPE_RC5),
	.decode	 = ir_rc5_decode,
};

int rc5_decode_init(void)
{
	if (u8InitFlag_rc5 == FALSE) {
		memset(&rc5, 0, sizeof(IR_RC5_Spec_t));
		MIRC_Decoder_Register(&rc5_handler);
		IR_PRINT("[IR Log] RC5 Spec protocol init\n");
		u8InitFlag_rc5 = TRUE;
	} else
		IR_PRINT("[IR Log] RC5 Spec Protocol Has been Initialized\n");

	return 0;
}

void rc5_decode_exit(void)
{
	if (u8InitFlag_rc5 == TRUE) {
		MIRC_Decoder_UnRegister(&rc5_handler);
		u8InitFlag_rc5 = FALSE;
	}
}

