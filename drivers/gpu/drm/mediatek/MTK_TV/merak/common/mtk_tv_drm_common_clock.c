// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include "clk-dtv.h"

#include "mtk_tv_drm_common_clock.h"
#include "mtk_tv_drm_kms.h"
#include "../mtk_tv_drm_log.h"
#include "hwreg_render_video_display.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_COMMON

static int _mtk_drm_common_set_parent_clock(
	struct device *dev,
	int clk_index,
	char *dev_clk_name,
	bool enable)
{
	int ret = 0;
	struct clk *dev_clk;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_clk_hw;

	dev_clk = devm_clk_get(dev, dev_clk_name);

	if (IS_ERR(dev_clk)) {
		DRM_ERROR("[%s, %d]: failed to get %s\n",
			__func__, __LINE__, dev_clk_name);
		return -ENODEV;
	}

	DRM_INFO("[%s][%d] clk_name = %s",
		__func__, __LINE__, __clk_get_name(dev_clk));

	//the way 2 : get change_parent clk
	clk_hw = __clk_get_hw(dev_clk);
	parent_clk_hw = clk_hw_get_parent_by_index(clk_hw, clk_index);
	if (IS_ERR(parent_clk_hw)) {
		DRM_ERROR("[%s, %d]: failed to get parent_clk_hw.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	//set_parent clk
	ret = clk_set_parent(dev_clk, parent_clk_hw->clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: failed to change clk_index [%d]\n",
			__func__, __LINE__, clk_index);

		return -ENODEV;
	}

	if (enable == true) {
		//prepare and enable clk
		ret = clk_prepare_enable(dev_clk);
		if (ret) {
			DRM_ERROR("[%s, %d]: failed to enable clk_index [%d]\n",
				__func__, __LINE__, clk_index);

			return -ENODEV;
		}
	} else
		clk_disable_unprepare(dev_clk);

	devm_clk_put(dev, dev_clk);

	return ret;
}

static int _mtk_drm_common_clk_parse_dts_max(
	struct device *dev,
	struct mtk_drm_clk *clk)
{
	struct device *property_dev = dev;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *clkMax;

	DRM_INFO("[%s, %d]: enter clk_parse_dts_max\n", __func__, __LINE__);

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "drm-clock-max");

	if (bank_node == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	clkMax = &clk->u32XC_FD_MAX_CLK;

	ret = of_property_read_u32(bank_node, "FD_MAX_CLK", clkMax);

	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to get FD_MAX_CLK dts\n", __func__, __LINE__);
		goto EXIT;
	}

	DRM_ERROR("[%s, %d]:[max]clk_fd = %d\n", __func__, __LINE__,
		clk->u32XC_FD_MAX_CLK);

	of_node_put(bank_node);

	return 0;

EXIT:
	if (bank_node != NULL)
		of_node_put(bank_node);
RET:
	return ret;
}

static int _mtk_drm_common_clk_parse_dts_target(
	struct device *dev,
	struct mtk_drm_clk *clk)
{
	struct device *property_dev = dev;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *clkTarget;

	DRM_INFO("[%s, %d]: enter clk_parse_dts_target\n", __func__, __LINE__);

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "drm-clock-target");

	if (bank_node == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	clkTarget = &clk->u32XC_FD_TARGET_CLK;

	ret = of_property_read_u32(bank_node, "FD_TARGET_CLK", clkTarget);

	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to get FD_TARGET_CLK dts\n", __func__, __LINE__);
		goto EXIT;
	}

	DRM_ERROR("[%s, %d]:[target]clk_fd = %d\n", __func__, __LINE__,
		clk->u32XC_FD_TARGET_CLK);

	of_node_put(bank_node);

	return 0;

EXIT:
	if (bank_node != NULL)
		of_node_put(bank_node);
RET:
	return ret;
}

static int _mtk_drm_common_clk_parse_dts_rate(
	struct device *dev,
	struct mtk_drm_clk *clk)
{
	struct device *property_dev = dev;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *rate;
	u32 *rateStart;
	u32 *rateEnd;
	u32 *rateSld;
	u32 *rateSldStart;
	u32 *rateSldEnd;

