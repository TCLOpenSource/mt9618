// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_alsa_amp_rt9120.c -- amp rt9120 driver
 *
 * Copyright (c) 2020 MediaTek Inc.
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


#include "mtk_alsa_amp_rt9120.h"
#include "mtk_ini_parser.h"

u8 AmpInitTblRT9120[] = {
//DataLen	Address	DataN	DataN+1...
	2,	0x20,	0x01,	0x80,		//volume to 0db
	1,	0x05,	0x80,
	1,	0x04,	0x21,			//SDO output
	0x00,
};

u8 AmpSWResetTbl[] = {
	1,	0x04,	0x80,
	0x00,
};

u8 AmpMuteTbl[] = {
	2,	0x20,	0x07,	0xff,
	0x00,
};

u8 AmpUnMuteTbl[] = {
	2,	0x20,	0x01,	0x80,
	0x00,
};

#define VOL_LEN	2

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

static int amp_rt9120_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct amp_rt9120_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->initialized)
		dev_info(priv->dev, "amp 9120 initialized !!!!!!!!!\n");
	else
		dev_info(priv->dev, "amp 9120 not init !!!!!!!!!!!\n");

	return 0;
}

static const struct snd_soc_dai_ops amp_rt9120_dai_ops = {
	.startup	= amp_rt9120_startup,
};

static struct snd_soc_dai_driver amp_rt9120_dais[] = {
	{
		.name = "AMP_RT9120",
		.id = 0,
		.playback = {
			.stream_name = "AMP RT9120",
		},
		.ops = &amp_rt9120_dai_ops,
	},
};

static int amp_write(struct amp_rt9120_priv *priv, unsigned char *InitTable)
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
/* sw reset only, there is no hw reset pin */
static int amp_rt9120_reset(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (bEnable) {
		ret = amp_write(priv, AmpSWResetTbl);
		if (ret < 0)
			dev_err(priv->dev, "[AMP 9120]Reset fail\n");
	}

	return 0;
}

static int amp_rt9120_powerdown(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);

	gpiod_set_value(priv->amp_powerdown, bEnable);

	return 0;
}

static int amp_rt9120_mute(struct snd_soc_component *component, bool bEnable)
{
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);

	if (bEnable)
		amp_write(priv, AmpMuteTbl);
	else
		amp_write(priv, AmpUnMuteTbl);

	return 0;
}

static int amp_rt9120_init(struct amp_rt9120_priv *priv, unsigned char *InitTable)
{
	int ret = 0;

	ret = amp_write(priv, InitTable);
	if (ret < 0) {
		dev_err(priv->dev, "[AMP 9120]Init fail\n");
		priv->initialized = 0;
	} else {
		dev_info(priv->dev, "[AMP 9120]Init success\n");
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
	struct amp_rt9120_priv *priv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", priv->initialized);
}

static ssize_t amp_mute_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t retval;
	int mute_flag = 0;
	struct amp_rt9120_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	retval = kstrtoint(buf, 10, &mute_flag);
	if (retval == 0) {
		if ((mute_flag != 0) && (mute_flag != 1))
			dev_err(dev, "You should enter 0 or 1 !!!\n");
	}

	dev_info(dev, "set amp mute %d\n", mute_flag);

	amp_rt9120_mute(component, mute_flag);

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

static int amp_rt9120_probe(struct snd_soc_component *component)
{
	struct amp_rt9120_priv *priv =
				snd_soc_component_get_drvdata(component);
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

	amp_rt9120_powerdown(component, 0);
	mtk_amp_delaytask_us(12*1000);
	amp_rt9120_reset(component, 1);
	mtk_amp_delaytask_us(11*1000);

	do {
		mtk_amp_delaytask_us(2*1000);
		if (priv->init_table)
			ret = amp_rt9120_init(priv, priv->init_table);
		else
			ret = amp_rt9120_init(priv, AmpInitTblRT9120);
		retry_time -= 1;
	} while ((ret < 0) && retry_time);

	priv->component = component;

	return 0;
}

static void amp_rt9120_remove(struct snd_soc_component *component)
{
	amp_rt9120_powerdown(component, 1);
}

static int amp_rt9120_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int val = ucontrol->value.integer.value[0];

	if (val < 0)
		return -EINVAL;

	dev_info(priv->dev, "set amp mute %d\n", val);

	amp_rt9120_mute(component, val);
	mtk_amp_delaytask_us(1000);

	return 0;
}

static int amp_rt9120_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);
	int val[VOL_LEN] = {0};
	int ret = 0;

	ret = regmap_raw_read(priv->regmap, 0x20, val, 2);
	if (ret < 0)
		dev_err(priv->dev, "read amp vol fail\n");

	dev_info(priv->dev, "get amp mute %x %x\n", val[0], val[1]);

	ucontrol->value.integer.value[0] = val[0];

	return 0;
}

