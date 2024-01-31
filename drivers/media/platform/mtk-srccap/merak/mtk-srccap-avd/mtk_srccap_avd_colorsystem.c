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
#include <linux/videodev2.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>

#include <linux/mtk-v4l2-srccap.h>
#include "sti_msos.h"
#include "mtk_srccap_avd_colorsystem.h"
#include "mtk_srccap.h"
#include "show_param.h"
#include "avd-ex.h"

/* ============================================================================================== */
/* -------------------------------------- Global Type Definitions ------------------------------- */
/* ============================================================================================== */
static MS_U16 _u16PreVtotal;

#define CROP_Y_ORI              13  ///< 10
#define CROP_H_ORI_PAL     530 ///< 556
#define CROP_H_ORI_NTSC    460 ///< 556

#define MAX_VTOTAL_COUNT                680 ///< 675
#define MID_VTOTAL_COUNT     575
#define MIN_VTOTAL_COUNT     475
#define PAL_VTOTAL_STD     625
#define NTSC_VTOTAL_STD     525

#define DELTA_VTOTAL     5
#define PAL_VTOTAL_MAX     (PAL_VTOTAL_STD+DELTA_VTOTAL)
#define PAL_VTOTAL_MIN     (PAL_VTOTAL_STD-DELTA_VTOTAL)
#define NTSC_VTOTAL_MAX     (NTSC_VTOTAL_STD+DELTA_VTOTAL)
#define NTSC_VTOTAL_MIN     (NTSC_VTOTAL_STD-DELTA_VTOTAL)

/* Crop info : ATV NTSC-M */
#define ATV_NTSC_M_CROPWIN_X 24
#define ATV_NTSC_M_CROPWIN_Y 26
#define ATV_NTSC_M_CROPWIN_Width 1379
#define ATV_NTSC_M_CROPWIN_Height 428
/* Crop info : ATV PAL_BGHI */
#define ATV_PAL_BGHI_CROPWIN_X 24
#define ATV_PAL_BGHI_CROPWIN_Y 36
#define ATV_PAL_BGHI_CROPWIN_Width 1370
#define ATV_PAL_BGHI_CROPWIN_Height 504
/* Crop info : CVBS NTSC-M */
#define CVBS_NTSC_M_CROPWIN_X 24
#define CVBS_NTSC_M_CROPWIN_Y 26
#define CVBS_NTSC_M_CROPWIN_Width 1379
#define CVBS_NTSC_M_CROPWIN_Height 428
/* Crop info : CVBS PAL_BGHI */
#define CVBS_PAL_BGHI_CROPWIN_X 24
#define CVBS_PAL_BGHI_CROPWIN_Y 26
#define CVBS_PAL_BGHI_CROPWIN_Width 1370
#define CVBS_PAL_BGHI_CROPWIN_Height 524

#define RANGE_HANDLE_STABLE_COUNTER     20
#define OUT_OF_HANDLE_STABLE_COUNTER    0xFF

v4l2_std_id u64TvSystem;
const struct std_descr standards[] = {
	{ V4L2_STD_NTSC,         "NTSC"      },
	{ V4L2_STD_NTSC_M,       "NTSC-M"    },
	{ V4L2_STD_NTSC_M_JP,    "NTSC-M-JP" },
	{ V4L2_STD_NTSC_M_KR,    "NTSC-M-KR" },
	{ V4L2_STD_NTSC_443,     "NTSC-443"  },
	{ V4L2_STD_PAL,          "PAL"       },
	{ V4L2_STD_PAL_BG,       "PAL-BG"    },
	{ V4L2_STD_PAL_B,        "PAL-B"     },
	{ V4L2_STD_PAL_B1,       "PAL-B1"    },
	{ V4L2_STD_PAL_G,        "PAL-G"     },
	{ V4L2_STD_PAL_H,        "PAL-H"     },
	{ V4L2_STD_PAL_I,        "PAL-I"     },
	{ V4L2_STD_PAL_DK,       "PAL-DK"    },
	{ V4L2_STD_PAL_D,        "PAL-D"     },
	{ V4L2_STD_PAL_D1,       "PAL-D1"    },
	{ V4L2_STD_PAL_K,        "PAL-K"     },
	{ V4L2_STD_PAL_M,        "PAL-M"     },
	{ V4L2_STD_PAL_N,        "PAL-N"     },
	{ V4L2_STD_PAL_Nc,       "PAL-Nc"    },
	{ V4L2_STD_PAL_60,       "PAL-60"    },
	{ V4L2_STD_SECAM,        "SECAM"     },
	{ V4L2_STD_SECAM_B,      "SECAM-B"   },
	{ V4L2_STD_SECAM_G,      "SECAM-G"   },
	{ V4L2_STD_SECAM_H,      "SECAM-H"   },
	{ V4L2_STD_SECAM_DK,     "SECAM-DK"  },
	{ V4L2_STD_SECAM_D,      "SECAM-D"   },
	{ V4L2_STD_SECAM_K,      "SECAM-K"   },
	{ V4L2_STD_SECAM_K1,     "SECAM-K1"  },
	{ V4L2_STD_SECAM_L,      "SECAM-L"   },
	{ V4L2_STD_SECAM_LC,     "SECAM-Lc"  },
	{ 0,                     "AUTO"   }
};
/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static enum v4l2_ext_avd_videostandardtype _MappingV4l2stdToVideoStandard(
			v4l2_std_id std_id);
static v4l2_std_id _MappingVideoStandardToV4l2std(
		enum v4l2_ext_avd_videostandardtype eVideoStandardType);
static bool _IsVifChLock(struct mtk_srccap_dev *srccap_dev);
static bool _IsVdScanDetect(struct mtk_srccap_dev *srccap_dev);
static enum V4L2_AVD_RESULT _mtk_avd_get_status(struct mtk_srccap_dev *srccap_dev,
	__u16 *u16Status);
static enum V4L2_AVD_RESULT _mtk_avd_set_debunce_count(struct mtk_srccap_dev *srccap_dev,
	struct stAvdCount *AvdCount);
static enum V4L2_AVD_RESULT _mtk_avd_get_notify_event(struct mtk_srccap_dev *srccap_dev,
	enum v4l2_timingdetect_status *pNotify);
static enum V4L2_AVD_RESULT _mtk_avd_set_timeout(struct mtk_srccap_dev *srccap_dev,
	struct stAvdCount *AvdCount);
static enum V4L2_AVD_RESULT _mtk_avd_set_signal_stage(struct mtk_srccap_dev *srccap_dev,
		enum V4L2_AVD_SIGNAL_STAGE enStg);
static enum V4L2_AVD_RESULT _mtk_avd_field_signal_patch(struct mtk_srccap_dev *srccap_dev,
	__u16 u16Status);
static enum V4L2_AVD_RESULT _mtk_avd_match_vd_sampling(
	enum v4l2_ext_avd_videostandardtype videotype,
	struct srccap_avd_vd_sampling *vdsampling_table,
	u16 table_entry_cnt,
	u16 *vdsampling_table_index);
const char *InputSourceToString(
	enum v4l2_srccap_input_source eInputSource)
{
	char *src = NULL;

	switch (eInputSource) {
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		src = "Cvbs";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		src = "Svideo";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		src = "Atv";
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		src = "Scart";
		break;
	default:
		src = "Other";
		break;
	}

	return src;
}

const char *ScanTypeToString(u8 ScanType)
{
	char *Type = NULL;

	if (ScanType == SCAN_MODE)
		Type = "Scan";
	else if (ScanType == PLAY_MODE)
		Type = "Play";

	return Type;
}

const char *VideoStandardToString(v4l2_std_id id)
{
	v4l2_std_id myid = id;
	int i;

	for (i = 0 ; standards[i].std ; i++)
		if (myid == standards[i].std)
			break;
	return standards[i].descr;
}

const char *VdStandardToString(
			enum v4l2_ext_avd_videostandardtype eVideoStandardType)
{
	char *standard = NULL;

	switch (eVideoStandardType) {
	case V4L2_EXT_AVD_STD_PAL_BGHI:
		standard = "PAL_BGHI";
		break;
	case V4L2_EXT_AVD_STD_NTSC_M:
		standard = "NTSC_M";
		break;
	case V4L2_EXT_AVD_STD_SECAM:
		standard = "SECAM";
		break;
	case V4L2_EXT_AVD_STD_NTSC_44:
		standard = "NTSC_443";
		break;
	case V4L2_EXT_AVD_STD_PAL_M:
		standard = "PAL_M";
		break;
	case V4L2_EXT_AVD_STD_PAL_N:
		standard = "PAL_N";
		break;
	case V4L2_EXT_AVD_STD_PAL_60:
		standard = "PAL_60";
		break;
	case V4L2_EXT_AVD_STD_NOTSTANDARD:
		standard = "NOT_STANDARD";
		break;
	case V4L2_EXT_AVD_STD_AUTO:
		standard = "AUTO";
		break;
	default:
		standard = "NOT_STANDARD";
		break;
	}
	return standard;
}

const char *SignalStatusToString(
			enum v4l2_timingdetect_status enStatus)
{
	char *status = NULL;

	switch (enStatus) {
	case V4L2_TIMINGDETECT_NO_SIGNAL:
		status = "AVD_NO_SIGNAL";
		break;
	case V4L2_TIMINGDETECT_STABLE_SYNC:
		status = "AVD_STABLE_SYNC";
		break;
	case V4L2_TIMINGDETECT_UNSTABLE_SYNC:
		status = "AVD_UNSTABLE";
		break;
	default:
		status = "Out of status";
		break;
	}
	return status;
}

