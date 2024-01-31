// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek tcon panel gamma driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include "mtk_tcon_dga.h"
#include "mtk_tcon_common.h"
#include "mtk_tv_drm_video_panel.h"
#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_dga.h"
#include "drv_scriptmgt.h"

#define ENABLE_PNL_GAMMA_AUTODOWNLOAD   (TRUE)

uint8_t *pu8VRRGammaTbl;
bool g_bForceDgaDisable;
bool g_bForceDgaBypass;

typedef struct {
	uint8_t u8GammaType;
	uint8_t u8Table2DNumber;
	uint8_t u8Table3DNumber;
	uint8_t u8TableFormat;
	uint16_t u16PreTableOrignalSize;
	uint16_t u16PreTableAlignedSize;
	uint32_t u32Table2DStartAddress;
	uint32_t u32Table3DStartAddress;
	uint8_t u8BinVersion_Minor2;
	uint8_t u8BinVersion_Minor1;
	uint8_t u8BinVersion_Major;
	uint8_t u8PanelSize;
	uint16_t u16Minute : 11;
	uint8_t u8Day : 5;
	uint8_t u8Month : 4;
	uint8_t u8Year : 5;
	uint8_t u8Reserved0 : 7;
	bool bGamma2LutEnable : 1;
	uint8_t u8Gamma2LutMode : 2;
	uint32_t u32Reserved1 : 29;
	uint32_t u32Reserved2;
} __packed ST_PNL_GAMMA_CONFIG;

//Panel Gamma
void _set_panelgamma_bypass(bool bEnable)
{
	if (!bEnable && g_bForceDgaBypass) {
		TCON_DEBUG("Force bypass. set %d -> 1\n", bEnable);
		bEnable = true;
	}

	drv_hwreg_render_tcon_dga_set_bypass(bEnable);
}

bool _get_panelgamma_bypass(void)
{
	return drv_hwreg_render_tcon_dga_get_bypass();
}

void _set_panelgamma_enable(bool bEnable)
{
	if (bEnable && g_bForceDgaDisable) {
		TCON_DEBUG("Force disable. set %d -> 0\n", bEnable);
		bEnable = false;
	}

	drv_hwreg_render_tcon_dga_set_enable(bEnable);
}

bool _get_panelgamma_enable(void)
{
	return drv_hwreg_render_tcon_dga_get_enable();
}

bool _set_to_lut_mode(struct mtk_panel_context *pCon, ST_PNL_GAMMA_CONFIG *pstPNLGammaConfig)
{
	bool bRet = TRUE;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	if (pCon->hw_info.pnl_lib_version != VERSION4) {
		drv_hwreg_render_tcon_dga_set_gamma2lut_mode(pstPNLGammaConfig->u8Gamma2LutMode);
		drv_hwreg_render_tcon_dga_set_gamma2lut_en(pstPNLGammaConfig->bGamma2LutEnable);
	}
	return bRet;
}

bool _get_to_lut_enable(struct mtk_panel_context *pCon)
{
	bool bRet = FALSE;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	if (pCon->hw_info.pnl_lib_version != VERSION4)
		bRet = drv_hwreg_render_tcon_dga_get_gamma2lut_en();

	return bRet;
}

bool _is_support_w_channel(void)
{
	return SUPPORT_PANEL_GAMMA_W_CHANNEL;
}

