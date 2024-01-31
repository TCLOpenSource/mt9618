// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author Owen.Tseng <Owen.Tseng@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/version.h>
#include <linux/of_address.h>
#include "spi-mt5896-coda.h"
#include "spi-mt5896-riu.h"
#include <soc/mediatek/mtk-bdma.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>

#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
/* SPI register offsets */

#define MSPI_ENABLE_BIT					BIT(0)
#define MSPI_RESET_BIT					BIT(1)
#define MSPI_ENABLE_INT_BIT				BIT(2)
#define MSPI_3WARE_MODE_BIT				BIT(4)
#define MSPI_CPHA_BIT					BIT(6)
#define MSPI_CPOL_BIT					BIT(7)
#define MSPI_LSB_FIRST_BIT				BIT(0)
#define MSPI_TRIGGER_BIT				BIT(0)
#define MSPI_CLEAR_DONE_FLAG_BIT		BIT(0)
#define MSPI_CHIP_SELECT_BIT			BIT(0)
#define WB16_INDEX						0x4
#define WB8_INDEX						0x3
#define MAX_TX_BUF_SIZE					0x20
#define MSPI_ENABLE_DUAL				BIT(0)
#define MSPI_DMA_TX_EN					BIT(0)
#define MSPI_DMA_RSTZ					BIT(4)
#define MSPI_DMA_TX_NBYTE2NBYTE			BIT(8)
#define MSPI_DMA_CS_SW					BIT(2)
#define MSPI_DMA_RX_EN					BIT(1)

#define DTV_SPI_TIMEOUT_MS				1000
#define DTV_SPI_MODE_BITS	(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_NO_CS | SPI_LSB_FIRST)

#define DRV_NAME	"mspi-dtv"
#define REG_SW_EN_MSPI_4				0x1C1C
#define REG_SW_EN_MSPI_CILINK			0x1C04
#define REG_SW_EN_MSPI_0				0x1C14
#define REG_SW_EN_MSPI_1				0x1BE4
#define REG_MSPI_CLK_SRC_MUX			0x1240
#define REG_MSPI_CLK_SRC				0x1248
#define REG_CI_CLK_SRC_MUX				0x1230
#define REG_CI_CLK_SRC					0x1238
#define MSPI_TO_MCU_SW_EN				0x1
#define MAX_CLK							24000000
#define MSPI_CI_CHANEL					8
#define MSPI_CH_CLK_OFFSET				0x10
#define MCU_CLK_108M_SEL				0x08
#define MCU_CLK_SEL						0x04
#define TR_BUF_IDX						4
#define TR_BUF_EXT_IDX					14
#define SPI_OFFSET_IDX					4
#define SPI_STRING_SIZE					8
#define SPI_BIT_LEN_FULL				8
#define SPI_BIT_MUX_LEN					3
#define MAX_SPI_HW_NUM					10
#define SPI_CTL_H						8
#define SPI_CTL_MASK					(0xFF << SPI_CTL_H)
#define CLK_OFFSET_FIEL					4
#define CLK_DIV_MASK					0xF
#define CLK_MUX_OFFSET					0x10
#define CLK_DIV_SHIFT_MAX				8
#define CLK_SRC_30M						30000000
#define CLK_SRC_48M						48000000
#define CLK_SRC_62M						62000000
#define CLK_SRC_72M						72000000
#define CLK_SRC_86M						86000000
#define CLK_SRC_108M					108000000
#define CLK_SRC_123M					123000000
#define CLK_SRC_172M					172000000
#define MCU_CLK_SEL_MASK				0x1F
#define MCU_CLK_30M_SEL					0x1C
#define MCU_CLK_48M_SEL					0x18
#define MCU_CLK_62M_SEL					0x14
#define MCU_CLK_72M_SEL					0x10
#define MCU_CLK_86M_SEL					0x0C
#define MCU_CLK_108M_SEL				0x08
#define MCU_CLK_123M_SEL				0x04
#define MCU_CLK_172M_SEL				0x00
#define CLK_DIV_IDX						2
#define LSB_SHIFT						16
#define NONPM_RIU_EN_IDX				3
#define RIU_RW_EN_REG					0x80
#define LNB_BYTE_MSK					0xFF
#define PA_OFFSET						0x20000000
#define COHERENT_SIZE					0x1000
#define MSPI_FRC_CHANEL					9
#define REG_MSPI_FRC_SW_EN				0x1AE0
#define MSPI_FRC_SW_EN					(BIT(0)|BIT(1))
#define STRING_SIZE						16
#define BDMA_SEL						2
#define XFER_MAX_NUM					32
#define SPI_BIT_MUX_IDX0				0
#define SPI_BIT_MUX_IDX1				1
#define SPI_BIT_MUX_IDX2				2
#define SPI_BIT_MUX_IDX3				3
#define WORD_MASK_VALUE					0xFFFF
#define XFER_BUF_IDX					6
#define XFER_BUF_EXT_IDX				2
#define SPI_BIT_MUX_BITS				3
#define MSPI_DUAL_PAD_SEL				BIT(4)
#define XTAL_HZ						24000000
#define SPI_MAX_BUF_SIZE				4096
#define SPI_DMA_MASK					32
#define REG_MSPIFRC_CLK_SRC_MUX			0x12C0
#define REG_MSPIFRC_CLK_SRC				0x12D0
#define LDM_SPI_CH					2
#define STRLEN						32
#define SIGGLETODUAL					1
#define DUALTOSINGLE					2
#define HIGHBYTE_MSK					0xFF00
#define MSPI_XTAL_RSTZ					BIT(2)
#define DMA_RESET_3T					3

struct dtv_spi {
	void __iomem *regs;
	void __iomem *clkgen_bank_1;
	void __iomem *clkgen_bank_2;
	void __iomem *clkgen_bank_3;
	bool full_duplex;
	u32 mspi_channel;
	int irq;
	struct completion done;
	const u8 *tx_buf;
	u8 *rx_buf;
	unsigned int len;
	unsigned int current_trans_len;
	unsigned int data_xfer_cfg;
	unsigned int xfer_interval;
	unsigned int xfer_trig_d;
	unsigned int xfer_done_d;
	unsigned int xfer_turn_d;
	unsigned int clk_div;
	unsigned int clk_sel;
	struct spi_device *slave_dev;
	unsigned int dma_sel;
	bool fifo_dma_switch;
	dma_addr_t phyaddr;
	char bdma_ch[STRING_SIZE];
	u32 speed;
	u8	xfer_bits[XFER_MAX_NUM];
	struct device *dev;
	bool dma_alloc;
	bool dma_nByte2nByte;
	bool sw_bits_per_word;
	unsigned int spi_dedicated;
	struct mtk_bdma_channel *bdma_dev;
	struct gpio_desc *cs_alternative;
	bool fnc_callback;
	ktime_t start_time_t;
	u32 event2irq_t;
	u8 dual2single;
	u32 irq2rcv_t;
	u32 dbg_speed;
};

