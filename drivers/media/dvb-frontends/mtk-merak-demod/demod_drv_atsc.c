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
#include "demod_drv_atsc.h"
#if ATSC3_ALP_PREPARSE_EN
#include "demod_drv_alppreparse.h"
#endif
#if DMD_ATSC_UTOPIA2_EN
#include "drvDMD_ATSC_v2.h"
#endif

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

//typedef u8(*IOCTL)
//(struct ATSC_IOCTL_DATA, enum DMD_ATSC_HAL_COMMAND, void*);
//static IOCTL _null_ioctl_fp_atsc;

static u8 (*_null_ioctl_fp_atsc)(struct ATSC_IOCTL_DATA*,
enum DMD_ATSC_HAL_COMMAND, void*);

static u16 err_accum;
static u32 request_freq;

#ifdef MSOS_TYPE_LINUX_KERNEL
#if !DMD_ATSC_UTOPIA_EN && DMD_ATSC_UTOPIA2_EN
//typedef u8(*SWI2C)(u8 busnum, u8 slaveid, u8 addrcnt,
//u8 * pu8addr, u16 size, u8 * pbuf);
//typedef u8(*HWI2C)(u8 u8Port, u8 slaveidiiC, u8 addrsizeiic,
//u8 * paddriic, u32 bufsizeiic, u8 * pbufiiC);

//static SWI2C _NULL_SWI2C_FP_ATSC;
//static HWI2C _null_hwi2c_fp_atsc;
static u8 (*_null_swi2c_fp_atsc)(u8 busnum, u8 slaveid,
u8 addrcnt, u8 *pu8addr, u16 size, u8 *pbuf);

static u8 (*_null_hwi2c_fp_atsc)(u8 busnum, u8 slaveid,
u8 addrcnt, u8 *pu8addr, u16 size, u8 *pbuf);

//#else
//static SWI2C NULL;
//static HWI2C NULL;
#endif
#endif

static u8 _default_ioctl_cmd(
struct ATSC_IOCTL_DATA *pdata, enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs)
{
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	#ifdef REMOVE_HAL_INTERN_ATSC
	PRINT("LINK ERROR: REMOVE_HAL_INTERN_ATSC\n");
	#else
	PRINT("LINK ERROR: Please link ext demod library\n");
	#endif
	#else
	PRINT("LINK	ERROR: Please link int or ext demod library\n");
	#endif

	return TRUE;
}

//#if defined(MSOS_TYPE_LINUX_KERNEL) && \
//defined(CONFIG_CC_IS_CLANG) && defined(CONFIG_MSTAR_CHIP)
#if !DVB_STI
static u8 (*hal_intern_atsc_ioctl_cmd)(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs);
static u8 (*hal_extern_atsc_ioctl_cmd)(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs);

static u8 (*mdrv_sw_iic_writebytes)(u8 busnum, u8 slaveid,
u8 addrcnt, u8 *pu8addr, u16 size, u8 *pbuf);

static u8 (*mdrv_sw_iic_readbytes)(u8 busnum, u8 slaveid,
u8 ucsubddr, u8 *paddr, u16 ucbuflen, u8 *pbuf);

static u8 (*mdrv_hw_iic_writebytes)(u8 u8Port, u8 slaveidiiC,
u8 addrsizeiic, u8 *paddriic, u32 bufsizeiic, u8 *pbufiiC);

static u8 (*mdrv_hw_iic_readbytes)(u8 u8Port, u8 slaveidiiC,
u8 addrsizeiic, u8 *paddriic, u32 bufsizeiic, u8 *pbufiiC);

#else
extern u8 hal_intern_atsc_ioctl_cmd(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs);// __attribute__((weak));

extern u8 hal_extern_atsc_ioctl_cmd(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs); //__attribute__((weak));

#include "../austin_demod/austin-demod.h"
extern struct austin_demod_dev *p_austin_dev;
#ifndef MSOS_TYPE_LINUX_KERNEL
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
u8 hal_extern_atsc_ioctl_cmd_0(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs) __attribute__((weak));
u8 hal_extern_atsc_ioctl_cmd_1(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs) __attribute__((weak));

u8 hal_extern_atsc_ioctl_cmd_2(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs) __attribute__((weak));

u8 hal_extern_atsc_ioctl_cmd_3(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pargs) __attribute__((weak));

typedef u8 (*ext_ioctl)(struct ATSC_IOCTL_DATA*,
enum DMD_ATSC_HAL_COMMAND, void*);

u8 EXT_ATSC_CHIP[4] = { 0x77, 0xB5, 0xC7, 0x00};
ext_ioctl EXT_ATSC_IOCTL[4] = {
hal_extern_atsc_ioctl_cmd_0, hal_extern_atsc_ioctl_cmd_1,
hal_extern_atsc_ioctl_cmd_2, hal_extern_atsc_ioctl_cmd_3};
#endif
#else	// #ifndef MSOS_TYPE_LINUX_KERNEL
#if !DMD_ATSC_UTOPIA_EN && DMD_ATSC_UTOPIA2_EN
u8 mdrv_sw_iic_writebytes(u8 busnum, u8 slaveid,
u8 addrcnt, u8 *pu8addr, u16 size,
u8 *pbuf) __attribute__((weak));

u8 mdrv_sw_iic_readbytes(u8 busnum, u8 slaveid,
u8 ucsubddr, u8 *paddr, u16 ucbuflen,
u8 *pbuf) __attribute__((weak));

u8 mdrv_hw_iic_writebytes(u8 u8Port, u8 slaveidiiC,
u8 addrsizeiic, u8 *paddriic, u32 bufsizeiic,
u8 *pbufiiC) __attribute__((weak));

u8 mdrv_hw_iic_readbytes(u8 u8Port, u8 slaveidiiC,
u8 addrsizeiic, u8 *paddriic, u32 bufsizeiic,
u8 *pbufiiC) __attribute__((weak));
#endif
#endif // #ifndef MSOS_TYPE_LINUX_KERNEL

#endif

//------------------------------------------------------
//	Local	Defines
//------------------------------------------------------

#if	DMD_ATSC_UTOPIA_EN && !DMD_ATSC_UTOPIA2_EN
 #define DMD_LOCK()		 \
	do {
		MS_ASSERT(MsOS_In_Interrupt() == FALSE);
		if (_s32DMD_ATSC_Mutex == -1)
			return FALSE;
		if (dmd_atsc_dbglevel == DMD_ATSC_DBGLV_DEBUG)
			PRINT("%s lock mutex\n", __func__);
		MsOS_ObtainMutex(_s32DMD_ATSC_Mutex, MSOS_WAIT_FOREVER);
		}	while (0)

 #define DMD_UNLOCK() \
	do	{	\
		MsOS_ReleaseMutex(_s32DMD_ATSC_Mutex);\
		if (dmd_atsc_dbglevel ==	DMD_ATSC_DBGLV_DEBUG)
			PRINT("%s unlock mutex\n", __func__); } while (0)
#elif (!DMD_ATSC_UTOPIA_EN \
&& (!DMD_ATSC_UTOPIA2_EN) && (DMD_ATSC_MULTI_THREAD_SAFE))
 #define DMD_LOCK() piodata->sys.lockdmd(TRUE)
 #define DMD_UNLOCK() piodata->sys.lockdmd(FALSE)
#else
 #define DMD_LOCK()
 #define DMD_UNLOCK()
#endif

#ifdef MS_DEBUG
#define	DMD_DBG(x)			(x)
#else
#define	DMD_DBG(x)			//(x)
#endif

#ifndef	UTOPIA_STATUS_SUCCESS
#define	UTOPIA_STATUS_SUCCESS				0x00000000
#endif
#ifndef	UTOPIA_STATUS_FAIL
#define	UTOPIA_STATUS_FAIL					0x00000001
#endif

#define SNR_COREFFICIENT 400
#define BER_ERR_WINDOW_COE 191488
//------------------------------------------------------
//	Local	Structurs
//------------------------------------------------------



//------------------------------------------------------
//	Global Variables
//------------------------------------------------------

#if	DMD_ATSC_UTOPIA2_EN
struct DMD_ATSC_RESDATA *psdmd_atsc_resdata;
u8 *pDMD_ATSC_EXT_CHIP_CH_NUM;
u8 *pDMD_ATSC_EXT_CHIP_CH_ID;
u8 *pDMD_ATSC_EXT_CHIP_I2C_CH;

struct ATSC_IOCTL_DATA dmd_atsc_iodata = { 0, 0, 0, 0, 0, { 0 } };
#endif

//------------------------------------------------------
//	Local	Variables
//------------------------------------------------------

#if !DMD_ATSC_UTOPIA2_EN
struct DMD_ATSC_RESDATA sDMD_ATSC_ResData[DMD_ATSC_MAX_DEMOD_NUM] = { { {0},
{0}, {0} } };
static u8 DMD_ATSC_EXT_CHIP_CH_NUM[DMD_ATSC_MAX_EXT_CHIP_NUM] = { 0 };
static u8 DMD_ATSC_EXT_CHIP_CH_ID[DMD_ATSC_MAX_DEMOD_NUM] = { 0 };
static u8 DMD_ATSC_EXT_CHIP_I2C_CH[DMD_ATSC_MAX_EXT_CHIP_NUM] = { 0 };

struct DMD_ATSC_RESDATA	*psdmd_atsc_resdata	=	sDMD_ATSC_ResData;
static u8 *pDMD_ATSC_EXT_CHIP_CH_NUM = DMD_ATSC_EXT_CHIP_CH_NUM;
static u8 *pDMD_ATSC_EXT_CHIP_CH_ID = DMD_ATSC_EXT_CHIP_CH_ID;
static u8 *pDMD_ATSC_EXT_CHIP_I2C_CH = DMD_ATSC_EXT_CHIP_I2C_CH;

struct ATSC_IOCTL_DATA dmd_atsc_iodata = { 0, 0, 0, 0, 0, { 0 } };
#endif

struct ATSC_IOCTL_DATA *piodata	=	&dmd_atsc_iodata;

#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
//static MSIF_Version	_drv_dmd_atsc_version;
#endif

#if	DMD_ATSC_UTOPIA_EN && !DMD_ATSC_UTOPIA2_EN
static s32	_s32DMD_ATSC_Mutex = -1;
#elif	defined	MTK_PROJECT
static HANDLE_T	_s32DMD_ATSC_Mutex = -1;
#elif	DVB_STI
static s32	_s32DMD_ATSC_Mutex = -1;
#endif

#ifdef UTPA2

#if	DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN

static void *_ppatscinstant;

static u32 _atscopen;

#endif

#endif

enum DMD_ATSC_DBGLV dmd_atsc_dbglevel = DMD_ATSC_DBGLV_NONE;

//------------------------------------------------
//	Debug	Functions
//------------------------------------------------


//------------------------------------------------
//	Local	Functions
//------------------------------------------------
static enum DMD_ATSC_LOCK_STATUS _ATSC_CheckLock(void)
{
	u8 (*ioctl)(struct ATSC_IOCTL_DATA *pdata, enum DMD_ATSC_HAL_COMMAND eCmd, void *pPara);
	u16   timeout;
	enum DMD_ATSC_DEMOD_TYPE type;
	struct DMD_ATSC_RESDATA  *pres  = piodata->pres;
	struct DMD_ATSC_INITDATA *pInit = &(pres->sdmd_atsc_initdata);
	struct DMD_ATSC_INFO     *pinfo = &(pres->sdmd_atsc_info);

	ioctl = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd;