bool is_channel0_tbl_valid(ST_PNL_PNLGAMMATBL *pstGetPNLGammaTbl)
{
	bool bRet = TRUE;
	bool bSupportWChannel = _is_support_w_channel();

	if (!pstGetPNLGammaTbl) {
		TCON_ERROR("pstGetPNLGammaTbl is NULL parameter\n");
		bRet &= FALSE;
		return bRet;
	}

	if (!pstGetPNLGammaTbl->pu16RChannel0) {
		TCON_ERROR("R Channel0 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (!pstGetPNLGammaTbl->pu16GChannel0) {
		TCON_ERROR("G Channel0 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (!pstGetPNLGammaTbl->pu16BChannel0) {
		TCON_ERROR("B Channel0 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (bSupportWChannel && !pstGetPNLGammaTbl->pu16WChannel0) {
		TCON_ERROR("W Channel0 is NULL parameter\n");
		bRet &= FALSE;
	}

	return bRet;
}

bool is_channel1_tbl_valid(ST_PNL_PNLGAMMATBL *pstGetPNLGammaTbl)
{
	bool bRet = TRUE;
	bool bSupportWChannel = _is_support_w_channel();

	if (!pstGetPNLGammaTbl) {
		TCON_ERROR("pstGetPNLGammaTbl is NULL parameter\n");
		bRet &= FALSE;
		return bRet;
	}

	if (!pstGetPNLGammaTbl->pu16RChannel1) {
		TCON_ERROR("R Channel1 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (!pstGetPNLGammaTbl->pu16GChannel1) {
		TCON_ERROR("G Channel1 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (!pstGetPNLGammaTbl->pu16BChannel1) {
		TCON_ERROR("B Channel1 is NULL parameter\n");
		bRet &= FALSE;
	}

	if (bSupportWChannel && !pstGetPNLGammaTbl->pu16WChannel1) {
		TCON_ERROR("W Channel1 is NULL parameter\n");
		bRet &= FALSE;
	}

	return bRet;
}

bool _get_panelgamma_tbl_from_bin(ST_PNL_PNLGAMMATBL *pstGetPNLGammaTbl)
{
	uint32_t u32index = 0, u32StartAddress = 0;
	ST_PNL_GAMMA_CONFIG stPNLGammaConfig;
	uint8_t *u8ptr = NULL;
	uint8_t *pu8ptr_R0 = NULL;
	uint8_t *pu8ptr_G0 = NULL;  // 64entry  * 32 byte =  2048
	uint8_t *pu8ptr_B0 = NULL;
	uint8_t *pu8ptr_W0 = NULL;
	uint8_t *pu8ptr_R1 = NULL;
	uint8_t *pu8ptr_G1 = NULL;
	uint8_t *pu8ptr_B1 = NULL;
	uint8_t *pu8ptr_W1 = NULL;
	bool bSupportWChannel = _is_support_w_channel();

	if (!is_channel0_tbl_valid(pstGetPNLGammaTbl)) {
		TCON_ERROR("Channel 0 table is invalid\n");
		return FALSE;
	}

	if (!(pstGetPNLGammaTbl->pu8GammaTbl)) {
		TCON_ERROR("pu8GammaTbl is NULL parameter\n");
		return FALSE;
	}

	memset(&stPNLGammaConfig, 0, sizeof(ST_PNL_GAMMA_CONFIG));
	memcpy(&stPNLGammaConfig,
		(pstGetPNLGammaTbl->pu8GammaTbl + PANEL_GAMMA_MAINHEADER_LEN),
		sizeof(ST_PNL_GAMMA_CONFIG));

	u32StartAddress = stPNLGammaConfig.u32Table2DStartAddress +
			stPNLGammaConfig.u16PreTableAlignedSize * pstGetPNLGammaTbl->u16Index;

	if (stPNLGammaConfig.bGamma2LutEnable) {

		if (!is_channel1_tbl_valid(pstGetPNLGammaTbl)) {
			TCON_ERROR("Channel 1 table is invalid\n");
			return FALSE;
		}
	}

	u8ptr = u32StartAddress + pstGetPNLGammaTbl->pu8GammaTbl;

	//[7:4]: 0x01 10BITS GAMMA
	//       0x02 12BITS GAMMA
	if ((stPNLGammaConfig.u8GammaType & BIT(4)))
		pstGetPNLGammaTbl->enGammaType = E_PNL_GAMMA_10BIT_TYPE;
	else if ((stPNLGammaConfig.u8GammaType & BIT(5)))
		pstGetPNLGammaTbl->enGammaType = E_PNL_GAMMA_12BIT_TYPE;

	//[3:0]: 0x01 256 entries
	//       0x02 1024 entries
	if ((stPNLGammaConfig.u8GammaType & BIT(0)))
		pstGetPNLGammaTbl->enGammaEntries = E_PNL_GAMMA_256_ENTRIES;
	else
		pstGetPNLGammaTbl->enGammaEntries = E_PNL_GAMMA_1024_ENTRIES;

	//data format
	//each cmd:
	//|  d_15  |   d_14  |   d_13  |  d_12   |   d_11  |   d_10  |   d_9   |...
	//|251:240 | 235:224 | 219:208 | 203:192 | 187:176 | 171:160 | 155:144 |...
	//...|   d_8   |   d_7   |   d_6  |  d_5  |  d_4  |  d_3  |  d_2  |  d_1  | d_0  |
	//...| 139:128 | 123:112 | 107:96 | 91:80 | 75:64 | 59:48 | 43:32 | 27:16 | 11:0 |
	//
	//R/G/B/W cmd order in DRAM
	//            |END | START |
	//  R_LUT0     63      0
	//  G_LUT0     127     64
	//  B_LUT0     191     128
	//  W_LUT0     255     192
	//  R_LUT1     319     256
	//  G_LUT1     383     320
	//  B_LUT1     447     384
	//  W_LUT1     511     448

	pu8ptr_R0 = u8ptr;
	pu8ptr_G0 = pu8ptr_R0 + PANEL_GAMMA_CHANNEL_OFFSET;  // 64entry  * 32 byte =  2048
	pu8ptr_B0 = pu8ptr_G0 + PANEL_GAMMA_CHANNEL_OFFSET;
	pu8ptr_W0 = pu8ptr_B0 + PANEL_GAMMA_CHANNEL_OFFSET;

	pu8ptr_R1 = pu8ptr_R0 + PANEL_GAMMA_TBL_OFFSET;
	pu8ptr_G1 = pu8ptr_R1 + PANEL_GAMMA_CHANNEL_OFFSET;
	pu8ptr_B1 = pu8ptr_G1 + PANEL_GAMMA_CHANNEL_OFFSET;
	pu8ptr_W1 = pu8ptr_B1 + PANEL_GAMMA_CHANNEL_OFFSET;

	for (u32index = 0; u32index < PANEL_GAMMA_ENTRY; u32index++) {
		//LUT 0
		pstGetPNLGammaTbl->pu16RChannel0[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_R0 + u32index));
		pstGetPNLGammaTbl->pu16GChannel0[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_G0 + u32index));
		pstGetPNLGammaTbl->pu16BChannel0[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_B0 + u32index));
		if (bSupportWChannel)
			pstGetPNLGammaTbl->pu16WChannel0[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_W0 + u32index));

		if (stPNLGammaConfig.bGamma2LutEnable) {
			//LUT 1
			pstGetPNLGammaTbl->pu16RChannel1[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_R1 + u32index));
			pstGetPNLGammaTbl->pu16GChannel1[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_G1 + u32index));
			pstGetPNLGammaTbl->pu16BChannel1[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_B1 + u32index));
			if (bSupportWChannel)
				pstGetPNLGammaTbl->pu16WChannel1[u32index] =
						((uint16_t)*((uint16_t *)pu8ptr_W1 + u32index));
		}
	}
	return TRUE;
}

