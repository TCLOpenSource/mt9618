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
#include <linux/timekeeping.h>
#include <linux/genalloc.h>
#include <linux/list.h>

#include <linux/dma-resv.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence.h>

#include <linux/io.h>

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include "pqu_msg.h"
#include "m_pqu_pq.h"
#include "ext_command_video_if.h"
#include "ext_command_ucd_if.h"

#include "mtk_pq.h"
#include "mtk_pq_enhance.h"
#include "mtk_pq_common.h"
#include "mtk_pq_b2r.h"
#include "mtk_pq_display.h"
#include "mtk_pq_pattern.h"
#include "mtk_pq_common_irq.h"
#include "mtk_pq_atrace.h"
#include "mtk_pq_slice.h"
#include "mtk_pq_svp.h"
#include "mtk_pq_buffer.h"

#include "apiXC.h"
#include "drvPQ.h"
#include "mtk_iommu_dtv_api.h"

#include "hwreg_common.h"
#include "hwreg_pq_common_trigger_gen.h"
#include "hwreg_pq_display_pattern.h"
#include <uapi/linux/mtk-v4l2-pq.h>
#include "mi_realtime.h"

#define REG_SIZE	(10)	// TODO: need to set from DD (source: DTS)
#define PQU_TIME_OUT_MS		(10000)
#define ALLOC_ORDER		 (10)
#define NODE_ID			 (-1)
#define TRIG_SW_HTT			(200)
#define TRIG_VCNT_UPD_MSK_RNG		(2)
#define TRIG_VS_DLY_H			(0x1)
#define TRIG_VS_DLY_V			(0x6)
#define TRIG_DMA_R_DLY_H		(0x1)
#define TRIG_DMA_R_DLY_V		(0x7)
#define TRIG_STR_DLY_V			(0x7)
#define PATTERN_REG_NUM_MAX		(400)// 3 * 127 + 6 + 2 = 389
#define PTN_H_SIZE	(64)
#define PTN_V_SIZE	(32)
#define PQU_SHUTTLE_FRAME_SIZE  (sizeof(struct msg_shuttle_frame_info))
#define PQU_SHUTTLE_FRAME_NUM   (32) // 16(win) * 2(in/out)
#define PQU_SHUTTLE_SIZE        (PQU_SHUTTLE_FRAME_SIZE * PQU_SHUTTLE_FRAME_NUM)
#define PQU_SHUTTLE_NUM         (32)
#define PQU_SHTTTLE_MEM_POOL    (PQU_SHUTTLE_SIZE * PQU_SHUTTLE_NUM)
#define _READY_CB_COUNT 8
#define _CB_INC(x) ((x++)&(_READY_CB_COUNT-1))
#define _CB_DEC(x) (x == 0 ? 0 : --x)
#define UCD_ALGO_ENGINE_R2		(1)
#define UCD_DEFAULT_VALUE		(0x00)

#define PQU_CFD_MAPPING_HDR_NUM (8)
#define PQU_CFD_MAPPING_WIN_CURVE_NUM (32)
#define PQU_CFD_WIN_NUM         (16)
#define PQU_U32_MAX_NUM         (0xFFFF)

#define STREAM_ON_ONE_SHOT (2)
#define THERMAL_POLL_TIME (42)

static struct gen_pool *pool;
static struct m_pq_common_debug_info g_stream_debug_info;
static bool isEnableRv55PquCurrent;
static struct msg_config_info _update_config_msg_info[_READY_CB_COUNT];
static struct msg_new_win_info _new_win_msg_info[_READY_CB_COUNT];
static struct msg_stream_on_info _stream_on_msg_info[_READY_CB_COUNT];
static struct msg_stream_off_info _stream_off_msg_info[_READY_CB_COUNT];
static int _update_config_count;
static int _new_win_count;
static int _stream_on_count;
static int _stream_off_count;
static LIST_HEAD(_frame_metadata_list);
static spinlock_t _frame_metadata_info_lock;
static LIST_HEAD(_stream_list);
static spinlock_t _stream_lock;
static bool bUcdConfigInit;
static uint32_t _get_cfd_curve_id[PQU_CFD_WIN_NUM];
static uint32_t _get_cfd_win_id;
static struct mtk_pq_thermal _pqu_thermal_info[PQ_WIN_MAX_NUM];

static void mtk_pq_common_dqbuf_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param);

static void mtk_pq_common_dovi_frame_meta_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param);

static void mtk_pq_common_stream_off_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param);

static void mtk_pq_common_bbd_cb(
	int error_cause,
	pid_t thread_id,
	uint32_t user_param,
	void *param);

static void mtk_pq_common_hdr_change_cb(
	int error_cause,
	pid_t thread_id,
	uint32_t user_param,
	void *param);

static int _mtk_pq_common_create_share_memory_pool(unsigned long addr, size_t size);
static int _mtk_pq_common_alloc_share_memory(unsigned long *paddr, size_t size);
static int _mtk_pq_common_get_frame(struct msg_queue_info *msg_info);
static void *_mtk_pq_common_find_frame_metadata_va(unsigned long pa);
static void _mtk_pq_common_release_frame_metadata(unsigned long pa);
static void _mtk_pq_common_release_all_frame(void);
static int _mtk_pq_common_add_stream_info(struct msg_stream_on_info *msg_info);
static int _mtk_pq_common_get_stream_info(struct msg_stream_off_done_info *msg_info);
static void _mtk_pq_common_release_stream(uint8_t win_id, uint8_t stream_type);


enum Ucd_command_list {
	UCD_CMD_INIT_MODE = 0,
	UCD_CMD_MAX,
};

struct mtk_pq_common_wait_qslice_cb {
	struct dma_fence_cb base;
	struct msg_queue_slice_info slice_info;
	struct mtk_pq_device *pq_dev;
	struct mtk_pq_buffer *pq_src_buf;
	struct mtk_pq_buffer *pq_dst_buf;
};

struct mtk_pq_common_metadata_node {
	unsigned long physical_addr;
	void *virtual_addr;
	size_t size;
	bool used;
	struct msg_queue_info queue_info;
	struct list_head list;
};

struct mtk_pq_common_dequeue_node {
	struct msg_dqueue_info dq_frame;
	struct list_head list;
};

struct mtk_pq_common_stream_node {
	struct msg_stream_on_info stream_info;
	struct list_head list;
};


static const int mtk_pq_cfd_mapping_hdr_mode[PQU_CFD_MAPPING_HDR_NUM] = {
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_SDR_TAG,
	0,
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HDR_TAG,
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HLG_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_SDR_TAG,
	0,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HDR_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HLG_TAG
};

static const int mtk_pq_cfd_mapping_win_curve[PQU_CFD_MAPPING_WIN_CURVE_NUM] = {
	EN_PQ_METATAG_CFD_SHM_WIN0_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN1_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN2_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN3_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN4_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN5_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN6_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN7_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN8_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN9_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN10_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN11_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN12_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN13_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN14_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN15_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN0_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN1_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN2_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN3_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN4_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN5_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN6_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN7_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN8_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN9_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN10_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN11_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN12_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN13_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN14_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN15_CURVE_TAG
};

static int _mtk_pq_common_check_meta_ret(int ret, void *ptr)
{
	if (ret || ptr == NULL)
		return -1;
	else
		return 0;
}

static __u64 _mtk_pq_common_get_ms_time(void)
{
	struct timespec ts;

	getrawmonotonic(&ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int _mtk_pq_common_map_fd(int fd, void **va, u64 *size)
{
	int ret = 0;
	struct dma_buf *db = NULL;

	if (size == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, fd=(%d), size is NULL?(%s)\n",
			fd, (size == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}

	db = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(db)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_get fail!\n");
		return -EPERM;
	}

	*size = db->size;

	*va = dma_buf_vmap(db);

	if (IS_ERR_OR_NULL(*va)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_vmap fail\n");
		dma_buf_put(db);
		return -EPERM;
	}

	dma_buf_put(db);

	return ret;
}

static int _mtk_pq_common_map_db(struct dma_buf *db, void **va, u64 *size)
{
	int ret = 0;

	if ((size == NULL) || (IS_ERR_OR_NULL(db))) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid ptr, db or size\n");
		return -EINVAL;
	}

	*size = db->size;

	*va = dma_buf_vmap(db);
	if (IS_ERR_OR_NULL(*va)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_vmap fail\n");
		return -EPERM;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "size=%llu\n", *size);
	return ret;
}

static int _mtk_pq_common_unmap_fd(int fd, void *va)
{
	struct dma_buf *db = NULL;

	if (va == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, fd=(%d), va is NULL?(%s)\n",
			fd, (va == NULL)?"TRUE":"FALSE");
		return -EPERM;
	}

	db = dma_buf_get(fd);
	if (db == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_get fail\n");
		return -1;
	}

	dma_buf_vunmap(db, va);
	dma_buf_put(db);
	return 0;
}

static int _mtk_pq_common_unmap_db(struct dma_buf *db, void *va)
{
	if ((va == NULL) || (IS_ERR_OR_NULL(db))) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid ptr, db or va\n");
		return -EINVAL;
	}

	dma_buf_vunmap(db, va);

	return 0;
}

static enum pqu_idr_input_path _mtk_pq_common_set_pqu_idr_input_path_meta(
					enum meta_pq_idr_input_path pq_idr_input_path)
{
	switch (pq_idr_input_path) {
	case META_PQ_PATH_IPDMA_0:
		return PQU_PATH_IPDMA_0;
	case META_PQ_PATH_IPDMA_1:
		return PQU_PATH_IPDMA_1;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "unknown enum :%d\n", pq_idr_input_path);
	}
	return 0;
}

static enum meta_pq_idr_input_path _mtk_pq_common_set_pq_idr_input_path_meta(
					enum pqu_idr_input_path input_path)
{
	switch (input_path) {
	case PQU_PATH_IPDMA_0:
		return META_PQ_PATH_IPDMA_0;
	case PQU_PATH_IPDMA_1:
		return META_PQ_PATH_IPDMA_1;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "unknown enum :%d\n", input_path);
	}
	return 0;
}

static int _mtk_pq_common_get_metaheader(
	enum mtk_pq_common_metatag meta_tag,
	struct meta_header *meta_header)
{
	int ret = 0;

	if (meta_header == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_header == NULL\n");
		return -EPERM;
	}

	switch (meta_tag) {
	case EN_PQ_METATAG_VDEC_SVP_INFO:
#if IS_ENABLED(CONFIG_OPTEE)
		meta_header->size = sizeof(struct vdec_dd_svp_info);
		meta_header->ver = 0;
		meta_header->tag = MTK_VDEC_DD_SVP_INFO_TAG;
#endif
		break;
	case EN_PQ_METATAG_SRCCAP_SVP_INFO:
#if IS_ENABLED(CONFIG_OPTEE)
		meta_header->size = sizeof(struct meta_srccap_svp_info);
		meta_header->ver = META_SRCCAP_SVP_INFO_VERSION;
		meta_header->tag = META_SRCCAP_SVP_INFO_TAG;
#endif
		break;
	case EN_PQ_METATAG_SVP_INFO:
		meta_header->size = sizeof(struct meta_pq_disp_svp);
		meta_header->ver = META_PQ_DISP_SVP_VERSION;
		meta_header->tag = PQ_DISP_SVP_META_TAG;
		break;
	case EN_PQ_METATAG_SH_FRM_INFO:
		meta_header->size = sizeof(struct mtk_pq_frame_info);
		meta_header->ver = 0;
		meta_header->tag = MTK_PQ_SH_FRM_INFO_TAG;
		break;
	case EN_PQ_METATAG_DISP_MDW_CTRL:
		meta_header->size = sizeof(struct m_pq_display_mdw_ctrl);
		meta_header->ver = M_PQ_DISPLAY_MDW_CTRL_VERSION;
		meta_header->tag = M_PQ_DISPLAY_MDW_CTRL_META_TAG;
		break;
	case EN_PQ_METATAG_DISP_IDR_CTRL:
		meta_header->size = sizeof(struct m_pq_display_idr_ctrl);
		meta_header->ver = M_PQ_DISPLAY_IDR_CTRL_VERSION;
		meta_header->tag = M_PQ_DISPLAY_IDR_CTRL_META_TAG;
		break;
	case EN_PQ_METATAG_DISP_B2R_CTRL:
		meta_header->size = sizeof(struct m_pq_display_b2r_ctrl);
		meta_header->ver = M_PQ_DISPLAY_B2R_CTRL_VERSION;
		meta_header->tag = M_PQ_DISPLAY_B2R_CTRL_META_TAG;
		break;
	case EN_PQ_METATAG_DISP_B2R_FILM_GRAIN:
		meta_header->size = sizeof(struct m_pq_film_grain_desc);
		meta_header->ver = M_PQ_FILM_GRAIN_VERSION;
		meta_header->tag = M_PQ_B2R_FILM_GRAIN_TAG;
		break;
	case EN_PQ_METATAG_PQU_DISP_FLOW_INFO:
		meta_header->size = sizeof(struct m_pq_display_flow_ctrl);
		meta_header->ver = M_PQ_DISPLAY_FLOW_CTRL_VERSION;
		meta_header->tag = M_PQ_DISPLAY_FLOW_CTRL_META_TAG;
		break;
	case EN_PQ_METATAG_PQU_DISP_WM_INFO:
		meta_header->size = sizeof(struct meta_pq_display_wm_info);
		meta_header->ver = M_PQ_DISPLAY_WM_VERSION;
		meta_header->tag = M_PQ_DISPLAY_WM_META_TAG;
		break;
	case EN_PQ_METATAG_OUTPUT_FRAME_INFO:
		meta_header->size = sizeof(struct meta_pq_output_frame_info);
		meta_header->ver = META_PQ_OUTPUT_FRAME_INFO_VERSION;
		meta_header->tag = META_PQ_OUTPUT_FRAME_INFO_TAG;
		break;
	case EN_PQ_METATAG_SRCCAP_FRAME_INFO:
		meta_header->size = sizeof(struct meta_srccap_frame_info);
		meta_header->ver = META_SRCCAP_FRAME_INFO_VERSION;
		meta_header->tag = META_SRCCAP_FRAME_INFO_TAG;
		break;
	case EN_PQ_METATAG_STREAM_INTERNAL_INFO:
		meta_header->size = sizeof(struct meta_pq_stream_internal_info);
		meta_header->ver = META_PQ_STREAM_INTERNAL_INFO_VERSION;
		meta_header->tag = META_PQ_STREAM_INTERNAL_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_DEBUG_INFO:
		meta_header->size = sizeof(struct m_pq_common_debug_info);
		meta_header->ver = M_PQ_COMMON_DEBUG_INFO_VERSION;
		meta_header->tag = M_PQ_COMMON_DEBUG_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_STREAM_INFO:
		meta_header->size = sizeof(struct m_pq_common_stream_info);
		meta_header->ver = M_PQ_COMMON_STREAM_INFO_VERSION;
		meta_header->tag = M_PQ_COMMON_STREAM_INFO_TAG;
		break;
	case EN_PQ_METATAG_STREAM_INFO:
		meta_header->size = sizeof(struct meta_pq_stream_info);
		meta_header->ver = META_PQ_STREAM_INFO_VERSION;
		meta_header->tag = PQ_DISP_STREAM_META_TAG;
		break;
	case EN_PQ_METATAG_PQU_PATTERN_INFO:
		meta_header->size = sizeof(struct pq_pattern_info);
		meta_header->ver = M_PQ_PATTERN_INFO_VERSION;
		meta_header->tag = M_PQ_PATTERN_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_PATTERN_SIZE_INFO:
		meta_header->size = sizeof(struct pq_pattern_size_info);
		meta_header->ver = M_PQ_PATTERN_SIZE_INFO_VERSION;
		meta_header->tag = M_PQ_PATTERN_SIZE_INFO_TAG;
		break;
	case EN_PQ_METATAG_BBD_INFO:
		meta_header->size = sizeof(struct meta_pq_bbd_info);
		meta_header->ver = META_PQ_BBD_INFO_VERSION;
		meta_header->tag = META_PQ_BBD_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_BBD_INFO:
		meta_header->size = sizeof(struct m_pq_bbd_info);
		meta_header->ver = M_PQ_BBD_INFO_VERSION;
		meta_header->tag = M_PQ_BBD_INFO_TAG;
		break;
	case EN_PQ_METATAG_DISPLAY_FLOW_INFO:
		meta_header->size = sizeof(struct meta_pq_display_flow_info);
		meta_header->ver = META_PQ_DISPLAY_FLOW_INFO_VERSION;
		meta_header->tag = META_PQ_DISPLAY_FLOW_INFO_TAG;
		break;
	case EN_PQ_METATAG_DISPLAY_WM_INFO:
		meta_header->size = sizeof(struct meta_pq_display_wm_info);
		meta_header->ver = META_PQ_DISPLAY_WM_INFO_VERSION;
		meta_header->tag = META_PQ_DISPLAY_WM_INFO_TAG;
		break;
	case EN_PQ_METATAG_INPUT_QUEUE_EXT_INFO:
		meta_header->size = sizeof(struct meta_pq_input_queue_ext_info);
		meta_header->ver = META_PQ_INPUT_QUEUE_EXT_INFO_VERSION;
		meta_header->tag = META_PQ_INPUT_QUEUE_EXT_INFO_TAG;
		break;
	case EN_PQ_METATAG_OUTPUT_QUEUE_EXT_INFO:
		meta_header->size = sizeof(struct meta_pq_output_queue_ext_info);
		meta_header->ver = META_PQ_OUTPUT_QUEUE_EXT_INFO_VERSION;
		meta_header->tag = META_PQ_OUTPUT_QUEUE_EXT_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_QUEUE_EXT_INFO:
		meta_header->size = sizeof(struct m_pq_queue_ext_info);
		meta_header->ver = M_PQ_QUEUE_EXT_INFO_VERSION;
		meta_header->tag = M_PQ_QUEUE_EXT_INFO_TAG;
		break;
	case EN_PQ_METATAG_FORCEP_INFO:
		meta_header->size = sizeof(struct meta_pq_forcep_info);
		meta_header->ver = META_PQ_FORCEP_INFO_VERSION;
		meta_header->tag = META_PQ_FORCEP_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_FORCEP_INFO:
		meta_header->size = sizeof(struct m_pq_forcep_info);
		meta_header->ver = M_PQ_FORCEP_INFO_VERSION;
		meta_header->tag = M_PQ_FORCEP_INFO_TAG;
		break;
	case EN_PQ_METATAG_OUTPUT_DISP_IDR_CTRL:
		meta_header->size = sizeof(struct meta_pq_display_idr_ctrl);
		meta_header->ver = META_PQ_DISPLAY_IDR_CTRL_VERSION;
		meta_header->tag = META_PQ_DISPLAY_IDR_CTRL_META_TAG;
		break;
	case EN_PQ_METATAG_DV_DEBUG:
		meta_header->size = sizeof(struct m_pq_dv_debug);
		meta_header->ver = M_PQ_DV_DEBUG_INFO_VERSION;
		meta_header->tag = M_PQ_DV_DEBUG_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQPARAM_TAG:
		meta_header->size = sizeof(struct meta_pq_pqparam);
		meta_header->ver = META_PQ_PQPARAM_VERSION;
		meta_header->tag = META_PQ_PQPARAM_TAG;
		break;
	case EN_PQ_METATAG_PQU_PQPARAM_TAG:
		meta_header->size = sizeof(struct m_pq_pqparam_meta);
		meta_header->ver = M_PQ_PQPARAM_VERSION;
		meta_header->tag = M_PQ_PQPARAM_TAG;
		break;
	case EN_PQ_METATAG_LOCALHSY_TAG:
		meta_header->size = sizeof(struct meta_pq_localhsy);
		meta_header->ver = META_PQ_LOCALHSY_VERSION;
		meta_header->tag = META_PQ_LOCALHSY_TAG;
		break;
	case EN_PQ_METATAG_PQU_LOCALHSY_TAG:
		meta_header->size = sizeof(struct m_pq_localhsy);
		meta_header->ver = M_PQ_LOCALHSY_VERSION;
		meta_header->tag = M_PQ_LOCALHSY_TAG;
		break;
	case EN_PQ_METATAG_LOCALVAC_TAG:
		meta_header->size = sizeof(struct meta_pq_localvac);
		meta_header->ver = META_PQ_LOCALVAC_VERSION;
		meta_header->tag = META_PQ_LOCALVAC_TAG;
		break;
	case EN_PQ_METATAG_PQU_LOCALVAC_TAG:
		meta_header->size = sizeof(struct m_pq_localvac);
		meta_header->ver = M_PQ_LOCALVAC_VERSION;
		meta_header->tag = M_PQ_LOCALVAC_TAG;
		break;
	case EN_PQ_METATAG_DV_INFO:
		meta_header->size = sizeof(struct meta_pq_dv_info);
		meta_header->ver = META_PQ_DV_INFO_VERSION;
		meta_header->tag = M_PQ_DV_INFO_TAG;
		break;
	case EN_PQ_METATAG_SRCCAP_DV_HDMI_INFO:
		meta_header->size = sizeof(struct meta_srccap_dv_info);
		meta_header->ver = META_SRCCAP_DV_INFO_VERSION;
		meta_header->tag = M_PQ_DV_HDMI_INFO_TAG;
		break;
	case EN_PQ_METATAG_VDEC_DV_DESCRB_INFO:
		meta_header->size = sizeof(struct vdec_dd_dolby_desc);
		meta_header->ver = MTK_VDEC_DD_DOLBY_DESC_VERSION;
		meta_header->tag = MTK_VDEC_DD_DOLBY_DESC_TAG;
		break;
	case EN_PQ_METATAG_VDEC_DV_PARSING_TAG:
		meta_header->size = sizeof(struct vdec_dd_dolby_desc_parsing);
		meta_header->ver = MTK_VDEC_DV_PARSING_VERSION;
		meta_header->tag = MTK_VDEC_DD_DV_PARSING_TAG;
		break;
	case EN_PQ_METATAG_THERMAL_INFO:
		meta_header->size = sizeof(struct meta_pq_thermal);
		meta_header->ver = META_PQ_THERMAL_INFO_VERSION;
		meta_header->tag = META_PQ_THERMAL_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_THERMAL_INFO:
		meta_header->size = sizeof(struct m_pq_thermal);
		meta_header->ver = M_PQ_THERMAL_INFO_VERSION;
		meta_header->tag = M_PQ_THERMAL_INFO_TAG;
		break;
	case EN_PQ_METATAG_DV_OUT_FRAME_INFO:
		meta_header->size = sizeof(struct meta_pq_dv_out_frame_info);
		meta_header->ver = META_PQ_DV_OUT_FRAME_INFO_VERSION;
		meta_header->tag = M_PQ_DV_OUTPUT_FRAME_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQ_CUSTOMER1_TAG:
		meta_header->size = 0;
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CUSTOMER1_TAG;
		break;
	case EN_PQ_METATAG_PQ_CUSTOMER1_PQU_TAG:
		meta_header->size = 0;
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CUSTOMER1_TAG;
		break;
	case EN_PQ_METATAG_FRM_STATISTIC:
		meta_header->size = sizeof(struct m_pq_frm_statistic);
		meta_header->ver = M_PQ_FRM_STATISTIC_VERSION;
		meta_header->tag = M_PQ_FRM_STATISTIC_TAG;
		break;
	case EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_SDR_TAG:
		meta_header->size = sizeof(struct meta_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_SDR_PIC1;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_SDR_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_SDR_PIC1;
		break;
	case EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HDR_TAG:
		meta_header->size = sizeof(struct meta_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_HDR_PIC1;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HDR_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_HDR_PIC1;
		break;
	case EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HLG_TAG:
		meta_header->size = sizeof(struct meta_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_HLG_PIC1;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HLG_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_calman_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_HLG_PIC1;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN0_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN0;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN0_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN0;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN1_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN1;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN1_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN1;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN2_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN2;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN2_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN2;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN3_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN3;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN3_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN3;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN4_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN4;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN4_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN4;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN5_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN5;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN5_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN5;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN6_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN6;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN6_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN6;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN7_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN7;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN7_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN7;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN8_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN8;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN8_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN8;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN9_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN9;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN9_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN9;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN10_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN10;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN10_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN10;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN11_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN11;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN11_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN11;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN12_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN12;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN12_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN12;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN13_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN13;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN13_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN13;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN14_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN14;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN14_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN14;
		break;
	case EN_PQ_METATAG_CFD_SHM_WIN15_CURVE_TAG:
		meta_header->size = sizeof(struct meta_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_WIN15;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_WIN15_CURVE_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_window_curve);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_WIN15;
		break;
	case EN_PQ_METATAG_CFD_SHM_CALMAN_HDR_CONF_TAG:
		meta_header->size = sizeof(struct meta_cfd_hdr_conf);
		meta_header->ver = 0;
		meta_header->tag = META_PQ_CFD_SHM_HDR_CONF_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_HDR_CONF_TAG:
		meta_header->size = sizeof(struct m_pqu_cfd_curve_hdr_conf);
		meta_header->ver = 0;
		meta_header->tag = M_PQ_CFD_SHM_HDR_CONF_INFO_TAG;
		break;
	case EN_PQ_METATAG_VSYNC_INFO_TAG:
		meta_header->size = sizeof(struct meta_pq_vsync_info);
		meta_header->ver = META_PQ_VSYNC_INFO_VERSION;
		meta_header->tag = META_PQ_VSYNC_INFO_TAG;
		break;
	case EN_PQ_METATAG_PQU_VSYNC_INFO_TAG:
		meta_header->size = sizeof(struct m_pq_vsync_info);
		meta_header->ver = M_PQ_VSYNC_INFO_VERSION;
		meta_header->tag = M_PQ_VSYNC_INFO_TAG;
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid metadata tag:(%d)\n", meta_tag);
		ret = -EPERM;
		break;
	}
	return ret;
}

int mtk_pq_common_read_metadata(int meta_fd,
	enum mtk_pq_common_metatag meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_map_fd(meta_fd, &va, &size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "trans fd to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = query_metadata_content(&buffer, &header, meta_content);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "query metadata content fail, tag:[%d]\n",
				   meta_tag);
		ret = -EPERM;
	}

out:
	_mtk_pq_common_unmap_fd(meta_fd, va);
	return ret;
}

int mtk_pq_common_read_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_map_db(meta_db, &va, &size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "trans db to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = query_metadata_content(&buffer, &header, meta_content);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "query metadata content fail, tag:[%d]\n",
				   meta_tag);
		ret = -EPERM;
	}

out:
	_mtk_pq_common_unmap_db(meta_db, va);
	return ret;
}

