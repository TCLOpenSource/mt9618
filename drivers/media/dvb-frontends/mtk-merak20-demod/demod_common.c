// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

#include <asm/io.h>
#include <linux/delay.h>
#include "demod_common.h"

#define USE_RIU_RW_IN_MBX 1

#define RIU_READ_BYTE(addr)       (readb(virt_riu_addr + (addr)))
#define RIU_WRITE_BYTE(addr, val)   (writeb(val, virt_riu_addr + (addr)))

#define RIU_READBYTE(reg)  ((reg)&0x01 \
	? (RIU_READ_BYTE((((reg)&0xFFFFFF00) + (((reg)&0x000000FE)<<1)+1))) \
	: (RIU_READ_BYTE((((reg)&0xFFFFFF00) + (((reg)&0x000000FE)<<1)))))
#define RIU_WRITEBYTE(reg, val)  ((reg)&0x01 \
	? (RIU_WRITE_BYTE((((reg)&0xFFFFFF00) + (((reg)&0x000000FE)<<1)+1), val)) \
	: (RIU_WRITE_BYTE((((reg)&0xFFFFFF00) + (((reg)&0x000000FE)<<1)), val)))


#define RIU_WRITE_BIT(reg, enable, mask) \
	(RIU_WRITE_BYTE((((reg) << 1) - ((reg) & 1)), (enable) \
		? (RIU_READ_BYTE((((reg) << 1) - ((reg) & 1))) | (mask)) \
		: (RIU_READ_BYTE((((reg) << 1) - ((reg) & 1))) & ~(mask))))

#define RIU_WRITE_BYTE_MASK(reg, u8Val, mask)  \
	(RIU_WRITE_BYTE((((reg) << 1) - ((reg) & 1)), \
		(RIU_READ_BYTE((((reg) << 1) - ((reg) & 1))) & ~(mask)) \
		| ((u8Val) & (mask))))


#define DMD_MBX_TIMEOUT      200
#define MAX_INT              0x7FFFFFFF
#define POW2_62              0x4000000000000000  /*  2^62  */

static u32 TS_CLK_TABLE[][32] = {
	/*  0x00      0x01     0x02      0x03      0x04      0x05      0x06      0x07  */
	{ 86400000, 43200000, 28800000, 21600000, 17280000, 14400000, 12342857, 10800000,
	/*  0x08      0x09     0x0a      0x0b      0x0c      0x0d      0x0e      0x0f  */
	9600000,  8640000,  7854545,  7200000,  6646154,  6171429,  5760000,  5400000,
	/*  0x10      0x11     0x12      0x13      0x14      0x15      0x16      0x17  */
	5082359,  4800000,  4547368,  4320000,  4114286,  3927273,  3756521,  3600000,
	/*  0x18      0x19     0x1a      0x1b      0x1c      0x1d      0x1e      0x1f  */
	3456000,  3323077,  3200000,  3085714,  2979310,  2880000,  2787097,  2700000 },
	/*  0x00      0x01     0x02      0x03      0x04      0x05      0x06      0x07  */
	{144000000, 72000000, 48000000, 36000000, 28800000, 24000000, 20571429, 18000000,
	/*  0x08      0x09     0x0a      0x0b      0x0c      0x0d      0x0e      0x0f  */
	16000000, 14400000, 13090909, 12000000, 11076923, 10285714,  9600000,  9000000,
	/*  0x10      0x11     0x12      0x13      0x14      0x15      0x16      0x17  */
	8470588,  8000000,  7578947,  7200000,  6857143,  6545455,  6260870,  6000000,
	/*  0x18      0x19     0x1a      0x1b      0x1c      0x1d      0x1e      0x1f  */
	5760000,  5538462,  5333333,  5142857,  4965517,  4800000,  4645161,  4500000 }
};

static u8 *virt_riu_addr;

static u8 *clk_reg_base_addr;
static bool bclk_init_base_addr;

void mtk_demod_init_riuaddr(struct dvb_frontend *fe)
{
	struct mtk_demod_dev *demod_prvi = fe->demodulator_priv;

	virt_riu_addr = demod_prvi->virt_riu_base_addr;
}

/*-----------------------------------------------------------------------------
 * Suspend the calling task for u32Ms milliseconds
 * @param  time_ms  \b IN: delay 1 ~ MSOS_WAIT_FOREVER ms
 * @return None
 * @note   Not allowed in interrupt context; otherwise, exception will occur.
 *----------------------------------------------------------------------------
 */
