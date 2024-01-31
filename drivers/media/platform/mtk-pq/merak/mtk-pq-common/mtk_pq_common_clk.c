// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/clk-provider.h>
#include <linux/timekeeping.h>
#include <linux/genalloc.h>
#include <linux/dma-resv.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence.h>
#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "clk-dtv.h"

#include "mtk_pq.h"
#include "mtk_pq_common.h"


//-----------------------------------------------------------------------------
//	Local Define
//-----------------------------------------------------------------------------
#define CLK_M   (1000000)
#define CLK_DIV (64)

//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------

static int _mtk_pq_common_clk_set_parent(struct mtk_pq_platform_device *pdev, int clk_index,
					char *dev_clk_name, bool enable)
{
	int ret = 0;
	struct clk *dev_clk = NULL;
	struct clk_hw *clk_hw = NULL;
	struct clk_hw *parent_clk_hw = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_set_parent\n");

	if (IS_ERR_OR_NULL(pdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pdev Pointer is NULL!\n");
		return -1;
	}

	if (IS_ERR_OR_NULL(dev_clk_name)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev_clk_name Pointer is NULL!\n");
		return -1;
	}

	dev_clk = devm_clk_get(pdev->dev, dev_clk_name);
	if (IS_ERR_OR_NULL(dev_clk)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to get %s\n", dev_clk_name);
		return -ENODEV;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "clk_name = %s\n", __clk_get_name(dev_clk));

	//the way 2 : get change_parent clk
	clk_hw = __clk_get_hw(dev_clk);
	printk(KERN_ERR "[PQDD CLK][%s,%5d] %s%p, %s%p, %s%s, %s%s, %s%p, %s%d\n",
		__func__, __LINE__,
		"dev_clk=", dev_clk,
		"dev=", pdev->dev,
		"dev_clk_name=", dev_clk_name,
		"clk_get_name=", __clk_get_name(dev_clk),
		"clk_hw=", clk_hw,
		"clk_index=", clk_index);

	if (IS_ERR_OR_NULL(clk_hw)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to get clk_hw\n");
		return -ENODEV;
	}
	parent_clk_hw = clk_hw_get_parent_by_index(clk_hw, clk_index);
	if (IS_ERR_OR_NULL(parent_clk_hw)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to get parent_clk_hw\n");
		return -ENODEV;
	}

	//set_parent clk
	ret = clk_set_parent(dev_clk, parent_clk_hw->clk);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to change clk_index [%d]\n", clk_index);
		return -ENODEV;
	}

	if (enable == true) {
		//prepare and enable clk
		ret = clk_prepare_enable(dev_clk);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"failed to enable clk_index [%d]\n", clk_index);
			return -ENODEV;
		}
	} else
		clk_disable_unprepare(dev_clk);

	devm_clk_put(pdev->dev, dev_clk);

	return ret;
}

static int _mtk_pq_common_clk_set_max(struct mtk_pq_platform_device *pdev, bool enable)
{
	int ret = 0;
	int edIdx   = 1;
		//"0:xtal_vcod1_24m_ck","1:xcpll_vcod1_720m_xc_ed_buf_ck","2:xcpll_vcod1_720m_ck"
	int fnIdx   = 1;
		//"0:xtal_vcod1_24m_ck","1:xcpll_vcod1_720m_xc_fn_buf_ck","2:xcpll_vcod1_720m_ck"
	int fsIdx   = 1;
		//"0:xtal_vcod1_24m_ck","1:xcpll_vcod1_720m_xc_fs_buf_ck","2:xcpll_vcod1_720m_ck"
	int srsIdx  = 1;
		//"0:xtal_vcod1_24m_ck","1:sys2pll_vcod3_480m_ck"
	int slowIdx = 1;
		//"0:xtal_vcod1_24m_ck","1:sys2pll_vcod4_360m_ck"
	int srIdx = 1;
		//"0:xc_srs_ck","1:xc_fn_ck","2:xc_630m_ck"

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_set_max\n");

	if (IS_ERR_OR_NULL(pdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pdev Pointer is NULL!\n");
		return -1;
	}

	ret = _mtk_pq_common_clk_set_parent(pdev, edIdx, "clk_xc_ed", enable);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_ed\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_set_parent(pdev, fnIdx, "clk_xc_fn", enable);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_fn\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_set_parent(pdev, fsIdx, "clk_xc_fs", enable);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_fs\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_set_parent(pdev, srsIdx, "clk_xc_srs", enable);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_srs\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_set_parent(pdev, slowIdx, "clk_xc_slow", enable);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_slow\n");
		return -EINVAL;
	}

	if ((pdev->pqcaps.u32AISR_Support) && (pdev->pqcaps.u32AISR_Version == 0)) {
		ret = _mtk_pq_common_clk_set_parent(pdev, srIdx, "clk_xc_sr_sram", enable);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to enable clk_xc_sr_sram\n");
			return -EINVAL;
		}
	}

	return ret;
}