int mtk_pq_common_read_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	struct meta_header header;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		return ret;
	}

	meta_ret = query_metadata_content(meta_buf, &header, meta_content);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "query metadata content fail, tag:[%d]\n",
			       meta_tag);
		ret = -EPERM;
	}

	return ret;
}

int mtk_pq_common_read_metadata_addr_ptr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void **meta_content)
{
	int ret = 0;
	struct meta_header header;
	void *content = NULL;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}
	*meta_content = NULL;

	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		return ret;
	}

	content = query_metadata_content_ptr(meta_buf, &header);
	if (content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "query metadata content ptr fail, tag:[%d]\n",
			       meta_tag);
		ret = -EPERM;
	}
	*meta_content = content;

	return ret;
}

int mtk_pq_common_write_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_map_db(meta_db, &va, &size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "trans db to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = attach_metadata(&buffer, header, meta_content);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attach metadata fail\n");
		ret = -EPERM;
		goto out;
	}

out:
	_mtk_pq_common_unmap_db(meta_db, va);
	return ret;
}

int mtk_pq_common_write_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	struct meta_header header;

	if (meta_content == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid input, meta_content=NULL\n");
		return -EPERM;
	}

	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		return ret;
	}

	meta_ret = attach_metadata(meta_buf, header, meta_content);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "attach metadata fail\n");
		ret = -EPERM;
		return ret;
	}

	return ret;
}

int mtk_pq_common_delete_metadata(int meta_fd,
	enum mtk_pq_common_metatag meta_tag)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_map_fd(meta_fd, &va, &size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "trans fd to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = delete_metadata(&buffer, &header);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "delete metadata fail\n");
		ret = -EPERM;
		goto out;
	}

out:
	_mtk_pq_common_unmap_fd(meta_fd, va);
	return ret;
}

int mtk_pq_common_delete_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_map_db(meta_db, &va, &size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "trans fd to va fail\n");
		ret = -EPERM;
		goto out;
	}

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		goto out;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = delete_metadata(&buffer, &header);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "delete metadata fail\n");
		ret = -EPERM;
		goto out;
	}

out:
	_mtk_pq_common_unmap_db(meta_db, va);
	return ret;
}

int mtk_pq_common_delete_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	struct meta_header header;

	memset(&header, 0, sizeof(struct meta_header));

	ret = _mtk_pq_common_get_metaheader(meta_tag, &header);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get meta header fail\n");
		ret = -EPERM;
		return ret;
	}

	meta_ret = delete_metadata(meta_buf, &header);
	if (!meta_ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "delete metadata fail\n");
		ret = -EPERM;
		return ret;
	}

	return ret;
}

int mtk_pq_common_set_input(struct mtk_pq_device *pq_dev,
	unsigned int input_index)
{
	int ret = 0;

	if (!pq_dev)
		return -ENOMEM;
	pq_dev->common_info.input_source = input_index;

	if (mtk_pq_enhance_set_input_source(pq_dev, input_index)) {
		ret = -EPERM;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"pq enhance set input failed!\n");
		goto ERR;
	}

ERR:
	return ret;
}

int mtk_pq_common_get_input(struct mtk_pq_device *pq_dev,
	unsigned int *p_input_index)
{
	if ((!pq_dev) || (!p_input_index))
		return -ENOMEM;
	*p_input_index = pq_dev->common_info.input_source;

	return 0;
}

int mtk_pq_common_set_fmt_cap_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt)
{
	int ret = 0;

	if ((!pq_dev) || (!fmt))
		return -ENOMEM;

	if (mtk_pq_enhance_set_pix_format_out_mplane(pq_dev, &fmt->fmt.pix_mp)) {
		ret = -EPERM;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"pq enhance set pix fmt failed!\n");
		goto ERR;
	}

	/* note: this function will change fmt->fmt.pix_mp.num_planes */
	ret = mtk_pq_display_mdw_set_pix_format_mplane(pq_dev, fmt);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_display_mdw_set_pix_format_mplane failed, ret=%d\n", ret);
		goto ERR;
	}

	memcpy(&pq_dev->common_info.format_cap, fmt,
		sizeof(struct v4l2_format));

ERR:
	return ret;
}

int mtk_pq_common_set_fmt_out_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt)
{
	int ret = 0;
	u32 source = 0;

	if ((!pq_dev) || (!fmt))
		return -ENOMEM;

	source = pq_dev->common_info.input_source;

	if (mtk_pq_enhance_set_pix_format_in_mplane(pq_dev, &fmt->fmt.pix_mp)) {
		ret = -EPERM;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"hdr set pix fmt failed!\n");
		goto ERR;
	}
	if (IS_INPUT_SRCCAP(source)) {
		/* note: this function will change fmt->fmt.pix_mp.num_planes */
		ret = mtk_pq_display_idr_set_pix_format_mplane(fmt, pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_display_idr_set_pix_format failed!\n");
			return ret;
		}
	} else if (IS_INPUT_B2R(source)) {
		if (mtk_pq_b2r_set_pix_format_in_mplane(fmt, &pq_dev->b2r_info) < 0) {
			ret = -EPERM;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"b2r set pix fmt failed!\n");
			goto ERR;
		}
	} else {
		ret = -EINVAL;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"unknown source %d!\n", source);
		goto ERR;
	}

	memcpy(&pq_dev->common_info.format_out, fmt,
		sizeof(struct v4l2_format));

ERR:
	return ret;
}

int mtk_pq_common_get_fmt_cap_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt)
{
	if ((!pq_dev) || (!fmt))
		return -ENOMEM;

	memcpy(&(fmt->fmt.pix_mp), &pq_dev->common_info.format_cap.fmt.pix_mp,
		sizeof(struct v4l2_pix_format_mplane));

	return 0;
}

int mtk_pq_common_get_fmt_out_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt)
{
	memcpy(&(fmt->fmt.pix_mp), &pq_dev->common_info.format_out.fmt.pix_mp,
		sizeof(struct v4l2_pix_format_mplane));

	return 0;
}
static void _mtk_pq_common_debug_drop_idx(struct mtk_pq_device *pq_dev)
{
	uint8_t idx = 0;
	uint8_t idx_shift = 1;

	for (idx = 0; idx < DROP_ARY_IDX; idx++) {
		if (pq_dev->pq_debug.output_dqueue_ext_info.u64Pts !=
			pq_dev->pq_debug.queue_pts[idx]) {
			pq_dev->pq_debug.drop_frame_pts[pq_dev->pq_debug.drop_pts_idx] =
				pq_dev->pq_debug.queue_pts[idx];
			pq_dev->pq_debug.drop_pts_idx++;
			if (idx_shift < DROP_ARY_IDX-1)
				idx_shift++;
			if (pq_dev->pq_debug.drop_pts_idx >= DROP_ARY_IDX-1)
				pq_dev->pq_debug.drop_pts_idx = 0;
		} else {
			break;
		}
	}
	/*Need adapt by shift idx*/
	for (idx = 0; idx < DROP_ARY_IDX - idx_shift; idx++) {
		pq_dev->pq_debug.queue_pts[idx] =
			pq_dev->pq_debug.queue_pts[idx+idx_shift];
	}
	if (pq_dev->pq_debug.pts_idx > 0 && idx_shift < pq_dev->pq_debug.pts_idx)
		pq_dev->pq_debug.pts_idx -= idx_shift;
}
static int _mtk_pq_common_get_delay_time(
	struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	__u8 win = pq_dev->dev_indx;
	struct v4l2_pq_delaytime_info delaytime_info;
	__u16 b2r_delay = 0;
	__u16 display_delay = 0;
	__u16 pq_delay = 0;

	memset(&delaytime_info, 0, sizeof(struct v4l2_pq_delaytime_info));
	if (mtk_pq_b2r_get_delaytime(win, &b2r_delay, &pq_dev->b2r_info)) {
		ret = -EPERM;
		goto ERR;
	}

	delaytime_info.delaytime = b2r_delay + display_delay + pq_delay;
	memcpy(ctrl, &delaytime_info, sizeof(struct v4l2_pq_delaytime_info));

ERR:
	return ret;
}

static int _mtk_pq_common_set_input_ext_info(
	struct mtk_pq_device *pq_dev, void *ctrl)
{
	return 0;
}

static int _mtk_pq_common_set_output_ext_info(
	struct mtk_pq_device *pq_dev, void *ctrl)
{
	return 0;
}

static int _mtk_pq_common_set_hflip(
	struct mtk_pq_device *pq_dev, void *ctrl)
{
	bool enable = false;
	__u8 input_source = 0;
	__u8 win = 0;

	if (!pq_dev || !ctrl) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}
	input_source = pq_dev->common_info.input_source;
	win = pq_dev->dev_indx;

	enable = *(bool *)ctrl;

	if (input_source == PQ_INPUT_SOURCE_DTV ||
		input_source == PQ_INPUT_SOURCE_STORAGE) {
		/* b2r mirror*/
		if (mtk_pq_b2r_set_flip(pq_dev->dev_indx, enable,
			&pq_dev->b2r_info, B2R_HFLIP) < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_b2r_set_flip failed\n");
			return -EPERM;
		}
	} else {
		/* xc mirror*/
	}

	return 0;
}

static int _mtk_pq_common_set_vflip(
	struct mtk_pq_device *pq_dev, void *ctrl)
{
	bool enable = false;

	if (!pq_dev || !ctrl) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}
	enable = *(bool *)ctrl;

	pq_dev->common_info.v_flip = enable;

	return 0;
}

static int _mtk_pq_common_set_forcep(
	struct mtk_pq_device *pq_dev, bool enable)
{
	int ret = 0;
	__u8 win = pq_dev->dev_indx;

	/* set b2r*/
	mtk_pq_b2r_set_forcep(win, enable, &pq_dev->b2r_info);

	return ret;
}

static int _mtk_pq_common_set_delay_time(struct mtk_pq_device *pq_dev,
	void *ctrl)
{
	int ret = 0;
	__u8 win = pq_dev->dev_indx;
	struct v4l2_pq_delaytime_info delaytime_info;

	memset(&delaytime_info, 0, sizeof(struct v4l2_pq_delaytime_info));
	memcpy(&delaytime_info, ctrl, sizeof(struct v4l2_pq_delaytime_info));

	if (mtk_pq_b2r_set_delaytime(win, delaytime_info.fps,
		&pq_dev->b2r_info)) {
		ret = -EPERM;
		goto ERR;
	}

ERR:
	return ret;
}

static int _mtk_pq_common_set_input_timing(struct mtk_pq_device *pq_dev,
	void *ctrl)
{
	int ret = 0;
	__u8 win = pq_dev->dev_indx;

	memcpy(&pq_dev->common_info.timing_in, ctrl,
		sizeof(struct v4l2_timing));

	mtk_b2r_set_ext_timing(pq_dev->dev, win, ctrl, &pq_dev->b2r_info);

	return ret;
}

static int _mtk_pq_common_get_input_timing(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;

	memcpy(ctrl, &pq_dev->common_info.timing_in, sizeof(struct v4l2_timing));

	return ret;
}

static int _mtk_pq_common_set_gen_pattern(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	__u8 win = pq_dev->dev_indx;
	struct v4l2_pq_gen_pattern gen_pat_info;

	memset(&gen_pat_info, 0, sizeof(struct v4l2_pq_gen_pattern));
	memcpy(&gen_pat_info, ctrl, sizeof(struct v4l2_pq_gen_pattern));

	switch (gen_pat_info.pat_type) {
	case V4L2_EXT_PQ_B2R_PAT_FRAMECOLOR:
		mtk_pq_b2r_set_pattern(win, gen_pat_info.pat_type,
			&gen_pat_info.pat_para.b2r_pat_info, &pq_dev->b2r_info);
		break;
	default:
		ret = -EINVAL;
		goto ERR;
	}

ERR:
	return ret;
}

static int _mtk_pq_common_set_input_mode(
	struct mtk_pq_device *pq_dev, u8 *val)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;
	enum mtk_pq_input_mode mode = (enum mtk_pq_input_mode)*val;

	if (!pq_dev)
		return -ENOMEM;

	pqdev = dev_get_drvdata(pq_dev->dev);

	pq_dev->common_info.input_mode = mode;
	pq_dev->display_info.idr.input_mode = mode;

	// temp: hardcode sw mode
	if (mode == MTK_PQ_INPUT_MODE_HW) {
		pq_dev->common_info.input_mode = MTK_PQ_INPUT_MODE_SW;
		pq_dev->display_info.idr.input_mode = MTK_PQ_INPUT_MODE_SW;
	}


	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "SwMode_Support=%d, input_mode=%d\n",
		pqdev->pqcaps.u32Idr_SwMode_Support, mode);

	return ret;
}

static int _mtk_pq_common_set_output_mode(
	struct mtk_pq_device *pq_dev, u8 *val)
{
	int ret = 0;
	enum mtk_pq_output_mode mode = (enum mtk_pq_output_mode)*val;

	if (!pq_dev)
		return -ENOMEM;

	pq_dev->common_info.output_mode = mode;
	pq_dev->display_info.mdw.output_mode = mode;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "output_mode=%d\n", mode);

	return ret;
}

static int _mtk_pq_common_set_sw_usr_mode(
	enum pq_common_triggen_domain domain,
	bool sw_user_mode_h,
	bool sw_user_mode_v)
{
	int ret = 0;
	struct pq_common_triggen_sw_user_mode paramInSwUserMode;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInSwUserMode, 0, sizeof(paramInSwUserMode));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	//set sw user mode
	paramInSwUserMode.domain = domain;
	paramInSwUserMode.sw_user_mode_h = sw_user_mode_h;
	paramInSwUserMode.sw_user_mode_v = sw_user_mode_v;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_all_sw_usr_mode(bool sw_mode)
{
	int ret = 0;

	/* set all trigger gen sw user */
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_B2R, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(B2R, T, T) fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(B2R, T, T) fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_IP, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(IP, T, T) fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_OP1, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(OP1, T, T) fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_OP2, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(OP2, T, T) fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_pq_common_set_sw_usr_mode(PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM, sw_mode, sw_mode);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_set_sw_usr_mode(FRC, T, T) fail, ret = %d\n", ret);
		return ret;
	}


	return 0;
}

static int _mtk_pq_common_set_pattern_size(bool enable, struct meta_buffer *meta_buf)
{
	int ret = 0;
#ifdef PQU_PATTERN_TEST
	struct pq_pattern_size_info *size_info_ptr = NULL;

	if (enable) {
		/* fill test pattern info start */
		ret = mtk_pq_common_read_metadata_addr_ptr(meta_buf,
			EN_PQ_METATAG_PQU_PATTERN_SIZE_INFO, (void **)&size_info_ptr);
		if (ret || size_info_ptr == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq read EN_PQ_METATAG_PQU_PATTERN_SIZE_INFO failed, ret = %d\n"
					, ret);
		}

		ret = mtk_pq_pattern_set_size_info(size_info_ptr);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_pattern_set_size_info failed, ret = %d\n", ret);
		}
		/* fill test pattern info end */
	}
#endif
	return ret;
}

static int _mtk_pq_common_set_output_dqbuf_meta(uint8_t win_id, struct meta_buffer meta_buf)
{
	int ret = 0;
	enum pqu_dip_cap_point idx = pqu_dip_cap_pqin;
	struct meta_pq_display_flow_info *disp_info_ptr = NULL;
	struct m_pq_display_flow_ctrl *pqu_disp_info_ptr = NULL;
	struct meta_pq_display_wm_info *pqu_wm_info_ptr = NULL;
	static struct meta_pq_bbd_info meta_bbd_info;
	struct m_pq_bbd_info *pqu_bbd_info_ptr = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct m_pq_thermal *pqu_therma_info = NULL;
	static struct mtk_pq_thermal thermal_info;
	static struct m_pq_vsync_info pqu_vsync_info;

	pq_dev = pqdev->pq_devs[win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return -1;
	}

	memset(&meta_bbd_info, 0, sizeof(struct meta_pq_bbd_info));
	memset(&thermal_info, 0, sizeof(struct mtk_pq_thermal));
	memset(&pqu_vsync_info, 0, sizeof(struct m_pq_vsync_info));

	/* fill display flow info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_DISPLAY_FLOW_INFO, (void *)&disp_info_ptr);
	if (ret || disp_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_DISPLAY_FLOW_INFO fail,ret = %d\n", ret);
		goto exit;
	}

	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_PQU_DISP_FLOW_INFO, (void *)&pqu_disp_info_ptr);
	if (ret || pqu_disp_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_DISP_FLOW_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	if (pqu_disp_info_ptr->pq_status_done) {
		memcpy(pq_dev->pqu_meta.pq_status,
			pqu_disp_info_ptr->pq_status, sizeof(bool)*PQU_IP_STATUS_NUM);
		pq_dev->pqu_meta.pq_status_done = true;
		pqu_disp_info_ptr->pq_status_done = false;
	}

	disp_info_ptr->win_id = win_id;
	for (idx = pqu_dip_cap_pqin; idx < pqu_dip_cap_max; idx++)
		memcpy(&(disp_info_ptr->dip_win[idx]), &(pqu_disp_info_ptr->dip_win[idx]),
				sizeof(struct dip_window_info));

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s : %s%d, %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
		"meta_pq_display_flow_info",
		"win_id = ", disp_info_ptr->win_id,
		"pqu_dip_cap_pqin = ", disp_info_ptr->dip_win[pqu_dip_cap_pqin].width,
			disp_info_ptr->dip_win[pqu_dip_cap_pqin].height,
			disp_info_ptr->dip_win[pqu_dip_cap_pqin].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_pqin].p_engine,
		"pqu_dip_cap_iphdr = ", disp_info_ptr->dip_win[pqu_dip_cap_iphdr].width,
			disp_info_ptr->dip_win[pqu_dip_cap_iphdr].height,
			disp_info_ptr->dip_win[pqu_dip_cap_iphdr].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_iphdr].p_engine,
		"pqu_dip_cap_prespf = ", disp_info_ptr->dip_win[pqu_dip_cap_prespf].width,
			disp_info_ptr->dip_win[pqu_dip_cap_prespf].height,
			disp_info_ptr->dip_win[pqu_dip_cap_prespf].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_prespf].p_engine);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
		"meta_pq_display_flow_info",
		"pqu_dip_cap_srs = ", disp_info_ptr->dip_win[pqu_dip_cap_srs].width,
			disp_info_ptr->dip_win[pqu_dip_cap_srs].height,
			disp_info_ptr->dip_win[pqu_dip_cap_srs].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_srs].p_engine,
		"pqu_dip_cap_vip = ", disp_info_ptr->dip_win[pqu_dip_cap_vip].width,
			disp_info_ptr->dip_win[pqu_dip_cap_vip].height,
			disp_info_ptr->dip_win[pqu_dip_cap_vip].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_vip].p_engine,
		"pqu_dip_cap_mdw = ", disp_info_ptr->dip_win[pqu_dip_cap_mdw].width,
			disp_info_ptr->dip_win[pqu_dip_cap_mdw].height,
			disp_info_ptr->dip_win[pqu_dip_cap_mdw].color_fmt,
			disp_info_ptr->dip_win[pqu_dip_cap_mdw].p_engine);
	/* fill display flow info end */

	/* fill thermal flow info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_PQU_THERMAL_INFO, (void *)&pqu_therma_info);
	if (_mtk_pq_common_check_meta_ret(ret, pqu_therma_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_THERMAL_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"Thermal/Algo aisr state=%u,%u\n",
		pqu_therma_info->thermal_state_aisr,
		pqu_therma_info->thermal_algo_state_aisr);

	thermal_info.thermal_state_aisr =
		(enum mtk_pq_thermal_state)pqu_therma_info->thermal_state_aisr;
	thermal_info.thermal_algo_state_aisr =
		(enum mtk_pq_algo_thermal_state)pqu_therma_info->thermal_algo_state_aisr;

	_pqu_thermal_info[win_id].thermal_state_aisr = thermal_info.thermal_state_aisr;
	_pqu_thermal_info[win_id].thermal_algo_state_aisr = thermal_info.thermal_algo_state_aisr;

	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_THERMAL_INFO, &thermal_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_THERMAL_INFO fail, ret=%d\n", ret);
		goto exit;
	}
	/* fill thermal flow info end */

	/* fill bbd info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_PQU_BBD_INFO, (void **)&pqu_bbd_info_ptr);
	if (ret || pqu_bbd_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_BBD_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	memcpy(&meta_bbd_info, pqu_bbd_info_ptr, sizeof(struct meta_pq_bbd_info));

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s : %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"meta_pq_bbd_info",
		"u8Validity = ", meta_bbd_info.u8Validity,
		"u16LeftOuterPos = ", meta_bbd_info.u16LeftOuterPos,
		"u16RightOuterPos = ", meta_bbd_info.u16RightOuterPos,
		"u16LeftInnerPos = ", meta_bbd_info.u16LeftInnerPos,
		"u16LeftInnerConf = ", meta_bbd_info.u16LeftInnerConf,
		"u16RightInnerPos = ", meta_bbd_info.u16RightInnerPos,
		"u16RightInnerConf = ", meta_bbd_info.u16RightInnerConf,
		"u16CurTopContentY = ", meta_bbd_info.u16CurTopContentY,
		"u16CurBotContentY = ", meta_bbd_info.u16CurBotContentY);

	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_BBD_INFO, (void *)&meta_bbd_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_BBD_INFO fail, ret=%d\n", ret);
		goto exit;
	}
	/* fill bbd info end */

	/* fill vsync info start */
	ret = mtk_pq_common_read_metadata_addr(&meta_buf,
		EN_PQ_METATAG_PQU_VSYNC_INFO_TAG, (void *)&pqu_vsync_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_PQU_VSYNC_INFO_TAG fail, ret=%d\n", ret);
		goto exit;
	}
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"vsync_info:{%llu, %llu}\n",
		pqu_vsync_info.vsync_timestamp, pqu_vsync_info.vsync_duration);
	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_VSYNC_INFO_TAG, (void *)&pqu_vsync_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_VSYNC_INFO_TAG fail, ret=%d\n", ret);
		goto exit;
	}
	/* fill vsync info end */

	ret = mtk_pq_common_read_metadata_addr_ptr(
		&meta_buf, EN_PQ_METATAG_PQU_DISP_WM_INFO, (void **)&pqu_wm_info_ptr);
	if (ret || pqu_wm_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_DISP_WM_INFO failed, ret = %d\n"
				, ret);
		goto exit;
	}

	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_DISPLAY_WM_INFO, pqu_wm_info_ptr);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_DISPLAY_WM_INFO failed, ret = %d\n"
				, ret);
		goto exit;
	}

	ret = _mtk_pq_common_set_pattern_size(pq_dev->pattern_info.enable, &meta_buf);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_pattern_size failed, ret = %d\n", ret);
		goto exit;
	}
exit:
	return ret;
}