	ioctl(piodata, DMD_ATSC_HAL_CMD_CheckType, &type);
	if (ioctl(piodata, DMD_ATSC_HAL_CMD_FEC_Lock, NULL)) {
		pinfo->atsclockstatus |= DMD_ATSC_LOCK_FEC_LOCK;
		#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
		pinfo->atscfeclocktime = MsOS_GetSystemTime();
		#else
		pinfo->atscfeclocktime = piodata->sys.getsystemtimems();
		#endif
		return DMD_ATSC_LOCK;
	} else {
		//FEC UNLOCK
		if (type == DMD_ATSC_DEMOD_ATSC_VSB)
			timeout = 100;
		else
			timeout = 500;

		#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
		if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_FEC_LOCK) &&
			((MsOS_GetSystemTime()-pinfo->atscfeclocktime) < timeout))
		#else
		if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_FEC_LOCK) &&
			((piodata->sys.getsystemtimems()-pinfo->atscfeclocktime) < timeout))
		#endif
		{
			return DMD_ATSC_LOCK;
		} else//Step 0 or 5
			pinfo->atsclockstatus &= (~DMD_ATSC_LOCK_VSB_FEC_LOCK);

		//Step 3,4
		if (pinfo->atsclockstatus & DMD_ATSC_LOCK_PRE_LOCK) {
			if (pinfo->atsclockstatus & DMD_ATSC_LOCK_FSYNC_LOCK) {
			//Step 4
				if (type == DMD_ATSC_DEMOD_ATSC_VSB)
					timeout = pInit->vsbfeclockchecktime;
				else
					timeout = pInit->u16OFDMFECLockCheckTime;

				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				if ((MsOS_GetSystemTime()-pinfo->atscscantimestart) < timeout)
				#else
				if ((piodata->sys.getsystemtimems() -
					pinfo->atscscantimestart) < timeout)
				#endif
				{
					if (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
						return DMD_ATSC_CHECKING;
				}
			} else {
				//Step 3
				if (ioctl(piodata, DMD_ATSC_HAL_CMD_FSync_Lock, NULL)) {
					pinfo->atsclockstatus |= DMD_ATSC_LOCK_FSYNC_LOCK;
					#ifdef MS_DEBUG
					if (_u8DMD_ATSC_DbgLevel >= DMD_ATSC_DBGLV_DEBUG)
						PRINT("DMD_ATSC_LOCK_FSYNC_LOCK\n");

					#endif
					#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
					pinfo->atscscantimestart = MsOS_GetSystemTime();
					#else
					pinfo->atscscantimestart = piodata->sys.getsystemtimems();
					#endif
					return DMD_ATSC_CHECKING;
				} else {
					if (type == DMD_ATSC_DEMOD_ATSC_VSB)
						timeout = pInit->vsbfsynclockchecktime;
					else
						timeout = pInit->u16OFDMFSyncLockCheckTime;

					#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
					if ((MsOS_GetSystemTime()-pinfo->atscscantimestart)
						< timeout)
					#else
					if ((piodata->sys.getsystemtimems() -
						pinfo->atscscantimestart) < timeout)
					#endif
					{
						#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
						if (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
							return DMD_ATSC_CHECKING;
						#else
						if (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
							return DMD_ATSC_CHECKING;
						#endif
					}
				}
			}
		} else {
			//Step 1,2
			if (ioctl(piodata, DMD_ATSC_HAL_CMD_PreLock, NULL)) {
				pinfo->atsclockstatus |= DMD_ATSC_LOCK_PRE_LOCK;
				#ifdef MS_DEBUG
				if (_u8DMD_ATSC_DbgLevel >= DMD_ATSC_DBGLV_DEBUG)
					PRINT("DMD_ATSC_LOCK_PRE_LOCK\n");
				#endif
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				pinfo->atscscantimestart = MsOS_GetSystemTime();
				#else
				pinfo->atscscantimestart = piodata->sys.getsystemtimems();
				#endif
				return DMD_ATSC_CHECKING;
			} else {
				if (type == DMD_ATSC_DEMOD_ATSC_VSB)
					timeout = pInit->vsbprelockchecktime;
				else
					timeout = pInit->u16OFDMPreLockCheckTime;

				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				if (((MsOS_GetSystemTime()-pinfo->atscscantimestart) < timeout)
					&& (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL)))
				#else
				if (((piodata->sys.getsystemtimems()-pinfo->atscscantimestart) <
					timeout) &&
					(ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL)))
				#endif
				{
					return DMD_ATSC_CHECKING;
				}
			}
		}
		return DMD_ATSC_UNLOCK;
	}
}

