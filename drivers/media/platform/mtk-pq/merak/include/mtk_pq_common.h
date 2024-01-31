/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_COMMON_H
#define _MTK_PQ_COMMON_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <uapi/linux/mtk-v4l2-pq.h>
#include "metadata_utility.h"
#include "pqu_msg.h"
#include "mtk_iommu_dtv_api.h"
#include "hwreg_common_bin.h"

#define PQ_MALLOC_SIZE_4K (1024 * 4)

#define PQ_CHECK_NULLPTR() \
	do { \
		pr_err("[%s,%5d] NULL POINTER\n", __func__, __LINE__); \
		dump_stack(); \
	} while (0)

#define PQ_MALLOC(p, size) \
	do { \
		if (size < PQ_MALLOC_SIZE_4K) \
			p = kmalloc(size, GFP_KERNEL); \
		else \
			p = vmalloc((size)); \
	} while (0)

#define PQ_FREE(p, size) \
	do { \
		if ((size) < PQ_MALLOC_SIZE_4K) { \
			kfree((void *)p); p = NULL; \
		} else { \
			vfree((void *)p); p = NULL; \
		} \
	} while (0)

#define PQ_NUM_16 (16)
#define PQ_NUM_8 (8)
#define PQ_NUM_4 (4)

#define MASK_L_U16 (0x00FF)
#define MASK_H_U16 (0xFF00)

enum mtk_pq_common_metatag {
	EN_PQ_METATAG_VDEC_SVP_INFO = 0,
	EN_PQ_METATAG_SRCCAP_SVP_INFO,
	EN_PQ_METATAG_SVP_INFO,
	EN_PQ_METATAG_SH_FRM_INFO,
	EN_PQ_METATAG_DISP_MDW_CTRL,
	EN_PQ_METATAG_DISP_IDR_CTRL,
	EN_PQ_METATAG_DISP_B2R_CTRL,
	EN_PQ_METATAG_DISP_B2R_FILM_GRAIN,
	EN_PQ_METATAG_PQU_DISP_FLOW_INFO,
	EN_PQ_METATAG_OUTPUT_FRAME_INFO,
	EN_PQ_METATAG_SRCCAP_FRAME_INFO,
	EN_PQ_METATAG_QBUF_INTERNAL_INFO,
	EN_PQ_METATAG_STREAM_INTERNAL_INFO,
	EN_PQ_METATAG_PQU_DEBUG_INFO,
	EN_PQ_METATAG_PQU_STREAM_INFO,
	EN_PQ_METATAG_PQU_BUFFER_INFO,
	EN_PQ_METATAG_PQU_PATTERN_INFO,
	EN_PQ_METATAG_PQU_PATTERN_SIZE_INFO,
	EN_PQ_METATAG_BBD_INFO,
	EN_PQ_METATAG_PQU_BBD_INFO,
	EN_PQ_METATAG_STREAM_INFO,
	EN_PQ_METATAG_DISPLAY_FLOW_INFO,
	EN_PQ_METATAG_INPUT_QUEUE_EXT_INFO,
	EN_PQ_METATAG_OUTPUT_QUEUE_EXT_INFO,
	EN_PQ_METATAG_PQU_QUEUE_EXT_INFO,
	EN_PQ_METATAG_FORCEP_INFO,
	EN_PQ_METATAG_PQU_FORCEP_INFO,
	EN_PQ_METATAG_OUTPUT_DISP_IDR_CTRL,
	EN_PQ_METATAG_DV_DEBUG,
	EN_PQ_METATAG_PQPARAM_TAG,
	EN_PQ_METATAG_PQU_PQPARAM_TAG,
	EN_PQ_METATAG_DISPLAY_WM_INFO,
	EN_PQ_METATAG_PQU_DISP_WM_INFO,
	EN_PQ_METATAG_DV_INFO,
	EN_PQ_METATAG_SRCCAP_DV_HDMI_INFO,
	EN_PQ_METATAG_VDEC_DV_DESCRB_INFO,
	EN_PQ_METATAG_VDEC_DV_PARSING_TAG,
	EN_PQ_METATAG_THERMAL_INFO,
	EN_PQ_METATAG_PQU_THERMAL_INFO,
	EN_PQ_METATAG_DV_OUT_FRAME_INFO,
	EN_PQ_METATAG_PQ_CUSTOMER1_TAG,
	EN_PQ_METATAG_PQ_CUSTOMER1_PQU_TAG,
	EN_PQ_METATAG_FRM_STATISTIC,
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_SDR_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_SDR_TAG,
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HDR_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HDR_TAG,
	EN_PQ_METATAG_CFD_SHM_CALMAN_CURVE_HLG_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_CURVE_HLG_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN0_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN0_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN1_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN1_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN2_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN2_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN3_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN3_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN4_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN4_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN5_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN5_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN6_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN6_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN7_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN7_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN8_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN8_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN9_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN9_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN10_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN10_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN11_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN11_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN12_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN12_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN13_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN13_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN14_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN14_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_WIN15_CURVE_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_WIN15_CURVE_TAG,
	EN_PQ_METATAG_CFD_SHM_CALMAN_HDR_CONF_TAG,
	EN_PQ_METATAG_PQU_CFD_SHM_CALMAN_HDR_CONF_TAG,
	EN_PQ_METATAG_LOCALHSY_TAG,
	EN_PQ_METATAG_PQU_LOCALHSY_TAG,
	EN_PQ_METATAG_LOCALVAC_TAG,
	EN_PQ_METATAG_PQU_LOCALVAC_TAG,
	EN_PQ_METATAG_VSYNC_INFO_TAG,
	EN_PQ_METATAG_PQU_VSYNC_INFO_TAG,
	EN_PQ_METATAG_MAX,
};

