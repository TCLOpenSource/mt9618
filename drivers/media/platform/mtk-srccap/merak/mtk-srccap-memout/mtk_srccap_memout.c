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
#include "mtk_srccap_memout.h"
#include "hwreg_srccap_memout.h"

#include "mtk_iommu_dtv_api.h"
#include "metadata_utility.h"

#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
#include "mtk_srccap_atrace.h"
static int buf_cnt[SRCCAP_DEV_NUM] = {0};
#endif
#include "mapi_pq_cfd_if.h"
#include "drv_scriptmgt.h"

#define SRCCAP_MEMOUT_PRINT_BUF_ADDR(plane_cnt, entry) \
{ \
	for (plane_cnt = 0; \
		plane_cnt < SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM; \
		plane_cnt++) \
		SRCCAP_MSG_DEBUG(" addr%d=%llu", \
			plane_cnt, \
			entry->addr[plane_cnt]); \
}

#define SRCCAP_MEMOUT_GET_FORMT_FROM_MAP(index) \
	((index >= FMT_NUM) ? fmt_map[0] : fmt_map[index])

#define FMT_NUM        (4)
unsigned int fmt_map[FMT_NUM] = {    //refer to fmt_map_dv/mi_pq_cfd_fmt_map in mi_pq_apply_cfd.c
	V4L2_PIX_FMT_RGB_8_8_8_CE,
	V4L2_PIX_FMT_YUV422_Y_UV_8B_CE,
	V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE,
	V4L2_PIX_FMT_YUV420_Y_UV_8B_CE};
static u8 buf_cnt_id[SRCCAP_DEV_NUM] = {0};
/* ============================================================================================== */
/* -------------------------------------- Local Functions --------------------------------------- */
/* ============================================================================================== */

static int memout_map_fd(int fd, void **va, u64 *size)
{
	struct dma_buf *db = NULL;

	if ((va == NULL) || (size == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (*va != NULL) {
		SRCCAP_MSG_ERROR("occupied va\n");
		return -EINVAL;
	}

	db = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(db)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	*size = db->size;

	*va = dma_buf_vmap(db);
	if (IS_ERR_OR_NULL(*va)) {
		SRCCAP_MSG_ERROR("kernel address not valid\n");
		dma_buf_put(db);
		return -ENXIO;
	}

	dma_buf_put(db);

	return 0;
}

static int memout_unmap_fd(int fd, void *va)
{
	struct dma_buf *db = NULL;

	if (va == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	db = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(db)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	dma_buf_vunmap(db, va);
	dma_buf_put(db);

	return 0;
}

static int memout_get_metaheader(
	enum srccap_memout_metatag meta_tag,
	struct meta_header *meta_header)
{
	if (meta_header == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (meta_tag) {
	case SRCCAP_MEMOUT_METATAG_FRAME_INFO:
		meta_header->tag = META_SRCCAP_FRAME_INFO_TAG;
		meta_header->ver = META_SRCCAP_FRAME_INFO_VERSION;
		meta_header->size = sizeof(struct meta_srccap_frame_info);
		break;
	case SRCCAP_MEMOUT_METATAG_SVP_INFO:
		meta_header->tag = META_SRCCAP_SVP_INFO_TAG;
		meta_header->ver = META_SRCCAP_SVP_INFO_VERSION;
		meta_header->size = sizeof(struct meta_srccap_svp_info);
		break;
	case SRCCAP_MEMOUT_METATAG_COLOR_INFO:
		meta_header->tag = META_SRCCAP_COLOR_INFO_TAG;
		meta_header->ver = META_SRCCAP_COLOR_INFO_VERSION;
		meta_header->size = sizeof(struct meta_srccap_color_info);
		break;
	case SRCCAP_MEMOUT_METATAG_HDR_PKT:
		meta_header->tag = META_SRCCAP_HDR_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDR_PKT_VERSION;
		meta_header->size = sizeof(struct meta_srccap_hdr_pkt);
		break;
	case SRCCAP_MEMOUT_METATAG_HDR_VSIF_PKT:
		meta_header->tag = META_SRCCAP_HDR_VSIF_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDR_VSIF_PKT_VERSION;
		meta_header->size = sizeof(struct meta_srccap_hdr_vsif_pkt);
		break;
	case SRCCAP_MEMOUT_METATAG_HDR_EMP_PKT:
		meta_header->tag = META_SRCCAP_HDR_EMP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDR_EMP_PKT_VERSION;
		meta_header->size = sizeof(struct meta_srccap_hdr_emp_pkt);
		break;
	case SRCCAP_MEMOUT_METATAG_SPD_PKT:
		meta_header->tag = META_SRCCAP_SPD_PKT_TAG;
		meta_header->ver = META_SRCCAP_SPD_PKT_VERSION;
		meta_header->size = sizeof(struct meta_srccap_spd_pkt);
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_AVI_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_AVI_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_AVI_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_avi_packet_info) *
				    META_SRCCAP_HDMIRX_AVI_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_GCP_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_GCP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_GCP_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_gc_packet_info) *
				META_SRCCAP_HDMIRX_GCP_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_SPD_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_SPD_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_SPD_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_packet_info) *
				    META_SRCCAP_HDMIRX_SPD_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_VSIF_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_VSIF_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_VSIF_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_vsif_packet_info) *
				    META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_HDR_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_HDR_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_HDR_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_vsif_packet_info) *
				    META_SRCCAP_HDMIRX_HDR_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_AUI_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_AUI_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_AUI_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_packet_info) *
				    META_SRCCAP_HDMIRX_AUI_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_MPG_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_MPG_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_MPG_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_packet_info) *
				    META_SRCCAP_HDMIRX_MPG_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_VS_EMP_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_VS_EMP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_VS_EMP_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_emp_packet_info) *
				    META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_DSC_EMP_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_DSC_EMP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_DSC_EMP_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_emp_packet_info) *
				    META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_VRR_EMP_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_VRR_EMP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_VRR_EMP_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_emp_packet_info) *
				    META_SRCCAP_HDMIRX_VRR_EMP_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_HDMIRX_DHDR_EMP_PKT:
		meta_header->tag = META_SRCCAP_HDMIRX_DHDR_EMP_PKT_TAG;
		meta_header->ver = META_SRCCAP_HDMIRX_DHDR_EMP_PKT_VERSION;
		meta_header->size =
			1 + sizeof(struct hdmi_emp_packet_info) *
				    META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE;
		break;
	case SRCCAP_MEMOUT_METATAG_DIP_POINT_INFO:
		meta_header->tag = META_SRCCAP_DIP_POINT_INFO_TAG;
		meta_header->ver = META_SRCCAP_DIP_POINT_INFO_VERSION;
		meta_header->size = sizeof(struct meta_srccap_dip_point_info);
		break;
	default:
		SRCCAP_MSG_ERROR("invalid metadata tag:%d\n", meta_tag);
		return -EINVAL;
	}

	return 0;
}