static int _mtk_pq_common_clk_set_rate_formula(
	u32 clk_target, u32 clk_max,
	u32 clk_oft, u32 clk_s, u32 clk_e,
	u32 clk_sld_oft, u32 clk_sld_s, u32 clk_sld_e)
{
	int ret = 0;
	int remainder = 0;
	u32 clk_rate = 0;

	if (clk_target < clk_max) {
		clk_rate  = (clk_target * CLK_DIV / clk_max);
		remainder = (clk_target * CLK_DIV % clk_max);
		clk_rate  = (remainder == 0) ? (clk_rate) : (clk_rate+1);
		if (clk_oft != 0)
			ret = clock_write(CLK_NONPM, clk_oft, clk_rate, clk_s, clk_e);
		if (clk_sld_oft != 0)
			ret = clock_write(CLK_NONPM, clk_sld_oft, 1, clk_sld_s, clk_sld_e);
	}

	return ret;
}

static int _mtk_pq_common_clk_set_target(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;
	u32 clk_target = 0, clk_max = 0, clk_rate = 0;
	u32 clk_oft = 0, clk_s = 0, clk_e = 0;
	u32 clk_sld_oft = 0, clk_sld_s = 0, clk_sld_e = 0;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_set_target\n");

	if (IS_ERR_OR_NULL(pdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pdev Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	if (IS_ERR_OR_NULL(pqcaps)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqcaps Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (IS_ERR_OR_NULL(clkRate)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "clkRate Pointer is NULL!\n");
		return -1;
	}

	//-----CLK ED-----//
	clk_oft     = clkRate->ED_CLK_RATE;
	clk_s       = clkRate->ED_CLK_RATE_S;
	clk_e       = clkRate->ED_CLK_RATE_E;
	clk_sld_oft = clkRate->ED_CLK_RATE_SLD;
	clk_sld_s   = clkRate->ED_CLK_RATE_SLD_S;
	clk_sld_e   = clkRate->ED_CLK_RATE_SLD_E;
	clk_target  = ((u32)pqcaps->u32XC_ED_TARGET_CLK/CLK_M);
	clk_max     = ((u32)pqcaps->u32XC_ED_MAX_CLK/CLK_M);

	_mtk_pq_common_clk_set_rate_formula(
		clk_target, clk_max,
		clk_oft, clk_s, clk_e,
		clk_sld_oft, clk_sld_s, clk_sld_e);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"[ED]max:0x%X target:0x%X rate:0x%X oft:0x%X s:0x%X e:0x%X\n",
		clk_max, clk_target, clk_rate, clk_oft, clk_s, clk_e);

	//-----CLK FN-----//
	clk_oft     = clkRate->FN_CLK_RATE;
	clk_s       = clkRate->FN_CLK_RATE_S;
	clk_e       = clkRate->FN_CLK_RATE_E;
	clk_sld_oft = clkRate->FN_CLK_RATE_SLD;
	clk_sld_s   = clkRate->FN_CLK_RATE_SLD_S;
	clk_sld_e   = clkRate->FN_CLK_RATE_SLD_E;
	clk_target  = ((int)pqcaps->u32XC_FN_TARGET_CLK/CLK_M);
	clk_max     = ((int)pqcaps->u32XC_FN_MAX_CLK/CLK_M);

	_mtk_pq_common_clk_set_rate_formula(
		clk_target, clk_max,
		clk_oft, clk_s, clk_e,
		clk_sld_oft, clk_sld_s, clk_sld_e);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"[FN]max:0x%X target:0x%X rate:0x%X oft:0x%X s:0x%X e:0x%X\n",
		clk_max, clk_target, clk_rate, clk_oft, clk_s, clk_e);

	//-----CLK FS-----//
	clk_oft     = clkRate->FS_CLK_RATE;
	clk_s       = clkRate->FS_CLK_RATE_S;
	clk_e       = clkRate->FS_CLK_RATE_E;
	clk_sld_oft = clkRate->FS_CLK_RATE_SLD;
	clk_sld_s   = clkRate->FS_CLK_RATE_SLD_S;
	clk_sld_e   = clkRate->FS_CLK_RATE_SLD_E;
	clk_target  = ((int)pqcaps->u32XC_FS_TARGET_CLK/CLK_M);
	clk_max     = ((int)pqcaps->u32XC_FS_MAX_CLK/CLK_M);

	_mtk_pq_common_clk_set_rate_formula(
		clk_target, clk_max,
		clk_oft, clk_s, clk_e,
		clk_sld_oft, clk_sld_s, clk_sld_e);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"[FS]max:0x%X target:0x%X rate:0x%X oft:0x%X s:0x%X e:0x%X\n",
		clk_max, clk_target, clk_rate, clk_oft, clk_s, clk_e);

	return ret;
}

