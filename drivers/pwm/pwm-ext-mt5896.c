// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author Owen.Tseng <Owen.Tseng@mediatek.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include "pwm-mt5896-coda.h"
#include "pwm-mt5896-riu.h"
#include <linux/clk.h>

struct mtk_pwmext_dat {
	char pwm_ch;
	unsigned int period;
	unsigned int duty_cycle;
	unsigned int shift;
	unsigned int div;
	bool pol;
	bool rst_mux;
	bool rst_vsync;
	unsigned int rstcnt;
	bool enable;
};

struct mtk_pwm {
	struct pwm_chip chip;
	struct mtk_pwmext_dat *data;
	void __iomem *base;
	struct clk *clk_gate_xtal;
	struct clk *clk_gate_pwm;
};


#define PWM_HW_SUPPORT_CH		1
#define PWM_CLK_SET				(0x1 << 6)
#define PWM_REG_OFFSET_INDEX	(0x3 << 2)
#define PWM_REG_SHIFT_INDEX		(0x2 << 2)
#define PWM_VDBEN_SET			(0x1 << 9)
#define PWM_VREST_SET			(0x1 << 10)
#define PWM_DBEN_SET			(0x1 << 11)
#define XTAL_12MHZ				12
#define FREQ_SCALE_XTAL			1000
#define POL_SHIFT_BIT			8


static inline struct mtk_pwm *to_mtk_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_pwm, chip);
}

static inline u16 dtvpwm_rd(struct mtk_pwm *mdp, u32 reg)
{
	return mtk_readw_relaxed((mdp->base + reg));
}

static inline u8 dtvpwm_rdl(struct mtk_pwm *mdp, u16 reg)
{
	return dtvpwm_rd(mdp, reg)&0xff;
}

static inline void dtvpwm_wr(struct mtk_pwm *mdp, u16 reg, u32 val)
{
	mtk_writew_relaxed((mdp->base + reg), val);
}

static inline void dtvpwm_wrl(struct mtk_pwm *mdp, u16 reg, u8 val)
{
	u16 val16 = dtvpwm_rd(mdp, reg)&0xff00;

	val16 |= val;
	dtvpwm_wr(mdp, reg, val16);
}

static void mtk_pwm_oen(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;
	unsigned char reg_tmp;
	unsigned char bitop_tmp;

	ch_mask = mdp->data->pwm_ch;

	reg_tmp = dtvpwm_rdl(mdp, REG_0004_PWM);

	if (data)
		bitop_tmp = reg_tmp | ((1 << ch_mask) | PWM_CLK_SET);
	else
		bitop_tmp = (reg_tmp | PWM_CLK_SET) & (~((1 << ch_mask)));

	dtvpwm_wrl(mdp, REG_0004_PWM, bitop_tmp);

}

static void mtk_pwm_period(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;

	ch_mask = mdp->data->pwm_ch;
	if (data > 1)
		data -= 1;

	dtvpwm_wr(mdp, REG_0008_PWM + (ch_mask*PWM_REG_OFFSET_INDEX), data);
}

static void mtk_pwm_duty(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;

	ch_mask = mdp->data->pwm_ch;
	if (data > 1)
		data -= 1;

	dtvpwm_wr(mdp, REG_000C_PWM + (ch_mask*PWM_REG_OFFSET_INDEX), data);
}

static void mtk_pwm_div(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;
	unsigned int reg_tmp;

	ch_mask = mdp->data->pwm_ch;
	if (data > 1)
		data -= 1;

	reg_tmp = dtvpwm_rdl(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX));
	reg_tmp |= data;
	dtvpwm_wrl(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX), reg_tmp);

}

static void mtk_pwm_shift(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;
	unsigned int reg_tmp;

	ch_mask = mdp->data->pwm_ch;

	reg_tmp = dtvpwm_rdl(mdp, REG_00A0_PWM + (ch_mask*PWM_REG_SHIFT_INDEX));
	reg_tmp |= data;
	dtvpwm_wrl(mdp, REG_00A0_PWM + (ch_mask*PWM_REG_SHIFT_INDEX), reg_tmp);
}