static int memout_map_source_meta(
	enum v4l2_srccap_input_source v4l2_src,
	enum m_input_source *meta_src)
{
	if (meta_src == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (v4l2_src) {
	case V4L2_SRCCAP_INPUT_SOURCE_NONE:
		*meta_src = SOURCE_NONE;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
		*meta_src = SOURCE_HDMI;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
		*meta_src = SOURCE_HDMI2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
		*meta_src = SOURCE_HDMI3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		*meta_src = SOURCE_HDMI4;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
		*meta_src = SOURCE_CVBS;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
		*meta_src = SOURCE_CVBS2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
		*meta_src = SOURCE_CVBS3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
		*meta_src = SOURCE_CVBS4;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
		*meta_src = SOURCE_CVBS5;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
		*meta_src = SOURCE_CVBS6;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
		*meta_src = SOURCE_CVBS7;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		*meta_src = SOURCE_CVBS8;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
		*meta_src = SOURCE_SVIDEO;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
		*meta_src = SOURCE_SVIDEO2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
		*meta_src = SOURCE_SVIDEO3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		*meta_src = SOURCE_SVIDEO4;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
		*meta_src = SOURCE_YPBPR;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
		*meta_src = SOURCE_YPBPR2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		*meta_src = SOURCE_YPBPR3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
		*meta_src = SOURCE_VGA;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
		*meta_src = SOURCE_VGA2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		*meta_src = SOURCE_VGA3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		*meta_src = SOURCE_ATV;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
		*meta_src = SOURCE_SCART;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		*meta_src = SOURCE_SCART2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
		*meta_src = SOURCE_DVI;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
		*meta_src = SOURCE_DVI2;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
		*meta_src = SOURCE_DVI3;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		*meta_src = SOURCE_DVI4;
		break;
	default:
		return -EINVAL;
	}

	if (SRCCAP_FORCE_SVIDEO || SRCCAP_FORCE_SCART)
		*meta_src = SOURCE_CVBS;
	return 0;
}

static int memout_map_path_meta(
	u16 dev_id,
	enum m_memout_path *meta_path)
{
	if (meta_path == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (dev_id) {
	case 0:
		*meta_path = PATH_IPDMA_0;
		break;
	case 1:
		*meta_path = PATH_IPDMA_1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int memout_get_ip_fmt(
	enum v4l2_srccap_input_source src,
	MS_HDMI_COLOR_FORMAT hdmi_fmt,
	enum reg_srccap_memout_format *ip_fmt)
{
	if (ip_fmt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
		*ip_fmt =
			(hdmi_fmt == MS_HDMI_COLOR_RGB) ?
			REG_SRCCAP_MEMOUT_FORMAT_RGB : REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS2:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS3:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS4:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS5:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS6:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS7:
	case V4L2_SRCCAP_INPUT_SOURCE_CVBS8:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO2:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO3:
	case V4L2_SRCCAP_INPUT_SOURCE_SVIDEO4:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;

		if (SRCCAP_FORCE_SVIDEO)
			*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;

		break;
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR2:
	case V4L2_SRCCAP_INPUT_SOURCE_YPBPR3:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_VGA:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA2:
	case V4L2_SRCCAP_INPUT_SOURCE_VGA3:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_ATV:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_SCART:
	case V4L2_SRCCAP_INPUT_SOURCE_SCART2:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		break;
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		*ip_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int memout_set_frame_capturing_parameters(
	u8 dev_id,
	struct reg_srccap_memout_format_conversion fmt_conv,
	bool pack,
	bool argb,
	u8 bitmode[SRCCAP_MAX_PLANE_NUM],
	u8 ce[SRCCAP_MAX_PLANE_NUM],
	struct reg_srccap_memout_resolution res,
	u32 hoffset,
	u32 pitch[SRCCAP_MAX_PLANE_NUM])
{
	int ret = 0;

	ret = drv_hwreg_srccap_memout_set_format(dev_id, fmt_conv, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_pack(dev_id, pack, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_argb(dev_id, argb, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_bit_mode(dev_id, bitmode, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_compression(dev_id, ce, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_input_resolution(dev_id, res, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_line_offset(
		dev_id, hoffset, TRUE, NULL, NULL); // unit: pixel
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_frame_pitch(
		dev_id, pitch, TRUE, NULL, NULL); // unit: n bits, n = align
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int memout_init_buffer_queue(struct mtk_srccap_dev *srccap_dev)
{


	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
	INIT_LIST_HEAD(&srccap_dev->memout_info.frame_q.inputq_head);
	INIT_LIST_HEAD(&srccap_dev->memout_info.frame_q.outputq_head);
	INIT_LIST_HEAD(&srccap_dev->memout_info.frame_q.processingq_head);
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);

	return 0;
}

static void memout_deinit_buffer_queue(struct mtk_srccap_dev *srccap_dev)
{
	struct list_head *list_ptr = NULL, *tmp_ptr = NULL;
	struct srccap_memout_buffer_entry *entry = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
	if ((srccap_dev->memout_info.frame_q.inputq_head.prev == NULL)
		|| (srccap_dev->memout_info.frame_q.inputq_head.next == NULL))
		SRCCAP_MSG_POINTER_CHECK();
	else
		list_for_each_safe(list_ptr, tmp_ptr,
				&srccap_dev->memout_info.frame_q.inputq_head) {
			entry = list_entry(list_ptr, struct srccap_memout_buffer_entry, list);
			list_del(&entry->list);
			kfree(entry);
		}

	if ((srccap_dev->memout_info.frame_q.outputq_head.prev == NULL)
		|| (srccap_dev->memout_info.frame_q.outputq_head.next == NULL))
		SRCCAP_MSG_POINTER_CHECK();
	else
		list_for_each_safe(list_ptr, tmp_ptr,
				&srccap_dev->memout_info.frame_q.outputq_head) {
			entry = list_entry(list_ptr, struct srccap_memout_buffer_entry, list);
			list_del(&entry->list);
			kfree(entry);
		}

	if ((srccap_dev->memout_info.frame_q.processingq_head.prev == NULL)
		|| (srccap_dev->memout_info.frame_q.processingq_head.next == NULL))
		SRCCAP_MSG_POINTER_CHECK();
	else
		list_for_each_safe(list_ptr, tmp_ptr,
				&srccap_dev->memout_info.frame_q.processingq_head) {
			entry = list_entry(list_ptr, struct srccap_memout_buffer_entry, list);
			list_del(&entry->list);
			kfree(entry);
		}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
}

static void memout_add_next_entry(
	struct list_head *head,
	struct srccap_memout_buffer_entry *entry)
{
	if ((head == NULL) || (entry == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	list_add_tail(&entry->list, head);
}

static void memout_add_last_entry(
	struct list_head *head,
	struct srccap_memout_buffer_entry *entry)
{
	if ((head == NULL) || (entry == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	list_add(&entry->list, head);
}

static struct srccap_memout_buffer_entry *memout_delete_last_entry(
	struct list_head *head)
{
	struct srccap_memout_buffer_entry *entry = NULL;

	if ((head == NULL) || (head->next == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return NULL;
	}

	if (head->next == head) {
		SRCCAP_MSG_ERROR("No entries in queue!\n");
		return NULL;
	}

	entry = list_first_entry(head, struct srccap_memout_buffer_entry, list);
	list_del(head->next);

	return entry;
}

static int memout_count_buffer_in_queue(
	struct list_head *head)
{
	struct list_head *list_ptr = NULL, *tmp_ptr = NULL;
	int count = 0;

	if ((head == NULL) ||
		(head->prev == NULL) ||
		(head->next == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return 0;
	}

	list_for_each_safe(list_ptr, tmp_ptr, head) {
		count++;
	}

	return count;
}

static bool memout_check_frame_or_field_appropriate_for_write(
	struct mtk_srccap_dev *srccap_dev,
	u8 src_field)
{
	int ret = 0;
	bool appropriate = FALSE, interlaced = FALSE, enough_buffer = FALSE, write_en = FALSE;
	u8 mode = 0;
	struct list_head *input_q = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	input_q = &srccap_dev->memout_info.frame_q.inputq_head;
	interlaced = srccap_dev->timingdetect_info.data.interlaced;

	if (interlaced) {
		enough_buffer = (memout_count_buffer_in_queue(input_q)
			>= SRCCAP_MEMOUT_MIN_BUF_FOR_CAPTURING_FIELDS);
		ret = drv_hwreg_srccap_memout_get_access(srccap_dev->dev_id, &mode);
		if (ret < 0)
			mode = 0;
		write_en = (mode > 0);

		appropriate = ((enough_buffer && (src_field == 0))
			|| (write_en && (src_field == 1)));
	} else
		appropriate = !list_empty(input_q);

	SRCCAP_MSG_DEBUG("%s:%d, %s:%u, %s:%d, %s:%d, %s:%u\n",
		"inputq_buf_cnt", memout_count_buffer_in_queue(input_q),
		"mode", mode,
		"enough_buffer", enough_buffer,
		"write_en", write_en,
		"src_field", src_field);

	return appropriate;
}

static int memout_prepare_dmabuf(
	struct mtk_srccap_dev *srccap_dev,
	struct vb2_buffer *vb,
	u64 addr[],
	u64 in_size[])
{
	int ret = 0;
	int plane_cnt = 0;
	u8 frame_num = 0;
	u8 num_bufs = 0;
	u8 num_planes = 0;
	u32 align = 0;
	u64 offset = 0;
	unsigned int size = 0;
	bool contiguous = FALSE;
	struct v4l2_memout_pre_calu_size format[SRCCAP_DEV_NUM];
	static unsigned int pre_size[SRCCAP_DEV_NUM];


	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((vb == NULL) || (addr == NULL) || (in_size == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&format, 0, sizeof(struct v4l2_memout_pre_calu_size) * SRCCAP_DEV_NUM);

	switch (srccap_dev->memout_info.buf_ctrl_mode) {
	case V4L2_MEMOUT_SWMODE:
		if (srccap_dev->swmode_buf_type == SRCCAP_SW_SINGLE_BUFFER) /* index control */
			frame_num = mtk_memount_get_frame_num(srccap_dev);
		else /* address control */
			frame_num = 1;
		break;
	case V4L2_MEMOUT_HWMODE:
		frame_num = mtk_memount_get_frame_num(srccap_dev);
		break;
	case V4L2_MEMOUT_BYPASSMODE:
		return 0;
	default:
		SRCCAP_MSG_ERROR("Invalid buffer control mode\n");
		return -EINVAL;
	}

	num_bufs = srccap_dev->memout_info.num_bufs;
	num_planes = srccap_dev->memout_info.num_planes;
	align = srccap_dev->srccap_info.cap.bpw;


	contiguous = num_bufs < num_planes;
	if (contiguous) {
		ret = mtkd_iommu_getiova(vb->planes[0].m.fd, &addr[0], &size);

		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		/* calculate fd size for contiguous planes */
		in_size[0] = size*SRCCAP_BYTE_SIZE;
		if (align != 0)
			do_div(in_size[0], align); // unit: word

		/* calculate frame size if any dscl is happened*/
		if ((srccap_dev->dscl_info.user_dscl_size.output.width !=
			format[srccap_dev->dev_id].width) &&
			(srccap_dev->dscl_info.user_dscl_size.output.height !=
			format[srccap_dev->dev_id].height)) {

			format[srccap_dev->dev_id].width =
					srccap_dev->dscl_info.user_dscl_size.output.width;
			format[srccap_dev->dev_id].height =
					srccap_dev->dscl_info.user_dscl_size.output.height;
			format[srccap_dev->dev_id].color_fmt =
				SRCCAP_MEMOUT_GET_FORMT_FROM_MAP(
					srccap_dev->common_info.color_info.out.data_format);
			format[srccap_dev->dev_id].interlaced =
					srccap_dev->timingdetect_info.data.interlaced;

			ret = mtk_memout_calculate_frame_size(srccap_dev,
				&format[srccap_dev->dev_id], srccap_dev->memout_info.frame_pitch);

			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}

			/* update the new hoffset */
			srccap_dev->memout_info.hoffset = format[srccap_dev->dev_id].width;
			ret = drv_hwreg_srccap_memout_set_line_offset(srccap_dev->dev_id,
					srccap_dev->memout_info.hoffset, TRUE, NULL, NULL);
			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}
		}

		/* calculate frame addresses for contiguous planes */
		offset = 0;
		for (plane_cnt = 0; plane_cnt < num_planes; plane_cnt++) {
			addr[plane_cnt] = addr[0] + offset;
			offset += srccap_dev->memout_info.frame_pitch[plane_cnt] * frame_num;
			SRCCAP_MSG_DEBUG("addr[%d]=%llu frame_pitch =%u size=%u\n",
					plane_cnt, addr[plane_cnt],
					srccap_dev->memout_info.frame_pitch[plane_cnt], size);
		}

		SRCCAP_MSG_DEBUG("Sum_of_frame_pitch = %llu current_fd_size= %u pre_fd_size= %u\n",
					offset, size, pre_size[srccap_dev->dev_id]);

		pre_size[srccap_dev->dev_id] = size;

	} else {
		for (plane_cnt = 0; plane_cnt < num_planes; ++plane_cnt) {
			ret = mtkd_iommu_getiova(vb->planes[plane_cnt].m.fd,
				&addr[plane_cnt], &size);

			if (ret < 0) {
				SRCCAP_MSG_RETURN_CHECK(ret);
				return ret;
			}

			/* calculate fd size for contiguous planes */
			in_size[plane_cnt] = size*SRCCAP_BYTE_SIZE;
			if (align != 0)
				do_div(in_size[plane_cnt], align); // unit: word
		}
	}

	SRCCAP_MSG_DEBUG("num_bufs=%u num_bufs=%u\n",
		srccap_dev->memout_info.num_bufs,
		srccap_dev->memout_info.num_planes);

	/* prepare base address */
	for (plane_cnt = 0; plane_cnt < num_planes; plane_cnt++) {
		addr[plane_cnt] *= SRCCAP_BYTE_SIZE;
		if (align != 0)
			do_div(addr[plane_cnt], align); // unit: word
	}

	return 0;
}

static int memout_check_reg_occupied(
	struct reg_info reg[],
	u16 count,
	bool *occupied)
{
	int index = 0;

	if ((reg == NULL) || (occupied == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	for (index = 0; index < count; index++) {
		if ((reg[index].addr == 0) || (reg[index].mask == 0))
			break;
	}

	*occupied = (index == count);

	return 0;
}

static int memout_write_ml_commands(
	struct reg_info reg[],
	u16 count,
	struct srccap_ml_script_info *ml_script_info)
{
	int ret = 0;
	enum sm_return_type ml_ret = 0;
	bool reg_occupied = FALSE;
	struct sm_ml_direct_write_mem_info ml_direct_write_info;

	if (ml_script_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&ml_direct_write_info, 0, sizeof(struct sm_ml_direct_write_mem_info));

	ret = memout_check_reg_occupied(reg, count, &reg_occupied);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (reg_occupied == FALSE) {
		SRCCAP_MSG_ERROR("No registers to be written\n");
		return -EINVAL;
	}

	ml_direct_write_info.reg = (struct sm_reg *)reg;
	ml_direct_write_info.cmd_cnt = count;
	ml_direct_write_info.va_address = ml_script_info->start_addr
			+ (ml_script_info->total_cmd_count  * SRCCAP_ML_CMD_SIZE);
	ml_direct_write_info.va_max_address = ml_script_info->max_addr;
	ml_direct_write_info.add_32_support = FALSE;

	ml_ret = sm_ml_write_cmd(&ml_direct_write_info);
	if (ml_ret != E_SM_RET_OK)
		SRCCAP_MSG_ERROR("sm_ml_write_cmd fail, ret:%d\n", ml_ret);

	ml_script_info->total_cmd_count += count;

	return 0;
}

static int memout_fire_ml_commands(
	struct reg_info reg[],
	u16 count,
	int ml_fd,
	u8 ml_mem_index,
	struct srccap_ml_script_info *ml_script_info)
{
	int ret = 0;
	enum sm_return_type ml_ret = 0;
	bool reg_occupied = FALSE;
	struct sm_ml_fire_info ml_fire_info;

	if (ml_script_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&ml_fire_info, 0, sizeof(struct sm_ml_fire_info));

	ret = memout_check_reg_occupied(reg, count, &reg_occupied);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (reg_occupied == FALSE) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return -EINVAL;
	}

	ml_fire_info.cmd_cnt = ml_script_info->total_cmd_count;
	ml_fire_info.external = FALSE;
	ml_fire_info.mem_index = ml_mem_index;
	ml_ret = sm_ml_fire(ml_fd, &ml_fire_info);
	if (ml_ret != E_SM_RET_OK)
		SRCCAP_MSG_ERROR("sm_ml_fire fail, ret:%d\n", ml_ret);

	return 0;
}

static void memout_set_sw_mode_settings(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_memout_buffer_entry *entry)
{
	int ret = 0;
	enum sm_return_type ml_ret = E_SM_RET_FAIL;
	u8 ml_mem_index = 0;
	struct reg_info reg[SRCCAP_MEMOUT_REG_NUM] = {{0}};
	u16 count = 0;
	struct srccap_ml_script_info ml_script_info;
	struct sm_ml_info ml_info;
	struct reg_srccap_memout_force_w_setting force_w_setting;
	u64 protect_addr[REG_SRCCAP_MEMOUT_PLANE_NUM] = {0};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	if (entry == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	if (!srccap_dev->streaming) {
		SRCCAP_MSG_ERROR("Not streaming\n");
		return;
	}

	memset(&ml_script_info, 0, sizeof(struct srccap_ml_script_info));
	memset(&ml_info, 0, sizeof(struct sm_ml_info));
	memset(&force_w_setting, 0, sizeof(struct reg_srccap_memout_force_w_setting));

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_ml);

	ml_ret = sm_ml_get_mem_index(srccap_dev->srccap_info.ml_info.fd_memout,
		&ml_mem_index, FALSE);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_MSG_DEBUG("sm_ml_get_mem_index fail, ml_ret:%d\n", ml_ret);
		goto EXIT;
	}

	ml_script_info.instance = srccap_dev->srccap_info.ml_info.fd_memout;
	ml_script_info.mem_index = ml_mem_index;

	ml_ret = sm_ml_get_info(ml_script_info.instance, ml_script_info.mem_index, &ml_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_MSG_ERROR("sm_ml_get_info fail\n");
		goto EXIT;
	}

	ml_script_info.start_addr = ml_info.start_va;
	ml_script_info.max_addr = (u64)((u8 *)ml_info.start_va + ml_info.mem_size);
	ml_script_info.mem_size = ml_info.mem_size;


	/* M6L and newer ver to support this func */
	if (srccap_dev->srccap_info.cap.hw_version != 1) {
		/* save for ipdma limit min address */
		protect_addr[0] = entry->addr[0];
		protect_addr[1] = entry->addr[0] + entry->fd_size;
		/* need to minus 1 for ipdma limit max address to fit whole fd size */
		/* otherwise it will exceed one more unit */
		protect_addr[1]--;

		/* set limit address */
		count = 0;
		ret = drv_hwreg_srccap_memout_set_limit_addr(srccap_dev->dev_id,
			protect_addr, FALSE, reg, &count, TRUE);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return;
		}

		ret = memout_write_ml_commands(reg, count, &ml_script_info);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return;
		}

		ml_script_info.memout_memory_addr_cmd_count += count;
	}

	/* set base address */
	count = 0;
	ret = drv_hwreg_srccap_memout_set_base_addr(srccap_dev->dev_id,
		entry->addr, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ml_script_info.memout_memory_addr_cmd_count += count;

	/* set w index if sw mode not supported by hw */

	count = 0;
	if (srccap_dev->swmode_buf_type == SRCCAP_SW_SINGLE_BUFFER)
		force_w_setting.index = entry->w_index;
	else
		force_w_setting.index = 0;

	force_w_setting.mode = 0;
	force_w_setting.en = TRUE;
	ret = drv_hwreg_srccap_memout_set_force_write(srccap_dev->dev_id,
		force_w_setting, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ml_script_info.memout_memory_addr_cmd_count += count;


	/* set access */
	count = 0;
	ret = drv_hwreg_srccap_memout_set_access(srccap_dev->dev_id,
		srccap_dev->memout_info.access_mode, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ml_script_info.memout_memory_addr_cmd_count += count;

	ret = memout_fire_ml_commands(reg, count,
		srccap_dev->srccap_info.ml_info.fd_memout, ml_mem_index, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}
EXIT:
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_ml);
	return;
}

static int memout_set_hw_mode_settings(
	struct mtk_srccap_dev *srccap_dev,
	u64 addr[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM])
{
	int ret = 0;

	ret = drv_hwreg_srccap_memout_set_base_addr(srccap_dev->dev_id, addr, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_access(srccap_dev->dev_id,
		srccap_dev->memout_info.access_mode, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int memout_disable_access(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;
	enum sm_return_type ml_ret = E_SM_RET_FAIL;
	u8 ml_mem_index = 0;
	struct reg_info reg[SRCCAP_MEMOUT_REG_NUM] = {{0}};
	u16 count = 0;
	struct srccap_ml_script_info ml_script_info;
	struct sm_ml_info ml_info;
	struct reg_srccap_memout_force_w_setting force_w_setting;
	u64 protect_addr[SRCCAP_MEMOUT_REG_NUM] = {0};

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&ml_script_info, 0, sizeof(struct srccap_ml_script_info));
	memset(&ml_info, 0, sizeof(struct sm_ml_info));
	memset(&force_w_setting, 0, sizeof(struct reg_srccap_memout_force_w_setting));

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_ml);
	ml_ret = sm_ml_get_mem_index(srccap_dev->srccap_info.ml_info.fd_memout,
		&ml_mem_index, FALSE);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_MSG_DEBUG("sm_ml_get_mem_index fail, ml_ret:%d\n", ml_ret);
		ret = -EPERM;
		goto EXIT;
	}

	ml_script_info.instance = srccap_dev->srccap_info.ml_info.fd_memout;
	ml_script_info.mem_index = ml_mem_index;

	ml_ret = sm_ml_get_info(ml_script_info.instance, ml_script_info.mem_index, &ml_info);
	if (ml_ret != E_SM_RET_OK) {
		SRCCAP_MSG_ERROR("sm_ml_get_info fail\n");
		ret = -EPERM;
		goto EXIT;
	}

	ml_script_info.start_addr = ml_info.start_va;
	ml_script_info.max_addr = (u64)((u8 *)ml_info.start_va + ml_info.mem_size);
	ml_script_info.mem_size = ml_info.mem_size;

	/* set access */
	count = 0;
	ret = drv_hwreg_srccap_memout_set_access(srccap_dev->dev_id, 0, FALSE, reg, &count);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_write_ml_commands(reg, count, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ml_script_info.memout_memory_addr_cmd_count += count;

	/* set w index if sw mode not supported by hw */
	if (srccap_dev->srccap_info.cap.hw_version == 1) {
		count = 0;
		/* switch index to a number that is never used so field type is not overwritten */
		force_w_setting.index = SRCCAP_MEMOUT_MAX_W_INDEX;
		force_w_setting.mode = 0;
		force_w_setting.en = TRUE;
		ret = drv_hwreg_srccap_memout_set_force_write(srccap_dev->dev_id,
			force_w_setting, FALSE, reg, &count);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ret = memout_write_ml_commands(reg, count, &ml_script_info);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ml_script_info.memout_memory_addr_cmd_count += count;
	}

	if (srccap_dev->srccap_info.cap.hw_version != 1) {
	/* reset limit protect */
		count = 0;
		ret = drv_hwreg_srccap_memout_set_limit_addr(srccap_dev->dev_id,
			protect_addr, FALSE, reg, &count, FALSE);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_ml);
			return ret;
		}

		ret = memout_write_ml_commands(reg, count, &ml_script_info);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_ml);
			return ret;
		}

		ml_script_info.memout_memory_addr_cmd_count += count;
	}

	ret = memout_fire_ml_commands(reg, count,
		srccap_dev->srccap_info.ml_info.fd_memout, ml_mem_index, &ml_script_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

EXIT:
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_ml);
	return ret;
}

static int memout_convert_timestamp_to_us(u32 clk, u32 timestamp, u64 *timestamp_in_us)
{
	if (timestamp_in_us == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	*timestamp_in_us = (u64)timestamp * SRCCAP_USEC_PER_SEC;
	do_div(*timestamp_in_us, clk);

	return 0;
}

static int memout_process_timestamp(
	struct mtk_srccap_dev *srccap_dev,
	u32 *hw_timestamp,
	u64 *timestamp_in_us)
{
	int ret = 0;
	u32 timestamp_report = 0;
	u64 timestamp_us = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* HDMI can get timestamp from hw but others can not */
	if ((srccap_dev->srccap_info.src >= V4L2_SRCCAP_INPUT_SOURCE_HDMI) &&
		(srccap_dev->srccap_info.src <= V4L2_SRCCAP_INPUT_SOURCE_HDMI4)) {
		ret = drv_hwreg_srccap_memout_get_timestamp(srccap_dev->dev_id, &timestamp_report);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = memout_convert_timestamp_to_us(srccap_dev->srccap_info.cap.xtal_clk,
			timestamp_report, &timestamp_us);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		*hw_timestamp = timestamp_report;
		*timestamp_in_us = timestamp_us;

		SRCCAP_MSG_DEBUG("%s:0x%x, %s:0x%llx\n",
			"hw_timestamp", *hw_timestamp,
			"timestamp_in_us", *timestamp_in_us);
	}

	return ret;
}

static int memout_get_cfd_color_format(u8 data_format, enum m_color_format *color_format)
{
	int ret = 0;

	switch (data_format) {
	case E_PQ_CFD_DATA_FORMAT_RGB:
		*color_format = FORMAT_RGB;
		break;
	case E_PQ_CFD_DATA_FORMAT_YUV420:
		*color_format = FORMAT_YUV420;
		break;
	case E_PQ_CFD_DATA_FORMAT_YUV422:
		*color_format = FORMAT_YUV422;
		break;
	case E_PQ_CFD_DATA_FORMAT_YUV444:
		*color_format = FORMAT_YUV444;
		break;
	default:
		SRCCAP_MSG_ERROR("unknoewn cfd data format\n");
		ret = -1;
		break;
	}
	return ret;

}

static int memout_get_dip_point_info(
	struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_dip_point_info *dip_point_info)
{
	int ret = 0;

	dip_point_info->width = srccap_dev->dscl_info.dscl_size.input.width;
	dip_point_info->height = srccap_dev->dscl_info.dscl_size.input.height;
	dip_point_info->crop_align = srccap_dev->srccap_info.cap.crop_align;

	ret = memout_get_cfd_color_format(
		srccap_dev->common_info.color_info.dip_point.data_format,
		&dip_point_info->color_format);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	SRCCAP_MSG_DEBUG("dip point info w:%d, h:%d, crop align:%d, format:%d, cfd format:%d\n",
		dip_point_info->width,
		dip_point_info->height,
		dip_point_info->crop_align,
		dip_point_info->color_format,
		srccap_dev->common_info.color_info.dip_point.data_format);

	return 0;
}

static int memout_get_field_info(struct mtk_srccap_dev *srccap_dev, u8 w_index)
{
	int ret = 0;
	u8 field_report = 0;

	if (srccap_dev == NULL) {

		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = drv_hwreg_srccap_memout_get_field(srccap_dev->dev_id, w_index, &field_report);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	srccap_dev->memout_info.field_info = field_report;
	SRCCAP_MSG_DEBUG("%s:%x\n",
			"field_info", field_report);

	return ret;
}

static void memout_set_frame_queue_done(
		struct mtk_srccap_dev *srccap_dev)
{
	struct list_head *list_ptr = NULL, *tmp_ptr = NULL;
	struct srccap_memout_buffer_entry *entry = NULL;

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
	if ((srccap_dev->memout_info.frame_q.inputq_head.prev == NULL)
		|| (srccap_dev->memout_info.frame_q.inputq_head.next == NULL))
		SRCCAP_MSG_POINTER_CHECK();
	else
		list_for_each_safe(list_ptr, tmp_ptr,
				&srccap_dev->memout_info.frame_q.inputq_head) {
			entry =  memout_delete_last_entry(
				&srccap_dev->memout_info.frame_q.inputq_head);
			memout_add_next_entry(
				&srccap_dev->memout_info.frame_q.processingq_head,
				entry);
		}

	if ((srccap_dev->memout_info.frame_q.processingq_head.prev == NULL)
		|| (srccap_dev->memout_info.frame_q.processingq_head.next == NULL))
		SRCCAP_MSG_POINTER_CHECK();
	else
		list_for_each_safe(list_ptr, tmp_ptr,
				&srccap_dev->memout_info.frame_q.processingq_head) {
			entry =  memout_delete_last_entry(
				&srccap_dev->memout_info.frame_q.processingq_head);
			if (entry != NULL) {
				memout_add_next_entry(
					&srccap_dev->memout_info.frame_q.outputq_head, entry);
				mutex_lock(
					&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
				vb2_buffer_done(entry->vb, VB2_BUF_STATE_DONE);
				mutex_unlock(
					&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
				SRCCAP_MSG_DEBUG("[outq->](%u): vb:%p vb_index:%d stage:%d",
					srccap_dev->dev_id,
					entry->vb, entry->vb->index, entry->stage);
			}
		}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
}

static int memout_execute_callback(struct mtk_srccap_dev *srccap_dev,
	enum srccap_memout_callback_stage stage)
{
	int ret = 0;
	int int_num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_MEMOUT_CB_STAGE_MAX) ||
		(stage < SRCCAP_MEMOUT_CB_STAGE_ENABLE_W_BUF)) {
		SRCCAP_MSG_ERROR("failed to execute callback: %d, (max %d)\n", stage,
		SRCCAP_MEMOUT_CB_STAGE_MAX);
		return -EINVAL;
	}

	for (int_num = 0; int_num < SRCCAP_MEMOUT_MAX_CB_NUM; int_num++) {
		SRCCAP_MEMOUT_INTCB cb =
			srccap_dev->memout_info.cb_info[stage][int_num].cb;
		void *param =
			srccap_dev->memout_info.cb_info[stage][int_num].param;
		if (cb != NULL)
			cb(param);
	}

	return ret;
}

static u8 memout_update_field_info(struct mtk_srccap_dev *srccap_dev, u8 w_index, u8 src_field)
{
	int ret = 0;
	u8 field_report = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return ret;
	}

	//update field info
	ret = memout_get_field_info(srccap_dev, w_index);
	if (ret >= 0) {
		field_report = (srccap_dev->memout_info.field_info == 1) ?
			SRCCAP_MEMOUT_TOP_FIELD : SRCCAP_MEMOUT_BOTTOM_FIELD;
	} else {
		field_report = (src_field == 1) ?
			SRCCAP_MEMOUT_TOP_FIELD : SRCCAP_MEMOUT_BOTTOM_FIELD;
	}

	SRCCAP_MSG_DEBUG("%s:%x field ret:%d\n",
			"field_info", field_report, ret);

	return field_report;
}

static bool memout_check_dscl_condition(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_memout_buffer_entry *entry)
{
	static struct v4l2_dscl_size pre_entry_output[SRCCAP_DEV_NUM];
	static struct v4l2_dscl_size pre_srccap_output[SRCCAP_DEV_NUM];


	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	if (entry == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	SRCCAP_MSG_DEBUG("INPUT info dev:%d\n", srccap_dev->dev_id);
	SRCCAP_MSG_DEBUG("entry->output_width:%u entry->output_width:%u\n",
					entry->output_width, entry->output_height);
	SRCCAP_MSG_DEBUG("src_dev->output_width:%u src_dev->output_width:%u\n",
					srccap_dev->dscl_info.dscl_size.output.width,
					srccap_dev->dscl_info.dscl_size.output.height);

	SRCCAP_MSG_DEBUG("STATIC info dev:%d\n", srccap_dev->dev_id);
	SRCCAP_MSG_DEBUG("pre_entry_output->output_width:%u pre_entry_output->output_width:%u\n",
					pre_entry_output[srccap_dev->dev_id].output.width,
					pre_entry_output[srccap_dev->dev_id].output.height);
	SRCCAP_MSG_DEBUG("pre_srccap_output->output_width:%u pre_srccap_output->output_width:%u\n",
					pre_srccap_output[srccap_dev->dev_id].output.width,
					pre_srccap_output[srccap_dev->dev_id].output.height);

	/* save lastest DSCL output info */
	pre_srccap_output[srccap_dev->dev_id].output.width =
				srccap_dev->dscl_info.dscl_size.output.width;

	pre_srccap_output[srccap_dev->dev_id].output.height =
				srccap_dev->dscl_info.dscl_size.output.height;

	/* Check if h/v change by frame */
	if (entry->output_width != pre_entry_output[srccap_dev->dev_id].output.width ||
		entry->output_height != pre_entry_output[srccap_dev->dev_id].output.height) {

		pre_entry_output[srccap_dev->dev_id].output.width = entry->output_width;
		pre_entry_output[srccap_dev->dev_id].output.height = entry->output_height;

		return TRUE;
	}

	return FALSE;

}

static int memout_enable_req_off(struct mtk_srccap_dev *srccap_dev, bool en)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return FALSE;
	}

	if (srccap_dev->srccap_info.cap.hw_version == 1) {
		//req_off isn't supported in m6e1, so m6e1 use miu mask.
		if (srccap_dev->dev_id) {
			ret = mtk_miu2gmc_mask(SRCCAP_MEMOUT_MIU_PORT_NUM_14, 1, en);
			if (ret) {
				SRCCAP_MSG_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);
				return ret;
			}

			ret = mtk_miu2gmc_mask(SRCCAP_MEMOUT_MIU_PORT_NUM_15, 1, en);
			if (ret) {
				SRCCAP_MSG_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);
				return ret;
			}
		} else {
			ret = mtk_miu2gmc_mask(SRCCAP_MEMOUT_MIU_PORT_NUM_16, 1, en);
			if (ret) {
				SRCCAP_MSG_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);
				return ret;
			}

			ret = mtk_miu2gmc_mask(SRCCAP_MEMOUT_MIU_PORT_NUM_17, 1, en);
			if (ret) {
				SRCCAP_MSG_ERROR("mtk_miu2gmc_mask fail (%d)\n", ret);
				return ret;
			}
		}
	} else {
		ret = drv_hwreg_srccap_memout_set_req_off(srccap_dev->dev_id, en, TRUE, NULL, NULL);
		if (ret) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}
	return ret;
}

int mtk_memout_calculate_frame_size(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_pre_calu_size *format,
	u32 pitch_out[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM])
{
	int plane_cnt = 0;
	int count = 0;
	bool pack = FALSE, argb = FALSE;
	struct reg_srccap_memout_resolution res;
	u8 bitmode_alpha = 0, bitmode[SRCCAP_MAX_PLANE_NUM] = { 0 };
	u8 bitwidth[SRCCAP_MAX_PLANE_NUM] = { 0 };
	u32 hoffset = 0, vsize = 0, align = 0, pitch[SRCCAP_MAX_PLANE_NUM] = { 0 };

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	memset(&res, 0, sizeof(struct reg_srccap_memout_resolution));

	SRCCAP_MSG_DEBUG("width=%d height=%d interlaced=%d color_fmt=%d dv_color_fmt= %d\n",
			format->width, format->height, format->interlaced, format->color_fmt,
			srccap_dev->dv_info.descrb_info.pkt_info.color_format);

	for (count = 0; count < FMT_NUM; count++) {
		if (format->color_fmt == fmt_map[count])
			break;
	}

	if (count == FMT_NUM)
		format->color_fmt = V4L2_PIX_FMT_RGB_8_8_8_CE;

	switch (format->color_fmt) {
	/* ------------------ RGB format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_RGB_8_8_8_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_RGB565:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_5;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_5;
		break;
	case V4L2_PIX_FMT_RGB888:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_RGB101010:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_RGB121212:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		break;
	case V4L2_PIX_FMT_ARGB8888:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_ABGR8888:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_RGBA8888:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_BGRA8888:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_ARGB2101010:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_ABGR2101010:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		pack = 1;
		argb = TRUE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	/* ------------------ YUV format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV444_Y_U_V_12B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_10B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		break;
	/* 3 plane -- uncontiguous planes */
	/* 2 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV422_Y_UV_12B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_10B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_NV16:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_6B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_6B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_10B:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_NV12:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_6B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_6B_CE:
		pack = 0;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_YUV444_YUV_12B:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		break;
	case V4L2_PIX_FMT_YUV444_YUV_10B:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_12B:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_10B:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_YUV_10B:
		pack = 1;
		argb = FALSE;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid memory format\n");
		return -EINVAL;
	}


	/* prepare input resolution */
	res.hsize = format->width;
	res.vsize = (format->interlaced) ?
		(format->height / SRCCAP_INTERLACE_HEIGHT_DIVISOR) :
		format->height;

	/* prepare line offset */
	hoffset = format->width;

	/* prepare frame pitch */
	vsize = (format->interlaced) ?
		(format->height / SRCCAP_INTERLACE_HEIGHT_DIVISOR) :
		format->height;

	align = srccap_dev->srccap_info.cap.bpw;
	if (pack) {
		bitwidth[0] = bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0]
		+ bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] + bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2];
		if (argb)
			bitwidth[0] += bitmode_alpha;
	} else {
		for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++)
			bitwidth[plane_cnt] = bitmode[plane_cnt];
	}

	for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++) {
		pitch[plane_cnt] = SRCCAP_MEMOUT_ALIGN((hoffset * bitwidth[plane_cnt]), align)
			* vsize / align;

		pitch_out[plane_cnt] = (pitch[plane_cnt] * align) / SRCCAP_BYTE_SIZE;
	}

	return 0;

}


/* ============================================================================================== */
/* --------------------------------------- v4l2_ctrl_ops ---------------------------------------- */
/* ============================================================================================== */
static int mtk_memout_set_vflip(
	struct mtk_srccap_dev *srccap_dev,
	bool vflip)
{
	int ret = 0;
	u8 dev_id = (u8)srccap_dev->dev_id;

	SRCCAP_MSG_INFO("[SRCCAP][%s]vflip:%d\n",
		__func__,
		vflip);

	/* set vflip */
	ret = drv_hwreg_srccap_memout_set_vflip(dev_id, vflip, TRUE, NULL, NULL);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	return 0;
}

static int mtk_memout_set_buf_ctrl_mode(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_memout_buf_ctrl_mode mode)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	SRCCAP_MSG_INFO("buf_ctrl_mode:%d\n", mode);

	srccap_dev->memout_info.buf_ctrl_mode = mode;

	return 0;
}

static int mtk_memout_get_timestamp(struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_timeinfo *timeinfo)
{
	int ret = 0;
	u32 hw_timestamp_val = 0;
	u64 timestamp_in_us = 0;
	struct v4l2_memout_timeval ktime;

	memset(&ktime, 0, sizeof(struct v4l2_memout_timeval));

	SRCCAP_GET_TIMESTAMP(&ktime);
	timeinfo->ktimes.tv_sec = ktime.tv_sec;
	timeinfo->ktimes.tv_usec = ktime.tv_usec;

	ret = memout_process_timestamp(srccap_dev, &hw_timestamp_val, &timestamp_in_us);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	timeinfo->timestamp = timestamp_in_us;

	return ret;
}

static int _mtk_memout_get_hdmi_port_by_src(enum v4l2_srccap_input_source src,
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

static int mtk_memout_updata_hdmi_packet_info(struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_hdr_pkt *hdr_pkt,
	struct meta_srccap_hdr_vsif_pkt *hdr_vsif_pkt,
	struct meta_srccap_spd_pkt *spd_pkt)
{
	#define DEBOUNCE_COUNT 2
	#define DEV_ID_CHECK(x) (((x) < SRCCAP_DEV_NUM) ? (x) : 0)

	int ret = 0;
	int hdmi_port = -1;
	u8 devId;
	enum v4l2_srccap_input_source src;
	struct hdmi_vsif_packet_info vsif_data_array[META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE] = {0};
	struct meta_srccap_hdr_vsif_pkt tempVsif_pkt = {0};
	static struct meta_srccap_hdr_vsif_pkt sLastVsif_pkt[SRCCAP_DEV_NUM] = {0};
	static int sHdmi_pkt_deb[SRCCAP_DEV_NUM] = {0};


	if (spd_pkt == NULL || srccap_dev == NULL || hdr_vsif_pkt == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}
	SRCCAP_ATRACE_BEGIN("mtk_memout_updata_hdmi_packet_info");

	src = srccap_dev->srccap_info.src;
	if (_mtk_memout_get_hdmi_port_by_src(src, &hdmi_port)) {
		SRCCAP_MSG_DEBUG("Not HDMI source!!!\n");
	} else {
		if (mtk_srccap_common_is_Cfd_stub_mode()) {
			/*hdr_pkt*/
			ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_HDRIF(hdr_pkt);
			if (ret == 0)
				SRCCAP_MSG_ERROR("failed to mtk_srccap_hdmirx_pkt_get_HDRIF\n");

			/*vsif_pkt*/
			ret = mtk_srccap_common_get_cfd_test_HDMI_pkt_VSIF(&vsif_data_array[0]);
			if (ret == 0)
				SRCCAP_MSG_ERROR("failed to mtk_srccap_hdmirx_pkt_get_VSIF\n");
			else
				mtk_common_parse_vsif_pkt(&vsif_data_array[0], ret, hdr_vsif_pkt);
		} else {
			/*hdr_pkt*/
			ret = mtk_srccap_hdmirx_pkt_get_HDRIF(src,
				(struct hdmi_hdr_packet_info *)hdr_pkt,
				sizeof(struct meta_srccap_hdr_pkt));
			if (ret == 0)
				SRCCAP_MSG_DEBUG("failed to mtk_srccap_hdmirx_pkt_get_HDRIF\n");

			/*hdr_vsif_pkt*/
			ret = mtk_srccap_hdmirx_pkt_get_VSIF(src,
				vsif_data_array, sizeof(struct hdmi_vsif_packet_info)*
				META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);

			devId = DEV_ID_CHECK(srccap_dev->dev_id);
			if (ret == 0) {
				SRCCAP_MSG_DEBUG(
				"failed to mtk_srccap_hdmirx_pkt_get_VSIF\n");

				if (srccap_dev->common_info.color_info.force_update != 0) {
					memset(&(sLastVsif_pkt[devId]), 0,
							sizeof(struct meta_srccap_hdr_vsif_pkt));
					memset(hdr_vsif_pkt, 0,
							sizeof(struct meta_srccap_hdr_vsif_pkt));
					sHdmi_pkt_deb[devId] = 0;
				} else {
					if (sHdmi_pkt_deb[devId] <= DEBOUNCE_COUNT) {
						sHdmi_pkt_deb[devId]++;
						memcpy(hdr_vsif_pkt, &(sLastVsif_pkt[devId]),
							sizeof(struct meta_srccap_hdr_vsif_pkt));
						SRCCAP_MSG_INFO("VSIF in debounce[%d] ",
							sHdmi_pkt_deb[devId]);
						SRCCAP_MSG_INFO("use before %x (%d, %d)\n",
							sLastVsif_pkt[devId].u32IEEECode,
							sLastVsif_pkt[devId].bValid,
							sLastVsif_pkt[devId].u8SourceSideTmoFlag);
					} else {
						memset(hdr_vsif_pkt, 0,
							sizeof(struct meta_srccap_hdr_vsif_pkt));
					}
				}
			} else {
				if (mtk_common_parse_vsif_pkt
					(vsif_data_array, ret, &tempVsif_pkt) != 0) {
					SRCCAP_MSG_ERROR
						("failed to parse vsif pkt\n");
				}

				memcpy(&(sLastVsif_pkt[devId]), &tempVsif_pkt,
					sizeof(struct meta_srccap_hdr_vsif_pkt));
				memcpy(hdr_vsif_pkt, &tempVsif_pkt,
					sizeof(struct meta_srccap_hdr_vsif_pkt));
				sHdmi_pkt_deb[devId] = 0;
			}

			/*spd_pkt*/
			ret = mtk_srccap_hdmirx_pkt_get_spd(src, devId,
				(struct meta_srccap_spd_pkt *)spd_pkt);
			if (ret != 0)
				SRCCAP_MSG_ERROR("failed to mtk_srccap_hdmirx_pkt_get_spd\n");
		}


		if (hdr_vsif_pkt->u32IEEECode == SRCCAP_HDMI_VSIF_PACKET_IEEE_OUI_HRD10_PLUS)
			srccap_dev->common_info.color_info.hdr_type = E_PQ_CFD_HDR_MODE_HDR10PLUS;
								//E_PQ_CFD_HDR_MODE_HDR10PLUS;
	}
	SRCCAP_ATRACE_END("mtk_memout_updata_hdmi_packet_info");
	return 0;
}

static int memout_set_crop(
	struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_frame_info *frame_info)
{
	u32 tmp = 0;
	bool enable = FALSE;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (frame_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	enable = srccap_dev->timingdetect_info.fv_crop.bEnable;
	if ((frame_info->width != 0) && (frame_info->height != 0) && (enable == TRUE)) {
		tmp = srccap_dev->timingdetect_info.fv_crop.CropWinX * frame_info->scaled_width;
		frame_info->crop_x = (u16)(tmp / frame_info->width);

		tmp = srccap_dev->timingdetect_info.fv_crop.CropWinWidth * frame_info->scaled_width;
		frame_info->crop_width = (u16)(tmp / frame_info->width);

		tmp = srccap_dev->timingdetect_info.fv_crop.CropWinY * frame_info->scaled_height;
		frame_info->crop_y = (u16)(tmp / frame_info->height);

		tmp = srccap_dev->timingdetect_info.fv_crop.CropWinHeight
			* frame_info->scaled_height;
		frame_info->crop_height = (u16)(tmp / frame_info->height);
	} else {
		frame_info->crop_x = frame_info->x;
		frame_info->crop_y = frame_info->y;
		frame_info->crop_width = frame_info->scaled_width;
		frame_info->crop_height = frame_info->scaled_height;
	}

	SRCCAP_MSG_DEBUG("fv_crop is %s\n",
		srccap_dev->timingdetect_info.fv_crop.bEnable == 0?"Disable":"Enable");

	return 0;
}

static int _memout_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, memout_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
	{
		SRCCAP_MSG_DEBUG("Get V4L2_CID_MIN_BUFFERS_FOR_CAPTURE\n");

		ret = mtk_memout_get_min_bufs_for_capture(
			srccap_dev, &ctrl->val);
		break;
	}
	case V4L2_CID_MEMOUT_SECURE_SRC_TABLE:
	{

#if IS_ENABLED(CONFIG_OPTEE)
		struct v4l2_memout_sst_info *sstinfo =
			(struct v4l2_memout_sst_info *)(ctrl->p_new.p);

#endif
		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

#if IS_ENABLED(CONFIG_OPTEE)
		SRCCAP_MSG_DEBUG("Get V4L2_CID_MEMOUT_SECURE_SRC_TABLE\n");

		ret = mtk_memout_get_sst(srccap_dev, sstinfo);
#endif
		break;
	}
	case V4L2_CID_MEMOUT_TIME_STAMP:
	{

		struct v4l2_memout_timeinfo *timeinfo =
			(struct v4l2_memout_timeinfo *)(ctrl->p_new.p);

		SRCCAP_MSG_DEBUG("Get V4L2_CID_MEMOUT_TIME_STAMP\n");

		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		ret = mtk_memout_get_timestamp(srccap_dev, timeinfo);
		break;
	}
	case V4L2_CID_MEMOUT_PRE_CALCULATE_SIZE:
	{

		struct v4l2_memout_pre_calu_size *pre_calu_size =
		(struct v4l2_memout_pre_calu_size *)(ctrl->p_new.p_u8);

		SRCCAP_MSG_DEBUG("Get V4L2_CID_MEMOUT_PRE_CAlCULATE_SIZE\n");

		if (ctrl->p_new.p_u8 == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		ret = mtk_memout_g_pre_calculate_size(srccap_dev);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		pre_calu_size->calu_size = ret;
		ret = 0;

		SRCCAP_MSG_DEBUG("MEMOUT_PRE_CALCULATE_SIZE = %d\n", pre_calu_size->calu_size);
		break;
	}

	default:
	{
		ret = -EINVAL;
		break;
	}
	}

	return ret;
}

static int _memout_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	int ret = 0;

	if (ctrl == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev = container_of(ctrl->handler, struct mtk_srccap_dev, memout_ctrl_handler);
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
	{
		bool vflip = (bool)(ctrl->val);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_VFLIP\n");

		ret = mtk_memout_set_vflip(srccap_dev, vflip);
		break;
	}
	case V4L2_CID_MEMOUT_BUF_CTRL_MODE:
	{
		enum v4l2_memout_buf_ctrl_mode buf_ctrl_mode =
			(enum v4l2_memout_buf_ctrl_mode)(ctrl->val);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_MEMOUT_BUF_CTRL_MODE\n");

		ret = mtk_memout_set_buf_ctrl_mode(srccap_dev, buf_ctrl_mode);
		break;
	}
	case V4L2_CID_MEMOUT_SECURE_MODE:
	{
#if IS_ENABLED(CONFIG_OPTEE)
		struct v4l2_memout_secure_info secureinfo;

		if (ctrl->p_new.p == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		secureinfo = *(struct v4l2_memout_secure_info *)(ctrl->p_new.p);

		SRCCAP_MSG_DEBUG("Set V4L2_CID_MEMOUT_SECURE_MODE\n");

		ret = mtk_memout_set_secure_mode(srccap_dev, &secureinfo);
#endif
		break;
	}
	case V4L2_CID_MEMOUT_PRE_CALCULATE_SIZE:
	{

		if (ctrl->p_new.p_u8 == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			return -EINVAL;
		}

		SRCCAP_MSG_DEBUG("Set V4L2_CID_MEMOUT_PRE_CALCULATE_SIZE\n");

		ret = mtk_memout_s_pre_calculate_size(srccap_dev,
			(struct v4l2_memout_pre_calu_size *)(ctrl->p_new.p_u8));

		break;
	}
	default:
	{
		ret = -EINVAL;
		break;
	}
	}

	return ret;
}

static const struct v4l2_ctrl_ops memout_ctrl_ops = {
	.g_volatile_ctrl = _memout_g_ctrl,
	.s_ctrl = _memout_s_ctrl,
};

static const struct v4l2_ctrl_config memout_ctrl[] = {
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "Srccap MemOut V Flip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.def = 0,
		.min = 0,
		.max = 1,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MEMOUT_BUF_CTRL_MODE,
		.name = "Srccap MemOut Buffer Ctrl Mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		.name = "Srccap MemOut Minimum Buffer Number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MEMOUT_SECURE_MODE,
		.name = "Srccap MemOut Security",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_memout_secure_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MEMOUT_SECURE_SRC_TABLE,
		.name = "Srccap MemOut Get SST",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_memout_sst_info)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MEMOUT_TIME_STAMP,
		.name = "Srccap MemOut Get TimeStamp",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_memout_timeinfo)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &memout_ctrl_ops,
		.id = V4L2_CID_MEMOUT_PRE_CALCULATE_SIZE,
		.name = "Srccap MemOut pre calculate the frame size",
		.type = V4L2_CTRL_TYPE_U8,
		.def = 0,
		.min = 0,
		.max = 0xff,
		.step = 1,
		.dims = {sizeof(struct v4l2_memout_pre_calu_size)},
		.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE
			| V4L2_CTRL_FLAG_VOLATILE,
	},
};

/* subdev operations */
static const struct v4l2_subdev_ops memout_sd_ops = {
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops memout_sd_internal_ops = {
};

/* ============================================================================================== */
/* -------------------------------------- Global Functions -------------------------------------- */
/* ============================================================================================== */
int mtk_memout_read_metadata(
	enum srccap_memout_metatag meta_tag,
	void *meta_content,
	struct meta_buffer *buffer)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	struct meta_header header;


	if (meta_content == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&header, 0, sizeof(struct meta_header));

	ret = memout_get_metaheader(meta_tag, &header);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	meta_ret = query_metadata_header(buffer, &header);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata header not found\n");
		ret = -EPERM;
		goto EXIT;
	}

	meta_ret = query_metadata_content(buffer, &header, meta_content);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata content not found\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	return ret;
}

int mtk_memout_write_metadata(
	enum srccap_memout_metatag meta_tag,
	void *meta_content,
	struct meta_buffer *buffer)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	struct meta_header header = {0};

	if (meta_content == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	ret = memout_get_metaheader(meta_tag, &header);
	if (ret) {
		//SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	if (meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_VSIF_PKT) {
		struct meta_srccap_hdmi_pkt *meta_hdmi_pkt =
			(struct meta_srccap_hdmi_pkt *)meta_content;
		header.size = 1 + sizeof(struct hdmi_vsif_packet_info) *
			meta_hdmi_pkt->u8Size;
	}
	if ((meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_VS_EMP_PKT) ||
		(meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_DSC_EMP_PKT) ||
		(meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_DHDR_EMP_PKT)) {
		struct meta_srccap_hdmi_pkt *meta_hdmi_pkt =
			(struct meta_srccap_hdmi_pkt *)meta_content;
		header.size = 1 + sizeof(struct hdmi_emp_packet_info) *
			meta_hdmi_pkt->u8Size;
	}

	meta_ret = attach_metadata(buffer, header, meta_content);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata not attached\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	return ret;
}

int mtk_memout_write_metadata_mmap_fd(
	int meta_fd,
	enum srccap_memout_metatag meta_tag,
	void *meta_content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (meta_content == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = memout_map_fd(meta_fd, &va, &size);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_get_metaheader(meta_tag, &header);
	if (ret) {
		//SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;

	if (meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_VSIF_PKT) {
		struct meta_srccap_hdmi_pkt *meta_hdmi_pkt =
			(struct meta_srccap_hdmi_pkt *)meta_content;
		header.size = 1 + sizeof(struct hdmi_vsif_packet_info) *
			meta_hdmi_pkt->u8Size;
	}
	if ((meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_VS_EMP_PKT) ||
		(meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_DSC_EMP_PKT) ||
		(meta_tag == SRCCAP_MEMOUT_METATAG_HDMIRX_DHDR_EMP_PKT)) {
		struct meta_srccap_hdmi_pkt *meta_hdmi_pkt =
			(struct meta_srccap_hdmi_pkt *)meta_content;
		header.size = 1 + sizeof(struct hdmi_emp_packet_info) *
			meta_hdmi_pkt->u8Size;
	}

	meta_ret = attach_metadata(&buffer, header, meta_content);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata not attached\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	memout_unmap_fd(meta_fd, va);
	return ret;
}


int mtk_memout_remove_metadata(
	int meta_fd,
	enum srccap_memout_metatag meta_tag)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = memout_map_fd(meta_fd, &va, &size);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_get_metaheader(meta_tag, &header);
	if (ret) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;

	meta_ret = delete_metadata(&buffer, &header);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata not deleted\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	memout_unmap_fd(meta_fd, va);
	return ret;
}

void mtk_memout_process_buffer(
	struct mtk_srccap_dev *srccap_dev,
	u8 src_field)
{

	int plane_cnt = 0;
	bool empty_processingq = FALSE;
	struct srccap_memout_buffer_entry *entry = NULL;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	SRCCAP_ATRACE_BEGIN("mtk_memout_process_buffer");
	SRCCAP_ATRACE_INT_FORMAT(srccap_dev->dev_id,
			"w%02u_S:srccap_process_buf", srccap_dev->dev_id);

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
	/* move every completed frame from processing queue to output queue */
	/* stop after reaching last frame(new) or out of frames */
	empty_processingq = list_empty(&srccap_dev->memout_info.frame_q.processingq_head);
	while (!empty_processingq) {
		entry =  memout_delete_last_entry(
			&srccap_dev->memout_info.frame_q.processingq_head);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			SRCCAP_ATRACE_END("mtk_memout_process_buffer");
			SRCCAP_ATRACE_INT_FORMAT(srccap_dev->dev_id,
				"w%02u_E:srccap_process_buf", srccap_dev->dev_id);
			return;
		}

		if (entry->stage == SRCCAP_MEMOUT_BUFFER_STAGE_NEW) {
			entry->stage = SRCCAP_MEMOUT_BUFFER_STAGE_CAPTURING;

			SRCCAP_MSG_DEBUG("[proq->proq](%u): vb:%p vb_index:%d stage:%d, %s:%u",
				srccap_dev->dev_id,
				entry->vb, entry->vb->index, entry->stage,
				"buf_cnt_id", entry->buf_cnt_id);
			SRCCAP_MEMOUT_PRINT_BUF_ADDR(plane_cnt, entry);

			//update field info
			entry->field = memout_update_field_info(srccap_dev, entry->w_index,
										src_field);

			SRCCAP_MSG_DEBUG(" index:%d field:%u\n",
				entry->w_index, entry->field);
			memout_add_last_entry(
				&srccap_dev->memout_info.frame_q.processingq_head,
				entry);

			break;
		} else if (entry->stage == SRCCAP_MEMOUT_BUFFER_STAGE_CAPTURING) {
			entry->stage = SRCCAP_MEMOUT_BUFFER_STAGE_DONE;

			SRCCAP_MSG_DEBUG("[proq->outq](%u): vb:%p vb_index:%d stage:%d, %s:%u",
				srccap_dev->dev_id,
				entry->vb, entry->vb->index, entry->stage,
				"buf_cnt_id", entry->buf_cnt_id);

			SRCCAP_MEMOUT_PRINT_BUF_ADDR(plane_cnt, entry);
			SRCCAP_MSG_DEBUG(" index:%d field:%u\n",
				entry->w_index, entry->field);

			memout_add_next_entry(
				&srccap_dev->memout_info.frame_q.outputq_head,
				entry);
			mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
			vb2_buffer_done(entry->vb, VB2_BUF_STATE_DONE);
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
		} else { /* entry->stage == SRCCAP_MEMOUT_BUFFER_STAGE_DONE */
			SRCCAP_MSG_ERROR("Buffer already done!\n");

			SRCCAP_MSG_DEBUG("[proq->outq](%u): vb:%p vb_index:%d stage:%d, %s:%d",
				srccap_dev->dev_id,
				entry->vb, entry->vb->index, entry->stage,
				"buf_cnt_id", entry->buf_cnt_id);

			//update field info
			entry->field = memout_update_field_info(srccap_dev, entry->w_index,
										src_field);

			SRCCAP_MEMOUT_PRINT_BUF_ADDR(plane_cnt, entry);
			SRCCAP_MSG_DEBUG(" index:%d field:%u\n",
				entry->w_index, entry->field);


			memout_add_next_entry(
				&srccap_dev->memout_info.frame_q.outputq_head,
				entry);
			mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
			vb2_buffer_done(entry->vb, VB2_BUF_STATE_DONE);
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_vb2_buffer_done);
		}

		empty_processingq =
			list_empty(&srccap_dev->memout_info.frame_q.processingq_head);
	}

	/* move a frame or two fields from input queue to processing queue */
	if (memout_check_frame_or_field_appropriate_for_write(srccap_dev, src_field)) {
		if (memout_execute_callback(srccap_dev,
			SRCCAP_MEMOUT_CB_STAGE_ENABLE_W_BUF))
			SRCCAP_MSG_ERROR("fail to callback\n");

		entry =  memout_delete_last_entry(
			&srccap_dev->memout_info.frame_q.inputq_head);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			SRCCAP_ATRACE_END("mtk_memout_process_buffer");
			SRCCAP_ATRACE_INT_FORMAT(srccap_dev->dev_id,
				"w%02u_E:srccap_process_buf", srccap_dev->dev_id);
			return;
		}

		SRCCAP_MSG_DEBUG(
			"[inq->proq](%u): vb:%p vb_index:%d stage:%d, %s:%u",
			srccap_dev->dev_id,
			entry->vb, entry->vb->index, entry->stage,
			"buf_cnt_id", entry->buf_cnt_id);
		for (plane_cnt = 0;
			plane_cnt < SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM; plane_cnt++)
			SRCCAP_MSG_DEBUG(" addr%d=%llu", plane_cnt, entry->addr[plane_cnt]);
		SRCCAP_MSG_DEBUG(" index:%d field:%u\n", entry->w_index, entry->field);

		if (entry->vb != NULL) {
			memout_set_sw_mode_settings(srccap_dev, entry);

			SRCCAP_MSG_DEBUG("entry->output_width:%u entry->output_width:%u\n",
						entry->output_width, entry->output_height);
			SRCCAP_MSG_DEBUG("src_dev->output_width:%u src_dev->output_width:%u\n",
						srccap_dev->dscl_info.dscl_size.output.width,
						srccap_dev->dscl_info.dscl_size.output.height);


			if (memout_check_dscl_condition(srccap_dev, entry))
				mtk_srccap_set_dscl(srccap_dev, entry);

		}

		memout_add_next_entry(
			&srccap_dev->memout_info.frame_q.processingq_head,
			entry);
	} else {
		if (memout_execute_callback(srccap_dev,
			SRCCAP_MEMOUT_CB_STAGE_DISABLE_W_BUF))
			SRCCAP_MSG_ERROR("fail to callback\n");

		if (memout_disable_access(srccap_dev))
			SRCCAP_MSG_ERROR("access off\n");
	}
	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);

