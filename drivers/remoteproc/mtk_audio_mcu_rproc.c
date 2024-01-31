// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <asm/barrier.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>

#include "remoteproc_internal.h"

#define REG_I0_ADR_OFFSET_0     0x48
#define REG_I0_ADR_OFFSET_1     0x4c
#define REG_D0_ADR_OFFSET_0     0x60
#define REG_D0_ADR_OFFSET_1     0x64
#define REG_WB_ADR_OFFSET_0     0xA8
#define REG_WB_ADR_OFFSET_1     0xAC
#define WB_ADR                  0xB000000D

struct mtk_audio_mcu {
	bool support_suspend;
	int irq;
	struct device *dev;
	struct rproc *rproc;
	struct clk *clk;
	void *fw_base, *intg_base, *frcpll;
	u32 phy_base, ipi_offset, ipi_bit;
	struct regmap *ipi_regmap, *ckgen_regmap;
	struct list_head clk_init_seq, pll_init_seq;
};

struct audio_mcu_clk {
	struct list_head list;
	u32 offset, mask, val;
};

#define OFFSET_INDEX	0
#define MASK_INDEX	1
#define VAL_INDEX	2

// @TODO: move the following codes to device tree
#define PLL_CONFIG_1	0x1110
#define PLL_CONFIG_2	0x53
#define PLL_CONFIG_3	0x1100
#define PLL_DELAY	100
#define REG_PD_RV55FRCPLL_MASK	0x1
#define REG_PD_RV55FRCPLL_CLK_DIV2P5_MASK	0x10
#define REG_PD_RV55FRCPLL_BIT	0
#define REG_PD_RV55FRCPLL_CLK_DIV2P5_BIT	4
#define PLL_ON	0x0
#define REG_0000_RV55FRCPLL	0x0
#define REG_0004_RV55FRCPLL	0x4
#define REG_0008_RV55FRCPLL	0x8
#define REG_000C_RV55FRCPLL	0xc
#define REG_0010_RV55FRCPLL	0x10
#define REG_0014_RV55FRCPLL	0x14
#define REG_0018_RV55FRCPLL	0x18
#define REG_GC_RV55FRCPLL_OUTPUT_DIV_MASK	0x700
#define REG_001C_RV55FRCPLL	0x1c
#define REG_0020_RV55FRCPLL	0x20
#define REG_GC_RV55FRCPLL_LOOPDIV_SECOND_MASK	0xff
#define REG_GC_RV55FRCPLL_LOOPDIV_SECOND_CONFIG	0x53
#define REG_0028_RV55FRCPLL	0x28
#define REG_0038_RV55FRCPLL	0x38

//aurv55
#define ADDR_REG_SW_EN_VIVA_XTAL_12M2XTAL_1D68	0x1c205D68
#define ADDR_REG_SW_EN_AU_MCU_BUS2AU_MCU_1B4C	0x1c207B4C
#define ADDR_REG_SW_EN_SMI2SCBE_GALS_1504		0x1c205504
#define ADDR_REG_SW_EN_RV_AUDEC2AUDEC_1D80		0x1c205d80
#define ADDR_REG_CKG_S_RV_AUDEC_102C			0x1c20502C
#define ADDR_REG_CKG_S_RV_AUSND_1034			0x1c205034
#define ADDR_REG_SW_EN_SMI2AURV55SMI_14FC		0x1c2054FC
#define ADDR_REG_SW_EN_XTAL2AUDEC_15B8			0x1c2075B8
#define ADDR_REG_SW_EN_XTAL2AUSND_15BC			0x1c2075BC
#define ADDR_REG_SW_EN_MCU_BUS2AUDEC_1B3C		0x1c207B3C
#define ADDR_REG_SW_EN_MCU_BUS2AUSND_1B40		0x1c207B40
#define ADDR_REG_CKG_S_MCU_BUS_11B8				0x1c2071B8
#define ADDR_REG_CKG_S_AU_MCU_BUS_11B8			0x1c2071BA
#define ADDR_REG_SW_EN_WB2AUDEC_15C4			0x1c2075C4
#define ADDR_REG_SW_EN_WB2AUSND_15C8			0x1c2075C8
#define ADDR_REG_SW_EN_DAP2AUDEC_1568			0x1c207568
#define ADDR_REG_SW_EN_DAP2AUSND_156C			0x1c20756C
#define ADDR_REG_CKG_S_DAP_01EC					0x1c2061EC
#define ADDR_REG_SW_EN_TCK_AUDEC2AUDEC_1D88		0x1c205D88
#define ADDR_REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC	0x1c205BAC
#define ADDR_REG_CKG_S_VIVA_AU_MCU_10E0			0x1c2070e0
#define ADDR_REG_CKG_S_VIVA_AU_MCU_10E2			0x1c2070E2
#define ADDR_REG_SW_EN_TSP2DSCRMB_1B4C			0x1c205B4C
#define ADDR_REG_SW_EN_VIVA_R2_WB2R2_WB_1CF0	0x1c205CF0
#define ADDR_REG_CKG_S_VIVA_R2_WB_0F30		0x1c204F30
#define ADDR_REG_153EF6							0x1c2A7DEC

