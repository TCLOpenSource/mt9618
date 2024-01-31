/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * mtk_alsa_amp_rt9114.h  --  Mediatek DTV AMP header
 *
 * Copyright (c) 2022 MediaTek Inc.
 *
 */

#ifndef _AMP_RT9119_HEADER
#define _AMP_RT9119_HEADER

#define MTK_AMP_RT9119_PARAM_COUNT	64

struct amp_rt9119_priv {
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

#endif /* _AMP_RT9119_HEADER */
