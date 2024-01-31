// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_PQ_HWBYPASS_C_
#define _MTK_PQ_HWBYPASS_C_

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <uapi/linux/mtk-v4l2-pq.h>
#include "mtk_pq_hwbypass.h"
#include "mtk_pq.h"
#include "m_pqu_pq.h"
//------------------------------------------------------------------------------
// Forward declaration
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Local Defines
//------------------------------------------------------------------------------
#define STRING_MAX	(512)
#define NUM_2	(2)
#define NUM_3	(3)
#define NUM_8	(8)
#define NUM_16	(16)
#define NUM_24	(24)

#define SHOW_0	(0)
#define SHOW_1	(1)
#define SHOW_2	(2)
#define SHOW_3	(3)
#define SHOW_4	(4)
//------------------------------------------------------------------------------
//  Local Type and Structurs
//------------------------------------------------------------------------------
struct pq_hwbypass {
	enum pqu_ip_bypass pau_ip;
	bool bSub;
	char pqu_ip_name[STRING_MAX];
	enum pqu_ip_bypass pau_group;
};
//------------------------------------------------------------------------------
//  Global Variables
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Local Variables
//------------------------------------------------------------------------------
const struct pq_hwbypass hw_pq_bypass[] = {
	// GROUP
	{PQU_IP_BYPASS_HSY,	0,	"HSY",	PQU_IP_BYPASS_HSY},
	{PQU_IP_BYPASS_VIP1,	0,	"VIP1",	PQU_IP_BYPASS_VIP1},
	{PQU_IP_BYPASS_VIP,	0,	"VIP",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_AISR_PQ,	0,	"AISR_PQ",	PQU_IP_BYPASS_AISR_PQ},
	{PQU_IP_BYPASS_HVSP_PRE,	0,	"HVSP_PRE",	PQU_IP_BYPASS_HVSP_PRE},
	{PQU_IP_BYPASS_HVSP_POST,	0,	"HVSP_POST",	PQU_IP_BYPASS_HVSP_POST},
	{PQU_IP_BYPASS_SPF,	0,	"SPF",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_HDR10,	0,	"HDR10",	PQU_IP_BYPASS_HDR10},
	{PQU_IP_BYPASS_VOP,	0,	"VOP",	PQU_IP_BYPASS_VOP},
	{PQU_IP_BYPASS_DV,	0,	"DV",	PQU_IP_BYPASS_DV},
	{PQU_IP_BYPASS_MCNR,	0,	"MCNR",	PQU_IP_BYPASS_MCNR},
	{PQU_IP_BYPASS_SR,	0,	"SR",	PQU_IP_BYPASS_SR},
	{PQU_IP_BYPASS_DLC,	0,	"DLC",	PQU_IP_BYPASS_DLC},
	{PQU_IP_BYPASS_UCD,	0,	"UCD",	PQU_IP_BYPASS_UCD},
	{PQU_IP_BYPASS_OTHERS,	0,	"OTHERS",	PQU_IP_BYPASS_OTHERS},
	// SUB
	{PQU_IP_BYPASS_SNR,	1,	"SNR",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_LOCALPQ_SNR,	1,	"LOCALPQ_SNR",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_MNC,	1,	"MNC",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_DMS,	1,	"DMS",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_DBK,	1,	"DBK",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_PRESNR,	1,	"PRESNR",	PQU_IP_BYPASS_OTHERS},
	{PQU_IP_BYPASS_DNR,	1,	"DNR",	PQU_IP_BYPASS_OTHERS},
	{PQU_IP_BYPASS_MLP,	1,	"MLP",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_FILM,	1,	"FILM",	PQU_IP_BYPASS_OTHERS},
	{PQU_IP_BYPASS_2DDS,	1,	"2DDS",	PQU_IP_BYPASS_OTHERS},
	{PQU_IP_BYPASS_PEAKING_PRE,	1,	"PEAKING_PRE",	PQU_IP_BYPASS_VIP1},
	{PQU_IP_BYPASS_LTI_PRE,	1,	"LTI_PRE",	PQU_IP_BYPASS_VIP1},
	{PQU_IP_BYPASS_PEAKING_POST,	1,	"PEAKING_POST",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_LTI_POST,	1,	"LTI_POST",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_LDB_PRE,	1,	"LDB_PRE",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_LDB_POST,	1,	"LDB_POST",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_LCE,	1,	"LCE",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_UVC,	1,	"UVC",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_YCBCR_FCC_ACK_FWC_CGAIN,	1,	"YCBCR_FCC_ACK_FWC_CGAIN",
	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_VIP_CTI,	1,	"VIP_CTI",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_RGB_OFFSET,	1,	"RGB_OFFSET",	PQU_IP_BYPASS_VOP},
	{PQU_IP_BYPASS_LINEAR_RGB,	1,	"LINEAR_RGB",	PQU_IP_BYPASS_VOP},
	{PQU_IP_BYPASS_3DLUT,	1,	"3DLUT",	PQU_IP_BYPASS_VOP},
	{PQU_IP_BYPASS_DLC_1,	1,	"DLC",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_UCD_1,	1,	"UCD",	PQU_IP_BYPASS_VIP},
	{PQU_IP_BYPASS_SPF_CTI,	1,	"SPF_CTI",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_SPIKENR,	1,	"SpikeNR",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_NOISEMASKING,	1,	"NoiseMasking",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_DIPF,	1,	"DIPF",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_ENR,	1,	"ENR",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_RLP,	1,	"RLP",	PQU_IP_BYPASS_SPF},
	{PQU_IP_BYPASS_HCTI,	1,	"HCTI",	PQU_IP_BYPASS_OTHERS},
	{PQU_IP_BYPASS_ABF,	1,	"ABF",	PQU_IP_BYPASS_SPF},
};
//------------------------------------------------------------------------------
//  Local Functions
//------------------------------------------------------------------------------
void mtk_pq_hwbypass_show_group_status(struct device *dev, __u32 Idx)
{
	__u8 i = 0, u8IPCount = 0;
	int count = 0, ScnSize = 0;
	char string[STRING_MAX] = {0};
	__u8 u8Bypass_IP = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return;
	}

	u8IPCount = sizeof(hw_pq_bypass)/sizeof(struct pq_hwbypass);
	if (Idx >= u8IPCount) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Idx oversize!\n");
		return;
	}

	u8Bypass_IP = hw_pq_bypass[Idx].pau_ip;

	//		|00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
	//[]XX	|O |O |O |O |O |O |O |O |O |O |O |O |O |O |O |O |
	ScnSize = scnprintf(string, STRING_MAX, "[%3d]%s",
			    hw_pq_bypass[Idx].pau_ip,
			    hw_pq_bypass[Idx].pqu_ip_name);
	count += ScnSize;
	if (ScnSize > NUM_8 && ScnSize <= NUM_16) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t\t");
		count += ScnSize;
	} else if (ScnSize > NUM_16 && ScnSize <= NUM_24) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t");
		count += ScnSize;
	} else if (ScnSize > NUM_24) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t");
		count += ScnSize;
	} else {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t\t\t");
		count += ScnSize;
	}

	for (i = 0; i < pqdev->xc_win_num; i++) {
		if (pqdev->pq_devs[i]->pqu_meta.pq_byp[u8Bypass_IP]) {
			ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|O ");
		} else {
			ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|X ");
		}
		count += ScnSize;
	}
	pr_crit("%s", string);
}

