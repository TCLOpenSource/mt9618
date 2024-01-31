/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_MEMOUT_H
#define MTK_SRCCAP_MEMOUT_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/mtk-v4l2-srccap.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#include "mtk_srccap_memout_svp.h"
#include "metadata_utility.h"
/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
#define SRCCAP_MEMOUT_BUF_PLANE_NUM_0 (0)
#define SRCCAP_MEMOUT_BUF_PLANE_NUM_1 (1)
#define SRCCAP_MEMOUT_BUF_PLANE_NUM_2 (2)
#define SRCCAP_MEMOUT_BUF_PLANE_NUM_3 (3)
#define SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM (SRCCAP_MEMOUT_BUF_PLANE_NUM_3)
#define SRCCAP_MEMOUT_FRAME_NUM_5 (5)
#define SRCCAP_MEMOUT_FRAME_NUM_10 (10)
#define SRCCAP_MEMOUT_FRAME_NUM_12 (12)
#define SRCCAP_MEMOUT_FRAME_NUM_14 (14)
#define SRCCAP_MEMOUT_4K_RESOLUTION (3840*2160)
#define SRCCAP_MEMOUT_8K_RESOLUTION (7680*4320)
#define SRCCAP_MEMOUT_1080P_RESOLUTION (1920*1080)
#define SRCCAP_MEMOUT_1080I_RESOLUTION (1920*1080/2)


#define SRCCAP_MEMOUT_RW_DIFF (2)
#define SRCCAP_MEMOUT_ONE_INDEX (1)
#define SRCCAP_MEMOUT_TWO_INDEX (2)
#define SRCCAP_MEMOUT_MAX_W_INDEX (31)
#define SRCCAP_MEMOUT_TOP_FIELD (1)
#define SRCCAP_MEMOUT_BOTTOM_FIELD (0)
#define SRCCAP_MEMOUT_MIN_BUF_FOR_CAPTURING_FIELDS (2)
#define SRCCAP_MEMOUT_MAX_THREAD_PROCESSING_TIME (8000)
#define SRCCAP_MEMOUT_REG_NUM (10)
#define SRCCAP_MEMOUT_MAX_CB_NUM (10)

#define SRCCAP_MEMOUT_BITMODE_INDEX_0 (0)
#define SRCCAP_MEMOUT_BITMODE_INDEX_1 (1)
#define SRCCAP_MEMOUT_BITMODE_INDEX_2 (2)
#define SRCCAP_MEMOUT_BITMODE_VALUE_0 (0)
#define SRCCAP_MEMOUT_BITMODE_VALUE_5 (5)
#define SRCCAP_MEMOUT_BITMODE_VALUE_6 (6)
#define SRCCAP_MEMOUT_BITMODE_VALUE_8 (8)
#define SRCCAP_MEMOUT_BITMODE_VALUE_10 (10)
#define SRCCAP_MEMOUT_BITMODE_VALUE_12 (12)
#define SRCCAP_MEMOUT_BITMODE_ALPHA_0 (0)
#define SRCCAP_MEMOUT_BITMODE_ALPHA_2 (2)
#define SRCCAP_MEMOUT_BITMODE_ALPHA_8 (8)


#define SRCCAP_MEMOUT_CE_INDEX_0 (0)
#define SRCCAP_MEMOUT_CE_INDEX_1 (1)
#define SRCCAP_MEMOUT_CE_INDEX_2 (2)
#define SRCCAP_MEMOUT_CE_VALUE_0 (0)
#define SRCCAP_MEMOUT_CE_VALUE_1 (1)
#define SRCCAP_MEMOUT_CE_VALUE_2 (2)

/*refer from DV_DMA_ACCESS_MODE_422 & DV_DMA_ACCESS_MODE_444 */
#define SRCCAP_MEMOUT_ACCESS_MODE_444   (0x7)
#define SRCCAP_MEMOUT_ACCESS_MODE_422   (0x6)

