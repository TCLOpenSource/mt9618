// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * mtk_amp_ad82089.c -- ESMT AMP ad82089 driver
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

#include "mtk_amp_ad82089.h"
#include "mtk_ini_parser.h"

// you can read out the register and ram data, and check them.
#define AD82089_REG_RAM_CHECK

u8 AmpInitTblAD82089[] = {
//DataLen	Address	DataN	DataN+1...
	/* don't need to do sw reset here, hw reset is applied */
	1,	0x00,	0x04,//##State_Control_1
	1,	0x01,	0x81,//##State_Control_2
	1,	0x02,	0x40,//##State_Control_3 ; default enable SW mute
	1,	0x03,	0x18,//##Master_volume_control ; default set to +0dB
	1,	0x04,	0x18,//##Channel_1_volume_control
	1,	0x05,	0x18,//##Channel_2_volume_control
	1,	0x06,	0x18,//##Channel_3_volume_control
	1,	0x07,	0x18,//##Channel_4_volume_control
	1,	0x08,	0x18,//##Channel_5_volume_control
	1,	0x09,	0x18,//##Channel_6_volume_control
	1,	0x0a,	0x10,//##Bass_Tone_Boost_and_Cut
	1,	0x0b,	0x10,//##treble_Tone_Boost_and_Cut
	1,	0x0c,	0x90,//##State_Control_4
	1,	0x0d,	0x00,//##Channel_1_configuration_registers
	1,	0x0e,	0x00,//##Channel_2_configuration_registers
	1,	0x0f,	0x00,//##Channel_3_configuration_registers
	1,	0x10,	0x00,//##Channel_4_configuration_registers
	1,	0x11,	0x00,//##Channel_5_configuration_registers
	1,	0x12,	0x00,//##Channel_6_configuration_registers
	1,	0x13,	0x00,//##Channel_7_configuration_registers
	1,	0x14,	0x00,//##Channel_8_configuration_registers
	1,	0x15,	0x6a,//##DRC1_limiter_attack/release_rate
	1,	0x16,	0x6a,//##DRC2_limiter_attack/release_rate
	1,	0x17,	0x6a,//##DRC3_limiter_attack/release_rate
	1,	0x18,	0x6a,//##DRC4_limiter_attack/release_rate
	1,	0x19,	0x06,//##Error_Delay
	1,	0x1a,	0x32,//##State_Control_5
	1,	0x1b,	0x01,//##HVUV_selection
	1,	0x1c,	0x00,//##State_Control_6
	1,	0x30,	0x00,//##Power_Stage_Status(Read_only)
	1,	0x31,	0x00,//##PWM_Output_Control
	1,	0x32,	0x00,//##Test_Mode_Control_Reg.
	1,	0x33,	0x6d,//##Qua-Ternary/Ternary_Switch_Level
	1,	0x37,	0x52,//##Device_ID_register
	0x00,
};

u8 AmpSWResetTbl[] = {
	1,	0x1a,	0x12,
	0x00,
};

u8 AmpNormalTbl[] = {
	1,	0x1a,	0x32,
	0x00,
};

u8 AmpMuteTbl[] = {
	1,	0x02,	0x7f,
	0x00,
};

u8 AmpUnMuteTbl[] = {
	1,	0x02,	0x00,
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

static int amp_ad82089_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct amp_ad82089_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->initialized)
		dev_info(priv->dev, "amp 82089 initialized !!!!!!!!!\n");
	else
		dev_info(priv->dev, "amp 82089 not init !!!!!!!!!!!\n");

	return 0;
}

static const struct snd_soc_dai_ops amp_ad82089_dai_ops = {
	.startup	= amp_ad82089_startup,
};

static struct snd_soc_dai_driver amp_ad82089_dais[] = {
	{
		.name = "AMP_AD82089",
		.id = 0,
		.playback = {
			.stream_name = "AMP AD82089",
		},
		.ops = &amp_ad82089_dai_ops,
	},
};

static struct snd_soc_dai_driver amp_ad82089_1_dais[] = {
	{
		.name = "AMP_AD82089_1",
		.id = 1,
		.playback = {
			.stream_name = "AMP AD82089_1",
		},
	},
};

static struct snd_soc_dai_driver amp_ad82089_2_dais[] = {
	{
		.name = "AMP_AD82089_2",
		.id = 2,
		.playback = {
			.stream_name = "AMP AD82089_2",
		},
	},
};

static struct snd_soc_dai_driver amp_ad82089_3_dais[] = {
	{
		.name = "AMP_AD82089_3",
		.id = 3,
		.playback = {
			.stream_name = "AMP AD82089_3",
		},
	},
};

static struct snd_soc_dai_driver amp_ad82089_4_dais[] = {
	{
		.name = "AMP_AD82089_4",
		.id = 4,
		.playback = {
			.stream_name = "AMP AD82089_4",
		},
	},
};

