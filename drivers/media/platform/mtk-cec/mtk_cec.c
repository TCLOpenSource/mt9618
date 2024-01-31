// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/sysfs.h>
#include <media/cec.h>
#include <soc/mediatek/mtk-pm.h>

#include "mtk_cec.h"
#include "mtk_cec_hw.h"

#define PATCH_FORCE_SET_LA0 //patch for VIZIO M51a soundbar

#ifdef PATCH_FORCE_SET_LA0
static bool gforcePollingFlag = false;
#endif

static int mtk_cec_adap_enable(struct cec_adapter *adap, bool enable)
{
	struct mtk_cec_dev *cec_dev = cec_get_drvdata(adap);

	if (enable) {
		cec_dev->adap->phys_addr = 0x0000;
		mtk_cec_enable(cec_dev);
	} else
		mtk_cec_disable(cec_dev);

	return 0;
}

static int mtk_cec_adap_log_addr(struct cec_adapter *adap, u8 addr)
{
	struct mtk_cec_dev *cec_dev = cec_get_drvdata(adap);

	mtk_cec_setlogicaladdress(cec_dev, addr);
	return 0;
}

static irqreturn_t mtk_cec_irq_handler(int irq, void *priv)
{
	struct mtk_cec_dev *cec_dev = priv;
	u16 status;

	status = mtk_cec_get_event_status(cec_dev);
	dev_dbg(cec_dev->dev, "[%s] IRQ status = %x\n", __func__, status);

	if (status != 0x0) {
		if (status & CEC_RECEIVE_SUCCESS) {
			if (cec_dev->rx != STATE_IDLE)
				dev_crit(cec_dev->dev, "[%s] Buffer overrun\n", __func__);

			cec_dev->rx = STATE_BUSY;
			mtk_cec_get_receive_message(cec_dev);
			cec_dev->rx = STATE_DONE;
			mtk_cec_clear_rx_event_status(cec_dev);
		}

		if (status > 0x01) {
			if (status & CEC_SNED_SUCCESS)
				cec_dev->tx = STATE_DONE;
			else if (status & CEC_RETRY_FAIL)
				cec_dev->tx = STATE_NACK;
			else if (status & CEC_LOST_ARBITRRATION)
				cec_dev->tx = STATE_ERROR;
			else if (status & CEC_TRANSMIT_NACK)
				cec_dev->tx = STATE_NACK;

			mtk_cec_clear_tx_event_status(cec_dev);
		}
		return IRQ_WAKE_THREAD;
	}    else {
		return IRQ_HANDLED;
	}
}

static irqreturn_t mtk_cec_irq_handler_thread(int irq, void *priv)
{
	struct mtk_cec_dev *cec_dev = priv;
	unsigned int len = 0, index = 0;

#ifdef PATCH_FORCE_SET_LA0
	if (gforcePollingFlag) {
		// customized patch: always return NACK to force set LA0
		cec_dev->tx = STATE_NACK;
		gforcePollingFlag = false;
		dev_info(cec_dev->dev, "[%s] Tx: force state to STATE_NACK\n", __func__);
	}
#endif

	switch (cec_dev->rx) {
	case STATE_DONE:
		{
			len = cec_dev->msg.len;

			dev_dbg(cec_dev->dev, "[%s] len = %x\n", __func__, len);
			for (index = 0; index < len; index++) {
				dev_dbg(cec_dev->dev, "[%s] msg = %x\n", __func__,
					cec_dev->msg.msg[index]);
			}

			cec_received_msg(cec_dev->adap, &cec_dev->msg);
			cec_dev->rx = STATE_IDLE;
			dev_dbg(cec_dev->dev, "[%s] Rx: STATE_DONE\n", __func__);
		}
		break;
	default:
		break;
	}

	switch (cec_dev->tx) {
	case STATE_DONE:
		{
			cec_transmit_attempt_done(cec_dev->adap, CEC_TX_STATUS_OK);
			cec_dev->tx = STATE_IDLE;
			dev_dbg(cec_dev->dev, "[%s] Tx: STATE_DONE\n", __func__);
		}
		break;

	case STATE_NACK:
		{
			cec_transmit_attempt_done(cec_dev->adap, CEC_TX_STATUS_NACK |
								CEC_TX_STATUS_MAX_RETRIES);
			cec_dev->tx = STATE_IDLE;
			dev_dbg(cec_dev->dev, "[%s] Tx: STATE_NACK\n", __func__);
		}
		break;

	case STATE_ERROR:
		{
			cec_transmit_attempt_done(cec_dev->adap, CEC_TX_STATUS_ARB_LOST |
								CEC_TX_STATUS_MAX_RETRIES);
			cec_dev->tx = STATE_IDLE;
			dev_dbg(cec_dev->dev, "[%s] Tx: STATE_ERROR\n", __func__);
		}
		break;
	default:
		break;
	}

	return IRQ_HANDLED;
}