void mtk_pq_hwbypass_show_status(struct device *dev, __u32 Idx)
{
	__u8 i = 0, u8IPCount = 0;
	int count = 0, ScnSize = 0;
	char string[STRING_MAX] = {0};
	__u8 u8Bypass_IP = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}
	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return;
	}

	u8IPCount = sizeof(hw_pq_bypass)/sizeof(struct pq_hwbypass);
	if (Idx >= u8IPCount) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Idx oversize!\n");
		return;
	}

	u8Bypass_IP = hw_pq_bypass[Idx].pau_ip;

	//		|00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
	//[]XX	|O |O |O |O |O |O |O |O |O |O |O |O |O |O |O |O |
	ScnSize = scnprintf(string, STRING_MAX,
		"[%3d]%s",
		hw_pq_bypass[Idx].pau_ip,
		hw_pq_bypass[Idx].pqu_ip_name);
	count += ScnSize;
	if (ScnSize > NUM_8 && ScnSize <= NUM_16) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t\t");
		count += ScnSize;
	} else if (ScnSize > NUM_16 && ScnSize <= NUM_24) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t");
		count += ScnSize;
	} else if (ScnSize > NUM_24) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t");
		count += ScnSize;
	} else {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
			"\t\t\t\t");
		count += ScnSize;
	}

	for (i = 0; i < pqdev->xc_win_num; i++) {
		if (pqdev->pq_devs[i]->pqu_meta.pq_byp[u8Bypass_IP]) {
			ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|O ");
		} else {
			ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|X ");
		}
		count += ScnSize;
	}
	pr_crit("%s", string);
}