static struct spi_device *_ptr_array[MAX_SPI_HW_NUM];

static inline u16 dtvspi_rd(struct dtv_spi *bs, u32 reg)
{
	return mtk_readw_relaxed((bs->regs + reg));
}

static inline u8 dtvspi_rdl(struct dtv_spi *bs, u16 reg)
{
	return dtvspi_rd(bs, reg)&0xff;
}
static inline void dtvspi_wr(struct dtv_spi *bs, u16 reg, u32 val)
{
	mtk_writew_relaxed((bs->regs + reg), val);
}

static inline void dtvspi_wrl(struct dtv_spi *bs, u16 reg, u8 val)
{
	u16 val16 = dtvspi_rd(bs, reg)&0xff00;

	val16 |= val;
	dtvspi_wr(bs, reg, val16);
}

static inline void dtvspi_wrh(struct dtv_spi *bs, u16 reg, u8 val)
{
	u16 val16 = dtvspi_rd(bs, reg)&0xff;

	val16 |= ((u16)val) << 8;
	dtvspi_wr(bs, reg, val16);
}

static const u16 mspi_txfifoaddr[] = {
	SPI_REG_0100_MSPI0,
	SPI_REG_0104_MSPI0,
	SPI_REG_0108_MSPI0,
	SPI_REG_010C_MSPI0,
	SPI_REG_0000_MSPI0,
	SPI_REG_0004_MSPI0,
	SPI_REG_0008_MSPI0,
	SPI_REG_000C_MSPI0,
	SPI_REG_0010_MSPI0,
	SPI_REG_0014_MSPI0,
	SPI_REG_0018_MSPI0,
	SPI_REG_001C_MSPI0,
	SPI_REG_0020_MSPI0,
	SPI_REG_0024_MSPI0,
	SPI_REG_0028_MSPI0,
	SPI_REG_002C_MSPI0,
};

static const u16 mspi_rxfifoaddr_halfdeplex[] = {
	SPI_REG_0110_MSPI0,
	SPI_REG_0114_MSPI0,
	SPI_REG_0118_MSPI0,
	SPI_REG_011C_MSPI0,
	SPI_REG_0040_MSPI0,
	SPI_REG_0044_MSPI0,
	SPI_REG_0048_MSPI0,
	SPI_REG_004C_MSPI0,
	SPI_REG_0050_MSPI0,
	SPI_REG_0054_MSPI0,
	SPI_REG_0058_MSPI0,
	SPI_REG_005C_MSPI0,
	SPI_REG_0060_MSPI0,
	SPI_REG_0064_MSPI0,
	SPI_REG_0068_MSPI0,
	SPI_REG_006C_MSPI0,
};
static const u16 mspi_rxfifoaddr[] = {
	SPI_REG_01E0_MSPI0,
	SPI_REG_01E4_MSPI0,
	SPI_REG_01E8_MSPI0,
	SPI_REG_01EC_MSPI0,
	SPI_REG_01F0_MSPI0,
	SPI_REG_01F4_MSPI0,
	SPI_REG_01F8_MSPI0,
	SPI_REG_01FC_MSPI0,
};

static const u16 mspi_wd_bit_len_sel[] = {
	SPI_REG_0130_MSPI0,
	SPI_REG_0134_MSPI0,
	SPI_REG_0080_MSPI0,
	SPI_REG_0084_MSPI0,
	SPI_REG_0088_MSPI0,
	SPI_REG_008C_MSPI0,
	SPI_REG_0090_MSPI0,
	SPI_REG_0094_MSPI0,
};

static const u32 mspi_clk_src[] = {
	CLK_SRC_30M,
	CLK_SRC_48M,
	CLK_SRC_62M,
	CLK_SRC_72M,
	CLK_SRC_86M,
	CLK_SRC_108M,
	CLK_SRC_123M,
	CLK_SRC_172M,
};

static const u8 mspi_clk_sel[] = {
	MCU_CLK_30M_SEL,
	MCU_CLK_48M_SEL,
	MCU_CLK_62M_SEL,
	MCU_CLK_72M_SEL,
	MCU_CLK_86M_SEL,
	MCU_CLK_108M_SEL,
	MCU_CLK_123M_SEL,
	MCU_CLK_172M_SEL,
};


static void dtvspi_hw_set_clock(struct dtv_spi *bs,
			struct spi_device *spi, struct spi_transfer *tfr)
{
	u8 val;
	u32 clk_tmp;
	u32 clk_src;
	u32 addr_tmp;

