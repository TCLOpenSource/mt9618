/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DRV_LDM_STI_H_
#define _DRV_LDM_STI_H_






//#include "sti_msos.h"
//#include "sti_msos_ldm.h"

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include "hwreg_render_video_globalpq.h"
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include "../../mtk_tv_drm_kms.h"
#include "mtk_tv_drm_ldm_leddevice.h"





#ifdef __cplusplus
extern "C"
{
#endif



#ifdef MDRV_LDM_C
#define MDRV_INTERFACE
#else
#define MDRV_INTERFACE extern
#endif

#undef printf
#define printf printk

#define MAX_PATH_LEN	128
#define MAX_BIN_INFO_NAME_LEN	14

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#endif

enum EN_LDM_HW_VERSION {
	E_LDM_HW_VERSION_1 = 1,
	E_LDM_HW_VERSION_2,
	E_LDM_HW_VERSION_3,
	E_LDM_HW_VERSION_4,
	E_LDM_HW_VERSION_5,
	E_LDM_HW_VERSION_6,
};

enum EN_LDM_SUPPORT {
	E_LDM_UNSUPPORT = 0,
	E_LDM_SUPPORT_TRUNK_PQU,
	E_LDM_SUPPORT_CUS,
	E_LDM_SUPPORT_TRUNK_FRC,
	E_LDM_SUPPORT_BE,
	E_LDM_SUPPORT_R2,
};

enum EN_SYSFS_LDM_CMD {
	E_LDM_CMD_HOST_INIT = 1,
	E_LDM_CMD_R2_INIT,
	E_LDM_CMD_SUSPEND,
	E_LDM_CMD_RESUME,
	E_LDM_CMD_BYPASS,
	E_LDM_CMD_CUS_INIT,
	E_LDM_CMD_MAX
};

enum EN_LDM_STATUS {
	E_LDM_STATUS_INIT = 1,
	E_LDM_STATUS_ENABLE = 2,
	E_LDM_STATUS_DISNABLE,
	E_LDM_STATUS_SUSPEND,
	E_LDM_STATUS_RESUME,
	E_LDM_STATUS_DEINIT,
};

enum EN_LDM_RETURN {
	E_LDM_RET_SUCCESS = 0,
	E_LDM_RET_FAIL = 1,
	E_LDM_RET_NOT_SUPPORTED,
	E_LDM_RET_PARAMETER_ERROR,
	E_LDM_RET_OBTAIN_MUTEX_FAIL,
};


enum EN_LDM_DEBUG_LEVEL {
	E_LDM_DEBUG_LEVEL_ERROR = 0x01,
	E_LDM_DEBUG_LEVEL_WARNING = 0x02,
	E_LDM_DEBUG_LEVEL_INFO = 0x04,
	E_LDM_DEBUG_LEVEL_ALL = 0x07,
	E_LDM_DEBUG_LEVEL_MAX
};

struct LD_CUS_PATH {
	char aCusPath[MAX_PATH_LEN];
	char aCusPathU[MAX_PATH_LEN];
};

struct ConfigFile_t {
	size_t size;
	char *p;
	char *name;
};

struct LDM_BIN_HEADER {
	char abin_info[MAX_PATH_LEN];//char abin_info[MAX_BIN_INFO_NAME_LEN];
	__u8 u8version1;
	__u8 u8version2;
};

enum EN_LDM_GET_DATA_TYPE {
	E_LDM_DATA_TYPE_LDF = 0x01,
	E_LDM_DATA_TYPE_LDB = 0x02,
	E_LDM_DATA_TYPE_SPI = 0x03,
	E_LDM_DATA_TYPE_HISTO = 0x04,
	E_LDM_DATA_TYPE_LD_PARA = 0x05,
	E_LDM_DATA_TYPE_MAX = E_LDM_DATA_TYPE_LD_PARA,
};

enum EN_LDM_BIN_TYPE {
	E_LDM_BIN_TYPE_COMPENSATION,
	E_LDM_BIN_TYPE_BACKLIGHTGAMMA,
	E_LDM_BIN_TYPE_EDGE2D,
	E_LDM_BIN_TYPE_MAX,
};


////remove-start trunk ld need refactor
//
//typedef enum {
//	E_LDM_DEMO_PATTERN_SWITCH_SINGLE_LED = 0x00,
//	E_LDM_DEMO_PATTERN_LEFT_RIGHT_HALF = 0x01,
//	E_LDM_DEMO_PATTERN_MARQUEE = 0x02,
//	E_LDM_DEMO_PATTERN_LEFT_RIGHT_COLOR_SHELTER = 0x03,
//	E_LDM_DEMO_PATTERN_MAX
//} EN_LDM_DEMO_PATTERN;
//
//
//typedef struct {
//	EN_LDM_GET_DATA_TYPE enDataType;
//	u64 phyAddr;
//} ST_LDM_GET_DATA;
//
//typedef struct {
//	bool bOn;
//	EN_LDM_DEMO_PATTERN enDemoPattern;
//	u16 u16LEDNum;
//} ST_LDM_DEMO_PATTERN;
////remove-end trunk ld need refactor


//----------------------------------------------------------------
// MDrv_LDM_Enable - start local dimming algorithm
/// @ingroup G_LDM_CONTROL
// @param: na
// @return: E_LDM_RET_SUCCESS is enable
//----------------------------------------------------------------
//u8 MDrv_LDM_GetStatus(void);
//u8 MDrv_LDM_Enable(void);
//u8 MDrv_LDM_Disable(u8 u8Lumma);
//u8 MDrv_LDM_Init(u64 phyAddr);
__u8 MDrv_LDM_Check_Reg_Sysfs(__u8 u8LDM_Version);
__u8 MDrv_LDM_Init_Check_SPI_Num(__u8 u8LDM_Version);
__u8 MDrv_LDM_Init_Check_En(__u8 u8LDM_Version);
bool MDrv_LDM_Get_En_Status(uint8_t u8ldm_version);
void MDrv_LDM_set_spi_self_trig_off(__u8 u8ldm_version);
bool MDrv_LDM_Check_Tgen_Changed(uint8_t u8ldm_version);
void Mdrv_ldm_set_GD_strength(uint8_t u8ldm_version,uint8_t u8value);
void Mdrv_ldm_set_spi_ready_flag(uint8_t u8ldm_version, uint8_t u8value);
bool Mdrv_ldm_get_spi_ready_flag(uint8_t u8ldm_version);

__u8 MDrv_LDM_Init(struct mtk_tv_kms_context *pctx, struct LD_CUS_PATH *Ldm_CusPath);
__u8 MDrv_LDM_Suspend(__u8 u8LDM_Version, struct mtk_tv_kms_context *pctx);
__u8 MDrv_LDM_Resume(struct mtk_tv_kms_context *pctx);
__u8 MDrv_LDM_Init_to_Mailbox(void);
__u8 MDrv_LDM_Init_to_CPUIF(void);
u8 MDrv_LDM_Init_SetLDC(u32 u32LdmType);
u8 MDrv_LDM_Init_SetSwSetCtrl(struct hwregSWSetCtrlIn *stSwSetCtrl);
u8 MDrv_LDM_Init_updateprofile(
	u64 pa,
	struct mtk_tv_kms_context *pctx);
char *MDrv_LDM_Get_BIN_Name(enum EN_LDM_BIN_TYPE eType);
__u8 MDrv_LDM_Get_BIN_Version1(enum EN_LDM_BIN_TYPE eType);
__u8 MDrv_LDM_Get_BIN_Version2(enum EN_LDM_BIN_TYPE eType);

//u8 MDrv_LDM_SetStrength(u8 u8Percent);
//u8 MDrv_LDM_DemoPattern(ST_LDM_DEMO_PATTERN stPattern);
//u8 MDrv_LDM_GetData(ST_LDM_GET_DATA *stData);


#ifdef __cplusplus
}
#endif


#endif // _DRV_LDM_H_
