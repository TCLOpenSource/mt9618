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
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/version.h>

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <asm-generic/div64.h>

#include "linux/metadata_utility/m_srccap.h"
#include "mtk_srccap.h"
#include "mtk_srccap_common.h"
#include "hwreg_srccap_common_trigger_gen.h"
#include "hwreg_srccap_common.h"
#include "mapi_pq_if.h"
#include "mapi_pq_nts_reg_if.h"
#include "mapi_pq_cfd_if.h"
#include "mtk-reserved-memory.h"
#include "m_pqu_render_frc.h"

#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
#include "mtk_srccap_atrace.h"
#endif

#define BIT_DEPTH_6_BIT		(6)
#define BIT_DEPTH_8_BIT		(8)
#define BIT_DEPTH_10_BIT	(10)
#define BIT_DEPTH_12_BIT	(12)
#define BIT_DEPTH_16_BIT	(16)
#define SRCCAP_DTSI_STR  "srccap-swreg"
#define BUSADDR_TO_IPADDR_OFFSET 0x20000000

enum srccap_cfd_info_index {
	E_PQ_CFD_ANALOG_YUV_NTSC,   // 0
	E_PQ_CFD_ANALOG_YUV_PAL,    // 1
	E_PQ_CFD_ANALOG_YUV_SECAM,  // 2
	E_PQ_CFD_ANALOG_RGB_NTSC,   // 3
	E_PQ_CFD_ANALOG_RGB_PAL,    // 4
	E_PQ_CFD_YPBPR_SD,          // 5
	E_PQ_CFD_YPBPR_FHD,         // 6
	E_PQ_CFD_YPBPR_P2P,         // 7
	E_PQ_CFD_DVI,               // 8
	E_PQ_CFD_VGA,               // 9
	E_PQ_CFD_HDMI,              // 10
	E_PQ_CFD_MAX,               // 11
};

struct srccap_color_info_vui {
	u8 color_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;
};

static enum srccap_stub_mode srccapStubMode;
//mapping HDMI AVI color space to VUI
static struct srccap_color_info_vui color_metry_vui_map[11] = {
	{1, 11, 5},    //0:MS_HDMI_EXT_COLOR_XVYCC601
	{1, 11, 1},    //1:MS_HDMI_EXT_COLOR_XVYCC709
	{1, 13, 5},    //2:MS_HDMI_EXT_COLOR_SYCC601
	{0x80, 21, 0}, //3:MS_HDMI_EXT_COLOR_ADOBEYCC601
	{0x80, 21, 0}, //4:MS_HDMI_EXT_COLOR_ADOBERGB
	{9, 15, 10},   //5:MS_HDMI_EXT_COLOR_BT2020YcCbcCrc
	{9, 14, 9},    //6:MS_HDMI_EXT_COLOR_BT2020RGBYCbCr
	{12, 13, 0},  //7:MS_HDMI_EXT_COLOR_ADDITIONAL_DCI_P3_RGB_D65
	{12, 13, 0},   //8:MS_HDMI_EXT_COLOR_ADDITIONAL_DCI_P3_RGB_THEATER
	{6, 6, 6},     //9:MS_HDMI_EXT_COLORIMETRY_SMPTE_170M
	{1, 1, 1},     //10:MS_HDMI_EXT_COLORIMETRY_ITU_R_BT_709
};

//mapping src type to CFD info
static struct srccap_main_color_info src_cfd_map[E_PQ_CFD_MAX] = {
	{2, 0, 8, 6, 6, 6},  // 0 E_PQ_CFD_ANALOG_YUV_NTSC
	{2, 0, 8, 5, 6, 5},  // 1 E_PQ_CFD_ANALOG_YUV_PAL
	{2, 0, 8, 5, 6, 5},  // 2 E_PQ_CFD_ANALOG_YUV_SECAM
	{0, 0, 8, 6, 6, 6},  // 3 E_PQ_CFD_ANALOG_RGB_NTSC
	{0, 0, 8, 5, 6, 5},  // 4 E_PQ_CFD_ANALOG_RGB_PAL
	{2, 0, 8, 5, 6, 5},  // 5 E_PQ_CFD_YPBPR_SD
	{2, 0, 8, 1, 1, 1},  // 6 E_PQ_CFD_YPBPR_FHD
	{2, 0, 8, 2, 2, 2},  // 7 E_PQ_CFD_YPBPR_P2P
	{0, 1, 8, 1, 13, 5}, // 8 E_PQ_CFD_DVI
	{0, 1, 8, 1, 13, 5}, // 9 E_PQ_CFD_VGA
	{2, 0, 8, 2, 2, 2},  // 10 E_PQ_CFD_HDMI
};

//mapping GCE bit depth from enum to true depth
static u8 bit_depth_map[M_HDMI_COLOR_DEPTH_UNKNOWN] = {
	BIT_DEPTH_6_BIT,   //M_HDMI_COLOR_DEPTH_6_BIT
	BIT_DEPTH_8_BIT,   //M_HDMI_COLOR_DEPTH_8_BIT
	BIT_DEPTH_10_BIT, //M_HDMI_COLOR_DEPTH_10_BIT
	BIT_DEPTH_12_BIT, //M_HDMI_COLOR_DEPTH_12_BIT
	BIT_DEPTH_16_BIT  //M_HDMI_COLOR_DEPTH_16_BIT
};

// used to remap from enum m_hdmi_colorimetry_format to MS_HDMI_EXT_COLORIMETRY_FORMAT
#define HDMI_COLORI_METRY_REMAP_SIZE  19
static MS_HDMI_EXT_COLORIMETRY_FORMAT enHdmiColoriMetryRemap[HDMI_COLORI_METRY_REMAP_SIZE] = {
	MS_HDMI_EXT_COLORIMETRY_ITU_R_BT_709,  // 0x0 M_HDMI_COLORIMETRY_NO_DATA
	MS_HDMI_EXT_COLORIMETRY_SMPTE_170M,  // 0x1 M_HDMI_COLORIMETRY_SMPTE_170M
	MS_HDMI_EXT_COLORIMETRY_ITU_R_BT_709,  // 0x2 M_HDMI_COLORIMETRY_ITU_R_BT_709
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0x3 M_HDMI_COLORIMETRY_EXTENDED
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0x4 empty
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0x5 empty
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0x6 empty
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0x7 empty
	MS_HDMI_EXT_COLOR_XVYCC601,  // 0x8 M_HDMI_EXT_COLORIMETRY_XVYCC601
	MS_HDMI_EXT_COLOR_XVYCC709,  // 0x9 M_HDMI_EXT_COLORIMETRY_XVYCC709
	MS_HDMI_EXT_COLOR_SYCC601,  // 0xA M_HDMI_EXT_COLORIMETRY_SYCC601
	MS_HDMI_EXT_COLOR_ADOBEYCC601,  // 0xB M_HDMI_EXT_COLORIMETRY_ADOBEYCC601
	MS_HDMI_EXT_COLOR_ADOBERGB,  // 0xC M_HDMI_EXT_COLORIMETRY_ADOBERGB
	MS_HDMI_EXT_COLOR_BT2020YcCbcCrc,  // 0xD M_HDMI_EXT_COLORIMETRY_BT2020YcCbcCrc
	MS_HDMI_EXT_COLOR_BT2020RGBYCbCr,  // 0xE M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr
	MS_HDMI_EXT_COLOR_BT2020RGBYCbCr,  // 0xF M_HDMI_EXT_COLORIMETRY_ADDITIONAL
	MS_HDMI_EXT_COLOR_ADDITIONAL_DCI_P3_RGB_D65,  // 0x10
				// M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_D65
	MS_HDMI_EXT_COLOR_ADDITIONAL_DCI_P3_RGB_THEATER,  // 0x11
				// M_HDMI_EXT_COLORIMETRY_ADDITIONAL_DCI_P3_RGB_THEATER
	MS_HDMI_EXT_COLOR_UNKNOWN,  // 0xffff M_HDMI_COLORMETRY_UNKNOWN
};

static enum m_hdmi_color_format pre_color_format[SRCCAP_DEV_NUM] = {M_HDMI_COLOR_DEFAULT,
	M_HDMI_COLOR_DEFAULT};
static bool m_hdmi_color_format_change[M_HDMI_COLOR_ID_RESERVED][SRCCAP_DEV_NUM] = {0};

#define CFD_FMT_NUM        (5)
#define CFD_GET_FORMAT_FROM_MAP(index) \
	((index >= CFD_FMT_NUM) ? cfd_fmt_map[0] : cfd_fmt_map[index])

static const u8 cfd_fmt_map[CFD_FMT_NUM] = {
	E_PQ_CFD_DATA_FORMAT_RGB,
	E_PQ_CFD_DATA_FORMAT_YUV422,
	E_PQ_CFD_DATA_FORMAT_YUV444,
	E_PQ_CFD_DATA_FORMAT_YUV420,
	E_PQ_CFD_DATA_FORMAT_RESERVED_START};

#define HDMI_COLORMETRI_REMAP(x) enHdmiColoriMetryRemap[((x) >= \
		HDMI_COLORI_METRY_REMAP_SIZE ? HDMI_COLORI_METRY_REMAP_SIZE - 1 : (x))]
#define IEEE_HRD_VIVID (0x037504)

#define	ALG_CAPABILITY_VERSION 0x00010000

#define VSYNC_TRIG_LEN 20
/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */
static int _get_hdr_type(struct srccap_color_info *color_info, MS_BOOL bHDRViVidInfoValid);
static int _srccap_alloc_by_mmap(uint32_t index, uint32_t offset, uint32_t *size,
	void **virt, uint64_t *bus, uint64_t *phys, struct mtk_srccap_dev *srccap_dev)
{
	struct device_node *np = NULL;
	struct of_mmap_info_data mmap_data = {0};
	uint32_t sz = *size;
	uint64_t ba, pa;
	void *va;
	int ret;

	if (srccap_dev->dev->of_node != NULL)
		of_node_get(srccap_dev->dev->of_node);
	else {
		SRCCAP_MSG_ERROR("of_node is NULL\n");
		return -EINVAL;
	}
	np = of_find_node_by_name(srccap_dev->dev->of_node, SRCCAP_DTSI_STR);
	if (np == NULL)
		SRCCAP_MSG_POINTER_CHECK();
	else {
		ret = of_mtk_get_reserved_memory_info_by_idx(
			np, 0, &mmap_data);
		if (ret < 0)
			SRCCAP_MSG_RETURN_CHECK(ret);
		else {
			if (sz == 0 && offset == 0) {
				// @sz is replaced by mmap size
				sz = mmap_data.buffer_size;
			} else if (offset + sz > mmap_data.buffer_size) {
				SRCCAP_MSG_DEBUG("offset(%u) + size(%u) > limit(%llu)", offset, sz,
					mmap_data.buffer_size);
				return -EINVAL;
			}
		}
	}
	SRCCAP_MSG_DEBUG("mmap size = 0x%llx", mmap_data.buffer_size);
	SRCCAP_MSG_DEBUG("mmap bus  = 0x%llx", mmap_data.start_bus_address);

	ba  = mmap_data.start_bus_address + offset;
	pa = ba - BUSADDR_TO_IPADDR_OFFSET;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	va = pfn_is_map_memory(__phys_to_pfn(ba)) ? __va(ba) : ioremap_wc(ba, (size_t)size);
#else
	va = pfn_valid(__phys_to_pfn(ba)) ? __va(ba) : ioremap_wc(ba, sz);
#endif
	if (IS_ERR_OR_NULL(va)) {
		SRCCAP_MSG_DEBUG("convert bus to virt fail, pfn: %d, bus: 0x%llx, size: 0x%x",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
		pfn_is_map_memory(__phys_to_pfn(ba)), ba, sz);
#else
		pfn_valid(__phys_to_pfn(ba)), ba, sz);
#endif
		return -EPERM;
	}

	*size = sz;
	*virt = va;
	*phys = pa;
	*bus  = ba;
	return 0;
}

static int _srccap_deinit_ctx_alg(struct mtk_srccap_dev *srccap_dev)
{
	uint64_t tmp_bus_addr;
	void *tmp_addr;

	tmp_bus_addr = srccap_dev->common_info.pqmap_info.mmap_info.bus_addr;
	tmp_addr = srccap_dev->common_info.pqmap_info.mmap_info.addr;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	if (!pfn_is_map_memory(__phys_to_pfn(tmp_bus_addr)))
#else
	if (!pfn_valid(__phys_to_pfn(tmp_bus_addr)))
#endif
		iounmap(tmp_addr);

	SRCCAP_MSG_INFO("virt free addr = %p\n", tmp_addr);

	return 0;
}