	if (bs->mspi_channel < MSPI_CI_CHANEL) {
		val = bs->mspi_channel;
		addr_tmp = (REG_MSPI_CLK_SRC_MUX + (CLK_MUX_OFFSET*val));
		val = mtk_readb(bs->clkgen_bank_1 + addr_tmp);
		val |= MCU_CLK_SEL;
		mtk_writeb(bs->clkgen_bank_1 + addr_tmp, val);
		val = bs->mspi_channel;
		addr_tmp = (REG_MSPI_CLK_SRC + (CLK_MUX_OFFSET*val));
		val = mtk_readb(bs->clkgen_bank_1 + addr_tmp);
		val &= (u8)(~MCU_CLK_SEL_MASK);
		val |= mspi_clk_sel[bs->clk_sel];
		mtk_writeb(bs->clkgen_bank_1 + addr_tmp, val);
		clk_src = (mspi_clk_src[bs->clk_sel] / ((bs->clk_div) + 1));
		for (val = 0; val < CLK_DIV_SHIFT_MAX; val++) {
			clk_tmp = clk_src >> (val + 1);
			if (bs->dbg_speed) {
				if (clk_tmp <= bs->dbg_speed)
					break;
			} else {
				if (clk_tmp <= tfr->speed_hz)
					break;
			}
		}
		clk_tmp = dtvspi_rd(bs, SPI_REG_0124_MSPI0);
		clk_tmp &= ~HIGHBYTE_MSK;
		clk_tmp |= (val << SPI_CTL_H);
		dtvspi_wr(bs, SPI_REG_0124_MSPI0, clk_tmp);
	}
	clk_tmp = mtk_readw_relaxed(bs->clkgen_bank_2);
	if (bs->mspi_channel == 0) {
		val = mtk_readb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_0);
		val |= MSPI_TO_MCU_SW_EN;
		mtk_writeb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_0, val);
		mtk_writeb(bs->clkgen_bank_2, bs->clk_div);
	} else if (bs->mspi_channel < MSPI_CI_CHANEL) {
		val = bs->mspi_channel;
		addr_tmp = (REG_SW_EN_MSPI_1 + (CLK_OFFSET_FIEL*(val - 1)));
		val = mtk_readb(bs->clkgen_bank_1 + addr_tmp);
		val |= MSPI_TO_MCU_SW_EN;
		mtk_writeb(bs->clkgen_bank_1 + addr_tmp, val);
		val = mtk_readb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_4);
		val |= MSPI_TO_MCU_SW_EN;
		mtk_writeb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_4, val);
		val = bs->mspi_channel;
		if (val >= CLK_OFFSET_FIEL)
			val -= CLK_OFFSET_FIEL;
		clk_tmp &= (u32)(~(CLK_DIV_MASK << (CLK_OFFSET_FIEL*val)));
		clk_tmp |= ((bs->clk_div) << (CLK_OFFSET_FIEL*val));
		mtk_writew_relaxed(bs->clkgen_bank_2, clk_tmp);
	} else if (bs->mspi_channel == MSPI_FRC_CHANEL) {
		val = mtk_readb(bs->clkgen_bank_1 + REG_MSPI_FRC_SW_EN);
		val |= MSPI_FRC_SW_EN;
		mtk_writeb(bs->clkgen_bank_1 + REG_MSPI_FRC_SW_EN, val);
		val = mtk_readb(bs->clkgen_bank_1 + REG_MSPIFRC_CLK_SRC_MUX);
		val |= MCU_CLK_SEL;
		mtk_writeb(bs->clkgen_bank_1 + REG_MSPIFRC_CLK_SRC_MUX, val);
		val = mtk_readb(bs->clkgen_bank_1 + REG_MSPIFRC_CLK_SRC);
		val |= mspi_clk_sel[bs->clk_sel];
		mtk_writeb(bs->clkgen_bank_1 + REG_MSPIFRC_CLK_SRC, val);
		clk_src = (mspi_clk_src[bs->clk_sel] / ((bs->clk_div) + 1));
		for (val = 0; val < CLK_DIV_SHIFT_MAX; val++) {
			clk_tmp = clk_src >> (val + 1);
			if (clk_tmp <= tfr->speed_hz)
				break;
		}
		clk_tmp = dtvspi_rd(bs, SPI_REG_0124_MSPI0);
		clk_tmp &= (u32)(~SPI_CTL_MASK);
		clk_tmp |= (val << SPI_CTL_H);
		dtvspi_wr(bs, SPI_REG_0124_MSPI0, clk_tmp);
		mtk_writeb(bs->clkgen_bank_2, bs->clk_div);
	} else {
		val = mtk_readb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_CILINK);
		val |= MSPI_TO_MCU_SW_EN;
		mtk_writeb(bs->clkgen_bank_1 + REG_SW_EN_MSPI_CILINK, val);
		val = mtk_readb(bs->clkgen_bank_1 + REG_CI_CLK_SRC_MUX);
		val &= ~0xF;
		mtk_writeb(bs->clkgen_bank_1 + REG_CI_CLK_SRC_MUX, val);
	}

	if (tfr->speed_hz >= MAX_CLK) {
		if (bs->mspi_channel == MSPI_CI_CHANEL) {
			val = mtk_readb(bs->clkgen_bank_1 + REG_CI_CLK_SRC_MUX);
			val |= MCU_CLK_SEL;
			mtk_writeb(bs->clkgen_bank_1 + REG_CI_CLK_SRC_MUX, val);
			val = mtk_readb(bs->clkgen_bank_1 + REG_CI_CLK_SRC);
			val &= (u8)(~MCU_CLK_SEL_MASK);
			val |= mspi_clk_sel[bs->clk_sel];
			mtk_writeb(bs->clkgen_bank_1 + REG_CI_CLK_SRC, val);
		}
	}

}

static void dtvspi_hw_enable_interrupt(struct dtv_spi *bs, bool enable)
{
	u8 val = dtvspi_rdl(bs, SPI_REG_0124_MSPI0);

	if (enable)
		val |= MSPI_ENABLE_INT_BIT;
	else
		val &= ~MSPI_ENABLE_INT_BIT;

	dtvspi_wrl(bs, SPI_REG_0124_MSPI0, val);
}

static void dtvspi_hw_set_bit_len_sel(struct dtv_spi *bs, struct spi_transfer *tfr)
{
	u16 i, j;
	u16 bit_len_num = ((u16)tfr->bits_per_word + SPI_BIT_LEN_FULL - 1) / SPI_BIT_LEN_FULL;
	u16 wd_num = (bit_len_num + SPI_OFFSET_IDX - 1) / SPI_OFFSET_IDX;

	for (i = 0; i < wd_num; i++) {
		u16 reg = 0;

		for (j = 0; j < SPI_OFFSET_IDX; j++) {
			// index 3, 2, 1, 0 for shift
			u16 index = i * SPI_OFFSET_IDX + (SPI_OFFSET_IDX - j - 1);

			reg <<= SPI_BIT_MUX_BITS;
			if (bs->dma_sel)
				reg |= (bs->xfer_bits[index] - 1); // dma mode use tr_mux table
			else {
				if ((index == bit_len_num - 1) &&
					(tfr->bits_per_word % SPI_BIT_LEN_FULL))
					reg |= (tfr->bits_per_word % SPI_BIT_LEN_FULL - 1);
				else
					reg |= (SPI_BIT_LEN_FULL - 1);
			}
		}
		dtvspi_wr(bs, mspi_wd_bit_len_sel[i], reg);
	}
}

static void dtvspi_hw_set_cus(struct dtv_spi *bs)
{
	u32 bits = 0;
	u32 msg_interval = 0;
	u32 addr_tmp = 0;
	u16 i = 0;
	u16 reg[XFER_MAX_NUM / SPI_OFFSET_IDX] = {0};

	/* generate register value mask */
	for (i = 0; i < XFER_MAX_NUM; i += SPI_OFFSET_IDX) {

		bits = bs->xfer_bits[i + SPI_BIT_MUX_IDX0] - 1;
		bits |= (bs->xfer_bits[i + SPI_BIT_MUX_IDX1] - 1)
			<< (SPI_BIT_MUX_IDX1 * SPI_BIT_MUX_BITS);
		bits |= (bs->xfer_bits[i + SPI_BIT_MUX_IDX2] - 1)
			<< (SPI_BIT_MUX_IDX2 * SPI_BIT_MUX_BITS);
		bits |= (bs->xfer_bits[i + SPI_BIT_MUX_IDX3] - 1)
			<< (SPI_BIT_MUX_IDX3 * SPI_BIT_MUX_BITS);

		reg[i / SPI_OFFSET_IDX] = bits & WORD_MASK_VALUE;
	}

	/* write registers */
	/* word 0~7 of write */
	dtvspi_wr(bs, SPI_REG_0130_MSPI0, reg[0]);
	dtvspi_wr(bs, SPI_REG_0134_MSPI0, reg[1]);
	/* word 0~7 of read */
	dtvspi_wr(bs, SPI_REG_0138_MSPI0, reg[0]);
	dtvspi_wr(bs, SPI_REG_013C_MSPI0, reg[1]);

	/* word 8~31 of write */
	addr_tmp = SPI_REG_0080_MSPI0;
	for (i = 0; i < XFER_BUF_IDX; i++) {
		dtvspi_wr(bs, addr_tmp, reg[i + XFER_BUF_EXT_IDX]);
		addr_tmp += SPI_OFFSET_IDX;
	}
	/* word 8~31 of read */
	addr_tmp = SPI_REG_00A0_MSPI0;
	for (i = 0; i < XFER_BUF_IDX; i++) {
		dtvspi_wr(bs, addr_tmp, reg[i + XFER_BUF_EXT_IDX]);
		addr_tmp += SPI_OFFSET_IDX;
	}
	if ((bs->xfer_interval) || (bs->xfer_turn_d)) {
		msg_interval = bs->xfer_interval | (bs->xfer_turn_d << SPI_CTL_H);
		dtvspi_wr(bs, SPI_REG_012C_MSPI0, msg_interval);
	}
	if ((bs->xfer_trig_d) || (bs->xfer_done_d)) {
		msg_interval = bs->xfer_trig_d | (bs->xfer_done_d << SPI_CTL_H);
		dtvspi_wr(bs, SPI_REG_0128_MSPI0, msg_interval);
	}
}

