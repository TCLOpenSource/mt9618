// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include "mtk_tv_drm_panel_common.h"
#include "mtk_tv_drm_video_panel.h"
#include "mtk_tcon_out_if.h"
#include <drm/mtk_tv_drm.h>
#include "hwreg_render_video_pnl.h"

static bool isPanelCommonEnable = FALSE;
static bool isMapRiu;
//-------------------------------------------------------------------------------------------------
// Prototype
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// Structure
//-------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_PANEL

#define VB1_2_LANE (2)
#define VB1_4_LANE (4)
#define VB1_8_LANE (8)
#define VB1_16_LANE (16)
#define VB1_32_LANE (32)
#define VB1_64_LANE (64)

#define OFFSET_32 32
#define TGEN_VTTV_BUF_4320 (6) //should sync with boot
#define TGEN_VTTV_BUF_2160 (6) //should sync with boot
#define TGEN_VTTV_BUF_1080 (3) //should sync with boot
#define TGEN_PIXELSHIFT_BUF (10) //should sync with boot
#define TGEN_VDE_ST_4320 (174)
#define TGEN_VDE_ST_2160 (84)
#define TGEN_VDE_ST_1080 (42)

#define TGEN_VDE_4320 (4320)
#define TGEN_VDE_3840 (3840)
#define TGEN_VDE_2160 (2160)
#define TGEN_VDE_1080 (1080)

#define DEC_100 (100)
#define DEC_2 (2)

//-------------------------------------------------------------------------------------------------
// Function
//-------------------------------------------------------------------------------------------------