static enum DMD_ATSC_LOCK_STATUS _vsb_checklock(void)
{
u8 (*ioctl)(struct ATSC_IOCTL_DATA *pdata,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pPara);
u16 timeout;

struct DMD_ATSC_RESDATA *pres = piodata->pres;
struct DMD_ATSC_INITDATA *pInit = &(pres->sdmd_atsc_initdata);
struct DMD_ATSC_INFO *pinfo = &(pres->sdmd_atsc_info);

//ioctl = qqq;
ioctl = (u8 (*)(struct ATSC_IOCTL_DATA *, enum DMD_ATSC_HAL_COMMAND,
void *))(pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd);

if (ioctl(piodata, DMD_ATSC_HAL_CMD_VSB_FEC_LOCK, NULL)) {
	pinfo->atsclockstatus |= DMD_ATSC_LOCK_VSB_FEC_LOCK;
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	pinfo->atscfeclocktime = MsOS_GetSystemTime();
	#else
	pinfo->atscfeclocktime = piodata->sys.getsystemtimems();
	#endif
	return DMD_ATSC_LOCK;
} else {
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_VSB_FEC_LOCK) &&
	((MsOS_GetSystemTime()-pinfo->atscfeclocktime) < 100))
	#else
	if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_VSB_FEC_LOCK) &&
	((piodata->sys.getsystemtimems()-pinfo->atscfeclocktime) < 100))
	#endif
	{
		return DMD_ATSC_LOCK;

	} else {
		pinfo->atsclockstatus &= (~DMD_ATSC_LOCK_VSB_FEC_LOCK);
	}

	if (pinfo->atsclockstatus & DMD_ATSC_LOCK_VSB_PRE_LOCK) {
		if (pinfo->atsclockstatus & DMD_ATSC_LOCK_VSB_FSYNC_LOCK) {
			#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
			if ((MsOS_GetSystemTime()-
			pinfo->atscscantimestart) <
			pInit->vsbfeclockchecktime)
			#else
			if ((piodata->sys.getsystemtimems()-
			pinfo->atscscantimestart) <
			pInit->vsbfeclockchecktime)
			#endif
			{
				if (ioctl(
				piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
					return DMD_ATSC_CHECKING;
			}
		} else {
			if (ioctl(piodata,
			DMD_ATSC_HAL_CMD_VSB_FSYNC_LOCK, NULL)) {
				pinfo->atsclockstatus |=
				DMD_ATSC_LOCK_VSB_FSYNC_LOCK;

				#ifdef MS_DEBUG
				if (dmd_atsc_dbglevel >=
				DMD_ATSC_DBGLV_DEBUG)
					PRINT("DMD_ATSC_LOCK_VSB_FSYNC_LOCK");
				#endif
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				pinfo->atscscantimestart =
				MsOS_GetSystemTime();
				#else
				pinfo->atscscantimestart =
				piodata->sys.getsystemtimems();
				#endif
				return DMD_ATSC_CHECKING;

			} else {
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				if ((MsOS_GetSystemTime()-
				pinfo->atscscantimestart) <
				pInit->vsbfsynclockchecktime)
				#else
				if ((piodata->sys.getsystemtimems()-
				pinfo->atscscantimestart) <
				pInit->vsbfsynclockchecktime)
				#endif
				{
					#if (DMD_ATSC_UTOPIA_EN) || \
					(DMD_ATSC_UTOPIA2_EN)
					if (ioctl(
				piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
						return DMD_ATSC_CHECKING;
					#else
					if (ioctl(
				piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
						return DMD_ATSC_CHECKING;
					#endif
				}
			}
		}
	}	else	{
		if (ioctl(piodata,
		DMD_ATSC_HAL_CMD_VSB_PRELOCK, NULL)) {
			pinfo->atsclockstatus |=
			DMD_ATSC_LOCK_VSB_PRE_LOCK;

			#ifdef MS_DEBUG
			if (dmd_atsc_dbglevel >=
				DMD_ATSC_DBGLV_DEBUG)
				PRINT("DMD_ATSC_LOCK_VSB_PRE_LOCK\n");
			#endif
			#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
			pinfo->atscscantimestart =
			MsOS_GetSystemTime();
			#else
			pinfo->atscscantimestart =
			piodata->sys.getsystemtimems();
			#endif
			return DMD_ATSC_CHECKING;

		} else {
			if (!ioctl(piodata,
			DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
				timeout = pInit->vsbagclockchecktime;
			else {
				timeout = pInit->vsbprelockchecktime;
				pinfo->atsclockstatus |= DMD_ATSC_LOCK_VSB_PRE_LOCK;
			}

			if (pinfo->atsclockstatus & DMD_ATSC_LOCK_VSB_PRE_LOCK)
				timeout = pInit->vsbprelockchecktime;

			#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
			if ((MsOS_GetSystemTime()-
			pinfo->atscscantimestart) < timeout)
			#else
			if ((piodata->sys.getsystemtimems()
			-pinfo->atscscantimestart) < timeout)
			#endif
			{
				return DMD_ATSC_CHECKING;
			}
		}
	}
	return DMD_ATSC_UNLOCK;
}
}

static enum DMD_ATSC_LOCK_STATUS _qam_checklock(void)
{
	u8 (*ioctl)(struct ATSC_IOCTL_DATA *pdata,
	enum DMD_ATSC_HAL_COMMAND eCmd, void *pPara);
	//u16 timeout;

	struct DMD_ATSC_RESDATA *pres = piodata->pres;
	struct DMD_ATSC_INITDATA *pInit = &(pres->sdmd_atsc_initdata);
	struct DMD_ATSC_INFO *pinfo = &(pres->sdmd_atsc_info);

	ioctl	=	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd;

	if (ioctl(piodata, DMD_ATSC_HAL_CMD_QAM_MAIN_LOCK, NULL))	{
		pinfo->atsclockstatus |=	DMD_ATSC_LOCK_QAM_MAIN_LOCK;
		#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
		pinfo->atscfeclocktime	=	MsOS_GetSystemTime();
		#else
		pinfo->atscfeclocktime = piodata->sys.getsystemtimems();
		#endif
		return DMD_ATSC_LOCK;

	} else {
		#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
		if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_QAM_MAIN_LOCK)
		&& ((MsOS_GetSystemTime()-pinfo->atscfeclocktime) < 100))
		#else
		if ((pinfo->atsclockstatus & DMD_ATSC_LOCK_QAM_MAIN_LOCK)
	&& ((piodata->sys.getsystemtimems()-pinfo->atscfeclocktime) < 100))
		#endif
		{
			return DMD_ATSC_LOCK;

		} else
			pinfo->atsclockstatus &=
			(~DMD_ATSC_LOCK_QAM_MAIN_LOCK);

		if (pinfo->atsclockstatus & DMD_ATSC_LOCK_QAM_PRE_LOCK) {
			#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
			if ((MsOS_GetSystemTime()-pinfo->atscscantimestart)
				< pInit->qammainlockchecktime)
			#else
			if ((piodata->sys.getsystemtimems()-
		pinfo->atscscantimestart) < pInit->qammainlockchecktime)
			#endif
			{
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				//if (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK,
				//NULL) || (MsOS_GetSystemTime()-
				//pinfo->atscscantimestart) <
				//pInit->qamagclockchecktime)
					return DMD_ATSC_CHECKING;
				#else
				//if (ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK,
				//NULL) || (piodata->sys.getsystemtimems()-
				//pinfo->atscscantimestart) <
				//pInit->qamagclockchecktime)
					return DMD_ATSC_CHECKING;
				#endif
			}
		} else {
			if (ioctl(piodata,
				DMD_ATSC_HAL_CMD_QAM_PRELOCK, NULL)) {
				pinfo->atsclockstatus |=
				DMD_ATSC_LOCK_QAM_PRE_LOCK;
				#ifdef MS_DEBUG
				if (dmd_atsc_dbglevel >=
					DMD_ATSC_DBGLV_DEBUG)
					PRINT("DMD_ATSC_LOCK_QAM_PRE_LOCK\n");
				#endif
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				pinfo->atscscantimestart =
				MsOS_GetSystemTime();
				#else
				pinfo->atscscantimestart =
				piodata->sys.getsystemtimems();
				#endif
				return DMD_ATSC_CHECKING;

			} else {
				#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
				if (((MsOS_GetSystemTime() -
				pinfo->atscscantimestart) < pInit->qamprelockchecktime)
				&& ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
				#else
				if (((piodata->sys.getsystemtimems()-
				pinfo->atscscantimestart) < pInit->qamprelockchecktime)
				&& ioctl(piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL))
				#endif
				{
					return DMD_ATSC_CHECKING;
				}
			}
		}
		return DMD_ATSC_UNLOCK;
	}
}

#if	defined	MTK_PROJECT

static u32	_getsystemtimems(void)
{
	u32 CurrentTime = 0;

	CurrentTime = x_os_get_sys_tick() * x_os_drv_get_tick_period();

	return CurrentTime;
}

static void	_delayms(u32	ms)
{
	mcDELAY_US(ms*1000);
}

static u8 _createmutex(u8 enable)
{
	if (enable) {
		if (_s32DMD_ATSC_Mutex ==	-1) {
			if (x_sema_create(&_s32DMD_ATSC_Mutex,
			X_SEMA_TYPE_MUTEX, X_SEMA_STATE_UNLOCK) != OSR_OK) {
				PRINT("%s: x_sema_create Fail!\n",
				__func__);
				return FALSE;
			}
		}
	}	else {
		x_sema_delete(_s32DMD_ATSC_Mutex);

		_s32DMD_ATSC_Mutex = -1;
	}

	return TRUE;
}

static void	_lockdmd(u8 enable)
{
	if (enable)
		x_sema_lock(_s32DMD_ATSC_Mutex,	X_SEMA_OPTION_WAIT);
	else
		x_sema_unlock(_s32DMD_ATSC_Mutex);
}

#elif	DVB_STI


static u8 _createmutex(u8 enable)
{
	return TRUE;
}

static void	_lockdmd(u8 enable)
{

}


static u32	_getsystemtimems(void)
{
	struct timespec			ts;

	getrawmonotonic(&ts);
	return ts.tv_sec *	1000 +	ts.tv_nsec/1000000;
}

//-------------------------------------------------
/* Delay for u32Us microseconds */
//-------------------------------------------------
//ckang
static void	_delayms(u32	ms)
{
	mtk_demod_delay_us(ms * 1000UL);
}

#elif	!DMD_ATSC_UTOPIA_EN	&& DMD_ATSC_UTOPIA2_EN

#ifdef MSOS_TYPE_LINUX_KERNEL
static u8 _sw_i2c_readbytes(u16 busnumslaveid,
u8 u8addrcount, u8 *pu8addr, u16 size, u8 *pdata_8)
{
	if (mdrv_sw_iic_writebytes !=	_NULL_SWI2C_FP_ATSC) {
		if (mdrv_sw_iic_writebytes((busnumslaveid>>8)&0x0F,
		busnumslaveid, u8addrcount, pu8addr, size, pdata_8) != 0)
			return FALSE;
		else
			return	TRUE;
	} else {
		PRINT("LINK	ERROR:
		I2c functionis missing and please check it \n");

		return FALSE;
	}
}

static u8 _sw_i2c_readbytes(u16 busnumslaveid,
u8 addrnum, u8 *paddr, u16 size, u8 *pdata_8)
{
	if (mdrv_sw_iic_readbytes	!= _NULL_SWI2C_FP_ATSC) {
		if (mdrv_sw_iic_readbytes((busnumslaveid>>8)&0x0F,
		busnumslaveid, addrnum, paddr, size, pdata_8) != 0)
			return FALSE;
		else
			return	TRUE;
	} else {
		PRINT("LINK	ERROR:
	I2c function is missing and please check it\n");

		return FALSE;
	}
}

static u8 _hw_i2c_writebytes(u16 busnumslaveid,
u8 u8addrcount, u8 *pu8addr, u16 size, u8 *pdata_8)
{
	if (mdrv_hw_iic_writebytes != _null_hwi2c_fp_atsc) {
		if (mdrv_hw_iic_writebytes((busnumslaveid>>12)&0x03,
		busnumslaveid, u8addrcount, pu8addr, size, pdata_8) != 0)
			return FALSE;
		else
			return	TRUE;
	}	else {
		PRINT("LINK	ERROR:
	I2c function is missing and please check it\n");

		return FALSE;
	}
}

static u8 _hw_i2c_readbytes(u16 busnumslaveid,
u8 addrnum, u8 *paddr, u16 size, u8 *pdata_8)
{
	if (mdrv_hw_iic_readbytes != _null_hwi2c_fp_atsc) {
		if (mdrv_hw_iic_readbytes((busnumslaveid>>12)&0x03,
		busnumslaveid, addrnum, paddr, size, pdata_8) != 0)
			return FALSE;
		else
			return	TRUE;
	} else {
		PRINT("LINK	ERROR:
	I2c function is missing and please check it\n");

		return FALSE;
	}
}
#endif

#endif

static void _updateiodata(u8 id, struct DMD_ATSC_RESDATA *pres)
{
	#if !DMD_ATSC_UTOPIA_EN && DMD_ATSC_UTOPIA2_EN
	#ifdef MSOS_TYPE_LINUX_KERNEL
	//u8I2CSlaveBus bit7 0 : old format 1 : new format
	//u8I2CSlaveBus bit6 0 : SW	I2C 1 : HW I2C
	//u8I2CSlaveBus bit4&5 HW I2C port number

	if ((pres->sdmd_atsc_initdata.u8I2CSlaveBus&0x80)	== 0x80) {
		if ((pres->sdmd_atsc_initdata.u8I2CSlaveBus&0x40) ==
			0x40) {
			pres->sdmd_atsc_initdata.I2C_WriteBytes =
			_hw_i2c_writebytes;
			pres->sdmd_atsc_initdata.I2C_ReadBytes =
			_hw_i2c_readbytes;
		}	else {
			pres->sdmd_atsc_initdata.I2C_WriteBytes =
			_sw_i2c_readbytes;
			pres->sdmd_atsc_initdata.I2C_ReadBytes =
			_sw_i2c_readbytes;
		}
	}
	#endif
	#endif //	#if	!DMD_ATSC_UTOPIA_EN	&& DMD_ATSC_UTOPIA2_EN

	#if DVB_STI
	if (pres->sdmd_atsc_initdata.bisextdemod) {
		//Austin_WriteRegister
		pres->sdmd_atsc_initdata.I2C_WriteBytes = p_austin_dev->fpWrite;
		//Austin_ReadRegister
		pres->sdmd_atsc_initdata.I2C_ReadBytes = p_austin_dev->fpRead;
	}
	#endif

	piodata->id	=	id;
	piodata->pres	=	pres;

	piodata->pextchipchnum = pDMD_ATSC_EXT_CHIP_CH_NUM + (id>>4);
	piodata->pextchipchid	 = pDMD_ATSC_EXT_CHIP_CH_ID	 + (id&0x0F);
	piodata->pextchipi2cch = pDMD_ATSC_EXT_CHIP_I2C_CH + (id>>4);
}

//-----------------------------------------------------
/*Global Functions*/
//-----------------------------------------------------

#ifdef UTPA2
u8 _mdrv_dmd_atsc_setdbglevel(enum DMD_ATSC_DBGLV dbglevel)
#else
u8 mdrv_dmd_atsc_setdbglevel(enum DMD_ATSC_DBGLV dbglevel)
#endif
{
	dmd_atsc_dbglevel = dbglevel;

	return TRUE;
}

#ifdef UTPA2
struct DMD_ATSC_INFO *_mdrv_dmd_atsc_getinfo(void)
#else
struct DMD_ATSC_INFO *mdrv_dmd_atsc_getinfo(void)
#endif
{
	psdmd_atsc_resdata->sdmd_atsc_info.version = 0;

	return &(psdmd_atsc_resdata->sdmd_atsc_info);
}

#if	DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN

///////////////////////////////////////////////////
/*		SINGLE DEMOD API		 */
///////////////////////////////////////////////////

u8 mdrv_dmd_atsc_init(struct DMD_ATSC_INITDATA *pdmd_atsc_initdata,
u32 initdatalen)
{
	return mdrv_dmd_atsc_md_init(0,	pdmd_atsc_initdata,	initdatalen);
}

u8 mdrv_dmd_atsc_exit(void)
{
	return mdrv_dmd_atsc_md_exit(0);
}

u32 mdrv_dmd_atsc_getconfig(struct DMD_ATSC_INITDATA *psdmd_atsc_initdata)
{
	return mdrv_dmd_atsc_md_getconfig(0, psdmd_atsc_initdata);
}

u8 mdrv_dmd_atsc_set_digrf_freq(u32	rf_freq)
{
	return mdrv_dmd_atsc_md_set_digrf_freq(0,	rf_freq);
}

u8 mdrv_dmd_atsc_setconfig(enum DMD_ATSC_DEMOD_TYPE etype, u8 benable)
{
	return mdrv_dmd_atsc_md_setconfig(0, etype,	benable);
}

u8 mdrv_dmd_atsc_setreset(void)
{
	return mdrv_dmd_atsc_md_setreset(0);
}

u8 mdrv_dmd_atsc_set_qam_sr(
enum DMD_ATSC_DEMOD_TYPE etype, u16 symbol_rate)
{
	return mdrv_dmd_atsc_md_set_qam_sr(0,	etype, symbol_rate);
}

u8 mdrv_dmd_atsc_setactive(u8 benable)
{
	return mdrv_dmd_atsc_md_setactive(0, benable);
}

#if	DMD_ATSC_STR_EN
u32 mdrv_dmd_atsc_setpowerstate(enum EN_POWER_MODE powerstate)
{
	return mdrv_dmd_atsc_md_setpowerstate(0, powerstate);
}
#endif

enum DMD_ATSC_LOCK_STATUS mdrv_dmd_atsc_getlock(
enum DMD_ATSC_GETLOCK_TYPE etype)
{
	return mdrv_dmd_atsc_md_getlock(0, etype);
}

enum DMD_ATSC_DEMOD_TYPE mdrv_dmd_atsc_getmodulationmode(void)
{
	return mdrv_dmd_atsc_md_getmodulationmode(0);
}

u8 mdrv_dmd_atsc_getsignalstrength(u16 *strength)
{
	return mdrv_dmd_atsc_md_getsignalstrength(0, strength);
}

enum DMD_ATSC_SIGNAL_CONDITION mdrv_dmd_atsc_getsignalquality(void)
{
	return mdrv_dmd_atsc_md_getsignalquality(0);
}

u8 mdrv_dmd_atsc_getsnrpercentage(void)
{
	return mdrv_dmd_atsc_md_getsnrpercentage(0);
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_get_qam_snr(float	*f_snr)
#else
u8 mdrv_dmd_atsc_get_qam_snr(struct DMD_ATSC_SNR_DATA	*f_snr)
#endif
{
	return mdrv_dmd_atsc_md_get_qam_snr(0, f_snr);
}

u8 mdrv_dmd_atsc_read_ucpkt_err(u16	*packeterr)
{
	return mdrv_dmd_atsc_md_read_ucpkt_err(0,	packeterr);
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_getpreviterbiber(float *ber)
#else
u8 mdrv_dmd_atsc_getpreviterbiber(struct DMD_ATSC_BER_DATA *ber)
#endif
{
	return mdrv_dmd_atsc_md_getpreviterbiber(0,	ber);
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_getpostviterbiber(float	*ber)
#else
u8 mdrv_dmd_atsc_getpostviterbiber(struct DMD_ATSC_BER_DATA	*ber)
#endif
{
	return mdrv_dmd_atsc_md_getpostviterbiber(0, ber);
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_readfrequencyoffset(s16 *cfo)
#else
u8 mdrv_dmd_atsc_readfrequencyoffset(DMD_ATSC_CFO_DATA *cfo)
#endif
{
	return mdrv_dmd_atsc_md_readfrequencyoffset(0, cfo);
}

u8 mdrv_dmd_atsc_setserialcontrol(u8 tsconfig_data)
{
	return mdrv_dmd_atsc_md_setserialcontrol(0,	tsconfig_data);
}

u8 mdrv_dmd_atsc_iic_bypass_mode(u8 benable)
{
	return mdrv_dmd_atsc_md_iic_bypass_mode(0, benable);
}

u8 mdrv_dmd_atsc_switch_sspi_gpio(u8 benable)
{
	return mdrv_dmd_atsc_md_switch_sspi_gpio(0,	benable);
}

u8 mdrv_dmd_atsc_gpio_get_level(u8 pin_8,	u8 *blevel)
{
	return mdrv_dmd_atsc_md_gpio_get_level(0,	pin_8, blevel);
}

u8 mdrv_dmd_atsc_gpio_set_level(u8 pin_8,	u8 blevel)
{
	return mdrv_dmd_atsc_md_gpio_set_level(0,	pin_8, blevel);
}

u8 mdrv_dmd_atsc_gpio_out_enable(u8 pin_8, u8 benableout)
{
	return mdrv_dmd_atsc_md_gpio_out_enable(0, pin_8,	benableout);
}

u8 mdrv_dmd_atsc_doiqswap(u8 bis_qpad)
{
	return mdrv_dmd_atsc_md_doiqswap(0,	bis_qpad);
}

u8 mdrv_dmd_atsc_getreg(u16	addr_16, u8 *pdata_8)
{
	return mdrv_dmd_atsc_md_getreg(0,	addr_16, pdata_8);
}

u8 mdrv_dmd_atsc_setreg(u16	addr_16, u8 data_8)
{
	return mdrv_dmd_atsc_md_setreg(0,	addr_16, data_8);
}

////////////////////////////////////////////
///			 BACKWARD	COMPATIBLE API	 ///
////////////////////////////////////////////

#ifndef	UTPA2
u8 mdrv_dmd_atsc_sel_dmd(enum eDMD_SEL eDMD_NUM)
{
	return TRUE;
}

u8 mdrv_dmd_atsc_loadfw(enum eDMD_SEL DMD_NUM)
{
	return TRUE;
}

u8 mdrv_dmd_atsc_setvsbmode(void)
{
	return mdrv_dmd_atsc_md_setconfig(0, DMD_ATSC_DEMOD_ATSC_VSB,	TRUE);
}

u8 mdrv_dmd_atsc_set256qammode(void)
{
	return mdrv_dmd_atsc_md_setconfig(0, DMD_ATSC_DEMOD_ATSC_256QAM, TRUE);
}

u8 mdrv_dmd_atsc_set64qammode(void)
{
	return mdrv_dmd_atsc_md_setconfig(0, DMD_ATSC_DEMOD_ATSC_64QAM,	TRUE);
}
#endif //	#ifndef	UTPA2

#endif //	#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN

////////////////////////////////////////////////
///				 MULTI DEMOD API			 ///
////////////////////////////////////////////////

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_init(u8 id,
struct DMD_ATSC_INITDATA *pdmd_atsc_initdata, u32 initdatalen)
#else
u8 mdrv_dmd_atsc_md_init(u8 id,
struct DMD_ATSC_INITDATA *pdmd_atsc_initdata, u32 initdatalen)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	#if	defined(MSOS_TYPE_LINUX_KERNEL) &&\
	defined(CONFIG_CC_IS_CLANG) && defined(CONFIG_MSTAR_CHIP)
	//const struct kernel_symbol *sym;
	#endif
	u8 brfagctristateenable = 0;
	#if	DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	u64/*MS_PHY*/ phynonpmbanksize;
	#endif

	#if ATSC3_ALP_PREPARSE_EN
	if (!mdrv_atsc3_bb2alp_parser_init())
		PRINT("mdrv_atsc3_bb2alp_parser_init fail!!!\n");
	#endif

	if (!(pres->sdmd_atsc_pridata.binit)) {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		if (sizeof(struct DMD_ATSC_INITDATA) == initdatalen) {
			MEMCPY(&(pres->sdmd_atsc_initdata),
			 pdmd_atsc_initdata, initdatalen);
		} else {
			DMD_DBG(
	PRINT("mdrv_dmd_atsc_init input data structure incorrect\n"));
			return FALSE;
		}
		#else
		MEMCPY(&(pres->sdmd_atsc_initdata),
		pdmd_atsc_initdata, sizeof(struct DMD_ATSC_INITDATA));
		#endif
	} else {
		// SYS_ATSC30
		pres->sdmd_atsc_initdata.u8PLP_num	   = pdmd_atsc_initdata->u8PLP_num;
		pres->sdmd_atsc_initdata.u8PLP_id[0]	 = pdmd_atsc_initdata->u8PLP_id[0];
		pres->sdmd_atsc_initdata.u8PLP_id[1]	 = pdmd_atsc_initdata->u8PLP_id[1];
		pres->sdmd_atsc_initdata.u8PLP_id[2]	 = pdmd_atsc_initdata->u8PLP_id[2];
		pres->sdmd_atsc_initdata.u8PLP_id[3]	 = pdmd_atsc_initdata->u8PLP_id[3];
		pres->sdmd_atsc_initdata.u8PLP_layer[0]  = pdmd_atsc_initdata->u8PLP_layer[0];
		pres->sdmd_atsc_initdata.u8PLP_layer[1]  = pdmd_atsc_initdata->u8PLP_layer[1];
		pres->sdmd_atsc_initdata.u8PLP_layer[2]  = pdmd_atsc_initdata->u8PLP_layer[2];
		pres->sdmd_atsc_initdata.u8PLP_layer[3]  = pdmd_atsc_initdata->u8PLP_layer[3];
		pres->sdmd_atsc_initdata.u8Minor_Version = pdmd_atsc_initdata->u8Minor_Version;

		DMD_DBG(PRINT("mdrv_dmd_atsc_init more than once\n"));

		return TRUE;
	}

	#if	defined	MTK_PROJECT
	piodata->sys.getsystemtimems	=	_getsystemtimems;
	piodata->sys.delayms			=	_delayms;
	piodata->sys.createmutex		=	_createmutex;
	piodata->sys.lockdmd			=	_lockdmd;
	#elif	DVB_STI
	piodata->sys.getsystemtimems	=	_getsystemtimems;
	piodata->sys.delayms			=	_delayms;
	piodata->sys.createmutex		=	_createmutex;
	piodata->sys.lockdmd			=	_lockdmd;
	#elif	DMD_ATSC_3PARTY_EN
	// Please initial mutex, timer and MSPI function pointer for 3 perty
	// piodata->sys.getsystemtimems	 = ;
	// piodata->sys.delayms			 = ;
	// piodata->sys.createmutex		 = ;
	// piodata->sys.lockdmd			 = ;
	// piodata->sys.mspi_init_ext	 = ;
	// piodata->sys.MSPI_SlaveEnable = ;
	// piodata->sys.mspi_write		 = ;
	// piodata->sys.mspi_read		 = ;
	#elif	!DMD_ATSC_UTOPIA2_EN
	piodata->sys.delayms			=	MsOS_DelayTask;
	piodata->sys.mspi_init_ext	=	MDrv_MSPI_Init_Ext;
	piodata->sys.MSPI_SlaveEnable	=	MDrv_MSPI_SlaveEnable;
	piodata->sys.mspi_write		=	MDrv_MSPI_Write;
	piodata->sys.mspi_read		=	MDrv_MSPI_Read;
	#endif

	#if	DMD_ATSC_UTOPIA_EN && !DMD_ATSC_UTOPIA2_EN
	if (_s32DMD_ATSC_Mutex == -1) {
		_s32DMD_ATSC_Mutex = MsOS_createmutex(
		E_MSOS_FIFO, "Mutex DMD ATSC", MSOS_PROCESS_SHARED);
		if (_s32DMD_ATSC_Mutex == -1) {
			DMD_DBG(
			PRINT("mdrv_dmd_atsc_init Create Mutex Fail\n"));
			return FALSE;
		}
	}
	#elif (!DMD_ATSC_UTOPIA_EN && \
	!DMD_ATSC_UTOPIA2_EN && DMD_ATSC_MULTI_THREAD_SAFE)
	if (!piodata->sys.createmutex(TRUE)) {
		DMD_DBG(PRINT("mdrv_dmd_atsc_init Create Mutex Fail\n"));
		return FALSE;
	}
	#endif

	#ifdef MS_DEBUG
	if (dmd_atsc_dbglevel >= DMD_ATSC_DBGLV_INFO)
		PRINT("mdrv_dmd_atsc_init\n");

	#endif

	#if	DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	if (!MDrv_MMIO_GetBASE(&
		(pres->sdmd_atsc_pridata.virtdmdbaseaddr),
		&phynonpmbanksize, MS_MODULE_PM)) {
		#ifdef MS_DEBUG
		PRINT("HAL_DMD_RegInit failure to get MS_MODULE_PM\n");
		#endif

		return FALSE;
	}
	#endif

	#if	DVB_STI
	pres->sdmd_atsc_pridata.virtdmdbaseaddr =
	pres->sdmd_atsc_initdata.virt_riu_base_addr;

	pres->sdmd_atsc_pridata.virt_reset_base_addr =
	pres->sdmd_atsc_initdata.virt_reset_base_addr;

	pres->sdmd_atsc_pridata.virt_ts_base_addr =
	pres->sdmd_atsc_initdata.virt_ts_base_addr;

	pres->sdmd_atsc_pridata.virt_clk_base_addr =
	pres->sdmd_atsc_initdata.virt_clk_base_addr;

	pres->sdmd_atsc_pridata.virt_sram_base_addr =
	pres->sdmd_atsc_initdata.virt_sram_base_addr;

	pres->sdmd_atsc_pridata.m6_version =
	pres->sdmd_atsc_initdata.m6_version;
	PRINT("mt5896 version is = 0x%x\n", pres->sdmd_atsc_pridata.m6_version);
	if ((pres->sdmd_atsc_pridata.m6_version & 0xFF0F) > 0x0100) {
		pres->sdmd_atsc_pridata.virt_pmux_base_addr =
		pres->sdmd_atsc_initdata.virt_pmux_base_addr;

		pres->sdmd_atsc_pridata.virt_vagc_base_addr =
		pres->sdmd_atsc_initdata.virt_vagc_base_addr;
	}
	#endif

	pres->sdmd_atsc_pridata.binit = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();

	#if	DMD_ATSC_STR_EN
	if (pres->sdmd_atsc_pridata.elaststate == 0) {
		pres->sdmd_atsc_pridata.elaststate = E_POWER_RESUME;

		pres->sdmd_atsc_pridata.elasttype = DMD_ATSC_DEMOD_ATSC_VSB;
		pres->sdmd_atsc_pridata.symrate = 10762;
	}
	#else
	pres->sdmd_atsc_pridata.elasttype  = DMD_ATSC_DEMOD_ATSC_VSB;
	pres->sdmd_atsc_pridata.symrate = 10762;
	#endif

	#if !DMD_ATSC_3PARTY_EN
	#if defined(MSOS_TYPE_LINUX_KERNEL)	&&\
	defined(CONFIG_CC_IS_CLANG) && defined(CONFIG_MSTAR_CHIP)
	#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	sym = find_symbol("hal_intern_atsc_ioctl_cmd", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		hal_intern_atsc_ioctl_cmd =
		(u8 (*)(struct ATSC_IOCTL_DATA, enum DMD_ATSC_HAL_COMMAND,
	void *))((unsigned long)offset_to_ptr(&sym->value_offset));
	else
		hal_intern_atsc_ioctl_cmd = _null_ioctl_fp_atsc;
	sym = find_symbol("hal_extern_atsc_ioctl_cmd", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		hal_extern_atsc_ioctl_cmd = (IOCTL)(
		(unsigned long)offset_to_ptr(&sym->value_offset));
	else
		hal_extern_atsc_ioctl_cmd = _null_ioctl_fp_atsc;
	sym = find_symbol("mdrv_sw_iic_writebytes", NULL,	NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_sw_iic_writebytes =
		(u8 (*)(u8, u8, u8, u8 *,
	u16, u8 *))((unsigned long)offset_to_ptr(&sym->value_offset));
	else
		mdrv_sw_iic_writebytes	=	_NULL_SWI2C_FP_ATSC;
	sym = find_symbol("mdrv_sw_iic_readbytes", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_sw_iic_readbytes =
		(u8 (*)(u8, u8, u8, u8 *,
	u16, u8 *))((unsigned long)offset_to_ptr(&sym->value_offset));
	else
		mdrv_sw_iic_readbytes = _NULL_SWI2C_FP_ATSC;
	sym = find_symbol("mdrv_hw_iic_writebytes", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_hw_iic_writebytes =
		(u8 (*)(u8, u8, u8, u8 *,
	u16, u8 *))((unsigned long)offset_to_ptr(&sym->value_offset));
	else
		mdrv_hw_iic_writebytes = _null_hwi2c_fp_atsc;
	sym = find_symbol("mdrv_hw_iic_readbytes", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_hw_iic_readbytes = (u8 (*)(u8, u8, u8,
u8 *, u16, u8 *))((unsigned long)offset_to_ptr(&sym->value_offset));
	else
		mdrv_hw_iic_readbytes = _null_hwi2c_fp_atsc;
	#else	// #ifdef	CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	sym = find_symbol("hal_intern_atsc_ioctl_cmd", NULL, NULL, 0, 0);
	if (sym	!= NULL) {
		hal_intern_atsc_ioctl_cmd = (u8 (*)(struct ATSC_IOCTL_DATA,
	enum DMD_ATSC_HAL_COMMAND, void *))(sym->value);

	} else {
		hal_intern_atsc_ioctl_cmd = _null_ioctl_fp_atsc;
	}
	sym = find_symbol("hal_extern_atsc_ioctl_cmd", NULL, NULL, 0, 0);
	if (sym	!= NULL) {
		hal_extern_atsc_ioctl_cmd = (u8 (*)(struct ATSC_IOCTL_DATA,
	enum DMD_ATSC_HAL_COMMAND, void *))(sym->value);

	} else {
		hal_extern_atsc_ioctl_cmd = _null_ioctl_fp_atsc;
	}
	sym = find_symbol("mdrv_sw_iic_writebytes", NULL, NULL, 0, 0);
	if (sym	!= NULL) {
		mdrv_sw_iic_writebytes = (u8 (*)(u8, u8, u8,
	u8 *, u16, u8 *))(sym->value);

	} else {
		mdrv_sw_iic_writebytes	=	_NULL_SWI2C_FP_ATSC;
	}
	sym = find_symbol("mdrv_sw_iic_readbytes", NULL, NULL, 0, 0);
	if (sym	!= NULL) {
		mdrv_sw_iic_readbytes = (u8 (*)(u8, u8, u8,
	u8 *, u16, u8 *))sym->value;
	} else {
		mdrv_sw_iic_readbytes = _NULL_SWI2C_FP_ATSC;
	}
	sym = find_symbol("mdrv_hw_iic_writebytes", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_hw_iic_writebytes = (u8 (*)(u8, u8,
		u8, u8 *, u16, u8 *))(sym->value);
	else
		mdrv_hw_iic_writebytes = _null_hwi2c_fp_atsc;
	sym = find_symbol("mdrv_hw_iic_readbytes", NULL, NULL, 0, 0);
	if (sym	!= NULL)
		mdrv_hw_iic_readbytes = (u8 (*)(u8, u8,
	u8, u8 *, u16, u8 *))sym->value;
	else
		mdrv_hw_iic_readbytes = _null_hwi2c_fp_atsc;
	#endif // #ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	#endif
	#endif

	#if !DVB_STI
	if (pres->sdmd_atsc_initdata.bisextdemod) {
		if (hal_extern_atsc_ioctl_cmd	== _null_ioctl_fp_atsc)
			pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd =
			_default_ioctl_cmd;
		else
			pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd =
			hal_extern_atsc_ioctl_cmd;
	} else {
		if (hal_intern_atsc_ioctl_cmd	== _null_ioctl_fp_atsc)
			pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd =
			_default_ioctl_cmd;
		else {
			pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd =
			hal_intern_atsc_ioctl_cmd;
		}
	}
	#else
	if (pres->sdmd_atsc_initdata.bisextdemod)
		pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd = hal_extern_atsc_ioctl_cmd;
	else
		pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd = hal_intern_atsc_ioctl_cmd;
	#endif

	#ifndef	MSOS_TYPE_LINUX_KERNEL
	#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
	if (hal_extern_atsc_ioctl_cmd_0	== _null_ioctl_fp_atsc)
		EXT_ATSC_IOCTL[0] = _default_ioctl_cmd;
	if (hal_extern_atsc_ioctl_cmd_1	== _null_ioctl_fp_atsc)
		EXT_ATSC_IOCTL[1] = _default_ioctl_cmd;
	if (hal_extern_atsc_ioctl_cmd_2	== _null_ioctl_fp_atsc)
		EXT_ATSC_IOCTL[2] = _default_ioctl_cmd;
	if (hal_extern_atsc_ioctl_cmd_3	== _null_ioctl_fp_atsc)
		EXT_ATSC_IOCTL[3] = _default_ioctl_cmd;
	#endif
	#endif //	#ifndef	MSOS_TYPE_LINUX_KERNEL

	_updateiodata(id, pres);

	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_INITCLK, &brfagctristateenable);

	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_DOWNLOAD, NULL);

	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_FWVERSION, NULL);

	err_accum = 0;
	DMD_UNLOCK();

	return TRUE;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_exit(u8 id)
#else
u8 mdrv_dmd_atsc_md_exit(u8 id)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (pres->sdmd_atsc_pridata.binit	== FALSE)
		return bret;
	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	#if ATSC3_ALP_PREPARSE_EN
	mdrv_atsc3_bb2alp_parser_close();
	#endif

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd
	(piodata, DMD_ATSC_HAL_CMD_EXIT, NULL);

	pres->sdmd_atsc_pridata.binit	=	FALSE;
	DMD_UNLOCK();

	#if	DMD_ATSC_UTOPIA_EN &&	!DMD_ATSC_UTOPIA2_EN
	MsOS_DeleteMutex(_s32DMD_ATSC_Mutex);

	_s32DMD_ATSC_Mutex = -1;
	#elif (!DMD_ATSC_UTOPIA_EN && \
	!DMD_ATSC_UTOPIA2_EN && DMD_ATSC_MULTI_THREAD_SAFE)
	piodata->sys.createmutex(FALSE);
	#endif

	return bret;
}

#ifdef UTPA2
u32 _mdrv_dmd_atsc_md_getconfig(
u8 id, struct DMD_ATSC_INITDATA *psdmd_atsc_initdata)
#else
u32 mdrv_dmd_atsc_md_getconfig(
u8 id, struct DMD_ATSC_INITDATA *psdmd_atsc_initdata)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + id;

	MEMCPY(psdmd_atsc_initdata,
	&(pres->sdmd_atsc_initdata), sizeof(struct DMD_ATSC_INITDATA));

	return UTOPIA_STATUS_SUCCESS;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_setconfig(
u8 id, enum DMD_ATSC_DEMOD_TYPE etype, u8 benable)
#else
u8 mdrv_dmd_atsc_md_setconfig(
u8 id, enum DMD_ATSC_DEMOD_TYPE etype, u8 benable)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_SOFTRESET, NULL);

	if (benable) {
		switch (etype) {
		case DMD_ATSC_DEMOD_ATSC_VSB:
			pres->sdmd_atsc_pridata.elasttype =
			DMD_ATSC_DEMOD_ATSC_VSB;
			pres->sdmd_atsc_pridata.symrate = 10762;
			bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
			piodata, DMD_ATSC_HAL_CMD_SETVSBMODE, NULL);
		break;
		case DMD_ATSC_DEMOD_ATSC_OFDM:
			pres->sdmd_atsc_pridata.elasttype =
			DMD_ATSC_DEMOD_ATSC_OFDM;
			pres->sdmd_atsc_pridata.symrate = 6912;
			bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
			piodata, DMD_ATSC_HAL_CMD_SetOFDMMode, NULL);
		break;
		case DMD_ATSC_DEMOD_ATSC_64QAM:
			pres->sdmd_atsc_pridata.elasttype =
			DMD_ATSC_DEMOD_ATSC_64QAM;
			pres->sdmd_atsc_pridata.symrate = 5056;
			bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
			piodata, DMD_ATSC_HAL_CMD_SET64QAMMODE, NULL);
		break;
		case DMD_ATSC_DEMOD_ATSC_256QAM:
			pres->sdmd_atsc_pridata.elasttype =
			DMD_ATSC_DEMOD_ATSC_256QAM;
			pres->sdmd_atsc_pridata.symrate = 5360;
			bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
			piodata, DMD_ATSC_HAL_CMD_SET256QAMMODE, NULL);
		break;
		default:
			pres->sdmd_atsc_pridata.elasttype = DMD_ATSC_DEMOD_NULL;
			pres->sdmd_atsc_pridata.symrate = 0;
			bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
			piodata, DMD_ATSC_HAL_CMD_SETMODECLEAN, NULL);
		break;
		}
	}

	#if	DMD_ATSC_UTOPIA_EN ||	DMD_ATSC_UTOPIA2_EN
	pres->sdmd_atsc_info.atscscantimestart =
	MsOS_GetSystemTime();
	#else
	pres->sdmd_atsc_info.atscscantimestart =
	piodata->sys.getsystemtimems();
	#endif
	pres->sdmd_atsc_info.atsclockstatus = 0;
	DMD_UNLOCK();

	return bret;
}

#if	DMD_ATSC_STR_EN
#ifdef UTPA2
u32 _mdrv_dmd_atsc_md_setpowerstate(u8 id,
enum EN_POWER_MODE powerstate)
#else
u32 mdrv_dmd_atsc_md_setpowerstate(u8 id,
enum EN_POWER_MODE powerstate)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u32 u32return_tmp = UTOPIA_STATUS_FAIL;

	if (powerstate	== E_POWER_SUSPEND) {
		pres->sdmd_atsc_pridata.bdownloaded = FALSE;
		pres->sdmd_atsc_pridata.bisdtv =
		pres->sdmd_atsc_pridata.binit;
		pres->sdmd_atsc_pridata.elaststate = powerstate;

		if (pres->sdmd_atsc_pridata.binit) {
			#ifdef UTPA2
			_mdrv_dmd_atsc_md_exit(id);
			#else
			mdrv_dmd_atsc_md_exit(id);
			#endif
		}

		u32return_tmp = UTOPIA_STATUS_SUCCESS;
	} else if	(powerstate == E_POWER_RESUME) {
		if (pres->sdmd_atsc_pridata.elaststate ==
			E_POWER_SUSPEND) {
			if (pres->sdmd_atsc_pridata.bisdtv) {
				#ifdef UTPA2
				_mdrv_dmd_atsc_md_init(id,
				&(pres->sdmd_atsc_initdata),
				sizeof(struct DMD_ATSC_INITDATA));

				#else
				mdrv_dmd_atsc_md_init(id,
				&(pres->sdmd_atsc_initdata),
				sizeof(struct DMD_ATSC_INITDATA));
				mdrv_dmd_atsc_md_doiqswap(id,
				pres->sdmd_atsc_pridata.bis_qpad);
				#endif

				if (pres->sdmd_atsc_pridata.elasttype !=
					DMD_ATSC_DEMOD_ATSC_VSB) {
					if (pres->sdmd_atsc_pridata.elasttype ==
						DMD_ATSC_DEMOD_ATSC_64QAM &&
				pres->sdmd_atsc_pridata.symrate == 5056) {
						#ifdef UTPA2
						_mdrv_dmd_atsc_md_setconfig(id,
				pres->sdmd_atsc_pridata.elasttype, TRUE);
						#else
						mdrv_dmd_atsc_md_setconfig(id,
				pres->sdmd_atsc_pridata.elasttype, TRUE);
						#endif
					} else if (
					pres->sdmd_atsc_pridata.elasttype ==
						DMD_ATSC_DEMOD_ATSC_256QAM &&
				pres->sdmd_atsc_pridata.symrate == 5360) {
						#ifdef UTPA2
						_mdrv_dmd_atsc_md_setconfig(id,
					pres->sdmd_atsc_pridata.elasttype,
						TRUE);
						#else
						mdrv_dmd_atsc_md_setconfig(id,
					pres->sdmd_atsc_pridata.elasttype,
						TRUE);
						#endif
					} else {
						#ifdef UTPA2

						#else
						mdrv_dmd_atsc_md_set_qam_sr(id,
					pres->sdmd_atsc_pridata.elasttype,
					pres->sdmd_atsc_pridata.symrate);
						#endif
					}
				} else {
					#ifdef UTPA2
					_mdrv_dmd_atsc_md_setconfig(id,
				pres->sdmd_atsc_pridata.elasttype, TRUE);
					#else
					mdrv_dmd_atsc_md_setconfig(id,
				pres->sdmd_atsc_pridata.elasttype, TRUE);
					#endif
				}
			}

			pres->sdmd_atsc_pridata.elaststate = powerstate;

			u32return_tmp	=	UTOPIA_STATUS_SUCCESS;
		} else {
			PRINT("It is not suspended yet. We shouldn't resume\n");

			u32return_tmp	=	UTOPIA_STATUS_FAIL;
		}
	} else {
		PRINT("Do Nothing: %d\n", powerstate);

		u32return_tmp	=	UTOPIA_STATUS_FAIL;
	}

	return u32return_tmp;
}
#endif

#ifdef UTPA2
enum DMD_ATSC_LOCK_STATUS _mdrv_dmd_atsc_md_getlock(
u8 id, enum DMD_ATSC_GETLOCK_TYPE etype)
#else
enum DMD_ATSC_LOCK_STATUS mdrv_dmd_atsc_md_getlock(
u8 id, enum DMD_ATSC_GETLOCK_TYPE etype)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	enum DMD_ATSC_LOCK_STATUS status = DMD_ATSC_UNLOCK;

	if (!(pres->sdmd_atsc_pridata.binit))
		return DMD_ATSC_UNLOCK;
	DMD_LOCK();
	_updateiodata(id, pres);

	switch (etype)	{
	case DMD_ATSC_GETLOCK:
		switch (pres->sdmd_atsc_pridata.elasttype)	{
		case DMD_ATSC_DEMOD_ATSC_VSB:
		status = _vsb_checklock();
		break;
	case DMD_ATSC_DEMOD_ATSC_OFDM:
		status = _ATSC_CheckLock();
		break;
		case DMD_ATSC_DEMOD_ATSC_16QAM:
		case DMD_ATSC_DEMOD_ATSC_32QAM:
		case DMD_ATSC_DEMOD_ATSC_64QAM:
		case DMD_ATSC_DEMOD_ATSC_128QAM:
		case DMD_ATSC_DEMOD_ATSC_256QAM:
		status = _qam_checklock();
		break;
		default:
		status = DMD_ATSC_UNLOCK;
		break;
		}
	break;
	case DMD_ATSC_GETLOCK_VSB_AGCLOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_VSB_PRELOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_VSB_PRELOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_VSB_FSYNCLOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_VSB_FSYNC_LOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_VSB_CELOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_VSB_CE_LOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_VSB_FECLOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_VSB_FEC_LOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_QAM_AGCLOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_AGCLOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_QAM_PRELOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_QAM_PRELOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	case DMD_ATSC_GETLOCK_QAM_MAINLOCK:
		status = (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
		piodata, DMD_ATSC_HAL_CMD_QAM_MAIN_LOCK, NULL)) ?
		DMD_ATSC_LOCK : DMD_ATSC_UNLOCK;
		break;
	default:
		status = DMD_ATSC_UNLOCK;
		break;
	}
	DMD_UNLOCK();

	return status;
}

#ifdef UTPA2
enum DMD_ATSC_DEMOD_TYPE _mdrv_dmd_atsc_md_getmodulationmode(u8 id)
#else
enum DMD_ATSC_DEMOD_TYPE mdrv_dmd_atsc_md_getmodulationmode(u8 id)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	enum DMD_ATSC_DEMOD_TYPE etype = DMD_ATSC_DEMOD_NULL;

	if (!(pres->sdmd_atsc_pridata.binit))
		return 0;
	DMD_LOCK();
	_updateiodata(id, pres);

	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_CHECK8VSB64_256QAM, &etype);

	DMD_UNLOCK();

	return etype;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_getsignalstrength(u8 id, u16 *strength)