static void dtvspi_hw_set_mode(struct dtv_spi *bs, struct spi_device *spi,
								struct spi_transfer *tfr)
{
	u8 val = dtvspi_rdl(bs, SPI_REG_0124_MSPI0);

	if (spi->mode&SPI_CPOL)
		val |= MSPI_CPOL_BIT;
	else
		val &= ~MSPI_CPOL_BIT;

	if (spi->mode&SPI_CPHA)
		val |= MSPI_CPHA_BIT;
	else
		val &= ~MSPI_CPHA_BIT;

	dtvspi_wrl(bs, SPI_REG_0124_MSPI0, val);
	val = dtvspi_rdl(bs, SPI_REG_0140_MSPI0);
	if (spi->mode&SPI_LSB_FIRST)
		val |= MSPI_LSB_FIRST_BIT;
	else
		val &= ~MSPI_LSB_FIRST_BIT;

	dtvspi_wrl(bs, SPI_REG_0140_MSPI0, val);

	val = dtvspi_rdl(bs, SPI_REG_00E0_MSPI0);
	if ((tfr->rx_nbits == SPI_NBITS_DUAL) || (tfr->tx_nbits == SPI_NBITS_DUAL)) {
		if (bs->dual2single == 0) {
		val |= MSPI_ENABLE_DUAL;
		val |= MSPI_DUAL_PAD_SEL;
		}
	} else {
		if (bs->dual2single == 0) {
		val &= ~MSPI_ENABLE_DUAL;
		val &= ~MSPI_DUAL_PAD_SEL;
	}
	}

	dtvspi_wrl(bs, SPI_REG_00E0_MSPI0, val);

}
static inline void dtvspi_hw_chip_select(struct dtv_spi *bs,
					struct spi_device *spi, bool enable)
{
	u8 val;

	if (spi->mode&SPI_NO_CS) {
		gpiod_set_value(bs->cs_alternative, !enable);
		return;
	}

	val = dtvspi_rdl(bs, SPI_REG_017C_MSPI0);
	if (enable != (!!(spi->mode&SPI_CS_HIGH)))
		val &= ~MSPI_CHIP_SELECT_BIT;
	else
		val |= MSPI_CHIP_SELECT_BIT;

	dtvspi_wrl(bs, SPI_REG_017C_MSPI0, val);
}
static inline void dtvspi_hw_clear_done(struct dtv_spi *bs)
{
	//dtvspi_wrl(bs, SPI_REG_0170_MSPI0, MSPI_CLEAR_DONE_FLAG_BIT);
	mtk_write_fld(bs->regs + SPI_REG_0170_MSPI0, 0x1, REG_MSPI_CLEAR_DONE_FLAG);
}

static inline void dtvspi_hw_dma_trig_rst(struct dtv_spi *bs)
{
	u16 val;

	val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
	val &= ~MSPI_DMA_RSTZ;
	dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
	val = dtvspi_rd(bs, SPI_REG_00F0_MSPI0);
	val &= ~MSPI_XTAL_RSTZ;
	dtvspi_wr(bs, SPI_REG_00F0_MSPI0, val);
	val = dtvspi_rd(bs, SPI_REG_0124_MSPI0);
	val &= ~MSPI_RESET_BIT;
	dtvspi_wr(bs, SPI_REG_0124_MSPI0, val);
	udelay(DMA_RESET_3T);
	val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
	val |= MSPI_DMA_RSTZ;
	dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
	val = dtvspi_rd(bs, SPI_REG_00F0_MSPI0);
	val |= MSPI_XTAL_RSTZ;
	dtvspi_wr(bs, SPI_REG_00F0_MSPI0, val);
	val = dtvspi_rd(bs, SPI_REG_0124_MSPI0);
	val |= MSPI_RESET_BIT;
	dtvspi_wr(bs, SPI_REG_0124_MSPI0, val);
}

static inline void dtvspi_hw_enable(struct dtv_spi *bs, bool enable)
{
	u8 val;

	val = dtvspi_rdl(bs, SPI_REG_0124_MSPI0);
	if (enable) {
		val |= MSPI_ENABLE_BIT;
		val |= MSPI_RESET_BIT;
	} else {
		val &= ~MSPI_ENABLE_BIT;
		val &= ~MSPI_RESET_BIT;
	}
	dtvspi_wrl(bs, SPI_REG_0124_MSPI0, val);
}
static inline void dtvspi_hw_transfer_trigger(struct dtv_spi *bs)
{
	//dtvspi_wr(bs, MSPI_TRIGGER, MSPI_TRIGGER_BIT);
	mtk_write_fld(bs->regs + SPI_REG_0168_MSPI0, 0x1, REG_MSPI_TRIGGER);
}
static void dtvspi_hw_txdummy(struct dtv_spi *bs, u8 len)
{
	int cnt;

	for (cnt = 0; cnt < len>>1; cnt++)
		dtvspi_wr(bs, mspi_txfifoaddr[cnt], 0xffff);

	if (len&1)
		dtvspi_wrl(bs, mspi_txfifoaddr[cnt], 0xff);

	dtvspi_wrl(bs, SPI_REG_0120_MSPI0, 0);
	dtvspi_wrh(bs, SPI_REG_0120_MSPI0, len);
}
static void dtvspi_hw_txfillfifo(struct dtv_spi *bs, const u8 *buffer, u8 len)
{
	int cnt;

	for (cnt = 0; cnt < len>>1; cnt++)
		dtvspi_wr(bs, mspi_txfifoaddr[cnt],
			buffer[cnt<<1]|(buffer[(cnt<<1)+1]<<8));

	if (len&1)
		dtvspi_wrl(bs, mspi_txfifoaddr[cnt], buffer[cnt<<1]);

	dtvspi_wrl(bs, SPI_REG_0120_MSPI0, len);
	dtvspi_wrh(bs, SPI_REG_0120_MSPI0, 0);
}
static void dtvspi_hw_rxgetfifo(struct dtv_spi *bs, u8 *buffer, u8 len)
{
	int	cnt;

	for (cnt = 0; cnt < (len>>1); cnt++) {
		u16 val = dtvspi_rd(bs, mspi_rxfifoaddr[cnt]);

		buffer[cnt<<1] = val&0xff;
		buffer[(cnt<<1)+1] = val>>8;
	}
	if (len&1)
		buffer[cnt<<1] = dtvspi_rdl(bs, mspi_rxfifoaddr[cnt]);
}

