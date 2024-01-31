/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_COMMON_H
#define MTK_SRCCAP_COMMON_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/mtk-v4l2-srccap.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#include "mtk_srccap_common_irq.h"
#include "linux/metadata_utility/m_srccap.h"


/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
#define SRCCAP_COMMON_FHD_W (1280)
#define SRCCAP_COMMON_FHD_H (720)
#define SRCCAP_COMMON_SW_IRQ_DLY (20)
#define SRCCAP_COMMON_PQ_IRQ_DLY (20)

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */
#define MALLOC(size)	((size >= PAGE_SIZE) ? vmalloc(size) : kmalloc(size, GFP_KERNEL))
#define FREE_MEM(ptr, size)			\
	do {						\
		if (size >= PAGE_SIZE)	\
			vfree(ptr);			\
		else					\
			kfree(ptr);			\
	} while (0)

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
enum srccap_irq_interrupts {
	SRCCAP_HDMI_IRQ_START,
	SRCCAP_HDMI_IRQ_PHY = SRCCAP_HDMI_IRQ_START,
	SRCCAP_HDMI_IRQ_MAC,
	SRCCAP_HDMI_IRQ_PKT_QUE,
	SRCCAP_HDMI_IRQ_PM_SQH_ALL_WK,
	SRCCAP_HDMI_IRQ_PM_SCDC,
	SRCCAP_HDMI_FIQ_CLK_DET_0,
	SRCCAP_HDMI_FIQ_CLK_DET_1,
	SRCCAP_HDMI_FIQ_CLK_DET_2,
	SRCCAP_HDMI_FIQ_CLK_DET_3,
	SRCCAP_HDMI_IRQ_END,
	SRCCAP_XC_IRQ_START = SRCCAP_HDMI_IRQ_END,
	SRCCAP_XC_IRQ = SRCCAP_XC_IRQ_START,
	SRCCAP_XC_IRQ_END,
	SRCCAP_VBI_IRQ_START = SRCCAP_XC_IRQ_END,
	SRCCAP_VBI_IRQ = SRCCAP_VBI_IRQ_START,
	SRCCAP_VBI_IRQ_END,
};

enum srccap_common_device_num {
	SRCCAP_COMMON_DEVICE_0 = 0,
	SRCCAP_COMMON_DEVICE_1,
};

enum E_SRCCAP_FILE_TYPE {
	E_SRCCAP_FILE_PIM_JSON,
	E_SRCCAP_FILE_RM_JSON,
	E_SRCCAP_FILE_RULELIST_JSON,
	E_SRCCAP_FILE_HWREG_JSON,
	E_SRCCAP_FILE_SWREG_JSON,
	E_SRCCAP_FILE_MAX,
};

enum E_SRCCAP_DITHER_OUT_DATA_MODE {
	E_SRCCAP_DITHER_OUT_DATA_MODE_DROP,
	E_SRCCAP_DITHER_OUT_DATA_MODE_ROUNDING,
	E_SRCCAP_DITHER_OUT_DATA_MODE_DITHER,
	E_SRCCAP_DITHER_OUT_DATA_MODE_MAX,
};

enum srccap_common_event {
	E_SRCCAP_COMMON_EVENT_DV = BIT(0), //bit0: DV event
	E_SRCCAP_COMMON_EVENT_FMM = BIT(1), //bit1: FMM event
	E_SRCCAP_COMMON_EVENT_VRR = BIT(2), //bit2: VRR event
	E_SRCCAP_COMMON_EVENT_FORMAT_CHANGE = BIT(3), //bit3: CFD event
	E_SRCCAP_COMMON_EVENT_HDR_CHANGE = BIT(4), //bit4: CFD event

	E_SRCCAP_COMMON_EVENT_HDMI = BIT(7), //bit7: HDMI event

	// Reserved bit 8 ~ 10 for HDMI
	E_SRCCAP_COMMON_EVENT_END,
	E_SRCCAP_COMMON_EVENT_MAX = ((E_SRCCAP_COMMON_EVENT_END - 1) << 1),
};
/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct srccap_ml_script_info;
struct hdmi_avi_packet_info;
struct meta_srccap_color_info;
struct meta_srccap_hdr_pkt;
struct hdmi_vsif_packet_info;
struct hdmi_emp_packet_info;
struct hdmi_gc_packet_info;
struct meta_srccap_hdr_vsif_pkt;

struct srccap_mmap_info {
	void *addr;
	uint32_t size;
	uint64_t bus_addr;
	uint64_t phy_addr;
};

struct srccap_common_pqmap_info {
	bool	inited;
	void	*pqmap_va;
	char	*pim_va;
	uint32_t pim_len;
	uint32_t hwreg_func_table_size;
	uint64_t va_size;
	void	*pPim;
	void	*alg_ctx;
	void	*hwreg_func_table_addr;
	void	*sw_reg;
	void	*meta_data;
	void	*hw_report;
	struct  srccap_mmap_info mmap_info;
};

