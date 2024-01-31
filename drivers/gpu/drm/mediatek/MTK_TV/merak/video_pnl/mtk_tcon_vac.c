// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek Panel driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/math64.h>

#include "mtk_tcon_vac.h"
#include "mtk_tv_drm_video_panel.h"
#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_vac.h"

bool g_bForceVacDisable;
bool g_bForceVacBypass;

//Dump EVA Table
void _set_vac_bypass(bool bEnable)
{
	if (!bEnable && g_bForceVacBypass)
		bEnable = true;

#if SUPPORT_VAC_256
	drv_hwreg_render_tcon_vac_set_vac_bypass(bEnable);
#endif
}

bool _get_vac_bypass(void)
{
#if SUPPORT_VAC_256
	if (drv_hwreg_render_tcon_vac_get_vac_bypass() >> 15)
		return TRUE;
#endif
	return FALSE;
}

void _set_vac_enable(bool bEnable)
{
	if (bEnable && g_bForceVacDisable)
		bEnable = false;

	drv_hwreg_render_tcon_vac_set_vac_enable(bEnable);
}

bool _get_vac_enable(void)
{
	return drv_hwreg_render_tcon_vac_get_vac_enable();
}

bool _set_eva_tbl(ST_PNL_EVA_TBL *pstEvaTbl)
{
#if SUPPORT_VAC_256

	bool bVACBypass = 0;
	bool bVACEnable = 0;

	// Store VAC Bypass and enable flag
	bVACBypass = _get_vac_bypass();
	bVACEnable = _get_vac_enable();

	TCON_DEBUG("bVACBypass=%d bVACEnable=%d\n.", bVACBypass, bVACEnable);

	_set_vac_bypass(FALSE);
	_set_vac_enable(FALSE);

	drv_hwreg_render_tcon_vac_set_eva_tbl(pstEvaTbl);

	//Restore VAC status
	if (bVACEnable)
		_set_vac_enable(TRUE);

#endif
	return TRUE;
}

bool _get_eva_from_reg(ST_PNL_EVA_TBL *pstEvaTbl)
{
#if SUPPORT_VAC_256

	bool bVACBypass = 0;
	bool bVACEnable = 0;

	bVACBypass = _get_vac_bypass();
	bVACEnable = _get_vac_enable();

	TCON_DEBUG("bVACBypass=%d bVACEnable=%d\n.", bVACBypass, bVACEnable);

	if (PNL_RGB_LUT_CHECK(pstEvaTbl->pu16RChannel0,
				pstEvaTbl->pu16GChannel0,
				pstEvaTbl->pu16BChannel0)
		||
		PNL_RGB_LUT_CHECK(pstEvaTbl->pu16RChannel1,
				pstEvaTbl->pu16GChannel1,
				pstEvaTbl->pu16BChannel1)) {
		TCON_DEBUG("Table error\n.");
		return FALSE;
	}

	_set_vac_bypass(FALSE); // VSC is ON

	drv_hwreg_render_tcon_vac_set_eva_tbl(pstEvaTbl);

	//Restore VAC status
	if (bVACEnable)
		_set_vac_enable(TRUE);

#endif
	return TRUE;
}

