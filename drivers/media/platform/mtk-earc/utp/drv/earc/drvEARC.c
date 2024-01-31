// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


//-------------------------------------------------------------------------------------------------
//  Include Files
//-------------------------------------------------------------------------------------------------
// Common Definition
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>

#include "drvEARC.h"
#include "mtk_earc.h"
#include "hwreg_earc_common.h"

//-------------------------------------------------------------------------------------------------
//  definition
//-------------------------------------------------------------------------------------------------
static struct HDMI_EARCTX_RESOURCE_PRIVATE *pEarcRes;
static struct task_struct *stEarcThread;
static EN_EARC_GET_CONNECT_STATUS u8PreConStus = EARC_CONNECTION_NONE_MODE;

#if (defined(SEL_MT5876) || defined(SEL_MT5879))
#define EARC_PATH_MODE 1 //1: old mode, 0: new path mode
#else
#define EARC_PATH_MODE 0
#endif
//-------------------------------------------------------------------------------------------------
//  Global variables
//-------------------------------------------------------------------------------------------------

EN_EARC_GET_CONNECT_STATUS _mdrv_EARC_TX_GetConnectState(void)
{
	EN_EARC_GET_CONNECT_STATUS u8ReValue = EARC_CONNECTION_LOST_5V;
	MS_U16 u16RegisterValue = 0;

	u16RegisterValue = HWREG_EARC_TX_GetDiscState();	// earc_tx_30[4:0]: reg_disc_state

	if (u16RegisterValue & BIT(0))
		u8ReValue = EARC_CONNECTION_LOST_5V;

	if (pEarcRes->u8EARCSupportMode == SUPPORT_NONE)
		u8ReValue = EARC_CONNECTION_NONE_MODE;

	if (u16RegisterValue & BIT(1))
		u8ReValue = EARC_CONNECTION_ARC_MODE;

	if (u16RegisterValue & BIT(2))
		u8ReValue = EARC_CONNECTION_EARC_DISC1;

	if (u16RegisterValue & BIT(3))
		u8ReValue = EARC_CONNECTION_EARC_DISC2;

	if (u16RegisterValue & BIT(4))
		u8ReValue = EARC_CONNECTION_EARC_MODE;

	return u8ReValue;
}

void _mdrv_EARC_TX_SetPacketInfo(void)
{
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	HWREG_EARC_TX_SetOpCodeDeviceId(
		pPollingInfo->u8OPCodeDeviceID);
	HWREG_EARC_TX_SetOpCodeOffset(
		pPollingInfo->u16OPCodeOffset);
	HWREG_EARC_TX_SetTransactionDataThreshold(
		pPollingInfo->u8TransactionDataThreshold);
}

void _mdrv_EARC_TX_HeartBeatReadDataTrigger(void)
{
	HWREG_EARC_TX_SetReadTrig(TRUE);
}

void _mdrv_EARC_TX_HeartBeatWriteDataTrigger(MS_BOOL bFIFOWriteTriFlag)
{
	if (bFIFOWriteTriFlag)
		HWREG_EARC_TX_SetPacketTxFifoWritePulse(TRUE);
	else
		HWREG_EARC_TX_SetWriteTrig(TRUE);
}

MS_BOOL _mdrv_EARC_TX_GetRxHeartBeatStatus(void)
{
	MS_BOOL ReValue = TRUE;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (pPollingInfo->bCommonModeCapBusyFlag) {
		if (HWREG_EARC_TX_GetHeartBeatStatus() & BIT(3))
			ReValue = TRUE;
		else
			ReValue = FALSE;
	} else if (pPollingInfo->bCommonModeStatBusyFlag) {
		if (HWREG_EARC_TX_GetHeartBeatStatus() & BIT(4))
			ReValue = TRUE;
		else
			ReValue = FALSE;
	}

	return ReValue;
}

MS_BOOL _mdrv_EARC_TX_GetReadTransactionResult(MS_BOOL bResetIRQFlag)
{
	MS_BOOL bReValue = FALSE;
	MS_U16 u16RegisterValue = 0;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (bResetIRQFlag) {
		HWREG_EARC_TX_SetSetIrqClear0(0xFF);
		HWREG_EARC_TX_SetSetIrqClear1(0xFF);
	} else {
		u16RegisterValue = HWREG_EARC_TX_GetIrqStatus1();

		if (u16RegisterValue & BIT(3)) {
			bReValue = TRUE;
			pPollingInfo->u8TranResult = HDMI_EARCTX_TRANSACTION_DONE;
		} else if (u16RegisterValue & BIT(4)) {
			pPollingInfo->u8TranResult = HDMI_EARCTX_RETRY_FORCE_STOP;
		} else if (u16RegisterValue & BIT(5)) {
			pPollingInfo->u8TranResult = HDMI_EARCTX_CONT_FORCE_STOP;
		} else if (u16RegisterValue & BIT(13)) {
			pPollingInfo->u8TranResult = HDMI_EARCTX_ECC_ERROR;
		} else {
			pPollingInfo->u8TranResult = HDMI_EARCTX_FAIL;
		}
	}

	return bReValue;
}

