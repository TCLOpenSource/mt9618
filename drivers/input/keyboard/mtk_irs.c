// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author Tsung-Hsien.Chiang <Tsung-Hsien.Chiang@mediatek.com>
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/pinctrl/consumer.h>
#include "mtk_irs.h"
#include "mtk_ir/ir-mt58xx-riu.h"
/* fifo size in elements (bytes) */
#define FIFO_SIZE	2048
/* lock for procfs read access */
static DEFINE_MUTEX(read_lock);
static inline u16 irs_read(void *reg)
{
	return mtk_readw_relaxed(reg);
}

static inline void irs_write(void *reg, uint16_t value)
{
	mtk_writew_relaxed(reg, value);
}
struct miscdata {
	int val;
	char *str;
	unsigned int size;
};

#define TEST_FIFO_SIZE	32
static const unsigned char expected_result[TEST_FIFO_SIZE] = {
	 3,  4,  5,  6,  7,  8,  9,  0,
	 1, 20, 21, 22, 23, 24, 25, 26,
	27, 28, 29, 30, 31, 32, 33, 34,
	35, 36, 37, 38, 39, 40, 41, 42,
};

#define	ISR_IOCTL_BASE	'W'

#define	ISRIOC_WRITERIUBAK		_IOR(ISR_IOCTL_BASE, 0, int)
#define	IRSIOC_HIGHSPEED		_IOR(ISR_IOCTL_BASE, 1, int)
#define	IRSIOC_NORMALSPEED		_IOR(ISR_IOCTL_BASE, 2, int)

#define IRS_PMUX_FUNC_NAME	"pm_irs_pmux"
#define IR_PMUX_FUNC_NAME	"pm_ir_pmux"

struct mtk_irs_dev {
	struct miscdevice misc;
	struct miscdata data;
	void __iomem *reg_base;
	struct device *dev;
	unsigned long hw_status;
	unsigned long irq_debug;
	spinlock_t lock;
};

static struct kfifo gread_fifo;
static int girs_irq;
static int mtk_irs_clearkfifo(void)
{
	int len = 0;
	u32 duration = 0;

	while (!kfifo_is_empty(&gread_fifo)) {
		kfifo_out(&gread_fifo, &duration, sizeof(duration));
		len++;
	}

	return len;
}
static ssize_t mtk_irs_portenable(struct mtk_irs_dev *pdata)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *default_state;
	int ret;

	spin_lock(&pdata->lock);
	if (pdata->hw_status) {
		/* enable irs pad mux select */
		pinctrl = devm_pinctrl_get(pdata->dev);
		if (IS_ERR(pinctrl)) {
			ret = PTR_ERR(pinctrl);
			goto exit;
		}

		default_state = pinctrl_lookup_state(pinctrl, IRS_PMUX_FUNC_NAME);
		if (IS_ERR(default_state)) {
			ret = PTR_ERR(default_state);
			goto exit;
		}

		pinctrl_select_state(pinctrl, default_state);

		dev_info(pdata->dev, "select padmux to %s, hw_status = %d\n",
			 IRS_PMUX_FUNC_NAME, (int)pdata->hw_status);

	} else {
		/* disable irs pad mux select */
		pinctrl = devm_pinctrl_get(pdata->dev);
		if (IS_ERR(pinctrl)) {
			ret = PTR_ERR(pinctrl);
			goto exit;
		}

		default_state = pinctrl_lookup_state(pinctrl, IR_PMUX_FUNC_NAME);
		if (IS_ERR(default_state)) {
			ret = PTR_ERR(default_state);
			goto exit;
		}
		pinctrl_select_state(pinctrl, default_state);

		dev_info(pdata->dev, "select padmux to %s, hw_status = %d\n",
			 IR_PMUX_FUNC_NAME, (int)pdata->hw_status);
	}
	ret = 0;