int _mtk_pq_common_clk_parse_dts_max(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;
	u32 *clkMax = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_parse_dts_max\n");

	if (pdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	property_dev = pdev->dev;
	if (property_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "clock-max");

	if (bank_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	//-----CLK ED-----//
	clkMax = &pqcaps->u32XC_ED_MAX_CLK;
	ret = of_property_read_u32(bank_node, "XC_ED_MAX_CLK", clkMax);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_ED_MAX_CLK resource\r\n");
		goto Fail;
	}

	//-----CLK FN-----//
	clkMax = &pqcaps->u32XC_FN_MAX_CLK;
	ret = of_property_read_u32(bank_node, "XC_FN_MAX_CLK", clkMax);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_FN_MAX_CLK resource\r\n");
		goto Fail;
	}

	//-----CLK FS-----//
	clkMax = &pqcaps->u32XC_FS_MAX_CLK;
	ret = of_property_read_u32(bank_node, "XC_FS_MAX_CLK", clkMax);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_FS_MAX_CLK resource\r\n");
		goto Fail;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[max]clk_fn = %d, clk_fs = %d\n",
		pqcaps->u32XC_FN_MAX_CLK, pqcaps->u32XC_FS_MAX_CLK);

	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

int _mtk_pq_common_clk_parse_dts_target(struct mtk_pq_platform_device *pdev)
{
	struct mtk_pq_caps *pqcaps = NULL;
	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *clkTarget = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_parse_dts_target\n");

	if (pdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	property_dev = pdev->dev;
	if (property_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	property_dev = pdev->dev;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "clock-target");

	if (bank_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	//-----CLK ED-----//
	clkTarget = &pqcaps->u32XC_ED_TARGET_CLK;
	ret = of_property_read_u32(bank_node, "XC_ED_TARGET_CLK", clkTarget);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_ED_TARGET_CLK resource\r\n");
		goto Fail;
	}

	//-----CLK FN-----//
	clkTarget = &pqcaps->u32XC_FN_TARGET_CLK;
	ret = of_property_read_u32(bank_node, "XC_FN_TARGET_CLK", clkTarget);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_FN_TARGET_CLK resource\r\n");
		goto Fail;
	}

	//-----CLK FS-----//
	clkTarget = &pqcaps->u32XC_FS_TARGET_CLK;
	ret = of_property_read_u32(bank_node, "XC_FS_TARGET_CLK", clkTarget);
	if (ret) {
		PQ_MSG_ERROR("Failed to get XC_FS_TARGET_CLK resource\r\n");
		goto Fail;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[target]clk_fn = %d, clk_fs = %d\n",
		pqcaps->u32XC_FN_TARGET_CLK, pqcaps->u32XC_FS_TARGET_CLK);

	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

static int _mtk_pq_common_clk_parse_dts_rate(struct mtk_pq_platform_device *pdev)
{
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *rate = NULL;
	u32 *rateStart = NULL;
	u32 *rateEnd = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_init\n");

	if (pdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	property_dev = pdev->dev;
	if (property_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	clkRate = &pqcaps->clkRate;
	property_dev = pdev->dev;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "clock-rate");

	if (bank_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	//-----CLK ED-----//
	rate      = &clkRate->ED_CLK_RATE;
	rateStart = &clkRate->ED_CLK_RATE_S;
	rateEnd   = &clkRate->ED_CLK_RATE_E;
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE", rate);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE_S", rateStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE_E", rateEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE_S\r\n");
		goto Fail;
	}

	//-----CLK FN-----//
	rate      = &clkRate->FN_CLK_RATE;
	rateStart = &clkRate->FN_CLK_RATE_S;
	rateEnd   = &clkRate->FN_CLK_RATE_E;
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE", rate);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE_S", rateStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE_E", rateEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE_S\r\n");
		goto Fail;
	}

	//-----CLK FS-----//
	rate      = &clkRate->FS_CLK_RATE;
	rateStart = &clkRate->FS_CLK_RATE_S;
	rateEnd   = &clkRate->FS_CLK_RATE_E;
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE", rate);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE_S", rateStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE_E", rateEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE_S\r\n");
		goto Fail;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "clk_fn: 0x%X, s: 0x%X, e: 0x%X\n",
		clkRate->FN_CLK_RATE, clkRate->FN_CLK_RATE_S, clkRate->FN_CLK_RATE_E);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "clk_fs: 0x%X, s: 0x%X, e: 0x%X\n",
		clkRate->FS_CLK_RATE, clkRate->FS_CLK_RATE_S, clkRate->FS_CLK_RATE_E);

	of_node_put(bank_node);

	return ret;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