static int amp_write(struct amp_ad82089_priv *priv, unsigned char *InitTable)
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

static int amp_ad82089_powerdown(struct snd_soc_component *component, bool bEnable)
{
	struct amp_ad82089_priv *priv = snd_soc_component_get_drvdata(component);

	gpiod_set_value(priv->amp_powerdown, bEnable);

	return 0;
}

static int amp_ad82089_mute(struct snd_soc_component *component, bool bEnable)
{
	struct amp_ad82089_priv *priv = snd_soc_component_get_drvdata(component);

	if (bEnable)
		amp_write(priv, AmpMuteTbl);
	else
		amp_write(priv, AmpUnMuteTbl);

	return 0;
}

static int amp_ad82089_init(struct amp_ad82089_priv *priv, unsigned char *InitTable)
{
	int ret = 0;

	ret = amp_write(priv, InitTable);
	if (ret < 0) {
		dev_err(priv->dev, "[AMP 82089]Init fail\n");
		priv->initialized = 0;
	} else {
		dev_info(priv->dev, "[AMP 82089]Init success\n");
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
	struct amp_ad82089_priv *priv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", priv->initialized);
}

static ssize_t amp_mute_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t retval;
	int mute_flag = 0;
	struct amp_ad82089_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	retval = kstrtoint(buf, 10, &mute_flag);
	if (retval == 0) {
		if ((mute_flag != 0) && (mute_flag != 1))
			dev_err(dev, "You should enter 0 or 1 !!!\n");
	}

	dev_info(dev, "set amp mute %d\n", mute_flag);

	amp_ad82089_mute(component, mute_flag);

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

static int amp_ad82089_probe(struct snd_soc_component *component)
{
	struct amp_ad82089_priv *priv =	snd_soc_component_get_drvdata(component);
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
		//wait t5 min time = 10ms
		mtk_amp_delaytask_us(10*1000);
		//pull high amp PD pin
		amp_ad82089_powerdown(component, 0);
		//wait t9 min time = 20ms then we can start I2C command
		mtk_amp_delaytask_us(20*1000);
	}
	//software reset amp, if no have hardware reset pin(24pin's ad82088)
	amp_write(priv, AmpSWResetTbl);
	mtk_amp_delaytask_us(5*1000);
	//normal operation
	amp_write(priv, AmpNormalTbl);
	mtk_amp_delaytask_us(20*1000);
	//set all channel mute during writing all of registers and RAM
	amp_write(priv, AmpMuteTbl);

	if (priv->init_table)
		ret = amp_ad82089_init(priv, priv->init_table);
	else
		ret = amp_ad82089_init(priv, AmpInitTblAD82089);
	if (ret < 0)
		dev_err(priv->dev, "[AMP 82089] init fail\n");

	mtk_amp_delaytask_us(2*1000);
	//set all channel un-mute
	amp_write(priv, AmpUnMuteTbl);

	priv->component = component;

	return 0;
}

static void amp_ad82089_remove(struct snd_soc_component *component)
{
	amp_ad82089_powerdown(component, 1);
}

static int amp_ad82089_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_ad82089_priv *priv = snd_soc_component_get_drvdata(component);
	int val = ucontrol->value.integer.value[0];

	if (val < 0)
		return -EINVAL;

	dev_info(priv->dev, "set amp mute %d\n", val);

	amp_ad82089_mute(component, val);

	return 0;
}

static int amp_ad82089_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_ad82089_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(priv->regmap, AD82089_STATE_CTRL3_REG, &val);

	dev_info(priv->dev, "get amp mute %d\n", val);
	if (ret < 0)
		dev_err(priv->dev, "fail to read mute register %d\n", ret);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static const struct snd_kcontrol_new amp_ad82089_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP Mute", 0, amp_ad82089_mute_get,
				amp_ad82089_mute_put),
};

const struct snd_soc_component_driver amp_ad82089_component = {
	.name		= "amp-ad82089-0",
	.probe		= amp_ad82089_probe,
	.remove		= amp_ad82089_remove,
	.controls	= amp_ad82089_controls,
	.num_controls	= ARRAY_SIZE(amp_ad82089_controls),
};

const struct snd_soc_component_driver amp_ad82089_1_component = {
	.name		= "amp-ad82089-1",
	.probe		= amp_ad82089_probe,
	.remove		= amp_ad82089_remove,
};

const struct snd_soc_component_driver amp_ad82089_2_component = {
	.name		= "amp-ad82089-2",
	.probe		= amp_ad82089_probe,
	.remove		= amp_ad82089_remove,
};

const struct snd_soc_component_driver amp_ad82089_3_component = {
	.name		= "amp-ad82089-3",
	.probe		= amp_ad82089_probe,
	.remove		= amp_ad82089_remove,
};

