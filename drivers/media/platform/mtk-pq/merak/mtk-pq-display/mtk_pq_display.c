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
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <uapi/linux/mtk-v4l2-pq.h>

#include "mtk_pq.h"
#include "mtk_pq_buffer.h"
#include "mtk_pq_svp.h"
#include "mtk_pq_display_b2r.h"
#include "mtk_pq_display_mdw.h"
#include "mtk_pq_display_idr.h"

#include "pqu_msg.h"
#include "m_pqu_pq.h"
#include "apiXC.h"

#include "hwreg_udma.h"

//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define PQ_DISP_C_STRING_END ('\0')

//dbg related Defines
#define DBG_STORE_BUF_SIZE (0x1000)
#define PQU_DBG_LOG_LEVEL_NUM (24)

struct pq_display_aisr_dbg _gAisrdbg[PQ_WIN_MAX_NUM];
static uint64_t _gu64PquLogLevel;
static bool _force_output;
const char *pcPQLogLabel[PQU_DBG_LOG_LEVEL_NUM] = {
	"PQU_DBG_LOG_LEVEL_SRS   (BIT(0))",
	"PQU_DBG_LOG_LEVEL_GCE   (BIT(1))",
	"PQU_DBG_LOG_LEVEL_UCD   (BIT(2))",
	"PQU_DBG_LOG_LEVEL_PQMAP (BIT(3))",
	"PQU_DBG_LOG_LEVEL_VIP   (BIT(4))",
	"PQU_DBG_LOG_LEVEL_ABF   (BIT(5))",
	"PQU_DBG_LOG_LEVEL_AVC   (BIT(6))",
	"PQU_DBG_LOG_LEVEL_FILM  (BIT(7))",
	"PQU_DBG_LOG_LEVEL_FOD   (BIT(8))",
	"PQU_DBG_LOG_LEVEL_HISTO (BIT(9))",
	"PQU_DBG_LOG_LEVEL_HSY   (BIT(10))",
	"PQU_DBG_LOG_LEVEL_LBX   (BIT(11))",
	"PQU_DBG_LOG_LEVEL_LCE   (BIT(12))",
	"PQU_DBG_LOG_LEVEL_HVSP  (BIT(13))",
	"PQU_DBG_LOG_LEVEL_LPQ   (BIT(14))",
	"PQU_DBG_LOG_LEVEL_MNC   (BIT(15))",
	"PQU_DBG_LOG_LEVEL_NR    (BIT(16))",
	"PQU_DBG_LOG_LEVEL_SR    (BIT(17))",
	"PQU_DBG_LOG_LEVEL_COM   (BIT(18))",
	"PQU_DBG_LOG_LEVEL_FHDQM (BIT(19))",
	"PQU_DBG_LOG_LEVEL_LDB   (BIT(20))",
	"PQU_DBG_LOG_LEVEL_DS    (BIT(21))",
	"PQU_DBG_LOG_LEVEL_DI    (BIT(22))",
	"PQU_DBG_LOG_LEVEL_IRQ   (BIT(23))"
};

struct debug_srs_info gSrsDbg[PQ_WIN_MAX_NUM];

static int _mtk_display_cal_rwdiff(__s32 *rwdiff, struct mtk_pq_device *pq_dev)
{
	int delay = 0;
	struct pq_buffer pq_buf;
	enum pqu_buffer_type buf_idx;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
		memset(&pq_buf, 0, sizeof(struct pq_buffer));
		mtk_get_pq_buffer(pq_dev, buf_idx, &pq_buf);

		if (pq_buf.frame_num != 0)
			delay += pq_buf.frame_delay;
	}

	*rwdiff = delay;

	return 0;
}

static int _mtk_display_cal_extra_frame(
	struct mtk_pq_device *pq_dev, struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	if ((!pq_dev) || (!ctrl))
		return -EFAULT;

	ret = mtk_pq_buffer_get_extra_frame(pq_dev, &pq_dev->extra_frame);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Failed to mtk_pq_buffer_get_extra_frame\n");
		return ret;
	}

	ctrl->val = (__s32)pq_dev->extra_frame;
	pq_dev->pq_debug.extra_frame = pq_dev->extra_frame;
	return 0;
}

static int _mtk_display_set_output_buffer_refresh(struct mtk_pq_device *pq_dev
	, struct v4l2_ctrl *ctrl)
{
	int ret = 0;

	if (WARN_ON(!ctrl) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, ctrl=%p, pq_dev=%p\n", ctrl, pq_dev);
		return -EINVAL;
	}

	if (ctrl->val)
		_force_output = ctrl->val;

	return ret;
}

static int _mtk_display_set_data(struct mtk_pq_device *pq_dev, void *data)
{
	int ret = 0;

	if (WARN_ON(!data) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, data=%p, pq_dev=%p\n", data, pq_dev);
		return -EINVAL;
	}

	memcpy(&(pq_dev->s_data), data, sizeof(struct mtk_pq_display_s_data));

	if (pq_dev->s_data.ctrl_type == MTK_PQ_DISPLAY_CTRL_TYPE_VIDEO_DELAY_TIME) {
		// update stream meta for forcep case
		struct meta_pq_stream_info *stream_info = NULL;

		stream_info = &(pq_dev->stream_meta);

		if ((stream_info->pq_scene == meta_pq_scene_force_p)
			&& (stream_info->bForceP == true)) {
			stream_info->scenario_idx = pq_dev->s_data.p.stVideoDelayInfo.scenarioIdx;

			mtk_pq_buffer_get_hwmap_info(pq_dev);
		}
	}

	return ret;
}

static int _mtk_display_cal_swbit(struct mtk_pq_display_g_data *g_data,
				struct mtk_pq_device *pq_dev)
{
	struct pq_buffer pq_buf;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}
	if (g_data == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	memset(&pq_buf, 0, sizeof(struct pq_buffer));

	mtk_get_pq_buffer(pq_dev, PQU_BUF_SCMI, &pq_buf);

	g_data->data.swbit.scmi_format = pq_buf.frame_format;
	g_data->data.swbit.scmi_frame = pq_buf.frame_num;
	g_data->data.swbit.scmi_delay = pq_buf.frame_delay;

	mtk_get_pq_buffer(pq_dev, PQU_BUF_UCM, &pq_buf);

	g_data->data.swbit.ucm_format = pq_buf.frame_format;
	g_data->data.swbit.ucm_frame = pq_buf.frame_num;
	g_data->data.swbit.ucm_delay = pq_buf.frame_delay;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY,
		"scmi_format:%d,scmi_frame:%d,scmi_delay:%d,ucm_format:%d,ucm_frame:%d,ucm_delay:%d\n",
		g_data->data.swbit.scmi_format,
		g_data->data.swbit.scmi_frame,
		g_data->data.swbit.scmi_delay,
		g_data->data.swbit.ucm_format,
		g_data->data.swbit.ucm_frame,
		g_data->data.swbit.ucm_delay);

	return 0;
}