struct mtk_srccap_dev;

struct srccap_main_color_info {
	u8 data_format;
	u8 data_range;
	u8 bit_depth;
	u8 color_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;
};

struct srccap_color_info {
	struct srccap_main_color_info in;
	struct srccap_main_color_info out;
	struct srccap_main_color_info dip_point;
	u32 chroma_sampling_mode;
	u8 hdr_type;
	u8 force_update;
	bool swreg_update;
	u8 hdr10p_sstm;
	u8 hdr10p_allm;
	u32 event_changed;
};

struct srccap_common_info {
	struct srccap_color_info color_info;
	struct srccap_common_irq_info irq_info;
	u8 srccap_handled_top_irq[SRCCAP_COMMON_IRQEVENT_MAX][SRCCAP_COMMON_IRQTYPE_MAX];
	struct v4l2_srccap_pqmap_rm_info rm_info;
	struct srccap_common_pqmap_info pqmap_info;
	struct srccap_common_pqmap_info vdpqmap_info;
	bool event_toggle;
	u32 event_changed;
	bool dqbuf_event_first_frame;
	enum m_input_source input_port;
};

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
bool mtk_common_check_color_format_change(struct mtk_srccap_dev *srccap_dev,
	enum m_hdmi_color_format_change_ID event_ID);
int mtk_common_updata_color_info(struct mtk_srccap_dev *srccap_dev);
int mtk_common_updata_frame_color_info(struct mtk_srccap_dev *srccap_dev);
int mtk_common_get_cfd_metadata(struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_color_info *cfd_meta);
int mtk_common_set_cfd_setting(struct mtk_srccap_dev *srccap_dev);
int mtk_common_parse_vsif_pkt(struct hdmi_vsif_packet_info *pvsif_data,
	u8 u8DataSize, struct meta_srccap_hdr_vsif_pkt *hdr_vsif_pkt);
int mtk_common_subscribe_event_vsync(struct mtk_srccap_dev *srccap_dev);
int mtk_common_unsubscribe_event_vsync(struct mtk_srccap_dev *srccap_dev);
int mtk_common_init_triggergen(struct mtk_srccap_dev *srccap_dev);
int mtk_common_report_info(struct mtk_srccap_dev *srccap_dev, char *buf, const u16 max_size);
int mtk_srccap_register_common_subdev(
	struct v4l2_device *srccap_dev,
	struct v4l2_subdev *subdev_common,
	struct v4l2_ctrl_handler *common_ctrl_handler);
void mtk_srccap_unregister_common_subdev(
	struct v4l2_subdev *subdev_common);
int mtk_srccap_common_set_pqmap_info(struct v4l2_srccap_pqmap_info *info,
	struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_get_pqmap_info(struct v4l2_srccap_pqmap_info *info,
	struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_load_pqmap(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_ml_script_info *ml_script_info);
int mtk_srccap_common_load_vd_pqmap(
	struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_deinit_pqmap(
	struct mtk_srccap_dev *srccap_dev);
void mtk_srccap_common_set_hdmi_420to444
	(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_set_hdmi422pack
	(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_streamon(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_streamoff(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_pq_hwreg_init(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_pq_hwreg_deinit(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_vd_pq_hwreg_init(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_common_vd_pq_hwreg_deinit(struct mtk_srccap_dev *srccap_dev);
void mtk_srccap_common_set_stub_mode(u8 stub);
bool mtk_srccap_common_is_Cfd_stub_mode(void);
int mtk_srccap_common_get_cfd_test_HDMI_pkt_HDRIF(struct meta_srccap_hdr_pkt *phdr_pkt);
int mtk_srccap_common_get_cfd_test_HDMI_pkt_AVI(struct hdmi_avi_packet_info *pavi_pkt);
int mtk_srccap_common_get_cfd_test_HDMI_pkt_VSIF(struct hdmi_vsif_packet_info *vsif_pkt);
int mtk_srccap_common_get_cfd_test_HDMI_pkt_EMP(struct hdmi_emp_packet_info *emp_pkt);
int mtk_srccap_common_get_cfd_test_HDMI_pkt_GCP(struct hdmi_gc_packet_info *gcp_pkt);
void mtk_srccap_common_clear_event(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_common_event event);
void mtk_srccap_common_set_event(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_common_event event);
void mtk_srccap_common_toggle_event(
	struct mtk_srccap_dev *srccap_dev);
enum m_input_source mtk_srccap_common_get_input_port(
	struct mtk_srccap_dev *srccap_dev);
#endif  // MTK_SRCCAP_COMMON_H
