// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
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
/* SPI register offsets */

#define CISPI_ENABLE_BIT				BIT(0)
#define CISPI_RESET_BIT					BIT(1)
#define CISPI_ENABLE_INT_BIT			BIT(2)
#define CISPI_CPHA_BIT					BIT(6)
#define CISPI_CPOL_BIT					BIT(7)
#define CISPI_TRIGGER_BIT				BIT(0)
#define CISPI_CLEAR_DONE_FLAG_BIT		BIT(0)
#define CISPI_CHIP_SELECT_BIT			BIT(0)
#define CIMAX_TX_BUF_SIZE				0x20
#define CISPI_ENABLE_DUAL				BIT(0)

#define CI_SPI_TIMEOUT_MS				1000
#define CI_SPI_MODE_BITS	(SPI_CS_HIGH | SPI_NO_CS | SPI_LSB_FIRST)

#define CI_NAME	"mspi-ci"

#define REG_SW_EN_CI_CILINK			0x1C04
#define REG_CISPI_CLK_SRC_MUX			0x1230
#define REG_CISPI_CLK_SRC				0x1238
#define CISPI_TO_MCU_SW_EN				0x1
#define CISPI_CLK						24000000
#define MCU_CICLK_108M_SEL				0x08
#define MCU_CICLK_SEL					0x04
#define MCU_CICLK_30M_SEL				0x1C
#define MCU_CICLK_48M_SEL				0x18
#define MCU_CICLK_62M_SEL				0x14
#define MCU_CICLK_72M_SEL				0x10
#define MCU_CICLK_86M_SEL				0x0C
#define MCU_CICLK_123M_SEL				0x04
#define MCU_CICLK_172M_SEL				0x00
#define CLK_CI_IDX						2
#define NONPM_CIRIU_EN_IDX				3
#define CISPI_DUAL_PAD_SEL				BIT(4)
#define HALF_WORD						8
#define DIV_2N4						4
#define CISPI_1MCLK					1000000
#define SPI_CTL_H					8

struct dtv_spi_ci {
	void __iomem *regs;
	void __iomem *clkgen_bank_1;
	void __iomem *clkgen_bank_2;
	void __iomem *clkgen_bank_3;
	u32 mspi_channel;
	int irq;
	struct completion done;
	const u8 *tx_buf;
	u8 *rx_buf;
	unsigned int len;
	unsigned int current_trans_len;
	unsigned int clk_div;
	unsigned int clk_sel;
	u32 speed;
	u32 mode;
	struct mutex bus_mutex;
};


static inline u16 dtvspici_rd(struct dtv_spi_ci *bs, u32 reg)
{
	return mtk_readw_relaxed((bs->regs + reg));
}

static inline u8 dtvspici_rdl(struct dtv_spi_ci *bs, u16 reg)
{
	return dtvspici_rd(bs, reg)&0xff;
}
static inline void dtvspici_wr(struct dtv_spi_ci *bs, u16 reg, u32 val)
{
	mtk_writew_relaxed((bs->regs + reg), val);
}

static inline void dtvspici_wrl(struct dtv_spi_ci *bs, u16 reg, u8 val)
{
	u16 val16 = dtvspici_rd(bs, reg)&0xff00;

	val16 |= val;
	dtvspici_wr(bs, reg, val16);
}

static inline void dtvspici_wrh(struct dtv_spi_ci *bs, u16 reg, u8 val)
{
	u16 val16 = dtvspici_rd(bs, reg)&0xff;

	val16 |= ((u16)val) << HALF_WORD;
	dtvspici_wr(bs, reg, val16);
}

static const u16 cispi_txfifoaddr[] = {
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

static const u16 cispi_rxfifoaddr_halfdeplex[] = {
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

static const u8 cispi_clk_sel[] = {
	MCU_CICLK_30M_SEL,
	MCU_CICLK_48M_SEL,
	MCU_CICLK_62M_SEL,
	MCU_CICLK_72M_SEL,
	MCU_CICLK_86M_SEL,
	MCU_CICLK_108M_SEL,
	MCU_CICLK_123M_SEL,
	MCU_CICLK_172M_SEL,
};


static void dtvspici_hw_set_clock(struct dtv_spi_ci *bs,
			struct spi_device *spi)
{
	u8 val;
	u16 clk_tmp;

	val = mtk_readb(bs->clkgen_bank_1 + REG_SW_EN_CI_CILINK);
	val |= CISPI_TO_MCU_SW_EN;
	mtk_writeb(bs->clkgen_bank_1 + REG_SW_EN_CI_CILINK, val);
	val = mtk_readb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC_MUX);
	val &= ~0xF;
	mtk_writeb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC_MUX, val);
	clk_tmp = dtvspici_rd(bs, SPI_REG_0124_MSPI0);
	if (spi->max_speed_hz <= CISPI_1MCLK) {
		clk_tmp |= (DIV_2N4 << SPI_CTL_H);
		dtvspici_wr(bs, SPI_REG_0124_MSPI0, clk_tmp);
	} else {
		clk_tmp &= ~(DIV_2N4 << SPI_CTL_H);
		dtvspici_wr(bs, SPI_REG_0124_MSPI0, clk_tmp);
	}
	if (spi->max_speed_hz >= CISPI_CLK) {
		val = mtk_readb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC_MUX);
		val |= MCU_CICLK_SEL;
		mtk_writeb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC_MUX, val);
		val = mtk_readb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC);
		val |= cispi_clk_sel[bs->clk_sel];
		mtk_writeb(bs->clkgen_bank_1 + REG_CISPI_CLK_SRC, val);
	}

}

