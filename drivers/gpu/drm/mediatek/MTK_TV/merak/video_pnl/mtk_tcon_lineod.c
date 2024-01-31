// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek tcon lineod driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include "mtk_tcon_lineod.h"
#include "mtk_tv_drm_video_panel.h"
#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_lineod.h"
#include "drv_scriptmgt.h"

#define ENABLE_PCID_AUTODOWNLOAD        (TRUE)

typedef struct {
	uint16_t u16Version;		//[15:8]: major, [7:0]: minor version
	uint8_t u8PcidEn;
	uint8_t u8PixelOdEn;		//0: do PCID, 1: do Pixel OD
	uint8_t u8PanelType;		//0: DRD, 1: V2LTD
	uint8_t u8SwapEn;		//Pixel OD input/output R/B swap
	uint8_t u8PcidMode;
	uint8_t u8XTR_En;
	uint16_t u16XTR_thresh0SamePol;
	uint16_t u16XTR_thresh1DiffPol;
	uint16_t u16XTR_threshWhite;
	uint16_t u16XTR_threshBlack;
	uint32_t u32YearMonthDay;	//[31:25]: reserved, [24:20]: year, [19:16]: month,
					//[15:11]: day, [10:0]: minute
	uint8_t u8VacEn;
	uint8_t u8Reverse;
} __packed ST_PCID_SUB_HEADER, *PST_PCID_SUB_HEADER;

typedef struct {
	uint16_t u16Version;		//[15:8]: major, [7:0]: minor version
	uint8_t u8LineOD_en;		//[0]: line od enable
	uint8_t u8SeparateFlag;		//[0]: channel separate flag
					//[1]: table separate flag (unused)
	uint32_t u32YearMonthDayMinute;	//[31:25]: reserved, [24:20]: year, [19:16]: month,
					//[15:11]: day, [10:0]: minute
	uint8_t u8TableMatrix;		//0: 19x19, 1: 17x17
	uint8_t u8RegionNumber;		//(Version>1 0: 1x1, 1: 2x8, 2:4X4)
					//(Version<=1 16: 4x4 or 2x8, 1: 1x1)
	uint8_t u8GainMatrix;		//0: 4x4, 1: 13x9
	uint8_t u8GainRegion;		//0: 1x1
	uint8_t u8TableBit;		//(Version>2 0: 8bit 1:12bit) (Version<=2 no this member)
} __packed ST_LINE_OD_SUB_HEADER, *PST_LINE_OD_SUB_HEADER;

bool g_bForcePcidDisable;
bool g_bForcePcidBypass;
bool g_bForceLineodDisable;

void _pcid_init(uint8_t *pu8Tbl, bool bPixelOverdriveEn, bool bXTREn)
{
	drv_hwreg_render_tcon_lineod_init_pcid(pu8Tbl, bPixelOverdriveEn, bXTREn);
}

void _set_pcid_bypass(bool bBypass)
{
	if (!bBypass && g_bForcePcidBypass)
		bBypass = true;

	drv_hwreg_render_tcon_lineod_set_pcid_bypass(bBypass);
}

bool _is_pcid_bypass(void)
{
	return drv_hwreg_render_tcon_lineod_is_pcid_bypass();
}

void _set_pcid_enable(bool bEnable)
{
	if (bEnable && g_bForcePcidDisable)
		bEnable = false;

	drv_hwreg_render_tcon_lineod_set_pcid_enable(bEnable);
}

bool _is_pcid_enable(void)
{
	return drv_hwreg_render_tcon_lineod_is_pcid_enable();
}

