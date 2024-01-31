/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author Apple Chen <applexx.chen@mediatek.com>
 */

#ifndef _MTK_PQ_B2R_H
#define _MTK_PQ_B2R_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include "hwreg_pq_display_b2r.h"

enum v4l2_b2r_flip {
	B2R_HFLIP,
	B2R_VFLIP,
};

struct v4l2_b2r_stat {
	__u8 hflip;
	__u8 vflip;

	__u8 forcep;
};

struct b2r_reg_timing {
	__u16 V_TotalCount;
	__u32 H_TotalCount;
	__u16 VBlank0_Start;
	__u16 VBlank0_End;
	__u16 VBlank1_Start;
	__u16 VBlank1_End;
	__u16 TopField_Start;
	__u16 BottomField_Start;
	__u16 TopField_VS;
	__u16 BottomField_VS;
	__u16 HActive_Start;
	__u16 HImg_Start;
	__u16 VImg_Start0;
	__u16 VImg_Start1;
	__u32 Hcnt_FineTune;
	__u32 vfreq;
};

struct b2r_reg_addr {
	__u32 luma_fb;
	__u32 chroma_fb;
	__u32 luma_bln_fb;
	__u32 chroma_bln_fb;
};

struct b2r_ctrl {
	struct b2r_reg_addr main;
	struct b2r_reg_addr sub1;
};

struct b2r_enable {
	bool main;
	bool sub1;
};

struct b2r_glb_timing {
	__u8 win_id;
	bool enable;
};

struct b2r_input_timing {
	__u32 de_width;
	__u32 de_height;
	__u32 vfreq;
};

//INFO OF B2R SUB DEV
struct v4l2_b2r_info {
	struct v4l2_pix_format_mplane pix_fmt_in;
	struct v4l2_timing timing_in;
	struct v4l2_b2r_stat b2r_stat;
	__u32 delaytime_fps;
	enum b2r_pattern b2r_pat;
	__u64 current_clk;
	struct b2r_timing_info timing_out;
	struct b2r_glb_timing global_timing;
	uint8_t plane_num;
	uint8_t buffer_num;
	u32 input_format;
};

typedef volatile u32 REG16;

enum v4l2_b2r_clock {
	B2R_CLK_SYNC,
	B2R_CLK_FREERUN,
	B2R_CLK_720MHZ            = 720000000ul,
	B2R_CLK_630MHZ            = 630000000ul,
	B2R_CLK_360MHZ            = 360000000ul,
	B2R_CLK_312MHZ            = 312000000ul,
	B2R_CLK_156MHZ            = 156000000ul,
};

struct b2r_device {
	u8 id;
	u32 irq;
	u8 pix_num;
	u64 win_count;
	u32 h_max_size;
	u32 v_max_size;
	u32 h_422_max_size;
	u32 rotate_support_h;
	u32 rotate_support_v;
	u8 rotate_interlace;
	u8 rotate_support_vsd;
	struct clk *b2r_swen_dc02scip;
	struct clk *b2r_swen_sub1_dc02scip;
	struct clk *b2r_swen_sram2scip;
	u32 b2r_ver;
	struct mutex b2r_mutex;
	struct b2r_input_timing cur_timing;
	u32 get_max_fps_w;
	u32 get_max_fps_h;
	u32 rotate_vsd_support_h;
	u32 rotate_vsd_support_v;
	u32 imode_vsd_support_h;
	u32 imode_vsd_support_v;
	spinlock_t b2r_slock;
};

#define B2R_RES_MAX 1
#define B2R_WIN_NONE 0xFF

struct resources_name {
	char *regulator[B2R_RES_MAX];
	char *clock[B2R_RES_MAX];
	char *reg[B2R_RES_MAX];
	char *interrupt[B2R_RES_MAX];
};

