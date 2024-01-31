// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <linux/rpmsg.h>
#include <linux/completion.h>

#define IPCM_MAX_DATA_SIZE	64
#define GPIO_MAX_DATA_SIZE	32
#define GPIO_REQUEST_TIMEOUT (5 * HZ)
#define DEFAULT_NUM_OF_GPIOS	32

enum ipcm_ack_t {
	IPCM_CMD_OP = 0,
	IPCM_CMD_BAD_ACK,
	IPCM_CMD_ACK,
};

struct ipcm_msg {
	uint8_t cmd;
	uint32_t size;
	uint8_t data[IPCM_MAX_DATA_SIZE];
};

enum gpio_cmd_t {
	GPIO_CMD_OPEN = 0,
	GPIO_CMD_GET_DIRECTION,
	GPIO_CMD_SET_DIRECTION_INPUT,
	GPIO_CMD_SET_DIRECTION_OUTPUT,
	GPIO_CMD_GET_VALUE,
	GPIO_CMD_SET_VALUE,
	GPIO_CMD_CLOSE,
	GPIO_CMD_MAX,
};

struct gpio_msg {
	uint8_t cmd;
	uint32_t offset;
	uint8_t data[GPIO_MAX_DATA_SIZE];
};

struct mtk_pmu_gpio_rpmsg {
	struct device *dev;
	struct rpmsg_device *rpdev;
	struct completion ack;
	int ack_status;
};

struct mtk_pmu_gpio {
	raw_spinlock_t lock;
	void __iomem *reg_base;
	struct gpio_chip gc;
	struct device *dev;
	const char *gpio_rpmsg_ept_name;
	struct rpmsg_device *rpdev;
};

static int mtk_pmu_gpio_rpmsg_callback(struct rpmsg_device *rpdev,
				       void *data, int len, void *priv, u32 src)
{
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	struct ipcm_msg *msg;

	if (!rpdev) {
		dev_err(NULL, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(&rpdev->dev, "private data is NULL\n");
		return -EINVAL;
	}

	if (!data || len == 0) {
		dev_err(&rpdev->dev, "Invalid data or length from src:%d\n",
			src);
		return -EINVAL;
	}

	msg = (struct ipcm_msg *)data;

	switch (msg->cmd) {
	case IPCM_CMD_ACK:
		dev_dbg(&rpdev->dev, "Got OK ack, result = %d\n",
			*(int *)&msg->data);
		pdata_rpmsg->ack_status = *(int *)&msg->data;
		complete(&pdata_rpmsg->ack);
		break;
	case IPCM_CMD_BAD_ACK:
		dev_dbg(&rpdev->dev, "Got BAD ack, result = %d\n",
			*(int *)&msg->data);
		pdata_rpmsg->ack_status = *(int *)&msg->data;
		complete(&pdata_rpmsg->ack);
		break;
	default:
		dev_err(&rpdev->dev, "Invalid command %d from src:%d\n",
			msg->cmd, src);
		return -EINVAL;
	}
	return 0;
}

static int _mtk_pmu_gpio_operations(struct rpmsg_device *rpdev,
				    struct mtk_pmu_gpio_rpmsg *pdata_rpmsg,
				    enum gpio_cmd_t cmd, int offset, int val)
{
	struct ipcm_msg op_msg = {0};
	struct gpio_msg *gpio_msg = (struct gpio_msg *)&op_msg.data;
	int ret, ret_wait;

	op_msg.cmd = IPCM_CMD_OP;
	op_msg.size = sizeof(struct gpio_msg);
	gpio_msg->cmd = cmd;
	gpio_msg->offset = offset;
	gpio_msg->data[0] = val;

	ret = rpmsg_send(rpdev->ept, &op_msg, sizeof(struct ipcm_msg));
	if (ret) {
		dev_err(&rpdev->dev, "rpmsg send failed, ret=%d\n", ret);
		return ret;
	}

	ret_wait = wait_for_completion_timeout(&pdata_rpmsg->ack,
					       GPIO_REQUEST_TIMEOUT);
	if (!ret_wait)
		ret = -ETIMEDOUT;
	else
		ret = pdata_rpmsg->ack_status;

	return ret;
}

static int mtk_pmu_gpio_get_direction(struct gpio_chip *gc,
				      unsigned int offset)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	int ret;

	if (offset >= gc->ngpio) {
		dev_err(pdata->dev,
			"set input: offset(%d) is bigger than ngpio(%d)\n",
			offset, gc->ngpio);
		return -EINVAL;
	}

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return -EINVAL;
	}

	ret = _mtk_pmu_gpio_operations(rpdev, pdata_rpmsg,
				       GPIO_CMD_GET_DIRECTION, offset, 0);

	dev_dbg(pdata->dev, "get dir: offset=%d, ret=%d\n", offset, ret);

	/* 0=out, 1=in */
	return ret;
}

