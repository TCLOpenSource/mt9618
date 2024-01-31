/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_H
#define _MTK_PQ_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/videobuf2-v4l2.h>
#include <uapi/linux/mtk-v4l2-pq.h>
#include <uapi/linux/metadata_utility/m_pq.h>
#include <uapi/linux/metadata_utility/m_srccap.h>
#include <uapi/linux/metadata_utility/m_vdec.h>
#include "metadata_utility.h"

#include "mtk_pq_common.h"
#include "mtk_pq_b2r.h"
#include "mtk_pq_enhance.h"
#include "mtk_pq_display.h"
#include "mtk_pq_pattern.h"
#include "mtk_pq_dv.h"
#include "mtk_pq_idk.h"
#include "mtk_pq_thermal.h"

#define PQ_WIN_MAX_NUM			(16)
#define DROP_ARY_IDX			(32)
#define PQ_NEVENTS			(64)
#define PQU_INPUT_STREAM_OFF_DONE	(BIT(0))
#define PQU_OUTPUT_STREAM_OFF_DONE	(BIT(1))
#define PQU_CLOSE_WIN_DONE		(BIT(2))
#define PQ_FILE_PATH_LENGTH		(256)
#define PQ_IRQ_MAX_NUM			(16)
#define IDR_DEBUG_MAX	6
#define MAX_WIN_NUM (16)
#define MAX_DEV_NAME_LEN (16)

#define STI_PQ_LOG_LEVEL_ERROR		BIT(0)
#define STI_PQ_LOG_LEVEL_COMMON		BIT(1)
#define STI_PQ_LOG_LEVEL_ENHANCE	BIT(2)
#define STI_PQ_LOG_LEVEL_B2R		BIT(3)
#define STI_PQ_LOG_LEVEL_DISPLAY	BIT(4)
#define STI_PQ_LOG_LEVEL_IDR		BIT(5)
#define STI_PQ_LOG_LEVEL_MDW		BIT(6)
#define STI_PQ_LOG_LEVEL_BUFFER		BIT(7)
#define STI_PQ_LOG_LEVEL_SVP		BIT(8)
#define STI_PQ_LOG_LEVEL_IRQ		BIT(9)
#define STI_PQ_LOG_LEVEL_PATTERN	BIT(10)
#define STI_PQ_LOG_LEVEL_DBG		BIT(11)
#define STI_PQ_LOG_LEVEL_ALL		(BIT(12) - 1)

#define IS_INPUT_VGA(x)		(x == MTK_PQ_INPUTSRC_VGA)
#define IS_INPUT_ATV(x)		(x == MTK_PQ_INPUTSRC_TV)
#define IS_INPUT_CVBS(x)	((x >= MTK_PQ_INPUTSRC_CVBS) && (x < MTK_PQ_INPUTSRC_CVBS_MAX))
#define IS_INPUT_SVIDEO(x)	((x >= MTK_PQ_INPUTSRC_SVIDEO) && (x < MTK_PQ_INPUTSRC_SVIDEO_MAX))
#define IS_INPUT_YPBPR(x)	((x >= MTK_PQ_INPUTSRC_YPBPR) && (x < MTK_PQ_INPUTSRC_YPBPR_MAX))
#define IS_INPUT_SCART(x)	((x >= MTK_PQ_INPUTSRC_SCART) && (x < MTK_PQ_INPUTSRC_SCART_MAX))
#define IS_INPUT_HDMI(x)	((x >= MTK_PQ_INPUTSRC_HDMI) && (x < MTK_PQ_INPUTSRC_HDMI_MAX))
#define IS_INPUT_DVI(x)		((x >= MTK_PQ_INPUTSRC_DVI) && (x < MTK_PQ_INPUTSRC_DVI_MAX))
#define IS_INPUT_DTV(x)		((x >= MTK_PQ_INPUTSRC_DTV) && (x < MTK_PQ_INPUTSRC_DTV_MAX))
#define IS_INPUT_MM(x)	((x >= MTK_PQ_INPUTSRC_STORAGE) && (x < MTK_PQ_INPUTSRC_STORAGE_MAX))