static int _mtk_display_get_data(struct mtk_pq_device *pq_dev, void *data)
{
	struct mtk_pq_display_g_data *g_data = NULL;
	int ret = 0;

	g_data = (struct mtk_pq_display_g_data *)data;

	if (WARN_ON(!data) || WARN_ON(!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ptr is NULL, data=%p, pq_dev=%p\n", data, pq_dev);
		return -EINVAL;
	}

	if (pq_dev->s_data.ctrl_type == MTK_PQ_DISPLAY_CTRL_TYPE_VIDEO_DELAY_TIME) {
		hwmap_swbit swbit;

		memset(&swbit, 0, sizeof(hwmap_swbit));

		drv_hwreg_common_config_load_tab_by_src(
			pq_dev->s_data.p.stVideoDelayInfo.scenarioIdx,
			PQ_BIN_IP_ALL, pqdev->config_info.u8Config_Version,
			false, NULL, NULL, NULL,  &swbit, NULL, NULL);

		g_data->data.stVideoDelayInfo.delayCnt =
			swbit.scmi_delay + swbit.ucm_delay;
	} else if (pq_dev->s_data.ctrl_type == MTK_PQ_DISPLAY_CTRL_TYPE_HWMAP_SWBIT) {
		ret = _mtk_display_cal_swbit(g_data, pq_dev);
	}

	return ret;
}

static int _mtk_display_get_pq_buffer(struct mtk_pq_device *pq_dev, struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct pq_buffer pq_buf;

	if ((!pq_dev) || (!ctrl))
		return -EFAULT;

	memset(&pq_buf, 0, sizeof(struct pq_buffer));

	mtk_get_pq_buffer(pq_dev, PQU_BUF_SCMI, &pq_buf);
	ret = mtk_pq_get_cal_buffer_size(pq_dev, PQU_BUF_SCMI, &pq_buf);
	pq_buf.size = PAGE_ALIGN(pq_buf.size_ch[PQU_BUF_CH_0] + pq_buf.size_ch[PQU_BUF_CH_1]
					+ pq_buf.size_ch[PQU_BUF_CH_2]);
	ctrl->val = (__s32)pq_buf.size;

	mtk_get_pq_buffer(pq_dev, PQU_BUF_UCM, &pq_buf);
	ret = mtk_pq_get_cal_buffer_size(pq_dev, PQU_BUF_UCM, &pq_buf);
	pq_buf.size = PAGE_ALIGN(pq_buf.size_ch[PQU_BUF_CH_0] + pq_buf.size_ch[PQU_BUF_CH_1]
					+ pq_buf.size_ch[PQU_BUF_CH_2]);
	ctrl->val += (__s32)pq_buf.size;

	return ret;
}

static int _display_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid Parameters!\n");
		return -EINVAL;
	}

	pq_dev = container_of(ctrl->handler,
	    struct mtk_pq_device, display_ctrl_handler);
	pqdev = dev_get_drvdata(pq_dev->dev);

	if (pq_dev->dev_indx >= pqdev->xc_win_num) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		    "Invalid WindowID!!WindowID = %d\n", pq_dev->dev_indx);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_GET_DISPLAY_DATA:
		ret = _mtk_display_get_data(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_GET_RW_DIFF:
		ret = _mtk_display_cal_rwdiff(&(ctrl->val), pq_dev);
		break;
	case V4L2_CID_GET_EXTRA_FRAME:
		ret = _mtk_display_cal_extra_frame(pq_dev, ctrl);
		break;
	case V4L2_CID_GET_PQ_BUF_ALLOC:
		ret = _mtk_display_get_pq_buffer(pq_dev, ctrl);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _display_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid Parameters!\n");
		return -EINVAL;
	}

	pq_dev = container_of(ctrl->handler,
	    struct mtk_pq_device, display_ctrl_handler);
	pqdev = dev_get_drvdata(pq_dev->dev);

	if (pq_dev->dev_indx >= pqdev->xc_win_num) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
		    "Invalid WindowID!!WindowID = %d\n", pq_dev->dev_indx);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_SET_DISPLAY_DATA:
		ret = _mtk_display_set_data(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_PQ_SECURE_MODE:
#if IS_ENABLED(CONFIG_OPTEE)
		ret = mtk_pq_svp_set_sec_md(pq_dev, ctrl->p_new.p_u8);
#endif
		break;
	case V4L2_CID_SET_OUTPUT_BUF:
		ret = _mtk_display_set_output_buffer_refresh(pq_dev, ctrl);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static bool _mtk_display_compare_dummy_meta(struct mtk_pq_device *pq_dev,
	struct output_meta_compare_info *info)
{
	bool change = false;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return false;
	}
	if (info == NULL) {
		PQ_CHECK_NULLPTR();
		return false;
	}

	if (info->u8Interlace != pq_dev->pqu_queue_ext_info.u8Interlace) {
		change = true;
		info->u8Interlace = pq_dev->pqu_queue_ext_info.u8Interlace;
	}
	if (info->u32OriginalInputFps != pq_dev->pqu_queue_ext_info.u32OriginalInputFps) {
		change = true;
		info->u32OriginalInputFps = pq_dev->pqu_queue_ext_info.u32OriginalInputFps;
	}
	if (info->u8MuteAction != pq_dev->pqu_queue_ext_info.u8MuteAction) {
		change = true;
		info->u8MuteAction = pq_dev->pqu_queue_ext_info.u8MuteAction;
	}
	if (info->u8PqMuteAction != pq_dev->pqu_queue_ext_info.u8PqMuteAction) {
		change = true;
		info->u8PqMuteAction = pq_dev->pqu_queue_ext_info.u8PqMuteAction;
	}
	if (info->u8SignalStable != pq_dev->pqu_queue_ext_info.u8SignalStable) {
		change = true;
		info->u8SignalStable = pq_dev->pqu_queue_ext_info.u8SignalStable;
	}
	if (info->u8DotByDotType != pq_dev->pqu_queue_ext_info.u8DotByDotType) {
		change = true;
		info->u8DotByDotType = pq_dev->pqu_queue_ext_info.u8DotByDotType;
	}
	if (info->u32RefreshRate != pq_dev->pqu_queue_ext_info.u32RefreshRate) {
		change = true;
		info->u32RefreshRate = pq_dev->pqu_queue_ext_info.u32RefreshRate;
	}
	if (info->u32HdrApplyType != pq_dev->pqu_queue_ext_info.u32HdrApplyType) {
		change = true;
		info->u32HdrApplyType = pq_dev->pqu_queue_ext_info.u32HdrApplyType;
	}
	if (pq_dev->pqu_queue_ext_info.bMuteChange == true) {
		change = true;
		info->bMuteChange = pq_dev->pqu_queue_ext_info.bMuteChange;
	}
	if (info->bFdMaskBypass != pq_dev->pqu_queue_ext_info.bFdMaskBypass) {
		change = true;
		info->bFdMaskBypass = pq_dev->pqu_queue_ext_info.bFdMaskBypass;
	}
	if (info->bTriggerInfiniteGen != pq_dev->pqu_queue_ext_info.bTriggerInfiniteGen) {
		change = true;
		info->bTriggerInfiniteGen = pq_dev->pqu_queue_ext_info.bTriggerInfiniteGen;
	}
	if (info->bEnableBbd != pq_dev->pqu_queue_ext_info.bEnableBbd) {
		change = true;
		info->bEnableBbd = pq_dev->pqu_queue_ext_info.bEnableBbd;
	}
	if (info->bPerFrameMode != pq_dev->pqu_queue_ext_info.bPerFrameMode) {
		change = true;
		info->bPerFrameMode = pq_dev->pqu_queue_ext_info.bPerFrameMode;
	}

	return change;
}

static bool _mtk_pq_get_dbg_value_from_string(char *buf, char *name,
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

static bool _mtk_pq_get_dbg_value_from_string_ull(char *buf, char *name,
	unsigned int len, uint64_t *value)
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
			if (kstrtoull(tmp_value, 0, value)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"kstrtoint fail!\n");
				return find;
			}
			find = true;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
					"name = %s, value = %llu\n", name, *value);
			return find;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"name(%s) was not found!\n", name);

	return find;
}

static bool _mtk_pq_get_aisr_dbg_type(char *buf, char *name,
	unsigned int len)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return false;
	}

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
			find = true;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "type = %s\n", name);
			return find;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"name(%s) was not found!\n", name);

	return find;
}

static int _mtk_pq_get_aisr_dbg_mode(char *buf,
	unsigned int len, enum AISR_DBG_TYPE *type)
{
	bool find = false;
	char *cmd;

	if ((buf == NULL) || (type == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return false;
	}

	*type = MTK_PQ_AISR_DBG_NONE;
	cmd = buf;

	find = _mtk_pq_get_aisr_dbg_type(cmd, "aisr_en", len);
	if (find) {
		*type = MTK_PQ_AISR_DBG_ENABLE;
		return 0;
	}

	find = _mtk_pq_get_aisr_dbg_type(cmd, "active_win", len);
	if (find) {
		*type = MTK_PQ_AISR_DBG_ACTIVE_WIN;
		return 0;
	}

	return -EINVAL;
}

static int _mtk_pq_set_aisr_dbg(
	struct mtk_pq_device *pq_dev,
	struct m_pq_display_flow_ctrl *pqu_meta)
{
	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (pqu_meta == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (_gAisrdbg[pqu_meta->win_id].bdbgModeEn) {
		if (!_gAisrdbg[pqu_meta->win_id].bAISREn && pqu_meta->aisr_en) {
			memcpy(&(pqu_meta->ip_win[pqu_ip_aisr_out]),
				&(pqu_meta->ip_win[pqu_ip_aisr_in]),
				sizeof(struct window_info));
			memcpy(&(pqu_meta->ip_win[pqu_ip_hvsp_in]),
				&(pqu_meta->ip_win[pqu_ip_aisr_out]),
				sizeof(struct window_info));
			pqu_meta->aisr_en = 0;
		} else if (_gAisrdbg[pqu_meta->win_id].bAISREn && pqu_meta->aisr_en) {
			if (_gAisrdbg[pqu_meta->win_id].u32Scale > 0) {
				pqu_meta->ip_win[pqu_ip_aisr_out].height =
					pqu_meta->ip_win[pqu_ip_aisr_in].height *
						_gAisrdbg[pqu_meta->win_id].u32Scale;
				pqu_meta->ip_win[pqu_ip_aisr_out].width =
					pqu_meta->ip_win[pqu_ip_aisr_in].width *
						_gAisrdbg[pqu_meta->win_id].u32Scale;
				memcpy(&(pqu_meta->ip_win[pqu_ip_hvsp_in]),
					&(pqu_meta->ip_win[pqu_ip_aisr_out]),
					sizeof(struct window_info));
			}
		}

		if ((pqu_meta->ip_win[pqu_ip_display].width != 0) &&
			(pqu_meta->ip_win[pqu_ip_display].height != 0)) {
			pqu_meta->bAisrActiveWinEn =
				_gAisrdbg[pqu_meta->win_id].aisrActiveWin.bEnable;
			pqu_meta->aisr_active_win_info.x =
				(pqu_meta->ip_win[pqu_ip_aisr_out].width *
					_gAisrdbg[pqu_meta->win_id].aisrActiveWin.x) /
					pqu_meta->ip_win[pqu_ip_display].width;
			pqu_meta->aisr_active_win_info.y =
				(pqu_meta->ip_win[pqu_ip_aisr_out].height *
					_gAisrdbg[pqu_meta->win_id].aisrActiveWin.y) /
					pqu_meta->ip_win[pqu_ip_display].height;
			pqu_meta->aisr_active_win_info.width =
				(pqu_meta->ip_win[pqu_ip_aisr_out].width *
					_gAisrdbg[pqu_meta->win_id].aisrActiveWin.width) /
					pqu_meta->ip_win[pqu_ip_display].width;
			pqu_meta->aisr_active_win_info.height =
				(pqu_meta->ip_win[pqu_ip_aisr_out].height *
					_gAisrdbg[pqu_meta->win_id].aisrActiveWin.height) /
					pqu_meta->ip_win[pqu_ip_display].height;
		}
	}

	_gAisrdbg[pqu_meta->win_id].bAISREnStatus = pqu_meta->aisr_en;
	if (pqu_meta->aisr_en && (pqu_meta->ip_win[pqu_ip_aisr_in].height > 0))
		_gAisrdbg[pqu_meta->win_id].u32ScaleStatus =
			(pqu_meta->ip_win[pqu_ip_aisr_out].height
			/ pqu_meta->ip_win[pqu_ip_aisr_in].height);
	else
		_gAisrdbg[pqu_meta->win_id].u32ScaleStatus = 0;

	return 0;
}

static const struct v4l2_ctrl_ops display_ctrl_ops = {
	.g_volatile_ctrl = _display_g_ctrl,
	.s_ctrl = _display_s_ctrl,
};

static const struct v4l2_ctrl_config display_ctrl[] = {
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_SET_DISPLAY_DATA,
		.name = "set display manager data",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct mtk_pq_display_s_data)},
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_GET_DISPLAY_DATA,
		.name = "get display manager data",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(struct mtk_pq_display_g_data)},
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_GET_RW_DIFF,
		.name = "get display rw diff",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_GET_EXTRA_FRAME,
		.name = "get extra frame",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_PQ_SECURE_MODE,
		.name = "xc secure mode",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.dims = {sizeof(u8)},
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_GET_PQ_BUF_ALLOC,
		.name = "get pq buf alloc",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xffffffff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &display_ctrl_ops,
		.id = V4L2_CID_SET_OUTPUT_BUF,
		.name = "set output buf",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops display_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops display_sd_internal_ops = {
};