bool _trigger_pnlgamma_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize)
{
	bool bRet;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	bRet = set_adl_proc(pu8Tbl, u32TblSize,
			E_SM_TCON_ADL_PANEL_GAMMA_0, E_SM_ADL_TRIGGER_MODE);

	return bRet;
}

bool _use_adl_set_panelgamma_tbl(struct mtk_panel_context *pCon, ST_PNL_PNLGAMMATBL *pstTbl)
{
#if (ENABLE_PNL_GAMMA_AUTODOWNLOAD == TRUE)
	uint8_t *pu8GammaTbl = NULL;
	bool bRet = TRUE;
	uint32_t u32index = 0, u32Size = 0;
	bool b2LutEn = _get_to_lut_enable(pCon);
	uint32_t u32PnlGammaADLCmdSize;
	uint8_t *pu8R0 = NULL, *pu8G0 = NULL, *pu8B0 = NULL, *pu8W0 = NULL;
	uint8_t *pu8R1 = NULL, *pu8G1 = NULL, *pu8B1 = NULL, *pu8W1 = NULL;
	bool bGammaByPass = _get_panelgamma_bypass();
	bool bSupportWChannel = _is_support_w_channel();

	TCON_DEBUG("LutEn=%d SupportWChannel=%d\n", b2LutEn, bSupportWChannel);

	if (!b2LutEn) {
		if (bSupportWChannel)
			u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_256_ENTRY;
		else
			u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_192_ENTRY;
	} else
		u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_384_ENTRY;

	u32Size = PNLGAMMA_ADL_CMD_LENGTH*u32PnlGammaADLCmdSize;

	PNL_MALLOC_MEM(pu8GammaTbl, u32Size, bRet);

	if (bRet) {
		//LUT 0
		pu8R0 = pu8GammaTbl;
		pu8G0 = pu8R0 + PANEL_GAMMA_CHANNEL_OFFSET;  // 64entry  * 32 byte =  2048
		pu8B0 = pu8G0 + PANEL_GAMMA_CHANNEL_OFFSET;

		if (bSupportWChannel)
			pu8W0 = pu8B0 + PANEL_GAMMA_CHANNEL_OFFSET;

		if (b2LutEn) {
			//LUT 1
			pu8R1 = pu8B0 + PANEL_GAMMA_CHANNEL_OFFSET;
			pu8G1 = pu8R1 + PANEL_GAMMA_CHANNEL_OFFSET;
			pu8B1 = pu8G1 + PANEL_GAMMA_CHANNEL_OFFSET;

			if (bSupportWChannel)
				pu8W1 = pu8B1 + PANEL_GAMMA_CHANNEL_OFFSET;
		}

		for (u32index = 0; u32index < PANEL_GAMMA_ENTRY; u32index++) {
			//LUT 0
			*((uint16_t *)pu8R0 + u32index) = pstTbl->pu16RChannel0[u32index];
			*((uint16_t *)pu8G0 + u32index) = pstTbl->pu16GChannel0[u32index];
			*((uint16_t *)pu8B0 + u32index) = pstTbl->pu16BChannel0[u32index];

			if (bSupportWChannel)
				*((uint16_t *)pu8W0 + u32index) = pstTbl->pu16WChannel0[u32index];

			if (b2LutEn) {
				//LUT 1
				*((uint16_t *)pu8R1 + u32index) = pstTbl->pu16RChannel1[u32index];
				*((uint16_t *)pu8G1 + u32index) = pstTbl->pu16GChannel1[u32index];
				*((uint16_t *)pu8B1 + u32index) = pstTbl->pu16BChannel1[u32index];

				if (bSupportWChannel)
					*((uint16_t *)pu8W1 + u32index) =
								pstTbl->pu16WChannel1[u32index];
			}
		}

		if (bGammaByPass)
			_set_panelgamma_bypass(FALSE);

		if (pCon->hw_info.pnl_lib_version == VERSION5) {
			TCON_DEBUG("Miffy set DGA set SW MODE\n");
			drv_hwreg_render_tcon_dga_set_sw_mode(TRUE);
		}

		bRet &= _trigger_pnlgamma_adl_proc(pu8GammaTbl, u32Size);

		PNL_FREE_MEM(pu8GammaTbl);
	} else {
		TCON_ERROR("Allocate buffer return fail\n.");
		return FALSE;
	}
	return bRet;
#else
	TCON_DEBUG("Auto download config disable.\n");
	return FALSE;
#endif
}

