/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_CLK_MT5896_MIXED_H
#define _DT_BINDINGS_CLK_MT5896_MIXED_H

/* TOPCKGEN fix */
#define CLK_TOPGEN_APUBUSPLL_CK	0
#define CLK_TOPGEN_CLK_SRC_RC100_SRC_CK	1
#define CLK_TOPGEN_CLK_DEMOD_864M_P	2
#define CLK_TOPGEN_DMPLL	3
#define CLK_TOPGEN_RV55FRCPLL	4
#define CLK_TOPGEN_SYS0PLL	5
#define CLK_TOPGEN_SYS1PLL	6
#define CLK_TOPGEN_SYS2PLL	7
#define CLK_TOPGEN_SYS3PLL	8
#define CLK_TOPGEN_VDECPLL	9
#define CLK_TOPGEN_XCPLL	10
#define CLK_TOPGEN_XCSRPLL	11
#define CLK_TOPGEN_XTAL	12
#define CLK_TOPGEN_FIX_N	13

/* TOPCKGEN fix div */
#define CLK_TOPGEN_SYS3PLL_VCOD2_456M_CK	0
#define CLK_TOPGEN_MPLL_VCOD4_216M_CK	1
#define CLK_TOPGEN_MPLL_VCOD6_144M_CK	2
#define CLK_TOPGEN_MPLL_VCOD7_123M_CK	3
#define CLK_TOPGEN_MPLL_VCOD8_108M_CK	4
#define CLK_TOPGEN_MPLL_VCOD10_86M_CK	5
#define CLK_TOPGEN_MPLL_VCOD12_72M_CK	6
#define CLK_TOPGEN_MPLL_VCOD14_62M_CK	7
#define CLK_TOPGEN_MPLL_VCOD16_54M_CK	8
#define CLK_TOPGEN_MPLL_VCOD20_43P2M_CK	9
#define CLK_TOPGEN_MPLL_VCOD24_36M_CK	10
#define CLK_TOPGEN_MPLL_VCOD32_27M_CK	11
#define CLK_TOPGEN_DMPLLDIV2_IN_CK	12
#define CLK_TOPGEN_DMPLLDIV3_IN_CK	13
#define CLK_TOPGEN_DMPLLDIV4_IN_CK	14
#define CLK_TOPGEN_DMPLLDIV5_IN_CK	15
#define CLK_TOPGEN_DMPLLDIV10_IN_CK	16
#define CLK_TOPGEN_DMPLLDIV17_IN_CK	17
#define CLK_TOPGEN_RV55FRCPLL_VCOD1_1000M_CK	18
#define CLK_TOPGEN_SYS0PLL_VCOD4_528M_CK	19
#define CLK_TOPGEN_SYS0PLL_VCOD6_352M_CK	20
#define CLK_TOPGEN_SYS0PLL_VCOD8_264M_CK	21
#define CLK_TOPGEN_SYS1PLL_VCOD2_624M_CK	22
#define CLK_TOPGEN_SYS1PLL_VCOD3_416M_CK	23
#define CLK_TOPGEN_SYS1PLL_VCOD4_312M_CK	24
#define CLK_TOPGEN_SYS1PLL_VCOD6_208M_CK	25
#define CLK_TOPGEN_SYS1PLL_VCOD8_156M_CK	26
#define CLK_TOPGEN_SYS1PLL_VCOD10_124M_CK	27
#define CLK_TOPGEN_SYS2PLL_VCOD2_720M_CK	28
#define CLK_TOPGEN_SYS2PLL_VCOD2P5_576M_CK	29
#define CLK_TOPGEN_SYS2PLL_VCOD3_480M_CK	30
#define CLK_TOPGEN_SYS2PLL_VCOD4_360M_CK	31
#define CLK_TOPGEN_SYS2PLL_VCOD6_240M_CK	32
#define CLK_TOPGEN_SYS2PLL_VCOD8_180M_CK	33
#define CLK_TOPGEN_SYS2PLL_VCOD10_144M_CK	34
#define CLK_TOPGEN_SYS2PLL_VCOD15_96M_CK	35
#define CLK_TOPGEN_SYS2PLL_VCOD30_48M_CK	36
#define CLK_TOPGEN_SYS2PLL_VCOD48_30M_CK	37
#define CLK_TOPGEN_MPLL_VCOD2_432M_CK	38
#define CLK_TOPGEN_MPLL_VCOD3_288M_CK	39
#define CLK_TOPGEN_MPLL_VCOD5_172M_CK	40
#define CLK_TOPGEN_VDECPLL_VCOD1_624M_CK	41
#define CLK_TOPGEN_XCPLL_VCOD1_720M_CK	42
#define CLK_TOPGEN_XCPLL_VCOD1_720M_XC_ED_BUF_CK	43
#define CLK_TOPGEN_XCPLL_VCOD1_720M_XC_FN_BUF_CK	44
#define CLK_TOPGEN_XCPLL_VCOD1_720M_XC_FS_BUF_CK	45
#define CLK_TOPGEN_XCPLL_VCOD1_720M_XC_FD_BUF_CK	46
#define CLK_TOPGEN_XCSRPLL_VCOD1_630M_CK	47
#define CLK_TOPGEN_XTAL_VCOD1_24M_CK	48
#define CLK_TOPGEN_XTAL_VCOD2_12M_CK	49
#define CLK_TOPGEN_XTAL_VCOD4_6M_CK	50
#define CLK_TOPGEN_XTAL_VCOD8_3M_CK	51
#define CLK_TOPGEN_XTAL_VCOD16_1P5M_CK	52
#define CLK_TOPGEN_XTAL_VCOD80_300K_CK	53
#define CLK_TOPGEN_FIXDIV_N	54

#endif