static int mtk_cec_adap_transmit(struct cec_adapter *adap, u8 attempts,
							u32 signal_free_time, struct cec_msg *msg)
{
	struct mtk_cec_dev *cec_dev = cec_get_drvdata(adap);

	dev_dbg(cec_dev->dev, "[%s] attempts %d, signal_free_time %d\n",
							__func__, attempts, signal_free_time);

#ifdef PATCH_FORCE_SET_LA0
	// customized patch: if ((opcode == 0x00) && (len == 1) && (polling 0)),
	// still send polling 0 cmd for get irq, but force to receive NACK
	if ((msg->msg[1] == 0x00) && (msg->len == 1) && ((msg->msg[0] & 0x0F) == 0)) {
		gforcePollingFlag = true;
		dev_info(cec_dev->dev, "[%s] Tx: ignore send polling 0 cmd\n", __func__);
	}
#endif

	if (!(mtk_cec_sendframe(cec_dev, msg->msg, msg->len, attempts)))
		return STATE_BUSY;

	return 0;
}

static const struct cec_adap_ops stCEC_adap_ops = {
	.adap_enable	=	mtk_cec_adap_enable,
	.adap_log_addr	=	mtk_cec_adap_log_addr,
	.adap_transmit	=	mtk_cec_adap_transmit,
};

static ssize_t cec_factory_pin_enable_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	int i = 0;
	struct mtk_cec_dev *mtk_cec = dev_get_drvdata(dev);

	dev_dbg(dev, "[%s] function called\n", __func__);
	dev_dbg(dev, "[%s] count = %zu", __func__, count);
	dev_dbg(dev, "[%s] buf = ", __func__);

	for (i = 0; i < count; i++)
		dev_dbg(dev, "[%s] buf[%d] =%X ", __func__, i, buf[i]);

	if (buf[0] == '1')
		mtk_cec_factory_pin_enable_store(mtk_cec, true);
	else if (buf[0] == '0')
		mtk_cec_factory_pin_enable_store(mtk_cec, false);

	return count;
}

static ssize_t cec_factory_pin_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int int_str_size = 0;

	//query HDMI number
	//sysfs_print(buf, &intStrSize, u16Size,6);

	return int_str_size;
}

static ssize_t cec_wakeup_command_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	int i = 0;

	dev_dbg(dev, "[%s] function called\n", __func__);
	dev_dbg(dev, "[%s] count = %zu", __func__, count);
	dev_dbg(dev, "[%s] buf = ", __func__);

	for (i = 0; i < count; i++)
		dev_dbg(dev, "[%s] buf[%d] =%X ", __func__, i, buf[i]);

	return count;
}
static ssize_t cec_wakeup_command_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	//u16 u16_size = 65535;
	int int_str_size = 0;

	//query HDMI number
	//sysfs_print(buf, &intStrSize, u16Size,6);

	return int_str_size;
}

static ssize_t cec_wakeup_config_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	int i = 0;

	dev_dbg(dev, "[%s] function called\n", __func__);
	dev_dbg(dev, "[%s] count = %zu", __func__, count);
	dev_dbg(dev, "[%s] buf = ", __func__);

	for (i = 0; i < count; i++)
		dev_dbg(dev, "[%s] buf[%d] =%X ", __func__, i, buf[i]);

	/*
	if (buf[0] == '1')
		pm_set_wakeup_config("cec0", true);
	else if (buf[0] == '0')
		pm_set_wakeup_config("cec0", false);

	dev_dbg(dev, "[%s] Set cec_wakeup_config = %c\n", __func__, buf[0]);
	*/

	return count;
}
static ssize_t cec_wakeup_config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	//u16 u16_size = 65535;
	int int_str_size = 0;

	//query HDMI number
	//sysfs_print(buf, &intStrSize, u16Size,6);

	return int_str_size;
}


