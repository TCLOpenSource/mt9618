/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _API_LDM_H_
#define _API_LDM_H_



#ifdef __cplusplus
extern "C"
{
#endif
// #include "MsDevice.h"
//#include "apiGOP_v2.h"
#include "drvLDM_sti.h"

////remove-start trunk ld need refactor
//typedef enum {
//	E_LDM_DEMO_PATTERN_CTRL_OFF = 0x00,
//	E_LDM_DEMO_PATTERN_CTRL_SINGLE = 0x01,
//	E_LDM_DEMO_PATTERN_CTRL_MARQUEE = 0x02,
//} EN_LDM_DEMO_PATTERN_CTRL;
////remove-end trunk ld need refactor


//int MApi_LDM_Enable(void);
//int MApi_LDM_Disable(__u8 u8luma);
int MApi_ld_Check_Reg_Sysfs(__u8 u8LDM_Version);
int MApi_ld_Init_Check_SPI_Num(__u8 u8LDM_Version);
int MApi_ld_Init_Check_En(__u8 u8LDM_Version);
bool MApi_ld_Get_En_Status(__u8 u8LDM_Version);
void MApi_ld_set_spi_self_trig_off(__u8 u8LDM_Version);
bool MApi_ld_Check_Tgen_Changed(__u8 u8LDM_Version);
void MApi_ld_set_GD_strength(uint8_t u8ldm_version,uint8_t u8value);
void MApi_ld_set_spi_ready_flag(__u8 u8value);
bool MApi_ld_get_spi_ready_flag(void);


int MApi_ld_Init(struct mtk_tv_kms_context *pctx, struct LD_CUS_PATH *Ldm_CusPath);
int MApi_ld_Suspend(__u8 u8LDM_Version, struct mtk_tv_kms_context *pctx);
int MApi_ld_Resume(struct mtk_tv_kms_context *pctx);
int MApi_ld_Init_to_Mailbox(void);
int MApi_ld_Init_to_CPUIF(void);
int MApi_ld_Init_SetLDC(__u32 u32SupportLdmType);
int MApi_ld_Init_SetSwSetCtrl(struct hwregSWSetCtrlIn *pstDrvSwSetCtrl);
int MApi_ld_Init_UpdateProfile(
	__u64 phyAddr,
	struct mtk_tv_kms_context *pctx);


//int MApi_LDM_SetStrength(__u8 u8Percent);
//uint8_t MApi_LDM_GetStatus(void);
//int MApi_LDM_SetDemoPattern(__u32 u32demopattern);
//int MApi_LDM_GetData(ST_LDM_GET_DATA *pstDrvData);



#ifdef __cplusplus
}
#endif

#endif // _API_GOP_H_

