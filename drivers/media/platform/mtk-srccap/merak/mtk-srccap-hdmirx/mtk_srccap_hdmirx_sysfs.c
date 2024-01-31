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
#include <linux/sysfs.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>

#include "mtk_srccap.h"
#include "mtk_srccap_hdmirx.h"
#include "mtk_srccap_hdmirx_phy.h"
#include "mtk_srccap_hdmirx_sysfs.h"
#include "drv_HDMI.h"
#include "drvXC_HDMI_Internal.h"
#include "drvXC_IOPort.h"
#include "drvXC_HDMI_if.h"

#include "hwreg_srccap_hdmirx_frl.h"
#include "hwreg_srccap_hdmirx.h"
#include "hwreg_srccap_hdmirx_mux.h"
#include "linux/metadata_utility/m_srccap.h"
#include "hwreg_srccap_hdmirx_phy.h"
#include "hwreg_srccap_hdmirx_mac.h"
#include "hwreg_srccap_hdmirx_frl.h"
#include "hwreg_srccap_hdmirx_hpd.h"
#include "hwreg_srccap_hdmirx_edid.h"
#include "hwreg_srccap_timingdetect.h"

/* interface for exporting hdmirx attributes */
struct hdmirx_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mtk_srccap_dev *srccap_dev,
			struct hdmirx_attribute *attr, char *buf);
	ssize_t (*store)(struct mtk_srccap_dev *srccap_dev,
			 struct hdmirx_attribute *attr, const char *buf,
			 size_t count);
};

/* hdmirx kobject attribute operations */
static ssize_t hdmirx_attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct hdmirx_attribute *kattr;
	struct mtk_srccap_dev *srccap_dev;
	struct srccap_hdmirx_info *hdmirx_info;
	ssize_t ret = -EIO;

	hdmirx_info = container_of(kobj, struct srccap_hdmirx_info, hdmirx_kobj);
	srccap_dev = container_of(hdmirx_info, struct mtk_srccap_dev, hdmirx_info);
	kattr = container_of(attr, struct hdmirx_attribute, attr);

	if (kattr->show)
		ret = kattr->show(srccap_dev, kattr, buf);
	return ret;
}

static ssize_t hdmirx_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *buf, size_t count)
{
	struct hdmirx_attribute *kattr;
	struct mtk_srccap_dev *srccap_dev;
	struct srccap_hdmirx_info *hdmirx_info;
	ssize_t ret = -EIO;

	hdmirx_info = container_of(kobj, struct srccap_hdmirx_info, hdmirx_kobj);
	srccap_dev = container_of(hdmirx_info, struct mtk_srccap_dev, hdmirx_info);
	kattr = container_of(attr, struct hdmirx_attribute, attr);
	if (kattr->store)
		ret = kattr->store(srccap_dev, kattr, buf, count);
	return ret;
}

const struct sysfs_ops hdmirx_sysfs_ops = {
	.show = hdmirx_attr_show,
	.store = hdmirx_attr_store,
};

#define HDMIRX_ATTR_RW(_name)                                                  \
	struct hdmirx_attribute hdmirx_attr_##_name = __ATTR_RW(_name)
#define HDMIRX_ATTR_RO(_name)                                                  \
	struct hdmirx_attribute hdmirx_attr_##_name = __ATTR_RO(_name)
#define HDMIRX_ATTR_WO(_name)                                                  \
	struct hdmirx_attribute hdmirx_attr_##_name = __ATTR_WO(_name)

/* interface for exporting hdmirx attributes */
struct hdmirx_p_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *attr, char *buf);
	ssize_t (*store)(struct mtk_srccap_dev *srccap_dev, int port,
			 struct hdmirx_p_attribute *attr, const char *buf,
			 size_t count);
};

/* hdmirx kobject attribute operations */
static ssize_t hdmirx_attr_p_show(struct kobject *kobj, struct attribute *attr,
				  char *buf)
{
	struct hdmirx_p_attribute *kattr;
	struct mtk_srccap_dev *srccap_dev;
	struct srccap_hdmirx_info *hdmirx_info;
	ssize_t ret = -EIO;
	int port;

	if (kstrtoint(kobj->name, 10, &port) != 0)
		return -EINVAL;
	hdmirx_info =
		container_of(kobj->parent, struct srccap_hdmirx_info, hdmirx_kobj);
	srccap_dev = container_of(hdmirx_info, struct mtk_srccap_dev, hdmirx_info);
	kattr = container_of(attr, struct hdmirx_p_attribute, attr);

	if (kattr->show)
		ret = kattr->show(srccap_dev, port, kattr, buf);
	return ret;
}

static ssize_t hdmirx_attr_p_store(struct kobject *kobj, struct attribute *attr,
				   const char *buf, size_t count)
{
	struct hdmirx_p_attribute *kattr;
	struct mtk_srccap_dev *srccap_dev;
	struct srccap_hdmirx_info *hdmirx_info;
	ssize_t ret = -EIO;
	int port;

	if (kstrtoint(kobj->name, 10, &port) != 0)
		return -EINVAL;
	hdmirx_info = container_of(kobj, struct srccap_hdmirx_info, hdmirx_kobj);
	srccap_dev = container_of(hdmirx_info, struct mtk_srccap_dev, hdmirx_info);
	kattr = container_of(attr, struct hdmirx_p_attribute, attr);

	if (kattr->store)
		ret = kattr->store(srccap_dev, port, kattr, buf, count);
	return ret;
}

const struct sysfs_ops hdmirx_sysfs_p_ops = {
	.show = hdmirx_attr_p_show,
	.store = hdmirx_attr_p_store,
};

#define HDMIRX_P_ATTR_RW(_name)                                                \
	struct hdmirx_p_attribute hdmirx_p_attr_##_name = __ATTR_RW(_name)
#define HDMIRX_P_ATTR_RO(_name)                                                \
	struct hdmirx_p_attribute hdmirx_p_attr_##_name = __ATTR_RO(_name)
#define HDMIRX_P_ATTR_WO(_name)                                                \
	struct hdmirx_p_attribute hdmirx_p_attr_##_name = __ATTR_WO(_name)

static ssize_t help_show(struct mtk_srccap_dev *srccap_dev,
			 struct hdmirx_attribute *kattr, char *buf)
{
	ssize_t len = 0;

	len += sysfs_emit_at(buf, len, "Debug Help:\n"
			    "- log_level <RW>: To control debug log level.\n"
			    "	levels:\n"
			    "		LOG_LEVEL_DEBUG   -- 6.\n"
			    "		LOG_LEVEL_INFO    -- 5.\n"
			    "		LOG_LEVEL_ERROR   -- 3.\n"
			    "		LOG_LEVEL_NORMAL  -- 1.\n");

	return len;
}

static ssize_t log_level_show(struct mtk_srccap_dev *srccap_dev,
			      struct hdmirx_attribute *kattr, char *buf)
{
	ssize_t len = 0;

	len += sysfs_emit_at(buf, len, "log_level = %d\n", hdmirxloglevel);

	return len;
}

static ssize_t log_level_store(struct mtk_srccap_dev *srccap_dev,
			       struct hdmirx_attribute *kattr, const char *buf,
			       size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	hdmirxloglevel = val;
	KDrv_SRCCAP_HDMIRx_setloglevel(hdmirxloglevel);
	return count;
}

struct hdmirx_crc {
	uint16_t ch_r;
	uint16_t ch_g;
	uint16_t ch_b;
	int nr_fail;
};

static void hdmi_crc_check(int port, struct hdmirx_crc *pcrc)
{
	uint16_t tmp = 0;
	E_MUX_INPUTPORT mux_port =
		mtk_hdmirx_to_muxinputport(port + V4L2_SRCCAP_INPUT_SOURCE_DVI);
	MDrv_HDMI_GetCRCValue(mux_port, MS_DVI_CHANNEL_R, &tmp);
	if (tmp ^ pcrc->ch_r) {
		pcrc->ch_r = tmp;
		pcrc->nr_fail++;
	}
	MDrv_HDMI_GetCRCValue(mux_port, MS_DVI_CHANNEL_G, &tmp);
	if (tmp ^ pcrc->ch_g) {
		pcrc->ch_g = tmp;
		pcrc->nr_fail++;
	}
	MDrv_HDMI_GetCRCValue(mux_port, MS_DVI_CHANNEL_B, &tmp);
	if (tmp ^ pcrc->ch_b) {
		pcrc->ch_b = tmp;
		pcrc->nr_fail++;
	}
}

#define crc_sleep_min 20000
#define crc_sleep_max 21000
#define crc_check_times 100
static ssize_t CRC_value_show(struct mtk_srccap_dev *srccap_dev,
			      struct hdmirx_attribute *kattr, char *buf)
{
	int count = 0;
	int nr_loop = crc_check_times;
	int port = srccap_dev->hdmirx_info.port_count;
	struct hdmirx_crc data[HDMI_PORT_MAX_NUM], *iter;

	//init status
	for (iter = &data[0]; iter < &data[port]; iter++) {
		hdmi_crc_check(iter - data, iter);
		iter->nr_fail = 0;
	}
	usleep_range(crc_sleep_min, crc_sleep_max);

	while (nr_loop-- > 0) {
		for (iter = &data[0]; iter < &data[port]; iter++)
			hdmi_crc_check(iter - data, iter);
		usleep_range(crc_sleep_min, crc_sleep_max);
	}

	count += sysfs_emit_at(buf, count, "HDMI port, Channel\t");
	count += sysfs_emit_at(buf, count, "R\tG\tB,\tfail_count\r\n");
	for (iter = &data[0]; iter < &data[port]; iter++) {
		count += sysfs_emit_at(buf, count, "%-8d\t", (int)(iter - data));
		count += sysfs_emit_at(buf, count, "%04x\t", iter->ch_r);
		count += sysfs_emit_at(buf, count, "%04x\t", iter->ch_g);
		count += sysfs_emit_at(buf, count, "%04x\t", iter->ch_b);
		count += sysfs_emit_at(buf, count, "%-8d\r\n", iter->nr_fail);
	}
	return count;
}

