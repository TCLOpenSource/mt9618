/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_DISPLAY_MDW_H
#define _MTK_PQ_DISPLAY_MDW_H

#include <linux/types.h>

#include "m_pqu_pq.h"

//-----------------------------------------------------------------------------
//  Driver Capability
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Macro and Define
//-----------------------------------------------------------------------------
#define MAX_PLANE_NUM		(4)
#define MAX_FPLANE_NUM		(MAX_PLANE_NUM - 1)	// exclude metadata plane
#define MDW_HW_QUEUE_FRAME_NUM	(3)
#define MDW_SW_QUEUE_FRAME_NUM	(3)
#define MDW_HW_FRAME_NUM	(3)
#define MDW_SW_FRAME_NUM	(1)

#define FRC_YUV420_BYTES_10PIX 9 // FRC bytes/10pixel 1.5x10x0.6/8


//-----------------------------------------------------------------------------
//  Enum
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Structure
//-----------------------------------------------------------------------------
struct pq_display_mdw_info {
	uint8_t plane_num;
	uint8_t buffer_num;
	enum mtk_pq_output_mode output_mode;
	uint32_t mem_fmt;
	uint16_t width;
	uint16_t height;
	uint16_t width_align_pitch[MAX_FPLANE_NUM];
	bool vflip;
	uint16_t line_offset;	// in pixel
	uint32_t plane_size[MAX_PLANE_NUM];	// in byte
	uint32_t frame_offset[MAX_FPLANE_NUM];		// in 256 bit
	uint16_t mdw_framecount;
	bool frc_efuse;
	bool frc_enable;
	uint64_t frc_meds_addr;
	uint32_t frc_ipm_offset;   // in byte
	uint32_t frc_meds_offset;  // in byte
	uint32_t frc_4kmi_offset;

	uint16_t frc_framecount;
	enum pqu_pq_output_path output_path;
	uint32_t frame_size;
};

struct pq_display_mdw_device {
	uint8_t v_align;
	uint8_t h_align;
};

#if (1)//(MEMC_CONFIG == 1)
/*from dts*/
struct pq_display_frc_device {
	uint32_t frc_width;
	uint32_t frc_height;
	uint32_t frc_meds_width;
	uint32_t frc_meds_height;
	uint16_t frc_h_align;
	uint8_t frc_meds_bpp;
	uint8_t frc_frame_cnt;
	uint8_t frc_ip_version;
	uint64_t frc_mvbff_adr;
	uint32_t frc_mvbff_size;
	uint16_t frc_4kmi_width;
	uint16_t frc_4kmi_height;
	uint8_t frc_max_frame_cnt;
	uint8_t frc_iommu_idx_frc_pq;
	uint8_t frc_buf_iommu_offset;
	uint64_t frc_iommu_frc_pq_adr;
	uint32_t frc_iommu_frc_pq_size;
	uint8_t frc_photo_enable;
	uint8_t frc_photo_frame_cnt;
};
#endif
//-----------------------------------------------------------------------------
//  Function and Variable
//-----------------------------------------------------------------------------
int mtk_pq_display_mdw_read_dts(
	struct device_node *display_node,
	struct pq_display_mdw_device *mdw_dev);
int mtk_pq_display_frc_read_dts(
	struct pq_display_frc_device *frc_dev);
int mtk_pq_display_mdw_queue_setup(struct vb2_queue *vq,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[]);
int mtk_pq_display_mdw_queue_setup_info(struct mtk_pq_device *pq_dev,
	void *bufferctrl);
int mtk_pq_display_mdw_set_vflip(struct pq_display_mdw_info *mdw_info, bool enable);
int mtk_pq_display_mdw_set_pix_format(
	struct v4l2_format *fmt,
	struct pq_display_mdw_info *mdw_info);
int mtk_pq_display_mdw_set_pix_format_mplane(
	struct mtk_pq_device *pq_dev,
	struct v4l2_format *fmt);
int mtk_pq_display_mdw_set_pix_format_mplane_info(
	struct mtk_pq_device *pq_dev,
	void *bufferctrl);
int mtk_pq_display_mdw_streamoff(struct pq_display_mdw_info *mdw_info);
int mtk_pq_display_mdw_qbuf(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *pq_buf);

#endif