static int mtk_pmu_gpio_direction_input(struct gpio_chip *gc,
					unsigned int offset)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	int ret;

	if (offset >= gc->ngpio) {
		dev_err(pdata->dev,
			"set input: offset(%d) is bigger than ngpio(%d)\n",
			offset, gc->ngpio);
		return -EINVAL;
	}

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return -EINVAL;
	}

	ret = _mtk_pmu_gpio_operations(rpdev, pdata_rpmsg,
				       GPIO_CMD_SET_DIRECTION_INPUT, offset, 0);

	dev_dbg(pdata->dev, "set input: offset=%d, ret=%d\n", offset, ret);

	return ret;
}

static int mtk_pmu_gpio_direction_output(struct gpio_chip *gc,
					 unsigned int offset, int value)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	int ret;

	if (offset >= gc->ngpio) {
		dev_err(pdata->dev,
			"set output: offset(%d) is bigger than ngpio(%d)\n",
			offset, gc->ngpio);
		return -EINVAL;
	}

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return -EINVAL;
	}

	ret = _mtk_pmu_gpio_operations(rpdev, pdata_rpmsg,
				       GPIO_CMD_SET_DIRECTION_OUTPUT,
				       offset, value);

	dev_dbg(pdata->dev, "set output: offset=%d, value=0x%x, ret=%d\n",
		offset, value, ret);

	return ret;
}

static int mtk_pmu_gpio_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	int ret;

	if (offset >= gc->ngpio) {
		dev_err(pdata->dev,
			"get value: offset(%d) is bigger than ngpio(%d)\n",
			offset, gc->ngpio);
		return -EINVAL;
	}

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return -EINVAL;
	}

	ret = _mtk_pmu_gpio_operations(rpdev, pdata_rpmsg,
				       GPIO_CMD_GET_VALUE, offset, 1);

	dev_dbg(pdata->dev, "get value: offset=%d, ret=%d\n", offset, ret);

	return ret;
}

static void mtk_pmu_gpio_set_value(struct gpio_chip *gc, unsigned int offset,
				   int value)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	int ret;

	if (offset >= gc->ngpio) {
		dev_err(pdata->dev,
			"set value: offset(%d) is bigger than ngpio(%d)\n",
			offset, gc->ngpio);
		return;
	}

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return;
	}

	ret = _mtk_pmu_gpio_operations(rpdev, pdata_rpmsg,
				       GPIO_CMD_SET_VALUE, offset, value);

	dev_dbg(pdata->dev, "set value: offset=%d, value=0x%x, ret=%d\n",
		offset, value, ret);
}

static int mtk_pmu_gpio_request(struct gpio_chip *gc, unsigned int offset)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	const char *pdev_name = dev_name(pdata->dev);
	int cnt = strlen(pdev_name);
	int ret, ret_wait;
	struct gpio_msg *gpio_msg;
	struct ipcm_msg open_msg = {
		.cmd = IPCM_CMD_OP,
		.size = 0,
		.data = {0},
	};
	struct rpmsg_device *rpdev = pdata->rpdev;
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;

	if (!rpdev) {
		dev_err(pdata->dev, "rpdev is NULL\n");
		return -EINVAL;
	}

	pdata_rpmsg = dev_get_drvdata(&rpdev->dev);
	if (!pdata_rpmsg) {
		dev_err(pdata->dev, "private data is NULL\n");
		return -EINVAL;
	}

	gpio_msg = (struct gpio_msg *)&open_msg.data;
	gpio_msg->cmd = GPIO_CMD_OPEN;
	strncpy(gpio_msg->data, pdev_name, cnt);

	ret = rpmsg_send(rpdev->ept, &open_msg, sizeof(struct ipcm_msg));
	if (ret) {
		dev_err(pdata->dev, "rpmsg send failed, ret=%d\n", ret);
		return ret;
	}

	ret_wait = wait_for_completion_timeout(&pdata_rpmsg->ack,
					       GPIO_REQUEST_TIMEOUT);

	if (!ret_wait)
		ret = -ETIMEDOUT;
	else
		ret = pdata_rpmsg->ack_status;

	return ret;
}

static int mtk_pmu_gpio_rpmsgchip_match(struct gpio_chip *gc, void *data)
{
	struct mtk_pmu_gpio *pdata = gpiochip_get_data(gc);
	int ret;

	if (pdata->gpio_rpmsg_ept_name)
		ret = !strcmp(pdata->gpio_rpmsg_ept_name, data);
	else
		ret = 0;

	return ret;
}

static int mtk_pmu_gpio_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct mtk_pmu_gpio_rpmsg *pdata_rpmsg;
	struct gpio_chip *gc;
	struct mtk_pmu_gpio *pdata;

	pdata_rpmsg = devm_kzalloc(&rpdev->dev,
				   sizeof(struct mtk_pmu_gpio_rpmsg),
				   GFP_KERNEL);
	if (!pdata_rpmsg)
		return -ENOMEM;

	pdata_rpmsg->rpdev = rpdev;
	pdata_rpmsg->dev = &rpdev->dev;
	init_completion(&pdata_rpmsg->ack);
	dev_set_drvdata(&rpdev->dev, pdata_rpmsg);

	/* hook rpmsg device to gpio private data */
	gc = gpiochip_find(rpdev->id.name, mtk_pmu_gpio_rpmsgchip_match);
	if (gc) {
		pdata = gpiochip_get_data(gc);
		pdata->rpdev = rpdev;
	} else {
		dev_err(&rpdev->dev, "rpmsg get gpio chip failed\n");
		return -ENXIO;
	}

	dev_info(&rpdev->dev, "gpio rpmsg probe done\n");

	return 0;
}