#define STR_LIMIT 30
#define COMMAND_LIMIT 10
static MS_U8 current_command_buf[COMMAND_LIMIT];
static MS_U16 current_command_type;
static MS_U16 current_command_buf_length;
static void Command_Parse(MS_U16 *command_type, MS_U8 *command_buf, char *buf,
			  size_t count)
{
	MS_U16 u16ForIndex;
	MS_U16 u16CommandInsertPtr = 0;
	MS_U8 save_command_type = 0;

	*command_type = 0;
	command_buf[u16CommandInsertPtr] = 0;
	if (count == 0) {
		*command_type = 0xff; //invalid command
	} else if (count <= STR_LIMIT) {
		for (u16ForIndex = 0; u16ForIndex < count; u16ForIndex++) {
			//printk("buff[%d] = %d\n",u16ForIndex,buf[u16ForIndex]);
			if (buf[u16ForIndex] == 32) {
				if (save_command_type == 0) {
					save_command_type = 1;
				} else {
					u16CommandInsertPtr = u16CommandInsertPtr + 1;
					command_buf[u16CommandInsertPtr] = 0;
				}
			} else if ((buf[u16ForIndex] >= '0') && (buf[u16ForIndex] <= '9')) {
				if (save_command_type == 0) {
					*command_type =
						(*command_type) * 10 + (buf[u16ForIndex] - '0');
				} else {
					command_buf[u16CommandInsertPtr] =
						command_buf[u16CommandInsertPtr] * 10 +
						(buf[u16ForIndex] - '0');
				}
			} else if (buf[u16ForIndex] == 10) {
				//line feed: end of command
				if (save_command_type == 0) {
					save_command_type = 1;
				} else {
					u16CommandInsertPtr = u16CommandInsertPtr + 1;
					command_buf[u16CommandInsertPtr] = 0;
				}
			}
		}
	}
	current_command_buf_length = u16CommandInsertPtr;
}

static ssize_t HDMI_PHY_TEST_show(struct mtk_srccap_dev *srccap_dev,
				  struct hdmirx_attribute *kattr, char *buf)
{
	//u16 u16Size = 65535;
	int intStrSize = 0;
	MS_U16 output_buff_length;
	//query CRC value
	//intStrSize =
	// snprintf(buf, 1000, "TEST this CLI command is alive, and test para is
	// %04x\n",test_para);

	// add test command
	return KDrv_SRCCAP_HDMIRx_phy_test_command_parser(
		current_command_type, current_command_buf_length, current_command_buf,
		&output_buff_length, NULL, &intStrSize, buf);
}

static int _sys_pkt_sel;
static const char *const _sysfs_pkt_name[] = {
	"MPEG",	    "AUI",	  "SPD",	"AVI",	   "GC",
	"ASAMPLE",  "ACR",	  "VS",		"NULL",	   "ISRC2",
	"ISRC1",    "ACP",	  "ONEBIT_AUD", "GM",	   "HBR",
	"VBI",	    "HDR",	  "RSV",	"EDR",	   "CHANNEL_STATUS",
	"MULTI_VS", "EMP_VENDOR", "EMP_VTEM",	"EMP_DSC", "EMP_HDR",
};

static ssize_t HDMI_INFO_show(struct mtk_srccap_dev *srccap_dev,
				  struct hdmirx_attribute *kattr, char *buf)
{
	int port = 0;
	int StrSize = 0;
	int Item = 0;
	int ItemSize = 0;
	MS_U32 tmp = 0;
	MS_U64 refresh_rate = 0;
	MS_U8 u8Y2_0 = 0;
	MS_U8 u8C1_0 = 0;
	MS_U8 u8EC2_0 = 0;
	MS_U8 u8ACE3_0 = 0;
	stHDMI_POLLING_INFO *pstPollingInfo;
	HDMI_STATUS_STRING *TxtContent;
	const char *const HDMI_STATUS_ITEM[] = {
		"5V",
		"Clock Detect",
		"DE Stable",
		"Source Version",
		"Width",
		"Height",
		"Htotal",
		"Vtotal",
		"HPolarity",
		"HSync",
		"HFPorch",
		"HBPorch",
		"VPolarity",
		"VSync",
		"VFPorch",
		"VBPorch",
		"Pixel Freq(KHz)",
		"Refresh Rate(mHz)",
		"I/P",
		"HDMI/DVI",
		"Color Space(1)",
		"Color Space(2)",
		"Color Depth",
		"Scrambling Status",
		"TMDS Clock Ratio",
		"TMDS Raw Clock(M)",
		"AV Mute",
		"BCH Error",
		"Packet Receive",
		"FRL Rate",
		"Lane Cnt",
		"Ratio Detect",
		"HDMI20 Flag",
		"YUV420 Flag",
		"HDMI Mode",
		"Dsc FSM",
		"Regen Fsm",
		"Hdcp status",
	};
	MS_U32 *InfoBuf;
	enum v4l2_srccap_input_source src = srccap_dev->srccap_info.src;

	pstPollingInfo = kmalloc(sizeof(stHDMI_POLLING_INFO),
			GFP_KERNEL);
	if (pstPollingInfo == NULL)
		return -EINVAL;

	TxtContent = kmalloc(HDMI_PORT_MAX_NUM*sizeof(HDMI_STATUS_STRING),
		GFP_KERNEL);
	if (TxtContent == NULL)
		return -EINVAL;


	InfoBuf = kmalloc(HDMI_PORT_MAX_NUM*ARRAY_SIZE(HDMI_STATUS_ITEM)*sizeof(MS_U32),
		GFP_KERNEL);
	if (InfoBuf == NULL)
		return -EINVAL;

	ItemSize = ARRAY_SIZE(HDMI_STATUS_ITEM);

	StrSize += sysfs_emit_at(
		buf, StrSize,
		"-------------------------HDMI_Status-------------------------\r\n");

	StrSize += sysfs_emit_at(buf, StrSize,
				"%-22s %10d (10:HDMI1)\t\n", "ActiveSrc(UI)", src);

	StrSize += sysfs_emit_at(buf, StrSize,
				"%-32sPort 0%10sPort 1%10sPort 2%10sPort 3\r\n\n", " ",
						" ", " ", " ");

	for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
		E_MUX_INPUTPORT eMuxPort = port + V4L2_SRCCAP_INPUT_SOURCE_DVI;

		MS_U8 u8HDMIInfoSource =
			KDrv_SRCCAP_HDMIRx_mux_inport_2_src(eMuxPort);