MS_BOOL _mdrv_EARC_TX_GetWriteTransactionResult(MS_BOOL bResetIRQFlag)
{
	MS_BOOL bReValue = FALSE;
	MS_U16 u16RegisterValue = 0;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (bResetIRQFlag) {
		HWREG_EARC_TX_SetSetIrqClear0(0xFF);
		HWREG_EARC_TX_SetSetIrqClear1(0xFF);
	} else {
		u16RegisterValue = HWREG_EARC_TX_GetIrqStatus0();

		if (u16RegisterValue & BIT(11)) {
			bReValue = TRUE;
			pPollingInfo->u8TranResult = HDMI_EARCTX_TRANSACTION_DONE;
		} else if (u16RegisterValue & BIT(12)) {
			pPollingInfo->u8TranResult = HDMI_EARCTX_RETRY_FORCE_STOP;
		} else if (HWREG_EARC_TX_GetIrqStatus1() & BIT(2)) {
			pPollingInfo->u8TranResult = HDMI_EARCTX_ECC_ERROR;
		} else {
			pPollingInfo->u8TranResult = HDMI_EARCTX_FAIL;
		}
	}

	return bReValue;
}

void _mdrv_EARC_TX_WriteDataContent(void)
{
	MS_U8 u8Temp = 0;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (pPollingInfo->bCommonModeSetLatencyBusyFlag) {
		for (u8Temp = 0;
		     u8Temp < pPollingInfo->u8TransactionDataThreshold;
		     u8Temp++)
			HWREG_EARC_TX_WriteByteCommand(u8Temp,
			       pPollingInfo->u8SetLatencyValue);
	}
}

void _mdrv_EARC_TX_ReadDataContent(void)
{
	MS_U8 u8Temp = 0;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (pPollingInfo->bCapChangeFlag) {
		for (u8Temp = 0;
			 u8Temp < pPollingInfo->u8TransactionDataThreshold;
		     u8Temp++)
			pPollingInfo->u8CapContent
			[pPollingInfo->u16OPCodeOffset + u8Temp] =
			HWREG_EARC_TX_ReadByteCommand(u8Temp);
	} else if (pPollingInfo->bStatChangeFlag) {
		pPollingInfo->u8LatencyContent = HWREG_EARC_TX_ReadByteCommand(0);
	}
}
void _mdrv_EARC_TX_HBSeqCheckCapChange(void)
{
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (HWREG_EARC_TX_GetCAPChangeFlag(FALSE)) {
		HWREG_EARC_TX_GetCAPChangeFlag(TRUE);

		pPollingInfo->bCapChangeFlag = TRUE;
		pPollingInfo->bCapChangeUpdateDoneFlag = FALSE;
		pPollingInfo->bCommonModeCapBusyFlag = FALSE;
		EARC_MSG_INFO("Get Capability Change\r\n");
	}
}

void _mdrv_EARC_TX_HBSeqCheckSTATChange(void)
{
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (HWREG_EARC_TX_GetSTATChangeFlag(FALSE)) {
		HWREG_EARC_TX_GetSTATChangeFlag(TRUE);

		pPollingInfo->bStatChangeFlag = TRUE;
		pPollingInfo->bStatChangeUpdateDoneFlag = FALSE;
		pPollingInfo->bCommonModeStatBusyFlag = FALSE;
		EARC_MSG_INFO("Get STAT Change\r\n");
	}
}