struct v4l2_common_info {
	__u32 input_source;
	__u32 op2_diff;
	__u32 diff_count;
	enum mtk_pq_input_mode input_mode;
	enum mtk_pq_output_mode output_mode;
	struct v4l2_timing timing_in;
	struct v4l2_format format_cap;
	struct v4l2_format format_out;
	bool v_flip;
	enum v4l2_pq_path pq_path;
	bool first_slice_frame;
};

struct mtk_pq_dma_buf;
struct mtk_pq_buffer;
struct mtk_pq_device;
struct mtk_pq_platform_device;

extern bool bPquEnable;
extern bool isRv55Init;

struct v4l2_device;
struct v4l2_subdev;
struct v4l2_ctrl_handler;
/* function*/
int mtk_pq_common_trigger_gen_init(bool stub);
int mtk_pq_common_set_input(struct mtk_pq_device *pq_dev,
		unsigned int input_index);
int mtk_pq_common_get_input(struct mtk_pq_device *pq_dev,
		unsigned int *p_input_index);
int mtk_pq_common_set_fmt_cap_mplane(
		struct mtk_pq_device *pq_dev,
		struct v4l2_format *fmt);
int mtk_pq_common_set_fmt_out_mplane(
		struct mtk_pq_device *pq_dev,
		struct v4l2_format *fmt);
int mtk_pq_common_get_fmt_cap_mplane(
		struct mtk_pq_device *pq_dev,
		struct v4l2_format *fmt);
int mtk_pq_common_get_fmt_out_mplane(
		struct mtk_pq_device *pq_dev,
		struct v4l2_format *fmt);
int mtk_pq_register_common_subdev(struct v4l2_device *pq_dev,
		struct v4l2_subdev *subdev_common,
		struct v4l2_ctrl_handler *common_ctrl_handler);
void mtk_pq_unregister_common_subdev(struct v4l2_subdev *subdev_common);
int mtk_pq_common_align_metadata(struct mtk_pq_device *pq_dev,
	enum mtk_pq_common_metatag meta_tag);
int mtk_pq_common_read_metadata(int meta_fd,
	enum mtk_pq_common_metatag meta_tag, void *meta_content);
int mtk_pq_common_read_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag, void *meta_content);
int mtk_pq_common_read_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void *meta_content);
int mtk_pq_common_read_metadata_addr_ptr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void **meta_content);
int mtk_pq_common_write_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag, void *meta_content);
int mtk_pq_common_write_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag, void *meta_content);
int mtk_pq_common_delete_metadata(int meta_fd,
	enum mtk_pq_common_metatag meta_tag);
int mtk_pq_common_delete_metadata_db(struct dma_buf *meta_db,
	enum mtk_pq_common_metatag meta_tag);
int mtk_pq_common_delete_metadata_addr(struct meta_buffer *meta_buf,
	enum mtk_pq_common_metatag meta_tag);
int mtk_pq_common_get_dma_buf(struct mtk_pq_device *pq_dev,
	struct mtk_pq_dma_buf *buf);
int mtk_pq_common_put_dma_buf(struct mtk_pq_dma_buf *buf);
int mtk_pq_common_stream_on(struct file *file, int type,
	struct mtk_pq_device *pq_dev,
	struct msg_stream_on_info *msg_info);
int mtk_pq_common_stream_off(struct mtk_pq_device *pq_dev, int type,
	struct msg_stream_off_info *msg_info);
int mtk_pq_common_qbuf(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf,
	struct msg_queue_info *msg_info);
int mtk_pq_common_config(struct msg_config_info *msg_info, bool is_pqu);
int mtk_pq_common_suspend(bool stub);
int mtk_pq_common_resume(bool stub, uint32_t u32IRQ_Version);
int mtk_pq_common_set_path_enable(struct mtk_pq_device *pq_dev, enum v4l2_pq_path pq_path);
int mtk_pq_common_set_path_disable(struct mtk_pq_device *pq_dev, enum v4l2_pq_path pq_path);
int mtk_pq_common_qslice(struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_src_buf,
	struct mtk_pq_buffer *pq_dst_buf);
int mtk_pq_common_is_slice(struct mtk_pq_device *pq_dev, struct mtk_pq_buffer *pq_src_buf);
int mtk_pq_common_init(struct mtk_pq_platform_device *pqdev);
int mtk_pq_common_set_pattern_info(struct mtk_pq_device *pq_dev, void *ctrl, void *cmd);
int mtk_pq_common_create_task(struct mtk_pq_platform_device *pqdev,
	struct mtk_pq_device *pq_dev, __u8 input_source);
int mtk_pq_common_destroy_task(struct mtk_pq_platform_device *pqdev);
#endif