static void dtvspici_hw_enable_interrupt(struct dtv_spi_ci *bs, bool enable)
{
	u8 val = dtvspici_rdl(bs, SPI_REG_0124_MSPI0);

	if (enable)
		val |= CISPI_ENABLE_INT_BIT;
	else
		val &= ~CISPI_ENABLE_INT_BIT;

	dtvspici_wrl(bs, SPI_REG_0124_MSPI0, val);
}


static void dtvspici_hw_set_mode(struct dtv_spi_ci *bs, struct spi_device *spi)
{
	u8 val = dtvspici_rdl(bs, SPI_REG_0124_MSPI0);

	if (spi->mode&SPI_CPOL)
		val |= CISPI_CPOL_BIT;
	else
		val &= ~CISPI_CPOL_BIT;

	if (spi->mode&SPI_CPHA)
		val |= CISPI_CPHA_BIT;
	else
		val &= ~CISPI_CPHA_BIT;

	dtvspici_wrl(bs, SPI_REG_0124_MSPI0, val);
	val = dtvspici_rdl(bs, SPI_REG_0140_MSPI0);

	val = dtvspici_rdl(bs, SPI_REG_00E0_MSPI0);
	if ((spi->mode&SPI_TX_DUAL) || (spi->mode&SPI_RX_DUAL)) {
		val |= CISPI_ENABLE_DUAL;
		val |= CISPI_DUAL_PAD_SEL;
	} else {
		val &= ~CISPI_ENABLE_DUAL;
		val &= ~CISPI_DUAL_PAD_SEL;
	}

	dtvspici_wrl(bs, SPI_REG_00E0_MSPI0, val);
}
static inline void dtvspi_hw_chip_select(struct dtv_spi_ci *bs,
					struct spi_device *spi, bool enable)
{
	u8 val;

	val = dtvspici_rdl(bs, SPI_REG_017C_MSPI0);
	if (enable)
		val &= ~CISPI_CHIP_SELECT_BIT;
	else
		val |= CISPI_CHIP_SELECT_BIT;

	dtvspici_wrl(bs, SPI_REG_017C_MSPI0, val);
}
static inline void dtvspi_hw_clear_done(struct dtv_spi_ci *bs)
{
	dtvspici_wrl(bs, SPI_REG_0170_MSPI0, CISPI_CLEAR_DONE_FLAG_BIT);
	//mtk_write_fld(bs->regs + SPI_REG_0170_MSPI0, 0x1, REG_MSPI_CLEAR_DONE_FLAG);
}

static inline void dtvspi_hw_enable(struct dtv_spi_ci *bs, bool enable)
{
	u8 val;

	val = dtvspici_rdl(bs, SPI_REG_0124_MSPI0);
	if (enable) {
		val |= CISPI_ENABLE_BIT;
		val |= CISPI_RESET_BIT;
	} else {
		val &= ~CISPI_ENABLE_BIT;
		val &= ~CISPI_RESET_BIT;
	}
	dtvspici_wrl(bs, SPI_REG_0124_MSPI0, val);
}
static inline void dtvspi_hw_transfer_trigger(struct dtv_spi_ci *bs)
{
	dtvspici_wrl(bs, SPI_REG_0168_MSPI0, CISPI_TRIGGER_BIT);
	//dtvspici_wr(bs->regs + SPI_REG_0168_MSPI0, 0x1, REG_MSPI_TRIGGER);
}

static void dtvspi_hw_txfillfifo(struct dtv_spi_ci *bs, const u8 *buffer, u8 len)
{
	u8 cnt;

	for (cnt = 0; cnt < (len>>1); cnt++)
		dtvspici_wr(bs, cispi_txfifoaddr[cnt],
			buffer[cnt<<1]|(buffer[(cnt<<1)+1]<<8));

	if (len&1)
		dtvspici_wrl(bs, cispi_txfifoaddr[cnt], buffer[cnt<<1]);

	dtvspici_wrl(bs, SPI_REG_0120_MSPI0, len);
	dtvspici_wrh(bs, SPI_REG_0120_MSPI0, 0);
}