enum b2r_clock_idx {
	B2R_CLK_720M_IDX       = 0x0,
	B2R_CLK_FRUN_IDX       = 0x1,
	B2R_CLK_156M_IDX       = 0x2,
	B2R_CLK_360M_IDX       = 0x3,
	B2R_CLK_312M_IDX       = 0x4,
	B2R_CLK_VBUF_IDX       = 0x5,
	B2R_CLK_SYNC_IDX       = 0x6,
	B2R_CLK_630M_IDX       = 0x7,
};

enum b2r_dev_ver {
	B2R_VER0       = 0x0,
	B2R_VER1       = 0x1,
	B2R_VER2       = 0x2,
	B2R_VER3       = 0x3,
	B2R_VER4       = 0x4,
	B2R_VER5       = 0x5,
	B2R_VER6       = 0x6,
};

enum b2r_sht_num {
	B2R_1P_SHT       = 0x0,
	B2R_2P_SHT       = 0x1,
	B2R_4P_SHT       = 0x2,
};

struct mtk_pq_device;

/* function*/
int mtk_pq_b2r_init(struct mtk_pq_device *pq_dev);
int mtk_pq_b2r_exit(struct mtk_pq_device *pq_dev);
int mtk_pq_b2r_set_pix_format_in_mplane(
		struct v4l2_format *format,
		struct v4l2_b2r_info *b2r_info);
int mtk_pq_b2r_set_flip(__u8 win, __s32 enable,
		struct v4l2_b2r_info *b2r_info, enum v4l2_b2r_flip flip_type);
int mtk_b2r_set_ext_timing(struct device *dev, __u8 win, void *ctrl,
		struct v4l2_b2r_info *b2r_info);
int mtk_b2r_get_ext_timing(__u8 win, void *ctrl,
		struct v4l2_b2r_info *b2r_info);
int mtk_pq_b2r_set_forcep(__u8 win, bool enable,
		struct v4l2_b2r_info *b2r_info);
int mtk_pq_b2r_set_delaytime(__u8 win, __u32 fps,
		struct v4l2_b2r_info *b2r_info);
int mtk_pq_b2r_get_delaytime(__u8 win, __u16 *b2r_delay,
		struct v4l2_b2r_info *b2r_info);
int mtk_pq_b2r_subdev_init(struct device *dev,
		struct b2r_device *b2r);
int mtk_pq_b2r_set_pattern(__u8 win, enum v4l2_ext_pq_pattern pat_type,
	struct v4l2_b2r_pattern *b2r_pat_para, struct v4l2_b2r_info *b2r_info);
int mtk_b2r_parse_dts(struct b2r_device *b2r_dev,
	struct platform_device *pdev);
int mtk_pq_register_b2r_subdev(struct v4l2_device *pq_dev,
		struct v4l2_subdev *subdev_b2r,
		struct v4l2_ctrl_handler *b2r_ctrl_handler);
void mtk_pq_unregister_b2r_subdev(struct v4l2_subdev *subdev_b2r);
int mtk_b2r_set_ext_global_timing(__u8 win, bool enable,
	struct v4l2_b2r_info *b2r_info);
int mtk_pq_display_b2r_queue_setup(struct vb2_queue *vq,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned int sizes[], struct device *alloc_devs[]);
int mtk_pq_display_b2r_queue_setup_info(struct mtk_pq_device *pq_dev,
					void *bufferctrl);
int mtk_pq_display_b2r_set_pix_format_mplane_info(struct mtk_pq_device *pq_dev,
						  void *bufferctrl);
int mtk_b2r_get_ext_cap(__u8 win, void *ctrl, struct b2r_device *b2r_dev);
int mtk_b2r_get_hw_gtgen(__u8 win, struct b2r_reg_timing *reg_timing_info);
int mtk_b2r_get_hw_addr(__u8 win, struct b2r_ctrl *reg_b2r_ctrl);
void mtk_b2r_set_reg_ctrl(struct b2r_ctrl *reg_b2r_ctrl);
struct b2r_enable mtk_b2r_get_enable(struct b2r_ctrl *reg_b2r_ctrl);

#endif