static int _mtk_pq_common_clk_parse_dts_rate_sld(struct mtk_pq_platform_device *pdev)
{
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	struct device *property_dev = NULL;
	struct device_node *bank_node = NULL;
	int ret = 0;
	u32 *sld = NULL;
	u32 *sldStart = NULL;
	u32 *sldEnd = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_init\n");

	if (pdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	property_dev = pdev->dev;
	if (property_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pdev->pqcaps;
	clkRate = &pqcaps->clkRate;
	property_dev = pdev->dev;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "clock-rate");

	if (bank_node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	//-----CLK ED-----//
	sld      = &clkRate->ED_CLK_RATE_SLD;
	sldStart = &clkRate->ED_CLK_RATE_SLD_S;
	sldEnd   = &clkRate->ED_CLK_RATE_SLD_E;
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE_SLD", sld);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE_SLD\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE_SLD_S", sldStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_ED_CLK_RATE_SLD_E", sldEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_ED_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}

	//-----CLK FN-----//
	sld      = &clkRate->FN_CLK_RATE_SLD;
	sldStart = &clkRate->FN_CLK_RATE_SLD_S;
	sldEnd   = &clkRate->FN_CLK_RATE_SLD_E;
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE_SLD", sld);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE_SLD\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE_SLD_S", sldStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FN_CLK_RATE_SLD_E", sldEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FN_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}

	//-----CLK FS-----//
	sld      = &clkRate->FS_CLK_RATE_SLD;
	sldStart = &clkRate->FS_CLK_RATE_SLD_S;
	sldEnd   = &clkRate->FS_CLK_RATE_SLD_E;
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE_SLD", sld);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE_SLD\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE_SLD_S", sldStart);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}
	ret = of_property_read_u32(bank_node, "REG_XC_FS_CLK_RATE_SLD_E", sldEnd);
	if (ret) {
		PQ_MSG_ERROR("Failed to get REG_XC_FS_CLK_RATE_SLD_S\r\n");
		goto Fail;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "clk_fn_sld: 0x%X, s: 0x%X, e: 0x%X\n",
		clkRate->FN_CLK_RATE_SLD, clkRate->FN_CLK_RATE_SLD_S, clkRate->FN_CLK_RATE_SLD_E);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "clk_fs_sld: 0x%X, s: 0x%X, e: 0x%X\n",
		clkRate->FS_CLK_RATE_SLD, clkRate->FS_CLK_RATE_SLD_S, clkRate->FS_CLK_RATE_SLD_E);

	of_node_put(bank_node);

	return ret;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

