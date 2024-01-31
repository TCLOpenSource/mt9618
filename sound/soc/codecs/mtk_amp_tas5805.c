// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * mtk_amp_tas5805.c -- TI AMP tas5805 driver
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

#include "mtk_amp_tas5805.h"
#include "mtk_ini_parser.h"

u8 AmpInitTblTAS5805[] = {
//DataLen	Address	DataN	DataN+1...
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x03,	0x02,
	1,	0x01,	0x11,
	1,	0x00,	0x00,
	1,	0x00,	0x00,
	1,	0x00,	0x00,
	1,	0x00,	0x00,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x03,	0x02,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x03,	0x00,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x46,	0x11,
	1,	0x03,	0x02,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x78,	0x80,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x61,	0x0b,
	1,	0x60,	0x01,
	1,	0x7d,	0x11,
	1,	0x7e,	0xff,
	1,	0x00,	0x01,
	1,	0x51,	0x05,
	1,	0x00,	0x00,
	1,	0x02,	0x11,
	1,	0x53,	0x20,
	1,	0x54,	0x00,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x66,	0x84,
	1,	0x7f,	0x8c,
	1,	0x00,	0x2a,
	1,	0x24,	0x01,
	1,	0x25,	0x33,
	1,	0x26,	0x0c,
	1,	0x27,	0xf5,
	1,	0x28,	0x01,
	1,	0x29,	0x33,
	1,	0x2a,	0x0c,
	1,	0x2b,	0xf5,
	1,	0x30,	0x00,
	1,	0x31,	0xe2,
	1,	0x32,	0xc4,
	1,	0x33,	0x6b,
	1,	0x00,	0x2c,
	1,	0x5c,	0x00,
	1,	0x5d,	0x00,
	1,	0x5e,	0xae,
	1,	0x5f,	0xc3,
	1,	0x60,	0x01,
	1,	0x61,	0x9a,
	1,	0x62,	0xf7,
	1,	0x63,	0x20,
	1,	0x64,	0x08,
	1,	0x65,	0x13,
	1,	0x66,	0x85,
	1,	0x67,	0x62,
	1,	0x68,	0xc0,
	1,	0x69,	0x00,
	1,	0x6a,	0x00,
	1,	0x6b,	0x00,
	1,	0x6c,	0x04,
	1,	0x6d,	0xc1,
	1,	0x6e,	0xff,
	1,	0x6f,	0x93,
	1,	0x74,	0x00,
	1,	0x75,	0x80,
	1,	0x76,	0x00,
	1,	0x77,	0x00,
	1,	0x00,	0x2d,
	1,	0x18,	0x7b,
	1,	0x19,	0x3e,
	1,	0x1a,	0x00,
	1,	0x1b,	0x6d,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x30,	0x00,
	1,	0x4c,	0x30,
	1,	0x03,	0x03,
	1,	0x00,	0x00,
	1,	0x7f,	0x00,
	1,	0x78,	0x80,
	0x00,
};

u8 AmpDeInitTblTAS5805[] = {
	1,	0x00,	0x00,	// w 58 00 00 #Go to page 0
	1,	0x7f,	0x00,	// w 58 7f 00 #Change the book to 0x00
	1,	0x03,	0x02,	// w 58 03 02 #Change the device into Hiz Mode
	1,	0x03,	0x00,	// w 58 03 00 #Change the device into Deep Sleep Mode
	1,	0x00,	0x00,
};

u8 AmpMuteTbl[] = {
	1,	0x03,	0x0b,
	0x00,
};

u8 AmpUnMuteTbl[] = {
	1,	0x03,	0x03,
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

static int amp_tas5805_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct amp_tas5805_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->initialized)
		dev_info(priv->dev, "amp 5805 initialized !!!!!!!!!\n");
	else
		dev_info(priv->dev, "amp 5805 not init !!!!!!!!!!!\n");

	return 0;
}

static const struct snd_soc_dai_ops amp_tas5805_dai_ops = {
	.startup	= amp_tas5805_startup,
};

static struct snd_soc_dai_driver amp_tas5805_dais[] = {
	{
		.name = "AMP_TAS5805",
		.id = 0,
		.playback = {
			.stream_name = "AMP TAS5805",
		},
		.ops = &amp_tas5805_dai_ops,
	},
};

static struct snd_soc_dai_driver amp_tas5805_1_dais[] = {
	{
		.name = "AMP_TAS5805_1",
		.id = 1,
		.playback = {
			.stream_name = "AMP TAS5805_1",
		},
	},
};

