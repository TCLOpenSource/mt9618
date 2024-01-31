/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * mtk_amp_ntp8928.h  --  NTP8928 AMP header
 *
 * Copyright (c) 2022 MediaTek Inc.
 *
 */

#ifndef _AMP_NTP8928_HEADER
#define _AMP_NTP8928_HEADER

#define MTK_AMP_NTP8928_PARAM_COUNT	64


#define NTP8928_FAULT_REG		0x75
#define NTP8928_MAX_REG			0x80

struct amp_ntp8928_priv {
	struct device	*dev;
	struct i2c_client *client;
	struct snd_soc_component *component;
	struct gpio_desc *amp_powerdown;
	struct gpio_desc *amp_reset;
	struct regmap *regmap;
	bool suspended;
	const char *mtk_ini;
	u8 *init_table;
	bool initialized;
	int type;
};

#endif /* _AMP_NTP8928_HEADER */