const char *SignalStageToString(
			int enStage)
{
	char *stage = NULL;

	switch ((enum V4L2_AVD_SIGNAL_STAGE)enStage) {
	case V4L2_SIGNAL_CHANGE_FALSE:
		stage = "V4L2_SIGNAL_CHANGE_FALSE";
		break;
	case V4L2_SIGNAL_CHANGE_TRUE:
		stage = "V4L2_SIGNAL_CHANGE_TRUE";
		break;
	case V4L2_SIGNAL_KEEP_TRUE:
		stage = "V4L2_SIGNAL_KEEP_TRUE";
		break;
	case V4L2_SIGNAL_KEEP_FALSE:
		stage = "V4L2_SIGNAL_KEEP_FALSE";
		break;
	default:
		stage = "Out of stage";
		break;
	}
	return stage;
}

static enum v4l2_ext_avd_videostandardtype _MappingV4l2stdToVideoStandard(
							v4l2_std_id std_id)
{
	enum v4l2_ext_avd_videostandardtype eVideoStandardType;

	switch (std_id) {
	case V4L2_STD_PAL:
		eVideoStandardType = V4L2_EXT_AVD_STD_PAL_BGHI;
		break;
	case V4L2_STD_NTSC:
		eVideoStandardType = V4L2_EXT_AVD_STD_NTSC_M;
		break;
	case V4L2_STD_SECAM:
		eVideoStandardType = V4L2_EXT_AVD_STD_SECAM;
		break;
	case V4L2_STD_NTSC_443:
		eVideoStandardType = V4L2_EXT_AVD_STD_NTSC_44;
		break;
	case V4L2_STD_PAL_M:
		eVideoStandardType = V4L2_EXT_AVD_STD_PAL_M;
		break;
	case V4L2_STD_PAL_N:
		eVideoStandardType = V4L2_EXT_AVD_STD_PAL_N;
		break;
	case V4L2_STD_PAL_60:
		eVideoStandardType = V4L2_EXT_AVD_STD_PAL_60;
		break;
	case V4L2_STD_UNKNOWN:
		eVideoStandardType = V4L2_EXT_AVD_STD_NOTSTANDARD;
		break;
	case V4L2_STD_ALL:
		eVideoStandardType = V4L2_EXT_AVD_STD_AUTO;
		break;
	default:
		eVideoStandardType = V4L2_EXT_AVD_STD_NOTSTANDARD;
		break;
	}
	return eVideoStandardType;
}

static v4l2_std_id _MappingVideoStandardToV4l2std(
			enum v4l2_ext_avd_videostandardtype eVideoStandardType)
{
	v4l2_std_id std_id;

	switch (eVideoStandardType) {
	case V4L2_EXT_AVD_STD_PAL_BGHI:
		std_id = V4L2_STD_PAL;
		break;
	case V4L2_EXT_AVD_STD_NTSC_M:
		std_id = V4L2_STD_NTSC;
		break;
	case V4L2_EXT_AVD_STD_SECAM:
		std_id = V4L2_STD_SECAM;
		break;
	case V4L2_EXT_AVD_STD_NTSC_44:
		std_id = V4L2_STD_NTSC_443;
		break;
	case V4L2_EXT_AVD_STD_PAL_M:
		std_id = V4L2_STD_PAL_M;
		break;
	case V4L2_EXT_AVD_STD_PAL_N:
		std_id = V4L2_STD_PAL_N;
		break;
	case V4L2_EXT_AVD_STD_PAL_60:
		std_id = V4L2_STD_PAL_60;
		break;
	case V4L2_EXT_AVD_STD_NOTSTANDARD:
		std_id = V4L2_STD_UNKNOWN;
		break;
	case V4L2_EXT_AVD_STD_AUTO:
		std_id = V4L2_STD_ALL;
		break;
	default:
		std_id = V4L2_STD_UNKNOWN;
		break;
	}
	return std_id;
}

static bool _CheckLock(__u16 curStatus, __u16 precurStatus)
{
	if (IsAVDLOCK(curStatus) && (curStatus == precurStatus))
		return TRUE;
	else
		return FALSE;
}

static bool _IsVifChLock(struct mtk_srccap_dev *srccap_dev)
{
	bool bVifLock;

	bVifLock = srccap_dev->avd_info.bIsVifLock;

	if (bVifLock == TRUE)
		return TRUE;
	else
		return FALSE;
}

static bool _IsVdScanDetect(struct mtk_srccap_dev *srccap_dev)
{
	static bool bpreDetect = FALSE;
	bool bVdDetect;

	bVdDetect = srccap_dev->avd_info.bStrAtvVdDet;

	/* Restart scan detect. The unstable signal to be sent again.*/
	if ((srccap_dev->avd_info.bStopAtvVdDet) && (bpreDetect == TRUE)) {
		bpreDetect = FALSE;
		SRCCAP_AVD_MSG_INFO("[AVD][Cancel][bpreDetect : %d]Reset scan detect.", bpreDetect);
	}

	if (bpreDetect == bVdDetect)
		return FALSE;

	SRCCAP_AVD_MSG_INFO("[AVD][bpreDetect : %d][bVdDetect : %d]", bpreDetect, bVdDetect);

	bpreDetect = bVdDetect;

	if (bVdDetect == TRUE)
		return TRUE;
	else
		return FALSE;
}