static struct snd_soc_dai_driver amp_tas5805_2_dais[] = {
	{
		.name = "AMP_TAS5805_2",
		.id = 2,
		.playback = {
			.stream_name = "AMP TAS5805_2",
		},
	},
};

static struct snd_soc_dai_driver amp_tas5805_3_dais[] = {
	{
		.name = "AMP_TAS5805_3",
		.id = 3,
		.playback = {
			.stream_name = "AMP TAS5805_3",
		},
	},
};

static struct snd_soc_dai_driver amp_tas5805_4_dais[] = {
	{
		.name = "AMP_TAS5805_4",
		.id = 4,
		.playback = {
			.stream_name = "AMP TAS5805_4",
		},
	},
};

static int amp_write(struct amp_tas5805_priv *priv, unsigned char *InitTable)
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

static int amp_tas5805_powerdown(struct snd_soc_component *component, bool bEnable)
{
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);

	gpiod_set_value(priv->amp_powerdown, bEnable);

	return 0;
}

static int amp_tas5805_mute(struct snd_soc_component *component, bool bEnable)
{
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);

	if (bEnable)
		amp_write(priv, AmpMuteTbl);
	else
		amp_write(priv, AmpUnMuteTbl);

	return 0;
}

static int amp_tas5805_init(struct amp_tas5805_priv *priv, unsigned char *InitTable)
{
	int ret = 0;

	ret = amp_write(priv, InitTable);
	if (ret < 0) {
		dev_err(priv->dev, "[AMP 5805]Init fail\n");
		priv->initialized = 0;
	} else {
		dev_info(priv->dev, "[AMP 5805]Init success\n");
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
	struct amp_tas5805_priv *priv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", priv->initialized);
}

static ssize_t amp_mute_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t retval;
	int mute_flag = 0;
	struct amp_tas5805_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	retval = kstrtoint(buf, 10, &mute_flag);
	if (retval == 0) {
		if ((mute_flag != 0) && (mute_flag != 1))
			dev_err(dev, "You should enter 0 or 1 !!!\n");
	}

	dev_info(dev, "set amp mute %d\n", mute_flag);

	amp_tas5805_mute(component, mute_flag);

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

static int amp_tas5805_probe(struct snd_soc_component *component)
{
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;
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
		amp_tas5805_powerdown(component, 1);
		mtk_amp_delaytask_us(1000);
		amp_tas5805_powerdown(component, 0);
		mtk_amp_delaytask_us(20*1000);
	}

	if (priv->init_table)
		ret = amp_tas5805_init(priv, priv->init_table);
	else
		ret = amp_tas5805_init(priv, AmpInitTblTAS5805);
	if (ret < 0)
		dev_err(priv->dev, "[AMP 5805] init fail\n");

	priv->component = component;

	return 0;
}

static void amp_tas5805_remove(struct snd_soc_component *component)
{
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = amp_write(priv, AmpDeInitTblTAS5805);
	if (ret < 0)
		dev_err(priv->dev, "[AMP 5805] deinit fail\n");

	amp_tas5805_powerdown(component, 1);
	mtk_amp_delaytask_us(10*000);

}

static int amp_tas5805_mute_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);
	int val = ucontrol->value.integer.value[0];

	if (val < 0)
		return -EINVAL;

	dev_info(priv->dev, "set amp mute %d\n", val);

	amp_tas5805_mute(component, val);

	return 0;
}

static int amp_tas5805_mute_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_tas5805_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(priv->regmap, TAS5805_VOLUME_CTRL_REG, &val);

	dev_info(priv->dev, "get amp mute %d\n", val);
	if (ret < 0)
		dev_err(priv->dev, "fail to read mute register %d\n", ret);

	ucontrol->value.integer.value[0] = (val & 0x08) >> 3;

	return 0;
}

static const struct snd_kcontrol_new amp_tas5805_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP Mute", 0, amp_tas5805_mute_get, amp_tas5805_mute_put),
};

const struct snd_soc_component_driver amp_tas5805_component = {
	.name		= "amp-tas5805-0",
	.probe		= amp_tas5805_probe,
	.remove		= amp_tas5805_remove,
	.controls	= amp_tas5805_controls,
	.num_controls	= ARRAY_SIZE(amp_tas5805_controls),
};

const struct snd_soc_component_driver amp_tas5805_1_component = {
	.name		= "amp-tas5805-1",
	.probe		= amp_tas5805_probe,
	.remove		= amp_tas5805_remove,
};

const struct snd_soc_component_driver amp_tas5805_2_component = {
	.name		= "amp-tas5805-2",
	.probe		= amp_tas5805_probe,
	.remove		= amp_tas5805_remove,
};