#define SRCCAP_MEMOUT_MIU_PORT_NUM_14 (14)
#define SRCCAP_MEMOUT_MIU_PORT_NUM_15 (15)
#define SRCCAP_MEMOUT_MIU_PORT_NUM_16 (16)
#define SRCCAP_MEMOUT_MIU_PORT_NUM_17 (17)
extern int mtk_miu2gmc_mask(u32 miu2gmc_id, u32 port_id, bool enable);

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */
#define SRCCAP_MEMOUT_ALIGN(val, align) (((val) + ((align) - 1)) & (~((align) - 1)))

/* Interrupt callback function */
typedef void (*SRCCAP_MEMOUT_INTCB) (void *param);

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
enum srccap_memout_buffer_stage {
	SRCCAP_MEMOUT_BUFFER_STAGE_NEW = 0,
	SRCCAP_MEMOUT_BUFFER_STAGE_CAPTURING,
	SRCCAP_MEMOUT_BUFFER_STAGE_DONE,
};

enum srccap_memout_metatag {
	SRCCAP_MEMOUT_METATAG_FRAME_INFO = 0,
	SRCCAP_MEMOUT_METATAG_SVP_INFO,
	SRCCAP_MEMOUT_METATAG_COLOR_INFO,
	SRCCAP_MEMOUT_METATAG_HDR_PKT,
	SRCCAP_MEMOUT_METATAG_HDR_VSIF_PKT,
	SRCCAP_MEMOUT_METATAG_HDR_EMP_PKT,
	SRCCAP_MEMOUT_METATAG_SPD_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_AVI_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_GCP_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_SPD_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_VSIF_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_HDR_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_AUI_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_MPG_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_VS_EMP_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_DSC_EMP_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_VRR_EMP_PKT,
	SRCCAP_MEMOUT_METATAG_HDMIRX_DHDR_EMP_PKT,
	SRCCAP_MEMOUT_METATAG_DIP_POINT_INFO,
};

enum srccap_memout_callback_stage {
	SRCCAP_MEMOUT_CB_STAGE_ENABLE_W_BUF = 0,
	SRCCAP_MEMOUT_CB_STAGE_DISABLE_W_BUF,
	SRCCAP_MEMOUT_CB_STAGE_MAX,
};

enum srccap_memout_idr_output_select {
	SRCCAP_MEMOUT_OUTPUT_SELECT_OFF = 0,
	SRCCAP_MEMOUT_OUTPUT_SELECT_IDR,
	SRCCAP_MEMOUT_OUTPUT_SELECT_IDW0,
	SRCCAP_MEMOUT_OUTPUT_SELECT_IDW1,
	SRCCAP_MEMOUT_OUTPUT_SELECT_BLK,
	SRCCAP_MEMOUT_OUTPUT_SELECT_MAX,
};

/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct mtk_srccap_dev;

enum srccap_memout_format {
	SRCCAP_MEMOUT_FORMAT_YUV420 = 0,
	SRCCAP_MEMOUT_FORMAT_YUV422 = 2,
	SRCCAP_MEMOUT_FORMAT_YUV444 = 4,
	SRCCAP_MEMOUT_FORMAT_RGB = 11,
	SRCCAP_MEMOUT_FORMAT_ARGB = 12,
	SRCCAP_MEMOUT_FORMAT_ABGR = 13,
	SRCCAP_MEMOUT_FORMAT_RGBA = 14,
	SRCCAP_MEMOUT_FORMAT_BGRA = 15,
};

struct srccap_memout_frame_capturing_info {
	enum srccap_memout_format fmt;
	u8 bitmode[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM];
	u8 ce[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM];
	bool argb;
	bool pack;
	u8 num_planes;
};

struct srccap_memout_buffer_entry {
	struct vb2_buffer *vb;
	enum srccap_memout_buffer_stage stage;
	u64 addr[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM];
	u64 fd_size;
	u32 output_width;
	u32 output_height;
	u8 w_index;
	u8 field;
	u8 buf_cnt_id;
	struct list_head list;
	struct srccap_memout_frame_capturing_info color_fmt_info;
};

struct srccap_memout_frame_queue {
	struct list_head inputq_head;
	struct list_head outputq_head;
	struct list_head processingq_head;
};