exit:
	spin_unlock(&pdata->lock);
	return ret;
}
static void mtk_irs_enable(u8 EnableIRS)
{
	if (EnableIRS) {
		pr_info("##### mtk_irs enable #####\n");

		//0x40[8]=0 unmask IRS_INT_MASK[0]
		irs_write(REG_IRS_INT_STS, (irs_read(REG_IRS_INT_STS) & (0xFEFF)));

		//0x42[11] IRS_THRESHOLD_CLR
		irs_write(REG_IRS_THRESHOLD_NUM,
			  (irs_read(REG_IRS_THRESHOLD_NUM) & (0xF7FF)) | 0x0800);

		//0x60[8]=1 reg_irs_sw_rst
		irs_write(REG_IRS_FIFO_RD, (irs_read(REG_IRS_FIFO_RD) | (0x0100)));
		enable_irq(girs_irq);
	} else {
		pr_info("##### mtk_irs disable #####\n");
		disable_irq(girs_irq);

		//0x40[8]=1 mask IRS_INT_MASK[0]
		irs_write(REG_IRS_INT_STS, (irs_read(REG_IRS_INT_STS) | (0x0100)));

		//0x42[11] IRS_THRESHOLD_CLR
		irs_write(REG_IRS_THRESHOLD_NUM,
			  (irs_read(REG_IRS_THRESHOLD_NUM) & (0xF7FF)) | 0x0800);

		//0x60[8]=1 reg_irs_sw_rst
		irs_write(REG_IRS_FIFO_RD, (irs_read(REG_IRS_FIFO_RD) | (0x0100)));
		//clear software kfifo
		mtk_irs_clearkfifo();
	}
}
static void mtk_irs_hw_config(void)
{
	pr_info("##### MTK IRS HW Config #####\n");
//1.	Enable clocks
//CKGEN01 0x6ee[0]REG_SW_EN_IRS2IRS 0x1
	irs_write(BANK_BASE_CLKGEN_1D_DC, (irs_read(BANK_BASE_CLKGEN_1D_DC) | 0x1));
//CKGEN00 0x2c2[4:2]REG_CKG_S_IRS 0x0 (12MHz)
	irs_write(BANK_BASE_CLKGEN_05_84, (irs_read(BANK_BASE_CLKGEN_05_84) & (0xFFE3)));
//CKGEN01 0x6ef[0]REG_SW_EN_MCU_NONPM2MCU_RIS 0x1
	irs_write(BANK_BASE_CLKGEN_1D_DE, (irs_read(BANK_BASE_CLKGEN_1D_DE) | 0x1));
//CKGEN01 0x410[4:2]REG_CKG_S_MCU_NONPM 0x1 (216MHz)
	irs_write(BANK_BASE_CLKGEN_18_20, (irs_read(BANK_BASE_CLKGEN_18_20) & (0xFFE3)) | 0x4);

//2.	set IRS HW
//`RIU_W( (`RIUBASE_IRS>>1)+ 7'h54, 2'b01, 16'h00_30);  // [ 7: 0] reg_irs_clkdiv_cnt = 8'h30
	irs_write(REG_IRS_CKDIV_CNT, IRS_CKDIV_CNT);

//`RIU_W( (`RIUBASE_IRS>>1)+ 7'h51, 2'b10, 16'h30_00); // [13:12]  reg_irs_shot_sel = 2'h3
	irs_write(REG_IRS_SHOT_SEL, 0x3000);

//`RIU_W( (`RIUBASE_IRS>>1)+ 7'h51, 2'b10, 16'h30_00); // [13:12]  reg_irs_shot_sel = 2'h3
	irs_write(REG_IRS_THRESHOLD_NUM, 0x0817);

//`RIU_W( (`RIUBASE_IRS>>1)+ 7'h7f, 2'b01, 16'h00_01); // [0]      reg_auto_clk_gate_en = 1'h1
	irs_write(REG_IRS_AUTO_CLK_GATE_EN, 0x01);

//`RIU_W( (`RIUBASE_IRS>>1)+ 7'h40, 2'b11, 16'h00_03); // [11:8]   reg_irs_int_mask = 4'h0
//                                                     // [1]      reg_irs_inv      = 1'h1
//                                                     // [0]      reg_irs_en       = 1'h1
	irs_write(REG_IRS_INT_STS, 0x03);

	//0x40[11:9] mas kIRS_INT_MASK[3:1]
	irs_write(REG_IRS_INT_STS, (irs_read(REG_IRS_INT_STS) | (0x0E00)));
}
static irqreturn_t mtk_irs_isr(int irq, void *data)
{

	struct mtk_irs_dev *pdata = data;
	u32 duration = 0;

	while (((irs_read(REG_IRS_SHOT_CNT_H) & IRS_FIFO_EMPTY) != IRS_FIFO_EMPTY)) {
		//1.	If FIFO is not empty, set 0x60[3] IRS_FIFO_RD = 1 to trigger read pulse
		irs_write(REG_IRS_FIFO_RD,
			  irs_read(REG_IRS_FIFO_RD) | 0x1UL << 3);

		duration = (((irs_read(REG_IRS_FIFO_DATA_H) & 0x7) << 16) |
			    ((irs_read(REG_IRS_FIFO_DATA_L)) & 0xFFFF)) * SAMPLE_RATE;

		if (kfifo_in(&gread_fifo, &duration, sizeof(duration)) != sizeof(duration))
			pr_err("Store IRS data Error! !!!!\n");
		if (pdata->irq_debug  == 1)
			pr_err("===>%s %d!!!!\n", __func__, duration);
	}

	//2.	Set 0x42[11] IRS_THRESHOLD_CLR to clear threshold interrupt
	irs_write(REG_IRS_THRESHOLD_NUM,
		  irs_read(REG_IRS_THRESHOLD_NUM) | 0x1UL << 11);
	return IRQ_HANDLED;
}
/**
 * usage:cat /dev/irs
 *
 * Return :
 * ret > 0 : number of signals
 * ret = 0 : empty.
 * ret < 0 : on failure
 */

