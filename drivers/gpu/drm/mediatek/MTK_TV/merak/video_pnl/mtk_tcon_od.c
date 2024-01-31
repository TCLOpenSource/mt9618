// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek tcon overdrive driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include "mtk_tcon_od.h"
#include "mtk_tv_drm_video_panel.h"
#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_od.h"

#define OD_MIU_PORT_NUM_4 (4)
#define OD_MIU_PORT_NUM_5 (5)
#define OD_MIU_PORT_NUM_6 (6)
#define OD_LARB_ID_8 (8)

ST_PNL_OD_NEW_MODE_DRAM_FORMAT * g_pstOD_ADL_DATA = NULL;
ST_PNL_OD_NEW_MODE_DRAM_FORMAT *g_pstVRR_OD_ADL_DATA;
bool g_bForceOdDisable;
bool g_bForceOdBypass;
bool g_bTconPowerOn = TRUE;
bool g_bOdReadyToSetOn = FALSE;
__u32 g_u32PnlVersion;

//Record the state of the OD before it was suspended.
enum EN_TCON_OD_VAL_TYPE g_enOdResumeState = E_TCON_OD_VAL_MAX;

bool _KHal_XC_GetMiuOffset(uint32_t u32MiuNo, u64 *pu64Offset)
{
	if (u32MiuNo == 0)
		*pu64Offset = MSTAR_MIU0_BUS_BASE;

	else if (u32MiuNo == 1)
		*pu64Offset = MSTAR_MIU1_BUS_BASE;

	else
		return FALSE;

	return TRUE;
}

int _set_od_bypass(bool bBypass)
{
	if (!bBypass && g_bForceOdBypass)
		bBypass = true;

	return drv_hwreg_render_tcon_overdrive_set_od_bypass(bBypass);
}

bool _is_od_bypass(void)
{
	return drv_hwreg_render_tcon_overdrive_is_od_bypass();
}

bool _is_od_enable(void)
{
	return drv_hwreg_render_tcon_overdrive_is_od_enable();
}

void _overdriver_vac_hscaling_setting(void)
{
	bool bRet = FALSE;
#if OVERDRIVE_ENABLE_VAC_HSCALING
	bRet = drv_hwreg_render_tcon_overdrive_vac_hscaling_setting();
#endif
	if (bRet == TRUE)
		TCON_DEBUG("vac hscaling setting success\n");

	else
		TCON_DEBUG("vac hscaling setting failed\n");
}
bool _overdriver_is_enable_rgbw(void)
{
#if SUPPORT_OVERDRIVE
	return drv_hwreg_render_tcon_overdrive_is_enable_rgbw();
#else
	return FALSE;
#endif
}
void _overdriver_enable_wchannel(bool bEn)
{
#if SUPPORT_OVERDRIVE
	//use dummy register to store tcon w channel enable status
	drv_hwreg_render_tcon_overdrive_enable_wchannel(bEn);
#endif
}

bool _set_od_weight_default(void)
{
	drv_hwreg_render_tcon_overdrive_set_od_weight_default();
	return TRUE;
}
bool _set_od_active_theshold_default(void)
{
	drv_hwreg_render_tcon_overdrive_active_theshold_default();
	return TRUE;
}

uint8_t mtk_tcon_OverDriverGetPixelBitNum(uint32_t u32ODModeType)
{
    uint8_t u8OdModeType = 0;
    uint8_t u8PixBit = 0;

    u8OdModeType = (uint8_t)u32ODModeType;
    if ((u8OdModeType <= E_HALPNL_OD_MODE_NONE) || (u8OdModeType >= E_HALPNL_OD_MODE_MAX))
    {
        u8PixBit = OVERDRIVE_MODE;
        TCON_DEBUG("[%s,%5d].default u8PixBit=%d\033[0m\n", __FUNCTION__, __LINE__, u8PixBit);
        return u8PixBit;
    }

    switch (u8OdModeType)
    {
        case E_HALPNL_OD_MODE_RGB_444:
        case E_HALPNL_OD_MODE_RGB_444_HSCALING:
        case E_HALPNL_OD_MODE_RGB_444_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_444_VSD_HSD_4X:
            u8PixBit = OVERDRIVE_RGB_444_MODE;
            break;
        case E_HALPNL_OD_MODE_RGB_565:
        case E_HALPNL_OD_MODE_RGB_565_HSCALING:
        case E_HALPNL_OD_MODE_RGB_565_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_565_VSD_HSD_4X:
            u8PixBit = OVERDRIVE_RGB_565_MODE;
            break;
        case E_HALPNL_OD_MODE_RGB_666:
        case E_HALPNL_OD_MODE_RGB_666_HSCALING:
        case E_HALPNL_OD_MODE_RGB_666_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_666_VSD_HSD_4X:
            u8PixBit = OVERDRIVE_RGB_666_MODE;
            break;
        case E_HALPNL_OD_MODE_RGB_555:
        case E_HALPNL_OD_MODE_RGB_555_HSCALING:
        case E_HALPNL_OD_MODE_RGB_555_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_555_VSD_HSD_4X:
            u8PixBit = OVERDRIVE_RGB_555_MODE;
            break;
        case E_HALPNL_OD_MODE_RGB_888:
        case E_HALPNL_OD_MODE_RGB_888_HSCALING:
        case E_HALPNL_OD_MODE_RGB_888_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_888_VSD_HSD_4X:
            u8PixBit = OVERDRIVE_RGB_888_MODE;
            break;
        case E_HALPNL_OD_MODE_COMPRESS_VLC:
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HVSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_CE:
        case E_HALPNL_OD_MODE_COMPRESS_CE_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD_HCALING:
            u8PixBit = OVERDRIVE_COMPRESS_MODE;
            break;
        default:
            u8PixBit = OVERDRIVE_MODE;
            break;
    }
    TCON_DEBUG("[%s,%5d].u8PixBit=%d\033[0m\n", __FUNCTION__, __LINE__, u8PixBit);
    return u8PixBit;
}

bool mtk_tcon_OverDriverGetHscaling2X(uint32_t u32ODModeType)
{
    uint8_t u8OdModeType = 0;
    uint8_t bHscaling = FALSE;

    u8OdModeType = (uint8_t)u32ODModeType;
    if ((u8OdModeType <= E_HALPNL_OD_MODE_NONE) || (u8OdModeType >= E_HALPNL_OD_MODE_MAX))
    {
        bHscaling = OVERDRIVE_ENABLE_HSCALING;
        TCON_DEBUG("[%s,%5d].default bHscaling=%d\033[0m\n", __FUNCTION__, __LINE__, bHscaling);
        return bHscaling;
    }

    switch (u8OdModeType)
    {
        case E_HALPNL_OD_MODE_RGB_444:
        case E_HALPNL_OD_MODE_RGB_565:
        case E_HALPNL_OD_MODE_RGB_666:
        case E_HALPNL_OD_MODE_RGB_555:
        case E_HALPNL_OD_MODE_RGB_888:
        case E_HALPNL_OD_MODE_COMPRESS_VLC:
        case E_HALPNL_OD_MODE_COMPRESS_CE:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD:
            bHscaling = FALSE;
            break;
        case E_HALPNL_OD_MODE_RGB_444_HSCALING:
        case E_HALPNL_OD_MODE_RGB_565_HSCALING:
        case E_HALPNL_OD_MODE_RGB_666_HSCALING:
        case E_HALPNL_OD_MODE_RGB_555_HSCALING:
        case E_HALPNL_OD_MODE_RGB_888_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HVSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_CE_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD_HCALING:
        case E_HALPNL_OD_MODE_RGB_444_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_565_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_666_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_555_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_888_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_444_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_565_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_666_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_555_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_888_VSD_HSD_4X:
            bHscaling = TRUE;
            break;
        default:
            bHscaling = OVERDRIVE_ENABLE_HSCALING;
            break;
    }
    TCON_DEBUG("[%s,%5d].bHscaling=%d\033[0m\n", __FUNCTION__, __LINE__, bHscaling);
    return bHscaling;
}