static ssize_t cec_wakeup_enable_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	int i = 0;

	dev_dbg(dev, "[%s] function called\n", __func__);
	dev_dbg(dev, "[%s] count = %zu", __func__, count);

	dev_dbg(dev, "[%s] buf = ", __func__);

	for (i = 0; i < count; i++)
		dev_dbg(dev, "[%s] buf[%d] =%X ", __func__, i, buf[i]);

	if (buf[0] == '1')
		pm_set_wakeup_config("cec0", true);
	else if (buf[0] == '0')
		pm_set_wakeup_config("cec0", false);

	dev_dbg(dev, "[%s] Set cec_wakeup_config = %c\n", __func__, buf[0]);

	return count;
}
static ssize_t cec_wakeup_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	//u16 u16_size = 65535;
	int int_str_size = 0;

	//query HDMI number
	//sysfs_print(buf, &intStrSize, u16Size,6);

	return int_str_size;
}

static ssize_t cec_wakeup_command_enable_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	int i = 0;

	dev_dbg(dev, "[%s] function called\n", __func__);
	dev_dbg(dev, "[%s] count = %zu", __func__, count);
	dev_dbg(dev, "[%s] buf = ", __func__);

	for (i = 0; i < count; i++)
		dev_dbg(dev, "[%s] buf[%d] =%X ", __func__, i, buf[i]);

	return count;
}

static ssize_t cec_wakeup_command_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int int_str_size = 0;

	//query HDMI number
	//sysfs_print(buf, &intStrSize, u16Size,6);

	return int_str_size;
}


/* ========================== Attribute Functions ========================== */
static ssize_t help_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t len = 0;

	len += sysfs_emit_at(buf, len, "Debug Help:\n"
				"- log_level <RW>: To control debug log level.\n"
				"                  Read log_level value.\n");

	return len;
}

static ssize_t log_level_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct mtk_cec_dev *mtk_cec = dev_get_drvdata(dev);

	len += sysfs_emit_at(buf, len, "log_level = %lu\n", mtk_cec->log_level);

	return len;
}

static ssize_t log_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;
	struct mtk_cec_dev *mtk_cec = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;
	if (val > 255)
		return -EINVAL;

	mtk_cec->log_level = val;
	return count;
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(log_level);
static DEVICE_ATTR_RW(cec_wakeup_command);
static DEVICE_ATTR_RW(cec_wakeup_enable);
static DEVICE_ATTR_RW(cec_wakeup_command_enable);
static DEVICE_ATTR_RW(cec_wakeup_config);
static DEVICE_ATTR_RW(cec_factory_pin_enable);

static struct attribute *mtk_dtv_cec_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_log_level.attr,
	&dev_attr_cec_wakeup_command.attr,
	&dev_attr_cec_wakeup_enable.attr,
	&dev_attr_cec_wakeup_command_enable.attr,
	&dev_attr_cec_wakeup_config.attr,
	&dev_attr_cec_factory_pin_enable.attr,
	NULL,
};
static const struct attribute_group mtk_dtv_cec_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_dtv_cec_attrs
};