		*pstPollingInfo = MDrv_HDMI_Get_HDMIPollingInfo(eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_5V) =
			MDrv_HDMI_GetSCDCCableDetectFlag(eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_CLOCK_DETECT) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_CLOCKSTABLE_STATUS, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_DE_STABLE) =
			KDrv_SRCCAP_HDMIRx_mac_get_destable(eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_PACKET_RECEIVE) =
			MDrv_HDMI_Get_PacketStatus(eMuxPort);

		switch (pstPollingInfo->ucSourceVersion) {
		case HDMI_SOURCE_VERSION_NOT_SURE:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8SourceVer,
				"NotSure", strlen("NotSure") + 1);
			break;
		case HDMI_SOURCE_VERSION_HDMI14:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8SourceVer,
				"1.4", strlen("1.4") + 1);
			break;
		case HDMI_SOURCE_VERSION_HDMI20:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8SourceVer,
				"2.0", strlen("2.0") + 1);
			break;
		case HDMI_SOURCE_VERSION_HDMI21:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8SourceVer,
				"2.1", strlen("2.1") + 1);
			break;
		default:
			break;
		}

		switch (pstPollingInfo->u8FRLRate) {
		case HDMI_FRL_MODE_LEGACY:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"LEGACY", strlen("LEGACY") + 1);
			break;
		case HDMI_FRL_MODE_FRL_3G_3CH:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_3G_3CH", strlen("FRL_3G_3CH") + 1);
			break;
		case HDMI_FRL_MODE_FRL_6G_3CH:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_6G_3CH", strlen("FRL_6G_3CH") + 1);
			break;
		case HDMI_FRL_MODE_FRL_6G_4CH:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_6G_4CH", strlen("FRL_6G_4CH") + 1);
			break;
		case HDMI_FRL_MODE_FRL_8G:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_8G_4CH", strlen("FRL_8G_4CH") + 1);
			break;
		case HDMI_FRL_MODE_FRL_10G:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_10G_4CH", strlen("FRL_10G_4CH") + 1);
			break;
		case HDMI_FRL_MODE_FRL_12G:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"FRL_12G_4CH", strlen("FRL_12G_4CH") + 1);
			break;
		case HDMI_FRL_MODE_NONE:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"None", strlen("None") + 1);
			break;
		case HDMI_FRL_MODE_LEGACY_14:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"LEGACY_14", strlen("LEGACY_14") + 1);
			break;
		case HDMI_FRL_MODE_LEGACY_20:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"LEGACY_20", strlen("LEGACY_20") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate,
				"N/A", strlen("N/A") + 1);
			break;
		}

		switch (pstPollingInfo->u8RatioDetect) {
		case HDMI_FRL_MODE_LEGACY_14:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RatDet,
				"LEGACY_14", strlen("LEGACY_14") + 1);
			break;
		case HDMI_FRL_MODE_LEGACY_20:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RatDet,
				"LEGACY_20", strlen("LEGACY_20") + 1);
			break;
		case HDMI_FRL_MODE_LEGACY:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RatDet,
				"LEGACY", strlen("LEGACY") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RatDet,
				"None", strlen("None") + 1);
			break;
		}

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_LANE_CNT) =
			pstPollingInfo->u8LaneCnt;

		/* Get DEP timing */
		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_WIDTH) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_HDE, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HEIGHT) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_VDE, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HTOTAL) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_HTT, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_VTOTAL) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_VTT, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_HPolarity) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_H_POLARITY, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HSync) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_HSYNC, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HFPorch) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_HFRONT, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HBPorch) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_HBACK, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_VPolarity) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_V_POLARITY, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_VSync) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_VSYNC, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_VFPorch) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_VFRONT, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_VBPorch) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_VBACK, eMuxPort);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_PIXEL_FREQUENCY) =
			MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_PIXEL_CLOCK, eMuxPort);

		if (MDrv_HDMI_GetDataInfoByPort(E_HDMI_GET_INTERLACE, eMuxPort))
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8IorP, "I",
				strlen("I") + 1);
		else
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8IorP, "P",
				strlen("P") + 1);

		if (MDrv_HDMI_IsHDMI_Mode(eMuxPort))
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8HDMIorDVI,
				"HDMI", strlen("HDMI") + 1);
		else
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8HDMIorDVI,
				"DVI", strlen("DVI") + 1);

		u8Y2_0 = (KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
					  u8HDMIInfoSource, _BYTE_1, pstPollingInfo) >>
				  AVI_INFO_Y20_START_BIT) &
				 VALID_3_BIT_MASK;
		switch (u8Y2_0) {
		case E_Y20_RGB:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1,
				"RGB", strlen("RGB") + 1);
			break;
		case E_Y20_YCC422:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1,
				"YCC422", strlen("YCC422") + 1);
			break;
		case E_Y20_YCC444:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1,
				"YCC444", strlen("YCC444") + 1);
			break;
		case E_Y20_YCC420:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1,
				"YCC420", strlen("YCC420") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1,
				"N/A", strlen("N/A") + 1);
			break;
		}

		u8C1_0 = (KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
					  u8HDMIInfoSource, _BYTE_2, pstPollingInfo) >>
				  AVI_INFO_C10_START_BIT) &
				 VALID_2_BIT_MASK;
		u8EC2_0 = (KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
					   u8HDMIInfoSource, _BYTE_3, pstPollingInfo) >>
				   AVI_INFO_EC20_START_BIT) &
				  VALID_3_BIT_MASK;
		u8ACE3_0 = (KDrv_SRCCAP_HDMIRx_pkt_avi_infoframe_info(
						u8HDMIInfoSource, _BYTE_14, pstPollingInfo) >>
					AVI_INFO_ACE20_START_BIT) &
				   VALID_4_BIT_MASK;
		tmp = (u8ACE3_0 << _BYTE_5) | (u8EC2_0 << _BYTE_2) | u8C1_0;
		switch (tmp) {
		case E_COLORIMETRY_SMPTE_170M:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"SMPTE_170M", strlen("SMPTE_170M") + 1);
			break;
		case E_COLORIMETRY_BT_709:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"BT_709", strlen("BT_709") + 1);
			break;
		case E_COLORIMETRY_xvYCC601:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"xvYCC601", strlen("xvYCC601") + 1);
			break;
		case E_COLORIMETRY_xvYCC709:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"xvYCC709", strlen("xvYCC709") + 1);
			break;
		case E_COLORIMETRY_sYCC601:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"sYCC601", strlen("sYCC601") + 1);
			break;
		case E_COLORIMETRY_Adobe_YCC601:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"Adobe_YCC601", strlen("Adobe_YCC601") + 1);
			break;
		case E_COLORIMETRY_Adobe_RGB:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"Adobe_RGB", strlen("Adobe_RGB") + 1);
			break;
		case E_COLORIMETRY_BT_2020_EC5:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"BT_2020_EC5", strlen("BT_2020_EC5") + 1);
			break;
		case E_COLORIMETRY_BT_2020_EC6:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"BT_2020_EC6", strlen("BT_2020_EC6") + 1);
			break;
		case E_COLORIMETRY_DCI_P3_D65:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"DCI_P3_D65", strlen("DCI_P3_D65") + 1);
			break;
		case E_COLORIMETRY_DCI_P3_Theater:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"DCI_P3_Theater", strlen("DCI_P3_Theater") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2,
				"N/A", strlen("N/A") + 1);
			break;
		}

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_SCRAMBLING_STATUS) =
			MDrv_HDMI_GetSCDCValue(eMuxPort, HDMI_SCDC_ADDRESS_20) & 0x01;

		tmp = (MDrv_HDMI_GetSCDCValue(eMuxPort, HDMI_SCDC_ADDRESS_20) >> 0x01) &
			  0x01;
		if (tmp)
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ClockRatio,
				"1/40", strlen("1/40") + 1);
		else
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8ClockRatio,
				"1/10", strlen("1/10") + 1);

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_TMDS_SAMPLE_CLOCK) =
			pstPollingInfo->usClockCount;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_AV_MUTE) =
			MDrv_HDMI_GC_Info(eMuxPort, G_CTRL_AVMUTE) & 0x01;

		tmp = (MDrv_HDMI_GC_Info(eMuxPort, G_Ctrl_CD_VAL) >>
			   GCP_COLOR_DEPTH_START_BIT) &
			  VALID_4_BIT_MASK;
		switch (tmp) {
		case E_COLORDEPTH_ZERO:
		case E_COLORDEPTH_24BITS_PER_PIXEL:
			*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
				E_VIDEO_STATUS_COLOR_DEPTH) = COLOR_DEPTH_VAL_8;
			break;
		case E_COLORDEPTH_30BITS_PER_PIXEL:
			*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
				E_VIDEO_STATUS_COLOR_DEPTH) = COLOR_DEPTH_VAL_10;
			break;
		case E_COLORDEPTH_36BITS_PER_PIXEL:
			*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
				E_VIDEO_STATUS_COLOR_DEPTH) = COLOR_DEPTH_VAL_12;
			break;
		case E_COLORDEPTH_48BITS_PER_PIXEL:
			*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
				E_VIDEO_STATUS_COLOR_DEPTH) = COLOR_DEPTH_VAL_16;
			break;
		default:
			*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
				E_VIDEO_STATUS_COLOR_DEPTH) = 0;
			break;
		}

		refresh_rate = *(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_PIXEL_FREQUENCY);

		if (*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_HTOTAL) > 0) {
			refresh_rate *= INTERNATIONAL_UNIT_KILO;
			do_div(refresh_rate, *(MS_U32 *)(InfoBuf +
				port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_HTOTAL));
		} else
			refresh_rate = 0;

		if (*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_VTOTAL) > 0) {
			refresh_rate *= INTERNATIONAL_UNIT_KILO;
			do_div(refresh_rate, *(MS_U32 *)(InfoBuf +
				port*ARRAY_SIZE(HDMI_STATUS_ITEM) + E_VIDEO_STATUS_VTOTAL));
		} else
			refresh_rate = 0;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_REFRESH_RATE) = (MS_U32)refresh_rate;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_BCH_ERROR) = pstPollingInfo->u8ErrorStatus;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_HDMI20_Flag) = pstPollingInfo->bHDMI20Flag;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_YUV420_Flag) = pstPollingInfo->bYUV420Flag;

		*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
			E_VIDEO_STATUS_HDMI_Mode) = pstPollingInfo->bHDMIModeFlag;

		switch (pstPollingInfo->u8Dsc) {
		case E_DSC_FSM_NONE_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM,
				"None", strlen("None") + 1);
			break;
		case E_DSC_FSM_HV_REGEN_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM,
				"DSC_REGEN", strlen("DSC_REGEN") + 1);
			break;
		case E_DSC_FSM_DSC_CONFIG_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM,
				"DSC_CONFIG", strlen("DSC_CONFIG") + 1);
			break;
		case E_DSC_FSM_REGEN_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM,
				"DSC_REGEN", strlen("DSC_REGEN") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM,
				"NA", strlen("NA") + 1);
			break;
		}

		switch (pstPollingInfo->u8RegenFsm) {
		case E_REGEN_INIT_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RegFSM,
				"REGEN_INIT", strlen("REGEN_INIT") + 1);
			break;
		case E_REGEN_CONFIG_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RegFSM,
				"REGEN_CONFIG", strlen("REGEN_CONFIG") + 1);
			break;
		case E_REGEN_DONE_SYS:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RegFSM,
				"REGEN_DONE", strlen("REGEN_DONE") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8RegFSM,
				"None", strlen("None") + 1);
			break;
		}

		switch (pstPollingInfo->ucHDCPState) {
		case HDMI_HDCP_NO_ENCRYPTION:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8Hdcp,
				"NO_ENC", strlen("NO_ENC") + 1);
			break;
		case HDMI_HDCP_1_4:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8Hdcp,
				"HDCP_1_4", strlen("HDCP_1_4") + 1);
			break;
		case HDMI_HDCP_2_2:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8Hdcp,
				"HDCP_2_2", strlen("HDCP_2_2") + 1);
			break;
		default:
			strncpy(((HDMI_STATUS_STRING *)(TxtContent + port))->u8Hdcp,
				"None", strlen("None") + 1);
			break;
		}
	}

	for (Item = 0; Item < ItemSize; Item++) {
		StrSize +=
			sysfs_emit_at(buf, StrSize, "%-22s", HDMI_STATUS_ITEM[Item]);
		for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
			tmp = Item;

			/*if (InfoBuf[port][E_VIDEO_STATUS_CLOCK_DETECT] &
				InfoBuf[port][E_VIDEO_STATUS_DE_STABLE])
				tmp = Item;
			else
				tmp = INVALID_DATA;
			*/

			switch (tmp) {
			case E_VIDEO_STATUS_INTERLACE:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8IorP);
				break;
			case E_VIDEO_STATUS_SOURCE_VERSION:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
				((HDMI_STATUS_STRING *)(TxtContent + port))->u8SourceVer);
				break;
			case E_VIDEO_STATUS_FRL_RATE:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8FrlRate);
				break;
			case E_VIDEO_STATUS_RATION_DET:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8RatDet);
				break;
			case E_VIDEO_STATUS_HDMI_DVI:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
				((HDMI_STATUS_STRING *)(TxtContent + port))->u8HDMIorDVI);
				break;
			case E_VIDEO_STATUS_COLOR_SPACE_1:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
				((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace1);
				break;
			case E_VIDEO_STATUS_COLOR_SPACE_2:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
				((HDMI_STATUS_STRING *)(TxtContent + port))->u8ColorSpace2);
				break;
			case E_VIDEO_STATUS_TMDS_CLOCK_RATIO:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
				((HDMI_STATUS_STRING *)(TxtContent + port))->u8ClockRatio);
				break;
			case E_VIDEO_STATUS_DSC_FSM:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8DscFSM);
				break;
			case E_VIDEO_STATUS_Regen_Fsm:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8RegFSM);
				break;
			case E_VIDEO_STATUS_HDCP_STATUS:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	",
					((HDMI_STATUS_STRING *)(TxtContent + port))->u8Hdcp);
				break;
			case INVALID_DATA:
				StrSize += sysfs_emit_at(buf, StrSize, "%14s	", "N/A");
				break;
			default:
				StrSize += sysfs_emit_at(buf, StrSize, "%14d	",
				*(MS_U32 *)(InfoBuf + port*ARRAY_SIZE(HDMI_STATUS_ITEM) +
					Item));
				break;
			}
		}
		StrSize += sysfs_emit_at(buf, StrSize, "\r\n");
	}

	kfree(pstPollingInfo);
	kfree(TxtContent);
	kfree(InfoBuf);

	return StrSize;
}