bool mtk_tcon_OverDriverGetHscaling4X(uint32_t u32ODModeType)
{
    uint8_t u8OdModeType = 0;
    uint8_t bHscaling4X = FALSE;

    u8OdModeType = (uint8_t)u32ODModeType;
    if ((u8OdModeType <= E_HALPNL_OD_MODE_NONE) || (u8OdModeType >= E_HALPNL_OD_MODE_MAX))
    {
        bHscaling4X = OVERDRIVE_ENABLE_HSD_4X;
        TCON_DEBUG("[%s,%5d].default bHscaling4X=%d\033[0m\n", __FUNCTION__, __LINE__, bHscaling4X);
        return bHscaling4X;
    }

    switch (u8OdModeType)
    {
        case E_HALPNL_OD_MODE_RGB_444_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_565_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_666_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_555_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_888_VSD_HSD_4X:
            bHscaling4X = TRUE;
            break;
        default:
            bHscaling4X = FALSE;
            break;
    }
    TCON_DEBUG("[%s,%5d].bHscaling4X=%d\033[0m\n", __FUNCTION__, __LINE__, bHscaling4X);
    return bHscaling4X;
}

bool mtk_tcon_OverDriverGetVscaling(uint32_t u32ODModeType)
{
    uint8_t u8OdModeType = 0;
    bool bVscaling = FALSE;

    u8OdModeType = (uint8_t)u32ODModeType;
    if ((u8OdModeType <= E_HALPNL_OD_MODE_NONE) || (u8OdModeType >= E_HALPNL_OD_MODE_MAX))
    {
        bVscaling = OVERDRIVE_ENABLE_VSCALING;
        TCON_DEBUG("[%s,%5d].default bVscaling=%d\033[0m\n", __FUNCTION__, __LINE__, bVscaling);
        return bVscaling;
    }

    switch (u8OdModeType)
    {
        case E_HALPNL_OD_MODE_RGB_444:
        case E_HALPNL_OD_MODE_RGB_444_HSCALING:
        case E_HALPNL_OD_MODE_RGB_565:
        case E_HALPNL_OD_MODE_RGB_565_HSCALING:
        case E_HALPNL_OD_MODE_RGB_666:
        case E_HALPNL_OD_MODE_RGB_666_HSCALING:
        case E_HALPNL_OD_MODE_RGB_555:
        case E_HALPNL_OD_MODE_RGB_555_HSCALING:
        case E_HALPNL_OD_MODE_RGB_888:
        case E_HALPNL_OD_MODE_RGB_888_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_VLC:
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_CE:
        case E_HALPNL_OD_MODE_COMPRESS_CE_HSCALING:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD:
        case E_HALPNL_OD_MODE_COMPRESS_ONE3RD_HCALING:
            bVscaling = FALSE;
            break;
        case E_HALPNL_OD_MODE_COMPRESS_VLC_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_444_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_565_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_666_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_555_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_888_HVSCALING:
        case E_HALPNL_OD_MODE_RGB_444_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_565_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_666_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_555_VSD_HSD_4X:
        case E_HALPNL_OD_MODE_RGB_888_VSD_HSD_4X:
            bVscaling = TRUE;
            break;
        default:
            bVscaling = OVERDRIVE_ENABLE_VSCALING;
            break;
    }

    TCON_DEBUG("[%s,%5d].bVscaling=%d\033[0m\n", __FUNCTION__, __LINE__, bVscaling);
    return bVscaling;
}

// Turn OD function //Have to concern
void _overdriver_init(
			struct mtk_panel_context *pCon,
			uint64_t u64OD_MSB_Addr,
			uint64_t u64OD_LSB_Addr,
			uint32_t u32OdModeType)
{
#if SUPPORT_OVERDRIVE

#if (OVERDRIVE_ENABLE_RGBW == TRUE)//not support w channel
	uint16_t u16Timeout = 0;
#endif

	ST_OVERDRIVE_INIT_DATA_INFO stInitData;

	stInitData.u64OD_MSB_Addr = u64OD_MSB_Addr;
	stInitData.u64OD_LSB_Addr = u64OD_LSB_Addr;
	stInitData.u16de_width = pCon->info.de_width;

	stInitData.u16hsize = ALIGN_8(pCon->info.de_width);//panel H, need align 8
	stInitData.u16vsize = pCon->info.de_height;//panel V

	TCON_DEBUG("H size=%d V size=%d\n", stInitData.u16hsize, stInitData.u16vsize);

	stInitData.u8PixBitNum = mtk_tcon_OverDriverGetPixelBitNum(u32OdModeType);
	stInitData.bVscaling = mtk_tcon_OverDriverGetVscaling(u32OdModeType);
	stInitData.bHscaling2X = mtk_tcon_OverDriverGetHscaling2X(u32OdModeType);
	stInitData.bHscaling4X = mtk_tcon_OverDriverGetHscaling4X(u32OdModeType);

	drv_hwreg_render_tcon_overdrive_init(&stInitData);

#endif
}

void _set_od_miu_mask(bool bEnable)
{
	int ret = 0;
	uint16_t u16larb_idx = 0;
	uint16_t u16miu2gmc_wr_port = 0;
	uint16_t u16miu2gmc_rd_port = 0;

	TCON_DEBUG("Panel Version=%d Enable=%d\n", g_u32PnlVersion, bEnable);

	if (g_u32PnlVersion == VERSION4) {
		u16larb_idx = OD_LARB_ID_8;
		u16miu2gmc_wr_port = OD_MIU_PORT_NUM_5;
		u16miu2gmc_rd_port = OD_MIU_PORT_NUM_6;
	} else if (g_u32PnlVersion == VERSION5) {
		u16larb_idx = OD_LARB_ID_8;
		u16miu2gmc_wr_port = OD_MIU_PORT_NUM_4;
		u16miu2gmc_rd_port = OD_MIU_PORT_NUM_5;
	} else {
		ret = ENODATA;
		TCON_ERROR("Unknown panel version\n");
	}

	if (!ret) {
		//od_w (1: can't accress, 0: can access)
		ret = mtk_miu2gmc_mask(u16larb_idx, u16miu2gmc_wr_port, bEnable);

		if (ret)
			TCON_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);

		//od_r (1: can't accress, 0: can access)
		ret = mtk_miu2gmc_mask(u16larb_idx, u16miu2gmc_rd_port, bEnable);

		if (ret)
			TCON_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);
	}
}