//-----------------------------------------------------------------------------
//  Global Functions
//-----------------------------------------------------------------------------
int mtk_pq_common_clk_parse_dts(struct mtk_pq_platform_device *pdev)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_parse_dts_target\n");

	if (pdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	ret =  _mtk_pq_common_clk_parse_dts_max(pdev);
	if (ret == -EINVAL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to parse_dts_max\n");
		return -EINVAL;
	}

	ret =  _mtk_pq_common_clk_parse_dts_target(pdev);
	if (ret == -EINVAL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to parse_dts_target\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_parse_dts_rate(pdev);
	if (ret == -EINVAL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to parse_dts_rate\n");
		return -EINVAL;
	}

	ret = _mtk_pq_common_clk_parse_dts_rate_sld(pdev);
	if (ret == -EINVAL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to parse_dts_rate_sld\n");
		return -EINVAL;
	}

	return ret;
}

int mtk_pq_common_clk_setup(struct mtk_pq_platform_device *pdev, bool enable)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "enter clk_setup\n");

	if (IS_ERR_OR_NULL(pdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pdev Pointer is NULL!\n");
		return -1;
	}

	if (enable) { //open clk
		ret = _mtk_pq_common_clk_set_max(pdev, true);
		if (ret == -EINVAL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to clk_set_max\n");
			return -EINVAL;
		}

		ret = _mtk_pq_common_clk_set_target(pdev);
		if (ret == -EINVAL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to clk_set_max\n");
			return -EINVAL;
		}
	} else { //close clk
		ret = _mtk_pq_common_clk_set_max(pdev, false);
		if (ret == -EINVAL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "failed to clk_set_max\n");
			return -EINVAL;
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
//  SysFs Functions
//-----------------------------------------------------------------------------
static ssize_t mtk_pq_clk_target_xc_ed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_ED_TARGET_CLK", pqdev->pqcaps.u32XC_ED_TARGET_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_target_xc_fn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FN_TARGET_CLK", pqdev->pqcaps.u32XC_FN_TARGET_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_target_xc_fs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FS_TARGET_CLK", pqdev->pqcaps.u32XC_FS_TARGET_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_max_xc_ed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_ED_MAX_CLK", pqdev->pqcaps.u32XC_ED_MAX_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_max_xc_fn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FN_MAX_CLK", pqdev->pqcaps.u32XC_FN_MAX_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_max_xc_fs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FS_MAX_CLK", pqdev->pqcaps.u32XC_FS_MAX_CLK);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_ed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE", clkRate->ED_CLK_RATE);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_ed_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE_S", clkRate->ED_CLK_RATE_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_ed_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE_E", clkRate->ED_CLK_RATE_E);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FN_RATE_CLK", clkRate->FN_CLK_RATE);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fn_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}
	PQ_CAPABILITY_SHOW("REG_XC_FN_CLK_RATE_S", clkRate->FN_CLK_RATE_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fn_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}
	PQ_CAPABILITY_SHOW("REG_XC_FN_CLK_RATE_E", clkRate->FN_CLK_RATE_E);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("XC_FS_RATE_CLK", clkRate->FS_CLK_RATE);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fs_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FS_CLK_RATE_S", clkRate->FS_CLK_RATE_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_xc_fs_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FS_CLK_RATE_E", clkRate->FS_CLK_RATE_E);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_ed_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE_SLD", clkRate->ED_CLK_RATE_SLD);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_ed_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE_SLD_S", clkRate->ED_CLK_RATE_SLD_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_ed_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_ED_CLK_RATE_SLD_E", clkRate->ED_CLK_RATE_SLD_E);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fn_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FN_CLK_RATE_SLD", clkRate->FN_CLK_RATE_SLD);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fn_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FN_CLK_RATE_SLD_S", clkRate->FN_CLK_RATE_SLD_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fn_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FN_CLK_RATE_SLD_E", clkRate->FN_CLK_RATE_SLD_E);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FS_CLK_RATE_SLD", clkRate->FS_CLK_RATE_SLD);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fs_s_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FS_CLK_RATE_SLD_S", clkRate->FS_CLK_RATE_SLD_S);

	return ssize;
}

