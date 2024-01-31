// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * mtk_amp_ntp8928.c -- AMP NTP8928 driver
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

#include "mtk_amp_ntp8928.h"
#include "mtk_ini_parser.h"

// you can read out the register and ram data, and check them.
#define NTP8928_REG_RAM_CHECK

u8 AmpInitTblNTP8928[] = {
//DataLen	Address	DataN	DataN+1...
	0x01,	0x7E,	0x00,
	0x01,	0x00,	0x00,
	0x01,	0x01,	0x00,
	0x01,	0x02,	0x00,
	0x01,	0x03,	0x00,
	0x01,	0x04,	0x4D,
	0x01,	0x05,	0x4E,
	0x01,	0x06,	0x00,
	0x01,	0x0E,	0x15,
	0x01,	0x0F,	0x15,
	0x01,	0x10,	0x07,
	0x01,	0x11,	0x07,
	0x01,	0x12,	0x55,
	0x01,	0x13,	0x55,
	0x01,	0x14,	0x05,
	0x01,	0x15,	0x05,
	0x01,	0x19,	0x02,
	0x01,	0x7E,	0x00,
	0x01,	0x20,	0x80,
	0x01,	0x21,	0x01,
	0x01,	0x22,	0x80,
	0x01,	0x23,	0x01,
	0x01,	0x29,	0x11,
	0x01,	0x2A,	0x80,
	0x01,	0x2B,	0x01,
	0x01,	0x2C,	0x01,
	0x01,	0x26,	0x00,
	0x01,	0x27,	0x41,
	0x01,	0x7E,	0x03,
	0x04,	0x32,	0x0D,	0x37,	0x9F,	0xFA,
	0x04,	0x33,	0x0D,	0x37,	0x9F,	0xFA,
	0x04,	0x34,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x35,	0x10,	0x52,	0x18,	0x01,
	0x04,	0x36,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x37,	0x0A,	0x0F,	0x6A,	0xD5,
	0x04,	0x38,	0x0B,	0x0F,	0x6A,	0xD5,
	0x04,	0x39,	0x0A,	0x0F,	0x6A,	0xD5,
	0x04,	0x3A,	0x11,	0x64,	0xC4,	0x1A,
	0x04,	0x3B,	0x10,	0xD2,	0x7E,	0xE1,
	0x04,	0x3C,	0x10,	0x69,	0x0C,	0x01,
	0x04,	0x3D,	0x10,	0xE9,	0x0C,	0x01,
	0x04,	0x3E,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x3F,	0x10,	0x52,	0x18,	0x01,
	0x04,	0x40,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x41,	0x10,	0x67,	0x01,	0xC5,
	0x04,	0x42,	0x11,	0xE7,	0x01,	0xC5,
	0x04,	0x43,	0x10,	0x67,	0x01,	0xC5,
	0x04,	0x44,	0x11,	0x64,	0xC4,	0x1A,
	0x04,	0x45,	0x10,	0xD2,	0x7E,	0xE1,
	0x04,	0x46,	0x0A,	0x1F,	0x4A,	0x74,
	0x04,	0x47,	0x0A,	0x1F,	0x4A,	0x74,
	0x04,	0x48,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x49,	0x10,	0x7B,	0x05,	0xAC,
	0x04,	0x4A,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x4B,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x4C,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x4D,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x4E,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x4F,	0x20,	0x00,	0x00,	0x00,
	0x01,	0x7E,	0x00,
	0x01,	0x16,	0x00,
	0x01,	0x17,	0xD0,
	0x01,	0x18,	0xD0,
	0x01,	0x3C,	0x4C,
	0x01,	0x41,	0x12,
	0x01,	0x43,	0x02,
	0x01,	0x67,	0x00,
	0x01,	0x62,	0x00,
	0x01,	0x63,	0x05,
	0x01,	0x64,	0x55,
	0x01,	0x7E,	0x03,
	0x04,	0x00,	0x10,	0x7E,	0xEC,	0xD8,
	0x04,	0x01,	0x11,	0xFE,	0xEC,	0xD8,
	0x04,	0x02,	0x10,	0x7E,	0xEC,	0xD8,
	0x04,	0x03,	0x11,	0x7E,	0xEC,	0x48,
	0x04,	0x04,	0x10,	0xFD,	0xDA,	0xD4,
	0x04,	0x05,	0x11,	0x00,	0x0F,	0xD2,
	0x04,	0x06,	0x11,	0xFE,	0xFB,	0x96,
	0x04,	0x07,	0x10,	0x7D,	0xD9,	0xCB,
	0x04,	0x08,	0x11,	0x7E,	0xFB,	0x96,
	0x04,	0x09,	0x10,	0xFD,	0xF9,	0x6E,
	0x04,	0x0A,	0x10,	0x7F,	0xC1,	0xD3,
	0x04,	0x0B,	0x11,	0xFF,	0x4F,	0xD8,
	0x04,	0x0C,	0x10,	0x7E,	0xED,	0x34,
	0x04,	0x0D,	0x11,	0x7F,	0x4F,	0xD8,
	0x04,	0x0E,	0x10,	0xFE,	0xAF,	0x06,
	0x04,	0x0F,	0x11,	0x00,	0x8E,	0x6D,
	0x04,	0x10,	0x11,	0xF9,	0x0F,	0xAE,
	0x04,	0x11,	0x10,	0x79,	0x7E,	0x32,
	0x04,	0x12,	0x11,	0x79,	0x0F,	0xAE,
	0x04,	0x13,	0x10,	0xFA,	0x9B,	0x0B,
	0x04,	0x14,	0x11,	0x02,	0xE9,	0xEF,
	0x04,	0x15,	0x11,	0xEB,	0x52,	0xDA,
	0x04,	0x16,	0x10,	0x6B,	0x32,	0x58,
	0x04,	0x17,	0x11,	0x6B,	0x52,	0xDA,
	0x04,	0x18,	0x10,	0xF1,	0x06,	0x39,
	0x04,	0x19,	0x11,	0x04,	0x9F,	0x86,
	0x04,	0x1A,	0x11,	0xD5,	0xA8,	0x2A,
	0x04,	0x1B,	0x10,	0x64,	0x2C,	0x57,
	0x04,	0x1C,	0x11,	0x55,	0xA8,	0x2A,
	0x04,	0x1D,	0x10,	0xED,	0x6B,	0x62,
	0x04,	0x1E,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x1F,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x20,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x21,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x22,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x23,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x24,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x25,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x26,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x27,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x28,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x29,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x2A,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x2B,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x2C,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x2D,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x2E,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x2F,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x30,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x31,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x5C,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x5D,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x5E,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x5F,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x60,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x61,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x62,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x63,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x64,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x65,	0x20,	0x00,	0x00,	0x00,
	0x01,	0x7E,	0x00,
	0x01,	0x7E,	0x08,
	0x04,	0x00,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x01,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x02,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x03,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x04,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x05,	0x10,	0x00,	0x00,	0x00,
	0x04,	0x06,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x07,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x08,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x09,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0A,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0B,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0C,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0D,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0E,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x0F,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x10,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x11,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x12,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x13,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x14,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x15,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x16,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x17,	0x20,	0x00,	0x00,	0x00,
	0x04,	0x1F,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x20,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x21,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x22,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x23,	0x11,	0x00,	0x00,	0x00,
	0x04,	0x24,	0x11,	0x00,	0x00,	0x00,
	0x01,	0x7E,	0x00,
	0x01,	0x0C,	0xFF,
	0x00,
};

