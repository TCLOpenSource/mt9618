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
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include "linux/metadata_utility/m_vdec.h"


#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-common.h>
#include <linux/videodev2.h>
#include <linux/dma-buf.h>
#include "metadata_utility.h"

#include "mtk_pq.h"
#include "mtk_pq_display_b2r.h"

#include "drvPQ.h"
#include "apiVDEC_EX.h"
#include "drvMVOP.h"
enum pqu_codec_type mtk_pq_codec_type_transform(enum vdec_codec_type codectype)
{
	switch (codectype) {
	case VDEC_TYPE_H264:
		return PQU_VDEC_TYPE_H264;
	case VDEC_TYPE_MJPEG:
		return PQU_VDEC_TYPE_MJPEG;
	case VDEC_TYPE_VP9:
		return PQU_VDEC_TYPE_VP9;
	case VDEC_TYPE_H265:
		return PQU_VDEC_TYPE_H265;
	case VDEC_TYPE_AV1:
		return PQU_VDEC_TYPE_AV1;
	case VDEC_TYPE_H266:
		return PQU_VDEC_TYPE_H266;
	default:
		return PQU_VDEC_TYPE_NONE;
	}
}

int mtk_pq_display_b2r_qbuf(struct mtk_pq_device *pq_dev, struct mtk_pq_buffer *pq_buf)
{
	int ret = 0;
	struct meta_buffer meta_buf;
	struct mtk_pq_frame_info *frm_info_ptr = NULL;
	struct m_pq_display_b2r_ctrl b2r_ctrl;
	struct m_pq_film_grain_desc film_grain;
	struct m_pq_frm_statistic frm_statistic;
	uint32_t mfcodec_info = 0;

	memset(&meta_buf, 0, sizeof(struct meta_buffer));
	memset(&b2r_ctrl, 0, sizeof(struct m_pq_display_b2r_ctrl));
	memset(&film_grain, 0, sizeof(struct m_pq_film_grain_desc));
	memset(&frm_statistic, 0, sizeof(struct m_pq_frm_statistic));

	/* meta buffer handle */
	meta_buf.paddr = pq_buf->meta_buf.va;
	meta_buf.size = pq_buf->meta_buf.size;

	ret = mtk_pq_common_read_metadata_addr_ptr(&meta_buf,
		EN_PQ_METATAG_SH_FRM_INFO, (void **)&frm_info_ptr);
	if (ret || frm_info_ptr == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq read EN_PQ_METATAG_SH_FRM_INFO Failed, ret = %d\n", ret);
		return ret;
	}

	/* set main b2r info */
	b2r_ctrl.main.luma_fb_offset = frm_info_ptr->vdec_offset.luma_0_offset;
	b2r_ctrl.main.chroma_fb_offset = frm_info_ptr->vdec_offset.chroma_0_offset;
	b2r_ctrl.main.luma_blen_offset = frm_info_ptr->vdec_offset.luma_blen_offset;
	b2r_ctrl.main.chroma_blen_offset = frm_info_ptr->vdec_offset.chroma_blen_offset;
	b2r_ctrl.main.x = (__u16)frm_info_ptr->stFrames[0].u32X;
	b2r_ctrl.main.y = (__u16)frm_info_ptr->stFrames[0].u32Y;
	b2r_ctrl.main.width = (__u16)frm_info_ptr->stFrames[0].u32Width;
	b2r_ctrl.main.height = (__u16)frm_info_ptr->stFrames[0].u32Height;
	b2r_ctrl.main.SrcWidth = (__u16)frm_info_ptr->stFrames[0].u32SrcWidth;
	b2r_ctrl.main.SrcHeight = (__u16)frm_info_ptr->stFrames[0].u32SrcHeight;
	b2r_ctrl.main.bitdepth = (__u16)frm_info_ptr->stFrames[0].u8LumaBitdepth;
	b2r_ctrl.main.pitch = (__u16)frm_info_ptr->stFrames[0].stHWFormat.u32LumaPitch;
	b2r_ctrl.main.modifier.tile = frm_info_ptr->modifier.tile;
	b2r_ctrl.main.modifier.raster = frm_info_ptr->modifier.raster;
	b2r_ctrl.main.modifier.compress = frm_info_ptr->modifier.compress;
	b2r_ctrl.main.modifier.jump = frm_info_ptr->modifier.jump;
	b2r_ctrl.main.modifier.vsd_mode =
			(enum pqu_fmt_modifier_vsd)frm_info_ptr->modifier.vsd_mode;
	b2r_ctrl.main.modifier.vsd_ce_mode = frm_info_ptr->modifier.vsd_ce_mode;
#if IS_ENABLED(CONFIG_OPTEE)
	b2r_ctrl.main.aid = pq_dev->display_info.secure_info.aid;
#endif
	b2r_ctrl.main.order_type = (enum pqu_order_type)frm_info_ptr->order_type;
	b2r_ctrl.main.scan_type = (enum pqu_scan_type)frm_info_ptr->scan_type;
	b2r_ctrl.main.field_type = (enum pqu_field_type)frm_info_ptr->field_type;
	b2r_ctrl.main.rotate_type = (enum pqu_rotate_type)frm_info_ptr->rotate_type;
	if (frm_info_ptr->color_format == MTK_PQ_COLOR_FORMAT_YUYV)
		b2r_ctrl.main.color_fmt_422 = TRUE;
	else
		b2r_ctrl.main.color_fmt_422 = FALSE;
	if (frm_info_ptr->color_format == MTK_PQ_COLOR_FORMAT_SW_ARGB8888)
		b2r_ctrl.main.rgb_type = PQU_RGB_8888;
	else
		b2r_ctrl.main.rgb_type = PQU_RGB_NONE;
	b2r_ctrl.main.codec_type = mtk_pq_codec_type_transform(frm_info_ptr->u32CodecType);
	mfcodec_info = frm_info_ptr->stFrames[0].stHWFormat.u32MFCodecInfo;
	b2r_ctrl.main.mfcodec.ppu =
		(mfcodec_info >> DISPLAY_B2R_PPU_SHIFT) & DISPLAY_B2R_PPU_MASK;
	b2r_ctrl.main.mfcodec.uncom =
		(mfcodec_info >> DISPLAY_B2R_UNCOM_SHIFT) & DISPLAY_B2R_UNCOM_MASK;
	b2r_ctrl.main.mfcodec.vp9 =
		(mfcodec_info >> DISPLAY_B2R_VP9_SHIFT) & DISPLAY_B2R_VP9_MASK;
	b2r_ctrl.main.mfcodec.av1 =
		(mfcodec_info >> DISPLAY_B2R_AV1_SHIFT) & DISPLAY_B2R_AV1_MASK;
	b2r_ctrl.main.mfcodec.pitch =
		(mfcodec_info >> DISPLAY_B2R_PITCH_SHIFT) & DISPLAY_B2R_PITCH_MASK;
	b2r_ctrl.main.mfcodec.select =
		(mfcodec_info >> DISPLAY_B2R_SELECT_SHIFT) & DISPLAY_B2R_SELECT_MASK;

	/*set debug info*/
	pq_dev->pq_debug.main.luma_fb_offset =
			frm_info_ptr->vdec_offset.luma_0_offset;
	pq_dev->pq_debug.main.chroma_fb_offset =
			frm_info_ptr->vdec_offset.chroma_0_offset;
	pq_dev->pq_debug.main.luma_blen_offset =
			frm_info_ptr->vdec_offset.luma_blen_offset;
	pq_dev->pq_debug.main.chroma_blen_offset =
			frm_info_ptr->vdec_offset.chroma_blen_offset;
	pq_dev->pq_debug.main.x = (__u16)frm_info_ptr->stFrames[0].u32X;
	pq_dev->pq_debug.main.y = (__u16)frm_info_ptr->stFrames[0].u32Y;
	pq_dev->pq_debug.main.width = (__u16)frm_info_ptr->stFrames[0].u32Width;
	pq_dev->pq_debug.main.height =
			(__u16)frm_info_ptr->stFrames[0].u32Height;
	pq_dev->pq_debug.main.SrcWidth =
			(__u16)frm_info_ptr->stFrames[0].u32SrcWidth;
	pq_dev->pq_debug.main.SrcHeight =
			(__u16)frm_info_ptr->stFrames[0].u32SrcHeight;
	pq_dev->pq_debug.main.bitdepth =
			(__u16)frm_info_ptr->stFrames[0].u8LumaBitdepth;
	pq_dev->pq_debug.main.pitch =
			(__u16)frm_info_ptr->stFrames[0].stHWFormat.u32LumaPitch;
	pq_dev->pq_debug.main.modifier.tile = frm_info_ptr->modifier.tile;
	pq_dev->pq_debug.main.modifier.raster = frm_info_ptr->modifier.raster;
	pq_dev->pq_debug.main.modifier.compress =
			frm_info_ptr->modifier.compress;
	pq_dev->pq_debug.main.modifier.jump = frm_info_ptr->modifier.jump;
	pq_dev->pq_debug.main.modifier.vsd_mode =
			(enum pqu_fmt_modifier_vsd)frm_info_ptr->modifier.vsd_mode;
	pq_dev->pq_debug.main.modifier.vsd_ce_mode =
					frm_info_ptr->modifier.vsd_ce_mode;
	pq_dev->pq_debug.main.order_type =
			(enum pqu_order_type)frm_info_ptr->order_type;
	pq_dev->pq_debug.main.scan_type =
			(enum pqu_scan_type)frm_info_ptr->scan_type;
	pq_dev->pq_debug.main.field_type =
			(enum pqu_field_type)frm_info_ptr->field_type;
	pq_dev->pq_debug.main.rotate_type =
			(enum pqu_rotate_type)frm_info_ptr->rotate_type;
	if (frm_info_ptr->color_format == MTK_PQ_COLOR_FORMAT_YUYV)
		pq_dev->pq_debug.main.color_fmt_422 = TRUE;

	/* set sub1 b2r info */
	b2r_ctrl.sub1.luma_fb_offset = frm_info_ptr->vdec_offset.sub_luma_0_offset;
	b2r_ctrl.sub1.chroma_fb_offset = frm_info_ptr->vdec_offset.sub_chroma_0_offset;
	b2r_ctrl.sub1.luma_blen_offset = frm_info_ptr->vdec_offset.luma_blen_offset;
	b2r_ctrl.sub1.chroma_blen_offset = frm_info_ptr->vdec_offset.chroma_blen_offset;
	b2r_ctrl.sub1.x = (__u16)frm_info_ptr->stSubFrame.u32X;
	b2r_ctrl.sub1.y = (__u16)frm_info_ptr->stSubFrame.u32Y;
	b2r_ctrl.sub1.width = (__u16)frm_info_ptr->stSubFrame.u32Width;
	b2r_ctrl.sub1.height = (__u16)frm_info_ptr->stSubFrame.u32Height;
	b2r_ctrl.sub1.bitdepth = (__u16)frm_info_ptr->stSubFrame.u8LumaBitdepth;
	b2r_ctrl.sub1.pitch = (__u16)frm_info_ptr->stSubFrame.stHWFormat.u32LumaPitch;
	b2r_ctrl.sub1.modifier.tile = frm_info_ptr->submodifier.tile;
	b2r_ctrl.sub1.modifier.raster = frm_info_ptr->submodifier.raster;
	b2r_ctrl.sub1.modifier.compress = frm_info_ptr->submodifier.compress;
	b2r_ctrl.sub1.modifier.jump = frm_info_ptr->submodifier.jump;
	b2r_ctrl.sub1.modifier.vsd_mode =
			(enum pqu_fmt_modifier_vsd)frm_info_ptr->submodifier.vsd_mode;
	b2r_ctrl.sub1.modifier.vsd_ce_mode = frm_info_ptr->submodifier.vsd_ce_mode;
#if IS_ENABLED(CONFIG_OPTEE)
	b2r_ctrl.sub1.aid = pq_dev->display_info.secure_info.aid;
#endif
	b2r_ctrl.sub1.order_type = (enum pqu_order_type)frm_info_ptr->order_type;
	b2r_ctrl.sub1.scan_type = (enum pqu_scan_type)frm_info_ptr->scan_type;
	b2r_ctrl.sub1.field_type = (enum pqu_field_type)frm_info_ptr->field_type;
	b2r_ctrl.sub1.rotate_type = (enum pqu_rotate_type)frm_info_ptr->rotate_type;
	if (frm_info_ptr->color_format == MTK_PQ_COLOR_FORMAT_YUYV)
		b2r_ctrl.sub1.color_fmt_422 = TRUE;
	else
		b2r_ctrl.sub1.color_fmt_422 = FALSE;
	b2r_ctrl.sub1.codec_type = mtk_pq_codec_type_transform(frm_info_ptr->u32CodecType);

	/*set debug info*/
	pq_dev->pq_debug.sub1.luma_fb_offset =
			frm_info_ptr->vdec_offset.sub_luma_0_offset;
	pq_dev->pq_debug.sub1.chroma_fb_offset =
			frm_info_ptr->vdec_offset.sub_chroma_0_offset;
	pq_dev->pq_debug.sub1.luma_blen_offset =
			frm_info_ptr->vdec_offset.luma_blen_offset;
	pq_dev->pq_debug.sub1.chroma_blen_offset =
			frm_info_ptr->vdec_offset.chroma_blen_offset;
	pq_dev->pq_debug.sub1.x = (__u16)frm_info_ptr->stSubFrame.u32X;
	pq_dev->pq_debug.sub1.y = (__u16)frm_info_ptr->stSubFrame.u32Y;
	pq_dev->pq_debug.sub1.width = (__u16)frm_info_ptr->stSubFrame.u32Width;
	pq_dev->pq_debug.sub1.height = (__u16)frm_info_ptr->stSubFrame.u32Height;
	pq_dev->pq_debug.sub1.bitdepth =
			(__u16)frm_info_ptr->stSubFrame.u8LumaBitdepth;
	pq_dev->pq_debug.sub1.pitch =
			(__u16)frm_info_ptr->stSubFrame.stHWFormat.u32LumaPitch;
	pq_dev->pq_debug.sub1.modifier.tile = frm_info_ptr->submodifier.tile;
	pq_dev->pq_debug.sub1.modifier.raster = frm_info_ptr->submodifier.raster;
	pq_dev->pq_debug.sub1.modifier.compress =
			frm_info_ptr->submodifier.compress;
	pq_dev->pq_debug.sub1.modifier.jump = frm_info_ptr->submodifier.jump;
	pq_dev->pq_debug.sub1.modifier.vsd_mode =
			(enum pqu_fmt_modifier_vsd)frm_info_ptr->submodifier.vsd_mode;
	pq_dev->pq_debug.sub1.modifier.vsd_ce_mode =
			frm_info_ptr->submodifier.vsd_ce_mode;
	pq_dev->pq_debug.sub1.order_type =
			(enum pqu_order_type)frm_info_ptr->order_type;
	pq_dev->pq_debug.sub1.scan_type =
			(enum pqu_scan_type)frm_info_ptr->scan_type;
	pq_dev->pq_debug.sub1.field_type =
			(enum pqu_field_type)frm_info_ptr->field_type;
	pq_dev->pq_debug.sub1.rotate_type =
			(enum pqu_rotate_type)frm_info_ptr->rotate_type;
	if (frm_info_ptr->color_format == MTK_PQ_COLOR_FORMAT_YUYV)
		pq_dev->pq_debug.sub1.color_fmt_422 = TRUE;

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%llx, %s%llx, %s%llx, %s%llx, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"[b2r_ctrl.main]",
		"luma_fb_offset = 0x", b2r_ctrl.main.luma_fb_offset,
		"chroma_fb_offset = 0x", b2r_ctrl.main.chroma_fb_offset,
		"luma_blen_offset = 0x", b2r_ctrl.main.luma_blen_offset,
		"chroma_blen_offset = 0x", b2r_ctrl.main.chroma_blen_offset,
		"x = ", b2r_ctrl.main.x,
		"y = ", b2r_ctrl.main.y,
		"width = ", b2r_ctrl.main.width,
		"height = ", b2r_ctrl.main.height,
		"pitch = ", b2r_ctrl.main.pitch,
		"bitdepth = ", b2r_ctrl.main.bitdepth);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"[b2r_ctrl.main]",
		"modifier.tile = ", b2r_ctrl.main.modifier.tile,
		"modifier.raster = ", b2r_ctrl.main.modifier.raster,
		"modifier.compress = ", b2r_ctrl.main.modifier.compress,
		"modifier.jump = ", b2r_ctrl.main.modifier.jump,
		"modifier.vsd_mode = ", b2r_ctrl.main.modifier.vsd_mode,
		"modifier.vsd_ce_mode = ", b2r_ctrl.main.modifier.vsd_ce_mode);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%u, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"[b2r_ctrl.main]",
		"aid = ", b2r_ctrl.main.aid,
		"order_type = ", b2r_ctrl.main.order_type,
		"scan_type = ", b2r_ctrl.main.scan_type,
		"field_type = ", b2r_ctrl.main.field_type,
		"rotate_type = ", b2r_ctrl.main.rotate_type,
		"color_fmt_422 = ", b2r_ctrl.main.color_fmt_422);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%llx, %s%llx, %s%llx, %s%llx, %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"[b2r_ctrl.sub1]",
		"luma_fb_offset = 0x", b2r_ctrl.sub1.luma_fb_offset,
		"chroma_fb_offset = 0x", b2r_ctrl.sub1.chroma_fb_offset,
		"luma_blen_offset = 0x", b2r_ctrl.sub1.luma_blen_offset,
		"chroma_blen_offset = 0x", b2r_ctrl.sub1.chroma_blen_offset,
		"x = ", b2r_ctrl.sub1.x,
		"y = ", b2r_ctrl.sub1.y,
		"width = ", b2r_ctrl.sub1.width,
		"height = ", b2r_ctrl.sub1.height,
		"pitch = ", b2r_ctrl.sub1.pitch,
		"bitdepth = ", b2r_ctrl.sub1.bitdepth);

	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%d, %s%d, %s%d, %s%d, %s%d, %s%d\n",
		"[b2r_ctrl.sub1]",
		"modifier.tile = ", b2r_ctrl.sub1.modifier.tile,
		"modifier.raster = ", b2r_ctrl.sub1.modifier.raster,
		"modifier.compress = ", b2r_ctrl.sub1.modifier.compress,
		"modifier.jump = ", b2r_ctrl.sub1.modifier.jump,
		"modifier.vsd_mode = ", b2r_ctrl.sub1.modifier.vsd_mode,
		"modifier.vsd_ce_mode = ", b2r_ctrl.sub1.modifier.vsd_ce_mode);

	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_DISP_B2R_CTRL, (void *)&b2r_ctrl);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_DISP_B2R_CTRL Failed, ret = %d\n", ret);
		goto exit;
	}

	if (frm_info_ptr->vdec_filmgrain.apply_grain) {
		STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
				"film_grain.apply_grain=%u\n", film_grain.apply_grain);
		if (sizeof(struct mtk_pq_film_grain_desc) == sizeof(struct m_pq_film_grain_desc)) {
			memcpy(&film_grain, &frm_info_ptr->vdec_filmgrain,
						sizeof(struct m_pq_film_grain_desc));
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"mtk-pq EN_PQ_METATAG_DISP_B2R_FILM_GRAIN wrong size\n");
			goto exit;
		}
	}
	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_DISP_B2R_FILM_GRAIN, (void *)&film_grain);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_DISP_B2R_FILM_GRAIN Failed, ret = %d\n", ret);
		goto exit;
	}

	frm_statistic.statistic_enable = frm_info_ptr->vdec_frmstatistic.statistic_enable;
	frm_statistic.scan_type = (__u16)frm_info_ptr->scan_type;
	frm_statistic.frame_type = (__u16)frm_info_ptr->stFrames[0].enFrameType;
	frm_statistic.order_type = (__u16)frm_info_ptr->order_type;
	memcpy(frm_statistic.histogram, frm_info_ptr->vdec_frmstatistic.histogram,
			sizeof(__u32)*VDEC_HIST_NUM);
	frm_statistic.err_mb_rate = frm_info_ptr->vdec_frmstatistic.err_mb_rate;
	frm_statistic.cplx = frm_info_ptr->vdec_frmstatistic.cplx;
	frm_statistic.min_qp = frm_info_ptr->vdec_frmstatistic.min_qp;
	frm_statistic.max_qp = frm_info_ptr->vdec_frmstatistic.max_qp;
	frm_statistic.pps_qp = frm_info_ptr->vdec_frmstatistic.pps_qp;
	frm_statistic.slice_qp = frm_info_ptr->vdec_frmstatistic.slice_qp;
	frm_statistic.mb_qp = frm_info_ptr->vdec_frmstatistic.mb_qp;
	frm_statistic.framerate = frm_info_ptr->u32FrameRate;
	frm_statistic.field_type = frm_info_ptr->field_type;
	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_FRM_STATISTIC, (void *)&frm_statistic);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"mtk-pq write EN_PQ_METATAG_FRM_STATISTIC Failed, ret = %d\n", ret);
		goto exit;
	}
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%u, %s%u, %s%u, %s%u, %s%u, %s%u\n",
		"[frm_statistic]",
		"statistic_enable = ", frm_statistic.statistic_enable,
		"scan_type = ", frm_statistic.scan_type,
		"frame_type = ", frm_statistic.frame_type,
		"order_type = ", frm_statistic.order_type,
		"err_mb_rate = ", frm_statistic.err_mb_rate,
		"cplx = ", frm_statistic.cplx);
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"%s %s%u, %s%u, %s%u, %s%u, %s%u, %s%u\n",
		"[frm_statistic]",
		"min_qp = ", frm_statistic.min_qp,
		"max_qp = ", frm_statistic.max_qp,
		"pps_qp = ", frm_statistic.pps_qp,
		"slice_qp = ", frm_statistic.slice_qp,
		"mb_qp = ", frm_statistic.mb_qp,
		"framerate = ", frm_statistic.framerate);

	//pq_dev->extra_frame = frm_info_ptr->u8ExtraFrameNum;
	STI_PQ_CD_LOG(pq_dev->log_cd, STI_PQ_LOG_LEVEL_DBG,
		"extra_frame = %u\n", pq_dev->extra_frame);
exit:

	return ret;
}