void common_vsync_isr(void *param)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	struct v4l2_event event;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	SRCCAP_ATRACE_INT_FORMAT(srccap_dev->dev_id,
		"41-S: [srccap]send_event_vsync_%u", srccap_dev->dev_id);

	if (video_is_registered(srccap_dev->vdev)) {
		SRCCAP_MSG_DEBUG("send V4L2_EVENT_VSYNC to event queue\n");
		memset(&event, 0, sizeof(struct v4l2_event));
		event.type = V4L2_EVENT_VSYNC;
		event.id = 0;
		v4l2_event_queue(srccap_dev->vdev, &event);
	}
}

static int _get_hdmi_port_by_src(enum v4l2_srccap_input_source src,
	int *hdmi_port)
{
	if ((src >= V4L2_SRCCAP_INPUT_SOURCE_DVI) && (src <= V4L2_SRCCAP_INPUT_SOURCE_DVI4))
		*hdmi_port = src - V4L2_SRCCAP_INPUT_SOURCE_DVI;
	else if ((src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI) && (src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4))
		*hdmi_port = src - V4L2_SRCCAP_INPUT_SOURCE_HDMI;
	else
		return -EINVAL;

	return 0;
}

static int _get_avd_source_type(bool b_scart_rgb, enum AVD_VideoStandardType e_standard,
	enum srccap_cfd_info_index *index)
{
	if (b_scart_rgb) {
		switch (e_standard) {
		case E_VIDEOSTANDARD_NTSC_44:            // 480
		case E_VIDEOSTANDARD_NTSC_M:             // 480
		case E_VIDEOSTANDARD_PAL_M:              // 480
		case E_VIDEOSTANDARD_PAL_60:             // 480
			*index = E_PQ_CFD_ANALOG_RGB_NTSC;  // for 480
			break;
		case E_VIDEOSTANDARD_PAL_N:              // 576
		case E_VIDEOSTANDARD_PAL_BGHI:           // 576
		case E_VIDEOSTANDARD_SECAM:              // 576
		default:
			*index = E_PQ_CFD_ANALOG_RGB_PAL;   // for 576
			break;
		}
	} else {
		switch (e_standard) {
		case E_VIDEOSTANDARD_PAL_M:
			*index = E_PQ_CFD_ANALOG_YUV_PAL;
			break;
		case E_VIDEOSTANDARD_PAL_N:
			*index = E_PQ_CFD_ANALOG_YUV_PAL;
			break;
		case E_VIDEOSTANDARD_NTSC_44:
			*index = E_PQ_CFD_ANALOG_YUV_NTSC;
			break;
		case E_VIDEOSTANDARD_PAL_60:
			*index = E_PQ_CFD_ANALOG_YUV_PAL;
			break;
		case E_VIDEOSTANDARD_NTSC_M:
			*index = E_PQ_CFD_ANALOG_YUV_NTSC;
			break;
		case E_VIDEOSTANDARD_SECAM:
			*index = E_PQ_CFD_ANALOG_YUV_SECAM;
			break;
		case E_VIDEOSTANDARD_PAL_BGHI:
		default:
			*index = E_PQ_CFD_ANALOG_YUV_PAL;
			break;
		}
	}

	return 0;
}

static u8 _get_avi_data_range(enum m_hdmi_color_format color_fmt,
	enum m_hdmi_color_range color_range)
{
	u8 retRange = E_PQ_CFD_COLOR_RANGE_LIMIT;

	switch (color_range) {
	case M_HDMI_COLOR_RANGE_LIMIT:
		retRange = E_PQ_CFD_COLOR_RANGE_LIMIT;
		break;

	case M_HDMI_COLOR_RANGE_FULL:
		retRange = E_PQ_CFD_COLOR_RANGE_FULL;
		break;

	default:
		if (color_fmt == M_HDMI_COLOR_RGB)
			retRange = E_PQ_CFD_COLOR_RANGE_FULL;
		else
			retRange = E_PQ_CFD_COLOR_RANGE_LIMIT;
		break;
	}
	return retRange;
}

static u8 _get_out_data_fmt(u8 in_color_fmt,
	u8 out_color_fmt,
	enum v4l2_memout_buf_ctrl_mode buf_ctrl_mode)
{
	u8 retFmt = out_color_fmt;

	if (buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE) {
		if (in_color_fmt == MS_HDMI_COLOR_YUV_422 || in_color_fmt == MS_HDMI_COLOR_YUV_420)
			retFmt = MS_HDMI_COLOR_YUV_444;
		else
			retFmt = in_color_fmt;
	}

	return retFmt;
}

static int mtk_common_get_hdmi_color_meta(struct mtk_srccap_dev *srccap_dev,
	struct hdmi_avi_packet_info *avi_info_packet, struct meta_srccap_hdr_pkt *hdr_info_packet,
	struct hdmi_emp_packet_info *emp_info_packet, struct hdmi_gc_packet_info *gc_info_packet,
	struct meta_srccap_hdr_vsif_pkt *hdr_vsif_pkt)
{
	int ret = 0;
	enum v4l2_srccap_input_source src;
	static struct hdmi_avi_packet_info avi_info[SRCCAP_DEV_NUM] = {0};
	static struct meta_srccap_hdr_pkt hdr_pkt[SRCCAP_DEV_NUM] = {0};
	static struct hdmi_emp_packet_info
			empPkt[SRCCAP_DEV_NUM][META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE] = {0};
	static struct hdmi_gc_packet_info gc_pkt[SRCCAP_DEV_NUM] = {0};
	static struct hdmi_vsif_packet_info
			vsif_data_array[SRCCAP_DEV_NUM][META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE] = {0};

	if (srccap_dev == NULL || avi_info_packet == NULL ||
		hdr_info_packet == NULL || emp_info_packet == NULL ||
		gc_info_packet == NULL || hdr_vsif_pkt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if ((srccap_dev->dev_id != SRCCAP_COMMON_DEVICE_0) &&
			(srccap_dev->dev_id != SRCCAP_COMMON_DEVICE_1)) {
		SRCCAP_MSG_ERROR("failed (dev_id = %d)\n", srccap_dev->dev_id);
		return -EINVAL;
	}

	memset(&(hdr_pkt[srccap_dev->dev_id]), 0, sizeof(hdr_pkt[srccap_dev->dev_id]));
	memset(&(avi_info[srccap_dev->dev_id]), 0, sizeof(avi_info[srccap_dev->dev_id]));
	memset(&(empPkt[srccap_dev->dev_id]), 0, sizeof(struct hdmi_emp_packet_info)
		* META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE);
	memset(&(gc_pkt[srccap_dev->dev_id]), 0, sizeof(gc_pkt[srccap_dev->dev_id]));
	memset(vsif_data_array[srccap_dev->dev_id], 0,
		sizeof(vsif_data_array[srccap_dev->dev_id]));

	src = srccap_dev->srccap_info.src;

	/*get pkt*/
	if (mtk_srccap_common_is_Cfd_stub_mode()) {
		ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_HDRIF(&(hdr_pkt[srccap_dev->dev_id]));
		ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_AVI(&(avi_info[srccap_dev->dev_id]));
		ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_EMP(&(empPkt[srccap_dev->dev_id][0]));
		ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_GCP(&(gc_pkt[srccap_dev->dev_id]));
		ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_VSIF(
				&(vsif_data_array[srccap_dev->dev_id][0]));
		if (ret != 0)
			mtk_common_parse_vsif_pkt(
				&(vsif_data_array[srccap_dev->dev_id][0]), ret, hdr_vsif_pkt);
		srccap_dev->dv_info.descrb_info.common.interface
			= SRCCAP_DV_DESCRB_INTERFACE_NONE;
	} else {
		ret = mtk_srccap_hdmirx_pkt_get_HDRIF(src,
			((struct hdmi_hdr_packet_info *)&(hdr_pkt[srccap_dev->dev_id])),
			sizeof(struct hdmi_hdr_packet_info));
		if (ret == 0)
			SRCCAP_MSG_DEBUG(
			"mtk_srccap_hdmirx_pkt_get_HDRIF failed, set 0 instead\n");
		ret = mtk_srccap_hdmirx_pkt_get_AVI(src, &(avi_info[srccap_dev->dev_id]),
			sizeof(struct hdmi_avi_packet_info));
		if (ret == 0)
			SRCCAP_MSG_DEBUG(
			"mtk_srccap_hdmirx_pkt_get_AVI failed, set 0 instead\n");
		ret = mtk_srccap_hdmirx_pkt_get_VS_EMP(src, &(empPkt[srccap_dev->dev_id][0]),
			sizeof(struct hdmi_emp_packet_info)
			* META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE);
		if (ret == 0)
			SRCCAP_MSG_DEBUG(
			"mtk_srccap_hdmirx_pkt_get_VS_EMP failed, set 0 instead\n");
		ret = mtk_srccap_hdmirx_pkt_get_GCP(src, &(gc_pkt[srccap_dev->dev_id]),
			sizeof(struct hdmi_gc_packet_info));
		if (ret == 0)
			SRCCAP_MSG_DEBUG(
			"mtk_srccap_hdmirx_pkt_get_GCP failed, set 0 instead\n");
		ret = mtk_srccap_hdmirx_pkt_get_VSIF(src, vsif_data_array[srccap_dev->dev_id],
			sizeof(struct hdmi_vsif_packet_info)*META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);
		if (ret == 0) {
			SRCCAP_MSG_DEBUG("failed to mtk_srccap_hdmirx_pkt_get_VSIF\n");
		} else {
			if (mtk_common_parse_vsif_pkt
				(vsif_data_array[srccap_dev->dev_id], ret, hdr_vsif_pkt) != 0) {
				SRCCAP_MSG_ERROR("failed to parse vsif pkt\n");
			}
		}
	}

	memcpy(avi_info_packet, &(avi_info[srccap_dev->dev_id]),
		sizeof(struct hdmi_avi_packet_info));
	memcpy(hdr_info_packet, &(hdr_pkt[srccap_dev->dev_id]),
		sizeof(struct meta_srccap_hdr_pkt));
	memcpy(emp_info_packet, &(empPkt[srccap_dev->dev_id][0]),
		sizeof(struct hdmi_emp_packet_info) * META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE);
	memcpy(gc_info_packet, &(gc_pkt[srccap_dev->dev_id]), sizeof(struct hdmi_gc_packet_info));

	return 1;
}

static void _check_hdmi_main_color_info(struct mtk_srccap_dev *srccap_dev,
	struct srccap_color_info *color_info, struct srccap_color_info *temp_info)
{
	if (color_info == NULL || srccap_dev == NULL || temp_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	if (color_info->hdr_type != temp_info->hdr_type ||
		color_info->hdr10p_sstm != temp_info->hdr10p_sstm ||
		color_info->hdr10p_allm != temp_info->hdr10p_allm) {
		SRCCAP_MSG_INFO("HDR type change (%d, %d)(%d, %d)(%d, %d)\n",
			color_info->hdr_type, temp_info->hdr_type,
			color_info->hdr10p_sstm, temp_info->hdr10p_sstm,
			color_info->hdr10p_allm, temp_info->hdr10p_allm);
		srccap_dev->common_info.color_info.event_changed
			|= E_SRCCAP_COMMON_EVENT_HDR_CHANGE;
	}

	if (temp_info->hdr_type == E_PQ_CFD_HDR_MODE_HDR10PLUS) {
		if (temp_info->in.transfer_characteristics == E_PQ_CFD_CFIO_TR_SMPTE2084 ||
			temp_info->in.transfer_characteristics == E_PQ_CFD_CFIO_TR_BT2020NCL) {
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_HDR10PLUS;
		} else {
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_SDR;
		}
	} else {
		color_info->hdr_type = temp_info->hdr_type;
	}

	color_info->hdr10p_sstm = temp_info->hdr10p_sstm;
	color_info->hdr10p_allm = temp_info->hdr10p_allm;

	if (color_info->in.data_format != temp_info->in.data_format) {
		SRCCAP_MSG_INFO("Color format change(%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)\n",
			color_info->in.data_format, temp_info->in.data_format,
			color_info->in.data_range, temp_info->in.data_range,
			color_info->in.bit_depth, temp_info->in.bit_depth,
			color_info->in.color_primaries, temp_info->in.color_primaries,
			color_info->in.transfer_characteristics,
			temp_info->in.transfer_characteristics,
			color_info->in.matrix_coefficients, temp_info->in.matrix_coefficients);

		srccap_dev->common_info.color_info.event_changed
			|= E_SRCCAP_COMMON_EVENT_FORMAT_CHANGE;

		if (srccap_dev->streaming == true) {
			if (pre_color_format[srccap_dev->dev_id] == E_PQ_CFD_DATA_FORMAT_RESERVED_START) {
				SRCCAP_MSG_INFO("[CFD]First color_format\n");
			} else if (pre_color_format[srccap_dev->dev_id] != temp_info->in.data_format) {
				SRCCAP_MSG_INFO("[CFD] color_format_change set true, pre %d, df %d\n",
					pre_color_format[srccap_dev->dev_id], temp_info->in.data_format);
				m_hdmi_color_format_change[M_HDMI_COLOR_SRC_TIMING][srccap_dev->dev_id] = true;
				m_hdmi_color_format_change[M_HDMI_COLOR_DV_DESCRB][srccap_dev->dev_id] = true;
			}
			pre_color_format[srccap_dev->dev_id] = temp_info->in.data_format;
		}
	}

	color_info->in.data_format = temp_info->in.data_format;
	color_info->in.data_range = temp_info->in.data_range;
	color_info->in.bit_depth = temp_info->in.bit_depth;
	color_info->in.color_primaries = temp_info->in.color_primaries;
	color_info->in.transfer_characteristics = temp_info->in.transfer_characteristics;
	color_info->in.matrix_coefficients = temp_info->in.matrix_coefficients;

}


static int _parse_hdmi_main_color_info(struct mtk_srccap_dev *srccap_dev,
	struct srccap_color_info *color_info)
{
	struct srccap_main_color_info *color_info_in = NULL;
	MS_HDMI_EXT_COLORIMETRY_FORMAT enColormetry;
	struct meta_srccap_hdr_pkt hdrPkt = {0};
	struct hdmi_avi_packet_info avi_info = {0};
	struct hdmi_emp_packet_info empPkt[META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE] = {0};
	struct hdmi_gc_packet_info gcPkt = {0};
	struct meta_srccap_hdr_vsif_pkt vsifPkt = {0};
	MS_BOOL bHDRViVidInfoValid = 0;
	struct srccap_color_info temp_color_info = {0};