__u32 get_dts_u32(struct device_node *np, const char *name)
{
	int u32Tmp = 0x0;
	int ret = 0;

	if (!np) {
		DRM_ERROR("[%s, %d]: device_node is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	if (!name) {
		DRM_ERROR("[%s, %d]: name is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0) {
		DRM_INFO("[%s, %d]: read u32 failed, name = %s\n",
			__func__, __LINE__, name);
	}
	return u32Tmp;
}

__u32 get_dts_u32_index(struct device_node *np, const char *name, int index)
{
	int u32Tmp = 0x0;
	int ret = 0;

	if (!np) {
		DRM_ERROR("[%s, %d]: device_node is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	if (!name) {
		DRM_ERROR("[%s, %d]: name is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = of_property_read_u32_index(np, name, index, &u32Tmp);
	if (ret != 0x0) {
		DRM_INFO("[%s, %d]: read u32 index failed, name = %s\n",
			__func__, __LINE__, name);
	}
	return u32Tmp;
}

__u32 get_dts_u32_elems(struct device_node *np, const char *name)
{
	int ret = 0;

	if (!np) {
		DRM_ERROR("[%s, %d]: device_node is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	if (!name) {
		DRM_ERROR("[%s, %d]: name is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = of_property_count_u32_elems(np, name);
	if (ret <= 0) {
		DRM_INFO("[%s, %d]: read u32 elem failed, name = %s\n",
			__func__, __LINE__, name);
	}
	return ret;
}

__u32 get_dts_u32_array(struct device_node *np, const char *name, uint32_t *array, int size)
{
	int ret = 0;

	if (!np) {
		DRM_ERROR("[%s, %d]: device_node is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	if (!name) {
		DRM_ERROR("[%s, %d]: name is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	if (!array) {
		DRM_ERROR("[%s, %d]: array is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = get_dts_u32_elems(np, name);
	if (ret != size) {
		DRM_ERROR("[%s, %d]: array '%s' size is ERROR, current(%d) != expected(%d)\n",
			__func__, __LINE__, name, ret, size);
		return -EPERM;
	}

	ret = of_property_read_u32_array(np, name, array, size);
	if (ret != 0) {
		DRM_INFO("[%s, %d]: read u32 array failed, name = %s\n",
			__func__, __LINE__, name);
	}
	return ret;
}

void panel_init_clk_V1(bool bEn)
{
	EN_MOD_CLK_V1_EN eCount = E_MOD_V1_EN_DELTA_B2P2_SRAM2SCACE;

	for (eCount = E_MOD_V1_EN_DELTA_B2P2_SRAM2SCACE; eCount < E_MOD_V1_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV1(bEn, eCount);
}

void panel_init_clk_V2(bool bEn)
{
	EN_MOD_CLK_V2_EN eCount = E_MOD_V2_EN_LPLL_SYN_4322FPLL;

	for (eCount = E_MOD_V2_EN_LPLL_SYN_4322FPLL; eCount < E_MOD_V2_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV2(bEn, eCount);
}

void panel_init_clk_V3(bool bEn)
{
	EN_MOD_CLK_V3_EN eCount = E_MOD_V3_EN_DAP2FRCIOPMRV55;

	for (eCount = E_MOD_V3_EN_DAP2FRCIOPMRV55; eCount < E_MOD_V3_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV3(bEn, eCount);
}

void panel_init_clk_V4(bool bEn)
{
	EN_MOD_CLK_V4_EN eCount;

	for (eCount = 0; eCount < E_MOD_V4_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV4(bEn, eCount);
}

void panel_init_clk_V5(bool bEn)
{
	EN_MOD_CLK_V5_EN eCount = E_MOD_V5_EN_SMI2SCIP;

	for (eCount = E_MOD_V5_EN_SMI2SCIP; eCount < E_MOD_V5_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV5(bEn, eCount);
}

void panel_init_clk_V6(bool bEn)
{
	EN_MOD_CLK_V6_EN eCount = E_MOD_V6_EN_SMI2SCBE;

	for (eCount = E_MOD_V6_EN_SMI2SCBE; eCount < E_MOD_V6_EN_NUM; eCount++)
		mtk_video_mod_enable_clkV6(bEn, eCount);
}

static int _get_riu_base(phys_addr_t *base)
{
#define NR_REG	2
#define DEFAULT_RIU_BASE      0x1c000000
	int i, ret;
	u32 reg[NR_REG];
	struct device_node *np;

	np = of_find_node_by_name(NULL, "riu-base");
	if (!np) {
		pr_err("cannot find riu-base on fdt, use pre-defined as 0x%x\n",
			DEFAULT_RIU_BASE);
		*base = DEFAULT_RIU_BASE;
		return 0;
	}

	ret = of_property_read_u32_array(np, "reg", reg, ARRAY_SIZE(reg));
	if (ret) {
		pr_err("%pOF has no valid 'reg' property (%d)", np, ret);
		of_node_put(np);
		*base = DEFAULT_RIU_BASE;
		return -EINVAL;
	}

	of_node_put(np);

	for (i = 0; i < ARRAY_SIZE(reg); ++i) {
		if (reg[i])
			break;
	}

	if (i == ARRAY_SIZE(reg)) {
		pr_err("invalid riu base\n");
		*base = DEFAULT_RIU_BASE;
		return -ENOMEM;
	}

	*base = reg[i];

	return 0;
}

static inline int _setup_hwreg_map(uint32_t u32RegAddr,
	uint32_t u32Size, uint32_t riu_base)
{
	void __iomem *pVaddr = NULL;

	pVaddr = ioremap(u32RegAddr, u32Size);
	if (!pVaddr) {
		DRM_ERROR("%s: map pa:0x%x+%x failed\n",
			__func__, (unsigned int)u32RegAddr,
				(unsigned int)u32Size);
		return -ENOMEM;
	}

	DRM_INFO("%s: map pa:0x%x+%x to va:%pa",
			__func__, (unsigned int)u32RegAddr,
				(unsigned int)u32Size, &pVaddr);
	drv_hwreg_common_setRIUaddr((u32RegAddr - riu_base),
			u32Size, (uint64_t)pVaddr);
	return 0;
}

void panel_common_enable(void)
{
#define BANK_CLKGEN00	0x1720
#define BANK_CLKGEN01	0x1020
#define BANK_CLKGEN02	0x1030
#define BANK_XCPLL	0x100D
	int ret;
	struct device_node *np = NULL;
	uint32_t clk_version = 0;
	phys_addr_t riu_base;
	uint32_t u32RegAddr = 0;
	uint32_t u32Size = 0;

	if (isPanelCommonEnable == TRUE)
		return;

	if (!isMapRiu) {
		ret = _get_riu_base(&riu_base);
		if (ret)
			DRM_ERROR("[%s, %d]: use default riu base address\n",
				__func__, __LINE__);

		np = of_find_node_by_name(NULL, PNL_LPLL_NODE);
		if (np != NULL) {
			u32RegAddr = get_dts_u32_index(np, PNL_LPLL_REG, 1);
			u32Size = get_dts_u32_index(np, PNL_LPLL_REG, 3);
			_setup_hwreg_map(u32RegAddr, u32Size, riu_base);
		} else {
			DRM_ERROR("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_MOD_NODE);
		}

		np = of_find_node_by_name(NULL, PNL_MOD_NODE);
		if (np != NULL) {
			u32RegAddr = get_dts_u32_index(np, PNL_MOD_REG, 1);
			u32Size = get_dts_u32_index(np, PNL_MOD_REG, 3);
			_setup_hwreg_map(u32RegAddr, u32Size, riu_base);
		} else {
			DRM_ERROR("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_MOD_NODE);
		}

		// PATCH
		_setup_hwreg_map(riu_base + (BANK_CLKGEN00 << 9),
				SZ_512, riu_base);
		_setup_hwreg_map(riu_base + (BANK_CLKGEN01 << 9),
				SZ_8K, riu_base);
		_setup_hwreg_map(riu_base + (BANK_CLKGEN02 << 9),
				SZ_8K, riu_base);
		_setup_hwreg_map(riu_base + (BANK_XCPLL << 9),
				SZ_4K, riu_base);
		isMapRiu = true;
	}

	//check clock version
	np = of_find_compatible_node(NULL, NULL, "MTK-drm-tv-kms");
	clk_version = get_dts_u32(np, "Clk_Version");

	if (clk_version >= 1) //1:MT5897 2:MT5896_E3
		drv_hwreg_render_pnl_init_clk_v2();

	isPanelCommonEnable = TRUE;
}

void panel_common_disable(void)
{
	isPanelCommonEnable = FALSE;
}

void init_mod_cfg(drm_mod_cfg *pCfg)
{
	if (!pCfg) {
		DRM_ERROR("[%s, %d]: drm_mod_cfg is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pCfg->linkIF = E_LINK_NONE;
	pCfg->lanes = 0;
	pCfg->div_sec = 0;
	pCfg->timing = E_OUTPUT_MODE_NONE;
	pCfg->format = E_PNL_OUTPUT_FORMAT_NUM;
}

__u32 setDisplayTiming(drm_st_pnl_info *pStPnlInfo)
{
	if (!pStPnlInfo) {
		DRM_ERROR("[%s, %d]: Panel INFO is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	if (pStPnlInfo->linkIF != E_LINK_NONE) {
		uint32_t u32HdeEnd = 0;
		uint32_t u32VdeEnd = 0;
		uint32_t u32Hbackporch = 0;
		uint32_t u32Vbackporch = 0;

		u32HdeEnd = pStPnlInfo->de_hstart + pStPnlInfo->de_width;
		u32VdeEnd = pStPnlInfo->de_vstart + pStPnlInfo->de_height;

		u32Hbackporch = pStPnlInfo->de_hstart -
					(pStPnlInfo->hsync_st + pStPnlInfo->hsync_w);
		u32Vbackporch = pStPnlInfo->de_vstart -
					(pStPnlInfo->vsync_st + pStPnlInfo->vsync_w);

		pStPnlInfo->displayTiming.pixelclock.typ = pStPnlInfo->typ_dclk;


		pStPnlInfo->displayTiming.hactive.max = pStPnlInfo->de_width;
		pStPnlInfo->displayTiming.hactive.typ = pStPnlInfo->de_width;
		pStPnlInfo->displayTiming.hactive.min = pStPnlInfo->de_width;


		if (pStPnlInfo->hsync_st < u32HdeEnd) {
			pStPnlInfo->displayTiming.hfront_porch.max =
				pStPnlInfo->max_htt - u32HdeEnd + pStPnlInfo->hsync_st;
			pStPnlInfo->displayTiming.hfront_porch.typ =
				pStPnlInfo->typ_htt - u32HdeEnd + pStPnlInfo->hsync_st;
			pStPnlInfo->displayTiming.hfront_porch.min =
				pStPnlInfo->min_htt - u32HdeEnd + pStPnlInfo->hsync_st;
		} else {
			pStPnlInfo->displayTiming.hfront_porch.max =
				pStPnlInfo->hsync_st - u32HdeEnd;
			pStPnlInfo->displayTiming.hfront_porch.typ =
				pStPnlInfo->hsync_st - u32HdeEnd;
			pStPnlInfo->displayTiming.hfront_porch.min =
				pStPnlInfo->hsync_st - u32HdeEnd;
		}

		pStPnlInfo->displayTiming.hback_porch.max = u32Hbackporch;
		pStPnlInfo->displayTiming.hback_porch.typ = u32Hbackporch;
		pStPnlInfo->displayTiming.hback_porch.min = u32Hbackporch;

		pStPnlInfo->displayTiming.hsync_len.max = pStPnlInfo->hsync_w;
		pStPnlInfo->displayTiming.hsync_len.typ = pStPnlInfo->hsync_w;
		pStPnlInfo->displayTiming.hsync_len.min = pStPnlInfo->hsync_w;

		pStPnlInfo->displayTiming.vactive.max = pStPnlInfo->de_height;
		pStPnlInfo->displayTiming.vactive.typ = pStPnlInfo->de_height;
		pStPnlInfo->displayTiming.vactive.min = pStPnlInfo->de_height;

		if (pStPnlInfo->vsync_st < u32VdeEnd) {
			pStPnlInfo->displayTiming.vfront_porch.max =
				pStPnlInfo->max_vtt - u32VdeEnd + pStPnlInfo->vsync_st;
			pStPnlInfo->displayTiming.vfront_porch.typ =
				pStPnlInfo->typ_vtt - u32VdeEnd + pStPnlInfo->vsync_st;
			pStPnlInfo->displayTiming.vfront_porch.min =
				pStPnlInfo->min_vtt - u32VdeEnd + pStPnlInfo->vsync_st;
		} else {
			pStPnlInfo->displayTiming.vfront_porch.max =
				pStPnlInfo->vsync_st - u32VdeEnd;
			pStPnlInfo->displayTiming.vfront_porch.typ =
				pStPnlInfo->vsync_st - u32VdeEnd;
			pStPnlInfo->displayTiming.vfront_porch.min =
				pStPnlInfo->vsync_st - u32VdeEnd;
		}

		pStPnlInfo->displayTiming.vback_porch.max = u32Vbackporch;
		pStPnlInfo->displayTiming.vback_porch.typ = u32Vbackporch;
		pStPnlInfo->displayTiming.vback_porch.min = u32Vbackporch;

		pStPnlInfo->displayTiming.vsync_len.max = pStPnlInfo->vsync_w;
		pStPnlInfo->displayTiming.vsync_len.typ = pStPnlInfo->vsync_w;
		pStPnlInfo->displayTiming.vsync_len.min = pStPnlInfo->vsync_w;

		//pStPnlInfo->displayTiming.flags if need
	}
	return 0;
}
EXPORT_SYMBOL(setDisplayTiming);

void _setout_mod_8k(drm_mod_cfg *dst_cfg, uint16_t vfreq)
{
	if (IS_VFREQ_60HZ_GROUP(vfreq))
		dst_cfg->timing = E_8K4K_60HZ;

	if (IS_VFREQ_120HZ_GROUP(vfreq))
		dst_cfg->timing = E_8K4K_120HZ;

	if (IS_VFREQ_144HZ_GROUP(vfreq))
		dst_cfg->timing = E_8K4K_144HZ;

	if (dst_cfg->timing == E_8K4K_60HZ) {
		//set lane number
		if (dst_cfg->format == E_OUTPUT_RGB ||
				dst_cfg->format == E_OUTPUT_YUV444)
			dst_cfg->lanes = VB1_32_LANE;

		if (dst_cfg->format == E_OUTPUT_YUV422)
			dst_cfg->lanes = VB1_16_LANE;
	} else {
		dst_cfg->lanes = VB1_64_LANE;
	}
}

void _setout_mod_4k(drm_mod_cfg *dst_cfg, uint16_t vfreq, uint16_t de_height)
{
	if (IS_VFREQ_60HZ_GROUP(vfreq)) {
		dst_cfg->timing = E_4K2K_60HZ;
		dst_cfg->lanes = VB1_8_LANE;
	}
	if (IS_VFREQ_120HZ_GROUP(vfreq)) {
		if (de_height == TGEN_VDE_1080) {
			dst_cfg->timing = E_4K2K_60HZ;
			dst_cfg->lanes = VB1_8_LANE;
		} else {
			dst_cfg->timing = E_4K2K_120HZ;
			dst_cfg->lanes = VB1_16_LANE;
		}
	}
	if (IS_VFREQ_144HZ_GROUP(vfreq)) {
		dst_cfg->timing = E_4K2K_144HZ;
		dst_cfg->lanes = VB1_16_LANE;
	}
	if (IS_VFREQ_240HZ_GROUP(vfreq)) {
		dst_cfg->timing = E_4K1K_240HZ;
		dst_cfg->lanes = VB1_16_LANE;
	}
}

void _setout_mod_2k(drm_mod_cfg *dst_cfg, uint16_t vfreq)
{
	if (IS_VFREQ_60HZ_GROUP(vfreq)) {
		dst_cfg->timing = E_FHD_60HZ;
		dst_cfg->lanes = VB1_2_LANE;
	}
	if (IS_VFREQ_120HZ_GROUP(vfreq)) {
		dst_cfg->timing = E_FHD_120HZ;
		dst_cfg->lanes = VB1_4_LANE;
	}
}

void set_out_mod_cfg(drm_st_pnl_info *src_info,
						drm_mod_cfg *dst_cfg)
{
	uint16_t vfreq = 0;
	uint16_t u16Lanes = 0;

	if (!src_info) {
		DRM_ERROR("[%s, %d]: Panel INFO is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (!dst_cfg) {
		DRM_ERROR("[%s, %d]: drm_mod_cfg is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (src_info->linkIF != E_LINK_NONE) {

		dst_cfg->linkIF =  src_info->linkIF;
		dst_cfg->div_sec = src_info->div_section;
		dst_cfg->format = src_info->op_format;

		if (src_info->typ_htt != 0 && src_info->typ_vtt != 0)
			vfreq = src_info->typ_dclk / src_info->typ_htt / src_info->typ_vtt;

		if (IS_OUT_8K4K(src_info->de_width, src_info->de_height))
			_setout_mod_8k(dst_cfg, vfreq);

		if (IS_OUT_4K2K(src_info->de_width, src_info->de_height)
			|| IS_OUT_4K1K(src_info->de_width, src_info->de_height))
			_setout_mod_4k(dst_cfg, vfreq, src_info->de_height);

		if (IS_OUT_2K1K(src_info->de_width, src_info->de_height))
			_setout_mod_2k(dst_cfg, vfreq);

		if (mtk_tcon_get_lanes_num_by_linktype(src_info->linkIF, &u16Lanes) == 0) {
			DRM_INFO("[%s, %d] tcon lanes=%d -> %d\n",
				__func__, __LINE__, dst_cfg->lanes, u16Lanes);
			dst_cfg->lanes = u16Lanes;
		}
	}
}

void dump_panel_info(drm_st_pnl_info *pStPnlInfo)
{
	if (!pStPnlInfo) {
		DRM_ERROR("[%s, %d]: Panel INFO is NULL.\n",
			__func__, __LINE__);
		return;
	}

	DRM_INFO("panel name = %s\n", pStPnlInfo->pnlname);
	DRM_INFO("linkIF (0:None, 1:VB1, 2:LVDS) = %d\n", pStPnlInfo->linkIF);
	DRM_INFO("VBO byte mode = %d\n", pStPnlInfo->vbo_byte);
	DRM_INFO("div_section = %d\n", pStPnlInfo->div_section);
	DRM_INFO("Panel OnTiming1 = %d\n", pStPnlInfo->ontiming_1);
	DRM_INFO("Panel OnTiming2 = %d\n", pStPnlInfo->ontiming_2);
	DRM_INFO("Panel OffTiming1 = %d\n", pStPnlInfo->offtiming_1);
	DRM_INFO("Panel OffTiming2 = %d\n", pStPnlInfo->offtiming_2);
	DRM_INFO("Hsync Start = %d\n", pStPnlInfo->hsync_st);
	DRM_INFO("HSync Width = %d\n", pStPnlInfo->hsync_w);
	DRM_INFO("Hsync Polarity = %d\n", pStPnlInfo->hsync_pol);
	DRM_INFO("Vsync Start = %d\n", pStPnlInfo->vsync_st);
	DRM_INFO("VSync Width = %d\n", pStPnlInfo->vsync_w);
	DRM_INFO("VSync Polarity = %d\n", pStPnlInfo->vsync_pol);
	DRM_INFO("Panel HStart = %d\n", pStPnlInfo->de_hstart);
	DRM_INFO("Panel VStart = %d\n", pStPnlInfo->de_vstart);
	DRM_INFO("Panel Width = %d\n", pStPnlInfo->de_width);
	DRM_INFO("Panel Height = %d\n", pStPnlInfo->de_height);
	DRM_INFO("Panel MaxHTotal = %d\n", pStPnlInfo->max_htt);
	DRM_INFO("Panel HTotal = %d\n", pStPnlInfo->typ_htt);
	DRM_INFO("Panel MinHTotal = %d\n", pStPnlInfo->min_htt);
	DRM_INFO("Panel MaxVTotal = %d\n", pStPnlInfo->max_vtt);
	DRM_INFO("Panel VTotal = %d\n", pStPnlInfo->typ_vtt);
	DRM_INFO("Panel MinVTotal = %d\n", pStPnlInfo->min_vtt);
	DRM_INFO("Panel MaxVTotalPanelProtect = %d\n", pStPnlInfo->max_vtt_panelprotect);
	DRM_INFO("Panel MinVTotalPanelProtect = %d\n", pStPnlInfo->min_vtt_panelprotect);
	DRM_INFO("Panel DCLK = %lld\n", pStPnlInfo->typ_dclk);
	DRM_INFO("Output Format = %d\n", pStPnlInfo->op_format);
	DRM_INFO("Content Output Format = %d\n", pStPnlInfo->cop_format);
	DRM_INFO("Output Color Space = %d\n", pStPnlInfo->op_color_space);
	DRM_INFO("Output Color Range = %d\n", pStPnlInfo->op_color_range);
	DRM_INFO("graphic_mixer1_outmode = %d\n", pStPnlInfo->graphic_mixer1_out_prealpha);
	DRM_INFO("graphic_mixer2_outmode = %d\n", pStPnlInfo->graphic_mixer2_out_prealpha);
	DRM_INFO("dlg_on = %d\n", pStPnlInfo->dlg_on);
}

const char *_print_linktype(drm_pnl_link_if linkIF)
{
	switch (linkIF) {
	case E_LINK_NONE:	return "No Link";
	case E_LINK_VB1:	return "VByOne";
	case E_LINK_LVDS:	return "LVDS";
	case E_LINK_VB1_TO_HDMITX:	return "VB1_TO_HDMITX";
	case E_LINK_MAX:
	default:
		return "NOT Supported link";
	}
}


const char *_print_format(drm_en_pnl_output_format format)
{
	switch (format) {
	case E_OUTPUT_RGB:	return "RGB";
	case E_OUTPUT_YUV444:	return "YUV444";
	case E_OUTPUT_YUV422:	return "YUV422";
	case E_OUTPUT_YUV420:	return "YUV420";
	case E_OUTPUT_ARGB8101010:	return "ARGB8101010";
	case E_OUTPUT_ARGB8888_W_DITHER:	return "ARGB8888 (w Dither)";
	case E_OUTPUT_ARGB8888_W_ROUND: return "ARGB8888 (w Round)";
	case E_OUTPUT_ARGB8888_MODE0:   return "MOD ARGB8888 (mode 0)";
	case E_OUTPUT_ARGB8888_MODE1:   return "MOD ARGB8888 (mode 1)";
	case E_PNL_OUTPUT_FORMAT_NUM:
	default:
		return "NOT Supported Format";
	}
}


const char *_print_timing(drm_output_mode mode)
{
	switch (mode) {
	case E_OUTPUT_MODE_NONE:	return "No Timing";
	case E_8K4K_120HZ:	return "8K4K 120HZ";
	case E_8K4K_60HZ:	return "8K4K 60HZ";
	case E_8K4K_30HZ:	return "8K4K 30HZ";
	case E_4K2K_120HZ:	return "4K2K 120HZ";
	case E_4K2K_144HZ:	return "4K2K 144HZ";
	case E_4K1K_240HZ:	return "4K1K 240HZ";
	case E_4K2K_60HZ:	return "4K2K 60HZ";
	case E_4K2K_30HZ:	return "4K2K 30HZ";
	case E_FHD_120HZ:	return "FHD 120HZ";
	case E_FHD_60HZ:	return "FHD 60HZ";
	case E_4K1K_120HZ:	return "4K1K 120HZ";
	case E_OUTPUT_MODE_MAX:
	default:
		return "NOT Supported Timing";
	}
}


void dump_mod_cfg(drm_mod_cfg *pCfg)
{
	if (!pCfg) {
		DRM_ERROR("[%s, %d]: drm_mod_cfg is NULL.\n",
			__func__, __LINE__);
		return;
	}

	DRM_INFO("link IF = %s\n", _print_linktype(pCfg->linkIF));
	DRM_INFO("div_sec = %d\n", pCfg->div_sec);
	DRM_INFO("lanes = %d\n", pCfg->lanes);
	DRM_INFO("format = %s\n", _print_format(pCfg->format));
	DRM_INFO("timing = %s\n", _print_timing(pCfg->timing));
}

uint32_t _tgentargetVdeStart(uint32_t u32DtsVde)
{
	uint32_t u32TargetVTT = TGEN_VDE_ST_2160;

	if (u32DtsVde == TGEN_VDE_4320)
		u32TargetVTT = TGEN_VDE_ST_4320;
	else if (u32DtsVde == TGEN_VDE_2160)
		u32TargetVTT = TGEN_VDE_ST_2160;
	else if (u32DtsVde <= TGEN_VDE_1080)
		u32TargetVTT = u32DtsVde*100*TGEN_VDE_ST_1080/1080/100;
	else
		u32TargetVTT = TGEN_VDE_ST_2160;

	return u32TargetVTT;
}

static uint32_t _tgentargetVTTVBuf(uint32_t u32DtsVde)
{
	uint32_t u32TargetVTTBuf = TGEN_VDE_ST_2160;

	if (u32DtsVde == TGEN_VDE_4320)
		u32TargetVTTBuf = TGEN_VTTV_BUF_4320;
	else if (u32DtsVde == TGEN_VDE_2160)
		u32TargetVTTBuf = TGEN_VTTV_BUF_2160;
	else if (u32DtsVde <= TGEN_VDE_1080)
		u32TargetVTTBuf = TGEN_VTTV_BUF_1080;
	else
		u32TargetVTTBuf = TGEN_VTTV_BUF_2160;

	return u32TargetVTTBuf;
}


int _parse_panel_info(struct device_node *np,
	drm_st_pnl_info *currentPanelInfo,
	struct list_head *panelInfoList)
{
	int ret = 0, i = 0, count = 0;
	struct device_node *infoNp = NULL;
	const char *name = NULL;
	const char *pnlname = NULL;
	const char *pnlInfoName = NULL;
	drm_st_pnl_info *info = NULL;
	uint32_t u32TargetVdeStart = 0;
	uint32_t u32TargetVTTVBuf = 0;
	uint32_t u32DtsVbp = 0;
	uint32_t u32Version = 0;
	struct device_node *np_ver;
	uint32_t u32framerate = 0;
	static __u32 u32typ_max_framerate, u32dlg_max_framerate;
	struct drm_mtk_panel_max_framerate pre_panel_framerate;
	drm_st_pnl_info *dlg_info = NULL;
	struct st_pnl_tgen_timing_info tgen_pnl_info;
	__u32 u32tmp = 0;

	////// get version
	np_ver = of_find_node_by_name(NULL, PNL_HW_INFO_NODE);
	if (np_ver == NULL) {
		DRM_ERROR("[%s, %d]: coundn't find node<%s>\n",
			__func__, __LINE__, PNL_HW_INFO_NODE);
		u32Version = DRV_PNL_VERSION0100;
	} else {
		u32Version = get_dts_u32(np_ver, PNL_LIB_VER);
	}
	dlg_info = kzalloc(sizeof(drm_st_pnl_info), GFP_KERNEL);
	if (!dlg_info) {
		ret = -ENOMEM;
		kfree(dlg_info);
		return ret;
	}
	memset(dlg_info, 0, sizeof(drm_st_pnl_info));
	/////////////////////////////////////////////

	if (np != NULL && of_device_is_available(np)) {

		count = of_property_count_strings(np, "panel_info_name_list");

		for (i = 0; i < count; i++) {
			info = kzalloc(sizeof(drm_st_pnl_info), GFP_KERNEL);
			if (!info) {
				ret = -ENOMEM;
				kfree(info);
				kfree(dlg_info);
				return ret;
			}
			of_property_read_string_index(np, "panel_info_name_list", i, &pnlInfoName);
			infoNp = of_find_node_by_name(np, pnlInfoName);
			if (infoNp != NULL && of_device_is_available(infoNp)) {
				name = NAME_TAG;
				ret = of_property_read_string(infoNp, name, &pnlname);
				if (ret != 0x0) {
					DRM_INFO("[%s, %d]: read string failed, name = %s\n",
						__func__, __LINE__, name);
					kfree(info);
					kfree(dlg_info);
					return ret;
				}
				memset(info->pnlname, 0, DRM_NAME_MAX_NUM);
				strncpy(info->pnlname, pnlname, DRM_NAME_MAX_NUM - 1);
				info->linkIF = get_dts_u32(infoNp, LINK_TYPE_TAG);
				info->vbo_byte = get_dts_u32(infoNp, VBO_BYTE_TAG);
				info->div_section = get_dts_u32(infoNp, DIV_SECTION_TAG);
				info->ontiming_1 = get_dts_u32(infoNp, ON_TIMING_1_TAG);
				info->ontiming_2 = get_dts_u32(infoNp, ON_TIMING_2_TAG);
				info->offtiming_1 = get_dts_u32(infoNp, OFF_TIMING_1_TAG);
				info->offtiming_2 = get_dts_u32(infoNp, OFF_TIMING_2_TAG);
				info->hsync_st = get_dts_u32(infoNp, HSYNC_ST_TAG);
				info->hsync_w = get_dts_u32(infoNp, HSYNC_WIDTH_TAG);
				info->hsync_pol = get_dts_u32(infoNp, HSYNC_POL_TAG);
				info->vsync_st = get_dts_u32(infoNp, VSYNC_ST_TAG);
				info->vsync_w = get_dts_u32(infoNp, VSYNC_WIDTH_TAG);
				info->vsync_pol = get_dts_u32(infoNp, VSYNC_POL_TAG);
				info->de_hstart = get_dts_u32(infoNp, HSTART_TAG);
				info->de_vstart = get_dts_u32(infoNp, VSTART_TAG);
				info->de_width = get_dts_u32(infoNp, WIDTH_TAG);
				info->de_height = get_dts_u32(infoNp, HEIGHT_TAG);
				info->max_htt = get_dts_u32(infoNp, MAX_HTOTAL_TAG);
				info->typ_htt = get_dts_u32(infoNp, HTOTAL_TAG);
				info->min_htt = get_dts_u32(infoNp, MIN_HTOTAL_TAG);
				info->max_vtt = get_dts_u32(infoNp, MAX_VTOTAL_TAG);
				info->typ_vtt = get_dts_u32(infoNp, VTOTAL_TAG);
				info->min_vtt = get_dts_u32(infoNp, MIN_VTOTAL_TAG);
				info->max_vtt_panelprotect =
					get_dts_u32(infoNp, MAX_VTOTAL_PANELPROTECT_TAG);
				info->min_vtt_panelprotect =
					get_dts_u32(infoNp, MIN_VTOTAL_PANELPROTECT_TAG);
				info->typ_dclk = ((uint64_t)get_dts_u32(
					infoNp, TYP_CLK_H_TAG) << OFFSET_32) |
					get_dts_u32(infoNp, TYP_CLK_L_TAG);
				info->op_format = get_dts_u32(infoNp, OUTPUT_FMT_TAG);
				info->cop_format = get_dts_u32(infoNp, COUTPUT_FMT_TAG);

				info->vrr_en = get_dts_u32(infoNp, VRR_EN_TAG);
				info->vrr_max_v = get_dts_u32(infoNp, VRR_MAX_TAG);
				info->vrr_min_v = get_dts_u32(infoNp, VRR_MIN_TAG);

				info->graphic_mixer1_out_prealpha = get_dts_u32(infoNp,
									GRAPHIC_MIXER1_OUTMODE_TAG);
				info->graphic_mixer2_out_prealpha = get_dts_u32(infoNp,
									GRAPHIC_MIXER2_OUTMODE_TAG);
				info->op_color_range = get_dts_u32(infoNp,
									OUTPUT_COLOR_RANGE_TAG);
				info->op_color_space = get_dts_u32(infoNp,
									OUTPUT_COLOR_SPACE_TAG);
				info->hpc_mode_en = get_dts_u32(infoNp, HPC_MODE_TAG);
				info->game_direct_fr_group = get_dts_u32(infoNp,
									GAME_DIRECT_FR_GROUP_TAG);
				info->dlg_on = get_dts_u32(infoNp, DLG_ON_TAG);

				/* do override panel info form tcon bin */
				mtk_tcon_update_panel_info(info);

				u32TargetVdeStart = _tgentargetVdeStart(info->de_height);
				u32TargetVTTVBuf = _tgentargetVTTVBuf(info->de_height);
				//Vde start protect
				if (((info->de_width == 3840) && (info->de_height == 2160)) &&
					((u32Version == DRV_PNL_VERSION0400) ||
					(u32Version == DRV_PNL_VERSION0500) ||
					(u32Version == DRV_PNL_VERSION0600)))
					u32TargetVdeStart = u32TargetVdeStart - TGEN_PIXELSHIFT_BUF;

				if (u32TargetVdeStart > info->de_vstart) {
					DRM_ERROR(" no Protect Vde start = %x\n",
						info->de_vstart);
					DRM_ERROR(" no Protect Vsync start = %x\n",
						info->vsync_st);
					u32DtsVbp = info->de_vstart -
						(info->vsync_st + info->vsync_w);
					info->de_vstart = u32TargetVdeStart;
					info->vsync_st = u32TargetVdeStart -
						info->vsync_w - u32DtsVbp;

					DRM_ERROR(" Protect Vde start = %x\n",
						u32TargetVdeStart);
					DRM_ERROR(" Protect Vsync start = %x\n",
						info->vsync_st);
				}

				if (info->de_vstart >
					(info->typ_vtt - info->de_height - u32TargetVTTVBuf)) {
					DRM_ERROR(" no Protect Vde start = %x\n",
						info->de_vstart);
					DRM_ERROR(" info->typ_vtt = %x\n",
						info->typ_vtt);
					DRM_ERROR(" info->de_height = %x\n",
						info->de_height);
					u32DtsVbp = info->de_vstart
						- (info->vsync_st + info->vsync_w);
					info->de_vstart = info->typ_vtt
						- info->de_height - u32TargetVTTVBuf;
					if (info->vsync_st > (info->vsync_w + u32DtsVbp))
						info->vsync_st = info->de_vstart
							- info->vsync_w - u32DtsVbp;
					else
						info->vsync_st = 0;

					DRM_ERROR(" Protect Vde start = %x\n",
						u32TargetVdeStart);
					DRM_ERROR(" Protect Vsync start = %x\n",
						info->vsync_st);
				}

				if (info->min_vtt < (info->de_vstart + info->de_height)) {
					info->min_vtt = info->de_vstart + info->de_height;
					DRM_ERROR(" Protect VTT MIN = %x\n",
						info->min_vtt);
				}

				if (info->min_vtt_panelprotect <
					(info->de_vstart + info->de_height)) {
					info->min_vtt_panelprotect =
						info->de_vstart + info->de_height;
					DRM_WARN(" Protect VTT MIN panel = %x\n",
						info->min_vtt);
				}

				if (i == 0)
					info->default_sel = 1;
				else
					info->default_sel = 0;

				//calculate panel framerate
				u32tmp = info->typ_htt * info->typ_vtt / DEC_2;
				u32framerate = (__u32)((info->typ_dclk * DEC_100 + u32tmp) /
				(__u64)info->typ_htt / (__u64)info->typ_vtt);
				pre_panel_framerate = mtk_panel_get_max_framerate();
				if (!info->dlg_on) {
					if (u32framerate > pre_panel_framerate.typ_max_framerate)
						u32typ_max_framerate = u32framerate;
					if (u32framerate > pre_panel_framerate.dlg_max_framerate)
						u32dlg_max_framerate = u32framerate;
				} else {
					if (u32framerate > pre_panel_framerate.dlg_max_framerate)
						u32dlg_max_framerate = u32framerate;
				}
				mtk_panel_set_max_framerate(u32typ_max_framerate,
					u32dlg_max_framerate);
				DRM_INFO("[%s] max_framerate [%d, %d]\n", __func__,
					u32typ_max_framerate, u32dlg_max_framerate);

				if (info->dlg_on)
					memcpy(dlg_info, info, sizeof(drm_st_pnl_info));
			} else {
				info->linkIF = E_LINK_NONE;
				DRM_INFO("Video path is not valid\n");
			}
			setDisplayTiming(info);
			list_add_tail(&info->list, panelInfoList);

			dump_panel_info(info);
		}
	}

	/* check current tgen setting */
	drv_hwreg_render_pnl_get_tgen_timing_info(&tgen_pnl_info);

	/* AC on with DLG timing, use DLG mode info*/
	if (((tgen_pnl_info.h_end - tgen_pnl_info.h_sta + 1) == TGEN_VDE_3840) &&
		((tgen_pnl_info.v_end - tgen_pnl_info.v_sta + 1) == TGEN_VDE_1080) &&
		(dlg_info->de_width == TGEN_VDE_3840) &&
		(dlg_info->de_height == TGEN_VDE_1080))
		memcpy(currentPanelInfo, dlg_info, sizeof(drm_st_pnl_info));
	else {
		info = NULL;
		info = list_first_entry(panelInfoList, drm_st_pnl_info, list);
		if (!info)
			DRM_INFO("[%s, %d]: panel info list is NULL\n",	__func__, __LINE__);
		else
			memcpy(currentPanelInfo, info, sizeof(drm_st_pnl_info));
	}

	kfree(dlg_info);

	return ret;
}


bool _isSameModeAndPanelInfo(struct drm_display_mode *mode,
		drm_st_pnl_info *pPanelInfo)
{
	bool isSameFlag = false;
	__u32 hsync_w = 0, vsync_w = 0, de_hstart = 0, de_vstart = 0;
	__u32 hfront_porch = 0, vfront_proch = 0;
	__u16 PanelInfo_de_hstart = 0, PanelInfo_de_vstart = 0;

	hfront_porch = mode->hsync_start - mode->hdisplay;
	vfront_proch = mode->vsync_start - mode->vdisplay;
	hsync_w = mode->hsync_end - mode->hsync_start;
	vsync_w = mode->vsync_end - mode->vsync_start;
	de_hstart = mode->htotal - mode->hdisplay - hfront_porch;
	de_vstart = mode->vtotal - mode->vdisplay - vfront_proch;

	if (pPanelInfo->hsync_st < (pPanelInfo->de_hstart + pPanelInfo->de_width))
		PanelInfo_de_hstart = pPanelInfo->de_hstart - pPanelInfo->hsync_st;
	else
		PanelInfo_de_hstart = pPanelInfo->de_hstart
		+ (pPanelInfo->typ_htt - pPanelInfo->hsync_st);

	if (pPanelInfo->vsync_st < (pPanelInfo->de_vstart + pPanelInfo->de_height))
		PanelInfo_de_vstart = pPanelInfo->de_vstart - pPanelInfo->vsync_st;
	else
		PanelInfo_de_vstart = pPanelInfo->de_vstart
		+ (pPanelInfo->typ_vtt - pPanelInfo->vsync_st);

	if (((pPanelInfo->typ_dclk/1000) == mode->clock)
		&& (pPanelInfo->de_width == mode->hdisplay)
		&& (pPanelInfo->typ_htt == mode->htotal)
		&& (pPanelInfo->de_height == mode->vdisplay)
		&& (pPanelInfo->typ_vtt == mode->vtotal)
		&& (pPanelInfo->hsync_w == hsync_w)
		&& (pPanelInfo->vsync_w == vsync_w)
		&& (PanelInfo_de_hstart == de_hstart)
		&& (PanelInfo_de_vstart == de_vstart)) {

		isSameFlag = true;
	}

	DRM_INFO("[%s, %d]: === IsSame: %d ====\n", __func__, __LINE__, isSameFlag);
	DRM_INFO("[%s, %d]: clock: %d,  %lld\n", __func__, __LINE__,
		mode->clock, (pPanelInfo->typ_dclk/1000));
	DRM_INFO("[%s, %d]: de_width: %d,  %d\n", __func__, __LINE__,
		mode->hdisplay, pPanelInfo->de_width);
	DRM_INFO("[%s, %d]: typ_htt: %d,  %d\n", __func__, __LINE__,
		mode->htotal, pPanelInfo->typ_htt);
	DRM_INFO("[%s, %d]: de_height: %d,  %d\n", __func__, __LINE__,
		mode->vdisplay, pPanelInfo->de_height);
	DRM_INFO("[%s, %d]: typ_vtt: %d,  %d\n", __func__, __LINE__,
		mode->vtotal, pPanelInfo->typ_vtt);
	DRM_INFO("[%s, %d]: hsync_w: %d,  %d\n", __func__, __LINE__,
		hsync_w, pPanelInfo->hsync_w);
	DRM_INFO("[%s, %d]: vsync_w: %d,  %d\n", __func__, __LINE__,
		vsync_w, pPanelInfo->vsync_w);
	DRM_INFO("[%s, %d]: de_hstart: %d,  %d\n", __func__, __LINE__,
		de_hstart, PanelInfo_de_hstart);
	DRM_INFO("[%s, %d]: de_vstart: %d,  %d\n", __func__, __LINE__,
		de_vstart, PanelInfo_de_vstart);

	return isSameFlag;
}
EXPORT_SYMBOL(_isSameModeAndPanelInfo);

int _copyPanelInfo(struct drm_crtc_state *crtc_state,
	drm_st_pnl_info *pCurrentInfo,
	struct list_head *pInfoList)
{
	drm_st_pnl_info *tmpInfo = NULL;
	int flag = 0;

	list_for_each_entry(tmpInfo, pInfoList, list) {

		if (_isSameModeAndPanelInfo(&crtc_state->mode, tmpInfo)) {
			memcpy(pCurrentInfo, tmpInfo, sizeof(drm_st_pnl_info));
			flag = 1;
			break;
		}

	}

	dump_panel_info(tmpInfo);

	if (!flag) {
		DRM_ERROR("[%s, %d]: panel info does not match.\n",
			__func__, __LINE__);
		return -1;
	} else
		return 0;

}

int _panelAddModes(struct drm_panel *panel,
	struct list_head *pInfoList,
	int *modeCouct)
{
	struct videomode _videomode = {0};
	const struct display_timing *displayTiming = NULL;
	drm_st_pnl_info *tmpInfo = NULL;
	struct drm_device *drmDevice = NULL;
	struct drm_connector *drmConnector = NULL;
	struct drm_display_mode *mode = NULL;
	__u32 u32tmp = 0;

	drmDevice = panel->drm;
	drmConnector = panel->connector;

	list_for_each_entry(tmpInfo, pInfoList, list) {

		displayTiming = &tmpInfo->displayTiming;
		if (displayTiming != NULL) {
			videomode_from_timing(displayTiming, &_videomode);
		} else {
			DRM_WARN("displayTiming NULL\n");
			return 0;
		}
		mode = drm_mode_create(drmDevice);

		if (!mode) {
			DRM_WARN("failed to create a new display mode.\n");

		} else {

			drm_display_mode_from_videomode(&_videomode, mode);

			if ((tmpInfo->typ_htt != 0) && (tmpInfo->typ_vtt != 0)) {
				u32tmp = tmpInfo->typ_htt * tmpInfo->typ_vtt / DEC_2;
				mode->vrefresh = (tmpInfo->typ_dclk + u32tmp) /
					tmpInfo->typ_htt / tmpInfo->typ_vtt;
			}
			mode->type = 0;
			if (tmpInfo->default_sel)
				mode->type |= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
			else
				mode->type |= DRM_MODE_TYPE_DRIVER;

			mode->width_mm = displayTiming->hactive.typ;
			mode->height_mm = displayTiming->vactive.typ;
			//directy access the real data
			mode->clock = (tmpInfo->typ_dclk)/1000;
			drm_mode_probed_add(drmConnector, mode);
			(*modeCouct)++;

			if (displayTiming->hactive.typ > drmConnector->display_info.width_mm) {
				drmConnector->display_info.width_mm = displayTiming->hactive.typ;
				drmConnector->display_info.height_mm = displayTiming->vactive.typ;
			}
		}
	}
	return 0;
}