#else
u8 mdrv_dmd_atsc_md_getsignalstrength(u8 id, u16	*strength)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_READIFAGC, strength);

	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_getsnrpercentage(u8 id)
#else
u8 mdrv_dmd_atsc_md_getsnrpercentage(u8 id)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 percentage = 0;

	if (!(pres->sdmd_atsc_pridata.binit))
		return 0;
	DMD_LOCK();
	_updateiodata(id, pres);

	pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_READSNRPERCENTAGE, &percentage);

	DMD_UNLOCK();

	return percentage;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_get_qam_snr(u8 id,
struct DMD_ATSC_SNR_DATA *f_snr)
#else
u8 mdrv_dmd_atsc_md_get_qam_snr(u8 id, float *f_snr)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_GET_QAM_SNR, f_snr);

	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_read_ucpkt_err(u8 id, u16	*packeterr)
#else
u8 mdrv_dmd_atsc_md_read_ucpkt_err(u8 id,	u16 *packeterr)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_READPKTERR, packeterr);

	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_getpreviterbiber(u8 id,
struct DMD_ATSC_BER_DATA *ber)
#else
u8 mdrv_dmd_atsc_md_getpreviterbiber(u8 id, float *ber)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_GETPREVITERBIBER, ber);

	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_getpostviterbiber(u8 id,
struct DMD_ATSC_BER_DATA *ber)
#else
u8 mdrv_dmd_atsc_md_getpostviterbiber(u8 id, float *ber)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_GETPOSTVITERBIBER, ber);
	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_readfrequencyoffset(
u8 id, struct DMD_ATSC_CFO_DATA *cfo)
#else
u8 mdrv_dmd_atsc_md_readfrequencyoffset(u8 id, s16 *cfo)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	u8 bret = TRUE;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
	piodata, DMD_ATSC_HAL_CMD_READFREQUENCYOFFSET, cfo);

	DMD_UNLOCK();

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_getreg(u8 id, u16 addr_16, u8 *pdata_8)
#else
u8 mdrv_dmd_atsc_md_getreg(u8 id, u16 addr_16, u8 *pdata_8)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	struct DMD_ATSC_REG_DATA reg;
	u8 bret = TRUE;

	reg.addr_16 = addr_16;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(piodata,
	DMD_ATSC_HAL_CMD_GET_REG, &reg);

	DMD_UNLOCK();

	*pdata_8 = reg.data_8;

	return bret;
}

