/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * mtk_amp_tas5805.h  --  TI TAS5805 AMP header
 *
 * Copyright (c) 2022 MediaTek Inc.
 *
 */

#ifndef _TAS5805_HEADER
#define _TAS5805_HEADER

#define TAS5805_PARAM_COUNT	64

/* Register Address Map */
#define TAS5805_VOLUME_CTRL_REG	0x03

struct amp_tas5805_priv {
	struct device	*dev;
	struct i2c_client *client;
	struct snd_soc_component *component;
	struct gpio_desc *amp_powerdown;
	struct regmap *regmap;
	bool suspended;
	const char *mtk_ini;
	u8 *init_table;
	bool initialized;
	int type;
};

#endif /* _TAS5805_HEADER */