	if (color_info == NULL || srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}
	color_info_in = &(temp_color_info.in);

	/*get pkt*/
	if (mtk_common_get_hdmi_color_meta
		(srccap_dev, &avi_info, &hdrPkt, empPkt, &gcPkt, &vsifPkt) == 0)
		SRCCAP_MSG_ERROR("mtk_common_get_hdmi_color_meta failed\n");

	bHDRViVidInfoValid =
		((((MS_U32)empPkt[0].au8md_first[0]) << 16)
		| (((MS_U32)empPkt[0].au8md_first[1]) << 8)
		| ((MS_U32)empPkt[0].au8md_first[2]) == IEEE_HRD_VIVID) ? TRUE : FALSE;

	enColormetry = HDMI_COLORMETRI_REMAP(avi_info.enColormetry);
	if (enColormetry < 0 || enColormetry > MS_HDMI_EXT_COLORIMETRY_ITU_R_BT_709)
		enColormetry = MS_HDMI_EXT_COLOR_XVYCC601;

	/*ctrl point 1*/
	color_info_in->data_format = CFD_GET_FORMAT_FROM_MAP(avi_info.enColorForamt);
	color_info_in->data_range = _get_avi_data_range(avi_info.enColorForamt,
		avi_info.enColorRange);

	//color fmt = YUV422, BitDepth always=12,
	//it can't refer to gcPkt in this case(by HDMI team)
	if (avi_info.enColorForamt != M_HDMI_COLOR_YUV_422) {
		if (gcPkt.enColorDepth >= M_HDMI_COLOR_DEPTH_UNKNOWN
			|| gcPkt.enColorDepth < M_HDMI_COLOR_DEPTH_6_BIT) {
			color_info_in->bit_depth = BIT_DEPTH_8_BIT;
		} else {
			color_info_in->bit_depth = bit_depth_map[gcPkt.enColorDepth];
		}
	} else
		color_info_in->bit_depth = BIT_DEPTH_12_BIT;
	color_info_in->color_primaries =
		color_metry_vui_map[enColormetry].color_primaries;

	/*transfer_characteristics logic*/
	if (bHDRViVidInfoValid) {
		if ((empPkt[0].au8md_first[14] >> 7) == 0) {
			//HDR
			color_info_in->transfer_characteristics = E_PQ_CFD_CFIO_TR_SMPTE2084;
		} else if ((empPkt[0].au8md_first[14] >> 7) == 1) {
			//HLG
			color_info_in->transfer_characteristics = E_PQ_CFD_CFIO_TR_HLG;
		} else {
			color_info_in->transfer_characteristics =
				color_metry_vui_map[enColormetry].transfer_characteristics;
		}
	} else if (hdrPkt.u8Length != 0) {
		if (hdrPkt.u8EOTF == 2) {
			//HDR10
			color_info_in->transfer_characteristics = E_PQ_CFD_CFIO_TR_SMPTE2084;
		} else if (hdrPkt.u8EOTF == 3) {
			//HLG
			color_info_in->transfer_characteristics = E_PQ_CFD_CFIO_TR_HLG;
		} else {
			color_info_in->transfer_characteristics =
				color_metry_vui_map[enColormetry].transfer_characteristics;
		}
	} else {
		color_info_in->transfer_characteristics =
			color_metry_vui_map[enColormetry].transfer_characteristics;
	}
	color_info_in->matrix_coefficients =
		color_metry_vui_map[enColormetry].matrix_coefficients;

	/*set common format*/
	if (srccap_dev->dv_info.descrb_info.common.interface
		!= SRCCAP_DV_DESCRB_INTERFACE_NONE) {
		temp_color_info.hdr_type = E_PQ_CFD_HDR_MODE_DOLBY; //E_PQ_CFD_HDR_MODE_DOLBY;
	} else if (vsifPkt.u32IEEECode == SRCCAP_HDMI_VSIF_PACKET_IEEE_OUI_HRD10_PLUS) {
		temp_color_info.hdr_type = E_PQ_CFD_HDR_MODE_HDR10PLUS;
		temp_color_info.hdr10p_allm = vsifPkt.u8AutoLowLatency;
		temp_color_info.hdr10p_sstm = vsifPkt.u8SourceSideTmoFlag;
	} else {
		_get_hdr_type(&temp_color_info, bHDRViVidInfoValid);
	}

	_check_hdmi_main_color_info(srccap_dev, color_info, &temp_color_info);
	SRCCAP_MSG_DEBUG("CFD get format %d, range %d, color space %d color depth %d hdr_type %d\n",
		avi_info.enColorForamt,
		avi_info.enColorRange,
		avi_info.enColormetry,
		color_info_in->bit_depth,
		color_info->hdr_type);

	return 0;
}

static int _parse_source_main_color_info(struct mtk_srccap_dev *srccap_dev,
	struct srccap_color_info *color_info)
{
	enum v4l2_srccap_input_source src;
	enum srccap_cfd_info_index index = E_PQ_CFD_MAX;
	u16 h_size;
	u16 v_size;
	bool b_hdmi_mode;
	bool b_interlace;
	bool b_scart_rgb;
	enum AVD_VideoStandardType e_standard;

