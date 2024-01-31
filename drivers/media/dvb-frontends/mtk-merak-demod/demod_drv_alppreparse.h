/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DEMOD_DRV_ALPPREPARSER_H_
#define _DEMOD_DRV_ALPPREPARSER_H_
//--------------------------------------------------
//	Driver Compiler	Options
//--------------------------------------------------
#ifndef	DVB_STI	// VT: Temp	define for DVB_Frontend
	#define	DVB_STI	 1
#endif

#if DVB_STI || defined UTPA2
#define	DMD_ATSC_UTOPIA_EN					1
#define	DMD_ATSC_UTOPIA2_EN					0
#else
#define	DMD_ATSC_UTOPIA_EN					0
#define	DMD_ATSC_UTOPIA2_EN					1
#endif

#if	DVB_STI	|| defined MTK_PROJECT
#define	DMD_ATSC_STR_EN						1
#define	DMD_ATSC_MULTI_THREAD_SAFE			1
#define	DMD_ATSC_MULTI_DMD_EN				1
#define	DMD_ATSC_3PARTY_EN					1
#define	DMD_ATSC_3PARTY_IN_KERNEL_EN		1
#define	DMD_ATSC_3PARTY_MSPI_EN				1
#else
#define	DMD_ATSC_STR_EN						1
#define	DMD_ATSC_MULTI_THREAD_SAFE			0
#define	DMD_ATSC_MULTI_DMD_EN				1
#define	DMD_ATSC_3PARTY_EN					0
#define	DMD_ATSC_3PARTY_IN_KERNEL_EN		0
#define	DMD_ATSC_3PARTY_MSPI_EN				0
#endif

#if	DMD_ATSC_3PARTY_EN
#undef DMD_ATSC_UTOPIA_EN
#undef DMD_ATSC_UTOPIA2_EN

#define	DMD_ATSC_UTOPIA_EN					0
#define	DMD_ATSC_UTOPIA2_EN					0

#if	DMD_ATSC_3PARTY_IN_KERNEL_EN
	#ifndef UTPA2
	#define	UTPA2
	#endif
	#ifndef MSOS_TYPE_LINUX_KERNEL
	#define	MSOS_TYPE_LINUX_KERNEL
	#endif
#endif //	#if	DMD_ATSC_3PARTY_IN_KERNEL_EN
#endif //	#if	DMD_ATSC_3PARTY_EN

//------------------------------------------------------
//	Include	Files
//------------------------------------------------------

#if	defined	MTK_PROJECT
#include "PD_Def.h"
#include "x_hal_arm.h"
#elif	DVB_STI
//ckang, #include "mtkm6_frontend.h"
#include <asm/io.h>
#include <linux/delay.h>
#include "demod_common.h"
#elif	DMD_ATSC_3PARTY_EN
// Please add header files needed by 3 perty platform PRINT and MEMCPY define
#endif

//#include "MsTypes.h"
#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
#ifndef	MSIF_TAG
#include "MsVersion.h"
#endif
#include "MsCommon.h"
#endif
#if	DMD_ATSC_UTOPIA2_EN ||	(DMD_ATSC_STR_EN &&	DMD_ATSC_UTOPIA_EN)
#include "utopia.h"
#endif
#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
#include "drvMSPI.h"
#endif
//----------------------------------------------------
//	Macro	and	Define
//---------------------------------------------------- 

#ifndef	DLL_PACKED
#define	DLL_PACKED
#endif

#ifndef	DLL_PUBLIC
#define	DLL_PUBLIC
#endif

#ifdef MSOS_TYPE_LINUX_KERNEL
#define	PRINT	 printk
#define	MEMCPY	 memcpy
#else	// #ifdef	MSOS_TYPE_LINUX_KERNEL
#define	PRINT	 printf
#define	MEMCPY	 memcpy
#endif //	#ifdef MSOS_TYPE_LINUX_KERNEL

//--------------------------------------------------------------------
//	Function and Variable
//--------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif

	DLL_PUBLIC extern u8 mdrv_atsc3_bb2alp_parser(u8* pu8BBStart,
	u32 u32BBSize, u8* pu8ALPStart, u32 *pu32ALPSize);
	DLL_PUBLIC extern void mdrv_atsc3_bb2alp_parser_close(void);
	DLL_PUBLIC extern u8 mdrv_atsc3_bb2alp_parser_init(void);

#ifdef __cplusplus
}
#endif

#endif //	_DRV_ATSC_H_