static ssize_t ALL_INFO_show(struct mtk_srccap_dev *srccap_dev,
				  struct hdmirx_attribute *kattr, char *buf)
{
	int intStrSize = 0;
	int port = 0;
	int pkt_sel = 0;
	int pkt_count = 0;
	int pkt_name_len = ARRAY_SIZE(_sysfs_pkt_name);
	unsigned int pkt_len;
	enum v4l2_srccap_input_source src;
	enum v4l2_ext_hdmi_packet_state enPacketType;
	struct hdmi_packet_info *gnl_ptr;

	gnl_ptr = kzalloc(sizeof(struct hdmi_packet_info)*
		META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE, GFP_KERNEL);

	if (!gnl_ptr) {
		HDMIRX_MSG_ERROR("Fail to allocate gnl_ptr\n");
		return 0;
	}

	intStrSize +=
		sysfs_emit_at(buf, intStrSize,
			"--------------------ALL INFO--------------------\r\n");
	intStrSize += sysfs_emit_at(buf, intStrSize,
			"%-20s\t0\t1\t2\t3\r\n\n", "Port");

	//Cable Show
	intStrSize +=
		sysfs_emit_at(buf, intStrSize, "%-20s\t", "Cable");
	for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
		E_MUX_INPUTPORT e_inputport = port + V4L2_SRCCAP_INPUT_SOURCE_DVI;

		intStrSize +=
			sysfs_emit_at(buf, intStrSize, "%d\t",
					 KDrv_SRCCAP_HDMIRx_mac_get_scdccabledet(e_inputport));
	}

	intStrSize += sysfs_emit_at(buf, intStrSize, "\r\n");

	//Frl Show
	intStrSize += sysfs_emit_at(buf, intStrSize, "%-20s\t", "Frl");
	for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
		E_MUX_INPUTPORT e_inputport = port + V4L2_SRCCAP_INPUT_SOURCE_DVI;
		if (KDrv_SRCCAP_HDMIRx_a_query_2p1_port(e_inputport)) {
			intStrSize += sysfs_emit_at(buf, intStrSize, "1\t");
		} else {
			intStrSize += sysfs_emit_at(buf, intStrSize, "0\t");
		}
	}
	intStrSize += sysfs_emit_at(buf, intStrSize, "\r\n");

	//DSC Show
	intStrSize += sysfs_emit_at(buf, intStrSize, "%-20s\t", "Dsc");
	for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
		E_MUX_INPUTPORT e_inputport = port + V4L2_SRCCAP_INPUT_SOURCE_DVI;
		if (KDrv_SRCCAP_HDMIRx_a_query_dsc_port(e_inputport)) {
			intStrSize += sysfs_emit_at(buf, intStrSize, "1\t");
		} else {
			intStrSize += sysfs_emit_at(buf, intStrSize, "0\t");
		}
	}
	intStrSize += sysfs_emit_at(buf, intStrSize, "\r\n");

	//Pkt Show
	intStrSize +=
		sysfs_emit_at(buf, intStrSize, "\r\nPkt_dump_count\r\n");
	for (pkt_sel = 0; pkt_sel < pkt_name_len; pkt_sel++) {
		intStrSize += sysfs_emit_at(buf, intStrSize, "%-20s\t",
							   _sysfs_pkt_name[pkt_sel]);

		for (port = 0; port < srccap_dev->hdmirx_info.port_count; port++) {
			src = mtk_hdmirx_search_hardware_port_num(port);
			enPacketType = (enum v4l2_ext_hdmi_packet_state)(1 << pkt_sel);

			switch (enPacketType) {
			case V4L2_EXT_HDMI_PKT_VS:
			case V4L2_EXT_HDMI_PKT_MULTI_VS:
				pkt_len = META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE *
						  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
				break;
			case V4L2_EXT_HDMI_PKT_EMP_VENDOR:
				pkt_len = META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE *
						  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
				break;
			case V4L2_EXT_HDMI_PKT_EMP_DSC:
				pkt_len = META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE *
						  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
				break;
			case V4L2_EXT_HDMI_PKT_EMP_HDR:
				pkt_len = META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE *
						  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
				break;
			default:
				pkt_len = 1 * META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
				break;
			}

			pkt_count = mtk_srccap_hdmirx_pkt_get_gnl(src, enPacketType,
							gnl_ptr, pkt_len);

			intStrSize +=
				sysfs_emit_at(buf, intStrSize, "%d\t", pkt_count);
		}
		intStrSize += sysfs_emit_at(buf, intStrSize, "\r\n");
	}

	kfree(gnl_ptr);

	return intStrSize;
}

#define TIMESTAMP_NUMBER 4
static ssize_t TIMING_DETECT_show(struct mtk_srccap_dev *srccap_dev,
				  struct hdmirx_attribute *kattr, char *buf)
{
	int intStrSize = 0;
	int i = 0, j = 0;
	int bufsz = 0;
	char TimePoint[] = {'A', 'B', 'C', 'D'};
	stHDMIRx_TIMING_LOG *log_buf;

	bufsz = (STR_UNIT_LENGTH << 1);

	log_buf = kmalloc(
		(sizeof(stHDMIRx_TIMING_LOG) * srccap_dev->hdmirx_info.port_count),
		GFP_KERNEL);

	if (!log_buf) {
		intStrSize += snprintf(buf + intStrSize, bufsz,
					"Error! memory not allocated.\r\n");

		return intStrSize;
	}

	KDrv_SRCCAP_HDMIRx_getTimingDetectData(log_buf);

	intStrSize += snprintf(
		buf + intStrSize, bufsz,
			"-------------------------TIMING DETECT-------------------------\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz, "<Timestamp>\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz, "A: Detect 5V\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz,
			"B: Clock Stable / Link Training Done\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz,
			"C: SCDT = Data Enable = Regen Start\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz,
			"D: Capture Done = De Stable = Regen Done\r\n");
	intStrSize += snprintf(buf + intStrSize, bufsz, "(Unit: ms)\r\n\n");
	intStrSize += snprintf(
		buf + intStrSize, bufsz,
			"    \t    Port 0  \t  Port 1  \t  Port 2  \t  Port 3  \t\r\n\n");

	for (i = 0; i < TIMESTAMP_NUMBER; i++) {
		intStrSize += snprintf(buf + intStrSize, bufsz,	"%c      \t", TimePoint[i]);

		for (j = 0; j < srccap_dev->hdmirx_info.port_count; j++) {
			if ((log_buf + j)->u8_TimingDetectStatus !=
				E_TIMING_DETECT_DE_STABLE)
				intStrSize +=
					snprintf(buf + intStrSize, bufsz, "%10s\t", "No Data");
			else
				intStrSize += snprintf(buf + intStrSize, bufsz, "%10d\t",
						(log_buf+j)->u32_TimeStamp[i]);
		}

		intStrSize += snprintf(buf + intStrSize, bufsz,	"\r\n");
	}

	intStrSize += snprintf(buf + intStrSize, bufsz,	"\r\n");

	for (i = 0; i < TIMESTAMP_NUMBER - 1; i++) {
		intStrSize += snprintf(buf + intStrSize, bufsz,	"%c to %c\t",
					TimePoint[i], TimePoint[i + 1]);

		for (j = 0; j < srccap_dev->hdmirx_info.port_count; j++) {
			if ((log_buf + j)->u8_TimingDetectStatus !=
				E_TIMING_DETECT_DE_STABLE)
				intStrSize +=
					snprintf(buf + intStrSize, bufsz, "%10s\t", "No Data");
			else
				intStrSize += snprintf(buf + intStrSize, bufsz, "%10d\t",
					((log_buf+j)->u32_TimeStamp[i + 1] -
					 (log_buf+j)->u32_TimeStamp[i]));
		}

		intStrSize += snprintf(buf + intStrSize, bufsz, "\r\n\n");
	}

	kfree(log_buf);

	return intStrSize;
}

static ssize_t HDMI_PHY_TEST_store(struct mtk_srccap_dev *srccap_dev,
				   struct hdmirx_attribute *kattr,
				   const char *buf, size_t count)
{
	Command_Parse(&current_command_type, current_command_buf, (char *)buf,
		      count);
	return count;
}

static ssize_t ps_enable_show(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	int port_sel = 0;
	u16 u16_hwdef[HDMI_VAL_4];
	u16 u16_swprog[HDMI_VAL_4];

	for (port_sel = 0; port_sel < srccap_dev->hdmirx_info.port_count;
		 port_sel++) {

		if (port_sel >= HDMI_VAL_4)
			break;

		u16_hwdef[port_sel]
			= mtk_srccap_hdmirx_get_pwr_saving_onoff_hwdef(
(enum v4l2_srccap_input_source)port_sel + V4L2_SRCCAP_INPUT_SOURCE_HDMI)
? 1 : 0;

		u16_swprog[port_sel]
			= mtk_srccap_hdmirx_get_pwr_saving_onoff(
(enum v4l2_srccap_input_source)port_sel + V4L2_SRCCAP_INPUT_SOURCE_HDMI)
? 1 : 0;
	}

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"\tHW_DEF\tSW_PROG\n"
		"P0\t%04x\t%04x\n"
		"P1\t%04x\t%04x\n"
		"P2\t%04x\t%04x\n"
		"P3\t%04x\t%04x\n",
		u16_hwdef[HDMI_VAL_0], u16_swprog[HDMI_VAL_0],
		u16_hwdef[HDMI_VAL_1], u16_swprog[HDMI_VAL_1],
		u16_hwdef[HDMI_VAL_2], u16_swprog[HDMI_VAL_2],
		u16_hwdef[HDMI_VAL_3], u16_swprog[HDMI_VAL_3]);

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}


static ssize_t ps_enable_store(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;
	int port_sel = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	for (port_sel = 0; port_sel < srccap_dev->hdmirx_info.port_count;
		 port_sel++) {
		mtk_srccap_hdmirx_set_pwr_saving_onoff(
(enum v4l2_srccap_input_source)port_sel + V4L2_SRCCAP_INPUT_SOURCE_HDMI,
			val);
	}
	return count;
}