//-----------------------------------------------------------------------------
//  FUNCTION
//-----------------------------------------------------------------------------
int mtk_display_compare_frame_metadata(struct mtk_pq_device *pq_dev,
	struct output_meta_compare_info *info, bool *meta_change)
{
	int ret = 0;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (info == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (meta_change == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	*meta_change = false;
	if (memcmp(&(info->cap_win), &(pq_dev->pqu_meta.cap_win),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->cap_win), &(pq_dev->pqu_meta.cap_win),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->crop_win), &(pq_dev->pqu_meta.crop_win),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->crop_win), &(pq_dev->pqu_meta.crop_win),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->disp_win), &(pq_dev->pqu_meta.disp_win),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->disp_win), &(pq_dev->pqu_meta.disp_win),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->displayArea), &(pq_dev->pqu_meta.displayArea),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->displayArea), &(pq_dev->pqu_meta.displayArea),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->displayRange), &(pq_dev->pqu_meta.displayRange),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->displayRange), &(pq_dev->pqu_meta.displayRange),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->displayBase), &(pq_dev->pqu_meta.displayBase),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->displayBase), &(pq_dev->pqu_meta.displayBase),
			sizeof(struct window_info));
	}
	if (memcmp(&(info->realDisplaySize), &(pq_dev->pqu_meta.realDisplaySize),
		sizeof(struct window_info))) {
		*meta_change = true;
		memcpy(&(info->realDisplaySize), &(pq_dev->pqu_meta.realDisplaySize),
			sizeof(struct window_info));
	}

	if (_mtk_display_compare_dummy_meta(pq_dev, info) == true)
		*meta_change = true;


	if (_force_output) {
		*meta_change = true;
		_force_output = false;
	}

	return ret;
}
int mtk_display_set_frame_metadata(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf)
{
	int ret = 0;
	int i = 0;
	__u8 input_source = 0;
	struct meta_pq_display_flow_info *pq_meta_ptr = NULL;
	struct meta_pq_forcep_info *pq_forcep_info_ptr = NULL;
	struct meta_pq_input_queue_ext_info *pq_queue_ext_info_ptr = NULL;

	if (pq_dev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (pq_buf == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	/* meta buffer handle */
	pq_dev->meta_buf.paddr = pq_buf->meta_buf.va;
	pq_dev->meta_buf.size = pq_buf->meta_buf.size;

	ret = mtk_pq_common_read_metadata_addr_ptr(
		&pq_dev->meta_buf, EN_PQ_METATAG_DISPLAY_FLOW_INFO, (void **)&pq_meta_ptr);
	if (ret || pq_meta_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_DISPLAY_FLOW_INFO Failed, ret = %d\n", ret);
		goto exit;
	}

	/* meta get window info */
	memcpy(&(pq_dev->pqu_meta.content), &(pq_meta_ptr->content),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.cap_win), &(pq_meta_ptr->capture),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.crop_win), &(pq_meta_ptr->crop),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.disp_win), &(pq_meta_ptr->display),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.displayArea), &(pq_meta_ptr->displayArea),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.displayRange), &(pq_meta_ptr->displayRange),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.displayBase), &(pq_meta_ptr->displayBase),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.realDisplaySize), &(pq_meta_ptr->realDisplaySize),
				sizeof(struct meta_pq_window));
	memcpy(&(pq_dev->pqu_meta.ip_win), &(pq_meta_ptr->ip_win),
				sizeof(struct meta_pq_window)*meta_ip_max);
	memcpy(&(pq_dev->pqu_meta.mwe_active_win), &(pq_meta_ptr->mwe_active_win),
				sizeof(struct meta_pq_window)*meta_ip_max);

	pq_dev->pqu_meta.win_id = pq_dev->dev_indx;

	/*fill debug info*/
	pq_dev->pq_debug.content_in.x = pq_meta_ptr->content.x;
	pq_dev->pq_debug.content_in.y = pq_meta_ptr->content.y;
	pq_dev->pq_debug.content_in.width = pq_meta_ptr->content.width;
	pq_dev->pq_debug.content_in.height = pq_meta_ptr->content.height;
	pq_dev->pq_debug.content_in.w_align = pq_meta_ptr->content.w_align;
	pq_dev->pq_debug.content_in.h_align = pq_meta_ptr->content.h_align;
	pq_dev->pq_debug.capture_in.x = pq_meta_ptr->capture.x;
	pq_dev->pq_debug.capture_in.y = pq_meta_ptr->capture.y;
	pq_dev->pq_debug.capture_in.width = pq_meta_ptr->capture.width;
	pq_dev->pq_debug.capture_in.height = pq_meta_ptr->capture.height;
	pq_dev->pq_debug.capture_in.w_align = pq_meta_ptr->capture.w_align;
	pq_dev->pq_debug.capture_in.h_align = pq_meta_ptr->capture.h_align;
	pq_dev->pq_debug.crop_in.x = pq_meta_ptr->crop.x;
	pq_dev->pq_debug.crop_in.y = pq_meta_ptr->crop.y;
	pq_dev->pq_debug.crop_in.width = pq_meta_ptr->crop.width;
	pq_dev->pq_debug.crop_in.height = pq_meta_ptr->crop.height;
	pq_dev->pq_debug.crop_in.w_align = pq_meta_ptr->crop.w_align;
	pq_dev->pq_debug.crop_in.h_align = pq_meta_ptr->crop.h_align;
	pq_dev->pq_debug.display_in.x = pq_meta_ptr->display.x;
	pq_dev->pq_debug.display_in.y = pq_meta_ptr->display.y;
	pq_dev->pq_debug.display_in.width = pq_meta_ptr->display.width;
	pq_dev->pq_debug.display_in.height = pq_meta_ptr->display.height;
	pq_dev->pq_debug.display_in.w_align = pq_meta_ptr->display.w_align;
	pq_dev->pq_debug.display_in.h_align = pq_meta_ptr->display.h_align;
	pq_dev->pq_debug.displayArea_in.x =
		pq_meta_ptr->displayArea.x;
	pq_dev->pq_debug.displayArea_in.y =
		pq_meta_ptr->displayArea.y;
	pq_dev->pq_debug.displayArea_in.width =
		pq_meta_ptr->displayArea.width;
	pq_dev->pq_debug.displayArea_in.height =
		pq_meta_ptr->displayArea.height;
	pq_dev->pq_debug.displayArea_in.w_align =
		pq_meta_ptr->displayArea.w_align;
	pq_dev->pq_debug.displayArea_in.h_align =
		pq_meta_ptr->displayArea.h_align;
	pq_dev->pq_debug.displayRange_in.x =
		pq_meta_ptr->displayRange.x;
	pq_dev->pq_debug.displayRange_in.y =
		pq_meta_ptr->displayRange.y;
	pq_dev->pq_debug.displayRange_in.width =
		pq_meta_ptr->displayRange.width;
	pq_dev->pq_debug.displayRange_in.height =
		pq_meta_ptr->displayRange.height;
	pq_dev->pq_debug.displayRange_in.w_align =
		pq_meta_ptr->displayRange.w_align;
	pq_dev->pq_debug.displayRange_in.h_align =
		pq_meta_ptr->displayRange.h_align;
	pq_dev->pq_debug.displayBase_in.x =
		pq_meta_ptr->displayBase.x;
	pq_dev->pq_debug.displayBase_in.y =
		pq_meta_ptr->displayBase.y;
	pq_dev->pq_debug.displayBase_in.width =
		pq_meta_ptr->displayBase.width;
	pq_dev->pq_debug.displayBase_in.height =
		pq_meta_ptr->displayBase.height;
	pq_dev->pq_debug.displayBase_in.w_align =
		pq_meta_ptr->displayBase.w_align;
	pq_dev->pq_debug.displayBase_in.h_align =
		pq_meta_ptr->displayBase.h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].x =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].y =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].width =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].height =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_scmi_out].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_scmi_out].h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].x =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].y =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].width =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].height =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_in].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_in].h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].x =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].y =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].width =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].height =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_aisr_out].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_aisr_out].h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].x =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].y =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].width =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].height =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_in].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_in].h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].x =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].y =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].width =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].height =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_hvsp_out].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_hvsp_out].h_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].x =
		pq_meta_ptr->ip_win[pq_debug_ip_display].x;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].y =
		pq_meta_ptr->ip_win[pq_debug_ip_display].y;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].width =
		pq_meta_ptr->ip_win[pq_debug_ip_display].width;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].height =
		pq_meta_ptr->ip_win[pq_debug_ip_display].height;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].w_align =
		pq_meta_ptr->ip_win[pq_debug_ip_display].w_align;
	pq_dev->pq_debug.ip_win[pq_debug_ip_display].h_align =
		pq_meta_ptr->ip_win[pq_debug_ip_display].h_align;
	/*fill debug info end*/

	pq_dev->pqu_meta.pq_win_type = (enum pqu_win_type)pq_meta_ptr->pq_win_type;
	pq_dev->pqu_meta.multiple_window = pq_meta_ptr->multiple_window;

	pq_dev->pqu_meta.flowControlEn = pq_dev->display_info.flowControl.enable;
	pq_dev->pqu_meta.flowControlFactor = pq_dev->display_info.flowControl.factor;

	pq_dev->pqu_meta.aisr_en = pq_meta_ptr->aisr_enable;
	pq_dev->pqu_meta.vip_path = pq_meta_ptr->vip_path;
	pq_dev->pqu_meta.aisr_ui_level = pq_meta_ptr->aisr_ui_level;
	pq_dev->pqu_meta.bAisrActiveWinEn = pq_dev->display_info.aisrActiveWin.bEnable;
	pq_dev->pqu_meta.aisr_active_win_info.x = pq_dev->display_info.aisrActiveWin.x;
	pq_dev->pqu_meta.aisr_active_win_info.y = pq_dev->display_info.aisrActiveWin.y;
	pq_dev->pqu_meta.aisr_active_win_info.width = pq_dev->display_info.aisrActiveWin.width;
	pq_dev->pqu_meta.aisr_active_win_info.height = pq_dev->display_info.aisrActiveWin.height;
	pq_dev->pqu_meta.pqu_log_level = _gu64PquLogLevel;
	if (pq_dev->dev_indx < PQ_WIN_MAX_NUM) {
		pq_dev->pqu_meta.srs_dbg.dbg_enable = gSrsDbg[pq_dev->dev_indx].dbg_enable;
		pq_dev->pqu_meta.srs_dbg.srs_enable = gSrsDbg[pq_dev->dev_indx].srs_enable;
		pq_dev->pqu_meta.srs_dbg.tgen_mode = gSrsDbg[pq_dev->dev_indx].tgen_mode;
		pq_dev->pqu_meta.srs_dbg.db_idx = gSrsDbg[pq_dev->dev_indx].db_idx;
		pq_dev->pqu_meta.srs_dbg.method = gSrsDbg[pq_dev->dev_indx].method;
		pq_dev->pqu_meta.srs_dbg.bin_mode = gSrsDbg[pq_dev->dev_indx].bin_mode;
		pq_dev->pqu_meta.srs_dbg.bin_db_idx = gSrsDbg[pq_dev->dev_indx].bin_db_idx;
	} else
		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
			"[SRS]pq_dev->dev_indx:invalid(%u)\n", pq_dev->dev_indx);
	/*fill debug info*/
	pq_dev->pq_debug.pq_win_type =
						(enum pq_debug_win_type)pq_meta_ptr->pq_win_type;
	pq_dev->pq_debug.multiple_window = pq_meta_ptr->multiple_window;
	pq_dev->pq_debug.flowControlEn = pq_dev->display_info.flowControl.enable;
	pq_dev->pq_debug.flowControlFactor =
						pq_dev->display_info.flowControl.factor;
	pq_dev->pq_debug.aisr_en = pq_meta_ptr->aisr_enable;
	pq_dev->pq_debug.vip_path = pq_meta_ptr->vip_path;
	pq_dev->pq_debug.bAisrActiveWinEn =
						pq_dev->display_info.aisrActiveWin.bEnable;
	pq_dev->pq_debug.aisr_active_win_info.x =
						pq_dev->display_info.aisrActiveWin.x;
	pq_dev->pq_debug.aisr_active_win_info.y =
						pq_dev->display_info.aisrActiveWin.y;
	pq_dev->pq_debug.aisr_active_win_info.width =
						pq_dev->display_info.aisrActiveWin.width;
	pq_dev->pq_debug.aisr_active_win_info.height =
						pq_dev->display_info.aisrActiveWin.height;
	pq_dev->pq_debug.pqu_log_level = _gu64PquLogLevel;
	if (pq_dev->dev_indx < PQ_WIN_MAX_NUM) {
		memcpy(&pq_dev->pq_debug.srs_dbg,
			&gSrsDbg[pq_dev->dev_indx],
			sizeof(struct debug_srs_info));
	} else
		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
			"[SRS]pq_dev->dev_indx:invalid(%u)\n", pq_dev->dev_indx);
	_mtk_pq_set_aisr_dbg(pq_dev, &pq_dev->pqu_meta);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"m_pq_display_flow_ctrl : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"content = ", pq_dev->pqu_meta.content.x, pq_dev->pqu_meta.content.y,
		pq_dev->pqu_meta.content.width, pq_dev->pqu_meta.content.height,
	"cap_win = ", pq_dev->pqu_meta.cap_win.x, pq_dev->pqu_meta.cap_win.y,
		pq_dev->pqu_meta.cap_win.width, pq_dev->pqu_meta.cap_win.height,
	"crop_win = ", pq_dev->pqu_meta.crop_win.x, pq_dev->pqu_meta.crop_win.y,
		pq_dev->pqu_meta.crop_win.width, pq_dev->pqu_meta.crop_win.height,
	"disp_win = ", pq_dev->pqu_meta.disp_win.x, pq_dev->pqu_meta.disp_win.y,
		pq_dev->pqu_meta.disp_win.width, pq_dev->pqu_meta.disp_win.height,
	"displayArea = ", pq_dev->pqu_meta.displayArea.x, pq_dev->pqu_meta.displayArea.y,
		pq_dev->pqu_meta.displayArea.width, pq_dev->pqu_meta.displayArea.height,
	"displayRange = ", pq_dev->pqu_meta.displayRange.x, pq_dev->pqu_meta.displayRange.y,
		pq_dev->pqu_meta.displayRange.width, pq_dev->pqu_meta.displayRange.height,
	"displayBase = ", pq_dev->pqu_meta.displayBase.x, pq_dev->pqu_meta.displayBase.y,
		pq_dev->pqu_meta.displayBase.width, pq_dev->pqu_meta.displayBase.height,
	"realDisplaySize = ", pq_dev->pqu_meta.realDisplaySize.x,
		pq_dev->pqu_meta.realDisplaySize.y, pq_dev->pqu_meta.realDisplaySize.width,
		pq_dev->pqu_meta.realDisplaySize.height);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"m_pq_display_get_ip_size : %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d), %s(%d,%d)\n",
	"scmi out = ", pq_dev->pqu_meta.ip_win[pqu_ip_scmi_out].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_scmi_out].height,
	"aisr_in = ", pq_dev->pqu_meta.ip_win[pqu_ip_aisr_in].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_aisr_in].height,
	"aisr_out = ", pq_dev->pqu_meta.ip_win[pqu_ip_aisr_out].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_aisr_out].height,
	"hvsp_in = ", pq_dev->pqu_meta.ip_win[pqu_ip_hvsp_in].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_hvsp_in].height,
	"hvsp_out = ", pq_dev->pqu_meta.ip_win[pqu_ip_hvsp_out].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_hvsp_out].height,
	"display = ", pq_dev->pqu_meta.ip_win[pqu_ip_display].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_display].height,
	"panel_display = ", pq_dev->pqu_meta.ip_win[pqu_ip_panel_display].width,
		pq_dev->pqu_meta.ip_win[pqu_ip_panel_display].height);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"m_pq_aisr_enable : %s%d, %s%d\n",
	"aisr_en = ", pq_dev->pqu_meta.aisr_en,
	"vip_path = ", pq_dev->pqu_meta.vip_path);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"m_pq_mwe_get_ip_size : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"scmi in = ", pq_dev->pqu_meta.mwe_active_win[pqu_ip_scmi_in].x,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_scmi_in].y,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_scmi_in].width,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_scmi_in].height,
	"scmi out = ", pq_dev->pqu_meta.mwe_active_win[meta_ip_scmi_out].x,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_scmi_out].y,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_scmi_out].width,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_scmi_out].height,
	"aisr_in = ", pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_in].x,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_in].y,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_in].width,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_in].height,
	"aisr_out = ", pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_out].x,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_out].y,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_out].width,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_aisr_out].height,
	"hvsp_in = ", pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_in].x,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_in].y,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_in].width,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_in].height,
	"hvsp_out = ", pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_out].x,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_out].y,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_out].width,
		pq_dev->pqu_meta.mwe_active_win[pqu_ip_hvsp_out].height,
	"display = ", pq_dev->pqu_meta.mwe_active_win[meta_ip_display].x,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_display].y,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_display].width,
		pq_dev->pqu_meta.mwe_active_win[meta_ip_display].height);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[FC]Flow control enable:%d factor:%d\n",
		pq_dev->pqu_meta.flowControlEn,
		pq_dev->pqu_meta.flowControlFactor);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[WinInfo]pq_win_type:%d multiple_window:%d\n",
		pq_dev->pqu_meta.pq_win_type,
		pq_dev->pqu_meta.multiple_window);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[AISR]Active win enable:%u x:%u, y:%u, w:%u, h:%u\n",
		pq_dev->pqu_meta.bAisrActiveWinEn,
		pq_dev->pqu_meta.aisr_active_win_info.x,
		pq_dev->pqu_meta.aisr_active_win_info.y,
		pq_dev->pqu_meta.aisr_active_win_info.width,
		pq_dev->pqu_meta.aisr_active_win_info.height);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[PQ Bypass CMD] HSY:%d, VIP1:%d, VIP:%d, AISR_PQ:%d, HVSP_PRE:%d, HVSP_POST:%d, SPF:%d, HDR10:%d, VOP:%d, DV:%d, MCNR:%d, ABF:%d, SR:%d, DLC:%d, UCD:%d\n",
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_HSY],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_VIP1],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_VIP],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_AISR_PQ],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_HVSP_PRE],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_HVSP_POST],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_SPF],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_HDR10],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_VOP],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_DV],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_MCNR],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_ABF],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_SR],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_DLC],
		pq_dev->pqu_meta.pq_byp[PQU_IP_BYPASS_UCD]);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"pqu_log_level : 0x%llx\n", pq_dev->pqu_meta.pqu_log_level);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"[SRS]dbg_enable:%d, srs_enable:%d, tgen_mode:%u, db_idx:%u, method:%u, bin_mode:%d, bin_db_idx:%u\n",
		pq_dev->pqu_meta.srs_dbg.dbg_enable,
		pq_dev->pqu_meta.srs_dbg.srs_enable, pq_dev->pqu_meta.srs_dbg.tgen_mode,
		pq_dev->pqu_meta.srs_dbg.db_idx, pq_dev->pqu_meta.srs_dbg.method,
		pq_dev->pqu_meta.srs_dbg.bin_mode, pq_dev->pqu_meta.srs_dbg.bin_db_idx);

	input_source = pq_dev->common_info.input_source;
	if (IS_INPUT_B2R(input_source)) {
		pq_dev->pqu_meta.InputHTT = pq_dev->b2r_info.timing_out.H_TotalCount;
		pq_dev->pqu_meta.InputVTT = pq_dev->b2r_info.timing_out.V_TotalCount;
	} else {
		pq_dev->pqu_meta.InputHTT = pq_dev->display_info.idr.inputHTT;
		pq_dev->pqu_meta.InputVTT = pq_dev->display_info.idr.inputVTT;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"input_source = %d, InputHTT = %d, InputVTT = %d\n",
		input_source, pq_dev->pqu_meta.InputHTT, pq_dev->pqu_meta.InputVTT);

	ret = mtk_pq_common_write_metadata_addr(
		&pq_dev->meta_buf, EN_PQ_METATAG_PQU_DISP_FLOW_INFO, &pq_dev->pqu_meta);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_PQU_DISP_FLOW_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	pq_dev->pqu_meta.pq_status_en = false;

	ret = mtk_pq_common_read_metadata_addr_ptr(
		&pq_dev->meta_buf, EN_PQ_METATAG_INPUT_QUEUE_EXT_INFO,
		(void **)&pq_queue_ext_info_ptr);
	if (ret || pq_queue_ext_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_INPUT_QUEUE_EXT_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	pq_dev->pqu_queue_ext_info.u64Pts = pq_queue_ext_info_ptr->u64Pts;
	pq_dev->pqu_queue_ext_info.u64UniIdx = pq_queue_ext_info_ptr->u64UniIdx;
	pq_dev->pqu_queue_ext_info.u64ExtUniIdx = pq_queue_ext_info_ptr->u64ExtUniIdx;
	pq_dev->pqu_queue_ext_info.u64TimeStamp = pq_queue_ext_info_ptr->u64TimeStamp;
	pq_dev->pqu_queue_ext_info.u64RenderTimeNs = pq_queue_ext_info_ptr->u64RenderTimeNs;
	pq_dev->pqu_queue_ext_info.u8BufferValid = pq_queue_ext_info_ptr->u8BufferValid;
	pq_dev->pqu_queue_ext_info.u32BufferSlot = pq_queue_ext_info_ptr->u32BufferSlot;
	pq_dev->pqu_queue_ext_info.u32GenerationIndex
		= pq_queue_ext_info_ptr->u32GenerationIndex;
	pq_dev->pqu_queue_ext_info.u32StreamUniqueId
		= pq_queue_ext_info_ptr->u32StreamUniqueId;
	pq_dev->pqu_queue_ext_info.u8RepeatStatus = pq_queue_ext_info_ptr->u8RepeatStatus;
	pq_dev->pqu_queue_ext_info.u8FrcMode = pq_queue_ext_info_ptr->u8FrcMode;
	pq_dev->pqu_queue_ext_info.u8Interlace = pq_queue_ext_info_ptr->u8Interlace;
	pq_dev->pqu_queue_ext_info.u32InputFps = pq_queue_ext_info_ptr->u32InputFps;
	pq_dev->pqu_queue_ext_info.u32OriginalInputFps
		= pq_queue_ext_info_ptr->u32OriginalInputFps;
	pq_dev->pqu_queue_ext_info.bEOS = pq_queue_ext_info_ptr->bEOS;
	pq_dev->pqu_queue_ext_info.u8MuteAction = pq_queue_ext_info_ptr->u8MuteAction;
	pq_dev->pqu_queue_ext_info.u8PqMuteAction = pq_queue_ext_info_ptr->u8PqMuteAction;
	pq_dev->pqu_queue_ext_info.u8SignalStable = pq_queue_ext_info_ptr->u8SignalStable;
	pq_dev->pqu_queue_ext_info.u8DotByDotType = pq_queue_ext_info_ptr->u8DotByDotType;
	pq_dev->pqu_queue_ext_info.u32RefreshRate = pq_queue_ext_info_ptr->u32RefreshRate;
	pq_dev->pqu_queue_ext_info.bReportFrameStamp = pq_queue_ext_info_ptr->bReportFrameStamp;
	pq_dev->pqu_queue_ext_info.bBypassAvsync = pq_queue_ext_info_ptr->bBypassAvsync;
	pq_dev->pqu_queue_ext_info.u32HdrApplyType = pq_queue_ext_info_ptr->u32HdrApplyType;
	pq_dev->pqu_queue_ext_info.u32QueueInputIndex
		= pq_queue_ext_info_ptr->u32QueueInputIndex;
	pq_dev->pqu_queue_ext_info.idrinfo.y = pq_queue_ext_info_ptr->idrinfo.y;
	pq_dev->pqu_queue_ext_info.idrinfo.height = pq_queue_ext_info_ptr->idrinfo.height;
	pq_dev->pqu_queue_ext_info.idrinfo.v_total = pq_queue_ext_info_ptr->idrinfo.v_total;
	pq_dev->pqu_queue_ext_info.idrinfo.vs_pulse_width
		= pq_queue_ext_info_ptr->idrinfo.vs_pulse_width;
	pq_dev->pqu_queue_ext_info.idrinfo.fdet_vtt0 = pq_queue_ext_info_ptr->idrinfo.fdet_vtt0;
	pq_dev->pqu_queue_ext_info.idrinfo.fdet_vtt1 = pq_queue_ext_info_ptr->idrinfo.fdet_vtt1;
	pq_dev->pqu_queue_ext_info.idrinfo.fdet_vtt2 = pq_queue_ext_info_ptr->idrinfo.fdet_vtt2;
	pq_dev->pqu_queue_ext_info.idrinfo.fdet_vtt3 = pq_queue_ext_info_ptr->idrinfo.fdet_vtt3;
	pq_dev->pqu_queue_ext_info.bMuteChange = pq_queue_ext_info_ptr->bMuteChange;
	pq_dev->pqu_queue_ext_info.bFdMaskBypass = pq_queue_ext_info_ptr->bFdMaskBypass;
	pq_dev->pqu_queue_ext_info.bTriggerInfiniteGen = pq_queue_ext_info_ptr->bTriggerInfiniteGen;
	pq_dev->pqu_queue_ext_info.bEnableBbd = pq_queue_ext_info_ptr->bEnableBbd;
	pq_dev->pqu_queue_ext_info.bPerFrameMode = pq_queue_ext_info_ptr->bPerFrameMode;

	/*fill the debug info*/
	pq_dev->pq_debug.input_queue_ext_info.u64Pts =
		pq_queue_ext_info_ptr->u64Pts;
	pq_dev->pq_debug.input_queue_ext_info.u64UniIdx =
		pq_queue_ext_info_ptr->u64UniIdx;
	pq_dev->pq_debug.input_queue_ext_info.u64ExtUniIdx =
		pq_queue_ext_info_ptr->u64ExtUniIdx;
	pq_dev->pq_debug.input_queue_ext_info.u64TimeStamp =
		pq_queue_ext_info_ptr->u64TimeStamp;
	pq_dev->pq_debug.input_queue_ext_info.u64RenderTimeNs =
		pq_queue_ext_info_ptr->u64RenderTimeNs;
	pq_dev->pq_debug.input_queue_ext_info.u8BufferValid =
		pq_queue_ext_info_ptr->u8BufferValid;
	pq_dev->pq_debug.input_queue_ext_info.u32BufferSlot =
		pq_queue_ext_info_ptr->u32BufferSlot;
	pq_dev->pq_debug.input_queue_ext_info.u32GenerationIndex
		= pq_queue_ext_info_ptr->u32GenerationIndex;
	pq_dev->pq_debug.input_queue_ext_info.u32StreamUniqueId
		= pq_queue_ext_info_ptr->u32StreamUniqueId;
	pq_dev->pq_debug.input_queue_ext_info.u8RepeatStatus =
		pq_queue_ext_info_ptr->u8RepeatStatus;
	pq_dev->pq_debug.input_queue_ext_info.u8FrcMode =
		pq_queue_ext_info_ptr->u8FrcMode;
	pq_dev->pq_debug.input_queue_ext_info.u8Interlace =
		pq_queue_ext_info_ptr->u8Interlace;
	pq_dev->pq_debug.input_queue_ext_info.u32InputFps =
		pq_queue_ext_info_ptr->u32InputFps;
	pq_dev->pq_debug.input_queue_ext_info.u32OriginalInputFps
		= pq_queue_ext_info_ptr->u32OriginalInputFps;
	pq_dev->pq_debug.input_queue_ext_info.bEOS =
		pq_queue_ext_info_ptr->bEOS;
	pq_dev->pq_debug.input_queue_ext_info.u8MuteAction =
		pq_queue_ext_info_ptr->u8MuteAction;
	pq_dev->pq_debug.input_queue_ext_info.u8PqMuteAction =
		pq_queue_ext_info_ptr->u8PqMuteAction;
	pq_dev->pq_debug.input_queue_ext_info.u8SignalStable =
		pq_queue_ext_info_ptr->u8SignalStable;
	pq_dev->pq_debug.input_queue_ext_info.u8DotByDotType =
		pq_queue_ext_info_ptr->u8DotByDotType;
	pq_dev->pq_debug.input_queue_ext_info.u32RefreshRate =
		pq_queue_ext_info_ptr->u32RefreshRate;
	pq_dev->pq_debug.input_queue_ext_info.bReportFrameStamp =
		pq_queue_ext_info_ptr->bReportFrameStamp;
	pq_dev->pq_debug.input_queue_ext_info.bBypassAvsync =
		pq_queue_ext_info_ptr->bBypassAvsync;
	pq_dev->pq_debug.input_queue_ext_info.u32HdrApplyType =
		pq_queue_ext_info_ptr->u32HdrApplyType;
	pq_dev->pq_debug.input_queue_ext_info.u32QueueInputIndex
		= pq_queue_ext_info_ptr->u32QueueInputIndex;
	pq_dev->pq_debug.input_queue_ext_info.bMuteChange
		= pq_queue_ext_info_ptr->bMuteChange;
	pq_dev->pq_debug.input_queue_ext_info.bFdMaskBypass
		= pq_queue_ext_info_ptr->bFdMaskBypass;
	pq_dev->pq_debug.input_queue_ext_info.bTriggerInfiniteGen
		= pq_queue_ext_info_ptr->bTriggerInfiniteGen;
	pq_dev->pq_debug.input_queue_ext_info.bEnableBbd
		= pq_queue_ext_info_ptr->bEnableBbd;
	pq_dev->pq_debug.input_queue_ext_info.bPerFrameMode
		= pq_queue_ext_info_ptr->bPerFrameMode;
	/*fill the debug info end*/
	pq_dev->pq_debug.queue_pts[pq_dev->pq_debug.pts_idx] =
		pq_dev->pqu_queue_ext_info.u64Pts;
	pq_dev->pq_debug.pts_idx++;
	if (pq_dev->pq_debug.pts_idx >= DROP_ARY_IDX-1)
		pq_dev->pq_debug.pts_idx = 0;
	/*debug info total que*/
	pq_dev->pq_debug.total_que++;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"m_pq_queue_ext_info : %s%llu, %s%llu, %s%llu, %s%llu, %s%llu, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%d, %s%u, %s%u, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"u64Pts = ", pq_dev->pqu_queue_ext_info.u64Pts,
	"u64UniIdx = ", pq_dev->pqu_queue_ext_info.u64UniIdx,
	"u64ExtUniIdx = ", pq_dev->pqu_queue_ext_info.u64ExtUniIdx,
	"u64TimeStamp = ", pq_dev->pqu_queue_ext_info.u64TimeStamp,
	"u64RenderTimeNs = ", pq_dev->pqu_queue_ext_info.u64RenderTimeNs,
	"u8BufferValid = ", pq_dev->pqu_queue_ext_info.u8BufferValid,
	"u32GenerationIndex = ", pq_dev->pqu_queue_ext_info.u32GenerationIndex,
	"u32StreamUniqueId = ", pq_dev->pqu_queue_ext_info.u32StreamUniqueId,
	"u8RepeatStatus = ", pq_dev->pqu_queue_ext_info.u8RepeatStatus,
	"u8FrcMode = ", pq_dev->pqu_queue_ext_info.u8FrcMode,
	"u8Interlace = ", pq_dev->pqu_queue_ext_info.u8Interlace,
	"u32InputFps = ", pq_dev->pqu_queue_ext_info.u32InputFps,
	"u32OriginalInputFps = ", pq_dev->pqu_queue_ext_info.u32OriginalInputFps,
	"bEOS = ", pq_dev->pqu_queue_ext_info.bEOS,
	"u8MuteAction = ", pq_dev->pqu_queue_ext_info.u8MuteAction,
	"u8PqMuteAction = ", pq_dev->pqu_queue_ext_info.u8PqMuteAction,
	"u8SignalStable = ", pq_dev->pqu_queue_ext_info.u8SignalStable,
	"u8DotByDotType = ", pq_dev->pqu_queue_ext_info.u8DotByDotType,
	"u32RefreshRate = ", pq_dev->pqu_queue_ext_info.u32RefreshRate,
	"bReportFrameStamp = ", pq_dev->pqu_queue_ext_info.bReportFrameStamp,
	"bBypassAvsync = ", pq_dev->pqu_queue_ext_info.bBypassAvsync,
	"u32HdrApplyType = ", pq_dev->pqu_queue_ext_info.u32HdrApplyType,
	"u32QueueOutputIndex = ", pq_dev->pqu_queue_ext_info.u32QueueInputIndex,
	"bMuteChange = ", pq_dev->pqu_queue_ext_info.bMuteChange,
	"bFdMaskBypass = ", pq_dev->pqu_queue_ext_info.bFdMaskBypass,
	"bTriggerInfiniteGen = ", pq_dev->pqu_queue_ext_info.bTriggerInfiniteGen,
	"bEnableBbd = ", pq_dev->pqu_queue_ext_info.bEnableBbd,
	"bPerFrameMode = ", pq_dev->pqu_queue_ext_info.bPerFrameMode);


	pq_dev->pqu_queue_ext_info.pqrminfo.u32RemainBWBudget =
		pq_queue_ext_info_ptr->pqrminfo.u32RemainBWBudget;
	pq_dev->pqu_queue_ext_info.pqrminfo.u8ActiveWinNum =
		pq_queue_ext_info_ptr->pqrminfo.u8ActiveWinNum;
	for (i = 0; i < pq_queue_ext_info_ptr->pqrminfo.u8ActiveWinNum; i++) {
		pq_dev->pqu_queue_ext_info.pqrminfo.win_info[i].u8PqID =
			pq_queue_ext_info_ptr->pqrminfo.win_info[i].u8PqID;
		pq_dev->pqu_queue_ext_info.pqrminfo.win_info[i].u32WinTime =
			pq_queue_ext_info_ptr->pqrminfo.win_info[i].u32WinTime;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"u8ActiveWinNum = %d\n",
		pq_dev->pqu_queue_ext_info.pqrminfo.u8ActiveWinNum);

	ret = mtk_pq_common_write_metadata_addr(
		&pq_dev->meta_buf, EN_PQ_METATAG_PQU_QUEUE_EXT_INFO, &pq_dev->pqu_queue_ext_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_PQU_QUEUE_EXT_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	/* thermal metadata */
	mtk_pq_cdev_get_thermal_info(&pq_dev->thermal_info);
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"Thermal aisr state(0:ON,1:KEEP,2:HALF,3:OFF)=%u\n",
		pq_dev->thermal_info.thermal_state_aisr);

	ret = mtk_pq_common_write_metadata_addr(
		&pq_dev->meta_buf, EN_PQ_METATAG_PQU_THERMAL_INFO, &pq_dev->thermal_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_PQU_THERMAL_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	if ((pq_dev->stream_meta.pq_scene == meta_pq_scene_force_p) &&
		(pq_dev->stream_meta.bForceP == true)) {
		/* forceP metadata */
		ret = mtk_pq_common_read_metadata_addr_ptr(
			&pq_dev->meta_buf, EN_PQ_METATAG_FORCEP_INFO, (void **)&pq_forcep_info_ptr);
		if (ret || pq_forcep_info_ptr == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq read EN_PQ_METATAG_FORCEP_INFO fail, ret=%d\n", ret);
			goto exit;
		}

		pq_dev->pqu_forcep_info.PIChange = pq_forcep_info_ptr->PIChange;
		pq_dev->pqu_forcep_info.PICounter = pq_forcep_info_ptr->PICounter;
		pq_dev->pqu_forcep_info.source_interlace = pq_forcep_info_ptr->source_interlace;
		pq_dev->pqu_forcep_info.source_level =
				(enum pqu_quality)pq_forcep_info_ptr->source_level;
		pq_dev->pqu_forcep_info.target_interlace = pq_forcep_info_ptr->target_interlace;
		pq_dev->pqu_forcep_info.target_level =
				(enum pqu_quality)pq_forcep_info_ptr->target_level;

		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
			"m_pq_forcep_info : %s%d, %s%llu, %s%u, %s%u, %s%u, %s%u\n",
			"PIChange = ", pq_dev->pqu_forcep_info.PIChange,
			"PICounter = ", pq_dev->pqu_forcep_info.PICounter,
			"source_interlace = ", pq_dev->pqu_forcep_info.source_interlace,
			"source_level = ", pq_dev->pqu_forcep_info.source_level,
			"target_interlace = ", pq_dev->pqu_forcep_info.target_interlace,
			"target_level = ", pq_dev->pqu_forcep_info.target_level);

		ret = mtk_pq_common_write_metadata_addr(
			&pq_dev->meta_buf, EN_PQ_METATAG_PQU_FORCEP_INFO, &pq_dev->pqu_forcep_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq write EN_PQ_METATAG_PQU_FORCEP_INFO fail, ret=%d\n", ret);
			goto exit;
		}
	}
