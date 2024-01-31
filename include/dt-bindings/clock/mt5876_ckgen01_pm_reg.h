/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_CLK_MT5876_CKGEN01_PM_H
#define _DT_BINDINGS_CLK_MT5876_CKGEN01_PM_H

#define REG_SW_EN_SMI2PM_SSUSB 0x200
#define REG_SW_EN_SMI2PM_SSUSB_S 0
#define REG_SW_EN_SMI2PM_SSUSB_E 0
#define REG_SW_EN_SMI2PM_SMI 0x200
#define REG_SW_EN_SMI2PM_SMI_S 1
#define REG_SW_EN_SMI2PM_SMI_E 1
#define REG_SW_EN_SMI2PM_PEXTP 0x200
#define REG_SW_EN_SMI2PM_PEXTP_S 2
#define REG_SW_EN_SMI2PM_PEXTP_E 2
#define REG_SW_EN_CM4_CORE2CM4_CORE 0x204
#define REG_SW_EN_CM4_CORE2CM4_CORE_S 0
#define REG_SW_EN_CM4_CORE2CM4_CORE_E 0
#define REG_SW_EN_CM4_DFS_REF2CM4 0x208
#define REG_SW_EN_CM4_DFS_REF2CM4_S 0
#define REG_SW_EN_CM4_DFS_REF2CM4_E 0
#define REG_SW_EN_CM4_SYSTICK2CM4_SYSTICK 0x20c
#define REG_SW_EN_CM4_SYSTICK2CM4_SYSTICK_S 0
#define REG_SW_EN_CM4_SYSTICK2CM4_SYSTICK_E 0
#define REG_SW_EN_CM4_TCK2TCK_INT 0x210
#define REG_SW_EN_CM4_TCK2TCK_INT_S 0
#define REG_SW_EN_CM4_TCK2TCK_INT_E 0
#define REG_SW_EN_CM4_TSVALUEB2CM4_TSVALUEB 0x214
#define REG_SW_EN_CM4_TSVALUEB2CM4_TSVALUEB_S 0
#define REG_SW_EN_CM4_TSVALUEB2CM4_TSVALUEB_E 0
#define REG_SW_EN_CM42CM4 0x218
#define REG_SW_EN_CM42CM4_S 0
#define REG_SW_EN_CM42CM4_E 0
#define REG_SW_EN_CODEC_I2S_MCK2CODEC_I2S_MCK 0x21c
#define REG_SW_EN_CODEC_I2S_MCK2CODEC_I2S_MCK_S 0
#define REG_SW_EN_CODEC_I2S_MCK2CODEC_I2S_MCK_E 0
#define REG_SW_EN_CODEC_I2S_RX_BCK2CODEC_I2S_RX_BCK 0x220
#define REG_SW_EN_CODEC_I2S_RX_BCK2CODEC_I2S_RX_BCK_S 0
#define REG_SW_EN_CODEC_I2S_RX_BCK2CODEC_I2S_RX_BCK_E 0
#define REG_SW_EN_CODEC_I2S_RX_MS_BCK_P2CODEC_I2S_RX_MS_BCK_P 0x224
#define REG_SW_EN_CODEC_I2S_RX_MS_BCK_P2CODEC_I2S_RX_MS_BCK_P_S 0
#define REG_SW_EN_CODEC_I2S_RX_MS_BCK_P2CODEC_I2S_RX_MS_BCK_P_E 0
#define REG_SW_EN_DIG_MIC2DIG_MIC 0x228
#define REG_SW_EN_DIG_MIC2DIG_MIC_S 0
#define REG_SW_EN_DIG_MIC2DIG_MIC_E 0
#define REG_SW_EN_I2S_RX_BCK_DG2I2S_RX_BCK_DG 0x22c
#define REG_SW_EN_I2S_RX_BCK_DG2I2S_RX_BCK_DG_S 0
#define REG_SW_EN_I2S_RX_BCK_DG2I2S_RX_BCK_DG_E 0
#define REG_SW_EN_I2S_TEST_TXI2I2S_TEST_TXI_DG 0x230
#define REG_SW_EN_I2S_TEST_TXI2I2S_TEST_TXI_DG_S 0
#define REG_SW_EN_I2S_TEST_TXI2I2S_TEST_TXI_DG_E 0
#define REG_SW_EN_I2S_TEST_TXO2I2S_TEST_TXO_DG 0x234
#define REG_SW_EN_I2S_TEST_TXO2I2S_TEST_TXO_DG_S 0
#define REG_SW_EN_I2S_TEST_TXO2I2S_TEST_TXO_DG_E 0
#define REG_SW_EN_IMI_DFS_REF2PAGANINI_IMI_REF 0x238
#define REG_SW_EN_IMI_DFS_REF2PAGANINI_IMI_REF_S 0
#define REG_SW_EN_IMI_DFS_REF2PAGANINI_IMI_REF_E 0
#define REG_SW_EN_IMI2PAGANINI_IMI 0x23c
#define REG_SW_EN_IMI2PAGANINI_IMI_S 0
#define REG_SW_EN_IMI2PAGANINI_IMI_E 0
#define REG_SW_EN_NF_SYNTH_REF2NF_SYNTH_REF 0x240
#define REG_SW_EN_NF_SYNTH_REF2NF_SYNTH_REF_S 0
#define REG_SW_EN_NF_SYNTH_REF2NF_SYNTH_REF_E 0
#define REG_SW_EN_NF_SYNTH_REF_SEL2NF_SYNTH_REF_SEL 0x240
#define REG_SW_EN_NF_SYNTH_REF_SEL2NF_SYNTH_REF_SEL_S 1
#define REG_SW_EN_NF_SYNTH_REF_SEL2NF_SYNTH_REF_SEL_E 1
#define REG_SW_EN_PLL2PAGADIV2PLLDIV 0x244
#define REG_SW_EN_PLL2PAGADIV2PLLDIV_S 0
#define REG_SW_EN_PLL2PAGADIV2PLLDIV_E 0
#define REG_SW_EN_SRC_A1_256FSI2SRC_A1_256FSI 0x248
#define REG_SW_EN_SRC_A1_256FSI2SRC_A1_256FSI_S 0
#define REG_SW_EN_SRC_A1_256FSI2SRC_A1_256FSI_E 0
#define REG_SW_EN_PAGA_PLL_SEL2PAGA_PLL_SEL 0x24c
#define REG_SW_EN_PAGA_PLL_SEL2PAGA_PLL_SEL_S 0
#define REG_SW_EN_PAGA_PLL_SEL2PAGA_PLL_SEL_E 0
#define REG_SW_EN_VREC_MAC2VREC_MAC 0x250
#define REG_SW_EN_VREC_MAC2VREC_MAC_S 0
#define REG_SW_EN_VREC_MAC2VREC_MAC_E 0
#define REG_SW_EN_XTAL_24M2CM4 0x254
#define REG_SW_EN_XTAL_24M2CM4_S 0
#define REG_SW_EN_XTAL_24M2CM4_E 0
#define REG_SW_EN_XTAL_PAGANINI_TESTBUS2XTAL_PAGANINI_TESTBUS 0x258
#define REG_SW_EN_XTAL_PAGANINI_TESTBUS2XTAL_PAGANINI_TESTBUS_S 0
#define REG_SW_EN_XTAL_PAGANINI_TESTBUS2XTAL_PAGANINI_TESTBUS_E 0
#define REG_SW_EN_IMI_DIV22PAGANINI_IMI 0x25c
#define REG_SW_EN_IMI_DIV22PAGANINI_IMI_S 0
#define REG_SW_EN_IMI_DIV22PAGANINI_IMI_E 0
#define REG_SW_EN_SMI_DIV22PM_SMI 0x260
#define REG_SW_EN_SMI_DIV22PM_SMI_S 0
#define REG_SW_EN_SMI_DIV22PM_SMI_E 0
#define REG_SW_EN_SMI_DIV22PM_MIIC0 0x260
#define REG_SW_EN_SMI_DIV22PM_MIIC0_S 1
#define REG_SW_EN_SMI_DIV22PM_MIIC0_E 1
#define REG_SW_EN_SMI_DIV22PM_MIIC1 0x260
#define REG_SW_EN_SMI_DIV22PM_MIIC1_S 2
#define REG_SW_EN_SMI_DIV22PM_MIIC1_E 2
#define REG_SW_EN_SMI_DIV22PM_FUART 0x260
#define REG_SW_EN_SMI_DIV22PM_FUART_S 3
#define REG_SW_EN_SMI_DIV22PM_FUART_E 3
#define REG_SW_EN_SMI_DIV22CH7_XD2MIU 0x260
#define REG_SW_EN_SMI_DIV22CH7_XD2MIU_S 4
#define REG_SW_EN_SMI_DIV22CH7_XD2MIU_E 4
#define REG_SW_EN_CEC12PM 0x264
#define REG_SW_EN_CEC12PM_S 0
#define REG_SW_EN_CEC12PM_E 0
#define REG_SW_EN_CEC22PM 0x268
#define REG_SW_EN_CEC22PM_S 0
#define REG_SW_EN_CEC22PM_E 0
#define REG_SW_EN_CEC2PM 0x26c
#define REG_SW_EN_CEC2PM_S 0
#define REG_SW_EN_CEC2PM_E 0
#define REG_SW_EN_CEC32PM 0x270
#define REG_SW_EN_CEC32PM_S 0
#define REG_SW_EN_CEC32PM_E 0
#define REG_SW_EN_DDC2PM 0x274
#define REG_SW_EN_DDC2PM_S 0
#define REG_SW_EN_DDC2PM_E 0
#define REG_SW_EN_DVI_RAW02PM 0x278
#define REG_SW_EN_DVI_RAW02PM_S 0
#define REG_SW_EN_DVI_RAW02PM_E 0
#define REG_SW_EN_DVI_RAW12PM 0x27c
#define REG_SW_EN_DVI_RAW12PM_S 0
#define REG_SW_EN_DVI_RAW12PM_E 0
#define REG_SW_EN_DVI_RAW22PM 0x280
#define REG_SW_EN_DVI_RAW22PM_S 0
#define REG_SW_EN_DVI_RAW22PM_E 0
#define REG_SW_EN_DVI_RAW32PM 0x284
#define REG_SW_EN_DVI_RAW32PM_S 0
#define REG_SW_EN_DVI_RAW32PM_E 0
#define REG_SW_EN_HOTPLUG2PM 0x288
#define REG_SW_EN_HOTPLUG2PM_S 0
#define REG_SW_EN_HOTPLUG2PM_E 0
#define REG_SW_EN_SCDC_P02PM 0x28c
#define REG_SW_EN_SCDC_P02PM_S 0
#define REG_SW_EN_SCDC_P02PM_E 0
#define REG_SW_EN_SCDC_P12PM 0x290
#define REG_SW_EN_SCDC_P12PM_S 0
#define REG_SW_EN_SCDC_P12PM_E 0
#define REG_SW_EN_SCDC_P22PM 0x294
#define REG_SW_EN_SCDC_P22PM_S 0
#define REG_SW_EN_SCDC_P22PM_E 0
#define REG_SW_EN_SCDC_P32PM 0x298
#define REG_SW_EN_SCDC_P32PM_S 0
#define REG_SW_EN_SCDC_P32PM_E 0
#define REG_SW_EN_SD2PM 0x2a4
#define REG_SW_EN_SD2PM_S 0
#define REG_SW_EN_SD2PM_E 0
#define REG_SW_EN_ADCPLL_TEST2EPHY 0x2a8
#define REG_SW_EN_ADCPLL_TEST2EPHY_S 0
#define REG_SW_EN_ADCPLL_TEST2EPHY_E 0
#define REG_SW_EN_EMAC_RX2ETH 0x2ac
#define REG_SW_EN_EMAC_RX2ETH_S 0
#define REG_SW_EN_EMAC_RX2ETH_E 0
#define REG_SW_EN_EMAC_TX2ETH 0x2b0
#define REG_SW_EN_EMAC_TX2ETH_S 0
#define REG_SW_EN_EMAC_TX2ETH_E 0
#define REG_SW_EN_EMAC_TESTRX1252PM 0x2b4
#define REG_SW_EN_EMAC_TESTRX1252PM_S 0
#define REG_SW_EN_EMAC_TESTRX1252PM_E 0
#define REG_SW_EN_GMAC_RX2ETH 0x2b8
#define REG_SW_EN_GMAC_RX2ETH_S 0
#define REG_SW_EN_GMAC_RX2ETH_E 0
#define REG_SW_EN_GMAC_TX2ETH 0x2bc
#define REG_SW_EN_GMAC_TX2ETH_S 0
#define REG_SW_EN_GMAC_TX2ETH_E 0
#define REG_SW_EN_GMAC_TX_PAD2ETH 0x2c0
#define REG_SW_EN_GMAC_TX_PAD2ETH_S 0
#define REG_SW_EN_GMAC_TX_PAD2ETH_E 0
#define REG_SW_EN_CLK_125M_TX_EEE2EPHY 0x2c4
#define REG_SW_EN_CLK_125M_TX_EEE2EPHY_S 0
#define REG_SW_EN_CLK_125M_TX_EEE2EPHY_E 0
#define REG_SW_EN_CLK_25M_RX2EPHY 0x2c8
#define REG_SW_EN_CLK_25M_RX2EPHY_S 0
#define REG_SW_EN_CLK_25M_RX2EPHY_E 0
#define REG_SW_EN_CLK_25M_TX2EPHY 0x2cc
#define REG_SW_EN_CLK_25M_TX2EPHY_S 0
#define REG_SW_EN_CLK_25M_TX2EPHY_E 0
#define REG_SW_EN_CLK_2P5M_RX2EPHY 0x2d0
#define REG_SW_EN_CLK_2P5M_RX2EPHY_S 0
#define REG_SW_EN_CLK_2P5M_RX2EPHY_E 0
#define REG_SW_EN_CLK_2P5M_TX2EPHY 0x2d4
#define REG_SW_EN_CLK_2P5M_TX2EPHY_S 0
#define REG_SW_EN_CLK_2P5M_TX2EPHY_E 0
#define REG_SW_EN_DAC1X2EPHY 0x2d8
#define REG_SW_EN_DAC1X2EPHY_S 0
#define REG_SW_EN_DAC1X2EPHY_E 0
#define REG_SW_EN_DAC2X2EPHY 0x2dc
#define REG_SW_EN_DAC2X2EPHY_S 0
#define REG_SW_EN_DAC2X2EPHY_E 0
#define REG_SW_EN_DACTX2EPHY 0x2e0
#define REG_SW_EN_DACTX2EPHY_S 0
#define REG_SW_EN_DACTX2EPHY_E 0
#define REG_SW_EN_ETHPLL_250M2EPHY 0x2e4
#define REG_SW_EN_ETHPLL_250M2EPHY_S 0
#define REG_SW_EN_ETHPLL_250M2EPHY_E 0
#define REG_SW_EN_LS2EPHY 0x2e8
#define REG_SW_EN_LS2EPHY_S 0
#define REG_SW_EN_LS2EPHY_E 0
#define REG_SW_EN_MCU_62P52EPHY 0x2ec
#define REG_SW_EN_MCU_62P52EPHY_S 0
#define REG_SW_EN_MCU_62P52EPHY_E 0
#define REG_SW_EN_MII_RX_FREE2EPHY 0x2f0
#define REG_SW_EN_MII_RX_FREE2EPHY_S 0
#define REG_SW_EN_MII_RX_FREE2EPHY_E 0
#define REG_SW_EN_MII_RX2EPHY 0x2f4
#define REG_SW_EN_MII_RX2EPHY_S 0
#define REG_SW_EN_MII_RX2EPHY_E 0
#define REG_SW_EN_MII_TX2EPHY 0x2f8
#define REG_SW_EN_MII_TX2EPHY_S 0
#define REG_SW_EN_MII_TX2EPHY_E 0
#define REG_SW_EN_RX_22EPHY 0x2fc
#define REG_SW_EN_RX_22EPHY_S 0
#define REG_SW_EN_RX_22EPHY_E 0
#define REG_SW_EN_RX_GAT1002EPHY 0x300
#define REG_SW_EN_RX_GAT1002EPHY_S 0
#define REG_SW_EN_RX_GAT1002EPHY_E 0
#define REG_SW_EN_RX2EPHY 0x304
#define REG_SW_EN_RX2EPHY_S 0
#define REG_SW_EN_RX2EPHY_E 0
#define REG_SW_EN_RX_LPI2EPHY 0x308
#define REG_SW_EN_RX_LPI2EPHY_S 0
#define REG_SW_EN_RX_LPI2EPHY_E 0
#define REG_SW_EN_XIN_FREE2EPHY 0x30c
#define REG_SW_EN_XIN_FREE2EPHY_S 0
#define REG_SW_EN_XIN_FREE2EPHY_E 0
#define REG_SW_EN_XIN2EPHY 0x310
#define REG_SW_EN_XIN2EPHY_S 0
#define REG_SW_EN_XIN2EPHY_E 0
#define REG_SW_EN_MCU_BUS_PM2PM 0x31c
#define REG_SW_EN_MCU_BUS_PM2PM_S 0
#define REG_SW_EN_MCU_BUS_PM2PM_E 0
#define REG_SW_EN_MCU_PM2PM 0x320
#define REG_SW_EN_MCU_PM2PM_S 0
#define REG_SW_EN_MCU_PM2PM_E 0
#define REG_SW_EN_MCU_PM2CM4 0x320
#define REG_SW_EN_MCU_PM2CM4_S 1
#define REG_SW_EN_MCU_PM2CM4_E 1
#define REG_SW_EN_MCU_PM2PM51 0x320
#define REG_SW_EN_MCU_PM2PM51_S 2
#define REG_SW_EN_MCU_PM2PM51_E 2
#define REG_SW_EN_MIIC0_PM2PM 0x324
#define REG_SW_EN_MIIC0_PM2PM_S 0
#define REG_SW_EN_MIIC0_PM2PM_E 0
#define REG_SW_EN_MIIC1_PM2PM 0x328
#define REG_SW_EN_MIIC1_PM2PM_S 0
#define REG_SW_EN_MIIC1_PM2PM_E 0
#define REG_SW_EN_SPI_PM2PM 0x32c
#define REG_SW_EN_SPI_PM2PM_S 0
#define REG_SW_EN_SPI_PM2PM_E 0
#define REG_SW_EN_UART0_PM2PM 0x330
#define REG_SW_EN_UART0_PM2PM_S 0
#define REG_SW_EN_UART0_PM2PM_E 0
#define REG_SW_EN_UART0_PM_SYNTH2PM 0x334
#define REG_SW_EN_UART0_PM_SYNTH2PM_S 0
#define REG_SW_EN_UART0_PM_SYNTH2PM_E 0
#define REG_SW_EN_UART1_PM2PM 0x338
#define REG_SW_EN_UART1_PM2PM_S 0
#define REG_SW_EN_UART1_PM2PM_E 0
#define REG_SW_EN_UART1_PM_SYNTH2PM 0x33c
#define REG_SW_EN_UART1_PM_SYNTH2PM_S 0
#define REG_SW_EN_UART1_PM_SYNTH2PM_E 0
#define REG_SW_EN_FUART0_PM2PM 0x340
#define REG_SW_EN_FUART0_PM2PM_S 0
#define REG_SW_EN_FUART0_PM2PM_E 0
#define REG_SW_EN_FUART0_PM_SYNTH2PM 0x344
#define REG_SW_EN_FUART0_PM_SYNTH2PM_S 0
#define REG_SW_EN_FUART0_PM_SYNTH2PM_E 0
#define REG_SW_EN_XIU_RX2PM 0x348
#define REG_SW_EN_XIU_RX2PM_S 0
#define REG_SW_EN_XIU_RX2PM_E 0
#define REG_SW_EN_PM_ICE_JTCK2PM 0x34c
#define REG_SW_EN_PM_ICE_JTCK2PM_S 0
#define REG_SW_EN_PM_ICE_JTCK2PM_E 0
#define REG_SW_EN_MCU_NONPM2PM 0x350
#define REG_SW_EN_MCU_NONPM2PM_S 0
#define REG_SW_EN_MCU_NONPM2PM_E 0
#define REG_SW_EN_XTAL_PADOUT2PM 0x35c
#define REG_SW_EN_XTAL_PADOUT2PM_S 0
#define REG_SW_EN_XTAL_PADOUT2PM_E 0
#define REG_SW_EN_RTC_PADOUT2PM 0x360
#define REG_SW_EN_RTC_PADOUT2PM_S 0
#define REG_SW_EN_RTC_PADOUT2PM_E 0
#define REG_SW_EN_IR2PM 0x364
#define REG_SW_EN_IR2PM_S 0
#define REG_SW_EN_IR2PM_E 0
#define REG_SW_EN_KREF2PM 0x368
#define REG_SW_EN_KREF2PM_S 0
#define REG_SW_EN_KREF2PM_E 0
#define REG_SW_EN_PM_SLEEP2PM 0x36c
#define REG_SW_EN_PM_SLEEP2PM_S 0
#define REG_SW_EN_PM_SLEEP2PM_E 0
#define REG_SW_EN_RTC22PM 0x370
#define REG_SW_EN_RTC22PM_S 0
#define REG_SW_EN_RTC22PM_E 0
#define REG_SW_EN_RTC2PM 0x374
#define REG_SW_EN_RTC2PM_S 0
#define REG_SW_EN_RTC2PM_E 0
#define REG_SW_EN_SAR2PM 0x378
#define REG_SW_EN_SAR2PM_S 0
#define REG_SW_EN_SAR2PM_E 0
#define REG_SW_EN_PWM_PM2PM 0x384
#define REG_SW_EN_PWM_PM2PM_S 0
#define REG_SW_EN_PWM_PM2PM_E 0
#define REG_SW_EN_AHB2PM_SSUSB 0x38c
#define REG_SW_EN_AHB2PM_SSUSB_S 0
#define REG_SW_EN_AHB2PM_SSUSB_E 0
#define REG_SW_EN_AHB2PM_SSUSB_U3PHY 0x38c
#define REG_SW_EN_AHB2PM_SSUSB_U3PHY_S 1
#define REG_SW_EN_AHB2PM_SSUSB_U3PHY_E 1
#define REG_SW_EN_AHB2SSUSB_U2PHY 0x38c
#define REG_SW_EN_AHB2SSUSB_U2PHY_S 2
#define REG_SW_EN_AHB2SSUSB_U2PHY_E 2
#define REG_SW_EN_AHB2PM_PEXTP 0x38c
#define REG_SW_EN_AHB2PM_PEXTP_S 3
#define REG_SW_EN_AHB2PM_PEXTP_E 3
#define REG_SW_EN_AXI2PM_SSUSB 0x390
#define REG_SW_EN_AXI2PM_SSUSB_S 0
#define REG_SW_EN_AXI2PM_SSUSB_E 0
#define REG_SW_EN_AXI2PM_PEXTP 0x390
#define REG_SW_EN_AXI2PM_PEXTP_S 1
#define REG_SW_EN_AXI2PM_PEXTP_E 1
#define REG_SW_EN_MCU_PM2PM_SSUSB 0x394
#define REG_SW_EN_MCU_PM2PM_SSUSB_S 0
#define REG_SW_EN_MCU_PM2PM_SSUSB_E 0
#define REG_SW_EN_MCU_PM2PM_SSUSB_U3PHY 0x394
#define REG_SW_EN_MCU_PM2PM_SSUSB_U3PHY_S 1
#define REG_SW_EN_MCU_PM2PM_SSUSB_U3PHY_E 1
#define REG_SW_EN_MCU_PM2SSUSB_U2PHY 0x394
#define REG_SW_EN_MCU_PM2SSUSB_U2PHY_S 2
#define REG_SW_EN_MCU_PM2SSUSB_U2PHY_E 2
#define REG_SW_EN_MCU_PM2PM_PEXTP 0x394
#define REG_SW_EN_MCU_PM2PM_PEXTP_S 3
#define REG_SW_EN_MCU_PM2PM_PEXTP_E 3
#define REG_SW_EN_SSUSB_DMA2PM_SSUSB 0x398
#define REG_SW_EN_SSUSB_DMA2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_DMA2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_FRMCNT2PM_SSUSB 0x39c
#define REG_SW_EN_SSUSB_FRMCNT2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_FRMCNT2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_PCLK2PM_SSUSB 0x3a0
#define REG_SW_EN_SSUSB_PCLK2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_PCLK2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_SCL2PM_SSUSB 0x3a4
#define REG_SW_EN_SSUSB_SCL2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_SCL2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_SDIN2PM_SSUSB 0x3a8
#define REG_SW_EN_SSUSB_SDIN2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_SDIN2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_SYS2PM_SSUSB 0x3ac
#define REG_SW_EN_SSUSB_SYS2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_SYS2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_UTMI2PM_SSUSB 0x3b0
#define REG_SW_EN_SSUSB_UTMI2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_UTMI2PM_SSUSB_E 0
#define REG_SW_EN_SSUSB_XHCI2PM_SSUSB 0x3b4
#define REG_SW_EN_SSUSB_XHCI2PM_SSUSB_S 0
#define REG_SW_EN_SSUSB_XHCI2PM_SSUSB_E 0
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB 0x3b8
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB_S 0
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB_E 0
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB_U3PHY 0x3b8
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB_U3PHY_S 1
#define REG_SW_EN_XTAL_24M_PM2PM_SSUSB_U3PHY_E 1
#define REG_SW_EN_XTAL_24M_PM2SSUSB_U2PHY 0x3b8
#define REG_SW_EN_XTAL_24M_PM2SSUSB_U2PHY_S 2
#define REG_SW_EN_XTAL_24M_PM2SSUSB_U2PHY_E 2
#define REG_SW_EN_XTAL_24M_PM2PM_PEXTP 0x3b8
#define REG_SW_EN_XTAL_24M_PM2PM_PEXTP_S 3
#define REG_SW_EN_XTAL_24M_PM2PM_PEXTP_E 3
#define REG_SW_EN_PEXTP_TL2PM_PEXTP 0x3bc
#define REG_SW_EN_PEXTP_TL2PM_PEXTP_S 3
#define REG_SW_EN_PEXTP_TL2PM_PEXTP_E 3
#define REG_SW_EN_XTAL_12M_PM2PM_PAD_TOP 0x3cc
#define REG_SW_EN_XTAL_12M_PM2PM_PAD_TOP_S 0
#define REG_SW_EN_XTAL_12M_PM2PM_PAD_TOP_E 0
#define REG_SW_EN_XTAL_24M_PM2PM_MBIST 0x3dc
#define REG_SW_EN_XTAL_24M_PM2PM_MBIST_S 0
#define REG_SW_EN_XTAL_24M_PM2PM_MBIST_E 0
#define REG_SW_EN_RIU_NONPM2RIU_DFT_NONPM_PM 0x3e0
#define REG_SW_EN_RIU_NONPM2RIU_DFT_NONPM_PM_S 0
#define REG_SW_EN_RIU_NONPM2RIU_DFT_NONPM_PM_E 0
#define REG_GATE_INV_WLOCK_PASSWD_PM 0x3e8
#define REG_GATE_INV_WLOCK_PASSWD_PM_S 0
#define REG_GATE_INV_WLOCK_PASSWD_PM_E 15
#define REG_ALL_SW_EN_PASSWD_PM 0x3ec
#define REG_ALL_SW_EN_PASSWD_PM_S 0
#define REG_ALL_SW_EN_PASSWD_PM_E 15
#define REG_PM_ALL_SW_EN_OVERWRITE 0x3f0
#define REG_PM_ALL_SW_EN_OVERWRITE_S 0
#define REG_PM_ALL_SW_EN_OVERWRITE_E 0
#define REG_PAGANINI_ALL_SW_EN_OVERWRITE 0x3f0
#define REG_PAGANINI_ALL_SW_EN_OVERWRITE_S 2
#define REG_PAGANINI_ALL_SW_EN_OVERWRITE_E 2
#define REG_PM_USB_PCIE_ALL_SW_EN_OVERWRITE 0x3f0
#define REG_PM_USB_PCIE_ALL_SW_EN_OVERWRITE_S 3
#define REG_PM_USB_PCIE_ALL_SW_EN_OVERWRITE_E 3
#define REG_PM_ETH_ALL_SW_EN_OVERWRITE 0x3f0
#define REG_PM_ETH_ALL_SW_EN_OVERWRITE_S 4
#define REG_PM_ETH_ALL_SW_EN_OVERWRITE_E 4
#define REG_PM_EPHY_ALL_SW_EN_OVERWRITE 0x3f0
#define REG_PM_EPHY_ALL_SW_EN_OVERWRITE_S 5
#define REG_PM_EPHY_ALL_SW_EN_OVERWRITE_E 5
#define REG_PM_ALL_SW_EN_VALUE 0x3f4
#define REG_PM_ALL_SW_EN_VALUE_S 0
#define REG_PM_ALL_SW_EN_VALUE_E 0
#define REG_PAGANINI_ALL_SW_EN_VALUE 0x3f4
#define REG_PAGANINI_ALL_SW_EN_VALUE_S 2
#define REG_PAGANINI_ALL_SW_EN_VALUE_E 2
#define REG_PM_USB_PCIE_ALL_SW_EN_VALUE 0x3f4
#define REG_PM_USB_PCIE_ALL_SW_EN_VALUE_S 3
#define REG_PM_USB_PCIE_ALL_SW_EN_VALUE_E 3
#define REG_PM_ETH_ALL_SW_EN_VALUE 0x3f4
#define REG_PM_ETH_ALL_SW_EN_VALUE_S 4
#define REG_PM_ETH_ALL_SW_EN_VALUE_E 4
#define REG_PM_EPHY_ALL_SW_EN_VALUE 0x3f4
#define REG_PM_EPHY_ALL_SW_EN_VALUE_S 5
#define REG_PM_EPHY_ALL_SW_EN_VALUE_E 5
#define REG_CKGEN01_PM_DUMMY0 0x3f8
#define REG_CKGEN01_PM_DUMMY0_S 0
#define REG_CKGEN01_PM_DUMMY0_E 15
#define REG_CKGEN01_PM_DUMMY1 0x3fc
#define REG_CKGEN01_PM_DUMMY1_S 0
#define REG_CKGEN01_PM_DUMMY1_E 15
#define REG_CKG_MIMI_BAD MIMI_BAD_REG
#define REG_CKG_MIMI_BAD_S 16
#define REG_CKG_MIMI_BAD_E 0
#endif