static ssize_t audio_fs_show(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	int port_sel = 0;
	u32 u32_audo_fs[HDMI_VAL_4];

	for (port_sel = 0; port_sel < srccap_dev->hdmirx_info.port_count;
		 port_sel++) {

		if (port_sel >= HDMI_VAL_4)
			break;

		u32_audo_fs[port_sel]
			= mtk_srccap_hdmirx_pkt_get_audio_fs_hz(
				(enum v4l2_srccap_input_source)port_sel +
				V4L2_SRCCAP_INPUT_SOURCE_HDMI);
	}

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"\tAUDIO_FS_Hz\n"
		"P0\t%08u\n"
		"P1\t%08u\n"
		"P2\t%08u\n"
		"P3\t%08u\n",
		u32_audo_fs[HDMI_VAL_0],
		u32_audo_fs[HDMI_VAL_1],
		u32_audo_fs[HDMI_VAL_2],
		u32_audo_fs[HDMI_VAL_3]);

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}


static ssize_t hdmi_suspend_store(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;
	int port_sel = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	if (val)
		mtk_hdmirx_SetPowerState(srccap_dev, E_POWER_SUSPEND);
	else {
		mtk_hdmirx_SetPowerState(srccap_dev, E_POWER_RESUME);

		for (port_sel = 0;
			port_sel < srccap_dev->hdmirx_info.port_count;
			port_sel++) {

			if (port_sel >= HDMI_VAL_4)
				break;

			/*pull high*/
			MDrv_HDMI_pullhpd(
				TRUE,
				(E_MUX_INPUTPORT)(port_sel + INPUT_PORT_DVI0),
				FALSE);
		}
	}

	return count;
}

static ssize_t hdmi_str_loop_store(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;
	int port_sel = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	while (val--) {
		mtk_hdmirx_SetPowerState(srccap_dev, E_POWER_SUSPEND);
		msleep(1000);
		mtk_hdmirx_SetPowerState(srccap_dev, E_POWER_RESUME);

		for (port_sel = 0;
			port_sel < srccap_dev->hdmirx_info.port_count;
			port_sel++) {

			if (port_sel >= HDMI_VAL_4)
				break;

			/*pull high*/
			MDrv_HDMI_pullhpd(
				TRUE,
				(E_MUX_INPUTPORT)(port_sel + INPUT_PORT_DVI0),
				FALSE);
		}

		msleep(3000);
	}
	return count;
}

static ssize_t unmute_dbs_show(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	int dbs_ms = (int)MDrv_HDMI_Get_UnMute_Debounce_ms();

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"unmute_debounce = %d (ms)\n", dbs_ms);

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}


static ssize_t unmute_dbs_store(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	MDrv_HDMI_Set_UnMute_Debounce_ms((MS_U32)val);

	return count;
}

static ssize_t mute_msk_show(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	int mute_msk = (int)MDrv_HDMI_Get_Mute_Msk();

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"mute_msk = 0x%x\n", mute_msk);

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}


static ssize_t mute_msk_store(
	struct mtk_srccap_dev *srccap_dev,
	struct hdmirx_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	MDrv_HDMI_Set_Mute_Msk((MS_U32)val);

	return count;
}

static HDMIRX_ATTR_RO(help);
static HDMIRX_ATTR_RW(log_level);
static HDMIRX_ATTR_RO(CRC_value);
static HDMIRX_ATTR_RW(HDMI_PHY_TEST);
static HDMIRX_ATTR_RO(ALL_INFO);
static HDMIRX_ATTR_RO(TIMING_DETECT);
static HDMIRX_ATTR_RO(HDMI_INFO);
static HDMIRX_ATTR_RW(ps_enable);
static HDMIRX_ATTR_RO(audio_fs);
static HDMIRX_ATTR_WO(hdmi_suspend);
static HDMIRX_ATTR_WO(hdmi_str_loop);
static HDMIRX_ATTR_RW(unmute_dbs);
static HDMIRX_ATTR_RW(mute_msk);

static struct attribute *mtk_src_cap_hdmirx_attrs[] = {
	&hdmirx_attr_help.attr,		 &hdmirx_attr_log_level.attr,
	&hdmirx_attr_CRC_value.attr, &hdmirx_attr_HDMI_PHY_TEST.attr,
	&hdmirx_attr_ALL_INFO.attr,  &hdmirx_attr_TIMING_DETECT.attr,
	&hdmirx_attr_HDMI_INFO.attr, &hdmirx_attr_ps_enable.attr,
	&hdmirx_attr_audio_fs.attr, &hdmirx_attr_hdmi_suspend.attr,
	&hdmirx_attr_hdmi_str_loop.attr, &hdmirx_attr_unmute_dbs.attr,
	&hdmirx_attr_mute_msk.attr, NULL,
};

static const struct attribute_group mtk_src_cap_hdmirx_attr_group = {
	.attrs = mtk_src_cap_hdmirx_attrs,
};

#define DUMP_WRAP_SIZE 8

static void _dump_pkt(int port, int pkt_sel, char *buf, ssize_t *buf_size)
{
	int pkt_count = 0;
	unsigned int pkt_len;
	enum v4l2_srccap_input_source src;
	struct hdmi_packet_info *gnl_ptr =  NULL;
	enum v4l2_ext_hdmi_packet_state enPacketType;
	if (!(pkt_sel >= 0 && pkt_sel < sizeof(_sysfs_pkt_name)/sizeof(char *)))
		return;

	gnl_ptr = kzalloc(
	sizeof(struct hdmi_packet_info)*META_SRCCAP_HDMIRX_PKT_COUNT_MAX_SIZE,
	GFP_KERNEL);

	if (!gnl_ptr)
		return;

	src = mtk_hdmirx_search_hardware_port_num(port);
	enPacketType = (enum v4l2_ext_hdmi_packet_state)(1 << pkt_sel);

	switch (enPacketType) {
	case V4L2_EXT_HDMI_PKT_VS:
	case V4L2_EXT_HDMI_PKT_MULTI_VS:
		pkt_len = META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE *
				  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
		break;
	case V4L2_EXT_HDMI_PKT_EMP_VENDOR:
		pkt_len = META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE *
				  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
		break;
	case V4L2_EXT_HDMI_PKT_EMP_DSC:
		pkt_len = META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE *
				  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
		break;
	case V4L2_EXT_HDMI_PKT_EMP_HDR:
		pkt_len = META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE *
				  META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
		break;
	default:
		pkt_len = 1 * META_SRCCAP_HDMIRX_PKT_MAX_SIZE;
		break;
	}

	pkt_count =
		mtk_srccap_hdmirx_pkt_get_gnl(src, enPacketType, gnl_ptr, pkt_len);

	(*buf_size) +=
		snprintf(buf + (*buf_size), sizeof(_sysfs_pkt_name),
				 "Dump %s , Count = %d\n", _sysfs_pkt_name[pkt_sel], pkt_count);

	if (pkt_count > 0) {
		int i;

		for (i = 0; i < pkt_count; i++) {
			int j;

			(*buf_size) +=
				snprintf(buf + (*buf_size), sizeof(_sysfs_pkt_name),
					"hb %02X %02X %02X\n", gnl_ptr[i].hb[E_INDEX_0],
						 gnl_ptr[i].hb[E_INDEX_1],
						 gnl_ptr[i].hb[E_INDEX_2]);
			(*buf_size) += snprintf(buf + (*buf_size), sizeof("pb"), "pb");
			for (j = 0; j < sizeof(gnl_ptr[i].pb); j++) {
				if ((j % DUMP_WRAP_SIZE) == 0)
				(*buf_size) +=
						snprintf(buf + (*buf_size), sizeof("\n"), "\n");
				(*buf_size) += snprintf(buf + (*buf_size), STR_UNIT_LENGTH,
						"%02X ", gnl_ptr[i].pb[j]);
			}
			(*buf_size) += snprintf(buf + (*buf_size), sizeof("\n"), "\n");
		}
	}
	kfree(gnl_ptr);

}

static ssize_t pkt_all_show(struct mtk_srccap_dev *srccap_dev, int port,
			    struct hdmirx_p_attribute *kattr, char *buf)
{
	int i;
	ssize_t len = 0;
	int pkt_name_len = ARRAY_SIZE(_sysfs_pkt_name);

	for (i = 0; i < pkt_name_len; i++)
		_dump_pkt(port, i, buf, &len);

	return len;
}

static ssize_t pkt_show(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *kattr, char *buf)
{
	int i;
	ssize_t len = 0;
	int pkt_name_len = ARRAY_SIZE(_sysfs_pkt_name);

	if ((_sys_pkt_sel < 0) || (_sys_pkt_sel >= pkt_name_len)) {
		for (i = 0; i < pkt_name_len; i++) {
			len += snprintf(buf + len, sizeof(_sysfs_pkt_name), "%d = %s\n", i,
					_sysfs_pkt_name[i]);
		}
	} else {
		_dump_pkt(port, _sys_pkt_sel, buf, &len);
	}

	return len;
}

static ssize_t pkt_store(struct mtk_srccap_dev *srccap_dev, int port,
			 struct hdmirx_p_attribute *kattr, const char *buf,
			 size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	_sys_pkt_sel = val;

	return count;
}

static ssize_t cable_show(struct mtk_srccap_dev *srccap_dev, int port,
			  struct hdmirx_p_attribute *kattr, char *buf)
{
	int value = 0;
	E_MUX_INPUTPORT e_inputport = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_DVI);

	if (KDrv_SRCCAP_HDMIRx_mac_get_scdccabledet(e_inputport))
		value = 1;

	return snprintf(buf, sizeof("\n") + 5, "%d\n", value);
}

static ssize_t frl_show(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *kattr, char *buf)
{
	int value = 0;
	E_MUX_INPUTPORT e_inputport = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_DVI);

	if (KDrv_SRCCAP_HDMIRx_a_query_2p1_port(
			e_inputport))
		value = 1;

	return snprintf(buf, sizeof("\n") + 5, "%d\n", value);
}

static ssize_t dsc_show(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *kattr, char *buf)
{
	int value = 0;
	E_MUX_INPUTPORT e_inputport = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_DVI);

	if (KDrv_SRCCAP_HDMIRx_a_query_dsc_port(e_inputport))
		value = 1;

	return snprintf(buf, sizeof("\n") + 5, "%d\n", value);
}