	if ((srccap_dev == NULL) || (color_info == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	src = srccap_dev->srccap_info.src;
	h_size = srccap_dev->timingdetect_info.data.h_de;
	v_size = srccap_dev->timingdetect_info.data.v_de;
	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		index = E_PQ_CFD_VGA;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		if (mtk_srccap_common_is_Cfd_stub_mode())
			index = E_PQ_CFD_HDMI;
		else {
			b_hdmi_mode = mtk_srccap_hdmirx_isHDMIMode(src);
			if (!b_hdmi_mode)
				index = E_PQ_CFD_DVI;
			else
				index = E_PQ_CFD_HDMI;
		}
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		b_interlace = srccap_dev->timingdetect_info.data.interlaced;
		if ((h_size >= SRCCAP_COMMON_FHD_W) && (v_size >= SRCCAP_COMMON_FHD_H))
			index = E_PQ_CFD_YPBPR_FHD;
		else
			index = E_PQ_CFD_YPBPR_SD;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		b_scart_rgb = (srccap_dev->adc_info.adc_src == SRCCAP_ADC_SOURCE_ONLY_SCART_RGB);
		e_standard = Api_AVD_GetLastDetectedStandard();
		_get_avd_source_type(b_scart_rgb, e_standard, &index);
		break;
	default:
		return -EINVAL;
	}

	if ((index >= E_PQ_CFD_ANALOG_YUV_NTSC) && (index < E_PQ_CFD_MAX)) {
		memcpy(&color_info->in,
			&src_cfd_map[index],
			sizeof(struct srccap_main_color_info));
		SRCCAP_MSG_DEBUG("Get Color info index: %d\n", index);
	} else {
		SRCCAP_MSG_ERROR("Color info index invalid: %d\n", index);
		return -EINVAL;
	}

	if (index == E_PQ_CFD_HDMI) {
		SRCCAP_MSG_DEBUG("Get Color info index HDMI, need paser HDMI info\n");
		_parse_hdmi_main_color_info(srccap_dev, color_info);
	}

	SRCCAP_MSG_DEBUG("CFD get format = %u, range = %u, cp = %u, tc = %u, mc = %u\n",
		color_info->in.data_format,
		color_info->in.data_range,
		color_info->in.color_primaries,
		color_info->in.transfer_characteristics,
		color_info->in.matrix_coefficients);

	return 0;
}

static int _get_hdr_type(struct srccap_color_info *color_info, MS_BOOL bHDRViVidInfoValid)
{
	if (color_info->in.transfer_characteristics == E_PQ_CFD_CFIO_TR_SMPTE2084) {
		//E_PQ_CFD_CFIO_TR_SMPTE2084
		if (bHDRViVidInfoValid)
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_HDRVIVID;
		else
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_HDR10;
	} else if (color_info->in.transfer_characteristics == E_PQ_CFD_CFIO_TR_HLG) {
		//E_PQ_CFD_CFIO_TR_HLG
		if (bHDRViVidInfoValid)
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_HDRVIVID;
		else
			color_info->hdr_type = E_PQ_CFD_HDR_MODE_HLG;
	} else {
		color_info->hdr_type = E_PQ_CFD_HDR_MODE_SDR;
	}
	SRCCAP_MSG_DEBUG("color_info hdr_type = %u\n", color_info->hdr_type);

	return 0;
}

/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
static int mtk_common_get_hdr_type(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_common_hdr_type *hdr_type)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (hdr_type == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = mtk_common_updata_frame_color_info(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	switch (srccap_dev->common_info.color_info.hdr_type) {
	case E_PQ_CFD_HDR_MODE_SDR:
		*hdr_type = V4L2_COMMON_HDR_NONE;
		break;
	case E_PQ_CFD_HDR_MODE_DOLBY:
		*hdr_type = V4L2_COMMON_HDR_DOLBY;
		break;
	case E_PQ_CFD_HDR_MODE_HDR10:
		*hdr_type = V4L2_COMMON_HDR_HDR10;
		break;
	case E_PQ_CFD_HDR_MODE_HLG:
		*hdr_type = V4L2_COMMON_HDR_HLG10;
		break;
	case E_PQ_CFD_HDR_MODE_TCH:
		*hdr_type = V4L2_COMMON_HDR_TCH;
		break;
	case E_PQ_CFD_HDR_MODE_HDR10PLUS:
		*hdr_type = V4L2_COMMON_HDR_HDR10PLUS;
		break;
	case E_PQ_CFD_HDR_MODE_HDRVIVID:
		*hdr_type = V4L2_COMMON_HDR_VIVID;
		break;
	default:
		*hdr_type = V4L2_COMMON_HDR_NONE;
		break;
	}

	return 0;
}

static int mtk_common_set_dither(
	struct mtk_srccap_dev *srccap_dev,
	bool en)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = drv_hwreg_srccap_common_set_dither(srccap_dev->dev_id, en, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int mtk_common_set_pqmap_rm_info(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_common_pqmap_rm_info rm_info)
{
	int ret = 0;
	u16 array_size = 0;
	u16 *node_array = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	array_size = rm_info.array_size;
	if (array_size == 0) {
		SRCCAP_MSG_ERROR("empty array!\n");
		return -EINVAL;
	}

	node_array = kcalloc(array_size, sizeof(u16), GFP_KERNEL);
	if (node_array == NULL) {
		SRCCAP_MSG_ERROR("failed to kcalloc!\n");
		return -ENOMEM;
	}

	if (copy_from_user((void *)node_array,
		(u16 __user *)rm_info.p.node_array, sizeof(u16)*array_size)) {
		SRCCAP_MSG_ERROR("failed to copy_from_user!\n");
		ret = -EFAULT;
		goto EXIT;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);
	srccap_dev->common_info.rm_info.array_size = array_size;

	if (srccap_dev->common_info.rm_info.p.node_array != NULL) {
		kfree(srccap_dev->common_info.rm_info.p.node_array);
		srccap_dev->common_info.rm_info.p.node_array = NULL;
	}

	srccap_dev->common_info.rm_info.p.node_array = kcalloc(array_size, sizeof(u16), GFP_KERNEL);
	if (srccap_dev->common_info.rm_info.p.node_array == NULL) {
		SRCCAP_MSG_ERROR("failed to kcalloc!\n");
		ret = -ENOMEM;
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);
		goto EXIT;
	}

	memcpy(srccap_dev->common_info.rm_info.p.node_array, node_array,
		(sizeof(u16) * array_size));
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);

	if (!srccap_dev->streaming) {
		ret = mtk_srccap_common_load_pqmap(srccap_dev, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}
	}

EXIT:
	kfree(node_array);
	return ret;
}

static int _common_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, common_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_COMMON_HDR_TYPE:
	{
		enum v4l2_common_hdr_type hdr_type = V4L2_COMMON_HDR_NONE;

		SRCCAP_MSG_DEBUG("Get V4L2_CID_COMMON_HDR_TYPE\n");

		ret = mtk_common_get_hdr_type(srccap_dev, &hdr_type);
		ctrl->val = (s32)hdr_type;
		break;
	}
	case V4L2_CID_SRCCAP_PQMAP_INFO:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_COMMON_PQMAP_INFO\n");
		ret = mtk_srccap_common_get_pqmap_info(
		(struct v4l2_srccap_pqmap_info *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	default:
	{
		ret = -1;
		break;
	}
	}

	return ret;
}

static int _common_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, common_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_COMMON_FORCE_VSYNC:
	{
		struct v4l2_common_force_vsync i;

		memset(&i, 0, sizeof(struct v4l2_common_force_vsync));
		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		i = *(struct v4l2_common_force_vsync *)(ctrl->p_new.p);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_COMMON_FORCE_VSYNC\n");

		ret = mtk_common_force_irq(i.type, i.event, i.set_force,
			srccap_dev->srccap_info.cap.u32IRQ_Version);
		break;
	}
	case V4L2_CID_COMMON_DITHER:
	{
		bool dither = (bool)(ctrl->val);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_COMMON_DITHER\n");

		ret = mtk_common_set_dither(srccap_dev, dither);
		break;
	}
	case V4L2_CID_SRCCAP_PQMAP_INFO:
	{
		SRCCAP_MSG_DEBUG("Set V4L2_CID_COMMON_PQMAP_INFO\n");

		ret = mtk_srccap_common_set_pqmap_info(
		(struct v4l2_srccap_pqmap_info *)ctrl->p_new.p_u8, srccap_dev);
		break;
	}
	case V4L2_CID_COMMON_PQMAP_RM_INFO:
	{
		struct v4l2_common_pqmap_rm_info rm_info;

		memset(&rm_info, 0, sizeof(struct v4l2_common_pqmap_rm_info));
		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		 rm_info =
			*(struct v4l2_common_pqmap_rm_info *)(ctrl->p_new.p);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_COMMON_PQMAP_RM_INFO\n");

		ret = mtk_common_set_pqmap_rm_info(srccap_dev, rm_info);
		break;
	}
	default:
	{
		ret = -1;
		break;
	}
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
		.id = V4L2_CID_COMMON_FORCE_VSYNC,
		.name = "Srccap Common Force Vsync IRQ",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_common_force_vsync)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_COMMON_DITHER,
		.name = "Srccap Common Dither",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_COMMON_HDR_TYPE,
		.name = "Srccap Common Hdr Type",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_SRCCAP_PQMAP_INFO,
		.name = "Srccap Common PQ Map Info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_srccap_pqmap_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &common_ctrl_ops,
		.id = V4L2_CID_COMMON_PQMAP_RM_INFO,
		.name = "Srccap Common PQ Map Rm Info",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_common_pqmap_rm_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE | V4L2_CTRL_FLAG_VOLATILE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops common_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops common_sd_internal_ops = {
};

static int srccap_common_init_alg_ctx(struct mtk_srccap_dev *srccap_dev)
{
	void *alg_ctx = NULL;
	void *sw_reg = NULL;
	void *meta_data = NULL;
	void *hw_report = NULL;
	int init_ctx_result = 0;
	int ret = 0;
	uint32_t index, offset, size;
	uint64_t bus, phys;
	void *virt;

	struct ST_PQ_CTX_SIZE_INFO ctx_size_info;
	struct ST_PQ_CTX_SIZE_STATUS ctx_size_status;
	struct ST_PQ_CTX_INFO ctx_info;
	struct ST_PQ_CTX_STATUS ctx_status;
	struct ST_PQ_FRAME_CTX_INFO frame_ctx_info;
	struct ST_PQ_FRAME_CTX_STATUS frame_ctx_status;
	struct ST_PQ_SWREG_SETWRITEBUF_INFO   swreg_set_info;
	struct ST_PQ_SWREG_SETWRITEBUF_STATUS swreg_set_stat;
	struct ST_PQ_SWREG_INITWRITEBUF_INFO   swreg_init_info;
	struct ST_PQ_SWREG_INITWRITEBUF_STATUS swreg_init_stat;
	enum EN_PQAPI_RESULT_CODES pq_ret = 0;

	memset(&ctx_size_info, 0, sizeof(struct ST_PQ_CTX_SIZE_INFO));
	memset(&ctx_size_status, 0, sizeof(struct ST_PQ_CTX_SIZE_STATUS));
	memset(&ctx_info, 0, sizeof(struct ST_PQ_CTX_INFO));
	memset(&ctx_status, 0, sizeof(struct ST_PQ_CTX_STATUS));
	memset(&frame_ctx_info, 0, sizeof(struct ST_PQ_FRAME_CTX_INFO));
	memset(&frame_ctx_status, 0, sizeof(struct ST_PQ_FRAME_CTX_STATUS));
	memset(&swreg_set_info, 0, sizeof(struct ST_PQ_SWREG_SETWRITEBUF_INFO));
	memset(&swreg_set_stat, 0, sizeof(struct ST_PQ_SWREG_SETWRITEBUF_STATUS));
	memset(&swreg_init_info, 0, sizeof(struct ST_PQ_SWREG_INITWRITEBUF_INFO));
	memset(&swreg_init_stat, 0, sizeof(struct ST_PQ_SWREG_INITWRITEBUF_STATUS));

	ctx_size_info.u32Version = 0;
	ctx_size_info.u32Length = sizeof(struct ST_PQ_CTX_SIZE_INFO);

	ctx_size_status.u32Version = 0;
	ctx_size_status.u32Length = sizeof(struct ST_PQ_CTX_SIZE_STATUS);
	ctx_size_status.u32PqCtxSize = 0;
	pq_ret = MApi_PQ_GetCtxSize(NULL, &ctx_size_info, &ctx_size_status);
	if (pq_ret == E_PQAPI_RC_SUCCESS) {
		if (ctx_size_status.u32PqCtxSize != 0) {
			alg_ctx = kvmalloc(ctx_size_status.u32PqCtxSize, GFP_KERNEL);
			if (alg_ctx)
				memset(alg_ctx, 0, ctx_size_status.u32PqCtxSize);
			else
				SRCCAP_MSG_ERROR("alg_ctx kvmalloc fail\n");
		} else {
			SRCCAP_MSG_ERROR("ctx_size_status.u32PqCtxSize is 0\n");
		}

		if (ctx_size_status.u32SwRegSize != 0) {
			sw_reg = kvmalloc(ctx_size_status.u32SwRegSize, GFP_KERNEL);
			if (sw_reg)
				memset(sw_reg, 0, ctx_size_status.u32SwRegSize);
			else
				SRCCAP_MSG_ERROR("sw_reg kvmalloc fail\n");
		} else {
			SRCCAP_MSG_ERROR("ctx_size_status.u32SwRegSize is 0\n");
		}

		if (ctx_size_status.u32MetadataSize != 0) {
			meta_data = kvmalloc(ctx_size_status.u32MetadataSize, GFP_KERNEL);
			if (meta_data)
				memset(meta_data, 0, ctx_size_status.u32MetadataSize);
			else
				SRCCAP_MSG_ERROR("meta_data kvmalloc fail\n");
		} else {
			SRCCAP_MSG_ERROR("ctx_size_status.u32MetadataSize is 0\n");
		}

		if (ctx_size_status.u32HwReportSize != 0) {
			hw_report = kvmalloc(ctx_size_status.u32HwReportSize, GFP_KERNEL);
			if (hw_report)
				memset(hw_report, 0, ctx_size_status.u32HwReportSize);
			else
				SRCCAP_MSG_ERROR("hw_report kvmalloc fail\n");
		} else {
			SRCCAP_MSG_ERROR("ctx_size_status.u32HwReportSize is 0\n");
		}
	} else {
		SRCCAP_MSG_ERROR("MApi_PQ_GetCtxSize failed! pq_ret = 0x%x\n", pq_ret);
	}

	ctx_info.eWinID = E_PQ_MAIN_WINDOW;
	init_ctx_result = MApi_PQ_InitCtx(alg_ctx, &ctx_info, &ctx_status);
	if (init_ctx_result > 0) {
		SRCCAP_MSG_ERROR("MApi_PQ_InitCtx fail, return:%d\n", init_ctx_result);
		ret = -1;
		goto EXIT;
	}
	srccap_dev->common_info.pqmap_info.alg_ctx = alg_ctx;
	srccap_dev->common_info.pqmap_info.sw_reg = sw_reg;
	srccap_dev->common_info.pqmap_info.meta_data = meta_data;
	srccap_dev->common_info.pqmap_info.hw_report = hw_report;
	SRCCAP_MSG_INFO("MApi_PQ_InitCtx success, u32PqCtxSize:%u\n", ctx_size_status.u32PqCtxSize);

	index = 0;
	offset = 0;
	size = 0;
	ret = _srccap_alloc_by_mmap(index, offset, &size, &virt, &bus, &phys, srccap_dev);
	if (ret) {
		SRCCAP_MSG_DEBUG("srccap_alloc_by_mmap fail ");
		ret = -1;
		goto EXIT;
	}
	srccap_dev->common_info.pqmap_info.mmap_info.bus_addr = bus;
	srccap_dev->common_info.pqmap_info.mmap_info.phy_addr = phys;
	srccap_dev->common_info.pqmap_info.mmap_info.addr = virt;

	SRCCAP_MSG_DEBUG("Get from mmap buffer virt addr = %p\n", virt);

	frame_ctx_info.pSwReg = virt;
	frame_ctx_info.pMetadata = meta_data;
	frame_ctx_info.pHwReport = hw_report;
	pq_ret = MApi_PQ_SetFrameCtx(alg_ctx, &frame_ctx_info, &frame_ctx_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		SRCCAP_MSG_ERROR("MApi_PQ_SetFrameCtx failed! pq_ret = 0x%x\n", pq_ret);
		ret = -1;
		goto EXIT;
	}
	SRCCAP_MSG_DEBUG("MApi_PQ_SetFrameCtx success\n");

	swreg_set_info.u32Version = API_VERSION_MAPI_PQ_SWREG_SETWRITEBUF;
	swreg_set_info.u32MaxSize = ctx_size_status.u32SwRegSize;
	swreg_set_info.pAddr      = virt;
	ret = MApi_PQ_SWReg_SetWriteBuf(alg_ctx, &swreg_set_info, &swreg_set_stat);
	if (ret == E_PQAPI_RC_FAIL) {
		SRCCAP_MSG_ERROR("MApi_PQ_SWReg_SetWriteBuf fail (0x%x)", ret);
		ret = -1;
		goto EXIT;
	}

	swreg_init_info.u32Version = API_VERSION_MAPI_PQ_SWREG_INITWRITEBUF;
	ret = MApi_PQ_SWReg_InitWriteBuf(alg_ctx, &swreg_init_info, &swreg_init_stat);
	if (ret == E_PQAPI_RC_FAIL) {
		SRCCAP_MSG_ERROR("MApi_PQ_SWReg_InitWriteBuf fail (0x%x)", ret);
		ret = -1;
		goto EXIT;
	}

	return ret;
EXIT:
	kvfree(alg_ctx);
	kvfree(sw_reg);
	kvfree(meta_data);
	kvfree(hw_report);
	_srccap_deinit_ctx_alg(srccap_dev);
	return ret;
}

static int srccap_common_free_alg_ctx(struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev->common_info.pqmap_info.alg_ctx) {
		kvfree(srccap_dev->common_info.pqmap_info.alg_ctx);
		srccap_dev->common_info.pqmap_info.alg_ctx = NULL;
	}

	if (srccap_dev->common_info.pqmap_info.sw_reg) {
		kvfree(srccap_dev->common_info.pqmap_info.sw_reg);
		srccap_dev->common_info.pqmap_info.sw_reg = NULL;
	}

	if (srccap_dev->common_info.pqmap_info.meta_data) {
		kvfree(srccap_dev->common_info.pqmap_info.meta_data);
		srccap_dev->common_info.pqmap_info.meta_data = NULL;
	}

	if (srccap_dev->common_info.pqmap_info.hw_report) {
		kvfree(srccap_dev->common_info.pqmap_info.hw_report);
		srccap_dev->common_info.pqmap_info.hw_report = NULL;
	}

