/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * mtk_amp_ad82089.h  --  ESMT AMP AD82089 header
 *
 * Copyright (c) 2022 MediaTek Inc.
 *
 */

#ifndef _AMP_AD82089_HEADER
#define _AMP_AD82089_HEADER

#define MTK_AMP_AD82089_PARAM_COUNT	64

/* Register Address Map */
#define AD82089_STATE_CTRL1_REG	0x00
#define AD82089_STATE_CTRL2_REG	0x01
#define AD82089_STATE_CTRL3_REG	0x02
#define AD82089_VOLUME_CTRL_REG	0x03
#define AD82089_STATE_CTRL5_REG	0x1A

#define AD82089_FAULT_REG		0x84
#define AD82089_MAX_REG			0x85

struct amp_ad82089_priv {
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

#endif /* _AMP_AD82089_HEADER */