static ssize_t ps_show(struct mtk_srccap_dev *srccap_dev, int port,
						struct hdmirx_p_attribute *kattr, char *buf)
{
	int value = 0;
	E_MUX_INPUTPORT e_inputport = mtk_hdmirx_to_muxinputport(
		(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_DVI);

	if (KDrv_SRCCAP_HDMIRx_mac_get_immeswitchstatus(e_inputport))
		value = 1;

	return snprintf(buf, sizeof("\n") + 5, "%d\n", value);
}

static ssize_t ps_enable_p_show(
	struct mtk_srccap_dev *srccap_dev,
	int port,
	struct hdmirx_p_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	bool b_hwdef = false;
	bool b_swprog = false;

	b_hwdef =
		mtk_srccap_hdmirx_get_pwr_saving_onoff_hwdef(
	(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_HDMI);

	b_swprog =
		mtk_srccap_hdmirx_get_pwr_saving_onoff(
	(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_HDMI);

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"\tHW_DEF\tSW_PROG\n"
		"P%d\t%04x\t%04x\n",
		port,
		b_hwdef ? 1 : 0,
		b_swprog ? 1 : 0);

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}


static ssize_t ps_enable_p_store(
	struct mtk_srccap_dev *srccap_dev,
	int port,
	struct hdmirx_p_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	mtk_srccap_hdmirx_set_pwr_saving_onoff(
(enum v4l2_srccap_input_source)port + V4L2_SRCCAP_INPUT_SOURCE_HDMI,
			val);

	return count;
}

static ssize_t dither_show(
	struct mtk_srccap_dev *srccap_dev,
	int port,
	struct hdmirx_p_attribute *kattr,
	char *buf)
{
	int intStrSize = 0;
	E_MUX_INPUTPORT eMuxPort =
		mtk_hdmirx_to_muxinputport((enum v4l2_srccap_input_source)port +
			V4L2_SRCCAP_INPUT_SOURCE_HDMI);
	bool value = MDrv_HDMI_Get_Dither_Value(eMuxPort);

	intStrSize = snprintf(
		buf, ONE_K_UNIT,
		"\tdither_value\n"
		"P%d\t%d %s\n",
		port, value ? 1 : 0, value ? "(round)" : "(truncate)");

	if (intStrSize > 0 && intStrSize < ONE_K_UNIT-1)
		return (ssize_t)intStrSize;
	else
		return 0;
}

static ssize_t dither_store(
	struct mtk_srccap_dev *srccap_dev,
	int port,
	struct hdmirx_p_attribute *kattr,
	const char *buf,
	size_t count)
{
	unsigned long val = 0;
	int ret = 0;
	E_MUX_INPUTPORT eMuxPort;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (ret > 255)
		return -EINVAL;

	eMuxPort =
		mtk_hdmirx_to_muxinputport((enum v4l2_srccap_input_source)port +
			V4L2_SRCCAP_INPUT_SOURCE_HDMI);

	MDrv_HDMI_Dither_Set(eMuxPort, E_HDMI_NO_DITHER, (MS_U32)val);

	return count;
}

static ssize_t edid_show(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *kattr, char *buf)
{
	char buffer[HDMI_EDID_SUPPORT_SIZE] = {0};
	ssize_t len = 0;
	int i = 0;

	const MS_U8 D16 = 16;

	if (port < E_XC_PROG_DVI0_EDID || port > E_XC_PROG_DVI3_EDID)
		return -EINVAL;
	//KDrv_SRCCAP_HDMIRx_edid_readedid(port, buf, nsize);
	//return nsize;

	KDrv_SRCCAP_HDMIRx_edid_readedid(port, buffer, HDMI_EDID_SUPPORT_SIZE);
	len += sysfs_emit_at(buf, len, "============= Port %d EDID =============", port);

	for (i = 0; i < HDMI_EDID_SUPPORT_SIZE; i++) {
		if ((i % D16) == 0) {
			if ((i % HDMI_EDID_SUPPORT_128) == 0)
				len += sysfs_emit_at(buf, len, "\nBlock #%02X ", (i / HDMI_EDID_SUPPORT_128));
			len += sysfs_emit_at(buf, len, "\n%03X  ",(i / D16 * BIT4));
		}
		len += sysfs_emit_at(buf, len, "%02x ", buffer[i]);
	}
	len += sysfs_emit_at(buf, len, "\n========================================\n");

	return len;
}

static ssize_t ddc_ctrl_store(
	struct mtk_srccap_dev *srccap_dev, int port,
	struct hdmirx_p_attribute *kattr, const char *buf, size_t count)
{
	unsigned long val = 0;
	const MS_U8 D255 = 255;
	int ret = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (ret > D255)
		return -EINVAL;
	mtk_srccap_hdmirx_ddc_bus_control(val,
		(E_MUX_INPUTPORT)port + V4L2_SRCCAP_INPUT_SOURCE_DVI);
	return count;
}

#define hpd_sleep_min 900
#define hpd_sleep_max 1000
#define hpd_sleep_count 900
static ssize_t hpd_store(struct mtk_srccap_dev *srccap_dev, int port,
	struct hdmirx_p_attribute *kattr, const char *buf, size_t count)
{
	const char *str = skip_spaces(buf);
	E_MUX_INPUTPORT mux_port = INPUT_PORT_DVI0 + port;

	if (!strncasecmp(str, "low", 3))
		KDrv_SRCCAP_HDMIRx_hpd_pull(FALSE, mux_port, FALSE, 0);
	else if (!strncasecmp(str, "high", 4))
		KDrv_SRCCAP_HDMIRx_hpd_pull(TRUE, mux_port, FALSE, 0);
	else if (!strncasecmp(str, "toggle", 6)) {
		// set default  HPD pull down delay as 810ms~900ms
		int temp = hpd_sleep_count;

		KDrv_SRCCAP_HDMIRx_hpd_pull(FALSE, mux_port, FALSE, 0);
		while (temp > 0) {
			usleep_range(hpd_sleep_min, hpd_sleep_max);
			--temp;
		}
		KDrv_SRCCAP_HDMIRx_hpd_pull(TRUE, mux_port, FALSE, 0);
	}
	return count;
}

static inline ssize_t dmesg_show_video(u8 port, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;

	offset += sysfs_emit_at(buf, offset, "=======HDMI P[%d] video status=======\r\n", port);
	offset += sysfs_emit_at(buf, offset, "please check with HDMI_INFO cmd\r\n");
	// fill the debug status for your need

	return offset - _offset;
}

static inline ssize_t dmesg_show_audio(u8 port, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;
	u32 n = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_AUD_N, port, NULL);
	u32 cts = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_AUD_CTS, port, NULL);
	u32 status = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_AUD_STATUS, port, NULL);
	u32 audio_fs = KDrv_SRCCAP_HDMIRx_mac_get_datainfo(E_HDMI_GET_FS, port, NULL);

	offset += sysfs_emit_at(buf, offset, "=======HDMI P[%d] audio status=======\r\n", port);
	offset += sysfs_emit_at(buf, offset, "N: %d, CTS: %d, PCM: %s\r\n",
		n, cts, (status & HDMI_AUDIO_NON_PCM_MODE) ? "no" : "yes");

	if (status & HDMI_AUDIO_FIFO_FULL)
		offset += sysfs_emit_at(buf, offset, "fifo full\r\n");
	else if (status & HDMI_AUDIO_FIFO_EMPTY)
		offset += sysfs_emit_at(buf, offset, "fifo empty\r\n");
	else if (status & HDMI_AUDIO_FIFO_OVERRUN)
		offset += sysfs_emit_at(buf, offset, "fifo overrun\r\n");
	else if (status & HDMI_AUDIO_FIFO_UNDERRUN)
		offset += sysfs_emit_at(buf, offset, "fifo underrun\r\n");
	else
		offset += sysfs_emit_at(buf, offset, "fifo OK\r\n");

	if (status & HDMI_AUDIO_ASP_LAYOUT)
		offset += sysfs_emit_at(buf, offset, "multi channels\r\n");
	else
		offset += sysfs_emit_at(buf, offset, "2 channels\r\n");

	offset += sysfs_emit_at(buf, offset, "SamplingRate:%d.%0d kHz\r\n",
		audio_fs / ONE_K_UNIT, audio_fs % ONE_K_UNIT);

	offset += sysfs_emit_at(buf, offset, "channel status:%02x %02x %02x %02x %02x %02x\r\n",
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_0, NULL),
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_1, NULL),
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_2, NULL),
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_3, NULL),
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_4, NULL),
		KDrv_SRCCAP_HDMIRx_pkt_audio_channel_status(port, _BYTE_5, NULL));

	return offset - _offset;
}

