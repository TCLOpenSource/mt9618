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
#include <uapi/linux/mtk-v4l2-pq.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>

#include "mtk_pq.h"
#include "mtk_pq_pqd.h"
#include "mtk_pq_dv.h"
#include "pqu_msg.h"
#include "m_pqu_pq.h"
#include "mtk_pq_common.h"
#include "ext_command_video_if.h"
//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define UNUSED_PARA(x) (void)(x)
#define PQD_WIN_NUM (16)
//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
#define _READY_CB_COUNT 8
#define _CB_INC(x) ((x++)&(_READY_CB_COUNT-1))
#define _CB_DEC(x) (x == 0 ? 0 : --x)

static struct msg_pqmap_info _pqmap_msg_info;
static struct msg_pqmeta_info _pqmeta_msg_info;
static struct msg_pqparam_info _pqparam_msg_info[_READY_CB_COUNT];
static int _pqparam_count;

//-----------------------------------------------------------------------------
// LOCAL FUNCTION
//-----------------------------------------------------------------------------
static int _mtk_pqd_map_fd(int fd, void **va, u64 *size, struct dma_buf **db)
{
	int ret = 0;

	if ((size == NULL) || (fd == 0) || (va == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, fd=(%d), size is NULL?(%s), va is NULL?(%s)\n",
			fd, (size == NULL)?"TRUE":"FALSE", (va == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}

	(*db) = dma_buf_get(fd);
	if (IS_ERR_OR_NULL((*db))) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_get fail\n");
		return -EPERM;
	}

	*size = (*db)->size;

	*va = dma_buf_vmap((*db));

	if (IS_ERR_OR_NULL(*va)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_vmapfail\n");
		dma_buf_put((*db));
		return -EPERM;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "va=0x%llx, size=%llu\n", (__u64)*va, (__u64)*size);
	return ret;
}

static int _mtk_pqd_unmap_fd(int fd, void **va, struct dma_buf **db)
{
	if ((va == NULL) || (fd == 0) || (db == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, fd=(%d), va is NULL?(%s) db is NULL?(%s)\n",
			fd, (va == NULL)?"TRUE":"FALSE",
			(va == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "va=0x%llx\n", (__u64)*va);
	if (((*va) == NULL) || ((*db) == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"(((*va) == NULL)||((*db) == NULL)) no need unmap!\n");
		return -0;
	}
	dma_buf_vunmap((*db), (*va));
	dma_buf_put(*db);
	(*db) = NULL;
	(*va) = NULL;
	return 0;
}

//-----------------------------------------------------------------------------
//  FUNCTION
//-----------------------------------------------------------------------------
int mtk_pqd_LoadPQSetting(struct st_pqd_info *pqd_info,
	__s32 load_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(load_enable);

	return 0;
}

int mtk_pqd_SetPointToPoint(
	struct st_pqd_info *pqd_info, __s32 is_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(is_enable);

	return 0;
}

int mtk_pqd_LoadPTPTable(struct st_pqd_info *pqd_info,
	__s32 ptp_type)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ptp_type);

	return 0;
}

int mtk_pqd_ACE_3DClonePQMap(
	struct st_pqd_info *pqd_info, __s32 weave_type)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(weave_type);

	return 0;
}

int mtk_pqd_Set3DCloneforPIP(struct st_pqd_info *pqd_info,
	__s32 pip_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(pip_enable);

	return 0;
}

int mtk_pqd_Set3DCloneforLBL(struct st_pqd_info *pqd_info,
	__s32 lbl_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(lbl_enable);

	return 0;
}

int mtk_pqd_SetGameMode(struct st_pqd_info *pqd_info,
	__s32 game_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(game_enable);

	return 0;
}

int mtk_pqd_SetBOBMode(struct st_pqd_info *pqd_info,
	__s32 bob_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(bob_enable);

	return 0;
}

int mtk_pqd_SetBypassMode(struct st_pqd_info *pqd_info,
	__s32 bypass_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(bypass_enable);

	return 0;
}

int mtk_pqd_DemoCloneWindow(struct st_pqd_info *pqd_info,
	__s32 mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(mode);

	return 0;
}

int mtk_pqd_ReduceBWforOSD(struct st_pqd_info *pqd_info,
	__s32 osd_on)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(osd_on);

	return 0;
}