void _mdrv_EARC_TX_HeartBeatSeqProc(void)
{
	MS_U8 u8CommonModeState = HDMI_EARCTX_CHECK_CAP_CHANGE;
	MS_U16 u16Temp = 0;
	MS_BOOL bContinueFlag = FALSE;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	do {
		switch (u8CommonModeState) {
		case HDMI_EARCTX_CHECK_CAP_CHANGE:
		_mdrv_EARC_TX_HBSeqCheckCapChange();
		bContinueFlag = TRUE;
		break;

		case HDMI_EARCTX_CHECK_STA_CHANGE:
		_mdrv_EARC_TX_HBSeqCheckSTATChange();
		bContinueFlag = TRUE;
		break;

		case HDMI_EARCTX_DO_COMMON_PROC:
		if (pPollingInfo->bCapChangeFlag) {
			if ((pPollingInfo->bCommonModeCapBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeStatBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeSetLatencyBusyFlag == FALSE)) {
				pPollingInfo->u16OPCodeOffset = 0;
				pPollingInfo->u8TransactioinRetryCnt = 0;
				pPollingInfo->bCommonModeCapBusyFlag = TRUE;
				pPollingInfo->u8TranState = HDMI_EARCTX_SET_OP_CODE_INFO;

				for (u16Temp = 0; u16Temp < HDMI_EARCTX_CAP_LEN; u16Temp++)
					pPollingInfo->u8CapContent[u16Temp]	= 0;
			}
		} else if (pPollingInfo->bStatChangeFlag) {
			if ((pPollingInfo->bCommonModeCapBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeStatBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeSetLatencyBusyFlag == FALSE)) {
				pPollingInfo->u16OPCodeOffset = HDMI_EARCTX_OFFSET_D2;
				pPollingInfo->u8TransactioinRetryCnt = 0;
				pPollingInfo->bCommonModeStatBusyFlag = TRUE;
				pPollingInfo->u8TranState = HDMI_EARCTX_SET_OP_CODE_INFO;
			}
		} else if (pPollingInfo->bSetLatencyFlag) {
			if ((pPollingInfo->bCommonModeCapBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeStatBusyFlag == FALSE) &&
				(pPollingInfo->bCommonModeSetLatencyBusyFlag == FALSE)) {
				pPollingInfo->u16OPCodeOffset = HDMI_EARCTX_OFFSET_D3;
				pPollingInfo->u8TransactioinRetryCnt = 0;
				pPollingInfo->bCommonModeSetLatencyBusyFlag = TRUE;
				pPollingInfo->u8TranState = HDMI_EARCTX_SET_OP_CODE_INFO;
			}
		}
		bContinueFlag = FALSE;
		break;

		default:
		bContinueFlag = FALSE;
		break;
		}

		u8CommonModeState++;
	} while (bContinueFlag);
}

void _mdrv_EARC_TX_CMProcSetOpCodeInfo(void)
{
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (pPollingInfo->bCommonModeCapBusyFlag) {
		pPollingInfo->u8OPCodeDeviceID =
			HDMI_EARCTX_READ_ONLY_CAPBILITIES_DATA_STRUCTURE;
		pPollingInfo->u8TranState =
			HDMI_EARCTX_WAIT_RX_HEARTBEAT_STATUS;
		pPollingInfo->u8TransactionDataThreshold
			= HDMI_EARCTX_READ_CAP_LEN_MAX;
	} else if (pPollingInfo->bCommonModeStatBusyFlag) {
		pPollingInfo->u8OPCodeDeviceID =
			HDMI_EARCTX_READ_WRITE_EARC_STATUS_AND_CONTROL_REGISTERS;
		pPollingInfo->u8TransactionDataThreshold
			= HDMI_EARCTX_STAT_LEN;
		pPollingInfo->u8TranState =
			HDMI_EARCTX_WAIT_RX_HEARTBEAT_STATUS;
	} else if (pPollingInfo->bCommonModeSetLatencyBusyFlag) {
		pPollingInfo->u8OPCodeDeviceID =
			HDMI_EARCTX_READ_WRITE_EARC_STATUS_AND_CONTROL_REGISTERS;
		pPollingInfo->u8TransactionDataThreshold
			= HDMI_EARCTX_STAT_LEN;
		pPollingInfo->u8TranState =
			HDMI_EARCTX_WRITE_TRIGGER;
		_mdrv_EARC_TX_WriteDataContent();
	}

}

void _mdrv_EARC_WAIT_RX_Heartbeat_Status(MS_BOOL *bNextStateFlag)
{
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	if (_mdrv_EARC_TX_GetRxHeartBeatStatus()) {
		pPollingInfo->u8TranState = HDMI_EARCTX_WAIT_RX_HEARTBEAT_STATUS;
		*bNextStateFlag = FALSE;
		//EARC_MSG_INFO("Wait eARC_Rx Heartbeat Status to 0\r\n");
	} else {
		pPollingInfo->u8TranState = HDMI_EARCTX_READ_TRIGGER;
		*bNextStateFlag = TRUE;
	}

}

void _mdrv_EARC_Check_CapLength(MS_BOOL *bContinFlag)
{
	MS_U16 u16CapLenTmp = 0;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	u16CapLenTmp = pPollingInfo->u16CapLen + 1;

	if (u16CapLenTmp > 0) {
		if (pPollingInfo->u8CapContent[u16CapLenTmp] != 0x00) {
			u16CapLenTmp = pPollingInfo->u16CapLen + 2;
			if (u16CapLenTmp < HDMI_EARCTX_CAP_LEN) {
				pPollingInfo->u16CapLen +=
					(pPollingInfo->u8CapContent[u16CapLenTmp] + 2);

				if (pPollingInfo->u16OPCodeOffset <
					pPollingInfo->u16CapLen) {
					pPollingInfo->u16OPCodeOffset +=
						HDMI_EARCTX_READ_CAP_LEN_MAX;
					pPollingInfo->u8TranState =
						HDMI_EARCTX_SET_OP_CODE_INFO;
					*bContinFlag = FALSE;
				} else {
					*bContinFlag = TRUE;
				}
			} else {
				pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
				*bContinFlag = FALSE;
			}
		} else {
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
			*bContinFlag = FALSE;
		}
	} else {
		pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
		*bContinFlag = FALSE;
	}
}