	if (srccap_dev->common_info.pqmap_info.mmap_info.addr) {
		_srccap_deinit_ctx_alg(srccap_dev);
		srccap_dev->common_info.pqmap_info.mmap_info.addr = NULL;
	}

	return 0;
}

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
bool mtk_common_check_color_format_change(
	struct mtk_srccap_dev *srccap_dev,
	enum m_hdmi_color_format_change_ID event_ID)
{
	bool ret = false;
	enum v4l2_srccap_input_source src;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return true;
	}
	src = srccap_dev->srccap_info.src;

	if (src < V4L2_SRCCAP_INPUT_SOURCE_HDMI ||
		src > V4L2_SRCCAP_INPUT_SOURCE_HDMI4) {
		return false;
	}

	if (srccap_dev->streaming == false) {
		struct hdmi_avi_packet_info avi_info[SRCCAP_DEV_NUM] = {0};

		memset(&(avi_info[srccap_dev->dev_id]), 0,
				sizeof(avi_info[srccap_dev->dev_id]));
		ret = mtk_srccap_hdmirx_pkt_get_AVI(src, &(avi_info[srccap_dev->dev_id]),
			sizeof(struct hdmi_avi_packet_info));
		if (ret == 0) {
			SRCCAP_MSG_DEBUG("[CFD] get AVI failed\n");
			return false;
		}

		if (CFD_GET_FORMAT_FROM_MAP(avi_info[srccap_dev->dev_id].enColorForamt) !=
				pre_color_format[srccap_dev->dev_id]) {

			SRCCAP_MSG_INFO("[streaming_off]src: %d, eventID: %d, avi: %d, pre: %d\n",
				srccap_dev->dev_id, event_ID,
				avi_info[srccap_dev->dev_id].enColorForamt,
				pre_color_format[srccap_dev->dev_id]);

			pre_color_format[srccap_dev->dev_id] =
				CFD_GET_FORMAT_FROM_MAP(avi_info[srccap_dev->dev_id].enColorForamt);
			m_hdmi_color_format_change[M_HDMI_COLOR_SRC_TIMING][srccap_dev->dev_id] =
				true;
			m_hdmi_color_format_change[M_HDMI_COLOR_DV_DESCRB][srccap_dev->dev_id] =
				true;
		}
	}

	if (m_hdmi_color_format_change[event_ID][srccap_dev->dev_id] == false)
		return false;

	if (m_hdmi_color_format_change[event_ID][srccap_dev->dev_id] == true) {
		m_hdmi_color_format_change[event_ID][srccap_dev->dev_id] = false;
		SRCCAP_MSG_INFO("[format_change][event]src: %d, eventID: %d,\n",
		srccap_dev->dev_id, event_ID);
		return true;
	}

	return false;
}

int mtk_common_updata_color_info(struct mtk_srccap_dev *srccap_dev)
{
	struct srccap_color_info *color_info;
	enum v4l2_srccap_input_source src;
	int hdmi_port;
	u16 dev_id = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	src = srccap_dev->srccap_info.src;
	color_info = &srccap_dev->common_info.color_info;
	dev_id = srccap_dev->dev_id;

	if (_parse_source_main_color_info(srccap_dev, color_info))
		SRCCAP_MSG_ERROR("Get Main color info failed!!!\n");

	if (_get_hdmi_port_by_src(src, &hdmi_port))
		SRCCAP_MSG_DEBUG("Not HDMI source!!!\n");

	color_info->out.data_range = color_info->in.data_range;
	color_info->out.color_primaries = color_info->in.color_primaries;
	color_info->out.transfer_characteristics = color_info->in.transfer_characteristics;
	// out.data_format is set in set_fmt, update here if bypass mode
	color_info->out.data_format =
		_get_out_data_fmt(color_info->in.data_format,
			color_info->out.data_format,
			srccap_dev->memout_info.buf_ctrl_mode);

	//If control point 1 is RGB format
	//and control point 3 is YUV format, default BT.709.
	if ((color_info->in.data_format == E_PQ_CFD_DATA_FORMAT_RGB) &&
	((color_info->out.data_format == E_PQ_CFD_DATA_FORMAT_YUV420) ||
	(color_info->out.data_format == E_PQ_CFD_DATA_FORMAT_YUV422) ||
	(color_info->out.data_format == E_PQ_CFD_DATA_FORMAT_YUV444))) {
		color_info->out.matrix_coefficients =
			0x1; //default BT.709  E_PQ_CFD_CFIO_MC_BT709_XVYCC709
	} else {
		color_info->out.matrix_coefficients = color_info->in.matrix_coefficients;
	}

	color_info->out.bit_depth = color_info->in.bit_depth;
	color_info->hdr_type = 0; //default SDR, no need to set this param here.

	SRCCAP_MSG_INFO("Srccap R2Y off\n");

	return 0;
}