int mtk_pqd_EnableHDRMode(struct st_pqd_info *pqd_info,
	__s32 grule_level_index)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(grule_level_index);

	return 0;
}

int mtk_pqd_PQExit(struct st_pqd_info *pqd_info,
	__s32 exit_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(exit_enable);

	return 0;
}

int mtk_pqd_ACE_SetRBChannelRange(
	struct st_pqd_info *pqd_info, __s32 range_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(range_enable);

	return 0;
}

int mtk_pqd_SetFilmMode(struct st_pqd_info *pqd_info,
	__s32 film_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(film_mode);

	return 0;
}

int mtk_pqd_SetNRMode(struct st_pqd_info *pqd_info,
	__s32 nr_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(nr_mode);

	return 0;
}

int mtk_pqd_SetMPEGNRMode(
	struct st_pqd_info *pqd_info, __s32 mpeg_nr_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(mpeg_nr_mode);

	return 0;
}

int mtk_pqd_SetXvyccMode(struct st_pqd_info *pqd_info,
	__s32 xvycc_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(xvycc_mode);

	return 0;
}

int mtk_pqd_SetDLCMode(struct st_pqd_info *pqd_info,
	__s32 dlc_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(dlc_mode);

	return 0;
}

int mtk_pqd_SetSWDRMode(struct st_pqd_info *pqd_info,
	__s32 swdr_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(swdr_mode);

	return 0;
}

int mtk_pqd_SetDynamicContrastMode(
	struct st_pqd_info *pqd_info, __s32 contrast_type)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(contrast_type);

	return 0;
}

int mtk_pqd_ACE_SetContrast(struct st_pqd_info *pqd_info,
	__s32 contrast_value)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(contrast_value);

	return 0;
}

int mtk_pqd_ACE_SetBrightness(struct st_pqd_info *pqd_info,
	__s32 brightness_value)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(brightness_value);

	return 0;
}

int mtk_pqd_ACE_SetHue(struct st_pqd_info *pqd_info,
	__s32 hue_value)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(hue_value);

	return 0;
}

int mtk_pqd_ACE_SetSaturation(struct st_pqd_info *pqd_info,
	__s32 saturation_value)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(saturation_value);

	return 0;
}

int mtk_pqd_ACE_SetSharpness(struct st_pqd_info *pqd_info,
	__s32 sharpness_value)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(sharpness_value);

	return 0;
}