static ssize_t mtk_pq_clk_rate_sld_xc_fs_e_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_caps *pqcaps = NULL;
	struct mtk_pq_clk_rate_table *clkRate = NULL;
	int ssize = 0;
	__u8 u8Size = MAX_SYSFS_SIZE;

	if ((dev == NULL) || (attr == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqdev = dev_get_drvdata(dev);
	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	pqcaps = &pqdev->pqcaps;
	if (pqcaps == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	clkRate = &pqcaps->clkRate;
	if (clkRate == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -1;
	}

	PQ_CAPABILITY_SHOW("REG_XC_FS_CLK_RATE_SLD_E", clkRate->FS_CLK_RATE_SLD_E);

	return ssize;
}

static DEVICE_ATTR_RO(mtk_pq_clk_target_xc_ed);
static DEVICE_ATTR_RO(mtk_pq_clk_target_xc_fn);
static DEVICE_ATTR_RO(mtk_pq_clk_target_xc_fs);

static DEVICE_ATTR_RO(mtk_pq_clk_max_xc_ed);
static DEVICE_ATTR_RO(mtk_pq_clk_max_xc_fn);
static DEVICE_ATTR_RO(mtk_pq_clk_max_xc_fs);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_ed);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_ed_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_ed_e);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fn);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fn_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fn_e);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fs);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fs_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_xc_fs_e);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_ed);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_ed_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_ed_e);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fn);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fn_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fn_e);

static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fs);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fs_s);
static DEVICE_ATTR_RO(mtk_pq_clk_rate_sld_xc_fs_e);

static struct attribute *mtk_pq_clk_attrs[] = {
	&dev_attr_mtk_pq_clk_target_xc_ed.attr,
	&dev_attr_mtk_pq_clk_target_xc_fn.attr,
	&dev_attr_mtk_pq_clk_target_xc_fs.attr,

	&dev_attr_mtk_pq_clk_max_xc_ed.attr,
	&dev_attr_mtk_pq_clk_max_xc_fn.attr,
	&dev_attr_mtk_pq_clk_max_xc_fs.attr,

	&dev_attr_mtk_pq_clk_rate_xc_ed.attr,
	&dev_attr_mtk_pq_clk_rate_xc_ed_s.attr,
	&dev_attr_mtk_pq_clk_rate_xc_ed_e.attr,

	&dev_attr_mtk_pq_clk_rate_xc_fn.attr,
	&dev_attr_mtk_pq_clk_rate_xc_fn_s.attr,
	&dev_attr_mtk_pq_clk_rate_xc_fn_e.attr,

	&dev_attr_mtk_pq_clk_rate_xc_fs.attr,
	&dev_attr_mtk_pq_clk_rate_xc_fs_s.attr,
	&dev_attr_mtk_pq_clk_rate_xc_fs_e.attr,

	&dev_attr_mtk_pq_clk_rate_sld_xc_ed.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_ed_s.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_ed_e.attr,

	&dev_attr_mtk_pq_clk_rate_sld_xc_fn.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_fn_s.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_fn_e.attr,

	&dev_attr_mtk_pq_clk_rate_sld_xc_fs.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_fs_s.attr,
	&dev_attr_mtk_pq_clk_rate_sld_xc_fs_e.attr,

	NULL,
};

static const struct attribute_group mtk_pq_clk_attrs_group = {
	.name = "pq_clk",
	.attrs = mtk_pq_clk_attrs
};

void mtk_pq_common_clk_create_sysfs(struct device *pdv)
{
	int ret = 0;

	PQ_MSG_INFO("Device_create_file initialized\n");

	ret = sysfs_create_group(&pdv->kobj, &mtk_pq_clk_attrs_group);
	if (ret)
		dev_err(pdv, "[%d]Fail to create sysfs files: %d\n", __LINE__, ret);
}

void mtk_pq_common_clk_remove_sysfs(struct device *pdv)
{
	dev_info(pdv, "Remove device attribute files");
	sysfs_remove_group(&pdv->kobj, &mtk_pq_clk_attrs_group);
}