exit:
	return ret;
}

int mtk_display_abf_blk_mapping(uint16_t mode)
{
	switch (mode) {
	case PQ_ABF_BLK_MODE1:
		return PQ_ABF_BLK_SIZE1;
	case PQ_ABF_BLK_MODE2:
		return PQ_ABF_BLK_SIZE2;
	case PQ_ABF_BLK_MODE3:
		return PQ_ABF_BLK_SIZE3;
	case PQ_ABF_BLK_MODE4:
		return PQ_ABF_BLK_SIZE4;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "abf block mode error\n");
		return PQ_ABF_BLK_SIZE4;
	}
}

void mtk_display_abf_blk_mode(int width, int height, uint16_t *ctur_mode, uint16_t *ctur2_mode)
{
	if ((width <= PQ_ABF_CUTR_MODE1_W_SIZE) && (height <= PQ_ABF_CUTR_MODE1_H_SIZE) &&
		((width * height) <= PQ_ABF_CUTR_MODE1_HV_SIZE)) {
		*ctur_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE2);
		*ctur2_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE1);
	} else if ((width <= PQ_ABF_CUTR_MODE2_W_SIZE) && (height <= PQ_ABF_CUTR_MODE2_H_SIZE) &&
		((width * height) <= PQ_ABF_CUTR_MODE2_HV_SIZE)) {
		*ctur_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE3);
		*ctur2_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE2);
	} else {
		*ctur_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE4);
		*ctur2_mode = mtk_display_abf_blk_mapping(PQ_ABF_BLK_MODE3);
	}
}