/////////////////////////////////////////set extern ctrl///////////////////////
int mtk_pqd_SetDbgLevel(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetPQInit(struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_MADIForceMotion(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetRFBLMode(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetGruleStatus(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetUCDMode(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetHSBBypass(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetVersion(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetHSYGrule(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetFBLStatus(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetIPHistogram(__u16 dev_indx, struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);

	return pqu_histogram_set_param(dev_indx, (struct m_pq_histogram_info *)(ctrl));
}

int mtk_pqd_SetNRSettings(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetDPUMode(struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetCCMInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetColorTuner(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetHSYSetting(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetHSYRange(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetManualLumaCurve(
	struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetStretchSettings(
	struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetPQCmd(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetPostColorTemp(
	struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_SetMWE(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetSWDRInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GenRGBPattern(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetBlueStretch(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_Set_H_NonlinearScaling(
	struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetMCDIMode(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetChromaInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetPQGamma(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetVideoDelayTime(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_DLCInit(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_SetDLCCaptureRange(
	struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

static void _mtk_pqd_ready_setPQMapInfo(void)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_pqmap_setup((void *)&_pqmap_msg_info);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_pqmap_setup fail! (ret=%d)\n", ret);
}

int mtk_pqd_resumeSetPQMapInfo(void)
{
	int ret = 0;

	if (_pqmap_msg_info.start_addr == 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "resume send pqmap va to pqu fail!\n");
		ret = -EINVAL;
		return ret;
	}

	ret = pqu_video_pqmap_setup((void *)&_pqmap_msg_info);
	if (ret == -ENODEV) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_pqmap_setup register ready cb!\n");
		ret = fw_ext_command_register_notify_ready_callback(0, _mtk_pqd_ready_setPQMapInfo);
		return ret;
	} else if (ret != 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"pqu_video_pqmap_setup fail (ret=%d)\n", ret);
	}

	return ret;
}

int mtk_pqd_SetPQMapInfo(struct mtk_pq_platform_device *pqdev,
	struct st_pqd_info *pqd_info, void *ctrl, void *pq_pa_addr,
	void *pq_va_addr, __u32 pq_bus_buf_size)
{
	struct v4l2_pqmap_info table;
	struct msg_pqmap_info msg_info;
	int ret = 0;

	if ((!pqd_info) || (!ctrl) || (!pqdev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "NULL parameters!\n");
		return -EFAULT;
	}

	memset(&table, 0, sizeof(struct v4l2_pqmap_info));
	memset(&msg_info, 0, sizeof(struct msg_pqmap_info));
	memcpy(&table, ctrl, sizeof(struct v4l2_pqmap_info));

	if ((table.u32MainPimLen == 0) && (table.u32MainRmLen == 0) &&
		(table.u32MainExPimLen == 0) && (table.u32MainExRmLen == 0)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "no pqmap here, no need to get va\n");
		return 0;
	}

	msg_info.start_addr = (__u64)pq_va_addr;
	msg_info.max_size = pq_bus_buf_size;
	msg_info.u32MainPimLen = table.u32MainPimLen;
	msg_info.u32MainRmLen = table.u32MainRmLen;
	msg_info.u32MainExPimLen = table.u32MainExPimLen;
	msg_info.u32MainExRmLen = table.u32MainExRmLen;
	u32PQMapBufLen = table.u32MainPimLen + table.u32MainRmLen
		+ table.u32MainExPimLen + table.u32MainExRmLen;

	if (bPquEnable) {
		msg_info.start_addr = (__u64)pq_pa_addr;
		memcpy(&_pqmap_msg_info, &msg_info, sizeof(struct msg_pqmap_info));
		ret = pqu_video_pqmap_setup((void *)&msg_info);
		if (ret == -ENODEV) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "send pqmap va to pqu timing fail\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
								_mtk_pqd_ready_setPQMapInfo);
			return ret;
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_pqmap_setup fail (ret=%d)\n", ret);
		}
	} else {
		if (pqdev->pqcaps.u32Qmap_Heap_Support != 0) {
			void *va = NULL;
			u64 size = 0;
			struct dma_buf *db = NULL;

			if (_mtk_pqd_map_fd(table.fd, &va, &size, &db) != 0) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pqd_map_fd fail\n");
				_mtk_pqd_unmap_fd(table.fd, &va, &db);
				return -EFAULT;
			}
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "start_addr:%llx, max_size:%llu\n",
				(__u64)va, size);
			msg_info.start_addr = (__u64)va;
			msg_info.max_size = size;
		}
		pqu_msg_send(PQU_MSG_SEND_PQMAP, (void *)&msg_info);
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "send pqmap va to pqu done\n");

	return 0;
}

int mtk_pqd_SetDIPMapInfo(struct mtk_pq_platform_device *pqdev,
	struct st_pqd_info *pqd_info, void *ctrl, void *pq_pa_addr,
	void *pq_va_addr, __u32 pq_bus_buf_size)
{
	struct v4l2_dipmap_info table;
	struct msg_dipmap_info msg_info;

	if ((!pqd_info) || (!ctrl))
		return -EFAULT;

	memset(&table, 0, sizeof(struct v4l2_dipmap_info));
	memset(&msg_info, 0, sizeof(struct msg_dipmap_info));
	memcpy(&table, ctrl, sizeof(struct v4l2_dipmap_info));

	if ((table.u32MainPimLen == 0) && (table.u32MainRmLen == 0) &&
		(table.u32MainExPimLen == 0) && (table.u32MainExRmLen == 0)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "no pqmap here, no need to get va\n");
		return -EFAULT;
	}

	msg_info.start_addr = (__u64)pq_va_addr;
	msg_info.max_size = pq_bus_buf_size;
	msg_info.u32MainPimLen = table.u32MainPimLen;
	msg_info.u32MainRmLen = table.u32MainRmLen;
	msg_info.u32MainExPimLen = table.u32MainExPimLen;
	msg_info.u32MainExRmLen = table.u32MainExRmLen;

	if (pqdev->pqcaps.u32Qmap_Heap_Support != 0) {
		void *va = NULL;
		u64 size = 0;
		struct dma_buf *db = NULL;

		if (_mtk_pqd_map_fd(table.fd, &va, &size, &db) != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pqd_map_fd fail\n");
			_mtk_pqd_unmap_fd(table.fd, &va, &db);
			return -EFAULT;
		}
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "start_addr:%llx, max_size:%llu\n",
			(__u64)va, size);
		msg_info.start_addr = (__u64)va;
		msg_info.max_size = size;
	}
	pqu_msg_send(PQU_MSG_SEND_DIPMAP, (void *)&msg_info);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "senddipmap va to pqu done\n");

	return 0;
}

static void _ready_cb_pqu_video_pqmeta_update(void)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_pqmeta_update((void *)&_pqmeta_msg_info);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_pqmeta_update fail!\n");
}

int mtk_pqd_SetPQMetaData(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	int ret = 0;
	struct v4l2_pq_metadata table;
	struct msg_pqmeta_info msg_info;
	static void *va[PQD_WIN_NUM] = {NULL};
	static u64 size[PQD_WIN_NUM] = {0};
	static struct dma_buf *db[PQD_WIN_NUM] = {NULL};
	uint8_t win_id = 0;
	static __u32 fd[PQD_WIN_NUM] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1};
	if ((!pqd_info) || (!ctrl))
		return -EFAULT;
	memset(&table, 0, sizeof(struct v4l2_pq_metadata));
	memset(&msg_info, 0, sizeof(struct msg_pqmeta_info));
	memcpy(&table, ctrl, sizeof(struct v4l2_pq_metadata));
	if (table.fd == -1) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "invalid fd\n");
		return 0;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "pqmeta fd = %d\n", table.fd);
	win_id = table.win_id;
	if (table.enable == true) {
		if (fd[win_id] != table.fd) {
			if ((db[win_id]) || (va[win_id])) {
				if (_mtk_pqd_unmap_fd(table.fd, &va[win_id], &db[win_id])) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"_mtk_pqd_unmap_fd fail\n");
					return -EFAULT;
				}
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "release fd[%u]:%d\n",
					win_id, fd[win_id]);
			}
			if (_mtk_pqd_map_fd(table.fd, &va[win_id],
				&size[win_id], &db[win_id]) != 0) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pqd_map_fd fail!\n");
				return -EFAULT;
			}
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "map fd[%u]:%d\n",
				win_id, fd[win_id]);
			fd[win_id] = table.fd;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "update fd[%u]:%d\n",
				win_id, fd[win_id]);
		}
		//ION Version, fix it after use pqmap pa
		msg_info.fd = table.fd;
		msg_info.meta_pa = (__u64)va[win_id];
		msg_info.meta_size = size[win_id];
		msg_info.win_id = table.win_id;
		//ION Version, fix it after use pqmap pa
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "va = %llx\n",
			msg_info.meta_pa);
		if (bPquEnable) {
			memcpy(&_pqmeta_msg_info, &msg_info, sizeof(struct msg_pqmeta_info));
			ret = pqu_video_pqmeta_update((void *)&msg_info);
			if (ret == -ENODEV) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"pqu_video_pqmeta_update register ready cb\n");
				ret = fw_ext_command_register_notify_ready_callback(0,
					_ready_cb_pqu_video_pqmeta_update);
			} else if (ret != 0) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"pqu_video_pqmeta_update fail (ret=%d)\n", ret);
			}
		}
		else
			pqu_msg_send(PQU_MSG_SEND_PQMETA, (void *)&msg_info);
	} else {
		if (_mtk_pqd_unmap_fd(table.fd, &va[win_id], &db[win_id]) != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pqd_unmap_fd fail!\n");
			return -EFAULT;
		}
		fd[win_id] = -1;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "release fd[%u]:%d\n", win_id, fd[win_id]);
	}
	return 0;
}

