/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_COMMON_CLOCK_H_
#define _MTK_TV_DRM_COMMON_CLOCK_H_

#define CLK_M   (1000000)
#define CLK_DIV (64)

//------------------------------------------------------------------------------
//  Enum
//------------------------------------------------------------------------------

enum fd_clk_index {
	E_FD_CLK_XTAL_VCODL_24M = 0,
	E_FD_CLK_XCPLL_VCODL_720M_XC_FD_BUF,
	E_FD_CLK_XCPLL_VCODL_720M,
	E_FD_CLK_MAX,
};
//------------------------------------------------------------------------------
//  define
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Structures
//------------------------------------------------------------------------------

struct mtk_drm_clk_rate {
	u32 FD_CLK_RATE;
	u32 FD_CLK_RATE_S;
	u32 FD_CLK_RATE_E;
	u32 FD_CLK_RATE_SLD;
	u32 FD_CLK_RATE_SLD_S;
	u32 FD_CLK_RATE_SLD_E;
};

struct mtk_drm_clk {
	struct mtk_drm_clk_rate clkRate;
	u32 u32XC_FD_MAX_CLK;
	u32 u32XC_FD_TARGET_CLK;
};

//------------------------------------------------------------------------------
//  Global Functions
//------------------------------------------------------------------------------
int mtk_drm_common_set_xc_clock(
	struct device *dev,
	struct mtk_drm_clk *clk,
	bool enable);

int mtk_drm_common_clk_parse_dts(
	struct device *dev,
	struct mtk_drm_clk *clk);

void mtk_drm_common_clk_create_sysfs(
	struct platform_device *pdev);

void mtk_drm_common_clk_remove_sysfs(
	struct platform_device *pdev);

#endif