bool _write_pcid_tbl(
		uint8_t *pu8RTbl, uint8_t *pu8GTbl, uint8_t *pu8BTbl,
		EN_PCID_LUT_TYPE eLutType)
{
	bool bEnable;
	bool bRet;
	ST_PCID_LUT_INFO stLutInfo;

	if (!pu8RTbl) {
		TCON_ERROR("pu8RTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu8GTbl) {
		TCON_ERROR("pu8GTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu8BTbl) {
		TCON_ERROR("pu8BTbl is NULL parameter\n");
		return FALSE;
	}

	bEnable = _is_pcid_enable();
	_set_pcid_enable(false);    // PCID Disable

	memset(&stLutInfo, 0, sizeof(ST_PCID_LUT_INFO));
	stLutInfo.pu8BTbl = pu8RTbl;
	stLutInfo.pu8GTbl = pu8GTbl;
	stLutInfo.pu8RTbl = pu8BTbl;
	stLutInfo.u8LutSramSeg = (uint8_t)eLutType;
	bRet = (drv_hwreg_render_tcon_lineod_write_pcid_tbl(&stLutInfo) != 0) ? FALSE : TRUE;

	_set_pcid_enable(bEnable);    // PCID recover

	return bRet;
}

bool _pcid_setting_proc(uint8_t *pu8TconTab)
{
	PST_PCID_SUB_HEADER pstHeader = NULL;
	uint8_t *pu8PCIDTabA1 = NULL;
	uint8_t *pu8PCIDTabA2 = NULL;
	uint8_t *pu8PCIDTabA3 = NULL;
	uint8_t *pu8PCIDTabB1 = NULL;
	uint8_t *pu8PCIDTabB2 = NULL;
	uint8_t *pu8PCIDTabB3 = NULL;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	PNL_MALLOC_MEM(pu8PCIDTabA1, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8PCIDTabA2, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8PCIDTabA3, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8PCIDTabB1, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8PCIDTabB2, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8PCIDTabB3, PCID_TABLE_SIZE_17X17 * sizeof(uint8_t), bRet);

	if (bRet) {
		uint8_t u8PixelOverDriveLutTab[PCID_PIXEL_OD_POLARITY_LUT_SIZE] = {0};
		uint16_t u16TargetIndex = 0;

		uint16_t u16Version                   = *((unsigned char *)(pu8TconTab));
		uint8_t u8PCID_En                     = *((unsigned char *)(pu8TconTab+2));
		uint8_t u8Pixel_OD_En                 = *((unsigned char *)(pu8TconTab+3));
		uint8_t u8PanelType                   = *((unsigned char *)(pu8TconTab+4));
		uint8_t u8Swap_En                     = *((unsigned char *)(pu8TconTab+5));
		uint8_t u8PCIDMode                    = *((unsigned char *)(pu8TconTab+6));
		uint8_t u8XTR_En                      = *((unsigned char *)(pu8TconTab+7));
		uint16_t u16XTR_threshold0_polarity   = *((unsigned char *)(pu8TconTab+8));
		uint16_t u16XTR_threshold1_polarity   = *((unsigned char *)(pu8TconTab+10));

		uint16_t u16XTR_thresholdW            = *((unsigned char *)(pu8TconTab+12));
		uint16_t u16XTR_thresholdB            = *((unsigned char *)(pu8TconTab+14));
		//uint32_t u32YearMonthDay              = *((unsigned char *)(pu8TconTab+16));
		uint8_t u8Reverse                     = *((unsigned char *)(pu8TconTab+20));

		pstHeader = (PST_PCID_SUB_HEADER)pu8TconTab;

		TCON_DEBUG("u16Version=%d -> %d\n", u16Version, pstHeader->u16Version);
		TCON_DEBUG("u8PCID_En=%d -> %d\n", u8PCID_En, pstHeader->u8PcidEn);
		TCON_DEBUG("u8Pixel_OD_En=%d -> %d\n", u8Pixel_OD_En, pstHeader->u8PixelOdEn);
		TCON_DEBUG("u8PanelType=%d -> %d\n", u8PanelType, pstHeader->u8PanelType);
		TCON_DEBUG("u8Swap_En=%d -> %d\n", u8Swap_En, pstHeader->u8SwapEn);
		TCON_DEBUG("u8PCIDMode=%d -> %d\n", u8PCIDMode, pstHeader->u8PcidMode);
		TCON_DEBUG("u8XTR_En=%d -> %d\n", u8XTR_En, pstHeader->u8XTR_En);

		TCON_DEBUG("u16XTR_threshold0_polarity=%d -> %d\n"
			"u16XTR_threshold1_polarity=%d -> %d\n",
			u16XTR_threshold0_polarity, pstHeader->u16XTR_thresh0SamePol,
			u16XTR_threshold1_polarity, pstHeader->u16XTR_thresh1DiffPol);
		TCON_DEBUG("u16XTR_thresholdW=%d -> %d, u16XTR_thresholdB=%d -> %d\n",
				u16XTR_thresholdW, pstHeader->u16XTR_threshWhite,
				u16XTR_thresholdB, pstHeader->u16XTR_threshBlack);
		TCON_DEBUG("u8Reverse=%d -> %d\n", u8Reverse, pstHeader->u8Reverse);

		u16TargetIndex = PCID_SKIP_HEADER;
		memcpy(pu8PCIDTabA1, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID R table for lut0

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(pu8PCIDTabA2, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID G table for lut0

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(pu8PCIDTabA3, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID B table for lut0

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(pu8PCIDTabB1, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID R table for lut1

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(pu8PCIDTabB2, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID G table for lut1

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(pu8PCIDTabB3, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_TABLE_SIZE_17X17); //Load PCID B table for lut1

		u16TargetIndex += PCID_TABLE_SIZE_17X17;
		memcpy(&u8PixelOverDriveLutTab, (pu8TconTab+u16TargetIndex),
			sizeof(uint8_t)*PCID_PIXEL_OD_POLARITY_LUT_SIZE);

		_pcid_init(u8PixelOverDriveLutTab, u8Pixel_OD_En, u8XTR_En);
		bRet &= _write_pcid_tbl(pu8PCIDTabA1, pu8PCIDTabA2, pu8PCIDTabA3, E_PCID_LUT0);
		bRet &= _write_pcid_tbl(pu8PCIDTabB1, pu8PCIDTabB2, pu8PCIDTabB3, E_PCID_LUT1);

		if (bRet)
			_set_pcid_enable(u8PCID_En);
	} else
		TCON_ERROR("Allocate buffer return fail\n.");

	PNL_FREE_MEM(pu8PCIDTabA1);
	PNL_FREE_MEM(pu8PCIDTabA2);
	PNL_FREE_MEM(pu8PCIDTabA3);
	PNL_FREE_MEM(pu8PCIDTabB1);
	PNL_FREE_MEM(pu8PCIDTabB2);
	PNL_FREE_MEM(pu8PCIDTabB3);

	return bRet;
}

void _set_lineod_enable(bool bEnable)
{
	if (bEnable && g_bForceLineodDisable)
		bEnable = false;

	drv_hwreg_render_tcon_lineod_set_lineod_enable(bEnable);
}

bool _is_lineod_enable(void)
{
	return drv_hwreg_render_tcon_lineod_is_lineod_enable();
}

bool _write_lineod_tbl(
			uint8_t *pu8EvenRTbl, uint8_t *pu8EvenGTbl, uint8_t *pu8EvenBTbl,
			uint8_t *pu8OddRTbl, uint8_t *pu8OddGTbl, uint8_t *pu8OddBTbl,
			uint8_t u8LutSramSeg)
{
	bool bRet;
	bool bPcidEnable, bLineODEnable;
	ST_PCID_LUT_INFO stLutInfo;

	if (RGB_LUT_CHECK(pu8EvenRTbl, pu8EvenGTbl, pu8EvenBTbl) ||
		RGB_LUT_CHECK(pu8OddRTbl, pu8OddGTbl, pu8OddBTbl)) {
		TCON_ERROR("Table error\n.");
		return FALSE;
	}

	memset(&stLutInfo, 0, sizeof(ST_PCID_LUT_INFO));

	bPcidEnable = _is_pcid_enable();
	bLineODEnable = _is_lineod_enable();

	_set_pcid_enable(false);        // PCID Disable
	_set_lineod_enable(false);      // LINE OD Disable

	u8LutSramSeg = 0;

	stLutInfo.pu8RTbl = pu8EvenRTbl;
	stLutInfo.pu8GTbl = pu8EvenGTbl;
	stLutInfo.pu8BTbl = pu8EvenBTbl;
	stLutInfo.pu8RTbl1 = pu8OddRTbl;
	stLutInfo.pu8GTbl1 = pu8OddGTbl;
	stLutInfo.pu8BTbl1 = pu8OddBTbl;
	stLutInfo.u8LutSramSeg = u8LutSramSeg;
	bRet = (drv_hwreg_render_tcon_lineod_write_lineod_full_tbl(&stLutInfo) != 0) ?
		FALSE : TRUE;

	_set_pcid_enable(bPcidEnable);      // PCID recover
	_set_lineod_enable(bLineODEnable);  // LINE OD recover
	return bRet;
}

bool _write_lineod_single_tbl(
			uint16_t *pu16RTbl, uint16_t *pu16GTbl, uint16_t *pu16BTbl,
			uint8_t u8LutSramSeg)
{
	bool bRet;
	bool bPcidEnable, bLineODEnable;
	ST_PCID_LUT_INFO stLutInfo;

	if (!pu16RTbl) {
		TCON_ERROR("pu16RTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16GTbl) {
		TCON_ERROR("pu16GTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16BTbl) {
		TCON_ERROR("pu16BTbl is NULL parameter\n");
		return FALSE;
	}

	bPcidEnable = _is_pcid_enable();
	bLineODEnable = _is_lineod_enable();

	if (!bPcidEnable || !bLineODEnable) {
		TCON_DEBUG("Pcid enable=%d lineOD enable=%d\n", bPcidEnable, bLineODEnable);
		return FALSE;
	}

	_set_pcid_enable(false);        // PCID Disable
	_set_lineod_enable(false);      // LINE OD Disable

	stLutInfo.pu16RTbl = pu16RTbl;
	stLutInfo.pu16GTbl = pu16GTbl;
	stLutInfo.pu16BTbl = pu16BTbl;
	stLutInfo.u8LutSramSeg = u8LutSramSeg;
	bRet = (drv_hwreg_render_tcon_lineod_write_lineod_single_tbl(&stLutInfo) != 0) ?
		FALSE : TRUE;

	_set_pcid_enable(bPcidEnable);      // PCID recover
	_set_lineod_enable(bLineODEnable);  // LINE OD recover

	return bRet;
}

bool _dump_lineod_gain_tbl(uint8_t *pu8ODGainTbl, uint16_t u16GainTableSize)
{
	bool bGainEnable = FALSE;

	if (!pu8ODGainTbl) {
		TCON_ERROR("pu8ODGainTbl is NULL parameter\n");
		return FALSE;
	}

	//Note:REG_SC_EXT_BK2D/332D/(Bank =104F Sub-Bank=2D)/0x302f00 0x2d -->
	//Bank = A02F Sub-Bank=A8/0xA3A8

	bGainEnable = drv_hwreg_render_tcon_lineod_is_gain_tbl_enable();

	drv_hwreg_render_tcon_lineod_write_gain_tbl(pu8ODGainTbl, u16GainTableSize);

	//gain enable status recovery
	drv_hwreg_render_tcon_lineod_set_gain_tbl_enable(bGainEnable);

	return TRUE;
}

// PCID Auto Download Format
// 181 * 3 * 32 bytes = 0x43E0 (R/G/B channels)
//|<--------------32 bytes------------->|
//    SRAM1/SRAM4        SRAM2/SRAM3
//   lut0 .... lut9     lut0 .... lut9
//0   0   ....  0         1  ....  1
//.
//89  322 ....  322      359 .... 359
//                      ---------------
//90  358 ....  358       19 ....  19
//.
//99  360 ....  360       37 ....  37
//--------------------
//100 20  ....   20       57 ....  57
//.
//179 338 ....   338     341 ....  341
//                       ---------------
//180 340 ....   340       x ....  x
//--------------------------------------
bool _fill_to_adl_sram14_data(PST_SRAM_DATA_FORMAT_INFO pstInfo)
{
	bool bRet = TRUE;
	ST_PCID_12BIT_DRAM_FORMAT *pstData = NULL;
	uint16_t u8TableIdx = 0;

	if (!pstInfo) {
		TCON_ERROR("pstInfo is NULL parameter\n");
		return FALSE;
	}

	pstData = pstInfo->pstData;
	u8TableIdx = pstInfo->u8TableIdx;

	if (u8TableIdx == 0) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut0 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut0 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut0 = pstInfo->u16BVal;
	} else if (u8TableIdx == 1) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut1 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut1 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut1 = pstInfo->u16BVal;
	} else if (u8TableIdx == 2) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut2 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut2 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut2 = pstInfo->u16BVal;
	} else if (u8TableIdx == 3) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut3 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut3 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut3 = pstInfo->u16BVal;
	} else if (u8TableIdx == 4) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut4 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut4 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut4 = pstInfo->u16BVal;
	} else if (u8TableIdx == 5) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut5 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut5 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut5 = pstInfo->u16BVal;
	} else if (u8TableIdx == 6) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut6 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut6 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut6 = pstInfo->u16BVal;
	} else if (u8TableIdx == 7) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut7 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut7 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut7 = pstInfo->u16BVal;
	} else if (u8TableIdx == 8) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut8 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut8 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut8 = pstInfo->u16BVal;
	} else if (u8TableIdx == 9) {
		pstData[pstInfo->u16RIndex].u16SRAM1_lut9 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM1_lut9 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM1_lut9 = pstInfo->u16BVal;
	}

	return bRet;
}