static void dtvspi_hw_rxgetfifo_ext(struct dtv_spi *bs, u8 *buffer, u8 len)
{
	int	cnt;

	bs->irq2rcv_t = ktime_us_delta(ktime_get(), bs->start_time_t);
	for (cnt = 0; cnt < (len>>1); cnt++) {
		u16 val = dtvspi_rd(bs, mspi_rxfifoaddr_halfdeplex[cnt]);

		buffer[cnt<<1] = val&0xff;
		buffer[(cnt<<1)+1] = val>>8;
	}
	if (len&1)
		buffer[cnt<<1] = dtvspi_rdl(bs, mspi_rxfifoaddr_halfdeplex[cnt]);
}

static void dtvspi_hw_rx_ext(struct dtv_spi *bs)
{
	int	j;

	bs->irq2rcv_t = ktime_us_delta(ktime_get(), bs->start_time_t);
	j = bs->current_trans_len;
	if (bs->rx_buf != NULL) {
		dtvspi_hw_rxgetfifo_ext(bs, bs->rx_buf, j);
		(bs->rx_buf) += j;
	}
}

static void dtvspi_hw_xfer_ext(struct dtv_spi *bs)
{
	int	j;

	j = bs->len;
	if (j >= MAX_TX_BUF_SIZE)
		j = MAX_TX_BUF_SIZE;

	if (bs->tx_buf != NULL) {
		dtvspi_hw_txfillfifo(bs, bs->tx_buf, j);
		(bs->tx_buf) += j;
	} else {
		if (bs->rx_buf != NULL) {
			dtvspi_wrh(bs, SPI_REG_0120_MSPI0, (j));
			dtvspi_wrl(bs, SPI_REG_0120_MSPI0, 0);
		}
	}
	bs->len -= j;
	bs->current_trans_len = j;
	dtvspi_hw_enable_interrupt(bs, 1);
	dtvspi_hw_transfer_trigger(bs);
}

static void dtv_spi_hw_receive(struct dtv_spi *bs)
{
	if (bs->rx_buf != NULL) {
		dtvspi_hw_rxgetfifo(bs, bs->rx_buf, bs->current_trans_len);
		bs->rx_buf += bs->current_trans_len;
	}
}

static void dtv_spi_ldmtx_config(struct dtv_spi *bs, struct spi_transfer *tfr)
{
	u16 val;

	if (bs->dma_sel) {
		val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
		val |= MSPI_DMA_TX_EN;
		val |= MSPI_DMA_RSTZ;
		if (tfr->cs_change == 1)
			val |= MSPI_DMA_CS_SW;
		else
			val &= ~MSPI_DMA_CS_SW;
		if (bs->dma_nByte2nByte) {
			val |= MSPI_DMA_TX_NBYTE2NBYTE;
			dtvspi_wrl(bs, SPI_REG_0120_MSPI0,
					(tfr->bits_per_word+(SPI_BIT_LEN_FULL-1))/SPI_BIT_LEN_FULL);
		}
		dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
		dtvspi_wr(bs, SPI_REG_0148_MSPI0, (u16)(bs->len));
		dtvspi_wr(bs, SPI_REG_014C_MSPI0, (u16)((bs->len) >> LSB_SHIFT));
		mtk_writeb(bs->clkgen_bank_3 + RIU_RW_EN_REG, LNB_BYTE_MSK);
		if (!bs->fnc_callback)
		dtvspi_hw_enable_interrupt(bs, 0);
		else
			dtvspi_hw_enable_interrupt(bs, 1);
		complete(&bs->done);
	} else {
		val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
		val &= ~MSPI_DMA_TX_EN;
		val &= ~MSPI_DMA_RSTZ;
		val &= ~MSPI_DMA_TX_NBYTE2NBYTE;
		dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
		dtvspi_wr(bs, SPI_REG_0148_MSPI0, 0);
		dtvspi_wr(bs, SPI_REG_014C_MSPI0, 0);
		if (!bs->spi_dedicated)
		mtk_writeb(bs->clkgen_bank_3 + RIU_RW_EN_REG, 0);
	}
}

static int dtv_spi_bdma_xfer(struct dtv_spi *bs, struct spi_transfer *tfr)
{
	int err = 0;
	uint64_t dstaddr = 0;
	uint64_t srcaddr = 0;
	u16 val;

	if (bs->dma_sel) {
		val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
		if (tfr->tx_dma)
		val |= MSPI_DMA_TX_EN;
		else if (tfr->rx_dma) {
			val |= MSPI_DMA_RX_EN;
			val |= MSPI_DMA_CS_SW;
		}
		val |= MSPI_DMA_RSTZ;
		if (bs->dma_nByte2nByte) {
			val |= MSPI_DMA_TX_NBYTE2NBYTE;
			dtvspi_wrl(bs, SPI_REG_0120_MSPI0,
					(tfr->bits_per_word+(SPI_BIT_LEN_FULL-1))/SPI_BIT_LEN_FULL);
		}
		dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
		dtvspi_wr(bs, SPI_REG_0148_MSPI0, (u16)(bs->len));
		dtvspi_wr(bs, SPI_REG_014C_MSPI0, (u16)((bs->len) >> LSB_SHIFT));
		dtvspi_hw_enable_interrupt(bs, 0);
		if (bs->phyaddr > PA_OFFSET) {
			bs->phyaddr -= PA_OFFSET;
			bs->bdma_dev = mtk_bdma_request_channel(bs->bdma_ch, bs->bdma_ch);
			if (tfr->tx_dma)
				err = mtk_mspi_bdma_write_dev(bs->bdma_dev,
						bs->phyaddr, dstaddr, bs->len);
			else if (tfr->rx_dma)
				err = mtk_mspi_bdma_read_dev(bs->bdma_dev,
						srcaddr, bs->phyaddr, bs->len);
		} else
			err = -EINVAL;
		if (!err)
			complete(&bs->done);
	} else {
		val = dtvspi_rd(bs, SPI_REG_0140_MSPI0);
		val &= ~MSPI_DMA_TX_EN;
		val &= ~MSPI_DMA_RSTZ;
		val &= ~MSPI_DMA_TX_NBYTE2NBYTE;
		val &= ~MSPI_DMA_RX_EN;
		val &= ~MSPI_DMA_CS_SW;
		dtvspi_wr(bs, SPI_REG_0140_MSPI0, val);
		dtvspi_wr(bs, SPI_REG_0148_MSPI0, 0);
		dtvspi_wr(bs, SPI_REG_014C_MSPI0, 0);
		dtvspi_hw_enable_interrupt(bs, 1);
	}
	return err;
}