#ifdef UTPA2
u8 _mdrv_dmd_atsc_md_setreg(u8 id, u16 addr_16, u8 data_8)
#else
u8 mdrv_dmd_atsc_md_setreg(u8 id, u16 addr_16, u8 data_8)
#endif
{
	struct DMD_ATSC_RESDATA *pres = psdmd_atsc_resdata + (id&0x0F);
	struct DMD_ATSC_REG_DATA reg;
	u8 bret = TRUE;

	reg.addr_16 = addr_16;
	reg.data_8 = data_8;

	if (!(pres->sdmd_atsc_pridata.binit))
		return FALSE;
	DMD_LOCK();
	_updateiodata(id, pres);

	bret = pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(piodata,
	DMD_ATSC_HAL_CMD_SET_REG, &reg);

	DMD_UNLOCK();

	return bret;
}

u8 _mdrv_dmd_atsc_md_get_info(struct dvb_frontend *fe,
			struct dtv_frontend_properties *p)
{
	u8 ret_tmp;
	u8 data1 = 0;
	u8 data2 = 0;
	u8 data3 = 0;
	u16 ifagc_tmp = 0;
	u16 ifagc = 0;
	u16 snr = 0, err = 0, sqi = 0;
	//s16 cfo;
	u32 postber;
	u8 bret;
	u8 fgdemodStatus = false;
	u8 fgagcstatus = false;
	enum DMD_ATSC_LOCK_STATUS atsclocksStatus = DMD_ATSC_NULL;
	struct DMD_ATSC_BER_DATA getber;
	struct DMD_ATSC_CFO_DATA cfoParameter;
	struct dtv_fe_stats *stats;
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	memset(&cfoParameter, 0, sizeof(struct DMD_ATSC_CFO_DATA));
	memset(&getber, 0, sizeof(struct DMD_ATSC_BER_DATA));