void _set_od_enable(bool bEnable, uint32_t u32OdModeType)
{
#if SUPPORT_OVERDRIVE
	uint16_t u16Taget_BR = 0; //Target bit rate
	//bool bEnableVscal = OVERDRIVE_ENABLE_VSCALING,
	//		bEnableHscal = OVERDRIVE_ENABLE_HSCALING;
	uint8_t u8PixBitNum = mtk_tcon_OverDriverGetPixelBitNum(u32OdModeType);
	bool bVscaling = mtk_tcon_OverDriverGetVscaling(u32OdModeType);
	bool bHscaling2X = mtk_tcon_OverDriverGetHscaling2X(u32OdModeType);
	// OD mode
	// OD used user weight to output blending directly
	// OD Enable
	if (bEnable && !g_bForceOdDisable) {
		u16Taget_BR = (u8PixBitNum * 64) + 3;//Formula = 64*bits_per_pixel + 3

		//Target bit rate of compression engine output for 4 bit compression
		drv_hwreg_render_tcon_overdrive_set_compression(u16Taget_BR);
#if (OVERDRIVE_ENABLE_RGBW == TRUE)

		if (_overdriver_is_enable_rgbw()) {//not support w channel
			//RGBW input Enable
			//drv_hwreg_render_tcon_overdrive_set_hv_scale_enable(
			//		REG_01DC_OD_1ST_BKA336_V004,
			//		0x1,
			//		REG_01DC_OD_1ST_BKA336_V004_REG_OD_RGBW_EN_01DC);
		}

#endif
		if ((bVscaling == TRUE) || (bHscaling2X == TRUE)) {
			//both need VScaling & HScaling
			drv_hwreg_render_tcon_overdrive_set_hv_scale_enable(
									bVscaling, bHscaling2X);
		}
#if OVERDRIVE_ENABLE_VAC_HSCALING
		_overdriver_vac_hscaling_setting();
#endif
		//Set OD Mode
		drv_hwreg_render_tcon_overdrive_set_od_mode((uint8_t)u32OdModeType);

		//eable miu
		_set_od_miu_mask(FALSE);

		// rd suggest enable od at last
		//OD enable
		drv_hwreg_render_tcon_overdrive_set_od_enable(true);
	} else {
		//OD disable
		drv_hwreg_render_tcon_overdrive_set_od_enable(false);

		//disable miu
		_set_od_miu_mask(TRUE);
	}
	TCON_DEBUG("OD enable=%d\n", _is_od_enable());
#endif
}

bool _set_od_rgbw_enable(bool bEnable)
{
#if (OVERDRIVE_ENABLE_RGBW == TRUE)//not support w channel
	if (_overdriver_is_enable_rgbw())
		drv_hwreg_render_tcon_overdrive_set_od_rgbw_enable(bEnable);
#else
	return FALSE;
#endif
	return TRUE;
}

bool _prepare_od_tbl_to_sram(
				uint16_t *pu16ODTbl,
				uint8_t *pu8OD_SRAM1, uint8_t *pu8OD_SRAM2,
				uint8_t *pu8OD_SRAM3, uint8_t *pu8OD_SRAM4)
{
	bool bRet = TRUE;
	uint16_t u16TableIndex = 0;
	uint16_t u16CurrentTableIndex = 0;
	uint16_t u16TargetIndex = 0;
	uint16_t u16Target = 0;

	if (!pu16ODTbl || !pu8OD_SRAM1 || !pu8OD_SRAM2 || !pu8OD_SRAM3 || !pu8OD_SRAM4) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	//sram1
	u16TargetIndex = OD_DECRYPTION_IDX_1;
	u16Target = pu16ODTbl[u16TargetIndex];
	for (u16TableIndex = 0; u16TableIndex < OVERDRIVER_SRAM1_SIZE; u16TableIndex++) {
		pu8OD_SRAM1[u16TableIndex] = (u16TableIndex == u16TargetIndex) ?
					(uint8_t)u16Target :
					(uint8_t)(pu16ODTbl[u16TableIndex]^u16Target);
	}
	//sram2
	u16CurrentTableIndex += OVERDRIVER_SRAM1_SIZE;
	u16TargetIndex += OD_DECRYPTION_IDX_2;
	u16Target = pu16ODTbl[(u16CurrentTableIndex+u16TargetIndex)];
	for (u16TableIndex = 0; u16TableIndex < OVERDRIVER_SRAM2_SIZE; u16TableIndex++) {
		pu8OD_SRAM2[u16TableIndex] = (u16TableIndex == u16TargetIndex) ?
			(uint8_t)u16Target :
			(uint8_t)(pu16ODTbl[(u16CurrentTableIndex+u16TableIndex)]^u16Target);
	}
	//sram3
	u16CurrentTableIndex += OVERDRIVER_SRAM2_SIZE;
	u16TargetIndex += OD_DECRYPTION_IDX_2;
	u16Target = pu16ODTbl[(u16CurrentTableIndex+u16TargetIndex)];
	for (u16TableIndex = 0; u16TableIndex < OVERDRIVER_SRAM3_SIZE; u16TableIndex++) {
		pu8OD_SRAM3[u16TableIndex] = (u16TableIndex == u16TargetIndex) ?
			(uint8_t)u16Target :
			(uint8_t)(pu16ODTbl[(u16CurrentTableIndex+u16TableIndex)]^u16Target);
	}
	//sram4
	u16CurrentTableIndex += OVERDRIVER_SRAM3_SIZE;
	u16TargetIndex += OD_DECRYPTION_IDX_2;
	u16Target = pu16ODTbl[(u16CurrentTableIndex+u16TargetIndex)];
	for (u16TableIndex = 0; u16TableIndex < OVERDRIVER_SRAM4_SIZE; u16TableIndex++) {
		pu8OD_SRAM4[u16TableIndex] = (u16TableIndex == u16TargetIndex) ?
			(uint8_t)u16Target :
			(uint8_t)(pu16ODTbl[(u16CurrentTableIndex+u16TableIndex)]^u16Target);
	}

	return bRet;
}


bool _overdriver_setting_without_adl(
				uint8_t *pu8OD_SRAM1, uint8_t *pu8OD_SRAM2,
				uint8_t *pu8OD_SRAM3, uint8_t *pu8OD_SRAM4,
				uint16_t u16SepatateMode)
{
	bool bRet = FALSE;

	bRet = drv_hwreg_render_tcon_overdrive_setting_without_adl(
				pu8OD_SRAM1, pu8OD_SRAM2,
				pu8OD_SRAM3, pu8OD_SRAM4,
				u16SepatateMode);
	return bRet;
}
bool _overdriver_adl_init(
			struct mtk_panel_context *pCon, uint32_t u32ADLSize)
{
	bool bRet = TRUE;
	static bool od_adl_data_init_done = FALSE;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pCon);

#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
	if (pCon->hw_info.pnl_lib_version == VERSION5) {
		//adl index user mode en have to be set 1 when boot time
		drv_hwreg_render_tcon_overdrive_adl_init(1);
	}
	//Always use new mode, support write independent R/G/B/W tables

	drv_hwreg_render_tcon_overdrive_adl_init(0);
	if (!od_adl_data_init_done) {
		PNL_MALLOC_MEM(g_pstOD_ADL_DATA, u32ADLSize, bRet);
		if (!bRet) {
			TCON_ERROR("OD Autodownload allocate mem fail\n");
			PNL_FREE_MEM(g_pstOD_ADL_DATA);
			return FALSE;
		}
		od_adl_data_init_done = TRUE;
	}
	bRet = drv_hwreg_render_tcon_overdrive_is_od_enable();