#define REG_SW_EN_VIVA_XTAL_12M2XTAL_1D68		0x1
#define REG_SW_EN_AU_MCU_BUS2AU_MCU_1B4C		0x10
#define REG_SW_EN_SMI2SCBE_GALS_1504			0x2
#define REG_SW_EN_RV_AUDEC2AUDEC_1D80			0xf
#define REG_CKG_S_RV_AUDEC_102C					0x4
#define REG_CKG_S_RV_AUSND_1034					0x4
#define REG_SW_EN_SMI2AURV55SMI_14FC			0x7
#define REG_SW_EN_XTAL2AUDEC_15B8				0x1
#define REG_SW_EN_XTAL2AUSND_15BC				0x1
#define REG_SW_EN_MCU_BUS2AUDEC_1B3C			0x1
#define REG_SW_EN_MCU_BUS2AUSND_1B40			0x1
#define REG_CKG_S_MCU_BUS_11B8					0x4
#define REG_CKG_S_AU_MCU_BUS_11B8				0x40
#define REG_SW_EN_WB2AUDEC_15C4					0x1
#define REG_SW_EN_WB2AUSND_15C8					0x1
#define REG_SW_EN_DAP2AUDEC_1568				0x1
#define REG_SW_EN_DAP2AUSND_156C				0x1
#define REG_CKG_S_DAP_01EC						0x4
#define REG_SW_EN_TCK_AUDEC2AUDEC_1D88			0x6
#define REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC		0x1
#define REG_CKG_S_VIVA_AU_MCU_10E0				0x4
#define REG_CKG_S_VIVA_AU_MCU_10E2				0x10
#define REG_SW_EN_TSP2DSCRMB_1B4C				0x10
#define REG_SW_EN_VIVA_R2_WB2R2_WB_1CF0			0x1
#define REG_CKG_S_VIVA_R2_WB_0F30				0x4
#define REG_153EF6								0x0

//au uart
#define ADDR_REG_SW_EN_AU_UART2AU_UART			0x1c207AD0
#define ADDR_REG_AU_UART_DIG_MUX				0x1c601004
#define ADDR_UART_PADMUX						0x1c2E40C0
#define ADDR_PM_SLEEP							0x1c020224
#define REG_SW_EN_AU_UART2AU_UART				0x3
#define REG_AU_UART0_DIG_MUX					0x4f
#define REG_AU_UART1_DIG_MUX					0x9f
#define REG_UART_PADMUX_TCON					0x1100
#define REG_UART_RX								0x0c00

#define REG_SW_RSTZ_VAL  0x1
#define REG_SIZE         0x10
#define MIU_BASE_ADR     0x20000000

#define REG_BIT_MASK	0xFFFF
#define REG_BIT_OFFSET	16
#define DTSI_MTK_IPI_BIT_OFFSET	2

static bool auto_boot;
module_param(auto_boot, bool, 0444);
MODULE_PARM_DESC(auto_boot, "Enable auto boot");

static int setup_init_seq(struct device *dev, struct mtk_audio_mcu *audio_mcu,
			   struct device_node *np)
{
	int count, i, rc;
	struct device_node node;
	struct of_phandle_args clkspec;

	count = of_count_phandle_with_args(np, "audio_mcu-clk", "#audio_mcu-clk-cells");
	if (count < 0) {
		dev_warn(dev, "audio_mcu-clk not found\n");
		return count;
	}

	dev_dbg(dev, "%d clock to configure\n", count);