static void dtv_spi_hw_transfer(struct dtv_spi *bs)
{
	int len = bs->len;

	if (len >= 16)
		len = 16;

	if (bs->tx_buf != NULL) {
		dtvspi_hw_txfillfifo(bs, bs->tx_buf, len);
		bs->tx_buf += len;
	} else
			dtvspi_hw_txdummy(bs, len);

	bs->current_trans_len = len;
	bs->len -= len;
	dtvspi_hw_enable_interrupt(bs, 1);
	dtvspi_hw_transfer_trigger(bs);
}

static void (*_spicallback[LDM_SPI_CH])(void);

static irqreturn_t dtv_spi_interrupt(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct dtv_spi *bs = spi_master_get_devdata(master);

	if (dtvspi_rd(bs, SPI_REG_016C_MSPI0)) {
		if (!bs->fnc_callback)
		dtvspi_hw_enable_interrupt(bs, 0);
		dtvspi_hw_clear_done(bs);
		bs->event2irq_t = ktime_us_delta(ktime_get(), bs->start_time_t);
		if (bs->fnc_callback) {
			if (_spicallback[bs->mspi_channel] != NULL)
				_spicallback[bs->mspi_channel]();
			return IRQ_HANDLED;
		}

		if (bs->current_trans_len != 0) {
			if (bs->full_duplex == 1)
					dtv_spi_hw_receive(bs);
			else
				dtvspi_hw_rx_ext(bs);
		} else {
			complete(&bs->done);
			return IRQ_HANDLED;
		}

		if ((bs->len) != 0) {
			if (bs->full_duplex == 1)
				dtv_spi_hw_transfer(bs);
			else
				dtvspi_hw_xfer_ext(bs);
		} else {
			bs->current_trans_len = 0;
			complete(&bs->done);
		}
		return IRQ_HANDLED;
	}
	dev_dbg(&master->dev, "Error:incorrect irq num :%x\n", irq);
	dtvspi_hw_clear_done(bs);
	return IRQ_NONE;
}

static int dtv_spi_start_transfer(struct spi_device *spi,
		struct spi_transfer *tfr)
{
	struct dtv_spi *bs = spi_master_get_devdata(spi->master);

	if ((tfr->tx_dma) || (tfr->rx_dma)) {
		if ((bs->fifo_dma_switch) || (bs->mspi_channel == MSPI_FRC_CHANEL))
			bs->dma_sel = BDMA_SEL;
		else
			bs->dma_sel = 1;
		if (bs->dma_alloc == 0)
			bs->phyaddr = tfr->tx_dma;
	} else
		bs->dma_sel = 0;

	if ((bs->speed != tfr->speed_hz) || (bs->dbg_speed)) {
		/*Setup mspi clock for this transfer.*/
		dtvspi_hw_set_clock(bs, spi, tfr);
		bs->speed = tfr->speed_hz;
	}
	if (bs->sw_bits_per_word)
		dtvspi_hw_set_bit_len_sel(bs, tfr);

	/*Setup mspi cpol & cpha for this transfer.*/
	dtvspi_hw_set_mode(bs, spi, tfr);

	/*Enable SPI master controller&&Interrupt.*/
	dtvspi_hw_enable(bs, true);
	dtvspi_hw_clear_done(bs);
	dtvspi_hw_enable_interrupt(bs, true);
	bs->start_time_t = ktime_get();
	/*Setup mspi chip select for this transfer.*/
	if (bs->dma_sel == 0)
	dtvspi_hw_chip_select(bs, spi, true);
	else
		dtvspi_hw_dma_trig_rst(bs);
	/*KERNEL_VERSION(3, 13, 1)*/
	//INIT_COMPLETION(bs->done);

	reinit_completion(&bs->done);
	bs->tx_buf = tfr->tx_buf;
	bs->rx_buf = tfr->rx_buf;
	bs->len = tfr->len;
	if ((bs->tx_buf == NULL) || (bs->rx_buf == NULL))
		bs->full_duplex = 0;
	else
		bs->full_duplex = 1;
	/*Flexible DMA config*/
	if (bs->mspi_channel < MSPI_CI_CHANEL)
		dtv_spi_ldmtx_config(bs, tfr);

	if (bs->dma_sel == BDMA_SEL)
		dtv_spi_bdma_xfer(bs, tfr);

	if (bs->dma_sel == 0) {
		/*Start transfer loop.*/
		if (bs->full_duplex == 1)
			dtv_spi_hw_transfer(bs);
		else
			dtvspi_hw_xfer_ext(bs);
	}
	return 0;
}

static int dtv_spi_finish_transfer(struct spi_device *spi,
		struct spi_transfer *tfr, bool cs_change)
{
	struct dtv_spi *bs = spi_master_get_devdata(spi->master);

	if (tfr->delay_usecs)
		udelay(tfr->delay_usecs);

	if (cs_change) {
		/*Cancel chip select.*/
		dtvspi_hw_chip_select(bs, spi, false);
	}

	return 0;
}

static int dtv_spi_finish_dmatransfer(struct spi_device *spi,
		struct spi_transfer *tfr)
{
	struct dtv_spi *bs = spi_master_get_devdata(spi->master);

	dtvspi_hw_chip_select(bs, spi, true);
	return 0;
}

static int dtv_spi_transfer_one(struct spi_master *master,
		struct spi_message *mesg)
{
	struct dtv_spi *bs = spi_master_get_devdata(master);
	struct spi_transfer *tfr;
	struct spi_device *spi = mesg->spi;
	int err = 0;
	unsigned int timeout;
	bool cs_change;
	// physical address of mspi_msg
	dma_addr_t msg_pa;
	// buffer size for dma_alloc_coherent
	size_t dma_alloc_size;
	void *va = NULL;