bool _use_adl_set_eva_tbl(ST_PNL_EVA_TBL *pstEvaTbl)
{
#if (defined(UFO_XC_AUTO_DOWNLOAD) && ENABLE_VAC_AUTODOWNLOAD == TRUE)
	uint32_t u32index = 0;
	uint8_t *pu8EvaTbl;
	bool bRet = TRUE, bAdlRet = FALSE;
	uint8_t *pu8R0 = NULL,
			*pu8G0 = NULL,
			*pu8B0 = NULL,
			*pu8R1 = NULL,
			*pu8G1 = NULL,
			*pu8B1 = NULL;

	PNL_MALLOC_MEM(pu8EvaTbl, XC_AUTODOWNLOAD_VAC_MEM_SIZE, bRet);

	if (bRet) {
		pu8R0 = pu8EvaTbl;
		pu8G0 = pu8R0 + XC_AUTODOWNLOAD_VAC_CHANNEL_OFFSET;  // 16entry  * 32 byte
		pu8B0 = pu8G0 + XC_AUTODOWNLOAD_VAC_CHANNEL_OFFSET;
		pu8R1 = pu8B0 + XC_AUTODOWNLOAD_VAC_CHANNEL_OFFSET;
		pu8G1 = pu8R1 + XC_AUTODOWNLOAD_VAC_CHANNEL_OFFSET;
		pu8B1 = pu8G1 + XC_AUTODOWNLOAD_VAC_CHANNEL_OFFSET;

	for (u32index = 0 ; u32index < EVA_LUT_SIZE_256; u32index++) {
		//LUT 0
		*((uint16_t *)pu8R0 + u32index) = pstEvaTbl->pu16RChannel0[u32index];
		*((uint16_t *)pu8G0 + u32index) = pstEvaTbl->pu16GChannel0[u32index];
		*((uint16_t *)pu8B0 + u32index) = pstEvaTbl->pu16BChannel0[u32index];
		//LUT 1
		*((uint16_t *)pu8R1 + u32index) = pstEvaTbl->pu16RChannel1[u32index];
		*((uint16_t *)pu8G1 + u32index) = pstEvaTbl->pu16GChannel1[u32index];
		*((uint16_t *)pu8B1 + u32index) = pstEvaTbl->pu16BChannel1[u32index];
	}

		bAdlRet = set_adl_proc(
					(uint8_t *)pu8EvaTbl,
					XC_AUTODOWNLOAD_VAC_MEM_SIZE,
					E_SM_TCON_ADL_VAC,
					E_SM_ADL_TRIGGER_MODE);

		if (!bAdlRet) {
			TCON_ERROR("Auto download setting Failed!!\n");
			PNL_FREE_MEM(pu8EvaTbl);
			return FALSE;
		}

		TCON_DEBUG("Auto download setting Success!!\n");

		//_set_vac_enable(TRUE);
		PNL_FREE_MEM(pu8EvaTbl);
	} else
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

//EVA Table
static bool _evatbl_mem_allocate(ST_PNL_EVA_TBL *pstEvaTbl)
{
	bool bRet = TRUE;

	PNL_MALLOC_MEM(pstEvaTbl->pu16RChannel0,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	PNL_MALLOC_MEM(pstEvaTbl->pu16GChannel0,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	PNL_MALLOC_MEM(pstEvaTbl->pu16BChannel0,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	PNL_MALLOC_MEM(pstEvaTbl->pu16RChannel1,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	PNL_MALLOC_MEM(pstEvaTbl->pu16GChannel1,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	PNL_MALLOC_MEM(pstEvaTbl->pu16BChannel1,
					sizeof(uint16_t)*EVA_TABLE_MATRIX_SIZE_257X2,
					bRet);

	if (!bRet) {
		TCON_ERROR("EVA_MALLOC_MEM Fail!!!!!!!!!!\n");

		PNL_FREE_MEM(pstEvaTbl->pu16RChannel0);
		PNL_FREE_MEM(pstEvaTbl->pu16GChannel0);
		PNL_FREE_MEM(pstEvaTbl->pu16BChannel0);
		PNL_FREE_MEM(pstEvaTbl->pu16RChannel1);
		PNL_FREE_MEM(pstEvaTbl->pu16GChannel1);
		PNL_FREE_MEM(pstEvaTbl->pu16BChannel1);
		return bRet;
	}

	if (pstEvaTbl->u16TblSize == EVA_TABLE_MATRIX_SIZE_17X2) {
		PNL_MALLOC_MEM(pstEvaTbl->pu16RChannel0_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		PNL_MALLOC_MEM(pstEvaTbl->pu16GChannel0_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		PNL_MALLOC_MEM(pstEvaTbl->pu16BChannel0_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		PNL_MALLOC_MEM(pstEvaTbl->pu16RChannel1_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		PNL_MALLOC_MEM(pstEvaTbl->pu16GChannel1_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		PNL_MALLOC_MEM(pstEvaTbl->pu16BChannel1_17,
						sizeof(uint16_t)*pstEvaTbl->u16TblSize,
						bRet);

		if (!bRet) {
			TCON_ERROR("EVA_MALLOC_MEM Fail!!!!!!!!!!\n");

			PNL_FREE_MEM(pstEvaTbl->pu16RChannel0_17);
			PNL_FREE_MEM(pstEvaTbl->pu16GChannel0_17);
			PNL_FREE_MEM(pstEvaTbl->pu16BChannel0_17);

			PNL_FREE_MEM(pstEvaTbl->pu16RChannel1_17);
			PNL_FREE_MEM(pstEvaTbl->pu16GChannel1_17);
			PNL_FREE_MEM(pstEvaTbl->pu16BChannel1_17);
			return bRet;
		}
	}
	return bRet;
}

void _convert_eva_17to257_tbl(ST_PNL_EVA_TBL *pstEvaTbl)
{
	uint16_t u16Index = 0;
	uint16_t i = 0;

	if (PNL_RGB_LUT_CHECK(
				pstEvaTbl->pu16RChannel0,
				pstEvaTbl->pu16GChannel0,
				pstEvaTbl->pu16BChannel0)
		||
		PNL_RGB_LUT_CHECK(
				pstEvaTbl->pu16RChannel1,
				pstEvaTbl->pu16GChannel1,
				pstEvaTbl->pu16BChannel1)
		||
		PNL_RGB_LUT_CHECK(
				pstEvaTbl->pu16RChannel0_17,
				pstEvaTbl->pu16GChannel0_17,
				pstEvaTbl->pu16BChannel0_17)
		||
		PNL_RGB_LUT_CHECK(
				pstEvaTbl->pu16RChannel1_17,
				pstEvaTbl->pu16GChannel1_17,
				pstEvaTbl->pu16BChannel1_17)) {
		TCON_ERROR("RGB Table is null\n.");
		return;
	}

	if (pstEvaTbl->u16TblSize == EVA_TABLE_MATRIX_SIZE_257X2)
		return;

	for (u16Index = 0;
		u16Index < EVA_TABLE_MATRIX_SIZE_257X2;
		u16Index++) { //0~256
		i = u16Index/EVA_CONVERT_UNIT;
		if (i+1 < pstEvaTbl->u16TblSize) {
			//llut0
			pstEvaTbl->pu16RChannel0[u16Index] =
						((pstEvaTbl->pu16RChannel0_17[i+1]-
						pstEvaTbl->pu16RChannel0_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16RChannel0_17[i])*EVA_CONVERT_UNIT);

			pstEvaTbl->pu16GChannel0[u16Index] =
						((pstEvaTbl->pu16GChannel0_17[i+1]-
						pstEvaTbl->pu16GChannel0_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16GChannel0_17[i])*EVA_CONVERT_UNIT);

			pstEvaTbl->pu16BChannel0[u16Index] =
						((pstEvaTbl->pu16BChannel0_17[i+1]-
						pstEvaTbl->pu16BChannel0_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16BChannel0_17[i])*EVA_CONVERT_UNIT);

			//llut1
			pstEvaTbl->pu16RChannel1[u16Index] =
						((pstEvaTbl->pu16RChannel1_17[i+1]-
						pstEvaTbl->pu16RChannel1_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16RChannel1_17[i])*EVA_CONVERT_UNIT);

			pstEvaTbl->pu16GChannel1[u16Index] =
						((pstEvaTbl->pu16GChannel1_17[i+1]-
						pstEvaTbl->pu16GChannel1_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16GChannel1_17[i])*EVA_CONVERT_UNIT);

			pstEvaTbl->pu16BChannel1[u16Index] =
						((pstEvaTbl->pu16BChannel1_17[i+1]-
						pstEvaTbl->pu16BChannel1_17[i])*
							(u16Index%EVA_CONVERT_UNIT))+
						((pstEvaTbl->pu16BChannel1_17[i])*EVA_CONVERT_UNIT);
		} else { // i=16 max value
			pstEvaTbl->pu16RChannel0[u16Index] =
					pstEvaTbl->pu16RChannel0_17[i]*EVA_CONVERT_UNIT;
			pstEvaTbl->pu16GChannel0[u16Index] =
					pstEvaTbl->pu16GChannel0_17[i]*EVA_CONVERT_UNIT;
			pstEvaTbl->pu16BChannel0[u16Index] =
					pstEvaTbl->pu16BChannel0_17[i]*EVA_CONVERT_UNIT;

			pstEvaTbl->pu16RChannel1[u16Index] =
					pstEvaTbl->pu16RChannel1_17[i]*EVA_CONVERT_UNIT;
			pstEvaTbl->pu16GChannel1[u16Index] =
					pstEvaTbl->pu16GChannel1_17[i]*EVA_CONVERT_UNIT;
			pstEvaTbl->pu16BChannel1[u16Index] =
					pstEvaTbl->pu16BChannel1_17[i]*EVA_CONVERT_UNIT;
		}
	}
}

bool mtk_tcon_vac_setting(void)
{
#if SUPPORT_VAC_256
	uint16_t u16TargetIndex = 0;
	uint16_t pu16max_value[EVA_MAX_VALUE_CNANNEL];
	pST_PNL_EVA_SUB_HEADER pstEvaSubHdr;
	bool bRetLut = FALSE, bChannelSeparate = FALSE;
	ST_PNL_EVA_TBL stEvaTbl;
	uint16_t u16Index;
	unsigned char *pdata_buf = NULL;
	bool data_exist = FALSE;
	loff_t data_len = 0;
	uint8_t *TconDumpAll;
	st_tcon_tab_info stInfo;
	bool bRet = TRUE;
	int i;

	data_exist = is_tcon_data_exist(&pdata_buf, &data_len);
	if (!data_exist) {
		TCON_DEBUG("VAC data does not exist.\n.");
		return FALSE;
	}

	TCON_DEBUG("VAC load success!\n.");

	memset(&stInfo, 0, sizeof(st_tcon_tab_info));
	stInfo.pu8Table = pdata_buf;
	stInfo.u8TconType = E_TCON_TAB_TYPE_VAC_TABLE;

	if (get_tcon_dump_table(&stInfo) != TRUE)
		return FALSE;

	memcpy(pu16max_value,
			(stInfo.pu8Table + EVA_MAX_VALUE_OFFSET),
			sizeof(uint16_t)*EVA_MAX_VALUE_CNANNEL);

	pstEvaSubHdr = (pST_PNL_EVA_SUB_HEADER)(stInfo.pu8Table + EVA_MAINHEADER_OFFSET);
	bChannelSeparate = pstEvaSubHdr->u8SeparateFlag & _BIT(0);

	TCON_DEBUG("channel separate=%d table matrix=%d table bit=%d\n",
				bChannelSeparate,
				pstEvaSubHdr->u8TableMatrix,
				pstEvaSubHdr->u8TableBit);

	for (u16Index = 0; u16Index < EVA_MAX_VALUE_CNANNEL; u16Index++)
		TCON_DEBUG("channel=%d max value=%d\n", u16Index, pu16max_value[u16Index]);

	memset(&stEvaTbl, 0, sizeof(ST_PNL_EVA_TBL));
	memset(&TconDumpAll, 0, sizeof(TconDumpAll));
	stEvaTbl.u16TblSize =
		(pstEvaSubHdr->u8TableMatrix ?
		EVA_TABLE_MATRIX_SIZE_257X2 : EVA_TABLE_MATRIX_SIZE_17X2);

	if (_evatbl_mem_allocate(&stEvaTbl) == FALSE)
		goto finally;

	u16TargetIndex = EVA_DATA_START;
	if (stEvaTbl.u16TblSize == EVA_TABLE_MATRIX_SIZE_17X2) {
		// get table from bin
		if (bChannelSeparate) {
			PNL_MALLOC_MEM(TconDumpAll,
							sizeof(uint8_t)*stInfo.u32ReglistSize,
							bRet);

			if (!bRet) {
				TCON_ERROR("TconDumpAll allocate mem fail\n");
				goto finally;
			}

			memcpy(TconDumpAll,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint8_t)*stInfo.u32ReglistSize);

			for (i = 0; i < stEvaTbl.u16TblSize; i++) {
				stEvaTbl.pu16RChannel0_17[i] = (uint8_t)TconDumpAll[3*i]; //R lut0
				stEvaTbl.pu16GChannel0_17[i] = (uint8_t)TconDumpAll[3*i+1]; //G lut0
				stEvaTbl.pu16BChannel0_17[i] = (uint8_t)TconDumpAll[3*i+2]; //B lut0
				stEvaTbl.pu16RChannel1_17[i] =
						(uint8_t)TconDumpAll[3*i+
						(uint32_t)stEvaTbl.u16TblSize*3]; //R lut1

				stEvaTbl.pu16GChannel1_17[i] =
						(uint8_t)TconDumpAll[3*i+1+
						(uint32_t)stEvaTbl.u16TblSize*3]; //G lut1
				stEvaTbl.pu16BChannel1_17[i] =
						(uint8_t)TconDumpAll[3*i+2+
						(uint32_t)stEvaTbl.u16TblSize*3]; //B lut1
			}

			TCON_DEBUG("====================RGB LUT0====================\n");

			for (i = 0; i < (uint32_t)stEvaTbl.u16TblSize; i++)
				TCON_DEBUG("%d  %d  %d\n.",
					stEvaTbl.pu16RChannel0_17[i],
					stEvaTbl.pu16GChannel0_17[i],
					stEvaTbl.pu16BChannel0_17[i]);

			TCON_DEBUG("\n====================RGB LUT1====================\n");
			for (i = 0; i < (uint32_t)stEvaTbl.u16TblSize; i++)
				TCON_DEBUG("%d  %d  %d\n.",
					stEvaTbl.pu16RChannel1_17[i],
					stEvaTbl.pu16GChannel1_17[i],
					stEvaTbl.pu16BChannel1_17[i]);

		} else {
			memcpy(stEvaTbl.pu16RChannel0_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut0

			memcpy(stEvaTbl.pu16GChannel0_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut0

			memcpy(stEvaTbl.pu16BChannel0_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut0

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16RChannel1_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut1

			memcpy(stEvaTbl.pu16GChannel1_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut1

			memcpy(stEvaTbl.pu16BChannel1_17,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut1
		}

		// replace last with max
		stEvaTbl.pu16RChannel0_17[stEvaTbl.u16TblSize-1] = pu16max_value[0]/16;
		stEvaTbl.pu16GChannel0_17[stEvaTbl.u16TblSize-1] = pu16max_value[1]/16;
		stEvaTbl.pu16BChannel0_17[stEvaTbl.u16TblSize-1] = pu16max_value[2]/16;
		stEvaTbl.pu16RChannel1_17[stEvaTbl.u16TblSize-1] = pu16max_value[3]/16;
		stEvaTbl.pu16GChannel1_17[stEvaTbl.u16TblSize-1] = pu16max_value[4]/16;
		stEvaTbl.pu16BChannel1_17[stEvaTbl.u16TblSize-1] = pu16max_value[5]/16;
		//convert to 257
		_convert_eva_17to257_tbl(&stEvaTbl);
	} else {
		// get table from bin
		if (bChannelSeparate) {
			memcpy(stEvaTbl.pu16RChannel0,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut0

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16RChannel1,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut1

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16GChannel0,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut0

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16GChannel1,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut1

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16BChannel0,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut0

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16BChannel1,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut1
		} else {
			memcpy(stEvaTbl.pu16RChannel0,
				(stInfo.pu8Table+u16TargetIndex),
				sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut0

			memcpy(stEvaTbl.pu16GChannel0,
				(stInfo.pu8Table+u16TargetIndex),
				sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut0

			memcpy(stEvaTbl.pu16BChannel0,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut0

			u16TargetIndex += stEvaTbl.u16TblSize*2;

			memcpy(stEvaTbl.pu16RChannel1,
					(stInfo.pu8Table+u16TargetIndex),
					sizeof(uint16_t)*stEvaTbl.u16TblSize); //R lut1

			memcpy(stEvaTbl.pu16GChannel1,
			(stInfo.pu8Table+u16TargetIndex),
			sizeof(uint16_t)*stEvaTbl.u16TblSize); //G lut1

			memcpy(stEvaTbl.pu16BChannel1,
			(stInfo.pu8Table+u16TargetIndex),
			sizeof(uint16_t)*stEvaTbl.u16TblSize); //B lut1
		}
	}

	bRetLut = _use_adl_set_eva_tbl(&stEvaTbl);
	if (bRetLut != TRUE) {
		// riu
		bRetLut = _set_eva_tbl(&stEvaTbl);
		TCON_DEBUG(" Eva table fail\n.");
	}

finally:
	PNL_FREE_MEM(stEvaTbl.pu16RChannel0);
	PNL_FREE_MEM(stEvaTbl.pu16GChannel0);
	PNL_FREE_MEM(stEvaTbl.pu16BChannel0);
	PNL_FREE_MEM(stEvaTbl.pu16RChannel1);
	PNL_FREE_MEM(stEvaTbl.pu16GChannel1);
	PNL_FREE_MEM(stEvaTbl.pu16BChannel1);

	if (stEvaTbl.u16TblSize == EVA_TABLE_MATRIX_SIZE_17X2) {
		PNL_FREE_MEM(stEvaTbl.pu16RChannel0_17);
		PNL_FREE_MEM(stEvaTbl.pu16GChannel0_17);
		PNL_FREE_MEM(stEvaTbl.pu16BChannel0_17);
		PNL_FREE_MEM(stEvaTbl.pu16RChannel1_17);
		PNL_FREE_MEM(stEvaTbl.pu16GChannel1_17);
		PNL_FREE_MEM(stEvaTbl.pu16BChannel1_17);
		PNL_FREE_MEM(TconDumpAll);
	}
	TCON_DEBUG("VAC setting done\n.");
	return bRetLut;
#else
	TCON_DEBUG("Not support VAC 256 setting\n.");
	return FALSE;
#endif
}

bool mtk_tcon_vac_reg_setting(uint8_t *pu8TconTab, uint16_t u16RegisterCount)
{
	uint32_t u32tabIdx = 0;
	uint32_t u32Addr = 0;
	uint8_t u8Mask = 0;
	uint8_t u8Value = 0;

	while (u16RegisterCount--) {
		u32Addr = (((uint32_t)pu8TconTab[u32tabIdx] << 24) +
					((uint32_t)pu8TconTab[(u32tabIdx + 1)] << 16) +
					((uint32_t)pu8TconTab[(u32tabIdx + 2)] << 8) +
					pu8TconTab[(u32tabIdx + 3)]) & 0x7FFFFFFF;
		u8Mask  = pu8TconTab[(u32tabIdx + 4)] & 0xFF;
		u8Value = pu8TconTab[(u32tabIdx + 5)] & 0xFF;

		TCON_INFO("addr=%04tx, msk=%02x, val=%02x\n", (ptrdiff_t)u32Addr, u8Mask, u8Value);
		drv_hwreg_render_tcon_vac_reg_setting(u32Addr, u8Value, u8Mask);

		u32tabIdx = u32tabIdx + 6;
	}
	TCON_DEBUG("VAC_REG setting done\n.");

	return TRUE;
}

int mtk_tcon_vac_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val)
{
	int ret = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	switch (enType) {
	case E_TCON_CMD_VAC_EN:
		g_bForceVacDisable = (u16Val) ? false : true;
		TCON_DEBUG("Force disable=%d\n", g_bForceVacDisable);
		_set_vac_enable((bool)u16Val);
		break;
	case E_TCON_CMD_VAC_BYPASS:
		g_bForceVacBypass = (u16Val) ? true : false;
		TCON_DEBUG("Force bypass=%d\n", g_bForceVacBypass);
		_set_vac_bypass((bool)u16Val);
		break;
	default:
		TCON_ERROR("Unknown cmd type=%d\n", enType);
		return -ENODEV;
	}

	return ret;

}

int mtk_tcon_vac_get_info(
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
		"Tcon vac info:\n"
		"	- Bypass = %d\n"
		"	- Force bypass = %d\n"
		"	- Enable = %d\n"
		"	- Force disable = %d\n",
		_get_vac_bypass(),
		g_bForceVacBypass,
		_get_vac_enable(),
		g_bForceVacDisable);

	return ((ret < 0) || (ret > u32InfoSize)) ? -EINVAL : ret;
}