static int _mtk_pq_common_set_capture_dqbuf_meta(uint8_t win_id, struct meta_buffer meta_buf)
{
	int ret = 0;
	int i = 0;
	enum pqu_dip_cap_point idx = pqu_dip_cap_pqin;
	struct m_pq_display_flow_ctrl *pqu_disp_info_ptr = NULL;
	struct meta_pq_output_queue_ext_info *pq_ext_info_ptr = NULL;
	struct m_pq_queue_ext_info *pqu_ext_info_ptr = NULL;
	static struct meta_pq_display_flow_info meta_disp_info;
	struct m_pq_display_idr_ctrl *pqu_idr_crtl_ptr = NULL;
	static struct meta_pq_display_idr_ctrl meta_disp_idr_crtl;
	struct meta_pq_output_frame_info *pq_out_frm_info_ptr = NULL;
	struct mtk_pq_device *pq_dev = NULL;

	pq_dev = pqdev->pq_devs[win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return -1;
	}

	memset(&meta_disp_info, 0, sizeof(struct meta_pq_display_flow_info));
	memset(&meta_disp_idr_crtl, 0, sizeof(struct meta_pq_display_idr_ctrl));

	/* fill queue ext info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(
		&meta_buf, EN_PQ_METATAG_OUTPUT_QUEUE_EXT_INFO, (void **)&pq_ext_info_ptr);
	if (ret || pq_ext_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_OUTPUT_QUEUE_EXT_INFO failed, ret = %d\n"
				, ret);
		goto exit;
	}

	ret = mtk_pq_common_read_metadata_addr_ptr(
		&meta_buf, EN_PQ_METATAG_PQU_QUEUE_EXT_INFO, (void **)&pqu_ext_info_ptr);
	if (ret || pqu_ext_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_QUEUE_EXT_INFO failed, ret = %d\n"
				, ret);
		goto exit;
	}

	pq_ext_info_ptr->u64Pts = pqu_ext_info_ptr->u64Pts;
	pq_ext_info_ptr->u64UniIdx = pqu_ext_info_ptr->u64UniIdx;
	pq_ext_info_ptr->u64ExtUniIdx = pqu_ext_info_ptr->u64ExtUniIdx;
	pq_ext_info_ptr->u64TimeStamp = pqu_ext_info_ptr->u64TimeStamp;
	pq_ext_info_ptr->u64RenderTimeNs = pqu_ext_info_ptr->u64RenderTimeNs;
	pq_ext_info_ptr->u8BufferValid = pqu_ext_info_ptr->u8BufferValid;
	pq_ext_info_ptr->u32BufferSlot = pqu_ext_info_ptr->u32BufferSlot;
	pq_ext_info_ptr->u32GenerationIndex = pqu_ext_info_ptr->u32GenerationIndex;
	pq_ext_info_ptr->u32StreamUniqueId = pqu_ext_info_ptr->u32StreamUniqueId;
	pq_ext_info_ptr->u8RepeatStatus = pqu_ext_info_ptr->u8RepeatStatus;
	pq_ext_info_ptr->u8FrcMode = pqu_ext_info_ptr->u8FrcMode;
	pq_ext_info_ptr->u8Interlace = pqu_ext_info_ptr->u8Interlace;
	pq_ext_info_ptr->u32InputFps = pqu_ext_info_ptr->u32InputFps;
	pq_ext_info_ptr->u32OriginalInputFps = pqu_ext_info_ptr->u32OriginalInputFps;
	pq_ext_info_ptr->bEOS = pqu_ext_info_ptr->bEOS;
	pq_ext_info_ptr->u8MuteAction = pqu_ext_info_ptr->u8MuteAction;
	pq_ext_info_ptr->u8PqMuteAction = pqu_ext_info_ptr->u8PqMuteAction;
	pq_ext_info_ptr->u8SignalStable = pqu_ext_info_ptr->u8SignalStable;
	pq_ext_info_ptr->u8DotByDotType = pqu_ext_info_ptr->u8DotByDotType;
	pq_ext_info_ptr->u32RefreshRate = pqu_ext_info_ptr->u32RefreshRate;
	pq_ext_info_ptr->bReportFrameStamp = pqu_ext_info_ptr->bReportFrameStamp;
	pq_ext_info_ptr->bBypassAvsync = pqu_ext_info_ptr->bBypassAvsync;
	pq_ext_info_ptr->u32HdrApplyType = pqu_ext_info_ptr->u32HdrApplyType;
	pq_ext_info_ptr->u32QueueInputIndex = pqu_ext_info_ptr->u32QueueInputIndex;
	pq_ext_info_ptr->idrinfo.y = pqu_ext_info_ptr->idrinfo.y;
	pq_ext_info_ptr->idrinfo.height = pqu_ext_info_ptr->idrinfo.height;
	pq_ext_info_ptr->idrinfo.v_total = pqu_ext_info_ptr->idrinfo.v_total;
	pq_ext_info_ptr->idrinfo.vs_pulse_width = pqu_ext_info_ptr->idrinfo.vs_pulse_width;
	pq_ext_info_ptr->idrinfo.fdet_vtt0 = pqu_ext_info_ptr->idrinfo.fdet_vtt0;
	pq_ext_info_ptr->idrinfo.fdet_vtt1 = pqu_ext_info_ptr->idrinfo.fdet_vtt1;
	pq_ext_info_ptr->idrinfo.fdet_vtt2 = pqu_ext_info_ptr->idrinfo.fdet_vtt2;
	pq_ext_info_ptr->idrinfo.fdet_vtt3 = pqu_ext_info_ptr->idrinfo.fdet_vtt3;
	pq_ext_info_ptr->bMuteChange = pqu_ext_info_ptr->bMuteChange;
	pq_ext_info_ptr->bFdMaskBypass = pqu_ext_info_ptr->bFdMaskBypass;

	/*fill debug info*/
	pq_dev->pq_debug.output_dqueue_ext_info.u64Pts =
		pqu_ext_info_ptr->u64Pts;
	pq_dev->pq_debug.output_dqueue_ext_info.u64UniIdx =
		pqu_ext_info_ptr->u64UniIdx;
	pq_dev->pq_debug.output_dqueue_ext_info.u64ExtUniIdx =
		pqu_ext_info_ptr->u64ExtUniIdx;
	pq_dev->pq_debug.output_dqueue_ext_info.u64TimeStamp =
		pqu_ext_info_ptr->u64TimeStamp;
	pq_dev->pq_debug.output_dqueue_ext_info.u64RenderTimeNs =
		pqu_ext_info_ptr->u64RenderTimeNs;
	pq_dev->pq_debug.output_dqueue_ext_info.u8BufferValid =
		pqu_ext_info_ptr->u8BufferValid;
	pq_dev->pq_debug.output_dqueue_ext_info.u32BufferSlot =
		pqu_ext_info_ptr->u32BufferSlot;
	pq_dev->pq_debug.output_dqueue_ext_info.u32GenerationIndex =
		pqu_ext_info_ptr->u32GenerationIndex;
	pq_dev->pq_debug.output_dqueue_ext_info.u32StreamUniqueId =
		pqu_ext_info_ptr->u32StreamUniqueId;
	pq_dev->pq_debug.output_dqueue_ext_info.u8RepeatStatus =
		pqu_ext_info_ptr->u8RepeatStatus;
	pq_dev->pq_debug.output_dqueue_ext_info.u8FrcMode =
		pqu_ext_info_ptr->u8FrcMode;
	pq_dev->pq_debug.output_dqueue_ext_info.u8Interlace =
		pqu_ext_info_ptr->u8Interlace;
	pq_dev->pq_debug.output_dqueue_ext_info.u32InputFps =
		pqu_ext_info_ptr->u32InputFps;
	pq_dev->pq_debug.output_dqueue_ext_info.u32OriginalInputFps =
		pqu_ext_info_ptr->u32OriginalInputFps;
	pq_dev->pq_debug.output_dqueue_ext_info.bEOS =
		pqu_ext_info_ptr->bEOS;
	pq_dev->pq_debug.output_dqueue_ext_info.u8MuteAction =
		pqu_ext_info_ptr->u8MuteAction;
	pq_dev->pq_debug.output_dqueue_ext_info.u8PqMuteAction =
		pqu_ext_info_ptr->u8PqMuteAction;
	pq_dev->pq_debug.output_dqueue_ext_info.u8SignalStable =
		pqu_ext_info_ptr->u8SignalStable;
	pq_dev->pq_debug.output_dqueue_ext_info.u8DotByDotType =
		pqu_ext_info_ptr->u8DotByDotType;
	pq_dev->pq_debug.output_dqueue_ext_info.u32RefreshRate =
		pqu_ext_info_ptr->u32RefreshRate;
	pq_dev->pq_debug.output_dqueue_ext_info.bReportFrameStamp =
		pqu_ext_info_ptr->bReportFrameStamp;
	pq_dev->pq_debug.output_dqueue_ext_info.bBypassAvsync =
		pqu_ext_info_ptr->bBypassAvsync;
	pq_dev->pq_debug.output_dqueue_ext_info.u32HdrApplyType =
		pqu_ext_info_ptr->u32HdrApplyType;
	pq_dev->pq_debug.output_dqueue_ext_info.u32QueueInputIndex =
		pqu_ext_info_ptr->u32QueueInputIndex;
	pq_dev->pq_debug.output_dqueue_ext_info.bMuteChange =
		pqu_ext_info_ptr->bMuteChange;
	pq_dev->pq_debug.output_dqueue_ext_info.bFdMaskBypass =
		pqu_ext_info_ptr->bFdMaskBypass;
	/*fill debug info end*/

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"meta_pq_output_queue_ext_info : %s%llu, %s%llu, %s%llu, %s%llu, %s%llu, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%u, %s%u, %s%u, %s%u, %s%u, %s%d, %s%d, %s%u, %s%u, %s%u, %s%d, %s%d\n",
	"u64Pts = ", pq_ext_info_ptr->u64Pts,
	"u64UniIdx = ", pq_ext_info_ptr->u64UniIdx,
	"u64ExtUniIdx = ", pq_ext_info_ptr->u64ExtUniIdx,
	"u64TimeStamp = ", pq_ext_info_ptr->u64TimeStamp,
	"u64RenderTimeNs = ", pq_ext_info_ptr->u64RenderTimeNs,
	"u8BufferValid = ", pq_ext_info_ptr->u8BufferValid,
	"u32GenerationIndex = ", pq_ext_info_ptr->u32GenerationIndex,
	"u32StreamUniqueId = ", pq_ext_info_ptr->u32StreamUniqueId,
	"u8RepeatStatus = ", pq_ext_info_ptr->u8RepeatStatus,
	"u8FrcMode = ", pq_ext_info_ptr->u8FrcMode,
	"u8Interlace = ", pq_ext_info_ptr->u8Interlace,
	"u32InputFps = ", pq_ext_info_ptr->u32InputFps,
	"u32OriginalInputFps = ", pq_ext_info_ptr->u32OriginalInputFps,
	"bEOS = ", pq_ext_info_ptr->bEOS,
	"u8MuteAction = ", pq_ext_info_ptr->u8MuteAction,
	"u8PqMuteAction = ", pq_ext_info_ptr->u8PqMuteAction,
	"u8SignalStable = ", pq_ext_info_ptr->u8SignalStable,
	"u8DotByDotType = ", pq_ext_info_ptr->u8DotByDotType,
	"u32RefreshRate = ", pq_ext_info_ptr->u32RefreshRate,
	"bReportFrameStamp = ", pq_ext_info_ptr->bReportFrameStamp,
	"bBypassAvsync = ", pq_ext_info_ptr->bBypassAvsync,
	"u32HdrApplyType = ", pq_ext_info_ptr->u32HdrApplyType,
	"u32QueueOutputIndex = ", pq_ext_info_ptr->u32QueueOutputIndex,
	"u32QueueInputIndex = ", pq_ext_info_ptr->u32QueueInputIndex,
	"bMuteChange = ", pq_ext_info_ptr->bMuteChange,
	"bFdMaskBypass = ", pq_ext_info_ptr->bFdMaskBypass);
	pq_ext_info_ptr->pqrminfo.u32RemainBWBudget =
			pqu_ext_info_ptr->pqrminfo.u32RemainBWBudget;
	pq_ext_info_ptr->pqrminfo.u8ActiveWinNum = pqu_ext_info_ptr->pqrminfo.u8ActiveWinNum;
	for (i = 0; i < pqu_ext_info_ptr->pqrminfo.u8ActiveWinNum; i++) {
		pq_ext_info_ptr->pqrminfo.win_info[i].u8PqID =
			pqu_ext_info_ptr->pqrminfo.win_info[i].u8PqID;
		pq_ext_info_ptr->pqrminfo.win_info[i].u32WinTime =
			pqu_ext_info_ptr->pqrminfo.win_info[i].u32WinTime;
	}
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"u8ActiveWinNum = %d\n",
		pq_ext_info_ptr->pqrminfo.u8ActiveWinNum);
	/* fill queue ext info end */

	/* fill queue ext info to output info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(
		&meta_buf, EN_PQ_METATAG_OUTPUT_FRAME_INFO, (void **)&pq_out_frm_info_ptr);
	if (ret || pq_out_frm_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"read EN_PQ_METATAG_OUTPUT_FRAME_INFO Failed, ret = %d\n", ret);
		goto exit;
	}
	pq_out_frm_info_ptr->pq_frame_id = pqu_ext_info_ptr->int32FrameId;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_COMMON, "%s%d\n",
		"pq_frame_id = ", pq_out_frm_info_ptr->pq_frame_id);
	/* fill queue ext info to output info end */

	/* fill display flow info start */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_PQU_DISP_FLOW_INFO, (void **)&pqu_disp_info_ptr);
	if (ret || pqu_disp_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_PQU_DISP_FLOW_INFO fail, ret=%d\n", ret);
		goto exit;
	}

	memcpy(&(meta_disp_info.content), &(pqu_disp_info_ptr->content),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.capture), &(pqu_disp_info_ptr->cap_win),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.crop), &(pqu_disp_info_ptr->crop_win),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.display), &(pqu_disp_info_ptr->disp_win),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.displayArea), &(pqu_disp_info_ptr->displayArea),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.displayRange), &(pqu_disp_info_ptr->displayRange),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.displayBase), &(pqu_disp_info_ptr->displayBase),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.realDisplaySize), &(pqu_disp_info_ptr->realDisplaySize),
				sizeof(struct meta_pq_window));
	/*fill debug info*/
	pq_dev->pq_debug.content_out.x = pqu_disp_info_ptr->content.x;
	pq_dev->pq_debug.content_out.y = pqu_disp_info_ptr->content.y;
	pq_dev->pq_debug.content_out.width = pqu_disp_info_ptr->content.width;
	pq_dev->pq_debug.content_out.height = pqu_disp_info_ptr->content.height;
	pq_dev->pq_debug.content_out.w_align = pqu_disp_info_ptr->content.w_align;
	pq_dev->pq_debug.content_out.h_align = pqu_disp_info_ptr->content.h_align;
	pq_dev->pq_debug.capture_out.x = pqu_disp_info_ptr->cap_win.x;
	pq_dev->pq_debug.capture_out.y = pqu_disp_info_ptr->cap_win.y;
	pq_dev->pq_debug.capture_out.width = pqu_disp_info_ptr->cap_win.width;
	pq_dev->pq_debug.capture_out.height = pqu_disp_info_ptr->cap_win.height;
	pq_dev->pq_debug.capture_out.w_align = pqu_disp_info_ptr->cap_win.w_align;
	pq_dev->pq_debug.capture_out.h_align = pqu_disp_info_ptr->cap_win.h_align;
	pq_dev->pq_debug.crop_out.x = pqu_disp_info_ptr->crop_win.x;
	pq_dev->pq_debug.crop_out.y = pqu_disp_info_ptr->crop_win.y;
	pq_dev->pq_debug.crop_out.width = pqu_disp_info_ptr->crop_win.width;
	pq_dev->pq_debug.crop_out.height = pqu_disp_info_ptr->crop_win.height;
	pq_dev->pq_debug.crop_out.w_align = pqu_disp_info_ptr->crop_win.w_align;
	pq_dev->pq_debug.crop_out.h_align = pqu_disp_info_ptr->crop_win.h_align;
	pq_dev->pq_debug.display_out.x = pqu_disp_info_ptr->disp_win.x;
	pq_dev->pq_debug.display_out.y = pqu_disp_info_ptr->disp_win.y;
	pq_dev->pq_debug.display_out.width = pqu_disp_info_ptr->disp_win.width;
	pq_dev->pq_debug.display_out.height = pqu_disp_info_ptr->disp_win.height;
	pq_dev->pq_debug.display_out.w_align = pqu_disp_info_ptr->disp_win.w_align;
	pq_dev->pq_debug.display_out.h_align = pqu_disp_info_ptr->disp_win.h_align;
	pq_dev->pq_debug.displayArea_out.x = pqu_disp_info_ptr->displayArea.x;
	pq_dev->pq_debug.displayArea_out.y = pqu_disp_info_ptr->displayArea.y;
	pq_dev->pq_debug.displayArea_out.width =
		pqu_disp_info_ptr->displayArea.width;
	pq_dev->pq_debug.displayArea_out.height =
		pqu_disp_info_ptr->displayArea.height;
	pq_dev->pq_debug.displayArea_out.w_align =
		pqu_disp_info_ptr->displayArea.w_align;
	pq_dev->pq_debug.displayArea_out.h_align =
		pqu_disp_info_ptr->displayArea.h_align;
	pq_dev->pq_debug.displayRange_out.x =
		pqu_disp_info_ptr->displayRange.x;
	pq_dev->pq_debug.displayRange_out.y =
		pqu_disp_info_ptr->displayRange.y;
	pq_dev->pq_debug.displayRange_out.width =
		pqu_disp_info_ptr->displayRange.width;
	pq_dev->pq_debug.displayRange_out.height =
		pqu_disp_info_ptr->displayRange.height;
	pq_dev->pq_debug.displayRange_out.w_align =
		pqu_disp_info_ptr->displayRange.w_align;
	pq_dev->pq_debug.displayRange_out.h_align =
		pqu_disp_info_ptr->displayRange.h_align;
	pq_dev->pq_debug.displayBase_out.x =
		pqu_disp_info_ptr->displayBase.x;
	pq_dev->pq_debug.displayBase_out.y =
		pqu_disp_info_ptr->displayBase.y;
	pq_dev->pq_debug.displayBase_out.width =
		pqu_disp_info_ptr->displayBase.width;
	pq_dev->pq_debug.displayBase_out.height =
		pqu_disp_info_ptr->displayBase.height;
	pq_dev->pq_debug.displayBase_out.w_align =
		pqu_disp_info_ptr->displayBase.w_align;
	pq_dev->pq_debug.displayBase_out.h_align =
		pqu_disp_info_ptr->displayBase.h_align;
	/*fill debug info end*/

	meta_disp_info.win_id = win_id;
	pq_dev->pq_debug.win_id_debug = win_id;
	for (idx = pqu_dip_cap_pqin; idx < pqu_dip_cap_max; idx++)
		memcpy(&(meta_disp_info.dip_win[idx]), &(pqu_disp_info_ptr->dip_win[idx]),
				sizeof(struct dip_window_info));
	memcpy(&(meta_disp_info.outcrop), &(pqu_disp_info_ptr->ip_win[pqu_ip_scmi_out]),
				sizeof(struct meta_pq_window));
	memcpy(&(meta_disp_info.outdisplay), &(pqu_disp_info_ptr->ip_win[pqu_ip_hvsp_out]),
				sizeof(struct meta_pq_window));
	/*fill debug info*/
	pq_dev->pq_debug.content_out.x = pqu_disp_info_ptr->content.x;
	pq_dev->pq_debug.content_out.y = pqu_disp_info_ptr->content.y;
	pq_dev->pq_debug.content_out.width = pqu_disp_info_ptr->content.width;
	pq_dev->pq_debug.content_out.height = pqu_disp_info_ptr->content.height;
	pq_dev->pq_debug.content_out.w_align = pqu_disp_info_ptr->content.w_align;
	pq_dev->pq_debug.content_out.h_align = pqu_disp_info_ptr->content.h_align;
	pq_dev->pq_debug.capture_out.x = pqu_disp_info_ptr->cap_win.x;
	pq_dev->pq_debug.capture_out.y = pqu_disp_info_ptr->cap_win.y;
	pq_dev->pq_debug.capture_out.width = pqu_disp_info_ptr->cap_win.width;
	pq_dev->pq_debug.capture_out.height = pqu_disp_info_ptr->cap_win.height;
	pq_dev->pq_debug.capture_out.w_align = pqu_disp_info_ptr->cap_win.w_align;
	pq_dev->pq_debug.capture_out.h_align = pqu_disp_info_ptr->cap_win.h_align;
	pq_dev->pq_debug.crop_out.x = pqu_disp_info_ptr->crop_win.x;
	pq_dev->pq_debug.crop_out.y = pqu_disp_info_ptr->crop_win.y;
	pq_dev->pq_debug.crop_out.width = pqu_disp_info_ptr->crop_win.width;
	pq_dev->pq_debug.crop_out.height = pqu_disp_info_ptr->crop_win.height;
	pq_dev->pq_debug.crop_out.w_align = pqu_disp_info_ptr->crop_win.w_align;
	pq_dev->pq_debug.crop_out.h_align = pqu_disp_info_ptr->crop_win.h_align;
	pq_dev->pq_debug.display_out.x = pqu_disp_info_ptr->disp_win.x;
	pq_dev->pq_debug.display_out.y = pqu_disp_info_ptr->disp_win.y;
	pq_dev->pq_debug.display_out.width = pqu_disp_info_ptr->disp_win.width;
	pq_dev->pq_debug.display_out.height = pqu_disp_info_ptr->disp_win.height;
	pq_dev->pq_debug.display_out.w_align = pqu_disp_info_ptr->disp_win.w_align;
	pq_dev->pq_debug.display_out.h_align = pqu_disp_info_ptr->disp_win.h_align;
	pq_dev->pq_debug.displayArea_out.x = pqu_disp_info_ptr->displayArea.x;
	pq_dev->pq_debug.displayArea_out.y = pqu_disp_info_ptr->displayArea.y;
	pq_dev->pq_debug.displayArea_out.width = pqu_disp_info_ptr->displayArea.width;
	pq_dev->pq_debug.displayArea_out.height =
		pqu_disp_info_ptr->displayArea.height;
	pq_dev->pq_debug.displayArea_out.w_align =
		pqu_disp_info_ptr->displayArea.w_align;
	pq_dev->pq_debug.displayArea_out.h_align =
		pqu_disp_info_ptr->displayArea.h_align;
	pq_dev->pq_debug.displayRange_out.x =
		pqu_disp_info_ptr->displayRange.x;
	pq_dev->pq_debug.displayRange_out.y =
		pqu_disp_info_ptr->displayRange.y;
	pq_dev->pq_debug.displayRange_out.width =
		pqu_disp_info_ptr->displayRange.width;
	pq_dev->pq_debug.displayRange_out.height =
		pqu_disp_info_ptr->displayRange.height;
	pq_dev->pq_debug.displayRange_out.w_align =
		pqu_disp_info_ptr->displayRange.w_align;
	pq_dev->pq_debug.displayRange_out.h_align =
		pqu_disp_info_ptr->displayRange.h_align;
	pq_dev->pq_debug.displayBase_out.x =
		pqu_disp_info_ptr->displayBase.x;
	pq_dev->pq_debug.displayBase_out.y =
		pqu_disp_info_ptr->displayBase.y;
	pq_dev->pq_debug.displayBase_out.width =
		pqu_disp_info_ptr->displayBase.width;
	pq_dev->pq_debug.displayBase_out.height =
		pqu_disp_info_ptr->displayBase.height;
	pq_dev->pq_debug.displayBase_out.w_align =
		pqu_disp_info_ptr->displayBase.w_align;
	pq_dev->pq_debug.displayBase_out.h_align =
		pqu_disp_info_ptr->displayBase.h_align;
	/*fill debug info end*/
	/*debug info total dque*/
	pq_dev->pq_debug.total_dque++;
	/*debug info drop frame*/
	_mtk_pq_common_debug_drop_idx(pq_dev);
	/*debug info drop frame end*/
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"meta_pq_display_flow_info : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"content = ", meta_disp_info.content.x, meta_disp_info.content.y,
		meta_disp_info.content.width, meta_disp_info.content.height,
	"capture = ", meta_disp_info.capture.x, meta_disp_info.capture.y,
		meta_disp_info.capture.width, meta_disp_info.capture.height,
	"crop = ", meta_disp_info.crop.x, meta_disp_info.crop.y,
		meta_disp_info.crop.width, meta_disp_info.crop.height,
	"display = ", meta_disp_info.display.x, meta_disp_info.display.y,
		meta_disp_info.display.width, meta_disp_info.display.height,
	"displayArea = ", meta_disp_info.displayArea.x, meta_disp_info.displayArea.y,
		meta_disp_info.displayArea.width, meta_disp_info.displayArea.height,
	"displayRange = ", meta_disp_info.displayRange.x, meta_disp_info.displayRange.y,
		meta_disp_info.displayRange.width, meta_disp_info.displayRange.height,
	"displayBase = ", meta_disp_info.displayBase.x, meta_disp_info.displayBase.y,
		meta_disp_info.displayBase.width, meta_disp_info.displayBase.height,
	"realDisplaySize = ", meta_disp_info.realDisplaySize.x, meta_disp_info.realDisplaySize.y,
		meta_disp_info.realDisplaySize.width, meta_disp_info.realDisplaySize.height);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"meta_pq_display_flow_info : %s(%d,%d,%d,%d), %s(%d,%d,%d,%d)\n",
	"outcrop = ", meta_disp_info.outcrop.x, meta_disp_info.outcrop.y,
		meta_disp_info.outcrop.width, meta_disp_info.outcrop.height,
	"outdisplay = ", meta_disp_info.outdisplay.x, meta_disp_info.outdisplay.y,
		meta_disp_info.outdisplay.width, meta_disp_info.outdisplay.height);

	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_DISPLAY_FLOW_INFO, (void *)&meta_disp_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_DISPLAY_FLOW_INFO fail, ret=%d\n", ret);
		goto exit;
	}
	/* fill display flow info end */

	/* fill display idr ctrl start */
	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_DISP_IDR_CTRL, (void **)&pqu_idr_crtl_ptr);
	if (ret || pqu_idr_crtl_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_DISP_IDR_CTRL fail, ret=%d\n", ret);
		goto exit;
	}

	meta_disp_idr_crtl.mem_fmt = pqu_idr_crtl_ptr->mem_fmt;
	meta_disp_idr_crtl.width = pqu_idr_crtl_ptr->width;
	meta_disp_idr_crtl.height = pqu_idr_crtl_ptr->height;
	meta_disp_idr_crtl.index = pqu_idr_crtl_ptr->index;
	meta_disp_idr_crtl.crop.left = pqu_idr_crtl_ptr->crop.left;
	meta_disp_idr_crtl.crop.top = pqu_idr_crtl_ptr->crop.top;
	meta_disp_idr_crtl.crop.width = pqu_idr_crtl_ptr->crop.width;
	meta_disp_idr_crtl.crop.height = pqu_idr_crtl_ptr->crop.height;
	meta_disp_idr_crtl.path = _mtk_pq_common_set_pq_idr_input_path_meta(pqu_idr_crtl_ptr->path);
	meta_disp_idr_crtl.bypass_ipdma = pqu_idr_crtl_ptr->bypass_ipdma;
	meta_disp_idr_crtl.v_flip = pqu_idr_crtl_ptr->v_flip;
	meta_disp_idr_crtl.aid = pqu_idr_crtl_ptr->aid;

	/*fill debug info*/
	pq_dev->pq_debug.display_idr_ctrl.mem_fmt = pqu_idr_crtl_ptr->mem_fmt;
	pq_dev->pq_debug.display_idr_ctrl.width = pqu_idr_crtl_ptr->width;
	pq_dev->pq_debug.display_idr_ctrl.height = pqu_idr_crtl_ptr->height;
	pq_dev->pq_debug.display_idr_ctrl.index = pqu_idr_crtl_ptr->index;
	pq_dev->pq_debug.display_idr_ctrl.crop.left = pqu_idr_crtl_ptr->crop.left;
	pq_dev->pq_debug.display_idr_ctrl.crop.top = pqu_idr_crtl_ptr->crop.top;
	pq_dev->pq_debug.display_idr_ctrl.crop.width = pqu_idr_crtl_ptr->crop.width;
	pq_dev->pq_debug.display_idr_ctrl.crop.height = pqu_idr_crtl_ptr->crop.height;
	pq_dev->pq_debug.display_idr_ctrl.path = pqu_idr_crtl_ptr->path;
	pq_dev->pq_debug.display_idr_ctrl.bypass_ipdma = pqu_idr_crtl_ptr->bypass_ipdma;
	pq_dev->pq_debug.display_idr_ctrl.v_flip = pqu_idr_crtl_ptr->v_flip;
	pq_dev->pq_debug.display_idr_ctrl.aid = pqu_idr_crtl_ptr->aid;
	/*fill debug info end*/

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
	"meta_disp_idr_crtl : %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
	"mem_fmt = ", meta_disp_idr_crtl.mem_fmt,
	"width = ", meta_disp_idr_crtl.width,
	"height = ", meta_disp_idr_crtl.height,
	"index = ", meta_disp_idr_crtl.index,
	"crop.left = ", meta_disp_idr_crtl.crop.left,
	"crop.top = ", meta_disp_idr_crtl.crop.top,
	"crop.width = ", meta_disp_idr_crtl.crop.width,
	"crop.height = ", meta_disp_idr_crtl.crop.height,
	"path = ", meta_disp_idr_crtl.path,
	"bypass_ipdma = ", meta_disp_idr_crtl.bypass_ipdma,
	"v_flip = ", meta_disp_idr_crtl.v_flip,
	"aid = ", meta_disp_idr_crtl.aid);

	ret = mtk_pq_common_write_metadata_addr(
		&meta_buf, EN_PQ_METATAG_OUTPUT_DISP_IDR_CTRL, &meta_disp_idr_crtl);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_OUTPUT_DISP_IDR_CTRL failed, ret = %d\n"
				, ret);
		goto exit;
	}
	/* fill display idr ctrl end */
