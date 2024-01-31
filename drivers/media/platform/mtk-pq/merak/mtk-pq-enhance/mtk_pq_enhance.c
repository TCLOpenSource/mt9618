// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <uapi/linux/mtk-v4l2-pq.h>

#include "mtk_pq.h"
#include "m_pqu_pq.h"
#include "mtk_pq_hdr.h"
#include "mtk_pq_pqd.h"
#include "mtk_pq_hwbypass.h"
#include "mtk_pq_swbypass.h"
//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define UNUSED_PARA(x) (void)(x)
#define SYSFS_MAX_BUF_COUNT (0x1000)
#define VAL_MAX_1BYTE	(0xFF)
#define STATUS_CMD_TIMEOUT_TIME	(2)
#define STATUS_CMD_WAITTIME_MS	(20)

//-----------------------------------------------------------------------------
//  Local Struct
//-----------------------------------------------------------------------------
//INFO OF PQ ENHANCE SUB DEV
struct v4l2_enhance_info {
	struct st_hdr_info hdr_info;
	struct st_pqd_info pqd_info;
};


//-----------------------------------------------------------------------------
//  LOCAL FUNCTION
//-----------------------------------------------------------------------------
static inline bool _mtk_pq_enhance_check_scnsize(int size, int max_size)
{
	if ((size < 0) || (size >= max_size)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "scan size %d overflow!\n", size);
		return false;
	}
	return true;
}

static bool _mtk_pq_enhance_get_dbg_value_from_string(char *buf, char *name,
	unsigned int len, __u32 *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return false;
	}

	*value = 0;
	cmd = buf;

	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0) {
			if (kstrtoint(tmp_value, 0, value)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"kstrtoint fail!\n");
				return find;
			}
			find = true;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
					"name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"name(%s) was not found!\n", name);

	return find;
}

char *_mtk_pq_enhance_get_dbg_type_from_string(char *buf, char *name,
	unsigned int len)
{
	char *string = NULL;
	char *tmp_name = NULL;
	char *cmd = NULL;
	char *tmp_value = NULL;

	if ((buf == NULL) || (name == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return NULL;
	}

	cmd = buf;
	tmp_name = NULL;
	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0)
			return tmp_value;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"name(%s) was not found!\n", name);

	return NULL;
}

static __u8 _mtk_pq_enhance_InputConvert(
	__u32 u32Input)
{
	__u8 u8PQEnhanceInput;

	switch (u32Input) {
	case MTK_PQ_INPUTSRC_NONE:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_NONE;
		break;
	case MTK_PQ_INPUTSRC_VGA:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_VGA;
		break;
	case MTK_PQ_INPUTSRC_TV:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_TV;
		break;
	case MTK_PQ_INPUTSRC_CVBS:
	case MTK_PQ_INPUTSRC_CVBS2:
	case MTK_PQ_INPUTSRC_CVBS3:
	case MTK_PQ_INPUTSRC_CVBS4:
	case MTK_PQ_INPUTSRC_CVBS5:
	case MTK_PQ_INPUTSRC_CVBS6:
	case MTK_PQ_INPUTSRC_CVBS7:
	case MTK_PQ_INPUTSRC_CVBS8:
	case MTK_PQ_INPUTSRC_CVBS_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_CVBS;
		break;
	case MTK_PQ_INPUTSRC_SVIDEO:
	case MTK_PQ_INPUTSRC_SVIDEO2:
	case MTK_PQ_INPUTSRC_SVIDEO3:
	case MTK_PQ_INPUTSRC_SVIDEO4:
	case MTK_PQ_INPUTSRC_SVIDEO_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_SVIDEO;
		break;
	case MTK_PQ_INPUTSRC_YPBPR:
	case MTK_PQ_INPUTSRC_YPBPR2:
	case MTK_PQ_INPUTSRC_YPBPR3:
	case MTK_PQ_INPUTSRC_YPBPR_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_YPBPR;
		break;
	case MTK_PQ_INPUTSRC_SCART:
	case MTK_PQ_INPUTSRC_SCART2:
	case MTK_PQ_INPUTSRC_SCART_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_SCART;
		break;
	case MTK_PQ_INPUTSRC_HDMI:
	case MTK_PQ_INPUTSRC_HDMI2:
	case MTK_PQ_INPUTSRC_HDMI3:
	case MTK_PQ_INPUTSRC_HDMI4:
	case MTK_PQ_INPUTSRC_HDMI_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_HDMI;
		break;
	case MTK_PQ_INPUTSRC_DVI:
	case MTK_PQ_INPUTSRC_DVI2:
	case MTK_PQ_INPUTSRC_DVI3:
	case MTK_PQ_INPUTSRC_DVI4:
	case MTK_PQ_INPUTSRC_DVI_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_DVI;
		break;
	case MTK_PQ_INPUTSRC_DTV:
	case MTK_PQ_INPUTSRC_DTV2:
	case MTK_PQ_INPUTSRC_DTV_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_DTV;
		break;
	case MTK_PQ_INPUTSRC_STORAGE:
	case MTK_PQ_INPUTSRC_STORAGE2:
	case MTK_PQ_INPUTSRC_STORAGE_MAX:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_STORAGE;
		break;
	default:
		u8PQEnhanceInput = E_PQ_ENHANCE_INPUT_SOURCE_NONE;
		break;
	}

	return u8PQEnhanceInput;
}

