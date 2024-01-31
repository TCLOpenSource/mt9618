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
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
//#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <asm/siginfo.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/videodev2.h>
#include <linux/errno.h>
#include <linux/compat.h>
#include <linux/delay.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>

#include "mtk_earc.h"
#include "mtk_earc_sysfs.h"
#include "drvEARC.h"

extern int sysfs_emit_at(char *buf, int at, const char *fmt, ...);

MS_U8 u8InfoSel = 0;
MS_U8 dts_device_node_num;
MS_U8 real_device_node_num;

/* interface for exporting earc attributes */
struct earc_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mtk_earc_dev *earc_dev,
			struct earc_attribute *attr, char *buf);
	ssize_t (*store)(struct mtk_earc_dev *earc_dev,
			 struct earc_attribute *attr, const char *buf,
			 size_t count);
};

/* earc kobject attribute operations */
static ssize_t earc_attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct earc_attribute *kattr;
	struct mtk_earc_dev *earc_dev;
	struct mtk_earc_info *earc_info;
	ssize_t ret = -EIO;

	earc_info = container_of(kobj, struct mtk_earc_info, earc_kobj);
	earc_dev = container_of(earc_info, struct mtk_earc_dev, earc_info);
	kattr = container_of(attr, struct earc_attribute, attr);

	if (kattr->show)
		ret = kattr->show(earc_dev, kattr, buf);
	return ret;
}

static ssize_t earc_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *buf, size_t count)
{
	struct earc_attribute *kattr;
	struct mtk_earc_dev *earc_dev;
	struct mtk_earc_info *earc_info;
	ssize_t ret = -EIO;

	earc_info = container_of(kobj, struct mtk_earc_info, earc_kobj);
	earc_dev = container_of(earc_info, struct mtk_earc_dev, earc_info);
	kattr = container_of(attr, struct earc_attribute, attr);

	if (kattr->store)
		ret = kattr->store(earc_dev, kattr, buf, count);
	return ret;
}

#define EARC_ATTR_RW(_name)	\
	struct earc_attribute earc_attr_##_name = __ATTR_RW(_name)
#define EARC_ATTR_RO(_name)	\
	struct earc_attribute earc_attr_##_name = __ATTR_RO(_name)
#define EARC_ATTR_WO(_name)	\
	struct earc_attribute earc_attr_##_name = __ATTR_WO(_name)

#define RD_ATTR_RW(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RW(_name)
#define RD_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define RD_ATTR_WO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_WO(_name)

/* sysfs function*/
static int eARC_DiscState(char *buf, int at, int bypassMode)
{
	int rc = 0;
	int val = mdrv_EARC_Get_DiscState();
	int eARCSupportMode = mdrv_EARC_Get_SupportMode();

	if (bypassMode)
		eARCSupportMode = SUPPORT_EARC;

	if (eARCSupportMode == SUPPORT_NONE) {
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: NONE\r\n");
		return rc;
	}

	switch (val) {
	case E_EARC_STATUS_IDLE1:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: IDLE1\r\n");
	break;
	case E_EARC_STATUS_IDLE2:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: IDLE2\r\n");
	break;
	case E_EARC_STATUS_DISC1:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: DISC1\r\n");
	break;
	case E_EARC_STATUS_DISC2:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: DISC2\r\n");
	break;
	case E_EARC_STATUS_EARC:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: eARC\r\n");
	break;
	default:
		rc += sysfs_emit_at(buf, at, "eARC_TX Status: ERROR!!!\r\n");
	break;
	}

	return rc;
}

static ssize_t LogLevel_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val < LOG_LEVEL_MAX)
		mdrv_EARC_Set_LogLevel((MS_U8)val);

	return count;
}

static ssize_t LogLevel_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"eARC Log Level = %d\r\n",
		mdrv_EARC_Get_LogLevel());

	return rc;
}

static ssize_t DiffTermination_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= EARC_DIFF_TERMINATION_MAX)
		mdrv_EARC_Set_DifferentialOnOff((MS_U8)val);

	return count;
}