exit:
	return ret;
}


static int _mtk_pq_common_set_flow_control(struct mtk_pq_device *pq_dev, void *flowctrl)
{
	struct v4l2_pq_flow_control flowcontrol;

	memset(&flowcontrol, 0, sizeof(struct v4l2_pq_flow_control));
	memcpy(&flowcontrol, flowctrl, sizeof(struct v4l2_pq_flow_control));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[FC] flow control enable=%d\n", flowcontrol.enable);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[FC] flow control factor=%d\n", flowcontrol.factor);

	pq_dev->display_info.flowControl.enable = flowcontrol.enable;
	pq_dev->display_info.flowControl.factor = flowcontrol.factor;

	return 0;
}

static int _mtk_pq_common_set_aisr_active_win(struct mtk_pq_device *pq_dev, void *win)
{
	struct v4l2_pq_aisr_active_win active_win;

	memset(&active_win, 0, sizeof(struct v4l2_pq_aisr_active_win));
	memcpy(&active_win, win, sizeof(struct v4l2_pq_aisr_active_win));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[AISR] Active win enable=%d\n", active_win.bEnable);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[AISR] Active win x=%u, y=%u, w=%u, h=%u\n",
				active_win.x, active_win.y, active_win.width, active_win.height);

	pq_dev->display_info.aisrActiveWin.bEnable = active_win.bEnable;
	pq_dev->display_info.aisrActiveWin.x = active_win.x;
	pq_dev->display_info.aisrActiveWin.y = active_win.y;
	pq_dev->display_info.aisrActiveWin.width = active_win.width;
	pq_dev->display_info.aisrActiveWin.height = active_win.height;

	return 0;
}

static int _mtk_pq_common_set_pix_format_info(struct mtk_pq_device *pq_dev, void *bufferctrl)
{
	struct v4l2_pq_s_buffer_info bufferInfo;
	u32 source = 0;
	int ret = 0;

	if ((!pq_dev) || (!bufferctrl))
		return -ENOMEM;

	source = pq_dev->common_info.input_source;

	memset(&bufferInfo, 0, sizeof(struct v4l2_pq_s_buffer_info));
	memcpy(&bufferInfo, bufferctrl, sizeof(struct v4l2_pq_s_buffer_info));

	switch (bufferInfo.type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (IS_INPUT_SRCCAP(source)) {
			ret = mtk_pq_display_idr_set_pix_format_mplane_info(pq_dev, bufferctrl);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "idr_set_pix_format failed!\n");
				return ret;
			}
		} else if (IS_INPUT_B2R(source)) {
			ret = mtk_pq_display_b2r_set_pix_format_mplane_info(pq_dev, bufferctrl);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					   "b2r_set_pix_format failed!\n");
				return ret;
			}
		} else {
			ret = -EINVAL;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				   "unknown source %d!\n", source);
			goto ERR;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		mtk_pq_display_mdw_set_pix_format_mplane_info(pq_dev,
			bufferctrl);
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid buffer type %d!\n", bufferInfo.type);
		return -EINVAL;
	}

ERR:
	return ret;
}

static int _mtk_pq_common_set_pq_memory_index(struct mtk_pq_device *pq_dev, u8 *val)
{
	int ret = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (!pq_dev)
		return -ENOMEM;

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!");
		return -EINVAL;
	}
	pqdev->pq_memory_index = (uint8_t)*val;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "set pq memory index = %d\n",
	pqdev->pq_memory_index);

	return ret;
}

static enum hwreg_pq_pattern_position _mtk_pq_common_pattern_position_mapping(
	enum pq_pattern_position position)
{
	switch (position) {
	case PQ_PATTERN_POSITION_IP2:
		return HWREG_PQ_PATTERN_POSITION_IP2;
	case PQ_PATTERN_POSITION_NR_DNR:
		return HWREG_PQ_PATTERN_POSITION_NR_DNR;
	case PQ_PATTERN_POSITION_NR_IPMR:
		return HWREG_PQ_PATTERN_POSITION_NR_IPMR;
	case PQ_PATTERN_POSITION_NR_OPM:
		return HWREG_PQ_PATTERN_POSITION_NR_OPM;
	case PQ_PATTERN_POSITION_NR_DI:
		return HWREG_PQ_PATTERN_POSITION_NR_DI;
	case PQ_PATTERN_POSITION_DI:
		return HWREG_PQ_PATTERN_POSITION_DI;
	case PQ_PATTERN_POSITION_SRS_IN:
		return HWREG_PQ_PATTERN_POSITION_SRS_IN;
	case PQ_PATTERN_POSITION_SRS_OUT:
		return HWREG_PQ_PATTERN_POSITION_SRS_OUT;
	case PQ_PATTERN_POSITION_VOP:
		return HWREG_PQ_PATTERN_POSITION_VOP;
	case PQ_PATTERN_POSITION_IP2_POST:
		return HWREG_PQ_PATTERN_POSITION_IP2_POST;
	case PQ_PATTERN_POSITION_SCIP_DV:
		return HWREG_PQ_PATTERN_POSITION_SCIP_DV;
	case PQ_PATTERN_POSITION_SCDV_DV:
		return HWREG_PQ_PATTERN_POSITION_SCDV_DV;
	case PQ_PATTERN_POSITION_B2R_DMA:
		return HWREG_PQ_PATTERN_POSITION_B2R_DMA;
	case PQ_PATTERN_POSITION_B2R_LITE1_DMA:
		return HWREG_PQ_PATTERN_POSITION_B2R_LITE1_DMA;
	case PQ_PATTERN_POSITION_B2R_PRE:
		return HWREG_PQ_PATTERN_POSITION_B2R_PRE;
	case PQ_PATTERN_POSITION_B2R_POST:
		return HWREG_PQ_PATTERN_POSITION_B2R_POST;
	case PQ_PATTERN_POSITION_VIP_IN:
		return HWREG_PQ_PATTERN_POSITION_VIP_IN;
	case PQ_PATTERN_POSITION_VIP_OUT:
		return HWREG_PQ_PATTERN_POSITION_VIP_OUT;
	case PQ_PATTERN_POSITION_HDR10:
		return HWREG_PQ_PATTERN_POSITION_HDR10;
	case PQ_PATTERN_POSITION_B2R_PRE_1:
		return HWREG_PQ_PATTERN_POSITION_B2R_PRE_1;
	case PQ_PATTERN_POSITION_B2R_PRE_2:
		return HWREG_PQ_PATTERN_POSITION_B2R_PRE_2;
	case PQ_PATTERN_POSITION_B2R_POST_1:
		return HWREG_PQ_PATTERN_POSITION_B2R_POST_1;
	case PQ_PATTERN_POSITION_B2R_POST_2:
		return HWREG_PQ_PATTERN_POSITION_B2R_POST_2;
	case PQ_PATTERN_POSITION_MDW:
		return HWREG_PQ_PATTERN_POSITION_MDW;
	case PQ_PATTERN_POSITION_DW_HW5POST:
		return HWREG_PQ_PATTERN_POSITION_DW_HW5POST;
	case PQ_PATTERN_POSITION_XVYCC:
		return HWREG_PQ_PATTERN_POSITION_XVYCC;
	default:
		return HWREG_PQ_PATTERN_POSITION_MAX;
	}

}

static int _mtk_pq_common_pattern_get_cap(struct stage_pattern_info *pattern_info)
{
	int ret = 0;
	struct hwreg_pq_pattern_capability pattern_cap;

	if (!pattern_info) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN, "Invalid argument!\n");
		ret = -EINVAL;
		goto ERR;
	}

	pattern_cap.position = _mtk_pq_common_pattern_position_mapping(pattern_info->position);

	ret = drv_hwreg_pq_pattern_get_capability(&pattern_cap);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
			"Get VOP capability fail, ret = %d!\n", ret);
		ret = -EINVAL;
		goto ERR;
	} else {
		if (pattern_cap.pattern_type == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
				"VOP not support\n");
			ret = -ENODEV;
			goto ERR;
		}
	}

ERR:
	return ret;

}

static int _mtk_pq_common_pattern_set_pure_color(
	struct stage_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	struct hwreg_pq_pattern_pure_color data;

	if ((!pattern_info) || (!pattern_reg)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN, "Invalid argument!\n");
		ret = -EINVAL;
		goto ERR;
	}

	memset(&data, 0, sizeof(struct hwreg_pq_pattern_pure_color));

	data.common.position = (enum hwreg_pq_pattern_position)(pattern_info->position);
	data.common.enable = pattern_info->enable;
	data.common.color_space =
		(enum hwreg_pq_pattern_color_space)(pattern_info->color_space);
	data.color.red = pattern_info->color.red;
	data.color.green = pattern_info->color.green;
	data.color.blue = pattern_info->color.blue;

	ret = drv_hwreg_pq_pattern_set_pure_color(true, &data, pattern_reg);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
			"set VOP pattern fail, ret = %d!\n", ret);
		ret = -EINVAL;
		goto ERR;
	}

ERR:
	return ret;

}

static int _mtk_pq_common_pattern_set_pure_colorbar(
	struct stage_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	struct hwreg_pq_pattern_pure_colorbar data;

	if ((!pattern_info) || (!pattern_reg)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN, "Invalid argument!\n");
		ret = -EINVAL;
		goto ERR;
	}

	memset(&data, 0, sizeof(struct hwreg_pq_pattern_pure_colorbar));

	data.common.position = (enum hwreg_pq_pattern_position)(pattern_info->position);
	data.common.enable = pattern_info->enable;
	data.common.color_space =
		(enum hwreg_pq_pattern_color_space)(pattern_info->color_space);
	data.diff_h = PTN_H_SIZE;
	data.diff_v = PTN_V_SIZE;

	ret = drv_hwreg_pq_pattern_set_pure_colorbar(true, &data, pattern_reg);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
			"set NR_OPM/SRS_OUT pattern fail, ret = %d!\n", ret);
		ret = -EINVAL;
		goto ERR;
	}

ERR:
	return ret;
}

static int _mtk_pq_common_set_stage_pattern(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	struct hwregOut pattern_reg;
	struct stage_pattern_info stage_pat_info;
	enum pq_pattern_position position;

	memset(&pattern_reg, 0, sizeof(struct hwregOut));
	PQ_MALLOC(pattern_reg.reg, sizeof(struct reg_info) * PATTERN_REG_NUM_MAX);
	if (pattern_reg.reg == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN, "PQ_MALLOC pattern reg fail!\n");
		ret = -EINVAL;
		goto ERR;
	}
	pattern_reg.regCount = 0;
	memset(&stage_pat_info, 0, sizeof(struct stage_pattern_info));
	memcpy(&stage_pat_info, ctrl, sizeof(struct stage_pattern_info));
	position = stage_pat_info.position;

	switch (position) {
	case PQ_PATTERN_POSITION_VOP:
		ret = _mtk_pq_common_pattern_get_cap(&stage_pat_info);
		if (ret) {
			if (ret == -ENODEV) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"VOP pattern Not support, ret = %d!\n", ret);
				goto ERR;
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"set VOP pattern fail, ret = %d!\n", ret);
				goto ERR;
			}
		}

		ret = _mtk_pq_common_pattern_set_pure_color(&stage_pat_info, &pattern_reg);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
				"set VOP pattern fail, ret = %d!\n", ret);
			ret = -EINVAL;
			goto ERR;
		}
		break;
	case PQ_PATTERN_POSITION_NR_OPM:
		ret = _mtk_pq_common_pattern_get_cap(&stage_pat_info);
		if (ret) {
			if (ret == -ENODEV) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"NR_OPM pattern Not support, ret = %d!\n", ret);
				goto ERR;
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"set NR_OPM pattern fail, ret = %d!\n", ret);
				goto ERR;
			}
		}

		ret = _mtk_pq_common_pattern_set_pure_colorbar(&stage_pat_info, &pattern_reg);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
				"set NR_OPM pattern fail, ret = %d!\n", ret);
			ret = -EINVAL;
			goto ERR;
		}
		break;
	case PQ_PATTERN_POSITION_SRS_OUT:
		ret = _mtk_pq_common_pattern_get_cap(&stage_pat_info);
		if (ret) {
			if (ret == -ENODEV) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"SRS_OUT pattern Not support, ret = %d!\n", ret);
				goto ERR;
			} else {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
					"set SRS_OUT pattern fail, ret = %d!\n", ret);
				goto ERR;
			}
		}

		ret = _mtk_pq_common_pattern_set_pure_colorbar(&stage_pat_info, &pattern_reg);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
				"set SRS_OUT pattern fail, ret = %d!\n", ret);
			ret = -EINVAL;
			goto ERR;
		}
		break;
	default:
		ret = -EINVAL;
		goto ERR;
	}

ERR:
	PQ_FREE(pattern_reg.reg, sizeof(struct reg_info) * PATTERN_REG_NUM_MAX);
	return ret;
}

int mtk_pq_common_get_dma_buf(struct mtk_pq_device *pq_dev,
	struct mtk_pq_dma_buf *buf)
{
	struct scatterlist *sg = NULL;

	if (!pq_dev || !buf || !buf->dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (buf->fd < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid dma fd::%d!\n", buf->fd);
		return -EINVAL;
	}

	if (buf->valid == true) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"dma buf (fd = %d) has been allocated!, file cnt = %ld\n",
			buf->fd, file_count(buf->db->file));
		return 0;
	}

	buf->db = dma_buf_get(buf->fd);
	if (IS_ERR_OR_NULL(buf->db)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_get fail!\n");
		return -EINVAL;
	}

	buf->size = buf->db->size;

	buf->va = dma_buf_vmap(buf->db);
	if (buf->va == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_vmap fail\n");
		goto vmap_fail;
	}

	buf->attach = dma_buf_attach(buf->db, buf->dev);
	if (IS_ERR(buf->attach)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_attach fail!\n");
		goto attach_fail;
	}

	buf->sgt = dma_buf_map_attachment(buf->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->sgt)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_map_attachment fail!\n");
		goto map_attachment_fail;
	}

	sg = buf->sgt->sgl;
	if (sg == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "sgl is NULL!\n");
		goto dms_address_fail;
	}

	buf->pa = page_to_phys(sg_page(sg)) + sg->offset;

	buf->iova = sg_dma_address(sg);
	if (buf->iova < 0x200000000) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Error iova=0x%llx\n", buf->iova);
		goto dms_address_fail;
	}

	if (bPquEnable) {
		unsigned long pa = 0;

		if (_mtk_pq_common_alloc_share_memory(&pa, buf->size)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_common_alloc_share_memory fail\n");
			return -ENOMEM;
		}
		buf->pa = pa;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"fd = %d, va = 0x%llx, pa = 0x%llx, iova=0x%llx, size=0x%llx, file cnt = %ld\n",
		buf->fd, (unsigned long long)buf->va, buf->pa, buf->iova, buf->size,
		file_count(buf->db->file));

	buf->valid = true;

	return 0;

dms_address_fail:
	dma_buf_unmap_attachment(buf->attach, buf->sgt, DMA_BIDIRECTIONAL);
map_attachment_fail:
	dma_buf_detach(buf->db, buf->attach);
attach_fail:
	dma_buf_vunmap(buf->db, buf->va);
vmap_fail:
	dma_buf_put(buf->db);

	return -EINVAL;
}

int mtk_pq_common_put_dma_buf(struct mtk_pq_dma_buf *buf)
{
	if ((!buf) || (buf->va == NULL) || !(buf->db && buf->attach && buf->sgt)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid params!\n");
		return -EINVAL;
	}

	if (buf->valid == false) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"dma buf (fd = %d) has been released!\n",
			buf->fd);
		return 0;
	}

	buf->valid = false;

	dma_buf_unmap_attachment(buf->attach, buf->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(buf->db, buf->attach);
	dma_buf_vunmap(buf->db, buf->va);
	dma_buf_put(buf->db);

	if (bPquEnable)
		gen_pool_free(pool, buf->pa, buf->size);

	buf->va = NULL;
	buf->pa = NULL;
	buf->iova = 0;
	buf->size = 0;
	buf->db = NULL;
	buf->attach = NULL;
	buf->sgt = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "puf dma buf (fd = %d) success!\n", buf->fd);

	return 0;
}

static int _mtk_pq_common_create_share_memory_pool(unsigned long addr, size_t size)
{
	if (pool != NULL)
		return 0;

	spin_lock_init(&_frame_metadata_info_lock);

	pool = gen_pool_create(ALLOC_ORDER, NODE_ID);
	if (pool == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Failed to gen_pool_create\n");
		return -1;
	}

	return gen_pool_add(pool, addr, size, -1);
}

static int _mtk_pq_common_alloc_share_memory(unsigned long *paddr, size_t size)
{
	if (paddr == NULL || pool == NULL)
		return -1;

	*paddr = gen_pool_alloc(pool, size);

	return 0;
}

static int _mtk_pq_common_get_frame(struct msg_queue_info *msg_info)
{
	struct mtk_pq_common_metadata_node *node = NULL;
	struct list_head *np, *n;
	int ret = 0;
	void *va = NULL;
	unsigned long pa = 0;
	size_t size = 0;

	if (msg_info == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Null pointer\n");
		return -1;
	}

	spin_lock(&_frame_metadata_info_lock);

	size = msg_info->meta_size;

	/* Find unused metadata */
	list_for_each_safe(np, n, &_frame_metadata_list) {
		node = list_entry(np, struct mtk_pq_common_metadata_node, list);
		if (node && node->used == false && node->size == size) {
			node->used = true;
			pa = node->physical_addr;
			va = node->virtual_addr;
			msg_info->meta_pa = pa;
			memcpy(&node->queue_info, msg_info, sizeof(struct msg_queue_info));
			ret = 0;
			goto Unlock;
		}
	}

	/* If no unused, alloc new node */
	if (_mtk_pq_common_alloc_share_memory(&pa, size) || pa == 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_alloc_share_memory fail 0x%lx\n", pa);
		ret = -1;
		goto Unlock;
	}

	node = vmalloc(sizeof(struct mtk_pq_common_metadata_node));
	if (node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Out of memory.\n");
		ret = -1;
		goto Unlock;
	}

	/* Add to list */
	node->physical_addr = pa;
	node->virtual_addr = ioremap(pa, size);
	node->size = size;
	node->used = true;
	msg_info->meta_pa = pa;
	memcpy(&node->queue_info, msg_info, sizeof(struct msg_queue_info));
	if (node->virtual_addr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "ioremap fail phy : 0x%lx virtual : 0x%p\n",
			node->physical_addr, node->virtual_addr);
		vfree(node);
		ret = -1;
		goto Unlock;
	}
	list_add_tail(&node->list, &_frame_metadata_list);

	va = node->virtual_addr;
	ret = 0;

Unlock:
	spin_unlock(&_frame_metadata_info_lock);
	if (ret == 0 && size > 0 && va)
		memcpy_toio(va, (void *)msg_info->meta_va, size);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "va = 0x%p pa = 0x%lx size = 0x%zx\n", va, pa, size);
	return ret;
}

static void *_mtk_pq_common_find_frame_metadata_va(unsigned long pa)
{
	struct list_head *np, *n;
	struct mtk_pq_common_metadata_node *node = NULL;
	void *virtual_addr = NULL;

	if (pa == 0)
		return NULL;

	spin_lock(&_frame_metadata_info_lock);

	/* Try to find match metadata */
	list_for_each_safe(np, n, &_frame_metadata_list) {
		node = list_entry(np, struct mtk_pq_common_metadata_node, list);
		if (node && node->physical_addr == pa && node->used == true) {
			virtual_addr = node->virtual_addr;
			goto Unlock;
		}
	}

Unlock:
	spin_unlock(&_frame_metadata_info_lock);
	return virtual_addr;
}