#endif

	// OD disable
	drv_hwreg_render_tcon_overdrive_set_od_enable(false);
	return bRet;
}

bool _vrr_overdriver_adl_init(
			struct mtk_panel_context *pCon, uint32_t u32ADLSize)
{
	bool bRet = TRUE;
	static bool vrr_od_adl_data_init_done = FALSE;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pCon);

#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
	if (pCon->hw_info.pnl_lib_version == VERSION5) {
		//adl index user mode en have to be set 1 when boot time
		drv_hwreg_render_tcon_overdrive_adl_init(1);
	}
	//Always use new mode, support write independent R/G/B/W tables

	drv_hwreg_render_tcon_overdrive_adl_init(0);
	if (!vrr_od_adl_data_init_done) {
		PNL_MALLOC_MEM(g_pstVRR_OD_ADL_DATA, u32ADLSize, bRet);
		if (!bRet) {
			TCON_ERROR("OD Autodownload allocate mem fail\n");
			PNL_FREE_MEM(g_pstVRR_OD_ADL_DATA);
			return FALSE;
		}
		vrr_od_adl_data_init_done = TRUE;
	}
	bRet = drv_hwreg_render_tcon_overdrive_is_od_enable();
#endif

	// OD disable
	drv_hwreg_render_tcon_overdrive_set_od_enable(false);
	return bRet;
}

bool _fill_to_adl_sram1_data(
			uint8_t *pu8Data,
			uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstOD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(g_pstOD_ADL_DATA);
	//TCON_CHECK_PARAMETER_NULL(pu8Data);
	if (u16TableIndex < OVERDRIVER_SRAM1_SIZE) {
		g_pstOD_ADL_DATA[u16TableIndex].u32Table_WriteAddr = u16TableIndex;
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE) {
			g_pstOD_ADL_DATA[u16TableIndex].u32OD_B_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)>>1;
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B1 = pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE) {
			g_pstOD_ADL_DATA[u16TableIndex].u32OD_G_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)>>2;
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G1 = pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE) {
			g_pstOD_ADL_DATA[u16TableIndex].u32OD_R_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)>>3;
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R1 = pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE) {
			g_pstOD_ADL_DATA[u16TableIndex].u32OD_W_WriteEn =
				u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE;
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W1 = pu8Data[u16TableIndex];
		}
	} else {
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B1 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G1 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R1 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W1 = 0;
	}
	return bRet;
}
bool _fill_to_adl_sram2_data(
				uint8_t *pu8Data,
				uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstOD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(g_pstOD_ADL_DATA);
	//TCON_CHECK_PARAMETER_NULL(pu8Data);
	if (u16TableIndex < OVERDRIVER_SRAM2_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B2 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G2 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R2 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W2 = pu8Data[u16TableIndex];

	} else {
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B2 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G2 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R2 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W2 = 0;
	}
	return bRet;
}
bool _fill_to_adl_sram3_data(
				uint8_t *pu8Data,
				uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstOD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(g_pstOD_ADL_DATA);
	//TCON_CHECK_PARAMETER_NULL(pu8Data);

	if (u16TableIndex < OVERDRIVER_SRAM3_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B3 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G3 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R3 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W3 = pu8Data[u16TableIndex];

	} else {
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B3 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G3 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R3 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W3 = 0;
	}
	return bRet;
}
bool _fill_to_adl_sram4_data(
			uint8_t *pu8Data,
			uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstOD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(g_pstOD_ADL_DATA);
	//TCON_CHECK_PARAMETER_NULL(pu8Data);

	if (u16TableIndex < OVERDRIVER_SRAM4_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B4 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G4 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R4 = pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W4 = pu8Data[u16TableIndex];

	} else {
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_B4 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_G4 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_R4 = 0;
		g_pstOD_ADL_DATA[u16TableIndex].u32SRAM_W4 = 0;
	}
	return bRet;
}

bool _vrr_od_fill_to_adl_sram1_data(
			uint32_t u32Dynamicindex, uint8_t *pu8Data,
			uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstVRR_OD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	if (u16TableIndex < OVERDRIVER_SRAM1_SIZE) {
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32Table_WriteAddr = u16TableIndex;
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE) {
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32OD_B_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)>>1;
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B1 =
				pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE) {
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32OD_G_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)>>2;
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G1 =
				pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE) {
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32OD_R_WriteEn =
				(u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)>>3;
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R1 =
				pu8Data[u16TableIndex];
		}
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE) {
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32OD_W_WriteEn =
				u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE;
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
				* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W1 =
				pu8Data[u16TableIndex];
		}
	} else {
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B1 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G1 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R1 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W1 = 0;
	}
	return bRet;
}
bool _vrr_od_fill_to_adl_sram2_data(
				uint32_t u32Dynamicindex, uint8_t *pu8Data,
				uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstVRR_OD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	if (u16TableIndex < OVERDRIVER_SRAM2_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B2 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G2 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R2 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W2 =
			pu8Data[u16TableIndex];

	} else {
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B2 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G2 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R2 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W2 = 0;
	}
	return bRet;
}
bool _vrr_od_fill_to_adl_sram3_data(
				uint32_t u32Dynamicindex, uint8_t *pu8Data,
				uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstVRR_OD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	if (u16TableIndex < OVERDRIVER_SRAM3_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B3 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G3 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R3 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W3 =
			pu8Data[u16TableIndex];

	} else {
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B3 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G3 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R3 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W3 = 0;
	}
	return bRet;
}
bool _vrr_od_fill_to_adl_sram4_data(
			uint32_t u32Dynamicindex, uint8_t *pu8Data,
			uint16_t u16TableIndex, uint16_t u16SepatateMode)
{
	bool bRet = TRUE;

	if (!g_pstVRR_OD_ADL_DATA || !pu8Data) {
		TCON_ERROR("NULL parameter !!!!\n");
		return FALSE;
	}

	if (u16TableIndex < OVERDRIVER_SRAM4_SIZE) {
		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_B_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B4 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_G_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G4 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_R_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R4 =
			pu8Data[u16TableIndex];

		if (u16SepatateMode & E_PNL_OVERDRIVER_SEPARATE_W_ENABLE)
			g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W4 =
			pu8Data[u16TableIndex];

	} else {
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_B4 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_G4 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_R4 = 0;
		g_pstVRR_OD_ADL_DATA[u16TableIndex + u32Dynamicindex
			* XC_AUTODOWNLOAD_OVERDRIVER_DEPTH].u32SRAM_W4 = 0;
	}
	return bRet;
}

