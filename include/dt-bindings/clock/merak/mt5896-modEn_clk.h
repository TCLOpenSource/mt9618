/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) *//* * Copyright (c) 2020 MediaTek Inc. */

#define CLK_MOD_EN_AISR_HVSP_SRAM2SCSR	1
#define CLK_MOD_EN_AISR_SRAM_PH_12SCSR	2
#define CLK_MOD_EN_AISR_SRAM2SCSR	3
#define CLK_MOD_EN_DAP2FRCIOPM	4
#define CLK_MOD_EN_DBK_HW_SRAM2SCPQ	5
#define CLK_MOD_EN_DELTA_B2P_SRAM2SCSCL	6
#define CLK_MOD_EN_DELTA_B2P2_SRAM2SCACE	7
#define CLK_MOD_EN_DEMURA_SRAM2SCTCON	8
#define CLK_MOD_EN_DIP1_HVSP_SRAM2SCMW	9
#define CLK_MOD_EN_DIP1_XVYCC_SRAM2SCMW	10
#define CLK_MOD_EN_DISP_MFDEC2SCNR	11
#define CLK_MOD_EN_DV_HW5_SRAM2SCDV	12
#define CLK_MOD_EN_FRC_FUART02FRCIOPM	13
#define CLK_MOD_EN_FRC_MSPI02FRCIOPM	14
#define CLK_MOD_EN_FRC_UART02FRCIOPM	15
#define CLK_MOD_EN_FRC_UART12FRCIOPM	16
#define CLK_MOD_EN_GOP0_SRAM2SCTCON	17
#define CLK_MOD_EN_GOP1_SRAM2SCTCON	18
#define CLK_MOD_EN_GOP2_SRAM2SCTCON	19
#define CLK_MOD_EN_GOP3_SRAM2SCTCON	20
#define CLK_MOD_EN_GUCD_18_LUT2_SRAM2SCACE	21
#define CLK_MOD_EN_GUCD_18_SRAM2SCSCL	22
#define CLK_MOD_EN_GUCD_256_LUT2_SRAM2SCACE	23
#define CLK_MOD_EN_GUCD_256_SRAM2SCSCL	24
#define CLK_MOD_EN_HSY_8P_SRAM2SCACE	25
#define CLK_MOD_EN_HSY_8P_SRAM2SCMW	26
#define CLK_MOD_EN_HSY_8P3_SRAM2SCACE	27
#define CLK_MOD_EN_HSY_8P3_SRAM2SCMW	28
#define CLK_MOD_EN_HSY_SP_SRAM2SCSCL	29
#define CLK_MOD_EN_HSY_SP3_SRAM2SCSCL	30
#define CLK_MOD_EN_HVSP_STP1_SRAM2SCSCL	31
#define CLK_MOD_EN_HVSP_STP2_SRAM2SCSCL	32
#define CLK_MOD_EN_LCE_ACE_SRAM2SCACE	33
#define CLK_MOD_EN_LCE_SCL_SRAM2SCSCL	34
#define CLK_MOD_EN_LD_EDGE2D_SRAM2SCTCON	35
#define CLK_MOD_EN_LDCOMP_SRAM2SCTCON	36
#define CLK_MOD_EN_LDM_GA_SRAM2SCTCON	37
#define CLK_MOD_EN_LDMCG_SRAM2SCTCON	38
#define CLK_MOD_EN_LPLL_SYN_4322FPLL	39
#define CLK_MOD_EN_LPLL_SYN_864_PIPE2FPLL	40
#define CLK_MOD_EN_LPLL_SYN_8642FPLL	41
#define CLK_MOD_EN_LPQ_PA_SRAM2SCTCON	42
#define CLK_MOD_EN_MOD_A_ODCLK2MOD_A	43
#define CLK_MOD_EN_MOD_D_ODCLK2LVDS	44
#define CLK_MOD_EN_MOD_D_ODCLK2MOD_D	45
#define CLK_MOD_EN_MOD_V1_SR_WCLK2V1	46
#define CLK_MOD_EN_MOD_V1_SR_WCLK2VBY1_V1	47
#define CLK_MOD_EN_MOD_V2_SR_WCLK2V2	48
#define CLK_MOD_EN_OSD_DV_HW5_SRAM2SCTCON	49
#define CLK_MOD_EN_PADJ_SRAM2SCTCON	50
#define CLK_MOD_EN_PANEL_GA_SRAM2SCTCON	51
#define CLK_MOD_EN_PQ_GAMMA_SRAM2SCMW	52
#define CLK_MOD_EN_RGB_3DLUT_SRAM2SCMW	53
#define CLK_MOD_EN_RV55DAP_JTAG_TCK2FRCIOPM	54
#define CLK_MOD_EN_SCNR_HDR_SRAM2SCPQ	55
#define CLK_MOD_EN_SIDCLK_HD02SCIP	56
#define CLK_MOD_EN_SIDCLK_HD12SCIP	57
#define CLK_MOD_EN_SIDCLK_HD22SCIP	58
#define CLK_MOD_EN_SIDCLK_HD32SCIP	59
#define CLK_MOD_EN_SMI2SCACE	60
#define CLK_MOD_EN_SMI2SCDV	61
#define CLK_MOD_EN_SMI2SCMW	62
#define CLK_MOD_EN_SMI2SCPQ	63
#define CLK_MOD_EN_SMI2SCSCL	64
#define CLK_MOD_EN_SMI2SCSR	65
#define CLK_MOD_EN_SPF_HDBK_SRAM2SCPQ	66
#define CLK_MOD_EN_UCD_ST_SRAM2SCIP	67
#define CLK_MOD_EN_V1_MOD_SR_RCLK_FINIAL2V1	68
#define CLK_MOD_EN_V1_MOD_SR_RCLK_PRE02V1	69
#define CLK_MOD_EN_V1_MOD_SR_RCLK_PRE12V1	70
#define CLK_MOD_EN_V1_MOD_SR_RCLK2V1	71
#define CLK_MOD_EN_V1_ODCLK_STG12V1	72
#define CLK_MOD_EN_V1_ODCLK_STG22V1	73
#define CLK_MOD_EN_V1_ODCLK_STG32V1	74
#define CLK_MOD_EN_V1_ODCLK_STG42V1	75
#define CLK_MOD_EN_V1_ODCLK_STG52V1	76
#define CLK_MOD_EN_V1_ODCLK_STG62V1	77
#define CLK_MOD_EN_V1_ODCLK_STG72V1	78
#define CLK_MOD_EN_V1_ODCLK_STG82V1	79
#define CLK_MOD_EN_V1_ODCLK_STG92V1	80
#define CLK_MOD_EN_V1_ODCLK2V1	81
#define CLK_MOD_EN_V2_MOD_SR_RCLK_PRE02V2	82
#define CLK_MOD_EN_V2_MOD_SR_RCLK_PRE12V2	83
#define CLK_MOD_EN_V2_MOD_SR_RCLK2V2	84
#define CLK_MOD_EN_V2_ODCLK_STG32V2	85
#define CLK_MOD_EN_VDBK_SRAM2SCPQ	86
#define CLK_MOD_EN_VIP_ACE_SRAM2SCACE	87
#define CLK_MOD_EN_VIP_PQ_SRAM2SCSCL	88
#define CLK_MOD_EN_XC_ADC2SCIP	89
#define CLK_MOD_EN_XC_AFBC2SCTCON	90
#define CLK_MOD_EN_XC_AISR2SCSR	91
#define CLK_MOD_EN_XC_DBG2MIU2SCIP	92
#define CLK_MOD_EN_XC_DIP02SCIP	93
#define CLK_MOD_EN_XC_DIP12SCIP	94
#define CLK_MOD_EN_XC_DIP22SCIP	95
#define CLK_MOD_EN_XC_DISP_MFDEC2SCIP	96
#define CLK_MOD_EN_XC_ED02SCIP	97
#define CLK_MOD_EN_XC_ED12SCIP	98
#define CLK_MOD_EN_XC_FD_ABF2SCTCON	99
#define CLK_MOD_EN_XC_FD_DV2SCTCON	100
#define CLK_MOD_EN_XC_FD_GOP02SCTCON	101
#define CLK_MOD_EN_XC_FD2SCIP	102
#define CLK_MOD_EN_XC_FD2SCMW	103
#define CLK_MOD_EN_XC_FD2SCSCL	104
#define CLK_MOD_EN_XC_FD2SCTCON	105
#define CLK_MOD_EN_XC_FN_22SCDV	106
#define CLK_MOD_EN_XC_FN2SCDV	107
#define CLK_MOD_EN_XC_FN2SCIP	108
#define CLK_MOD_EN_XC_FN2SCMW	109
#define CLK_MOD_EN_XC_FN2SCNR	110
#define CLK_MOD_EN_XC_FN2SCPQ_DOLBY	111
#define CLK_MOD_EN_XC_FN2SCPQ_SPF	112
#define CLK_MOD_EN_XC_FN2SCSCL	113
#define CLK_MOD_EN_XC_FN2SCSR	114
#define CLK_MOD_EN_XC_FN2SCTCON	115
#define CLK_MOD_EN_XC_FS2SCACE_HSY	116
#define CLK_MOD_EN_XC_FS2SCACE_VIP	117
#define CLK_MOD_EN_XC_FS2SCIP	118
#define CLK_MOD_EN_XC_FS2SCMW	119
#define CLK_MOD_EN_XC_FS2SCSCL	120
#define CLK_MOD_EN_XC_FS2SCSR	121
#define CLK_MOD_EN_XC_FS2SCTCON	122
#define CLK_MOD_EN_XC_GOP_SCL2SCTCON	123
#define CLK_MOD_EN_XC_GOP0_DST2SCTCON	124
#define CLK_MOD_EN_XC_GOP2SCTCON	125
#define CLK_MOD_EN_XC_GOPC_DST2SCTCON	126
#define CLK_MOD_EN_XC_GOPG_DST2SCTCON	127
#define CLK_MOD_EN_XC_HD2MIU_M2SCIP	128
#define CLK_MOD_EN_XC_HD2MIU_S2SCIP	129
#define CLK_MOD_EN_XC_HDMI02SCIP	130
#define CLK_MOD_EN_XC_HDMI12SCIP	131
#define CLK_MOD_EN_XC_HDMI22SCIP	132
#define CLK_MOD_EN_XC_HDMI32SCIP	133
#define CLK_MOD_EN_XC_ID02SCIP	134
#define CLK_MOD_EN_XC_ID12SCIP	135
#define CLK_MOD_EN_XC_IP2_SRC2SCIP	136
#define CLK_MOD_EN_XC_OD_DELTA_META2SCTCON	137
#define CLK_MOD_EN_XC_OD_DELTA2SCIP	138
#define CLK_MOD_EN_XC_OD_DELTA2SCSCL	139
#define CLK_MOD_EN_XC_OD_DELTA2SCTCON	140
#define CLK_MOD_EN_XC_OD_DIV22SCTCON	141
#define CLK_MOD_EN_XC_OD_DIV42SCTCON	142
#define CLK_MOD_EN_XC_OD_EDGE2SCTCON	143
#define CLK_MOD_EN_XC_OD_META2SCTCON	144
#define CLK_MOD_EN_XC_OD_OSD_META2SCTCON	145
#define CLK_MOD_EN_XC_OD_OSD2SCTCON	146
#define CLK_MOD_EN_XC_OD2SCIP	147
#define CLK_MOD_EN_XC_OD2SCMW	148
#define CLK_MOD_EN_XC_OD2SCSCL	149
#define CLK_MOD_EN_XC_OD2SCTCON	150
#define CLK_MOD_EN_XC_SLOW2SCIP	151
#define CLK_MOD_EN_XC_SR_ALAI2SCSR	152
#define CLK_MOD_EN_XC_SR_SRAM_SHARE2SCSR	153
#define CLK_MOD_EN_XC_SRS2SCSR	154
#define CLK_MOD_EN_XC_UCD_STAT2SCIP	155
#define CLK_MOD_EN_XC_VD2SCIP	156
#define CLK_MOD_EN_XC_VE2SCIP	157
#define CLK_MOD_EN_XC_VEIN2SCIP	158
#define CLK_MOD_EN_XTAL_12M2FRCIOPM	159
#define CLK_MOD_EN_XTAL_12M2MOD_A	160
#define CLK_MOD_EN_XTAL_12M2SCACE	161
#define CLK_MOD_EN_XTAL_12M2SCDV	162
#define CLK_MOD_EN_XTAL_12M2SCIP	163
#define CLK_MOD_EN_XTAL_12M2SCSCL	164
#define CLK_MOD_EN_XTAL_12M2SCSR	165
#define CLK_MOD_EN_XTAL_12M2SCTCON	166
#define CLK_MOD_EN_XTAL_24M2FPLL	167
#define CLK_MOD_EN_XTAL_24M2SCACE	168
#define CLK_MOD_EN_XTAL_24M2SCDV	169
#define CLK_MOD_EN_XTAL_24M2SCIP	170
#define CLK_MOD_EN_XTAL_24M2SCMW	171
#define CLK_MOD_EN_XTAL_24M2SCNR	172
#define CLK_MOD_EN_XTAL_24M2SCPQ	173
#define CLK_MOD_EN_XTAL_24M2SCSCL	174
#define CLK_MOD_EN_XTAL_24M2SCSR	175
#define CLK_MOD_EN_XTAL_24M2SCTCON	176
#define CLK_MOD_EN_XTAL_IP12SCIP	177
#define CLK_MOD_EN_XVYCC_SRAM2SCMW	178
#define CLK_MOD_EN_SMI2SCM	179
#define CLK_MOD_EN_SMI2SCMO	180
#define CLK_MOD_EN_XC_SUB_DC02SCIP	181
#define CLK_MOD_EN_XC_TGEN_WIN2SCIP	182
#define CLK_MOD_EN_XC_VEDAC2SCIP	183
#define CLK_MOD_EN_NR	184