static void _mtk_pq_common_release_frame_metadata(unsigned long pa)
{
	struct list_head *np, *n;
	struct mtk_pq_common_metadata_node *node = NULL;

	spin_lock(&_frame_metadata_info_lock);

	/* Find match metadata and mark unused */
	list_for_each_safe(np, n, &_frame_metadata_list) {
		node = list_entry(np, struct mtk_pq_common_metadata_node, list);
		if (node && node->physical_addr == pa) {
			node->used = false;
			goto Unlock;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "FATAL release metadata fail!!\n");

Unlock:
	spin_unlock(&_frame_metadata_info_lock);
}

static void _mtk_pq_common_release_all_frame(void)
{
	struct list_head *np, *n;
	struct mtk_pq_common_metadata_node *node = NULL;
	struct mtk_pq_common_dequeue_node *dq_node = NULL, *tmp_node = NULL;
	LIST_HEAD(temp_list);

	/* PQU RV55 only */
	if (bPquEnable == false)
		return;

	spin_lock(&_frame_metadata_info_lock);

	list_for_each_safe(np, n, &_frame_metadata_list) {
		node = list_entry(np, struct mtk_pq_common_metadata_node, list);
		if (node && node->used == true) {
			dq_node = vmalloc(sizeof(struct mtk_pq_common_dequeue_node));
			if (dq_node == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Out of memory.\n");
				goto unlock;
			}
			dq_node->dq_frame.win_id = node->queue_info.win_id;
			dq_node->dq_frame.frame_type = node->queue_info.frame_type;
			dq_node->dq_frame.dq_status = 1;
			dq_node->dq_frame.meta_va = node->queue_info.meta_pa;
			dq_node->dq_frame.meta_size = node->queue_info.meta_size;
			dq_node->dq_frame.meta_host_va = node->queue_info.meta_va;
			dq_node->dq_frame.priv = node->queue_info.priv;
			dq_node->dq_frame.InValidFrame = 1;

			list_add_tail(&dq_node->list, &temp_list);

			node->used = false;
		}
	}

unlock:
	spin_unlock(&_frame_metadata_info_lock);

	/* Because mtk_pq_common_dqbuf_cb also use the same lock,
	 * use a temp list to keep dequeue info and call the function.
	 */
	list_for_each_entry_safe(dq_node, tmp_node, &temp_list, list) {
		if (dq_node) {
			mtk_pq_common_dqbuf_cb(0, 0, 0, &dq_node->dq_frame);
			list_del(&dq_node->list);
			vfree(dq_node);
		}
	}
}

static int _mtk_pq_common_add_stream_info(struct msg_stream_on_info *msg_info)
{
	int ret = 0;
	struct mtk_pq_common_stream_node *node = NULL;

	if (msg_info == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	node = vmalloc(sizeof(struct mtk_pq_common_stream_node));
	if (node == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Out of memory.\n");
		ret = -ENOMEM;
		return ret;
	}
	memcpy(&node->stream_info, msg_info, sizeof(struct msg_stream_on_info));
	spin_lock(&_stream_lock);
	list_add_tail(&node->list, &_stream_list);
	spin_unlock(&_stream_lock);

	ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Add stream info win=%u type=%u\n",
				msg_info->win_id,
				msg_info->stream_type);

	return ret;
}

static int _mtk_pq_common_get_stream_info(struct msg_stream_off_done_info *msg_info)
{
	int ret = 0;
	struct mtk_pq_common_stream_node *node = NULL, *n = NULL;

	if (msg_info == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	spin_lock(&_stream_lock);

	list_for_each_entry_safe(node, n, &_stream_list, list) {
		if (node &&
			node->stream_info.win_id == msg_info->win_id &&
			node->stream_info.stream_type == msg_info->stream_type) {
			msg_info->stream_meta_va = node->stream_info.stream_meta_va;
			msg_info->stream_meta_pa = node->stream_info.stream_meta_pa;
			msg_info->stream_meta_size = node->stream_info.stream_meta_size;
			msg_info->status = 0;
			list_del(&node->list);
			vfree(node);
			ret = 0;

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Get stream info win=%u type=%u\n",
				msg_info->win_id,
				msg_info->stream_type);
			goto unlock;
		}
	}
	ret = -ENOENT;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "No stream info exist win=%u type=%u\n",
		msg_info->win_id,
		msg_info->stream_type);

unlock:
	spin_unlock(&_stream_lock);
	return ret;
}

static void _mtk_pq_common_release_stream(uint8_t win_id, uint8_t stream_type)
{
	struct mtk_pq_common_stream_node *node = NULL, *n = NULL;
	struct msg_stream_off_done_info stream_info = {0};

	/* PQU RV55 only */
	if (bPquEnable == false)
		return;

	spin_lock(&_stream_lock);

	list_for_each_entry_safe(node, n, &_stream_list, list) {
		if (node &&
			node->stream_info.win_id == win_id &&
			node->stream_info.stream_type == stream_type) {
			stream_info.win_id = node->stream_info.win_id;
			stream_info.status = 0;
			stream_info.stream_type = node->stream_info.stream_type;
			stream_info.stream_meta_va = node->stream_info.stream_meta_va;
			stream_info.stream_meta_pa = node->stream_info.stream_meta_pa;
			stream_info.stream_meta_size = node->stream_info.stream_meta_size;
			mtk_pq_common_stream_off_cb(0, 0, 0, &stream_info);

			list_del(&node->list);
			vfree(node);

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Release stream win=%u type=%u\n",
				stream_info.win_id,
				stream_info.stream_type);
		}
	}

	spin_unlock(&_stream_lock);
}

static void _ready_cb_pqu_video_new_win(void)
{
	int ret = 0;
	struct msg_new_win_done_info reply_info = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_new_win((void *)&_new_win_msg_info[_CB_DEC(_new_win_count)], &reply_info);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_new_win fail!\n");
}

static int _mtk_pq_common_open(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	struct msg_new_win_done_info reply_info = {0};
	struct msg_new_win_info msg_info;

	memset(&msg_info, 0, sizeof(struct msg_new_win_info));

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	msg_info.win_id = (__u8)pq_dev->dev_indx;
	msg_info.path = (__u8)pq_dev->common_info.pq_path;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter common open, dev_indx=%u! bPquEnable=%d\n",
		pq_dev->dev_indx, bPquEnable);

	if (bPquEnable) {
		ret = pqu_video_new_win(&msg_info, &reply_info);
		if (ret == -ENODEV) {
			memcpy(&_new_win_msg_info[_CB_INC(_new_win_count)],
				&msg_info,
				sizeof(struct msg_new_win_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_new_win register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_video_new_win);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_new_win fail (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_NEW_WIN, (void *)&msg_info);
	}

	return ret;
}

static int _mtk_pq_common_close(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	unsigned int timeout = 0;
	struct msg_destroy_win_done_info reply_info = {0};
	struct msg_destroy_win_info msg_info;

	memset(&msg_info, 0, sizeof(struct msg_destroy_win_info));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"enter common close, dev_indx=%u! bPquEnable=%d\n", pq_dev->dev_indx, bPquEnable);

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	msg_info.win_id = pq_dev->dev_indx;

	if (bPquEnable) {
		ret = pqu_video_destroy_win(&msg_info, &reply_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_destroy_win fail! (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_DESTROY_WIN, (void *)&msg_info);

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Waiting for PQU close win done!\n");

		timeout = msecs_to_jiffies(PQU_TIME_OUT_MS);
		ret = wait_event_timeout(pq_dev->wait,
			pq_dev->event_flag & PQU_CLOSE_WIN_DONE, timeout);
		if (ret == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Wait PQU close win time out(%ums)!\n", PQU_TIME_OUT_MS);
			return -ETIMEDOUT;
		}

		pq_dev->event_flag &= ~PQU_CLOSE_WIN_DONE;
	}
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "PQU close win done!\n");

	return 0;
}

int mtk_pq_common_set_path_enable(struct mtk_pq_device *pq_dev, enum v4l2_pq_path pq_path)
{
	int ret = 0;
	struct mtk_pq_platform_device *pdev = NULL;

	pdev = dev_get_drvdata(pq_dev->dev);

	mutex_lock(&pq_dev->mutex_set_path);

	if (pq_dev->bSetPath == false) {
		pq_dev->bSetPath = true;
		pq_dev->path = pq_path;
	} else {
		mutex_unlock(&pq_dev->mutex_set_path);
		return ret;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pq open, pq_path=%d isEnableRv55PquCurrent=%d\n",
		       pq_path, isEnableRv55PquCurrent);
	if (pq_path == V4L2_PQ_PATH_PQ) {
		if (bPquEnable) {
			if (pq_dev->qbuf_meta_pool_addr == 0 || pq_dev->qbuf_meta_pool_size == 0) {
				ret = mtk_pq_get_dedicate_memory_info(
					pqdev->dev, MMAP_METADATA_INDEX,
					&pq_dev->qbuf_meta_pool_addr, &pq_dev->qbuf_meta_pool_size);
				if (ret) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Failed to _mtk_pq_get_memory_info(index=%d)\n",
					MMAP_METADATA_INDEX);
					goto exit;
				}
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"qbuf_meta_pool_addr = 0x%llx qbuf_meta_pool_size = 0x%x\n",
				pq_dev->qbuf_meta_pool_addr, pq_dev->qbuf_meta_pool_size);
			}
		}
	}

	pq_dev->common_info.pq_path = pq_path;

	ret = _mtk_pq_common_open(pq_dev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_open fail!\n");
			goto exit;
	}

exit:
	mutex_unlock(&pq_dev->mutex_set_path);


	return ret;
}

int mtk_pq_common_set_path_disable(struct mtk_pq_device *pq_dev, enum v4l2_pq_path pq_path)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pq close\n");

	mutex_lock(&pq_dev->mutex_set_path);

	if ((pq_dev->bSetPath == true) && (pq_dev->path == pq_path)) {
		pq_dev->bSetPath = false;
		pq_dev->path = V4L2_PQ_PATH_NONE;
	} else {
		mutex_unlock(&pq_dev->mutex_set_path);
		return ret;
	}

	ret = _mtk_pq_common_close(pq_dev);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pq_common_close fail!\n");
		goto exit;
	}

exit:
	mutex_unlock(&pq_dev->mutex_set_path);
	return ret;
}

static void _ready_cb_pqu_video_stream_on(void)
{
	int ret = 0;
	struct callback_info stream_on_callback_info = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_stream_on((void *)&_stream_on_msg_info[_CB_DEC(_stream_on_count)],
			&stream_on_callback_info);
	if (ret)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_stream_on fail!\n");

}

static void _mtk_pq_common_stream_on_new_win(struct msg_stream_on_info *msg_info)
{
	int ret = 0;
	struct callback_info stream_on_callback_info = {0};

	if (bPquEnable) {
		ret = _mtk_pq_common_add_stream_info(msg_info);
		if (ret)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_common_add_stream_info fail (ret=%d)\n", ret);
		ret = pqu_video_stream_on(msg_info, &stream_on_callback_info);
		if (ret == -ENODEV) {
			memcpy(&_stream_on_msg_info[_CB_INC(_stream_on_count)],
				msg_info,
				sizeof(struct msg_stream_on_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_stream_on register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_video_stream_on);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_stream_on fail (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_STREAM_ON, (void *)msg_info);
	}
}

static void _mtk_pq_common_b2r_init(__u8 input_source, struct mtk_pq_device *pq_dev)
{
	int ret = 0;

	if (IS_INPUT_B2R(input_source)) {
		ret = mtk_pq_b2r_init(pq_dev);
		if (ret < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"MTK PQ : b2r init failed\n");
		}
	}
}

static void _mtk_pq_common_b2r_exit(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	__u8 input_source = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	input_source = pq_dev->common_info.input_source;

	if (IS_INPUT_B2R(input_source)) {
		ret = mtk_pq_b2r_exit(pq_dev);
		if (ret < 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"MTK PQ : b2r exit failed\n");
		}
	}
}

static int _mtk_pq_source_type_convert(int input_source)
{
	if (IS_INPUT_HDMI(input_source))
		return pqu_source_type_hdmi;
	else if (IS_INPUT_VGA(input_source))
		return pqu_source_type_vga;
	else if (IS_INPUT_ATV(input_source))
		return pqu_source_type_atv;
	else if (IS_INPUT_YPBPR(input_source))
		return pqu_source_type_ypbpr;
	else if (IS_INPUT_CVBS(input_source))
		return pqu_source_type_cvbs;
	else if (IS_INPUT_DTV(input_source))
		return pqu_source_type_dtv;
	else if (IS_INPUT_MM(input_source))
		return pqu_source_type_mm;
	else
		return pqu_source_type_max;
}

int mtk_pq_common_stream_on(struct file *file, int type,
	struct mtk_pq_device *pq_dev,
	struct msg_stream_on_info *msg_info)
{
	int ret = 0;
	struct meta_pq_stream_info *stream_meta = NULL;
	struct meta_pq_stream_internal_info internal_info;
	struct pq_buffer pq_buf;
	enum pqu_buffer_type buf_idx;
	enum pqu_buffer_channel buf_ch;
	struct meta_buffer pqu_meta_buf;
	struct m_pq_common_stream_info stream_info;
	struct mtk_pq_platform_device *pqdev = NULL;
	__u8 input_source = 0;
	bool ip_enable = false;

	if (!file || !pq_dev || !msg_info) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	stream_meta = &(pq_dev->stream_meta);

	memset(&pqu_meta_buf, 0, sizeof(struct meta_buffer));
	memset(&internal_info, 0, sizeof(struct meta_pq_stream_internal_info));
	memset(&stream_info, 0, sizeof(struct m_pq_common_stream_info));

	pqdev = dev_get_drvdata(pq_dev->dev);
	input_source = pq_dev->common_info.input_source;
	pq_dev->common_info.first_slice_frame = true;

	pqu_meta_buf.paddr = (unsigned char *)pq_dev->pqu_stream_dma_info.va;
	pqu_meta_buf.size = (unsigned int)pq_dev->pqu_stream_dma_info.size;

	if (!pq_dev->stream_on_ref) {
		memset((void *)pq_dev->pqu_stream_dma_info.va, 0, pq_dev->pqu_stream_dma_info.size);

		internal_info.file = (u64)file;
		internal_info.pq_dev = (u64)pq_dev;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"%s%llx, %s%llx\n",
			"file = 0x", internal_info.file,
			"pq_dev = 0x", internal_info.pq_dev);

		ret = mtk_pq_common_write_metadata_addr(&pqu_meta_buf,
			EN_PQ_METATAG_STREAM_INTERNAL_INFO, (void *)&internal_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq write EN_PQ_METATAG_STREAM_INTERNAL_INFO Failed, ret = %d\n"
					, ret);
			return ret;
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"mtk-pq write EN_PQ_METATAG_STREAM_INTERNAL_INFO success\n");
		}

		mtk_pq_common_set_path_enable(pq_dev, V4L2_PQ_PATH_PQ);
	}
	pq_dev->stream_on_ref++;
	/* enable b2r clock */
	if (V4L2_TYPE_IS_OUTPUT(type))
		_mtk_pq_common_b2r_init(input_source, pq_dev);

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		stream_info.input_source	= input_source;//pq_dev->common_info.input_source;
		stream_info.source_type = _mtk_pq_source_type_convert(input_source);
		stream_info.width		   = stream_meta->width;
		stream_info.height		  = stream_meta->height;
		stream_info.interlace	   = stream_meta->interlace;
		stream_info.adaptive_stream = stream_meta->adaptive_stream;
		stream_info.low_latency	 = stream_meta->low_latency;
		stream_info.v_flip		  = pq_dev->common_info.v_flip;
		stream_info.source		  = (enum pqu_source)stream_meta->pq_source;
		stream_info.mode			= (enum pqu_mode)stream_meta->pq_mode;
		stream_info.scene		   = (enum pqu_scene)stream_meta->pq_scene;
		stream_info.framerate	   = (enum pqu_framerate)stream_meta->pq_framerate;
		stream_info.colorformat	 = (enum pqu_colorformat)stream_meta->pq_colorformat;
		stream_info.align_info.di_h = pqdev->display_dev.di_align;
		stream_info.align_info.pre_hvsp_h = pqdev->display_dev.pre_hvsp_align;
		stream_info.align_info.hvsp_h = pqdev->display_dev.hvsp_align;
		stream_info.align_info.post_hvsp_h = pqdev->display_dev.post_hvsp_align;
		stream_info.aisr_support	= ((bool)pqdev->pqcaps.u32AISR_Support);
		stream_info.phase_ip2	   = pqdev->pqcaps.u32Phase_IP2;
		stream_info.stream_b2r.b2r_version	= pqdev->b2r_dev.b2r_ver;
		stream_info.stream_b2r.h_max_size	= pqdev->b2r_dev.h_max_size;
		stream_info.stream_b2r.v_max_size	= pqdev->b2r_dev.v_max_size;
		stream_info.stream_b2r.rotate_support_h = pqdev->b2r_dev.rotate_support_h;
		stream_info.stream_b2r.rotate_support_v = pqdev->b2r_dev.rotate_support_v;
		stream_info.stream_b2r.rotate_interlace = pqdev->b2r_dev.rotate_interlace;
		stream_info.stream_b2r.rotate_support_vsd = pqdev->b2r_dev.rotate_support_vsd;
		stream_info.mdw_version	= pqdev->pqcaps.u32MDW_Version;
		stream_info.ip2_crop_version	= pqdev->pqcaps.u32IP2_Crop_Version;
		stream_info.vip_version = pqdev->pqcaps.u32VIP_Version;
		stream_info.scenario_ml_idx	= stream_meta->scenario_idx;
		stream_info.scenario_ds_idx	= stream_meta->scenario_idx;
		stream_info.p_engine_ip2 = pqdev->pqcaps.u32P_ENGINE_IP2;
		stream_info.p_engine_di = pqdev->pqcaps.u32P_ENGINE_DI;
		stream_info.p_engine_aisr = pqdev->pqcaps.u32P_ENGINE_AISR;
		stream_info.p_engine_scl2 = pqdev->pqcaps.u32P_ENGINE_SCL2;
		stream_info.p_engine_srs = pqdev->pqcaps.u32P_ENGINE_SRS;
		stream_info.p_engine_vip = pqdev->pqcaps.u32P_ENGINE_VIP;
		stream_info.p_engine_mdw = pqdev->pqcaps.u32P_ENGINE_MDW;
		stream_info.scmi_write_limit_support
			= ((bool)pqdev->pqcaps.u8Scmi_Write_Limit_Support);
		stream_info.ucm_write_limit_support
			= ((bool)pqdev->pqcaps.u8Ucm_Write_Limit_Support);

		stream_info.bForceP = stream_meta->bForceP;
		stream_info.bPureVideoPath = stream_meta->bPureVideoPath;

		stream_info.ip_path = stream_meta->ip_path;
		stream_info.op_path = stream_meta->op_path;
		stream_info.panel_window.x = stream_meta->panel_window.x;
		stream_info.panel_window.y = stream_meta->panel_window.y;
		stream_info.panel_window.width = stream_meta->panel_window.width;
		stream_info.panel_window.height = stream_meta->panel_window.height;

		stream_info.bvdec_statistic_en = stream_meta->bvdec_statistic_en;

		stream_info.ip_diff = 0;
		stream_info.stub = stream_meta->stub;

		stream_info.idr_input_mode = pq_dev->display_info.idr.input_mode;

		stream_info.idr_input_path = _mtk_pq_common_set_pqu_idr_input_path_meta(
								stream_meta->pq_idr_input_path);
		stream_info.idr_sw_mode_supported = pqdev->pqcaps.u32Idr_SwMode_Support;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"idr_input_mode:%d, idr_input_path:%d\n",
			stream_info.idr_input_mode, stream_info.idr_input_path);

		/* meta get pq buf info */
		for (buf_idx = PQU_BUF_SCMI; buf_idx < PQU_BUF_MAX; buf_idx++) {
			mtk_get_pq_buffer(pq_dev, buf_idx, &pq_buf);

			if (pq_buf.valid == true) {
				for (buf_ch = PQU_BUF_CH_0; buf_ch < PQU_BUF_CH_MAX; buf_ch++) {
					stream_info.pq_buf[buf_idx].addr[buf_ch] =
									pq_buf.addr_ch[buf_ch];
					stream_info.pq_buf[buf_idx].size[buf_ch] =
									pq_buf.size_ch[buf_ch];
					stream_info.pq_buf[buf_idx].frame_num =
									pq_buf.frame_num;
					stream_info.pq_buf[buf_idx].frame_diff =
									pq_buf.frame_diff;
					stream_info.pq_buf[buf_idx].frame_delay =
									pq_buf.frame_delay;
				}

				if (pq_buf.frame_delay != 0)
					ip_enable = true;
				else
					ip_enable = false;

				if (buf_idx == PQU_BUF_SCMI) {
					stream_info.opf_diff += pq_buf.frame_delay;
					stream_info.opb_diff += pq_buf.frame_delay;
					stream_info.op2_diff += pq_buf.frame_delay;
					stream_info.scmi_enable = true; //scmi diff = scmi delay
				} else if (buf_idx == PQU_BUF_UCM) {
					stream_info.opb_diff += pq_buf.frame_delay;
					stream_info.op2_diff += pq_buf.frame_delay;
					stream_info.ucm_enable =
						(pqdev->pqcaps.u32ZNR_Support) ? ip_enable : false;
				}
			} else {
				/* clear plane info */
				memset(&(stream_info.pq_buf[buf_idx]),
							0, sizeof(struct buffer_info));
			}
		}

		pq_dev->common_info.op2_diff = stream_info.op2_diff;
		stream_info.h_max_size = pqdev->display_dev.h_max_size;
		stream_info.v_max_size = pqdev->display_dev.v_max_size;

		stream_info.znr_me_h_max_size = pqdev->display_dev.znr_me_h_max_size;
		stream_info.znr_me_v_max_size = pqdev->display_dev.znr_me_v_max_size;
		stream_info.hvsp_12tap_size = pqdev->display_dev.hvsp_12tap_size;
		stream_info.spf_vtap_size = pqdev->display_dev.spf_vtap_size;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"spf_vtap_size = %d\n",
			stream_info.spf_vtap_size);

		stream_info.out_buf_ctrl_mode =
			(enum pqu_buf_ctrl_mode)pq_dev->common_info.output_mode;

		/* to notify pqu update scmi opm rwdiff */
		stream_info.scmi_opm_diff_fixup = pq_dev->scmi_opm_diff_fixup;
		stream_info.scmi_protect_line = pq_dev->scmi_protect_line;

		/* cus01 customization*/
		stream_info.g9_mute = pqdev->pq_config_info.g9_mute;

		ret = mtk_pq_common_write_metadata_addr(&pqu_meta_buf,
			EN_PQ_METATAG_PQU_STREAM_INFO, (void *)&stream_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq write EN_PQ_METATAG_PQU_STREAM_INFO Failed, ret=%d\n", ret);
			goto put_dma_buf;
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"mtk-pq write EN_PQ_METATAG_PQU_STREAM_INFO success\n");
		}
	}

	msg_info->win_id = pq_dev->dev_indx;
	msg_info->stream_meta_va = (u64)(pq_dev->pqu_stream_dma_info.va);
	msg_info->stream_meta_pa = (u64)(pq_dev->pqu_stream_dma_info.pa);
	msg_info->stream_meta_size = (u32)(pq_dev->pqu_stream_dma_info.size);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"dev_indx=%u,buffer type=%d,meta_va=0x%llx,meta_iova=0x%llx,meta_size=0x%x\n",
		pq_dev->dev_indx,
		type,
		msg_info->stream_meta_va,
		msg_info->stream_meta_pa,
		msg_info->stream_meta_size);

	/* send new win cmd to pqu */
	_mtk_pq_common_stream_on_new_win(msg_info);

	if (V4L2_TYPE_IS_OUTPUT(type))
		pq_dev->event_flag &= ~PQU_INPUT_STREAM_OFF_DONE;
	else
		pq_dev->event_flag &= ~PQU_OUTPUT_STREAM_OFF_DONE;

	ret = mtk_pq_common_create_task(pqdev, pq_dev, input_source);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_PQU_STREAM_INFO Failed, ret=%d\n",
			ret);
	}

	return ret;

put_dma_buf:
	if (pq_dev->stream_on_ref == 0)
		mtk_pq_common_put_dma_buf(&pq_dev->stream_dma_info);

	return ret;
}

static void _ready_cb_pqu_video_stream_off(void)
{
	int ret = 0;
	struct callback_info stream_off_callback_info = {0};

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	stream_off_callback_info.callback = mtk_pq_common_stream_off_cb;
	ret = pqu_video_stream_off(&_stream_off_msg_info[_CB_DEC(_stream_off_count)],
		&stream_off_callback_info);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_stream_off fail!\n");
}