int mtk_display_dynamic_ultra_init(struct mtk_pq_platform_device *pqdev)
{
	struct hwregWriteMethodInfo info;

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}
	memset(&info, 0, sizeof(struct hwregWriteMethodInfo));

	info.eMethod = HWREG_WRITE_BY_RIU;

	drv_hwreg_udma_init_version(pqdev->pqcaps.u32HAL_Version);

	drv_hwreg_udma_init_priority_level_srccap(&info);

	drv_hwreg_udma_init_priority_level_pq(&info);

	drv_hwreg_udma_init_priority_level_render(&info);

	return 0;
}

int mtk_display_parse_dts(struct display_device *display_dev, struct mtk_pq_platform_device *pqdev)
{
	int ret = 0;
	struct device_node *np;
	struct device *property_dev = pqdev->dev;
	__u32 u32Tmp = 0;
	const char *name;

	if (property_dev->of_node)
		of_node_get(property_dev->of_node);

	np = of_find_node_by_name(property_dev->of_node, "display");

	if (np != NULL) {
		// read byte per word
		name = "byte_per_word";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->bpw = (u8)u32Tmp;

		name = "BUF_IOMMU_OFFSET";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->buf_iommu_offset = u32Tmp;

		// read pq buffer tag
		name = "PQ_IOMMU_IDX_DV";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_iommu_dv = u32Tmp;

		// read pq buffer tag
		name = "PQ_IOMMU_IDX_SCMI";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_iommu_idx_scmi = u32Tmp;

		// read pq buffer tag
		name = "PQ_IOMMU_IDX_UCM";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_iommu_idx_ucm = u32Tmp;

		// read pq buffer tag
		name = "PQ_IOMMU_IDX_WIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_iommu_window = u32Tmp;

		// read pq dip buffer tag
		name = "PQ_IOMMU_IDX_DIP";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_iommu_idx_dip = u32Tmp;

		// read usable window count
		name = "USABLE_WIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		pqdev->usable_win = u32Tmp;

		// read h pixel alignment
		name = "PQ_H_DI_P_ALIGN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->di_align = (__u8)u32Tmp;

		name = "PQ_H_PREHVSP_P_ALIGN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pre_hvsp_align = (__u8)u32Tmp;

		name = "PQ_H_HVSP_P_ALIGN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->hvsp_align = (__u8)u32Tmp;

		name = "PQ_H_POSTHVSP_P_ALIGN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->post_hvsp_align = (__u8)u32Tmp;

		// read h pixel alignment
		name = "PQ_H_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->h_max_size = u32Tmp;

		// read h pixel alignment
		name = "PQ_V_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->v_max_size = u32Tmp;

		// read znr me h max size
		name = "PQ_ZNR_ME_H_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->znr_me_h_max_size = u32Tmp;

		// read znr me v max size
		name = "PQ_ZNR_ME_V_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->znr_me_v_max_size = u32Tmp;

		// read hvsp 12tap size
		name = "PQ_HVSP_12Tap_Size";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->hvsp_12tap_size = u32Tmp;

		name = "PQ_SPF_VTap_Size";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->spf_vtap_size = u32Tmp;

		name = "HIST_EXTRAFRAME_P";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->extraframe_p = u32Tmp;

		name = "HIST_EXTRAFRAME_I";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->extraframe_i = u32Tmp;

		// read mdw dts
		ret = mtk_pq_display_mdw_read_dts(np, &display_dev->mdw_dev);
		if (ret != 0x0)
			goto Fail;
#if (1)//(MEMC_CONFIG == 1)
		ret = mtk_pq_display_frc_read_dts(&display_dev->frc_dev);
		if (ret != 0x0)
			goto Fail;
#endif
		// read dip window num
		name = "DIP_WIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		pqdev->dip_win_num = u32Tmp;
		// read xc window num
		name = "XC_WIN";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		pqdev->xc_win_num = u32Tmp;

		// read h max size for pq cap
		name = "PQ_CHIP_CAP_INPUT_WIDTH_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_cap_width_max_size = u32Tmp;

		// read v max size for pq cap
		name = "PQ_CHIP_CAP_INPUT_HEIGHT_MAX_SIZE";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		display_dev->pq_cap_height_max_size = u32Tmp;

		// read thermal ctrl for pq
		name = "PQ_THERMAL_CTRL";
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0)
			goto Fail;
		pqdev->pq_thermal_ctrl_en = (__u8)u32Tmp;
	}

	of_node_put(np);

	return 0;