static void dtvspi_hw_rxgetfifo_ext(struct dtv_spi_ci *bs, u8 *buffer, u8 len)
{
	u8	cnt;

	for (cnt = 0; cnt < (len>>1); cnt++) {
		u16 val = dtvspici_rd(bs, cispi_rxfifoaddr_halfdeplex[cnt]);

		buffer[cnt<<1] = val&0xff;
		buffer[(cnt<<1)+1] = val>>HALF_WORD;
	}
	if (len&1)
		buffer[cnt<<1] = dtvspici_rdl(bs, cispi_rxfifoaddr_halfdeplex[cnt]);
}

static void dtvspi_hw_rx_ext(struct dtv_spi_ci *bs)
{
	int	j;

	j = bs->current_trans_len;
	if (bs->rx_buf != NULL) {
		dtvspi_hw_rxgetfifo_ext(bs, bs->rx_buf, j);
		(bs->rx_buf) += j;
	}
}

static void dtvspi_hw_xfer_ext(struct dtv_spi_ci *bs)
{
	int	j;

	j = bs->len;
	if (j >= CIMAX_TX_BUF_SIZE)
		j = CIMAX_TX_BUF_SIZE;

	if (bs->tx_buf != NULL) {
		dtvspi_hw_txfillfifo(bs, bs->tx_buf, j);
		(bs->tx_buf) += j;
	} else {
		if (bs->rx_buf != NULL) {
			dtvspici_wrh(bs, SPI_REG_0120_MSPI0, (j));
			dtvspici_wrl(bs, SPI_REG_0120_MSPI0, 0);
		}
	}
	bs->len -= j;
	bs->current_trans_len = j;
	dtvspici_hw_enable_interrupt(bs, 1);
	dtvspi_hw_transfer_trigger(bs);
}

static irqreturn_t dtv_ci_interrupt(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct dtv_spi_ci *bs = spi_master_get_devdata(master);

	if (dtvspici_rd(bs, SPI_REG_016C_MSPI0)) {
		dtvspici_hw_enable_interrupt(bs, 0);
		dtvspi_hw_clear_done(bs);
		if (bs->current_trans_len != 0)
			dtvspi_hw_rx_ext(bs);
		else
			return IRQ_NONE;

		if ((bs->len) != 0)
			dtvspi_hw_xfer_ext(bs);
		else {
			bs->current_trans_len = 0;
			complete(&bs->done);
		}
		return IRQ_HANDLED;
	}
	dev_dbg(&master->dev, "Error:incorrect irq num :%x\n", irq);
	dtvspi_hw_clear_done(bs);
	return IRQ_NONE;
}

void ci_slave_config(struct spi_device *spi, u8 enable)
{
	struct dtv_spi_ci *bs = spi_master_get_devdata(spi->master);

	if (bs->speed != spi->max_speed_hz) {
		/*Setup mspi clock for this transfer.*/
		dtvspici_hw_set_clock(bs, spi);
		bs->speed = spi->max_speed_hz;
	}

	/*Setup mspi cpol & cpha for this transfer.*/
	dtvspici_hw_set_mode(bs, spi);

	/*Enable SPI master controller&&Interrupt.*/
	dtvspi_hw_enable(bs, enable);
	dtvspi_hw_clear_done(bs);
	//dtvspici_hw_enable_interrupt(bs, enable);

	reinit_completion(&bs->done);
}
EXPORT_SYMBOL(ci_slave_config);


void ci_slave_cs(struct spi_device *spi, u8 enable)
{
	struct dtv_spi_ci *bs = spi_master_get_devdata(spi->master);

	mutex_lock(&bs->bus_mutex);
	dtvspi_hw_chip_select(bs, spi, enable);
	mutex_unlock(&bs->bus_mutex);
}
EXPORT_SYMBOL(ci_slave_cs);

int ci_slave_write(struct spi_device *spi, u8 *buffer, u8 len)
{
	struct dtv_spi_ci *bs = spi_master_get_devdata(spi->master);
	unsigned int timeout;

	//printk("@@@@@ci_slave_write\n");
	mutex_lock(&bs->bus_mutex);
	bs->rx_buf = NULL;
	bs->tx_buf = buffer;
	bs->len = len;
	reinit_completion(&bs->done);
	dtvspi_hw_xfer_ext(bs);
	timeout = wait_for_completion_timeout(&bs->done,
			msecs_to_jiffies(CI_SPI_TIMEOUT_MS));

	if (!timeout) {
		mutex_unlock(&bs->bus_mutex);
		return -ETIMEDOUT;
	}
	mutex_unlock(&bs->bus_mutex);
	return 0;
}
EXPORT_SYMBOL(ci_slave_write);

