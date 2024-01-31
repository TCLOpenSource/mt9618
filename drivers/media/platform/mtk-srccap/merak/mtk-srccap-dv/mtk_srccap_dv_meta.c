// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-----------------------------------------------------------------------------
// Include Files
//-----------------------------------------------------------------------------
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

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

#include "mtk_srccap.h"

#include "metadata_utility.h"
#include "linux/metadata_utility/m_srccap.h"

//-----------------------------------------------------------------------------
// Driver Capability
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Macros and Defines
//-----------------------------------------------------------------------------
#define STUB_MODE_DV_META_LEN 119

#define SRCCAP_DV_META_CHECK_FRAME_META_USE(hw_ver) \
	(((hw_ver) == DV_SRCCAP_HW_TAG_V4) || ((hw_ver) == DV_SRCCAP_HW_TAG_V5))

#define SRCCAP_DV_META_CHECK_WRITE(buf, dqbuf_mode) \
	((buf != NULL) && (dqbuf_mode != SRCCAP_DV_DQBUF_MODE_FROM_IRQ))

//-----------------------------------------------------------------------------
// Enums and Structures
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
static int dv_meta_map_fd(int fd, void **va, u64 *size)
{
	struct dma_buf *db = NULL;

	if ((fd == 0) || (va == NULL) || (size == NULL)) {
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
	SRCCAP_MSG_INFO("va=0x%llx, size=%llu\n", (u64)*va, *size);

	return 0;
}

static int dv_meta_unmap_fd(int fd, void *va)
{
	struct dma_buf *db = NULL;

	if ((fd == 0) || (va == NULL)) {
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

static int dv_meta_get_header(
	enum srccap_dv_meta_tag tag,
	struct meta_header *header)
{
	if (header == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	switch (tag) {
	case SRCCAP_DV_META_TAG_DV_INFO:
		header->tag = M_PQ_DV_HDMI_INFO_TAG;
		header->ver = META_SRCCAP_DV_INFO_VERSION;
		header->size = sizeof(struct meta_srccap_dv_info);
		break;
	default:
		SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
			"invalid metadata tag: %d\n", tag);
		return -EINVAL;
	}

	return 0;
}

static int dv_meta_set_single_frame_meta(
	struct srccap_dv_dqbuf_in *in,
	struct meta_srccap_dv_info *meta_dv_info,
	uint32_t hw_ver)
{
	if (in == NULL || meta_dv_info == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	if (SRCCAP_DV_META_CHECK_FRAME_META_USE(hw_ver) &&
		(in->dev->dv_info.descrb_info.buf.neg_ctrl == M_DV_CRC_STATE_OK) &&
		mtk_dv_is_descrb_control()) {
		spin_lock_irq(&dv_frame_meta_lock);
		memcpy((void *)dv_frame_meta, (void *)meta_dv_info,
			sizeof(struct meta_srccap_dv_info));
		spin_unlock_irq(&dv_frame_meta_lock);
	}

	return 0;
}
//-----------------------------------------------------------------------------
// Global Functions
//-----------------------------------------------------------------------------
int mtk_dv_meta_dqbuf(
	struct srccap_dv_dqbuf_in *in,
	struct srccap_dv_dqbuf_out *out,
	enum srccap_dv_dqbuf_mode dqbuf_mode)
{
	int ret = 0;
	static struct meta_srccap_dv_info buf_dv_meta_info[SRCCAP_DEV_NUM];
	struct meta_srccap_dv_info *meta_dv_info = NULL;
	u8 index;
	enum srccap_dv_descrb_interface interface = SRCCAP_DV_DESCRB_INTERFACE_NONE;
	uint32_t data_len = 0;
	uint32_t mem_size = 0;
	u16 dev_id = 0;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "meta dequeue buffer\n");

	if ((in == NULL) || (out == NULL)) {
		SRCCAP_DV_LOG_CHECK_POINTER(-EINVAL);
		goto exit;
	}

	if (in->dev == NULL) {
		SRCCAP_DV_LOG_CHECK_POINTER(-ENXIO);
		goto exit;
	}

	dev_id = in->dev->dev_id;
	if (dev_id >= SRCCAP_DEV_NUM) {
		pr_err("Invalid dev_id %d\r\n", dev_id);
		return -ENODEV;
	}

	meta_dv_info = (struct meta_srccap_dv_info *)&buf_dv_meta_info[dev_id];
	memset(meta_dv_info, 0, sizeof(struct meta_srccap_dv_info));

	meta_dv_info->common.path = in->dev->dev_id;
	meta_dv_info->common.dv_src_hw_version =
		in->dev->srccap_info.cap.u32DV_Srccap_HWVersion;
	meta_dv_info->common.hdmi_422_pack_en =
		in->dev->dv_info.descrb_info.common.hdmi_422_pack_en;

	interface = in->dev->dv_info.descrb_info.common.interface;
	if ((interface < SRCCAP_DV_DESCRB_INTERFACE_NONE)
		|| (interface >= SRCCAP_DV_DESCRB_INTERFACE_MAX))
		goto exit;

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "interface: %d\n", interface);

	/* set metadata */
	mem_size = sizeof(meta_dv_info->meta.data);
	switch (interface) {
	case SRCCAP_DV_DESCRB_INTERFACE_SINK_LED:
		if (IS_SRCCAP_DV_STUB_MODE()) {
		/* stub mode test */
			meta_dv_info->meta.data_length = STUB_MODE_DV_META_LEN;
			memset(meta_dv_info->meta.data, 0, STUB_MODE_DV_META_LEN);
		} else {
			index = in->dev->dv_info.descrb_info.buf.irq_rptr;
			data_len = in->dev->dv_info.descrb_info.buf.irq_info[index].data_length;
			/* check data length */
			if (data_len > mem_size) {
				SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
					"meta length %d > memory size %d\n",
					data_len, mem_size);
				ret = -ENOMEM;
				goto exit;
			}
			memcpy(meta_dv_info->meta.data,
				in->dev->dv_info.descrb_info.buf.irq_info[index].data,
				data_len);
			meta_dv_info->meta.data_length = data_len;
		}
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_SOURCE_LED_RGB:
	case SRCCAP_DV_DESCRB_INTERFACE_SOURCE_LED_YUV:
		data_len = SRCCAP_DV_DESCRB_VSIF_SIZE;
		/* check data length */
		if (data_len > mem_size) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"meta length %d > memory size %d\n",
				data_len, mem_size);
			ret = -ENOMEM;
			goto exit;
		}
		memcpy(meta_dv_info->meta.data,
			in->dev->dv_info.descrb_info.pkt_info.vsif,
			data_len);
		meta_dv_info->meta.data_length = data_len;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_DRM_SOURCE_LED_YUV:
	case SRCCAP_DV_DESCRB_INTERFACE_DRM_SOURCE_LED_RGB:
		data_len = SRCCAP_DV_DESCRB_DRM_SIZE;
		/* check data length */
		if (data_len > mem_size) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"meta length %d > memory size %d\n",
				data_len, mem_size);
			ret = -ENOMEM;
			goto exit;
		}
		memcpy(meta_dv_info->meta.data,
			in->dev->dv_info.descrb_info.pkt_info.drm,
			data_len);
		meta_dv_info->meta.data_length = data_len;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_1:
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_2_RGB:
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_2_YUV:
		data_len = in->dev->dv_info.descrb_info.pkt_info.vsem_size;
		/* check data length */
		if (data_len > sizeof(in->dev->dv_info.descrb_info.pkt_info.vsem)) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"vsem length %d > vsem memory size %ld\n",
				data_len,
				sizeof(in->dev->dv_info.descrb_info.pkt_info.vsem));
			ret = -EPERM;
			goto exit;
		}
		if (data_len > mem_size) {
			SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_ERR,
				"meta length %d > memory size %d\n",
				data_len, mem_size);
			ret = -ENOMEM;
			goto exit;
		}
		memcpy(meta_dv_info->meta.data,
			in->dev->dv_info.descrb_info.pkt_info.vsem,
			data_len);
		meta_dv_info->meta.data_length = data_len;
		break;
	default:
		goto exit;
	}

	/* convert interface enum */
	switch (interface) {
	case SRCCAP_DV_DESCRB_INTERFACE_SINK_LED:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_SINK_LED;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_SOURCE_LED_RGB:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_SOURCE_LED_RGB;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_SOURCE_LED_YUV:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_SOURCE_LED_YUV;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_DRM_SOURCE_LED_RGB:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_DRM_SOURCE_LED_RGB;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_DRM_SOURCE_LED_YUV:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_DRM_SOURCE_LED_YUV;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_1:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_FORM_1;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_2_RGB:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_FORM_2_RGB;
		break;
	case SRCCAP_DV_DESCRB_INTERFACE_FORM_2_YUV:
		meta_dv_info->descrb.interface = M_DV_INTERFACE_FORM_2_YUV;
		break;
	default:
		goto exit;
	}

	/* convert dma status enum */
	switch (in->dev->dv_info.dma_info.dma_status) {
	case SRCCAP_DV_DMA_STATUS_DISABLE:
		meta_dv_info->dma.status = M_DV_DMA_STATUS_DISABLE;
		break;
	case SRCCAP_DV_DMA_STATUS_ENABLE_FB:
		meta_dv_info->dma.status = M_DV_DMA_STATUS_ENABLE_FB;
		break;
	case SRCCAP_DV_DMA_STATUS_ENABLE_FBL:
		meta_dv_info->dma.status = M_DV_DMA_STATUS_ENABLE_FBL;
		break;
	default:
		goto exit;
	}

	/* set dv dma information */
	meta_dv_info->dma.w_index = in->dev->dv_info.dma_info.w_index;
	meta_dv_info->dma.width = in->dev->dv_info.dma_info.mem_fmt.dma_width;
	meta_dv_info->dma.height = in->dev->dv_info.dma_info.mem_fmt.dma_height;
	meta_dv_info->dma.svp_id_valid = in->dev->dv_info.dma_info.secure.svp_id_valid;
	meta_dv_info->dma.svp_id = in->dev->dv_info.dma_info.secure.svp_id;

	dv_meta_set_single_frame_meta(
			in, meta_dv_info, meta_dv_info->common.dv_src_hw_version);

	if (SRCCAP_DV_META_CHECK_WRITE(in->buf, dqbuf_mode)) {
		ret = mtk_dv_meta_write(
			in->buf->m.planes[in->buf->length - 1].m.fd,
			SRCCAP_DV_META_TAG_DV_INFO,
			meta_dv_info);
		if (ret < 0) {
			//SRCCAP_DV_LOG_CHECK_RETURN(ret, ret);
			goto exit;
		}
	}

	SRCCAP_DV_LOG_TRACE(SRCCAP_DV_DBG_LEVEL_INFO, "meta dequeue buffer done\n");