	for (i = 0; i < count; i++) {
		struct audio_mcu_clk *clk;

		rc = of_parse_phandle_with_args(np, "audio_mcu-clk",
				"#audio_mcu-clk-cells", i, &clkspec);
		if (rc < 0) {
			dev_err(dev, "audio_mcu-clk parse fail with err %d\n", rc);
			return rc;
		}

		dev_dbg(dev, "clk\t%d:\t%x\t%x\t%x\n", i, clkspec.args[OFFSET_INDEX],
			clkspec.args[MASK_INDEX], clkspec.args[VAL_INDEX]);

		clk = kzalloc(sizeof(struct audio_mcu_clk), GFP_KERNEL);
		if (!clk)
			return -ENOMEM;

		clk->offset = clkspec.args[OFFSET_INDEX];
		clk->mask = clkspec.args[MASK_INDEX];
		clk->val = clkspec.args[VAL_INDEX];

		list_add_tail(&clk->list, &audio_mcu->clk_init_seq);
	}

	return 0;
}

static void audio_mcu_clk_init(struct device *dev, struct mtk_audio_mcu *audio_mcu)
{
	struct audio_mcu_clk *clk;

	if (list_empty(&audio_mcu->clk_init_seq)) {
		dev_warn(dev, "clk_init_seq is not set\n");
		return;
	}

	list_for_each_entry(clk, &audio_mcu->clk_init_seq, list) {
		dev_dbg(dev, "clk.offset = %x, clk.mask = %x, clk.val = %x\n",
			 clk->offset, clk->mask, clk->val);
		regmap_update_bits_base(audio_mcu->ckgen_regmap, clk->offset, clk->mask,
					clk->val, NULL, false, true);
	}
}