	list_for_each_entry(tfr, &mesg->transfers, transfer_list) {

		if (bs->dma_alloc == true) {
			dma_alloc_size = (tfr->len);
			va = dma_alloc_coherent(bs->dev, dma_alloc_size,
						&msg_pa, GFP_KERNEL);
			if ((va == NULL) || (dma_alloc_size > SPI_MAX_BUF_SIZE)) {
				dev_err(bs->dev, "mspi failed allocate memory for msgs[]\n");
				return -ENOMEM;
			}
			if ((tfr->tx_buf))
				memcpy(va, tfr->tx_buf, tfr->len);

			bs->phyaddr = msg_pa;
		}
		err = dtv_spi_start_transfer(spi, tfr);
		if (err)
			goto out;

		timeout = wait_for_completion_timeout(&bs->done,
				msecs_to_jiffies(DTV_SPI_TIMEOUT_MS));

		if (!timeout) {
			err = -ETIMEDOUT;
			goto out;
		}

		if ((bs->dma_sel == 1) && (tfr->cs_change == 1))
			err = dtv_spi_finish_dmatransfer(spi, tfr);
		else {
			if (bs->dma_alloc)
				cs_change = tfr->cs_change;
			else
			cs_change = tfr->cs_change ||
				list_is_last(&tfr->transfer_list, &mesg->transfers);

			err = dtv_spi_finish_transfer(spi, tfr, cs_change);
		}

		if (err)
			goto out;

		mesg->actual_length += (tfr->len - bs->len);
	}

out:
	if (bs->dma_alloc == true)
		dma_free_coherent(bs->dev, dma_alloc_size, va, msg_pa);

	if (bs->bdma_dev) {
		mtk_bdma_release_channel(bs->bdma_dev);
		bs->bdma_dev = NULL;
	}
	mesg->status = err;
	spi_finalize_current_message(master);

	return 0;
}
static const struct of_device_id dtvspi_mspi_match[] = {
	{ .compatible = "mediatek,mt5896-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, dtvspi_mspi_match);

struct spi_device *km_spidev_node_get(u8 channel)
{
	return _ptr_array[channel];
}
EXPORT_SYMBOL(km_spidev_node_get);

int request_spi_irq(u8 channel, void *ptr)
{
	_spicallback[channel] = ptr;
	return 0;
}
EXPORT_SYMBOL(request_spi_irq);


static ssize_t irq_t_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->event2irq_t);
}
static DEVICE_ATTR_RO(irq_t);

static ssize_t rcv_t_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->irq2rcv_t);
}
static DEVICE_ATTR_RO(rcv_t);

static ssize_t dualsel_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);
	unsigned int tmp = 0;
	u8 val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &tmp);
	bs->dual2single = tmp;
	val = dtvspi_rdl(bs, SPI_REG_00E0_MSPI0);
	if (bs->dual2single == SIGGLETODUAL) {
		val &= ~MSPI_ENABLE_DUAL;
		val &= ~MSPI_DUAL_PAD_SEL;
	} else if (bs->dual2single == DUALTOSINGLE) {
		val |= MSPI_ENABLE_DUAL;
		val |= MSPI_DUAL_PAD_SEL;
	}
	dtvspi_wrl(bs, SPI_REG_00E0_MSPI0, val);
	return ret ? : size;
}

static ssize_t dualsel_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->dual2single);
}
static DEVICE_ATTR_RW(dualsel);

static ssize_t clkdiv_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);
	unsigned int tmp = 0;
	int ret;

	ret = kstrtouint(buf, 0, &tmp);
	bs->clk_div = tmp;
	return ret ? : size;
}

static ssize_t clkdiv_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->clk_div);
}
static DEVICE_ATTR_RW(clkdiv);

static ssize_t clksel_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);
	unsigned int tmp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &tmp);
	bs->clk_sel = tmp;
	return ret ? : size;
}

static ssize_t clksel_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->clk_sel);
}
static DEVICE_ATTR_RW(clksel);

static ssize_t speed_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);
	unsigned int tmp = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &tmp);
	bs->dbg_speed = tmp;
	return ret ? : size;
}

static ssize_t speed_show(struct device *child,
			   struct device_attribute *attr,
			   char *buf)
{
	struct spi_master *master = dev_get_drvdata(child);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	return scnprintf(buf, STRLEN, "%d\n", bs->dbg_speed);
}
static DEVICE_ATTR_RW(speed);

static struct attribute *spi_extend_attrs[] = {
	&dev_attr_irq_t.attr,
	&dev_attr_rcv_t.attr,
	&dev_attr_dualsel.attr,
	&dev_attr_clkdiv.attr,
	&dev_attr_clksel.attr,
	&dev_attr_speed.attr,
	NULL,
};
static const struct attribute_group spi_extend_attr_group = {
	.name = "spi_extend",
	.attrs = spi_extend_attrs
};