	SRCCAP_ATRACE_END("mtk_memout_process_buffer");
	SRCCAP_ATRACE_INT_FORMAT(srccap_dev->dev_id,
		"w%02u_E:srccap_process_buf", srccap_dev->dev_id);
}

int mtk_memout_s_fmt_vid_cap(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	return 0;
}

int mtk_memout_s_fmt_vid_cap_mplane(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format)
{
	int ret = 0;
	int plane_cnt = 0;
	u8 dev_id = 0;
	struct reg_srccap_memout_format_conversion fmt_conv;
	struct reg_srccap_memout_resolution res;
	bool pack = FALSE, argb = FALSE;
	u8 num_planes = 0, num_bufs = 0, access_mode = 0;
	u8 bitmode_alpha = 0, bitmode[SRCCAP_MAX_PLANE_NUM] = { 0 },
	   ce[SRCCAP_MAX_PLANE_NUM] = { 0 };
	u8 bitwidth[SRCCAP_MAX_PLANE_NUM] = { 0 };
	u32 hoffset = 0, vsize = 0, align = 0, pitch[SRCCAP_MAX_PLANE_NUM] = { 0 };

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	dev_id = (u8)srccap_dev->dev_id;
	memset(&fmt_conv, 0, sizeof(struct reg_srccap_memout_format_conversion));
	memset(&res, 0, sizeof(struct reg_srccap_memout_resolution));

	if (SRCCAP_FORCE_VGA)
		format->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_RGB_8_8_8_CE;

	switch (format->fmt.pix_mp.pixelformat) {
	/* ------------------ RGB format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_RGB_8_8_8_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_3;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_1;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_RGB565:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_5;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_5;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_RGB888:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_RGB101010:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_RGB121212:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_ARGB8888:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_ARGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_ABGR8888:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_ABGR;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_RGBA8888:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGBA;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_BGRA8888:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_BGRA;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_ARGB2101010:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_ARGB;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_ABGR2101010:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_ABGR;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_RGBA;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_BGRA;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_RGB;
		argb = TRUE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_2;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	/* ------------------ YUV format ------------------ */
	/* 3 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV444_Y_U_V_12B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_3;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_3;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_3;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV444_Y_U_V_8B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_3;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_1;
		break;
	/* 3 plane -- uncontiguous planes */
	/* 2 plane -- contiguous planes */
	case V4L2_PIX_FMT_YUV422_Y_UV_12B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_NV16:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_8B_6B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_Y_UV_6B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_NV12:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_8B_6B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_8;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_1;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_Y_UV_6B_CE:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_2;
		num_bufs = 1;
		pack = 0;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_6;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_2;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	/* 1 plane */
	case V4L2_PIX_FMT_YUV444_YUV_12B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV444;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV444_YUV_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV444;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_444;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_12B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_12;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV422_YUV_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV422;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV422;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	case V4L2_PIX_FMT_YUV420_YUV_10B:
		num_planes = SRCCAP_MEMOUT_BUF_PLANE_NUM_1;
		num_bufs = 1;
		pack = 1;
		fmt_conv.ipdma_fmt = REG_SRCCAP_MEMOUT_FORMAT_YUV420;
		srccap_dev->common_info.color_info.out.data_format = E_PQ_CFD_DATA_FORMAT_YUV420;
		argb = FALSE;
		access_mode = SRCCAP_MEMOUT_ACCESS_MODE_422;
		bitmode_alpha = SRCCAP_MEMOUT_BITMODE_ALPHA_0;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] = SRCCAP_MEMOUT_BITMODE_VALUE_10;
		bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2] = SRCCAP_MEMOUT_BITMODE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_0] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_1] = SRCCAP_MEMOUT_CE_VALUE_0;
		ce[SRCCAP_MEMOUT_CE_INDEX_2] = SRCCAP_MEMOUT_CE_VALUE_0;
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid memory format\n");
		return -EINVAL;
	}

	/* prepare format */
	ret = memout_get_ip_fmt(srccap_dev->srccap_info.src, srccap_dev->hdmirx_info.color_format,
		&fmt_conv.ip_fmt);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = mtk_common_updata_color_info(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}
	srccap_dev->common_info.color_info.swreg_update = FALSE; //swreg control
	ret = mtk_common_set_cfd_setting(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	fmt_conv.en = (fmt_conv.ip_fmt != fmt_conv.ipdma_fmt) ? TRUE : FALSE;

	/* prepare input resolution */
	srccap_dev->memout_info.streamon_interlaced =
		srccap_dev->timingdetect_info.data.interlaced;
	res.hsize = format->fmt.pix_mp.width;
	res.vsize = (srccap_dev->timingdetect_info.data.interlaced) ?
		(format->fmt.pix_mp.height / SRCCAP_INTERLACE_HEIGHT_DIVISOR) :
		format->fmt.pix_mp.height;

	/* prepare line offset */
	hoffset = format->fmt.pix_mp.width;

	/* prepare frame pitch */
	vsize = (srccap_dev->timingdetect_info.data.interlaced) ?
		(format->fmt.pix_mp.height / SRCCAP_INTERLACE_HEIGHT_DIVISOR) :
		format->fmt.pix_mp.height;
	align = srccap_dev->srccap_info.cap.bpw;
	if (pack) {
		bitwidth[0] = bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_0] +
				bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_1] +
				bitmode[SRCCAP_MEMOUT_BITMODE_INDEX_2];
		if (argb)
			bitwidth[0] += bitmode_alpha;
	} else {
		for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++)
			bitwidth[plane_cnt] = bitmode[plane_cnt];
	}

	for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++)
		pitch[plane_cnt] = SRCCAP_MEMOUT_ALIGN((hoffset * bitwidth[plane_cnt]), align)
			* vsize / align;

	/* set parameters for frame capturing */
	if ((srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_SWMODE) ||
		(srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_HWMODE)) {
		srccap_dev->memout_info.color_fmt_info.fmt
			= (enum srccap_memout_format) fmt_conv.ipdma_fmt;
		srccap_dev->memout_info.color_fmt_info.pack = pack;
		srccap_dev->memout_info.color_fmt_info.argb = argb;
		srccap_dev->memout_info.color_fmt_info.num_planes = num_planes;
		for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++) {
			srccap_dev->memout_info.color_fmt_info.bitmode[plane_cnt]
						= bitmode[plane_cnt];
			srccap_dev->memout_info.color_fmt_info.ce[plane_cnt]
						= ce[plane_cnt];
			SRCCAP_MSG_DEBUG(" plane_cnt:%d, bitmode:%u, ce:%u",
				plane_cnt,
				srccap_dev->memout_info.color_fmt_info.bitmode[plane_cnt],
				srccap_dev->memout_info.color_fmt_info.ce[plane_cnt]);
		}
		SRCCAP_MSG_DEBUG(" fmt:%d, pack:%d, argb:%d",
			srccap_dev->memout_info.color_fmt_info.fmt,
			srccap_dev->memout_info.color_fmt_info.pack,
			srccap_dev->memout_info.color_fmt_info.argb);

		ret = memout_set_frame_capturing_parameters(dev_id, fmt_conv, pack, argb, bitmode,
			ce, res, hoffset, pitch);
	}
	else /* srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE */
		SRCCAP_MSG_INFO("Bypass mode\n");

	/* return buffer number */
	format->fmt.pix_mp.num_planes = num_bufs;

	/* save format info */
	srccap_dev->memout_info.fmt_mp = format->fmt.pix_mp;
	srccap_dev->memout_info.num_planes = num_planes;
	srccap_dev->memout_info.num_bufs = num_bufs;

	for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++) {
		srccap_dev->memout_info.frame_pitch[plane_cnt] =
			pitch[plane_cnt] * align / SRCCAP_BYTE_SIZE; // unit: byte
	}

	srccap_dev->memout_info.access_mode = access_mode;
	srccap_dev->memout_info.hoffset = hoffset;

	return 0;
}

