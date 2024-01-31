/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _DT_BINDINGS_CLK_MT5873_CKGEN00_PM_H
#define _DT_BINDINGS_CLK_MT5873_CKGEN00_PM_H
#define REG_CKG_S_CM4 0x0
#define REG_CKG_S_CM4_S 2
#define REG_CKG_S_CM4_E 3
#define REG_CKG_S_CM4_CORE 0x4
#define REG_CKG_S_CM4_CORE_S 2
#define REG_CKG_S_CM4_CORE_E 3
#define REG_CKG_S_CM4_DFS 0x8
#define REG_CKG_S_CM4_DFS_S 2
#define REG_CKG_S_CM4_DFS_E 3
#define REG_CKG_S_CM4_SYSTICK 0xc
#define REG_CKG_S_CM4_SYSTICK_S 2
#define REG_CKG_S_CM4_SYSTICK_E 2
#define REG_CKG_S_CM4_TCK 0x10
#define REG_CKG_S_CM4_TCK_S 2
#define REG_CKG_S_CM4_TCK_E 2
#define REG_CKG_S_CM4_TSVALUEB 0x14
#define REG_CKG_S_CM4_TSVALUEB_S 2
#define REG_CKG_S_CM4_TSVALUEB_E 2
#define REG_CKG_S_CODEC_I2S_MCK 0x18
#define REG_CKG_S_CODEC_I2S_MCK_S 2
#define REG_CKG_S_CODEC_I2S_MCK_E 3
#define REG_CKG_S_CODEC_I2S_RX_BCK 0x1c
#define REG_CKG_S_CODEC_I2S_RX_BCK_S 2
#define REG_CKG_S_CODEC_I2S_RX_BCK_E 4
#define REG_CKG_S_CODEC_I2S_RX_MS_BCK_P 0x20
#define REG_CKG_S_CODEC_I2S_RX_MS_BCK_P_S 2
#define REG_CKG_S_CODEC_I2S_RX_MS_BCK_P_E 4
#define REG_CKG_S_DIG_MIC 0x24
#define REG_CKG_S_DIG_MIC_S 2
#define REG_CKG_S_DIG_MIC_E 3
#define REG_CKG_S_I2S_RX_BCK_DG 0x28
#define REG_CKG_S_I2S_RX_BCK_DG_S 2
#define REG_CKG_S_I2S_RX_BCK_DG_E 4
#define REG_CKG_S_NF_SYNTH_REF 0x2c
#define REG_CKG_S_NF_SYNTH_REF_S 2
#define REG_CKG_S_NF_SYNTH_REF_E 4
#define REG_CKG_S_NF_SYNTH_REF_SEL 0x2c
#define REG_CKG_S_NF_SYNTH_REF_SEL_S 10
#define REG_CKG_S_NF_SYNTH_REF_SEL_E 10
#define REG_CKG_S_PAGANINI_IMI 0x30
#define REG_CKG_S_PAGANINI_IMI_S 2
#define REG_CKG_S_PAGANINI_IMI_E 3
#define REG_CKG_S_PAGANINI_IMI_DFS 0x34
#define REG_CKG_S_PAGANINI_IMI_DFS_S 2
#define REG_CKG_S_PAGANINI_IMI_DFS_E 3
#define REG_CKG_S_PLL2PAGADIV 0x38
#define REG_CKG_S_PLL2PAGADIV_S 2
#define REG_CKG_S_PLL2PAGADIV_E 3
#define REG_CKG_S_SRC_A1_256FSI 0x3c
#define REG_CKG_S_SRC_A1_256FSI_S 2
#define REG_CKG_S_SRC_A1_256FSI_E 3
#define REG_CKG_S_PAGA_PLL_SEL 0x40
#define REG_CKG_S_PAGA_PLL_SEL_S 2
#define REG_CKG_S_PAGA_PLL_SEL_E 4
#define REG_CKG_S_PAGA_PLL_SEL2 0x40
#define REG_CKG_S_PAGA_PLL_SEL2_S 10
#define REG_CKG_S_PAGA_PLL_SEL2_E 12
#define REG_CKG_S_VREC_MAC 0x44
#define REG_CKG_S_VREC_MAC_S 2
#define REG_CKG_S_VREC_MAC_E 4
#define REG_CKG_S_PAGANINI_IMI_DIV2 0x4c
#define REG_CKG_S_PAGANINI_IMI_DIV2_S 2
#define REG_CKG_S_PAGANINI_IMI_DIV2_E 2
#define REG_CKG_S_BIST_PM 0x60
#define REG_CKG_S_BIST_PM_S 2
#define REG_CKG_S_BIST_PM_E 4
#define REG_CKG_S_CEC 0x70
#define REG_CKG_S_CEC_S 2
#define REG_CKG_S_CEC_E 3
#define REG_CKG_S_CEC1 0x74
#define REG_CKG_S_CEC1_S 2
#define REG_CKG_S_CEC1_E 3
#define REG_CKG_S_CEC2 0x78
#define REG_CKG_S_CEC2_S 2
#define REG_CKG_S_CEC2_E 3
#define REG_CKG_S_CEC3 0x7c
#define REG_CKG_S_CEC3_S 2
#define REG_CKG_S_CEC3_E 3
#define REG_CKG_S_DDC 0x80
#define REG_CKG_S_DDC_S 2
#define REG_CKG_S_DDC_E 4
#define REG_CKG_S_DVI_RAW0 0x84
#define REG_CKG_S_DVI_RAW0_S 2
#define REG_CKG_S_DVI_RAW0_E 2
#define REG_CKG_S_DVI_RAW1 0x88
#define REG_CKG_S_DVI_RAW1_S 2
#define REG_CKG_S_DVI_RAW1_E 2
#define REG_CKG_S_DVI_RAW2 0x8c
#define REG_CKG_S_DVI_RAW2_S 2
#define REG_CKG_S_DVI_RAW2_E 2
#define REG_CKG_S_DVI_RAW3 0x90
#define REG_CKG_S_DVI_RAW3_S 2
#define REG_CKG_S_DVI_RAW3_E 2
#define REG_CKG_S_HOTPLUG 0x94
#define REG_CKG_S_HOTPLUG_S 2
#define REG_CKG_S_HOTPLUG_E 3
#define REG_CKG_S_XTAL_PADOUT 0xa8
#define REG_CKG_S_XTAL_PADOUT_S 2
#define REG_CKG_S_XTAL_PADOUT_E 2
#define REG_CKG_S_RTC_PADOUT 0xac
#define REG_CKG_S_RTC_PADOUT_S 2
#define REG_CKG_S_RTC_PADOUT_E 3
#define REG_CKG_S_SD 0xc8
#define REG_CKG_S_SD_S 2
#define REG_CKG_S_SD_E 4
#define REG_CKG_S_GMAC_RX 0xd4
#define REG_CKG_S_GMAC_RX_S 2
#define REG_CKG_S_GMAC_RX_E 3
#define REG_CKG_S_GMAC_TX 0xd8
#define REG_CKG_S_GMAC_TX_S 2
#define REG_CKG_S_GMAC_TX_E 3
#define REG_CKG_S_MCU_BUS_PM 0xe8
#define REG_CKG_S_MCU_BUS_PM_S 2
#define REG_CKG_S_MCU_BUS_PM_E 4
#define REG_CKG_S_MCU_PM 0xec
#define REG_CKG_S_MCU_PM_S 2
#define REG_CKG_S_MCU_PM_E 4
#define REG_CKG_S_MIIC0_PM 0xf0
#define REG_CKG_S_MIIC0_PM_S 2
#define REG_CKG_S_MIIC0_PM_E 3
#define REG_CKG_S_MIIC1_PM 0xf4
#define REG_CKG_S_MIIC1_PM_S 2
#define REG_CKG_S_MIIC1_PM_E 3
#define REG_CKG_S_SPI_PM 0xf8
#define REG_CKG_S_SPI_PM_S 2
#define REG_CKG_S_SPI_PM_E 4
#define REG_CKG_S_UART0_PM 0xfc
#define REG_CKG_S_UART0_PM_S 2
#define REG_CKG_S_UART0_PM_E 3
#define REG_CKG_S_UART0_PM_SYNTH 0x100
#define REG_CKG_S_UART0_PM_SYNTH_S 2
#define REG_CKG_S_UART0_PM_SYNTH_E 3
#define REG_CKG_S_UART1_PM 0x104
#define REG_CKG_S_UART1_PM_S 2
#define REG_CKG_S_UART1_PM_E 3
#define REG_CKG_S_UART1_PM_SYNTH 0x108
#define REG_CKG_S_UART1_PM_SYNTH_S 2
#define REG_CKG_S_UART1_PM_SYNTH_E 3
#define REG_CKG_S_FUART0_PM 0x10c
#define REG_CKG_S_FUART0_PM_S 2
#define REG_CKG_S_FUART0_PM_E 3
#define REG_CKG_S_FUART0_PM_SYNTH 0x110
#define REG_CKG_S_FUART0_PM_SYNTH_S 2
#define REG_CKG_S_FUART0_PM_SYNTH_E 3
#define REG_CKG_S_XIU2SRAM 0x114
#define REG_CKG_S_XIU2SRAM_S 2
#define REG_CKG_S_XIU2SRAM_E 2
#define REG_CKG_S_IR 0x148
#define REG_CKG_S_IR_S 2
#define REG_CKG_S_IR_E 4
#define REG_CKG_S_IR_TX0_PM 0x14c
#define REG_CKG_S_IR_TX0_PM_S 2
#define REG_CKG_S_IR_TX0_PM_E 4
#define REG_CKG_S_KREF 0x158
#define REG_CKG_S_KREF_S 2
#define REG_CKG_S_KREF_E 3
#define REG_CKG_S_RTC 0x160
#define REG_CKG_S_RTC_S 2
#define REG_CKG_S_RTC_E 4
#define REG_CKG_S_SAR 0x164
#define REG_CKG_S_SAR_S 2
#define REG_CKG_S_SAR_E 4
#define REG_CKG_S_PM_SLEEP 0x168
#define REG_CKG_S_PM_SLEEP_S 2
#define REG_CKG_S_PM_SLEEP_E 4
#define REG_CKG_S_PWM_PM 0x184
#define REG_CKG_S_PWM_PM_S 2
#define REG_CKG_S_PWM_PM_E 2
#define REG_CKG_S_PM_SSUSB_FRMCNT_CTL 0x194
#define REG_CKG_S_PM_SSUSB_FRMCNT_CTL_S 2
#define REG_CKG_S_PM_SSUSB_FRMCNT_CTL_E 2
#define REG_CKG_S_PM_SSUSB_PCLK_CTL 0x198
#define REG_CKG_S_PM_SSUSB_PCLK_CTL_S 2
#define REG_CKG_S_PM_SSUSB_PCLK_CTL_E 2
#define REG_CKG_S_PM_SSUSB_SCL_CTL 0x19c
#define REG_CKG_S_PM_SSUSB_SCL_CTL_S 2
#define REG_CKG_S_PM_SSUSB_SCL_CTL_E 2
#define REG_CKG_S_PM_SSUSB_SDIN_CTL 0x1a0
#define REG_CKG_S_PM_SSUSB_SDIN_CTL_S 2
#define REG_CKG_S_PM_SSUSB_SDIN_CTL_E 2
#define REG_CKG_S_PM_SSUSB_UTMI_CTL 0x1a8
#define REG_CKG_S_PM_SSUSB_UTMI_CTL_S 2
#define REG_CKG_S_PM_SSUSB_UTMI_CTL_E 2
#define REG_CKG_S_PM_PEXTP 0x1b0
#define REG_CKG_S_PM_PEXTP_S 2
#define REG_CKG_S_PM_PEXTP_E 3
#define REG_PM_TEST_ENABLE 0x1c0
#define REG_PM_TEST_ENABLE_S 0
#define REG_PM_TEST_ENABLE_E 0
#define REG_PM_TEST_DIV_SEL 0x1c0
#define REG_PM_TEST_DIV_SEL_S 4
#define REG_PM_TEST_DIV_SEL_E 6
#define REG_PM_TEST_SRC_SEL 0x1c0
#define REG_PM_TEST_SRC_SEL_S 8
#define REG_PM_TEST_SRC_SEL_E 15
#define REG_PM_USB_PCIE_TEST_ENABLE 0x1c4
#define REG_PM_USB_PCIE_TEST_ENABLE_S 0
#define REG_PM_USB_PCIE_TEST_ENABLE_E 0
#define REG_PM_USB_PCIE_TEST_DIV_SEL 0x1c4
#define REG_PM_USB_PCIE_TEST_DIV_SEL_S 4
#define REG_PM_USB_PCIE_TEST_DIV_SEL_E 6
#define REG_PM_USB_PCIE_TEST_SRC_SEL 0x1c4
#define REG_PM_USB_PCIE_TEST_SRC_SEL_S 8
#define REG_PM_USB_PCIE_TEST_SRC_SEL_E 15
#define REG_PM_ETH_TEST_ENABLE 0x1c8
#define REG_PM_ETH_TEST_ENABLE_S 0
#define REG_PM_ETH_TEST_ENABLE_E 0
#define REG_PM_ETH_TEST_DIV_SEL 0x1c8
#define REG_PM_ETH_TEST_DIV_SEL_S 4
#define REG_PM_ETH_TEST_DIV_SEL_E 6
#define REG_PM_ETH_TEST_SRC_SEL 0x1c8
#define REG_PM_ETH_TEST_SRC_SEL_S 8
#define REG_PM_ETH_TEST_SRC_SEL_E 15
#define REG_PM_EPHY_TEST_ENABLE 0x1cc
#define REG_PM_EPHY_TEST_ENABLE_S 0
#define REG_PM_EPHY_TEST_ENABLE_E 0
#define REG_PM_EPHY_TEST_DIV_SEL 0x1cc
#define REG_PM_EPHY_TEST_DIV_SEL_S 4
#define REG_PM_EPHY_TEST_DIV_SEL_E 6
#define REG_PM_EPHY_TEST_SRC_SEL 0x1cc
#define REG_PM_EPHY_TEST_SRC_SEL_S 8
#define REG_PM_EPHY_TEST_SRC_SEL_E 15
#define REG_PAGANINI_TEST_ENABLE 0x1d0
#define REG_PAGANINI_TEST_ENABLE_S 0
#define REG_PAGANINI_TEST_ENABLE_E 0
#define REG_PAGANINI_TEST_DIV_SEL 0x1d0
#define REG_PAGANINI_TEST_DIV_SEL_S 4
#define REG_PAGANINI_TEST_DIV_SEL_E 6
#define REG_PAGANINI_TEST_SRC_SEL 0x1d0
#define REG_PAGANINI_TEST_SRC_SEL_S 8
#define REG_PAGANINI_TEST_SRC_SEL_E 15
#define REG_CKGEN00_PM_DUMMY0 0x1f8
#define REG_CKGEN00_PM_DUMMY0_S 0
#define REG_CKGEN00_PM_DUMMY0_E 15
#define REG_CKGEN00_PM_DUMMY1 0x1fc
#define REG_CKGEN00_PM_DUMMY1_S 0
#define REG_CKGEN00_PM_DUMMY1_E 15
#endif