static enum V4L2_AVD_RESULT _mtk_avd_get_status(struct mtk_srccap_dev *srccap_dev,
	__u16 *u16Status)
{
	struct v4l2_ext_avd_scan_hsyc_check stScanStatus;
	struct v4l2_ext_avd_standard_detection psStatus;
	bool ScanType;

	ScanType = (bool)srccap_dev->avd_info.bIsATVScanMode;

	if (ScanType == TRUE) {
		memset(&stScanStatus, 0, sizeof(struct v4l2_ext_avd_scan_hsyc_check));
		stScanStatus.u8HtotalTolerance = 20;
		mtk_avd_scan_hsync_check(&stScanStatus);
		*u16Status = stScanStatus.u16ScanHsyncCheck;
	} else if (ScanType == FALSE) {
		memset(&psStatus, 0, sizeof(struct v4l2_ext_avd_standard_detection));
		mtk_avd_standard_detetion(&psStatus);
		*u16Status = psStatus.u16LatchStatus;
	} else {
		SRCCAP_AVD_MSG_ERROR("[AVD]InValid Scan Type.\n");
		return V4L2_EXT_AVD_NOT_SUPPORT;
	}

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_set_debunce_count(struct mtk_srccap_dev *srccap_dev,
	struct stAvdCount *AvdCount)
{
	bool ScanType;
	enum v4l2_srccap_input_source eSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (AvdCount == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ScanType = (bool)srccap_dev->avd_info.bIsATVScanMode;
	eSrc = srccap_dev->srccap_info.src;

	switch (eSrc) {
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		if (ScanType == FALSE) {
			AvdCount[PLAY_MODE].u8StableCheckCount =
				_AVD_CVBS_PLAY_STABLE_CHECK_COUNT;
			AvdCount[PLAY_MODE].u8PollingTime =
				_AVD_CVBS_PLAY_HSYNC_POLLING_TIME;
			AvdCount[PLAY_MODE].u8CheckCount =
				_AVD_CVBS_PLAY_CHECK_COUNT;
			AvdCount[PLAY_MODE].u8Timeout =
				_AVD_CVBS_PLAY_TIME_OUT;
		} else {
			SRCCAP_AVD_MSG_ERROR("[AVD]InValid Scan Type.\n");
			return V4L2_EXT_AVD_NOT_SUPPORT;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		if (ScanType == TRUE) {
			if (srccap_dev->avd_info.region_type & V4L2_STD_PAL_60) {
				AvdCount[SCAN_MODE].u8StableCheckCount =
					_AVD_ATV_PAL60_SCAN_STABLE_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8PollingTime =
					_AVD_ATV_PAL60_SCAN_HSYNC_POLLING_TIME;
				AvdCount[SCAN_MODE].u8CheckCount =
					_AVD_ATV_PAL60_SCAN_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8Timeout =
					_AVD_ATV_PAL60_SCAN_TIME_OUT;
			} else if (srccap_dev->avd_info.region_type & V4L2_STD_PAL_M) {
				AvdCount[SCAN_MODE].u8StableCheckCount =
					_AVD_ATV_PALM_SCAN_STABLE_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8PollingTime =
					_AVD_ATV_PALM_SCAN_HSYNC_POLLING_TIME;
				AvdCount[SCAN_MODE].u8CheckCount =
					_AVD_ATV_PALM_SCAN_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8Timeout =
					_AVD_ATV_PALM_SCAN_TIME_OUT;
			} else {
				AvdCount[SCAN_MODE].u8StableCheckCount =
					_AVD_ATV_SCAN_STABLE_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8PollingTime =
					_AVD_ATV_SCAN_HSYNC_POLLING_TIME;
				AvdCount[SCAN_MODE].u8CheckCount =
					_AVD_ATV_SCAN_CHECK_COUNT;
				AvdCount[SCAN_MODE].u8Timeout =
					_AVD_ATV_SCAN_TIME_OUT;
			}
		} else if (ScanType == FALSE) {
			AvdCount[PLAY_MODE].u8StableCheckCount =
				_AVD_ATV_PLAY_STABLE_CHECK_COUNT;
			AvdCount[PLAY_MODE].u8PollingTime =
				_AVD_ATV_PLAY_HSYNC_POLLING_TIME;
			AvdCount[PLAY_MODE].u8CheckCount =
				_AVD_ATV_PLAY_CHECK_COUNT;
			AvdCount[PLAY_MODE].u8Timeout =
				_AVD_ATV_PLAY_TIME_OUT;
		} else {
			SRCCAP_AVD_MSG_ERROR("[AVD]InValid Scan Type.\n");
			return V4L2_EXT_AVD_NOT_SUPPORT;
		}
		break;
	default:
		SRCCAP_AVD_MSG_ERROR("[AVD]InValid Source Type.\n");
		return V4L2_EXT_AVD_NOT_SUPPORT;
		break;
	}
	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_get_notify_event(struct mtk_srccap_dev *srccap_dev,
	enum v4l2_timingdetect_status *pNotify)
{
	v4l2_std_id system = 0;
	enum V4L2_AVD_SIGNAL_STAGE Stage = 0;
	bool ScanMode = 0;
	bool TimeOut = 0;
	enum v4l2_srccap_input_source eSrc = 0;
	bool bIsVifLock  = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	system = srccap_dev->avd_info.u64DetectTvSystem;
	Stage = srccap_dev->avd_info.enSignalStage;
	ScanMode = srccap_dev->avd_info.bIsATVScanMode;
	TimeOut = srccap_dev->avd_info.bIsTimeOut;
	eSrc = srccap_dev->srccap_info.src;
	bIsVifLock  = srccap_dev->avd_info.bIsVifLock;

	if (pNotify == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (ScanMode == SCAN_MODE) {
		if (system != V4L2_STD_UNKNOWN)
			*pNotify = V4L2_TIMINGDETECT_STABLE_SYNC;
		else if (system == V4L2_STD_UNKNOWN)
			*pNotify = (TimeOut) ? V4L2_TIMINGDETECT_NO_SIGNAL :
			V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		else
			SRCCAP_AVD_MSG_INFO("[AVD][Input : %s][Type : %s]:InValid Case.",
				InputSourceToString(eSrc),
				ScanTypeToString(ScanMode));

		//Vd detect finish at this time.
		if ((*pNotify != V4L2_TIMINGDETECT_UNSTABLE_SYNC) &&
			(bIsVifLock == true)) {
			srccap_dev->avd_info.bIsVifLock = FALSE;
			srccap_dev->avd_info.bStrAtvVdDet = FALSE;
			SRCCAP_AVD_MSG_INFO(
				"[AVD][Input:%s][Type:%s]bIsVifLock=(%d),bStrAtvVdDet=(%d)",
			InputSourceToString(eSrc),
			ScanTypeToString(ScanMode),
			srccap_dev->avd_info.bIsVifLock,
			srccap_dev->avd_info.bStrAtvVdDet);
		}

	} else if (ScanMode == PLAY_MODE) {
		switch (Stage) {
		case V4L2_SIGNAL_CHANGE_FALSE:
			srccap_dev->avd_info.u64DetectTvSystem = V4L2_STD_UNKNOWN;
			*pNotify = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
			break;
		case V4L2_SIGNAL_CHANGE_TRUE:
			if (system != V4L2_STD_UNKNOWN)
				*pNotify = V4L2_TIMINGDETECT_STABLE_SYNC;
			break;
		case V4L2_SIGNAL_KEEP_TRUE:
			*pNotify = V4L2_TIMINGDETECT_STABLE_SYNC;
			break;
		case V4L2_SIGNAL_KEEP_FALSE:
			*pNotify = V4L2_TIMINGDETECT_NO_SIGNAL;
			break;
		default:
			SRCCAP_AVD_MSG_ERROR("[AVD][Input : %s][Type : %s]:InValid Case.",
				InputSourceToString(eSrc),
				ScanTypeToString(ScanMode));
			break;
		}
	} else
		SRCCAP_AVD_MSG_ERROR("[AVD][Input : %s][Type : %s]:InValid Type.",
			InputSourceToString(eSrc),
			ScanTypeToString(ScanMode));

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_set_timeout(struct mtk_srccap_dev *srccap_dev,
	struct stAvdCount *AvdCount)
{
	bool ScanMode = 0;
	bool TimeOut = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ScanMode = srccap_dev->avd_info.bIsATVScanMode;
	TimeOut = srccap_dev->avd_info.bIsTimeOut;

	if (ScanMode == SCAN_MODE) {
		/*Stopping time-out and wait for a new trigger of the scan detection*/
		if (srccap_dev->avd_info.bStopAtvVdDet)
			return V4L2_EXT_AVD_OK;

		if (TimeOut == FALSE)
			SRCCAP_AVD_MSG_INFO("[AVD][Type:%s]Reach detect time(%d)\n",
			ScanTypeToString(ScanMode), AvdCount[ScanMode].u8Timeout);
		srccap_dev->avd_info.bIsTimeOut = TRUE;
	}

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_field_signal_patch(struct mtk_srccap_dev *srccap_dev,
	__u16 u16Status)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((u64TvSystem == V4L2_STD_UNKNOWN) && IsAVDLOCK(u16Status)) {
		if (IsVSYNCTYPE(u16Status))
			u64TvSystem = V4L2_STD_PAL;
		else
			u64TvSystem = V4L2_STD_NTSC;
	}

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_set_signal_stage(struct mtk_srccap_dev *srccap_dev,
		enum V4L2_AVD_SIGNAL_STAGE cntStg)
{
	enum V4L2_AVD_SIGNAL_STAGE preStg = 0;
	enum v4l2_srccap_input_source eSrc = 0;
	bool ScanMode = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	preStg = srccap_dev->avd_info.enSignalStage;
	eSrc = srccap_dev->srccap_info.src;
	ScanMode = srccap_dev->avd_info.bIsATVScanMode;

	srccap_dev->avd_info.enSignalStage = cntStg;

	if (preStg != cntStg)
		switch (cntStg) {
		case V4L2_SIGNAL_CHANGE_FALSE:
		case V4L2_SIGNAL_CHANGE_TRUE:
		case V4L2_SIGNAL_KEEP_TRUE:
		case V4L2_SIGNAL_KEEP_FALSE:
			SRCCAP_AVD_MSG_INFO("[AVD]%s.\n", SignalStageToString((int)cntStg));
			break;
		default:
			SRCCAP_AVD_MSG_INFO("[AVD][Input : %s][Type : %s]:InValid Stage.",
				InputSourceToString(eSrc),
				ScanTypeToString(ScanMode));
			return V4L2_EXT_AVD_NOT_SUPPORT;
		}
	return V4L2_EXT_AVD_OK;
}

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
enum V4L2_AVD_RESULT mtk_avd_SetColorSystem(v4l2_std_id std)
{
	MS_U16 colorSystem = 0;
	MS_U8 u8Value;

	SRCCAP_AVD_MSG_INFO("in\n");
	//Set color system NTSC-M enable
	if ((std & V4L2_STD_NTSC) == V4L2_STD_NTSC) {
		colorSystem |= (BIT(0));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_NTSC]\n");
	}

	//Set color system PAL enable
	if ((std & V4L2_STD_PAL) == V4L2_STD_PAL) {
		colorSystem |= (BIT(1));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_PAL]\n");
	}

	//Set color system PAL-Nc enable
	if ((std & V4L2_STD_PAL_Nc) == V4L2_STD_PAL_Nc) {
		colorSystem |= (BIT(2));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_PAL_Nc]\n");
	}

	//Set color system PAL-M enable
	if ((std & V4L2_STD_PAL_M) == V4L2_STD_PAL_M) {
		colorSystem |= (BIT(3));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_PAL_M]\n");
	}

	//Set color system SECAM enable
	if ((std & V4L2_STD_SECAM) ==  V4L2_STD_SECAM) {
		colorSystem |= (BIT(4));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_SECAM]\n");
	}

	//Set color system NTSC443 enable
	if ((std & V4L2_STD_NTSC_443) == V4L2_STD_NTSC_443) {
		colorSystem |= (BIT(5));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_NTSC_443]\n");
	}

	//Set color system PAL60 enable
	if ((std & V4L2_STD_PAL_60) == V4L2_STD_PAL_60) {
		colorSystem |= (BIT(6));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_PAL_60]\n");
	}

	//Set a color system NTSC, PAL-M, NTSC-443, PAL-60 enable
	if ((std & V4L2_STD_525_60) == V4L2_STD_525_60) {
		colorSystem |= (BIT(0) | BIT(3) | BIT(5) | BIT(6));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_525_60]\n");
	}

	//Set a color system PAL,PAL-N, SECAM enable
	if ((std & V4L2_STD_625_50) == V4L2_STD_625_50) {
		colorSystem |= (BIT(1) | BIT(2) | BIT(4));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_625_50]\n");
	}

	//Set all color system enable
	if ((std & V4L2_STD_ALL) == V4L2_STD_ALL) {
		colorSystem |=
		(BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6));
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_ALL]\n");
	}

	if (std == V4L2_STD_UNKNOWN)
		SRCCAP_AVD_MSG_INFO("[V4L2_STD_UNKNOWN]\n");

	u8Value = Api_AVD_GetStandard_Detection_Flag();

	//all disable
	u8Value |=  (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(6));

	if (colorSystem&(BIT(0)))	//Set color system NTSC-M enable
		u8Value &= ~(BIT(2));

	if (colorSystem&(BIT(1)))	//Set color system PAL enable
		u8Value &= ~(BIT(0));

	if (colorSystem&(BIT(2)))	//Set color system PAL-Nc enable
		u8Value &= ~(BIT(6));

	if (colorSystem&(BIT(3)))	//Set color system PAL-M enable
		u8Value &= ~(BIT(4));

	if (colorSystem&(BIT(4)))	//Set color system SECAM enable
		u8Value &= ~(BIT(1));

	if (colorSystem&(BIT(5)))	//Set color system NTSC443 enable
		u8Value &= ~(BIT(3));

	if (colorSystem&(BIT(6)))	//Set color system PAL60 enable
		u8Value &= ~(BIT(0));

	// Reset dsp when we change color system
	u8Value |= BIT(7);
	Api_AVD_SetStandard_Detection_Flag(u8Value);

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_standard_detetion(
		struct v4l2_ext_avd_standard_detection *pDetection)
{

#ifdef DECOMPOSE
	enum v4l2_ext_avd_videostandardtype eVideoStandardType = 0;
	unsigned short u16LatchStatus = 0;
#endif