u8 AmpSWResetTbl[] = {
	1,	0x0B,	0x01,
	0x00,
};


u8 AmpMuteTbl[] = {
	1,	0x33,	0x03,
	0x00,
};

u8 AmpUnMuteTbl[] = {
	1,	0x33,	0x00,
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

static int amp_ntp8928_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct amp_ntp8928_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->initialized)
		dev_info(priv->dev, "amp 8928 initialized !!!!!!!!!\n");
	else
		dev_info(priv->dev, "amp 8928 not init !!!!!!!!!!!\n");

	return 0;
}

static const struct snd_soc_dai_ops amp_ntp8928_dai_ops = {
	.startup	= amp_ntp8928_startup,
};

static struct snd_soc_dai_driver amp_ntp8928_dais[] = {
	{
		.name = "AMP_NTP8928",
		.id = 0,
		.playback = {
			.stream_name = "AMP NTP8928",
		},
		.ops = &amp_ntp8928_dai_ops,
	},
};

static struct snd_soc_dai_driver amp_ntp8928_1_dais[] = {
	{
		.name = "AMP_NTP8928_1",
		.id = 1,
		.playback = {
			.stream_name = "AMP NTP8928_1",
		},
	},
};

static struct snd_soc_dai_driver amp_ntp8928_2_dais[] = {
	{
		.name = "AMP_NTP8928_2",
		.id = 2,
		.playback = {
			.stream_name = "AMP NTP8928_2",
		},
	},
};

