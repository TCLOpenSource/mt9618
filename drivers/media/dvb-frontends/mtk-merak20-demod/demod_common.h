/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

#ifndef _MERAK_DEMOD_COMMON_H_
#define _MERAK_DEMOD_COMMON_H_

#include <linux/math64.h>

#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
#include "../cfm/cfm.h"

#if !defined(TRUE) && !defined(FALSE)
/* definition for TRUE */
#define TRUE                        true
/* definition for FALSE */
#define FALSE                       false
#endif

/* Demod Top */
#define DEMOD_TOP_REG_BASE  0x4000
/* Demod MailBox */
#define MB_REG_BASE   0x5E00
/* Dmd51 */
#define DMD51_REG_BASE  0x62300
/* MCU2 */
#define MCU2_REG_BASE  0x62400
/* DMD51_RST */
#define DMD51_RST_REG_BASE  0x62800

/* ANA */
#define ANA_REG_BASE  0x5C00
#define DEMOD_TOP_EXT_REG_BASE  0x24200
#define DEMOD_CLKGEN_REG_BASE  0x6A00
#define DEMOD_CLKGEN_EXT_REG_BASE  0x24400
#define DEMOD_SRAM_SHARE_REG_BASE  0x24C00
//#define DEMOD_REMAP_REG_BASE  0x0400
#define DEMOD_REMAP_REG_BASE  0x0200
#define DEMOD_DVBS2_REG_BASE  0x7400

#define	ISDBT_TDP_REG_BASE  0x1400
#define	ISDBT_FDP_REG_BASE  0x1500
#define	ISDBT_FDPE_REG_BASE 0x1600
#define	ISDBT_OUTER_REG_BASE 0x1700
#define BACKEND_REG_BASE  0x1f00
#define TOP_REG_BASE  0x2000
#define TDP_REG_BASE  0x2100
#define FDP_REG_BASE  0x2200
#define FEC_REG_BASE  0x2300
#define TDF_REG_BASE  0x2800
#define T2L1_REG_BASE  0x2b00
#define DMDANA_REG_BASE  0x2E00
#define T2TDP_REG_BASE  0x3000
#define T2FDP_REG_BASE  0x3100
#define T2FEC_REG_BASE  0x3300
#define DVBTM_REG_BASE  0x3400
#define REG_CLK_DVBS2_DISEQC       (DEMOD_CLKGEN_REG_BASE+0x2E*2)
#define REG_CLK_SEL_MANUAL       (DEMOD_CLKGEN_REG_BASE+0x03*2+1)

#define BIT0 1
#define BIT1 (1 << 1)
#define BIT2 (1 << 2)
#define BIT3 (1 << 3)
#define BIT4 (1 << 4)
#define BIT5 (1 << 5)
#define BIT6 (1 << 6)
#define BIT7 (1 << 7)
#define BIT8 (1 << 8)
#define BIT9 (1 << 9)

struct mtk_demod_dev {
	struct platform_device *pdev;
	struct dvb_adapter *adapter;
	struct dvb_frontend fe;
	u8     *virt_riu_base_addr;
	u8     *virt_reset_base_addr;
	u8     *virt_ts_base_addr;
	u8     *virt_clk_base_addr;
	u8     *virt_sram_base_addr;
	u8     *virt_pmux_base_addr;
	u8     *virt_vagc_base_addr;
	u8     *virt_mpll_base_addr;
	unsigned long     dram_base_addr;
	u8     *virt_dram_base_addr;
	u8     *virt_bw_ultra_base_addr;
	u8     *virt_efuse_base_addr;
	u8     *virt_demod_base_addr;
	u8     spectrum_inversion;
	u8     agc_polarity;
	u8     rf_strength;
	u16    ip_version;
	u8     if_gain;
	u8     model_info;
	int previous_system;
	bool fw_downloaded;
	unsigned int start_time;
	struct cfm_demod_ops dmd_ops;
};