static void _ready_cb_pqu_video_pqparam_update(void)
{
	int ret = 0;
	struct callback_info pqparam_callback_info = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_pqparam_update((void *)&_pqparam_msg_info[_CB_DEC(_pqparam_count)],
		&pqparam_callback_info);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_pqparam_update fail!\n");
}

int _mtk_pqd_SetLocalHSY(struct meta_buffer *meta_buf)
{
	static struct meta_pq_localhsy stLocalhsyPqhal;
	static struct m_pq_localhsy stLocalhsyPqu;
	int ret = 0;

	if (!meta_buf) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid meta_buf input\n");
		return -EFAULT;
	}

	memset(&stLocalhsyPqhal, 0, sizeof(struct meta_pq_localhsy));
	memset(&stLocalhsyPqu, 0, sizeof(struct m_pq_localhsy));

	/* read metadata written by pqhal */
	ret = mtk_pq_common_read_metadata_addr(meta_buf,
		EN_PQ_METATAG_LOCALHSY_TAG, &stLocalhsyPqhal);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "Local HSY don't need to update\n");
		return 0;
	}

	memcpy(&stLocalhsyPqu.s8HuebyHueLut, &stLocalhsyPqhal.s8HuebyHueLut,
		sizeof(stLocalhsyPqu.s8HuebyHueLut));
	memcpy(&stLocalhsyPqu.u16HuebySatLut, &stLocalhsyPqhal.u16HuebySatLut,
		sizeof(stLocalhsyPqu.u16HuebySatLut));
	memcpy(&stLocalhsyPqu.u16HuebyLumaLut, &stLocalhsyPqhal.u16HuebyLumaLut,
		sizeof(stLocalhsyPqu.u16HuebyLumaLut));
	memcpy(&stLocalhsyPqu.u16SatbyHueCurrentLut, &stLocalhsyPqhal.u16SatbyHueCurrentLut,
		sizeof(stLocalhsyPqu.u16SatbyHueCurrentLut));
	memcpy(&stLocalhsyPqu.u16SatbyHueMinLut, &stLocalhsyPqhal.u16SatbyHueMinLut,
		sizeof(stLocalhsyPqu.u16SatbyHueMinLut));
	memcpy(&stLocalhsyPqu.u8SatbyLumaLut, &stLocalhsyPqhal.u8SatbyLumaLut,
		sizeof(stLocalhsyPqu.u8SatbyLumaLut));
	memcpy(&stLocalhsyPqu.s16LumabySatLut, &stLocalhsyPqhal.s16LumabySatLut,
		sizeof(stLocalhsyPqu.s16LumabySatLut));
	memcpy(&stLocalhsyPqu.s16LumabyLumaLut, &stLocalhsyPqhal.s16LumabyLumaLut,
		sizeof(stLocalhsyPqu.s16LumabyLumaLut));

	/* write metadata for pqu read */
	ret = mtk_pq_common_write_metadata_addr(meta_buf,
		EN_PQ_METATAG_PQU_LOCALHSY_TAG, &stLocalhsyPqu);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			" write EN_PQ_METATAG_PQU_LOCALHSY_TAG Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