void mtk_pq_hwbypass_show_group_list(void)
{
	__u8 i = 0;
	int count = 0, ScnSize = 0;
	char string[STRING_MAX] = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	ScnSize = scnprintf(string, STRING_MAX,
		"======================================\n");
	count += ScnSize;

	ScnSize = scnprintf(string+count, STRING_MAX-count,
		"Group list\n");
	count += ScnSize;

	ScnSize = scnprintf(string+count, STRING_MAX-count,
		"======================================\n");
	count += ScnSize;
	ScnSize = scnprintf(string+count, STRING_MAX-count,
		"\t\t\t\t\t");
	count += ScnSize;
	for (i = 0; i < pqdev->xc_win_num; i++) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|%02d", i);
		count += ScnSize;
	}
	ScnSize = scnprintf(string+count, STRING_MAX-count,
		"\n");
	count += ScnSize;
	pr_crit("%s", string);
}

void mtk_pq_hwbypass_show_list(struct device *dev, __u32 Idx)
{
	int count = 0, ScnSize = 0;
	__u8 i = 0, u8Count = 0, u8IPCount = 0;
	char string[STRING_MAX] = {0};
	struct mtk_pq_platform_device *pqdev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}
	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return;
	}
	u8IPCount = sizeof(hw_pq_bypass)/sizeof(struct pq_hwbypass);
	if (Idx >= u8IPCount) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Idx oversize!\n");
		return;
	}
	ScnSize = scnprintf(string, STRING_MAX,
		"======================================\n");
	count += ScnSize;

	u8Count = sizeof(hw_pq_bypass)/sizeof(struct pq_hwbypass);

	for (i = 0; i < u8Count; i++) {
		if (hw_pq_bypass[i].pau_ip == hw_pq_bypass[Idx].pau_group) {
			ScnSize = scnprintf(string + count, STRING_MAX - count,
				"Group: [%3d]%s\n",
				hw_pq_bypass[i].pau_ip,
				hw_pq_bypass[i].pqu_ip_name);
			count += ScnSize;
			break;
		}
	}

	ScnSize = scnprintf(string+count, STRING_MAX - count,
		"======================================\n");
	count += ScnSize;
	ScnSize = scnprintf(string+count, STRING_MAX - count,
		"\t\t\t\t\t");
	count += ScnSize;
	for (i = 0; i < pqdev->xc_win_num; i++) {
		ScnSize = scnprintf(string+count, STRING_MAX-count,
				"|%02d", i);
		count += ScnSize;
	}
	ScnSize = scnprintf(string+count, STRING_MAX-count,
		"\n");
	count += ScnSize;
	pr_crit("%s", string);
}

void _mtk_pq_hwbypass_set_bypass(struct device *dev, __u32 win_id, __u32 bypass, __u32 ip)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	__u8 i = 0, j = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}
	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return;
	}
	if (ip == BYPASS_CMD_ALL_IP) {
		for (i = 0; i < PQU_IP_BYPASS_NUM; i++) {
			if (win_id == BYPASS_CMD_ALL_WIN) {
				for (j = 0; j < pqdev->xc_win_num; j++)
					pqdev->pq_devs[j]->pqu_meta.pq_byp[i] = (bool)bypass;
			} else {
				pqdev->pq_devs[win_id]->pqu_meta.pq_byp[i] = (bool)bypass;
			}
		}
		return;
	} else if (ip >= PQU_IP_BYPASS_NUM) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Bypass IP::%u Not Support!\n", ip);
		return;
	}
	if (win_id == BYPASS_CMD_ALL_WIN)
		for (i = 0; i < pqdev->xc_win_num; i++)
			pqdev->pq_devs[i]->pqu_meta.pq_byp[ip] = (bool)bypass;
	else
		pqdev->pq_devs[win_id]->pqu_meta.pq_byp[ip] = (bool)bypass;
}