static struct snd_soc_dai_driver amp_ntp8928_3_dais[] = {
	{
		.name = "AMP_NTP8928_3",
		.id = 3,
		.playback = {
			.stream_name = "AMP NTP8928_3",
		},
	},
};

static struct snd_soc_dai_driver amp_ntp8928_4_dais[] = {
	{
		.name = "AMP_NTP8928_4",
		.id = 4,
		.playback = {
			.stream_name = "AMP NTP8928_4",
		},
	},
};

static int amp_write(struct amp_ntp8928_priv *priv, unsigned char *InitTable)
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

static int amp_ntp8928_reset(struct snd_soc_component *component, bool bEnable)
{
	struct amp_ntp8928_priv *priv =
			snd_soc_component_get_drvdata(component);

	if (priv->amp_reset != NULL)
		gpiod_set_value(priv->amp_reset, bEnable);

	return 0;
}

static int amp_ntp8928_powerdown(struct snd_soc_component *component, bool bEnable)
{
	struct amp_ntp8928_priv *priv = snd_soc_component_get_drvdata(component);

	gpiod_set_value(priv->amp_powerdown, bEnable);

	return 0;
}

static int amp_ntp8928_mute(struct snd_soc_component *component, bool bEnable)
{
	struct amp_ntp8928_priv *priv = snd_soc_component_get_drvdata(component);

	if (bEnable)
		amp_write(priv, AmpMuteTbl);
	else
		amp_write(priv, AmpUnMuteTbl);

	return 0;
}

static int amp_ntp8928_init(struct amp_ntp8928_priv *priv, unsigned char *InitTable)
{
	int ret = 0;

	ret = amp_write(priv, InitTable);
	if (ret < 0) {
		dev_err(priv->dev, "[AMP 8928]Init fail\n");
		priv->initialized = 0;
	} else {
		dev_info(priv->dev, "[AMP 8928]Init success\n");
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
	struct amp_ntp8928_priv *priv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", priv->initialized);
}

static ssize_t amp_mute_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t retval;
	int mute_flag = 0;
	struct amp_ntp8928_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	retval = kstrtoint(buf, 10, &mute_flag);
	if (retval == 0) {
		if ((mute_flag != 0) && (mute_flag != 1))
			dev_err(dev, "You should enter 0 or 1 !!!\n");
	}

	dev_info(dev, "set amp mute %d\n", mute_flag);

	amp_ntp8928_mute(component, mute_flag);

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

static int amp_ntp8928_probe(struct snd_soc_component *component)
{
	struct amp_ntp8928_priv *priv =	snd_soc_component_get_drvdata(component);
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
		amp_ntp8928_powerdown(component, 0);
		//pull down amp PD pin
		amp_ntp8928_reset(component, 0);
		//wait t2 min time = 10ms
		mtk_amp_delaytask_us(10*1000);
		//pull up amp PD pin
		amp_ntp8928_reset(component, 1);
		//wait t3 min time = 5ms
		mtk_amp_delaytask_us(5*1000);
		//pull up amp PD pin
		amp_ntp8928_reset(component, 0);
		//wait t4 min time = 1ms
		mtk_amp_delaytask_us(1*1000);
	}

	if (priv->init_table)
		ret = amp_ntp8928_init(priv, priv->init_table);
	else
		ret = amp_ntp8928_init(priv, AmpInitTblNTP8928);
	if (ret < 0)
		dev_err(priv->dev, "[AMP 8928] init fail\n");

	priv->component = component;

	return 0;
}