bool _overdriver_adl_setting_tbl(
				uint16_t *pu16ODTbl, uint16_t u16SepatateMode)
{
	uint16_t u16TableIndex = 0;
	uint8_t *pu8OD_SRAM1 = NULL;
	uint8_t *pu8OD_SRAM2 = NULL;
	uint8_t *pu8OD_SRAM3 = NULL;
	uint8_t *pu8OD_SRAM4 = NULL;
	bool bRet = TRUE;

	if (!pu16ODTbl) {
		TCON_ERROR("pu16ODTbl is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pu16ODTbl);
	PNL_MALLOC_MEM(pu8OD_SRAM1, OVERDRIVER_SRAM1_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM2, OVERDRIVER_SRAM2_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM3, OVERDRIVER_SRAM3_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM4, OVERDRIVER_SRAM4_SIZE, bRet);
	if (!bRet) {
		TCON_ERROR("Error: malloc od sram failed\n");
		goto finally;
	}

	//Decrypt OD table.
	_prepare_od_tbl_to_sram(pu16ODTbl, pu8OD_SRAM1, pu8OD_SRAM2, pu8OD_SRAM3, pu8OD_SRAM4);

	for (u16TableIndex = 0;
			u16TableIndex < XC_AUTODOWNLOAD_OVERDRIVER_DEPTH;
			u16TableIndex++) {
		_fill_to_adl_sram1_data(pu8OD_SRAM1, u16TableIndex, u16SepatateMode);
		_fill_to_adl_sram2_data(pu8OD_SRAM2, u16TableIndex, u16SepatateMode);
		_fill_to_adl_sram3_data(pu8OD_SRAM3, u16TableIndex, u16SepatateMode);
		_fill_to_adl_sram4_data(pu8OD_SRAM4, u16TableIndex, u16SepatateMode);
	}

finally:
	PNL_FREE_MEM(pu8OD_SRAM1);
	PNL_FREE_MEM(pu8OD_SRAM2);
	PNL_FREE_MEM(pu8OD_SRAM3);
	PNL_FREE_MEM(pu8OD_SRAM4);
	return bRet;
}

bool _vrr_overdriver_adl_setting_tbl(
				uint16_t *pu16ODTbl, uint16_t u16SepatateMode,
				uint8_t u8ODTableIndex, struct mtk_panel_context *pCon)
{
	uint16_t u16TableIndex = 0;
	uint8_t *pu8OD_SRAM1 = NULL;
	uint8_t *pu8OD_SRAM2 = NULL;
	uint8_t *pu8OD_SRAM3 = NULL;
	uint8_t *pu8OD_SRAM4 = NULL;
	bool bRet = TRUE;
	uint32_t u32Dynamicindex = 0;

	if (!pu16ODTbl) {
		TCON_ERROR("pu16ODTbl is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pu16ODTbl);
	PNL_MALLOC_MEM(pu8OD_SRAM1, OVERDRIVER_SRAM1_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM2, OVERDRIVER_SRAM2_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM3, OVERDRIVER_SRAM3_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM4, OVERDRIVER_SRAM4_SIZE, bRet);
	if (!bRet) {
		TCON_ERROR("Error: malloc od sram failed\n");
		goto finally;
	}

	//Decrypt OD table.
	_prepare_od_tbl_to_sram(pu16ODTbl, pu8OD_SRAM1, pu8OD_SRAM2, pu8OD_SRAM3, pu8OD_SRAM4);

	for (u32Dynamicindex = pCon->VRR_OD_info->stVRR_OD_Table[u8ODTableIndex].u16FreqLowBound;
	u32Dynamicindex <= pCon->VRR_OD_info->stVRR_OD_Table[u8ODTableIndex].u16FreqHighBound;
	u32Dynamicindex++) {
		for (u16TableIndex = 0;
				u16TableIndex < XC_AUTODOWNLOAD_OVERDRIVER_DEPTH;
				u16TableIndex++) {
			_vrr_od_fill_to_adl_sram1_data(u32Dynamicindex,
				pu8OD_SRAM1, u16TableIndex, u16SepatateMode);
			_vrr_od_fill_to_adl_sram2_data(u32Dynamicindex,
				pu8OD_SRAM2, u16TableIndex, u16SepatateMode);
			_vrr_od_fill_to_adl_sram3_data(u32Dynamicindex,
				pu8OD_SRAM3, u16TableIndex, u16SepatateMode);
			_vrr_od_fill_to_adl_sram4_data(u32Dynamicindex,
				pu8OD_SRAM4, u16TableIndex, u16SepatateMode);
		}
	}

finally:
	PNL_FREE_MEM(pu8OD_SRAM1);
	PNL_FREE_MEM(pu8OD_SRAM2);
	PNL_FREE_MEM(pu8OD_SRAM3);
	PNL_FREE_MEM(pu8OD_SRAM4);
	return bRet;
}

void _overdriver_adl_fire(
				uint32_t u32ADLSize, bool bEnableOD)
{
	bool bRet = FALSE;

#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
	if (!g_pstOD_ADL_DATA) {
		TCON_ERROR("Auto download command is null!!\n");
		return;
	}

	bRet = set_adl_proc((uint8_t *)g_pstOD_ADL_DATA, u32ADLSize,
				E_SM_TCON_ADL_OD, E_SM_ADL_TRIGGER_MODE);

	if (!bRet) {
		TCON_ERROR("Auto download setting Failed!!\n");
		return;
	}
#endif

	// OD enable
	if (!g_bForceOdDisable)
		drv_hwreg_render_tcon_overdrive_set_od_enable(bEnableOD);
}

void _vrr_overdriver_adl_fire(
				uint32_t u32ADLSize, bool bEnableOD)
{
	bool bRet = FALSE;

#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
	if (!g_pstVRR_OD_ADL_DATA) {
		TCON_ERROR("Auto download command is null!!\n");
		return;
	}

	bRet = set_vrr_od_adl_proc((uint8_t *)g_pstVRR_OD_ADL_DATA, u32ADLSize,
		E_SM_TCON_ADL_OD, E_SM_ADL_TRIGGER_MODE);

	if (!bRet) {
		TCON_ERROR("Auto download setting Failed!!\n");
		return;
	}
#endif

	// OD enable
	if (!g_bForceOdDisable)
		drv_hwreg_render_tcon_overdrive_set_od_enable(bEnableOD);
}

void _overdriver_setting_tbl(
		uint16_t *pu16ODTbl,
		uint32_t u32ODTabSize,
		uint16_t u16SepatateMode)
{

	bool bEnable = FALSE;
	bool bRet = TRUE;
	uint8_t *pu8OD_SRAM1 = NULL;
	uint8_t *pu8OD_SRAM2 = NULL;
	uint8_t *pu8OD_SRAM3 = NULL;
	uint8_t *pu8OD_SRAM4 = NULL;

#if SUPPORT_OVERDRIVE
	if (!pu16ODTbl)
		return;

	PNL_MALLOC_MEM(pu8OD_SRAM1, OVERDRIVER_SRAM1_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM2, OVERDRIVER_SRAM2_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM3, OVERDRIVER_SRAM3_SIZE, bRet);
	PNL_MALLOC_MEM(pu8OD_SRAM4, OVERDRIVER_SRAM4_SIZE, bRet);
	if (!bRet)
		goto finally;

	//Decrypt OD table.
	_prepare_od_tbl_to_sram(pu16ODTbl, pu8OD_SRAM1, pu8OD_SRAM2, pu8OD_SRAM3, pu8OD_SRAM4);

	bEnable = drv_hwreg_render_tcon_overdrive_is_od_enable();

	//set od disable
	drv_hwreg_render_tcon_overdrive_set_od_enable(false);

	//Disable Overdriver RGBW to prevent write od table fail
	_set_od_rgbw_enable(FALSE);
	_overdriver_setting_without_adl(
			pu8OD_SRAM1,
			pu8OD_SRAM2,
			pu8OD_SRAM3,
			pu8OD_SRAM4,
			u16SepatateMode);

	//recover od setting
	_set_od_rgbw_enable(bEnable);

	//Recover OD status
	if (!g_bForceOdDisable)
		drv_hwreg_render_tcon_overdrive_set_od_enable(bEnable);
finally:
	PNL_FREE_MEM(pu8OD_SRAM1);
	PNL_FREE_MEM(pu8OD_SRAM2);
	PNL_FREE_MEM(pu8OD_SRAM3);
	PNL_FREE_MEM(pu8OD_SRAM4);
#endif
}
void _overdriver_init_proc(
			struct mtk_panel_context *pCon,
			u64 u64OD_MSB_Addr, u64 u64OD_LSB_Addr,
			uint16_t *pu16ODTbl, uint32_t u32ODTabSize, uint32_t u32OdModeType)
{
	u64 u64Offset;
	uint8_t u8MIUSel = 0;

	_KHal_XC_GetMiuOffset(u8MIUSel, &u64Offset);

	u64OD_MSB_Addr = u64OD_MSB_Addr - u64Offset;
	u64OD_LSB_Addr = u64OD_LSB_Addr - u64Offset;
	u64OD_MSB_Addr = (u64OD_MSB_Addr / BYTE_PER_WORD) & 0xFFFFFFFF;
	u64OD_LSB_Addr = (u64OD_LSB_Addr / BYTE_PER_WORD) & 0xFFFFFFFF;
	if (SUPPORT_OVERDRIVE)
		_overdriver_init(pCon, u64OD_MSB_Addr, u64OD_LSB_Addr, u32OdModeType);
}

void _overdriver_riu_setting_tbl(
			struct mtk_panel_context *pCon,
			u64 u64OD_MSB_Addr, u64 u64OD_LSB_Addr,
			uint16_t *pu16ODTbl, uint32_t u32ODTabSize,
			uint16_t u16SepatateMode, uint32_t u32OdModeType)
{
	u64 u64Offset;
	uint8_t u8MIUSel = 0;

	_KHal_XC_GetMiuOffset(u8MIUSel, &u64Offset);

	u64OD_MSB_Addr = u64OD_MSB_Addr - u64Offset;
	u64OD_LSB_Addr = u64OD_LSB_Addr - u64Offset;
	u64OD_MSB_Addr = (u64OD_MSB_Addr / BYTE_PER_WORD) & 0xFFFFFFFF;
	u64OD_LSB_Addr = (u64OD_LSB_Addr / BYTE_PER_WORD) & 0xFFFFFFFF;
	if (SUPPORT_OVERDRIVE) {
		_overdriver_init(pCon, u64OD_MSB_Addr, u64OD_LSB_Addr, u32OdModeType);
		_overdriver_setting_tbl(pu16ODTbl, u32ODTabSize, u16SepatateMode);
	}
}

bool _get_od_mem_info(
	u64 *_u64OD_MSB_Addr, u64 *_u64OD_LSB_Addr)
{
	bool bRet = TRUE;
	int ret = 0;
	//MMAP
	struct dts_mmap mmap;
	struct device_node *mmap_info;
	struct device_node *my_map;
	uint32_t reg[4];

	mmap_info = of_find_node_by_name(NULL, "mmap_info");
	my_map = of_find_node_by_name(mmap_info, "MI_DISP_OD_BUF");
	if (!my_map || !mmap_info) {
		TCON_DEBUG("mmap find fail\n");
		return false;
	}

	ret = of_property_read_u32_array(my_map, "reg", reg, ARRAY_SIZE(reg));

	memset(&mmap, 0, sizeof(mmap));
	mmap.address = reg[0]*FOURGIGABYTE + reg[1]; //32 bits limit.
	mmap.size = reg[3];

	if (ret < 0 || mmap.address == 0) {
		TCON_ERROR("Error: Get MI_DISP_OD_BUF buffer from DTS mmap failure\n");
		return FALSE;
	}

	*_u64OD_MSB_Addr = mmap.address;//BA address
	*_u64OD_LSB_Addr = mmap.address + mmap.size;
	TCON_DEBUG("od address = %llx, size = %llx\n", mmap.address, mmap.size);

	return bRet;
}

bool _mtk_tcon_od_reg_setting(struct mtk_panel_context *pCon, uint8_t *pu8TconTab,
	uint32_t u32OdModeType)
{
	bool bRet = TRUE;
	bool bSupportWChannel = FALSE;
	u64 _u32OD_MSB_Addr = 0;
	u64 _u32OD_LSB_Addr = 0;
	uint8_t u8RGBSeparateFlag = 0;
	uint8_t u8MaxODSizeFlag = 0;
	uint8_t u8TConBinVersion = 0;
	uint16_t u16TargetIndex = 0;
	uint16_t u16SepatateMode = 0;
	uint16_t u16Index = 0;
	uint16_t *pu16OverDriveTable = NULL;
	uint32_t _ODTbl_Size;
	uint32_t u32ADLSize = XC_AUTODOWNLOAD_OVERDRIVER_DEPTH*BYTE_PER_WORD;
	bool bEnableOD = FALSE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pu8TconTab);

	u8RGBSeparateFlag = (*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(0);
	u8MaxODSizeFlag = ((*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(2)) >> 2;
	u8TConBinVersion = (*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(3))));

	//TCON_DEBUG("u8RGBSeparateFlag = %d, u8MaxODSizeFlag = %d, u8TConBinVersion = %d\n",
	//                    u8RGBSeparateFlag, u8MaxODSizeFlag, u8TConBinVersion);

	if (u8TConBinVersion >= TCON_BIN_VERSION_RGBW) {
		bSupportWChannel =
			((*((unsigned char *)(pu8TconTab +
			TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(4)) >> 4;
		u16SepatateMode = E_PNL_OVERDRIVER_SEPARATE_R_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_G_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_B_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_W_ENABLE;
		u16TargetIndex = TCON_BIN_HEADER_BYTE_NUBMER(5);
	} else {
		u16SepatateMode = E_PNL_OVERDRIVER_SEPARATE_R_ENABLE|
						E_PNL_OVERDRIVER_SEPARATE_G_ENABLE|
						E_PNL_OVERDRIVER_SEPARATE_B_ENABLE;
		u16TargetIndex = TCON_BIN_HEADER_BYTE_NUBMER(4);
	}

	_get_od_mem_info(&_u32OD_MSB_Addr, &_u32OD_LSB_Addr);
	TCON_DEBUG("MSB addr=%llx, LSB addr=%llx\n", _u32OD_MSB_Addr, _u32OD_LSB_Addr);
	_overdriver_enable_wchannel(bSupportWChannel);


	if (!u8MaxODSizeFlag)
		_ODTbl_Size = OVERDRIVER_TABLE_SIZE_33X32;
	else
		_ODTbl_Size = OVERDRIVER_TABLE_SIZE_33X33;

	PNL_MALLOC_MEM(pu16OverDriveTable, sizeof(uint16_t) * MAX_OVERDRIVER_TABLE, bRet);

	if (!pu16OverDriveTable) {
		TCON_ERROR("pu16OverDriveTable is NULL parameter !!!!\n");
		return FALSE;
	}

	if (u8RGBSeparateFlag) {//RGB sepatate
//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_init_proc(
				pCon, _u32OD_MSB_Addr,
				_u32OD_LSB_Addr,
				pu16OverDriveTable,
				_ODTbl_Size,
				u32OdModeType);
		bEnableOD = _overdriver_adl_init(pCon, u32ADLSize);
//#endif
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_adl_setting_tbl(
				pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_R_ENABLE);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_R_ENABLE);
//#endif
		u16TargetIndex = u16TargetIndex + u16Index;
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_adl_setting_tbl(
				pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_G_ENABLE);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_G_ENABLE);
//#endif
		u16TargetIndex = u16TargetIndex + u16Index;
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_adl_setting_tbl(
				pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_B_ENABLE);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_B_ENABLE);
//#endif
		if (bSupportWChannel) {
			u16TargetIndex = u16TargetIndex + u16Index;
			for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
				pu16OverDriveTable[u16Index] = pu8TconTab[u16Index+u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_adl_setting_tbl(
				pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_W_ENABLE);
//else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_W_ENABLE);
//endif
		}
		_overdriver_adl_fire(u32ADLSize, bEnableOD);
	} else {
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

		_overdriver_riu_setting_tbl(pCon,
					_u32OD_MSB_Addr,
					_u32OD_LSB_Addr,
					pu16OverDriveTable,
					_ODTbl_Size,
					u16SepatateMode,
					u32OdModeType);
	}

	PNL_FREE_MEM(pu16OverDriveTable);
	TCON_FUNC_EXIT(bRet);
	return bRet;
}

void _vrr_overdriver_set_HwMode(bool bIsHwMode)
{
	drv_hwreg_render_tcon_overdrive_set_HwMode(bIsHwMode);
}

bool _mtk_tcon_vrr_od_reg_setting(struct mtk_panel_context *pCon,
	uint8_t *pu8TconTab, uint8_t u8ODTableIndex, bool bApply, uint32_t u32OdModeType)
{
	bool bRet = TRUE;
	bool bSupportWChannel = FALSE;
	u64 _u32OD_MSB_Addr = 0;
	u64 _u32OD_LSB_Addr = 0;
	uint8_t u8RGBSeparateFlag = 0;
	uint8_t u8MaxODSizeFlag = 0;
	uint8_t u8TConBinVersion = 0;
	uint16_t u16TargetIndex = 0;
	uint16_t u16SepatateMode = 0;
	uint16_t u16Index = 0;
	uint16_t *pu16OverDriveTable = NULL;
	uint32_t _ODTbl_Size;
	uint32_t u32ADLSize = XC_AUTODOWNLOAD_OVERDRIVER_DEPTH * BYTE_PER_WORD *
		(TOTAL_FREQ_TABLE + 1);
	bool bEnableOD = FALSE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter !!!!\n");
		return FALSE;
	}

	//TCON_CHECK_PARAMETER_NULL(pu8TconTab);

	u8RGBSeparateFlag = (*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(0);
	u8MaxODSizeFlag = ((*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(2)) >> 2;
	u8TConBinVersion = (*((unsigned char *)(pu8TconTab +
						TCON_BIN_HEADER_BYTE_NUBMER(3))));

	//TCON_DEBUG("u8RGBSeparateFlag = %d, u8MaxODSizeFlag = %d, u8TConBinVersion = %d\n",
	//                    u8RGBSeparateFlag, u8MaxODSizeFlag, u8TConBinVersion);

	if (u8TConBinVersion >= TCON_BIN_VERSION_RGBW) {
		bSupportWChannel =
			((*((unsigned char *)(pu8TconTab +
			TCON_BIN_HEADER_BYTE_NUBMER(2)))) & BIT(4)) >> 4;
		u16SepatateMode = E_PNL_OVERDRIVER_SEPARATE_R_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_G_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_B_ENABLE|
			E_PNL_OVERDRIVER_SEPARATE_W_ENABLE;
		u16TargetIndex = TCON_BIN_HEADER_BYTE_NUBMER(5);
	} else {
		u16SepatateMode = E_PNL_OVERDRIVER_SEPARATE_R_ENABLE|
						E_PNL_OVERDRIVER_SEPARATE_G_ENABLE|
						E_PNL_OVERDRIVER_SEPARATE_B_ENABLE;
		u16TargetIndex = TCON_BIN_HEADER_BYTE_NUBMER(4);
	}

	_get_od_mem_info(&_u32OD_MSB_Addr, &_u32OD_LSB_Addr);
	TCON_DEBUG("MSB addr=%llx, LSB addr=%llx\n", _u32OD_MSB_Addr, _u32OD_LSB_Addr);
	_overdriver_enable_wchannel(bSupportWChannel);


	if (!u8MaxODSizeFlag)
		_ODTbl_Size = OVERDRIVER_TABLE_SIZE_33X32;
	else
		_ODTbl_Size = OVERDRIVER_TABLE_SIZE_33X33;

	PNL_MALLOC_MEM(pu16OverDriveTable, sizeof(uint16_t) * MAX_OVERDRIVER_TABLE, bRet);

	if (!pu16OverDriveTable) {
		TCON_ERROR("pu16OverDriveTable is NULL parameter !!!!\n");
		return FALSE;
	}

	TCON_DEBUG("Tbl[%d] Fps=%d ~ %d\n", u8ODTableIndex,
		pCon->VRR_OD_info->stVRR_OD_Table[u8ODTableIndex].u16FreqLowBound,
		pCon->VRR_OD_info->stVRR_OD_Table[u8ODTableIndex].u16FreqHighBound);

	if (u8RGBSeparateFlag) {//RGB sepatate
//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_overdriver_init_proc(
				pCon, _u32OD_MSB_Addr,
				_u32OD_LSB_Addr,
				pu16OverDriveTable,
				_ODTbl_Size,
				u32OdModeType);
		bEnableOD = _vrr_overdriver_adl_init(pCon, u32ADLSize);
//#endif
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_vrr_overdriver_adl_setting_tbl(
			pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_R_ENABLE,
			u8ODTableIndex, pCon);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_R_ENABLE);
//#endif
		u16TargetIndex = u16TargetIndex + u16Index;
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_vrr_overdriver_adl_setting_tbl(
			pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_G_ENABLE,
			u8ODTableIndex, pCon);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_G_ENABLE);
//#endif
		u16TargetIndex = u16TargetIndex + u16Index;
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_vrr_overdriver_adl_setting_tbl(
			pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_B_ENABLE,
			u8ODTableIndex, pCon);
//#else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_B_ENABLE);
//#endif
		if (bSupportWChannel) {
			u16TargetIndex = u16TargetIndex + u16Index;
			for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
				pu16OverDriveTable[u16Index] = pu8TconTab[u16Index+u16TargetIndex];

//#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_OD_AUTODOWNLOAD == TRUE)
		_vrr_overdriver_adl_setting_tbl(
			pu16OverDriveTable, E_PNL_OVERDRIVER_SEPARATE_W_ENABLE,
			u8ODTableIndex, pCon);
//else
//		_overdriver_riu_setting_tbl(pCon,
//					_u32OD_MSB_Addr,
//					_u32OD_LSB_Addr,
//					pu16OverDriveTable,
//					_ODTbl_Size,
//					E_PNL_OVERDRIVER_SEPARATE_W_ENABLE);
//endif
		}

		if (bApply) {
			_vrr_overdriver_adl_fire(u32ADLSize, bEnableOD);
			_vrr_overdriver_set_HwMode(TRUE);
		}
	} else {
		for (u16Index = 0; u16Index < _ODTbl_Size; u16Index++)
			pu16OverDriveTable[u16Index] = pu8TconTab[u16Index + u16TargetIndex];

		_overdriver_riu_setting_tbl(pCon,
					_u32OD_MSB_Addr,
					_u32OD_LSB_Addr,
					pu16OverDriveTable,
					_ODTbl_Size,
					u16SepatateMode,
					u32OdModeType);
	}

	PNL_FREE_MEM(pu16OverDriveTable);
	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool _mtk_tcon_od_str_proc(const uint16_t u16Val)
{
	bool bRet = TRUE;
	bool bEnable = FALSE;

	if (u16Val >= E_TCON_OD_VAL_MAX) {
		TCON_ERROR("Input value is over the range\n");
		return FALSE;
	}

	if (u16Val == E_TCON_OD_VAL_OFF) {
		//restore current od state
		bEnable = drv_hwreg_render_tcon_overdrive_is_od_enable();
		g_enOdResumeState = (bEnable) ? E_TCON_OD_VAL_ON : E_TCON_OD_VAL_OFF;

		//set od disable
		if (bEnable) {
			drv_hwreg_render_tcon_overdrive_set_od_enable(FALSE);

			//disable miu
			_set_od_miu_mask(TRUE);

			//so delay 1 vsync for OD double buffer
			mdelay(ONE_VSYNC_TIME_MS);
		}

		TCON_DEBUG("OD has suspended(=%d), and then resume state='%s'\n",
			_is_od_enable(), (g_enOdResumeState == E_TCON_OD_VAL_ON) ? "On" : "Off");
	} else if (u16Val == E_TCON_OD_VAL_ON && g_enOdResumeState == E_TCON_OD_VAL_ON) {
		//recover last od state
		drv_hwreg_render_tcon_overdrive_set_od_enable(TRUE);

		//enable miu
		_set_od_miu_mask(FALSE);

		g_enOdResumeState = E_TCON_OD_VAL_MAX;
		TCON_DEBUG("Recover OD enable state\n");
	}

	return bRet;
}

bool mtk_tcon_od_setting(struct mtk_panel_context *pCon, uint32_t u32OdModeType)
{
	bool data_exist = FALSE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	st_tcon_tab_info stInfo;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pu8TconTab is NULL parameter !!!!\n");
		return FALSE;
	}

	g_bOdReadyToSetOn = FALSE;
	g_u32PnlVersion = pCon->hw_info.pnl_lib_version;

	if (pCon->cus.overdrive_en) {
		data_exist = is_tcon_data_exist(&pdata_buf, &data_len);
	} else {
		_set_od_enable(FALSE, u32OdModeType);
		bRet = FALSE;
		TCON_FUNC_EXIT(bRet);
		return bRet;
	}

	if (data_exist) {
		memset(&stInfo, 0, sizeof(st_tcon_tab_info));
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_OVERDRIVER;
		if ((get_tcon_dump_table(&stInfo) == TRUE) &&
			(stInfo.u8Version > TCON20_VERSION)) {
			_mtk_tcon_od_reg_setting(pCon, stInfo.pu8Table, u32OdModeType);

			g_bOdReadyToSetOn = TRUE;
			if (g_bTconPowerOn && g_bOdReadyToSetOn)
				_set_od_enable(TRUE, u32OdModeType);

			TCON_DEBUG("Tcon power on=%d OD set on=%d\n",
					g_bTconPowerOn, g_bOdReadyToSetOn);
		}
	} else {
		_set_od_enable(FALSE, u32OdModeType);
		bRet = FALSE;
		TCON_FUNC_EXIT(bRet);
		return bRet;
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool mtk_tcon_vrr_od_setting(struct mtk_panel_context *pCon,
	unsigned char uTableIdx, bool bApply, uint32_t u32OdModeType)
{
	bool data_exist = FALSE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	st_tcon_tab_info stInfo;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pu8TconTab is NULL parameter !!!!\n");
		return FALSE;
	}

	g_bOdReadyToSetOn = FALSE;
	g_u32PnlVersion = pCon->hw_info.pnl_lib_version;

	if (pCon->vrr_od_En) {
		data_exist = is_vrr_tcon_data_exist(&pdata_buf, &data_len, uTableIdx);
	} else {
		_set_od_enable(FALSE, u32OdModeType);
		bRet = FALSE;
		TCON_FUNC_EXIT(bRet);
		return bRet;
	}

	if (data_exist) {
		memset(&stInfo, 0, sizeof(st_tcon_tab_info));
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_OVERDRIVER;
		if ((get_tcon_dump_table(&stInfo) == TRUE) &&
			(stInfo.u8Version > TCON20_VERSION)) {
			_mtk_tcon_vrr_od_reg_setting(pCon, stInfo.pu8Table, uTableIdx, bApply, u32OdModeType);

			g_bOdReadyToSetOn = TRUE;
			if (bApply && g_bTconPowerOn && g_bOdReadyToSetOn)
				_set_od_enable(TRUE, u32OdModeType);

			TCON_DEBUG("Apply=%d Tcon power on=%d OD set on=%d\n",
					bApply, g_bTconPowerOn, g_bOdReadyToSetOn);
		}
	} else {
		_set_od_enable(FALSE, u32OdModeType);
		bRet = FALSE;
		TCON_FUNC_EXIT(bRet);
		return bRet;
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

int mtk_tcon_od_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType,
	const uint16_t u16Val, uint32_t u32OdModeType)
{
	int ret = 0;

	switch (enType) {
	case E_TCON_CMD_OD_EN:
		if (u16Val == E_TCON_OD_VAL_FORCE_DISABLE || u16Val == E_TCON_OD_VAL_FORCE_ENABLE) {
			g_bForceOdDisable = (u16Val) ? false : true;
			TCON_DEBUG("Force disable=%d\n", g_bForceOdDisable);
			ret = drv_hwreg_render_tcon_overdrive_set_od_enable((bool)u16Val);
		} else
			ret = _mtk_tcon_od_str_proc(u16Val);
		break;
	case E_TCON_CMD_OD_BYPASS:
		g_bForceOdBypass = (u16Val) ? true : false;
		TCON_DEBUG("Force bypass=%d\n", g_bForceOdBypass);
		ret = _set_od_bypass((bool)u16Val);
		break;
	case E_TCON_CMD_ONOFF:
		g_bTconPowerOn = (u16Val) ? true : false;
		if (g_bTconPowerOn && g_bOdReadyToSetOn)
			_set_od_enable(TRUE, u32OdModeType);
		TCON_DEBUG("Tcon power on=%d OD set on=%d\n", g_bTconPowerOn, g_bOdReadyToSetOn);
		break;
	default:
		TCON_ERROR("Unknown cmd type=%d\n", enType);
		return -ENODEV;
	}

	return ret;
}

int mtk_tcon_od_get_info(
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
		"Tcon od info:\n"
		"	- Dtso od enable = %d\n"
		"	- Dtso vrr od enable = %d\n"
		"	- Bypass = %d\n"
		"	- Force bypass = %d\n"
		"	- Enable = %d\n"
		"	- Force disable = %d\n",
		pCon->cus.overdrive_en,
		pCon->vrr_od_En,
		_is_od_bypass(),
		g_bForceOdBypass,
		_is_od_enable(),
		g_bForceOdDisable);

	return ((ret < 0) || (ret > u32InfoSize)) ? -EINVAL : ret;
}