int mtk_common_parse_vsif_pkt(struct hdmi_vsif_packet_info *pvsif_data,
	u8 u8DataSize, struct meta_srccap_hdr_vsif_pkt *hdr_vsif_pkt)
{
	int u32IEEEOUI = 0;
	u8 u8Index = 0;

	if ((pvsif_data == NULL) || (hdr_vsif_pkt == NULL)
		|| (u8DataSize >= SRCCAP_MAX_VSIF_PACKET_NUMBER)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	for (u8Index = 0; u8Index < u8DataSize; u8Index++) {
		u32IEEEOUI = ((pvsif_data[u8Index].au8ieee[0])
				| (pvsif_data[u8Index].au8ieee[1] << 8)
				| (pvsif_data[u8Index].au8ieee[2] << 16));

		switch (u32IEEEOUI) {
		case SRCCAP_HDMI_VSIF_PACKET_IEEE_OUI_HRD10_PLUS:
			hdr_vsif_pkt->u16Version = 0;
			hdr_vsif_pkt->u16Size = sizeof(struct meta_srccap_hdr_vsif_pkt);
			hdr_vsif_pkt->bValid = TRUE;
			hdr_vsif_pkt->u8VSIFTypeCode =
				(SRCCAP_HDMI_VS_PACKET_TYPE) & 0x7F; //(0x81) & 0x7F
			hdr_vsif_pkt->u8VSIFVersion = pvsif_data[u8Index].u8version;
			hdr_vsif_pkt->u8Length = pvsif_data[u8Index].u5length & 0x1F;
			hdr_vsif_pkt->u32IEEECode = u32IEEEOUI;
			//PB4
			hdr_vsif_pkt->u8ApplicationVersion =
				(pvsif_data[u8Index].au8payload[0] & 0xC0) >> 6;
			hdr_vsif_pkt->u8TargetSystemDisplayMaxLuminance =
				(pvsif_data[u8Index].au8payload[0] & 0x3E) >> 1;
			hdr_vsif_pkt->u8AverageMaxRGB = pvsif_data[u8Index].au8payload[1];
			hdr_vsif_pkt->au8DistributionValues[0] = pvsif_data[u8Index].au8payload[2];
			hdr_vsif_pkt->au8DistributionValues[1] = pvsif_data[u8Index].au8payload[3];
			hdr_vsif_pkt->au8DistributionValues[2] = pvsif_data[u8Index].au8payload[4];
			hdr_vsif_pkt->au8DistributionValues[3] = pvsif_data[u8Index].au8payload[5];
			hdr_vsif_pkt->au8DistributionValues[4] = pvsif_data[u8Index].au8payload[6];
			hdr_vsif_pkt->au8DistributionValues[5] = pvsif_data[u8Index].au8payload[7];
			hdr_vsif_pkt->au8DistributionValues[6] = pvsif_data[u8Index].au8payload[8];
			hdr_vsif_pkt->au8DistributionValues[7] = pvsif_data[u8Index].au8payload[9];
			hdr_vsif_pkt->au8DistributionValues[8] = pvsif_data[u8Index].au8payload[10];
			hdr_vsif_pkt->u16KneePointX =
				(((pvsif_data[u8Index].au8payload[12] & 0xFC) >> 2)
				| ((pvsif_data[u8Index].au8payload[11] & 0x0F) << 6));
			hdr_vsif_pkt->u16KneePointY =
				(pvsif_data[u8Index].au8payload[13] & 0xFF)
				| ((pvsif_data[u8Index].au8payload[12] & 0x03) << 8);
			hdr_vsif_pkt->u8NumBezierCurveAnchors =
				(pvsif_data[u8Index].au8payload[11] & 0xF0) >> 4;
			hdr_vsif_pkt->au8BezierCurveAnchors[0] = pvsif_data[u8Index].au8payload[14];
			hdr_vsif_pkt->au8BezierCurveAnchors[1] = pvsif_data[u8Index].au8payload[15];
			hdr_vsif_pkt->au8BezierCurveAnchors[2] = pvsif_data[u8Index].au8payload[16];
			hdr_vsif_pkt->au8BezierCurveAnchors[3] = pvsif_data[u8Index].au8payload[17];
			hdr_vsif_pkt->au8BezierCurveAnchors[4] = pvsif_data[u8Index].au8payload[18];
			hdr_vsif_pkt->au8BezierCurveAnchors[5] = pvsif_data[u8Index].au8payload[19];
			hdr_vsif_pkt->au8BezierCurveAnchors[6] = pvsif_data[u8Index].au8payload[20];
			hdr_vsif_pkt->au8BezierCurveAnchors[7] = pvsif_data[u8Index].au8payload[21];
			hdr_vsif_pkt->au8BezierCurveAnchors[8] = pvsif_data[u8Index].au8payload[22];
			hdr_vsif_pkt->u8GraphicsOverlayFlag =
				(pvsif_data[u8Index].au8payload[23] & 0x80) >> 7;
			hdr_vsif_pkt->u8NoDelayFlag =
				(pvsif_data[u8Index].au8payload[23] & 0x40) >> 6;
			hdr_vsif_pkt->u8SourceSideTmoFlag =
				(pvsif_data[u8Index].au8payload[23] & 0x20) >> 5;
			hdr_vsif_pkt->u8AutoLowLatency =
				(pvsif_data[u8Index].au8payload[23] & 0x10) >> 4;

			return 0;
		default:
			break;
		}
	}
	return 0;
}

int mtk_common_updata_frame_color_info(struct mtk_srccap_dev *srccap_dev)
{
	struct srccap_color_info *color_info;
	enum v4l2_srccap_input_source src;
	int hdmi_port;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	SRCCAP_ATRACE_BEGIN("mtk_common_updata_frame_color_info");

	src = srccap_dev->srccap_info.src;
	color_info = &srccap_dev->common_info.color_info;
	color_info->event_changed = 0;

	if (_get_hdmi_port_by_src(src, &hdmi_port)) {
		SRCCAP_MSG_DEBUG("Not HDMI source!!!\n");
		/*set dip data format*/
		color_info->dip_point.data_format = color_info->out.data_format;

		color_info->dip_point.data_range = color_info->out.data_range;
		color_info->dip_point.color_primaries = color_info->out.color_primaries;
		color_info->dip_point.transfer_characteristics =
			color_info->out.transfer_characteristics;
		color_info->dip_point.matrix_coefficients = color_info->out.matrix_coefficients;
		color_info->dip_point.bit_depth = color_info->out.bit_depth;

	} else {
		//if HDMI
		if (mtk_srccap_hdmirx_isHDMIMode(src) || mtk_srccap_common_is_Cfd_stub_mode())
			_parse_hdmi_main_color_info(srccap_dev, color_info);

		/*set output data format*/
		color_info->out.data_range = color_info->in.data_range;
		color_info->out.color_primaries = color_info->in.color_primaries;
		color_info->out.transfer_characteristics = color_info->in.transfer_characteristics;
		// out.data_format is set in set_fmt, update here if bypass mode
		color_info->out.data_format =
			_get_out_data_fmt(color_info->in.data_format,
			color_info->out.data_format,
			srccap_dev->memout_info.buf_ctrl_mode);

		if (color_info->in.data_format == E_PQ_CFD_DATA_FORMAT_RGB) {
			switch (color_info->out.data_format) {
			case E_PQ_CFD_DATA_FORMAT_YUV420:
			case E_PQ_CFD_DATA_FORMAT_YUV422:
			case E_PQ_CFD_DATA_FORMAT_YUV444:
				color_info->out.matrix_coefficients =
					0x1; //E_PQ_CFD_CFIO_MC_BT709_XVYCC709; //default BT.709
				break;
			default:
				color_info->out.matrix_coefficients =
					color_info->in.matrix_coefficients;
				break;
			}


			color_info->dip_point.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		} else {
			color_info->dip_point.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
			color_info->out.matrix_coefficients = color_info->in.matrix_coefficients;
		}

		color_info->out.bit_depth = color_info->in.bit_depth;

		/*set dip data format*/

		color_info->dip_point.data_range = color_info->out.data_range;
		color_info->dip_point.color_primaries = color_info->out.color_primaries;
		color_info->dip_point.transfer_characteristics =
			color_info->out.transfer_characteristics;
		color_info->dip_point.matrix_coefficients = color_info->out.matrix_coefficients;
		color_info->dip_point.bit_depth = color_info->out.bit_depth;

	}
	SRCCAP_ATRACE_END("mtk_common_updata_frame_color_info");

	return 0;
}

int mtk_common_get_cfd_metadata(struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_color_info *cfd_meta)
{
	struct srccap_color_info *color_info;

	if ((srccap_dev == NULL) || (cfd_meta == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	color_info = &srccap_dev->common_info.color_info;
	//TS-domain input dataformat
	cfd_meta->u8DataFormat = color_info->out.data_format;
	cfd_meta->u8DataRange = color_info->out.data_range;
	cfd_meta->u8BitDepth = color_info->out.bit_depth;
	cfd_meta->u8ColorPrimaries = color_info->out.color_primaries;
	cfd_meta->u8TransferCharacteristics = color_info->out.transfer_characteristics;
	cfd_meta->u8MatrixCoefficients = color_info->out.matrix_coefficients;
	cfd_meta->u8HdrType = color_info->hdr_type;
	cfd_meta->u32SamplingMode = color_info->chroma_sampling_mode;
	cfd_meta->u8Source_Format = color_info->in.data_format;
	cfd_meta->u8IPDMA_Format = color_info->out.data_format;

	SRCCAP_MSG_DEBUG("[CFD][%s, %d] cfd_meta->(%d, %d, %d, %d, %d)(%d, %d, %d, %d, %d)\n",
		__func__, __LINE__,
		cfd_meta->u8DataFormat,
		cfd_meta->u8DataRange,
		cfd_meta->u8ColorPrimaries,
		cfd_meta->u8TransferCharacteristics,
		cfd_meta->u8MatrixCoefficients,
		cfd_meta->u8HdrType,
		cfd_meta->u32SamplingMode,
		cfd_meta->u8BitDepth,
		cfd_meta->u8Source_Format,
		cfd_meta->u8IPDMA_Format);

	return 0;
}

int mtk_common_set_cfd_setting(struct mtk_srccap_dev *srccap_dev)
{
	static struct ST_PQ_CFD_420_CONTROL_PARAMS cfd_420_control_param;
	static struct ST_PQ_CFD_420_CONTROL_INFO cfd_420_control_info;

	static struct ST_PQ_CFD_SET_INPUT_CSC_PARAMS st_cfd_set_csc_in = {0};
	static struct ST_PQ_CFD_SET_CUS_CSC_INFO st_cfd_set_csc_out = {0};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	SRCCAP_ATRACE_BEGIN("mtk_common_set_cfd_setting");

	// update hdr and color info for hdr change in srccap input
	if (pqu_render_frc_get_freeze_support()) {
		struct st_pqu_hdr_change_calc_params *pstHdrChangeCalc = NULL;

		pstHdrChangeCalc = pqu_render_frc_get_hdr_change_calc_ptr(hdr_change_input_srccap);
		if (pstHdrChangeCalc != NULL) {
			pstHdrChangeCalc->hdr_type = (enum EN_PQ_CFD_HDR_MODE)
				(srccap_dev->common_info.color_info.hdr_type);
			pstHdrChangeCalc->data_format = (enum EN_PQ_CFD_DATA_FORMAT)
				(srccap_dev->common_info.color_info.in.data_format);
			pstHdrChangeCalc->color_range = (enum EN_PQ_CFD_COLOR_RANGE)
				(srccap_dev->common_info.color_info.in.data_range);
			pstHdrChangeCalc->cp = (enum EN_PQ_CFD_CFIO_CP)
				(srccap_dev->common_info.color_info.in.color_primaries);
			pstHdrChangeCalc->tr = (enum EN_PQ_CFD_CFIO_TR)
				(srccap_dev->common_info.color_info.in.transfer_characteristics);
			pstHdrChangeCalc->mc = (enum EN_PQ_CFD_CFIO_MC)
				(srccap_dev->common_info.color_info.in.matrix_coefficients);

			pqu_render_frc_hdr_change_dbg_check_input_update(hdr_change_input_srccap);
		}
	}

	memset(&cfd_420_control_param, 0, sizeof(struct ST_PQ_CFD_420_CONTROL_PARAMS));
	memset(&cfd_420_control_info, 0, sizeof(struct ST_PQ_CFD_420_CONTROL_INFO));

	cfd_420_control_param.u32Version = 0;
	cfd_420_control_param.u32Length = sizeof(struct ST_PQ_CFD_420_CONTROL_PARAMS);
	cfd_420_control_param.u32Input_DataFormat =
		srccap_dev->common_info.color_info.in.data_format;
	cfd_420_control_param.u32IPDMA_DataFormat =
		srccap_dev->common_info.color_info.out.data_format;
	cfd_420_control_param.u32Path = srccap_dev->dev_id;
	cfd_420_control_param.bForceUpdate = srccap_dev->common_info.color_info.force_update;
	cfd_420_control_param.interlaced = srccap_dev->timingdetect_info.data.interlaced;

	MApi_PQ_CFD_SetChromaSampling(srccap_dev->common_info.pqmap_info.alg_ctx,
		 &cfd_420_control_param, &cfd_420_control_info);

	SRCCAP_MSG_DEBUG("[CFD][SetChromaSampling](%s = %d, %s = %d)(%s = %d, %s = %d, %s = %d)\n",
		"cfd_420_control_param.u32Input_DataFormat",
		cfd_420_control_param.u32Input_DataFormat,
		"cfd_420_control_param.u32IPDMA_DataFormat",
		cfd_420_control_param.u32IPDMA_DataFormat,
		"u8_420_Input_DataFormat", cfd_420_control_info.u8_420_Input_DataFormat,
		"u8_420_In_IPDMA_DataFormat", cfd_420_control_info.u8_420_In_IPDMA_DataFormat,
		"u8_420_Out_DataFormat", cfd_420_control_info.u8_420_Out_DataFormat);

	/*TODO: set CSC and chroma sampling (SrcInputCSC /InputDomain_420_control)*/

	srccap_dev->common_info.color_info.chroma_sampling_mode =
		cfd_420_control_info.u32ChromaStatus;

	st_cfd_set_csc_in.u8WindowType = srccap_dev->dev_id;
	st_cfd_set_csc_in.u8InputDataFormat = srccap_dev->common_info.color_info.in.data_format;
	st_cfd_set_csc_in.u8InputRange = srccap_dev->common_info.color_info.in.data_range;
	switch (srccap_dev->common_info.color_info.in.matrix_coefficients) {
	case E_PQ_CFD_CFIO_MC_BT709_XVYCC709:
		st_cfd_set_csc_in.u8InputColorSpace = E_CFD_COLORSPACE_BT709;
		break;
	case E_PQ_CFD_CFIO_MC_BT2020CL:
	case E_PQ_CFD_CFIO_MC_BT2020NCL:
		st_cfd_set_csc_in.u8InputColorSpace = E_CFD_COLORSPACE_BT2020;
		break;
	default:
		st_cfd_set_csc_in.u8InputColorSpace = E_CFD_COLORSPACE_BT601;
		break;
	}
	st_cfd_set_csc_in.u8OutputDataFormat = st_cfd_set_csc_in.u8InputDataFormat;
	st_cfd_set_csc_in.u8OutputRange = st_cfd_set_csc_in.u8InputRange;
	st_cfd_set_csc_in.u8OutputColorSpace = st_cfd_set_csc_in.u8InputColorSpace;
	st_cfd_set_csc_in.bForceUpdate = srccap_dev->common_info.color_info.force_update;

	if (srccap_dev->common_info.color_info.swreg_update == TRUE) {
		MApi_PQ_CFD_Source_Domain_GetSWRegCsc(
			srccap_dev->common_info.pqmap_info.alg_ctx,
			&st_cfd_set_csc_in, &st_cfd_set_csc_out);
	}

	SRCCAP_MSG_DEBUG("[CFD](%s=%d, %s=%d, %s=%d, %s=%d, %s=%d, %s=%d %s=%d %s=%d)\n",
		"DataFormat", st_cfd_set_csc_in.u8InputDataFormat,
		"u8InputRange", st_cfd_set_csc_in.u8InputRange,
		"u8InputColorSpace", st_cfd_set_csc_in.u8InputColorSpace,
		"u8OutputDataFormat", st_cfd_set_csc_in.u8OutputDataFormat,
		"u8OutputRange", st_cfd_set_csc_in.u8OutputRange,
		"u8OutputColorSpace", st_cfd_set_csc_in.u8OutputColorSpace,
		"force update", srccap_dev->common_info.color_info.force_update,
		"swreg_update", srccap_dev->common_info.color_info.swreg_update);

	MApi_PQ_CFD_InputDomain_SetInputCsc(
		srccap_dev->common_info.pqmap_info.alg_ctx,
		&st_cfd_set_csc_in, &st_cfd_set_csc_out);

	SRCCAP_ATRACE_END("mtk_common_set_cfd_setting");

	return 0;
}

int mtk_common_subscribe_event_vsync(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum srccap_common_irq_event event = SRCCAP_COMMON_IRQEVENT_MAX;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->streaming == false)
		return -EPERM;

	if (srccap_dev->srccap_info.sts.dqbuf_chg == TRUE)
		return -EPERM;

	if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_0)
		event = SRCCAP_COMMON_IRQEVENT_SW_IRQ_TRIG_SRC0;
	else if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_1)
		event = SRCCAP_COMMON_IRQEVENT_SW_IRQ_TRIG_SRC1;
	else {
		SRCCAP_MSG_ERROR("failed to subscribe event (type, event = %d,%d)\n",
			SRCCAP_COMMON_IRQTYPE_HK, event);
		return -EINVAL;
	}

	ret = mtk_common_attach_irq(srccap_dev, SRCCAP_COMMON_IRQTYPE_HK, event,
		common_vsync_isr,
		(void *)srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to subscribe event (type,event = %d,%d)\n",
			SRCCAP_COMMON_IRQTYPE_HK, event);
		return -EPERM;
	}

	srccap_dev->srccap_info.sts.vsync = TRUE;

	return 0;
}

int mtk_common_unsubscribe_event_vsync(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum srccap_common_irq_event event = SRCCAP_COMMON_IRQEVENT_MAX;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_0) {
		event = SRCCAP_COMMON_IRQEVENT_SW_IRQ_TRIG_SRC0;
	} else if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_1) {
		event = SRCCAP_COMMON_IRQEVENT_SW_IRQ_TRIG_SRC1;
	} else {
		SRCCAP_MSG_ERROR(
		"failed to subscribe event (type,event = %d,%d)\n",
		SRCCAP_COMMON_IRQTYPE_HK,
		event);
		return -EPERM;
	}

	ret = mtk_common_detach_irq(srccap_dev, SRCCAP_COMMON_IRQTYPE_HK, event,
		common_vsync_isr,
		(void *)srccap_dev);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to unsubscribe event %d\n", event);
		return -EPERM;
	}

	srccap_dev->srccap_info.sts.vsync = FALSE;

	return 0;
}