static int mtk_pmu_gpio_probe(struct platform_device *pdev)
{
	struct mtk_pmu_gpio *pdata;
	struct device_node *dn;
	int ret, of_base, of_ngpio;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	platform_set_drvdata(pdev, pdata);
	pdata->dev = &pdev->dev;

	dn = pdev->dev.of_node;

	raw_spin_lock_init(&pdata->lock);

	pdata->gc.direction_input = mtk_pmu_gpio_direction_input;
	pdata->gc.direction_output = mtk_pmu_gpio_direction_output;
	pdata->gc.get = mtk_pmu_gpio_get_value;
	pdata->gc.set = mtk_pmu_gpio_set_value;
	pdata->gc.request = mtk_pmu_gpio_request;
	pdata->gc.get_direction = mtk_pmu_gpio_get_direction;

	ret = of_property_read_string(dn, "rpmsg-ept-name",
				      &pdata->gpio_rpmsg_ept_name);
	if (ret) {
		dev_err(&pdev->dev, "get rpmsg-ept-name error: ret=%d\n", ret);
		return ret;
	}

	pdata->gc.label = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s-%s",
					 dev_name(&pdev->dev),
					 pdata->gpio_rpmsg_ept_name);
	pdata->gc.parent = &pdev->dev;
	pdata->gc.owner = THIS_MODULE;
	if (of_property_read_u32(dn, "base", &of_base)) {
		/* let gpio framework control the base */
		pdata->gc.base = -1;
		dev_info(&pdev->dev, "set base to default(-1)\n");
	} else {
		pdata->gc.base = of_base;
	}

	if (of_property_read_u32(dn, "ngpios", &of_ngpio)) {
		pdata->gc.ngpio = DEFAULT_NUM_OF_GPIOS;
		dev_info(&pdev->dev, "set ngpios to default(32)\n");
	} else {
		pdata->gc.ngpio = of_ngpio;
	}

	dev_dbg(&pdev->dev, "base(%d), ngpios(%d), rpmsg-ept-name(%s)\n",
		 pdata->gc.base, pdata->gc.ngpio, pdata->gpio_rpmsg_ept_name);

	ret = devm_gpiochip_add_data(&pdev->dev, &pdata->gc, pdata);
	if (ret) {
		dev_err(&pdev->dev, "probe add data error: ret=%d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "gpio probe done\n");

	return 0;
}

static void mtk_pmu_gpio_rpmsg_remove(struct rpmsg_device *rpdev)
{
	dev_set_drvdata(&rpdev->dev, NULL);
	dev_info(&rpdev->dev, "gpio rpmsg remove done\n");
}

static struct rpmsg_device_id mtk_pmu_gpio_rpmsg_id_table[] = {
	{ .name	= "gpio-pmu-ept" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, mtk_pmu_gpio_rpmsg_id_table);

static const struct of_device_id mtk_pmu_gpio_match[] = {
	{
		.compatible = "mediatek,mtk-pmu-gpio",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_pmu_gpio_match);

static struct rpmsg_driver mtk_pmu_gpio_rpmsg_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= mtk_pmu_gpio_rpmsg_id_table,
	.probe		= mtk_pmu_gpio_rpmsg_probe,
	.callback	= mtk_pmu_gpio_rpmsg_callback,
	.remove		= mtk_pmu_gpio_rpmsg_remove,
};

static struct platform_driver mtk_pmu_gpio_driver = {
	.probe		= mtk_pmu_gpio_probe,
	.driver = {
		.name	= "mtk_pmu_gpio",
		.of_match_table = of_match_ptr(mtk_pmu_gpio_match),
	},
};

static int __init mtk_pmu_gpio_init_driver(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_pmu_gpio_driver);
	if (ret) {
		pr_err("pmu gpio driver register failed (%d)\n", ret);
		return ret;
	}

	ret = register_rpmsg_driver(&mtk_pmu_gpio_rpmsg_driver);
	if (ret) {
		pr_err("rpmsg gpio driver register failed (%d)\n", ret);
		platform_driver_unregister(&mtk_pmu_gpio_driver);
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_pmu_gpio_init_driver);

static void __exit mtk_pmu_gpio_exit_driver(void)
{
	unregister_rpmsg_driver(&mtk_pmu_gpio_rpmsg_driver);
	platform_driver_unregister(&mtk_pmu_gpio_driver);
}
module_exit(mtk_pmu_gpio_exit_driver);

MODULE_DESCRIPTION("MediaTek DTV SoC based GPIO Driver");
MODULE_AUTHOR("Kevin Ho <kevin-yc.ho@mediatek.com>");
MODULE_LICENSE("GPL");
