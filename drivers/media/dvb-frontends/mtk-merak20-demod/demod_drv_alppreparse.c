// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
//------------------------------------------------------
// Driver Compiler Options
//------------------------------------------------------

//------------------------------------------------------
// Include Files
//------------------------------------------------------

#include <media/dvb_frontend.h>
#include "demod_drv_alppreparse.h"

#if !defined MTK_PROJECT
#ifdef MSOS_TYPE_LINUX_KERNEL
#include <linux/string.h>
#include <linux/kernel.h>
#if defined(CONFIG_CC_IS_CLANG) && defined(CONFIG_MSTAR_CHIP)
#include <linux/module.h>
#endif
#else
#include <string.h>
#include <stdio.h>
#include <math.h>
#endif
#endif

#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
#include "MsOS.h"
#include "drvMMIO.h"
#endif

#if	DMD_ATSC_3PARTY_EN
// Please add header files needed by 3 perty platform
#endif

#ifdef MSOS_TYPE_LINUX_KERNEL
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#endif

//------------------------------------------------------
//	Local	Defines
//------------------------------------------------------
#ifdef MS_DEBUG
#define	DMD_DBG(x)			(x)
#else
#define	DMD_DBG(x)			//(x)
#endif

//#define L_Mode
#define Repeat16BytesPatch_En 1
//------------------------------------------------------
//	Global Variables
//------------------------------------------------------
static u8 bPLPEverStart[4] = {0, 0, 0, 0};
static u16 u16L2Remained[4] = {0, 0, 0, 0};
static u8 *pALPQueue[4];