Fail:
	if (np != NULL)
		of_node_put(np);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY,
		"[%s, %d]: read DTS failed, name = %s\n",
		__func__, __LINE__, name);
	return ret;
}

int mtk_pq_register_display_subdev(struct v4l2_device *pq_dev,
			struct v4l2_subdev *subdev_display,
			struct v4l2_ctrl_handler *display_ctrl_handler)
{
	int ret = 0;
	__u32 ctrl_count;
	__u32 ctrl_num = sizeof(display_ctrl) / sizeof(struct v4l2_ctrl_config);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DISPLAY, "sid.sun::note!\n");

	v4l2_ctrl_handler_init(display_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++)
		v4l2_ctrl_new_custom(display_ctrl_handler,
		    &display_ctrl[ctrl_count], NULL);

	ret = display_ctrl_handler->error;
	if (ret) {
		v4l2_err(pq_dev, "failed to create display ctrl handler\n");
		goto exit;
	}
	subdev_display->ctrl_handler = display_ctrl_handler;

	v4l2_subdev_init(subdev_display, &display_sd_ops);
	subdev_display->internal_ops = &display_sd_internal_ops;
	strlcpy(subdev_display->name, "mtk-display",
	    sizeof(subdev_display->name));

	ret = v4l2_device_register_subdev(pq_dev, subdev_display);
	if (ret) {
		v4l2_err(pq_dev, "failed to register display subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(display_ctrl_handler);
	return ret;
}

void mtk_pq_unregister_display_subdev(struct v4l2_subdev *subdev_display)
{
	v4l2_ctrl_handler_free(subdev_display->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_display);
}

int mtk_pq_aisr_dbg_show(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	__u8 u8MaxSize = MAX_SYSFS_SIZE, win_id = 0;
	int count = 0;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);

	count += snprintf(buf + count, u8MaxSize - count,
		"AISR_Support=%d\n", pqdev->pqcaps.u32AISR_Support);
	count += snprintf(buf + count, u8MaxSize - count,
		"AISR_Version=%d\n", pqdev->pqcaps.u32AISR_Version);
	for (win_id = 0; win_id < PQ_WIN_MAX_NUM; win_id++)
		count += snprintf(buf + count, u8MaxSize - count,
			"W=%u En=%u R=%u\n", win_id,
			_gAisrdbg[win_id].bAISREnStatus,
			_gAisrdbg[win_id].u32ScaleStatus);

	return count;
}