void _mdrv_EARC_TX_CommonModeProc(void)
{
	MS_BOOL bNextStateFlag = FALSE;
	MS_BOOL bContinFlag = FALSE;
	stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
		&(pEarcRes->stEarcCmPollingInfo);

	do {

		switch (pPollingInfo->u8TranState) {
		case HDMI_EARCTX_SET_OP_CODE_INFO:
		_mdrv_EARC_TX_CMProcSetOpCodeInfo();
		_mdrv_EARC_TX_SetPacketInfo();
		bNextStateFlag = TRUE;
		break;

		case HDMI_EARCTX_WAIT_RX_HEARTBEAT_STATUS:
		_mdrv_EARC_WAIT_RX_Heartbeat_Status(&bNextStateFlag);
		break;

		case HDMI_EARCTX_READ_TRIGGER:
		_mdrv_EARC_TX_GetReadTransactionResult(TRUE);
		_mdrv_EARC_TX_HeartBeatReadDataTrigger();
		pPollingInfo->u8TranState = HDMI_EARCTX_WAIT_READ_TRANSACTION_DONE;
		bNextStateFlag = TRUE;
		break;

		case HDMI_EARCTX_WRITE_TRIGGER:
		_mdrv_EARC_TX_HeartBeatWriteDataTrigger(TRUE);
		_mdrv_EARC_TX_GetWriteTransactionResult(TRUE);
		_mdrv_EARC_TX_HeartBeatWriteDataTrigger(FALSE);
		pPollingInfo->u8TranState = HDMI_EARCTX_WAIT_WRITE_TRANSACTION_DONE;
		bNextStateFlag = TRUE;
		break;

		case HDMI_EARCTX_WAIT_READ_TRANSACTION_DONE:
		if (_mdrv_EARC_TX_GetReadTransactionResult(FALSE)) {
			pPollingInfo->u8TransactioinRetryCnt = 0;
			pPollingInfo->u8TranState = HDMI_EARCTX_READ_DATA_CONTENT;
			bNextStateFlag = TRUE;
			//EARC_MSG_INFO("Read Transaction Done, Offset = %d\r\n",
				//pPollingInfo->u16OPCodeOffset);
		} else if (pPollingInfo->u8TransactioinRetryCnt < HDMI_EARCTX_RETRY_CNT) {
			pPollingInfo->u8TransactioinRetryCnt++;
			pPollingInfo->u8TranState = HDMI_EARCTX_WAIT_READ_TRANSACTION_DONE;
			bNextStateFlag = FALSE;
		} else {
			pPollingInfo->u8TransactioinRetryCnt = 0;
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
			bNextStateFlag = TRUE;
			//EARC_MSG_INFO("Read Transation Done, but fail(%d) Offset = %d\r\n",
			//pPollingInfo->u8TranResult,
			//pPollingInfo->u16OPCodeOffset);
		}
		break;

		case HDMI_EARCTX_WAIT_WRITE_TRANSACTION_DONE:
		if (_mdrv_EARC_TX_GetWriteTransactionResult(FALSE)) {
			pPollingInfo->u8TransactioinRetryCnt = 0;
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
			bNextStateFlag = TRUE;
			//EARC_MSG_INFO("Write Transaction Done, Offset = %d\r\n",
				//pPollingInfo->u16OPCodeOffset);
		} else if (pPollingInfo->u8TransactioinRetryCnt	< HDMI_EARCTX_RETRY_CNT) {
			pPollingInfo->u8TransactioinRetryCnt++;
			pPollingInfo->u8TranState = HDMI_EARCTX_WAIT_WRITE_TRANSACTION_DONE;
			bNextStateFlag = FALSE;
		} else {
			pPollingInfo->u8TransactioinRetryCnt = 0;
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
			bNextStateFlag = TRUE;
			//EARC_MSG_INFO
			//("Write Transaction Done, but fail(%d), Offset = %d\n",
			//pPollingInfo->u8TranResult,
			//pPollingInfo->u16OPCodeOffset);
		}
		break;

		case HDMI_EARCTX_READ_DATA_CONTENT:
		if ((pPollingInfo->bCommonModeCapBusyFlag) &&
			(pPollingInfo->u16OPCodeOffset < HDMI_EARCTX_CAP_LEN)) {

			_mdrv_EARC_TX_ReadDataContent();

		do {
			if (pPollingInfo->u16OPCodeOffset == 0) {
				// issue: patch for keysight.
				// heartbeat would fail when access capability above 128 bytes.
				// sw check if block id is defined in the spec
				if (pPollingInfo->u8CapContent[1] == 0) {
					pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
				} else {
					// CapLen count from 0
					pPollingInfo->u16CapLen = pPollingInfo->u8CapContent[2] + 2;
					pPollingInfo->u16OPCodeOffset +=
						HDMI_EARCTX_READ_CAP_LEN_MAX;
					pPollingInfo->u8TranState =
						HDMI_EARCTX_SET_OP_CODE_INFO;
				}
				bContinFlag = FALSE;
			} else {
				if (pPollingInfo->u16OPCodeOffset > pPollingInfo->u16CapLen) {
					_mdrv_EARC_Check_CapLength(&bContinFlag);
				} else {
					pPollingInfo->u16OPCodeOffset +=
						HDMI_EARCTX_READ_CAP_LEN_MAX;
					pPollingInfo->u8TranState = HDMI_EARCTX_SET_OP_CODE_INFO;
					bContinFlag = FALSE;
				}
			}
		} while (bContinFlag);
		} else if (pPollingInfo->bCommonModeStatBusyFlag) {
			_mdrv_EARC_TX_ReadDataContent();
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
		} else {
			pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_FINISH;
		}

		bNextStateFlag = TRUE;
		break;

		case HDMI_EARCTX_TRANSACTION_FINISH:
		if (pPollingInfo->bCommonModeCapBusyFlag) {
			pPollingInfo->bCapChangeFlag = FALSE;
			pPollingInfo->bCapChangeUpdateDoneFlag = TRUE;
			pPollingInfo->bCommonModeCapBusyFlag = FALSE;
			pPollingInfo->u8CapabilityChangeCnt =
			pPollingInfo->u8CapabilityChangeCnt + 1;
			EARC_MSG_INFO("Read Capability Transaction Complete. CapLen = %d\r\n",
								(pPollingInfo->u16CapLen+1));
		} else if (pPollingInfo->bCommonModeStatBusyFlag) {
			pPollingInfo->bStatChangeFlag = FALSE;
			pPollingInfo->bStatChangeUpdateDoneFlag = TRUE;
			pPollingInfo->bCommonModeStatBusyFlag = FALSE;
			EARC_MSG_INFO
		    ("Read Latency Transaction Complete. Get Latency Value = %d\r\n",
		     pPollingInfo->u8LatencyContent);
		} else if (pPollingInfo->bCommonModeSetLatencyBusyFlag) {
			pPollingInfo->bSetLatencyFlag = FALSE;
			pPollingInfo->bCommonModeSetLatencyBusyFlag = FALSE;
			EARC_MSG_INFO
		    ("Write Latency Transaction Complete. Set Latency Value = %d\r\n",
		     pPollingInfo->u8SetLatencyValue);
		}
		pPollingInfo->u8TranState = HDMI_EARCTX_TRANSACTION_NONE;
		bNextStateFlag = FALSE;
		break;

		default:
			bNextStateFlag = FALSE;
			break;
		}
	} while (bNextStateFlag);
}