static int mtk_cec_probe(struct platform_device *pdev)
{
	struct mtk_cec_dev *pdata;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *dn;
	int ret = 0;

	dev_info(&pdev->dev, "[%s] CEC probe start\n", __func__);

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
/*
	if (!pdata) {
		dev_err(&pdev->dev, "[%s] Get resource fail\n", __func__);
		return -ENOMEM;
	}
*/
	platform_set_drvdata(pdev, pdata);
	pdata->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pdata->reg_base)) {
		dev_err(&pdev->dev, "err reg_base=%#lx\n", (unsigned long)pdata->reg_base);
		return PTR_ERR(pdata->reg_base);
	}
	dev_info(&pdev->dev, "reg_base=%#lx\n", (unsigned long)pdata->reg_base);

	dn = pdev->dev.of_node;

	ret = of_property_read_u32(dn, "xtal", &pdata->xtal);
	if ((ret < 0) || (pdata->xtal <= 0)) {
		dev_crit(&pdev->dev, "bad xtal(%d), set it to default value(%d)\n",
							pdata->xtal, MTK_GenSys_CEC_DEFAULT_XTAL);
		pdata->xtal = MTK_GenSys_CEC_DEFAULT_XTAL;
	}
	dev_dbg(&pdev->dev, "[%s] xtal = %d\n", __func__, pdata->xtal);

	pdata->adap = cec_allocate_adapter(&stCEC_adap_ops, pdata, "mediatek,cec",
							CEC_CAP_DEFAULTS, CEC_MAX_LOG_ADDRS);
	ret = PTR_ERR_OR_ZERO(pdata->adap);
	if (ret) {
		dev_err(&pdev->dev, "[%s] Couldn't create cec adapter\n", __func__);
		return ret;
	}

	ret = cec_register_adapter(pdata->adap, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "[%s] Couldn't register device\n", __func__);
		goto err_delete_adapter;
	}

	pdata->irq = platform_get_irq(pdev, 0);
	if (pdata->irq <= 0) {
		dev_err(&pdev->dev, "[%s] no IRQ resource\n", __func__);
		return pdata->irq;
	}

	ret = devm_request_threaded_irq(dev, pdata->irq, mtk_cec_irq_handler,
		mtk_cec_irq_handler_thread, IRQF_ONESHOT|IRQF_TRIGGER_RISING, pdev->name, pdata);
	if (ret) {
		dev_err(&pdev->dev, "[%s] Fail to request thread irq\n", __func__);
		return ret;
	}

	pdata->log_level = 4;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_dtv_cec_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s] Couldn't create sysfs\n", __func__);
		goto err_attr;
	}

	mtk_cec_enable(pdata);

	dev_info(&pdev->dev, "[%s] CEC successfully probe\n", __func__);

	return 0;

err_attr:
	dev_info(&pdev->dev, "Remove device attribute files\n");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_dtv_cec_attr_group);

err_delete_adapter:
	dev_err(&pdev->dev, "err_delete_adapter FAIL\n");
	cec_delete_adapter(pdata->adap);
	return ret;

}

static int mtk_cec_remove(struct platform_device *pdev)
{
	struct mtk_cec_dev *cec_dev = dev_get_drvdata(&pdev->dev);

	cec_unregister_adapter(cec_dev->adap);
	pm_runtime_disable(cec_dev->dev);

	return 0;
}

static int mtk_cec_suspend(struct device *dev)
{
	struct mtk_cec_dev *cec = dev_get_drvdata(dev);

	mtk_cec_config_wakeup(cec);
	dev_info(dev, "CEC suspend done\n");
	return 0;
}

static int mtk_cec_resume(struct device *dev)
{
	struct mtk_cec_dev *cec = dev_get_drvdata(dev);
	const uint8_t h50 = 0x50, h04 = 0x04;
	const uint8_t d2 = 2;

	mtk_cec_enable(cec);
	dev_info(dev, "CEC resume done\n");
	if (pm_get_wakeup_reason() == h04) {	// #define PM_WK_CEC (0x04)
		struct cec_msg cecmsg;

		cecmsg.len = d2;
		cecmsg.msg[0] = h50;
		cecmsg.msg[1] = h04;
		cec_received_msg(cec->adap, &cecmsg);
	}
	return 0;
}

static void mtk_cec_shutdown(struct platform_device *pdev)
{
	struct mtk_cec_dev *cec = dev_get_drvdata(&pdev->dev);

	mtk_cec_config_wakeup(cec);
	dev_info(&pdev->dev, "CEC shutdown done\n");
}

static const struct dev_pm_ops mtk_cec_pm_ops = {
	.suspend = mtk_cec_suspend,
	.resume = mtk_cec_resume,
};

static const struct of_device_id mtk_cec_match[] = {
	{
		.compatible = "mediatek,cec",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_cec_match);

static struct platform_driver mtk_cec_pdrv = {
	.probe	=	mtk_cec_probe,
	.remove	=	mtk_cec_remove,
	.driver	=	{
		.name	=	"mediatek,cec",
		.of_match_table	=	mtk_cec_match,
		.pm = &mtk_cec_pm_ops,
	},
	.shutdown =	mtk_cec_shutdown,
};


module_platform_driver(mtk_cec_pdrv);

MODULE_AUTHOR("Mediatek TV");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek CEC driver");