int mtk_pq_aisr_dbg_store(struct device *dev, const char *buf)
{
	int ret = 0;
	int len = 0;
	__u32 dbg_en = 0;
	__u32 win_id = 0, enable = 0, x = 0, y = 0, width = 0, height = 0;
	__u32 scale = 1;
	enum AISR_DBG_TYPE dbg_mode = MTK_PQ_AISR_DBG_NONE;
	bool find = false;
	char *cmd = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;

	if ((buf == NULL) || (dev == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}

	if (!pqdev->pqcaps.u32AISR_Support) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "AISR not support!\n");
		return 0;
	}

	PQ_MALLOC(cmd, (sizeof(char) * 0x1000));
	if (cmd == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQ_MALLOC cmd fail!\n");
		return -EINVAL;
	}

	memset(cmd, 0, sizeof(char) * 0x1000);

	len = strlen(buf);

	ret = snprintf(cmd, len + 1, "%s", buf);
	if ((ret < 0) || (ret >= len + 1)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf failed!\n");
		ret = -EINVAL;
		goto exit;
	}

	find = _mtk_pq_get_dbg_value_from_string(cmd, "dbg_en", len, &dbg_en);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, type should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	find = _mtk_pq_get_dbg_value_from_string(cmd, "window", len, &win_id);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, type should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	pq_dev = pqdev->pq_devs[win_id];

	if (dbg_en) {
		_mtk_pq_get_aisr_dbg_mode(cmd, len, &dbg_mode);

		switch (dbg_mode) {
		case MTK_PQ_AISR_DBG_ENABLE:
			find = _mtk_pq_get_dbg_value_from_string(cmd, "aisr_en", len, &enable);
			if (!find) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"get enable fail, dbg_mode=scaling, find = %d!\n", find);
				goto exit;
			}
			_gAisrdbg[win_id].bAISREn = enable;

			if (_gAisrdbg[win_id].bAISREn) {
				find = _mtk_pq_get_dbg_value_from_string(cmd, "scale", len, &scale);
				if (!find) {
					_gAisrdbg[win_id].u32Scale = 1;
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"get scale fail, dbg_mode=scaling, find = %d!\n",
							find);
					goto exit;
				}
				_gAisrdbg[win_id].u32Scale = scale;
			}
			break;
		case MTK_PQ_AISR_DBG_ACTIVE_WIN:
			find = _mtk_pq_get_dbg_value_from_string(cmd, "active_win", len, &enable);
			if (!find) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"get enable fail, dbg_mode=active, find = %d!\n", find);
				goto exit;
			}
			_gAisrdbg[win_id].bAISREn = TRUE;
			_gAisrdbg[win_id].aisrActiveWin.bEnable = enable;
			if (!enable)
				goto exit;

			find &= _mtk_pq_get_dbg_value_from_string(cmd, "x", len, &x);
			find &= _mtk_pq_get_dbg_value_from_string(cmd, "y", len, &y);
			find &= _mtk_pq_get_dbg_value_from_string(cmd, "w", len, &width);
			find &= _mtk_pq_get_dbg_value_from_string(cmd, "h", len, &height);
			if (!find) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"cmd fail, dbg_mode=active, find = %d!\n", find);
				goto exit;
			}

			_gAisrdbg[win_id].aisrActiveWin.x = x;
			_gAisrdbg[win_id].aisrActiveWin.y = y;
			_gAisrdbg[win_id].aisrActiveWin.width = width;
			_gAisrdbg[win_id].aisrActiveWin.height = height;
			break;
		default:
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Not Support dbg mode=%d!\n", dbg_mode);
			ret = -EINVAL;
			goto exit;
		}
	}

	_gAisrdbg[win_id].bdbgModeEn = dbg_en;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "dbg_en=%u dbg_mode=%d\n", dbg_en, dbg_mode);