static int _mtk_pq_hwbypass_check_fail(struct device *dev)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	__u8 i = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}
	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}
	for (i = 0; i < pqdev->xc_win_num; i++) {
		if (pqdev->pq_devs[i] == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev WIN = %d is NULL!\n", i);
			return -EINVAL;
		}
	}
	return 0;
}
//------------------------------------------------------------------------------
//  Global Functions
//------------------------------------------------------------------------------
void mtk_pq_hwbypass_cmd(struct device *dev, __u32 win_id, __u32 bypass, __u32 ip, __u32 print)
{
	int ret = 0;
	__u8 u8Idx = 0;
	__u8 i = 0;
	bool flag = 0;
	u8 u8Count = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s %d\n", __func__, __LINE__);

	ret = _mtk_pq_hwbypass_check_fail(dev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "function check fail!\n");
		return;
	}
	if (ip == BYPASS_CMD_ALL_IP) {
		if (bypass > 0)
			pr_crit("Not support set all IP bypass, WIN:%d!!\n", win_id);
		else {
			_mtk_pq_hwbypass_set_bypass(dev, win_id, bypass, ip);
			pr_crit("reset all IP bypass, WIN:%d\n", win_id);
		}
		return;
	} else if (ip >= PQU_IP_BYPASS_NUM) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Bypass IP::%u Not Support!\n", ip);
		return;
	}
	u8Count = sizeof(hw_pq_bypass)/sizeof(struct pq_hwbypass);

	for (i = 0; i < u8Count; i++) {
		if (ip == hw_pq_bypass[i].pau_ip) {
			u8Idx = i;
			flag = 1;
			break;
		}
	}
	if (flag) {
		switch (print) {
		case SHOW_0:
			_mtk_pq_hwbypass_set_bypass(dev, win_id, bypass, ip);
		break;
		case SHOW_1:
			mtk_pq_hwbypass_show_group_list();
			for (i = 0; i < u8Count; i++) {
				if (hw_pq_bypass[i].bSub == 0)
					mtk_pq_hwbypass_show_group_status(dev, i);
			}
			pr_crit("\n");
		break;
		case SHOW_2:
			mtk_pq_hwbypass_show_list(dev, u8Idx);
			for (i = 0; i < u8Count; i++) {
				if ((hw_pq_bypass[i].bSub == 1) &&
			(hw_pq_bypass[u8Idx].pau_group == hw_pq_bypass[i].pau_group)) {
					mtk_pq_hwbypass_show_status(dev, i);
				}
			}
			pr_crit("\n");
		break;
		case SHOW_3:
			_mtk_pq_hwbypass_set_bypass(dev, win_id, bypass, ip);
			mtk_pq_hwbypass_show_group_list();
			for (i = 0; i < u8Count; i++) {
				if (hw_pq_bypass[i].bSub == 0)
					mtk_pq_hwbypass_show_group_status(dev, i);
			}
			pr_crit("\n");
		break;
		case SHOW_4:
			_mtk_pq_hwbypass_set_bypass(dev, win_id, bypass, ip);
			mtk_pq_hwbypass_show_list(dev, u8Idx);
			for (i = 0; i < u8Count; i++) {
				if ((hw_pq_bypass[i].bSub == 1) &&
			(hw_pq_bypass[u8Idx].pau_group == hw_pq_bypass[i].pau_group)) {
					mtk_pq_hwbypass_show_status(dev, i);
				}
			}
			pr_crit("\n");
		break;
		default:
			pr_crit("not support\n");
		break;
		}
	} else {
		pr_crit("not support\n");
	}
}

#endif		// _MTK_PQ_HWBYPASS_C_
