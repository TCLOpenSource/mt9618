/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_CLK_MT5896_H
#define _DT_BINDINGS_CLK_MT5896_H

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

/* TOPCKGEN Mux Gate */
#define CLK_TOPGEN_FCIE_CK	0
#define CLK_TOPGEN_FCIE_SYN_CK	1
#define CLK_TOPGEN_IMI_CK	2
#define CLK_TOPGEN_NJPD_CK	3
#define CLK_TOPGEN_LZMA_CK	4
#define CLK_TOPGEN_XTAL_12M_CK	5
#define CLK_TOPGEN_XTAL_24M_CK	6
#define CLK_TOPGEN_SMI_CK	7
#define CLK_TOPGEN_PARSER_CK	8
#define CLK_TOPGEN_TSP_CK	9
#define CLK_TOPGEN_XC_ED_CK	10
#define CLK_TOPGEN_XC_FN_CK	11
#define CLK_TOPGEN_XC_FS_CK	12
#define CLK_TOPGEN_XC_SRS_CK	13
#define CLK_TOPGEN_XC_SLOW_CK	14
#define CLK_TOPGEN_XC_DC0_CK	15
#define CLK_TOPGEN_XC_SUB1_DC0_CK	16
#define CLK_TOPGEN_AIA_MDLA_CK	17
#define CLK_TOPGEN_UFS_CK	18
#define CLK_TOPGEN_UFS_AES_CK	19
#define CLK_TOPGEN_UFS_UNIPRO_CG_SYN_CK	20
#define CLK_TOPGEN_SMART_CK	21
#define CLK_TOPGEN_SMART_SYNTH_432_CK	22
#define CLK_TOPGEN_SMART_SYNTH_27_CK	23
#define CLK_TOPGEN_PEXTP_TL_CLK_CK	24
#define CLK_TOPGEN_VENC_CK	25
#define CLK_TOPGEN_GE_CK	26
#define CLK_TOPGEN_EXTRA_SCAN_432M_CK	27
#define CLK_TOPGEN_DRAMC_CH0_CK	28
#define CLK_TOPGEN_DRAMC_CH2_CK	29
#define CLK_TOPGEN_DRAMC_CH4_CK	30
#define CLK_TOPGEN_RC_CK	31
#define CLK_TOPGEN_MIIC0_CK	32
#define CLK_TOPGEN_MIIC1_CK	33
#define CLK_TOPGEN_MIIC2_CK	34
#define CLK_TOPGEN_MIIC3_CK	35
#define CLK_TOPGEN_FUART0_SYNTH_CK	36
#define CLK_TOPGEN_FUART0_CK	37
#define CLK_TOPGEN_UART0_SYNTH_CK	38
#define CLK_TOPGEN_UART0_CK	39
#define CLK_TOPGEN_UART1_SYNTH_CK	40
#define CLK_TOPGEN_UART1_CK	41
#define CLK_TOPGEN_UART2_SYNTH_CK	42
#define CLK_TOPGEN_UART2_CK	43
#define CLK_TOPGEN_FRC_UART0_SYNTH_CK	44
#define CLK_TOPGEN_FRC_UART0_CK	45
#define CLK_TOPGEN_FRC_UART1_SYNTH_CK	46
#define CLK_TOPGEN_FRC_UART1_CK	47
#define CLK_TOPGEN_AU_UART0_SYNTH_CK	48
#define CLK_TOPGEN_AU_UART0_CK	49
#define CLK_TOPGEN_FRC_FUART0_SYNTH_CK	50
#define CLK_TOPGEN_FRC_FUART0_CK	51
#define CLK_TOPGEN_RIU_NONPM_CK	52
#define CLK_TOPGEN_MCU_BUS_CK	53
#define CLK_TOPGEN_MCU_NONPM_CK	54
#define CLK_TOPGEN_MCU_CODEC_CK	55
#define CLK_TOPGEN_MCU_VD_CK	56
#define CLK_TOPGEN_MCU_DMD_CK	57
#define CLK_TOPGEN_MCU_FRC_CK	58
#define CLK_TOPGEN_MCU_SC_CK	59
#define CLK_TOPGEN_MCU_AU_CK	60
#define CLK_TOPGEN_MCU_HDMI_CK	61
#define CLK_TOPGEN_SPI_CK	62
#define CLK_TOPGEN_MSPI0_DIV_CK	63
#define CLK_TOPGEN_MSPI0_CK	64
#define CLK_TOPGEN_MSPI1_DIV_CK	65
#define CLK_TOPGEN_MSPI1_CK	66
#define CLK_TOPGEN_MSPI2_DIV_CK	67
#define CLK_TOPGEN_MSPI2_CK	68
#define CLK_TOPGEN_MSPI3_DIV_CK	69
#define CLK_TOPGEN_MSPI3_CK	70
#define CLK_TOPGEN_MSPI4_DIV_CK	71
#define CLK_TOPGEN_MSPI4_CK	72
#define CLK_TOPGEN_MSPI5_DIV_CK	73
#define CLK_TOPGEN_MSPI5_CK	74
#define CLK_TOPGEN_MSPI6_DIV_CK	75
#define CLK_TOPGEN_MSPI6_CK	76
#define CLK_TOPGEN_MSPI7_DIV_CK	77
#define CLK_TOPGEN_MSPI7_CK	78
#define CLK_TOPGEN_MSPI_CILINK_DIV_CK	79
#define CLK_TOPGEN_MSPI_CILINK_CK	80
#define CLK_TOPGEN_FRC_MSPI0_DIV_CK	81
#define CLK_TOPGEN_FRC_MSPI0_CK	82
#define CLK_TOPGEN_HDMI_R2_CK	83
#define CLK_TOPGEN_ADCPLL_CK	84
#define CLK_TOPGEN_VD_ADC_CK	85
#define CLK_TOPGEN_CLKD_ADC_CK	86
#define CLK_TOPGEN_CLKD_ADC2SC_CK	87
#define CLK_TOPGEN_CLKD_ADC2GEMIU_CK	88
#define CLK_TOPGEN_CLKD_VD_CK	89
#define CLK_TOPGEN_CLKFSCX2_VD_CK	90
#define CLK_TOPGEN_VD_CK	91
#define CLK_TOPGEN_VD2X_CK	92
#define CLK_TOPGEN_VD_32FSC_CK	93
#define CLK_TOPGEN_VE_CK	94
#define CLK_TOPGEN_VEDAC_DIGITAL_CK	95
#define CLK_TOPGEN_GPD_CK	96
#define CLK_TOPGEN_ECC_CK	97
#define CLK_TOPGEN_GMAC_AHB_CK	98
#define CLK_TOPGEN_AESDMA_CK	99
#define CLK_TOPGEN_TSO_OUT_DIV_MN_SRC_CK	100
#define CLK_TOPGEN_TSO2_OUT_DIV_MN_SRC_CK	101
#define CLK_TOPGEN_DEMOD_864M_CK	102
#define CLK_TOPGEN_XC_SUB_DC0_CK	103
#define CLK_TOPGEN_XC_FD_CK	104
#define CLK_TOPGEN_XC_OD_CK	105
#define CLK_TOPGEN_XC_B2R_CK	106
#define CLK_TOPGEN_XC_B2R_SUB_CK	107
#define CLK_TOPGEN_FRC_FCLK_CK	108
#define CLK_TOPGEN_FRC_FCLK_2X_CK	109
#define CLK_TOPGEN_FRC_FCLK_2XPLUS_CK	110
#define CLK_TOPGEN_DAP_CK	111
#define CLK_TOPGEN_FRC_IMI_CK	112
#define CLK_TOPGEN_RV55FRC_CK	113
#define CLK_TOPGEN_RV55PQU_CK	114
#define CLK_TOPGEN_DISP_MFDEC_CK	115
#define CLK_TOPGEN_VDEC_LAE_CK	116
#define CLK_TOPGEN_VDEC_LAE_SLOW_CK	117
#define CLK_TOPGEN_VDEC_SYS_CK	118
#define CLK_TOPGEN_VDEC_SYS_SLOW_CK	119
#define CLK_TOPGEN_SMART2_CK	120
#define CLK_TOPGEN_SMART2_SYNTH_432_CK	121
#define CLK_TOPGEN_SMART2_SYNTH_27_CK	122
#define CLK_TOPGEN_AUSDM_3M_CK	123
#define CLK_TOPGEN_HDMIRX_TIMESTAMP_CK	124
#define CLK_TOPGEN_DEMOD_ADC_INT_CK	125
#define CLK_TOPGEN_DMPLLDIV2_ATSC_INT_CK	126
#define CLK_TOPGEN_DMPLLDIV2_DTMB_INT_CK	127
#define CLK_TOPGEN_DMPLLDIV2_INT_CK	128
#define CLK_TOPGEN_DMPLLDIV3_DTMB_INT_CK	129
#define CLK_TOPGEN_DMPLLDIV3_DVBC_INT_CK	130
#define CLK_TOPGEN_DMPLLDIV3_DVBT2_INT_CK	131
#define CLK_TOPGEN_DMPLLDIV3_INT_CK	132
#define CLK_TOPGEN_DMPLLDIV3_ISDBT_INT_CK	133
#define CLK_TOPGEN_DMPLLDIV3_ISDBT_OUTER_RS_M_OVER_N_INT_CK	134
#define CLK_TOPGEN_DMPLLDIV4_DVB_OPPRO_M_OVER_N_INT_CK	135
#define CLK_TOPGEN_DMPLLDIV4_DVBS2_BCH_M_OVER_N_INT_CK	136
#define CLK_TOPGEN_DMPLLDIV4_DVBTC_RS_M_OVER_N_INT_CK	137
#define CLK_TOPGEN_DMPLLDIV4_INT_CK	138
#define CLK_TOPGEN_DMPLLDIV5_INT_CK	139
#define CLK_TOPGEN_DMPLLDIV10_INT_CK	140
#define CLK_TOPGEN_DMPLLDIV17_INT_CK	141
#define CLK_TOPGEN_AIA_BUS_CK	142
#define CLK_TOPGEN_NR	143
#define CLK_TOPGEN_VIVA_R2_WB_MUX_CK	143
#define CLK_MT5897_TOPGEN_NR	144
#define CLK_TOPGEN_ZDEC_LZD_CK	144
#define CLK_TOPGEN_ZDEC_VLD_CK	145
#define CLK_TOPGEN_AU_UART1_CK	146
#define CLK_TOPGEN_AU_UART1_SYNTH_CK	147
#define CLK_TOPGEN_PCM_CK	148
#define CLK_TOPGEN_KG1_CK	149
#define CLK_MT5876_TOPGEN_NR	150
#define CLK_TOPGEN_MIIC4_CK	150
#define CLK_MT5879_TOPGEN_NR	151




#endif