int mtk_pq_common_stream_off(struct mtk_pq_device *pq_dev,
	int type, struct msg_stream_off_info *msg_info)
{
	int ret = 0;
	unsigned long timeout = 0;
	struct v4l2_event ev;
	struct mtk_pq_ctx *pq_ctx = NULL;
	struct v4l2_m2m_buffer *src_buf = NULL;
	struct v4l2_m2m_buffer *dst_buf = NULL;
	struct mtk_pq_buffer *pq_src_buf = NULL;
	struct mtk_pq_buffer *pq_dst_buf = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"enter common stream off, dev_indx=%u, buffer type = %d!\n",
		pq_dev->dev_indx, type);

	if (!pq_dev || !msg_info) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	/*debug info reset*/
	pq_dev->pq_debug.pts_idx = 0;
	pq_dev->pq_debug.drop_pts_idx = 0;
	pq_dev->pq_debug.total_que = 0;
	pq_dev->pq_debug.total_dque = 0;
	memset(pq_dev->pq_debug.drop_frame_pts, 0, DROP_ARY_IDX*sizeof(uint32_t));
	memset(pq_dev->pq_debug.queue_pts, 0, DROP_ARY_IDX*sizeof(uint32_t));
	/*debug info reset end*/
	memset(&ev, 0, sizeof(struct v4l2_event));

	msg_info->win_id = pq_dev->dev_indx;

	pq_ctx = pq_dev->m2m.ctx;

	pq_ctx->state = MTK_STATE_ABORT;

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		//dequeue input buffer in rdy_queue
		ev.type = V4L2_EVENT_MTK_PQ_INPUT_DONE;
		v4l2_m2m_for_each_src_buf(pq_ctx->m2m_ctx, src_buf) {
			if (src_buf == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Input Buffer Is NULL!\n");
				break;
			}

			pq_src_buf = (struct mtk_pq_buffer *)src_buf;

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"[input] frame fd = %d, meta fd = %d, index = %d!\n",
				pq_src_buf->frame_buf.fd, pq_src_buf->meta_buf.fd,
				pq_src_buf->vb.vb2_buf.index);

			vb2_buffer_done(&src_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
			ev.u.data[0] = src_buf->vb.vb2_buf.index;
			ev.u.data[EV_DATA_INVALID_FRMAE] = true;
			v4l2_event_queue(&(pq_dev->video_dev), &ev);
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Send DeQueue Input Buffer Event!\n");
		}
		INIT_LIST_HEAD(&pq_ctx->m2m_ctx->out_q_ctx.rdy_queue);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Clear Input Ready Queue Done!\n");

		mutex_lock(&pq_dev->mutex_stream_off);
		pq_dev->out_turing_stream_off = true;
		mutex_unlock(&pq_dev->mutex_stream_off);
	} else {
		//dequeue output buffer in rdy_queue
		ev.type = V4L2_EVENT_MTK_PQ_OUTPUT_DONE;
		v4l2_m2m_for_each_dst_buf(pq_ctx->m2m_ctx, dst_buf) {
			if (dst_buf == NULL) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Output Buffer Is NULL!\n");
				break;
			}

			pq_dst_buf = (struct mtk_pq_buffer *)dst_buf;

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"[output] frame fd = %d, meta fd = %d, index = %d!\n",
				pq_dst_buf->frame_buf.fd, pq_dst_buf->meta_buf.fd,
				pq_dst_buf->vb.vb2_buf.index);

			vb2_buffer_done(&dst_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
			ev.u.data[0] = dst_buf->vb.vb2_buf.index;
			ev.u.data[EV_DATA_INVALID_FRMAE] = true;
			v4l2_event_queue(&(pq_dev->video_dev), &ev);
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Send DeQueue Output Buffer Event!\n");
		}
		INIT_LIST_HEAD(&pq_ctx->m2m_ctx->cap_q_ctx.rdy_queue);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Clear Output Ready Queue Done!\n");

		mutex_lock(&pq_dev->mutex_stream_off);
		pq_dev->cap_turing_stream_off = true;
		mutex_unlock(&pq_dev->mutex_stream_off);
	}

	if (bPquEnable) {
		struct callback_info stream_off_callback_info = {0};
		struct msg_stream_off_done_info stream_info = {0};

		if (pq_dev->bInvalid) {
			_mtk_pq_common_release_stream(msg_info->win_id, msg_info->stream_type);
			return 0;
		}

		stream_info.win_id = msg_info->win_id;
		stream_info.stream_type = msg_info->stream_type;
		ret = _mtk_pq_common_get_stream_info(&stream_info);
		if (ret)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_common_get_stream_info fail\n");

		stream_off_callback_info.callback = mtk_pq_common_stream_off_cb;
		ret = pqu_video_stream_off(msg_info, &stream_off_callback_info);
		if (ret == -ENODEV) {
			memcpy(&_stream_off_msg_info[_CB_INC(_stream_off_count)],
				msg_info,
				sizeof(struct msg_stream_off_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_stream_off register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_video_stream_off);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_stream_off fail (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_STREAM_OFF, (void *)msg_info);
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "Waiting for PQU stream off done!\n");

	timeout = msecs_to_jiffies(PQU_TIME_OUT_MS);

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		ret = wait_event_timeout(pq_dev->wait,
			pq_dev->event_flag & PQU_INPUT_STREAM_OFF_DONE, timeout);
		if (ret == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Wait PQU input stream off time out(%ums)!\n", PQU_TIME_OUT_MS);
			return -ETIMEDOUT;
		}

		pq_dev->event_flag &= ~PQU_INPUT_STREAM_OFF_DONE;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "PQU input stream off done!\n");
	} else {
		ret = wait_event_timeout(pq_dev->wait,
			pq_dev->event_flag & PQU_OUTPUT_STREAM_OFF_DONE, timeout);
		if (ret == 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Wait PQU output stream off time out(%ums)!\n", PQU_TIME_OUT_MS);
			return -ETIMEDOUT;
		}

		pq_dev->event_flag &= ~PQU_OUTPUT_STREAM_OFF_DONE;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "PQU output stream off done!\n");
	}

	pqdev = dev_get_drvdata(pq_dev->dev);
	if (pq_dev->stream_meta.pq_mode == meta_pq_mode_legacy)
		ret = mtk_pq_common_destroy_task(pqdev);

	return 0;
}

static int _mtk_pq_common_stream_off_cb_memctrl(
	struct mtk_pq_device *pq_dev, int buf_type)
{
	int ret = 0;
	bool release_output = true;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	mutex_lock(&pq_dev->mutex_stream_off);
	if (buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/* capture stream off is still procssing */
		if (pq_dev->cap_turing_stream_off == true)
			release_output = false;
		pq_dev->out_turing_stream_off = false;
	} else {
		/* waiting for output stream off */
		if (pq_dev->stream_on_ref != 0)
			release_output = false;
		pq_dev->cap_turing_stream_off = false;
	}
	mutex_unlock(&pq_dev->mutex_stream_off);

	if (release_output == true) {
		//before release buffer, set all pq trigger gen to sw mode
		ret = _mtk_pq_common_set_all_sw_usr_mode(true);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_common_set_all_sw_usr_mode fail, Buffer Type=%d, ret=%d\n",
				buf_type, ret);
			return ret;
		}

#if IS_ENABLED(CONFIG_OPTEE)
		/* svp out stream off process start */
		ret = mtk_pq_svp_out_streamoff(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk_pq_svp_out_streamoff fail! ret=%d\n", ret);
			return ret;
		}
		/* svp out stream off process end */
#endif

		ret = mtk_pq_buffer_release(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq buf release fail!\n");
			return ret;
		}
	}

	return ret;
}

/* pqu stream off cb */
static void mtk_pq_common_stream_off_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param)
{
	int ret = 0;
	int type;
	struct msg_stream_off_done_info *msg_info = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct file *file = NULL;
	struct meta_pq_stream_internal_info *internal_info_ptr = NULL;
	struct meta_buffer meta_buf;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter stream off cb!\n");

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	msg_info = (struct msg_stream_off_done_info *)param;

	if (msg_info->win_id >= (pqdev->usable_win + pqdev->dip_win_num)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid Window ID::%d!\n", msg_info->win_id);
		return;
	}

	pq_dev = pqdev->pq_devs[msg_info->win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return;
	}

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Window (%d) is not open!\n", msg_info->win_id);
		return;
	}

	if (bPquEnable) {
		meta_buf.paddr = (unsigned char *)pq_dev->pqu_stream_dma_info.va;
		meta_buf.size = (unsigned int)pq_dev->pqu_stream_dma_info.size;
		if (msg_info->stream_meta_pa != pq_dev->pqu_stream_dma_info.pa) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"(stream_meta_pa=0x%llx) != (pqu_stream_dma_info.pa=0x%llx)\n",
				msg_info->stream_meta_pa, pq_dev->pqu_stream_dma_info.pa);
		}
	} else {
		meta_buf.paddr = (unsigned char *)msg_info->stream_meta_va;
		meta_buf.size = (unsigned int)msg_info->stream_meta_size;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"meta_va = 0x%llx, meta_iova = 0x%llx, meta_size = 0x%x!\n",
		msg_info->stream_meta_va,
		msg_info->stream_meta_pa,
		msg_info->stream_meta_size);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "meta_buf.paddr = 0x%llx, meta_buf.size = 0x%x!\n",
		(uint64_t)meta_buf.paddr, meta_buf.size);

	if (msg_info->stream_type == PQU_MSG_BUF_TYPE_INPUT)
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "buffer type = %d!\n", type);

	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_STREAM_INTERNAL_INFO, (void **)&internal_info_ptr);
	if (ret || internal_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_STREAM_INTERNAL_INFO Failed, ret = %d\n", ret);
		goto EXIT;
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"mtk-pq read EN_PQ_METATAG_STREAM_INTERNAL_INFO success\n");
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"%s%llx, %s%llx\n",
		"file = 0x", internal_info_ptr->file,
		"pq_dev = 0x", internal_info_ptr->pq_dev);

	file = (struct file *)(internal_info_ptr->file);
	pq_dev = (struct mtk_pq_device *)(internal_info_ptr->pq_dev);

	if (pq_dev->stream_on_ref > 0) {
		pq_dev->stream_on_ref--;
		/* disable b2r clock */
		if (V4L2_TYPE_IS_OUTPUT(type))
			_mtk_pq_common_b2r_exit(pq_dev);
	}

	if (pq_dev->stream_on_ref >= STREAM_ON_ONE_SHOT) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Error control flow! ID=%d"
			" Thread info: %s(%d %d), ref_count=%u, stream_on_ref=%u\n",
			pq_dev->dev_indx, current->comm, current->pid, current->tgid,
			pq_dev->ref_count, pq_dev->stream_on_ref);
		goto EXIT;
	}

	if (pq_dev->stream_on_ref == 0) {
		mtk_pq_common_put_dma_buf(&pq_dev->stream_dma_info);
	}

	ret = _mtk_pq_common_stream_off_cb_memctrl(pq_dev, type);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_stream_off_cb_memctrl fail, Buffer Type=%d, ret=%d\n",
			type, ret);
		goto EXIT;
	}
EXIT:
	ret = v4l2_m2m_streamoff(file, pq_dev->m2m.ctx->m2m_ctx, type);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Stream off fail!Buffer Type = %d, ret=%d\n", type, ret);
	}

	if (V4L2_TYPE_IS_OUTPUT(type))
		pq_dev->event_flag |= PQU_INPUT_STREAM_OFF_DONE;
	else
		pq_dev->event_flag |= PQU_OUTPUT_STREAM_OFF_DONE;

	wake_up(&(pq_dev->wait));
}

int mtk_pq_common_is_slice(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_src_buf)
{
	int ret = 0;
	struct dma_fence_array *array;
	struct dma_fence *fence;

	pq_src_buf->slice_mode = false;
	pq_src_buf->slice_qbuf = false;
	if (mtk_pq_slice_get_dbg_mode() == PQ_SLICE_MODE_TEST) {
		fence = mtk_pq_slice_create_fence(1);
		dma_resv_add_excl_fence(pq_src_buf->frame_buf.db->resv, fence);
		dma_fence_put(fence);
		pq_src_buf->slice_mode = true;
	} else if (mtk_pq_slice_get_dbg_mode() == PQ_SLICE_MODE_SKIP) {
		return ret;
	}

	fence = dma_resv_get_excl_rcu(pq_src_buf->frame_buf.db->resv);
	if (!fence)
		return -EINVAL;

	if (dma_fence_is_array(fence)) {
		array = to_dma_fence_array(fence);
		if ((!array) || (array && !array->num_fences))
			return -EINVAL;
		pq_src_buf->slice_mode = true;
	}
	dma_fence_put(fence);
	return ret;
}

int mtk_pq_common_init(struct mtk_pq_platform_device *pqdev)
{
	struct device *dev = NULL;
	u64 metadata_addr = 0;
	u32 metadata_size = 0;
	unsigned long meta_pool_addr = 0;
	unsigned long shuttle_pool_addr = 0;
	size_t meta_pool_size = 0;
	int ret = 0;

	if (pqdev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev NULL\n");
		return -EINVAL;
	}

	dev = pqdev->dev;
	if (dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dev NULL\n");
		return -EINVAL;
	}

	spin_lock_init(&pqdev->spinlock_thermal_handling_task);

	if (bPquEnable) {
		struct callback_info dequeue_frame_callback_info = {0};
		struct callback_info bbd_callback_info = {0};
		struct callback_info hdr_change_callback_info = {0};

		dequeue_frame_callback_info.callback = mtk_pq_common_dqbuf_cb;
		ret = pqu_video_dequeue_frame(&dequeue_frame_callback_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_dequeue_frame fail (ret=%d)\n", ret);
			return ret;
		}

		bbd_callback_info.callback = mtk_pq_common_bbd_cb;
		ret = pqu_video_bbd_info(&bbd_callback_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_bbd_info fail (ret=%d)\n", ret);
			return ret;
		}

		hdr_change_callback_info.callback = mtk_pq_common_hdr_change_cb;
		ret = pqu_video_hdr_change_info(&hdr_change_callback_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_hdr_change_info fail (ret=%d)\n", ret);
			return ret;
		}

		ret = mtk_pq_get_dedicate_memory_info(
				dev, MMAP_METADATA_INDEX,
				&metadata_addr, &metadata_size);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Failed to _mtk_pq_get_memory_info(index=%d)\n",
				MMAP_METADATA_INDEX);
			return -EINVAL;
		}
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"metadata_addr = 0x%llx metadata_size = 0x%x\n",
			metadata_addr, metadata_size);

		meta_pool_addr = metadata_addr;
		meta_pool_size = metadata_size - PQU_SHTTTLE_MEM_POOL;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"meta_pool_addr = 0x%lx meta_pool_size = 0x%zx\n",
			meta_pool_addr, meta_pool_size);
		if (_mtk_pq_common_create_share_memory_pool(
			meta_pool_addr, meta_pool_size)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "gen pool fail!\n");
			return -ENOMEM;
		}

		shuttle_pool_addr = metadata_addr + metadata_size - PQU_SHTTTLE_MEM_POOL;
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"shuttle_pool_addr = 0x%lx shuttle_pool_size = 0x%x\n",
			shuttle_pool_addr, (uint32_t)PQU_SHTTTLE_MEM_POOL);
		if (pqu_video_shuttle_init((uint32_t)shuttle_pool_addr,
					(uint32_t)PQU_SHTTTLE_MEM_POOL))
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"pqu_video_shuttle_init fail!\n");
		spin_lock_init(&_stream_lock);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "PQU RV55 Init done\n");
	}

	return 0;
}

void mtk_pq_common_qslice_cb(struct dma_fence *fence,
	struct dma_fence_cb *cb)
{
	int ret = 0;
	struct mtk_pq_common_wait_qslice_cb *wait =
			container_of(cb, struct mtk_pq_common_wait_qslice_cb, base);
	struct msg_queue_slice_info slice_info;
	struct msg_queue_info msg_info;
	u8 slice_num = 0;
	bool first_frame = false;

	if (fence == NULL || cb == NULL || wait == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memcpy(&slice_info, &(wait->slice_info), sizeof(struct msg_queue_slice_info));
	memset(&msg_info, 0, sizeof(struct msg_queue_info));

	if (wait->pq_dev && wait->pq_src_buf) {
		slice_num = slice_info.slice_num;
		first_frame = wait->pq_dev->common_info.first_slice_frame;

		if ((!first_frame && slice_info.slice_done_id == 0) ||
			(first_frame && slice_info.slice_done_id == (slice_num - 1))) {
			wait->pq_src_buf->slice_mode = true;
			wait->pq_src_buf->slice_qbuf = true;
			msg_info.frame_type = PQU_MSG_BUF_TYPE_INPUT;
			ret = mtk_pq_common_qbuf(wait->pq_dev, wait->pq_src_buf, &msg_info);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Common Queue Input Buffer Fail!\n");
			}

			if (wait->pq_dst_buf && wait->pq_dst_buf->slice_need_output) {
				wait->pq_dst_buf->slice_mode = true;
				wait->pq_dst_buf->slice_qbuf = true;
				msg_info.frame_type = PQU_MSG_BUF_TYPE_OUTPUT;
				ret = mtk_pq_common_qbuf(wait->pq_dev, wait->pq_dst_buf, &msg_info);
				if (ret) {
					STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"Common Queue Output Buffer Fail!\n");
				}
				wait->pq_dst_buf->slice_need_output = false;
			}

			wait->pq_dev->common_info.first_slice_frame = false;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "slice_num:%d slice_done_id:%d [%s]\n",
		slice_info.slice_num, slice_info.slice_done_id,
		fence->ops->get_timeline_name(fence));

	if (bPquEnable) {
		ret = pqu_video_queue_slice(&slice_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_queue_slice fail! (ret=%d)\n", ret);
		}
	}
	else
		pqu_msg_send(PQU_MSG_SEND_QUEUE_SLICE, (void *)&slice_info);

	free(wait);
}

int mtk_pq_common_qslice(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_src_buf, struct mtk_pq_buffer *pq_dst_buf)
{
	int ret = 0;
	int idx;
	struct dma_fence_array *array;
	struct dma_fence *fence;
	struct dma_fence *slice_fence;
	struct mtk_pq_common_wait_qslice_cb *wait;
	struct msg_queue_info msg_info;
	bool first_frame = false;

	fence = dma_resv_get_excl_rcu(pq_src_buf->frame_buf.db->resv);
	if (!fence)
		return -EINVAL;

	if (dma_fence_is_array(fence)) { //slice mode
		array = to_dma_fence_array(fence);
		if (!array)
			return -EINVAL;

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"[slice mode]frame fd:%d meta fd:%d index:%d num_fences:%d slice_mode:%d\n",
			pq_src_buf->frame_buf.fd, pq_src_buf->meta_buf.fd,
			pq_src_buf->vb.vb2_buf.index, array->num_fences,
			pq_src_buf->slice_mode);

		for (idx = 0; idx < array->num_fences; idx++) {
			slice_fence = array->fences[idx];

			if (!slice_fence->ops)
				continue;

			wait = (struct mtk_pq_common_wait_qslice_cb *)
					malloc(sizeof(struct mtk_pq_common_wait_qslice_cb));
			if (!wait) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"Failed to malloc qslice info\n");
				continue;
			}

			wait->slice_info.slice_num = array->num_fences;
			wait->slice_info.slice_done_id = idx;
			wait->pq_dev = pq_dev;
			wait->pq_src_buf = pq_src_buf;
			wait->pq_dst_buf = pq_dst_buf;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "idx:%d ops:%p [%s]\n",
				idx, slice_fence->ops,
				slice_fence->ops->get_timeline_name(slice_fence));
			ret = dma_fence_add_callback(slice_fence, &wait->base,
					mtk_pq_common_qslice_cb);

			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
					"Slice Fence[%s] already signaled! ret = %d\n",
					slice_fence->ops->get_timeline_name(slice_fence), ret);
				first_frame = pq_dev->common_info.first_slice_frame;
				if ((!first_frame && idx == 0) ||
					(first_frame && idx == (array->num_fences - 1))) {
					memset(&msg_info, 0, sizeof(struct msg_queue_info));
					pq_src_buf->slice_qbuf = true;
					msg_info.frame_type = PQU_MSG_BUF_TYPE_INPUT;
					mtk_pq_common_qbuf(pq_dev, pq_src_buf, &msg_info);

					if (pq_dst_buf && pq_dst_buf->slice_need_output) {
						pq_dst_buf->slice_qbuf = true;
						msg_info.frame_type = PQU_MSG_BUF_TYPE_OUTPUT;
						mtk_pq_common_qbuf(pq_dev, pq_dst_buf, &msg_info);
						pq_dst_buf->slice_need_output = false;
					}

					pq_dev->common_info.first_slice_frame = false;
				}
				if (bPquEnable) {
					ret = pqu_video_queue_slice(&(wait->slice_info));
					if (ret)
						STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"pqu_video_queue_slice fail! (ret=%d)\n", ret);
				}
				else
					pqu_msg_send(PQU_MSG_SEND_QUEUE_SLICE,
							(void *)&(wait->slice_info));
				free(wait);
			}
		}
	} else { //frame mode
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"[frame mode]frame fd:%d meta fd:%d index:%d ops:%p is_array:%d slice_mode:%d\n",
			pq_src_buf->frame_buf.fd, pq_src_buf->meta_buf.fd,
			pq_src_buf->vb.vb2_buf.index, fence->ops,
			dma_fence_is_array(fence), pq_src_buf->slice_mode);
	}
	dma_fence_put(fence);

	if (mtk_pq_slice_get_dbg_mode() == PQ_SLICE_MODE_TEST) {
		fence = dma_resv_get_excl_rcu(pq_src_buf->frame_buf.db->resv);
		if (fence) {
			mtk_pq_slice_fence_signal(fence, 0);
			dma_fence_put(fence);
		}
	}
	return ret;
}

int mtk_pq_common_qbuf(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf,
	struct msg_queue_info *msg_info)
{
	int ret = 0;
	u32 source = 0;
	struct meta_buffer meta_buf;
	struct callback_info queue_frame_callback_info = {0};

	memset(&meta_buf, 0, sizeof(struct meta_buffer));

	if (!pq_dev || !pq_buf || !msg_info) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
		"enter common qbuf, dev_indx=%u, buffer type = %d!\n",
		pq_dev->dev_indx, pq_buf->vb.vb2_buf.type);

	if (pq_buf->slice_mode && !pq_buf->slice_qbuf) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"slice_mode=%u, slice_qbuf=%u\n",
			pq_buf->slice_mode, pq_buf->slice_qbuf);
		return ret;
	}

	source = pq_dev->common_info.input_source;
	meta_buf.paddr = (unsigned char *)pq_buf->meta_buf.va;
	meta_buf.size = (unsigned int)pq_buf->meta_buf.size;

	msg_info->win_id = (u8)(pq_dev->dev_indx);
	msg_info->frame_iova = (u64)pq_buf->frame_buf.iova;
	msg_info->meta_va = (u64)pq_buf->meta_buf.va;
	msg_info->meta_size = (u32)pq_buf->meta_buf.size;
	msg_info->priv = (u64)pq_buf;
	msg_info->extra_frame_num =
		(V4L2_TYPE_IS_OUTPUT(pq_buf->vb.vb2_buf.type)) ?
		(pq_dev->dv_win_info.common.extra_frame_num) : 0;
	if (IS_INPUT_B2R(source))
		if (pq_dev->extra_frame > msg_info->extra_frame_num)
			msg_info->extra_frame_num = pq_dev->extra_frame;


	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"meta_va=0x%llx,meta_size=0x%x,priv=0x%llx,frame_iova=0x%llx,extra=%d\n",
		msg_info->meta_va,
		msg_info->meta_size,
		msg_info->priv,
		msg_info->frame_iova,
		msg_info->extra_frame_num);

#ifdef PQU_PATTERN_TEST
	if (pq_dev->trigger_pattern) {
		ret = mtk_pq_common_write_metadata_addr(&meta_buf,
			EN_PQ_METATAG_PQU_PATTERN_INFO, (void *)&pq_dev->pattern_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN,
				"mtk-pq write EN_PQ_METATAG_PQU_PATTERN_INFO Failed,ret=%d\n", ret);
			//return ret;
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"mtk-pq write EN_PQ_METATAG_PQU_PATTERN_INFO success\n");
		}
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_PATTERN, "Write Pattern Metada Success!\n");
		pq_dev->trigger_pattern = false;
	}
#endif
	if (bPquEnable) {
		if (pq_buf->meta_buf.db == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
			return -EINVAL;
		}
		/* flush metadata to make sure rv55 got the data correctly */
		dma_buf_end_cpu_access(pq_buf->meta_buf.db, DMA_BIDIRECTIONAL);

		if (_mtk_pq_common_get_frame(msg_info)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"_mtk_pq_common_get_frame fail\n");
			return -ENOMEM;
		}

		ret = pqu_video_queue_frame(msg_info, &queue_frame_callback_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_queue_frame fail! (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_QUEUE, (void *)msg_info);
	}
	pq_buf->queue_time = _mtk_pq_common_get_ms_time();

	return ret;
}