/*
 *  E_POWER_SUSPEND:    State is backup in DRAM, components power off.
 *  E_POWER_RESUME:     Resume from Suspend or Hibernate mode
 *  E_POWER_MECHANICAL: Full power off mode. System return to working state
 *      only after full reboot
 *  E_POWER_SOFT_OFF:   The system appears to be off, but some components
 *      remain powered for trigging boot-up.
 */
enum EN_POWER_MODE {
	E_POWER_SUSPEND    = 1,
	E_POWER_RESUME     = 2,
	E_POWER_MECHANICAL = 3,
	E_POWER_SOFT_OFF   = 4,
};

enum op_type {
	ADD = 0,
	MINUS,
	MULTIPLY,
	DIVIDE
};

struct ms_float_st {
	s32 data;
	s8 exp;
};

#define SM_COMMA  ","
#define SM_END    ";"
#define SM_CHIP_REV "Chip Rev.(Demod)"
#define SM_PILOT_OFFSET "Pilot Offset"
#define SM_CNR  "CNR"
#define SM_C_N  "C/N" // SD_SM
#define SD_SIGNAL_QAULITY   "SQI"
#define SD_SIGNAL_STRENGTH   "SSI"
#define SD_STATUS   "Status"
#define SM_UEC  "UEC (Instant)" // SD_SM
#define SM_UEC_ACCU  "UEC (Accumulate)" // SD_SM
#define SM_AGC  "AGC" // SD_SM
#define SM_INTERLEAVING "Interleaving"
#define SM_MODULATION "Modulation" // SD_SM
#define SM_MODULATION_TYPE "Modulation Type"
#define SM_DEMOD_LOCK "Demod Lock"
#define SM_TS_LOCK  "TS Lock"
#define SD_PRE_RS "Pre RS"
#define SM_BER_BEFORE_RS "BER before RS"
#define SM_BER_BEFORE_BCH "BER before BCH"
#define SM_BER_BEFORE_RS_BCH  "BER before RS/BCH"
#define SM_BER_BEFORE_RS_BCH_5S "BER before RS/BCH (5s)"
#define SD_SIGNAL_LEVEL "Signal level"
#define SM_TS_RATE "TS Rate"
#define SM_PN_PHASE  "PN Phase"
#define SM_FREQ_OFFSET  "Freq. Offset"
#define SM_GUARD_INTERVAL "Guard Interval" // SD_SM
#define SM_GUARD  "Guard"
#define SM_TIME_INTERLEAVE  "Time Interleave"
#define SM_CODE_RATE "Code Rate" // SD_SM
#define SM_TRANSMISSION_MODE "Transmission Mode"
#define SD_FFT_MODE "FFT Size"
#define SD_POST_LDPC "Post LDPC"
#define SD_POST_VITERBI "Post Viterbi"
#define SM_SYSTEM "System"
#define SM_TMCC_DET  "TMCC Det"
#define SM_MODE  "Mode"
#define SM_LAYER  "Layer"
#define SM_LAYER_A_UEC_ACCU  "A_UEC (Accumulate)"
#define SM_LAYER_A_UEC  "A_UEC (Instant)"
#define SM_LAYER_A_MODULATION  "A_Modulation"
#define SM_LAYER_A_CODERATE "A_CodeRate"
#define SM_LAYER_A_INTERLEAVE  "A_Interleave"
#define SM_LAYER_A_SEGMENT  "A_Segment"
#define SM_LAYER_A_BER_BEFORE_RS  "A_BER before RS"
#define SM_LAYER_B_UEC_ACCU  "B_UEC (Accumulate)"
#define SM_LAYER_B_UEC  "B_UEC (Instant)"
#define SM_LAYER_B_MODULATION  "B_Modulation"
#define SM_LAYER_B_CODERATE "B_CodeRate"
#define SM_LAYER_B_INTERLEAVE  "B_Interleave"
#define SM_LAYER_B_SEGMENT  "B_Segment"
#define SM_LAYER_B_BER_BEFORE_RS  "B_BER before RS"
#define SM_LAYER_C_UEC_ACCU  "C_UEC (Accumulate)"
#define SM_LAYER_C_UEC  "C_UEC (Instant)"
#define SM_LAYER_C_MODULATION  "C_Modulation"
#define SM_LAYER_C_CODERATE "C_CodeRate"
#define SM_LAYER_C_INTERLEAVE  "C_Interleave"
#define SM_LAYER_C_SEGMENT  "C_Segment"
#define SM_LAYER_C_BER_BEFORE_RS  "C_BER before RS"
#define SM_EMERGENCY_FLAG "Emergency Flag"
#define SM_LOCK_STATUS  "Lock Status"
#define SM_VISION_CARRIER_OFFSET  "Vision Carrier Offset"
#define SM_PLL_LOCK  "PLL Lock"
#define SM_SYMBOL_RATE  "Symbol rate" // SD_SM
#define SM_PILOT  "Pilot"
#define SM_22K  "22k"
#define SM_SELECTED_DISEQC  "Selected DiSEqC"
#define SM_RSSI "RSSI"
#define SM_SPECTRUM_INVERT_FLAG  "Spectrum invert flag"
#define SM_BANDWIDTH  "Bandwidth" // SD_SM
#define SM_XIXO  "XIXO"
#define SM_NUMBER_OF_T2_FRAME  "Number of T2 frame"
#define SM_PAPR  "PAPR"
#define SM_FEF  "FEF"
#define SM_NUMBER_OF_PLPS "Number of PLPs"
#define SM_L1_MODULATION  "L1 modulation"
#define SM_PILOT_PATTERN  "Pilot Pattern"
#define SM_DATA_SYMBOLS  "Data Symbols"
#define SD_SM_PLP_ID  "PLP ID" // SD_SM
#define SM_FEC_TYPE "FEC Type"
#define SM_CELL_ID "CELL ID"
#define SM_DOT "."
#define SM_NONE "---"
#define SM_CARRIER_MODE "Carrier Mode"
#define SM_ROLLOFF "Roll off"
#define SM_FREQUENCY "Request Freq."
#define SM_kHz "[kHz]"
#define SM_MHzBW "[MHzBW]"
#define SM_dB "[dB]"
#define SM_sps "[sps]"
#define SM_ksps "[ksps]"
/*---------------------------------------------------------------------------*/
void mtk_demod_init_riuaddr(struct dvb_frontend *fe);