static void amp_ntp8928_remove(struct snd_soc_component *component)
{
	amp_ntp8928_powerdown(component, 1);
}

static int amp_ntp8928_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_ntp8928_priv *priv = snd_soc_component_get_drvdata(component);
	int val = ucontrol->value.integer.value[0];

	if (val < 0)
		return -EINVAL;

	dev_info(priv->dev, "set amp mute %d\n", val);

	amp_ntp8928_mute(component, val);

	return 0;
}

static int amp_ntp8928_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct amp_ntp8928_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int val = 0;
	int ret = 0;

	ret = regmap_read(priv->regmap, 0x33, &val);

	dev_info(priv->dev, "get amp mute %d\n", val);
	if (ret < 0)
		dev_err(priv->dev, "fail to read mute register %d\n", ret);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static const struct snd_kcontrol_new amp_ntp8928_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP Mute", 0, amp_ntp8928_mute_get,
				amp_ntp8928_mute_put),
};

static const struct snd_kcontrol_new amp_ntp8928_1_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP-1 Mute", 0, amp_ntp8928_mute_get,
				amp_ntp8928_mute_put),
};

static const struct snd_kcontrol_new amp_ntp8928_2_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP-2 Mute", 0, amp_ntp8928_mute_get,
				amp_ntp8928_mute_put),
};

static const struct snd_kcontrol_new amp_ntp8928_3_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP-3 Mute", 0, amp_ntp8928_mute_get,
				amp_ntp8928_mute_put),
};

static const struct snd_kcontrol_new amp_ntp8928_4_controls[] = {
	SOC_SINGLE_BOOL_EXT("AMP-4 Mute", 0, amp_ntp8928_mute_get,
				amp_ntp8928_mute_put),
};

const struct snd_soc_component_driver amp_ntp8928_component = {
	.name		= "amp-ntp8928-0",
	.probe		= amp_ntp8928_probe,
	.remove		= amp_ntp8928_remove,
	.controls	= amp_ntp8928_controls,
	.num_controls	= ARRAY_SIZE(amp_ntp8928_controls),
};

const struct snd_soc_component_driver amp_ntp8928_1_component = {
	.name		= "amp-ntp8928-1",
	.probe		= amp_ntp8928_probe,
	.remove		= amp_ntp8928_remove,
	.controls	= amp_ntp8928_1_controls,
	.num_controls	= ARRAY_SIZE(amp_ntp8928_1_controls),
};

const struct snd_soc_component_driver amp_ntp8928_2_component = {
	.name		= "amp-ntp8928-2",
	.probe		= amp_ntp8928_probe,
	.remove		= amp_ntp8928_remove,
	.controls	= amp_ntp8928_2_controls,
	.num_controls	= ARRAY_SIZE(amp_ntp8928_2_controls),
};

const struct snd_soc_component_driver amp_ntp8928_3_component = {
	.name		= "amp-ntp8928-3",
	.probe		= amp_ntp8928_probe,
	.remove		= amp_ntp8928_remove,
	.controls	= amp_ntp8928_3_controls,
	.num_controls	= ARRAY_SIZE(amp_ntp8928_3_controls),
};

const struct snd_soc_component_driver amp_ntp8928_4_component = {
	.name		= "amp-ntp8928-4",
	.probe		= amp_ntp8928_probe,
	.remove		= amp_ntp8928_remove,
	.controls	= amp_ntp8928_4_controls,
	.num_controls	= ARRAY_SIZE(amp_ntp8928_4_controls),
};

static bool ntp8928_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef	AD82088_REG_RAM_CHECK
	if (reg < NTP8928_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case NTP8928_FAULT_REG:
		return true;
	default:
		return false;
	}
#endif
}

static const struct regmap_config ntp8928_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = NTP8928_MAX_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = ntp8928_is_volatile_reg,
};