exit:
	PQ_FREE(cmd, (sizeof(char) * 0x1000));

	return ret;
}

int mtk_pq_pqu_loglevel_dbg_show(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	const __u16 u16MaxSize = DBG_STORE_BUF_SIZE;
	int count = 0;
	__u8 u8Idx = 0;
	__u16 u16RemainingSize = 0;
	char *pcCurrBufPos = NULL;
	int iRetSize = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}
	//initialization
	u16RemainingSize = u16MaxSize;
	pcCurrBufPos = buf;
	//cap info
	iRetSize = snprintf(pcCurrBufPos, u16RemainingSize,
		"pqu_log_level:0x%llx\n", _gu64PquLogLevel);
	if (iRetSize < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf Fail!\n");
		return -EINVAL;
	}
	count += iRetSize;
	u16RemainingSize -= iRetSize;
	pcCurrBufPos += iRetSize;
	//each win info
	for (u8Idx = 0; u8Idx < PQU_DBG_LOG_LEVEL_NUM; u8Idx++) {
		iRetSize = snprintf(pcCurrBufPos, u16RemainingSize, "%s\n", pcPQLogLabel[u8Idx]);
		if (iRetSize < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf Fail!\n");
			return -EINVAL;
		}
		count += iRetSize;
		u16RemainingSize -= iRetSize;
		pcCurrBufPos += iRetSize;
	}
	return count;
}

int mtk_pq_pqu_loglevel_dbg_store(struct device *dev, const char *buf)
{
	int ibufLen = 0;
	bool find = false;
	char *cmd = NULL;
	uint64_t  u64PquLogLevel = 0;
	__u16 u16CmdSize = 0;
	int iCheckRet = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}
	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}
	u16CmdSize = (sizeof(char) * DBG_STORE_BUF_SIZE);
	ibufLen = strlen(buf);
	if (ibufLen >= u16CmdSize) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ibufLen:%d over max size:%d\n", ibufLen, u16CmdSize);
		return -EINVAL;
	}
	PQ_MALLOC(cmd, u16CmdSize);
	if (!cmd) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQ_MALLOC cmd fail!\n");
		return -EINVAL;
	}
	memset(cmd, PQ_DISP_C_STRING_END, u16CmdSize);
	iCheckRet = snprintf(cmd, u16CmdSize, "%s", buf);//copy buf to cmd
	if (iCheckRet < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf fail!\n");
	find = _mtk_pq_get_dbg_value_from_string_ull(cmd, "log_level", ibufLen, &u64PquLogLevel);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(log_level), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	_gu64PquLogLevel = u64PquLogLevel;
	PQ_FREE(cmd, u16CmdSize);
	return iCheckRet;
}

int mtk_pq_srs_dbg_show(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	const __u16 u16MaxSize = DBG_STORE_BUF_SIZE;
	int count = 0;
	__u8 u8Idx = 0;
	__u16 u16RemainingSize = 0;
	char *pcCurrBufPos = NULL;
	int iRetSize = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}
	//initialization
	u16RemainingSize = u16MaxSize;
	pcCurrBufPos = buf;
	//each win info
	for (u8Idx = 0; u8Idx < PQ_WIN_MAX_NUM; u8Idx++) {
		iRetSize = snprintf(pcCurrBufPos, u16RemainingSize,
			"win:%u, dbg_enable:%d, srs_enable:%d, tgen_mode:%u, db_idx:%u, method:%u, bin_mode:%d, bin_db_idx:%u\n",
			u8Idx, gSrsDbg[u8Idx].dbg_enable, gSrsDbg[u8Idx].srs_enable,
			gSrsDbg[u8Idx].tgen_mode, gSrsDbg[u8Idx].db_idx, gSrsDbg[u8Idx].method,
			gSrsDbg[u8Idx].bin_mode, gSrsDbg[u8Idx].bin_db_idx);
		if (iRetSize < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf Fail!\n");
			return -EINVAL;
		}
		if (iRetSize > u16RemainingSize) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf full!\n");
			return -EINVAL;
		}
		count += iRetSize;
		u16RemainingSize -= iRetSize;
		pcCurrBufPos += iRetSize;
	}
	return count;
}

int mtk_pq_srs_dbg_store(struct device *dev, const char *buf)
{
	int ibufLen = 0;
	bool find = false;
	char *cmd = NULL;
	uint32_t win = 0;
	uint32_t dbg_enable = false;
	uint32_t srs_enable = false;
	uint32_t tgen_mode = 0;
	uint32_t db_idx = 0;
	uint32_t method = 0;
	uint32_t bin_mode = 0;
	uint32_t bin_db_idx = 0;
	__u16 u16CmdSize = 0;
	int iCheckRet = 0;

	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev is NULL!\n");
		return -EINVAL;
	}
	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf is NULL!\n");
		return -EINVAL;
	}
	u16CmdSize = (sizeof(char) * DBG_STORE_BUF_SIZE);
	ibufLen = strlen(buf);
	if (ibufLen >= u16CmdSize) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"ibufLen:%d over max size:%d\n", ibufLen, u16CmdSize);
		return -EINVAL;
	}
	PQ_MALLOC(cmd, u16CmdSize);
	if (!cmd) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "PQ_MALLOC cmd fail!\n");
		return -EINVAL;
	}
	memset(cmd, PQ_DISP_C_STRING_END, u16CmdSize);
	iCheckRet = snprintf(cmd, u16CmdSize, "%s", buf);//copy buf to cmd
	if (iCheckRet < 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "snprintf fail!\n");
	find = _mtk_pq_get_dbg_value_from_string(cmd, "win", ibufLen, &win);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(win), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "dbg_enable", ibufLen, &dbg_enable);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(dbg_enable), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "srs_enable", ibufLen, &srs_enable);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(srs_enable), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "tgen_mode", ibufLen, &tgen_mode);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(tgen_mode), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "db_idx", ibufLen, &db_idx);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(db_idx), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "method", ibufLen, &method);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(method), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "bin_mode", ibufLen, &bin_mode);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(bin_mode), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	find = _mtk_pq_get_dbg_value_from_string(cmd, "bin_db_idx", ibufLen, &bin_db_idx);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error(bin_db_idx), type should be set, please echo help!\n");
		PQ_FREE(cmd, u16CmdSize);
		return -EINVAL;
	}
	//update value.
	if (win < PQ_WIN_MAX_NUM) {
		gSrsDbg[win].dbg_enable = dbg_enable;
		gSrsDbg[win].srs_enable = srs_enable;
		gSrsDbg[win].tgen_mode = tgen_mode;
		gSrsDbg[win].db_idx = db_idx;
		gSrsDbg[win].method = method;
		gSrsDbg[win].bin_mode = bin_mode;
		gSrsDbg[win].bin_db_idx = bin_db_idx;
	} else
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"win:invalid(%lu)\n", (unsigned long)win);
	PQ_FREE(cmd, u16CmdSize);
	return iCheckRet;
}