static int audio_amp_get_param(struct snd_kcontrol *kcontrol,
						unsigned int __user *data,
						unsigned int size)
{
	return 0;
}

static int audio_amp_put_param(struct snd_kcontrol *kcontrol,
						const unsigned int __user *data,
						unsigned int size)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct amp_rt9120_priv *priv = snd_soc_component_get_drvdata(component);
	int *param = priv->param;

	if (size > MTK_AMP_RT9120_PARAM_COUNT) {
		dev_err(priv->dev, "TLV kcontrol Size overflow\n");
		return -EINVAL;
	}

	if (!param)
		return -ENOMEM;

	if (copy_from_user(param, data, size * sizeof(int))) {
		dev_err(priv->dev, "copy_from_user fail\n");
		return -EFAULT;
	}

	return 0;
}

static const struct snd_kcontrol_new amp_rt9120_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP Mute", 0, amp_rt9120_mute_get,
				amp_rt9120_mute_put),
	SND_SOC_BYTES_TLV("AMP PARAMETER ARRAY", MTK_AMP_RT9120_PARAM_COUNT,
				audio_amp_get_param, audio_amp_put_param),
};

const struct snd_soc_component_driver amp_rt9120_component = {
	.name		= "amp-rt9120-platform",
	.probe		= amp_rt9120_probe,
	.remove		= amp_rt9120_remove,
	.controls	= amp_rt9120_controls,
	.num_controls	= ARRAY_SIZE(amp_rt9120_controls),
};

static const struct regmap_config rt9120_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int amp_rt9120_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct amp_rt9120_priv *priv;
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

	/* GPIO */
	priv->amp_powerdown = devm_gpiod_get(&client->dev, "amppowerdown",
						GPIOD_OUT_LOW);
	if (IS_ERR(priv->amp_powerdown)) {
		dev_err(&client->dev, "Unable to retrieve gpio amp_powerdown\n");
		return PTR_ERR(priv->amp_powerdown);
	}

	priv->regmap = devm_regmap_init_i2c(client, &rt9120_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->param = devm_kzalloc(dev,
		(sizeof(int) * MTK_AMP_RT9120_PARAM_COUNT), GFP_KERNEL);
	if (!priv->param)
		return -ENOMEM;

	ret = devm_snd_soc_register_component(&client->dev,
					&amp_rt9120_component,
					amp_rt9120_dais,
					ARRAY_SIZE(amp_rt9120_dais));
	if (ret) {
		dev_err(dev, "AMP soc_register_component fail %d\n", ret);
		return ret;
	}

	/* parse dts */
	ret = of_property_read_string(node, "mtk_ini", &name);
	if (ret)
		dev_err(&client->dev, "[AMP 9120]can't get mtk ini path\n");

	priv->mtk_ini = name;

	ret = sysfs_create_groups(&priv->dev->kobj, amp_attr_groups);
	if (ret)
		dev_err(&client->dev, "Audio amp Sysfs init fail\n");

	return 0;
}

static int amp_rt9120_i2c_remove(struct i2c_client *client)
{
	struct amp_rt9120_priv *priv = i2c_get_clientdata(client);

	if (priv->init_table)
		vfree(priv->init_table);

	sysfs_remove_groups(&priv->dev->kobj, amp_attr_groups);

	return 0;
}

#ifdef CONFIG_PM
int amp_rt9120_pm_suspend(struct device *dev)
{
	struct amp_rt9120_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (priv->suspended)
		return 0;

	amp_rt9120_mute(component, 1);
	amp_rt9120_powerdown(component, 1);

	priv->suspended = true;

	return 0;
}

int amp_rt9120_pm_resume(struct device *dev)
{
	struct amp_rt9120_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (!priv->suspended)
		return 0;

	amp_rt9120_probe(component);

	priv->suspended = false;

	return 0;
}

static const struct dev_pm_ops amp_rt9120_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amp_rt9120_pm_suspend,
			   amp_rt9120_pm_resume)
};
#endif

static const struct of_device_id mtk_amp_dt_match[] = {
	{ .compatible = "mediatek, rt9120", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_amp_dt_match);

static const struct i2c_device_id rt9120_i2c_id[] = {
	{ "richtek, rt9120", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt9120_i2c_id);

static struct i2c_driver amp_rt9120_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "richtek-rt9120",
		.of_match_table	= of_match_ptr(mtk_amp_dt_match),
#ifdef CONFIG_PM
		.pm	= &amp_rt9120_pm_ops,
#endif
	},
	.probe		= amp_rt9120_i2c_probe,
	.remove		= amp_rt9120_i2c_remove,
	.id_table	= rt9120_i2c_id,
};

module_i2c_driver(amp_rt9120_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC AMP RT9120 platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("AMP RT9120 driver");