bool _write_1lut_to_reg(ST_PNL_PNLGAMMATBL *pstTbl, const uint16_t u16DataIdx,
								const uint8_t u8Ch)
{
	st_drv_lut_info stLutInfo;

	memset(&stLutInfo, 0, sizeof(st_drv_lut_info));

	if (!pstTbl) {
		TCON_ERROR("pstTbl is NULL parameter\n");
		return FALSE;
	};

	stLutInfo.pu16RChannel0 = pstTbl->pu16RChannel0;
	stLutInfo.pu16GChannel0 = pstTbl->pu16GChannel0;
	stLutInfo.pu16BChannel0 = pstTbl->pu16BChannel0;
	stLutInfo.pu16WChannel0 = pstTbl->pu16WChannel0;

	stLutInfo.pu16RChannel1 = pstTbl->pu16RChannel1;
	stLutInfo.pu16GChannel1 = pstTbl->pu16GChannel1;
	stLutInfo.pu16BChannel1 = pstTbl->pu16BChannel1;
	stLutInfo.pu16WChannel1 = pstTbl->pu16WChannel1;

	stLutInfo.u16DataIdx = u16DataIdx;
	stLutInfo.u8Channel = u8Ch;
	stLutInfo.bSupportWChannel = SUPPORT_PANEL_GAMMA_W_CHANNEL;

	return (drv_hwreg_render_tcon_dga_write_1lut_to_reg(&stLutInfo) == 0);
}

bool _write_2lut_to_reg(ST_PNL_PNLGAMMATBL *pstTbl, const uint16_t u16DataIdx,
								const uint8_t u8Ch)
{
	st_drv_lut_info stLutInfo;

	memset(&stLutInfo, 0, sizeof(st_drv_lut_info));

	if (!pstTbl) {
		TCON_ERROR("pstTbl is NULL parameter\n");
		return FALSE;
	};

	stLutInfo.pu16RChannel0 = pstTbl->pu16RChannel0;
	stLutInfo.pu16GChannel0 = pstTbl->pu16GChannel0;
	stLutInfo.pu16BChannel0 = pstTbl->pu16BChannel0;
	stLutInfo.pu16WChannel0 = pstTbl->pu16WChannel0;

	stLutInfo.pu16RChannel1 = pstTbl->pu16RChannel1;
	stLutInfo.pu16GChannel1 = pstTbl->pu16GChannel1;
	stLutInfo.pu16BChannel1 = pstTbl->pu16BChannel1;
	stLutInfo.pu16WChannel1 = pstTbl->pu16WChannel1;

	stLutInfo.u16DataIdx = u16DataIdx;
	stLutInfo.u8Channel = u8Ch;
	stLutInfo.bSupportWChannel = SUPPORT_PANEL_GAMMA_W_CHANNEL;

	return (drv_hwreg_render_tcon_dga_write_2lut_to_reg(&stLutInfo) == 0);
}

bool _set_panelgamma_tbl(struct mtk_panel_context *pCon, ST_PNL_PNLGAMMATBL *pstTbl)
{
	uint16_t u16NumOfLevel = PANEL_GAMMA_ENTRY;
	uint16_t u16DataIdx = 0;
	uint8_t u8Channel = 0;
	bool bGammaEnable = 0;
	bool b2LutEn = _get_to_lut_enable(pCon);
	// 1: RGBRGB, 0: RGBW
	uint8_t u8MaxChannel = b2LutEn ?
					PANEL_GAMMA_2LUT_MAX_CHANNEL : PANEL_GAMMA_1LUT_MAX_CHANNEL;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	// Store Panel Gamma enable flag
	bGammaEnable = _get_panelgamma_enable();
	TCON_DEBUG("Current gamma enable=%d\n", bGammaEnable);

	if (_use_adl_set_panelgamma_tbl(pCon, pstTbl))
		return TRUE;

	TCON_DEBUG("Auto download setting return fail\n");

	_set_panelgamma_bypass(FALSE);

	//Disable Panel Gamma
	if (bGammaEnable)
		_set_panelgamma_enable(FALSE);

	//Due to odclk and mcu clock not the same, the set value not stable.
	//Need add 4 NOP for burst write to guarantee that set value correct.
	//So use normal write to save time.

	for (u8Channel = 0; u8Channel < u8MaxChannel; u8Channel++) {
		//BW enable
		drv_hwreg_render_tcon_dga_set_bw_enable(TRUE);

		if (b2LutEn == FALSE) {
			// set Write channel
			drv_hwreg_render_tcon_dga_select_channel(u8Channel);

			if (pCon->hw_info.pnl_lib_version != VERSION4) {
				//reg_gamma_lut_sel, select lut0 load table
				drv_hwreg_render_tcon_dga_select_gammalut(0);
			}
		} else {
			// set Write channel
			drv_hwreg_render_tcon_dga_select_channel(u8Channel % 3);

			if (pCon->hw_info.pnl_lib_version != VERSION4) {
				//reg_gamma_lut_sel, select lut0 or lut1 to load table
				drv_hwreg_render_tcon_dga_select_gammalut(
								(u8Channel >= 3) ? 0x1 : 0x0);
			}
		}

		//BW enable
		drv_hwreg_render_tcon_dga_set_bw_enable(TRUE);

		for (u16DataIdx = 0; u16DataIdx < u16NumOfLevel; u16DataIdx++) {
			//2. set LUT_ADDR
			drv_hwreg_render_tcon_dga_set_gamma_addr(u16DataIdx);

			//3. set write data
			if (b2LutEn == FALSE)
				_write_1lut_to_reg(pstTbl, u16DataIdx, u8Channel);
			else
				_write_2lut_to_reg(pstTbl, u16DataIdx, u8Channel);  //lut0 & lut1
		}

		//Afetr writing one channel, disable BW.
		drv_hwreg_render_tcon_dga_set_bw_enable(FALSE);
	}

	// restore setting
	if (bGammaEnable)
		_set_panelgamma_enable(TRUE);

	return TRUE;
}