struct srccap_memout_cb_info {
	void *param;
	SRCCAP_MEMOUT_INTCB cb;
};

struct srccap_memout_info {
	enum v4l2_memout_buf_ctrl_mode buf_ctrl_mode;
#if IS_ENABLED(CONFIG_OPTEE)
	struct srccap_memout_svp_info  secure_info;
#endif
	struct v4l2_pix_format_mplane fmt_mp;
	u8 num_planes;
	u8 num_bufs;
	u32 frame_pitch[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM]; // unit: byte
	u8 access_mode;
	struct srccap_memout_frame_queue frame_q;
	u32 frame_id;
	u8 frame_num;
	bool first_frame;
	bool streamon_interlaced;
	u32 hoffset;
	u8 field_info;
	struct srccap_memout_cb_info
		cb_info[SRCCAP_MEMOUT_CB_STAGE_MAX][SRCCAP_MEMOUT_MAX_CB_NUM];
	u32 pre_calculate_frame_pitch[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM]; // unit: byte
	struct srccap_memout_frame_capturing_info color_fmt_info;
};

/* ============================================================================================== */
/* ----------------------------------------- Variables ------------------------------------------ */
/* ============================================================================================== */


/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
int mtk_memout_read_metadata(
	enum srccap_memout_metatag meta_tag,
	void *meta_content,
	struct meta_buffer *buffer);
int mtk_memout_write_metadata(
	enum srccap_memout_metatag meta_tag,
	void *meta_content,
	struct meta_buffer *pbuffer);
int mtk_memout_write_metadata_mmap_fd(
	int meta_fd,
	enum srccap_memout_metatag meta_tag,
	void *meta_content);
int mtk_memout_remove_metadata(
	int meta_fd,
	enum srccap_memout_metatag meta_tag);
void mtk_memout_process_buffer(
	struct mtk_srccap_dev *srccap_dev,
	u8 src_field);
int mtk_memout_s_fmt_vid_cap(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format);
int mtk_memout_s_fmt_vid_cap_mplane(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format);
int mtk_memout_g_fmt_vid_cap(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format);
int mtk_memout_g_fmt_vid_cap_mplane(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format);
int mtk_memout_reqbufs(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_requestbuffers *req_buf);
int mtk_memout_qbuf(
	struct mtk_srccap_dev *srccap_dev,
	struct vb2_buffer *vb);
int mtk_memout_dqbuf(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_buffer *buf);
int mtk_memout_streamon(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_buf_type buf_type);
int mtk_memout_streamoff(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_buf_type buf_type);
void mtk_memout_vsync_isr(
	void *param);
int mtk_srccap_register_memout_subdev(
	struct v4l2_device *srccap_dev,
	struct v4l2_subdev *subdev_memout,
	struct v4l2_ctrl_handler *memout_ctrl_handler);
void mtk_srccap_unregister_memout_subdev(
	struct v4l2_subdev *subdev_memout);
void mtk_memout_set_capture_size(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_ml_script_info *ml_script_info,
	struct v4l2_area_size *capture_size);
int mtk_memout_release_buffer(
	struct mtk_srccap_dev *srccap_dev);
int mtk_memout_register_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_memout_callback_stage stage,
	SRCCAP_MEMOUT_INTCB intcb,
	void *param);
int mtk_memout_unregister_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_memout_callback_stage stage,
	SRCCAP_MEMOUT_INTCB intcb,
	void *param);
void mtk_memount_set_frame_num(
	struct mtk_srccap_dev *srccap_dev);
u8 mtk_memount_get_frame_num(
	struct mtk_srccap_dev *srccap_dev);
int mtk_memout_s_pre_calculate_size(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_pre_calu_size *format);
int mtk_memout_g_pre_calculate_size(
	struct mtk_srccap_dev *srccap_dev);
int mtk_memout_get_min_bufs_for_capture(
	struct mtk_srccap_dev *srccap_dev,
	s32 *min_bufs);
int mtk_memout_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size);
int mtk_memout_calculate_frame_size(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_pre_calu_size *format,
	u32 pitch_out[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM]);
#endif  // MTK_SRCCAP_MEMOUT_H