bool _fill_to_adl_sram23_data(PST_SRAM_DATA_FORMAT_INFO pstInfo)
{
	bool bRet = TRUE;
	ST_PCID_12BIT_DRAM_FORMAT *pstData = 0;
	uint16_t u8TableIdx = 0;

	if (!pstInfo) {
		TCON_ERROR("pstInfo is NULL parameter\n");
		return FALSE;
	}

	pstData = pstInfo->pstData;
	u8TableIdx = pstInfo->u8TableIdx;

	if (u8TableIdx == 0) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut0 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut0 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut0 = pstInfo->u16BVal;
	} else if (u8TableIdx == 1) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut1 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut1 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut1 = pstInfo->u16BVal;
	} else if (u8TableIdx == 2) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut2 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut2 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut2 = pstInfo->u16BVal;
	} else if (u8TableIdx == 3) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut3 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut3 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut3 = pstInfo->u16BVal;
	} else if (u8TableIdx == 4) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut4 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut4 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut4 = pstInfo->u16BVal;
	} else if (u8TableIdx == 5) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut5 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut5 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut5 = pstInfo->u16BVal;
	} else if (u8TableIdx == 6) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut6 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut6 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut6 = pstInfo->u16BVal;
	} else if (u8TableIdx == 7) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut7 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut7 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut7 = pstInfo->u16BVal;
	} else if (u8TableIdx == 8) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut8 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut8 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut8 = pstInfo->u16BVal;
	} else if (u8TableIdx == 9) {
		pstData[pstInfo->u16RIndex].u16SRAM2_lut9 = pstInfo->u16RVal;
		pstData[pstInfo->u16GIndex].u16SRAM2_lut9 = pstInfo->u16GVal;
		pstData[pstInfo->u16BIndex].u16SRAM2_lut9 = pstInfo->u16BVal;
	}

	return bRet;
}

