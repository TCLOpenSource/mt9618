// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * mtk_alsa_amp_rt9119.c -- amp rt9119 driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <sound/soc.h>
#include <sound/control.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/vmalloc.h>


#include "mtk_alsa_amp_rt9119.h"
#include "mtk_ini_parser.h"

u8 AmpInitTblRT9119[] = {
//DataLen	Address	DataN	DataN+1...
	/* don't need to do sw reset here, hw reset is applied */
	1,	0x02,	0x06,
	1,	0x0E,	0x07,
	1,	0xD8,	0x01,
	1,	0x10,	0x21,
	1,	0x06,	0x07,
	1,	0xE0,	0x31,
	2,	0x07,	0x01,	0x80,	//volume to 0db
	1,	0x05,	0x01,
	0x00,
};

u8 AmpMuteTbl[] = {
	2,	0x07,	0x07,	0xff,
	0x00,
};

u8 AmpUnMuteTbl[] = {
	2,	0x07,	0x01,	0x80,
	0x00,
};

static void mtk_amp_delaytask_us(unsigned int u32Us)
{
	/*
	 * Refer to Documentation/timers/timers-howto.txt
	 * Non-atomic context
	 *		< 10us	:	udelay()
	 *	10us ~ 20ms	:	usleep_range()
	 *		> 10ms	:	msleep() or msleep_interruptible()
	 */

	if (u32Us < 10UL)
		udelay(u32Us);
	else if (u32Us < 20UL * 1000UL)
		usleep_range(u32Us, u32Us + 1);
	else
		msleep_interruptible((unsigned int)(u32Us / 1000UL));
}

static int amp_rt9119_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct amp_rt9119_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->initialized)
		dev_info(priv->dev, "amp 9119 initialized !!!!!!!!!\n");
	else
		dev_info(priv->dev, "amp 9119 not init !!!!!!!!!!!\n");

	return 0;
}

static const struct snd_soc_dai_ops amp_rt9119_dai_ops = {
	.startup	= amp_rt9119_startup,
};

static struct snd_soc_dai_driver amp_rt9119_dais[] = {
	{
		.name = "AMP_RT9119",
		.id = 0,
		.playback = {
			.stream_name = "AMP RT9119",
		},
		.ops = &amp_rt9119_dai_ops,
	},
};

static struct snd_soc_dai_driver amp_rt9119_1_dais[] = {
	{
		.name = "AMP_RT9119_1",
		.id = 1,
		.playback = {
			.stream_name = "AMP RT9119_1",
		},
	},
};

static struct snd_soc_dai_driver amp_rt9119_2_dais[] = {
	{
		.name = "AMP_RT9119_2",
		.id = 2,
		.playback = {
			.stream_name = "AMP RT9119_2",
		},
	},
};

static struct snd_soc_dai_driver amp_rt9119_3_dais[] = {
	{
		.name = "AMP_RT9119_3",
		.id = 3,
		.playback = {
			.stream_name = "AMP RT9119_3",
		},
	},
};

static struct snd_soc_dai_driver amp_rt9119_4_dais[] = {
	{
		.name = "AMP_RT9119_4",
		.id = 4,
		.playback = {
			.stream_name = "AMP RT9119_4",
		},
	},
};

static int amp_write(struct amp_rt9119_priv *priv, unsigned char *InitTable)
{
	u8 *pu8Str = InitTable;
	u8 *buf;
	u8 reg;
	u8 size;
	int ret;

	do {
		size = *pu8Str;
		pu8Str++;

		reg = *pu8Str;
		pu8Str++;

		buf = pu8Str;
		ret = regmap_raw_write(priv->regmap, reg, buf, size);
		if (ret < 0) {
			dev_err(priv->dev, "i2c write fail\n");
			return -1;
		}

		pu8Str = pu8Str + size;
	} while (*pu8Str != 0);

	return 0;
}

static int amp_rt9119_reset(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9119_priv *priv = snd_soc_component_get_drvdata(component);

	if (priv->amp_reset)
		gpiod_set_value(priv->amp_reset, bEnable);

	return 0;
}

static int amp_rt9119_powerdown(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9119_priv *priv = snd_soc_component_get_drvdata(component);

	if (priv->amp_powerdown)
		gpiod_set_value(priv->amp_powerdown, bEnable);

	return 0;
}

static int amp_rt9119_mute(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9119_priv *priv = snd_soc_component_get_drvdata(component);

	if (bEnable)
		amp_write(priv, AmpMuteTbl);
	else
		amp_write(priv, AmpUnMuteTbl);

	return 0;
}