int _mtk_pqd_SetPQParam(struct meta_buffer *meta_buf)
{
	static struct meta_pq_pqparam stPqparamPqhal;
	static struct m_pq_pqparam_meta stPqparamPqu;
	int ret = 0;

	if (!meta_buf) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid meta_buf input\n");
		return -EFAULT;
	}

	memset(&stPqparamPqhal, 0, sizeof(struct meta_pq_pqparam));
	memset(&stPqparamPqu, 0, sizeof(struct m_pq_pqparam_meta));

	/* read metadata written by pqhal */
	ret = mtk_pq_common_read_metadata_addr(meta_buf,
		EN_PQ_METATAG_PQPARAM_TAG, &stPqparamPqhal);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "va:%#llx, size:%x\n",
			(__u64)meta_buf->paddr, meta_buf->size);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			" read EN_PQ_METATAG_PQPARAM_TAG Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	memcpy(&stPqparamPqu.data, &stPqparamPqhal.data, sizeof(stPqparamPqu.data));

	/* write metadata for pqu read */
	ret = mtk_pq_common_write_metadata_addr(meta_buf,
		EN_PQ_METATAG_PQU_PQPARAM_TAG, &stPqparamPqu);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			" write EN_PQ_METATAG_PQU_PQPARAM_TAG Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