	/* record center freq */
	request_freq = c->frequency;

	/* SQI */
	sqi = _mdrv_dmd_atsc_md_getsnrpercentage(0);
	stats = &p->quality;
	stats->len = 1;
	stats->stat[0].uvalue = sqi;
	stats->stat[0].scale = FE_SCALE_COUNTER;

	/* SNR */
	snr = _mdrv_dmd_atsc_md_getsnrpercentage(0);
	snr = snr * SNR_COREFFICIENT; //steve DBG / 1000;
	stats = &p->cnr;
	stats->len = 1;
	stats->stat[0].svalue = snr;
	stats->stat[0].scale = FE_SCALE_DECIBEL;

	PRINT("DTD_GetSignalSNR:%d\n", snr);

	/* Packet Error */
	ret_tmp = _mdrv_dmd_atsc_md_read_ucpkt_err(0, &err);
	stats = &p->block_error;
	stats->len = 1;
	stats->stat[0].uvalue = err;
	stats->stat[0].scale = FE_SCALE_COUNTER;
	PRINT("HELLO,e_get_type =TUNER_GET_TYPE_UEC\r\n");
	PRINT("UEC = %u\n", err);

	/* BER */
	ret_tmp = _mdrv_dmd_atsc_md_getpostviterbiber(0, &getber);
	if (getber.biterr <= 0)
		postber = 0;
	else
		postber = (getber.biterr * 10000) / (
		(u32)(getber.error_window)*(191488));
	postber = postber * 100;

	p->post_ber = postber;

	stats = &p->post_bit_error;
	stats->len = 1;
	stats->stat[0].uvalue = getber.biterr;
	stats->stat[0].scale = FE_SCALE_COUNTER;

	stats = &p->post_bit_count;
	stats->len = 1;
	stats->stat[0].uvalue = ((u64)getber.error_window)*(BER_ERR_WINDOW_COE);
	stats->stat[0].scale = FE_SCALE_COUNTER;

	PRINT("HELLO,e_get_type =TUNER_GET_TYPE_BER\r\n");
	PRINT("error_window = %d\n", getber.biterr);
	PRINT("error_window = %d\n", getber.error_window);
	PRINT("postber = %d\n", postber);

	/* CFO */
	ret_tmp = _mdrv_dmd_atsc_md_readfrequencyoffset(0, &cfoParameter);
	p->frequency_offset = (s32)cfoParameter.rate;
	PRINT("HELLO,e_get_type =TUNER_GET_TYPE_CFO\r\n");
	PRINT("Carrier Frequency Offset = %d\n", cfoParameter.rate);

	/* Interleaving Mode */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x0624, &data1);
	data1 = data1>>4;

	PRINT("Interleaving Mode = %d\n", data1);
	if (c->delivery_system == SYS_ATSC)
		p->interleaving = (enum fe_interleaving) INTERLEAVING_NONE;
	else {
		if (data1 == 0)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_1_0;
		else if (data1 == 1)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_1_1;
		else if (data1 == 3)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_64_2;
		else if (data1 == 5)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_32_4;
		else if (data1 == 7)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_16_8;
		else if (data1 == 9)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_8_16;
		else if (data1 == 2)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_2;
		else if (data1 == 4)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_3;
		else if (data1 == 6)
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_4;
		else
			p->interleaving = (enum fe_interleaving) INTERLEAVING_128_1_0;
	}

	/* Lock Status */
	atsclocksStatus = _mdrv_dmd_atsc_md_getlock(0, DMD_ATSC_GETLOCK);
	if (atsclocksStatus == DMD_ATSC_LOCK)
		fgdemodStatus = TRUE;
	else if (atsclocksStatus == DMD_ATSC_UNLOCK)
		fgdemodStatus = FALSE;

	PRINT("HELLO,e_get_type =TUNER_GET_TYPE_DEMOD_LOCK_STATUS\r\n");
	PRINT("DEMOD Lock = %d\n", fgdemodStatus);

	/* AGC Lock Status */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x2829, &data1);
	if (data1 == 0x01)
		fgagcstatus = TRUE;
	else
		fgagcstatus = FALSE;

	PRINT("HELLO,e_get_type =TUNER_GET_TYPE_AGC_STATUS\r\n");
	PRINT("DEMOD AGC Lock = %d\n", fgagcstatus);

	/* AGC Level */
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2822, 0x03);
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2805, 0x80);

	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x2824, &data1);
	ifagc_tmp = (u16)data1;
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x2825, &data1);
	ifagc_tmp |= (u16)data1<<8;
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2805, 0x00);

	ifagc_tmp = (ifagc_tmp>>8)&0xFF;
	ifagc = (~ifagc_tmp)&0xFF;
	p->agc_val = ifagc;

	PRINT("e_get_type =TUNER_GET_TYPE_AGC\r\n");
	PRINT("DTD AGC: %d\n", ifagc);

	/* Get FW Version */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20C4, &data1);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20CF, &data2);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20D0, &data3);
	PRINT("INTERN_ATSC_FW_VERSION:%x.%x.%x\n", data1,	data2, data3);

	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x0400, &data1);
	if (data1 == 0x02)
		p->modulation = QAM_256;
	else if (data1 == 0x04)
		p->modulation = VSB_8;
	else
		p->modulation = QAM_64;

	bret = 0;

	return bret;

}

