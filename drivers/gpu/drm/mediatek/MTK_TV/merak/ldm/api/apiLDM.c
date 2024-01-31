// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-------------------------------------------------------------------------------------------------
//  Include Files
//-------------------------------------------------------------------------------------------------
#if !defined(MSOS_TYPE_LINUX_KERNEL)
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#else
#include <linux/string.h>
#include <linux/slab.h>
#endif


#include "apiLDM.h"



//-------------------------------------------------------------------------------------------------
//  Local Compiler Options
//-------------------------------------------------------------------------------------------------
////remove-start trunk ld need refactor
//int MApi_LDM_Enable(void)
//{
//	int ret = MDrv_LDM_Enable();
//
//	return ret;
//}
//int MApi_LDM_Disable(__u8 u8luma)
//{
//	int ret = MDrv_LDM_Disable(u8luma);
//
//	return ret;
//}
//

////remove-end trunk ld need refactor
int MApi_ld_Init_Check_SPI_Num(__u8 u8LDM_Version)
{
	int ret = MDrv_LDM_Init_Check_SPI_Num(u8LDM_Version);

	return ret;
}

int MApi_ld_Init_Check_En(__u8 u8LDM_Version)
{
	int ret = MDrv_LDM_Init_Check_En(u8LDM_Version);

	return ret;
}
bool MApi_ld_Check_Tgen_Changed(__u8 u8LDM_Version)
{
	bool ret = MDrv_LDM_Check_Tgen_Changed(u8LDM_Version);

	return ret;
}

bool MApi_ld_Get_En_Status(__u8 u8LDM_Version)
{
	bool ret = MDrv_LDM_Get_En_Status(u8LDM_Version);

	return ret;
}
void MApi_ld_set_spi_self_trig_off(__u8 u8LDM_Version)
{
	MDrv_LDM_set_spi_self_trig_off(u8LDM_Version);
}

void MApi_ld_set_GD_strength(uint8_t u8ldm_version,uint8_t u8value)
{
    Mdrv_ldm_set_GD_strength(u8ldm_version, u8value);
}



void MApi_ld_set_spi_ready_flag(__u8 u8value)
{
	Mdrv_ldm_set_spi_ready_flag(E_LDM_HW_VERSION_5, u8value);
}

bool MApi_ld_get_spi_ready_flag(void)
{
	bool ret = Mdrv_ldm_get_spi_ready_flag(E_LDM_HW_VERSION_5);

	return ret;
}


int MApi_ld_Check_Reg_Sysfs(__u8 u8LDM_Version)
{
	int ret = MDrv_LDM_Check_Reg_Sysfs(u8LDM_Version);

	return ret;
}

int MApi_ld_Init(struct mtk_tv_kms_context *pctx, struct LD_CUS_PATH *Ldm_CusPath)
{
	int ret = MDrv_LDM_Init(pctx, Ldm_CusPath);

	return ret;
}

int MApi_ld_Suspend(__u8 u8LDM_Version, struct mtk_tv_kms_context *pctx)
{
	int ret = MDrv_LDM_Suspend(u8LDM_Version, pctx);

	return ret;
}

int MApi_ld_Resume(struct mtk_tv_kms_context *pctx)
{
	int ret = MDrv_LDM_Resume(pctx);

	return ret;
}

int MApi_ld_Init_to_Mailbox(void)
{
	int ret = MDrv_LDM_Init_to_Mailbox();

	return ret;
}


int MApi_ld_Init_to_CPUIF(void)
{
	int ret = MDrv_LDM_Init_to_CPUIF();

	return ret;
}

//for sony_ip
int MApi_ld_Init_SetLDC(__u32 u32SupportLdmType)
{
	int ret = MDrv_LDM_Init_SetLDC(u32SupportLdmType);

	return ret;
}

int MApi_ld_Init_SetSwSetCtrl(
	struct hwregSWSetCtrlIn *pstDrvSwSetCtrl)
{
	int ret = MDrv_LDM_Init_SetSwSetCtrl(pstDrvSwSetCtrl);

	return ret;
}

int MApi_ld_Init_UpdateProfile(
	__u64 phyAddr,
	struct mtk_tv_kms_context *pctx)
{
	int ret = MDrv_LDM_Init_updateprofile(phyAddr, pctx);

	return ret;
}

////remove-start trunk ld need refactor
//int MApi_LDM_SetStrength(__u8 u8Percent)
//{
//	int ret = MDrv_LDM_SetStrength(u8Percent);
//
//	return ret;
//}
//uint8_t MApi_LDM_GetStatus(void)
//{
//	uint8_t ret = MDrv_LDM_GetStatus();
//
//	return ret;
//}
//
//int MApi_LDM_SetDemoPattern(__u32 u32demopattern)
//{
//
//	ST_LDM_DEMO_PATTERN stDrvPattern = {0};
//	__u16 u16DemoPatternCtrl = (u32demopattern & 0xFFFF0000)>>16;
//	__u16 u16DemoLEDNum = (u32demopattern & 0x0000FFFF);
//	__u8 ret = 0;
//
//	switch (u16DemoPatternCtrl) {
//	default:
//	case E_LDM_DEMO_PATTERN_CTRL_OFF:
//	{
//		stDrvPattern.bOn = false;
//		stDrvPattern.enDemoPattern = E_LDM_DEMO_PATTERN_SWITCH_SINGLE_LED;
//		stDrvPattern.u16LEDNum = u16DemoLEDNum;
//
//		break;
//	}
//	case E_LDM_DEMO_PATTERN_CTRL_SINGLE:
//	{
//		stDrvPattern.bOn = true;
//		stDrvPattern.enDemoPattern = E_LDM_DEMO_PATTERN_SWITCH_SINGLE_LED;
//		stDrvPattern.u16LEDNum = u16DemoLEDNum;
//		break;
//	}
//	case E_LDM_DEMO_PATTERN_CTRL_MARQUEE:
//	{
//		stDrvPattern.bOn = true;
//		stDrvPattern.enDemoPattern = E_LDM_DEMO_PATTERN_MARQUEE;
//		stDrvPattern.u16LEDNum = u16DemoLEDNum;
//		break;
//	}
//	}
//
//	ret = MDrv_LDM_DemoPattern(stDrvPattern);
//
//	return ret;
//}
//
//int MApi_LDM_GetData(ST_LDM_GET_DATA *pstDrvData)
//{
//	int ret = MDrv_LDM_GetData(pstDrvData);
//
//	return ret;
//}
////remove-end trunk ld need refactor