	DRM_INFO("[%s, %d]: enter clk_parse_dts_rate\n", __func__, __LINE__);

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "drm-clock-rate");

	if (bank_node == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	rate = &clk->clkRate.FD_CLK_RATE;
	rateStart = &clk->clkRate.FD_CLK_RATE_S;
	rateEnd = &clk->clkRate.FD_CLK_RATE_E;
	rateSld = &clk->clkRate.FD_CLK_RATE_SLD;
	rateSldStart = &clk->clkRate.FD_CLK_RATE_SLD_S;
	rateSldEnd = &clk->clkRate.FD_CLK_RATE_SLD_E;

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE", rate);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate dts\n", __func__, __LINE__);
		goto EXIT;
	}

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE_S", rateStart);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate_s dts\n", __func__, __LINE__);
		goto EXIT;
	}

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE_E", rateEnd);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate_e dts\n", __func__, __LINE__);
		goto EXIT;
	}

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE_SLD", rateSld);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate_sld dts\n", __func__, __LINE__);
		goto EXIT;
	}

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE_SLD_S", rateSldStart);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate_sld_s dts\n", __func__, __LINE__);
		goto EXIT;
	}

	ret = of_property_read_u32(bank_node, "REG_FD_CLK_RATE_SLD_E", rateSldEnd);
	if (ret) {
		DRM_ERROR("[%s, %d]:Failed to get clk_rate_sld_d dts\n", __func__, __LINE__);
		goto EXIT;
	}

	DRM_ERROR("[%s, %d]:[FD clk]rate = 0x%x rate_s = 0x%x rate_e = 0x%x\n",
		__func__, __LINE__,
		clk->clkRate.FD_CLK_RATE,
		clk->clkRate.FD_CLK_RATE_S,
		clk->clkRate.FD_CLK_RATE_E);

	DRM_ERROR("[%s, %d]:[FD clk]rateSld = 0x%x rateSldStart = 0x%x rateSldEnd = 0x%x\n",
		__func__, __LINE__,
		clk->clkRate.FD_CLK_RATE_SLD,
		clk->clkRate.FD_CLK_RATE_SLD_S,
		clk->clkRate.FD_CLK_RATE_SLD_E);

	of_node_put(bank_node);

	return 0;

EXIT:
	if (bank_node != NULL)
		of_node_put(bank_node);
RET:
	return ret;
}

int mtk_drm_common_clk_parse_dts(
	struct device *dev,
	struct mtk_drm_clk *clk)
{
	int ret = 0;

	if (dev == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	ret = _mtk_drm_common_clk_parse_dts_max(dev, clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_clk_parse_dts_max\n",
			__func__, __LINE__);
		goto RET;
	}

	ret = _mtk_drm_common_clk_parse_dts_target(dev, clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_clk_parse_dts_target\n",
			__func__, __LINE__);
		goto RET;
	}

	ret = _mtk_drm_common_clk_parse_dts_rate(dev, clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_clk_parse_dts_rate\n",
			__func__, __LINE__);
		goto RET;
	}

	return 0;
RET:
	return ret;
}

static int _mtk_drm_common_clk_set_rate_formula(
	struct mtk_drm_clk *clk)
{
	int ret = 0;
	__u64 remainder = 0;
	__u64 clk_rate = 0;
	__u64 clk_target = 0;
	__u64 clk_max = 0;
	u32 clk_oft = 0, clk_s = 0, clk_e = 0;
	u32 clk_sld_oft = 0, clk_sld_s = 0, clk_sld_e = 0;

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	clk_max = div64_u64(clk->u32XC_FD_MAX_CLK, CLK_M);
	clk_target = div64_u64(clk->u32XC_FD_TARGET_CLK, CLK_M);
	clk_oft = clk->clkRate.FD_CLK_RATE;
	clk_s = clk->clkRate.FD_CLK_RATE_S;
	clk_e = clk->clkRate.FD_CLK_RATE_E;
	clk_sld_oft = clk->clkRate.FD_CLK_RATE_SLD;
	clk_sld_s = clk->clkRate.FD_CLK_RATE_SLD_S;
	clk_sld_e = clk->clkRate.FD_CLK_RATE_SLD_E;

	if (clk_target < clk_max) {
		clk_rate = div64_u64_rem(clk_target * CLK_DIV, clk_max, &remainder);
		clk_rate  = (remainder == 0) ? (clk_rate) : (clk_rate+1);

		DRM_ERROR("[%s, %d]:clk_rate = %llu\r\n", __func__, __LINE__, clk_rate);

		if (clk_oft != 0)
			clock_write(CLK_NONPM, clk_oft, clk_rate, clk_s, clk_e);
		if (clk_sld_oft != 0)
			clock_write(CLK_NONPM, clk_sld_oft, 1, clk_sld_s, clk_sld_e);
	}

RET:
	return ret;
}

static int _mtk_drm_common_set_xcpll_clock(int enable)
{
	int ret = 0;
	struct reg_info reg[DRM_HWREG_XCPLL_REG_NUM];
	struct hwregXCPLLClockIn paramIn;
	struct hwregOut paramOut;

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregXCPLLClockIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramIn.enable = enable;
	paramIn.RIU = true;
	paramOut.reg = reg;

	ret = drv_hwreg_render_display_set_xcpll_clock(paramIn, &paramOut);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to drv_hwreg_render_display_set_xcpll_clock\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}