/* pqu dqbuf cb */
static void mtk_pq_common_dqbuf_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param)
{
	int ret = 0;
	int type = 0;
	struct msg_dqueue_info *msg_info = (struct msg_dqueue_info *)param;
	enum mtk_pq_input_source_type input_source;
	struct meta_buffer meta_buf;
	struct v4l2_event ev;
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_buffer *pq_buf = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter dqbuf cb!\n");

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memset(&meta_buf, 0, sizeof(struct meta_buffer));
	memset(&ev, 0, sizeof(struct v4l2_event));

	//Invalid Event
	ev.u.data[EV_DATA_INVALID_FRMAE] = msg_info->InValidFrame;

	if (msg_info->win_id >= pqdev->usable_win) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid Window ID::%d!\n", msg_info->win_id);
		return;
	}

	pq_dev = pqdev->pq_devs[msg_info->win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return;
	}

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Window (%d) is not open!\n", msg_info->win_id);
		return;
	}

	if (bPquEnable) {
		void *va = NULL;

		va = _mtk_pq_common_find_frame_metadata_va(msg_info->meta_va);
		if (va == NULL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "ioremap_nocache fail\n");
			return;
		}

		memcpy_fromio((void *)msg_info->meta_host_va, va, msg_info->meta_size);
		meta_buf.paddr = (unsigned char *)msg_info->meta_host_va;
		_mtk_pq_common_release_frame_metadata(msg_info->meta_va);
	} else {
		meta_buf.paddr = (unsigned char *)msg_info->meta_va;
	}
	meta_buf.size = (unsigned int)msg_info->meta_size;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"meta_va = 0x%llx, meta_size = 0x%x, priv = 0x%llx, InValidFrame = %d!\n",
		msg_info->meta_va,
		msg_info->meta_size,
		msg_info->priv,
		msg_info->InValidFrame);

	pq_buf = (struct mtk_pq_buffer *)(msg_info->priv);
	if ((!pq_buf) || (!pq_buf->meta_buf.db)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_buf is NULL\n");
		return;
	}

	pq_buf->dequeue_time = _mtk_pq_common_get_ms_time();
	pq_buf->process_time = pq_buf->dequeue_time - pq_buf->queue_time;

	input_source = pq_dev->common_info.input_source;

	/* flush metadata to make sure rv55 got the data correctly */
	if (bPquEnable) {
		ret = dma_buf_begin_cpu_access(pq_buf->meta_buf.db, DMA_BIDIRECTIONAL);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "dma_buf_begin_cpu_access fail\n");
			goto exit;
		}
	}

	if (msg_info->frame_type == PQU_MSG_BUF_TYPE_INPUT) {
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
		PQ_ATRACE_INT_FORMAT(pq_buf->process_time,
			"w%02d_i_pq_dd_inq_proc_time", msg_info->win_id);
		PQ_ATRACE_INT_FORMAT(pq_buf->vb.vb2_buf.index,
			"w%02d_i_07-E:pq_dqbuf_in_idx", msg_info->win_id);
#endif
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		ev.type = V4L2_EVENT_MTK_PQ_INPUT_DONE;
	} else if (msg_info->frame_type == PQU_MSG_BUF_TYPE_OUTPUT) {
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
		PQ_ATRACE_INT_FORMAT(pq_buf->process_time,
			"w%02d_o_pq_dd_outq_proc_time", msg_info->win_id);
		PQ_ATRACE_INT_FORMAT(pq_buf->vb.vb2_buf.index,
			"w%02d_o_26-E:pq_dqbuf_out_idx", msg_info->win_id);
#endif
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		ev.type = V4L2_EVENT_MTK_PQ_OUTPUT_DONE;
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"error frame type::%d!\n",
				msg_info->frame_type);
		goto exit;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "buffer type = %d!\n", pq_buf->vb.vb2_buf.type);

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = _mtk_pq_common_set_output_dqbuf_meta(msg_info->win_id, meta_buf);
		if (ret) {
			goto exit;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = _mtk_pq_common_set_capture_dqbuf_meta(msg_info->win_id, meta_buf);
		if (ret) {
			goto exit;
		}
		break;
	default:
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Invalid buffer type!\n");
		goto exit;
	}
exit:
	vb2_buffer_done(&pq_buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	ev.u.data[0] = pq_buf->vb.vb2_buf.index;
	v4l2_event_queue(&(pq_dev->video_dev), &ev);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "queue buffer done event, buffer type = %d!\n", type);
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
	switch (ev.type) {
	case V4L2_EVENT_MTK_PQ_INPUT_DONE:
		PQ_ATRACE_INT_FORMAT(pq_buf->vb.vb2_buf.index,
			"w%02d_i_08-S:pq_dqbuf_event_in_idx", msg_info->win_id);
		break;

	case V4L2_EVENT_MTK_PQ_OUTPUT_DONE:
		PQ_ATRACE_INT_FORMAT(pq_buf->vb.vb2_buf.index,
			"w%02d_o_27-S:pq_dqbuf_event_out_idx", msg_info->win_id);
		break;
	default:
		break;
	}
#endif
}

/* pqu dovi frame meta cb */
static void mtk_pq_common_dovi_frame_meta_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param)
{
	struct msg_dovi_frame_meta_info *msg_info = (struct msg_dovi_frame_meta_info *)param;
	struct v4l2_event ev;
	struct mtk_pq_device *pq_dev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter dovi fame meta cb!\n");

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memset(&ev, 0, sizeof(struct v4l2_event));

	if (msg_info->win_id >= pqdev->usable_win) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid Window ID::%d!\n", msg_info->win_id);
		return;
	}

	pq_dev = pqdev->pq_devs[msg_info->win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return;
	}

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Window (%d) is not open!\n", msg_info->win_id);
		return;
	}

	ev.u.data[0] = msg_info->win_id;
	ev.u.data[1] = msg_info->if_change;
	ev.type = V4L2_EVENT_MTK_PQ_DOVI_FRAME_META;
	v4l2_event_queue(&(pq_dev->video_dev), &ev);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "dovi frame meta event\n");
}

static void mtk_pq_common_close_cb(
			int error_cause,
			pid_t thread_id,
			uint32_t user_param,
			void *param)
{
	struct msg_destroy_win_done_info *msg_info = NULL;
	struct mtk_pq_device *pq_dev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter close win cb!\n");

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	msg_info = (struct msg_destroy_win_done_info *)param;

	if (msg_info->win_id >= pqdev->usable_win) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid Window ID::%d!\n", msg_info->win_id);
		return;
	}

	pq_dev = pqdev->pq_devs[msg_info->win_id];
	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev in NULL!\n");
		return;
	}

	if (!pq_dev->ref_count) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Window (%d) is already closed!\n", msg_info->win_id);
		return;
	}

	pq_dev->event_flag |= PQU_CLOSE_WIN_DONE;
	wake_up(&(pq_dev->wait));
}

static int _mtk_pq_common_set_path(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	bool bEn = ((struct v4l2_pq_path_info *)ctrl)->bEn;
	enum v4l2_pq_path pq_path = ((struct v4l2_pq_path_info *)ctrl)->pq_path;

	if (bEn == true)
		ret = mtk_pq_common_set_path_enable(pq_dev, pq_path);
	else
		ret = mtk_pq_common_set_path_disable(pq_dev, pq_path);

	return ret;
}

static int  _mtk_pq_common_set_panel_color_info(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	struct v4l2_pq_panel_color_info stPanelColorInfo;
	struct msg_panel_color_info msgPanelColorInfo;

	memset(&stPanelColorInfo, 0, sizeof(struct v4l2_pq_panel_color_info));
	memset(&msgPanelColorInfo, 0, sizeof(struct msg_panel_color_info));

	if (!ctrl || !pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EFAULT;
	}

	memcpy(&stPanelColorInfo, (struct v4l2_pq_panel_color_info *)ctrl,
		sizeof(struct v4l2_pq_panel_color_info));

	msgPanelColorInfo.pnl_color_info_version = stPanelColorInfo.pnl_color_info_version;
	msgPanelColorInfo.pnl_color_info_length = stPanelColorInfo.pnl_color_info_length;
	msgPanelColorInfo.pnl_color_info_format = stPanelColorInfo.pnl_color_info_format;
	msgPanelColorInfo.pnl_color_info_rx = stPanelColorInfo.pnl_color_info_rx;
	msgPanelColorInfo.pnl_color_info_ry = stPanelColorInfo.pnl_color_info_ry;
	msgPanelColorInfo.pnl_color_info_gx = stPanelColorInfo.pnl_color_info_gx;
	msgPanelColorInfo.pnl_color_info_gy = stPanelColorInfo.pnl_color_info_gy;
	msgPanelColorInfo.pnl_color_info_bx = stPanelColorInfo.pnl_color_info_bx;
	msgPanelColorInfo.pnl_color_info_by = stPanelColorInfo.pnl_color_info_by;
	msgPanelColorInfo.pnl_color_info_wx = stPanelColorInfo.pnl_color_info_wx;
	msgPanelColorInfo.pnl_color_info_wy = stPanelColorInfo.pnl_color_info_wy;
	msgPanelColorInfo.pnl_color_info_maxlum = stPanelColorInfo.pnl_color_info_maxlum;
	msgPanelColorInfo.pnl_color_info_medlum = stPanelColorInfo.pnl_color_info_medlum;
	msgPanelColorInfo.pnl_color_info_minlum = stPanelColorInfo.pnl_color_info_minlum;
	msgPanelColorInfo.pnl_color_info_linear_rgb = stPanelColorInfo.pnl_color_info_linear_rgb;
	msgPanelColorInfo.pnl_color_info_hdrnits = stPanelColorInfo.pnl_color_info_hdrnits;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
	"panel color info v4l2 set version(%d), format(%d)\n",
	msgPanelColorInfo.pnl_color_info_version, msgPanelColorInfo.pnl_color_info_format);

	/* send pqu cmd */
	if (bPquEnable) {
		ret = pqu_video_set_panel_color_info(&msgPanelColorInfo);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_panel_color_info fail! (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_PANEL_COLOR_INFO, (void *)&msgPanelColorInfo);
	}

	return ret;
}

int mtk_pq_common_set_pattern_info(struct mtk_pq_device *pq_dev, void *ctrl, void *cmd)
{
	int ret = 0;
	struct v4l2_pq_pattern_info_set s_pattern_cmd;
	struct msg_pq_pattern_info msg_pattern_info;

	memset(&s_pattern_cmd, 0, sizeof(struct v4l2_pq_pattern_info_set));
	memset(&msg_pattern_info, 0, sizeof(struct msg_pq_pattern_info));

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EFAULT;
	}

	if (ctrl) {
		memcpy(&s_pattern_cmd, (struct v4l2_pq_pattern_info_set *)ctrl,
			sizeof(struct v4l2_pq_pattern_info_set));

		msg_pattern_info.debug_mode = s_pattern_cmd.debug_mode;
		msg_pattern_info.force_rgb = s_pattern_cmd.force_rgb;
		msg_pattern_info.position = s_pattern_cmd.position;
		msg_pattern_info.pattern_type = s_pattern_cmd.pattern_type;
		msg_pattern_info.win_id = s_pattern_cmd.win_id;
		msg_pattern_info.enable_ts = s_pattern_cmd.enable_ts;
		msg_pattern_info.enable = s_pattern_cmd.enable;
		msg_pattern_info.force_timing_enable = s_pattern_cmd.force_timing_enable;
		msg_pattern_info.force_h_size = s_pattern_cmd.force_h_size;
		msg_pattern_info.force_v_size = s_pattern_cmd.force_v_size;
		msg_pattern_info.m.pure_color.color.red = s_pattern_cmd.pure_color_red;
		msg_pattern_info.m.pure_color.color.green = s_pattern_cmd.pure_color_green;
		msg_pattern_info.m.pure_color.color.blue = s_pattern_cmd.pure_color_blue;
	} else if (cmd)
		memcpy(&msg_pattern_info, (struct msg_pq_pattern_info *)cmd,
			sizeof(struct msg_pq_pattern_info));

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"pattern info v4l2 set position(%d), type(%d), enable(%d)\n",
		msg_pattern_info.position, msg_pattern_info.pattern_type, msg_pattern_info.enable);

	/* send pqu cmd */
	if (bPquEnable) {
		ret = pqu_video_set_pattern_info(&msg_pattern_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_pattern_info fail! (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_PATTERN_INFO, (void *)&msg_pattern_info);
	}

	return ret;
}

static int _mtk_pq_common_cfd_set_curve(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0, hdr_type = 0, win_id = 0;
	struct v4l2_pq_cfd_curve_set s_calman_cmd;
	struct meta_buffer meta_buf;
	struct msg_calman_info calman_info;
	struct mtk_pq_platform_device *pqdev = NULL;
	void *pq_va_addr;
	void *pq_pa_addr;
	__u32 pq_bus_buf_size;

	/* test hdr conf calman command */
	static struct meta_cfd_hdr_conf stCfdShmHdrConfPqhal;
	static struct m_pqu_cfd_curve_hdr_conf stCfdShmHdrConfPqu;
	/* test oetf calman command */
	static struct meta_cfd_calman_curve stCfdShmCalmanCurvePqhal;
	static struct m_pqu_cfd_calman_curve stCfdShmCalmanCurvePqu;

	static struct meta_cfd_window_curve stCfdShmWinCurvePqhal;
	static struct m_pqu_cfd_window_curve stCfdShmWinCurvePqu;

	memset(&s_calman_cmd, 0, sizeof(struct v4l2_pq_cfd_curve_set));
	memset(&meta_buf, 0, sizeof(struct meta_buffer));
	memset(&calman_info, 0, sizeof(struct msg_calman_info));

	pqdev = dev_get_drvdata(pq_dev->dev);

	pq_va_addr = (void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].va;
	pq_pa_addr = (void *)pqdev->DDBuf[MMAP_CFD_BUF_INDEX].pa;
	pq_bus_buf_size = pqdev->DDBuf[MMAP_CFD_BUF_INDEX].size;

	memcpy(&s_calman_cmd, (struct v4l2_pq_cfd_curve_set *)ctrl,
		sizeof(struct v4l2_pq_cfd_curve_set));

	meta_buf.paddr = (unsigned char *) pq_va_addr;
	meta_buf.size = (unsigned int) pq_bus_buf_size;

	win_id = s_calman_cmd.command_id;
	hdr_type = s_calman_cmd.data_struct_size;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[PQDD][CMD_INFO][win_id:%d][hdr_type:%d]\n",
		win_id, hdr_type);

	if ((s_calman_cmd.data[0] > 0 && s_calman_cmd.data[0] < PQU_U32_MAX_NUM) &&
		(s_calman_cmd.data[1] > 0 && s_calman_cmd.data[1] < PQU_U32_MAX_NUM)) {
		_get_cfd_win_id = s_calman_cmd.data[1] - 1;
		_get_cfd_curve_id[_get_cfd_win_id] = s_calman_cmd.data[0] - 1;
		return ret;
	}

	/* read calman hdr conf written by pqhal */
	ret = mtk_pq_common_read_metadata_addr(&meta_buf,
		EN_PQ_METATAG_CFD_SHM_CALMAN_HDR_CONF_TAG, &stCfdShmHdrConfPqhal);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "va:%#llx, size:%x\n",
			(__u64)pq_va_addr, pq_bus_buf_size);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
			" read EN_PQ_METATAG_CFD_SHM_CALMAN_HDR_CONF_TAG Failed, ret = %d\n", ret);
	} else {
		memcpy(&stCfdShmHdrConfPqu, &stCfdShmHdrConfPqhal,
			sizeof(struct m_pqu_cfd_curve_hdr_conf));

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
			"[PQDD][CALMAN HDR CONF][pqhal : [%d, %d], pqu : [%d, %d]\n",
			stCfdShmHdrConfPqhal.data.EOTF, stCfdShmHdrConfPqhal.u32State,
			stCfdShmHdrConfPqu.data.EOTF, stCfdShmHdrConfPqu.u32State);

		/* write calman conf hdr metadata for pqu read */
		ret = mtk_pq_common_write_metadata_addr(&meta_buf,
			EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_HDR_CONF_TAG, &stCfdShmHdrConfPqu);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				" write EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_HDR_CONF_TAG Failed, ret = %d\n",
				ret);
		}
	}

	/* read calman curve metadata written by pqhal */
	if (hdr_type < PQ_NUM_4) {
		ret = mtk_pq_common_read_metadata_addr(&meta_buf,
			mtk_pq_cfd_mapping_hdr_mode[hdr_type], &stCfdShmCalmanCurvePqhal);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "va:%#llx, size:%x\n",
				(__u64)pq_va_addr, pq_bus_buf_size);
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
				" read EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_TAG Failed, hdr_type : %d, ret = %d\n",
				hdr_type, ret);
		} else {
			memcpy(&stCfdShmCalmanCurvePqu, &stCfdShmCalmanCurvePqhal,
				sizeof(struct m_pqu_cfd_calman_curve));

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
	"[PQDD][CALMAN CURVE][hdr type:%d][pqhal : [%d, %d, %d, %d], pqu : [%d, %d, %d, %d]\n",
				hdr_type,
				stCfdShmCalmanCurvePqhal.eotf.u32State,
				stCfdShmCalmanCurvePqhal.oetf.u32State,
				stCfdShmCalmanCurvePqhal.matrix.u32State,
				stCfdShmCalmanCurvePqhal.rgb_3dlut.u32State,
				stCfdShmCalmanCurvePqu.eotf.u32State,
				stCfdShmCalmanCurvePqu.oetf.u32State,
				stCfdShmCalmanCurvePqu.matrix.u32State,
				stCfdShmCalmanCurvePqu.rgb_3dlut.u32State);

			/* write calman curve metadata for pqu read */
			ret = mtk_pq_common_write_metadata_addr(&meta_buf,
			mtk_pq_cfd_mapping_hdr_mode[hdr_type + PQ_NUM_4], &stCfdShmCalmanCurvePqu);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					" write EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_TAG(%d) Failed, ret = %d\n",
					mtk_pq_cfd_mapping_hdr_mode[hdr_type + PQ_NUM_4], ret);
			}
		}
	}

	/* read win curve metadata written by pqhal */
	ret = mtk_pq_common_read_metadata_addr(&meta_buf,
		mtk_pq_cfd_mapping_win_curve[win_id], &stCfdShmWinCurvePqhal);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "va:%#llx, size:%x\n",
			(__u64)pq_va_addr, pq_bus_buf_size);
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
			" read EN_PQ_METATAG_CFD_SHM_WIN_CURVE_TAG Failed, win_id : %d, ret = %d\n",
			win_id, ret);
	} else {
		memcpy(&stCfdShmWinCurvePqu, &stCfdShmWinCurvePqhal,
			sizeof(struct m_pqu_cfd_window_curve));

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
	"[PQDD][WIN CURVE][win id:%d, pqhal(%lu):[%d, %d, %d, %d, %d, %d, %d, %d], pqu(%lu):[%d, %d, %d, %d, %d, %d, %d, %d]\n",
			win_id,
			sizeof(struct meta_cfd_window_curve),
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_OETF],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_EOTF],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_3DLUT],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_3x3],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_TMO],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_EOTF],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_OOTF],
			stCfdShmWinCurvePqhal.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_OETF],
			sizeof(struct m_pqu_cfd_window_curve),
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_OETF],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_EOTF],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_3DLUT],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_3x3],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_TMO],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_EOTF],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_OOTF],
			stCfdShmWinCurvePqu.u32CurveState[M_PQ_CFD_STATE_CURVE_HDR_OETF]);

		/* write win curve metadata for pqu read */
		ret = mtk_pq_common_write_metadata_addr(&meta_buf,
			mtk_pq_cfd_mapping_win_curve[win_id + PQ_NUM_16], &stCfdShmWinCurvePqu);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				" write EN_PQ_METATAG_PQU_CFD_SHM_WIN_CURVE_TAG Failed(%d), ret = %d\n",
				mtk_pq_cfd_mapping_win_curve[win_id + PQ_NUM_16], ret);
		}
	}

	/* write metadata */
	calman_info.win_id = (__u8)win_id;
	calman_info.va = (__u64)pq_va_addr;
	calman_info.pa = (__u64)pq_pa_addr;
	calman_info.da = (__u64)pq_pa_addr;
	calman_info.size = (__u64)pq_bus_buf_size;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON, "[PQDD][MSG_INFO][%d][%#llx][%#llx][%#llx]\n",
		calman_info.win_id, calman_info.va, calman_info.pa,
		calman_info.size);

	/* send pqu cmd */
	if (bPquEnable) {
		ret = pqu_video_set_calman(&calman_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_set_calman fail! (ret=%d)\n", ret);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_CALMAN_SET, (void *)&calman_info);
	}

	return ret;

}


static int _mtk_pq_common_cfd_get_curve(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;

	static struct v4l2_pq_cfd_curve_get g_cmd;

	memset(&g_cmd, 0, sizeof(struct v4l2_pq_cfd_curve_get));

	//memcpy(&g_cmd, ctrl, sizeof(struct v4l2_pq_cfd_curve_get));
	g_cmd.u8WinID = _get_cfd_win_id;
	if (g_cmd.u8WinID < PQU_CFD_WIN_NUM) {
		g_cmd.u32CurveId = _get_cfd_curve_id[g_cmd.u8WinID];
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
			"[PQDD][SEND][GET_CURVE][%d][%d]\n",
			g_cmd.u32CurveId, g_cmd.u8WinID);
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Invalid win id[%d]\n", g_cmd.u8WinID);
		return ret;
	}

	pqu_cfd_shm_get_result((struct m_pqu_cfd_get_curve *)&g_cmd);

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_COMMON,
		"[PQDD][REPLY][GET_CURVE][%d][%d]\n",
		g_cmd.u32CurveId, g_cmd.u8WinID);

	memcpy(ctrl, &g_cmd, sizeof(struct v4l2_pq_cfd_curve_get));

	_get_cfd_curve_id = 0;
	_get_cfd_win_id = 0;

	return ret;
}

static void _ready_cb_pqu_video_update_config(void)
{
	int ret = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "pqu cpuif ready callback function\n");

	ret = pqu_video_update_config(&_update_config_msg_info[_CB_DEC(_update_config_count)]);
	if (ret != 0)
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqu_video_update_config fail!\n");
}

int _mtk_pq_common_ucd_config(struct msg_config_info *msg_info)
{
	struct pqu_ucd_send_cmd_param send = {UCD_DEFAULT_VALUE};
	struct callback_info callback_info = {UCD_DEFAULT_VALUE};
	int ret = 0;

	if (IS_ERR_OR_NULL(msg_info)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "ucd msg_info nulll!\n");
		return -EINVAL;
	}

	/* only mt5876/mt5897 need send shared memory to R2 */
	if ((bUcdConfigInit == FALSE) && (msg_info->u16ucd_alg_engine == UCD_ALGO_ENGINE_R2)) {
		/* UCD_CMD_INIT_MODE cmd */
		send.CmdID = UCD_CMD_INIT_MODE;
		send.D1    = (int32_t) (msg_info->u64ucd_shm_pa - BUSADDRESS_TO_IPADDRESS_OFFSET);
		send.D2    = (int32_t) msg_info->u32ucd_shm_size;
		send.D3    = UCD_DEFAULT_VALUE;
		send.D4    = UCD_DEFAULT_VALUE;
		ret = pqu_send_cmd_ucd(&send, &callback_info);
		if (ret != 0)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"CMD UCD_CMD_INIT_MODE Fail\n");

		bUcdConfigInit = TRUE;
	}
	return ret;
}

int mtk_pq_common_config(struct msg_config_info *msg_info, bool is_pqu)
{
	int ret = 0;

	if (!msg_info) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter common config! is_pqu=%d\n", is_pqu);

	if (is_pqu) {
		ret = pqu_video_update_config(msg_info);
		if (ret == -ENODEV) {
			memcpy(&_update_config_msg_info[_CB_INC(_update_config_count)],
				msg_info,
				sizeof(struct msg_config_info));
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_update_config register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_video_update_config);
		} else if (ret != 0) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"pqu_video_update_config fail (ret=%d)\n", ret);
		}
	} else {
		ret = _mtk_pq_common_ucd_config(msg_info);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "set ucd config fail\n");
			return ret;
		}
		pqu_msg_send(PQU_MSG_SEND_CONFIG, (void *)msg_info);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_pq_common_config);


static int _mtk_pq_common_set_stream_metadata(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;
	struct meta_buffer stream_meta_buffer;
	int meta_fd = 0;

	if ((!pq_dev) || (!ctrl))
		return -ENOMEM;

	memset(&stream_meta_buffer, 0, sizeof(struct meta_buffer));
	meta_fd = ((struct v4l2_pq_stream_meta_info *)ctrl)->meta_fd;

	if (pq_dev->stream_dma_info.fd != meta_fd)
		mtk_pq_common_put_dma_buf(&pq_dev->stream_dma_info);

	pq_dev->stream_dma_info.fd = meta_fd;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "stream meta fd = %d\n",
		pq_dev->stream_dma_info.fd);

	ret = mtk_pq_common_get_dma_buf(pq_dev, &pq_dev->stream_dma_info);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "get stream meta buf fail\n");
		return ret;
	}

	stream_meta_buffer.paddr = (unsigned char *)pq_dev->stream_dma_info.va;
	stream_meta_buffer.size = (unsigned int)pq_dev->stream_dma_info.size;

	/* read meta from stream metadata */
	ret = mtk_pq_common_read_metadata_addr(&stream_meta_buffer,
		EN_PQ_METATAG_STREAM_INFO, &(pq_dev->stream_meta));
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_STREAM_INFO Failed, ret = %d\n", ret);
		return ret;
	}

	/* align width & height in stream metadata */
	pq_dev->stream_meta.width
		= PQ_ALIGN(pq_dev->stream_meta.width, pqdev->display_dev.pre_hvsp_align);
	pq_dev->stream_meta.height = PQ_ALIGN(pq_dev->stream_meta.height, PQ_Y_ALIGN);

	if (pq_dev->stream_meta.pq_scene != meta_pq_scene_force_p)
		mtk_pq_buffer_get_hwmap_info(pq_dev);

	return ret;
}