bool _mem_free(ST_PNL_PNLGAMMATBL *stPNLGammaTbl)
{
	PNL_FREE_MEM(stPNLGammaTbl->pu16RChannel0);
	PNL_FREE_MEM(stPNLGammaTbl->pu16GChannel0);
	PNL_FREE_MEM(stPNLGammaTbl->pu16BChannel0);
	PNL_FREE_MEM(stPNLGammaTbl->pu16WChannel0);

	PNL_FREE_MEM(stPNLGammaTbl->pu16RChannel1);
	PNL_FREE_MEM(stPNLGammaTbl->pu16GChannel1);
	PNL_FREE_MEM(stPNLGammaTbl->pu16BChannel1);
	PNL_FREE_MEM(stPNLGammaTbl->pu16WChannel1);

	return TRUE;
}

bool _mem_allocate(ST_PNL_PNLGAMMATBL *stPNLGammaTbl, bool bGamma2LutEnable)
{
	bool bRet = TRUE;
	bool bSupportWChannel = _is_support_w_channel();
	const uint16_t u16ChannelLen = PANEL_GAMMA_ENTRY*sizeof(uint16_t);

	if (!bGamma2LutEnable) {
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16RChannel0, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16GChannel0, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16BChannel0, u16ChannelLen, bRet);
		if (bSupportWChannel)
			PNL_MALLOC_MEM(stPNLGammaTbl->pu16WChannel0, u16ChannelLen, bRet);
	} else {
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16RChannel0, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16GChannel0, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16BChannel0, u16ChannelLen, bRet);

		PNL_MALLOC_MEM(stPNLGammaTbl->pu16RChannel1, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16GChannel1, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16BChannel1, u16ChannelLen, bRet);

		PNL_MALLOC_MEM(stPNLGammaTbl->pu16WChannel0, u16ChannelLen, bRet);
		PNL_MALLOC_MEM(stPNLGammaTbl->pu16WChannel1, u16ChannelLen, bRet);
	}

	if (!bRet)
		_mem_free(stPNLGammaTbl);

	return bRet;
}

//--------------------------------------------------------------------------------------------
// Panel Gamma Auto Download Format
// Lut0                         2Lut case
// R/G/B/W cmd format in DRAM   R/G/B cmd format in DRAM
//            |END | START |              |END | START |
//  R_LUT0     63      0        R_LUT0     63      0
//  G_LUT0     127     64       G_LUT0     127     64
//  B_LUT0     191     128      B_LUT0     191     128
//  W_LUT0     255     192      R_LUT1     255     192
//                              G_LUT1     319     256
//                              B_LUT1     383     320
// Panel Gamma Bin File Format
// Lut0                         2Lut case
//  R_LUT0     63      0        R_LUT0     63      0
//  G_LUT0     127     64       G_LUT0     127     64
//  B_LUT0     191     128      B_LUT0     191     128
//  W_LUT0     255     192      W_LUT0     255     192
//                              R_LUT1     319     256
//                              G_LUT1     383     320
//                              B_LUT1     447     384
//                              W_LUT1     511     448
//--------------------------------------------------------------------------------------------
bool _set_panelgamma_proc(struct mtk_panel_context *pCon, uint8_t *pu8GammaTbl,
					uint32_t u32Size, uint8_t u8Index)
{
	bool bRet = TRUE;
	ST_PNL_PNLGAMMATBL stPnlGammaTbl;
	ST_PNL_GAMMA_CONFIG stPnlGammaConfig;

	if (!pu8GammaTbl) {
		TCON_ERROR("pu8GammaTbl is NULL parameter\n");
		return FALSE;
	}

	if (u32Size < PANEL_GAMMA_MAINHEADER_LEN) {
		TCON_ERROR("Table Size(%u) is too small.\n", u32Size);
		return FALSE;
	}

	//get panel gamma sub header
	memset(&stPnlGammaConfig, 0, sizeof(ST_PNL_GAMMA_CONFIG));
	memcpy(&stPnlGammaConfig,
		(pu8GammaTbl + PANEL_GAMMA_MAINHEADER_LEN),
		sizeof(ST_PNL_GAMMA_CONFIG));

	//get panel gamma RGBW curve table from BIN
	memset(&stPnlGammaTbl, 0, sizeof(ST_PNL_PNLGAMMATBL));
	stPnlGammaTbl.pu8GammaTbl = pu8GammaTbl;
	stPnlGammaTbl.u16Index = u8Index;

	//allocate memory
	bRet &= _mem_allocate(&stPnlGammaTbl, stPnlGammaConfig.bGamma2LutEnable);
	if (!bRet) {
		TCON_DEBUG("\033[1;32m _mem_allocate retuen fail\033[0m\n");
		return bRet;
	}

	bRet &= _get_panelgamma_tbl_from_bin(&stPnlGammaTbl);
	if (bRet) {
		//Set VAC 2Lut mode
		_set_to_lut_mode(pCon, &stPnlGammaConfig);
		_set_panelgamma_tbl(pCon, &stPnlGammaTbl);

		//Enable Panel gamma
		_set_panelgamma_enable(TRUE);
	} else
		TCON_ERROR("Get panel gamma table from bin return failed.\n");

	//free memory
	_mem_free(&stPnlGammaTbl);

	return bRet;
}