const struct snd_soc_component_driver amp_tas5805_3_component = {
	.name		= "amp-tas5805-3",
	.probe		= amp_tas5805_probe,
	.remove		= amp_tas5805_remove,
};

const struct snd_soc_component_driver amp_tas5805_4_component = {
	.name		= "amp-tas5805-4",
	.probe		= amp_tas5805_probe,
	.remove		= amp_tas5805_remove,
};

static const struct regmap_config tas5805_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x78,
	.cache_type = REGCACHE_RBTREE,
};

static void amp_register_component(struct amp_tas5805_priv *priv)
{
	int ret;

	if (priv->type == 0) {				/* master */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_tas5805_component,
						amp_tas5805_dais,
						ARRAY_SIZE(amp_tas5805_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component fail %d\n", ret);
	} else if (priv->type == 1) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_tas5805_1_component,
						amp_tas5805_1_dais,
						ARRAY_SIZE(amp_tas5805_1_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 1 fail %d\n", ret);
	} else if (priv->type == 2) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_tas5805_2_component,
						amp_tas5805_2_dais,
						ARRAY_SIZE(amp_tas5805_2_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 2 fail %d\n", ret);
	} else if (priv->type == 3) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_tas5805_3_component,
						amp_tas5805_3_dais,
						ARRAY_SIZE(amp_tas5805_3_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 3 fail %d\n", ret);
	} else if (priv->type == 4) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_tas5805_4_component,
						amp_tas5805_4_dais,
						ARRAY_SIZE(amp_tas5805_4_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 4 fail %d\n", ret);
	}
}

static int amp_tas5805_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct amp_tas5805_priv *priv;
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
		dev_info(&client->dev, "[AMP 5805]can't get type\n");
		priv->type = 0;
	}

	/* GPIO : master get gpio handler */
	if (priv->type == 0) {
		priv->amp_powerdown = devm_gpiod_get(&client->dev, "amppowerdown", GPIOD_OUT_LOW);
		if (IS_ERR(priv->amp_powerdown)) {
			dev_err(&client->dev, "Unable to retrieve gpio amp_powerdown\n");
			return PTR_ERR(priv->amp_powerdown);
		}
	}

	priv->regmap = devm_regmap_init_i2c(client, &tas5805_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	amp_register_component(priv);

	/* parse dts */
	ret = of_property_read_string(node, "mtk_ini", &name);
	if (ret)
		dev_err(&client->dev, "[AMP 5805]can't get mtk ini path\n");

	priv->mtk_ini = name;

	/* Create device attribute files */
	ret = sysfs_create_groups(&priv->dev->kobj, amp_attr_groups);
	if (ret)
		dev_err(priv->dev, "Audio amp Sysfs init fail\n");

	return 0;
}

static int amp_tas5805_i2c_remove(struct i2c_client *client)
{
	struct amp_tas5805_priv *priv = i2c_get_clientdata(client);

	if (priv->init_table)
		vfree(priv->init_table);

	sysfs_remove_groups(&priv->dev->kobj, amp_attr_groups);

	return 0;
}

#ifdef CONFIG_PM
int amp_tas5805_pm_suspend(struct device *dev)
{
	struct amp_tas5805_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (priv->suspended)
		return 0;

	amp_tas5805_mute(component, 1);
	amp_tas5805_powerdown(component, 1);

	priv->suspended = true;

	return 0;
}

int amp_tas5805_pm_resume(struct device *dev)
{
	struct amp_tas5805_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (!priv->suspended)
		return 0;

	amp_tas5805_probe(component);

	priv->suspended = false;

	return 0;
}

static const struct dev_pm_ops amp_tas5805_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amp_tas5805_pm_suspend, amp_tas5805_pm_resume)
};
#endif

static const struct of_device_id mtk_amp_dt_match[] = {
	{ .compatible = "ti, tas5805", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_amp_dt_match);

static const struct i2c_device_id tas5805_i2c_id[] = {
	{ "tas5805", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5805_i2c_id);

static struct i2c_driver amp_tas5805_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tas5805",
		.of_match_table	= of_match_ptr(mtk_amp_dt_match),
#ifdef CONFIG_PM
		.pm	= &amp_tas5805_pm_ops,
#endif
	},
	.probe		= amp_tas5805_i2c_probe,
	.remove		= amp_tas5805_i2c_remove,
	.id_table	= tas5805_i2c_id,
};

module_i2c_driver(amp_tas5805_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC AMP TAS5805 platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("AMP TAS5805 driver");