ssize_t atsc_get_information(char *buf)
{
	u8 ret_tmp = 0;
	u8 data1 = 0, data2 = 0, data3 = 0;
	u8 err1, err2, err3;
	u8 *fgdemodStatus = NULL;
	u8 *modulaton_type;
	u8 *interleave_mode;
	u8 *system_type = NULL;
	u8 *bandwidth = NULL;
	u8 demodstatus = FALSE;
	u16 snr, err, sqi, snr_float;
	u16 ifagc_tmp, ifagc;
	s16 cfo;
	u32 dmd_rev;
	u32 postber;
	s32 stemp32 = 0;
	char order_ber[8] = "e+";

	struct DMD_ATSC_CFO_DATA cfoParameter;
	struct DMD_ATSC_BER_DATA getber;
	//enum mtk_fe_interleaving interleaving_mode;
	enum DMD_ATSC_LOCK_STATUS atsclocksStatus = DMD_ATSC_NULL;
	//enum DMD_ATSC_GETLOCK_TYPE etype;

	memset(&cfoParameter, 0, sizeof(struct DMD_ATSC_CFO_DATA));
	memset(&getber, 0, sizeof(struct DMD_ATSC_BER_DATA));
	/* SQI */
	sqi = _mdrv_dmd_atsc_md_getsnrpercentage(0);

	/* SNR */
	snr = _mdrv_dmd_atsc_md_getsnrpercentage(0);
	snr = snr * 4; //steve DBG / 10;
	snr_float = snr % 10;
	snr = snr / 10;
	PRINT("DTD_GetSignalSNR:%d\n", snr);

	/* Get Lock Status */
	atsclocksStatus = _mdrv_dmd_atsc_md_getlock(0, DMD_ATSC_GETLOCK);
	if (atsclocksStatus == DMD_ATSC_LOCK) {
		fgdemodStatus = "Lock";
		demodstatus = TRUE;
	} else if (atsclocksStatus == DMD_ATSC_UNLOCK) {
		err_accum = 0;
		fgdemodStatus = "Unlock";
		demodstatus = FALSE;
	} else {
		fgdemodStatus = "Checking";
	}
	//PRINT("TUNER_GET_DEMOD_LOCK: %d\r\n", fgdemodStatus);

	/* BER */
	ret_tmp = _mdrv_dmd_atsc_md_getpostviterbiber(0, &getber);
	if (getber.biterr <= 0)
		postber = 0;
	else
		postber = getber.biterr * 5222 / (u32)(getber.error_window);

	if (demodstatus && postber == 0) {
		strncpy(order_ber, "e+", sizeof(order_ber));
		err1 = 0;
		err2 = 0;
		err3 = 0;
	}	else if (demodstatus == FALSE) {
		strncpy(order_ber, "e+", sizeof(order_ber));
		err1 = 1;
		err2 = 0;
		err3 = 0;
	}	else {
		stemp32 = postber;
		for (data3 = 0; stemp32 >= 100; data3++)
			stemp32 = (s32) (stemp32/10);
		err1 = (s32) (stemp32/10);
		err2 = stemp32 - err1 * 10;
		err3 = 9 - data3 - 1;
		strncpy(order_ber, "e-", sizeof(order_ber));
	}
	//(getber.biterr * 10000) / ((u32)(getber.error_window)*(191488));
	//postber = postber * 100;

	//PRINT("HELLO,e_get_type =TUNER_GET_TYPE_BER\r\n");
	//PRINT("error_window = %d\n", getber.biterr);
	//PRINT("error_window = %d\n", getber.error_window);
	//PRINT("postber = %d\n", postber);

	/* Packet Error */
	//ret_tmp = _mdrv_dmd_atsc_md_read_ucpkt_err(0, &err);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x1f66, &data1);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x1f67, &data2);
	err = ((u16)data2<<8) | ((u16)data1);
	PRINT("UEC = %u\n", err);

	err_accum += err;
	PRINT("UEC Accumulate= %u\n", err);

	/* AGC Level */
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2822, 0x03);
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2805, 0x80);

	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x2824, &data1);
	ifagc_tmp = (u16)data1;
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x2825, &data1);
	ifagc_tmp |= (u16)data1<<8;
	ret_tmp = _mdrv_dmd_atsc_md_setreg(0, 0x2805, 0x00);

	ifagc_tmp = (ifagc_tmp>>8)&0xFF;
	ifagc = (~ifagc_tmp)&0xFF;

	PRINT("TUNER_GET_TYPE_AGC\r\n");
	PRINT("DTD AGC: %d\n", ifagc);

	/* CFO */
	ret_tmp = _mdrv_dmd_atsc_md_readfrequencyoffset(0, &cfoParameter);
	cfo = (s32)cfoParameter.rate;
	PRINT("Carrier Frequency Offset = %d\n", cfoParameter.rate);

	/* Interleaving Mode */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x0624, &data1);
	data1 = data1>>4;

	PRINT("Interleaving Mode = %d\n", data1);

	if (data1 == 0)
		interleave_mode = "I=128/J=1";//interleaving_mode = INTERLEAVING_128_1_0;
	else if (data1 == 1)
		interleave_mode = "I=128/J=1";//interleaving_mode = INTERLEAVING_128_1_1;
	else if (data1 == 3)
		interleave_mode = "I=64/J=2";//interleaving_mode = INTERLEAVING_64_2;
	else if (data1 == 5)
		interleave_mode = "I=32/J=4";//interleaving_mode = INTERLEAVING_32_4;
	else if (data1 == 7)
		interleave_mode = "I=16/J=8";//interleaving_mode = INTERLEAVING_16_8;
	else if (data1 == 9)
		interleave_mode = "I=8/J=16";//interleaving_mode = INTERLEAVING_8_16;
	else if (data1 == 2)
		interleave_mode = "I=128/J=2";//interleaving_mode = INTERLEAVING_128_2;
	else if (data1 == 4)
		interleave_mode = "I=128/J=3";//interleaving_mode = INTERLEAVING_128_3;
	else if (data1 == 6)
		interleave_mode = "I=128/J=4";//interleaving_mode = INTERLEAVING_128_4;
	else
		interleave_mode = "Invalid";//interleaving_mode = INTERLEAVING_NONE;

	/* Get FW Version */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20C4, &data1);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20CF, &data2);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x20D0, &data3);
	dmd_rev = (data1 << 16) | (data2 << 8) | data3;
	PRINT("INTERN_ATSC_FW_VERSION:%x.%x.%x\n", data1, data2, data3);

	/* Bandwidth*/
	bandwidth = "6MHz";
	/* System Type */
	/* Get Modulation */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(0, 0x0400, &data1);
	if (data1 == 0x02) {
		system_type = "J.83B";
		modulaton_type = "256QAM";// DMD_ATSC_DEMOD_ATSC_256QAM;
	} else if (data1 == 0x04) {
		system_type = "ATSC1.0";
		modulaton_type = "8VSB";//DMD_ATSC_DEMOD_ATSC_VSB;
		interleave_mode = "Invalid";//interleaving_mode = INTERLEAVING_NONE;
	} else {
		system_type = "J.83B";
		modulaton_type = "64QAM";//etype = DMD_ATSC_DEMOD_ATSC_64QAM;
	}

	return scnprintf(buf, PAGE_SIZE,
	"%s%s%s%s %s%s%s%s %s%s%s%s %s%s%d%s %s%s%d%s %s%s%d%s%d%s %s%s%d%s %s%s%d%s %s%s%d%s %s%s%d%s %s%s%s%s %s%s%s%s %s%s%s%s %s%s%s%s %s%s%d%s%d%s%d%s %s%s%d%s%d%s %s%s%d%s",
	SM_CHIP_REV, SM_COMMA, SM_NONE, SM_END,
	SM_SYSTEM, SM_COMMA, system_type, SM_END,
	SM_BANDWIDTH, SM_COMMA, bandwidth, SM_END,
	SM_FREQUENCY, SM_COMMA, request_freq, SM_END,
	SM_PILOT_OFFSET, SM_COMMA, cfo, SM_END,
	SM_CNR, SM_COMMA, snr, SM_DOT, snr_float, SM_END,
	SD_SIGNAL_QAULITY, SM_COMMA, SM_NONE, SM_END,
	SM_UEC, SM_COMMA, err, SM_END,
	SM_UEC_ACCU, SM_COMMA, err_accum, SM_END,
	SM_AGC, SM_COMMA, ifagc, SM_END,
	SM_INTERLEAVING, SM_COMMA, interleave_mode, SM_END,
	SM_MODULATION, SM_COMMA, modulaton_type, SM_END,
	SM_DEMOD_LOCK, SM_COMMA, fgdemodStatus, SM_END,
	SM_TS_LOCK, SM_COMMA, fgdemodStatus, SM_END,
	SM_BER_BEFORE_RS, SM_COMMA, err1, SM_DOT, err2, order_ber, err3, SM_END,
	SM_C_N, SM_COMMA, snr, SM_DOT, snr_float, SM_END,
	SD_STATUS, SM_COMMA, demodstatus, SM_END);
}
u8 _mdrv_dmd_atsc3_md_get_info(struct dvb_frontend *fe,
			struct dtv_frontend_properties *p)
{
	u8 ret_tmp;
	u8 data1 = 0;
	u8 data2 = 0;
	u8 data3 = 0;
	u8 bret = 0;
	u8 u1tmp = 0;
	u64 u64_PLP_core_layer_ID;
	u64 u64_PLP_Enhanced_layer_ID;
	u64 u64_lls_PLP_Flag;

	struct dtv_frontend_properties *c = &fe->dtv_property_cache;

	/* Get FW Version */
	ret_tmp = _mdrv_dmd_atsc_md_getreg(1, 0x20C4, &data1);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(1, 0x20C5, &data2);
	ret_tmp = _mdrv_dmd_atsc_md_getreg(1, 0x20C6, &data3);
	PRINT("INTERN_ATSC_FW_VERSION:%x.%x.%x\n", data1,   data2, data3);
	pr_err("ATSC_FW_VERSION:%x.%x.%x\n", data1,   data2, data3);

	bret = _mdrv_dmd_atsc_md_getreg(1, 0x1737, &u1tmp);
	u64_PLP_core_layer_ID = u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1736, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &=  _mdrv_dmd_atsc_md_getreg(1, 0x1735, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1734, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &=  _mdrv_dmd_atsc_md_getreg(1, 0x1733, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1732, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1731, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;

	bret &=  _mdrv_dmd_atsc_md_getreg(1, 0x1730, &u1tmp);
	u64_PLP_core_layer_ID = u64_PLP_core_layer_ID << 8 | u1tmp;
	/////Enhanced layer PLP
	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173F, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173E, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173D, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173C, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173B, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x173A, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &=  _mdrv_dmd_atsc_md_getreg(1, 0x1739, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1738, &u1tmp);
	u64_PLP_Enhanced_layer_ID = u64_PLP_Enhanced_layer_ID << 8 | u1tmp;

	///lls PLP
	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1787, &u1tmp);
	u64_lls_PLP_Flag = u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1786, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1785, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1784, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1783, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1782, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &=  _mdrv_dmd_atsc_md_getreg(1, 0x1781, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	bret &= _mdrv_dmd_atsc_md_getreg(1, 0x1780, &u1tmp);
	u64_lls_PLP_Flag = u64_lls_PLP_Flag << 8 | u1tmp;

	pr_err("u64Cl_Plp_Id = 0x%llx\n", u64_PLP_core_layer_ID);
	pr_err("u64El_Plp_Id = 0x%llx\n", u64_PLP_Enhanced_layer_ID);
	pr_err("u64Lls_Plp_Id = 0x%llx\n", u64_lls_PLP_Flag);

	p->plpId_bitmap.bitmap = u64_PLP_core_layer_ID|u64_PLP_Enhanced_layer_ID;
	p->llsFlag_bitmap.bitmap = u64_lls_PLP_Flag;
	bret = 0;
	return bret;
}
#ifdef UTPA2

#if	DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN

////////////////////////////////////////////////
///				 UTOPIA	2K API				 ///
////////////////////////////////////////////////

u8 mdrv_dmd_atsc_setdbglevel(enum DMD_ATSC_DBGLV dbglevel)
{
	ATSC_DBG_LEVEL_PARAM dbglevelparam;

	if (!_atscopen)
		return	FALSE;

	dbglevelparam.dbglevel = dbglevel;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_SetDbgLevel,
		&dbglevelparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

struct DMD_ATSC_INFO *mdrv_dmd_atsc_getinfo(void)
{
	ATSC_GET_INFO_PARAM	getinfoparam = {0};

	if (!_atscopen)
		return	NULL;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_GetInfo, &getinfoparam)
		== UTOPIA_STATUS_SUCCESS) {
		return getinfoparam.pinfo;
	} else {
		return NULL;
	}
}

u8 mdrv_dmd_atsc_md_init(
u8 id, struct DMD_ATSC_INITDATA *pdmd_atsc_initdata, u32 initdatalen)
{
	void	*pattribte	=	NULL;
	ATSC_INIT_PARAM	initparam	=	{0};

	if (_atscopen ==	0) {
		if (UtopiaOpen(MODULE_ATSC, &_ppatscinstant, 0, pattribte)
		== UTOPIA_STATUS_SUCCESS)
			_atscopen = 1;
		else {
			return FALSE;
		}
	}

	if (!_atscopen)
		return	FALSE;

	initparam.id = id;
	initparam.pdmd_atsc_initdata = pdmd_atsc_initdata;
	initparam.initdatalen = initdatalen;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_Init,
		&initparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_exit(u8 id)
{
	ATSC_ID_PARAM idparam = {0};

	if (!_atscopen)
		return	FALSE;

	idparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_Exit,
		&idparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u32 mdrv_dmd_atsc_md_getconfig(
u8 id, struct DMD_ATSC_INITDATA *psdmd_atsc_initdata)
{
	ATSC_INIT_PARAM	initparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	initparam.id = id;
	initparam.pdmd_atsc_initdata = psdmd_atsc_initdata;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetConfig,
		&initparam) == UTOPIA_STATUS_SUCCESS)
		return UTOPIA_STATUS_SUCCESS;
	else
		return UTOPIA_STATUS_ERR_NOT_AVAIL;

}

u8 mdrv_dmd_atsc_md_setconfig(
u8 id, enum DMD_ATSC_DEMOD_TYPE etype, u8 benable)
{
	ATSC_SET_CONFIG_PARAM	setconfigparam = {0};

	if (!_atscopen)
		return	FALSE;

	setconfigparam.id	=	id;
	setconfigparam.etype = etype;
	setconfigparam.benable = benable;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SetConfig,
		&setconfigparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_setreset(u8 id)
{
	ATSC_ID_PARAM	idparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	idparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SetReset,
		idparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_set_qam_sr(
u8 id, enum DMD_ATSC_DEMOD_TYPE etype, u16 symbol_rate)
{
	ATSC_SET_QAM_SR_PARAM	setqamsrparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	setqamsrparam.id = id;
	setqamsrparam.etype = etype;
	setqamsrparam.symbol_rate = symbol_rate;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_Set_QAM_SR,
		&setqamsrparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_setactive(u8 id, u8 benable)
{
	ATSC_SET_ACTIVE_PARAM	setactiveparam = {0};

	if (!_atscopen)
		return	FALSE;

	setactiveparam.id	=	id;
	setactiveparam.benable = benable;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SetActive,
		&setactiveparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

#if	DMD_ATSC_STR_EN
u32 mdrv_dmd_atsc_md_setpowerstate(u8 id,
enum EN_POWER_MODE powerstate)
{
	ATSC_SET_POWER_STATE_PARAM setpowerstateparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	setpowerstateparam.id	=	id;
	setpowerstateparam.powerstate = powerstate;

	return UtopiaIoctl(_ppatscinstant,
	DMD_ATSC_DRV_CMD_MD_SetPowerState, &setpowerstateparam);
}
#endif

enum DMD_ATSC_LOCK_STATUS mdrv_dmd_atsc_md_getlock(
u8 id, enum DMD_ATSC_GETLOCK_TYPE etype)
{
	ATSC_GET_LOCK_PARAM	getlockparam = {0};

	if (!_atscopen)
		return	DMD_ATSC_NULL;

	getlockparam.id	=	id;
	getlockparam.etype = etype;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetLock,
		&getlockparam) == UTOPIA_STATUS_SUCCESS)
		return getlockparam.status;
	else
		return DMD_ATSC_NULL;

}

enum DMD_ATSC_DEMOD_TYPE mdrv_dmd_atsc_md_getmodulationmode(u8 id)
{
	ATSC_GET_MODULATION_MODE_PARAM getmodulationmodeparam;

	if (!_atscopen)
		return	FALSE;

	getmodulationmodeparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetModulationMode,
		&getmodulationmodeparam) == UTOPIA_STATUS_SUCCESS)
		return getmodulationmodeparam.etype;
	else
		return DMD_ATSC_DEMOD_NULL;

}

u8 mdrv_dmd_atsc_md_getsignalstrength(u8 id, u16 *strength)
{
	ATSC_GET_SIGNAL_STRENGTH_PARAM getsignalstrengthparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	getsignalstrengthparam.id	=	id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetSignalStrength,
		&getsignalstrengthparam) == UTOPIA_STATUS_SUCCESS) {
		*strength = getsignalstrengthparam.strength;

		return TRUE;
	} else {
		return FALSE;
	}
}