static ssize_t DiffTermination_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"Differential Termination = %d\r\n",
		mdrv_EARC_Get_DifferentialOnOff());

	return rc;
}

static ssize_t DiffDriveStrength_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	/* drive strength : (1) > (0) > (2) > (3) */
	/* Default : (0) */
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= EARC_DIFF_DRIVE_STRENGTH_MAX)
		mdrv_EARC_Set_DifferentialDriveStrength((MS_U8)val);

	return count;
}

static ssize_t DiffDriveStrength_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"Differential Drive Strength = %d\r\n",
		mdrv_EARC_Get_DifferentialDriveStrength());
	rc += sysfs_emit_at(buf, rc,
		"**(Note)DiffDriveStrength : (1) > (0) > (2) > (3)**\r\n");

	return rc;
}

static ssize_t DiffSkew_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	/* skew : (2) > (1) > (0) */
	/* Default : (0) */
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= EARC_DIFF_SKEW_MAX)
		mdrv_EARC_Set_DifferentialSkew((MS_U8)val);

	return count;
}

static ssize_t DiffSkew_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"Differential Skew = %d\r\n",
		mdrv_EARC_Get_DifferentialSkew());
	rc += sysfs_emit_at(buf, rc,
		"**(Note)DiffSkew : (2) > (1) > (0)**\r\n");

	return rc;
}

static ssize_t CommTermination_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= EARC_COMM_TERMINATION_MAX)
		mdrv_EARC_Set_CommonOnOff((MS_U8)val);

	return count;
}

static ssize_t CommTermination_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"Common Termination = %d\r\n",
		mdrv_EARC_Get_CommonOnOff());

	return rc;
}

static ssize_t CommDriveStrength_store(struct mtk_earc_dev *earc_dev,
			       struct earc_attribute *kattr, const char *buf,
			       size_t count)
{
	/* drive strength : (3) > (1) > (0) > (2) */
	/* Default : (0) */
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= EARC_COMM_DRIVE_STRENGTH_MAX)
		mdrv_EARC_Set_CommonDriveStrength((MS_U8)val);

	return count;
}

static ssize_t CommDriveStrength_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"Common Drive Strength = %d\r\n",
		mdrv_EARC_Get_CommonDriveStrength());
	rc += sysfs_emit_at(buf, rc,
		"**(Note)CommDriveStrength : (3) > (1) > (0) > (2)**\r\n");

	return rc;
}

static ssize_t EarcStatus_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	return eARC_DiscState(buf, 0, 0);
}

static ssize_t ArcPin_show(struct mtk_earc_dev *earc_dev,
				  struct earc_attribute *kattr, char *buf)
{
	int rc = 0;

	if (mdrv_EARC_GetArcPin() <= 1)
		rc += sysfs_emit_at(buf, rc,
			"Arc Pin Status:%d\r\n",
			mdrv_EARC_GetArcPin());
	else
		rc += sysfs_emit_at(buf, rc, "Arc Pin Status:N/A\r\n");

	return rc;
}

static EARC_ATTR_RW(DiffTermination);
static EARC_ATTR_RW(DiffDriveStrength);
static EARC_ATTR_RW(DiffSkew);
static EARC_ATTR_RW(CommTermination);
static EARC_ATTR_RW(CommDriveStrength);
static EARC_ATTR_RO(EarcStatus);
static EARC_ATTR_RO(ArcPin);
static EARC_ATTR_RW(LogLevel);

static ssize_t All_contents_are_mtk_internal_use_only_show
	(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc += sysfs_emit_at(buf, rc,
		"**Unauthorized operations may cause unexpected problems!**\r\n");

	return rc;
}

static ssize_t SupportMode_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= BIT1)
		mdrv_EARC_SetEarcSupportMode((MS_U8)val);

	return count;
}

static ssize_t Info_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= BIT0)
		u8InfoSel = val;

	return count;
}