static int _mdrv_EARC_PollingTask(void *punused __attribute__ ((unused)))
{
	if (pEarcRes != NULL) {
		stHDMI_EARCTX_COMMON_MODE_POLLING_INFO *pPollingInfo =
			&(pEarcRes->stEarcCmPollingInfo);

		while (pEarcRes->bEARCTaskProcFlag) {

			EN_EARC_GET_CONNECT_STATUS u8RegValue = _mdrv_EARC_TX_GetConnectState();

			if (u8PreConStus != u8RegValue) {
				switch (u8RegValue) {
				case EARC_CONNECTION_LOST_5V:
					EARC_MSG_INFO("eARC Connect State: TX IDLE1(LOST_5V)\n");
					break;
				case EARC_CONNECTION_ARC_MODE:
					EARC_MSG_INFO("eARC Connect State: TX IDLE2(ARC_MODE)\n");
					break;
				case EARC_CONNECTION_EARC_DISC1:
					EARC_MSG_INFO("eARC Connect State: TX DISC1(EARC_BUSY)\n");
					break;
				case EARC_CONNECTION_EARC_DISC2:
					EARC_MSG_INFO("eARC Connect State: TX DISC2(EARC_BUSY)\n");
					break;
				case EARC_CONNECTION_EARC_MODE:
					EARC_MSG_INFO("eARC Connect State: TX eARC(EARC_MODE)\n");
					break;
				default:
					EARC_MSG_INFO("eARC Connect State: NONE_MODE\n");
					break;
				}

				if ((u8RegValue == EARC_CONNECTION_EARC_DISC1) ||
					(u8RegValue == EARC_CONNECTION_EARC_DISC2)) {
					pEarcRes->u8EARCTXConnectState = EARC_CONNECTION_EARC_BUSY;
				} else {
					pEarcRes->u8EARCTXConnectState = u8RegValue;
				}

				pPollingInfo->bCapChangeUpdateDoneFlag = FALSE;
				pPollingInfo->bStatChangeUpdateDoneFlag = FALSE;
				pPollingInfo->bCapChangeFlag = FALSE;
				pPollingInfo->bStatChangeFlag = FALSE;
				pPollingInfo->u8CapabilityChangeCnt = 0;
				pPollingInfo->u16CapLen = 0;

#if (!EARC_PATH_MODE) // 1: old path mode, 0:new path mode
				if (pEarcRes->u8EARCTXConnectState == EARC_CONNECTION_EARC_MODE) {
					pEarcRes->u8EARCArcPin = TRUE;
					HWREG_EARC_ArcPin(TRUE);
					EARC_MSG_INFO("Enable ARC pin\r\n");
				}
#endif
				u8PreConStus = u8RegValue;
			}

			if (pEarcRes->u8EARCTXConnectState == EARC_CONNECTION_EARC_MODE) {
				_mdrv_EARC_TX_HeartBeatSeqProc();
				_mdrv_EARC_TX_CommonModeProc();
			}
			HWREG_EARC_TX_ResetGetOnce();
			MsOS_DelayTask(EARC_POLLING_INTERVAL);

		}
	}

	return 0;

}