static ssize_t mtk_irsmisc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;

	if (mutex_lock_interruptible(&read_lock))
		return -ERESTARTSYS;

	ret = kfifo_to_user(&gread_fifo, buf, count, &copied);

	mutex_unlock(&read_lock);
	pr_info("===>>ret:[%d],read buffer:[%d] !!!!\n", ret, copied);

	if (ret)
		return ret;
	return copied;

}

/**
 * NOTE:
 * How to use?
 * step 1.# echo "irin2irs" > /dev/irs
 *         :change port to IRS
 *
 * step 2.#echo "enable" > /dev/irs
 *        :enable IRS
 *
 * step 3.#cat /dev/irs
 *       Read IRS data
 *
 * step 4.echo "irs2irin" > /dev/irs
 *      :change port to IR_IN
 *
 * step 5.echo "disable" > /dev/irs
 *      :disable IRS
 *
 * Command :
 * echo "irin2irs" > /dev/irs
 *      : When use IR_IN port to IRS,must run echo "irin2irs" > /dev/irs
 *
 * echo "irs2irin" > /dev/irs
 *      : When use IR_IN port to IR,must run echo "irs2irin" > /dev/irs
 *
 * echo "enable" > /dev/irs
 *      : When use IRS,must run echo "enable" > /dev/irs
 *
 * echo "disable" > /dev/irs
 *      : When dose not use IRS,must run echo "disable" > /dev/irs
 *        and clear all software kfifo
 *
 * Debug command :
 *
 * echo "debug_enable" > /dev/irs
 *      : show irq raw data.
 *
 * echo "debug_disable" > /dev/irs
 *      : disable show irq raw data.
 *
 */