static int amp_rt9119_init(struct amp_rt9119_priv *priv, unsigned char *InitTable)
{
	int ret = 0;

	ret = amp_write(priv, InitTable);
	if (ret < 0) {
		dev_err(priv->dev, "[AMP 9119]Init fail\n");
		priv->initialized = 0;
	} else {
		dev_info(priv->dev, "[AMP 9119]Init success %x\n", priv->client->addr);
		priv->initialized = 1;
	}

	return ret;
}

//[Sysfs] HELP command
static ssize_t help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Debug Help:\n"
		       "- amp_status\n"
		       "             <R>: Read amp status.\n"
		       "- amp_mute   <W>: write(1)/Clear(0) amp mute status.\n"
		       );
}

static ssize_t amp_status_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct amp_rt9119_priv *priv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", priv->initialized);
}

static ssize_t amp_mute_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t retval;
	int mute_flag = 0;
	struct amp_rt9119_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	retval = kstrtoint(buf, 10, &mute_flag);
	if (retval == 0) {
		if ((mute_flag != 0) && (mute_flag != 1))
			dev_err(dev, "You should enter 0 or 1 !!!\n");
	}

	dev_info(dev, "set amp mute %d\n", mute_flag);

	amp_rt9119_mute(component, mute_flag);

	return (retval < 0) ? retval : count;
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RO(amp_status);
static DEVICE_ATTR_WO(amp_mute);

static struct attribute *amp_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_amp_status.attr,
	&dev_attr_amp_mute.attr,
	NULL,
};

static const struct attribute_group amp_attr_group = {
	.name = "mtk_dbg",
	.attrs = amp_attrs,
};

static const struct attribute_group *amp_attr_groups[] = {
	&amp_attr_group,
	NULL,
};

static int amp_rt9119_probe(struct snd_soc_component *component)
{
	struct amp_rt9119_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;
	int retry_time = 5;
	const struct firmware *fw = NULL;

	if (priv->mtk_ini && (priv->init_table == NULL)) {
		ret = request_firmware_direct(&fw, priv->mtk_ini, priv->dev);
		if (ret < 0)
			dev_info(priv->dev, "Err! load ext ini fail, fallback to internal init\n");
		else {
			priv->init_table = mtk_parser(fw);
			if (priv->init_table)
				dev_info(priv->dev, "load ext ini success\n");
		}
		release_firmware(fw);
	}

	if (priv->type == 0) {
		amp_rt9119_reset(component, 1);
		mtk_amp_delaytask_us(1*1000);
		amp_rt9119_powerdown(component, 0);
		mtk_amp_delaytask_us(1*1000);
		amp_rt9119_reset(component, 0);
		mtk_amp_delaytask_us(11*1000);
	}

	do {
		mtk_amp_delaytask_us(1*1000);
		if (priv->init_table)
			ret = amp_rt9119_init(priv, priv->init_table);
		else
			ret = amp_rt9119_init(priv, AmpInitTblRT9119);
		retry_time -= 1;
	} while ((ret < 0) && retry_time);

	priv->component = component;

	return 0;
}

static void amp_rt9119_remove(struct snd_soc_component *component)
{
	amp_rt9119_powerdown(component, 1);
	mtk_amp_delaytask_us(15*1000);
	amp_rt9119_reset(component, 1);
	mtk_amp_delaytask_us(1000);
}

const struct snd_soc_component_driver amp_rt9119_component = {
	.name		= "amp-rt9119-0",
	.probe		= amp_rt9119_probe,
	.remove		= amp_rt9119_remove,
};

const struct snd_soc_component_driver amp_rt9119_1_component = {
	.name		= "amp-rt9119-1",
	.probe		= amp_rt9119_probe,
	.remove		= amp_rt9119_remove,
};

const struct snd_soc_component_driver amp_rt9119_2_component = {
	.name		= "amp-rt9119-2",
	.probe		= amp_rt9119_probe,
	.remove		= amp_rt9119_remove,
};

const struct snd_soc_component_driver amp_rt9119_3_component = {
	.name		= "amp-rt9119-3",
	.probe		= amp_rt9119_probe,
	.remove		= amp_rt9119_remove,
};

const struct snd_soc_component_driver amp_rt9119_4_component = {
	.name		= "amp-rt9119-4",
	.probe		= amp_rt9119_probe,
	.remove		= amp_rt9119_remove,
};