bool _trigger_vrr_pnlgamma_adl_proc(uint8_t *pu8Tbl, uint32_t u32TblSize,
	uint32_t cmdcnt)
{
	bool bRet;

	if (!pu8Tbl) {
		TCON_ERROR("pu8Tbl is NULL parameter\n");
		return FALSE;
	}

	bRet = set_vrr_pnlgamma_adl_proc(pu8Tbl, u32TblSize,
			E_SM_TCON_ADL_PANEL_GAMMA_0, E_SM_ADL_TRIGGER_MODE, cmdcnt);

	return bRet;
}

bool _use_adl_set_vrr_panelgamma_tbl(struct mtk_panel_context *pCon, ST_PNL_PNLGAMMATBL *pstTbl,
	unsigned char uTableIdx)
{
#if (ENABLE_PNL_GAMMA_AUTODOWNLOAD == TRUE)
	uint8_t *pu8GammaTbl = NULL;
	bool bRet = TRUE;
	uint32_t u32index = 0, u32Size = 0;
	bool b2LutEn = _get_to_lut_enable(pCon);
	uint32_t u32PnlGammaADLCmdSize;
	uint8_t *pu8R0 = NULL, *pu8G0 = NULL, *pu8B0 = NULL, *pu8W0 = NULL;
	uint8_t *pu8R1 = NULL, *pu8G1 = NULL, *pu8B1 = NULL, *pu8W1 = NULL;
	bool bGammaByPass = _get_panelgamma_bypass();
	bool bSupportWChannel = _is_support_w_channel();
	uint32_t u32Dynamicindex = 0;
	static unsigned char pu8VRRGammaTbl_init;

	TCON_DEBUG("LutEn=%d SupportWChannel=%d\n", b2LutEn, bSupportWChannel);

	if (!b2LutEn) {
		if (bSupportWChannel)
			u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_256_ENTRY;
		else
			u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_192_ENTRY;
	} else
		u32PnlGammaADLCmdSize = PNLGAMMA_ADL_CMD_384_ENTRY;

	u32Size = PNLGAMMA_ADL_CMD_LENGTH*u32PnlGammaADLCmdSize;

	PNL_MALLOC_MEM(pu8GammaTbl, u32Size, bRet);

	 //Allocate Dynamic pannel gamma memory
	if (pu8VRRGammaTbl_init == 0) {
		PNL_MALLOC_MEM(pu8VRRGammaTbl, u32Size*(TOTAL_FREQ_TABLE + 1), bRet);
		pu8VRRGammaTbl_init = 1;
	}

	if (bRet) {
		//LUT 0
		pu8R0 = pu8GammaTbl;
		pu8G0 = pu8R0 + PANEL_GAMMA_CHANNEL_OFFSET;  // 64entry  * 32 byte =  2048
		pu8B0 = pu8G0 + PANEL_GAMMA_CHANNEL_OFFSET;

		if (bSupportWChannel)
			pu8W0 = pu8B0 + PANEL_GAMMA_CHANNEL_OFFSET;

		if (b2LutEn) {
			//LUT 1
			pu8G0 = pu8R0 + PANEL_GAMMA_CHANNEL_OFFSET;  // 64entry  * 32 byte =  2048
			pu8B0 = pu8G0 + PANEL_GAMMA_CHANNEL_OFFSET;

			pu8R1 = pu8B0 + PANEL_GAMMA_CHANNEL_OFFSET;
			pu8G1 = pu8R1 + PANEL_GAMMA_CHANNEL_OFFSET;
			pu8B1 = pu8G1 + PANEL_GAMMA_CHANNEL_OFFSET;

			if (bSupportWChannel)
				pu8W1 = pu8B1 + PANEL_GAMMA_CHANNEL_OFFSET;
		}

		for (u32index = 0; u32index < PANEL_GAMMA_ENTRY; u32index++) {
			//LUT 0
			*((uint16_t *)pu8R0 + u32index) = pstTbl->pu16RChannel0[u32index];
			*((uint16_t *)pu8G0 + u32index) = pstTbl->pu16GChannel0[u32index];
			*((uint16_t *)pu8B0 + u32index) = pstTbl->pu16BChannel0[u32index];

			if (b2LutEn) {
				//LUT 1
				*((uint16_t *)pu8R1 + u32index) = pstTbl->pu16RChannel1[u32index];
				*((uint16_t *)pu8G1 + u32index) = pstTbl->pu16GChannel1[u32index];
				*((uint16_t *)pu8B1 + u32index) = pstTbl->pu16BChannel1[u32index];

				if (bSupportWChannel)
					*((uint16_t *)pu8W1 + u32index) =
								pstTbl->pu16WChannel1[u32index];
			}
		}

		for (u32Dynamicindex =
			pCon->VRR_PGA_info->stVRR_PGA_Table[uTableIdx].u16FreqLowBound;
		u32Dynamicindex <=
			pCon->VRR_PGA_info->stVRR_PGA_Table[uTableIdx].u16FreqHighBound;
		u32Dynamicindex++) {
			memcpy(pu8VRRGammaTbl + (u32Dynamicindex*u32Size), pu8GammaTbl, u32Size);
		}

		if (bGammaByPass)
			_set_panelgamma_bypass(FALSE);

		bRet &= _trigger_vrr_pnlgamma_adl_proc(pu8VRRGammaTbl,
			u32Size * (TOTAL_FREQ_TABLE + 1), u32Size);

		PNL_FREE_MEM(pu8GammaTbl);
	} else {
		TCON_ERROR("Allocate buffer return fail\n.");
		PNL_FREE_MEM(pu8GammaTbl);
		return FALSE;
	}
	return bRet;
#else
	TCON_DEBUG("Auto download config disable.\n");
	return FALSE;
#endif
}

