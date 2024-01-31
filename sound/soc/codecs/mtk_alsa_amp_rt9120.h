/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_amp_rt9114.h  --  Mediatek DTV AMP header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#ifndef _AMP_RT9120_HEADER
#define _AMP_RT9120_HEADER

#define MTK_AMP_RT9120_PARAM_COUNT	64

struct amp_rt9120_priv {
	struct device	*dev;
	struct i2c_client *client;
	struct snd_soc_component *component;
	struct gpio_desc *amp_powerdown;
	struct regmap *regmap;
	bool suspended;
	int *param;
	const char *mtk_ini;
	u8 *init_table;
	bool initialized;
};

#endif /* _AMP_RT9120_HEADER */