int mtk_common_init_triggergen(struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	struct reg_srccap_common_triggen_input_src_sel src_sel;
	struct reg_srccap_common_triggen_sw_irq_dly sw_irq_dly;
	struct reg_srccap_common_triggen_pq_irq_dly pq_irq_dly;
	struct reg_srccap_common_triggen_sw_vsync_len sw_vsync_len;

	memset(&src_sel, 0, sizeof(struct reg_srccap_common_triggen_input_src_sel));
	memset(&sw_irq_dly, 0, sizeof(struct reg_srccap_common_triggen_sw_irq_dly));
	memset(&pq_irq_dly, 0, sizeof(struct reg_srccap_common_triggen_pq_irq_dly));
	memset(&sw_vsync_len, 0, sizeof(struct reg_srccap_common_triggen_sw_vsync_len));

	if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_0) {
		src_sel.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0;
		src_sel.src = SRCCAP_COMMON_TRIGEN_INPUT_IP0;
		ret = drv_hwreg_srccap_common_triggen_set_inputSrcSel(src_sel, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		sw_irq_dly.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0;
		sw_irq_dly.sw_irq_dly_v = SRCCAP_COMMON_SW_IRQ_DLY;
		sw_irq_dly.swirq_trig_sel = 0;
		ret = drv_hwreg_srccap_common_triggen_set_swIrqTrig(sw_irq_dly, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		pq_irq_dly.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0;
		pq_irq_dly.pq_irq_dly_v = SRCCAP_COMMON_PQ_IRQ_DLY;
		pq_irq_dly.pqirq_trig_sel = 0;
		ret = drv_hwreg_srccap_common_triggen_set_pqIrqTrig(pq_irq_dly, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		sw_vsync_len.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0;
		sw_vsync_len.sw_vsync_len = VSYNC_TRIG_LEN;
		ret = drv_hwreg_srccap_common_triggen_set_swVsyncLen(sw_vsync_len,
									TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

	} else if (srccap_dev->dev_id == SRCCAP_COMMON_DEVICE_1) {
		src_sel.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1;
		src_sel.src = SRCCAP_COMMON_TRIGEN_INPUT_IP1;
		ret = drv_hwreg_srccap_common_triggen_set_inputSrcSel(
			src_sel, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		sw_irq_dly.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1;
		sw_irq_dly.sw_irq_dly_v = SRCCAP_COMMON_SW_IRQ_DLY;
		sw_irq_dly.swirq_trig_sel = 0;
		ret = drv_hwreg_srccap_common_triggen_set_swIrqTrig(sw_irq_dly, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		pq_irq_dly.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1;
		pq_irq_dly.pq_irq_dly_v = SRCCAP_COMMON_PQ_IRQ_DLY;
		pq_irq_dly.pqirq_trig_sel = 0;
		ret = drv_hwreg_srccap_common_triggen_set_pqIrqTrig(pq_irq_dly, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		sw_vsync_len.domain = SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1;
		sw_vsync_len.sw_vsync_len = VSYNC_TRIG_LEN;
		ret = drv_hwreg_srccap_common_triggen_set_swVsyncLen(sw_vsync_len,
									TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

	} else
		SRCCAP_MSG_ERROR("(%s)device id not found\n", srccap_dev->v4l2_dev.name);

	return 0;
}

int mtk_srccap_register_common_subdev(
	struct v4l2_device *v4l2_dev, struct v4l2_subdev *subdev_common,
	struct v4l2_ctrl_handler *common_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(common_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(common_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(common_ctrl_handler,
					&common_ctrl[ctrl_count], NULL);
	}

	ret = common_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create common ctrl handler\n");
		goto exit;
	}
	subdev_common->ctrl_handler = common_ctrl_handler;

	v4l2_subdev_init(subdev_common, &common_sd_ops);
	subdev_common->internal_ops = &common_sd_internal_ops;
	strlcpy(subdev_common->name, "mtk-common",
					sizeof(subdev_common->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_common);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register common subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(common_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_common_subdev(struct v4l2_subdev *subdev_common)
{
	v4l2_ctrl_handler_free(subdev_common->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_common);
}

void mtk_srccap_common_set_hdmi_420to444(struct mtk_srccap_dev *srccap_dev)
{
	bool enable = false;
	u16 dev_id = srccap_dev->dev_id;
	int ret = 0;

	if (srccap_dev->timingdetect_info.data.yuv420 == true)
		enable = true;
	SRCCAP_MSG_DEBUG("yuv420:%d, enable:%d\n",
		srccap_dev->timingdetect_info.data.yuv420, enable);
	ret = drv_hwreg_srccap_common_set_hdmi_420to444(dev_id, enable, true, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

}

int mtk_srccap_common_set_hdmi422pack(struct mtk_srccap_dev *srccap_dev)
{
	bool enable = false;
	u16 dev_id = 0;
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	dev_id = srccap_dev->dev_id;

	if (SRCCAP_IS_HDMI_SRC(srccap_dev->srccap_info.src)
		|| SRCCAP_IS_DVI_SRC(srccap_dev->srccap_info.src)) {
		if (srccap_dev->dv_info.descrb_info.common.hdmi_422_pack_en == true)
			enable = true;
		if ((srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_4
			|| srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_5
			|| srccap_dev->srccap_info.cap.hw_version == SRCCAP_HW_VERSION_6)
			&& srccap_dev->hdmirx_info.color_format == MS_HDMI_COLOR_YUV_422)
			enable = true;
	}
	SRCCAP_MSG_DEBUG("HDMI422pack enable:%d, dv_en=%d, color_fmt=%d\n",
		enable, srccap_dev->dv_info.descrb_info.common.hdmi_422_pack_en,
		srccap_dev->hdmirx_info.color_format);
	ret = drv_hwreg_srccap_common_set_hdmi422pack(dev_id, enable, true, NULL, NULL);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}

int mtk_srccap_common_streamon(struct mtk_srccap_dev *srccap_dev)
{
	srccap_dev->common_info.dqbuf_event_first_frame = TRUE;

	return srccap_common_init_alg_ctx(srccap_dev);
}

int mtk_srccap_common_streamoff(struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev->common_info.dqbuf_event_first_frame = FALSE;

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);
	kfree(srccap_dev->common_info.rm_info.p.node_array);
	srccap_dev->common_info.rm_info.p.node_array = NULL;
	memset(&srccap_dev->fmm_info, 0, sizeof(struct srccap_fmm_info));
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_pqmap);

	return srccap_common_free_alg_ctx(srccap_dev);
}


int mtk_srccap_common_pq_hwreg_init(struct mtk_srccap_dev *srccap_dev)
{
	///void *pCtx = NULL;
	enum EN_PQAPI_RESULT_CODES pq_ret = E_PQAPI_RC_SUCCESS;
	struct ST_PQ_NTS_CAPABILITY_INFO capability_info;
	struct ST_PQ_NTS_CAPABILITY_STATUS capability_status;
	struct ST_PQ_NTS_GETHWREG_INFO get_hwreg_info;
	struct ST_PQ_NTS_GETHWREG_STATUS get_hwreg_status;
	struct ST_PQ_NTS_INITHWREG_INFO init_hwreg_info;
	struct ST_PQ_NTS_INITHWREG_STATUS init_hwreg_status;
	//u32 alg_capability_array[ALG_CAPABILITY_SIZE] = {ALG_CAPABILITY_VERSION, 0x0};

	memset(&capability_info, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_INFO));
	memset(&capability_status, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS));
	memset(&get_hwreg_info, 0, sizeof(struct ST_PQ_NTS_GETHWREG_INFO));
	memset(&get_hwreg_status, 0, sizeof(struct ST_PQ_NTS_GETHWREG_STATUS));
	memset(&init_hwreg_info, 0, sizeof(struct ST_PQ_NTS_INITHWREG_INFO));
	memset(&init_hwreg_status, 0, sizeof(struct ST_PQ_NTS_INITHWREG_STATUS));

	capability_info.u16CapabilitySize = ALG_CAPABILITY_SIZE; /// To-Do: from DTS
	capability_info.pCapability = &srccap_dev->srccap_info.cap.u32AlgCap[0]; /// To-Do: from DTS
	capability_info.eDomain = E_PQ_DOMAIN_SOURCE;
	capability_info.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_INFO);
	capability_info.u32Version = API_VERSION_MAPI_PQ_SETCAPABILITY_NONTS;

	capability_status.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS);
	pq_ret |= MApi_PQ_SetCapability_NonTs(&capability_info, &capability_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		SRCCAP_MSG_ERROR("MApi_PQ_SetCapability_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}

	get_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_GETHWREG_INFO);
	get_hwreg_info.u32Version = API_VERSION_MAPI_PQ_GETHWREGINFO_NONTS;
	get_hwreg_info.eDomain = E_PQ_DOMAIN_SOURCE;

	get_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_GETHWREG_STATUS);

	pq_ret = MApi_PQ_GetHWRegInfo_NonTs(&get_hwreg_info, &get_hwreg_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		SRCCAP_MSG_ERROR("MApi_PQ_GetHWRegInfo_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}

	init_hwreg_info.u32FuncTableSize = get_hwreg_status.u32FuncTableSize;
	init_hwreg_info.pFuncTableAddr = MALLOC(init_hwreg_info.u32FuncTableSize);
	if (init_hwreg_info.pFuncTableAddr == NULL) {
		SRCCAP_MSG_ERROR("[%s] xcalg_kvmalloc failed!\n", __func__);
		return -1;
	}

	init_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_INITHWREG_INFO);
	init_hwreg_info.u32Version = API_VERSION_MAPI_PQ_INITHWREGTABLE_NONTS;
	init_hwreg_info.eDomain = E_PQ_DOMAIN_SOURCE;

	init_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_INITHWREG_STATUS);

	pq_ret =  MApi_PQ_InitHWRegTable_NonTs(&init_hwreg_info, &init_hwreg_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		FREE_MEM(init_hwreg_info.pFuncTableAddr, init_hwreg_info.u32FuncTableSize);
		SRCCAP_MSG_ERROR("MApi_PQ_InitHWRegTable_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}
	srccap_dev->common_info.pqmap_info.hwreg_func_table_addr =
		init_hwreg_info.pFuncTableAddr;
	srccap_dev->common_info.pqmap_info.hwreg_func_table_size =
		init_hwreg_info.u32FuncTableSize;

	return 0;
}

int mtk_srccap_common_pq_hwreg_deinit(struct mtk_srccap_dev *srccap_dev)
{
	///void *pCtx = NULL;
	enum EN_PQAPI_RESULT_CODES pq_ret = E_PQAPI_RC_SUCCESS;
	struct ST_PQ_NTS_DEINITHWREG_INFO deinit_hwreg_info;
	struct ST_PQ_NTS_DEINITHWREG_STATUS deinit_hwreg_status;

	memset(&deinit_hwreg_info, 0, sizeof(struct ST_PQ_NTS_DEINITHWREG_INFO));
	memset(&deinit_hwreg_status, 0, sizeof(struct ST_PQ_NTS_DEINITHWREG_STATUS));

	deinit_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_DEINITHWREG_INFO);
	deinit_hwreg_info.u32Version = API_VERSION_MAPI_PQ_DEINITHWREGTABLE_NONTS;
	deinit_hwreg_info.eDomain = E_PQ_DOMAIN_SOURCE;

	deinit_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_DEINITHWREG_STATUS);
	if (srccap_dev->common_info.pqmap_info.hwreg_func_table_addr) {
		pq_ret =  MApi_PQ_DeInitHWRegTable_NonTs(
					&deinit_hwreg_info, &deinit_hwreg_status);
		if (pq_ret != E_PQAPI_RC_SUCCESS) {
			SRCCAP_MSG_ERROR("MApi_PQ_DeInitHWRegTable_NonTs failed!%d\n", pq_ret);
			return -1;
		}
		FREE_MEM(srccap_dev->common_info.pqmap_info.hwreg_func_table_addr,
			srccap_dev->common_info.pqmap_info.hwreg_func_table_size);
	}

	return 0;
}

int mtk_srccap_common_vd_pq_hwreg_init(struct mtk_srccap_dev *srccap_dev)
{
	///void *pCtx = NULL;
	enum EN_PQAPI_RESULT_CODES pq_ret = E_PQAPI_RC_SUCCESS;
	struct ST_PQ_NTS_CAPABILITY_INFO capability_info;
	struct ST_PQ_NTS_CAPABILITY_STATUS capability_status;
	struct ST_PQ_NTS_GETHWREG_INFO get_hwreg_info;
	struct ST_PQ_NTS_GETHWREG_STATUS get_hwreg_status;
	struct ST_PQ_NTS_INITHWREG_INFO init_hwreg_info;
	struct ST_PQ_NTS_INITHWREG_STATUS init_hwreg_status;
	u32 alg_capability_array[ALG_CAPABILITY_SIZE] = {ALG_CAPABILITY_VERSION, 0x0};

	memset(&capability_info, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_INFO));
	memset(&capability_status, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS));
	memset(&get_hwreg_info, 0, sizeof(struct ST_PQ_NTS_GETHWREG_INFO));
	memset(&get_hwreg_status, 0, sizeof(struct ST_PQ_NTS_GETHWREG_STATUS));
	memset(&init_hwreg_info, 0, sizeof(struct ST_PQ_NTS_INITHWREG_INFO));
	memset(&init_hwreg_status, 0, sizeof(struct ST_PQ_NTS_INITHWREG_STATUS));

	capability_info.u16CapabilitySize = ALG_CAPABILITY_SIZE; /// To-Do: from DTS
	capability_info.pCapability = &alg_capability_array[0]; /// To-Do: from DTS
	capability_info.eDomain = E_PQ_DOMAIN_VD;
	capability_info.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_INFO);
	capability_info.u32Version = API_VERSION_MAPI_PQ_SETCAPABILITY_NONTS;

	capability_status.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS);
	pq_ret = MApi_PQ_SetCapability_NonTs(&capability_info, &capability_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		SRCCAP_MSG_ERROR("MApi_PQ_SetCapability_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}

	get_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_GETHWREG_INFO);
	get_hwreg_info.u32Version = API_VERSION_MAPI_PQ_GETHWREGINFO_NONTS;
	get_hwreg_info.eDomain = E_PQ_DOMAIN_VD;

	get_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_GETHWREG_STATUS);

	pq_ret = MApi_PQ_GetHWRegInfo_NonTs(&get_hwreg_info, &get_hwreg_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		SRCCAP_MSG_ERROR("MApi_PQ_GetHWRegInfo_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}

	init_hwreg_info.u32FuncTableSize = get_hwreg_status.u32FuncTableSize;
	init_hwreg_info.pFuncTableAddr = MALLOC(init_hwreg_info.u32FuncTableSize);
	if (init_hwreg_info.pFuncTableAddr == NULL) {
		SRCCAP_MSG_ERROR("[%s] xcalg_vmalloc failed!\n", __func__);
		return -1;
	}

	init_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_INITHWREG_INFO);
	init_hwreg_info.u32Version = API_VERSION_MAPI_PQ_INITHWREGTABLE_NONTS;
	init_hwreg_info.eDomain = E_PQ_DOMAIN_VD;

	init_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_INITHWREG_STATUS);

	pq_ret =  MApi_PQ_InitHWRegTable_NonTs(&init_hwreg_info, &init_hwreg_status);
	if (pq_ret != E_PQAPI_RC_SUCCESS) {
		FREE_MEM(init_hwreg_info.pFuncTableAddr, init_hwreg_info.u32FuncTableSize);
		SRCCAP_MSG_ERROR("MApi_PQ_InitHWRegTable_NonTs failed! pq_ret: %d\n", pq_ret);
		return -1;
	}
	srccap_dev->common_info.vdpqmap_info.hwreg_func_table_addr =
		init_hwreg_info.pFuncTableAddr;
	srccap_dev->common_info.vdpqmap_info.hwreg_func_table_size =
		init_hwreg_info.u32FuncTableSize;

	return 0;
}

int mtk_srccap_common_vd_pq_hwreg_deinit(struct mtk_srccap_dev *srccap_dev)
{
	///void *pCtx = NULL;
	enum EN_PQAPI_RESULT_CODES pq_ret = E_PQAPI_RC_SUCCESS;
	struct ST_PQ_NTS_DEINITHWREG_INFO deinit_hwreg_info;
	struct ST_PQ_NTS_DEINITHWREG_STATUS deinit_hwreg_status;

	memset(&deinit_hwreg_info, 0, sizeof(struct ST_PQ_NTS_DEINITHWREG_INFO));
	memset(&deinit_hwreg_status, 0, sizeof(struct ST_PQ_NTS_DEINITHWREG_STATUS));

	deinit_hwreg_info.u32Length = sizeof(struct ST_PQ_NTS_DEINITHWREG_INFO);
	deinit_hwreg_info.u32Version = API_VERSION_MAPI_PQ_DEINITHWREGTABLE_NONTS;
	deinit_hwreg_info.eDomain = E_PQ_DOMAIN_VD;

	deinit_hwreg_status.u32Length = sizeof(struct ST_PQ_NTS_DEINITHWREG_INFO);
	if (srccap_dev->common_info.pqmap_info.hwreg_func_table_addr) {
		pq_ret =  MApi_PQ_DeInitHWRegTable_NonTs(
					&deinit_hwreg_info, &deinit_hwreg_status);
		if (pq_ret != E_PQAPI_RC_SUCCESS) {
			SRCCAP_MSG_ERROR("MApi_PQ_DeInitHWRegTable_NonTs failed!%d\n", pq_ret);
			return -1;
		}
		FREE_MEM(srccap_dev->common_info.vdpqmap_info.hwreg_func_table_addr,
			srccap_dev->common_info.vdpqmap_info.hwreg_func_table_size);
	}

	return 0;
}

void mtk_srccap_common_set_stub_mode(u8 stub)
{
	srccapStubMode = stub;
}

bool mtk_srccap_common_is_Cfd_stub_mode(void)
{
	if (srccapStubMode >= SRCCAP_STUB_MODE_CFD_SDR_RGB_RGB
			&& srccapStubMode <= SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV420) {
		return true;
	}
	return false;
}

int mtk_srccap_common_get_cfd_test_HDMI_pkt_HDRIF(struct meta_srccap_hdr_pkt *phdr_pkt)
{
	#define EOTF_HDR 2
	#define EOTF_HLG 3
	phdr_pkt->u8Length = sizeof(struct meta_srccap_hdr_pkt);
	phdr_pkt->u8EOTF = 0;
	if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV
		|| srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV422
		|| srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV420)
		phdr_pkt->u8EOTF = EOTF_HDR;
	else if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HLG_YUV_RGB)
		phdr_pkt->u8EOTF = EOTF_HLG;

	return 1;
}