exit:
	return ret;
}

int mtk_dv_meta_read(
	int fd,
	enum srccap_dv_meta_tag tag,
	void *content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (content == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = dv_meta_map_fd(fd, &va, &size);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = dv_meta_get_header(tag, &header);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;
	meta_ret = query_metadata_header(&buffer, &header);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata header not found\n");
		ret = -EPERM;
		goto EXIT;
	}

	meta_ret = query_metadata_content(&buffer, &header, content);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata content not found\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	dv_meta_unmap_fd(fd, va);
	return ret;
}

int mtk_dv_meta_write(
	int fd,
	enum srccap_dv_meta_tag tag,
	void *content)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	if (content == NULL) {
		SRCCAP_MSG_POINTER_CHECK();
		return -EINVAL;
	}

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = dv_meta_map_fd(fd, &va, &size);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = dv_meta_get_header(tag, &header);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	buffer.paddr = (unsigned char *)va;
	buffer.size = (unsigned int)size;

	meta_ret = attach_metadata(&buffer, header, content);
	if (!meta_ret) {
		SRCCAP_MSG_ERROR("metadata not attached\n");
		ret = -EPERM;
		goto EXIT;
	}

EXIT:
	dv_meta_unmap_fd(fd, va);
	return ret;
}

int mtk_dv_meta_remove(
	int fd,
	enum srccap_dv_meta_tag tag)
{
	int ret = 0;
	unsigned char meta_ret = 0;
	void *va = NULL;
	u64 size = 0;
	struct meta_buffer buffer;
	struct meta_header header;

	memset(&buffer, 0, sizeof(struct meta_buffer));
	memset(&header, 0, sizeof(struct meta_header));

	ret = dv_meta_map_fd(fd, &va, &size);
	if (ret < 0) {
		SRCCAP_MSG_RETURN_CHECK(ret);
		goto EXIT;
	}

	ret = dv_meta_get_header(tag, &header);
	if (ret < 0) {
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
	dv_meta_unmap_fd(fd, va);
	return ret;
}

//-----------------------------------------------------------------------------
// Debug Functions
//-----------------------------------------------------------------------------