void mtk_demod_delay_ms(u32 time_ms);
void mtk_demod_delay_us(u32 time_us);
unsigned int mtk_demod_get_time(void);
int mtk_demod_dvb_time_diff(u32 time);

int mtk_demod_mbx_dvb_wait_handshake(void);
int mtk_demod_mbx_dvb_write(u16 addr, u8 value);
int mtk_demod_mbx_dvb_read(u16 addr, u8 *value);
int mtk_demod_mbx_dvb_write_dsp(u16 addr, u8 value);
int mtk_demod_mbx_dvb_read_dsp(u16 addr, u8 *value);

unsigned int mtk_demod_read_byte(u32 addr);
void mtk_demod_write_byte(u32 addr, u8 value);
void mtk_demod_write_byte_mask(u32 reg, u8 val, u8 msk);
void mtk_demod_write_bit(u32 reg, bool enable_b, u8 mask);

struct ms_float_st mtk_demod_float_op(struct ms_float_st st_rn,
			struct ms_float_st st_rd, enum op_type op_code);
s64 mtk_demod_float_to_s64(struct ms_float_st msf_input);

void mtk_demod_clk_base_addr_init(struct mtk_demod_dev *demod_prvi);
u16 mtk_demod_clk_reg_base_addr_write(u16 u16regoffset, u16 value);
u16 mtk_demod_clk_reg_base_addr_read(u16 u16regoffset, u16 *value);
void mtk_demod_diseqc_clk_enable(void);
u16 mtk_demod_read_demod_clk_sel(void);
bool mtk_demod_ts_get_clk_rate(struct dvb_frontend *fe, u32 *ts_clk);
void mtk_demod_reset_to_default(struct dvb_frontend *fe);
void mtk_demod_ana_trim(struct dvb_frontend *fe);
/*---------------------------------------------------------------------------*/
#endif

