// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author MediaTek <MediaTek@mediatek.com>
 */

#include <linux/bitrev.h>
#include <linux/module.h>
#include "../ir_core.h"
#include "../ir_common.h"

static u8 u8InitFlag_rc6 = FALSE;
static IR_RC6_Spec_t rc6;
static unsigned long  KeyTime;
static u32 u32preData;

#define RC6_STR_2		2
#define RC6_STR_3		3
#define RC6_STR_6		6
#define RC6_STR_16		16
#define RC6_STR_8		8
#define RC6_STR_28		28
#define RC6_STR_255		255
#define RC6_STR_0x0003		0x0003
#define RC6_STR_0x007C0		0x007C0
#define RC6_STR_0x00800		0x00800
#define RC6_STR_0x01000		0x01000
#define RC6_STR_0x40		0x40
#define RC6_STR_0x03		0x03
#define RC6_STR_130		130
#define RC6_STR_0x7		0x7
#define RC6_STR_0xF		0xF
#define RC6_STR_0xFF		0xFF
#define RC6_STR_0x8000		0x8000
#define RC6_STR_0x07		0x07
static u32 databits_to_data(u64 data_bits, u8 bits)
{
	u32 ret_data = 0;
	u8 i = 0;
	u8 number = bits>>1;

	for (i = 1; i < number+1; i++) {
		ret_data <<= 1;
		if (((data_bits >> (bits-i*RC6_STR_2)) & RC6_STR_0x03) == 1)
			ret_data |= 0x01;
	}
	if (bits%2) {
		ret_data <<= 1;
		if ((data_bits&0x01) == 0)
			ret_data |= 0x01;
	}
	return ret_data;
}
/* 0: on going 1:return 1 */
static int searc_support_ir(struct mtk_ir_dev *dev, u16 u16Addr, u32 u32Data, u8 u8Cmd)
{
	u8 i = 0;
	IR_RC6_Spec_t *data = &rc6;
	struct ir_scancode *sc = &dev->raw->this_sc;
	u32 speed = 0;
	u8 u8RepeatTimes = 0;

	for (i = 0; i < dev->support_num; i++) {
		if ((u16Addr == dev->support_ir[i].u32HeadCode)
		&& ((dev->support_ir[i].eIRType == IR_TYPE_RC6_MODE0)
		|| (dev->support_ir[i].eIRType == IR_TYPE_RC6))
		&& (dev->support_ir[i].u8Enable == TRUE)) {
			speed = dev->support_ir[i].u32IRSpeed;
			if (((speed != 0) && (data->u8RepeatTimes >= (speed - 1)))
			|| ((speed == 0) && (data->u8RepeatTimes >= dev->speed))) {
				if ((u32)(MIRC_Get_System_Time() - KeyTime) <= RC6_STR_130) {
					if (u32Data == u32preData)
						dev->raw->u8RepeatFlag = 1;
					}

					KeyTime = MIRC_Get_System_Time();
					u32preData = u32Data;
#ifdef CONFIG_MTK_TV_MIRC_INPUT_DEVICE
					sc->scancode = (u16Addr<<RC6_STR_8) | u8Cmd;
					sc->scancode_protocol = (1<<IR_TYPE_RC6);
					dev->map_num = dev->support_ir[i].u32HeadCode;
#else
					sc->scancode = (u8Cmd<<RC6_STR_8) | 0x00;
					sc->scancode |= (0x01UL << RC6_STR_28);
					sc->scancode_protocol = (1<<IR_TYPE_RC6);
#endif
					memset(data, 0, sizeof(IR_RC6_Spec_t));
					return 1;
			}
		}
	}
	u8RepeatTimes = ++data->u8RepeatTimes;
	IRDBG_INFO("[RC6] repeattimes =%d\n", data->u8RepeatTimes);
	memset(data, 0, sizeof(IR_RC6_Spec_t));
	data->u8RepeatTimes = u8RepeatTimes;
	return 0;
}
/*1: goto ret_ok */
static int datahandle(u32 u32Data, u16 *u16Addr, u8 *u8Toggle, u8 *u8Cmd)
{
	IR_RC6_Spec_t *data = &rc6;

	//data handle
	//MODE_0  command + keycode
	if ((data->u8RC6Mode&0x07) == 0) {
		//Need 16 bit data
		if (data->u8BitCount >= (RC6_MODE0_NBITS*RC6_STR_2 - 1)) {
			u32Data = databits_to_data(data->u64DataBits, data->u8BitCount);
			data->eStatus = STATE_INACTIVE;
			IRDBG_INFO("[RC6] u8RC6Mode: MODE_0\n");
			IRDBG_INFO("[RC6] u32Data =%x\n", u32Data);

			*u16Addr  = (u32Data  >> RC6_STR_8) & RC6_STR_0xFF;
			*u8Cmd	= u32Data & RC6_STR_0xFF;
			// goto ret_ok;
			return 1;
		}
	} else if ((data->u8RC6Mode&0x07) == RC6_STR_6) {
		//Need 32bit data //MODE_6A 2bytes command +2bytes keycode [key value=last byte]
		if (data->u8BitCount >= (RC6_MODE6A_32_NBITS*RC6_STR_2 - 1)) {
			u32Data = databits_to_data(data->u64DataBits, data->u8BitCount);
			data->eStatus = STATE_INACTIVE;
			IRDBG_INFO("[RC6] u8RC6Mode: MODE_6A\n");
			IRDBG_INFO("[RC6] u32Data =%x\n", u32Data);

			*u16Addr  = u32Data  >> RC6_STR_16;
			*u8Toggle = (u32Data & RC6_STR_0x8000) ? 1 : 0;
			*u8Cmd	= u32Data & RC6_STR_0xFF;
			// goto ret_ok;
			return 1;
		}
	} else if ((data->u8RC6Mode&0x07) == 1) {
		IRDBG_ERR("[RC6] u8RC6Mode =%x Decoder Not Support\n", data->u8RC6Mode);
		//return -EINVAL;
	} else {
		//for RC6 else mode [unrealized]
		IRDBG_ERR("[RC6] u8RC6Mode =%x Decoder Not Support\n", data->u8RC6Mode);
		data->eStatus  = STATE_INACTIVE;
		//return -EINVAL;
	}

	return -EINVAL;
}
/**
 * ir_rc6_decode() - Decode one RCA pulse or space
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the pulse violates the state machine
 */