void mtk_demod_delay_ms(u32 time_ms)
{
	mtk_demod_delay_us(time_ms * 1000UL);
}

/*-----------------------------------------------------------------------------
 *  Delay for u32Us microseconds
 *  @param  time_us  \b IN: delay 0 ~ 999 us
 *  @return None
 *  @note   implemented by "busy waiting".
 *         Plz call MsOS_DelayTask directly for ms-order delay
 *----------------------------------------------------------------------------
 */
void mtk_demod_delay_us(u32 time_us)
{
	/*
	 * Refer to Documentation/timers/timers-howto.txt
	 * Non-atomic context
	 *    < 10us  : udelay()
	 * 10us ~ 20ms  : usleep_range()
	 *    > 10ms  : msleep() or msleep_interruptible()
	 */
	if (time_us < 10UL)
		udelay(time_us);
	else if (time_us < 20UL * 1000UL)
		usleep_range(time_us, time_us+1);
	else
		msleep_interruptible((unsigned int)(time_us / 1000UL));
}

/*----------------------------------------------------------------------------
 * Get current system time in ms
 * @return system time in ms
 *----------------------------------------------------------------------------
 */
unsigned int mtk_demod_get_time(void)
{
	struct timespec ts;

	getrawmonotonic(&ts);
	return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

/*-----------------------------------------------------------------------------
 *  Time difference between current time and task time
 *  @return system time diff in ms
 *----------------------------------------------------------------------------
 */
int mtk_demod_dvb_time_diff(u32 time)
{
	return (mtk_demod_get_time() - time);
}


static int mtk_demod_mbx_dvb_wait_ready(void)
{
	u32 start_time = mtk_demod_get_time();

	while (RIU_READBYTE(MB_REG_BASE + 0x00)) {
		/* wait VDMCU ready */
		if (mtk_demod_dvb_time_diff(start_time)
			> DMD_MBX_TIMEOUT) {
			pr_err("HAL_SYS_DMD_VD_MBX_DVB_WaitReady Timeout\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

int mtk_demod_mbx_dvb_wait_handshake(void)
{
	u32 start_time = mtk_demod_get_time();

	while (RIU_READBYTE(MB_REG_BASE + 0x00) != 0xFF) {
		/* wait MB_CNTL set done */
		if (mtk_demod_dvb_time_diff(start_time)
			> DMD_MBX_TIMEOUT) {
			if (RIU_READBYTE(MB_REG_BASE + 0x00) == 0xFF) {
				pr_err("HAL_SYS_DMD_VD_MBX_DVB_WaitHandShake Busy\n");
				return 0;
			}
			pr_err("HAL_SYS_DMD_VD_MBX_DVB_WaitHandShake Timeout\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

int mtk_demod_mbx_dvb_write(u16 addr, u8 value)
{
#if USE_RIU_RW_IN_MBX
	mtk_demod_write_byte(((addr&0xff00)*2 + (addr&0x00ff)), value);
	return 0;
#else
	if (mtk_demod_mbx_dvb_wait_ready()) {
		pr_err("mtk_demod_mbx_dvb_write_wait_ready_timeout\n");
		return -ETIMEDOUT;
	}

	/* ADDR_H */
	RIU_WRITEBYTE(MB_REG_BASE + 0x02, (u8)(addr >> 8));
	/* ADDR_L */
	RIU_WRITEBYTE(MB_REG_BASE + 0x01, (u8)addr);
	/* REG_DATA */
	RIU_WRITEBYTE(MB_REG_BASE + 0x03, value);
	/* MB_CNTL set write mode */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x02);

	/* assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x02);
	/* de-assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x00);

	if (mtk_demod_mbx_dvb_wait_handshake()) {
		pr_err("mtk_demod_mbx_dvb_write_wait_handshake_timeout\n");
		return -ETIMEDOUT;
	}

	/* MB_CNTL clear */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x00);
	return 0;
#endif
}

int mtk_demod_mbx_dvb_read(u16 addr, u8 *value)
{
#if USE_RIU_RW_IN_MBX
	*value = mtk_demod_read_byte((addr&0xff00)*2 + (addr&0x00ff));
	return 0;
#else
	if (mtk_demod_mbx_dvb_wait_ready()) {
		pr_err("mtk_demod_mbx_dvb_read_wait_ready_timeout\n");
		return -ETIMEDOUT;
	}

	/* ADDR_H */
	RIU_WRITEBYTE(MB_REG_BASE + 0x02, (u8)(addr >> 8));
	/* ADDR_L */
	RIU_WRITEBYTE(MB_REG_BASE + 0x01, (u8)addr);
	/* MB_CNTL set read mode */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x01);

	/* assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x02);
	/* de-assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x00);

	if (mtk_demod_mbx_dvb_wait_handshake()) {
		pr_err("mtk_demod_mbx_dvb_read_wait_handshake_timeout\n");
		return -ETIMEDOUT;
	}

	/* REG_DATA get */
	*value = RIU_READBYTE(MB_REG_BASE + 0x03);
	/* MB_CNTL clear */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x00);

	return 0;
#endif
}

int mtk_demod_mbx_dvb_write_dsp(u16 addr, u8 value)
{
	if (mtk_demod_mbx_dvb_wait_ready()) {
		pr_err("mtk_demod_mbx_dvb_write_dsp_wait_ready_timeout\n");
		return -ETIMEDOUT;
	}

	/* ADDR_H */
	RIU_WRITEBYTE(MB_REG_BASE + 0x02, (u8)(addr >> 8));
	/* ADDR_L */
	RIU_WRITEBYTE(MB_REG_BASE + 0x01, (u8)addr);
	/* REG_DATA */
	RIU_WRITEBYTE(MB_REG_BASE + 0x03, value);
	/* MB_CNTL set write mode */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x04);

	/* assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x02);
	/* de-assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x00);

	if (mtk_demod_mbx_dvb_wait_handshake()) {
		pr_err("mtk_demod_mbx_dvb_write_dsp_wait_handshake_timeout\n");
		return -ETIMEDOUT;
	}

	/* MB_CNTL clear */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x00);

	return 0;
}

int mtk_demod_mbx_dvb_read_dsp(u16 addr, u8 *value)
{
	if (mtk_demod_mbx_dvb_wait_ready()) {
		pr_err("mtk_demod_mbx_dvb_read_dsp_wait_ready_timeout\n");
		return -ETIMEDOUT;
	}

	/* ADDR_H */
	RIU_WRITEBYTE(MB_REG_BASE + 0x02, (u8)(addr >> 8));
	/* ADDR_L */
	RIU_WRITEBYTE(MB_REG_BASE + 0x01, (u8)addr);
	/* MB_CNTL set read mode */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x03);

	/* assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x02);
	/* de-assert interrupt to DMD51 */
	RIU_WRITEBYTE(DMD51_REG_BASE + 0x03, 0x00);

	if (mtk_demod_mbx_dvb_wait_handshake()) {
		pr_err("mtk_demod_mbx_dvb_read_dsp_wait_handshake_timeout\n");
		return -ETIMEDOUT;
	}

	/* REG_DATA get */
	*value = RIU_READBYTE(MB_REG_BASE + 0x03);
	/* MB_CNTL clear */
	RIU_WRITEBYTE(MB_REG_BASE + 0x00, 0x00);

	return 0;
}

unsigned int mtk_demod_read_byte(u32 addr)
{
	return RIU_READBYTE(addr);
}

void mtk_demod_write_byte(u32 addr, u8 value)
{
	RIU_WRITEBYTE(addr, value);
}

void mtk_demod_write_bit(u32 reg, bool enable_b, u8 mask)
{
	RIU_WRITE_BIT(reg, enable_b, mask);
}

void mtk_demod_write_byte_mask(u32 reg, u8 val, u8 msk)
{
	RIU_WRITE_BYTE_MASK(reg, val, msk);
}

static u32 abs_32(s32 input)
{
	u32 result = 0;

	if (input < 0)
		result = (-1)*input;
	else
		result = input;

	return result;
}

static u64 abs_64(s64 input)
{
	u64 result = 0;

	if (input < 0)
		result = (-1)*input;
	else
		result = input;

	return result;
}

static u8 find_msb(s64 input)
{
	s8 iter = -1;
	u64 data = abs_64(input);

	while (data != 0) {
		++iter;
		data >>= 1;
	}

	if (iter >= 0)
		return iter;


	return 0;
}

static void normalize(struct ms_float_st *input)
{
	u8 lsb = 0, sign_flag = 0;
	s8 exp = 0;
	u32 data = 0;

	if ((*input).data == 0) {
		(*input).exp = 0;
	} else {
		lsb = 0;

		if ((*input).data < 0) /* negative value */
			sign_flag = 1;
		else
			sign_flag = 0;

		data = abs_32((*input).data);
		exp = (*input).exp;

		if (exp != 0) {
			while ((data & 0x01) == 0x00) {
				++lsb;
				data >>= 1;
			}

			exp += lsb;

			(*input).data = data;
			(*input).exp = exp;

			if (sign_flag == 1)
				(*input).data *= (-1);
		}
	}
}

struct ms_float_st mtk_demod_float_op(struct ms_float_st st_rn,
			struct ms_float_st st_rd, enum op_type op_code)
{
	struct ms_float_st result = {.data = 0, .exp = 0};
	s32 data1 = 0, data2 = 0;
	u32 udata1 = 0, udata2 = 0;
	s8 exp_1 = 0, exp_2 = 0, exp_diff;
	s8 iter = 0, msb = 0, msb_tmp = 0;

	s64 temp = 0;
	u64 u64_temp = 0;

	normalize(&st_rn);
	normalize(&st_rd);

	data1 = st_rn.data;
	data2 = st_rd.data;

	udata1 = abs_32(data1);
	udata2 = abs_32(data2);

	exp_1 = st_rn.exp;
	exp_2 = st_rd.exp;

	switch (op_code) {
	case ADD:
	{
		if (exp_1 == exp_2) {
			temp = data1;
			temp += data2;

			/* DATA overflow (32 bits) */
			if ((temp > MAX_INT) || (temp < (-1)*MAX_INT)) {
				temp >>= 1;
				result.data = temp;
				result.exp = (exp_1+1);
			} else {
				result.data = (data1+data2);
				result.exp = exp_1;
			}
		} else {
			if (exp_1 > exp_2) {
				/* get the EXP difference of st_rn and st_rd */
				exp_diff = exp_1-exp_2;

				/* get the MSB of st_rn */
				temp = data1;
				msb = find_msb(temp);

				if ((msb + exp_diff) < 62) {
					temp = data1;

					for (iter = 0; iter < exp_diff; ++iter)
						temp = (temp<<1);

					temp += data2;
				} else {
					temp = data1;

					for (iter = 0; iter < (62-msb); ++iter)
						temp = (temp<<1);

					exp_1 = exp_1-(62-msb);

					for (iter = 0; iter < (exp_1-exp_2);
						++iter) {
						data2 = (data2>>1);
					}

					exp_2 = exp_1;

					temp += data2;
				}

				/* DATA overflow (32 bits) */
				if ((temp > MAX_INT) || (temp < (-1)*MAX_INT)) {
					msb = find_msb(temp);

					if (msb >= 30)
						temp >>= (msb-30);

					result.data = temp;
					result.exp = (exp_2+(msb-30));
				} else {
					result.data = temp;
					result.exp = exp_2;
				}
			} else {
				return mtk_demod_float_op(st_rd,
						st_rn, ADD);
			}
		}
	}
		break;

	case MINUS:
	{
		st_rd.data *= (-1);
		return mtk_demod_float_op(st_rn, st_rd, ADD);
	}
		break;

	case MULTIPLY:
	{
		if (data1 == 0 || data2 == 0) {
			result.data = 0;
			result.exp = 0;
		} else {
			temp = data1;
			temp *= data2;

			if ((temp <= MAX_INT) && (temp >= (-1*MAX_INT))) {
				result.data = data1*data2;
				result.exp = exp_1+exp_2;
			} else {
				msb = find_msb(temp);
				if (msb-30 >= 0) {
					temp = temp>>(msb-30);
					result.data = (s32)temp;
					result.exp = exp_1+exp_2+(msb-30);
				} else {
					result.data = (s32)0;
					result.exp = 0;
				}
			}
		}
	}
		break;

	case DIVIDE:
	{
		if (data1 != 0 && data2 != 0) {
			if (udata1 < udata2) {
				u64_temp = POW2_62;
				do_div(u64_temp, data2);
				temp = u64_temp*data1;

				msb = find_msb(temp);

				if (msb > 30) {
					temp >>= (msb-30);
					result.data = temp;
					result.exp = exp_1-exp_2+(msb-30)-62;
				} else {
					result.data = temp;
					result.exp = exp_1-exp_2-62;
				}
			} else if (udata1 == udata2) {
				result.data = data1/data2;
				result.exp = exp_1-exp_2;
			} else {
				msb = find_msb(data1);
				msb_tmp = find_msb(data2);

				exp_2 -= ((msb-msb_tmp) + 1);

				u64_temp = POW2_62;
				if ((msb-msb_tmp) >= 0) {
					do_div(u64_temp,
					(((s64)data2)<<((msb-msb_tmp+1))));
					temp = u64_temp*data1;
				}

				msb = find_msb(temp);

				if (msb > 30) {
					temp >>= (msb-30);
					result.data = temp;
					result.exp = exp_1-exp_2+(msb-30)-62;
				} else {
					result.data = temp;
					result.exp = exp_1-exp_2-62;
				}
			}
		} else {
			result.data = 0;
			result.exp = 0;
		}
	}
		break;

	default:
		break;
	}

	normalize(&result);

	return result;
}

s64 mtk_demod_float_to_s64(struct ms_float_st msf_input)
{
	s64 s64result;
	u32 u32result;
	u8  offset_num;

	u32result = abs_32(msf_input.data);
	s64result = u32result;

	if (msf_input.exp > 0) {
		offset_num = (u8)msf_input.exp;
		s64result = s64result<<offset_num;
	} else {
		offset_num = (u8)(-1*msf_input.exp);
		s64result = s64result>>offset_num;
	}

	if (msf_input.data < 0)
		s64result = s64result*(-1);

	return s64result;
}
void mtk_demod_clk_base_addr_init(struct mtk_demod_dev *demod_prvi)
{
	bclk_init_base_addr = true;
	clk_reg_base_addr = demod_prvi->virt_clk_base_addr;
}

u16 mtk_demod_clk_reg_base_addr_write(u16 u16regoffset, u16 value)
{
	u16 bRet = 1;

	if (bclk_init_base_addr)
		writew(value, clk_reg_base_addr + u16regoffset);
	else
		bRet = 0;


	return bRet;
}

u16 mtk_demod_clk_reg_base_addr_read(u16 u16regoffset, u16 *value)
{
	u16 bRet = 1;
	u16 u16out = 0;

	if (bclk_init_base_addr) {
		u16out = readw(clk_reg_base_addr + u16regoffset);
		*value = u16out;
	} else {
		bRet = 0;
	}
	return bRet;
}

void mtk_demod_diseqc_clk_enable(void)
{
	u8   u8Data = 0;
	u32  address = 0;

	pr_info("[%s][%d]\n", __func__, __LINE__);
	address = REG_CLK_DVBS2_DISEQC;
	u8Data = mtk_demod_read_byte(address);
	u8Data &= (BIT7|BIT6|BIT5|BIT4);
	mtk_demod_write_byte(address, u8Data);

	address = REG_CLK_SEL_MANUAL;
	u8Data = mtk_demod_read_byte(address);
	u8Data |= BIT4;
	mtk_demod_write_byte(address, u8Data);
	pr_info("[%s][%d] Done\n", __func__, __LINE__);

}

bool mtk_demod_ts_get_clk_rate(struct dvb_frontend *fe, u32 *ts_clk)
{
	struct mtk_demod_dev *demod_prvi = fe->demodulator_priv;
	u8 u8_tmp = 0;
	u8 clk_source = 0;

	clk_source = readb(demod_prvi->virt_ts_base_addr + 0x01);
	clk_source = clk_source & 0x01;
	u8_tmp = readb(demod_prvi->virt_ts_base_addr)&0x1f;

	*ts_clk = TS_CLK_TABLE[clk_source][u8_tmp];
	return TRUE;
}

u16 mtk_demod_read_demod_clk_sel(void)
{
	u8   u8Data = 0;
	u16  u16outdata = 0;

	pr_err("[mdbgin_merak_demod_dd][%s][%d]\n", __func__, __LINE__);

	// ==============================================================
	// Start demod_0 CLKGEN setting ......
	// ==============================================================
	// h0000 h0000 7 0 reg_clock_default_spec_sel 7 0 8 h0
	//  1: SRD (1)
	//  2: ATSC_SRD (2)
	//  3: ATSC_J83B (3)
	//  4: DTMB_PN (4)
	//  5: DTMB_SC (5)
	//  6: DTMB_SCLS (6)
	//  7: DTMB_MC (7)
	//  8: DVBS_S2_JOIN (8)
	//  9: DVBS (9)
	// 10: DVBT_T2_JOIN (10)
	// 11: DVBT2 (11)
	// 12: DVBT (12)
	// 13: DVBC (13)
	// 14: ISDBT (14)
	// 15: VIF (15)
	// 16: ZNR(16)
	u8Data = mtk_demod_read_byte(DEMOD_CLKGEN_REG_BASE+1);
	u16outdata = u8Data;
	u16outdata = (u16outdata << BIT3);
	u8Data = mtk_demod_read_byte(DEMOD_CLKGEN_REG_BASE);
	u16outdata |= u8Data;
	pr_info("[%s][%d] standard type 0x%x\n", __func__, __LINE__, u16outdata);

	return u16outdata;

}

static u16 dmd_top[0x80] = { // 0x20, for check patch column100
	0x3010, 0x0070, 0x0022, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF,
	0xFFFF, 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 0x01[13] keep 0
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0432, 0x0119, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x01FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x03FF, 0xFFFF,
	0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x00FF, 0x0000, 0x0000, 0x0000, 0x030F, 0x0000, 0x00FF,
	0x0400, 0x0000, 0x0000, 0x1FC8, 0x060B, 0x3727, 0xD86F, 0x93A7,
	0x0000, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0001, 0x0000, 0x0000,
	0x2032, 0x3924, 0x2C09, 0x4203, 0x9392, 0x32C0, 0x2420, 0x0939,
	0x6637, 0x006E, 0x5526, 0xCD03, 0x37AB, 0x6E66, 0x2600, 0x0355,
	0x6647, 0x8033, 0x6970, 0x3552, 0x6647, 0x8033, 0x6970, 0x3552,
	0x6199, 0x2388, 0x9E02, 0x8619, 0x2238, 0x99E0, 0x8861, 0x0223,
};

static u16 dmd_top_ext[0x80] = { // 0x121
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x03FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0008, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3727, 0x3727,
	0xD86F, 0x93A7, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0013, 0x0011,
	0x1000, 0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static u16 dmd_clkgen[0x80] = { // 0x35
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x1010, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111,
	0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111,
	0x1011, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x1111, 0x0011,
	0x0000, 0x0001, 0x0500, 0x1101, 0x1111, 0x0011, 0x0101, 0x1101,
	0x0111, 0x1101, 0x0101, 0x1111, 0x0011, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3333, 0xCCCC,
};

static u16 dmd_clkgen_ext[0x80] = { // 0x122
	0x0101, 0x0001, 0x0001, 0x0001, 0x0001, 0x0101, 0x0000, 0x0101,
	0x1111, 0x0000, 0x1011, 0x0101, 0x0001, 0x0100, 0x0101, 0x0001,
	0x0000, 0x0000, 0x0001, 0x0001, 0x0000, 0x0000, 0x0001, 0x0001,
	0x0001, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0101, 0x0100,
	0x0101, 0x0001, 0x0001, 0x0000, 0x0010, 0x0101, 0x0001, 0x0101,
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0101,
	0x0000, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101,
	0x0000, 0x0101, 0x1001, 0x1001, 0x1111, 0x0111, 0x1101, 0x1111,
	0x1111, 0x1111, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static u16 dmd_sram_share[0x80] = { // 0x126
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static u16 dmd_ana[0x80] = { // 0x2e
	0xFF00, 0x000F, 0x0000, 0x0000, 0x0033, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0007, 0x0000, 0x0000, 0x0080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x000C, 0x0000,
	0x0000, 0x2101, 0x0000, 0x0000, 0x0081, 0x1CC3, 0x0000, 0x0000,
	0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0xFF00, 0xFF00, 0x0000, 0x0000,
	0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x2400, 0x0000, 0x00FF,
	0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x0001, 0x0000, 0x0000,// 0x3d[0] keep 1
	0x0000, 0xFFFF, 0x0000, 0x0000, 0x0780, 0x0000, 0x0000, 0x0000,
	0x8CFC, 0x0018, 0x0000, 0x0B9D, 0x2CDC, 0x330C, 0xB066, 0x0008,
	0x0000, 0x0000, 0x1FF0, 0x0000, 0x0003, 0x0000, 0x0110, 0x2098,
	0x0300, 0x0000, 0x0380, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x084B, 0x009A, 0x0000,
	0x0000, 0x0000, 0x0001, 0x0700, 0x0000, 0x0300, 0x0000, 0x01FB,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0100, 0x0020, 0x01FF,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

/*
 * static u16 dmd_remap[0x80] = { // 0x02
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
 *};
 */

static u16 dmd_dvbs2[0x80] = { // 0x3a
	0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0010, 0x0000, 0x0006, 0x0028, 0xFFFF, 0x0FFF, 0x0000, 0xFFFF,
	0xFFFF, 0x0FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0006, 0x0100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0088, 0x0000,
	0x001F, 0x1283, 0x0DAC, 0x001B, 0x000B, 0x0A20, 0x1C15, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

void mtk_demod_reset_to_default(struct dvb_frontend *fe)
{
	u8 i;
	struct mtk_demod_dev *demod_prvi = fe->demodulator_priv;
	/* [0]: reset demod 0 */
	/* [4]: reset demod 1 */
	writeb(0x01, demod_prvi->virt_reset_base_addr);// only reset demod 1
	mtk_demod_delay_ms(1);
	writeb(0x11, demod_prvi->virt_reset_base_addr);
	mtk_demod_delay_ms(1);

	for (i = 0; i < 0x80; i++) {
		writew(dmd_top[i], demod_prvi->virt_riu_base_addr + DEMOD_TOP_REG_BASE + i*4);
		writew(dmd_top_ext[i], demod_prvi->virt_riu_base_addr +
			DEMOD_TOP_EXT_REG_BASE + i*4);
		writew(dmd_clkgen[i], demod_prvi->virt_riu_base_addr + DEMOD_CLKGEN_REG_BASE + i*4);
		writew(dmd_clkgen_ext[i], demod_prvi->virt_riu_base_addr +
			DEMOD_CLKGEN_EXT_REG_BASE + i*4);
		writew(dmd_sram_share[i], demod_prvi->virt_riu_base_addr +
			DEMOD_SRAM_SHARE_REG_BASE + i*4);
		writew(dmd_ana[i], demod_prvi->virt_riu_base_addr + ANA_REG_BASE + i*4);
		//writew(dmd_remap[i], demod_prvi->virt_riu_base_addr + DEMOD_REMAP_REG_BASE + i*4);
		writew(dmd_dvbs2[i], demod_prvi->virt_riu_base_addr + DEMOD_DVBS2_REG_BASE + i*4);
	}
	mtk_demod_write_byte(0x0200 + 0x57*2, 0x02);// remapping reset
	mtk_demod_write_byte(0x0200 + 0x57*2, 0x00);
}

void mtk_demod_ana_trim(struct dvb_frontend *fe)
{
	u8 u8tmp = 0;
	u16 u16tmp = 0;
	u32 u32_efuse_value = 0, check_val = 0;
	struct mtk_demod_dev *demod_prvi = fe->demodulator_priv;

	// for hang issue
	/* [4][8]: reset demod 0/1 */
	u16tmp = readw(demod_prvi->virt_ts_base_addr + 0x04);
	u16tmp &= 0xfeef;
	writew(u16tmp, demod_prvi->virt_ts_base_addr + 0x04);
	mtk_demod_delay_ms(1);
	u16tmp = readw(demod_prvi->virt_ts_base_addr + 0x04);
	u16tmp |= 0x0110;
	writew(u16tmp, demod_prvi->virt_ts_base_addr + 0x04);
	mtk_demod_delay_ms(1);

	// ana trim
	u16tmp = (0x1a<<2) | 0x2000;// [13] read trigger, [9:2] addr
	writew(u16tmp, demod_prvi->virt_efuse_base_addr + (0x28<<2));

	u16tmp = readw(demod_prvi->virt_efuse_base_addr + (0x2d<<2));
	u32_efuse_value = (u16tmp<<16);
	u16tmp = readw(demod_prvi->virt_efuse_base_addr + (0x2c<<2));
	u32_efuse_value |= u16tmp;

	check_val = (u32_efuse_value>>15) & 0x01;
	if (check_val != 0) {
		u8tmp = mtk_demod_read_byte(DEMOD_TOP_REG_BASE + 0x01*2 + 1);// release bit13
		u8tmp &= (~0x20);
		mtk_demod_write_byte(DEMOD_TOP_REG_BASE + 0x01*2 + 1, u8tmp);

		u8tmp = mtk_demod_read_byte(ANA_REG_BASE + 0x47*2);
		u8tmp &= ~(0x1f);
		u8tmp |= (u8)((u32_efuse_value>>8)&0x1f);
		mtk_demod_write_byte(ANA_REG_BASE + 0x47*2, u8tmp);
	}
}