enum DMD_ATSC_SIGNAL_CONDITION mdrv_dmd_atsc_md_getsignalquality(u8 id)
{
	ATSC_GET_SIGNAL_QUALITY_PARAM	getsignalqualityparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	getsignalqualityparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetSignalQuality,
		&getsignalqualityparam) == UTOPIA_STATUS_SUCCESS) {
		return getsignalqualityparam.equality;
	} else {
		return DMD_ATSC_SIGNAL_NO;
	}
}

u8 mdrv_dmd_atsc_md_getsnrpercentage(u8 id)
{
	ATSC_GET_SNR_PERCENTAGE_PARAM	getsnrpercentageparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	getsnrpercentageparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetSNRPercentage,
		&getsnrpercentageparam == UTOPIA_STATUS_SUCCESS))
		return getsnrpercentageparam.percentage;
	else
		return 0;

}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_md_get_qam_snr(u8 id, float *f_snr)
#else
u8 mdrv_dmd_atsc_md_get_qam_snr(u8 id, struct DMD_ATSC_SNR_DATA *f_snr)
#endif
{
	ATSC_GET_SNR_PARAM getsnrparam = {0};

	if (!_atscopen)
		return	FALSE;

	getsnrparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GET_QAM_SNR,
		&getsnrparam) == UTOPIA_STATUS_SUCCESS) {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		*f_snr = (float)getsnrparam.snr.noisepower/100;
		#else
		f_snr->noisepower	=	getsnrparam.snr.noisepower;
		f_snr->sym_num	=	getsnrparam.snr.sym_num;
		#endif

		return TRUE;

	} else {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		*f_snr = 0;
		#else
		f_snr->noisepower	=	0;
		f_snr->sym_num	=	0;
		#endif

		return FALSE;
	}
}

u8 mdrv_dmd_atsc_md_read_ucpkt_err(u8 id, u16 *packeterr)
{
	ATSC_GET_UCPKT_ERR_PARAM getucpkt_errparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	getucpkt_errparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_Read_uCPKT_ERR,
		&getucpkt_errparam) == UTOPIA_STATUS_SUCCESS) {
		*packeterr = getucpkt_errparam.packeterr;

		return TRUE;
	} else {
		return FALSE;
	}
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_md_getpreviterbiber(u8 id, float *ber)
#else
u8 mdrv_dmd_atsc_md_getpreviterbiber(u8 id,
struct DMD_ATSC_BER_DATA *ber)
#endif
{
	ATSC_GET_BER_PARAM getberparam = {0};

	if (!_atscopen)
		return	FALSE;

	getberparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetPreViterbiBer,
		&getberparam) == UTOPIA_STATUS_SUCCESS) {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		*ber = 0;
		#else
		ber->biterr		=	0;
		ber->error_window	=	0;
		ber->win_unit		=	0;
		#endif

		return TRUE;

	} else {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		*ber = 0;
		#else
		ber->biterr		=	0;
		ber->error_window	=	0;
		ber->win_unit		=	0;
		#endif

		return FALSE;
	}
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_md_getpostviterbiber(u8 id, float *ber)
#else
u8 mdrv_dmd_atsc_md_getpostviterbiber(u8 id,
struct DMD_ATSC_BER_DATA *ber)
#endif
{
	ATSC_GET_BER_PARAM getberparam = {0};

	if (!_atscopen)
		return	FALSE;

	getberparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetPostViterbiBer,
		&getberparam) == UTOPIA_STATUS_SUCCESS) {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		if (!getberparam.ber.biterr &&
		!getberparam.ber.error_window && !getberparam.ber.win_unit)
			*ber = 0;
		else if	(getberparam.ber.biterr	<= 0)
			*ber = 0.5f / ((float)(getberparam.ber.error_window)*(
		getberparam.ber.win_unit));
		else
			*ber = (float)(getberparam.ber.biterr) / ((float)(
		getberparam.ber.error_window)*(getberparam.ber.win_unit));
		#else
		ber->biterr		=	getberparam.ber.biterr;
		ber->error_window	=	getberparam.ber.error_window;
		ber->win_unit		=	getberparam.ber.win_unit;
		#endif

		return TRUE;

	} else {
		#ifndef	MSOS_TYPE_LINUX_KERNEL
		*ber = 0;
		#else
		ber->biterr		=	0;
		ber->error_window	=	0;
		ber->win_unit		=	0;
		#endif

		return FALSE;
	}
}

#ifndef	MSOS_TYPE_LINUX_KERNEL
u8 mdrv_dmd_atsc_md_readfrequencyoffset(u8 id, s16 *cfo)
#else
u8 mdrv_dmd_atsc_md_readfrequencyoffset(u8 id, DMD_ATSC_CFO_DATA *cfo)
#endif
{
	ATSC_READ_FREQ_OFFSET_PARAM readfreq_offsetparam = {0};

	if (!_atscopen)
		return FALSE;

	readfreq_offsetparam.id = id;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_ReadFrequencyOffset,
		&readfreq_offsetparam) == UTOPIA_STATUS_SUCCESS) {
		#ifndef MSOS_TYPE_LINUX_KERNEL
		*cfo = readfreq_offsetparam.cfo.rate;
		#else
		cfo->mode = readfreq_offsetparam.cfo.mode;
		cfo->ff   = readfreq_offsetparam.cfo.ff;
		cfo->rate = readfreq_offsetparam.cfo.rate;
		#endif

		return TRUE;

	} else {
		#ifndef MSOS_TYPE_LINUX_KERNEL
		*cfo = 0;
		#else
		cfo->mode = 0;
		cfo->ff   = 0;
		cfo->rate = 0;
		#endif

		return FALSE;
	}
}

u8 mdrv_dmd_atsc_md_setserialcontrol(u8 id, u8 tsconfig_data)
{
	ATSC_SET_SERIAL_CONTROL_PARAM	setserial_controlparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	setserial_controlparam.id = id;
	setserial_controlparam.tsconfig_data = tsconfig_data;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SetSerialControl,
		&setserial_controlparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_iic_bypass_mode(u8 id, u8 benable)
{
	ATSC_IIC_BYPASS_MODE_PARAM iicbypass_modeparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	iicbypass_modeparam.id	=	id;
	iicbypass_modeparam.benable = benable;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_IIC_BYPASS_MODE,
		&iicbypass_modeparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_switch_sspi_gpio(u8 id, u8 benable)
{
	ATSC_SWITCH_SSPI_GPIO_PARAM	switchsspi_gpioparam	=	{0};

	if (!_atscopen)
		return	FALSE;

	switchsspi_gpioparam.id = id;
	switchsspi_gpioparam.benable = benable;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SWITCH_SSPI_GPIO,
		&switchsspi_gpioparam) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_gpio_get_level(u8 id, u8 pin_8, u8 *blevel)
{
	ATSC_GPIO_LEVEL_PARAM	gpioLevel_param = {0};

	if (!_atscopen)
		return	FALSE;

	gpioLevel_param.id	=	id;
	gpioLevel_param.pin_8 = pin_8;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GPIO_GET_LEVEL,
		&gpioLevel_param) == UTOPIA_STATUS_SUCCESS) {
		*blevel	=	gpioLevel_param.blevel;

		return TRUE;
	} else {
		return FALSE;
	}
}

u8 mdrv_dmd_atsc_md_gpio_set_level(u8 id, u8 pin_8, u8 blevel)
{
	ATSC_GPIO_LEVEL_PARAM gpioLevel_param = {0};

	if (!_atscopen)
		return	FALSE;

	gpioLevel_param.id = id;
	gpioLevel_param.pin_8 = pin_8;
	gpioLevel_param.blevel = blevel;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GPIO_SET_LEVEL,
		&gpioLevel_param) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_gpio_out_enable(u8 id,
u8 pin_8, u8 benableout)
{
	ATSC_GPIO_OUT_ENABLE_PARAM gpiooutenable_param	=	{0};

	if (!_atscopen)
		return	FALSE;

	gpiooutenable_param.id	=	id;
	gpiooutenable_param.pin_8 = pin_8;
	gpiooutenable_param.benableout	=	benableout;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GPIO_OUT_ENABLE,
		&gpiooutenable_param) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_doiqswap(u8 id, u8 bis_qpad)
{
	ATSC_DO_IQ_SWAP_PARAM	doiqswap_param	=	{0};

	if (!_atscopen)
		return	FALSE;

	doiqswap_param.id = id;
	doiqswap_param.bis_qpad = bis_qpad;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_DoIQSwap,
		&doiqswap_param) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_getreg(u8 id, u16 addr_16, u8 *pdata_8)
{
	ATSC_REG_PARAM reg_param = {0};

	if (!_atscopen)
		return	FALSE;

	reg_param.id	=	id;
	reg_param.addr_16 = addr_16;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_GetReg,
		&reg_param) == UTOPIA_STATUS_SUCCESS) {
		*pdata_8 = reg_param.data_8;

		return TRUE;
	} else {
		return FALSE;
	}
}

u8 mdrv_dmd_atsc_md_setreg(u8 id, u16 addr_16, u8 data_8)
{
	ATSC_REG_PARAM reg_param	=	{0};

	if (!_atscopen)
		return	FALSE;

	reg_param.id = id;
	reg_param.addr_16 = addr_16;
	reg_param.data_8 = data_8;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SetReg,
		&reg_param) == UTOPIA_STATUS_SUCCESS)
		return TRUE;
	else
		return FALSE;

}

u8 mdrv_dmd_atsc_md_set_digrf_freq(u8 id, u32 rf_freq)
{
	ATSC_SET_DIGRF_PARAM DigRfParam	=	{0};

	if (!_atscopen)
		return	FALSE;

	DigRfParam.id	=	id;
	DigRfParam.rf_freq = rf_freq;

	if (UtopiaIoctl(_ppatscinstant, DMD_ATSC_DRV_CMD_MD_SET_DIGRF_FREQ,
		&DigRfParam) == UTOPIA_STATUS_SUCCESS) {
		return TRUE;
	} else {
		return FALSE;
	}
}

#endif // #if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN

#endif // #ifdef UTPA2