int mtk_drm_common_set_xc_clock(
	struct device *dev,
	struct mtk_drm_clk *clk,
	bool enable)
{
	int ret = 0;
	enum fd_clk_index clk_xc_fd_index = E_FD_CLK_XCPLL_VCODL_720M_XC_FD_BUF;

	if (dev == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	if (clk == NULL) {
		DRM_ERROR("[%s, %d]: pointer is null\n", __func__, __LINE__);
		ret = -1;
		goto RET;
	}

	ret = _mtk_drm_common_set_xcpll_clock(enable);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_set_xcpll_clock\n",
			__func__, __LINE__);
		goto RET;
	}

	ret = _mtk_drm_common_set_parent_clock(dev, clk_xc_fd_index, "clk_xc_fd", enable);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_set_parent_clock\n",
			__func__, __LINE__);
		goto RET;
	}

	ret = _mtk_drm_common_clk_set_rate_formula(clk);
	if (ret) {
		DRM_ERROR("[%s, %d]: Failed to _mtk_drm_common_clk_set_rate_formula\n",
			__func__, __LINE__);
		goto RET;
	}

	return 0;
RET:
	return ret;
}

static ssize_t fd_clk_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_max = pctx->clk.u32XC_FD_MAX_CLK;

	fd_clk_max = div64_u64(fd_clk_max, CLK_M);

	return snprintf(buf, PAGE_SIZE, "%u MHz\n", fd_clk_max);
}

static ssize_t fd_clk_target_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_target = pctx->clk.u32XC_FD_TARGET_CLK;

	fd_clk_target = div64_u64(fd_clk_target, CLK_M);

	return snprintf(buf, PAGE_SIZE, "%u MHz\n", fd_clk_target);
}

static ssize_t fd_clk_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate = pctx->clk.clkRate.FD_CLK_RATE;

	return snprintf(buf, PAGE_SIZE, "cpu address offset: 0x%x\n", fd_clk_rate);
}

static ssize_t fd_clk_rate_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate_s = pctx->clk.clkRate.FD_CLK_RATE_S;

	return snprintf(buf, PAGE_SIZE, "bit start: %d\n", fd_clk_rate_s);
}

static ssize_t fd_clk_rate_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate_e = pctx->clk.clkRate.FD_CLK_RATE_E;

	return snprintf(buf, PAGE_SIZE, "bit end: %d\n", fd_clk_rate_e);
}

static ssize_t fd_clk_rate_sld_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate_sld = pctx->clk.clkRate.FD_CLK_RATE_SLD;

	return snprintf(buf, PAGE_SIZE, "cpu address offset: 0x%x\n", fd_clk_rate_sld);
}

static ssize_t fd_clk_rate_sld_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate_sld_s = pctx->clk.clkRate.FD_CLK_RATE_SLD_S;

	return snprintf(buf, PAGE_SIZE, "bit start:%d\n", fd_clk_rate_sld_s);
}

static ssize_t fd_clk_rate_sld_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	u32 fd_clk_rate_sld_e = pctx->clk.clkRate.FD_CLK_RATE_SLD_E;

	return snprintf(buf, PAGE_SIZE, "bit end:%d\n", fd_clk_rate_sld_e);
}

static DEVICE_ATTR_RO(fd_clk_max);
static DEVICE_ATTR_RO(fd_clk_target);
static DEVICE_ATTR_RO(fd_clk_rate);
static DEVICE_ATTR_RO(fd_clk_rate_s);
static DEVICE_ATTR_RO(fd_clk_rate_e);
static DEVICE_ATTR_RO(fd_clk_rate_sld);
static DEVICE_ATTR_RO(fd_clk_rate_sld_s);
static DEVICE_ATTR_RO(fd_clk_rate_sld_e);

static struct attribute *mtk_drm_clk_attrs[] = {
	&dev_attr_fd_clk_max.attr,
	&dev_attr_fd_clk_target.attr,
	&dev_attr_fd_clk_rate.attr,
	&dev_attr_fd_clk_rate_s.attr,
	&dev_attr_fd_clk_rate_e.attr,
	&dev_attr_fd_clk_rate_sld.attr,
	&dev_attr_fd_clk_rate_sld_s.attr,
	&dev_attr_fd_clk_rate_sld_e.attr,
	NULL
};

static const struct attribute_group mtk_drm_clk_attr_group = {
	.name = "clk",
	.attrs = mtk_drm_clk_attrs
};

void mtk_drm_common_clk_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_drm_clk_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}
}

void mtk_drm_common_clk_remove_sysfs(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove device attribute files");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_drm_clk_attr_group);
}
