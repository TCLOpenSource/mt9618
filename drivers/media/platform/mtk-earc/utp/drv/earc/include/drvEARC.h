/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */



#ifndef _DRV_EARC_H_
#define _DRV_EARC_H_

#include "sti_msos.h"
#include "mtk_earc.h"

#ifdef __cplusplus
extern "C" {
#endif


//-------------------------------------------------------------------------------------------------
//  Driver Capability
//-------------------------------------------------------------------------------------------------
#define MSTAR   1
#define STELLAR 2
#define CUSTOMER MSTAR

#if (CUSTOMER == MSTAR)
#define MSTAR_STR
#define MSTAR_FLAG
#elif (CUSTOMER == STELLAR)
#define STELLAR_STR
#define STELLAR_FLAG
#endif
//-------------------------------------------------------------------------------------------------
//  Macro and Define
//-------------------------------------------------------------------------------------------------
#define EARC_DRV_LABEL "[eARC] "

#define LOG_LEVEL_MASK			0xFF
#define LOG_LEVEL_EMERGENCY		0
#define LOG_LEVEL_ALERT			1
#define LOG_LEVEL_CRITICAL		2
#define LOG_LEVEL_ERROR			3
#define LOG_LEVEL_WARNING		4
#define LOG_LEVEL_NOTICE		5
#define LOG_LEVEL_INFO			6
#define LOG_LEVEL_DEBUG			7
#define LOG_LEVEL_MAX			8


#define EARC_LOG(_dbgLevel_, _fmt, _args...) \
do { \
	switch (_dbgLevel_) { \
	case LOG_LEVEL_EMERGENCY: \
			pr_emerg(EARC_DRV_LABEL "[Emerg] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_ALERT: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_ALERT)  \
			pr_emerg(EARC_DRV_LABEL "[Alert] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
		else \
			pr_alert(EARC_DRV_LABEL "[Alert] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_CRITICAL: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_CRITICAL)  \
			pr_emerg(EARC_DRV_LABEL "[Crit] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_crit(EARC_DRV_LABEL "[Crit] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_ERROR: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_ERROR)  \
			pr_emerg(EARC_DRV_LABEL "[Error] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_err(EARC_DRV_LABEL "[Error] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_WARNING: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_WARNING)  \
			pr_emerg(EARC_DRV_LABEL "[Warning] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_warn(EARC_DRV_LABEL "[Warning] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_NOTICE: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_NOTICE)  \
			pr_emerg(EARC_DRV_LABEL "[Notice] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_notice(EARC_DRV_LABEL "[Notice] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	case LOG_LEVEL_INFO: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_INFO)  \
			pr_emerg(EARC_DRV_LABEL "[Info] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_info(EARC_DRV_LABEL "[Info] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
		break; \
	case LOG_LEVEL_DEBUG: \
		if ((mdrv_EARC_Get_LogLevel() & LOG_LEVEL_MASK) >= LOG_LEVEL_DEBUG)  \
			pr_emerg(EARC_DRV_LABEL "[Debug] [%s,%d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else \
			pr_debug(EARC_DRV_LABEL "[Debug] [%s,%d]"_fmt, \
				__func__, __LINE__, ##_args); \
	break; \
	default: \
	break; \
	} \
} while (0)

//The system is unusable
#define EARC_MSG_EMERG(format, args...) \
			EARC_LOG(LOG_LEVEL_EMERGENCY, format, ##args)
//Actions that must be tacken care of immediately
#define EARC_MSG_ALERT(format, args...) \
			EARC_LOG(LOG_LEVEL_ALERT, format, ##args)
//Critical conditions
#define EARC_MSG_CRIT(format, args...) \
			EARC_LOG(LOG_LEVEL_CRITICAL, format, ##args)
//Noncritical error conditions
#define EARC_MSG_ERROR(format, args...) \
			EARC_LOG(LOG_LEVEL_ERROR, format, ##args)
//Warning conditions that should be taken care of
#define EARC_MSG_WARNING(format, args...) \
			EARC_LOG(LOG_LEVEL_WARNING, format, ##args)
//Normal, but significant events
#define EARC_MSG_NOTICE(format, args...) \
			EARC_LOG(LOG_LEVEL_NOTICE, format, ##args)
//Informational messages that require no action
#define EARC_MSG_INFO(format, args...) \
			EARC_LOG(LOG_LEVEL_INFO, format, ##args)
//Kernel debugging messages, output by the kernel if the developer enabled debugging at compile time
#define EARC_MSG_DEBUG(format, args...) \
			EARC_LOG(LOG_LEVEL_DEBUG, format, ##args)


#define EARC_POLLING_INTERVAL			10U

#ifdef MSOS_TYPE_ECOS
#define HDMI_EARC_POLLING_STACK_SIZE	2048U
#else
#define HDMI_EARC_POLLING_STACK_SIZE	1U
#endif

#define HDMI_EARCTX_SELECT_ATOP_MODE    EARC_ATOP_MODE2

#define HDMI_EARCTX_READ_CAP_LEN_MAX    16U
#define HDMI_EARCTX_READ_CAP_LEN        2U
#define HDMI_EARCTX_CAP_LEN             256U
#define HDMI_EARCTX_STAT_LEN            1U
#define HDMI_EARCTX_RETRY_CNT           4U

#define HDMI_EARCTX_READ_ONLY_CAPBILITIES_DATA_STRUCTURE           0xA0U
#define HDMI_EARCTX_READ_WRITE_EARC_STATUS_AND_CONTROL_REGISTERS   0x74U

#define HDMI_EARCTX_OFFSET_D0    0xD0U
#define HDMI_EARCTX_OFFSET_D1    0xD1U
#define HDMI_EARCTX_OFFSET_D2    0xD2U
#define HDMI_EARCTX_OFFSET_D3    0xD3U


//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
//  Structure
//-------------------------------------------------------------------------------------------------

typedef struct {


	MS_BOOL bCapChangeFlag;
	MS_BOOL bStatChangeFlag;
	MS_BOOL bSetLatencyFlag;
	MS_BOOL bCommonModeCapBusyFlag;
	MS_BOOL bCommonModeStatBusyFlag;
	MS_BOOL bCommonModeSetLatencyBusyFlag;
	MS_BOOL bCapChangeUpdateDoneFlag;
	MS_BOOL bStatChangeUpdateDoneFlag;
	MS_U16 u16OPCodeOffset;
	MS_U16 u16CapLen;
	MS_U8 u8OPCodeDeviceID;
	MS_U8 u8TranState;
	MS_U8 u8TranResult;
	MS_U8 u8TransactioinRetryCnt;
	MS_U8 u8TransactionDataThreshold;
	MS_U8 u8CapContent[HDMI_EARCTX_CAP_LEN];
	MS_U8 u8LatencyContent;
	MS_U8 u8SetLatencyValue;
	MS_U8 u8CapabilityChangeCnt;
} stHDMI_EARCTX_COMMON_MODE_POLLING_INFO;


struct HDMI_EARCTX_RESOURCE_PRIVATE {
	MS_U8 u8EarcPortSel;
	MS_BOOL bSelfCreateTaskFlag;
	MS_BOOL bEARCTaskProcFlag;
	MS_BOOL bEnable5VDetectInvert;
	MS_BOOL bEnableHPDInvert;
	MS_U8 u8EARCOutputPort;
	MS_U8 u8EARCTXConnectState;
	MS_U8 u8EARCSupportMode;
	MS_U8 u8EARCArcPin;
	MS_U8 ucEARCPollingStack[HDMI_EARC_POLLING_STACK_SIZE];
	MS_S32 slEARCPollingTaskID;
	EN_POWER_MODE usPrePowerState;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO stEarcCmPollingInfo;
	struct mtk_earc_hdmi_port_map map[HDMI_PORT_MAX_NUM];
};

enum EARC_ATOP_SELECT_MODE {
	EARC_ATOP_MODE1 = 1,
	EARC_ATOP_MODE2,
};

enum HDMI_EARCTX_COMMON_MODE_STATE {
	HDMI_EARCTX_CHECK_CAP_CHANGE,
	HDMI_EARCTX_CHECK_STA_CHANGE,
	HDMI_EARCTX_DO_COMMON_PROC,
	HDMI_EARCTX_CHECK_NONE,
};

enum HDMI_EARCTX_TRANSACTION_STATE {
	HDMI_EARCTX_SET_OP_CODE_INFO,
	HDMI_EARCTX_WAIT_RX_HEARTBEAT_STATUS,
	HDMI_EARCTX_SET_DATA_CONTENT,
	HDMI_EARCTX_FIFO_WRITE_PULSE,
	HDMI_EARCTX_READ_TRIGGER,
	HDMI_EARCTX_WRITE_TRIGGER,
	HDMI_EARCTX_WAIT_READ_TRANSACTION_DONE,
	HDMI_EARCTX_WAIT_WRITE_TRANSACTION_DONE,
	HDMI_EARCTX_READ_DATA_CONTENT,
	HDMI_EARCTX_TRANSACTION_FINISH,
	HDMI_EARCTX_TRANSACTION_NONE,
};

enum HDMI_EARCTX_TRANSACTION_RESULT_TYPE {
	HDMI_EARCTX_FREE,
	HDMI_EARCTX_FAIL,
	HDMI_EARCTX_ECC_ERROR,
	HDMI_EARCTX_RETRY_FORCE_STOP,
	HDMI_EARCTX_CONT_FORCE_STOP,
	HDMI_EARCTX_TRANSACTION_DONE,
};


typedef enum {
	EARC_CONNECTION_LOST_5V = 0,
	EARC_CONNECTION_ARC_MODE,
	EARC_CONNECTION_EARC_BUSY,
	EARC_CONNECTION_EARC_MODE,
	EARC_CONNECTION_NONE_MODE,
	EARC_CONNECTION_EARC_DISC1,
	EARC_CONNECTION_EARC_DISC2,
} EN_EARC_GET_CONNECT_STATUS;


typedef enum {
	SUPPORT_NONE = 0,
	SUPPORT_EARC,
	SUPPORT_ARC,
} EN_EARC_SUPPORT_MODE;

//-------------------------------------------------------------------------------------------------
//  Function and Variable
//-------------------------------------------------------------------------------------------------
MS_BOOL mdrv_EARC_Get_DifferentialOnOff(void);
MS_BOOL mdrv_EARC_Resume_Init(void);
MS_BOOL mdrv_EARC_Init(MS_U8 u8DefaultSupportMode);
MS_U8 mdrv_EARC_GetArcPin(void);
MS_U8 mdrv_EARC_Get_CommonOnOff(void);
MS_U8 mdrv_EARC_GetEarcPortSel(void);
MS_U8 mdrv_EARC_GetEarcUIPortSel(void);
MS_U8 mdrv_EARC_GetCapibility(MS_U8 u8Index);
MS_U8 mdrv_EARC_GetLatency(void);
MS_U8 mdrv_EARC_GetConnectState(void);
MS_U8 mdrv_EARC_GetCapibilityChange(void);
MS_U8 mdrv_EARC_GetLatencyChange(void);
MS_U8 mdrv_EARC_Get_DifferentialDriveStrength(void);
MS_U8 mdrv_EARC_Get_DifferentialSkew(void);
MS_U8 mdrv_EARC_Get_CommonDriveStrength(void);
MS_U8 mdrv_EARC_Get_DiscState(void);
MS_U8 mdrv_EARC_Get_SupportMode(void);
MS_U8 mdrv_EARC_Get_LogLevel(void);
MS_U8 mdrv_EARC_Get_Stub(bool *bstub);
MS_U8 mdrv_EARC_Set_Stub(bool bEnable);
void mdrv_EARC_SetRIUAddr(MS_U32 u32P_Addr, MS_U32 u32P_Size, MS_U64 u64V_Addr);
void mdrv_EARC_HWInit(MS_U32 Para1, MS_U32 Para2);
void mdrv_EARC_SetArcPin(MS_U8 u8ArcPinEnable);
void mdrv_EARC_SetCapibilityChange(void);
void mdrv_EARC_SetCapibilityChangeClear(void);
void mdrv_EARC_SetLatencyChange(void);
void mdrv_EARC_SetLatencyChangeClear(void);
void mdrv_EARC_SetEarcPort(MS_U8 u8EarcPort, MS_BOOL bEnable5VDetectInvert,
			   MS_BOOL bEnableHPDInvert);
void mdrv_EARC_SetEarcSupportMode(MS_U8 u8EarcSupportMode);
void mdrv_EARC_SetLatencyInfo(MS_U8 u8LatencyValue);
void mdrv_EARC_Set_HeartbeatStatus(MS_U8 u8HeartbeatStatus,
	MS_BOOL bOverWriteEnable, MS_BOOL bSetValue);
void mdrv_EARC_Set_DifferentialDriveStrength(MS_U8 u8Level);
void mdrv_EARC_Set_DifferentialSkew(MS_U8 u8Level);
void mdrv_EARC_Set_CommonDriveStrength(MS_U8 u8Level);
void mdrv_EARC_Set_DifferentialOnOff(MS_BOOL bOnOff);
void mdrv_EARC_Set_CommonOnOff(MS_BOOL bOnOff);
void mdrv_EARC_Get_Info(struct HDMI_EARCTX_RESOURCE_PRIVATE *pEarcInfo);
void mdrv_EARC_Set_LogLevel(MS_U8 loglevel);
void mdrv_EARC_Set_HDMI_Port_Map(struct mtk_earc_hdmi_port_map *map);

#endif				// _DRV_EARC_H_