void audio_mcu_pll_init(struct mtk_audio_mcu *audio_mcu)
{
	void __iomem *reg;
	unsigned int tmp;

	reg = ioremap(ADDR_REG_SW_EN_VIVA_XTAL_12M2XTAL_1D68, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_VIVA_XTAL_12M2XTAL_1D68;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102eb4             0x01
	// [0]reg_sw_en_viva_xtal_12m2xtal_1D68

	reg = ioremap(ADDR_REG_SW_EN_AU_MCU_BUS2AU_MCU_1B4C, REG_SIZE);
	tmp = readw(reg);
	tmp |= REG_SW_EN_AU_MCU_BUS2AU_MCU_1B4C;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103da6             0x10
	// [4]reg_sw_en_au_mcu_bus2au_mcu_1B4C

	reg = ioremap(ADDR_REG_SW_EN_SMI2SCBE_GALS_1504, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_SMI2SCBE_GALS_1504;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102A82             0x02
	// reg_sw_en_smi2scbe_gals_1504

	reg = ioremap(ADDR_REG_SW_EN_RV_AUDEC2AUDEC_1D80, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_RV_AUDEC2AUDEC_1D80;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102EC0             0xF
	// [0]reg_sw_en_rv_audec2audec_1D80
	// [1]reg_sw_en_rv_audec2audecdfs_1D80
	// [2]reg_sw_en_rv_ausnd2ausnd_1D80
	// [3]reg_sw_en_rv_ausnd2ausnddfs_1D80

	reg = ioremap(ADDR_REG_CKG_S_RV_AUDEC_102C, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_RV_AUDEC_102C;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102816             0x4
	// [4:2]reg_ckg_s_rv_audec_102C

	reg = ioremap(ADDR_REG_CKG_S_RV_AUSND_1034, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_RV_AUSND_1034;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x10281A             0x4
	// [4:2]reg_ckg_s_rv_ausnd_1034

	reg = ioremap(ADDR_REG_SW_EN_SMI2AURV55SMI_14FC, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_SMI2AURV55SMI_14FC;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102A7E             0x7
	// [0]reg_sw_en_smi2audec_14FC
	// [1]reg_sw_en_smi2ausnd_14FC
	// [2]reg_sw_en_smi2aurv55smi_14FC

	reg = ioremap(ADDR_REG_SW_EN_XTAL2AUDEC_15B8, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_XTAL2AUDEC_15B8;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103ADC             0x1
	// [0]reg_sw_en_xtal2audec_15B8

	reg = ioremap(ADDR_REG_SW_EN_XTAL2AUSND_15BC, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_XTAL2AUSND_15BC;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103ADE             0x1
	// [0]reg_sw_en_xtal2ausnd_15BC

	reg = ioremap(ADDR_REG_SW_EN_MCU_BUS2AUDEC_1B3C, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_MCU_BUS2AUDEC_1B3C;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103D9E             0x1
	// [0]reg_sw_en_mcu_bus2audec_1B3C

	reg = ioremap(ADDR_REG_SW_EN_MCU_BUS2AUSND_1B40, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_MCU_BUS2AUSND_1B40;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103DA0             0x1
	// [0]reg_sw_en_mcu_bus2ausnd_1B40

	reg = ioremap(ADDR_REG_CKG_S_MCU_BUS_11B8, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_MCU_BUS_11B8;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x1038DC             0x04
	// [4:2]//reg_ckg_s_mcu_bus_11B8

	reg = ioremap(ADDR_REG_CKG_S_AU_MCU_BUS_11B8, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_AU_MCU_BUS_11B8;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x1038DD             0x40
	// [14]reg_ckg_s_au_mcu_bus_11B8

	reg = ioremap(ADDR_REG_SW_EN_WB2AUDEC_15C4, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_WB2AUDEC_15C4;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103AE2             0x1
	// [0]reg_sw_en_wb2audec_15C4

	reg = ioremap(ADDR_REG_SW_EN_WB2AUSND_15C8, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_WB2AUSND_15C8;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103AE4             0x1
	// [0] reg_sw_en_wb2ausnd_15C8

	reg = ioremap(ADDR_REG_SW_EN_DAP2AUDEC_1568, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_DAP2AUDEC_1568;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103AB4             0x1
	// [0]reg_sw_en_dap2audec_1568

	reg = ioremap(ADDR_REG_SW_EN_DAP2AUSND_156C, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_DAP2AUSND_156C;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103AB6             0x1
	// [0]reg_sw_en_dap2ausnd_156C

	reg = ioremap(ADDR_REG_CKG_S_DAP_01EC, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_DAP_01EC;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x1030F6             0x4
	// [3:2]reg_ckg_s_dap_01EC

	reg = ioremap(ADDR_REG_SW_EN_TCK_AUDEC2AUDEC_1D88, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_TCK_AUDEC2AUDEC_1D88;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102EC4             0x6
	// [1]reg_sw_en_tck_audec2audec_1D88
	// [2]reg_sw_en_tck_ausnd2ausnd_1D88

	reg = ioremap(ADDR_REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102DD6             0x1
	// [0]reg_sw_en_viva_au_mcu2au_mcu_1BAC

	reg = ioremap(ADDR_REG_CKG_S_VIVA_AU_MCU_10E0, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_VIVA_AU_MCU_10E0;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103870             0x4
	// [4:2]reg_ckg_s_viva_au_mcu_10E0

	reg = ioremap(ADDR_REG_CKG_S_VIVA_AU_MCU_10E2, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_VIVA_AU_MCU_10E2;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x103871             0x10
	// [12]reg_ckg_s_viva_au_mcu_10E0

	reg = ioremap(ADDR_REG_SW_EN_TSP2DSCRMB_1B4C, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_TSP2DSCRMB_1B4C;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102DA6             0x10
	// [4]reg_sw_en_tsp2dscrmb_1B4C

	reg = ioremap(ADDR_REG_SW_EN_VIVA_R2_WB2R2_WB_1CF0, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_VIVA_R2_WB2R2_WB_1CF0;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102E78             0x1
	// [0]reg_sw_en_viva_r2_wb2r2_wb_1CF0

	reg = ioremap(ADDR_REG_CKG_S_VIVA_R2_WB_0F30, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_CKG_S_VIVA_R2_WB_0F30;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x102798             0x4
	// [2]reg_ckg_s_viva_r2_wb_0F30

	reg = ioremap(ADDR_REG_153EF6, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_153EF6;
	writew(tmp, reg);
	iounmap(reg);
	//wriu     0x153EF6             0x0
}

void audio_mcu_uart_init(struct mtk_audio_mcu *audio_mcu)
{
	unsigned int tmp;
	void __iomem *reg;

	//ck_sw_en
	//reg = ioremap(ADDR_REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC, REG_SIZE);
	//tmp = readw(reg);
	//tmp = REG_SW_EN_VIVA_AU_MCU2AU_MCU_1BAC;
	//writew(tmp, reg);
	//iounmap(reg);
	//wriu 0x0102dd6 0x01
	//reg_sw_en_viva_au_mcu2au_mcu

	reg = ioremap(ADDR_REG_SW_EN_AU_UART2AU_UART, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_SW_EN_AU_UART2AU_UART;
	writew(tmp, reg);
	iounmap(reg);
	//wriu 0x0103d68 0x01 //reg_sw_en_au_uart02au_uart0
	//wriu 0x0103d68 0x02 //reg_sw_en_au_uart12au_uart1

#if AU_UART0_DIG_MUX
	//uart dig mux
	reg = ioremap(ADDR_REG_AU_UART_DIG_MUX, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_AU_UART0_DIG_MUX;
	writew(tmp, reg);
	iounmap(reg);
	//wriu 0x0300802 0x4f //au_uart0 to dig_mux port1
#else
	//uart dig mux
	reg = ioremap(ADDR_REG_AU_UART_DIG_MUX, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_AU_UART1_DIG_MUX;
	writew(tmp, reg);
	iounmap(reg);
	//wriu 0x0300802 0x9f //au_uart1 to dig_mux port1
#endif
	//uart padmux
	reg = ioremap(ADDR_UART_PADMUX, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_UART_PADMUX_TCON;
	writew(tmp, reg);
	iounmap(reg);
	//wriu 0x0172061 0x11 //PAD_TCON1 PAD_TCON0

	reg = ioremap(ADDR_PM_SLEEP, REG_SIZE);
	tmp = readw(reg);
	tmp = REG_UART_RX;
	writew(tmp, reg);
	iounmap(reg);
}

static int audio_mcu_start(struct rproc *rproc)
{
	struct mtk_audio_mcu *audio_mcu = rproc->priv;
	u32 emi_offset, wb_offset;

	emi_offset = audio_mcu->phy_base - MIU_BASE_ADR;
	wb_offset = WB_ADR;
	dev_dbg(audio_mcu->dev, "fw mem base = %x, emi_offset = %x\n",
		audio_mcu->phy_base, emi_offset);
	writew(emi_offset & REG_BIT_MASK, audio_mcu->intg_base + REG_I0_ADR_OFFSET_0);
	writew(emi_offset >> REG_BIT_OFFSET, audio_mcu->intg_base + REG_I0_ADR_OFFSET_1);
	writew(emi_offset & REG_BIT_MASK, audio_mcu->intg_base + REG_D0_ADR_OFFSET_0);
	writew(emi_offset >> REG_BIT_OFFSET, audio_mcu->intg_base + REG_D0_ADR_OFFSET_1);
	writew(wb_offset & REG_BIT_MASK, audio_mcu->intg_base + REG_WB_ADR_OFFSET_0);
	writew(wb_offset >> REG_BIT_OFFSET, audio_mcu->intg_base + REG_WB_ADR_OFFSET_1);

	writew(REG_SW_RSTZ_VAL, audio_mcu->intg_base); //REG_SW_RSTZ
	return 0;
}

static int audio_mcu_stop(struct rproc *rproc)
{
	struct mtk_audio_mcu *audio_mcu = rproc->priv;

	/* pll disable */

	writew(0, audio_mcu->intg_base);
	return 0;
}

static void *audio_mcu_da_to_va(struct rproc *rproc, u64 da, int len)
{
	struct mtk_audio_mcu *audio_mcu = rproc->priv;

	return audio_mcu->fw_base + da;
}

/* kick a virtqueue */
static void audio_mcu_kick(struct rproc *rproc, int vqid)
{
	struct mtk_audio_mcu *audio_mcu = rproc->priv;

	regmap_update_bits_base(audio_mcu->ipi_regmap,
							audio_mcu->ipi_offset,
							BIT(audio_mcu->ipi_bit),
							BIT(audio_mcu->ipi_bit),
							NULL,
							false,
							true);
}

static const struct rproc_ops audio_mcu_ops = {
	.start		= audio_mcu_start,
	.stop		= audio_mcu_stop,
	.kick		= audio_mcu_kick,
	.da_to_va	= audio_mcu_da_to_va,
};

/**
 * handle_event() - inbound virtqueue message workqueue function
 *
 * This function is registered as a kernel thread and is scheduled by the
 * kernel handler.
 */
static irqreturn_t handle_event(int irq, void *p)
{
	struct rproc *rproc = (struct rproc *)p;

	/* Process incoming buffers on all our vrings */
	rproc_vq_interrupt(rproc, 0);
	rproc_vq_interrupt(rproc, 1);

	return IRQ_HANDLED;
}

static int mtk_audio_mcu_check_mem_region(struct device *dev, struct device_node *np)
{
	int ret;
	struct device_node *node;
	struct resource dma, fwbuf;

	/* dma mem */
	node = of_parse_phandle(np, "memory-region", 0);
	if (!node) {
		dev_err(dev, "no dma buf\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(node, 0, &dma);
	if (ret)
		return ret;

	/* firmware buffer */
	node = of_parse_phandle(np, "memory-region", 1);
	if (!node) {
		dev_err(dev, "no fw buf\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(node, 0, &fwbuf);
	if (ret)
		return ret;

	if (dma.start < fwbuf.start) {
		dev_err(dev, "invalid memory region 0:%lx 1:%lx\n",
			dma.start, fwbuf.start);
		return -EINVAL;
	}

	return 0;
}

static int mtk_audio_mcu_parse_dt(struct platform_device *pdev)
{
	const char *key;
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *node, *syscon_np;
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_audio_mcu *audio_mcu = rproc->priv;
	struct resource *res;

	audio_mcu->irq = platform_get_irq(pdev, 0);
	if (audio_mcu->irq < 0) {
		dev_err(dev, "%s no irq found\n", dev->of_node->name);
		return -ENXIO;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intg");
	audio_mcu->intg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(audio_mcu->intg_base)) {
		dev_err(dev, "no intg base\n");
		return PTR_ERR(audio_mcu->intg_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "frcpll");
	// FIXME: frcpll is shared by mulitple mrv
	// audio_mcu->frcpll = devm_ioremap_resource(dev, res);
	audio_mcu->frcpll = ioremap(res->start, resource_size(res));
	if (IS_ERR(audio_mcu->frcpll)) {
		dev_err(dev, "no frcpll\n");
		return PTR_ERR(audio_mcu->frcpll);
	}

	syscon_np = of_parse_phandle(np, "mtk,ipi", 0);
	if (!syscon_np) {
		dev_err(dev, "no mtk,ipi node\n");
		return -ENODEV;
	}

	audio_mcu->ipi_regmap = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(audio_mcu->ipi_regmap))
		return PTR_ERR(audio_mcu->ipi_regmap);

	/* TODO: Remove this after memcu clk init seq ready */
	audio_mcu->support_suspend = of_property_read_bool(np,
					"mediatek,audio_mcu-support-suspend");
	if (audio_mcu->support_suspend) {
		syscon_np = of_parse_phandle(np, "audio_mcu-clk-bank", 0);
		if (!syscon_np) {
			dev_err(dev, "no mtk,ckgen node\n");
			return -ENODEV;
		}

		audio_mcu->ckgen_regmap = syscon_node_to_regmap(syscon_np);
		if (IS_ERR(audio_mcu->ckgen_regmap))
			return PTR_ERR(audio_mcu->ckgen_regmap);

		ret = setup_init_seq(dev, audio_mcu, np);
		if (ret)
			return ret;
	}

	key = "mtk,ipi";
	ret = of_property_read_u32_index(np, key, 1, &audio_mcu->ipi_offset);
	if (ret < 0) {
		dev_err(dev, "no offset in %s\n", key);
		return -EINVAL;
	}

	ret = of_property_read_u32_index(np, key, DTSI_MTK_IPI_BIT_OFFSET, &audio_mcu->ipi_bit);
	if (ret < 0) {
		dev_err(dev, "no bit in %s\n", key);
		return -EINVAL;
	}

	/* check memory region, share dma region should be after fw mem */
	ret = mtk_audio_mcu_check_mem_region(dev, np);
	if (ret)
		return ret;

	ret = of_reserved_mem_device_init(dev);
	if (ret)
		return ret;

	node = of_parse_phandle(np, "memory-region", 1);
	if (!node) {
		dev_err(dev, "fwmem not found\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(node, 0, res);
	if (ret)
		return ret;

	dev_info(dev, "fwmem start = %lx, size = %lx\n",
		 (unsigned long)res->start, (unsigned long)resource_size(res));
	audio_mcu->fw_base = devm_ioremap_wc(dev, res->start, resource_size(res));
	if (IS_ERR(audio_mcu->fw_base)) {
		of_reserved_mem_device_release(dev);
		return PTR_ERR(audio_mcu->fw_base);
	}

	audio_mcu->phy_base = (u32)res->start;

	return 0;
}

static int __maybe_unused mtk_audio_mcu_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_audio_mcu *audio_mcu = rproc->priv;

	if (!audio_mcu->support_suspend) {
		dev_warn(dev, "device doesn't support supsend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	dev_info(dev, "shutdown audio_mcu\n");
	rproc_shutdown(rproc);
	return 0;
}

static int __maybe_unused mtk_audio_mcu_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_audio_mcu *audio_mcu = rproc->priv;
	int ret;

	if (!audio_mcu->support_suspend) {
		dev_warn(dev, "device doesn't support supsend\n");
		return 0;
	}

	/* do not handle pm ops if !auto_boot*/
	if (!rproc->auto_boot)
		return 0;

	dev_info(dev, "reinit audio_mcu clk\n");
	audio_mcu_pll_init(audio_mcu);
	audio_mcu_clk_init(dev, audio_mcu);
	udelay(PLL_DELAY);
	audio_mcu_uart_init(audio_mcu);
	udelay(PLL_DELAY);

	dev_info(dev, "boot audio_mcu\n");
	return rproc_boot(rproc);
}

static const struct dev_pm_ops mtk_audio_mcu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_audio_mcu_suspend, mtk_audio_mcu_resume)
};

static int mtk_audio_mcu_rproc_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct mtk_audio_mcu *audio_mcu;
	struct rproc *rproc;
	const char *firmware;

	ret = of_property_read_string_index(pdev->dev.of_node, "firmware-name",
					    0, &firmware);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	rproc = rproc_alloc(dev, np->name, &audio_mcu_ops,
				firmware, sizeof(*audio_mcu));
	if (!rproc) {
		dev_err(dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	/*
	 * At first we lookup the device tree setting.
	 * If there's no auto-boot prop, then check the module param.
	 */
	rproc->auto_boot = of_property_read_bool(np, "mediatek,auto-boot");
	if (!rproc->auto_boot)
		rproc->auto_boot = auto_boot;

	rproc->has_iommu = false;

	audio_mcu = (struct mtk_audio_mcu *)rproc->priv;
	audio_mcu->rproc = rproc;
	audio_mcu->dev = dev;
	INIT_LIST_HEAD(&audio_mcu->clk_init_seq);
	INIT_LIST_HEAD(&audio_mcu->pll_init_seq);

	platform_set_drvdata(pdev, rproc);
	ret = mtk_audio_mcu_parse_dt(pdev);
	if (ret) {
		dev_err(dev, "parse dt fail\n");
		return ret;
	}

	dev->dma_pfn_offset = PFN_DOWN(audio_mcu->phy_base);
	device_enable_async_suspend(dev);
	ret = devm_request_threaded_irq(dev, audio_mcu->irq, NULL,
				handle_event, IRQF_ONESHOT,
				"mtk-audio-mcu", rproc);
	if (ret) {
		dev_err(dev, "request irq @%d failed\n", audio_mcu->irq);
		return ret;
	}

	dev_info(dev, "request irq @%d for kick audio_mcu\n",  audio_mcu->irq);
	audio_mcu_pll_init(audio_mcu);
	udelay(PLL_DELAY);
	audio_mcu_uart_init(audio_mcu);
	udelay(PLL_DELAY);

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed! ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_audio_mcu_rproc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct mtk_audio_mcu *audio_mcu = rproc->priv;

	of_reserved_mem_device_release(dev);
	rproc_del(rproc);
	rproc_free(rproc);
	return 0;
}

static const struct of_device_id mtk_audio_mcu_of_match[] = {
	{ .compatible = "mediatek,audio-mcu"},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_audio_mcu_of_match);

static struct platform_driver mtk_audio_mcu_rproc = {
	.probe = mtk_audio_mcu_rproc_probe,
	.remove = mtk_audio_mcu_rproc_remove,
	.driver = {
		.name = "mtk-audio-mcu",
		.pm = &mtk_audio_mcu_pm_ops,
		.of_match_table = of_match_ptr(mtk_audio_mcu_of_match),
	},
};

module_platform_driver(mtk_audio_mcu_rproc);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Allen-HW.Hsu <allen-hw.hsu@mediatek.com>");
MODULE_DESCRIPTION("MediaTek AUDIO_MCU Remoteproce driver");