static ssize_t ArcPin_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, CONVERT_STR_LENGTH, &val);
	if (ret < 0)
		return ret;

	if (ret > GENERIC_ERROR_CODE_MAX)
		return -EINVAL;

	if (val >= 0 && val <= BIT0)
		mdrv_EARC_SetArcPin(val);

	return count;
}

static ssize_t Info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int i = 0;
	MS_U8 ConnectState[(STR_UNIT_LENGTH >> BIT1)];
	MS_U8 SupportMode[(STR_UNIT_LENGTH >> BIT1)];
	struct HDMI_EARCTX_RESOURCE_PRIVATE *pInfo = NULL;

	pInfo = kmalloc((sizeof(struct HDMI_EARCTX_RESOURCE_PRIVATE)), GFP_KERNEL);

	mdrv_EARC_Get_Info((struct HDMI_EARCTX_RESOURCE_PRIVATE *)pInfo);

	rc += sysfs_emit_at(buf, rc,
		"Task Status:%d, Polling Status:%d (%d)\r\n",
		pInfo->bSelfCreateTaskFlag, pInfo->bEARCTaskProcFlag,
		pInfo->slEARCPollingTaskID);
	rc += sysfs_emit_at(buf, rc,
		"5V Invert:%d, HPD Invert:%d, ArcPin:%d\r\n",
		pInfo->bEnable5VDetectInvert, pInfo->bEnableHPDInvert,
		mdrv_EARC_GetArcPin());

	switch (mdrv_EARC_GetConnectState()) {
	case EARC_CONNECTION_LOST_5V:
		strncpy(ConnectState, "LOST_5V", strlen("LOST_5V") + 1);
		break;
	case EARC_CONNECTION_ARC_MODE:
		strncpy(ConnectState, "ARC_MODE", strlen("ARC_MODE") + 1);
		break;
	case EARC_CONNECTION_EARC_BUSY:
		strncpy(ConnectState, "EARC_BUSY", strlen("EARC_BUSY") + 1);
		break;
	case EARC_CONNECTION_EARC_MODE:
		strncpy(ConnectState, "EARC_MODE", strlen("EARC_MODE") + 1);
		break;
	case EARC_CONNECTION_NONE_MODE:
		strncpy(ConnectState, "NONE_MODE", strlen("NONE_MODE") + 1);
		break;
	default:
		strncpy(ConnectState, "Unknown", strlen("Unknown") + 1);
		break;
	}

	switch (mdrv_EARC_Get_SupportMode()) {
	case SUPPORT_NONE:
		strncpy(SupportMode, "NONE", strlen("NONE") + 1);
		break;
	case SUPPORT_EARC:
		strncpy(SupportMode, "EARC", strlen("EARC") + 1);
		break;
	case SUPPORT_ARC:
		strncpy(SupportMode, "ARC", strlen("ARC") + 1);
		break;
	default:
		strncpy(SupportMode, "Unknown", strlen("Unknown") + 1);
		break;
	}

	rc += sysfs_emit_at(buf, rc,
		"SupportMode = %s, ", SupportMode);
	rc += eARC_DiscState(buf, rc, 1);
	rc += sysfs_emit_at(buf, rc,
		"ConnectState = %s, HWPortSel = %d, UIPortSel = %d\r\n",
		ConnectState, mdrv_EARC_GetEarcPortSel(), mdrv_EARC_GetEarcUIPortSel());

	if (!u8InfoSel) {
		kfree(pInfo);
		return rc;
	}

	rc += sysfs_emit_at(buf, rc,
		"-----------------------------------------------\r\n");

	rc += sysfs_emit_at(buf, rc,
		"CapChangeFlag    = %3d\r\n",
		pInfo->stEarcCmPollingInfo.bCapChangeFlag);
	rc += sysfs_emit_at(buf, rc,
		"CommCapBusyFlag  = %3d, CapChangeUpdateDoneFlag = %3d\r\n",
		pInfo->stEarcCmPollingInfo.bCommonModeCapBusyFlag,
		pInfo->stEarcCmPollingInfo.bCapChangeUpdateDoneFlag);
	rc += sysfs_emit_at(buf, rc,
		"CapChangeCnt     = %3d\r\n",
		pInfo->stEarcCmPollingInfo.u8CapabilityChangeCnt);
	rc += sysfs_emit_at(buf, rc,
		"u8CapContent :(Hexadecimal)\r\n");
	for (i = 0; i < HDMI_EARCTX_CAP_LEN; i++) {
		rc += sysfs_emit_at(buf, rc, "%2X ",
			pInfo->stEarcCmPollingInfo.u8CapContent[i]);
		if ((((i+1) % ((STR_UNIT_LENGTH >> BIT1) + 1)) == 0) ||
			(i == HDMI_EARCTX_CAP_LEN - 1))
			rc += sysfs_emit_at(buf, rc, "\n");
	}
	rc += sysfs_emit_at(buf, rc,
		"-----------------------------------------------\r\n");

	rc += sysfs_emit_at(buf, rc,
		"StatChangeFlag  = %3d\r\n",
		pInfo->stEarcCmPollingInfo.bStatChangeFlag);
	rc += sysfs_emit_at(buf, rc,
		"CommCapBusyFlag = %3d, StatChangeUpdateDoneFlag = %3d\r\n",
		pInfo->stEarcCmPollingInfo.bCommonModeStatBusyFlag,
		pInfo->stEarcCmPollingInfo.bStatChangeUpdateDoneFlag);

	rc += sysfs_emit_at(buf, rc,
		"-----------------------------------------------\r\n");

	rc += sysfs_emit_at(buf, rc,
		"SetLatencyFlag = %3d, CommSetLatencyBusyFlag = %3d\r\n",
		pInfo->stEarcCmPollingInfo.bSetLatencyFlag,
		pInfo->stEarcCmPollingInfo.bCommonModeSetLatencyBusyFlag);
	rc += sysfs_emit_at(buf, rc,
		"LatencyContent = %3d, SetLatencyValue        = %3d\r\n",
		pInfo->stEarcCmPollingInfo.u8LatencyContent,
		pInfo->stEarcCmPollingInfo.u8SetLatencyValue);

	rc += sysfs_emit_at(buf, rc,
		"-----------------------------------------------\r\n");

	rc += sysfs_emit_at(buf, rc,
		"OPCodeOffset  = %3d, OPCodeDeviceID     = %3d\r\n",
		pInfo->stEarcCmPollingInfo.u16OPCodeOffset,
		pInfo->stEarcCmPollingInfo.u8OPCodeDeviceID);
	rc += sysfs_emit_at(buf, rc,
		"TransState    = %3d, TransResult        = %3d\r\n",
		pInfo->stEarcCmPollingInfo.u8TranState,
		pInfo->stEarcCmPollingInfo.u8TranResult);
	rc += sysfs_emit_at(buf, rc,
		"TransRetryCnt = %3d, TransDataThreshold = %3d\r\n",
		pInfo->stEarcCmPollingInfo.u8TransactioinRetryCnt,
		pInfo->stEarcCmPollingInfo.u8TransactionDataThreshold);

	kfree(pInfo);

	return rc;
}