static const struct regmap_config rt9119_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static void amp_register_component(struct amp_rt9119_priv *priv)
{
	int ret;

	if (priv->type == 0) {				/* master */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_rt9119_component,
						amp_rt9119_dais,
						ARRAY_SIZE(amp_rt9119_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component fail %d\n", ret);
	} else if (priv->type == 1) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_rt9119_1_component,
						amp_rt9119_1_dais,
						ARRAY_SIZE(amp_rt9119_1_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 1 fail %d\n", ret);
	} else if (priv->type == 2) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_rt9119_2_component,
						amp_rt9119_2_dais,
						ARRAY_SIZE(amp_rt9119_2_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 2 fail %d\n", ret);
	} else if (priv->type == 3) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_rt9119_3_component,
						amp_rt9119_3_dais,
						ARRAY_SIZE(amp_rt9119_3_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 3 fail %d\n", ret);
	} else if (priv->type == 4) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_rt9119_4_component,
						amp_rt9119_4_dais,
						ARRAY_SIZE(amp_rt9119_4_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 4 fail %d\n", ret);
	}
}

static int amp_rt9119_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct amp_rt9119_priv *priv;
	struct device *dev = &client->dev;
	struct device_node *node;
	int ret;
	const char *name = NULL;

	node = dev->of_node;
	if (!node)
		return -ENODEV;

	if (of_device_is_available(node) == 0)
		return 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &client->dev;
	priv->client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	i2c_set_clientdata(client, priv);

	/* parse dts */
	/*
	 *	get type :
	 *	type = 0 : master
	 *	type = 1/2/3/4 : slave
	 *	if can't get type : means master
	 */
	ret = of_property_read_u32(node, "type", &priv->type);
	if (ret) {
		dev_info(&client->dev, "[AMP 9119]can't get type\n");
		priv->type = 0;
	}

	/* GPIO : master get gpio handler */
	if (priv->type == 0) {
		priv->amp_powerdown = devm_gpiod_get(&client->dev, "amppowerdown", GPIOD_OUT_LOW);
		if (IS_ERR(priv->amp_powerdown)) {
			dev_err(&client->dev, "Unable to retrieve gpio amp_powerdown\n");
			return PTR_ERR(priv->amp_powerdown);
		}

		priv->amp_reset = devm_gpiod_get(&client->dev, "ampreset", GPIOD_OUT_LOW);
		if (IS_ERR(priv->amp_reset)) {
			dev_err(&client->dev, "Unable to retrieve gpio amp_reset\n");
			return PTR_ERR(priv->amp_reset);
		}
	}

	priv->regmap = devm_regmap_init_i2c(client, &rt9119_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	amp_register_component(priv);

	/* parse dts */
	ret = of_property_read_string(node, "mtk_ini", &name);
	if (ret)
		dev_err(&client->dev, "[AMP 9119]can't get mtk ini path\n");

	priv->mtk_ini = name;

	ret = sysfs_create_groups(&priv->dev->kobj, amp_attr_groups);
	if (ret)
		dev_err(&client->dev, "Audio amp Sysfs init fail\n");

	return 0;
}

static int amp_rt9119_i2c_remove(struct i2c_client *client)
{
	struct amp_rt9119_priv *priv = i2c_get_clientdata(client);

	if (priv->init_table)
		vfree(priv->init_table);

	sysfs_remove_groups(&priv->dev->kobj, amp_attr_groups);

	return 0;
}

#ifdef CONFIG_PM
int amp_rt9119_pm_suspend(struct device *dev)
{
	struct amp_rt9119_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (priv->suspended)
		return 0;

	amp_rt9119_mute(component, 1);
	amp_rt9119_powerdown(component, 1);

	priv->suspended = true;

	return 0;
}

int amp_rt9119_pm_resume(struct device *dev)
{
	struct amp_rt9119_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (!priv->suspended)
		return 0;

	amp_rt9119_probe(component);

	priv->suspended = false;

	return 0;
}

static const struct dev_pm_ops amp_rt9119_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amp_rt9119_pm_suspend, amp_rt9119_pm_resume)
};
#endif

static const struct of_device_id mtk_amp_dt_match[] = {
	{ .compatible = "mediatek, rt9119", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_amp_dt_match);

static const struct i2c_device_id rt9119_i2c_id[] = {
	{ "richtek, rt9119", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt9119_i2c_id);

static struct i2c_driver amp_rt9119_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "richtek-rt9119",
		.of_match_table	= of_match_ptr(mtk_amp_dt_match),
#ifdef CONFIG_PM
		.pm	= &amp_rt9119_pm_ops,
#endif
	},
	.probe		= amp_rt9119_i2c_probe,
	.remove		= amp_rt9119_i2c_remove,
	.id_table	= rt9119_i2c_id,
};

module_i2c_driver(amp_rt9119_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC AMP RT9119 platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("AMP RT9119 driver");
