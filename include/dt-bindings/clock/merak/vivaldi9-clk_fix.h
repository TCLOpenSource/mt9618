/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _DT_BINDINGS_CLK_MT5896_VIVALDI9_H
#define _DT_BINDINGS_CLK_MT5896_VIVALDI9_H
#define AU_RIU_R2_WR_CK	1
#define AU_RIU_AU_SRC_CK	2
#define AU_MPLL_216M_CK	3
#define AU_MPLL_216M_DIV2_CK	4
#define AU_MPLL_216M_DIV4_CK	5
#define AU_MPLL_216M_DIV8_CK	6
#define AU_MPLL_216M_DIV16_CK	7
#define AU_MPLL_432M_CK	8
#define AU_MPLL_345M_CK	9
#define AU_MPLL_288M_CK	10
#define AU_MPLL_172M_CK	11
#define AU_MPLL_144M_CK	12
#define AU_AUPLL_KF0_CK	13
#define AU_AUPLL_KF0_DIV_10_CK	14
#define AU_AUPLL_KP0_DIV10_CK	15
#define AU_AUPLL_KP1_CK	16
#define AU_AUPLL_KP1_DIV2_CK	17
#define AU_AUPLL_KP1_DIV10_CK	18
#define AU_AUPLL_KPDIV_CK	19
#define AU_AUPLL_KPDIV_DIV2_CK	20
#define AU_AUPLL_KPDIV_DIV4_CK	21
#define AU_AUPLL_KPDIV_DIV8_CK	22
#define AU_AUPLL_KPDIV_DIV16_CK	23
#define AU_UTMI_480M_CK	24
#define AU_AUR2PLL_CK	25
#define AU_RV_TCK_CK	26
#define AU_DAP_TCK_CK	27
#define AU_DMD_MPLL_DIV8_CK	28
#define AU_DMD_MPLL_DIV10_CK	29
#define AU_DMD_MPLL_DIV4_CK	30
#define AU_DMD_MPLL_DIV5_CK	31
#define AU_SYNTH_HDMI_CTSN_256FS_OUT_CK	32
#define AU_SYNTH_HDMI_CTSN_256FS_OUT_DIV4_CK	33
#define AU_SYNTH_HDMI_CTSN_256FS_OUT_DIV8_CK	34
#define AU_SYNTH_HDMI_CTSN_256FS_OUT_DIV16_CK	35
#define AU_SYNTH_HDMI_CTSN_256FS_OUT_DIV32_CK	36
#define AU_SYNTH_HDMI_CTSN_128FS_OUT_CK	37
#define AU_SYNTH_HDMI_PATGEN_256FS_OUT_CK	38
#define AU_SYNTH_HDMI_PATGEN_256FS_OUT_DIV2_CK	39
#define AU_SYNTH_HDMI2_CTSN_256FS_OUT_CK	40
#define AU_SYNTH_HDMI2_CTSN_256FS_OUT_DIV4_CK	41
#define AU_SYNTH_HDMI2_CTSN_256FS_OUT_DIV8_CK	42
#define AU_SYNTH_HDMI2_CTSN_256FS_OUT_DIV16_CK	43
#define AU_SYNTH_HDMI2_CTSN_256FS_OUT_DIV32_CK	44
#define AU_SYNTH_HDMI2_CTSN_128FS_OUT_CK	45
#define AU_SYNTH_HDMI2_PATGEN_256FS_OUT_CK	46
#define AU_SYNTH_HDMI2_PATGEN_256FS_OUT_DIV2_CK	47
#define AU_PAD_TEST_IN_CK	48
#define AU_SYNTH_2ND_ORDER_OUT_CK	49
#define AU_SYNTH_SSC_2ND_ORDER_OUT_DIV4_CK	50
#define AU_SYNTH_SSC_2ND_ORDER_OUT_DIV8_CK	51
#define AU_SYNTH_SSC_2ND_ORDER_OUT_DIV16_CK	52
#define AU_SYNTH_CODEC_OUT_CK	53
#define AU_SYNTH_DVB1_256FS_OUT_CK	54
#define AU_SYNTH_DVB1_256FS_OUT_DIV2_CK	55
#define AU_SYNTH_DVB2_256FS_OUT_CK	56
#define AU_SYNTH_DVB2_256FS_OUT_DIV2_CK	57
#define AU_SYNTH_DVB3_256FS_OUT_CK	58
#define AU_SYNTH_DVB3_256FS_OUT_DIV2_CK	59
#define AU_SYNTH_DVB4_256FS_OUT_CK	60
#define AU_SYNTH_DVB4_256FS_OUT_DIV2_CK	61
#define AU_SYNTH_DVB5_256FS_OUT_CK	62
#define AU_SYNTH_DVB6_256FS_OUT_CK	63
#define AU_SYNTH_SIF_OUT_CK	64
#define AU_SYNTH_SIF_OUT_DIV2_CK	65
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_CK	66
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_DIV2_CK	67
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_DIV4_CK	68
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_DIV8_CK	69
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_DIV16_CK	70
#define AU_SYNTH_SPDIF_CDR_256FS_OUT_DIV32_CK	71
#define AU_SYNTH_SPDIF_CDR_128FS_OUT_CK	72
#define AU_SYNTH_I2S_FS_256FS_OUT_CK	73
#define AU_SYNTH_PCM_TRX_256FS_OUT_CK	74
#define AU_SYNTH_I2S_2_FS_256FS_OUT_CK	75
#define AU_SYNTH_AUDMA_V2_R1_256FS_OUT_CK	76
#define AU_SYNTH_AUDMA_V2_R2_256FS_OUT_CK	77
#define AU_PAD_I2S_RX_BCLK_CK	78
#define AU_BCLK_I2S_DECODER_TIME_GEN_CK	79
#define AU_SYNTH_CLK_I2S_BCLK_DECODER_CK	80
#define AU_PAD_I2S_RX2_BCLK_CK	81
#define AU_BCLK_I2S_DECODER2_TIME_GEN_CK	82
#define AU_SYNTH_MCLK_I2S_ENCODER_OUT_CK	83
#define AU_SYNTH_BCLK_I2S_ENCODER_OUT_CK	84
#define AU_SYNTH_MCLK_I2S_ENCODER2_OUT_CK	85
#define AU_SYNTH_BCLK_I2S_ENCODER2_OUT_CK	86
#define AU_SYNTH_SPDIF_TX_1024FS_OUT_CK	87
#define AU_SYNTH_SPDIF_TX_1024FS_OUT_DIV2_CK	88
#define AU_SYNTH_SPDIF_NPCM_256FS_OUT_CK	89
#define AU_SYNTH_SPDIF_NPCM_256FS_OUT_DIV2_CK	90
#define AU_SYNTH_SPDIF_NPCM2_256FS_OUT_CK	91
#define AU_SYNTH_SPDIF_NPCM2_256FS_OUT_DIV2_CK	92
#define AU_SYNTH_SPDIF_NPCM3_256FS_OUT_CK	93
#define AU_SYNTH_SPDIF_NPCM3_256FS_OUT_DIV2_CK	94
#define AU_SYNTH_SPDIF_NPCM3_256FS_OUT_DIV4_CK	95
#define AU_SYNTH_SPDIF_NPCM3_256FS_OUT_DIV8_CK	96
#define AU_SYNTH_NPCM4_256FS_OUT_CK	97
#define AU_SYNTH_NPCM5_256FS_OUT_CK	98
#define AU_EARC_TX_PLL_CK	99
#define AU_SYNTH_EARC_TX_GPA_1024FS_OUT_CK	100
#define AU_SYNTH_EARC_TX_256FS_OUT_CK	101
#define AU_SYNTH_EARC_TX_IIR_HDMI_RX_256FS_OUT_CK	102
#define AU_FIX_N	103
#endif