int _mtk_pqd_SetLocalVAC(struct meta_buffer *meta_buf)
{
	static struct meta_pq_localvac stLocalvacPqhal;
	static struct m_pq_localvac stLocalvacPqu;
	int ret = 0;

	if (!meta_buf) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid meta_buf input\n");
		return -EFAULT;
	}

	memset(&stLocalvacPqhal, 0, sizeof(struct meta_pq_localvac));
	memset(&stLocalvacPqu, 0, sizeof(struct m_pq_localvac));

	/* read metadata written by pqhal */
	ret = mtk_pq_common_read_metadata_addr(meta_buf,
		EN_PQ_METATAG_LOCALVAC_TAG, &stLocalvacPqhal);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "Local HSY don't need to update\n");
		return 0;
	}
	switch (stLocalvacPqhal.type) {
	case M_PQ_VAC_POS_RANGE_1024:
		stLocalvacPqu.type = PQU_VAC_POS_RANGE_1024;
		break;
	case M_PQ_VAC_POS_RANGE_2048:
		stLocalvacPqu.type = PQU_VAC_POS_RANGE_2048;
		break;
	case M_PQ_VAC_POS_RANGE_4096:
		stLocalvacPqu.type = PQU_VAC_POS_RANGE_4096;
		break;
	case M_PQ_VAC_POS_RANGE_8192:
		stLocalvacPqu.type = PQU_VAC_POS_RANGE_8192;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"stLocalvacPqhal.type undefine %d\n", stLocalvacPqhal.type);
		return -EFAULT;
	}

	memcpy(&stLocalvacPqu.en, &stLocalvacPqhal.en,
		sizeof(stLocalvacPqu.en));
	memcpy(&stLocalvacPqu.u8SatbyHueLut, &stLocalvacPqhal.u8SatbyHueLut,
		sizeof(stLocalvacPqu.u8SatbyHueLut));
	memcpy(&stLocalvacPqu.u8SatbyPosLut, &stLocalvacPqhal.u8SatbyPosLut,
		sizeof(stLocalvacPqu.u8SatbyPosLut));

	/* write metadata for pqu read */
	ret = mtk_pq_common_write_metadata_addr(meta_buf,
		EN_PQ_METATAG_PQU_LOCALVAC_TAG, &stLocalvacPqu);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			" write EN_PQ_METATAG_PQU_LOCALVAC_TAG Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

int mtk_pqd_SetPQParam(struct mtk_pq_device *pq_dev, struct st_pqd_info *pqd_info, void *ctrl,
	void *pq_va_addr, void *pq_pa_addr, __u32 pq_bus_buf_size)
{
	struct callback_info pqparam_callback_info = {0};
	struct v4l2_pq_pqparam table;
	struct msg_pqparam_info msg_info;
	struct meta_buffer meta_buf;
	int ret = 0;

	if ((!pqd_info) || (!ctrl) || (!pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid input\n");
		return -EFAULT;
	}

	memset(&table, 0, sizeof(struct v4l2_pq_pqparam));
	memset(&msg_info, 0, sizeof(struct msg_pqparam_info));
	memcpy(&table, ctrl, sizeof(struct v4l2_pq_pqparam));
	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	meta_buf.paddr = (unsigned char *) pq_va_addr;
	meta_buf.size = (unsigned int) pq_bus_buf_size;

	ret = _mtk_pqd_SetPQParam(&meta_buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pqd_SetPQParam Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	ret = _mtk_pqd_SetLocalHSY(&meta_buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pqd_SetLocalHSY Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	ret = _mtk_pqd_SetLocalVAC(&meta_buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pqd_SetLocalVAC Failed, ret = %d\n", ret);
		return -EFAULT;
	}

	msg_info.win_id = (__u8)table.win_id;
	msg_info.va = (__u64)pq_va_addr;
	msg_info.pa = (__u64)pq_pa_addr;
	msg_info.da = (__u64)pq_pa_addr;
	msg_info.size = (__u64)pq_bus_buf_size;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "[PQDD][PQPARAM][%d][%#llx][%#llx][%#llx]\n",
		table.win_id, msg_info.va, msg_info.pa, msg_info.size);

	if (bPquEnable) {
		ret = pqu_video_pqparam_update(&msg_info, &pqparam_callback_info);
		if (ret == -ENODEV) {
			memcpy(&_pqparam_msg_info[_CB_INC(_pqparam_count)],
				&msg_info,
				sizeof(struct msg_pqparam_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_pqparam_update register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_video_pqparam_update);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_pqparam_update fail (ret=%d)\n", ret);
		}
	}
	else
		pqu_msg_send(PQU_MSG_SEND_PQPARAM, (void *)&msg_info);
	//STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, " win_id:%ld, va:0x%lx, size:%ld\n",
	//	msg_info.win_id, msg_info.va, msg_info.size);
	//STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, " send pqparam info to pqu done\n");
	return 0;
}

//////////////////////////////////////////////////////get ctrl/////////////////

int mtk_pqd_GetPointToPoint(
	struct st_pqd_info *pqd_info, __s32 *ptp_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ptp_enable);

	return 0;
}

int mtk_pqd_GetGameModeStatus(
	struct st_pqd_info *pqd_info, __s32 *game_enable)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(game_enable);

	return 0;
}


int mtk_pqd_GetDualViewState(
	struct st_pqd_info *pqd_info, __s32 *dual_view_state)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(dual_view_state);

	return 0;
}