int mtk_memout_s_pre_calculate_size(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_pre_calu_size *format)
{

	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = mtk_memout_calculate_frame_size(
		srccap_dev,
		format,
		srccap_dev->memout_info.pre_calculate_frame_pitch);

	return ret;

}


int mtk_memout_g_fmt_vid_cap(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	return 0;
}

int mtk_memout_g_fmt_vid_cap_mplane(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_format *format)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (format == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	/* load format info */
	format->fmt.pix_mp = srccap_dev->memout_info.fmt_mp;

	return 0;
}


int mtk_memout_g_pre_calculate_size(
	struct mtk_srccap_dev *srccap_dev)
{
	int total_size = 0;
	int count = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	for (count = 0; count < SRCCAP_MAX_PLANE_NUM; count++)
		total_size += srccap_dev->memout_info.pre_calculate_frame_pitch[count];

	SRCCAP_MSG_DEBUG("total_size=%d\n", total_size);

	return total_size;

}

int mtk_memout_reqbufs(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_requestbuffers *req_buf)
{
	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	return 0;
}

int mtk_memout_qbuf(
	struct mtk_srccap_dev *srccap_dev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	int plane_cnt = 0;
	u8 dev_id = 0;
	u64 size[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM] = { 0 };
	u64 addr[SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM] = { 0 };
	struct srccap_memout_buffer_entry *entry = NULL;
	static u8 q_index[SRCCAP_DEV_NUM] = {0};
	s32 min_bufs = 1;



	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (vb == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	dev_id = (u8)srccap_dev->dev_id;

		SRCCAP_ATRACE_BEGIN("mtk_memout_qbuf");
		SRCCAP_ATRACE_INT_FORMAT(vb->index,  "w%02u_i_40-S:srccap_qbuf", dev_id);
		SRCCAP_ATRACE_INT_FORMAT(buf_cnt[dev_id],  "w%02u_srccap_buf_cnt", dev_id);

		srccap_dev->dscl_info.dscl_size.input.width =
			(srccap_dev->timingdetect_info.data.yuv420 == true) ?
			srccap_dev->timingdetect_info.cap_win.width * SRCCAP_YUV420_WIDTH_DIVISOR :
			srccap_dev->timingdetect_info.cap_win.width;

		srccap_dev->dscl_info.dscl_size.input.height =
			srccap_dev->timingdetect_info.cap_win.height;


		srccap_dev->dscl_info.dscl_size.output.width =
			((srccap_dev->dscl_info.user_dscl_size.output.width == 0)
			|| (srccap_dev->dscl_info.user_dscl_size.output.width
				> srccap_dev->dscl_info.dscl_size.input.width)) ?
			srccap_dev->dscl_info.dscl_size.input.width :
			srccap_dev->dscl_info.user_dscl_size.output.width;

		srccap_dev->dscl_info.dscl_size.output.height =
			((srccap_dev->dscl_info.user_dscl_size.output.height == 0)
			|| (srccap_dev->dscl_info.user_dscl_size.output.height
				> srccap_dev->dscl_info.dscl_size.input.height)) ?
			srccap_dev->dscl_info.dscl_size.input.height :
			srccap_dev->dscl_info.user_dscl_size.output.height;

		SRCCAP_MSG_DEBUG("dscl(%u):%s(%u,%u),%s(%u,%u), %s(%u,%u),%s:%d\n",
			srccap_dev->dev_id,
			"input",
			srccap_dev->dscl_info.dscl_size.input.width,
			srccap_dev->dscl_info.dscl_size.input.height,
			"output",
			srccap_dev->dscl_info.dscl_size.output.width,
			srccap_dev->dscl_info.dscl_size.output.height,
			"user_outputsize",
			srccap_dev->dscl_info.user_dscl_size.output.width,
			srccap_dev->dscl_info.user_dscl_size.output.height,
			"yuv420",
			srccap_dev->timingdetect_info.data.yuv420);

	switch (vb->memory) {
	case V4L2_MEMORY_DMABUF:
		ret = memout_prepare_dmabuf(srccap_dev, vb, addr, size);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid memory type\n");
		ret = -EINVAL;
		goto EXIT;
	}

	switch (srccap_dev->memout_info.buf_ctrl_mode) {
	case V4L2_MEMOUT_SWMODE:
		mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
		entry = (struct srccap_memout_buffer_entry *)
			kzalloc(sizeof(struct srccap_memout_buffer_entry), GFP_KERNEL);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			ret = -ENOMEM;
			goto EXIT;
		}
		entry->vb = vb;
		entry->stage = SRCCAP_MEMOUT_BUFFER_STAGE_NEW;
		entry->fd_size = size[0];
		memcpy(&entry->addr, &addr, (sizeof(u64) * SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM));
		entry->output_width = srccap_dev->dscl_info.dscl_size.output.width;
		entry->output_height = srccap_dev->dscl_info.dscl_size.output.height;

		entry->color_fmt_info.num_planes =
			srccap_dev->memout_info.color_fmt_info.num_planes;
		entry->color_fmt_info.fmt = srccap_dev->memout_info.color_fmt_info.fmt;
		entry->color_fmt_info.pack = srccap_dev->memout_info.color_fmt_info.pack;
		entry->color_fmt_info.argb = srccap_dev->memout_info.color_fmt_info.argb;
		for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++) {
			entry->color_fmt_info.bitmode[plane_cnt]
				= srccap_dev->memout_info.color_fmt_info.bitmode[plane_cnt];
			entry->color_fmt_info.ce[plane_cnt]
				= srccap_dev->memout_info.color_fmt_info.ce[plane_cnt];
			SRCCAP_MSG_DEBUG(" plane_cnt:%d, bitmode:%d, ce:%d",
				plane_cnt, entry->color_fmt_info.bitmode[plane_cnt],
				entry->color_fmt_info.ce[plane_cnt]);
		}
		SRCCAP_MSG_DEBUG(" fmt:%d, pack:%d, argb:%d",
			entry->color_fmt_info.fmt,
			entry->color_fmt_info.pack,
			entry->color_fmt_info.argb);

		//use if the frame number change
		q_index[dev_id] = q_index[dev_id] % mtk_memount_get_frame_num(srccap_dev);

		entry->w_index = q_index[dev_id]; // internal index counter
		q_index[dev_id] = (q_index[dev_id] + 1) % mtk_memount_get_frame_num(srccap_dev);

		mtk_memout_get_min_bufs_for_capture(srccap_dev, &min_bufs);
		entry->buf_cnt_id = buf_cnt_id[dev_id];
		buf_cnt_id[dev_id] = (buf_cnt_id[dev_id] + 1) % (u8)min_bufs;

		SRCCAP_MSG_DEBUG("[->inq](%u): vb:%p vb_index:%d stage:%d, buf_cnt_id:%u",
			dev_id,
			entry->vb, entry->vb->index, entry->stage,
			entry->buf_cnt_id);
		for (plane_cnt = 0; plane_cnt < SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM; plane_cnt++)
			SRCCAP_MSG_DEBUG(" addr%d=%llu", plane_cnt, entry->addr[plane_cnt]);
		SRCCAP_MSG_DEBUG(" index:%d\n", entry->w_index);

		memout_add_next_entry(&srccap_dev->memout_info.frame_q.inputq_head, entry);
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);

		break;
	case V4L2_MEMOUT_HWMODE:
		ret = memout_set_hw_mode_settings(srccap_dev, addr);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);

		break;
	case V4L2_MEMOUT_BYPASSMODE:
		mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
		entry = (struct srccap_memout_buffer_entry *)
			kzalloc(sizeof(struct srccap_memout_buffer_entry), GFP_KERNEL);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			ret = -ENOMEM;
			goto EXIT;
		}
		entry->vb = vb;
		memout_add_next_entry(&srccap_dev->memout_info.frame_q.outputq_head, entry);
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid buffer control mode\n");
		ret = -EINVAL;
		goto EXIT;
	}