static ssize_t CapChangeNotify_show
	(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	mdrv_EARC_SetCapibilityChange();
	rc += sysfs_emit_at(buf, rc,
		"V4L2_EVENT_CTRL_CH_EARC_CAPIBILITY is triggered!\r\n");

	return rc;
}

static ssize_t LatencyChangeNotify_show
	(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	mdrv_EARC_SetLatencyChange();
	rc += sysfs_emit_at(buf, rc,
		"V4L2_EVENT_CTRL_CH_EARC_LATENCY is triggered!\r\n");

	return rc;
}

static ssize_t DeviceNodeNum_show
	(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	//Do not arbitrarily modify the output format!
	rc += sysfs_emit_at(buf, rc,
		"%d,%d (Real, DTS)\r\n",
		real_device_node_num, dts_device_node_num);

	return rc;
}

static RD_ATTR_RO(All_contents_are_mtk_internal_use_only);
static RD_ATTR_RW(Info);
static RD_ATTR_WO(SupportMode);
static RD_ATTR_RO(CapChangeNotify);
static RD_ATTR_RO(LatencyChangeNotify);
static RD_ATTR_RO(DeviceNodeNum);
static RD_ATTR_WO(ArcPin);


const struct sysfs_ops earc_sysfs_ops = {
	.show = earc_attr_show,
	.store = earc_attr_store,
};