int mtk_pqd_GetZeroFrameMode(struct st_pqd_info *pqd_info,
	__s32 *zero_frame_mode)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(zero_frame_mode);

	return 0;
}

//////////////////////////////////////////////////////get extern ctrl//////////

int mtk_pqd_GetRFBLMode(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetDPUStatus(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ReadPQCmd(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetSWDRInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetVideoDelayTime(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetPQCaps(struct st_pqd_info *pqd_info,
	void *ctrl,
	struct mtk_pq_platform_device *pqdev)
{
	struct v4l2_pq_cap_info *pq_cap_info = NULL;
	bool pr_support = FALSE;

	UNUSED_PARA(pqd_info);

	if (ctrl == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	if (pqdev == NULL) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	mtk_pq_dv_get_pr_cap(pqdev, &pr_support);

	pq_cap_info = (struct v4l2_pq_cap_info *)ctrl;
	pq_cap_info->bDvPD_Supported = pr_support;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "get PD support = %d\n", pq_cap_info->bDvPD_Supported);

	return 0;
}

int mtk_pqd_GetPQVersion(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetHSYGrule(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetFBLStatus(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_GetIPHistogram(__u16 dev_indx, struct st_pqd_info *pqd_info, void *ctrl)
{
	UNUSED_PARA(pqd_info);

	return pqu_histogram_get_result(dev_indx, (struct m_pq_histogram_info *)ctrl);
}

int mtk_pqd_ACE_GetHSYSetting(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_GetHSYRange(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_GetLumaInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_ACE_GetChromaInfo(struct st_pqd_info *pqd_info,
	void *ctrl)
{
	UNUSED_PARA(pqd_info);
	UNUSED_PARA(ctrl);

	return 0;
}

int mtk_pqd_Open(struct st_pqd_info *pqd_info)
{
	if (!pqd_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD open\n");

	return 0;
}

int mtk_pqd_Close(struct st_pqd_info *pqd_info)
{
	if (!pqd_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD close\n");
	//todo
	return 0;
}

int mtk_pqd_SetInputSource(struct st_pqd_info *pqd_info,
	__u8 u8Input)
{
	if (!pqd_info)
		return -EFAULT;

	pqd_info->pqd_input_source = u8Input;
	return 0;
}

int mtk_pqd_SetPixFmtIn(struct st_pqd_info *pqd_info,
	struct v4l2_pix_format_mplane *pix)
{
	if ((!pqd_info) || (!pix))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD pix format in\n");

	//todo
	return 0;
}

int mtk_pqd_SetPixFmtOut(struct st_pqd_info *pqd_info,
	struct v4l2_pix_format_mplane *pix)
{
	if ((!pqd_info) || (!pix))
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD pix format out\n");

	//todo
	return 0;
}


// stream on
int mtk_pqd_StreamOn(struct st_pqd_info *pqd_info)
{
	if (!pqd_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD pix stream on\n");

	//todo
	return 0;
}

// stream off
int mtk_pqd_StreamOff(struct st_pqd_info *pqd_info)
{
	if (!pqd_info)
		return -EFAULT;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ENHANCE, "PQD stream off\n");

	//todo
	return 0;
}