bool _set_vrr_panelgamma_tbl(struct mtk_panel_context *pCon, ST_PNL_PNLGAMMATBL *pstTbl,
	unsigned char uTableIdx)
{
	uint16_t u16NumOfLevel = PANEL_GAMMA_ENTRY;
	uint16_t u16DataIdx = 0;
	uint8_t u8Channel = 0;
	bool bGammaEnable = 0;
	bool b2LutEn = _get_to_lut_enable(pCon);
	// 1: RGBRGB, 0: RGBW
	uint8_t u8MaxChannel = b2LutEn ?
					PANEL_GAMMA_2LUT_MAX_CHANNEL : PANEL_GAMMA_1LUT_MAX_CHANNEL;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	// Store Panel Gamma enable flag
	bGammaEnable = _get_panelgamma_enable();
	TCON_DEBUG("Current gamma enable=%d\n", bGammaEnable);

	if (_use_adl_set_vrr_panelgamma_tbl(pCon, pstTbl, uTableIdx))
		return TRUE;

	TCON_DEBUG("Auto download setting return fail\n");

	_set_panelgamma_bypass(FALSE);

	//Disable Panel Gamma
	if (bGammaEnable)
		_set_panelgamma_enable(FALSE);

	//Due to odclk and mcu clock not the same, the set value not stable.
	//Need add 4 NOP for burst write to guarantee that set value correct.
	//So use normal write to save time.

	for (u8Channel = 0; u8Channel < u8MaxChannel; u8Channel++) {
		//BW enable
		drv_hwreg_render_tcon_dga_set_bw_enable(TRUE);

		if (b2LutEn == FALSE) {
			// set Write channel
			drv_hwreg_render_tcon_dga_select_channel(u8Channel);

			if (pCon->hw_info.pnl_lib_version != VERSION4) {
				//reg_gamma_lut_sel, select lut0 load table
				drv_hwreg_render_tcon_dga_select_gammalut(0);
			}
		} else {
			// set Write channel
			drv_hwreg_render_tcon_dga_select_channel(u8Channel % 3);

			if (pCon->hw_info.pnl_lib_version != VERSION4) {
				//reg_gamma_lut_sel, select lut0 or lut1 to load table
				drv_hwreg_render_tcon_dga_select_gammalut(
								(u8Channel >= 3) ? 0x1 : 0x0);
			}
		}

		//BW enable
		drv_hwreg_render_tcon_dga_set_bw_enable(TRUE);

		for (u16DataIdx = 0; u16DataIdx < u16NumOfLevel; u16DataIdx++) {
			//2. set LUT_ADDR
			drv_hwreg_render_tcon_dga_set_gamma_addr(u16DataIdx);

			//3. set write data
			if (b2LutEn == FALSE)
				_write_1lut_to_reg(pstTbl, u16DataIdx, u8Channel);
			else
				_write_2lut_to_reg(pstTbl, u16DataIdx, u8Channel);  //lut0 & lut1
		}

		//Afetr writing one channel, disable BW.
		drv_hwreg_render_tcon_dga_set_bw_enable(FALSE);
	}

	// restore setting
	if (bGammaEnable)
		_set_panelgamma_enable(TRUE);

	return TRUE;
}