static inline ssize_t dmesg_show_misc_avi(
	struct hdmi_avi_packet_info *info, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;
	static const char *const color_fmt[] = { "rgb", "yuv422", "yuv444", "yuv420", "reserved" };
	static const char *const color_range[] = { "default", "limit", "full", "reserved" };
	static const char *const aspect_ratio[] = { "reserve0", "reserve1", "16x9 top", "14x9 top",
		"GT 16x9", "reserve5", "reserve6", "reserve7", "same", "4x3 C", "16x9 C",
		"14x9 C", "reserve12", "4x3 with 14x9 C", "16x9 with 4x3 C"};

	if ((info->enColorForamt >= 0) &&
		(info->enColorForamt < ARRAY_SIZE(color_fmt)))
		offset += sysfs_emit_at(buf, offset, "color format: %s\r\n",
			color_fmt[info->enColorForamt]);
	else
		offset += sysfs_emit_at(buf, offset,
			"error: out-of-range color format value: %d\r\n", info->enColorForamt);

	if ((info->enColorRange >= 0) &&
		(info->enColorRange < ARRAY_SIZE(color_range)))
		offset += sysfs_emit_at(buf, offset, "color range: %s\r\n",
			color_range[info->enColorRange]);
	else
		offset += sysfs_emit_at(buf, offset,
			"error: out-of-range color range value: %d\r\n", info->enColorRange);

	offset += sysfs_emit_at(buf, offset, "colorimetry: ");
	switch (info->enColormetry) {
	case M_HDMI_COLORIMETRY_NO_DATA:
		offset += sysfs_emit_at(buf, offset, "NO_DATA\r\n"); break;
	case M_HDMI_COLORIMETRY_SMPTE_170M:
		offset += sysfs_emit_at(buf, offset, "SMPTE_170M\r\n"); break;
	case M_HDMI_COLORIMETRY_ITU_R_BT_709:
		offset += sysfs_emit_at(buf, offset, "ITU_R_BT_709\r\n"); break;
	case M_HDMI_COLORIMETRY_EXTENDED:
		offset += sysfs_emit_at(buf, offset, "EXTENDED\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_XVYCC709:
		offset += sysfs_emit_at(buf, offset, "XVYCC_709\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_XVYCC601:
		offset += sysfs_emit_at(buf, offset, "XVYCC_601\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_SYCC601:
		offset += sysfs_emit_at(buf, offset, "SYCC_601\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_ADOBEYCC601:
		offset += sysfs_emit_at(buf, offset, "Adobe YCC_601\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_ADOBERGB:
		offset += sysfs_emit_at(buf, offset, "Adobe RGB\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_BT2020YcCbcCrc:
		offset += sysfs_emit_at(buf, offset, "BT.2020 YcCbCrc\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr:
		offset += sysfs_emit_at(buf, offset, "BT.2020 RGBYCbCr\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_ADDITIONAL:
		offset += sysfs_emit_at(buf, offset, "Additional\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_D65:
		offset += sysfs_emit_at(buf, offset, "Additional DCI_P3 RGB_D65\r\n"); break;
	case M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_THEATER:
		offset += sysfs_emit_at(buf, offset, "Additional DCI_P3 RGB_THEATER\r\n"); break;
	default:
		offset += sysfs_emit_at(buf, offset, "error: unknown value of colorimetry\r\n");
		break;
	}

	if (info->enPixelRepetition <= M_HDMI_PIXEL_REPETITION_RESERVED)
		offset += sysfs_emit_at(buf, offset,
			"pixel repetition: %dX\r\n", info->enPixelRepetition);
	else
		offset += sysfs_emit_at(buf, offset,
			"error: out-of-range pixel repetition value: %d\r\n",
			info->enPixelRepetition);

	if ((info->enAspectRatio >= 0) &&
		(info->enAspectRatio < ARRAY_SIZE(aspect_ratio))) {
		offset += sysfs_emit_at(buf, offset,
			"aspect ratio: %s\r\n", aspect_ratio[info->enAspectRatio]);
	} else {
		offset += sysfs_emit_at(buf, offset, "active format aspect ratio:");
		switch (info->enActiveFormatAspectRatio) {
                case M_HDMI_Pic_AR_NODATA:
                        offset += sysfs_emit_at(buf, offset, "no data\r\n"); break;
		case M_HDMI_Pic_AR_4x3:
			offset += sysfs_emit_at(buf, offset, "4x3\r\n"); break;
		case M_HDMI_Pic_AR_16x9:
			offset += sysfs_emit_at(buf, offset, "16x9\r\n"); break;
		case M_HDMI_Pic_AR_RSV:
		default:
			offset += sysfs_emit_at(buf, offset, "reserved\r\n");
			break;
		}
	}

	offset += sysfs_emit_at(buf, offset, "IT Value: %d\r\n", info->u8ITCValue);
	if (info->u8ITCValue) {
		switch (info->u8CN10Value) {
		case E_HDMI_AVI_CN10_GRAPHICS:
			offset += sysfs_emit_at(buf, offset, "IT Content Type:%d, Graphics\r\n",
				info->u8CN10Value);
			break;
		case E_HDMI_AVI_CN10_PHOTO:
			offset += sysfs_emit_at(buf, offset, "IT Content Type:%d, Photo\r\n",
				info->u8CN10Value);
			break;
		case E_HDMI_AVI_CN10_CINEMA:
			offset += sysfs_emit_at(buf, offset, "IT Content Type:%d, Cinema\r\n",
				info->u8CN10Value);
			break;
		case E_HDMI_AVI_CN10_GAME:
			offset += sysfs_emit_at(buf, offset, "IT Content Type:%d, Game\r\n",
				info->u8CN10Value);
			break;
		default:
			offset += sysfs_emit_at(buf, offset, "IT Content Type Not Defined:%d\r\n",
				info->u8CN10Value);
			break;
		}
	} else {
		offset += sysfs_emit_at(buf, offset, "IT Content Type: %d\r\n",
			info->u8CN10Value);
	}
	offset += sysfs_emit_at(buf, offset, "VIC index: %d\r\n", info->u8VICValue);

	return offset - _offset;
}

#define HF_VSIF_IEEE_CODE 0xc45dd8
#define DV_VSIF_IEEE_CODE 0x00d046
static inline ssize_t dmesg_show_misc_vsif(
	struct hdmi_vsif_packet_info *info, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;
	u32 code = 0;

	memcpy(&code, info->au8ieee, _BYTE_3);
	switch (code) {
	case HF_VSIF_IEEE_CODE: {
		// au8payload[0]: PB4; au8payload[1]: PB5; etc...
		u8 allm = (info->au8payload[_BYTE_1] >> _BYTE_1) & _BYTE_1;

		offset += sysfs_emit_at(buf, offset, "ALLM: %d\r\n", allm);
	} break;
	case DV_VSIF_IEEE_CODE: {
		// au8payload[0]: PB4; au8payload[1]: PB5; etc...
		u8 dv_std = (info->au8payload[_BYTE_0] >> _BYTE_1) & _BYTE_15;
		u8 dv_llm = (info->au8payload[_BYTE_0] >> _BYTE_0) & _BYTE_1;

		offset += sysfs_emit_at(buf, offset,
			"dolby vision: std %d, low latency %d\r\n", dv_std, dv_llm);
	} break;
	}

	return offset - _offset;
}

static inline ssize_t dmesg_show_vrr_emp(u8 *data, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;

	offset += sysfs_emit_at(buf, offset, "EMP Class 0 for Gaming-VRR and FVA\r\n");
	offset += sysfs_emit_at(buf, offset, "VRR_EN: %d\r\n",
		(data[_BYTE_10]) & _BYTE_1); // MD0[0:0]
	offset += sysfs_emit_at(buf, offset, "FVA_Factor: %d\r\n",
		(data[_BYTE_10] >> _BYTE_4) & _BYTE_15); // MD0[7:4]

	offset += sysfs_emit_at(buf, offset, "EMP Class 1 for QMS-VRR\r\n");
	offset += sysfs_emit_at(buf, offset, "M_CONST: %d\r\n",
		(data[_BYTE_10] >> _BYTE_1) & _BYTE_1); // MD0[1:1]
	offset += sysfs_emit_at(buf, offset, "QMS_EN: %d\r\n",
		(data[_BYTE_10] >> _BYTE_2) & _BYTE_1); // MD0[2:2]
	offset += sysfs_emit_at(buf, offset, "Next_TFR: %d\r\n",
		(data[_BYTE_12] >> _BYTE_4) & _BYTE_15); // MD2[7:3]

	offset += sysfs_emit_at(buf, offset, "base Vfront: %d\r\n", data[_BYTE_11]); // MD1[7:0]
	offset += sysfs_emit_at(buf, offset, "base refresh rate: %d\r\n",
		(data[_BYTE_12] & _BYTE_3) | data[_BYTE_13]); // MD2[1:0] || MD3[7:0]

	return offset - _offset;
}

#define FREESYNC_SPD_IEEE_CODE 0x00001a
static inline ssize_t dmesg_show_misc_spd(
	struct hdmi_packet_info *info, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;
	u32 code = 0;

	memcpy(&code, &info->pb[_BYTE_1], _BYTE_3);
	switch (code) {
	case FREESYNC_SPD_IEEE_CODE:
		offset += sysfs_emit_at(buf, offset, "Freesync SPD info found:\r\n");
		offset += sysfs_emit_at(buf, offset,
			"Support: %d, Enable: %d, Active: %d\r\n",
			(info->pb[_BYTE_6] >> _BYTE_0) & _BYTE_1,
			(info->pb[_BYTE_6] >> _BYTE_1) & _BYTE_1,
			(info->pb[_BYTE_6] >> _BYTE_2) & _BYTE_1);
		offset += sysfs_emit_at(buf, offset, "Min RefreshRate: %d\r\n", info->pb[_BYTE_7]);
		offset += sysfs_emit_at(buf, offset, "Max RefreshRate: %d\r\n", info->pb[_BYTE_8]);

		if (info->hb[_BYTE_1] == _BYTE_2) {
			offset += sysfs_emit_at(buf, offset, "Freesync SPD version 2:\r\n");

			offset += sysfs_emit_at(buf, offset, "Native Color Space Active: %d\r\n",
				(info->pb[_BYTE_6] >> _BYTE_3) & _BYTE_1);
			offset += sysfs_emit_at(buf, offset, "Brightness Control Active: %d\r\n",
				(info->pb[_BYTE_6] >> _BYTE_4) & _BYTE_1);
			offset += sysfs_emit_at(buf, offset,
				"Seamless Local Dimming Disable Control: %d\r\n",
				(info->pb[_BYTE_6] >> _BYTE_5) & _BYTE_1);
			// marked bit that is not indicated in Freesync WhitePaper 4.0 documents
			// offset += sysfs_emit_at(buf, offset, "sRGB_EOTF_Active: %d, "
				// (info->pb[_BYTE_9] >> _BYTE_0) & _BYTE_1);
			// offset += sysfs_emit_at(buf, offset, "BT709_EOTF_Active: %d, ",
				// (info->pb[_BYTE_9] >> _BYTE_1) & _BYTE_1);
			offset += sysfs_emit_at(buf, offset, "Gamma22_EOTF_Active: %d, ",
				(info->pb[_BYTE_9] >> _BYTE_2) & _BYTE_1);
			// offset += sysfs_emit_at(buf, offset, "Gamma26_EOTF_Active: %d, ",
				// (info->pb[_BYTE_9] >> _BYTE_3) & _BYTE_1);
			offset += sysfs_emit_at(buf, offset, "PQ_EOTF_Active: %d, ",
				(info->pb[_BYTE_9] >> _BYTE_5) & _BYTE_1);
			offset += sysfs_emit_at(buf, offset, "BCtl: %d, ", info->pb[_BYTE_10]);
		}

		break;
	default:
		return 0;
	}

	return offset - _offset;
}

static inline ssize_t dmesg_show_misc_hdr(
	struct hdmi_hdr_packet_info *info, char *buf, ssize_t offset)
{
	ssize_t _offset = offset;

	offset += sysfs_emit_at(buf, offset, "HDR EOTF: %d ", info->u8EOTF);
	offset += sysfs_emit_at(buf, offset, "(0/1/2/3) for (SDR/HDR/PQ/HLG)\r\n");
	offset += sysfs_emit_at(buf, offset,
		"HDR static metadata id: %d\r\n", info->u8Static_Metadata_ID);

	return offset - _offset;
}

static inline ssize_t dmesg_show_misc(u8 port, char *buf, ssize_t offset)
{
	struct hdmi_avi_packet_info avi;
	struct hdmi_vsif_packet_info vsif[META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE];
	struct hdmi_emp_packet_info packet;
	struct hdmi_hdr_packet_info hdrif;
	struct hdmi_packet_info spd;
	// maybe need change to use the multi packet flow for spd & vsif??
	ssize_t _offset = offset;
	int ret;

	offset += sysfs_emit_at(buf, offset, "=======HDMI P[%d] misc status========\r\n", port);

	ret = mtk_srccap_hdmirx_pkt_get_AVI(
		port + V4L2_SRCCAP_INPUT_SOURCE_HDMI, &avi, sizeof(avi));
	if (ret == 0)
		offset += sysfs_emit_at(buf, offset, "AVI info not received\r\n");
	else
		offset += dmesg_show_misc_avi(&avi, buf, offset);

	ret = mtk_srccap_hdmirx_pkt_get_VSIF(
		port + V4L2_SRCCAP_INPUT_SOURCE_HDMI, vsif, sizeof(struct hdmi_vsif_packet_info)
		* META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);
	if (ret == 0)
		offset += sysfs_emit_at(buf, offset, "VSIF info not received\r\n");
	else {
		int i = 0;

		for (i = 0; i < ret; i++)
			offset += dmesg_show_misc_vsif(&vsif[i], buf, offset);
	}

	ret = mtk_srccap_hdmirx_pkt_get_VRR_EMP(
		port + V4L2_SRCCAP_INPUT_SOURCE_HDMI, &packet, sizeof(packet));
	if (ret == 0)
		offset += sysfs_emit_at(buf, offset, "VRR EMP data not received\r\n");
	else
		offset += dmesg_show_vrr_emp((u8 *)&packet, buf, offset);

	ret = mtk_srccap_hdmirx_pkt_get_gnl(
		port + V4L2_SRCCAP_INPUT_SOURCE_HDMI, V4L2_EXT_HDMI_PKT_SPD, &spd, sizeof(spd));
	if (ret == 0)
		offset += sysfs_emit_at(buf, offset, "SPD info data not received\r\n");
	else
		offset += dmesg_show_misc_spd(&spd, buf, offset);

	ret = mtk_srccap_hdmirx_pkt_get_HDRIF(
		port + V4L2_SRCCAP_INPUT_SOURCE_HDMI, &hdrif, sizeof(hdrif));
	if (ret == 0)
		offset += sysfs_emit_at(buf, offset, "HDR Info data not received\r\n");
	else
		offset += dmesg_show_misc_hdr(&hdrif, buf, offset);

	return offset - _offset;
}

static ssize_t dmesg_show(struct mtk_srccap_dev *srccap_dev, int port,
			struct hdmirx_p_attribute *kattr, char *buf)
{
	ssize_t offset = 0;

	if (port < E_XC_PROG_DVI0_EDID || port > E_XC_PROG_DVI3_EDID)
		return -EINVAL;

	offset += dmesg_show_video(port, buf, offset);
	offset += dmesg_show_audio(port, buf, offset);
	offset += dmesg_show_misc(port, buf, offset);
	offset += sysfs_emit_at(buf, offset, "====================================\r\n");

	return offset;
}

static HDMIRX_P_ATTR_RO(dmesg);
static HDMIRX_P_ATTR_WO(hpd);
static HDMIRX_P_ATTR_RO(edid);
static HDMIRX_P_ATTR_RO(pkt_all);
static HDMIRX_P_ATTR_RW(pkt);
static HDMIRX_P_ATTR_RO(cable);
static HDMIRX_P_ATTR_RO(frl);
static HDMIRX_P_ATTR_RO(dsc);
static HDMIRX_P_ATTR_RO(ps);
static HDMIRX_P_ATTR_RW(ps_enable_p);
static HDMIRX_P_ATTR_RW(dither);
static HDMIRX_P_ATTR_WO(ddc_ctrl);

static struct attribute *mtk_src_cap_hdmirx_p_attrs[] = {
	&hdmirx_p_attr_dmesg.attr,
	&hdmirx_p_attr_hpd.attr,
	&hdmirx_p_attr_edid.attr,
	&hdmirx_p_attr_pkt_all.attr,
	&hdmirx_p_attr_pkt.attr,
	&hdmirx_p_attr_cable.attr,
	&hdmirx_p_attr_frl.attr,
	&hdmirx_p_attr_dsc.attr,
	&hdmirx_p_attr_ps.attr,
	&hdmirx_p_attr_ps_enable_p.attr,
	&hdmirx_p_attr_dither.attr,
	&hdmirx_p_attr_ddc_ctrl.attr,
	NULL,
};

static const struct attribute_group mtk_src_cap_hdmirx_p_attr_group = {
	.attrs = mtk_src_cap_hdmirx_p_attrs,
};

static struct kobj_type mtk_srccap_hdmirx_ktype = {
	.sysfs_ops = &hdmirx_sysfs_ops,
};

static struct kobj_type mtk_srccap_hdmirx_p_ktype = {
	.sysfs_ops = &hdmirx_sysfs_p_ops,
};

static int _createHdmiRxPortSysfs(struct mtk_srccap_dev *srccap_dev, int port)
{
	int ret = 0, n = 0, tmp = 0;
	char *s_port;

	n = snprintf(NULL, 0, "%d", port);

	if (n < 0) {
		HDMIRX_MSG_ERROR("Fail to do snprintf, val = %d\n", n);
		return n;
	}
	s_port = kmalloc(n + 1, GFP_KERNEL);
	tmp = snprintf(s_port, n + 1, "%d", port);

	if (tmp < 0) {
		HDMIRX_MSG_ERROR("Fail to do snprintf, val = %d\n", tmp);
		return tmp;
	}

	ret =
		kobject_init_and_add(&srccap_dev->hdmirx_info.hdmirx_p_kobj[port],
				&mtk_srccap_hdmirx_p_ktype,
				&srccap_dev->hdmirx_info.hdmirx_kobj, "%s", s_port);

	kfree(s_port);

	if (ret) {
		HDMIRX_MSG_ERROR("Fail to create hdmirx p%d sysfs files: %d\n", port,
						 ret);
		return ret;
	}

	kobject_uevent(&srccap_dev->hdmirx_info.hdmirx_p_kobj[port], KOBJ_ADD);
	ret = sysfs_create_group(&srccap_dev->hdmirx_info.hdmirx_p_kobj[port],
				 &mtk_src_cap_hdmirx_p_attr_group);
	if (ret) {
		HDMIRX_MSG_ERROR("Fail to create hdmirx p%d sysfs files: %d\n", port,
						 ret);
		return ret;
	}

	return ret;
}

static void _delHdmiRxPortSysfs(struct mtk_srccap_dev *srccap_dev, int port)
{
	sysfs_remove_group(&srccap_dev->hdmirx_info.hdmirx_p_kobj[port],
			   &mtk_src_cap_hdmirx_p_attr_group);
	kobject_del(&srccap_dev->hdmirx_info.hdmirx_p_kobj[port]);
}

int mtk_srccap_hdmirx_sysfs_init(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0, n = 0, tmp = 0;
	char *s_data;

	n = snprintf(NULL, 0, "%s", "hdmirx");

	if (n < 0) {
		HDMIRX_MSG_ERROR("Fail to do snprintf, val = %d\n", n);
		return n;
	}

	s_data = kmalloc(n + 1, GFP_KERNEL);
	tmp = snprintf(s_data, n + 1, "%s", "hdmirx");

	if (tmp < 0) {
		HDMIRX_MSG_ERROR("Fail to do snprintf, val = %d\n", tmp);
		return tmp;
	}

	ret = kobject_init_and_add(&srccap_dev->hdmirx_info.hdmirx_kobj,
					&mtk_srccap_hdmirx_ktype,
					srccap_dev->mtkdbg_kobj, "%s", s_data);

	kfree(s_data);

	if (ret) {
		HDMIRX_MSG_ERROR("Fail to create hdmirx sysfs files: %d\n", ret);
		return ret;
	}

	kobject_uevent(&srccap_dev->hdmirx_info.hdmirx_kobj, KOBJ_ADD);
	ret = sysfs_create_group(&srccap_dev->hdmirx_info.hdmirx_kobj,
				 &mtk_src_cap_hdmirx_attr_group);
	if (ret) {
		HDMIRX_MSG_ERROR("Fail to create hdmirx sysfs files: %d\n", ret);
		return ret;
	}

	ret = _createHdmiRxPortSysfs(srccap_dev, 0);
	if (ret) {
		HDMIRX_MSG_ERROR("Fail to create hdmirx P0 sysfs files: %d\n", ret);
		return ret;
	}

	if (srccap_dev->hdmirx_info.port_count > 1) {
		ret = _createHdmiRxPortSysfs(srccap_dev, 1);
		if (ret) {
			HDMIRX_MSG_ERROR("Fail to create hdmirx P1 sysfs files: %d\n", ret);
			return ret;
		}
	}

	if (srccap_dev->hdmirx_info.port_count > 2) {
		ret = _createHdmiRxPortSysfs(srccap_dev, 2);
		if (ret) {
			HDMIRX_MSG_ERROR("Fail to create hdmirx P2 sysfs files: %d\n", ret);
			return ret;
		}
	}

	if (srccap_dev->hdmirx_info.port_count > 3) {
		ret = _createHdmiRxPortSysfs(srccap_dev, 3);
		if (ret) {
			HDMIRX_MSG_ERROR("Fail to create hdmirx P3 sysfs files: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

int mtk_srccap_hdmirx_sysfs_deinit(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev->hdmirx_info.port_count > 3)
		_delHdmiRxPortSysfs(srccap_dev, 3);

	if (srccap_dev->hdmirx_info.port_count > 2)
		_delHdmiRxPortSysfs(srccap_dev, 2);

	if (srccap_dev->hdmirx_info.port_count > 1)
		_delHdmiRxPortSysfs(srccap_dev, 1);

	_delHdmiRxPortSysfs(srccap_dev, 0);

	sysfs_remove_group(&srccap_dev->hdmirx_info.hdmirx_kobj,
			   &mtk_src_cap_hdmirx_attr_group);
	kobject_del(&srccap_dev->hdmirx_info.hdmirx_kobj);

	return ret;
}