bool _arrange_adl_pcid_settbl(
			ST_PCID_12BIT_DRAM_FORMAT *pstPCID_ADL_CMD,
			uint16_t *pu16RTbl, uint16_t *pu16GTbl, uint16_t *pu16BTbl,
			uint16_t u16TableSize, uint8_t u8TableIdx)
{
	bool bRet = TRUE;
	uint16_t u16CodeTableX = 0;
	uint16_t u16CodeTableY = 0;
	uint16_t u16lutIndex  = 0;
	uint16_t u16Sram1Idx = 0, u16Sram2Idx = 0, u16Sram3Idx = 0, u16Sram4Idx = 0;
	uint16_t u16ROW_MAX = 0, u16COL_MAX = 0;
	uint16_t u16SRAM3_Offset = 0, u16SRAM4_Offset = 0, u16Channel_Offset = 0;
	ST_SRAM_DATA_FORMAT_INFO stInfo;

	if (!pstPCID_ADL_CMD) {
		TCON_ERROR("pstPCID_ADL_CMD is NULL parameter\n");
		return FALSE;
	}

	if (!pu16RTbl) {
		TCON_ERROR("pu16RTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16GTbl) {
		TCON_ERROR("pu16GTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16BTbl) {
		TCON_ERROR("pu16BTbl is NULL parameter\n");
		return FALSE;
	}

	memset(&stInfo, 0, sizeof(stInfo));
	stInfo.pstData = pstPCID_ADL_CMD;
	stInfo.u8TableIdx = u8TableIdx;

	u16ROW_MAX = (u16TableSize == LINE_OD_TABLE_SIZE_19X19) ?
				LINE_OD_TABLE_ROW_10 : LINE_OD_TABLE_ROW_9;

	u16COL_MAX = (u16TableSize == LINE_OD_TABLE_SIZE_19X19) ?
				LINE_OD_TABLE_COL_10 : LINE_OD_TABLE_COL_9;

	u16Channel_Offset = (u16TableSize == LINE_OD_TABLE_SIZE_19X19) ?
				XC_AUTODOWNLOAD_PCID_19X19_OFFSET :
				XC_AUTODOWNLOAD_PCID_17X17_OFFSET;

	u16SRAM3_Offset = u16ROW_MAX*(u16COL_MAX - 1);
	u16SRAM4_Offset = u16ROW_MAX*u16COL_MAX;

	for (u16CodeTableY = 0; u16CodeTableY < u16ROW_MAX; u16CodeTableY++) {
		for (u16CodeTableX = 0; u16CodeTableX < u16COL_MAX; u16CodeTableX++) {
			//sram1
			u16lutIndex = u16CodeTableX * 2 +
					u16CodeTableY * (u16COL_MAX*2-1) * 2;

			if (u16lutIndex >= LINE_OD_TABLE_SIZE_19X19) {
				TCON_ERROR("lutIndex error=%d\n", u16lutIndex);
				return FALSE;
			}

			stInfo.u16RIndex = u16Sram1Idx;
			stInfo.u16GIndex = u16Sram1Idx + u16Channel_Offset;
			stInfo.u16BIndex = u16Sram1Idx + u16Channel_Offset*2;
			stInfo.u16RVal = pu16RTbl[u16lutIndex];
			stInfo.u16GVal = pu16GTbl[u16lutIndex];
			stInfo.u16BVal = pu16BTbl[u16lutIndex];

			_fill_to_adl_sram14_data(&stInfo);
			u16Sram1Idx++;

			//sram2
			if (u16CodeTableX < u16COL_MAX-1) {
				u16lutIndex = u16CodeTableX * 2 +
				u16CodeTableY * (u16COL_MAX*2-1) * 2 + 1;

				stInfo.u16RIndex = u16Sram2Idx;
				stInfo.u16GIndex = u16Sram2Idx + u16Channel_Offset;
				stInfo.u16BIndex = u16Sram2Idx + u16Channel_Offset*2;
				stInfo.u16RVal = pu16RTbl[u16lutIndex];
				stInfo.u16GVal = pu16GTbl[u16lutIndex];
				stInfo.u16BVal = pu16BTbl[u16lutIndex];

				_fill_to_adl_sram23_data(&stInfo);
				u16Sram2Idx++;
			}

			//sram3
			if (u16CodeTableY < u16ROW_MAX-1) {
				u16lutIndex = u16CodeTableX * 2 +
					u16CodeTableY * (u16COL_MAX*2-1) * 2 + (u16COL_MAX*2-1);

				stInfo.u16RIndex = u16Sram3Idx + u16SRAM3_Offset;
				stInfo.u16GIndex = u16Sram3Idx + u16SRAM3_Offset +
							u16Channel_Offset;
				stInfo.u16BIndex = u16Sram3Idx + u16SRAM3_Offset +
							u16Channel_Offset*2;
				stInfo.u16RVal = pu16RTbl[u16lutIndex];
				stInfo.u16GVal = pu16GTbl[u16lutIndex];
				stInfo.u16BVal = pu16BTbl[u16lutIndex];

				_fill_to_adl_sram23_data(&stInfo);
				u16Sram3Idx++;
			}

			//sram4
			if (u16CodeTableX < u16COL_MAX-1 && u16CodeTableY < u16ROW_MAX-1) {
				u16lutIndex = u16CodeTableX * 2 +
					u16CodeTableY * (u16COL_MAX*2-1) * 2 + (u16COL_MAX*2-1) + 1;

				stInfo.u16RIndex = u16Sram4Idx + u16SRAM4_Offset;
				stInfo.u16GIndex = u16Sram4Idx + u16SRAM4_Offset +
							u16Channel_Offset;
				stInfo.u16BIndex = u16Sram4Idx + u16SRAM4_Offset +
							u16Channel_Offset*2;
				stInfo.u16RVal = pu16RTbl[u16lutIndex];
				stInfo.u16GVal = pu16GTbl[u16lutIndex];
				stInfo.u16BVal = pu16BTbl[u16lutIndex];

				_fill_to_adl_sram14_data(&stInfo);
				u16Sram4Idx++;
			}
		}
	}

	return bRet;
}

bool _prepare_adl_data_format(
			ST_PCID_12BIT_DRAM_FORMAT *pstPCID_ADL_CMD,
			uint16_t *pu16RTbl, uint16_t *pu16GTbl, uint16_t *pu16BTbl,
			uint16_t u16TableSize, uint8_t u8TableNum)
{
	bool bRet = TRUE;

	if (!pstPCID_ADL_CMD) {
		TCON_ERROR("pstPCID_ADL_CMD is NULL parameter\n");
		return FALSE;
	}

	if (!pu16RTbl) {
		TCON_ERROR("pu16RTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16GTbl) {
		TCON_ERROR("pu16GTbl is NULL parameter\n");
		return FALSE;
	}

	if (!pu16BTbl) {
		TCON_ERROR("pu16BTbl is NULL parameter\n");
		return FALSE;
	}

	if (u8TableNum < LINE_OD_TOTAL_LUT_NUM) {
		_arrange_adl_pcid_settbl(
				pstPCID_ADL_CMD,
				pu16RTbl, pu16GTbl, pu16BTbl,
				u16TableSize, u8TableNum);
	} else
		TCON_ERROR("Table Num=%d over the range(=%d)\n", u8TableNum, LINE_OD_TOTAL_LUT_NUM);

	return bRet;
}

uint8_t _get_table_num(PST_LINE_OD_SUB_HEADER pstHeader, bool bTableSeparate)
{
	uint8_t u8TableNum = 0;

	if (!pstHeader) {
		TCON_ERROR("input pstHeader parameter is null\n");
		return u8TableNum;
	}

	if ((pstHeader->u16Version > 2) && (pstHeader->u8TableBit == LINE_OD_TABLE_UNIT_SIZE_12)) {
		if (pstHeader->u8RegionNumber == 0)
			u8TableNum = LINE_OD_REG_NUM_1X1;
		else if (pstHeader->u8RegionNumber == 1)
			u8TableNum = LINE_OD_REG_NUM_2X8;
		else if (pstHeader->u8RegionNumber == 2)
			u8TableNum = LINE_OD_REG_NUM_4X4;
		else if (pstHeader->u8RegionNumber == 3)
			u8TableNum = LINE_OD_REG_NUM_10X1;
		else
			u8TableNum = LINE_OD_REG_NUM_1X1;
	} else {
		//after verion 1 region number define different
		if (pstHeader->u16Version > 1) {
			if (pstHeader->u8RegionNumber == 0)
				u8TableNum = LINE_OD_REG_NUM_1X1;
			else if (pstHeader->u8RegionNumber == 1)
				u8TableNum = LINE_OD_REG_NUM_2X8;
			else if (pstHeader->u8RegionNumber == 2)
				u8TableNum = LINE_OD_REG_NUM_4X4;
			else
				u8TableNum = LINE_OD_REG_NUM_1X1;
		} else
			u8TableNum = (bTableSeparate ? pstHeader->u8RegionNumber : 1);
	}
	TCON_DEBUG("table num=%d\n", u8TableNum);

	return u8TableNum;
}

bool _gain_table_setting(
			PST_LINE_OD_SUB_HEADER pstHeader,
			uint8_t *pu8TconTab, uint16_t u16TargetIndex)
{
	bool bRet = TRUE;
	uint8_t *pu8Gain = NULL;
	uint16_t u16GainTableSize = 0;

	if (!pstHeader) {
		TCON_ERROR("pstHeader is NULL parameter\n");
		return FALSE;
	}

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	if (pstHeader->u16Version > 1) {
		if (pstHeader->u8GainMatrix == 0)
			u16GainTableSize = LINE_OD_GAIN_TABLE_SIZE_4X4;
		else if (pstHeader->u8GainMatrix == 1)
			u16GainTableSize = LINE_OD_GAIN_TABLE_SIZE_13X9;
		else
			u16GainTableSize = LINE_OD_GAIN_TABLE_SIZE_13X9;

		PNL_MALLOC_MEM(pu8Gain, u16GainTableSize*sizeof(uint8_t), bRet);
		if (bRet) {
			memcpy(pu8Gain,
				(pu8TconTab + u16TargetIndex),
				sizeof(uint8_t)*u16GainTableSize);
			bRet = _dump_lineod_gain_tbl(pu8Gain, u16GainTableSize);
		}
		PNL_FREE_MEM(pu8Gain);
	}

	return bRet;
}

bool _is_lineod_adl_support(void)
{
	return is_adl_support(E_SM_TCON_ADL_PCID);
}

bool _trigger_lineod_tbl_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize)
{
	bool bRet;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	bRet = set_adl_proc(pu8Tbl, u32TblSize,
			E_SM_TCON_ADL_PCID, E_SM_ADL_TRIGGER_MODE);

	return bRet;
}

bool _lineod_table_setting_ver2(
			PST_LINE_OD_SUB_HEADER pstHeader,
			uint8_t *pu8TconTab,
			bool bChannelSeparate, bool bTableSeparate,
			uint16_t u16TableSize)
{
	bool bRet = TRUE;
	uint16_t u16TargetIndex = 0;
	uint8_t u8TabIndex, u8TableNum;
	uint16_t *pu16RLut = NULL;
	uint16_t *pu16GLut = NULL;
	uint16_t *pu16BLut = NULL;
	uint32_t u32AdlSize = 0;
	ST_PCID_12BIT_DRAM_FORMAT *pstPCID_ADL_CMD = NULL;
	uint8_t u8LutSramSeg;
	bool bSupportAdl = _is_lineod_adl_support();

	TCON_FUNC_ENTER();

	PNL_MALLOC_MEM(pu16RLut, u16TableSize*sizeof(uint16_t), bRet);
	PNL_MALLOC_MEM(pu16GLut, u16TableSize*sizeof(uint16_t), bRet);
	PNL_MALLOC_MEM(pu16BLut, u16TableSize*sizeof(uint16_t), bRet);

	if (!bRet)
		goto finally;

	u16TargetIndex = sizeof(ST_LINE_OD_SUB_HEADER);
	u8TableNum = _get_table_num(pstHeader, bTableSeparate);

	if (bSupportAdl) {
		if (u16TableSize == LINE_OD_TABLE_SIZE_17X17)
			u32AdlSize = XC_AUTODOWNLOAD_PCID_17X17_OFFSET;// 32 bytes * 145 * 3
		else
			u32AdlSize = XC_AUTODOWNLOAD_PCID_19X19_OFFSET;// 32 bytes * 181 * 3

		u32AdlSize = u32AdlSize * sizeof(ST_PCID_12BIT_DRAM_FORMAT) * PCID_RGB_CHANNEL;

		if (!pstPCID_ADL_CMD) {
			PNL_MALLOC_MEM(pstPCID_ADL_CMD, u32AdlSize, bRet);
			if (!bRet) {
				TCON_ERROR("Autodownload allocate mem fail\n");
				goto finally;
			}
		}
		memset(pstPCID_ADL_CMD, 0, u32AdlSize);
	}

	u8TabIndex = 0;
	while (u8TabIndex < u8TableNum) {
		if (bChannelSeparate) {
			memcpy(pu16RLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //R
			u16TargetIndex += u16TableSize*2;
			memcpy(pu16GLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //G
			u16TargetIndex += u16TableSize*2;
			memcpy(pu16BLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //B
			u16TargetIndex += u16TableSize*2;
		} else {
			memcpy(pu16RLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //R
			memcpy(pu16GLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //G
			memcpy(pu16BLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint16_t)*u16TableSize); //B
			u16TargetIndex += u16TableSize*2;
		}

		if (bSupportAdl) {
			bRet = _prepare_adl_data_format(pstPCID_ADL_CMD,
					pu16RLut, pu16GLut, pu16BLut,
					u16TableSize, u8TabIndex);
			if (!bRet)
				TCON_ERROR("_prepare_adl_data_format u8TabIndex=%d\n.",
					u8TabIndex);
		} else {
			u8LutSramSeg = u8TabIndex;
			bRet = _write_lineod_single_tbl(
					pu16RLut, pu16GLut, pu16BLut, u8LutSramSeg);
			if (!bRet)
				TCON_ERROR("_dump_lineod_single_tbl return fail\n.");
		}
		u8TabIndex += 1;
	}

	_set_lineod_enable(FALSE);
	TCON_DEBUG("lineod enable=%d ...\n", _is_lineod_enable());

	if (bSupportAdl)
		bRet &= _trigger_lineod_tbl_adl_proc((uint8_t *)pstPCID_ADL_CMD, u32AdlSize);

	//verify sram data
	//if (bRet) {
	//	TCON_DEBUG("u32AdlSize=%d ....\n", u32AdlSize);
	//	drv_hwreg_render_tcon_lineod_check_sramdata(
	//	                    (uint8_t *)pstPCID_ADL_CMD, u16TableSize);
	//}

	TCON_DEBUG("support auto download =%d\n", bSupportAdl);
	_set_lineod_enable(TRUE);

	bRet &= _gain_table_setting(pstHeader, pu8TconTab, u16TargetIndex);

finally:
	//free allocated memory
	PNL_FREE_MEM(pstPCID_ADL_CMD);
	PNL_FREE_MEM(pu16RLut);
	PNL_FREE_MEM(pu16GLut);
	PNL_FREE_MEM(pu16BLut);

	TCON_FUNC_EXIT(bRet);

	return bRet;
}

bool _lineod_table_setting_ver1(
			PST_LINE_OD_SUB_HEADER pstHeader,
			uint8_t *pu8TconTab,
			bool bChannelSeparate, bool bTableSeparate,
			uint16_t u16TableSize)
{
	bool bRet = TRUE;
	uint8_t u8LutSramSeg;
	uint16_t u16TargetIndex = 0;
	uint8_t u8TabIndex, u8TableNum;
	uint8_t *pu8EvenRLut = NULL;
	uint8_t *pu8EvenGLut = NULL;
	uint8_t *pu8EvenBLut = NULL;
	uint8_t *pu8OddRLut = NULL;
	uint8_t *pu8OddGLut = NULL;
	uint8_t *pu8OddBLut = NULL;

	TCON_FUNC_ENTER();

	PNL_MALLOC_MEM(pu8EvenRLut, u16TableSize*sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8EvenGLut, u16TableSize*sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8EvenBLut, u16TableSize*sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8OddRLut, u16TableSize*sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8OddGLut, u16TableSize*sizeof(uint8_t), bRet);
	PNL_MALLOC_MEM(pu8OddBLut, u16TableSize*sizeof(uint8_t), bRet);

	if (!bRet)
		goto finally;

	u16TargetIndex = sizeof(ST_LINE_OD_SUB_HEADER);
	u8TableNum = _get_table_num(pstHeader, bTableSeparate);
	u8TabIndex = 0;

	while (u8TabIndex < u8TableNum) {
		if (bChannelSeparate) {
			//even lut
			memcpy(pu8EvenRLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //R
			u16TargetIndex += u16TableSize;
			memcpy(pu8EvenGLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //G
			u16TargetIndex += u16TableSize;
			memcpy(pu8EvenBLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //B
			u16TargetIndex += u16TableSize;

			//avoid to copy last odd lut table that doesn't exist
			if ((u8TabIndex + 1) != u8TableNum) {
				//odd lut
				memcpy(pu8OddRLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //R
				u16TargetIndex += u16TableSize;
				memcpy(pu8OddGLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //G
				u16TargetIndex += u16TableSize;
				memcpy(pu8OddBLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //B
				u16TargetIndex += u16TableSize;
			}
		} else {
			//even lut
			memcpy(pu8EvenRLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //R
			memcpy(pu8EvenGLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //G
			memcpy(pu8EvenBLut, (pu8TconTab+u16TargetIndex),
				sizeof(uint8_t)*u16TableSize); //B
			u16TargetIndex += u16TableSize;

			//avoid to copy last odd lut table that doesn't exist
			if ((u8TabIndex + 1) != u8TableNum) {
				//odd lut
				memcpy(pu8OddRLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //R
				memcpy(pu8OddGLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //G
				memcpy(pu8OddBLut, (pu8TconTab+u16TargetIndex),
					sizeof(uint8_t)*u16TableSize); //B
				u16TargetIndex += u16TableSize;
			}
		}

		u8LutSramSeg = u8TabIndex/2;
		bRet = _write_lineod_tbl(
				pu8EvenRLut, pu8EvenGLut, pu8EvenBLut,
				pu8OddRLut, pu8OddGLut, pu8OddBLut, u8LutSramSeg);

		u8TabIndex += 2;
	}

	bRet &= _gain_table_setting(pstHeader, pu8TconTab, u16TargetIndex);

finally:
	//free allocated memory
	PNL_FREE_MEM(pu8EvenRLut);
	PNL_FREE_MEM(pu8EvenGLut);
	PNL_FREE_MEM(pu8EvenBLut);
	PNL_FREE_MEM(pu8OddRLut);
	PNL_FREE_MEM(pu8OddGLut);
	PNL_FREE_MEM(pu8OddBLut);

	TCON_FUNC_EXIT(bRet);

	return bRet;
}

bool _lineod_table_setting_proc(uint8_t *pu8TconTab)
{
	PST_LINE_OD_SUB_HEADER pstLineODSubHdr = NULL;
	bool bRetLut = TRUE, bChannelSeparate = FALSE, bTableSeparate = FALSE;
	uint16_t u16TableSize;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	pstLineODSubHdr = (PST_LINE_OD_SUB_HEADER)pu8TconTab;
	bChannelSeparate = pstLineODSubHdr->u8SeparateFlag & BIT(0);
	bTableSeparate = (pstLineODSubHdr->u8SeparateFlag & BIT(1)) >> 1;

	TCON_DEBUG("\nVersion=%d LineOD_en=%d\n"
		"channel separate=%d, table separate=%d\n"
		"year=%d month=%d day=%d minute=%d\n"
		"table matrix=%d region number=%d\n",
		pstLineODSubHdr->u16Version, pstLineODSubHdr->u8LineOD_en,
		bChannelSeparate, bTableSeparate,
		(pstLineODSubHdr->u32YearMonthDayMinute & 0x1F00000)>>20,
		(pstLineODSubHdr->u32YearMonthDayMinute & 0xF0000)>>16,
		(pstLineODSubHdr->u32YearMonthDayMinute & 0xF800)>>11,
		(pstLineODSubHdr->u32YearMonthDayMinute & 0x7FF),
		pstLineODSubHdr->u8TableMatrix, pstLineODSubHdr->u8RegionNumber);

	//after verion 2 add new member table unit
	if (pstLineODSubHdr->u16Version > 2) {
		//Table Bit: 0: 8 bits, 1: 12 bits
		TCON_DEBUG("table bit=%d\n", pstLineODSubHdr->u8TableBit);
	}

	if (pstLineODSubHdr->u16Version > 1) {
		//Gain Matrix Size: 0: 4x4, 1: 13x9
		TCON_DEBUG("gain matrix=%d\n", pstLineODSubHdr->u8GainMatrix);
		TCON_DEBUG("gain region number=%d\n", pstLineODSubHdr->u8GainRegion);
	}
	u16TableSize = (pstLineODSubHdr->u8TableMatrix ?
			LINE_OD_TABLE_SIZE_17X17 : LINE_OD_TABLE_SIZE_19X19);
	TCON_DEBUG("table size=%d\n", u16TableSize);

	if ((pstLineODSubHdr->u16Version > 2) &&
		(pstLineODSubHdr->u8TableBit == LINE_OD_TABLE_UNIT_SIZE_12))
		bRetLut &= _lineod_table_setting_ver2(
					pstLineODSubHdr,
					pu8TconTab,
					bChannelSeparate, bTableSeparate, u16TableSize);
	else
		bRetLut &= _lineod_table_setting_ver1(
					pstLineODSubHdr,
					pu8TconTab,
					bChannelSeparate, bTableSeparate, u16TableSize);

	TCON_FUNC_EXIT(bRetLut);

	return bRetLut;
}

bool _lineod_reg_setting_proc(uint8_t *pu8TconTab, uint16_t u16RegisterCount)
{
	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	drv_hwreg_render_tcon_lineod_write_reg_setting(pu8TconTab, u16RegisterCount);

	TCON_FUNC_EXIT(TRUE);
	return TRUE;
}

bool mtk_tcon_lineod_setting(struct mtk_panel_context *pCon)
{
	bool bRet = TRUE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	st_tcon_tab_info stInfo;

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	memset(&stInfo, 0, sizeof(st_tcon_tab_info));

	//do PCID setting process
	/***
	 * PCID mainly contains two major functions pixelOD / LineOD,
	 * which is mainly used to compensate for insufficient charging of CELL MAPPING,
	 * thereby reducing the crosstalk phenomenon
	 ***/
	if (is_tcon_data_exist(&pdata_buf, &data_len)) {
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_PCID;

		if (get_tcon_dump_table(&stInfo))
			bRet &= _pcid_setting_proc(stInfo.pu8Table);
	}

	//do lineod reg. setting process
	if (is_tcon_data_exist(&pdata_buf, &data_len)) {
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_LINE_OD_REG;

		if (get_tcon_dump_table(&stInfo))
			bRet &= _lineod_reg_setting_proc(stInfo.pu8Table, stInfo.u16RegCount);
	}

	//do lineod table setting process
	if (is_tcon_data_exist(&pdata_buf, &data_len)) {
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_LINE_OD_TABLE;

		if (get_tcon_dump_table(&stInfo))
			bRet &= _lineod_table_setting_proc(stInfo.pu8Table);
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

int mtk_tcon_lineod_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val)
{
	int ret = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	switch (enType) {
	case E_TCON_CMD_LINEOD_EN:
		g_bForceLineodDisable = (u16Val) ? false : true;
		TCON_DEBUG("Force lineod disable=%d\n", g_bForceLineodDisable);
		_set_lineod_enable((bool)u16Val);
		break;
	case E_TCON_CMD_PCID_EN:
		g_bForcePcidDisable = (u16Val) ? false : true;
		TCON_DEBUG("Force pcid disable=%d\n", g_bForcePcidDisable);
		_set_pcid_enable((bool)u16Val);
		break;
	case E_TCON_CMD_PCID_BYPASS:
		g_bForcePcidBypass = (u16Val) ? true : false;
		TCON_DEBUG("Force pcid bypass=%d\n", g_bForcePcidBypass);
		_set_pcid_bypass((bool)u16Val);
		break;
	default:
		TCON_ERROR("Unknown cmd type=%d\n", enType);
		return -ENODEV;
	}

	return ret;
}

int mtk_tcon_lineod_get_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize)
{
	int ret = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	if (!pInfo || u32InfoSize == 0) {
		TCON_ERROR("pInfo is NULL or info size is zero\n");
		return -EINVAL;
	}

	ret = snprintf(pInfo, u32InfoSize,
		"Tcon lineod info:\n"
		"	- Pcid enable = %d\n"
		"	- Force pcid disable = %d\n"
		"	- Pcid bypass = %d\n"
		"	- Force pcid bypass = %d\n"
		"	- Lineod enable = %d\n"
		"	- Force lineod disable = %d\n",
		_is_pcid_enable(),
		g_bForcePcidDisable,
		_is_pcid_bypass(),
		g_bForcePcidBypass,
		_is_lineod_enable(),
		g_bForceLineodDisable);

	return ((ret < 0) || (ret > u32InfoSize)) ? -EINVAL : ret;
}