bool _set_vrr_panelgamma_proc(struct mtk_panel_context *pCon, uint8_t *pu8GammaTbl,
					uint32_t u32Size, uint8_t u8Index, unsigned char uTableIdx)
{
	bool bRet = TRUE;
	ST_PNL_PNLGAMMATBL stPnlGammaTbl;
	ST_PNL_GAMMA_CONFIG stPnlGammaConfig;

	if (!pu8GammaTbl) {
		TCON_ERROR("pu8GammaTbl is NULL parameter\n");
		return FALSE;
	}

	if (u32Size < PANEL_GAMMA_MAINHEADER_LEN) {
		TCON_ERROR("Table Size(%u) is too small.\n", u32Size);
		return FALSE;
	}

	//get panel gamma sub header
	memset(&stPnlGammaConfig, 0, sizeof(ST_PNL_GAMMA_CONFIG));
	memcpy(&stPnlGammaConfig,
		(pu8GammaTbl + PANEL_GAMMA_MAINHEADER_LEN),
		sizeof(ST_PNL_GAMMA_CONFIG));

	//get panel gamma RGBW curve table from BIN
	memset(&stPnlGammaTbl, 0, sizeof(ST_PNL_PNLGAMMATBL));
	stPnlGammaTbl.pu8GammaTbl = pu8GammaTbl;
	stPnlGammaTbl.u16Index = u8Index;

	//allocate memory
	bRet &= _mem_allocate(&stPnlGammaTbl, stPnlGammaConfig.bGamma2LutEnable);
	if (!bRet) {
		TCON_DEBUG("\033[1;32m _mem_allocate retuen fail\033[0m\n");
		return bRet;
	}

	bRet &= _get_panelgamma_tbl_from_bin(&stPnlGammaTbl);
	if (bRet) {
		//Set VAC 2Lut mode
		_set_to_lut_mode(pCon, &stPnlGammaConfig);
		_set_vrr_panelgamma_tbl(pCon, &stPnlGammaTbl, uTableIdx);

		//Enable Panel gamma
		_set_panelgamma_enable(TRUE);
	} else
		TCON_ERROR("Get panel gamma table from bin return failed.\n");

	//free memory
	_mem_free(&stPnlGammaTbl);

	return bRet;
}

bool mtk_tcon_panelgamma_setting(struct mtk_panel_context *pCon)
{
	bool bRet = TRUE;
	bool data_exist = FALSE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;

#if defined(__KERNEL__)
	boottime_print("Tcon panel gamma setting [begin]\n");
#endif

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	data_exist = is_pnlgamma_data_exist(&pdata_buf, &data_len);

	if (data_exist && data_len > 1) {
		TCON_DEBUG("Data len=%lld [0]=0x%X [1]=0x%X\n",
							data_len, pdata_buf[0], pdata_buf[1]);

		//set panel gamma process
		bRet &= _set_panelgamma_proc(pCon, pdata_buf, (uint32_t)data_len, 0);
		if (!bRet)
			TCON_ERROR("_set_panelgamma_proc fail\n");

	} else
		TCON_DEBUG("Not support panel gamma function. Data len=%lld\n", data_len);

#if defined(__KERNEL__)
	boottime_print("Tcon panel gamma setting [end]\n");
#endif

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool mtk_tcon_vrr_panelgamma_setting(struct mtk_panel_context *pCon,
	unsigned char uTableIdx)
{
	bool bRet = TRUE;
	bool data_exist = FALSE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;

#if defined(__KERNEL__)
	boottime_print("Tcon panel gamma setting [begin]\n");
#endif

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	data_exist = is_vrr_pnlgamma_data_exist(&pdata_buf, &data_len, uTableIdx);

	if (data_exist && data_len > 1) {
		TCON_DEBUG("Data len=%lld [0]=0x%X [1]=0x%X\n",
							data_len, pdata_buf[0], pdata_buf[1]);

		//set panel gamma process
		bRet &= _set_vrr_panelgamma_proc(pCon,
			pdata_buf, (uint32_t)data_len, 0, uTableIdx);
		if (!bRet)
			TCON_ERROR("_set_panelgamma_proc fail\n");

	} else {
		TCON_DEBUG("Not support panel gamma function. Data len=%lld\n", data_len);
		bRet = FALSE;
	}

#if defined(__KERNEL__)
	boottime_print("Tcon panel gamma setting [end]\n");
#endif

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

int mtk_tcon_panelgamma_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val)
{
	int ret = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	switch (enType) {
	case E_TCON_CMD_DGA_EN:
		g_bForceDgaDisable = (u16Val) ? false : true;
		TCON_DEBUG("Force disable=%d\n", g_bForceDgaDisable);
		_set_panelgamma_enable((bool)u16Val);
		break;
	case E_TCON_CMD_DGA_BYPASS:
		g_bForceDgaBypass = (u16Val) ? true : false;
		TCON_DEBUG("Force bypass=%d\n", g_bForceDgaBypass);
		_set_panelgamma_bypass((bool)u16Val);
		break;
	default:
		TCON_ERROR("Unknown cmd type=%d\n", enType);
		return -ENODEV;
	}

	return ret;

}

int mtk_tcon_panelgamma_get_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize)
{
	int ret = 0;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	if (!pInfo || u32InfoSize == 0) {
		TCON_ERROR("pInfo is NULL or info size is zero\n");
		return -EINVAL;
	}

	ret = snprintf(pInfo, u32InfoSize,
		"Tcon panel gamma info:\n"
		"	- Data exist = %d\n"
		"	- Bypass = %d Force bypass = %d\n"
		"	- Enable = %d Force disable = %d\n",
		is_pnlgamma_data_exist(&pdata_buf, &data_len),
		_get_panelgamma_bypass(), g_bForceDgaBypass,
		_get_panelgamma_enable(), g_bForceDgaDisable);

	return ((ret < 0) || (ret > u32InfoSize)) ? -EINVAL : ret;
}