static struct kobj_type mtk_earc_ktype = {
	.sysfs_ops = &earc_sysfs_ops,
};

static struct attribute *mtk_earc_attrs[] = {
	&earc_attr_DiffTermination.attr,
	&earc_attr_DiffDriveStrength.attr,
	&earc_attr_DiffSkew.attr,
	&earc_attr_CommTermination.attr,
	&earc_attr_CommDriveStrength.attr,
	&earc_attr_EarcStatus.attr,
	&earc_attr_ArcPin.attr,
	&earc_attr_LogLevel.attr,
	NULL,
};

static struct attribute *mtk_earc_rd_attrs[] = {
	&dev_attr_All_contents_are_mtk_internal_use_only.attr,
	&dev_attr_Info.attr,
	&dev_attr_SupportMode.attr,
	&dev_attr_CapChangeNotify.attr,
	&dev_attr_LatencyChangeNotify.attr,
	&dev_attr_DeviceNodeNum.attr,
	&dev_attr_ArcPin.attr,
	NULL,
};


static const struct attribute_group mtk_earc_attr_group = {
	.attrs = mtk_earc_attrs,
};

static const struct attribute_group mtk_earc_rd_attr_group = {
	.name = "RD",
	.attrs = mtk_earc_rd_attrs,
};

int mtk_earc_sysfs_init(struct mtk_earc_dev *earc_dev)
{
	int ret = 0, n = 0, tmp = 0;
	char *s_data;

	dts_device_node_num = earc_dev->earc_fixed_dd_index;
	real_device_node_num = earc_dev->vdev->num;

	n = snprintf(NULL, 0, "%s", "eARC");

	if (n < 0) {
		EARC_MSG_ERROR("Fail to do snprintf, val = %d\n", n);
		return n;
	}

	s_data = kmalloc(n + 1, GFP_KERNEL);
	tmp = snprintf(s_data, n + 1, "%s", "eARC");

	if (tmp < 0) {
		EARC_MSG_ERROR("Fail to do snprintf, val = %d\n", tmp);
		return tmp;
	}
	ret = kobject_init_and_add(&earc_dev->earc_info.earc_kobj,
			&mtk_earc_ktype, earc_dev->mtkdbg_kobj, "%s", s_data);

	kfree(s_data);

	if (ret) {
		EARC_MSG_ERROR("Fail to create eARC sysfs files: %d\n", ret);
		return ret;
	}

	kobject_uevent(&earc_dev->earc_info.earc_kobj, KOBJ_ADD);
	ret = sysfs_create_group(&earc_dev->earc_info.earc_kobj,
			&mtk_earc_attr_group);
	if (ret) {
		EARC_MSG_ERROR("Fail to create eARC sysfs group: %d\n", ret);
		return ret;
	}

	/* sysfs for RD debug (internal use only) */
	ret = sysfs_create_group(&earc_dev->dev->kobj,
			&mtk_earc_rd_attr_group);
	if (ret) {
		EARC_MSG_ERROR("Fail to create eARC RD sysfs group: %d\n", ret);
		return ret;
	}

	return ret;
}

int mtk_earc_sysfs_deinit(struct mtk_earc_dev *earc_dev)
{
	int ret = 0;

	sysfs_remove_group(&earc_dev->earc_info.earc_kobj,
		&mtk_earc_attr_group);

	kobject_del(&earc_dev->earc_info.earc_kobj);

	/* sysfs for RD debug (internal use only) */
	sysfs_remove_group(&earc_dev->dev->kobj,
		&mtk_earc_rd_attr_group);

	return ret;
}