const struct snd_soc_component_driver amp_ad82089_4_component = {
	.name		= "amp-ad82089-4",
	.probe		= amp_ad82089_probe,
	.remove		= amp_ad82089_remove,
};

static bool ad82089_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef	AD82088_REG_RAM_CHECK
	if (reg < AD82089_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case AD82089_FAULT_REG:
	case AD82089_STATE_CTRL1_REG:
	case AD82089_STATE_CTRL2_REG:
	case AD82089_STATE_CTRL3_REG:
	case AD82089_STATE_CTRL5_REG:
		return true;
	default:
		return false;
	}
#endif
}

static const struct regmap_config ad82089_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = AD82089_MAX_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = ad82089_is_volatile_reg,
};

static void amp_register_component(struct amp_ad82089_priv *priv)
{
	int ret;

	if (priv->type == 0) {				/* master */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ad82089_component,
						amp_ad82089_dais,
						ARRAY_SIZE(amp_ad82089_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component fail %d\n", ret);
	} else if (priv->type == 1) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ad82089_1_component,
						amp_ad82089_1_dais,
						ARRAY_SIZE(amp_ad82089_1_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 1 fail %d\n", ret);
	} else if (priv->type == 2) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ad82089_2_component,
						amp_ad82089_2_dais,
						ARRAY_SIZE(amp_ad82089_2_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 2 fail %d\n", ret);
	} else if (priv->type == 3) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ad82089_3_component,
						amp_ad82089_3_dais,
						ARRAY_SIZE(amp_ad82089_3_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 3 fail %d\n", ret);
	} else if (priv->type == 4) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ad82089_4_component,
						amp_ad82089_4_dais,
						ARRAY_SIZE(amp_ad82089_4_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 4 fail %d\n", ret);
	}
}

static int amp_ad82089_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct amp_ad82089_priv *priv;
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

	/*
	 *	get type :
	 *	type = 0 : master
	 *	type = 1/2/3/4 : slave
	 *	if can't get type : means master
	 */
	ret = of_property_read_u32(node, "type", &priv->type);
	if (ret) {
		dev_info(&client->dev, "[AMP 82089]can't get type\n");
		priv->type = 0;
	}

	/* GPIO : master get gpio handler */
	if (priv->type == 0) {
		priv->amp_powerdown = devm_gpiod_get(&client->dev, "amppowerdown",
							GPIOD_OUT_LOW);
		if (IS_ERR(priv->amp_powerdown)) {
			dev_err(&client->dev, "Unable to retrieve gpio amp powerdown\n");
			return PTR_ERR(priv->amp_powerdown);
		}
	}

	priv->regmap = devm_regmap_init_i2c(client, &ad82089_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	amp_register_component(priv);

	/* parse dts */
	ret = of_property_read_string(node, "mtk_ini", &name);
	if (ret)
		dev_err(&client->dev, "[AMP 82089]can't get mtk ini path\n");

	priv->mtk_ini = name;

	/* Create device attribute files */
	ret = sysfs_create_groups(&priv->dev->kobj, amp_attr_groups);
	if (ret)
		dev_err(priv->dev, "Audio amp Sysfs init fail\n");

	return 0;
}

static int amp_ad82089_i2c_remove(struct i2c_client *client)
{
	struct amp_ad82089_priv *priv = i2c_get_clientdata(client);

	if (priv->init_table)
		vfree(priv->init_table);

	sysfs_remove_groups(&priv->dev->kobj, amp_attr_groups);

	return 0;
}

#ifdef CONFIG_PM
int amp_ad82089_pm_suspend(struct device *dev)
{
	struct amp_ad82089_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (priv->suspended)
		return 0;
	//pull low amp PD pin
	amp_ad82089_powerdown(component, 1);

	priv->suspended = true;

	return 0;
}

int amp_ad82089_pm_resume(struct device *dev)
{
	struct amp_ad82089_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (!priv->suspended)
		return 0;

	amp_ad82089_probe(component);

	priv->suspended = false;

	return 0;
}

static const struct dev_pm_ops amp_ad82089_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amp_ad82089_pm_suspend,
			   amp_ad82089_pm_resume)
};
#endif

static const struct of_device_id mtk_amp_dt_match[] = {
	{ .compatible = "ESMT, ad82089", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_amp_dt_match);

static const struct i2c_device_id ad82089_i2c_id[] = {
	{ "esmt,amp", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad82089_i2c_id);

static struct i2c_driver amp_ad82089_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "esmt-amp82089",
		.of_match_table	= of_match_ptr(mtk_amp_dt_match),
#ifdef CONFIG_PM
		.pm	= &amp_ad82089_pm_ops,
#endif
	},
	.probe		= amp_ad82089_i2c_probe,
	.remove		= amp_ad82089_i2c_remove,
	.id_table	= ad82089_i2c_id,
};

module_i2c_driver(amp_ad82089_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC AMP ad82089 platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("AMP ad82089 driver");