int mtk_srccap_common_get_cfd_test_HDMI_pkt_AVI(struct hdmi_avi_packet_info *pavi_pkt)
{
	pavi_pkt->enColorRange = M_HDMI_COLOR_RANGE_DEFAULT;
	if (srccapStubMode == SRCCAP_STUB_MODE_CFD_SDR_RGB_RGB
		|| srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV) {
		pavi_pkt->enColorForamt = M_HDMI_COLOR_RGB;
		pavi_pkt->enColormetry = M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr;
	} else if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_VIVID_HDR
		|| srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_VIVID_HLG) {
		pavi_pkt->enColorForamt = M_HDMI_COLOR_YUV_444;
		pavi_pkt->enColormetry = M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr;
	} else if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV422) {
		pavi_pkt->enColorForamt = M_HDMI_COLOR_YUV_422;
		pavi_pkt->enColormetry = M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr;
	} else if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV420) {
		pavi_pkt->enColorForamt = M_HDMI_COLOR_YUV_420;
		pavi_pkt->enColormetry = M_HDMI_EXT_COLORIMETRY_BT2020RGBYCbCr;
	} else {
		pavi_pkt->enColorForamt = M_HDMI_COLOR_YUV_420;
		pavi_pkt->enColormetry = M_HDMI_EXT_COLORIMETRY_BT2020YcCbcCrc;
	}
	return 1;
}

int mtk_srccap_common_get_cfd_test_HDMI_pkt_GCP(struct hdmi_gc_packet_info *gcp_pkt)
{
	gcp_pkt->enColorDepth = M_HDMI_COLOR_DEPTH_UNKNOWN;
	if (srccapStubMode == SRCCAP_STUB_MODE_CFD_SDR_RGB_RGB
		|| srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV) {
		gcp_pkt->enColorDepth = M_HDMI_COLOR_DEPTH_8_BIT;
	} else {
		gcp_pkt->enColorDepth = M_HDMI_COLOR_DEPTH_12_BIT;
	}

	return 1;
}

int mtk_srccap_common_get_cfd_test_HDMI_pkt_VSIF(struct hdmi_vsif_packet_info *vsif_pkt)
{
	#define MAGIC_VSIF_IEEE_HRD10P_LENG 3
	static const u8 MAGIC_VSIF_IEEE_HRD10P[MAGIC_VSIF_IEEE_HRD10P_LENG] = {0x8B, 0x84, 0x90};

	int i = 0;

	if (srccapStubMode != SRCCAP_STUB_MODE_CFD_HDR_RGB_YUV) {
		for (i = 0; i < MAGIC_VSIF_IEEE_HRD10P_LENG; i++)
			vsif_pkt->au8ieee[i] = 0;
	} else {
		for (i = 0; i < MAGIC_VSIF_IEEE_HRD10P_LENG; i++)
			vsif_pkt->au8ieee[i] = MAGIC_VSIF_IEEE_HRD10P[i];
	}
	return 1;
}

int mtk_srccap_common_get_cfd_test_HDMI_pkt_EMP(struct hdmi_emp_packet_info *emp_pkt)
{
	#define MAGIC_EMP_IEEE_HRDVIVID_LENG 3
	static const u8 MAGIC_EMP_IEEE_HDRVIVID[MAGIC_EMP_IEEE_HRDVIVID_LENG] = {0x03, 0x75, 0x04};
	int i = 0;

	if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_VIVID_HDR) {
		for (i = 0; i < MAGIC_EMP_IEEE_HRDVIVID_LENG; i++)
			emp_pkt[0].au8md_first[i] =  MAGIC_EMP_IEEE_HDRVIVID[i];
		emp_pkt[0].au8md_first[14] = 0x00;
	} else if (srccapStubMode == SRCCAP_STUB_MODE_CFD_HDR_VIVID_HLG) {
		for (i = 0; i < MAGIC_EMP_IEEE_HRDVIVID_LENG; i++)
			emp_pkt[0].au8md_first[i] =  MAGIC_EMP_IEEE_HDRVIVID[i];
		emp_pkt[0].au8md_first[14] = 0x80;
	}
	return 1;
}

int mtk_common_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size)
{
	int total_string_size = 0;
	int temp_string_size = 0;
	int ret = 0;
	u16 dev_id = 0;
	bool en = 0;
	u8 mode = 0;
	u8 *dither_mode = NULL;

	if ((srccap_dev == NULL) || (buf == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id >= SRCCAP_DEV_NUM) {
		SRCCAP_MSG_ERROR("Wrong srccap_dev->dev_id = %d\n", srccap_dev->dev_id);
		return -ENXIO;
	}

	dev_id = srccap_dev->dev_id;

	/* get dither enable/disable status */
	ret = drv_hwreg_srccap_common_get_dither(srccap_dev->dev_id, &en);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	temp_string_size = snprintf(buf, max_size,
	"Common Info:\n"
	"Current Source(dev:%d):	%d\n"
	"Dither Status:	%s\n",
	dev_id, srccap_dev->srccap_info.src,
	en ? "ENABLE":"DISABLE");


	if (temp_string_size < 0 || temp_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	total_string_size = temp_string_size;

	if (en == TRUE) {
		/* get dither node if dither enable */
		ret = drv_hwreg_srccap_common_get_dither_mode(srccap_dev->dev_id, &mode);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		switch (mode) {
		case E_SRCCAP_DITHER_OUT_DATA_MODE_DROP:
			dither_mode = "Truncate";
			break;
		case E_SRCCAP_DITHER_OUT_DATA_MODE_ROUNDING:
			dither_mode = "Round off";
			break;
		case E_SRCCAP_DITHER_OUT_DATA_MODE_DITHER:
			dither_mode = "Random carry using LFSR";
			break;
		default:
			dither_mode = "UNKNOWN MODE";
			SRCCAP_MSG_ERROR("Not support Dither out data mode = %d\n", mode);
			break;
		}

		temp_string_size = snprintf(buf + total_string_size, max_size - total_string_size,
		"Dither Mode:	%s (%d)\n"
		"\n", dither_mode, mode);
	} else
		temp_string_size = snprintf(buf + total_string_size, max_size - total_string_size,
		"\n");

	if (temp_string_size < 0 || temp_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	total_string_size += temp_string_size;

	if (total_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	return total_string_size;

}

void mtk_srccap_common_clear_event(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_common_event event)
{
	if ((srccap_dev == NULL) || (event >= E_SRCCAP_COMMON_EVENT_MAX)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev->common_info.event_changed &= ~event;
}

void mtk_srccap_common_set_event(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_common_event event)
{
	if ((srccap_dev == NULL) || (event >= E_SRCCAP_COMMON_EVENT_MAX)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev->common_info.event_changed |= event;
}

void mtk_srccap_common_toggle_event(
	struct mtk_srccap_dev *srccap_dev)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev->common_info.event_toggle = !srccap_dev->common_info.event_toggle;
}

enum m_input_source mtk_srccap_common_get_input_port(
	struct mtk_srccap_dev *srccap_dev)
{
	enum m_input_source input_port = SOURCE_NONE;
	enum v4l2_srccap_input_source src;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		goto exit;
	}

	src = srccap_dev->srccap_info.src;
	if (src < V4L2_SRCCAP_INPUT_SOURCE_NONE ||
		src >= V4L2_SRCCAP_INPUT_SOURCE_NUM) {
		SRCCAP_MSG_ERROR("invalid source %d\n",
			srccap_dev->srccap_info.src);
		goto exit;
	}

	switch (srccap_dev->srccap_info.map[src].data_port) {
	case SRCCAP_INPUT_PORT_HDMI0:
		input_port = SOURCE_HDMI;
		break;
	case SRCCAP_INPUT_PORT_HDMI1:
		input_port = SOURCE_HDMI2;
		break;
	case SRCCAP_INPUT_PORT_HDMI2:
		input_port = SOURCE_HDMI3;
		break;
	case SRCCAP_INPUT_PORT_HDMI3:
		input_port = SOURCE_HDMI4;
		break;
	case SRCCAP_INPUT_PORT_DVI0:
		input_port = SOURCE_DVI;
		break;
	case SRCCAP_INPUT_PORT_DVI1:
		input_port = SOURCE_DVI2;
		break;
	case SRCCAP_INPUT_PORT_DVI2:
		input_port = SOURCE_DVI3;
		break;
	case SRCCAP_INPUT_PORT_DVI3:
		input_port = SOURCE_DVI4;
		break;
	default:
		input_port = SOURCE_NONE;
		break;
	}

	SRCCAP_MSG_DEBUG("src:%d data_port:%d input_port:%d\n",
		srccap_dev->srccap_info.src,
		srccap_dev->srccap_info.map[src].data_port,
		input_port);
exit:
	return input_port;
}