static ssize_t mtk_irsmisc_write(struct file *file,
				 const char __user *buf, size_t count,
				 loff_t *f_pos)
{
	char tmp[64];
	struct mtk_irs_dev *devp = (struct mtk_irs_dev *)(file->private_data);
	struct mtk_irs_dev *pdata;

	pdata = dev_get_drvdata(devp->misc.this_device);

	memset(tmp, 0, 64);
	if (count > 64 || copy_from_user(tmp, buf, count)) {
		pr_info("copy_from_user fail\n");
		return count;
	}

	if (strncmp(tmp, "disable", strlen("disable")) == 0) {
		mtk_irs_enable(false);
		pr_info("disable IRS IRQ\n");
	} else if (strncmp(tmp, "enable", strlen("enable")) == 0) {
		mtk_irs_enable(true);
		pr_info("enable IRS IRQ\n");
	} else if (strncmp(tmp, "irin2irs", strlen("irin2irs")) == 0) {
		pdata->hw_status = 1;
		mtk_irs_portenable(pdata);
		pr_info("Use irin to irs\n");
	} else if (strncmp(tmp, "irs2irin", strlen("irs2irin")) == 0) {
		pdata->hw_status = 0;
		mtk_irs_portenable(pdata);
		pr_info("Use irin to irs\n");
	} else if (strncmp(tmp, "debug_enable", strlen("debug_enable")) == 0) {
		pdata->irq_debug = 1;
		pr_info("Use irq debug enable\n");
	} else if (strncmp(tmp, "debug_disable", strlen("debug_disable")) == 0) {
		pdata->irq_debug = 0;
		pr_info("Use irq debug disable\n");
	}
	return count;
}
int mtk_irsmisc_open(struct inode *inode, struct file *file)
{

	return 0;
}
int mtk_irsmisc_release(struct inode *inode, struct file *file)
{
	return 0;
}
//For LTP
long mtk_irs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	int i = 0;
	u32 duration = 0;

	pr_info("%s\n", __func__);

	switch (cmd) {
	case ISRIOC_WRITERIUBAK:
		irs_write(REG_IRS_MCU_FORCE_WDATA, MAGIC_NUM);
		if (irs_read(REG_IRS_MCU_FORCE_WDATA) == MAGIC_NUM)
			ret = 0;
		else
			ret = -EINVAL;
		pr_info("ISRIOC_WRITERIUBAK\n");
		break;
	case IRSIOC_HIGHSPEED:
			mtk_irs_enable(false);
			pr_info("IRSIOC_HIGHSPEED\n");
			//out of bounds test
			for (i = 0; i < FIFO_SIZE + 100; i++) {
				duration = MAGIC_NUM;
				if (kfifo_in(&gread_fifo, &duration,
					     sizeof(duration)) != sizeof(duration)) {
					//pr_info("Store IRS data Error! !!!!\n");
					ret = 0;//pass
				}
			}
		break;
	case IRSIOC_NORMALSPEED:
			mtk_irs_enable(false);

			for (i = 0; i < TEST_FIFO_SIZE; i++) {
				duration = expected_result[i];
				if (kfifo_in(&gread_fifo, &duration,
					     sizeof(duration)) != sizeof(duration)) {
					pr_info("Store IRS data Error! !!!!\n");
					ret = -EINVAL;
					goto out;
				}
			}
			for (i = 0; i < TEST_FIFO_SIZE; i++) {
				kfifo_out(&gread_fifo, &duration, sizeof(duration));
				if (duration != expected_result[i]) {
					pr_info("value mismatch: test failed\n");
					ret = -EINVAL;
					goto out;
				}
			}

			ret = 0;//Pass
			pr_info("IRSIOC_NORMALSPEED\n");
		break;
	default:
		pr_info("No cmd\n");
		ret = -EINVAL;
		break;

	}