#define IS_INPUT_B2R(x)		(IS_INPUT_DTV(x) || IS_INPUT_MM(x))
#define IS_INPUT_SRCCAP(x)	(IS_INPUT_VGA(x) || IS_INPUT_CVBS(x) || IS_INPUT_SVIDEO(x) || \
				IS_INPUT_YPBPR(x) || IS_INPUT_SCART(x) || IS_INPUT_HDMI(x) || \
				IS_INPUT_DVI(x) || IS_INPUT_ATV(x))

#define IPAUTH_VIDEO_HDR10PLUS      (43)
#define IPAUTH_VIDEO_CUVAHDR        (85)
#define IPAUTH_VIDEO_FMM            (87)
#define IPAUTH_VIDEO_SLHDR          (123)
#define MTK_PQ_CFD_HASH_HDR10PLUS   (0)
#define MTK_PQ_CFD_HASH_CUVA        (1)
#define MTK_PQ_CFD_HASH_TCH         (2)
#define MTK_PQ_HASH_FMM             (0)
#define MTK_PQ_MAX_RWDIFF			(10)

extern __u32 u32DbgLevel;
extern __u32 atrace_enable_pq;
extern __u32 u32PQMapBufLen;

#define STI_PQ_LOG(_dbgLevel_, _fmt, _args...) { \
	if ((_dbgLevel_ & u32DbgLevel) != 0) { \
		if (_dbgLevel_ & STI_PQ_LOG_LEVEL_ERROR) \
			pr_info("[STI PQ ERROR]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args); \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_COMMON) { \
			pr_info("[STI PQ COMMON]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args); \
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_ENHANCE) { \
			pr_info("[STI PQ ENHANCE]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args); \
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_B2R) { \
			pr_info("[STI PQ B2R]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args); \
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_DISPLAY) { \
			pr_info("[STI PQ DISPLAY]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_IDR) { \
			pr_info("[STI PQ IDR]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_MDW) { \
			pr_info("[STI PQ MDW]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_BUFFER) { \
			pr_info("[STI PQ BUFFER]  [%s,%5d]"_fmt, \
			__func__, __LINE__, ##_args); \
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_SVP) {\
			pr_info("[STI PQ SVP]  [%s,%5d]"_fmt,\
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_IRQ) {\
			pr_info("[STI PQ IRQ]  [%s,%5d]"_fmt,\
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_PATTERN) {\
			pr_info("[STI PQ PATTERN]  [%s,%5d]"_fmt,\
			__func__, __LINE__, ##_args);\
		} \
		else if (_dbgLevel_ & STI_PQ_LOG_LEVEL_DBG) {\
			pr_info("[STI PQ DBG]  [%s,%5d]"_fmt,\
			__func__, __LINE__, ##_args);\
		} \
	} \
}

#define STI_PQ_CD_LOG_RUN(_cd) \
do { \
	if (_cd > 0) \
		_cd--; \
} while (0)

#define STI_PQ_CD_LOG_RESET(_cd, _init_cd) \
do { \
	if (_cd == 0) \
		_cd = _init_cd; \
} while (0)

#define STI_PQ_CD_LOG(_cd, _dbgLevel_, _fmt, _args...) \
do { \
	if (_cd == 0) \
		STI_PQ_LOG(_dbgLevel_, _fmt, ##_args); \
} while (0)

#define PQ_LABEL    "[PQ] "
#define PQ_MSG_DEBUG(format, args...) \
	pr_debug(PQ_LABEL format, ##args)
#define PQ_MSG_INFO(format, args...) \
	pr_info(PQ_LABEL format, ##args)
#define PQ_MSG_WARNING(format, args...) \
	pr_warn(PQ_LABEL format, ##args)
#define PQ_MSG_ERROR(format, args...) \
	pr_err(PQ_LABEL format, ##args)
#define PQ_MSG_FATAL(format, args...) \
	pr_crit(PQ_LABEL format, ##args)
#define PQ_CAPABILITY_SHOW(PQItem, PQItemCap) { \
	if (ssize > u8Size) \
		PQ_MSG_ERROR("Failed to get" PQItem "\n"); \
	else \
		ssize += snprintf(buf + ssize, u8Size - ssize, "%d\n", \
		PQItemCap); \
}
#define PQ_CAPABILITY_CHECK_RET(ret, PQItemCap)          \
	do { \
		if (ret) { \
			PQ_MSG_ERROR("Failed to get" PQItemCap "resource\r\n"); \
		} \
	} while (0)

#define PQ_DIVIDE_BASE_2	(2)
#define PQ_X_ALIGN		(2)
#define PQ_Y_ALIGN		(2)
#define PQ_ALIGN(val, align) ((((val) + (align - 1)) / align) * align)

#define MAX_SYSFS_SIZE (255)
#define MAX_PLANE_NUM  (4)
#define MAX_FPLANE_NUM (MAX_PLANE_NUM - 1)

extern int boottime_print(const char *s);
/*
 * enum mtk_ctx_state - The state of an MTK pq instance.
 * @MTK_STATE_FREE - default state when instance is created
 * @MTK_STATE_INIT - job instance is initialized
 * @MTK_STATE_ABORT - job should be aborted
 */
enum mtk_ctx_state {
	MTK_STATE_FREE = 0,
	MTK_STATE_INIT = 1,
	MTK_STATE_ABORT = 2,
};

struct mtk_pq_ctx {
	struct v4l2_fh fh;
	struct mtk_pq_device *pq_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	enum mtk_ctx_state state;
};

struct mtk_pq_m2m_dev {
	struct video_device *video_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct mtk_pq_ctx *ctx;
	int refcnt;
};

struct mtk_pq_clk_rate_table {
	u32 ED_CLK_RATE;
	u32 ED_CLK_RATE_S;
	u32 ED_CLK_RATE_E;
	u32 ED_CLK_RATE_SLD;
	u32 ED_CLK_RATE_SLD_S;
	u32 ED_CLK_RATE_SLD_E;
	u32 FN_CLK_RATE;
	u32 FN_CLK_RATE_S;
	u32 FN_CLK_RATE_E;
	u32 FN_CLK_RATE_SLD;
	u32 FN_CLK_RATE_SLD_S;
	u32 FN_CLK_RATE_SLD_E;
	u32 FS_CLK_RATE;
	u32 FS_CLK_RATE_S;
	u32 FS_CLK_RATE_E;
	u32 FS_CLK_RATE_SLD;
	u32 FS_CLK_RATE_SLD_S;
	u32 FS_CLK_RATE_SLD_E;
};

struct mtk_pq_caps {
	struct mtk_pq_clk_rate_table clkRate;
	u32 u32XC_ED_MAX_CLK;
	u32 u32XC_FN_MAX_CLK;
	u32 u32XC_FS_MAX_CLK;
	u32 u32XC_ED_TARGET_CLK;
	u32 u32XC_FN_TARGET_CLK;
	u32 u32XC_FS_TARGET_CLK;
	u32 u32Window_Num;
	u32 u32SRS_Support;
	u32 u32Device_register_Num[MAX_WIN_NUM];
	u32 u32AISR_Support;
	u32 u32Memc;
	u32 u32Phase_IP2;
	u32 u32AISR_Version;
	u32 u32HSY_Support;
	u32 u32B2R_Version;
	u32 u32IRQ_Version;
	u32 u32CFD_PQ_Version;
	u32 u32MDW_Version;
	u32 u32Config_Version;
	u32 u32HAL_Version;
	u32 u32Idr_SwMode_Support;
	u32 u32DV_PQ_Version;
	u32 u32HSY_Version;
	u32 u32Qmap_Version;
	u32 u32IP2_Crop_Version;
	u32 u32ScreenSeamless_Support;
	u32 u32Support_TS;
	u32 u32P_ENGINE_IP2;
	u32 u32P_ENGINE_DI;
	u32 u32P_ENGINE_AISR;
	u32 u32P_ENGINE_SCL2;
	u32 u32P_ENGINE_SRS;
	u32 u32P_ENGINE_VIP;
	u32 u32P_ENGINE_MDW;
	u8 u8PQ_InQueue_Min;
	u8 u8PQ_OutQueue_Min;
	u32 u32YLite_Version;
	u32 u32VIP_Version;
	u32 u32input_throughput_single_win;
	u32 u32input_throughput_multi_win;
	u32 u32output_throughput_single_win;
	u32 u32output_throughput_multi_win;
	u32 u32output_throughput_win_num;
	u32 u32bw_vsd_coe;
	u32 u32bw_hdmi_w_coe;
	u32 u32bw_hdmi_sd_w_coe;
	u32 u32bw_mdla_coe;
	u32 u32p_inprocess_time;
	u32 u32i_inprocess_time;
	u32 u32outprocess_time;
	u32 u32bw_b2r;
	u32 u32bw_hdmi_r;
	u32 u32bw_dvpr;
	u32 u32bw_scmi;
	u32 u32bw_znr;
	u32 u32bw_mdw;
	u32 u32bw_b2r_effi;
	u32 u32bw_hdmi_r_effi;
	u32 u32bw_dvpr_effi;
	u32 u32bw_scmi_effi;
	u32 u32bw_znr_effi;
	u32 u32bw_mdw_effi;
	u32 u32bw_memc_ip_effi;
	u32 u32bw_aipq_effi;
	u32 u32i_tsin_phase;
	u32 u32i_tsin_clk_rate;
	u32 u32p_tsin_phase;
	u32 u32p_tsin_clk_rate;
	u32 u32tsout_phase;
	u32 u32tsout_clk_rate;
	u32 u32h_blk_reserve_4k;
	u32 u32h_blk_reserve_8k;
	u32 u32frame_v_active_time_60;
	u32 u32frame_v_active_time_120;
	u32 u32chipcap_tsin_outthroughput;
	u32 u32chipcap_mdw_outthroughput;
	u32 u32p_mode_pqlevel_scmi0;
	u32 u32p_mode_pqlevel_scmi1;
	u32 u32p_mode_pqlevel_scmi2;
	u32 u32p_mode_pqlevel_scmi3;
	u32 u32p_mode_pqlevel_scmi4;
	u32 u32p_mode_pqlevel_scmi5;
	u32 u32p_mode_pqlevel_znr0;
	u32 u32p_mode_pqlevel_znr1;
	u32 u32p_mode_pqlevel_znr2;
	u32 u32p_mode_pqlevel_znr3;
	u32 u32p_mode_pqlevel_znr4;
	u32 u32p_mode_pqlevel_znr5;
	u32 u32i_mode_pqlevel_scmi0;
	u32 u32i_mode_pqlevel_scmi1;
	u32 u32i_mode_pqlevel_scmi2;
	u32 u32i_mode_pqlevel_scmi3;
	u32 u32i_mode_pqlevel_scmi4;
	u32 u32i_mode_pqlevel_scmi5;
	u32 u32i_mode_pqlevel_znr0;
	u32 u32i_mode_pqlevel_znr1;
	u32 u32i_mode_pqlevel_znr2;
	u32 u32i_mode_pqlevel_znr3;
	u32 u32i_mode_pqlevel_znr4;
	u32 u32i_mode_pqlevel_znr5;
	u32 u32reserve_ml_table0;
	u32 u32reserve_ml_table1;
	u32 u32reserve_ml_table2;
	u32 u32reserve_ml_table3;
	u32 u32reserve_ml_table4;
	u32 u32reserve_ml_table5;
	u32 u32reserve_ml_table6;
	u32 u32reserve_ml_table7;
	u32 u32reserve_ml_table8;
	u32 u32reserve_ml_table9;
	u32 u32reserve_ml_table10;
	u32 u32reserve_ml_table11;
	u32 u32reserve_ml_table12;
	u32 u32reserve_ml_table13;
	u32 u32reserve_ml_table14;
	u32 u32reserve_ml_table15;
	u32 u32bw_vsd_effi;
	u32 u32bw_hdmi_w_effi;
	u32 u32bw_hdmi_sd_w_effi;
	u32 u32bwofts_height;
	u32 u32bwofts_width;
	u32 u32memc_ip_single_hdmi;
	u32 u32memc_ip_single_mm;
	u32 u32memc_ip_multi;
	u32 u32memc_ip_repeat;
	u32 u32memc_ip_pcmode;
	u32 u32memc_ip_memclevel_single_hdmi;
	u32 u32memc_ip_memclevel_single_mm;
	u32 u32memc_ip_memclevel_multi;
	u32 u32memc_ip_colorformat_single_hdmi;
	u32 u32memc_ip_colorformat_single_mm;
	u32 u32memc_ip_colorformat_multi;
	u32 u32forcep_pqlevel_h;
	u32 u32forcep_pqlevel_l;
	u32 u32ic_code;
	u8 u8HFR_Check;
	u32 u32Dipmap_Version;
	u8 u8CFD_Efuse_Support;
	u8 u8CFD_Hash_Support;
	u8 u8FMM_Hash_Support;
	u8 u8Scmi_Write_Limit_Support;
	u8 u8Ucm_Write_Limit_Support;
	u32 u32Fully_PQ_Cnt;

	/* efuse for i_view */
	u8 u8support_i_view;
	u8 u8support_i_view_pip;

	/* dramc */
	u32 u32dramc_freq_1;
	u32 u32dramc_freq_2;
	u32 u32dramc_size;
	u32 u32dramc_type;
	u8 u8dramc_pkg;

	u32 u32ZNR_Support;
	u32 u32Qmap_Heap_Support;

	/* op path throughput */
	u32 u32op_path_throughput;
};

/* refer to struct st_dv_config_hw_info */
struct mtk_pq_dv_config_hw_info {
	bool updatedFlag;
	__u32 hw_ver;  //ts domain dv hw version (include FE & BE)
};

struct mtk_pq_dma_buf {
	int fd;
	bool valid;
	void *va;
	__u64 pa;
	__u64 iova;
	__u64 size;
	struct dma_buf *db;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct device *dev;
	void *priv;
};

struct mtk_pq_remap_buf {
	__u64 pa;
	__u64 va;
	__u32 size;
};

struct v4l2_pix_format_info {
	struct v4l2_pix_format_mplane v4l2_fmt_pix_idr_mp;
	struct v4l2_pix_format_mplane v4l2_fmt_pix_mdw_mp;
};

struct mtk_pq_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head	list;

	struct mtk_pq_dma_buf meta_buf;
	struct mtk_pq_dma_buf frame_buf;

	__u64 queue_time;
	__u64 dequeue_time;
	__u64 process_time;

	bool slice_mode;
	bool slice_qbuf;
	bool slice_need_output;
};

struct mtk_pq_stream_info {
	bool streaming;
	enum v4l2_buf_type type;
	struct mtk_pq_dma_buf meta;
};

struct mtk_pq_ucd_device {
	struct mtk_pq_remap_buf ucd_shm;
	__u16 u16ucd_alg_engine;
};

struct pq_debug_b2r_info {
	uint64_t luma_fb_offset;
	uint64_t chroma_fb_offset;
	uint64_t luma_blen_offset;
	uint64_t chroma_blen_offset;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	uint16_t SrcWidth;
	uint16_t SrcHeight;
	uint16_t bitdepth;
	uint16_t pitch;
	enum pqu_order_type order_type;
	enum pqu_scan_type scan_type;
	enum pqu_field_type field_type;
	enum pqu_rotate_type rotate_type;
	struct pqu_fmt_modifier modifier;
	uint8_t aid;
	uint8_t color_fmt_422;
	enum pqu_rgb_type rgb_type;
};

struct pq_debug_window {
	__u16 x;
	__u16 y;
	__u16 width;
	__u16 height;
	__u16 w_align;
	__u16 h_align;
};

struct pq_debug_input_queue_ext_info {
	__u64 u64Pts;
	__u64 u64UniIdx;
	__u64 u64ExtUniIdx;
	__u64 u64TimeStamp;
	__u64 u64RenderTimeNs;
	__u8 u8BufferValid;
	__u32 u32BufferSlot;
	__u32 u32GenerationIndex;
	__u32 u32StreamUniqueId;
	__u8 u8RepeatStatus;
	__u8 u8FrcMode;
	__u8 u8Interlace;
	__u32 u32InputFps;
	__u32 u32OriginalInputFps;
	bool bEOS;
	__u8 u8MuteAction;
	__u8 u8PqMuteAction;
	__u8 u8SignalStable;
	__u8 u8DotByDotType;
	__u32 u32RefreshRate;
	__u8 u8FastForwardFlag;
	bool bReportFrameStamp;
	bool bBypassAvsync;
	__u32 u32HdrApplyType;
	__u32 u32QueueInputIndex;
	bool bMuteChange;
	bool bFdMaskBypass;
	bool bTriggerInfiniteGen;
	bool bEnableBbd;
	bool bPerFrameMode;
};

struct pq_debug_output_queue_ext_info {
	int32_t int32FrameId;
	__u64 u64Pts;
	__u64 u64UniIdx;
	__u64 u64ExtUniIdx;
	__u64 u64TimeStamp;
	__u64 u64RenderTimeNs;
	__u8 u8BufferValid;
	__u32 u32BufferSlot;
	__u32 u32GenerationIndex;
	__u32 u32StreamUniqueId;
	__u8 u8RepeatStatus;
	__u8 u8FrcMode;
	__u8 u8Interlace;
	__u32 u32InputFps;
	__u32 u32OriginalInputFps;
	bool bEOS;
	__u8 u8MuteAction;
	__u8 u8PqMuteAction;
	__u8 u8SignalStable;
	__u8 u8DotByDotType;
	__u32 u32RefreshRate;
	bool bReportFrameStamp;
	bool bBypassAvsync;
	__u32 u32HdrApplyType;
	__u32 u32QueueInputIndex;
	__u32 u32QueueOutputIndex;
	__u8 u8FieldType;
	enum pqu_order_type order_type;
	bool bMuteChange;
	bool bFdMaskBypass;
};

struct pq_debug_display_idr_ctrl {
	bool sw_mode_supported;
	struct buffer_info fbuf[3];
	struct m_pq_display_rect crop;
	uint32_t mem_fmt;
	uint8_t yuv_mode;
	uint32_t width;
	uint32_t height;
	uint16_t index;		/* force read index */
	uint64_t vb;		// ptr of struct vb2_buffer, use in callback
	enum pqu_idr_input_path path;
	bool bypass_ipdma;
	bool v_flip;
	uint8_t aid;		// access id
	enum pqu_idr_buf_ctrl_mode buf_ctrl_mode;
	uint32_t frame_pitch[MAX_FPLANE_NUM];
	uint32_t line_offset;
	struct m_pq_display_idr_duplicate_out duplicate_out;
	enum pqu_idr_buf_sw_mode sw_buffer_mode;
};

enum pq_debug_win_type {
	pq_debug_win_type_main = 0,
	pq_debug_win_type_sub,
	pq_debug_win_type_max,
};

enum pq_debug_ip_window {
	/* control size */
	pq_debug_ip_capture = 0,
	pq_debug_ip_ip2_in,
	pq_debug_ip_ip2_out,
	pq_debug_ip_scmi_in,
	pq_debug_ip_scmi_out,
	pq_debug_ip_aisr_in,
	pq_debug_ip_aisr_out,
	pq_debug_ip_hvsp_in,
	pq_debug_ip_hvsp_out,
	pq_debug_ip_display,

	/* size fixed */
	pq_debug_ip_hdr,
	pq_debug_ip_ucm,
	pq_debug_ip_abf,
	pq_debug_ip_1st_vip,
	pq_debug_ip_vip,
	pq_debug_ip_film_ip,
	pq_debug_ip_film_op,
	pq_debug_ip_spf,
	pq_debug_ip_yhsl,
	pq_debug_ip_ylite,
	pq_debug_ip_freq_det,
	pq_debug_ip_3d_meter,
	pq_debug_ip_obj_hist,

	pq_debug_ip_max,
};

struct debug_aisr_window_info {
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
};

struct debug_srs_info {
	bool dbg_enable;
	bool srs_enable;
	uint8_t tgen_mode;
	uint8_t db_idx;
	uint8_t method;
	bool bin_mode;
	uint8_t bin_db_idx;
};

struct pq_debug_info {
	uint16_t src_dup_idx[IDR_DEBUG_MAX];
	uint16_t idr_dup_idx[IDR_DEBUG_MAX];
	uint8_t src_dup_field[IDR_DEBUG_MAX];
	uint8_t idr_dup_field[IDR_DEBUG_MAX];
	struct pq_debug_b2r_info main;
	struct pq_debug_b2r_info sub1;
	struct pq_debug_window content_in;
	struct pq_debug_window capture_in;
	struct pq_debug_window crop_in;
	struct pq_debug_window display_in;
	struct pq_debug_window displayArea_in;
	struct pq_debug_window displayRange_in;
	struct pq_debug_window displayBase_in;
	struct pq_debug_window outcrop_in;
	struct pq_debug_window outdisplay_in;
	struct pq_debug_window ip_win[pq_debug_ip_max];
	enum pq_debug_win_type pq_win_type;
	bool multiple_window;
	bool flowControlEn;
	uint16_t flowControlFactor;
	bool bAisrActiveWinEn;
	bool aisr_en;
	bool vip_path;
	uint8_t win_id_debug;
	unsigned int idr_idx;
	struct pq_debug_window multiple_window_in;
	struct debug_aisr_window_info aisr_active_win_info;

	struct pq_debug_window content_out;
	struct pq_debug_window capture_out;
	struct pq_debug_window crop_out;
	struct pq_debug_window idrcrop_out;
	struct pq_debug_window display_out;
	struct pq_debug_window displayArea_out;
	struct pq_debug_window displayRange_out;
	struct pq_debug_window displayBase_out;
	struct pq_debug_window outcrop_out;
	struct pq_debug_window outdisplay_out;
	struct pq_debug_input_queue_ext_info input_queue_ext_info;
	struct pq_debug_output_queue_ext_info output_dqueue_ext_info;
	struct pq_debug_display_idr_ctrl display_idr_ctrl;
	uint32_t queue_pts[DROP_ARY_IDX];
	uint32_t drop_frame_pts[DROP_ARY_IDX];
	__u8 pts_idx;
	__u8 drop_pts_idx;
	uint32_t total_que;
	uint32_t total_dque;
	uint64_t pqu_log_level;
	struct debug_srs_info srs_dbg;
	hwmap_swbit swbit;
	__u8 extra_frame;
};

/*INFO OF PQ DD*/
struct mtk_pq_device {
	spinlock_t slock;
	unsigned int ref_count;
	wait_queue_head_t wait;
	unsigned int event_flag;
	__u8 stream_on_ref;
	struct mutex mutex_stream_off;
	bool cap_turing_stream_off; // between stream off ~ stream off cb
	bool out_turing_stream_off;  // between stream off ~ stream off cb
	enum v4l2_pq_path path;
	bool bSetPath;
	bool bInvalid;
	__u64 memory_bus_base;
	bool trigger_pattern;
	struct mutex mutex;
	struct v4l2_device v4l2_dev;
	struct video_device video_dev;
	struct device *dev;
	struct mtk_pq_m2m_dev m2m;
	struct v4l2_subdev subdev_common;
	struct v4l2_subdev subdev_enhance;
	struct v4l2_subdev subdev_xc;
	struct v4l2_subdev subdev_b2r;
	struct v4l2_subdev subdev_display;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl_handler common_ctrl_handler;
	struct v4l2_ctrl_handler enhance_ctrl_handler;
	struct v4l2_ctrl_handler xc_ctrl_handler;
	struct v4l2_ctrl_handler b2r_ctrl_handler;
	struct v4l2_ctrl_handler display_ctrl_handler;

	__u16 dev_indx;    /*pq device ID or window number*/
	__u16 log_cd;
	__u32 hdr_type;

	struct v4l2_common_info common_info;
	void *enhance_info;	//pointer to struct v4l2_enhance_info
	struct v4l2_b2r_info b2r_info;
	struct v4l2_display_info display_info;
	struct v4l2_pix_format_info pix_format_info;

	struct b2r_device b2r_dev;
	struct pq_pattern_info pattern_info;
	struct pq_pattern_status pattern_status;
	struct mtk_pq_stream_info input_stream;
	struct mtk_pq_stream_info output_stream;
	struct mtk_pq_dv_win_info dv_win_info;

	struct mtk_pq_dma_buf stream_dma_info;
	struct mtk_pq_remap_buf pqu_stream_dma_info;
	struct meta_pq_stream_info stream_meta;
	__u64 qbuf_meta_pool_addr;
	__u32 qbuf_meta_pool_size;
	struct m_pq_display_flow_ctrl pqu_meta;
	struct m_pq_queue_ext_info pqu_queue_ext_info;
	struct mtk_pq_thermal thermal_info;
	struct meta_buffer meta_buf;
	// device attribute
	int attr_mdw_buf_num;
	struct mtk_pq_display_s_data s_data;
	struct m_pq_forcep_info pqu_forcep_info;
	int16_t debug_calc_num;
	__u8 extra_frame;
	bool output_meta_change[MTK_PQ_MAX_RWDIFF];
	bool need_output;

	//PQ_debug
	struct pq_debug_info pq_debug;
	// scmi opm rwdiff fixup
	bool scmi_opm_diff_fixup;
	__u16 scmi_protect_line;
	struct mutex mutex_set_path;
};

/*INFO FOR FRC*/
struct mtk_frc_device {
	struct pq_buffer_frc_pq frc_pq_buffer;
	struct device *dev;
};

/*INFO FOR CUS01 CUSTOMIZATION*/
struct cus01_pq_config_info {
	bool g9_mute;
};

struct mtk_pq_platform_device {
	struct mtk_pq_device *pq_devs[PQ_WIN_MAX_NUM];
	struct device *dev;
	bool cus_dev;
	__u8 cus_id[PQ_WIN_MAX_NUM];

	struct b2r_device b2r_dev;
	struct mtk_pq_caps pqcaps;
	struct mtk_pq_enhance pq_enhance;
	struct display_device display_dev;
	struct mtk_pq_dv_ctrl dv_ctrl;
	__u8 pq_irq[PQ_IRQ_MAX_NUM];
	uint8_t usable_win;
	uint8_t pq_memory_index;
	struct mtk_pq_remap_buf hwmap_config;
	struct mtk_pq_remap_buf dv_config;
	struct mtk_pq_remap_buf DDBuf[MMAP_DDBUF_MAX];
	struct msg_config_info config_info;
	struct mtk_pq_ucd_device ucd_dev;
	struct mtk_frc_device frc_dev;
	struct cus01_pq_config_info pq_config_info;
	uint8_t dip_win_num;
	uint8_t xc_win_num;

	spinlock_t spinlock_thermal_handling_task;
	struct task_struct *thermal_handling_task;
	bool thermal_task_available;
	uint8_t pq_thermal_ctrl_en;
};

extern struct mtk_pq_platform_device *pqdev;

#endif