static int ir_rc6_decode(struct mtk_ir_dev *dev, struct ir_raw_data ev)
{
	IR_RC6_Spec_t *data = &rc6;

	u16 u16Addr = 0;
	u8 u8Cmd = 0;
	u32 u32Data = 0;
	u8 u8Toggle = 0;
	u8 u8RepeatTimes = 0;
	int ret = 0;

	if (!(dev->enabled_protocols & (1<<IR_TYPE_RC6)))
		return -EINVAL;

	switch (data->eStatus) {
	case STATE_INACTIVE:
		if (!ev.pulse)
			break;
		if (eq_margin(ev.duration, RC6_HEADER_PULSE_LWB, RC6_HEADER_PULSE_UPB)) {
			data->u8BitCount = 0;
			data->eStatus = STATE_HEADER_SPACE;
			return 0;
		}
		//else
		break;

	case STATE_HEADER_SPACE:
		if (ev.pulse)
			break;
		if (eq_margin(ev.duration, RC6_HEADER_SPACE_LWB, RC6_HEADER_SPACE_UPB)) {
			u8RepeatTimes = data->u8RepeatTimes;
			memset(data, 0, sizeof(IR_RC6_Spec_t));
			data->u8RepeatTimes = u8RepeatTimes;
			data->eStatus = STATE_BIT_DATA;
			return 0;
		}
		break;

	case STATE_BIT_DATA:
		if (eq_margin(ev.duration, RC6_BIT_MIN_LWB, RC6_BIT_MIN_UPB)) {
			data->u8BitCount++;
			data->u64DataBits <<= 1;
			if (!ev.pulse)
				data->u64DataBits |= 0x01;
		} else if (eq_margin(ev.duration, RC6_BIT_MAX_LWB, RC6_BIT_MAX_UPB)) {
			data->u8BitCount += RC6_STR_2;
			data->u64DataBits <<= RC6_STR_2;
			if (!ev.pulse)
				data->u64DataBits |= RC6_STR_0x03;
		} else if (eq_margin(ev.duration, RC6_TRAILER_MAX_LOB, RC6_TRAILER_MAX_UPB)) {
			data->u8BitCount += RC6_STR_3;
			data->u64DataBits <<= RC6_STR_3;
			if (!ev.pulse)
				data->u64DataBits |= RC6_STR_0x07;
		} else {
			IRDBG_INFO("[RC6] Error Bit break Shotcount =%d!!!\n", ev.duration);
			break;
		}

		if (!data->u8DataFlag) {
			if (data->u8BitCount >= TRAILER_BITS) {
				data->u8RC6Mode = databits_to_data(
				((data->u64DataBits)>>(data->u8BitCount - MODE_BITS)), MODE_BITS);

				data->u8DataFlag = 1;
				data->u8BitCount = data->u8BitCount - TRAILER_BITS;
				data->u64DataBits &= ~((~0UL)<<(data->u8BitCount));
			}
		} else {
			ret = datahandle(u32Data, &u16Addr, &u8Toggle, &u8Cmd);
			if (ret == -EINVAL)
				return -EINVAL;
			ret = searc_support_ir(dev, u16Addr, u32Data, u8Cmd);

			if (ret == 1)
				return 1;

		}
		return -EINVAL;
	default:
		break;
	}

	data->eStatus = STATE_INACTIVE;
	return -EINVAL;
}

static struct ir_decoder_handler rc6_handler = {
	.protocols = (1<<IR_TYPE_RC6),
	.decode	= ir_rc6_decode,
};

int rc6_decode_init(void)
{
	if (u8InitFlag_rc6 == FALSE) {
		memset(&rc6, 0, sizeof(IR_RC6_Spec_t));
		MIRC_Decoder_Register(&rc6_handler);
		IR_PRINT("[IR Log] RC6 Spec protocol init\n");
		u8InitFlag_rc6 = TRUE;
	} else
		IR_PRINT("[IR Log] RC6 Spec Protocol Has been Initialized\n");

	return 0;
}

void rc6_decode_exit(void)
{
	if (u8InitFlag_rc6 == TRUE) {
		MIRC_Decoder_UnRegister(&rc6_handler);
		u8InitFlag_rc6 = FALSE;
	}
}