static void amp_register_component(struct amp_ntp8928_priv *priv)
{
	int ret;

	if (priv->type == 0) {				/* master */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ntp8928_component,
						amp_ntp8928_dais,
						ARRAY_SIZE(amp_ntp8928_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component fail %d\n", ret);
	} else if (priv->type == 1) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ntp8928_1_component,
						amp_ntp8928_1_dais,
						ARRAY_SIZE(amp_ntp8928_1_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 1 fail %d\n", ret);
	} else if (priv->type == 2) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ntp8928_2_component,
						amp_ntp8928_2_dais,
						ARRAY_SIZE(amp_ntp8928_2_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 2 fail %d\n", ret);
	} else if (priv->type == 3) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ntp8928_3_component,
						amp_ntp8928_3_dais,
						ARRAY_SIZE(amp_ntp8928_3_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 3 fail %d\n", ret);
	} else if (priv->type == 4) {			/* slave */
		ret = devm_snd_soc_register_component(&priv->client->dev,
						&amp_ntp8928_4_component,
						amp_ntp8928_4_dais,
						ARRAY_SIZE(amp_ntp8928_4_dais));
		if (ret)
			dev_err(priv->dev, "AMP soc_register_component 4 fail %d\n", ret);
	}
}

static int amp_ntp8928_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct amp_ntp8928_priv *priv;
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
		dev_info(&client->dev, "[AMP 8928]can't get type\n");
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
		priv->amp_reset = devm_gpiod_get(&client->dev, "ampreset",
							GPIOD_OUT_LOW);
		if (IS_ERR(priv->amp_reset)) {
			dev_err(&client->dev, "Unable to retrieve gpio amp reset\n");
			return PTR_ERR(priv->amp_reset);
		}

	}
	priv->regmap = devm_regmap_init_i2c(client, &ntp8928_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	amp_register_component(priv);

	/* parse dts */
	ret = of_property_read_string(node, "mtk_ini", &name);
	if (ret)
		dev_err(&client->dev, "[AMP 8928]can't get mtk ini path\n");

	priv->mtk_ini = name;

	/* Create device attribute files */
	ret = sysfs_create_groups(&priv->dev->kobj, amp_attr_groups);
	if (ret)
		dev_err(priv->dev, "Audio amp Sysfs init fail\n");

	return 0;
}

static int amp_ntp8928_i2c_remove(struct i2c_client *client)
{
	struct amp_ntp8928_priv *priv = i2c_get_clientdata(client);

	if (priv->init_table)
		vfree(priv->init_table);

	sysfs_remove_groups(&priv->dev->kobj, amp_attr_groups);

	return 0;
}

#ifdef CONFIG_PM
int amp_ntp8928_pm_suspend(struct device *dev)
{
	struct amp_ntp8928_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (priv->suspended)
		return 0;
	//pull low amp PD pin
	amp_ntp8928_powerdown(component, 1);

	priv->suspended = true;

	return 0;
}

int amp_ntp8928_pm_resume(struct device *dev)
{
	struct amp_ntp8928_priv *priv = dev_get_drvdata(dev);
	struct snd_soc_component *component = priv->component;

	if (!priv->suspended)
		return 0;

	amp_ntp8928_probe(component);

	priv->suspended = false;

	return 0;
}

static const struct dev_pm_ops amp_ntp8928_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(amp_ntp8928_pm_suspend,
			   amp_ntp8928_pm_resume)
};
#endif

static const struct of_device_id mtk_amp_dt_match[] = {
	{ .compatible = "NTP, ntp8928", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_amp_dt_match);

static const struct i2c_device_id ntp8928_i2c_id[] = {
	{ "ntp,amp", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ntp8928_i2c_id);

static struct i2c_driver amp_ntp8928_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ntp-amp8928",
		.of_match_table	= of_match_ptr(mtk_amp_dt_match),
#ifdef CONFIG_PM
		.pm	= &amp_ntp8928_pm_ops,
#endif
	},
	.probe		= amp_ntp8928_i2c_probe,
	.remove		= amp_ntp8928_i2c_remove,
	.id_table	= ntp8928_i2c_id,
};

module_i2c_driver(amp_ntp8928_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC AMP ntp8928 platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("AMP ntp8928 driver");