	SRCCAP_AVD_MSG_DEBUG("in\n");

#ifdef DECOMPOSE
	eVideoStandardType = (int)Api_AVD_GetStandardDetection(&u16LatchStatus);
	pDetection->u64VideoStandardType =
		_MappingVideoStandardToV4l2std(eVideoStandardType);
	pDetection->u16LatchStatus = u16LatchStatus;

	if ((pDetection->u64VideoStandardType == V4L2_STD_UNKNOWN) &&
		IsAVDLOCK(pDetection->u16LatchStatus)) {
		if (IsVSYNCTYPE(pDetection->u64VideoStandardType))
			pDetection->u64VideoStandardType = V4L2_STD_PAL;
		else
			pDetection->u64VideoStandardType = V4L2_STD_NTSC;
	}
#endif

#if (TEST_MODE) //For test
	pDetection->u64VideoStandardType = V4L2_STD_NTSC;
	pDetection->u16LatchStatus = 0xE443;
#endif

	SRCCAP_AVD_MSG_DEBUG("\t Videostandardtype={%s}\n",
	VideoStandardToString(pDetection->u64VideoStandardType));
	show_v4l2_ext_avd_standard_detection(pDetection);

	SRCCAP_AVD_MSG_DEBUG("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_status(unsigned short *u16vdstatus)
{
	SRCCAP_AVD_MSG_DEBUG("in\n");

#ifdef DECOMPOSE
	if (u16vdstatus == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}
	*u16vdstatus = Api_AVD_GetStatus();
#endif

#if (TEST_MODE) //For test
	*u16vdstatus = 0xE443;
#endif

	show_v4l2_ext_avd_status(u16vdstatus);

	SRCCAP_AVD_MSG_DEBUG("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_force_video_standard(
				v4l2_std_id *u64Videostandardtype)
{
#ifdef DECOMPOSE
	enum v4l2_ext_avd_videostandardtype eVideoStandardType;
#endif

	SRCCAP_AVD_MSG_INFO("in\n");
	SRCCAP_AVD_MSG_INFO("\t Videostandardtype={%s(0x%02x)}\n",
		VideoStandardToString(*u64Videostandardtype),
		_MappingV4l2stdToVideoStandard(*u64Videostandardtype));
	show_v4l2_ext_avd_force_video_standard(u64Videostandardtype);

#ifdef DECOMPOSE
	eVideoStandardType =
		_MappingV4l2stdToVideoStandard(*u64Videostandardtype);

	if (eVideoStandardType > V4L2_EXT_AVD_STD_AUTO)
		SRCCAP_AVD_MSG_ERROR("Out of video standard(%d).\n", eVideoStandardType);

	if (Api_AVD_ForceVideoStandard((int)eVideoStandardType) == 0) {
		SRCCAP_AVD_MSG_ERROR("\t Videostandardtype={0x%02x, }\n",
			_MappingV4l2stdToVideoStandard(*u64Videostandardtype));
		show_v4l2_ext_avd_force_video_standard(u64Videostandardtype);
		SRCCAP_AVD_MSG_ERROR("Fail(-1)\n");
		return V4L2_EXT_AVD_FAIL;
	}
#endif

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_video_standard(
		struct v4L2_ext_avd_video_standard *pVideoStandard)
{
#ifdef DECOMPOSE
	enum v4l2_ext_avd_videostandardtype eVideoStandardType;
#endif

	SRCCAP_AVD_MSG_INFO("in\n");

	SRCCAP_AVD_MSG_INFO("\t Videostandardtype={%s(0x%02x) }\n",
		VideoStandardToString(pVideoStandard->u64Videostandardtype),
		_MappingV4l2stdToVideoStandard(pVideoStandard->u64Videostandardtype));
	show_v4L2_ext_avd_video_standard(pVideoStandard);

#ifdef DECOMPOSE
	eVideoStandardType =
	_MappingV4l2stdToVideoStandard(pVideoStandard->u64Videostandardtype);

	if (eVideoStandardType > V4L2_EXT_AVD_STD_AUTO)
		SRCCAP_AVD_MSG_ERROR("Out of video standard(%d).\n", eVideoStandardType);

	if (Api_AVD_SetVideoStandard((int)eVideoStandardType,
				pVideoStandard->bIsInAutoTuning) == 0) {
		SRCCAP_AVD_MSG_ERROR("\t Videostandardtype={0x%02x, }\n",
			_MappingV4l2stdToVideoStandard(pVideoStandard->u64Videostandardtype));
		show_v4L2_ext_avd_video_standard(pVideoStandard);
		SRCCAP_AVD_MSG_ERROR("Fail(-1)\n");
		return V4L2_EXT_AVD_FAIL;
	}
#endif

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_start_auto_standard_detetion(void)
{
	SRCCAP_AVD_MSG_INFO("in\n");

#ifdef DECOMPOSE
	Api_AVD_StartAutoStandardDetection();
#endif

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_reg_from_dsp(void)
{
	SRCCAP_AVD_MSG_DEBUG("in\n");

#ifdef DECOMPOSE
	Api_AVD_SetRegFromDSP();
#endif

	SRCCAP_AVD_MSG_DEBUG("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_scan_hsync_check(
			struct v4l2_ext_avd_scan_hsyc_check *pHsync)
{
	SRCCAP_AVD_MSG_DEBUG("in\n");

#ifdef DECOMPOSE
	if (pHsync == NULL) {
		SRCCAP_AVD_MSG_POINTER_CHECK();
		return -EINVAL;
	}
	pHsync->u16ScanHsyncCheck =
	Api_AVD_Scan_HsyncCheck(pHsync->u8HtotalTolerance);
#endif

#if (TEST_MODE) //For test
	pHsync->u16ScanHsyncCheck = 0xE443;
#endif

	show_v4l2_ext_avd_scan_hsyc_check(pHsync);

	SRCCAP_AVD_MSG_DEBUG("out\n");
	return V4L2_EXT_AVD_OK;
}

static bool _mtk_avd_compare_source(struct mtk_srccap_dev *srccap_dev)
{
	enum v4l2_srccap_input_source eSrc, epreSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	epreSrc = srccap_dev->avd_info.epresrc;

	//retuen TRUE if It's different between pre-source and crn-source.
	if (epreSrc != eSrc) {
		SRCCAP_AVD_MSG_INFO("[AVD][Pre-source : %s][Crn-source : %s] Init paramete.\n",
			InputSourceToString(epreSrc), InputSourceToString(eSrc));
		return TRUE;
	} else
		return FALSE;
}

static bool _mtk_avd_idle_check(struct mtk_srccap_dev *srccap_dev)
{
	enum V4L2_AVD_STATUS_TYPE eAvdStatus;
	__u8 u8AtvScan;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eAvdStatus = srccap_dev->avd_info.eAvdStatus;
	u8AtvScan = srccap_dev->avd_info.bIsATVScanMode;

	if (u8AtvScan == TRUE)
		srccap_dev->avd_info.eAvdStatus = V4L2_STATUS_TYPE_SCAN;

	if (eAvdStatus == V4L2_STATUS_TYPE_IDLE) {
		SRCCAP_AVD_MSG_DEBUG("[AVD]Current ______ IDLE______ mode.\n");
		return TRUE;
	} else {
		return FALSE;
	}
}

static enum V4L2_AVD_RESULT _mtk_avd_store_PreSource(struct mtk_srccap_dev *srccap_dev)
{
	enum v4l2_srccap_input_source eSrc, epreSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	epreSrc = srccap_dev->avd_info.epresrc;

	//Store the source if source be changed from vd source(CVBS) to anather one(ATV).
	if (epreSrc != eSrc) {
		srccap_dev->avd_info.epresrc = eSrc;
		SRCCAP_AVD_MSG_INFO("[AVD][Store Previous source : %s]\n",
			InputSourceToString(srccap_dev->avd_info.epresrc));
	}

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT _mtk_avd_store_StableTiming(struct mtk_srccap_dev *srccap_dev)
{
	bool bSnow;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	bSnow = srccap_dev->avd_info.bAtvSnow;

	if (bSnow) {
		/*Atv snow mode, force fake timing*/
		srccap_dev->avd_info.eStableTvsystem = V4L2_EXT_AVD_STD_NTSC_M;
		SRCCAP_AVD_MSG_INFO("[SnowFlow]bAtvSnow = %d, eStableTvsystem = %d\n",
			bSnow, srccap_dev->avd_info.eStableTvsystem);
	} else {
		/*Normal mode, result from timing detect*/
		srccap_dev->avd_info.eStableTvsystem =
			_MappingV4l2stdToVideoStandard(srccap_dev->avd_info.u64DetectTvSystem);
	}

	return V4L2_EXT_AVD_OK;
}

static void _mtk_avd_delay_ms(MS_U32 u32Ms)
{
	#define DELAY_MS 1000
	#define DELAY_RANGE 1000

	MS_U32 u32DelayTime = 0;

	u32DelayTime = u32Ms * DELAY_MS;
	usleep_range(u32DelayTime, (DELAY_RANGE + u32DelayTime));
}

enum V4L2_AVD_RESULT mtk_avd_SourceSignalMonitor(enum v4l2_timingdetect_status *pAvdNotify,
			struct mtk_srccap_dev *srccap_dev)
{
	enum V4L2_AVD_RESULT ret;

	if (pAvdNotify == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = V4L2_EXT_AVD_OK;

	switch (srccap_dev->srccap_info.src) {
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		ret = mtk_avd_CvbsTimingMonitor(pAvdNotify, srccap_dev);
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		ret = mtk_avd_AtvTimingMonitor(pAvdNotify, srccap_dev);
		break;
	default:
		break;
	}

	ret |= _mtk_avd_store_PreSource(srccap_dev);

	if (ret == V4L2_EXT_AVD_OK)
		return V4L2_EXT_AVD_OK;
	else
		return V4L2_EXT_AVD_FAIL;
}

enum V4L2_AVD_RESULT mtk_avd_AtvTimingMonitor(enum v4l2_timingdetect_status *pAvdNotify,
			struct mtk_srccap_dev *srccap_dev)
{
	struct v4l2_ext_avd_standard_detection psStatus;
	struct stAvdCount _AvdCount[2] = {0};
	enum v4l2_srccap_input_source eSrc;
	enum V4L2_AVD_SIGNAL_STAGE enStg;
	static bool	bPrebLock = FALSE;
	static __u16 u16preAvdStatus;
	static __u32 u32preTime;
	__u16 u16curAvdStatus = 0;
	__u32 u32DiffTime = 0;
	bool bVifFstLock, bVdScanDet, bLock = FALSE, bForce = FALSE;
	__u8 u8CheckCnt = 0, u8AtvScan, u8StableCheckCount = 0;
	v4l2_std_id u64PreTvSystem = V4L2_STD_UNKNOWN, u64ForceVideostandard;
	struct v4L2_ext_avd_video_standard VideoStandard;
	enum v4l2_ext_avd_videostandardtype eResultTvsystem;

	eSrc = srccap_dev->srccap_info.src;
	u8AtvScan = srccap_dev->avd_info.bIsATVScanMode;
	bVifFstLock = srccap_dev->avd_info.bIsVifLock;

	//If fisrt time init vd, init monitor patameter.
	if (srccap_dev->avd_info.bAtvResetParameter == TRUE) {
		SRCCAP_AVD_MSG_INFO("[AVD]Fisrt time init vd, init atv monitor patameter.\n");
		bPrebLock = FALSE;
		*pAvdNotify = V4L2_TIMINGDETECT_NO_SIGNAL;
		srccap_dev->avd_info.u64DetectTvSystem = V4L2_STD_UNKNOWN;
		u16preAvdStatus = 0;
		srccap_dev->avd_info.bAtvResetParameter = FALSE;
		srccap_dev->avd_info.eAvdStatus = V4L2_STATUS_TYPE_IDLE;
		return V4L2_EXT_AVD_OK;
	}

	//Check if first enter this source, init monitor parameter.
	if (_mtk_avd_compare_source(srccap_dev) == TRUE) {
		bPrebLock = FALSE;
		*pAvdNotify = V4L2_TIMINGDETECT_NO_SIGNAL;
		srccap_dev->avd_info.u64DetectTvSystem = V4L2_STD_UNKNOWN;
		u16preAvdStatus = 0;
		return V4L2_EXT_AVD_OK;
	}

	//Check if in idle mode, skip this time detect.
	if (_mtk_avd_idle_check(srccap_dev) == TRUE)
		return V4L2_EXT_AVD_OK;

	//Set dedunce count
	_mtk_avd_set_debunce_count(srccap_dev, _AvdCount);

	//Check Vd detect flag enable or not.
	bVdScanDet = _IsVdScanDetect(srccap_dev);

	//Return if vif not ready during scan time.
	if ((u8AtvScan == TRUE) && (_IsVifChLock(srccap_dev) == FALSE)) {
		*pAvdNotify = V4L2_TIMINGDETECT_NO_SIGNAL;
		srccap_dev->avd_info.u64DetectTvSystem = V4L2_STD_UNKNOWN;
		//Avoid to notify Interval is too short.
		_mtk_avd_delay_ms(_AvdCount[u8AtvScan].u8PollingTime);
		return V4L2_EXT_AVD_OK;
	}

	//Request Vd detect during scan time.
	if ((u8AtvScan == TRUE) && (bVdScanDet == TRUE)) {
		u32preTime = MsOS_GetSystemTime();
		srccap_dev->avd_info.bIsTimeOut = FALSE;
		SRCCAP_AVD_MSG_INFO("[AVD][Input : %s][Type : %s][Start Time : %d]:VIF Lock\n",
			InputSourceToString(eSrc),
			ScanTypeToString(u8AtvScan), u32preTime);
	    *pAvdNotify = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
		/*new vif-lock flow has already been restarted.*/
		srccap_dev->avd_info.bStopAtvVdDet = FALSE;
		return V4L2_EXT_AVD_OK;
	}

	//Get VD status
	_mtk_avd_get_status(srccap_dev, &u16curAvdStatus);

	//Check VD status
	bLock = _CheckLock(u16curAvdStatus, u16preAvdStatus);

	//Get Signal stage
	enStg = FIND_STAGE_BY_KEY(bPrebLock, bLock);

	//Set Signal stage
	_mtk_avd_set_signal_stage(srccap_dev, enStg);

	switch (enStg) {
	case V4L2_SIGNAL_CHANGE_FALSE:
		bPrebLock = bLock;
		SRCCAP_AVD_MSG_INFO("[AVD][preStatus = 0x%04x][Status = 0x%04x]:Lock->UnLock.\n",
			u16preAvdStatus, u16curAvdStatus);
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		mtk_srccap_FV_RangeReset(srccap_dev);
		break;
	case V4L2_SIGNAL_CHANGE_TRUE:
		//Unlock->Lock
		SRCCAP_AVD_MSG_INFO("[AVD][Status = 0x%04x]:Unlock->Lock.\n", u16curAvdStatus);
		// Check signal stable
		u8StableCheckCount = _AvdCount[u8AtvScan].u8StableCheckCount;
		u8CheckCnt = _AvdCount[u8AtvScan].u8CheckCount;
		do {
			mtk_avd_standard_detetion(&psStatus);
			u64TvSystem = psStatus.u64VideoStandardType;
			u16curAvdStatus = psStatus.u16LatchStatus;
			//In play mode, skip detect flow while signal change to unatable.
			if (!IsAVDLOCK(u16curAvdStatus) && (u8AtvScan == FALSE)) {
				SRCCAP_AVD_MSG_INFO(
					"[AVD][Input : %s][Type : %s][Status = 0x%04x]:Skip detect, Lock->Ulock.\n",
					InputSourceToString(eSrc),
					ScanTypeToString(u8AtvScan),
					u16curAvdStatus);
				goto SKIP_DETECT;
			}

			//In scan mode, skip detect flow while new vif-lock notify received.
			if (srccap_dev->avd_info.bStopAtvVdDet) {
				srccap_dev->avd_info.bStopAtvVdDet = FALSE;
				/*Reset time out count*/
				u32preTime = MsOS_GetSystemTime();
				SRCCAP_AVD_MSG_INFO(
					"[AVD][Cancel][Input : %s][Type : %s]Cancel detect during detecting.\n",
					InputSourceToString(eSrc),
					ScanTypeToString(u8AtvScan));
				*pAvdNotify = V4L2_TIMINGDETECT_UNSTABLE_SYNC;
				goto SKIP_DETECT;
			}

			if ((u64PreTvSystem == u64TvSystem) && (u64TvSystem != V4L2_STD_UNKNOWN)) {
				u8CheckCnt--;
				if (u8CheckCnt == 0) {
					SRCCAP_AVD_MSG_INFO(
						"[AVD][Input : %s][Type : %s][Time : %d][Status = 0x%04x]:Break(%d)\n",
						InputSourceToString(eSrc),
						ScanTypeToString(u8AtvScan),
						MsOS_GetSystemTime(),
						u16curAvdStatus, u8CheckCnt);
					break;
				}
			} else {
				u8CheckCnt = _AvdCount[u8AtvScan].u8CheckCount;
			}

			SRCCAP_AVD_MSG_INFO(
				"[AVD][Input : %s][Type : %s][Time : %d][Cnt = %d][Status = 0x%04x][TvSystem = 0x%04x]",
				InputSourceToString(eSrc), ScanTypeToString(u8AtvScan),
				MsOS_GetSystemTime(), u8CheckCnt, u16curAvdStatus,
				_MappingV4l2stdToVideoStandard(u64TvSystem));
			u64PreTvSystem = u64TvSystem;
			_mtk_avd_delay_ms(_AvdCount[u8AtvScan].u8PollingTime);
			u8StableCheckCount--;

		} while (u8StableCheckCount > 0);

		_mtk_avd_field_signal_patch(srccap_dev, u16curAvdStatus);
		//Check force Tv system or not.
		mtk_avd_CheckForceSystem(srccap_dev, &bForce);

		if (bForce == TRUE) {
			mtk_avd_GetCurForceSystem(srccap_dev, &u64ForceVideostandard);
			//The result of Tv system is from Upper layer.
			srccap_dev->avd_info.u64DetectTvSystem = u64ForceVideostandard;
		} else {
			//The result of Tv system is from driver detect..
			srccap_dev->avd_info.u64DetectTvSystem = u64TvSystem;
			//Set VD Htt.
			mtk_avd_SetSamplingByHtt
			(srccap_dev, &srccap_dev->avd_info.u64DetectTvSystem);
			//Set VD standard .
			memset(&VideoStandard, 0, sizeof(struct v4L2_ext_avd_video_standard));
			VideoStandard.u64Videostandardtype = srccap_dev->avd_info.u64DetectTvSystem;
			mtk_avd_video_standard(&VideoStandard);
		}

		//In order to show real detect system by sysfs.
		srccap_dev->avd_info.u64RealTvSystem = u64TvSystem;

		//Store Tv system for XC getting capture win/format.
		_mtk_avd_store_StableTiming(srccap_dev);

		//Final detect tv system.
		eResultTvsystem =
			_MappingV4l2stdToVideoStandard(srccap_dev->avd_info.u64DetectTvSystem);

		SRCCAP_AVD_MSG_INFO(
			"[AVD][Input : %s][Type : %s][Time : %d][Force:%d][Status = 0x%04x][Standard : %s]\n",
			InputSourceToString(eSrc),
			ScanTypeToString(u8AtvScan),
			MsOS_GetSystemTime(),
			bForce, u16curAvdStatus,
			VdStandardToString(eResultTvsystem));

		bPrebLock = bLock;
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		mtk_srccap_FV_StartRangeHandle(srccap_dev);
		break;
	case V4L2_SIGNAL_KEEP_TRUE:
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		break;
	case V4L2_SIGNAL_KEEP_FALSE:
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		break;
	default:
		SRCCAP_AVD_MSG_ERROR("[AVD][Input : %s][Type : %s]:InValid Stage.",
			InputSourceToString(eSrc),
			ScanTypeToString(u8AtvScan));
		break;
	}

SKIP_DETECT:

	if (u16preAvdStatus != u16curAvdStatus)
		u16preAvdStatus = u16curAvdStatus;

	//Check time out or not.
	u32DiffTime =  MsOS_GetSystemTime() - u32preTime;
	if (u32DiffTime > _AvdCount[u8AtvScan].u8Timeout)
		_mtk_avd_set_timeout(srccap_dev, _AvdCount);

	mtk_avd_reg_from_dsp();
	mtk_srccap_FV_SyncRangeHandler(srccap_dev);
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_CvbsTimingMonitor(enum v4l2_timingdetect_status *pAvdNotify,
	struct mtk_srccap_dev *srccap_dev)
{
	struct v4l2_ext_avd_standard_detection psStatus;
	struct stAvdCount _AvdCount[2] = {0};
	enum v4l2_srccap_input_source eSrc;
	enum V4L2_AVD_SIGNAL_STAGE enStg;
	static bool	bPrebLock = FALSE;
	static __u16 u16preAvdStatus;
	bool bLock = FALSE, bForce = FALSE;
	__u16 u16curAvdStatus = 0;
	__u8 u8CheckCnt = 0, u8StableCheckCount = 0;
	v4l2_std_id u64PreTvSystem = V4L2_STD_UNKNOWN, u64ForceVideostandard;
	struct v4L2_ext_avd_video_standard VideoStandard;

	eSrc = srccap_dev->srccap_info.src;

	//If fisrt time init vd, init monitor patameter.
	if (srccap_dev->avd_info.bAtvResetParameter == TRUE) {
		SRCCAP_AVD_MSG_INFO("[AVD]Fisrt time init vd, init cvbs monitor patameter.\n");
		bPrebLock = FALSE;
		*pAvdNotify = V4L2_TIMINGDETECT_NO_SIGNAL;
		srccap_dev->avd_info.u64DetectTvSystem = V4L2_STD_UNKNOWN;
		u16preAvdStatus = 0;
		srccap_dev->avd_info.bAtvResetParameter = FALSE;
		return V4L2_EXT_AVD_OK;
	}

	//Set dedunce count
	_mtk_avd_set_debunce_count(srccap_dev, _AvdCount);

	//Get VD status
	_mtk_avd_get_status(srccap_dev, &u16curAvdStatus);

	//Check VD status
	if (IsAVDLOCK(u16curAvdStatus) && (u16curAvdStatus == u16preAvdStatus))
		bLock = TRUE;
	else
		bLock = FALSE;

	//Get Signal stage
	enStg = FIND_STAGE_BY_KEY(bPrebLock, bLock);

	//Set Signal stage
	_mtk_avd_set_signal_stage(srccap_dev, enStg);

	switch (enStg) {
	case V4L2_SIGNAL_CHANGE_FALSE:
		 // Lock -> UnLock
		bPrebLock = bLock;
		SRCCAP_AVD_MSG_INFO("[AVD][preStatus = 0x%04x][Status = 0x%04x]:Lock->UnLock.\n",
			u16preAvdStatus, u16curAvdStatus);
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		mtk_srccap_FV_RangeReset(srccap_dev);
		break;
	case V4L2_SIGNAL_CHANGE_TRUE:
		 //Unlock->Lock
		SRCCAP_AVD_MSG_INFO("[AVD][Status = 0x%04x]:Unlock->Lock.\n", u16curAvdStatus);

		// Check signal stable
		u8StableCheckCount = _AvdCount[PLAY_MODE].u8StableCheckCount;
		u8CheckCnt = _AvdCount[PLAY_MODE].u8CheckCount;
		do {
			mtk_avd_standard_detetion(&psStatus);
			u64TvSystem = psStatus.u64VideoStandardType;
			u16curAvdStatus = psStatus.u16LatchStatus;

			if ((u64PreTvSystem == u64TvSystem) && (u64TvSystem != V4L2_STD_UNKNOWN)) {
				u8CheckCnt--;
				if (u8CheckCnt == 0) {
					SRCCAP_AVD_MSG_INFO(
						"[AVD][Input : %s][Type : %s][Status = 0x%04x]:Break(%d)\n",
						InputSourceToString(eSrc),
						ScanTypeToString(PLAY_MODE),
						u16curAvdStatus, u8CheckCnt);
					break;
				}
			} else {
				u8CheckCnt = _AvdCount[PLAY_MODE].u8CheckCount;
			}

			SRCCAP_AVD_MSG_INFO(
				"[AVD][Input : %s][Type : %s][Cnt = %d][Status = 0x%04x][TvSystem = 0x%04x]",
				InputSourceToString(eSrc), ScanTypeToString(PLAY_MODE),
				u8CheckCnt, u16curAvdStatus,
				_MappingV4l2stdToVideoStandard(u64TvSystem));
			u64PreTvSystem = u64TvSystem;
			_mtk_avd_delay_ms(_AvdCount[PLAY_MODE].u8PollingTime);
			u8StableCheckCount--;

		} while (u8StableCheckCount > 0);

		_mtk_avd_field_signal_patch(srccap_dev, u16curAvdStatus);
		//Check force Tv system or not.
		mtk_avd_CheckForceSystem(srccap_dev, &bForce);

		if (bForce == TRUE) {
			mtk_avd_GetCurForceSystem(srccap_dev, &u64ForceVideostandard);
			//The result of Tv system is from Upper layer.
			srccap_dev->avd_info.u64DetectTvSystem = u64ForceVideostandard;
		} else {
			//The result of Tv system is from driver detect.
			srccap_dev->avd_info.u64DetectTvSystem = u64TvSystem;
			//Set VD Htt.
			mtk_avd_SetSamplingByHtt
			(srccap_dev, &srccap_dev->avd_info.u64DetectTvSystem);
			//Set VD standard .
			memset(&VideoStandard, 0, sizeof(struct v4L2_ext_avd_video_standard));
			VideoStandard.u64Videostandardtype = srccap_dev->avd_info.u64DetectTvSystem;
			mtk_avd_video_standard(&VideoStandard);
		}

		//In order to show real detect system by sysfs.
		srccap_dev->avd_info.u64RealTvSystem = u64TvSystem;

		//Store Tv system for XC getting capture win/format.
		srccap_dev->avd_info.eStableTvsystem =
			_MappingV4l2stdToVideoStandard(srccap_dev->avd_info.u64DetectTvSystem);

		SRCCAP_AVD_MSG_INFO(
			"[AVD][Input : %s][Type : %s][Force:%d][Status = 0x%04x][Standard : %s]\n",
			InputSourceToString(eSrc),
			ScanTypeToString(PLAY_MODE), bForce,
			u16curAvdStatus,
			VdStandardToString(srccap_dev->avd_info.eStableTvsystem));

		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		mtk_srccap_FV_StartRangeHandle(srccap_dev);
		bPrebLock = bLock;
		break;
	case V4L2_SIGNAL_KEEP_TRUE:
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		break;
	case V4L2_SIGNAL_KEEP_FALSE:
		_mtk_avd_get_notify_event(srccap_dev, pAvdNotify);
		break;
	default:
		SRCCAP_AVD_MSG_ERROR("[AVD][Input : %s][Type : %s]:InValid Stage.",
			InputSourceToString(eSrc),
			ScanTypeToString(PLAY_MODE));
		break;
	}

	if (u16preAvdStatus != u16curAvdStatus)
		u16preAvdStatus = u16curAvdStatus;

	mtk_avd_reg_from_dsp();
	mtk_srccap_FV_SyncRangeHandler(srccap_dev);
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_SetSamplingByHtt(struct mtk_srccap_dev *srccap_dev,
		v4l2_std_id *u64Videostandardtype)
{
	int ret;
	__u32 u32VDPatchFlag;
	__u16 u16Htt, vdsampling_entry_cnt = 0, vdsampling_index = 0;
	struct srccap_avd_vd_sampling *vdsapmling_table;

	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (u64Videostandardtype == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	// get vd sampling table
	vdsampling_entry_cnt = srccap_dev->avd_info.vdsampling_table_entry_count;
	vdsapmling_table = srccap_dev->avd_info.vdsampling_table;

	ret = _mtk_avd_match_vd_sampling
		(_MappingV4l2stdToVideoStandard(*u64Videostandardtype),
			vdsapmling_table, vdsampling_entry_cnt, &vdsampling_index);

	if (ret == -ERANGE) {
		SRCCAP_MSG_ERROR("vd sampling not found\n");
		return ret;
	}

	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	SRCCAP_AVD_MSG_INFO
		("[vst:0x%4X][hst:0x%4X][hde:0x%4X][vde:0x%4X][htt:0x%4X][vfreqx1000:%d]\n",
		vdsapmling_table[vdsampling_index].v_start,
		vdsapmling_table[vdsampling_index].h_start,
		vdsapmling_table[vdsampling_index].h_de,
		vdsapmling_table[vdsampling_index].v_de,
		vdsapmling_table[vdsampling_index].h_total,
		vdsapmling_table[vdsampling_index].v_freqx1000);

	memset(&u16Htt, 0, sizeof(__u16));
	u16Htt = vdsapmling_table[vdsampling_index].h_total;
	ret = mtk_avd_htt_user_md(&u16Htt);
	if (ret != V4L2_EXT_AVD_OK)
		return V4L2_EXT_AVD_FAIL;

	memset(&u32VDPatchFlag, 0, sizeof(__u32));
	ret = mtk_avd_g_flag(&u32VDPatchFlag);
	if (ret != V4L2_EXT_AVD_OK)
		return V4L2_EXT_AVD_FAIL;

	u32VDPatchFlag = ((u32VDPatchFlag & ~(AVD_PATCH_HTOTAL_MASK)) | AVD_PATCH_HTOTAL_USER);
	ret = mtk_avd_s_flag(&u32VDPatchFlag);
	if (ret != V4L2_EXT_AVD_OK)
		return V4L2_EXT_AVD_FAIL;

	SRCCAP_AVD_MSG_INFO("[ForceTvSystem = %s][PatchFlag = 0x%08x],[Htt = 0x%04x]\n",
		VideoStandardToString(*u64Videostandardtype),
		u32VDPatchFlag, u16Htt);

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_SetCurForceSystem(struct mtk_srccap_dev *srccap_dev,
	v4l2_std_id *u64Videostandardtype)
{
	enum v4l2_srccap_input_source eSrc;
	__u8 u8AtvScan;

	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (u64Videostandardtype == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	eSrc = srccap_dev->srccap_info.src;
	u8AtvScan = srccap_dev->avd_info.bIsATVScanMode;

	srccap_dev->avd_info.u64ForceTvSystem = *u64Videostandardtype;

	SRCCAP_AVD_MSG_INFO(
		"[AVD][Input : %s][Type : %s][Time : %d][Force Standard : %s(%d)]\n",
		InputSourceToString(eSrc), ScanTypeToString(u8AtvScan),
		MsOS_GetSystemTime(),
		VideoStandardToString(srccap_dev->avd_info.u64ForceTvSystem),
		_MappingV4l2stdToVideoStandard(srccap_dev->avd_info.u64ForceTvSystem));

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_GetCurForceSystem(struct mtk_srccap_dev *srccap_dev,
	v4l2_std_id *u64Videostandardtype)
{
	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (u64Videostandardtype == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	*u64Videostandardtype = srccap_dev->avd_info.u64ForceTvSystem;

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_avd_CheckForceSystem(struct mtk_srccap_dev *srccap_dev,
	bool *bForce)
{
	enum v4l2_srccap_input_source eSrc;
	bool u8AtvScan;
	v4l2_std_id u64ForceVideostandard = V4L2_STD_UNKNOWN;

	SRCCAP_AVD_MSG_INFO("in\n");

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (bForce == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	eSrc = srccap_dev->srccap_info.src;
	u8AtvScan = srccap_dev->avd_info.bIsATVScanMode;
	mtk_avd_GetCurForceSystem(srccap_dev, &u64ForceVideostandard);

	if ((u8AtvScan == FALSE) && (u64ForceVideostandard != V4L2_STD_UNKNOWN))
		*bForce = TRUE;
	else
		*bForce = FALSE;

	srccap_dev->avd_info.bForce = *bForce;
	SRCCAP_AVD_MSG_INFO(
		"[AVD][Input : %s][Type : %s][Time : %d][Force Eanble : %d][Force Standard : %s]\n",
		InputSourceToString(eSrc), ScanTypeToString(u8AtvScan),	MsOS_GetSystemTime(),
		*bForce, VideoStandardToString(u64ForceVideostandard));

	SRCCAP_AVD_MSG_INFO("out\n");
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT _mtk_avd_match_vd_sampling(enum v4l2_ext_avd_videostandardtype videotype,
	struct srccap_avd_vd_sampling *vdsampling_table,
	u16 table_entry_cnt,
	u16 *vdsampling_table_index)
{
	SRCCAP_AVD_MSG_INFO("in\n");

	if (vdsampling_table == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (vdsampling_table_index == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	SRCCAP_AVD_MSG_INFO("%s:%u, %s:%d\n",
		"video type", videotype,
		"vdtable_entry_count", table_entry_cnt);

	for (*vdsampling_table_index = 0; (*vdsampling_table_index) < table_entry_cnt;
		(*vdsampling_table_index)++) {
		SRCCAP_AVD_MSG_INFO("[%u], %s:%d\n",
			*vdsampling_table_index,
			"status", vdsampling_table[*vdsampling_table_index].videotype);

		if (videotype == vdsampling_table[*vdsampling_table_index].videotype)
			break;
	}

	if (*vdsampling_table_index == table_entry_cnt) {
		SRCCAP_MSG_ERROR("video type not supported\n");
		return -ERANGE;
	}

	SRCCAP_AVD_MSG_INFO("out\n");
	return 0;
}

enum V4L2_AVD_RESULT mtk_avd_GetOnlineDetect(struct mtk_srccap_dev *srccap_dev,
	bool *bSync)
{
	static bool bPreSync = FALSE;
	enum v4l2_srccap_input_source eSrc;
	enum V4L2_AVD_RESULT ret;
	unsigned short u16vdstatus;
	bool bAlive = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (bSync == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;

	u16vdstatus = 0;
	ret = mtk_avd_status(&u16vdstatus);
	if (ret < 0)
		return V4L2_EXT_AVD_FAIL;

	ret = mtk_avd_alive_check(&bAlive);
	if (ret < 0)
		return V4L2_EXT_AVD_FAIL;

	if ((u16vdstatus & VD_SYNC_LOCKED) && (u16vdstatus & VD_HSYNC_LOCKED) && (bAlive == TRUE))
		*bSync = TRUE;
	else
		*bSync = FALSE;

	if (bPreSync != *bSync) {
		bPreSync = *bSync;
		SRCCAP_AVD_MSG_INFO(
			"[AVD][Online_Detect][Input : %s][Sync : %d][Alive: %d]\n",
			InputSourceToString(eSrc), *bSync, bAlive);
	}

	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_srccap_FV_StartRangeHandle(struct mtk_srccap_dev *srccap_dev)
{
	MS_U16 wVtotal = 0;
	enum v4l2_srccap_input_source eSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	wVtotal = srccap_dev->avd_info.u16vtotal;

	if ((wVtotal < MAX_VTOTAL_COUNT) && (wVtotal > MID_VTOTAL_COUNT))
		_u16PreVtotal = PAL_VTOTAL_STD;
	else
		_u16PreVtotal = NTSC_VTOTAL_STD;

	SRCCAP_AVD_MSG_INFO("[FV][Input : %s] Set STD Vtotal to u16PreVtotal(%d)\n",
		InputSourceToString(eSrc), _u16PreVtotal);
	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_srccap_FV_RangeReset(struct mtk_srccap_dev *srccap_dev)
{
	enum v4l2_srccap_input_source eSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	_u16PreVtotal = 0;

	SRCCAP_AVD_MSG_INFO("[FV][Input : %s] Reset u16PreVtotal to 0!!\n",
		InputSourceToString(eSrc));

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT FV_ForceCropScreenInfo(struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_fv_crop *fv_force_crop)
{
	enum v4l2_srccap_input_source eSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (fv_force_crop == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	if (eSrc == V4L2_SRCCAP_INPUT_SOURCE_ATV) {
		//ATV
		if (srccap_dev->avd_info.eStableTvsystem == V4L2_EXT_AVD_STD_NTSC_M) {
			fv_force_crop->CropWinX = ATV_NTSC_M_CROPWIN_X;
			fv_force_crop->CropWinY = ATV_NTSC_M_CROPWIN_Y;
			fv_force_crop->CropWinWidth = ATV_NTSC_M_CROPWIN_Width;
			fv_force_crop->CropWinHeight = ATV_NTSC_M_CROPWIN_Height;
		} else if (srccap_dev->avd_info.eStableTvsystem == V4L2_EXT_AVD_STD_PAL_BGHI) {
			fv_force_crop->CropWinX = ATV_PAL_BGHI_CROPWIN_X;
			fv_force_crop->CropWinY = ATV_PAL_BGHI_CROPWIN_Y;
			fv_force_crop->CropWinWidth = ATV_PAL_BGHI_CROPWIN_Width;
			fv_force_crop->CropWinHeight = ATV_PAL_BGHI_CROPWIN_Height;
			}
	} else {
		//CVBS
		if (srccap_dev->avd_info.eStableTvsystem == V4L2_EXT_AVD_STD_NTSC_M) {
			fv_force_crop->CropWinX = CVBS_NTSC_M_CROPWIN_X;
			fv_force_crop->CropWinY = CVBS_NTSC_M_CROPWIN_Y;
			fv_force_crop->CropWinWidth = CVBS_NTSC_M_CROPWIN_Width;
			fv_force_crop->CropWinHeight = CVBS_NTSC_M_CROPWIN_Height;
		} else if (srccap_dev->avd_info.eStableTvsystem == V4L2_EXT_AVD_STD_PAL_BGHI) {
			fv_force_crop->CropWinX = CVBS_PAL_BGHI_CROPWIN_X;
			fv_force_crop->CropWinY = CVBS_PAL_BGHI_CROPWIN_Y;
			fv_force_crop->CropWinWidth = CVBS_PAL_BGHI_CROPWIN_Width;
			fv_force_crop->CropWinHeight = CVBS_PAL_BGHI_CROPWIN_Height;
		}
	}

	SRCCAP_AVD_MSG_INFO("[FV][cropscreen][Input:%s] X = [%d],Y = [%d], W = [%d], H = [%d]\n",
		InputSourceToString(eSrc),
		fv_force_crop->CropWinX,
		fv_force_crop->CropWinY,
		fv_force_crop->CropWinWidth,
		fv_force_crop->CropWinHeight);

	return V4L2_EXT_AVD_OK;
}

static enum V4L2_AVD_RESULT FV_CalCropRange(struct mtk_srccap_dev *srccap_dev,
	struct srccap_timingdetect_fv_crop *fv_crop, MS_U16 wVtotal)
{
	MS_U16 VShift = 0;
	enum v4l2_srccap_input_source eSrc;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (fv_crop == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

#define DELTA_Y 2
#define START_CROP_X 30
#define BOUNDARY_CROP_Y 21
#define DELTA_H 1
#define START_CROP_H 680
#define SCALE 1
#define DELTA_W 4

	eSrc = srccap_dev->srccap_info.src;

	/* Used for Debug. Fix crop screen. */
	if (srccap_dev->avd_info.bIsForceCropScreenInfo == TRUE)
		FV_ForceCropScreenInfo(srccap_dev, fv_crop);
	else {
	/* Crop Screen Info from Displink. */
		fv_crop->CropWinX =
			srccap_dev->avd_info.fv_crop.CropWinX;
		fv_crop->CropWinY =
			srccap_dev->avd_info.fv_crop.CropWinY;
		fv_crop->CropWinWidth =
			srccap_dev->avd_info.fv_crop.CropWinWidth;
		fv_crop->CropWinHeight =
			srccap_dev->avd_info.fv_crop.CropWinHeight;
	}

	SRCCAP_AVD_MSG_INFO("[FV][Input : %s][Get]X = [%d], Y = [%d], W = [%d], H = [%d]\n",
		InputSourceToString(eSrc), fv_crop->CropWinX, fv_crop->CropWinY,
		fv_crop->CropWinWidth, fv_crop->CropWinHeight);

	///< 575--620  and 630--680
	if (((wVtotal < PAL_VTOTAL_MIN) && (wVtotal > MID_VTOTAL_COUNT)) ||
		((wVtotal > PAL_VTOTAL_MAX) && (wVtotal < MAX_VTOTAL_COUNT))) {
		SRCCAP_AVD_MSG_INFO("[FV][Input : %s] PAL START\n", InputSourceToString(eSrc));
		if (wVtotal > PAL_VTOTAL_STD) {
			/* Bypass crop. Because -1 to -3 no symptom*/
			fv_crop->bEnable = FALSE;
			SRCCAP_AVD_MSG_INFO(
				"[FV][Input : %s] Vtotal(%d) > PAL_Vtotal_STD(625), delta: +%d\n",
				InputSourceToString(eSrc), wVtotal, wVtotal - PAL_VTOTAL_STD);
		} else {
			/* Enable fV Sync Range hanlder*/
			fv_crop->bEnable = TRUE;
			SRCCAP_AVD_MSG_INFO(
				"[FV][Input : %s] Vtotal(%d) < PAL_Vtotal_STD(625), delta: -%d\n",
				InputSourceToString(eSrc), wVtotal, PAL_VTOTAL_STD - wVtotal);
			VShift = PAL_VTOTAL_STD - wVtotal;  ///<  625 - wVtotal
			SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][0](VShift/SCALE) = %d, Y = %d\n",
				InputSourceToString(eSrc),
				(VShift/SCALE),
				fv_crop->CropWinY);
			if ((VShift/SCALE) <= fv_crop->CropWinY) {
				fv_crop->CropWinHeight = fv_crop->CropWinHeight + (VShift/SCALE);
				fv_crop->CropWinY = (fv_crop->CropWinY - (VShift/SCALE));
				SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][1]H = %d, Y = %d\n",
				InputSourceToString(eSrc),
				fv_crop->CropWinHeight,
				fv_crop->CropWinY);
			} else {
				fv_crop->CropWinHeight -= ((VShift/SCALE) - fv_crop->CropWinY);
				fv_crop->CropWinY = CROP_Y_ORI;
				SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][2]H = %d, Y = %d\n",
				InputSourceToString(eSrc),
				fv_crop->CropWinHeight,
				fv_crop->CropWinY);
			}
		}
	} else if (((wVtotal < NTSC_VTOTAL_MIN) && (wVtotal > MIN_VTOTAL_COUNT)) ||
	((wVtotal > NTSC_VTOTAL_MAX) && (wVtotal < MID_VTOTAL_COUNT))) {
		SRCCAP_AVD_MSG_INFO("[FV][Input : %s] NTSC START\n", InputSourceToString(eSrc));
		if (wVtotal > NTSC_VTOTAL_STD) {
			/* Bypass crop. Because -1 to -3 no symptom.*/
			fv_crop->bEnable = FALSE;
			SRCCAP_AVD_MSG_INFO(
				"[FV][Input : %s] Vtotal(%d) > NTSC_Vtotal_STD(525), delta: +%d\n",
				InputSourceToString(eSrc), wVtotal, wVtotal - NTSC_VTOTAL_STD);
		} else {
			/* Enable fV Sync Range hanlder*/
			fv_crop->bEnable = TRUE;
			SRCCAP_AVD_MSG_INFO(
				"[FV][Input : %s] Vtotal(%d) < NTSC_Vtotal_STD(525), delta: -%d\n",
				InputSourceToString(eSrc), wVtotal, NTSC_VTOTAL_STD - wVtotal);
			//<  525 - wVtotal
			VShift = NTSC_VTOTAL_STD - wVtotal;
			SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][0](VShift/SCALE) = %d, Y = %d\n",
				InputSourceToString(eSrc),
				(VShift/SCALE),
				fv_crop->CropWinY);
			if ((VShift/SCALE) <= fv_crop->CropWinY) {
				fv_crop->CropWinHeight = fv_crop->CropWinHeight + (VShift/SCALE);
				fv_crop->CropWinY = (fv_crop->CropWinY - (VShift/SCALE));
				SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][1]H = %d, Y = %d\n",
				InputSourceToString(eSrc),
				fv_crop->CropWinHeight,
				fv_crop->CropWinY);
			} else {
				fv_crop->CropWinHeight -= ((VShift/SCALE) - fv_crop->CropWinY);
				fv_crop->CropWinY = CROP_Y_ORI;
				SRCCAP_AVD_MSG_DEBUG("[FV][Input : %s][2]H = %d, Y = %d\n",
				InputSourceToString(eSrc),
				fv_crop->CropWinHeight,
				fv_crop->CropWinY);
			}
		}
	}

	return V4L2_EXT_AVD_OK;
}

enum V4L2_AVD_RESULT mtk_srccap_FV_SyncRangeHandler(struct mtk_srccap_dev *srccap_dev)
{
	static MS_U8 u8SyncStableCounter;
	struct srccap_timingdetect_fv_crop cnt_fv_crop;
	enum v4l2_srccap_input_source eSrc;

	enum V4L2_AVD_RESULT ret;
	MS_U16 wVtotal = 0;
	__u8 u8AtvScan, u8lock;
	static struct srccap_timingdetect_fv_crop pre_fv_crop = {
		.CropWinX = 0,
		.CropWinY = 0,
		.CropWinWidth = 0,
		.CropWinHeight = 0,
		.bEnable = FALSE};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	eSrc = srccap_dev->srccap_info.src;
	wVtotal = srccap_dev->avd_info.u16vtotal;
	u8AtvScan = srccap_dev->avd_info.bIsATVScanMode;

	/* Get Sync lock */
	memset(&u8lock, 0, sizeof(__u8));
	ret = mtk_avd_is_synclocked(&u8lock);
	if (ret < 0)
		return V4L2_EXT_AVD_FAIL;

	/*Skip fV Sync Range hanlder*/
	if ((u8lock != TRUE) || (u8AtvScan == TRUE)) {
		_u16PreVtotal = 0;
		u8SyncStableCounter = 0;
		return V4L2_EXT_AVD_OK;
	}


	/*Do fV Sync Range hanlder*/
	if (((wVtotal <= PAL_VTOTAL_MAX) &&
		(wVtotal >= PAL_VTOTAL_MIN) && (_u16PreVtotal == PAL_VTOTAL_STD)) ||
		((wVtotal <= NTSC_VTOTAL_MAX) &&
		(wVtotal >= NTSC_VTOTAL_MIN) && (_u16PreVtotal == NTSC_VTOTAL_STD))) {
		if (_u16PreVtotal != wVtotal)
			_u16PreVtotal = wVtotal;
		u8SyncStableCounter = 0;
		/* Disable fV Sync Range hanlder*/
		memset(&srccap_dev->timingdetect_info.fv_crop, 0,
			sizeof(struct srccap_timingdetect_fv_crop));
	} else {
		if (_u16PreVtotal != wVtotal) {
			_u16PreVtotal = wVtotal;
			u8SyncStableCounter = 0;
		} else if (u8SyncStableCounter < RANGE_HANDLE_STABLE_COUNTER)
			u8SyncStableCounter++;

		if (u8SyncStableCounter == RANGE_HANDLE_STABLE_COUNTER) {
			u8SyncStableCounter = OUT_OF_HANDLE_STABLE_COUNTER;
			memset(&cnt_fv_crop, 0, sizeof(struct srccap_timingdetect_fv_crop));
			FV_CalCropRange(srccap_dev, &cnt_fv_crop, wVtotal);
			memcpy(&srccap_dev->timingdetect_info.fv_crop, &cnt_fv_crop,
				sizeof(struct srccap_timingdetect_fv_crop));
		}
	}

	if (memcmp(&pre_fv_crop, &srccap_dev->timingdetect_info.fv_crop,
		sizeof(struct srccap_timingdetect_fv_crop))) {
		SRCCAP_AVD_MSG_INFO(
			"[FV][Input : %s][Final]bEnable = %d, X = [%d], Y = [%d], W = [%d], H =[%d]\n",
			InputSourceToString(eSrc),
			srccap_dev->timingdetect_info.fv_crop.bEnable,
			srccap_dev->timingdetect_info.fv_crop.CropWinX,
			srccap_dev->timingdetect_info.fv_crop.CropWinY,
			srccap_dev->timingdetect_info.fv_crop.CropWinWidth,
			srccap_dev->timingdetect_info.fv_crop.CropWinHeight);
		memcpy(&pre_fv_crop, &srccap_dev->timingdetect_info.fv_crop,
			sizeof(struct srccap_timingdetect_fv_crop));
	}

	return V4L2_EXT_AVD_OK;
}