out:
	return ret;
}
static const struct file_operations mtk_irs_fops = {
	.owner = THIS_MODULE,
	.open = mtk_irsmisc_open,
	.read = mtk_irsmisc_read,
	.write = mtk_irsmisc_write,
	.release = mtk_irsmisc_release,
	.unlocked_ioctl = mtk_irs_ioctl,
	.compat_ioctl = mtk_irs_ioctl,
};
static int mtk_irs_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	int error;
	struct device_node *np = NULL;
	u32 duration = 0;
	static struct mtk_irs_dev *pdata;
	struct device_node *dn;

	pr_info("%s\n", __func__);
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	platform_set_drvdata(pdev, pdata);

	pdata->dev = &pdev->dev;

	pdata->misc.minor = MISC_DYNAMIC_MINOR;
	pdata->misc.name = "irs";

	pdata->misc.fops = &mtk_irs_fops;

	np = of_find_compatible_node(NULL, NULL, "mediatek,mtk-irs");
	if (np == NULL)
		pr_info("%s:fail to find node\n", __func__);
	irs_map_membase = of_iomap(np, 0);
	clkgen_bank_membase = of_iomap(np, 1);
	// pr_info("==>map_membase:(0x%x ,0x%x)!!!!\n", (unsigned int)irs_map_membase,
		// (unsigned int)clkgen_bank_membase);
	if (IS_ERR(pdata->reg_base)) {
		dev_err(&pdev->dev, "err reg_base=0x%lx\n", (unsigned long)pdata->reg_base);
		return PTR_ERR(pdata->reg_base);
	}

	dn = pdev->dev.of_node;
	/*pdata->irs_irq */girs_irq = irq_of_parse_and_map(dn, 0);

	pr_info("mtk_irs irq =%d\n", girs_irq);

	/* register irq */
	ret = devm_request_irq(&pdev->dev, girs_irq,
				mtk_irs_isr, IRQF_SHARED,
				"mtk_irs", pdata);
	if (ret) {
		dev_err(&pdev->dev, "Error requesting IRQ %d: %d\n",
			girs_irq, ret);
		return ret;
	}

	memset(&(pdata->data), 0, sizeof(pdata->data));


	error = misc_register(&pdata->misc);

	pr_info("%s:misc_register %d\n", __func__, error);

	dev_set_drvdata(pdata->misc.this_device, pdata);
	mtk_irs_hw_config();

	//init kfifo
	ret = kfifo_alloc(&gread_fifo, FIFO_SIZE*sizeof(duration), GFP_KERNEL);
	if (ret) {
		pr_err("error kfifo_alloc fail\n");
		return ret;
	}
	//default disable IRS
	mtk_irs_enable(false);

	spin_lock_init(&pdata->lock);

	/* disable irs hw as default */
	pdata->hw_status = 0;
	pdata->irq_debug = 0;
	return error;
}
static int mtk_irs_drv_remove(struct platform_device *pdev)
{
	int error = 0;
	struct mtk_irs_dev *priv = platform_get_drvdata(pdev);

	misc_deregister(&priv->misc);
	kfifo_free(&gread_fifo);

	return error;
}
#ifdef CONFIG_PM
static int mtk_irs_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mtk_irs_drv_resume(struct platform_device *pdev)
{
	return 0;
}
#endif
static const struct of_device_id mtk_dtv_irs_match[] = {
	{
		.compatible = "mediatek,mtk-irs",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_dtv_irs_match);
	static struct platform_driver mtk_irs_platform_driver = {
	.driver = {
		.name = "mtk_irs",
		.of_match_table = of_match_ptr(mtk_dtv_irs_match),
	},
	.probe = mtk_irs_drv_probe,
	.remove = mtk_irs_drv_remove,
	#ifdef CONFIG_PM
	.suspend	= mtk_irs_drv_suspend,
	.resume		= mtk_irs_drv_resume,
	#endif
	};
module_platform_driver(mtk_irs_platform_driver);

MODULE_DESCRIPTION("MediaTek mt5896 SoC based IRS Driver");
MODULE_AUTHOR("Tsung-Hsien.Chiang@mediatek.com");
MODULE_LICENSE("GPL");