static void mtk_pwm_pol(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;
	unsigned int reg_tmp;

	ch_mask = mdp->data->pwm_ch;

	reg_tmp = dtvpwm_rd(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX));
	if (data)
		reg_tmp |= (0x1 << POL_SHIFT_BIT);
	else
		reg_tmp &= ~(0x1 << POL_SHIFT_BIT);

	dtvpwm_wr(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX), reg_tmp);
}

static void mtk_pwm_vsync(struct mtk_pwm *mdp, unsigned int data)
{
	unsigned char ch_mask;
	unsigned int reg_tmp;

	ch_mask = mdp->data->pwm_ch;

	reg_tmp = dtvpwm_rd(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX));
	if (data) {
		reg_tmp |= PWM_VDBEN_SET;
		reg_tmp |= PWM_VREST_SET;
		reg_tmp &= ~PWM_DBEN_SET;
	} else {
		reg_tmp &= ~PWM_VDBEN_SET;
		reg_tmp &= ~PWM_VREST_SET;
		reg_tmp |= PWM_DBEN_SET;
	}
	dtvpwm_wr(mdp, REG_0010_PWM + (ch_mask*PWM_REG_OFFSET_INDEX), reg_tmp);

}

static int mtk_pwm_config(struct pwm_chip *chip,
							struct pwm_device *pwm,
						int duty_t, int period_t)
{
	struct mtk_pwm *mdp = to_mtk_pwm(chip);
	unsigned int duty_set, period_set;

	if ((duty_t == 0) || (duty_t > period_t)) {
		/*solve glitch when duty is 0*/
		mtk_pwm_shift(mdp, 1);
	}

	mdp->data->pwm_ch = pwm->hwpwm;
	/*12hz XTAL timing is nsec*/
	period_set = (XTAL_12MHZ*period_t)/FREQ_SCALE_XTAL;
	duty_set = (XTAL_12MHZ*duty_t)/FREQ_SCALE_XTAL;

	mtk_pwm_period(mdp, period_set);
	mtk_pwm_duty(mdp, duty_set);

	mtk_pwm_div(mdp, mdp->data->div);
	mtk_pwm_pol(mdp, mdp->data->pol);
	mtk_pwm_vsync(mdp, mdp->data->rst_vsync);
	mtk_pwm_oen(mdp, 1);

	return 0;
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm *mdp = to_mtk_pwm(chip);

	mtk_pwm_oen(mdp, 1);
	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_pwm *mdp = to_mtk_pwm(chip);

	mtk_pwm_oen(mdp, 0);
}

static void mtk_pwm_get_state(struct pwm_chip *chip,
							struct pwm_device *pwm,
							struct pwm_state *state)
{
	struct mtk_pwm *mdp = to_mtk_pwm(chip);

	state->period = mdp->data->period;
	state->duty_cycle = mdp->data->duty_cycle;
	state->polarity = mdp->data->pol;
	state->enabled = mdp->data->enable;
}

static const struct pwm_ops mtk_pwm_ops = {
	.config = mtk_pwm_config,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
	.get_state = mtk_pwm_get_state,
	.owner = THIS_MODULE,
};