static int _enhance_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev =
		container_of(ctrl->handler, struct mtk_pq_device,
			enhance_ctrl_handler);
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"Window = %d, CID = %d\n", pq_dev->dev_indx, ctrl->id);

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	pqdev = dev_get_drvdata(pq_dev->dev);

	switch (ctrl->id) {
	case V4L2_CID_HDR_TYPE:
		ret = mtk_hdr_GetHDRType(hdr_info, &ctrl->val, pq_dev);
		break;
	case V4L2_CID_HDR_SEAMLESS_STATUS:
		ret = mtk_hdr_GetSeamlessStatus(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DOT_BY_DOT:
		ret = mtk_pqd_GetPointToPoint(pqd_info, &ctrl->val);
		break;
	case V4L2_CID_GAME_MODE:
		ret = mtk_pqd_GetGameModeStatus(pqd_info, &ctrl->val);
		break;
	case V4L2_CID_DUAL_VIEW_STATUS:
		ret = mtk_pqd_GetDualViewState(pqd_info, &ctrl->val);
		break;
	case V4L2_CID_ZERO_FRAME_MODE:
		ret = mtk_pqd_GetZeroFrameMode(pqd_info, &ctrl->val);
		break;
	case V4L2_CID_RFBL_MODE:
		ret = mtk_pqd_GetRFBLMode(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DPU_MODULE_SETTINGS:
		ret = mtk_pqd_GetDPUStatus(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_CMD:
		ret = mtk_pqd_ReadPQCmd(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_SWDR_INFO:
		ret = mtk_pqd_GetSWDRInfo(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DEALYTIME:
		ret = mtk_pqd_GetVideoDelayTime(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_CAPS:
		ret = mtk_pqd_GetPQCaps(pqd_info, ctrl->p_new.p, pqdev);
		break;
	case V4L2_CID_PQ_VERSION:
		ret = mtk_pqd_GetPQVersion(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PICTUREMODE:
		ret = mtk_pqd_GetHSYGrule(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_EVALUATE_FBL:
		ret = mtk_pqd_GetFBLStatus(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HISTOGRAM:
		ret = mtk_pqd_GetIPHistogram(pq_dev->dev_indx, pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HSY:
		ret = mtk_pqd_ACE_GetHSYSetting(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HSY_RANGE:
		ret = mtk_pqd_ACE_GetHSYRange(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_LUMA_CURVE:
		ret = mtk_pqd_ACE_GetLumaInfo(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_CHROMA:
		ret = mtk_pqd_ACE_GetChromaInfo(pqd_info, ctrl->p_new.p);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int _enhance_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev =
		container_of(ctrl->handler,
			struct mtk_pq_device, enhance_ctrl_handler);
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"Window = %d, CID = %d\n", pq_dev->dev_indx, ctrl->id);

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	pqdev = dev_get_drvdata(pq_dev->dev);

	switch (ctrl->id) {
	case V4L2_CID_HDR_TYPE:
		pq_dev->hdr_type = (__u32)*ctrl->p_new.p_u8;
		break;
	case V4L2_CID_DOLBY_BIN_DATA: //9964087
		ret = mtk_hdr_SetDolbyBinMmpStatus(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DOLBY_BIN_BUFFER: //9964088
		ret = mtk_hdr_SetDVBinDone(hdr_info, ctrl->p_new.p, pqdev);
		break;
	case V4L2_CID_DOLBY_LS_DATA:
		ret = mtk_pq_dv_set_ambient(ctrl->p_new.p, pq_dev);
		break;
	case V4L2_CID_DOLBY_PD_MODE:
		ret = mtk_pq_dv_set_pr_ctrl(ctrl->p_new.p, pqdev);
		break;
	case V4L2_CID_DOLBY_HDR_MODE:
		ret = mtk_hdr_SetDolbyViewMode(hdr_info, ctrl->val);
		break;
	case V4L2_CID_HDR_ENABLE_MODE:
		ret = mtk_hdr_SetHDRMode(hdr_info, ctrl->val);
		break;
	case V4L2_CID_TMO_MODE:
		ret = mtk_hdr_SetTmoLevel(hdr_info, ctrl->val);
		break;
	case V4L2_CID_DOLBY_3DLUT:
		ret = mtk_hdr_SetDolby3DLut(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_CUS_COLOR_RANGE:
		ret = mtk_hdr_SetCUSColorRange(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_CFD_CUSTOMER_SETTING:
		ret = mtk_hdr_SetCustomerSetting(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HDR_ATTR:
		ret = mtk_hdr_SetHDRAttr(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HDR_SEAMLESS_STATUS:
		ret = mtk_hdr_SetSeamlessStatus(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HDR_CUS_COLORI_METRY:
		ret = mtk_hdr_SetCusColorMetry(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HDR_DOLBY_GD_INFO:
		ret = mtk_hdr_SetDolbyGDInfo(hdr_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_SETTING:
		ret = mtk_pqd_LoadPQSetting(pqd_info, ctrl->val);
		break;
	case V4L2_CID_DOT_BY_DOT:
		ret = mtk_pqd_SetPointToPoint(pqd_info, ctrl->val);
		break;
	case V4L2_CID_LOAD_PTP:
		ret = mtk_pqd_LoadPTPTable(pqd_info, ctrl->val);
		break;
	case V4L2_CID_3D_CLONE_PQMAP:
		ret = mtk_pqd_ACE_3DClonePQMap(pqd_info, ctrl->val);
		break;
	case V4L2_CID_3D_CLONE_PIP:
		ret = mtk_pqd_Set3DCloneforPIP(pqd_info, ctrl->val);
		break;
	case V4L2_CID_3D_CLONE_LBL:
		ret = mtk_pqd_Set3DCloneforLBL(pqd_info, ctrl->val);
		break;
	case V4L2_CID_GAME_MODE:
		ret = mtk_pqd_SetGameMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_BOB_MODE:
		ret = mtk_pqd_SetBOBMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_BYPASS_MODE:
		ret = mtk_pqd_SetBypassMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_DMEO_CLONE:
		ret = mtk_pqd_DemoCloneWindow(pqd_info, ctrl->val);
		break;
	case V4L2_CID_REDUCE_BW:
		ret = mtk_pqd_ReduceBWforOSD(pqd_info, ctrl->val);
		break;
	case V4L2_CID_HDR_LEVEL:
		ret = mtk_pqd_EnableHDRMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_PQ_EXIT:
		ret = mtk_pqd_PQExit(pqd_info, ctrl->val);
		break;
	case V4L2_CID_RB_CHANNEL:
		ret = mtk_pqd_ACE_SetRBChannelRange(pqd_info, ctrl->val);
		break;
	case V4L2_CID_FILM_MODE:
		ret = mtk_pqd_SetFilmMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_NR_MODE:
		ret = mtk_pqd_SetNRMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_MEPG_NR_MODE:
		ret = mtk_pqd_SetMPEGNRMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_XVYCC_MODE:
		ret = mtk_pqd_SetXvyccMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_DLC_MODE:
		ret = mtk_pqd_SetDLCMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_SWDR_MODE:
		ret = mtk_pqd_SetSWDRMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_DYNAMIC_CONTRAST_ENABLE:
		ret = mtk_pqd_SetDynamicContrastMode(pqd_info, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		ret = mtk_pqd_ACE_SetContrast(pqd_info, ctrl->val);
		break;
	case V4L2_CID_BRIGHTNESS:
		ret = mtk_pqd_ACE_SetBrightness(pqd_info, ctrl->val);
		break;
	case V4L2_CID_HUE:
		ret = mtk_pqd_ACE_SetHue(pqd_info, ctrl->val);
		break;
	case V4L2_CID_SATURATION:
		ret = mtk_pqd_ACE_SetSaturation(pqd_info, ctrl->val);
		break;
	case V4L2_CID_SHARPNESS:
		ret = mtk_pqd_ACE_SetSharpness(pqd_info, ctrl->val);
		break;
	case V4L2_CID_PQ_DBGLEVEL:
		ret = mtk_pqd_SetDbgLevel(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_INIT:
		ret = mtk_pqd_SetPQInit(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_MADI_MOTION:
		ret = mtk_pqd_MADIForceMotion(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_RFBL_MODE:
		ret = mtk_pqd_SetRFBLMode(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_TABLE_STATUS:
		ret = mtk_pqd_SetGruleStatus(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_UCD_MODE:
		ret = mtk_pqd_SetUCDMode(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HSB_BYPASS:
		ret = mtk_pqd_SetHSBBypass(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_VERSION:
		ret = mtk_pqd_SetVersion(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PICTUREMODE:
		ret = mtk_pqd_SetHSYGrule(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_EVALUATE_FBL:
		ret = mtk_pqd_SetFBLStatus(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HISTOGRAM:
		ret = mtk_pqd_SetIPHistogram(pq_dev->dev_indx, pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_NR_SETTINGS:
		ret = mtk_pqd_SetNRSettings(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DPU_MODULE_SETTINGS:
		ret = mtk_pqd_SetDPUMode(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_CCM:
		ret = mtk_pqd_ACE_SetCCMInfo(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_COLOR_TUNER:
		ret = mtk_pqd_ACE_SetColorTuner(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HSY:
		ret = mtk_pqd_ACE_SetHSYSetting(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_HSY_RANGE:
		ret = mtk_pqd_ACE_SetHSYRange(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_MANUAL_LUMA_CURVE:
		ret = mtk_pqd_ACE_SetManualLumaCurve(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_BLACK_WHITE_STRETCH:
		ret = mtk_pqd_ACE_SetStretchSettings(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_CMD:
		ret = mtk_pqd_ACE_SetPQCmd(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_COLORTEMP:
		ret = mtk_pqd_ACE_SetPostColorTemp(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_MWE:
		ret = mtk_pqd_ACE_SetMWE(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_SWDR_INFO:
		ret = mtk_pqd_SetSWDRInfo(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_RGB_PATTERN:
		ret = mtk_pqd_GenRGBPattern(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_BLUE_STRETCH:
		ret = mtk_pqd_SetBlueStretch(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_H_NONLINEARSCALING:
		ret = mtk_pqd_Set_H_NonlinearScaling(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_MCDI_MODE:
		ret = mtk_pqd_SetMCDIMode(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_CHROMA:
		ret = mtk_pqd_SetChromaInfo(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQ_GAMMA:
		ret = mtk_pqd_SetPQGamma(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DEALYTIME:
		ret = mtk_pqd_SetVideoDelayTime(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DLC_INIT:
		ret = mtk_pqd_DLCInit(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_DLC_HISTO_RANGE:
		ret = mtk_pqd_SetDLCCaptureRange(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQMAP_INFO:
		ret = mtk_pqd_SetPQMapInfo(pqdev, pqd_info, ctrl->p_new.p,
			(void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].pa,
			(void *)pqdev->DDBuf[MMAP_PQMAP_INDEX].va,
			pqdev->DDBuf[MMAP_PQMAP_INDEX].size);
		break;
	case V4L2_CID_PQ_METADATA:
		ret = mtk_pqd_SetPQMetaData(pqd_info, ctrl->p_new.p);
		break;
	case V4L2_CID_PQPARAM:
		ret = mtk_pqd_SetPQParam(pq_dev, pqd_info, ctrl->p_new.p,
			(void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].va,
			(void *)pqdev->DDBuf[MMAP_PQPARAM_INDEX].pa,
			pqdev->DDBuf[MMAP_PQPARAM_INDEX].size);
		break;
	case V4L2_CID_DIPMAP_INFO:
		ret = mtk_pqd_SetDIPMapInfo(pqdev, pqd_info, ctrl->p_new.p,
			(void *)(pqdev->DDBuf[MMAP_PQMAP_INDEX].pa + u32PQMapBufLen),
			(void *)(pqdev->DDBuf[MMAP_PQMAP_INDEX].va + u32PQMapBufLen),
			pqdev->DDBuf[MMAP_PQMAP_INDEX].size);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}


static const struct v4l2_ctrl_ops enhance_ctrl_ops = {
	.g_volatile_ctrl = _enhance_g_ctrl,
	.s_ctrl = _enhance_s_ctrl,
};

static const struct v4l2_ctrl_config hdr_ctrl[] = {
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_HDR_MODE,
		.name = "dolby mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_DOLBY_VIEW_MODE_VIVID,
		.max = V4L2_DOLBY_VIEW_MODE_USER,
		.def = V4L2_DOLBY_VIEW_MODE_VIVID,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_ENABLE_MODE,
		.name = "hdr mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_HDR_ENABLE_MODE_AUTO,
		.max = V4L2_HDR_ENABLE_MODE_DISABLE,
		.def = V4L2_HDR_ENABLE_MODE_AUTO,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_TMO_MODE,
		.name = "tmo level",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_TMO_LEVEL_TYPE_LOW,
		.max = V4L2_TMO_LEVEL_TYPE_HIGH,
		.def = V4L2_TMO_LEVEL_TYPE_MID,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_TYPE,
		.name = "hdr type",
		.type = V4L2_CTRL_TYPE_U8,
		.min = V4L2_HDR_TYPE_NONE,
		.max = V4L2_HDR_TYPE_HDR10_PLUS,
		.def = V4L2_HDR_TYPE_NONE,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_3DLUT,
		.name = "dolby 3dlut info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_dolby_3dlut)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_CUS_COLOR_RANGE,
		.name = "cus color range info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_cus_color_range)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_CFD_CUSTOMER_SETTING,
		.name = "cus ip info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_cus_ip_setting)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_ATTR,
		.name = "hdr attr struct",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_hdr_attr_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_SEAMLESS_STATUS,
		.name = "seamless status",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_seamless_status)},
		.flags = V4L2_CTRL_FLAG_VOLATILE
			| V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_CUS_COLORI_METRY,
		.name = "cus colori metry",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_cus_colori_metry)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_DOLBY_GD_INFO,
		.name = "dolby gd info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_dolby_gd_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_BIN_DATA,
		.name = "dolby bin info",
		.type = V4L2_CTRL_TYPE_U32,
		.def = 0,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_BIN_BUFFER,
		.name = "dolby bin buffer",
		.type = V4L2_CTRL_TYPE_U32,
		.def = 0,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_LS_DATA,
		.name = "dolby light sense",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct meta_pq_dv_ambient)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOLBY_PD_MODE,
		.name = "dolby precision detail",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = VAL_MAX_1BYTE,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
};


static const struct v4l2_ctrl_config pqd_ctrl[] = {
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_SETTING,
		.name = "pq setting",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DOT_BY_DOT,
		.name = "point to point",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_LOAD_PTP,
		.name = "load PTP table",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_PTP_PTP,
		.max = V4L2_E_PQ_PTP_NUM,
		.def = V4L2_E_PQ_PTP_NUM,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_3D_CLONE_PQMAP,
		.name = "3D Clone PQ MAP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_WEAVETYPE_NONE,
		.max = V4L2_E_PQ_WEAVETYPE_MAX,
		.def = V4L2_E_PQ_WEAVETYPE_MAX,	//FIXME
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_3D_CLONE_PIP,
		.name = "3D PIP",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_3D_CLONE_LBL,
		.name = "3D LBL",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_GAME_MODE,
		.name = "game mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_BOB_MODE,
		.name = "bob mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_BYPASS_MODE,
		.name = "bypass mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DMEO_CLONE,
		.name = "demo clone window",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_REDUCE_BW,
		.name = "reduce banwitch by osd",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HDR_LEVEL,
		.name = "enable HDR mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFFFF,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_EXIT,
		.name = "PQ ace ecit",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_RB_CHANNEL,
		.name = "set RB channel range",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_FILM_MODE,
		.name = "enable film mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_FilmMode_MIN,
		.max = V4L2_E_PQ_FilmMode_NUM,
		.def = V4L2_E_PQ_FilmMode_DEFAULT,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_NR_MODE,
		.name = "load nr table",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_3D_NR_MIN,
		.max = V4L2_E_PQ_3D_NR_NUM,
		.def = V4L2_E_PQ_3D_NR_DEFAULT,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_MEPG_NR_MODE,
		.name = "set mpeg nr mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_MPEG_NR_MIN,
		.max = V4L2_E_PQ_MPEG_NR_NUM,
		.def = V4L2_E_PQ_MPEG_NR_DEFAULT,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_XVYCC_MODE,
		.name = "set xvycc mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_XVYCC_NORMAL,
		.max = V4L2_E_PQ_XVYCC_NUM,
		.def = V4L2_E_PQ_XVYCC_NORMAL,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DLC_MODE,
		.name = "set dlc mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_DLC_DEFAULT,
		.max = V4L2_E_PQ_DLC_MAX,
		.def = V4L2_E_PQ_DLC_DEFAULT,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_SWDR_MODE,
		.name = "set swdr mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_SWDR_OFF,
		.max = V4L2_E_PQ_SWDR_DLC_OFF,
		.def = V4L2_E_PQ_SWDR_OFF,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DYNAMIC_CONTRAST_ENABLE,
		.name = "load dynamic contrast table",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = V4L2_E_PQ_DynContr_MIN,
		.max = V4L2_E_PQ_DynContr_NUM,
		.def = V4L2_E_PQ_DynContr_DEFAULT,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_CONTRAST,
		.name = "set contrast",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0x80,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_BRIGHTNESS,
		.name = "set brignhtness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0x80,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HUE,
		.name = "set hue",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0x80,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_SATURATION,
		.name = "set stauration",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0x80,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_SHARPNESS,
		.name = "set sharpness",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 0xFF,
		.def = 0x80,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_DBGLEVEL,
		.name = "set debug level",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_dbg_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_INIT,
		.name = "pq init",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_init_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_MADI_MOTION,
		.name = "set MADI motions",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_madi_force_motion)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_RFBL_MODE,
		.name = "set rfbl mode",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_rfbl_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_TABLE_STATUS,
		.name = "set grule status",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_table_status)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_UCD_MODE,
		.name = "set ucd mode",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_ucd_level_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HSB_BYPASS,
		.name = "set hsb bypass",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_hsb_bypass)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PICTUREMODE,
		.name = "set hsy grule",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_hsy_grule_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_NR_SETTINGS,
		.name = "set nr setting",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_nr_settings)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DPU_MODULE_SETTINGS,
		.name = "set dpu setting",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_dpu_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_CCM,
		.name = "color matrix setting",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_ccm_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_COLOR_TUNER,
		.name = "color tuner",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_color_tuner_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HSY,
		.name = "hsy setting",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_hsy_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_MANUAL_LUMA_CURVE,
		.name = "manual luma curve",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_luma_curve_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_BLACK_WHITE_STRETCH,
		.name = "stretch setting",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_stretch_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_CMD,
		.name = "pq cmd",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_cmd_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_COLORTEMP,
		.name = "set color temperature",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_color_temp_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_MWE,
		.name = "mwe info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_mwe_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_SWDR_INFO,
		.name = "swdr info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_swdr_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_RGB_PATTERN,
		.name = "RGB pattern",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_rgb_pattern_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_BLUE_STRETCH,
		.name = "set blue stretch",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_blue_stretch)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_H_NONLINEARSCALING,
		.name = "set horizontal nonlinear scaling",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_h_nonlinear_scaling_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_MCDI_MODE,
		.name = "enable MCDI",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_mcdi_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_CHROMA,
		.name = "chroma info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_chroma_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_GAMMA,
		.name = "pq gamma",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_gamma_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DLC_INIT,
		.name = "dlc init",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_dlc_init_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DLC_HISTO_RANGE,
		.name = "dlc capture range info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_dlc_capture_range_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DUAL_VIEW_STATUS,
		.name = "dlc capture range info",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_ZERO_FRAME_MODE,
		.name = "get zero frame mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.min = 0,
		.max = 1,
		.def = 0,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DEALYTIME,
		.name = "pq delay time",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_video_delaytime_info)}
		,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_CAPS,
		.name = "pq capability",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_cap_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_VERSION,
		.name = "pq version",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_get_bin_version)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_EVALUATE_FBL,
		.name = "get fbl status",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_fbl_status)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HISTOGRAM,
		.name = "get ip histogram",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_histogram_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_HSY_RANGE,
		.name = "hsy adjust range",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_hsy_range_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_LUMA_CURVE,
		.name = "luma info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_luma_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQMAP_INFO,
		.name = "pqmap info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pqmap_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQ_METADATA,
		.name = "pq metadata",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_metadata)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_PQPARAM,
		.name = "pq parameter",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_pqparam)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &enhance_ctrl_ops,
		.id = V4L2_CID_DIPMAP_INFO,
		.name = "dipmap info",
		.type = V4L2_CTRL_TYPE_U8,
		.min = 0,
		.max = 0xFF,
		.def = 0,
		.step = 1,
		.dims = {sizeof(struct v4l2_dipmap_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops enhance_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops enhance_sd_internal_ops = {
};


//-----------------------------------------------------------------------------
//  FUNCTION
//-----------------------------------------------------------------------------

int mtk_pq_register_enhance_subdev(
	struct v4l2_device *pq_dev,
	struct v4l2_subdev *subdev_enhance,
	struct v4l2_ctrl_handler *enhance_ctrl_handler)
{
	int ret = 0;
	__u32 ctrl_count;
	__u32 ctrl_num_pqd = sizeof(pqd_ctrl) / sizeof(struct v4l2_ctrl_config);
	__u32 ctrl_num_hdr = sizeof(hdr_ctrl) / sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(enhance_ctrl_handler,
		(ctrl_num_pqd + ctrl_num_hdr));

	for (ctrl_count = 0; ctrl_count < ctrl_num_hdr; ctrl_count++) {
		v4l2_ctrl_new_custom(enhance_ctrl_handler,
				     &hdr_ctrl[ctrl_count], NULL);
	}

	for (ctrl_count = 0; ctrl_count < ctrl_num_pqd; ctrl_count++) {
		v4l2_ctrl_new_custom(enhance_ctrl_handler,
				     &pqd_ctrl[ctrl_count], NULL);
	}

	ret = enhance_ctrl_handler->error;
	if (ret) {
		v4l2_err(pq_dev, "failed to create drv pq ctrl handler\n");
		goto exit;
	}
	subdev_enhance->ctrl_handler = enhance_ctrl_handler;

	v4l2_subdev_init(subdev_enhance, &enhance_sd_ops);
	subdev_enhance->internal_ops = &enhance_sd_internal_ops;
	strlcpy(subdev_enhance->name, "mtk-pq-enhance",
		sizeof(subdev_enhance->name));

	ret = v4l2_device_register_subdev(pq_dev, subdev_enhance);
	if (ret) {
		v4l2_err(pq_dev, "failed to register pq enhance subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(enhance_ctrl_handler);
	return ret;
}

void mtk_pq_unregister_enhance_subdev(
	struct v4l2_subdev *subdev_enhance)
{
	v4l2_ctrl_handler_free(subdev_enhance->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_enhance);
}

int mtk_pq_enhance_open(struct mtk_pq_device *pq_dev)
{
	struct mtk_pq_platform_device *pqdev = NULL;

	if ((!pq_dev) || (!pq_dev->dev))
		return -EFAULT;
	pqdev = dev_get_drvdata(pq_dev->dev);

	if (!pq_dev->enhance_info) {
		struct v4l2_enhance_info *enhance_info;

		enhance_info = kzalloc(sizeof(struct v4l2_enhance_info), GFP_KERNEL);
		if (!enhance_info)
			return -EFAULT;
		pq_dev->enhance_info = (void *)enhance_info;

		mtk_hdr_Open(&enhance_info->hdr_info);
		mtk_pqd_Open(&enhance_info->pqd_info);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "pq enhance init success\n");
	} else
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "pq enhance already init\n");

	return 0;
}

int mtk_pq_enhance_close(struct mtk_pq_device *pq_dev)
{
	if ((!pq_dev) || (!pq_dev->dev))
		return -EFAULT;

	if (pq_dev->enhance_info != NULL) {
		struct v4l2_enhance_info *enhance_info;

		enhance_info =
			(struct v4l2_enhance_info *)pq_dev->enhance_info;
		mtk_hdr_Close(&enhance_info->hdr_info);
		mtk_pqd_Close(&enhance_info->pqd_info);
		kfree(enhance_info);
		pq_dev->enhance_info = NULL;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "pq enhance release success\n");
	} else
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "pq enhance already release\n");

	return 0;
}

int mtk_pq_enhance_set_input_source(
	struct mtk_pq_device *pq_dev, __u32 input)
{
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	__u8 u8PQEnhanceInput;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "input source = %d\n", input);

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;
	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	u8PQEnhanceInput = _mtk_pq_enhance_InputConvert(input);

	mtk_hdr_SetInputSource(hdr_info, u8PQEnhanceInput);
	mtk_pqd_SetInputSource(pqd_info, u8PQEnhanceInput);

	return 0;
}

// set format
int mtk_pq_enhance_set_pix_format_in_mplane(
	struct mtk_pq_device *pq_dev, struct v4l2_pix_format_mplane *pix)
{
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	ret = mtk_hdr_SetPixFmtIn(hdr_info, pix);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"HDR pixel format in failed!\n");
		return ret;
	}

	ret = mtk_pqd_SetPixFmtIn(pqd_info, pix);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"PQD pixel format in failed!\n");
		return ret;
	}

	return ret;
}


int mtk_pq_enhance_set_pix_format_out_mplane(
	struct mtk_pq_device *pq_dev, struct v4l2_pix_format_mplane *pix)
{
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	ret = mtk_hdr_SetPixFmtOut(hdr_info, pix);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"HDR pixel format out failed!\n");
		return ret;
	}

	ret = mtk_pqd_SetPixFmtOut(pqd_info, pix);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"PQD pixel format out failed!\n");
		return ret;
	}

	return ret;
}

// VIDIOC_STREAMON
int mtk_pq_enhance_stream_on(struct mtk_pq_device *pq_dev)
{
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	ret = mtk_pqd_StreamOn(pqd_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQD stream on failed!\n");
		return ret;
	}

	//after load pq table
	ret = mtk_hdr_StreamOn(hdr_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "HDR stream on failed!\n");
		return ret;
	}

	return ret;
}

// VIDIOC_STREAMOFF
int mtk_pq_enhance_stream_off(struct mtk_pq_device *pq_dev)
{
	struct v4l2_enhance_info *enhance_info;
	struct st_hdr_info *hdr_info;
	struct st_pqd_info *pqd_info;
	int ret = 0;

	if ((!pq_dev) || (!pq_dev->enhance_info))
		return -EFAULT;

	enhance_info = (struct v4l2_enhance_info *)pq_dev->enhance_info;
	hdr_info = &enhance_info->hdr_info;
	pqd_info = &enhance_info->pqd_info;

	ret = mtk_hdr_StreamOff(hdr_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "HDR stream off failed!\n");
		return ret;
	}

	ret = mtk_pqd_StreamOff(pqd_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQD stream off failed!\n");
		return ret;
	}

	return ret;
}

int mtk_pq_enhance_subscribe_event(
	struct mtk_pq_device *pq_dev, __u32 event_type)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"subscribe event = %d\n", event_type);

	return 0;
}

int mtk_pq_enhance_unsubscribe_event(
	struct mtk_pq_device *pq_dev, __u32 event_type)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "unsubscribe event = %u\n",
		event_type);

	return 0;
}

int mtk_pq_enhance_event_queue(struct mtk_pq_device *pq_dev,
	__u32 event_type)
{
	struct v4l2_event event;
	memset(&event, 0, sizeof(struct v4l2_event));

	if (event_type == HDR_EVENT_TYPE_SEAMLESS_MUTE) {
		if (video_is_registered(&pq_dev->video_dev)) {
			event.type = V4L2_EVENT_HDR_SEAMLESS_MUTE;
			v4l2_event_queue(&pq_dev->video_dev, &event);
		}
	} else if (event_type == HDR_EVENT_TYPE_SEAMLESS_UNMUTE) {
		if (video_is_registered(&pq_dev->video_dev)) {
			event.type = V4L2_EVENT_HDR_SEAMLESS_UNMUTE;
			v4l2_event_queue(&pq_dev->video_dev, &event);
		}
	} else {
		return -EPERM;
	}

	return 0;
}

int mtk_pq_enhance_parse_dts(struct mtk_pq_enhance *pq_enhance,
	struct platform_device *pdev)
{
	int ret = 0;
	struct device *property_dev = &pdev->dev;
	struct device_node *bank_node;
	const char *pq_bin_path;
	__u32 u32Tmp = 0;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	bank_node = of_find_node_by_name(property_dev->of_node, "pq-enhance");

	if (bank_node == NULL) {
		PQ_MSG_ERROR("Failed to get enhance node\r\n");
		goto Fail;
	}

	// read PQ bin path
	ret = of_property_read_string(bank_node, "PQ_BINPATH", &pq_bin_path);
	if (ret) {
		PQ_MSG_ERROR("Failed to get PQ_BINPATH resource\r\n");
		goto Fail;
	}
	strncpy(pq_enhance->pqd_settings.PqBinPath, pq_bin_path,
		V4L2_PQ_FILE_PATH_LENGTH - 1);

	// read is ddr2
	ret = of_property_read_u32(bank_node, "IS_DDR2", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get DDR2 resource\r\n");
		goto Fail;
	}
	pq_enhance->pqd_settings.IsDDR2 = (bool)u32Tmp;

	// read ddr frequency
	ret = of_property_read_u32(bank_node, "DDR_FREQ", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get DDR_FREQ resource\r\n");
		goto Fail;
	}
	pq_enhance->pqd_settings.DDR_Freq = u32Tmp;

	// read bus width
	ret = of_property_read_u32(bank_node, "BUS_WIDTH", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get BUS_WIDTH resource\r\n");
		goto Fail;
	}
	pq_enhance->pqd_settings.Bus_Width = (__u8)u32Tmp;

	// read pq memory address
	ret = of_property_read_u32(bank_node, "PQVR_ADDR", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get PQVR_ADDR resource\r\n");
		goto Fail;
	}
	pq_enhance->pqd_settings.PQVR_Addr = (__u64)u32Tmp;

	// read pq memory length
	ret = of_property_read_u32(bank_node, "PQVR_LEN", &u32Tmp);
	if (ret) {
		PQ_MSG_ERROR("Failed to get PQVR_LEN resource\r\n");
		goto Fail;
	}
	pq_enhance->pqd_settings.PQVR_Len = u32Tmp;
	of_node_put(bank_node);

	return 0;

Fail:
	if (bank_node != NULL)
		of_node_put(bank_node);

	return ret;
}

int mtk_pq_enhance_set_atv_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_atv_ext_info *atv_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"set atv ext info to pq enhance!!!\n");

	return 0;
}

int mtk_pq_enhance_set_hdmi_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_hdmi_ext_info *hdmi_info)
{
	return 0;
}

int mtk_pq_enhance_set_dtv_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_dtv_ext_info *dtv_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"set dtv ext info to pq enhance!!!\n");

	return 0;
}

int mtk_pq_enhance_set_av_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_av_ext_info *av_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"set av ext info to pq enhance!!!\n");

	return 0;
}

int mtk_pq_enhance_set_sv_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_sv_ext_info *sv_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"set sv ext info to pq enhance!!!\n");

	return 0;
}

int mtk_pq_enhance_set_scart_ext_info(
	struct mtk_pq_device *pq_dev, struct v4l2_scart_ext_info *scart_info)
{
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE,
		"set scart ext info to pq enhance!!!\n");

	return 0;
}

static int _mtk_pq_enhance_bypass_show_ip_list(
	char *buf, ssize_t used_count)
{
	int count = 0, ScnSize = 0;

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		"PQ IP Bypass CMD Status:\n");
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		goto exit;

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		" --------------------------------------------------------------------------------------------------------------------\n"
		"| Window | HSY | VIP1 | VIP | AISR_PQ | HVSP_PRE | HVSP_POST | SPF | HDR10 | VOP | DV | MCNR | ABF | SR | DLC | UCD |\n"
		" --------------------------------------------------------------------------------------------------------------------\n");
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		goto exit;

exit:
	return count;
}

static int _mtk_pq_enhance_bypass_show_status(
	struct mtk_pq_platform_device *pqdev, char *buf, ssize_t used_count)
{
	int count = 0, ScnSize = 0;
	unsigned char win_id = 0, ip_idx = 0;
	bool opened = false;
	struct mtk_pq_device *pq_dev = NULL;

	if ((pqdev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	for (win_id = 0; win_id < pqdev->xc_win_num; win_id++) {
		pq_dev = pqdev->pq_devs[win_id];
		if (!pq_dev) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev is NULL!\n");
			continue;
		}

		if (pq_dev->stream_on_ref) {
			opened = true;
			ScnSize = scnprintf(buf + used_count + count,
				SYSFS_MAX_BUF_COUNT - count - used_count,
				"|   %02u   |", win_id);
			count += ScnSize;
			if (!_mtk_pq_enhance_check_scnsize(
					ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
				return count;

			//Print IP bypass status
			for (ip_idx = 0; ip_idx < PQU_IP_BYPASS_NUM; ip_idx++) {
				if (pq_dev->pqu_meta.pq_byp[ip_idx]) {
					switch (ip_idx) {
					case PQU_IP_BYPASS_DV:
					case PQU_IP_BYPASS_SR:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						" ON |");
						break;
					case PQU_IP_BYPASS_HSY:
					case PQU_IP_BYPASS_VIP:
					case PQU_IP_BYPASS_SPF:
					case PQU_IP_BYPASS_VOP:
					case PQU_IP_BYPASS_ABF:
					case PQU_IP_BYPASS_DLC:
					case PQU_IP_BYPASS_UCD:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"  ON |");
						break;
					case PQU_IP_BYPASS_VIP1:
					case PQU_IP_BYPASS_MCNR:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"   ON |");
						break;
					case PQU_IP_BYPASS_HDR10:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"    ON |");
						break;
					case PQU_IP_BYPASS_AISR_PQ:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"      ON |");
						break;
					case PQU_IP_BYPASS_HVSP_PRE:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"       ON |");
						break;
					case PQU_IP_BYPASS_HVSP_POST:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"        ON |");
						break;
					default:
						break;
					}
				} else {
					switch (ip_idx) {
					case PQU_IP_BYPASS_DV:
					case PQU_IP_BYPASS_SR:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						" OFF|");
						break;
					case PQU_IP_BYPASS_HSY:
					case PQU_IP_BYPASS_VIP:
					case PQU_IP_BYPASS_SPF:
					case PQU_IP_BYPASS_VOP:
					case PQU_IP_BYPASS_ABF:
					case PQU_IP_BYPASS_DLC:
					case PQU_IP_BYPASS_UCD:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						" OFF |");
						break;
					case PQU_IP_BYPASS_VIP1:
					case PQU_IP_BYPASS_MCNR:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"  OFF |");
						break;
					case PQU_IP_BYPASS_HDR10:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"   OFF |");
						break;
					case PQU_IP_BYPASS_AISR_PQ:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"     OFF |");
						break;
					case PQU_IP_BYPASS_HVSP_PRE:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"      OFF |");
						break;
					case PQU_IP_BYPASS_HVSP_POST:
						ScnSize = scnprintf(buf + used_count + count,
						SYSFS_MAX_BUF_COUNT - count - used_count,
						"       OFF |");
						break;
					default:
						break;
					}
				}
				count += ScnSize;
				if (!_mtk_pq_enhance_check_scnsize(
						ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
					return count;
			}

			ScnSize = scnprintf(buf + used_count + count,
				SYSFS_MAX_BUF_COUNT - count - used_count,
				"\n --------------------------------------------------------------------------------------------------------------------\n");
			count += ScnSize;
			if (!_mtk_pq_enhance_check_scnsize(
					ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
				return count;
		}
	}
	if (!opened) {
		ScnSize = scnprintf(buf + used_count + count,
			SYSFS_MAX_BUF_COUNT - count - used_count,
			"No Window Opened!\n");
		count += ScnSize;
		if (!_mtk_pq_enhance_check_scnsize(
				ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
			return count;
	}

	return count;
}

static int _mtk_pq_enhance_bypass_show_example(char *buf, ssize_t used_count)
{
	int count = 0, ScnSize = 0;

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		"\nControl Cmd Example:\n"
		"echo window=0 bypass=1 ip=0 > /sys/devices/platform/mtk-pq/mtk_dbg/mtk_pq_bypass\n"
		"Support IP List: [0] HSY [1] VIP1 [2] VIP [3] AISR_PQ [4] HVSP_PRE [5] HVSP_POST [6] SPF\n"
		"                 [7] HDR10 [8] VOP [9] DV [10] MCNR [11] ABF [12] SR [13] DLC [14] UCD\n");
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		return count;

	return count;
}

int mtk_pq_enhance_bypass_cmd_show(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	int count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}

	count += _mtk_pq_enhance_bypass_show_ip_list(buf, count);
	count += _mtk_pq_enhance_bypass_show_status(pqdev, buf, count);
	count += _mtk_pq_enhance_bypass_show_example(buf, count);

	return count;
}

int mtk_pq_enhance_bypass_cmd_store(struct device *dev, const char *buf)
{
	int ret = 0;
	int len = 0;
	int ScnSize = 0;
	__u32 win_id = 0;
	__u32 bypass = 0;
	__u32 ip = 0;
	__u32 print = 0;
	bool find = false;
	char *cmd = NULL;
	char *ptr = NULL;
	if ((buf == NULL) || (dev == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	cmd = vmalloc(sizeof(char) * SYSFS_MAX_BUF_COUNT);
	if (cmd == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "vmalloc cmd fail!\n");
		return -EINVAL;
	}
	memset(cmd, 0, sizeof(char) * SYSFS_MAX_BUF_COUNT);

	len = strlen(buf);
	ScnSize = snprintf(cmd, len + 1, "%s", buf);
	if (!_mtk_pq_enhance_check_scnsize(ScnSize, len + 1)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_enhance_check_scnsize fail!\n");
		ret = -ENOMEM;
		goto exit;
	}

	find = _mtk_pq_enhance_get_dbg_value_from_string(cmd, "window", len, &win_id);
	if (!find) {
		ptr = _mtk_pq_enhance_get_dbg_type_from_string(cmd, "window", len);
		if (ptr == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Cmdline format error, window should be set, please echo help!\n");
			ret = -EINVAL;
			goto exit;
		} else {
			if (strncmp(ptr, "all", 3) == 0)
				win_id = BYPASS_CMD_ALL_WIN;
			else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Cmdline format error, window should be set, please echo help!\n");
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	find = _mtk_pq_enhance_get_dbg_value_from_string(cmd, "ip", len, &ip);
	if (!find) {
		ptr = _mtk_pq_enhance_get_dbg_type_from_string(cmd, "ip", len);
		if (ptr == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Cmdline format error, ip should be set, please echo help!\n");
			ret = -EINVAL;
			goto exit;
		} else {
			if (strncmp(ptr, "all", 3) == 0)
				ip = BYPASS_CMD_ALL_IP;
			else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Cmdline format error, ip should be set, please echo help!\n");
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	find = _mtk_pq_enhance_get_dbg_value_from_string(cmd, "bypass", len, &bypass);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, bypass should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	find = _mtk_pq_enhance_get_dbg_value_from_string(cmd, "print", len, &print);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, show should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	ptr = _mtk_pq_enhance_get_dbg_type_from_string(cmd, "type", len);
	if (ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, type should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}
	if (strncmp(ptr, "hw", 2) == 0)
		mtk_pq_hwbypass_cmd(dev, win_id, bypass, ip, print);
	else if (strncmp(ptr, "sw", 2) == 0)
		mtk_pq_swbypass_cmd(dev, win_id, bypass, ip, print);
	else
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline not support\n");

exit:
	vfree(cmd);
	return ret;
}

static int _mtk_pq_enhance_status_show_info(
	int win, struct mtk_pq_device *pqdev, char *buf, ssize_t used_count)
{
	int count = 0, ScnSize = 0, idx = 0;
	char *IPList[PQU_IP_STATUS_NUM] = {
		"ZNR",
		"DI",
		"ABF",
		"SPF",
		"AISR",
		"VIP1",
		"VIP",
		"HSY"
	};

	if ((pqdev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		" Window ID      :              %02u\n", win);
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(
			ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		return count;

	for (idx = 0; idx < PQU_IP_STATUS_NUM; idx++) {
		ScnSize = scnprintf(buf + used_count + count,
			SYSFS_MAX_BUF_COUNT - count - used_count,
			" %-15s:%16s\n",
			IPList[idx],
			pqdev->pqu_meta.pq_status[idx]?"ON":"OFF");
		count += ScnSize;
		if (!_mtk_pq_enhance_check_scnsize(ScnSize,
				SYSFS_MAX_BUF_COUNT - count - used_count))
			goto exit;
	}

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		"----------------------------------\n");
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(
			ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		return count;

exit:
	return count;
}

int mtk_pq_enhance_status_cmd_show(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	int count = 0, win_id = 0, ScnSize = 0;
	unsigned long wait_win = 0;
	struct timespec ts_start, ts_current;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	memset(&ts_start, 0, sizeof(struct timespec));
	memset(&ts_current, 0, sizeof(struct timespec));

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}

	ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
		"==== PQ status command start =====\n");
	count += ScnSize;
	if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
		return count;

	for (win_id = 0; win_id < pqdev->xc_win_num; win_id++) {
		pq_dev = pqdev->pq_devs[win_id];
		if (!pq_dev) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev is NULL!\n");
			continue;
		}
		if (pq_dev->stream_on_ref) {
			pq_dev->pqu_meta.pq_status_en = true;
			pq_dev->pqu_meta.pq_status_done = false;
			wait_win |= BIT(win_id);
		}
	}
	if (!wait_win) {
		ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
			"No Window Opened!\n");
		count += ScnSize;
		if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
			return count;
		goto OUT;
	}

	ktime_get_ts(&ts_start);

	while (1) {
		ktime_get_ts(&ts_current);

		for (win_id = 0; win_id < pqdev->xc_win_num; win_id++) {
			if (BIT(win_id) & wait_win) {
				pq_dev = pqdev->pq_devs[win_id];
				if (!pq_dev) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev is NULL!\n");
					wait_win -= BIT(win_id);
					continue;
				}
				if (pq_dev->pqu_meta.pq_status_done) {
					//Show status info
					count += _mtk_pq_enhance_status_show_info(win_id,
						pq_dev, buf, count);
					wait_win -= BIT(win_id);
					pq_dev->pqu_meta.pq_status_done = false;
				}
			}
		}

		if (!wait_win) {
			// All window status read done
			ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
				"All window PQ status read back finish.\n");
			count += ScnSize;
			if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
				return count;
			break;
		}
		if (ts_current.tv_sec - ts_start.tv_sec >= STATUS_CMD_TIMEOUT_TIME) {
			// Time out handle
			ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
				"Something ERROR!\nPQ status command time out, please try again later.\n");
			count += ScnSize;
			if (!_mtk_pq_enhance_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
				return count;
			break;
		}
		msleep(STATUS_CMD_WAITTIME_MS);
	}

OUT:
	ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
		"==== PQ status command end =======\n");
	count += ScnSize;
	return count;
}