EXIT:

#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
	if (atrace_enable_srccap == 1) {
		buf_cnt[dev_id]++;
	}
#endif
		SRCCAP_ATRACE_END("mtk_memout_qbuf");
		SRCCAP_ATRACE_INT_FORMAT(vb->index,  "w%02u_i_40-E:srccap_qbuf", dev_id);
		SRCCAP_ATRACE_INT_FORMAT(buf_cnt[dev_id],  "w%02u_srccap_buf_cnt", dev_id);

	return ret;
}

static int
_mtk_memout_write_hdmirxdata(struct mtk_srccap_dev *srccap_dev,
			     struct v4l2_buffer *buf,
			     enum srccap_memout_metatag meta_tag,
			     struct meta_srccap_hdmi_pkt *pmeta_hdmi_pkt,
			     struct meta_buffer *pbuffer)
{
	int ret = 0;

	if (pmeta_hdmi_pkt->u8Size >= 0) {
		ret = mtk_memout_write_metadata(meta_tag, pmeta_hdmi_pkt, pbuffer);
		if (ret < 0) {
			//SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	}

	return 0;
}

static int _mtk_srccap_fill_hdmi_pkt_info(struct mtk_srccap_dev *srccap_dev,
	struct meta_srccap_hdmi_pkt *pmeta_hdmi_pkt,
	struct v4l2_buffer *buf, struct meta_buffer buffer)
{
	int ret = 0;

	if (srccap_dev == NULL || pmeta_hdmi_pkt == NULL || buf == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	ret = _mtk_memout_write_hdmirxdata(
		srccap_dev, buf,
		SRCCAP_MEMOUT_METATAG_HDMIRX_VS_EMP_PKT,
		pmeta_hdmi_pkt, &buffer);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	return ret;
}

static int _mtk_memout_fill_hdmirxdata(struct mtk_srccap_dev *srccap_dev,
				       struct v4l2_buffer *buf, struct meta_buffer buffer)
{
	int ret = 0;
	u16 u16dev_id = 0;
	struct meta_srccap_hdmi_pkt *pmeta_hdmi_pkt = NULL;
	static struct meta_srccap_hdmi_pkt buf_meta_hdmi_pkt[SRCCAP_DEV_NUM];

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	u16dev_id = srccap_dev->dev_id;
	if (u16dev_id >= SRCCAP_DEV_NUM) {
		return -ENODEV;
	}

	switch (srccap_dev->srccap_info.src) {
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI2:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI3:
	case V4L2_SRCCAP_INPUT_SOURCE_HDMI4:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI2:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI3:
	case V4L2_SRCCAP_INPUT_SOURCE_DVI4:
		ret = 0;
		break;
	default:
		ret = ENXIO;
		break;
	}

	if (ret == ENXIO) {
		return -ENXIO;
	}

	if (buf == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (buf->m.planes[0].m.userptr == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	SRCCAP_ATRACE_BEGIN("_mtk_memout_fill_hdmirxdata");
	if (buf->memory == V4L2_MEMORY_DMABUF) {
		pmeta_hdmi_pkt = &buf_meta_hdmi_pkt[u16dev_id];
		if (pmeta_hdmi_pkt == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			SRCCAP_ATRACE_END("_mtk_memout_fill_hdmirxdata");
			return -EPERM;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_avi_packet_info) *
			       META_SRCCAP_HDMIRX_AVI_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_AVI(
			srccap_dev->srccap_info.src,
			(struct hdmi_avi_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_avi_packet_info) *
				META_SRCCAP_HDMIRX_AVI_PKT_MAX_SIZE);

		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "AVI packet", pmeta_hdmi_pkt->u8Size);

		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_AVI_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
			sizeof(struct hdmi_gc_packet_info) *
			META_SRCCAP_HDMIRX_GCP_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_GCP(
			srccap_dev->srccap_info.src,
			(struct hdmi_gc_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_gc_packet_info) *
				META_SRCCAP_HDMIRX_GCP_PKT_MAX_SIZE);

		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
			"HMDI port", srccap_dev->srccap_info.src,
			"GCP packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_GCP_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			//SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_packet_info) *
			       META_SRCCAP_HDMIRX_SPD_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_gnl(
			srccap_dev->srccap_info.src,
			V4L2_EXT_HDMI_PKT_SPD,
			(struct hdmi_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_packet_info) *
				META_SRCCAP_HDMIRX_SPD_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "SPD packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_SPD_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_vsif_packet_info) *
			       META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_VSIF(
			srccap_dev->srccap_info.src,
			(struct hdmi_vsif_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_vsif_packet_info) *
				META_SRCCAP_HDMIRX_VSIF_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "VSIF packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_VSIF_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct meta_srccap_hdr_pkt) *
			       META_SRCCAP_HDMIRX_HDR_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_HDRIF(
			srccap_dev->srccap_info.src,
			(struct hdmi_hdr_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_hdr_packet_info) *
				META_SRCCAP_HDMIRX_HDR_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "HDR_VSIF packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_HDR_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_packet_info) *
			       META_SRCCAP_HDMIRX_AUI_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_gnl(
			srccap_dev->srccap_info.src,
			V4L2_EXT_HDMI_PKT_AUI,
			(struct hdmi_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_packet_info) *
				META_SRCCAP_HDMIRX_AUI_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "AUI packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_AUI_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_packet_info) *
			       META_SRCCAP_HDMIRX_MPG_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_gnl(
			srccap_dev->srccap_info.src,
			V4L2_EXT_HDMI_PKT_MPEG,
			(struct hdmi_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_packet_info) *
				META_SRCCAP_HDMIRX_MPG_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "MPG packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf, SRCCAP_MEMOUT_METATAG_HDMIRX_MPG_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_emp_packet_info) *
			       META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_VS_EMP(
			srccap_dev->srccap_info.src,
			(struct hdmi_emp_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_emp_packet_info) *
				META_SRCCAP_HDMIRX_VS_EMP_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "VS_EMP packet", pmeta_hdmi_pkt->u8Size);

		ret = _mtk_srccap_fill_hdmi_pkt_info(
			srccap_dev, pmeta_hdmi_pkt,
			buf, buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_emp_packet_info) *
			       META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_DSC_EMP(
			srccap_dev->srccap_info.src,
			(struct hdmi_emp_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_emp_packet_info) *
				META_SRCCAP_HDMIRX_DSC_EMP_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "DSC_EMP packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf,
			SRCCAP_MEMOUT_METATAG_HDMIRX_DSC_EMP_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_emp_packet_info) *
			       META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_Dynamic_HDR_EMP(
			srccap_dev->srccap_info.src,
			(struct hdmi_emp_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_emp_packet_info) *
			META_SRCCAP_HDMIRX_DHDR_EMP_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "Dynamic_HDR_EMP packet",
				 pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf,
			SRCCAP_MEMOUT_METATAG_HDMIRX_DHDR_EMP_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		memset(pmeta_hdmi_pkt->u8Data, 0,
		       sizeof(struct hdmi_emp_packet_info) *
			       META_SRCCAP_HDMIRX_VRR_EMP_PKT_MAX_SIZE);

		pmeta_hdmi_pkt->u8Size = mtk_srccap_hdmirx_pkt_get_VRR_EMP(
			srccap_dev->srccap_info.src,
			(struct hdmi_emp_packet_info *)pmeta_hdmi_pkt->u8Data,
			sizeof(struct hdmi_emp_packet_info) *
				META_SRCCAP_HDMIRX_VRR_EMP_PKT_MAX_SIZE);
		SRCCAP_MSG_DEBUG("[CTRL_FLOW]metadata: %s=%d  %s=%d\n",
				 "HMDI port", srccap_dev->srccap_info.src,
				 "VRR EMP packet", pmeta_hdmi_pkt->u8Size);
		ret = _mtk_memout_write_hdmirxdata(
			srccap_dev, buf,
			SRCCAP_MEMOUT_METATAG_HDMIRX_VRR_EMP_PKT,
			pmeta_hdmi_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}
	}
	SRCCAP_ATRACE_END("_mtk_memout_fill_hdmirxdata");
	return 0;

EXIT:
	SRCCAP_ATRACE_END("_mtk_memout_fill_hdmirxdata");
	return ret;
}

int mtk_memout_dqbuf(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	int plane_cnt = 0;
	u32 hw_timestamp = 0;
	u64 timestamp_in_us = 0;
	u16 scaled_width = 0, scaled_height = 0;
	struct meta_srccap_frame_info frame_info = {0};
	struct meta_srccap_color_info color_info = {0};
	static u32 frame_id[SRCCAP_DEV_NUM] = {0};
	struct meta_srccap_hdr_pkt hdr_pkt = {0};
	struct meta_srccap_hdr_vsif_pkt hdr_vsif_pkt = {0};
	struct meta_srccap_spd_pkt spd_pkt = {0};
	u16 dev_id = 0;
	struct meta_srccap_dip_point_info dip_point_info = {0};
	struct srccap_memout_buffer_entry *entry = NULL;
	struct meta_buffer buffer = {0};
	u16 entry_idx = 0;
	bool frame_invalid = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	} else
		dev_id = srccap_dev->dev_id;

	if (buf == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (buf->m.planes[0].m.userptr == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	SRCCAP_ATRACE_BEGIN("mtk_memout_dqbuf");
	SRCCAP_ATRACE_INT_FORMAT(buf->index,  "w%02u_i_42-S:srccap_memout_dqbuf", dev_id);
	SRCCAP_ATRACE_INT_FORMAT(buf_cnt[dev_id],  "w%02u_srccap_buf_cnt", dev_id);

	if (srccap_dev->memout_info.frame_id == 0)
		frame_id[srccap_dev->dev_id] = 0;

	frame_info.frame_id = frame_id[srccap_dev->dev_id]++;
	srccap_dev->memout_info.frame_id = frame_id[srccap_dev->dev_id];

	/* prepare metadata info */
	ret = memout_map_source_meta(srccap_dev->srccap_info.src, &frame_info.source);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = memout_map_path_meta(srccap_dev->dev_id, &frame_info.path);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = drv_hwreg_srccap_memout_get_w_index(srccap_dev->dev_id, &frame_info.w_index);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

#if !defined(CONFIG_MSTAR_ARM_BD_FPGA) || defined(LTP_BUILD)
	ret = mtk_common_updata_frame_color_info(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}
#endif
	srccap_dev->common_info.color_info.swreg_update = TRUE;
	ret = mtk_common_set_cfd_setting(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	/* get timestamp */
	SRCCAP_GET_TIMESTAMP(&buf->timestamp);

	ret = memout_process_timestamp(srccap_dev, &hw_timestamp, &timestamp_in_us);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

#if !defined(CONFIG_MSTAR_ARM_BD_FPGA) || defined(LTP_BUILD)
	ret = mtk_memout_updata_hdmi_packet_info(srccap_dev, &hdr_pkt, &hdr_vsif_pkt, &spd_pkt);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}
#endif

	frame_info.dev_id = srccap_dev->dev_id;
	frame_info.x = srccap_dev->timingdetect_info.data.h_start;
	frame_info.y = srccap_dev->timingdetect_info.data.v_start;
	frame_info.width = srccap_dev->timingdetect_info.data.h_de;
	frame_info.height = srccap_dev->timingdetect_info.data.v_de;
	frame_info.frameratex1000 = srccap_dev->timingdetect_info.data.v_freqx1000;
	frame_info.interlaced = srccap_dev->timingdetect_info.data.interlaced;
	frame_info.h_total = srccap_dev->timingdetect_info.data.h_total;
	frame_info.v_total = srccap_dev->timingdetect_info.data.v_total;
	frame_info.ktimes.tv_sec = buf->timestamp.tv_sec;
	frame_info.ktimes.tv_usec = buf->timestamp.tv_usec;
	frame_info.timestamp = timestamp_in_us;
	frame_info.vrr_type = (enum m_vrr_type)srccap_dev->timingdetect_info.data.vrr_type;
	frame_info.refresh_rate = srccap_dev->timingdetect_info.data.refresh_rate;
	frame_info.vs_pulse_width = srccap_dev->timingdetect_info.data.vs_pulse_width;
	frame_info.fdet_vtt0 = srccap_dev->timingdetect_info.data.fdet_vtt0;
	frame_info.fdet_vtt1 = srccap_dev->timingdetect_info.data.fdet_vtt1;
	frame_info.fdet_vtt2 = srccap_dev->timingdetect_info.data.fdet_vtt2;
	frame_info.fdet_vtt3 = srccap_dev->timingdetect_info.data.fdet_vtt3;

	scaled_width = (srccap_dev->dscl_info.scaled_size_meta.width == 0) ?
		srccap_dev->dscl_info.dscl_size.input.width :
		srccap_dev->dscl_info.scaled_size_meta.width;
	frame_info.scaled_width = scaled_width;

	scaled_height = (srccap_dev->dscl_info.scaled_size_meta.height == 0) ?
		srccap_dev->dscl_info.dscl_size.input.height :
		srccap_dev->dscl_info.scaled_size_meta.height;
	frame_info.scaled_height = scaled_height;

	frame_info.org_size_width = srccap_dev->timingdetect_info.org_size_width;
	frame_info.org_size_height = srccap_dev->timingdetect_info.org_size_height;
	frame_info.vd_video_standard_type
		= (enum srccap_avd_video_standard_type)srccap_dev->avd_info.eStableTvsystem;

	switch (srccap_dev->memout_info.buf_ctrl_mode) {
	case V4L2_MEMOUT_SWMODE:
		mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
		entry =  memout_delete_last_entry(
			&srccap_dev->memout_info.frame_q.outputq_head);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			ret = -ENOMEM;
			goto EXIT;
		}

		SRCCAP_MSG_DEBUG("[outq->](%u): vb:%p vb_index:%d stage:%d",
			srccap_dev->dev_id,
			entry->vb, entry->vb->index, entry->stage);
		for (plane_cnt = 0; plane_cnt < SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM; plane_cnt++)
			SRCCAP_MSG_DEBUG(" addr%d=%llu", plane_cnt, entry->addr[plane_cnt]);
		SRCCAP_MSG_DEBUG(" index:%d field:%u\n", entry->w_index, entry->field);


		if (srccap_dev->swmode_buf_type == SRCCAP_SW_SINGLE_BUFFER) {
			frame_info.w_index = entry->w_index;
			frame_info.sw_buffer_mode = SRCCAP_SW_SINGLE_BUFFER;
		} else {
			frame_info.w_index = 0;
			frame_info.sw_buffer_mode = SRCCAP_SW_MULTI_BUFFER;
		}

		frame_info.field = entry->field;

		scaled_width = entry->output_width;
		frame_info.scaled_width = scaled_width;

		scaled_height = entry->output_height;
		frame_info.scaled_height = scaled_height;

		entry_idx = (u16)entry->vb->index;

		frame_info.color_fmt_info.num_planes = entry->color_fmt_info.num_planes;
		frame_info.color_fmt_info.fmt = (enum m_memout_format) entry->color_fmt_info.fmt;
		frame_info.color_fmt_info.pack = entry->color_fmt_info.pack;
		frame_info.color_fmt_info.argb = entry->color_fmt_info.argb;

		for (plane_cnt = 0; plane_cnt < SRCCAP_MAX_PLANE_NUM; plane_cnt++) {
			frame_info.color_fmt_info.bitmode[plane_cnt]
				= entry->color_fmt_info.bitmode[plane_cnt];
			frame_info.color_fmt_info.ce[plane_cnt]
				= entry->color_fmt_info.ce[plane_cnt];
		}

		frame_info.buf_cnt_id = entry->buf_cnt_id;
		frame_invalid = (entry->stage == SRCCAP_MEMOUT_BUFFER_STAGE_DONE) ? 0 : 1;
		kfree(entry);
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);

		break;
	case V4L2_MEMOUT_HWMODE:
		frame_info.w_index = (srccap_dev->memout_info.first_frame == true) ?
			(frame_info.w_index + mtk_memount_get_frame_num(srccap_dev) -
			SRCCAP_MEMOUT_ONE_INDEX) % mtk_memount_get_frame_num(srccap_dev) :
			(frame_info.w_index + mtk_memount_get_frame_num(srccap_dev) -
			SRCCAP_MEMOUT_TWO_INDEX) % mtk_memount_get_frame_num(srccap_dev);

		/* get field info */
		memout_get_field_info(srccap_dev, frame_info.w_index);
		frame_info.field = srccap_dev->memout_info.field_info;

		break;
	case V4L2_MEMOUT_BYPASSMODE:
		mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
		entry =  memout_delete_last_entry(
			&srccap_dev->memout_info.frame_q.outputq_head);
		if (entry == NULL) {
			SRCCAP_MSG_POINTER_CHECK();
			mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);
			ret = -ENOMEM;
			goto EXIT;
		}
		entry_idx = (u16)entry->vb->index;
		kfree(entry);
		mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_buffer_q);

		frame_info.w_index = 0;

		break;
	default:
		frame_info.w_index = 0;

		break;
	}

	ret = memout_set_crop(srccap_dev, &frame_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	SRCCAP_MSG_DEBUG("first_frame:%d, frame_info.w_index:%u\n",
		srccap_dev->memout_info.first_frame,
		frame_info.w_index);

	/* DV */
	frame_info.dv_crc_mute = srccap_dev->dv_info.descrb_info.buf.neg_ctrl;
	frame_info.dv_game_mode = srccap_dev->dv_info.descrb_info.common.dv_game_mode;
	/* Input port for PQ domain */
	frame_info.input_port = mtk_srccap_common_get_input_port(srccap_dev);

	srccap_dev->memout_info.first_frame = false;
	srccap_dev->dscl_info.scaled_size_meta.width = srccap_dev->dscl_info.scaled_size_ml.width;
	srccap_dev->dscl_info.scaled_size_meta.height = srccap_dev->dscl_info.scaled_size_ml.height;

	if (srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_SWMODE) {
		srccap_dev->dscl_info.scaled_size_ml.width = scaled_width;
		srccap_dev->dscl_info.scaled_size_ml.height = scaled_height;
	} else {
		srccap_dev->dscl_info.scaled_size_ml.width =
				srccap_dev->dscl_info.dscl_size.output.width;
		srccap_dev->dscl_info.scaled_size_ml.height =
				srccap_dev->dscl_info.dscl_size.output.height;
	}
	memcpy(frame_info.frame_pitch, srccap_dev->memout_info.frame_pitch,
				sizeof(u32)*SRCCAP_MEMOUT_MAX_BUF_PLANE_NUM);
	frame_info.hoffset = srccap_dev->memout_info.hoffset;

	/* FMM */
	frame_info.isFMM = srccap_dev->fmm_info.isFMM;

	ret = memout_get_dip_point_info(srccap_dev, &dip_point_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = mtk_common_get_cfd_metadata(srccap_dev, &color_info);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	SRCCAP_MSG_DEBUG("dscl menuload size(%d, %d), tmp meta size:(%d, %d)\n",
		srccap_dev->dscl_info.scaled_size_ml.width,
		srccap_dev->dscl_info.scaled_size_ml.height,
		srccap_dev->dscl_info.scaled_size_meta.width,
		srccap_dev->dscl_info.scaled_size_meta.height);


	buffer.paddr = (unsigned char *)srccap_dev->va_addr[entry_idx];
	buffer.size = (unsigned int)srccap_dev->va_size[entry_idx];

	/* write metadata */
	switch (buf->memory) {
	case V4L2_MEMORY_DMABUF:
		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_FRAME_INFO, &frame_info, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_COLOR_INFO, &color_info, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_HDR_PKT, &hdr_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_HDR_VSIF_PKT, &hdr_vsif_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}
		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_SPD_PKT, &spd_pkt, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}

		ret = mtk_memout_write_metadata(
			SRCCAP_MEMOUT_METATAG_DIP_POINT_INFO, &dip_point_info, &buffer);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			goto EXIT;
		}
		break;
	default:
		SRCCAP_MSG_ERROR("Invalid memory type\n");
		ret = -EINVAL;
		goto EXIT;
	}

	srccap_dev->common_info.color_info.force_update = false;

	if (frame_invalid)
		ret = -EIO;

	SRCCAP_MSG_DEBUG("[CTRL_FLOW]%s(%d): %s=%d, %s=%d, %s=%d,"
		"%s=%u, %s=%d, %s=%u, %s=%u, %s=%d, %s=%d, %s=%d, %s=%u, "
		"%s=[%u, %u, %u], %s=[%u, %u, %u]\n",
		"metadata", dev_id,
		"source", frame_info.source,
		"path", frame_info.path,
		"bypass_ipdma", frame_info.bypass_ipdma,
		"w_index", frame_info.w_index,
		"format", frame_info.format,
		"x", frame_info.x,
		"y", frame_info.y,
		"fmt", frame_info.color_fmt_info.fmt,
		"pack", frame_info.color_fmt_info.pack,
		"argb", frame_info.color_fmt_info.argb,
		"num_planes", frame_info.color_fmt_info.num_planes,
		"bitmode", frame_info.color_fmt_info.bitmode[SRCCAP_MEMOUT_BUF_PLANE_NUM_0],
		frame_info.color_fmt_info.bitmode[SRCCAP_MEMOUT_BUF_PLANE_NUM_1],
		frame_info.color_fmt_info.bitmode[SRCCAP_MEMOUT_BUF_PLANE_NUM_2],
		"ce", frame_info.color_fmt_info.ce[SRCCAP_MEMOUT_BUF_PLANE_NUM_0],
		frame_info.color_fmt_info.ce[SRCCAP_MEMOUT_BUF_PLANE_NUM_1],
		frame_info.color_fmt_info.ce[SRCCAP_MEMOUT_BUF_PLANE_NUM_2]);

	SRCCAP_MSG_DEBUG("%s=%u, %s=%u, %s=%u, %s=%d, %s=%u, %s=%u, %s=%u, %s=%llu, %s=%d, %s=%d\n",
		"width", frame_info.width,
		"height", frame_info.height,
		"frameratex1000", frame_info.frameratex1000,
		"interlaced", frame_info.interlaced,
		"h_total", frame_info.h_total,
		"v_total", frame_info.v_total,
		"pixels_aligned", frame_info.pixels_aligned,
		"timestamp", frame_info.timestamp,
		"vrr_type", frame_info.vrr_type,
		"refresh_rate", frame_info.refresh_rate);

	SRCCAP_MSG_DEBUG("%s=%d %s=%u, %s=%u, %s=%llu.%llu, %s=%u, %s=%u, %s=%d\n",
		"sw_buffer_mode", frame_info.sw_buffer_mode,
		"scaled_width", frame_info.scaled_width,
		"scaled_height", frame_info.scaled_height,
		"ktimes", frame_info.ktimes.tv_sec, frame_info.ktimes.tv_usec,
		"org_size_width", frame_info.org_size_width,
		"org_size_height", frame_info.org_size_height,
		"vd_video_standard_type", frame_info.vd_video_standard_type);

	SRCCAP_MSG_DEBUG("%s=%u %s=%u, %s=%u, %s=%u, %s:%u, %s:%u, %s:%d, %s:%u\n",
		"crop_x", frame_info.crop_x,
		"crop_y", frame_info.crop_y,
		"crop_width", frame_info.crop_width,
		"crop_height", frame_info.crop_height,
		"frame id", frame_info.frame_id,
		"buf_cnt_id", frame_info.buf_cnt_id,
		"frame_invalid", frame_invalid,
		"dev_id", frame_info.dev_id);

	SRCCAP_MSG_DEBUG("%s:%d, %s:%d\n",
		"isFMM", frame_info.isFMM,
		"freesync", spd_pkt.freesync_type);

#if !defined(CONFIG_MSTAR_ARM_BD_FPGA) || defined(LTP_BUILD)
	_mtk_memout_fill_hdmirxdata(srccap_dev, buf, buffer);
#endif


EXIT:
#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
	if (atrace_enable_srccap == 1) {
		if (buf_cnt[dev_id] > 0)
			buf_cnt[dev_id]--;
	}
#endif
	SRCCAP_ATRACE_END("mtk_memout_dqbuf");
	SRCCAP_ATRACE_INT_FORMAT(buf->index,  "w%02u_i_42-E:srccap_memout_dqbuf", dev_id);
	SRCCAP_ATRACE_INT_FORMAT(buf_cnt[dev_id],  "w%02u_srccap_buf_cnt", dev_id);
	return ret;
}

int mtk_memout_streamon(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_buf_type type)
{
	int ret = 0;
	u8 dev_id = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	dev_id = (u8)srccap_dev->dev_id;

	ret = memout_init_buffer_queue(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_frame_num(dev_id,
		mtk_memount_get_frame_num(srccap_dev), TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	if (srccap_dev->swmode_buf_type == SRCCAP_SW_SINGLE_BUFFER) {
		drv_hwreg_srccap_memout_set_rw_diff(dev_id, SRCCAP_MEMOUT_RW_DIFF,
								TRUE, NULL, NULL);
	} else {
		drv_hwreg_srccap_memout_set_rw_diff(dev_id, 0, TRUE, NULL, NULL);
	}

	if (srccap_dev->memout_info.buf_ctrl_mode == V4L2_MEMOUT_BYPASSMODE) {
		ret = drv_hwreg_srccap_memout_set_access(dev_id, 0, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = drv_hwreg_srccap_memout_set_idr_access(dev_id, 0, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = drv_hwreg_srccap_memout_set_output_select(dev_id,
					SRCCAP_MEMOUT_OUTPUT_SELECT_IDW0, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}
	} else {
		ret = drv_hwreg_srccap_memout_set_access(dev_id, 0, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}

		ret = drv_hwreg_srccap_memout_set_output_select(dev_id,
					SRCCAP_MEMOUT_OUTPUT_SELECT_IDR, TRUE, NULL, NULL);
		if (ret < 0) {
			SRCCAP_MSG_RETURN_CHECK(ret);
			return ret;
		}


	}

	ret = memout_enable_req_off(srccap_dev, FALSE);
	if (ret < 0) {
		SRCCAP_MSG_ERROR("memout_enable_req_off fail, ret :%d\n", ret);
		return ret;
	}

	srccap_dev->memout_info.first_frame = true;
	srccap_dev->common_info.color_info.force_update = true;

	return 0;
}

int mtk_memout_streamoff(
	struct mtk_srccap_dev *srccap_dev,
	enum v4l2_buf_type type)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	srccap_dev->memout_info.frame_id = 0;
	srccap_dev->memout_info.first_frame = true;
	buf_cnt_id[srccap_dev->dev_id] = 0;
	ret = memout_enable_req_off(srccap_dev, TRUE);
	if (ret < 0) {
		SRCCAP_MSG_ERROR("memout_enable_req_off fail, ret :%d\n", ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_set_access(srccap_dev->dev_id, 0, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_reset_ipdma(srccap_dev->dev_id, TRUE, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = drv_hwreg_srccap_memout_reset_ipdma(srccap_dev->dev_id, FALSE, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	ret = mtk_memout_release_buffer(srccap_dev);
	if (ret < 0) {
		SRCCAP_MSG_ERROR("mtk_memout_release_buffer fail, ret :%d\n", ret);
		return ret;
	}

	memout_deinit_buffer_queue(srccap_dev);

	return 0;
}

int mtk_memout_release_buffer(
	struct mtk_srccap_dev *srccap_dev)
{
	int ret = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->memout_info.buf_ctrl_mode != V4L2_MEMOUT_SWMODE)
		return 0;

	ret = drv_hwreg_srccap_memout_set_access(srccap_dev->dev_id, 0, TRUE, NULL, NULL);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		return ret;
	}

	memout_set_frame_queue_done(srccap_dev);

	return 0;
}

void mtk_memout_vsync_isr(
	void *param)
{
	struct mtk_srccap_dev *srccap_dev = NULL;
	spinlock_t *spinlock_buffer_handling_task = NULL;
	unsigned long flags = 0;

	if (param == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}

	srccap_dev = (struct mtk_srccap_dev *)param;

	spinlock_buffer_handling_task =
		&srccap_dev->srccap_info.spinlock_list.spinlock_buffer_handling_task;

	spin_lock_irqsave(spinlock_buffer_handling_task, flags);
	if (srccap_dev->srccap_info.buffer_handling_task != NULL) {
		srccap_dev->srccap_info.waitq_list.buffer_done = 1;
		wake_up_interruptible(&srccap_dev->srccap_info.waitq_list.waitq_buffer);
	}
	spin_unlock_irqrestore(spinlock_buffer_handling_task, flags);
}

int mtk_srccap_register_memout_subdev(
	struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *subdev_memout,
	struct v4l2_ctrl_handler *memout_ctrl_handler)
{
	int ret = 0;
	u32 ctrl_count;
	u32 ctrl_num = sizeof(memout_ctrl)/sizeof(struct v4l2_ctrl_config);

	v4l2_ctrl_handler_init(memout_ctrl_handler, ctrl_num);
	for (ctrl_count = 0; ctrl_count < ctrl_num; ctrl_count++) {
		v4l2_ctrl_new_custom(memout_ctrl_handler,
			&memout_ctrl[ctrl_count], NULL);
	}

	ret = memout_ctrl_handler->error;
	if (ret) {
		SRCCAP_MSG_ERROR("failed to create memout ctrl handler\n");
		goto exit;
	}
	subdev_memout->ctrl_handler = memout_ctrl_handler;

	v4l2_subdev_init(subdev_memout, &memout_sd_ops);
	subdev_memout->internal_ops = &memout_sd_internal_ops;
	strlcpy(subdev_memout->name, "mtk-memout", sizeof(subdev_memout->name));

	ret = v4l2_device_register_subdev(v4l2_dev, subdev_memout);
	if (ret) {
		SRCCAP_MSG_ERROR("failed to register memout subdev\n");
		goto exit;
	}

	return 0;

exit:
	v4l2_ctrl_handler_free(memout_ctrl_handler);
	return ret;
}

void mtk_srccap_unregister_memout_subdev(struct v4l2_subdev *subdev_memout)
{
	v4l2_ctrl_handler_free(subdev_memout->ctrl_handler);
	v4l2_device_unregister_subdev(subdev_memout);
}

void mtk_memout_set_capture_size(
	struct mtk_srccap_dev *srccap_dev,
	struct srccap_ml_script_info *ml_script_info,
	struct v4l2_area_size *capture_size)
{
	int ret = 0;
	u16 count = 0;
	u32 width = 0, height = 0;
	struct reg_info reg[SRCCAP_MEMOUT_REG_NUM] = {{0}};
	bool stub = 0;
	struct sm_ml_direct_write_mem_info ml_direct_write_info;

	if ((srccap_dev == NULL) || (ml_script_info == NULL) || (capture_size == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return;
	}
	SRCCAP_ATRACE_BEGIN("mtk_memout_set_capture_size");

	memset(&ml_direct_write_info, 0, sizeof(struct sm_ml_direct_write_mem_info));

		SRCCAP_MSG_DEBUG("capture_size:(%u, %u), count:%d\n",
			capture_size->width,
			capture_size->height,
			count);

	width = capture_size->width;
	height = (srccap_dev->memout_info.streamon_interlaced == TRUE) ?
		capture_size->height / SRCCAP_INTERLACE_HEIGHT_DIVISOR :
		capture_size->height;

	ret = drv_hwreg_srccap_memout_set_ipdmaw_size(srccap_dev->dev_id,
					false,
					width,
					height,
					reg, &count);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	ret = drv_hwreg_common_get_stub(&stub);
	if (ret < 0)
		SRCCAP_MSG_RETURN_CHECK(ret);

	ml_script_info->memout_capture_size_cmd_count = count;

	if (stub == 0) {
		//3.add cmd
		ml_direct_write_info.reg = (struct sm_reg *)&reg;
		ml_direct_write_info.cmd_cnt = count;
		ml_direct_write_info.va_address = ml_script_info->start_addr
				+ (ml_script_info->total_cmd_count  * SRCCAP_ML_CMD_SIZE);
		ml_direct_write_info.va_max_address = ml_script_info->max_addr;
		ml_direct_write_info.add_32_support = FALSE;

		SRCCAP_MSG_DEBUG("ml_write_info: cmd_count: auto_gen:%d, dscl:%d, ipdmaw:%d\n",
			ml_script_info->auto_gen_cmd_count,
			ml_script_info->dscl_cmd_count,
			count);
		SRCCAP_MSG_DEBUG("ml_write_info: va_address:0x%llx,  va_max_address:0x%llx\n",
			ml_direct_write_info.va_address,
			ml_direct_write_info.va_max_address);

		ret = sm_ml_write_cmd(&ml_direct_write_info);
		if (ret != E_SM_RET_OK)
			SRCCAP_MSG_ERROR("sm_ml_write_cmd fail, ret:%d\n", ret);
	}
	ml_script_info->total_cmd_count = ml_script_info->total_cmd_count
					+ ml_script_info->memout_capture_size_cmd_count;
	SRCCAP_ATRACE_END("mtk_memout_set_capture_size");
}

int mtk_memout_register_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_memout_callback_stage stage,
	SRCCAP_MEMOUT_INTCB intcb,
	void *param)
{
	int ret = 0;
	int int_num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_MEMOUT_CB_STAGE_MAX) ||
		(stage < SRCCAP_MEMOUT_CB_STAGE_ENABLE_W_BUF)) {
		SRCCAP_MSG_ERROR("failed to register callback: %d, (max %d)\n", stage,
		SRCCAP_MEMOUT_CB_STAGE_MAX);
		return -EINVAL;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_memout_cb_info);

	for (int_num = 0; int_num < SRCCAP_MEMOUT_MAX_CB_NUM; int_num++) {
		if (srccap_dev->memout_info.cb_info[stage][int_num].cb == NULL) {
			srccap_dev->memout_info.cb_info[stage][int_num].param = param;
			srccap_dev->memout_info.cb_info[stage][int_num].cb = intcb;
			break;
		}
	}

	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_memout_cb_info);

	return ret;
}

int mtk_memout_unregister_callback(
	struct mtk_srccap_dev *srccap_dev,
	enum srccap_memout_callback_stage stage,
	SRCCAP_MEMOUT_INTCB intcb,
	void *param)
{
	int ret = 0;
	int int_num = 0;

	if (srccap_dev == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if ((stage >= SRCCAP_MEMOUT_CB_STAGE_MAX) ||
		(stage < SRCCAP_MEMOUT_CB_STAGE_ENABLE_W_BUF)) {
		SRCCAP_MSG_ERROR("failed to unregister callback: %d, (max %d)\n", stage,
		SRCCAP_MEMOUT_CB_STAGE_MAX);
		return -EINVAL;
	}

	mutex_lock(&srccap_dev->srccap_info.mutex_list.mutex_memout_cb_info);

	for (int_num = 0; int_num < SRCCAP_MEMOUT_MAX_CB_NUM; int_num++) {
		if (srccap_dev->memout_info.cb_info[stage][int_num].cb == intcb) {
			srccap_dev->memout_info.cb_info[stage][int_num].param = NULL;
			srccap_dev->memout_info.cb_info[stage][int_num].cb = NULL;
			break;
		}
	}

	mutex_unlock(&srccap_dev->srccap_info.mutex_list.mutex_memout_cb_info);

	return ret;
}

void mtk_memount_set_frame_num(
	struct mtk_srccap_dev *srccap_dev)
{
	u64 level = 0;
	u16 h_de = 0;
	u16 v_de = 0;
	u32 max_level = 0;
	bool interlaced = 0;

	h_de = srccap_dev->timingdetect_info.data.h_de;
	v_de = srccap_dev->timingdetect_info.data.v_de;
	interlaced = srccap_dev->timingdetect_info.data.interlaced;
	level = (h_de * v_de) >> interlaced;

	/* m6:1 m6l:2 */
	if (srccap_dev->srccap_info.cap.hw_version == 1)
		max_level = SRCCAP_MEMOUT_8K_RESOLUTION;
	else
		max_level = SRCCAP_MEMOUT_4K_RESOLUTION;

	if (max_level == level)
		srccap_dev->memout_info.frame_num = SRCCAP_MEMOUT_FRAME_NUM_10;
	else if ((max_level > level) && (level >= SRCCAP_MEMOUT_1080P_RESOLUTION))
		srccap_dev->memout_info.frame_num = SRCCAP_MEMOUT_FRAME_NUM_12;
	else if ((level < SRCCAP_MEMOUT_1080P_RESOLUTION) &&
		(level > SRCCAP_MEMOUT_1080I_RESOLUTION))
		srccap_dev->memout_info.frame_num = SRCCAP_MEMOUT_FRAME_NUM_12;
	else
		srccap_dev->memout_info.frame_num = SRCCAP_MEMOUT_FRAME_NUM_14;

	if (srccap_dev->memout_info.buf_ctrl_mode != V4L2_MEMOUT_HWMODE) {
		if (srccap_dev->swmode_buf_type == SRCCAP_SW_MULTI_BUFFER)
			srccap_dev->memout_info.frame_num = 1;
	}

	SRCCAP_MSG_DEBUG("h_de:%d v_de:%d interlaced:%d max_level:%d frame_num:%d\n",
		h_de, v_de, interlaced, max_level, srccap_dev->memout_info.frame_num);
}

u8 mtk_memount_get_frame_num(struct mtk_srccap_dev *srccap_dev)
{
	SRCCAP_MSG_DEBUG("frame_num:%d\n", srccap_dev->memout_info.frame_num);
	return srccap_dev->memout_info.frame_num;
}

int mtk_memout_get_min_bufs_for_capture(
	struct mtk_srccap_dev *srccap_dev,
	s32 *min_bufs)
{
	if (min_bufs == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (srccap_dev->swmode_buf_type == SRCCAP_SW_SINGLE_BUFFER) /* index control */
		*min_bufs = mtk_memount_get_frame_num(srccap_dev);
	else /* address control */
		*min_bufs = SRCCAP_MEMOUT_FRAME_NUM_5;

	return 0;
}

int mtk_memout_report_info(
	struct mtk_srccap_dev *srccap_dev,
	char *buf,
	const u16 max_size)
{
	int ret = 0, count = 0;
	int total_string_size = 0;
	int inpq_num[SRCCAP_MEMOUT_TWO_INDEX + SRCCAP_MEMOUT_ONE_INDEX] = {0};
	int proq_num[SRCCAP_MEMOUT_TWO_INDEX + SRCCAP_MEMOUT_ONE_INDEX] = {0};
	int outq_num[SRCCAP_MEMOUT_TWO_INDEX + SRCCAP_MEMOUT_ONE_INDEX] = {0};
	u8 qbuf_cnt_id[SRCCAP_MEMOUT_TWO_INDEX + SRCCAP_MEMOUT_ONE_INDEX] = {0};

	u16 dev_id = 0;
	s32 min_bufs = 0;

	if ((srccap_dev == NULL) || (buf == NULL)) {
		SRCCAP_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	if (srccap_dev->dev_id >= SRCCAP_DEV_NUM) {
		SRCCAP_MSG_ERROR("Wrong srccap_dev->dev_id = %d\n", srccap_dev->dev_id);
		return -ENXIO;
	}

	ret = mtk_memout_get_min_bufs_for_capture(srccap_dev, &min_bufs);

	if (ret < 0) {
		SRCCAP_MSG_ERROR(" dev(%d): mtk_memout_get_min_bufs_for_capture fail\n",
										srccap_dev->dev_id);
		return -ENXIO;
	}

	dev_id = srccap_dev->dev_id;

	for (count = 0; count < SRCCAP_MEMOUT_TWO_INDEX + SRCCAP_MEMOUT_ONE_INDEX; count++) {
		inpq_num[count] = memout_count_buffer_in_queue(
					 &(srccap_dev->memout_info.frame_q.inputq_head));
		proq_num[count] = memout_count_buffer_in_queue(
					&(srccap_dev->memout_info.frame_q.processingq_head));
		outq_num[count] = memout_count_buffer_in_queue(
					&(srccap_dev->memout_info.frame_q.outputq_head));

		qbuf_cnt_id[count] = buf_cnt_id[dev_id];

		usleep_range(SRCCAP_TASK_SLEEP_4000, SRCCAP_TASK_SLEEP_4100); // sleep 4ms
		usleep_range(SRCCAP_TASK_SLEEP_4000, SRCCAP_TASK_SLEEP_4100); // sleep 4ms
		usleep_range(SRCCAP_TASK_SLEEP_4000, SRCCAP_TASK_SLEEP_4100); // sleep 4ms
		usleep_range(SRCCAP_TASK_SLEEP_4000, SRCCAP_TASK_SLEEP_4100); // sleep 4ms
	}

	total_string_size = snprintf(buf, max_size,
	"Memout Info:\n"
	"Current Source(dev:%d):	%d\n"
	"color_fmt from DV:	(%d)\n"
	"pixelformat:	(%d)\n"
	"pixel height:	(%d)\n"
	"pixel width:	(%d)\n"
	"num_planes:	(%d)\n"
	"num_bufs:	(%d)\n"
	"access_mode:	(%d)\n"
	"hoffset:	(%d)\n"
	"frame_pitch:	(0x%x 0x%x 0x%x)\n"
	"buf ctrl mode:	(%d)\n"
	"min frame num:	(%d)\n"
	"buf_cnt_id:	(%d %d %d)\n"
	"inpq_num:	(%d %d %d)\n"
	"proq_num:	(%d %d %d)\n"
	"outq_num:	(%d %d %d)\n"
	"\n",
	dev_id, srccap_dev->srccap_info.src,
	srccap_dev->dv_info.descrb_info.pkt_info.color_format,
	srccap_dev->memout_info.fmt_mp.pixelformat,
	srccap_dev->memout_info.fmt_mp.height,
	srccap_dev->memout_info.fmt_mp.width,
	srccap_dev->memout_info.num_planes,
	srccap_dev->memout_info.num_bufs,
	srccap_dev->memout_info.access_mode,
	srccap_dev->memout_info.hoffset,
	srccap_dev->memout_info.frame_pitch[0],
	srccap_dev->memout_info.frame_pitch[1],
	srccap_dev->memout_info.frame_pitch[2],
	srccap_dev->memout_info.buf_ctrl_mode,
	min_bufs,
	qbuf_cnt_id[0], qbuf_cnt_id[1], qbuf_cnt_id[2],
	inpq_num[0], inpq_num[1], inpq_num[2],
	proq_num[0], proq_num[1], proq_num[2],
	outq_num[0], outq_num[1], outq_num[2]);


	if (total_string_size < 0 || total_string_size >= max_size) {
		SRCCAP_MSG_ERROR("invalid string size\n");
		return -EPERM;
	}

	return total_string_size;

}