static int _mtk_pq_common_get_stream_metadata(struct mtk_pq_device *pq_dev, void *ctrl)
{
	int ret = 0;

	((struct v4l2_pq_stream_meta_info *)ctrl)->meta_fd = pq_dev->stream_dma_info.fd;

	return ret;
}

static int _common_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev = NULL;
	int ret = 0;
	u32 source = 0;

	if (!ctrl)
		return -ENOMEM;
	pq_dev = container_of(ctrl->handler, struct mtk_pq_device,
		common_ctrl_handler);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "ctrl id = 0x%x.\n", ctrl->id);

	source = pq_dev->common_info.input_source;

	switch (ctrl->id) {
	case V4L2_CID_DELAY_TIME:
		ret = _mtk_pq_common_get_delay_time(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_INPUT_TIMING:
		ret = _mtk_pq_common_get_input_timing(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_INPUT_EXT_INFO:
	case V4L2_CID_OUTPUT_EXT_INFO:
		ret = 0;
		break;
	case V4L2_CID_SET_STREAMMETA_DATA:
		ret = _mtk_pq_common_get_stream_metadata(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_STREAM_DEBUG:
		ret = 0;
		memcpy(ctrl->p_new.p, &g_stream_debug_info, sizeof(struct m_pq_common_debug_info));
		break;
	case V4L2_CID_PQ_G_BUFFER_OUT_INFO:
		if (IS_INPUT_SRCCAP(source))
			ret = mtk_pq_display_idr_queue_setup_info(pq_dev, ctrl->p_new.p);
		else if (IS_INPUT_B2R(source))
			ret = mtk_pq_display_b2r_queue_setup_info(pq_dev, ctrl->p_new.p);
		else
			ret = -EINVAL;
		break;
	case V4L2_CID_PQ_G_BUFFER_CAP_INFO:
		ret = mtk_pq_display_mdw_queue_setup_info(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_CFD_GET_CURVE:
		ret = _mtk_pq_common_cfd_get_curve(pq_dev, ctrl->p_new.p);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static int _common_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_pq_device *pq_dev;
	int ret = 0;

	if (!ctrl)
		return -ENOMEM;
	pq_dev = container_of(ctrl->handler, struct mtk_pq_device,
		common_ctrl_handler);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "ctrl id = 0x%x.\n", ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		ret = _mtk_pq_common_set_hflip(pq_dev, ctrl->p_new.p_u8);
		break;
	case V4L2_CID_VFLIP:
		ret = _mtk_pq_common_set_vflip(pq_dev, ctrl->p_new.p_u8);
		break;
	case V4L2_CID_FORCE_P_MODE:
		ret = _mtk_pq_common_set_forcep(pq_dev, (bool)ctrl->cur.val);
		break;
	case V4L2_CID_DELAY_TIME:
		ret = _mtk_pq_common_set_delay_time(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_INPUT_TIMING:
		ret = _mtk_pq_common_set_input_timing(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_INPUT_EXT_INFO:
		ret = _mtk_pq_common_set_input_ext_info(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_OUTPUT_EXT_INFO:
		ret = _mtk_pq_common_set_output_ext_info(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_GEN_PATTERN:
		ret = _mtk_pq_common_set_gen_pattern(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_STREAMMETA_DATA:
		ret = _mtk_pq_common_set_stream_metadata(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_SET_INPUT_MODE:
		ret = _mtk_pq_common_set_input_mode(pq_dev, ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_OUTPUT_MODE:
		ret = _mtk_pq_common_set_output_mode(pq_dev, ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_FLOW_CONTROL:
		ret = _mtk_pq_common_set_flow_control(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_PQ_MEMORY_INDEX:
		ret = _mtk_pq_common_set_pq_memory_index(pq_dev, ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_AISR_ACTIVE_WIN:
		ret = _mtk_pq_common_set_aisr_active_win(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_PQ_S_BUFFER_INFO:
		ret = _mtk_pq_common_set_pix_format_info(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_STAGE_PATTERN:
		ret = _mtk_pq_common_set_stage_pattern(pq_dev, (void *)ctrl->p_new.p_u8);
		break;
	case V4L2_CID_SET_PATH:
		ret = _mtk_pq_common_set_path(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_CFD_SET_CURVE:
		ret = _mtk_pq_common_cfd_set_curve(pq_dev, ctrl->p_new.p);
		break;
	case V4L2_CID_SET_PATTERN_INFO:
		ret = mtk_pq_common_set_pattern_info(pq_dev, ctrl->p_new.p, NULL);
		break;
	case V4L2_CID_SET_PANEL_COLOR_INFO:
		ret = _mtk_pq_common_set_panel_color_info(pq_dev, ctrl->p_new.p);
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops common_ctrl_ops = {
	.g_volatile_ctrl = _common_g_ctrl,
	.s_ctrl = _common_s_ctrl,
};

static const struct v4l2_ctrl_config common_ctrl[] = {
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_INPUT_EXT_INFO,
		.name = "input ext info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_input_ext_info)},
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_OUTPUT_EXT_INFO,
		.name = "output ext info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_output_ext_info)},
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.name = "hflip enable",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.dims = {sizeof(u8)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "vflip enable",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.dims = {sizeof(u8)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_FORCE_P_MODE,
		.name = "force p mode enable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_DELAY_TIME,
		.name = "delay time",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_delaytime_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_INPUT_TIMING,
		.name = "input timing",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_timing)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_GEN_PATTERN,
		.name = "generate pattern",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_gen_pattern)},
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_STREAMMETA_DATA,
		.name = "Stream metadata",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_stream_meta_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_INPUT_MODE,
		.name = "set pq output mode",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = MTK_PQ_INPUT_MODE_MAX,
		.step = 1,
		.dims = {sizeof(u8)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_OUTPUT_MODE,
		.name = "set pq output mode",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = MTK_PQ_OUTPUT_MODE_MAX,
		.step = 1,
		.dims = {sizeof(u8)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_FLOW_CONTROL,
		.name = "set flow control",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_flow_control)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_PQ_MEMORY_INDEX,
		.name = "set pq memory index",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(u8)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_AISR_ACTIVE_WIN,
		.name = "set aisr active win",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_aisr_active_win)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_STREAM_DEBUG,
		.name = "Stream debug",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_stream_debug)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_PQ_S_BUFFER_INFO,
		.name = "set buffer info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_s_buffer_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_PQ_G_BUFFER_OUT_INFO,
		.name = "get buffer info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_g_buffer_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_PQ_G_BUFFER_CAP_INFO,
		.name = "get buffer info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_g_buffer_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_STAGE_PATTERN,
		.name = "set stage pattern",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct stage_pattern_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_PATH,
		.name = "set pq path info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_path_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_CFD_SET_CURVE,
		.name = "set calman cmd",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_cfd_curve_set)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_CFD_GET_CURVE,
		.name = "get calman cmd",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_cfd_curve_get)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_PATTERN_INFO,
		.name = "set pattern info cmd",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_pattern_info_set)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SET_PANEL_COLOR_INFO,
		.name = "set panel color info cmd",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_pq_panel_color_info)},
		.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops common_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops common_sd_internal_ops = {
};


int mtk_pq_register_common_subdev(
	struct v4l2_device *pq_dev,
	struct v4l2_subdev *subdev_common,
	struct v4l2_ctrl_handler *common_ctrl_handler)
{
	int ret = 0;
	__u32 count;
	__u32 ctrl_num = sizeof(common_ctrl)/sizeof(struct v4l2_ctrl_config);

	if ((!pq_dev) || (!subdev_common) || (!common_ctrl_handler))
		return -ENOMEM;

	v4l2_ctrl_handler_init(common_ctrl_handler, ctrl_num);
	for (count = 0; count < ctrl_num; count++)
		v4l2_ctrl_new_custom(common_ctrl_handler,
			&common_ctrl[count], NULL);

	ret = common_ctrl_handler->error;
	if (ret) {
		v4l2_err(pq_dev, "failed to create common ctrl handler\n");
		goto exit;
	}
	subdev_common->ctrl_handler = common_ctrl_handler;

	v4l2_subdev_init(subdev_common, &common_sd_ops);
	subdev_common->internal_ops = &common_sd_internal_ops;
	strlcpy(subdev_common->name, "mtk-common", sizeof(subdev_common->name));

	ret = v4l2_device_register_subdev(pq_dev, subdev_common);
	if (ret) {
		v4l2_err(pq_dev, "failed to register common subdev\n");
		goto exit;
	}

	if (!bPquEnable) {
		/* register stream off done event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_STREAM_OFF, mtk_pq_common_stream_off_cb);
		/* register dqbuf ready event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_DQUEUE, mtk_pq_common_dqbuf_cb);
		/* register close window done event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_DESTROY_WIN, mtk_pq_common_close_cb);
		/* register dovi frame meta event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_DOVI_FRAME_META,
			mtk_pq_common_dovi_frame_meta_cb);
		/* register bbd change event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_BBD_INFO, mtk_pq_common_bbd_cb);
		/* register hdr changes within a video event */
		pqu_msg_register_notify_func(PQU_MSG_REPLY_HDR_CHANGE_INFO,
			mtk_pq_common_hdr_change_cb);
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(common_ctrl_handler);
	return ret;
}

void mtk_pq_unregister_common_subdev(
	struct v4l2_subdev *subdev_common)
{
	if (subdev_common) {
		v4l2_ctrl_handler_free(subdev_common->ctrl_handler);
		v4l2_device_unregister_subdev(subdev_common);
	}
}

static int _mtk_pq_common_set_input_source_select(bool stub)
{
	int ret = 0;
	struct pq_common_triggen_input_src_sel paramIn;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramIn, 0, sizeof(struct pq_common_triggen_input_src_sel));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	//set input source select
	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	paramIn.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramIn.src = PQ_COMMON_TRIGEN_INPUT_TGEN;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_inputSrcSel(paramIn, true, &out, stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_inputSrcSel failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_htt_size(void)
{
	int ret = 0;
	struct pq_common_triggen_sw_htt_size paramInSwHtt;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInSwHtt, 0, sizeof(struct pq_common_triggen_sw_htt_size));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	//set sw htt
	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	paramInSwHtt.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInSwHtt.sw_htt = TRIG_SW_HTT;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swHttSize(paramInSwHtt, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swHttSize failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_sw_user_mode(void)
{
	int ret = 0;
	struct pq_common_triggen_sw_user_mode paramInSwUserMode;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInSwUserMode, 0, sizeof(struct pq_common_triggen_sw_user_mode));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	//set sw user mode
	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	paramInSwUserMode.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInSwUserMode.sw_user_mode_h = true;
	paramInSwUserMode.sw_user_mode_v = true;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_swUserMode(paramInSwUserMode, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_swUserMode failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_vcount_keep_enable(void)
{
	int ret = 0;
	struct pq_common_triggen_vcnt_keep_enable paramInVcntKeepEn;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInVcntKeepEn, 0, sizeof(struct pq_common_triggen_vcnt_keep_enable));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	//set vcnt keep enable
	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	paramInVcntKeepEn.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInVcntKeepEn.vcnt_keep_en = true;
	paramInVcntKeepEn.vcnt_upd_mask_range = TRIG_VCNT_UPD_MSK_RNG;

	/* call hwreg to get addr/mask */
	ret = drv_hwreg_pq_common_triggen_set_vcntKeepEnable(paramInVcntKeepEn, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vcntKeepEnable failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_vsync_delay_trigger(void)
{
	int ret = 0;
	struct pq_common_triggen_vs_dly paramInVsDelay;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInVsDelay, 0, sizeof(struct pq_common_triggen_vs_dly));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	paramInVsDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInVsDelay.vs_dly_h = TRIG_VS_DLY_H;
	paramInVsDelay.vs_dly_v = TRIG_VS_DLY_V;
	paramInVsDelay.vs_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set vsync delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_vsyncTrig(paramInVsDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_vsyncTrig failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_dma_rd_trigger(void)
{
	int ret = 0;
	struct pq_common_triggen_dma_r_dly paramInDmaRDelay;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInDmaRDelay, 0, sizeof(struct pq_common_triggen_dma_r_dly));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	paramInDmaRDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInDmaRDelay.dma_r_dly_h = TRIG_DMA_R_DLY_H;
	paramInDmaRDelay.dma_r_dly_v = TRIG_DMA_R_DLY_V;
	paramInDmaRDelay.dmar_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set dma_rd trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_dmaRdTrig(paramInDmaRDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_dmaRdTrig failed!\n");
		return ret;
	}

	return 0;
}

static int _mtk_pq_common_set_str_delay_trigger(void)
{
	int ret = 0;
	struct pq_common_triggen_str_dly paramInStrDelay;
	struct reg_info reg[REG_SIZE];
	struct hwregOut out;

	memset(&paramInStrDelay, 0, sizeof(struct pq_common_triggen_str_dly));
	memset(reg, 0, sizeof(reg));
	memset(&out, 0, sizeof(out));

	out.reg = reg;

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2R;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_IP;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP1;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_OP2;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}

	paramInStrDelay.domain = PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM;
	paramInStrDelay.str_dly_v = TRIG_STR_DLY_V;
	paramInStrDelay.str_trig_sel = PQ_COMMON_TRIGEN_TRIG_SEL_VS;

	/* set str delay trigger gen */
	ret = drv_hwreg_pq_common_triggen_set_strTrig(paramInStrDelay, true, &out);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"drv_hwreg_pq_common_triggen_set_strTrig failed!\n");
		return ret;
	}


	return 0;
}

int mtk_pq_common_trigger_gen_init(bool stub)
{
	int ret = 0;

	ret = _mtk_pq_common_set_input_source_select(stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_input_source_select failed!\n");
		return ret;
	}

	if (stub)
		return 0;

	ret = _mtk_pq_common_set_htt_size();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_htt_size failed!\n");
		return ret;
	}

	ret = _mtk_pq_common_set_sw_user_mode();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_sw_user_mode failed!\n");
		return ret;
	}

	ret = _mtk_pq_common_set_vcount_keep_enable();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_vcount_keep_enable failed!\n");
		return ret;
	}

	/* If DS/ML can't finish before vsync, need to adjust following triggers*/
	ret = _mtk_pq_common_set_vsync_delay_trigger();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_vsync_delay_trigger failed!\n");
		return ret;
	}

	ret = _mtk_pq_common_set_dma_rd_trigger();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_dma_rd_trigger failed!\n");
		return ret;
	}

	ret = _mtk_pq_common_set_str_delay_trigger();
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_common_set_str_delay_trigger failed!\n");
		return ret;
	}

	return 0;
}

static bool _mtk_pq_common_thermal_change(struct mtk_pq_platform_device *pqdev,
	struct mtk_pq_device *pq_dev,
	uint8_t win_id)
{
	static struct mtk_pq_thermal _thermal_info[PQ_WIN_MAX_NUM] = {0};
	bool bRet = false;

	if (pq_dev == NULL || pqdev == NULL || (win_id >= PQ_WIN_MAX_NUM))
		return false;

	if (pq_dev->pqu_queue_ext_info.bPerFrameMode == true)
		return false;

	if (pqdev->pqcaps.u32AISR_Support) {
		if (pq_dev->thermal_info.thermal_state_aisr !=
			_thermal_info[win_id].thermal_state_aisr) {
			_thermal_info[win_id].thermal_state_aisr =
				pq_dev->thermal_info.thermal_state_aisr;
			bRet = true;
		} else if ((pq_dev->thermal_info.thermal_state_aisr
			== MTK_PQ_THERMAL_STATE_AISR_ON)
			&& (_pqu_thermal_info[win_id].thermal_algo_state_aisr
				!= MTK_PQ_ALGO_THERMAL_AISR_ON))
			bRet = true;
		else if ((pq_dev->thermal_info.thermal_state_aisr
			== MTK_PQ_THERMAL_STATE_AISR_HALF)
			&& (_pqu_thermal_info[win_id].thermal_algo_state_aisr
				!= MTK_PQ_ALGO_THERMAL_AISR_HALF))
			bRet = true;
		else if ((pq_dev->thermal_info.thermal_state_aisr
			== MTK_PQ_THERMAL_STATE_AISR_OFF)
			&& (_pqu_thermal_info[win_id].thermal_algo_state_aisr
				!= MTK_PQ_ALGO_THERMAL_AISR_OFF))
			bRet = true;

		if (bRet)
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
				"Thermal aisr state(0:ON,1:KEEP,2:HALF,3:OFF)=%u, %s=%d\n",
				pq_dev->thermal_info.thermal_state_aisr,
				"Thermal algo(0:ON,1:OFF,2:HALF,3:PROCESS,4:NONE)",
				_pqu_thermal_info[win_id].thermal_algo_state_aisr);
	}

	return bRet;
}

static int thermal_handling_task(void *data)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	uint8_t win_id = 0;
	struct v4l2_event ev = {0};

	if (data == NULL)
		return -EINVAL;

	pqdev = (struct mtk_pq_platform_device *)data;
	ev.type = V4L2_EVENT_MTK_PQ_RE_QUEUE_TRIGGER;
	ev.u.data[0] = V4L2_RE_QUEUE_THERMAL;
	while (!kthread_should_stop()) {
		msleep(THERMAL_POLL_TIME);
		for (win_id = 0; win_id < pqdev->usable_win; win_id++) {
			pq_dev = pqdev->pq_devs[win_id];
			if (pq_dev->input_stream.streaming) {
				mtk_pq_cdev_get_thermal_info(&pq_dev->thermal_info);
				if (_mtk_pq_common_thermal_change(pqdev, pq_dev, win_id))
					v4l2_event_queue(&(pq_dev->video_dev), &ev);
			}
		}
	}

	return 0;
}

int mtk_pq_common_create_task(struct mtk_pq_platform_device *pqdev,
	struct mtk_pq_device *pq_dev, __u8 input_source)
{
	spinlock_t *spinlock_thermal_handling_task = NULL;
	unsigned long flags = 0;
	struct task_struct *task_thermal_handling = NULL;
	struct meta_pq_stream_info *stream_meta = NULL;

	if (pqdev == NULL || pq_dev == NULL)
		return -ENOMEM;

	if (!pqdev->pq_thermal_ctrl_en)
		return 0;

	stream_meta = &(pq_dev->stream_meta);

	if ((stream_meta->pq_mode == meta_pq_mode_legacy) &&
		(!IS_INPUT_DTV(input_source) && !IS_INPUT_MM(input_source))) {
		if (!pqdev->thermal_task_available) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "create task start\n");
			spinlock_thermal_handling_task = &pqdev->spinlock_thermal_handling_task;

			task_thermal_handling = kthread_create(
				thermal_handling_task,
				pqdev,
				"thermal handling task");

			if (IS_ERR(task_thermal_handling)) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "task not created\n");
				return PTR_ERR(task_thermal_handling);
			}

			get_task_struct(task_thermal_handling);
			wake_up_process(task_thermal_handling);

			spin_lock_irqsave(spinlock_thermal_handling_task, flags);
			pqdev->thermal_handling_task = task_thermal_handling;
			spin_unlock_irqrestore(spinlock_thermal_handling_task, flags);

			pqdev->thermal_task_available = true;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "create task end\n");
		}
	}

	return 0;
}

int mtk_pq_common_destroy_task(struct mtk_pq_platform_device *pqdev)
{
	spinlock_t *spinlock_thermal_handling_task = NULL;
	unsigned long flags = 0;
	struct task_struct *task_thermal_handling = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "common_destroy_task start\n");

	if (pqdev == NULL)
		return -ENOMEM;

	if (!pqdev->pq_thermal_ctrl_en)
		return 0;

	spinlock_thermal_handling_task = &pqdev->spinlock_thermal_handling_task;

	spin_lock_irqsave(spinlock_thermal_handling_task, flags);
	task_thermal_handling = pqdev->thermal_handling_task;
	pqdev->thermal_handling_task = NULL;
	spin_unlock_irqrestore(spinlock_thermal_handling_task, flags);

	if (IS_ERR(task_thermal_handling)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "task not destroyed\n");
		return PTR_ERR(task_thermal_handling);
	}

	if (task_thermal_handling != NULL) {
		kthread_stop(task_thermal_handling);
		put_task_struct(task_thermal_handling);
	}

	pqdev->thermal_task_available = false;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "common_destroy_task end\n");

	return 0;
}

static void mtk_pq_common_bbd_cb(
	int error_cause,
	pid_t thread_id,
	uint32_t user_param,
	void *param)
{
	struct msg_bbd_info *msg_info = (struct msg_bbd_info *)param;
	struct v4l2_event ev;
	struct mtk_pq_device *pq_dev = NULL;
	uint8_t data_idx = 0;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter bbd cb!\n");

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memset(&ev, 0, sizeof(struct v4l2_event));

	pq_dev = pqdev->pq_devs[msg_info->win_id];

	ev.type = V4L2_EVENT_MTK_PQ_BBD_TRIGGER;
	ev.u.data[data_idx] = msg_info->u8Validity;
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16LeftOuterPos & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16LeftOuterPos & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16RightOuterPos & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16RightOuterPos & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16LeftInnerPos & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16LeftInnerPos & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16LeftInnerConf & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16LeftInnerConf & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16RightInnerPos & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16RightInnerPos & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16RightInnerConf & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16RightInnerConf & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16CurTopContentY & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16CurTopContentY & MASK_H_U16) >> PQ_NUM_8);
	data_idx++;
	ev.u.data[data_idx] = (__u8) (msg_info->u16CurBotContentY & MASK_L_U16);
	data_idx++;
	ev.u.data[data_idx] = (__u8) ((msg_info->u16CurBotContentY & MASK_H_U16) >> PQ_NUM_8);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s : %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"meta_pq_bbd_info",
		"u8Validity = ", msg_info->u8Validity,
		"u16LeftOuterPos = ", msg_info->u16LeftOuterPos,
		"u16RightOuterPos = ", msg_info->u16RightOuterPos,
		"u16LeftInnerPos = ", msg_info->u16LeftInnerPos,
		"u16LeftInnerConf = ", msg_info->u16LeftInnerConf,
		"u16RightInnerPos = ", msg_info->u16RightInnerPos,
		"u16RightInnerConf = ", msg_info->u16RightInnerConf,
		"u16CurTopContentY = ", msg_info->u16CurTopContentY,
		"u16CurBotContentY = ", msg_info->u16CurBotContentY);

	v4l2_event_queue(&(pq_dev->video_dev), &ev);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "bbd event cb done\n");
}

static void mtk_pq_common_hdr_change_cb(
	int error_cause,
	pid_t thread_id,
	uint32_t user_param,
	void *param)
{
	struct msg_hdr_change_info *msg_info = NULL;
	struct v4l2_event ev;
	struct mtk_pq_device *pq_dev = NULL;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "enter hdr_change cb!\n");

	msg_info = (struct msg_hdr_change_info *)param;

	if (!param) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	memset(&ev, 0, sizeof(struct v4l2_event));

	pq_dev = pqdev->pq_devs[msg_info->win_id];

	if (!pq_dev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return;
	}

	ev.type = V4L2_EVENT_MTK_PQ_RE_QUEUE_TRIGGER;
	ev.u.data[0] = V4L2_RE_QUEUE_HDR_CHANGE;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s : %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"meta_pq_hdr_change_info",
		"hdr_type = ", msg_info->hdr_type,
		"data_format = ", msg_info->data_format,
		"color_range = ", msg_info->color_range,
		"cp = ", msg_info->cp,
		"tr = ", msg_info->tr,
		"mc = ", msg_info->mc,
		"frame_id = ", msg_info->frame_id,
		"stream_id = ", msg_info->stream_id);

	v4l2_event_queue(&(pq_dev->video_dev), &ev);
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG, "hdr change event cb done\n");
}

int mtk_pq_common_suspend(bool stub)
{
	int ret = 0;

	ret = mtk_pq_common_irq_suspend(stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_irq_resume failed!\n");
		return ret;
	}
	_mtk_pq_common_release_all_frame();
	isEnableRv55PquCurrent = false;
	bUcdConfigInit = false;

	return 0;
}
EXPORT_SYMBOL(mtk_pq_common_suspend);

int mtk_pq_common_resume(bool stub, uint32_t u32IRQ_Version)
{
	int ret = 0;

	ret = mtk_pq_common_trigger_gen_init(stub);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_trigger_gen_init failed!\n");
		return ret;
	}

	ret = mtk_pq_common_irq_resume(stub, u32IRQ_Version);
	if (ret < 0) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk_pq_common_irq_resume failed!\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_pq_common_resume);