static int mtk_pwm_probe(struct platform_device *pdev)
{
	struct mtk_pwm *mdp;
	int ret;
	struct resource *res;
	struct device_node *np = NULL;
	struct mtk_pwmext_dat *pwmdat;

	mdp = devm_kzalloc(&pdev->dev, sizeof(*mdp), GFP_KERNEL);
	pwmdat = devm_kzalloc(&pdev->dev, sizeof(*pwmdat), GFP_KERNEL);
	np = pdev->dev.of_node;

	if ((!mdp) || (!pwmdat))
		return -ENOMEM;

	if (!np) {
		dev_err(&pdev->dev, "Failed to find node\n");
		ret = -EINVAL;
		return ret;
	}
	memset(pwmdat, 0, sizeof(*pwmdat));
	mdp->data = pwmdat;
	mdp->chip.dev = &pdev->dev;
	mdp->chip.ops = &mtk_pwm_ops;
	/*It will return error without this setting*/
	mdp->chip.npwm = PWM_HW_SUPPORT_CH;
	mdp->chip.base = -1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mdp->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));

	mdp->clk_gate_xtal = devm_clk_get(&pdev->dev, "xtal_12m2pwm");
	if (IS_ERR_OR_NULL(mdp->clk_gate_xtal)) {
		if (IS_ERR(mdp->clk_gate_xtal))
			dev_info(&pdev->dev, "err mdp->clk_gate_xtal=%#lx\n",
				(unsigned long)mdp->clk_gate_xtal);
		else
			dev_info(&pdev->dev, "err mdp->clk_gate_xtal = NULL\n");
		return -EINVAL;
	}
	ret = clk_prepare_enable(mdp->clk_gate_xtal);
	if (ret)
		dev_info(&pdev->dev, "Failed to enable xtal_12m2pwm: %d\n", ret);

	mdp->clk_gate_pwm = devm_clk_get(&pdev->dev, "pwm_ck");
	if (IS_ERR_OR_NULL(mdp->clk_gate_pwm)) {
		if (IS_ERR(mdp->clk_gate_pwm))
			dev_info(&pdev->dev, "err mdp->clk_gate_pwm=%#lx\n",
				(unsigned long)mdp->clk_gate_pwm);
		else
			dev_info(&pdev->dev, "err mdp->clk_gate_pwm = NULL\n");
	}
	ret = clk_prepare_enable(mdp->clk_gate_pwm);
	if (ret)
		dev_info(&pdev->dev, "Failed to enable pwm_ck: %d\n", ret);

	ret = pwmchip_add(&mdp->chip);

	if (ret < 0)
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);

	platform_set_drvdata(pdev, mdp);

	return ret;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	if (!(pdev->name) ||
		strcmp(pdev->name, "mediatek,mt5896-pwm-ext") || pdev->id != 0)
		return -1;

	pdev->dev.platform_data = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int mtk_dtv_pwm_suspend(struct device *pdev)
{
	struct mtk_pwm *mdp = dev_get_drvdata(pdev);

	if (!mdp)
		return -EINVAL;

	if (!IS_ERR_OR_NULL(mdp->clk_gate_xtal))
		clk_disable_unprepare(mdp->clk_gate_xtal);

	if (!IS_ERR_OR_NULL(mdp->clk_gate_pwm))
		clk_disable_unprepare(mdp->clk_gate_pwm);

	return 0;
}

static int mtk_dtv_pwm_resume(struct device *pdev)
{
	struct mtk_pwm *mdp = dev_get_drvdata(pdev);
	int ret;

	if (!mdp)
		return -EINVAL;

	if (!IS_ERR_OR_NULL(mdp->clk_gate_xtal)) {
		ret = clk_prepare_enable(mdp->clk_gate_xtal);
		if (ret)
			dev_info(pdev, "resume failed to enable xtal_12m2pwm: %d\n", ret);
	}
	if (!IS_ERR_OR_NULL(mdp->clk_gate_pwm)) {
		ret = clk_prepare_enable(mdp->clk_gate_pwm);
		if (ret)
			dev_info(pdev, "resume failed to enable pwm_ck: %d\n", ret);
	}
	mtk_pwm_oen(mdp, 1);
	return 0;
}
#endif

#if defined(CONFIG_OF)
static const struct of_device_id mtkpwm_of_device_ids[] = {
		{.compatible = "mediatek,mt5896-pwm-ext" },
		{},
};
#endif

#ifdef CONFIG_PM
static SIMPLE_DEV_PM_OPS(pwm_pwmnormal_pm_ops,
		mtk_dtv_pwm_suspend, mtk_dtv_pwm_resume);
#endif
static struct platform_driver Mtk_pwm_driver = {
	.probe		= mtk_pwm_probe,
	.remove		= mtk_pwm_remove,
	.driver		= {
#if defined(CONFIG_OF)
	.of_match_table = mtkpwm_of_device_ids,
#endif
#ifdef CONFIG_PM
	.pm	= &pwm_pwmnormal_pm_ops,
#endif
	.name   = "mediatek,mt5896-pwm-ext",
	.owner  = THIS_MODULE,
}
};

static int __init mtk_pwm_init_module(void)
{
	int retval = 0;

	retval = platform_driver_register(&Mtk_pwm_driver);

	return retval;
}

static void __exit mtk_pwm_exit_module(void)
{
	platform_driver_unregister(&Mtk_pwm_driver);
}

module_init(mtk_pwm_init_module);
module_exit(mtk_pwm_exit_module);


MODULE_AUTHOR("Owen.Tseng@mediatek.com");
MODULE_DESCRIPTION("pwm driver");
MODULE_LICENSE("GPL");