int ci_slave_read(struct spi_device *spi, u8 *buffer, u8 len)
{
	struct dtv_spi_ci *bs = spi_master_get_devdata(spi->master);
	unsigned int timeout;

	//printk("@@@@@ci_slave_read\n");
	mutex_lock(&bs->bus_mutex);
	bs->tx_buf = NULL;
	bs->rx_buf = buffer;
	bs->len = len;
	reinit_completion(&bs->done);
	dtvspi_hw_xfer_ext(bs);
	timeout = wait_for_completion_timeout(&bs->done,
			msecs_to_jiffies(CI_SPI_TIMEOUT_MS));

	if (!timeout) {
		mutex_unlock(&bs->bus_mutex);
		return -ETIMEDOUT;
	}
	mutex_unlock(&bs->bus_mutex);
	return 0;
}
EXPORT_SYMBOL(ci_slave_read);

static const struct of_device_id dtvspi_ci_match[] = {
	{ .compatible = "mediatek,mtkdtv-spi-ci", },
	{}
};
MODULE_DEVICE_TABLE(of, dtvspi_ci_match);


static int dtv_spici_transfer_one(struct spi_master *master,
		struct spi_message *mesg)
{
	return 0;
}

static int dtvci_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct dtv_spi_ci *bs;
	int err = -ENODEV;
	struct device_node *np = NULL;

	master = spi_alloc_master(&pdev->dev, sizeof(*bs));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master() failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, master);

	master->mode_bits = CI_SPI_MODE_BITS;
	master->bits_per_word_mask = 0;
	master->bus_num = -1;
	master->num_chipselect = 1;
	master->dev.of_node = pdev->dev.of_node;
	master->transfer_one_message = dtv_spici_transfer_one;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "fail to find node\n");
		return -EINVAL;
	}

	bs = spi_master_get_devdata(master);

	init_completion(&bs->done);

	//bs->regs = devm_ioremap_resource(&pdev->dev, res);
	bs->regs = of_iomap(np, 0);
	bs->clkgen_bank_1 = of_iomap(np, 1);
	bs->clkgen_bank_2 = of_iomap(np, CLK_CI_IDX);
	bs->clkgen_bank_3 = of_iomap(np, NONPM_CIRIU_EN_IDX);

	//bs->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!bs->regs)
		return -EBUSY;

	bs->irq = platform_get_irq(pdev, 0);
	if (bs->irq < 0) {
		dev_err(&pdev->dev, "could not get irq resource\n");
		return -EINVAL;
	}

	err = of_property_read_u32(pdev->dev.of_node,
				"mspi_channel", &bs->mspi_channel);
	if (err)
		dev_info(&pdev->dev, "could not get chanel resource\n");

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

	if (!bs->regs || !bs->irq) {
		dev_err(&pdev->dev, "could not get resource\n");
		return -EINVAL;
	}

	err = devm_request_irq(&pdev->dev, bs->irq, dtv_ci_interrupt,
			IRQF_ONESHOT, dev_name(&pdev->dev), master);
	if (err) {
		dev_err(&pdev->dev,
				"could not request IRQ: %d:%d\n", bs->irq, err);
		return err;
	}

	devm_spi_register_master(&pdev->dev, master);
	mutex_init(&bs->bus_mutex);
	if (err) {
		dev_err(&pdev->dev, "could not register SPI master: %d\n", err);
		return err;
	}

	return 0;
}

static int dtvci_spi_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int mtk_dtvci_spi_suspend(struct device *dev)
{
	return 0;
}

static int mtk_dtvci_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct dtv_spi_ci *bs = spi_master_get_devdata(master);

	if (!bs)
		return -EINVAL;
	/*Transfer message will set speed again(tfr->speed)*/
	bs->speed = 0;
	bs->mode = 0;
	return 0;
}

static SIMPLE_DEV_PM_OPS(dtvspi_pm_ops, mtk_dtvci_spi_suspend, mtk_dtvci_spi_resume);
#endif

static struct platform_driver dtvci_spi_driver = {
	.driver			= {
		.name		= CI_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = dtvspi_ci_match,
	},
	.probe		= dtvci_spi_probe,
	.remove		= dtvci_spi_remove,
};

module_platform_driver(dtvci_spi_driver);

MODULE_DESCRIPTION("MSPI controller driver");
MODULE_AUTHOR("Owen Tseng <Owen.Tseng@mediatek.com>");
MODULE_LICENSE("GPL v2");