static int dtv_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct dtv_spi *bs;
	int err = -ENODEV;
	struct device_node *np = NULL;
	struct spi_board_info board_info = {
		.modalias   = "spidev",
	};
	const char *channel;

	master = spi_alloc_master(&pdev->dev, sizeof(*bs));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master() failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);

	master->mode_bits = DTV_SPI_MODE_BITS;
	master->bits_per_word_mask = BIT(8 - 1)|BIT(7 - 1)
					|BIT(6 - 1)|BIT(5 - 1)
					|BIT(4 - 1)|BIT(3 - 1)
					|BIT(2 - 1)|BIT(1 - 1);
	master->bus_num = -1;
	master->num_chipselect = 1;
	master->transfer_one_message = dtv_spi_transfer_one;
	master->dev.of_node = pdev->dev.of_node;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "fail to find node\n");
		return -EINVAL;
	}

	bs = spi_master_get_devdata(master);
	bs->dev = &pdev->dev;
	bs->bdma_dev = NULL;

	init_completion(&bs->done);

	//bs->regs = devm_ioremap_resource(&pdev->dev, res);
	bs->regs = of_iomap(np, 0);
	bs->clkgen_bank_1 = of_iomap(np, 1);
	bs->clkgen_bank_2 = of_iomap(np, CLK_DIV_IDX);
	bs->clkgen_bank_3 = of_iomap(np, NONPM_RIU_EN_IDX);

	//bs->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!bs->regs)
		return -EBUSY;

	bs->irq = platform_get_irq(pdev, 0);
	if (bs->irq < 0) {
		dev_err(&pdev->dev, "could not get irq resource\n");
		return -EINVAL;
	}

	if (of_get_property(pdev->dev.of_node, "full-duplex", NULL))
		bs->full_duplex = true;
	else
		bs->full_duplex = 0;

	if (of_get_property(pdev->dev.of_node, "fifo-dma-switch", NULL))
		bs->fifo_dma_switch = true;
	else
		bs->fifo_dma_switch = 0;

	if (of_get_property(pdev->dev.of_node, "dma-alloc", NULL))
		bs->dma_alloc = true;
	else
		bs->dma_alloc = 0;

	if (of_get_property(pdev->dev.of_node, "dma_nByes2nByte", NULL))
		bs->dma_nByte2nByte = true;
	else
		bs->dma_nByte2nByte = 0;

	if (of_get_property(pdev->dev.of_node, "sw_bits_per_word", NULL))
		bs->sw_bits_per_word = true;
	else
		bs->sw_bits_per_word = 0;

	if (of_get_property(pdev->dev.of_node, "fnc-callback", NULL))
		bs->fnc_callback = true;
	else
		bs->fnc_callback = 0;

	err = of_property_read_u32(pdev->dev.of_node,
				"mspi_channel", &bs->mspi_channel);
	if (err) {
		dev_info(&pdev->dev, "could not get chanel resource\n");
	}

	err = of_property_read_u32(pdev->dev.of_node,
				"data-xfer-cfg", &bs->data_xfer_cfg);
	if (err) {
		dev_info(&pdev->dev, "could not get data-xfer-cfg\n");
		bs->data_xfer_cfg = 0;
	}

	if (of_get_property(pdev->dev.of_node, "spi-deticated", NULL))
		bs->spi_dedicated = true;
	else
		bs->spi_dedicated = 0;

	if (bs->data_xfer_cfg || bs->dma_nByte2nByte) {
		err = of_property_read_u8_array(np, "tr_mux", bs->xfer_bits, XFER_MAX_NUM);
		if (err)
			return err;

		err = of_property_read_u32(pdev->dev.of_node,
					"xfer_trig_d", &bs->xfer_trig_d);
		if (err) {
			dev_info(&pdev->dev, "could not get xfer_trig_d\n");
			bs->xfer_trig_d = 0;
		}

		err = of_property_read_u32(pdev->dev.of_node,
					"xfer_interval", &bs->xfer_interval);
		if (err) {
			dev_info(&pdev->dev, "could not get xferinterval\n");
			bs->xfer_interval = 0;
		}

		err = of_property_read_u32(pdev->dev.of_node,
					"xfer_done_d", &bs->xfer_done_d);
		if (err) {
			dev_info(&pdev->dev, "could not get xfer_done_d\n");
			bs->xfer_done_d = 0;
		}

		err = of_property_read_u32(pdev->dev.of_node,
					"xfer_turn_d", &bs->xfer_turn_d);
		if (err) {
			dev_info(&pdev->dev, "could not get xfer_turn_d\n");
			bs->xfer_turn_d = 0;
		}
		/*Setup mspi transfer bits for this transfer.*/
		dtvspi_hw_set_cus(bs);
	}
	err = of_property_read_u32(pdev->dev.of_node,
				"clk_div", &bs->clk_div);
	if (err) {
		dev_info(&pdev->dev, "could not get clk_div\n");
		bs->clk_div = 0;
	}

	err = of_property_read_u32(pdev->dev.of_node,
				"clk_sel", &bs->clk_sel);
	if (err) {
		dev_info(&pdev->dev, "could not get clk_sel\n");
		bs->clk_sel = 0;
	}
	err = of_property_read_u32(pdev->dev.of_node,
				"speed", &bs->speed);
	if (err) {
		dev_info(&pdev->dev, "could not get speed\n");
		bs->speed = 0;
	} else {
		struct spi_transfer tfr;

		memset(&tfr, 0, sizeof(struct spi_transfer));
		tfr.speed_hz = bs->speed;
		dtvspi_hw_set_clock(bs, NULL, &tfr);
	}
	err = of_property_read_u32(pdev->dev.of_node,
				"dma_sel", &bs->dma_sel);
	if (err) {
		dev_info(&pdev->dev, "could not get dma_sel\n");
		bs->dma_sel = 0;
	}
	if (bs->dma_sel == BDMA_SEL) {
		err = of_property_read_string(pdev->dev.of_node,
				"bdma", &channel);
		if (!err)
			strscpy(bs->bdma_ch, channel, STRING_SIZE);
		if (bs->dma_alloc == true) {
			err = of_reserved_mem_device_init(&pdev->dev);
			if (err) {
				dev_err(&pdev->dev, "init reserved memory failed\n");
				of_reserved_mem_device_release(&pdev->dev);
				spi_master_put(master);
				return err;
			}
			dev_err(&pdev->dev, "coherent_dma_mask %llX, dma_mask %llX\n",
			pdev->dev.coherent_dma_mask, *(pdev->dev.dma_mask));

			if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(SPI_DMA_MASK))) {
				dev_err(&pdev->dev, "dma_set_mask_and_coherent failed\n");
				err = -ENODEV;
				of_reserved_mem_device_release(&pdev->dev);
				spi_master_put(master);
				return err;
			}
		}
	}

	if (!bs->regs || !bs->irq) {
		dev_err(&pdev->dev, "could not get resource\n");
		return -EINVAL;
	}

	err = devm_request_irq(&pdev->dev, bs->irq, dtv_spi_interrupt,
			IRQF_SHARED, dev_name(&pdev->dev), master);
	if (err) {
		dev_err(&pdev->dev,
				"could not request IRQ: %d:%d\n", bs->irq, err);
		return err;
	}

	devm_spi_register_master(&pdev->dev, master);
	if (err) {
		dev_err(&pdev->dev, "could not register SPI master: %d\n", err);
		return err;
	}

	bs->slave_dev = spi_new_device(master, &board_info);
	if (!bs->slave_dev)
		dev_warn(&pdev->dev, "Create user space control device failed\n");

	if (bs->mspi_channel < MAX_SPI_HW_NUM)
		_ptr_array[bs->mspi_channel] = (bs->slave_dev);

	bs->cs_alternative = devm_gpiod_get(&pdev->dev, "cs-select2", GPIOD_OUT_HIGH);
	if (IS_ERR(bs->cs_alternative))
		dev_warn(&pdev->dev, "bs->cs_alternative failed\n");

	err = sysfs_create_group(&pdev->dev.kobj, &spi_extend_attr_group);
	if (err < 0)
		dev_info(&pdev->dev, "spi sysfs_create_group failed: %d\n", err);

	return 0;
}

static int dtv_spi_remove(struct platform_device *pdev)
{
	return 0;
}
#ifdef CONFIG_PM
static int mtk_dtv_spi_suspend(struct device *dev)
{
	return 0;
}

static int mtk_dtv_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct dtv_spi *bs = spi_master_get_devdata(master);

	if (!bs)
		return -EINVAL;
	/*Transfer message will set speed again(tfr->speed)*/
	bs->speed = 0;
	return 0;
}

static SIMPLE_DEV_PM_OPS(dtvspi_pm_ops, mtk_dtv_spi_suspend, mtk_dtv_spi_resume);
#endif
static struct platform_driver dtv_spi_driver = {
	.driver			= {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = dtvspi_mspi_match,
#ifdef CONFIG_PM
		.pm         = &dtvspi_pm_ops,
#endif
	},
	.probe		= dtv_spi_probe,
	.remove		= dtv_spi_remove,
};

module_platform_driver(dtv_spi_driver);

MODULE_DESCRIPTION("MSPI controller driver");
MODULE_AUTHOR("Owen Tseng <Owen.Tseng@mediatek.com>");
MODULE_LICENSE("GPL v2");