MS_BOOL _mdrv_EARC_CreateTask(void)
{
	MS_BOOL bCreateSuccess = TRUE;

	if (pEarcRes->slEARCPollingTaskID < 0) {
		stEarcThread = kthread_create(_mdrv_EARC_PollingTask, pEarcRes, "HDMI EARC task");

		if (IS_ERR(stEarcThread)) {
			bCreateSuccess = FALSE;
			stEarcThread = NULL;
			EARC_MSG_EMERG("Create Thread Failed\r\n");

		} else {
			pEarcRes->bEARCTaskProcFlag = TRUE;
			pEarcRes->slEARCPollingTaskID = stEarcThread->tgid;
			wake_up_process(stEarcThread);
			EARC_MSG_INFO("Create Thread OK\r\n");
		}
	}

	if (pEarcRes->slEARCPollingTaskID < 0) {
		EARC_MSG_EMERG("Create Thread Failed!!\n");
		bCreateSuccess = FALSE;
	}

	return bCreateSuccess;


}

MS_U8 mdrv_EARC_GetEarcPortSel(void)
{
	MS_U8 u8RetValue = 0;

	if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->u8EarcPortSel); // report HW port num

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetEarcUIPortSel(void)
{
	int i = 0;
	MS_U8 u8RetValue = 0;

	if (pEarcRes != NULL) {
		for (i = 0; i < HDMI_PORT_MAX_NUM; i++) {
			if (pEarcRes->u8EarcPortSel == pEarcRes->map[i].hw_port) {
				u8RetValue = pEarcRes->map[i].ui_port; // report UI port num
				EARC_MSG_INFO("u8RetValue = %d, u8EarcPortSel %d\n"
						, u8RetValue, pEarcRes->u8EarcPortSel);
				break;
			}
		}
	}

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetConnectState(void)
{
	MS_U8 u8RetValue = 0;

	if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->u8EARCTXConnectState);

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetCapibilityChange(void)
{
	int ret = 0;
	bool stub = 0;
	MS_U8 u8RetValue = 0;

	ret = HWREG_EARC_TX_GetStub(&stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (stub)
		u8RetValue = HWREG_EARC_TX_GetCapbilityChange();
	else if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag);

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetLatencyChange(void)
{
	int ret = 0;
	bool stub = 0;
	MS_U8 u8RetValue = 0;

	ret = HWREG_EARC_TX_GetStub(&stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (stub)
		u8RetValue = HWREG_EARC_TX_GetLatencyChange();
	else if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag);

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetCapibility(MS_U8 u8Index)
{
	int ret = 0;
	bool stub = 0;
	MS_U8 u8RetValue = 0;

	ret = HWREG_EARC_TX_GetStub(&stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (stub)
		u8RetValue = HWREG_EARC_TX_GetCapbilityValue();
	else if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->stEarcCmPollingInfo.u8CapContent[u8Index]);

	return u8RetValue;
}

MS_U8 mdrv_EARC_GetLatency(void)
{
	int ret = 0;
	bool stub = 0;
	MS_U8 u8RetValue = 0;

	ret = HWREG_EARC_TX_GetStub(&stub);
	if (ret < 0) {
		EARC_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (stub)
		u8RetValue = HWREG_EARC_TX_GetLatencyValue();
	else if (pEarcRes != NULL)
		u8RetValue = (pEarcRes->stEarcCmPollingInfo.u8LatencyContent);

	return u8RetValue;
}

void mdrv_EARC_SetRIUAddr(MS_U32 u32P_Addr, MS_U32 u32P_Size, MS_U64 u64V_Addr)
{
	HWREG_EARC_TX_SetRIUAddr(u32P_Addr, u32P_Size, u64V_Addr);
}

void mdrv_EARC_SetCapibilityChange(void)
{
	if (pEarcRes != NULL)
		pEarcRes->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag = TRUE;
}

void mdrv_EARC_SetCapibilityChangeClear(void)
{
	if (pEarcRes != NULL)
		pEarcRes->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag = FALSE;
}

void mdrv_EARC_SetLatencyChange(void)
{
	if (pEarcRes != NULL)
		pEarcRes->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag = TRUE;
}

void mdrv_EARC_SetLatencyChangeClear(void)
{
	if (pEarcRes != NULL)
		pEarcRes->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag = FALSE;
}

void mdrv_EARC_SetEarcPort(MS_U8 u8EarcPort, MS_BOOL bEnable5VDetectInvert,
			   MS_BOOL bEnableHPDInvert)
{
	if (pEarcRes != NULL) {
		pEarcRes->u8EarcPortSel = u8EarcPort;
		pEarcRes->bEnable5VDetectInvert = bEnable5VDetectInvert;
		pEarcRes->bEnableHPDInvert = bEnableHPDInvert;
		HWREG_EARC_OutputPortSelect(pEarcRes->u8EarcPortSel, bEnable5VDetectInvert,
			bEnableHPDInvert);
	}
}

void mdrv_EARC_SetEarcSupportMode(MS_U8 u8EarcSupportMode)
{
	if (pEarcRes != NULL) {
		pEarcRes->u8EARCSupportMode = u8EarcSupportMode;
		HWREG_EARC_SupportMode(pEarcRes->u8EARCSupportMode);
	}
}

void mdrv_EARC_SetArcPin(MS_U8 u8ArcPinEnable)
{
	if (pEarcRes != NULL) {
		if (pEarcRes->u8EARCTXConnectState != EARC_CONNECTION_EARC_MODE) {
			pEarcRes->u8EARCArcPin = u8ArcPinEnable;
			HWREG_EARC_ArcPin(pEarcRes->u8EARCArcPin);
		}
	}
}

MS_U8 mdrv_EARC_GetArcPin(void)
{
	return HWREG_EARC_TX_GetArcPinStatus();
}

void mdrv_EARC_SetLatencyInfo(MS_U8 u8LatencyValue)
{
	if (pEarcRes != NULL) {
		pEarcRes->stEarcCmPollingInfo.bSetLatencyFlag = TRUE;
		pEarcRes->stEarcCmPollingInfo.u8SetLatencyValue = u8LatencyValue;
	}
}

void mdrv_EARC_Set_HeartbeatStatus(MS_U8 u8HeartbeatStatus,
	MS_BOOL bOverWriteEnable, MS_BOOL bSetValue)
{
	if (pEarcRes != NULL) {
		HWREG_EARC_TX_ConfigHeartbeatStatus(pEarcRes->u8EARCTXConnectState,
			u8HeartbeatStatus, bOverWriteEnable, bSetValue);
	}
}

void mdrv_EARC_Set_HDMI_Port_Map(struct mtk_earc_hdmi_port_map *map)
{
	if (pEarcRes != NULL) {
		int i = 0;

		for (i = 0; i < HDMI_PORT_MAX_NUM; i++) {
			pEarcRes->map[i].ui_port = map[i].ui_port;
			pEarcRes->map[i].hw_port = map[i].hw_port;
			EARC_MSG_INFO("pEarcRes->map[%d].ui_port = %d, hw_port = %d\n",
				i, pEarcRes->map[i].ui_port, pEarcRes->map[i].hw_port);
		}
	}
}

void mdrv_EARC_Set_DifferentialDriveStrength(MS_U8 u8Level)
{
	HWREG_EARC_TX_SetDifferentialDriveStrength(u8Level);
}

MS_U8 mdrv_EARC_Get_DifferentialDriveStrength(void)
{
	return HWREG_EARC_TX_GetDifferentialDriveStrength();
}

void mdrv_EARC_Set_DifferentialSkew(MS_U8 u8Level)
{
	HWREG_EARC_TX_SetDifferentialSkew(u8Level);
}

MS_U8 mdrv_EARC_Get_DifferentialSkew(void)
{
	return HWREG_EARC_TX_GetDifferentialSkew();
}

void mdrv_EARC_Set_CommonDriveStrength(MS_U8 u8Level)
{
	HWREG_EARC_TX_SetCommonDriveStrength(u8Level);
}

MS_U8 mdrv_EARC_Get_CommonDriveStrength(void)
{
	return HWREG_EARC_TX_GetCommonDriveStrength();
}

void mdrv_EARC_Set_DifferentialOnOff(MS_BOOL bOnOff)
{
	HWREG_EARC_TX_SetDifferentialOnOff(bOnOff);
}

MS_BOOL mdrv_EARC_Get_DifferentialOnOff(void)
{
	return HWREG_EARC_TX_GetDifferentialOnOff();
}

void mdrv_EARC_Set_CommonOnOff(MS_BOOL bOnOff)
{
	HWREG_EARC_TX_SetCommonOnOff(bOnOff);
}

MS_U8 mdrv_EARC_Get_CommonOnOff(void)
{
	return HWREG_EARC_TX_GetCommonOnOff();
}

MS_U8 mdrv_EARC_Get_DiscState(void)
{
	return HWREG_EARC_TX_GetDiscState();
}

MS_U8 mdrv_EARC_Set_Stub(bool bEnable)
{
	HWREG_EARC_TX_SetStub(bEnable);

	return 0;
}

MS_U8 mdrv_EARC_Get_Stub(bool *bstub)
{
	if (bstub == NULL) {
		EARC_MSG_ERROR("[%s,%d] pointer is NULL\n", __func__, __LINE__);
		return -EFAULT;
	}

	return HWREG_EARC_TX_GetStub(bstub);
}

void mdrv_EARC_Set_LogLevel(MS_U8 loglevel)
{
	HWREG_EARC_TX_SetLogLevel(loglevel);
}

MS_U8 mdrv_EARC_Get_LogLevel(void)
{
	return HWREG_EARC_TX_GetLogLevel();
}

MS_U8 mdrv_EARC_Get_SupportMode(void)
{
	if (pEarcRes == NULL) {
		EARC_MSG_ERROR("pEarcRes is NULL\n");
		return 0;
	}

	return pEarcRes->u8EARCSupportMode;
}

void mdrv_EARC_Get_Info(struct HDMI_EARCTX_RESOURCE_PRIVATE *pEarcInfo)
{
	if (pEarcRes != NULL) {
		memcpy(pEarcInfo, pEarcRes, sizeof(struct HDMI_EARCTX_RESOURCE_PRIVATE));
		MsOS_DelayTask(EARC_POLLING_INTERVAL << 1);
	}
}

MS_BOOL mdrv_EARC_Resume_Init(void)
{
	MS_BOOL bReValue = FALSE;

	if (pEarcRes != NULL) {
		HWREG_EARC_Init();
		HWREG_EARC_SupportMode(pEarcRes->u8EARCSupportMode);
		HWREG_EARC_OutputPortSelect(pEarcRes->u8EarcPortSel,
			pEarcRes->bEnable5VDetectInvert, pEarcRes->bEnableHPDInvert);

		pEarcRes->u8EARCTXConnectState = EARC_CONNECTION_LOST_5V;
		pEarcRes->stEarcCmPollingInfo.bCapChangeFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bStatChangeFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bSetLatencyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeCapBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeStatBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeSetLatencyBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.u8TransactioinRetryCnt = 0;
		pEarcRes->stEarcCmPollingInfo.u8TranResult = HDMI_EARCTX_FREE;
		pEarcRes->stEarcCmPollingInfo.u8TranState =
			HDMI_EARCTX_TRANSACTION_NONE;
		pEarcRes->stEarcCmPollingInfo.u8CapabilityChangeCnt = 0;

		EARC_MSG_INFO("Resume Initial Setting Done !!!\n");

		bReValue = TRUE;
	} else {
		EARC_MSG_ERROR("Resume Initial Setting Failed !!!\n");
	}

	return bReValue;
}

MS_BOOL mdrv_EARC_Init(MS_U8 u8DefaultSupportMode)
{
	MS_BOOL bReValue = FALSE;

	pEarcRes = kmalloc(sizeof(struct HDMI_EARCTX_RESOURCE_PRIVATE), GFP_KERNEL);

	if (pEarcRes != NULL) {
		pEarcRes->bSelfCreateTaskFlag = FALSE;
		pEarcRes->bEARCTaskProcFlag = FALSE;
		pEarcRes->slEARCPollingTaskID = -1;
		pEarcRes->u8EARCTXConnectState = EARC_CONNECTION_LOST_5V;
		pEarcRes->u8EARCSupportMode = u8DefaultSupportMode;
		pEarcRes->stEarcCmPollingInfo.bCapChangeFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bStatChangeFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bSetLatencyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeCapBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeStatBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.bCommonModeSetLatencyBusyFlag = FALSE;
		pEarcRes->stEarcCmPollingInfo.u8TransactioinRetryCnt = 0;
		pEarcRes->stEarcCmPollingInfo.u8TranResult = HDMI_EARCTX_FREE;
		pEarcRes->stEarcCmPollingInfo.u8TranState =
		    HDMI_EARCTX_TRANSACTION_NONE;
		pEarcRes->stEarcCmPollingInfo.u8CapabilityChangeCnt = 0;

		HWREG_EARC_Init();
		HWREG_EARC_SupportMode(u8DefaultSupportMode);

		if (!pEarcRes->bSelfCreateTaskFlag) {
			if (_mdrv_EARC_CreateTask())
				pEarcRes->bSelfCreateTaskFlag = TRUE;
		}
	}

	EARC_MSG_INFO("Initial Setting Done\n");

	bReValue = TRUE;

	return bReValue;
}

void mdrv_EARC_HWInit(MS_U32 Para1, MS_U32 Para2)
{
	HWREG_EARC_SetHWStatus(Para1, Para2);
	HWREG_EARC_TX_SetLogLevel(0);
}