#ifdef L_Mode
static u8 u8Known_PLP_ID_63[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static u8 bBBStarted;
static u8 *pBBtemp;
static u16 u16BBtempWriteindex;
#endif
//------------------------------------------------
//	Local	Functions
//------------------------------------------------
void _BB2ALP_Parser_Reset(void)
{
	bPLPEverStart[0] = 0;
	u16L2Remained[0] = 0;
	bPLPEverStart[1] = 0;
	u16L2Remained[1] = 0;
	bPLPEverStart[2] = 0;
	u16L2Remained[2] = 0;
	bPLPEverStart[3] = 0;
	u16L2Remained[3] = 0;
	#ifdef L_Mode
	u8Known_PLP_ID_63[0] = 0xFF;
	u8Known_PLP_ID_63[1] = 0xFF;
	u8Known_PLP_ID_63[2] = 0xFF;
	u8Known_PLP_ID_63[3] = 0xFF;
	bBBStarted = 0;
	u16BBtempWriteindex = 0;
	#endif
}

u8 mdrv_atsc3_bb2alp_parser_init(void)
{
	pALPQueue[0] = kmalloc(0x10000, GFP_KERNEL);
	pALPQueue[1] = kmalloc(0x10000, GFP_KERNEL);
	pALPQueue[2] = kmalloc(0x10000, GFP_KERNEL);
	pALPQueue[3] = kmalloc(0x10000, GFP_KERNEL);
	#ifdef L_Mode
	pBBtemp = kmalloc(0x10000, GFP_KERNEL);
	#endif
	_BB2ALP_Parser_Reset();

	#ifdef L_Mode
	return (pALPQueue[0] != NULL && pALPQueue[1] != NULL &&
	pALPQueue[2] != NULL && pALPQueue[3] != NULL && pBBtemp != NULL);
	#else
	return (pALPQueue[0] != NULL && pALPQueue[1] != NULL &&
	pALPQueue[2] != NULL && pALPQueue[3] != NULL);
	#endif
}

void mdrv_atsc3_bb2alp_parser_close(void)
{
	kfree(pALPQueue[0]);
	kfree(pALPQueue[1]);
	kfree(pALPQueue[2]);
	kfree(pALPQueue[3]);
	#ifdef L_Mode
	kfree(pBBtemp);
	#endif
}

static u16 _GetALPLength(u8 *pu8ALP_Start)
{
	u16 u16ALP_length;

	if ((pu8ALP_Start[0] & 0x08) == 0x08) {
		u16ALP_length = pu8ALP_Start[2] & 0xF8;

		u16ALP_length |= pu8ALP_Start[0] & 0x07;

		u16ALP_length = (u16ALP_length << 8) + pu8ALP_Start[1];

		u16ALP_length += 3;
	} else {
		u16ALP_length = pu8ALP_Start[0] & 0x07;

		u16ALP_length = (u16ALP_length << 8) + pu8ALP_Start[1];

		u16ALP_length += 2;
	}

	if ((pu8ALP_Start[0] & 0xE0) == 0x80) //signaling
		return (u16ALP_length + 5);
	else
		return u16ALP_length;
}

#ifndef L_Mode
u8 mdrv_atsc3_bb2alp_parser(u8 *pu8BBStart,
u32 u32BBSize, u8 *pu8ALPStart, u32 *pu32ALPSize)
{
	u16 u16ALPLength;
	u16 u16L1;
	u16 u16L2;
	u16 u16BBReadindex = 0;
	u16 u16ALPWriteindex;
	u8  bPTP_ALP;
	u8  u8PLP_ID;
	static u8 u8PLP_ID_temp;
	#if Repeat16BytesPatch_En
	u8 bRepeat16Bytes;
	u8 u8temp;
	#endif

	if (pu8BBStart == NULL || pu8ALPStart == NULL)
		return FALSE;

	bPTP_ALP = ((*pu8BBStart & 0x04) == 0x04) ? 1 : 0;
	u8PLP_ID = *pu8BBStart & 0x03;

	u16L1 = (((u16)(*(pu8BBStart + 1)) << 8) | (u16)(*(pu8BBStart + 2))) & 0x1FFF;
	u16L2 = (((u16)(*(pu8BBStart + 3)) << 8) | (u16)(*(pu8BBStart + 4))) & 0x1FFF;

	DMD_DBG(PRINT("[%s][%d]L1 = 0x%x, L2 = 0x%x\n",
	__func__, __LINE__, u16L1, u16L2));
	if ((u16L1 != 0x1fff && u16L1 > u16L2) || (u16L2 == 0) || (u16L2 + 5 != u32BBSize)) {
		#if Repeat16BytesPatch_En
		if ((u16L2 + 5) == (u32BBSize - 16)) {
			bRepeat16Bytes = 1;
			for (u8temp = 0; u8temp < 16; u8temp++) {
				if (pu8BBStart[u32BBSize - 1 - u8temp] !=
				pu8BBStart[u32BBSize - 1 - 16 - u8temp]) {
					bRepeat16Bytes = 0;
					break;
				}
			}
			if (bRepeat16Bytes)
				u32BBSize -= 16;
		} else {
			_BB2ALP_Parser_Reset();
			PRINT("[%s][%d] ALP preparser error\n", __func__, __LINE__);
			return FALSE;
		}
		#else
		_BB2ALP_Parser_Reset();
		PRINT("[%s][%d] ALP preparser error\n", __func__, __LINE__);

		return FALSE;
		#endif
	}

	if (bPTP_ALP) {
		if (u16L1 != 0 || u16L2 != 29) {
			_BB2ALP_Parser_Reset();
			return FALSE;
		}
		memcpy(pu8ALPStart, pu8BBStart + 5, 29);
		*pu32ALPSize = 29;
		DMD_DBG(PRINT("[%s][%d] bPTP_ALP = 1\n",
		__func__, __LINE__));
		return TRUE;
	}

	if (!bPLPEverStart[u8PLP_ID] || (u16L1 == 0)) {
		if (u16L1 == 0x1fff) {
			*pu32ALPSize = 0;
			return TRUE;
		}

		if ((u16L1 == 0) && (u16L2Remained[u8PLP_ID] != 0))
			PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);

		bPLPEverStart[u8PLP_ID] = 1;
		u16L2 -= u16L1;
		u16BBReadindex = (5 + u16L1);
		u16L1 = 0;
		u16ALPWriteindex = 0;

		u16ALPLength = _GetALPLength(pu8BBStart + u16BBReadindex);
		DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
		__func__, __LINE__, u16ALPLength));
		if (u16ALPLength > u16L2) {
			*pu32ALPSize = 0;
			u16L2Remained[u8PLP_ID] = u16L2;
			memcpy(&(pALPQueue[u8PLP_ID][0]),
			pu8BBStart + u16BBReadindex,  u16L2);
			return TRUE;
		}

		memcpy(pu8ALPStart, pu8BBStart + u16BBReadindex, u16ALPLength);
		u16ALPWriteindex += u16ALPLength;
		u16BBReadindex += u16ALPLength;
		*pu32ALPSize = u16ALPLength;
	} else {
		if (u16L1 == 0x1fff) {
			memcpy(pu8ALPStart, &(pALPQueue[u8PLP_ID][0]), u16L2Remained[u8PLP_ID]);
			u16BBReadindex = 5;
			u16ALPWriteindex = u16L2Remained[u8PLP_ID];
			*pu32ALPSize = u16L2Remained[u8PLP_ID];

			memcpy(pu8ALPStart + u16ALPWriteindex,
			pu8BBStart + u16BBReadindex, u16L2);
			u16ALPWriteindex += u16L2;
			u16BBReadindex += u16L2;
			*pu32ALPSize += u16L2;
			u16ALPLength = _GetALPLength(pu8ALPStart);
			DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
			__func__, __LINE__, u16ALPLength));
			if (u16ALPLength == (u16L2Remained[u8PLP_ID] + u16L2)) {
				u16L2Remained[u8PLP_ID] = 0;
				*pu32ALPSize = u16ALPLength;
			} else if (u16ALPLength > (u16L2Remained[u8PLP_ID] + u16L2)) {
				memcpy(&(pALPQueue[u8PLP_ID][u16L2Remained[u8PLP_ID]]),
				pu8BBStart + u16BBReadindex, u16L2);
				u16L2Remained[u8PLP_ID] += u16L2;
				*pu32ALPSize = 0;
			} else {
				PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);
				u16L2Remained[u8PLP_ID] = 0;
				*pu32ALPSize = 0;
			}
			return TRUE;
		}

		memcpy(pu8ALPStart, &(pALPQueue[u8PLP_ID][0]), u16L2Remained[u8PLP_ID]);
		u16BBReadindex = 5;
		u16ALPWriteindex = u16L2Remained[u8PLP_ID];
		*pu32ALPSize = u16L2Remained[u8PLP_ID];

		memcpy(pu8ALPStart + u16ALPWriteindex, pu8BBStart + u16BBReadindex, u16L1);
		u16ALPWriteindex += u16L1;
		u16BBReadindex += u16L1;
		*pu32ALPSize += u16L1;
		u16ALPLength = _GetALPLength(pu8ALPStart);
		DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
		__func__, __LINE__, u16ALPLength));
		if (u16ALPLength != (u16L2Remained[u8PLP_ID] + u16L1)) {
			PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);

			u16L2 -= u16L1;
			u16BBReadindex = (5 + u16L1);
			u16L1 = 0;
			u16ALPWriteindex = 0;

			u16ALPLength = _GetALPLength(pu8BBStart + u16BBReadindex);
			DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
			__func__, __LINE__, u16ALPLength));
			if (u16ALPLength > u16L2) {
				*pu32ALPSize = 0;
				u16L2Remained[u8PLP_ID] = u16L2;
				memcpy(&(pALPQueue[u8PLP_ID][0]),
				pu8BBStart + u16BBReadindex, u16L2Remained[u8PLP_ID]);
				return TRUE;
			}

			memcpy(pu8ALPStart, pu8BBStart + u16BBReadindex, u16ALPLength);
			u16ALPWriteindex += u16ALPLength;
			u16BBReadindex += u16ALPLength;
			*pu32ALPSize = u16ALPLength;
		}
	}

	do {
		u16ALPLength = _GetALPLength(pu8BBStart + u16BBReadindex);

		if ((u32BBSize - u16BBReadindex < u16ALPLength)
			|| (u32BBSize - u16BBReadindex < 3)) {
			u16L2Remained[u8PLP_ID] = u32BBSize - u16BBReadindex;
			memcpy(&(pALPQueue[u8PLP_ID][0]),
			pu8BBStart + u16BBReadindex,  u16L2Remained[u8PLP_ID]);
			break;
		}

		memcpy(pu8ALPStart + u16ALPWriteindex, pu8BBStart + u16BBReadindex,  u16ALPLength);
		u16ALPWriteindex += u16ALPLength;
		u16BBReadindex += u16ALPLength;
		*pu32ALPSize += u16ALPLength;

		DMD_DBG(PRINT(
		"[%s][%d]u16ALPWriteindex = 0x%x, u16BBReadindex = 0x%x, *pu32ALPSize = 0x%x\n",
		__func__, __LINE__, u16ALPWriteindex, u16BBReadindex, *pu32ALPSize));

	} while (1);
	DMD_DBG(PRINT("[%s][%d]L1 = 0x%x, L2 = 0x%x, u16L2Remained = 0x%x\n",
	__func__, __LINE__, u16L1, u16L2, u16L2Remained[u8PLP_ID]));

	return  TRUE;
}
#else
// L mode
u8 mdrv_atsc3_bb2alp_parser(u8 *pu8BBStart, u32 u32BBSize,
u8 *pu8ALPStart, u32 *pu32ALPSize)
{
	u8  bMode = 0;
	u8  i = 0;
	u8  u8PLP_ID = 0;
	u16 u16ALPLength = 0;
	u16 u16L1 = 0;
	u16 u16L2 = 0;
	u16 u16BBReadindex = 0;
	u16 u16ALPWriteindex = 0;
	u16 u16Pointer = 0;
	u16 u16FieldLength = 0;
	static u16 u16Length;
	static u8 u8CC;
	static u8 u8PTPCnt;

	if (pu8BBStart == NULL || pu8ALPStart == NULL)
		return FALSE;

	if (bBBStarted == 0)
		bBBStarted = 1;
	else
		goto L_BBStarted;

	if ((pu8BBStart[0] != 0x5A) || (pu8BBStart[1] != 0x5A) ||
		(pu8BBStart[2] != 0x5A) || (pu8BBStart[3] != 0x5A)) {
		DMD_DBG(PRINT("[%s][%d] Incorrect starter!!\n", __func__, __LINE__));
		_BB2ALP_Parser_Reset();
		return FALSE;
	}
	u16Length = (u16)((pu8BBStart[5] << 8)) | pu8BBStart[6];

L_BBStarted:
	memcpy(&(pBBtemp[u16BBtempWriteindex]), pu8BBStart, 188);
	u16BBtempWriteindex += 188;

	if (u16BBtempWriteindex < u16Length + 17) {
		if (u16BBtempWriteindex == 188) {
			if (++u8PTPCnt >= 10) {
				u8PTPCnt = 0;
				pu8ALPStart[0] = 0x08;
				pu8ALPStart[1] = 0x00;
				pu8ALPStart[2] = 0x01;
				pu8ALPStart[3] = 0x00;
				pu8ALPStart[4] = 0x17;

				pu8ALPStart[5] = (pu8BBStart[8] & 0x03) << 6;
				pu8ALPStart[5] |= ((pu8BBStart[9] & 0xFC) >> 2);
				pu8ALPStart[6] = (pu8BBStart[9] & 0x03) << 6;
				pu8ALPStart[6] |= ((pu8BBStart[10] & 0xFC) >> 2);
				pu8ALPStart[7] = (pu8BBStart[10] & 0x03) << 6;
				pu8ALPStart[7] |= ((pu8BBStart[11] & 0xFC) >> 2);
				pu8ALPStart[8] = (pu8BBStart[11] & 0x03) << 6;
				pu8ALPStart[8] |= ((pu8BBStart[12] & 0xFC) >> 2);
				pu8ALPStart[9] = (pu8BBStart[12] & 0x03) << 6;
				pu8ALPStart[9] |= ((pu8BBStart[13] & 0xFC) >> 2);
				pu8ALPStart[10] = (pu8BBStart[13] & 0x03) << 6;
				pu8ALPStart[10] |= ((pu8BBStart[14] & 0xFC) >> 2);
				pu8ALPStart[11] = (pu8BBStart[14] & 0x03) << 6;
				pu8ALPStart[11] |= ((pu8BBStart[15] & 0xFC) >> 2);
				pu8ALPStart[12] = (pu8BBStart[15] & 0x03) << 6;
				pu8ALPStart[12] |= ((pu8BBStart[16] & 0xFC) >> 2);

				pu8ALPStart[13] = 0x00;
				pu8ALPStart[14] = 0x00;
				pu8ALPStart[15] = 0x00;
				pu8ALPStart[16] = 0x00;
				pu8ALPStart[17] = 0x00;
				pu8ALPStart[18] = 0x00;
				pu8ALPStart[19] = 0x00;
				pu8ALPStart[20] = 0x00;
				pu8ALPStart[21] = 0x00;
				pu8ALPStart[22] = 0x00;
				pu8ALPStart[23] = 0x00;
				pu8ALPStart[24] = 0x00;
				pu8ALPStart[25] = 0x00;
				pu8ALPStart[26] = 0x00;
				pu8ALPStart[27] = 0x00;
				pu8ALPStart[28] = 0x00;
				*pu32ALPSize = 29;
				return TRUE;
			}
		}

		*pu32ALPSize = 0;
		return TRUE;
	}

	u16BBtempWriteindex = 0;

	if ((pBBtemp[0] != 0x5A) || (pBBtemp[1] != 0x5A) ||
		(pBBtemp[2] != 0x5A) || (pBBtemp[3] != 0x5A)) {
		DMD_DBG(PRINT("[%s][%d] Incorrect starter!!\n", __func__, __LINE__));
		_BB2ALP_Parser_Reset();
		return FALSE;
	}

	if (u8Known_PLP_ID_63[0] != 0xFF) {
		if (++u8CC != pBBtemp[7]) {
			DMD_DBG(PRINT("[%s][%d] Incorrect CC!!\n", __func__, __LINE__));
			_BB2ALP_Parser_Reset();
			return FALSE;
		}
	} else
		u8CC = pBBtemp[7];

	for (i = 0; i < 4; i++) {
		if (u8Known_PLP_ID_63[i] != 0xFF) {
			if (((pBBtemp[4] & 0x1F) == u8Known_PLP_ID_63[i])) {
				u8PLP_ID = i;
				break;
			}
		} else {
			u8PLP_ID = i;
			u8Known_PLP_ID_63[i] = pBBtemp[4] & 0x1F;
			break;
		}
	}

	if (i == 4) {
		DMD_DBG(PRINT("[%s][%d] No PLP ID Matched!!\n", __func__, __LINE__));
		_BB2ALP_Parser_Reset();
		return FALSE;
	}
	DMD_DBG(PRINT("[%s][%d] PLP ID = %d!!\n",
	__func__, __LINE__, u8Known_PLP_ID_63[i]));

	u16Length = (u16)((pBBtemp[5] << 8)) | pBBtemp[6];

	bMode = ((pBBtemp[17] & 0x80) == 0x80) ? 1 : 0;

	if (bMode == 1) {
		u16FieldLength = 2;
		u16Pointer = (((u16)(pBBtemp[18] & 0xFC)) << 5) | (pBBtemp[17] & 0x7F);
	} else {
		u16FieldLength = 1;
		u16Pointer = pBBtemp[17] & 0x7F;
	}

	u16L1 = u16Pointer;
	u16L2 = u16Length - u16FieldLength;

	DMD_DBG(PRINT("[%s][%d]L1 = 0x%x, L2 = 0x%x\n",
	__func__, __LINE__, u16L1, u16L2));

	if (((u16L1 != 0x1fff) && (u16L1 > u16L2)) || (u16L2 == 0)) {
		_BB2ALP_Parser_Reset();

		PRINT("[%s][%d] 0x%2x  0x%2x  0x%2x  0x%2x  0x%2x  0x%2x\n",
		__func__, __LINE__, pBBtemp[1], pBBtemp[2],
		pBBtemp[3], pBBtemp[4], pBBtemp[5], pBBtemp[6]);
		PRINT("[%s][%d] 0x%2x  0x%2x\n",
		__func__, __LINE__, pBBtemp[17], pBBtemp[18]);
		return FALSE;
	}

	if (!bPLPEverStart[u8PLP_ID] || (u16L1 == 0)) {
		if (u16L1 == 0x1fff) {
			*pu32ALPSize = 0;
			return TRUE;
		}

		if ((u16L1 == 0) && (u16L2Remained[u8PLP_ID] != 0))
			PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);

		bPLPEverStart[u8PLP_ID] = 1;
		u16L2 -= u16L1;
		u16BBReadindex = (17 + u16FieldLength + u16L1);
		u16L1 = 0;
		u16ALPWriteindex = 0;

		u16ALPLength = _GetALPLength(pBBtemp + u16BBReadindex);
		DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n", __func__, __LINE__, u16ALPLength));
		if (u16ALPLength > u16L2) {
			*pu32ALPSize = 0;
			u16L2Remained[u8PLP_ID] = u16L2;
			memcpy(&(pALPQueue[u8PLP_ID][0]), pBBtemp + u16BBReadindex,  u16L2);
			return TRUE;
		}

		memcpy(pu8ALPStart, pBBtemp + u16BBReadindex, u16ALPLength);
		u16ALPWriteindex += u16ALPLength;
		u16BBReadindex += u16ALPLength;
		*pu32ALPSize = u16ALPLength;
	} else {
		if (u16L1 == 0x1fff) {
			memcpy(pu8ALPStart, &(pALPQueue[u8PLP_ID][0]), u16L2Remained[u8PLP_ID]);
			u16BBReadindex = 17 + u16FieldLength;
			u16ALPWriteindex = u16L2Remained[u8PLP_ID];
			*pu32ALPSize = u16L2Remained[u8PLP_ID];

			memcpy(pu8ALPStart + u16ALPWriteindex, pBBtemp + u16BBReadindex, u16L2);
			u16ALPWriteindex += u16L2;
			u16BBReadindex += u16L2;
			*pu32ALPSize += u16L2;
			u16ALPLength = _GetALPLength(pu8ALPStart);
			DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
			__func__, __LINE__, u16ALPLength));

			if (u16ALPLength == (u16L2Remained[u8PLP_ID] + u16L2)) {
				u16L2Remained[u8PLP_ID] = 0;
				*pu32ALPSize = u16ALPLength;
			} else if (u16ALPLength > (u16L2Remained[u8PLP_ID] + u16L2)) {
				memcpy(&(pALPQueue[u8PLP_ID][u16L2Remained[u8PLP_ID]]),
				pBBtemp + u16BBReadindex, u16L2);
				u16L2Remained[u8PLP_ID] += u16L2;
				*pu32ALPSize = 0;
			} else {
				PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);
				u16L2Remained[u8PLP_ID] = 0;
				*pu32ALPSize = 0;
			}

			return TRUE;
		}

		memcpy(pu8ALPStart, &(pALPQueue[u8PLP_ID][0]), u16L2Remained[u8PLP_ID]);
		u16BBReadindex = 17 + u16FieldLength;
		u16ALPWriteindex = u16L2Remained[u8PLP_ID];
		*pu32ALPSize = u16L2Remained[u8PLP_ID];

		memcpy(pu8ALPStart + u16ALPWriteindex, pBBtemp + u16BBReadindex, u16L1);
		u16ALPWriteindex += u16L1;
		u16BBReadindex += u16L1;
		*pu32ALPSize += u16L1;
		u16ALPLength = _GetALPLength(pu8ALPStart);
		DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n", __func__, __LINE__, u16ALPLength));
		if (u16ALPLength != (u16L2Remained[u8PLP_ID] + u16L1)) {
			PRINT("[%s][%d] Some BBP missed!!\n", __func__, __LINE__);

			u16L2 -= u16L1;
			u16BBReadindex = (5 + u16L1);
			u16L1 = 0;
			u16ALPWriteindex = 0;

			u16ALPLength = _GetALPLength(pBBtemp + u16BBReadindex);
			DMD_DBG(PRINT("[%s][%d]u16ALPLength = 0x%x\n",
			__func__, __LINE__, u16ALPLength));
			if (u16ALPLength > u16L2) {
				*pu32ALPSize = 0;
				u16L2Remained[u8PLP_ID] = u16L2;
				memcpy(&(pALPQueue[u8PLP_ID][0]),
				pBBtemp + u16BBReadindex, u16L2Remained[u8PLP_ID]);
				return TRUE;
			}

			memcpy(pu8ALPStart, pBBtemp + u16BBReadindex, u16ALPLength);
			u16ALPWriteindex += u16ALPLength;
			u16BBReadindex += u16ALPLength;
			*pu32ALPSize = u16ALPLength;
		}
	}

	do {
		u16ALPLength = _GetALPLength(pBBtemp + u16BBReadindex);

		if (((u16Length + 17 - u16BBReadindex) < u16ALPLength) ||
			((u16Length + 17 - u16BBReadindex) < 3)) {
			u16L2Remained[u8PLP_ID] = u16Length + 17 - u16BBReadindex;
			memcpy(&(pALPQueue[u8PLP_ID][0]),
			pBBtemp + u16BBReadindex,  u16L2Remained[u8PLP_ID]);
			break;
		}

		memcpy(pu8ALPStart + u16ALPWriteindex, pBBtemp + u16BBReadindex,  u16ALPLength);
		u16ALPWriteindex += u16ALPLength;
		u16BBReadindex += u16ALPLength;
		*pu32ALPSize += u16ALPLength;
		DMD_DBG(PRINT(
		"[%s][%d]u16ALPWriteindex = 0x%x, u16BBReadindex = 0x%x, *pu32ALPSize = 0x%x\n",
		__func__, __LINE__, u16ALPWriteindex, u16BBReadindex, *pu32ALPSize));

	} while (1);
	DMD_DBG(PRINT("[%s][%d]L1 = 0x%x, L2 = 0x%x, u16L2Remained = 0x%x\n",
	__func__, __LINE__, u16L1, u16L2, u16L2Remained[u8PLP_ID]));

	return  TRUE;
}
#endif
EXPORT_SYMBOL(mdrv_atsc3_bb2alp_parser);