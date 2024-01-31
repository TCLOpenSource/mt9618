// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_pqu_metadata.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_common.h"
#include "mtk_tv_sm_ml.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "m_pqu_pq.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define pqu_metadata_to_kms(x) container_of(x, struct mtk_tv_kms_context, pqu_metadata_priv)
#define MTK_DRM_MODEL MTK_DRM_MODEL_PQU_METADATA
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define PQU_ENDIAN_CONVERT_ENABLE       (0) // is the endian difference between host and pqu
#if PQU_ENDIAN_CONVERT_ENABLE
#define host_to_pqu_16(v)               bswap_16(v)
#define host_to_pqu_32(v)               bswap_32(v)
#define host_to_pqu_64(v)               bswap_64(v)
#else
#define host_to_pqu_16(v)               (v)
#define host_to_pqu_32(v)               (v)
#define host_to_pqu_64(v)               (v)
#endif
#define pqu_to_host_16(v)               host_to_pqu_16(v)
#define pqu_to_host_32(v)               host_to_pqu_32(v)
#define pqu_to_host_64(v)               host_to_pqu_64(v)

#define REG_MAX_NUM                     (8)

#define PQU_METADATA_MAGIC              (0x724b544d) // it means "MTKr"
#define INVALID_PQ_ID			(0xff)
#define INVALID_FRAME_ID		(0xff)

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_pqu_metadata_init(
	struct mtk_pqu_metadata_context *context)
{
	struct pqu_render_be_sharemem_info_param pqu_shm;
	struct msg_render_be_sharemem_info msg_shm;
	uint8_t i;
	int ret;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == true) {
		ERR("context has already inited");
		return 0;
	}
	memset(context, 0, sizeof(struct mtk_pqu_metadata_context));

	// get mmap
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&context->pqu_metabuf,
		MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_MATADATA)) {
		ERR("get mmap MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_MATADATA fail");
		return 0;
	}
	memset(context->pqu_metabuf.addr, 0, context->pqu_metabuf.size);
	for (i = 0; i < MTK_TV_DRM_PQU_METADATA_COUNT; ++i) {
		context->pqu_metadata_ptr[i] = context->pqu_metabuf.addr +
			(sizeof(struct drm_mtk_tv_pqu_metadata) * i);
	}
	LOG("msg_sharemem.phys  = %llu", context->pqu_metabuf.mmap_info.phy_addr);
	LOG("msg_sharemem.size  = %zu",  sizeof(struct drm_mtk_tv_pqu_metadata));
	LOG("msg_sharemem.count = %u",   MTK_TV_DRM_PQU_METADATA_COUNT);

	// notify pqu
	memset(&pqu_shm, 0, sizeof(struct pqu_render_be_sharemem_info_param));
	memset(&msg_shm, 0, sizeof(struct msg_render_be_sharemem_info));
	pqu_shm.phys  = msg_shm.phys  = context->pqu_metabuf.mmap_info.phy_addr;
	pqu_shm.size  = msg_shm.size  = sizeof(struct drm_mtk_tv_pqu_metadata);
	pqu_shm.count = msg_shm.count = MTK_TV_DRM_PQU_METADATA_COUNT;
	ret = MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_BE_SHAREMEM_INFO, &msg_shm, &pqu_shm);
	if (ret) {
		mtk_tv_drm_metabuf_free(&context->pqu_metabuf);
		memset(context, 0, sizeof(struct mtk_pqu_metadata_context));
		ERR("pqu fail, ret = %d", ret);
		return 0;
	}
	context->init = true;

	return 0;
}

int mtk_tv_drm_pqu_metadata_deinit(
	struct mtk_pqu_metadata_context *context)
{
	struct pqu_render_be_sharemem_info_param pqu_shm;
	struct msg_render_be_sharemem_info msg_shm;
	int ret = 0;

	if (!context) {
		ERR("Invalid input, context = %p", context);
		return -EINVAL;
	}
	if (context->init == true) {
		memset(&pqu_shm, 0, sizeof(struct pqu_render_be_sharemem_info_param));
		memset(&msg_shm, 0, sizeof(struct msg_render_be_sharemem_info));
		MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_BE_SHAREMEM_INFO, &msg_shm, &pqu_shm);
		mtk_tv_drm_metabuf_free(&context->pqu_metabuf);
	}
	memset(context, 0, sizeof(struct mtk_pqu_metadata_context));
	return ret;
}

int mtk_tv_drm_pqu_metadata_suspend(
	struct mtk_pqu_metadata_context *context)
{
	// do nothing
	return 0;
}

int mtk_tv_drm_pqu_metadata_resume(
	struct mtk_pqu_metadata_context *context)
{
	struct pqu_render_be_sharemem_info_param pqu_shm;
	struct msg_render_be_sharemem_info msg_shm;
	int ret = 0;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == false) {
		ERR("render context has not been inited");
		return 0;
	}

	// notify pqu
	memset(&pqu_shm, 0, sizeof(struct pqu_render_be_sharemem_info_param));
	memset(&msg_shm, 0, sizeof(struct msg_render_be_sharemem_info));
	pqu_shm.phys  = msg_shm.phys  = context->pqu_metabuf.mmap_info.phy_addr;
	pqu_shm.size  = msg_shm.size  = sizeof(struct drm_mtk_tv_pqu_metadata);
	pqu_shm.count = msg_shm.count = MTK_TV_DRM_PQU_METADATA_COUNT;
	ret = MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_BE_SHAREMEM_INFO, &msg_shm, &pqu_shm);
	if (ret) {
		mtk_tv_drm_metabuf_free(&context->pqu_metabuf);
		memset(context, 0, sizeof(struct mtk_pqu_metadata_context));
		return -EINVAL;
	}
	return 0;
}

int mtk_tv_drm_pqu_metadata_set_attr(
	struct mtk_pqu_metadata_context *context,
	enum mtk_pqu_metadata_attr attr,
	void *value)
{
	bool update_be_setting = false;
	int ret = 0;

	if (!context || !value) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (context->init == false) {
		ERR("render context has not been inited");
		return -EINVAL;
	}

	switch (attr) {
	case MTK_PQU_METADATA_ATTR_GLOBAL_FRAME_ID:
		context->pqu_metadata.global_frame_id = *(uint32_t *)value;
		// LOG("Update global_frame_id = %u", context->pqu_metadata.global_frame_id);
		break;

	case MTK_PQU_METADATA_ATTR_RENDER_MODE:
		context->render_setting.render_mode = *(uint32_t *)value;
		LOG("Update render_mode = %u", context->render_setting.render_mode);
		update_be_setting = true;
		break;

	default:
		ERR("Unknown attr %d", attr);
		return -EINVAL;
	}

	if (update_be_setting) {
		struct msg_render_be_setting msg;
		struct pqu_render_be_setting_param pqu;

		memset(&msg, 0, sizeof(struct msg_render_be_setting));
		memset(&pqu, 0, sizeof(struct pqu_render_be_setting_param));
		msg.render_mode = pqu.render_mode = context->render_setting.render_mode;
		MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_BE_SETTING, &msg, &pqu);
	}
	return ret;
}

int mtk_tv_drm_pqu_metadata_add_window_setting(
	struct mtk_pqu_metadata_context *context,
	uint32_t plane_index,
	struct mtk_plane_state *state)
{
	struct mtk_tv_kms_context *kms = NULL;
	struct mtk_video_context *video_ctx = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	struct video_plane_prop *plane_props = NULL;
	struct meta_pq_display_flow_info pqdd_display_flow_info;
	struct meta_pq_output_frame_info pqdd_output_frame_info;
	struct drm_mtk_tv_pqu_window_setting window_setting;

	if (!context || !state) {
		ERR("Invalid input");
		return -EINVAL;
	}
	kms = pqu_metadata_to_kms(context);
	video_ctx = &kms->video_priv;
	plane_ctx = video_ctx->plane_ctx + plane_index;
	plane_props = video_ctx->video_plane_properties + plane_index;

	if (context->init == false) {
		ERR("render context has not been inited");
		return -EINVAL;
	}

	// check total window num
	if (context->pqu_metadata.window_num >= MTK_TV_DRM_WINDOW_NUM_MAX) {
		ERR("Too many window, MAX %d", MTK_TV_DRM_WINDOW_NUM_MAX);
		return -EINVAL;
	}

	// read metadata
	memset(&pqdd_display_flow_info, 0, sizeof(struct meta_pq_display_flow_info));
	memset(&pqdd_output_frame_info, 0, sizeof(struct meta_pq_output_frame_info));
	if (mtk_render_common_read_metadata(plane_ctx->meta_db,
		RENDER_METATAG_PQDD_DISPLAY_INFO, &pqdd_display_flow_info)) {
#if (!MARK_METADATA_LOG)
		ERR("Read metadata RENDER_METATAG_PQDD_DISPLAY_INFO fail");
#endif
		pqdd_display_flow_info.win_id = INVALID_PQ_ID;
	}
	if (mtk_render_common_read_metadata(plane_ctx->meta_db,
			RENDER_METATAG_PQDD_OUTPUT_FRAME_INFO, &pqdd_output_frame_info)) {
#if (!MARK_METADATA_LOG)
		ERR("Read metadata RENDER_METATAG_PQDD_OUTPUT_FRAME_INFO fail");
#endif
		pqdd_output_frame_info.pq_frame_id = INVALID_FRAME_ID;
	}

	// update window setting
	memset(&window_setting, 0, sizeof(struct drm_mtk_tv_pqu_window_setting));
	window_setting.mute          = plane_props->prop_val[EXT_VPLANE_PROP_MUTE_SCREEN];
	window_setting.pq_id         = pqdd_display_flow_info.win_id;
	window_setting.frame_id      = pqdd_output_frame_info.pq_frame_id;
	window_setting.window_x      = state->base.crtc_x;
	window_setting.window_y      = state->base.crtc_y;
	window_setting.window_z      = state->base.zpos;
	window_setting.window_width  = state->base.crtc_w;
	window_setting.window_height = state->base.crtc_h;
	memcpy(&context->pqu_metadata.window_setting[context->pqu_metadata.window_num++],
		&window_setting, sizeof(struct drm_mtk_tv_pqu_window_setting));
	// LOG("window   = %u", context->pqu_metadata->window_num);
	// LOG("mute     = %u", window_setting->mute);
	// LOG("pq_id    = %u", window_setting->pq_id);
	// LOG("frame_id = %u", window_setting->frame_id);
	// LOG("window_x = %u", window_setting->window_x);
	// LOG("window_y = %u", window_setting->window_y);
	// LOG("window_z = %u", window_setting->window_z);
	// LOG("window_w = %u", window_setting->window_width);
	// LOG("window_h = %u", window_setting->window_height);

	/// why Panel need to get pq frame id here ??
	memcpy(&kms->panel_priv.gu32frameID, &window_setting.frame_id, sizeof(__u32));
	return 0;
}

int mtk_tv_drm_pqu_metadata_fire_window_setting(
	struct mtk_pqu_metadata_context *context)
{
	struct mtk_tv_kms_context *kms = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct drm_mtk_tv_pqu_metadata *pqu_metadata = NULL;
	struct hwregDummyValueIn dummy_reg_in;
	struct hwregOut dummy_reg_out;
	struct sm_ml_add_info ml_cmd;
	struct reg_info reg[REG_MAX_NUM];

	if (!context) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (context->init == false) {
		ERR("render context has not been inited");
		return -EINVAL;
	}
	kms = pqu_metadata_to_kms(context);
	pctx_video = &kms->video_priv;
	pqu_metadata = &context->pqu_metadata;

	memset(&ml_cmd, 0, sizeof(struct sm_ml_add_info));
	memset(&dummy_reg_in, 0, sizeof(struct hwregDummyValueIn));
	memset(&dummy_reg_out, 0, sizeof(struct hwregOut));

	// set pqu_metadata magic
	context->pqu_metadata.magic = PQU_METADATA_MAGIC;

#if PQU_ENDIAN_CONVERT_ENABLE
	// if pqurv55 enable, convert endian
	if (bPquEnable) {
		struct drm_mtk_tv_pqu_window_setting *setting = NULL;
		uint32_t i;

		for (i = 0; i < pqu_metadata->window_num; ++i) {
			setting = &pqu_metadata->window_setting[i];
			setting->mute          = host_to_pqu_32(setting->mute);
			setting->pq_id         = host_to_pqu_32(setting->pq_id);
			setting->frame_id      = host_to_pqu_32(setting->frame_id);
			setting->window_x      = host_to_pqu_32(setting->window_x);
			setting->window_y      = host_to_pqu_32(setting->window_y);
			setting->window_z      = host_to_pqu_32(setting->window_z);
			setting->window_width  = host_to_pqu_32(setting->window_width);
			setting->window_height = host_to_pqu_32(setting->window_height);
		}
		pqu_metadata->magic           = host_to_pqu_32(pqu_metadata->magic);
		pqu_metadata->window_num      = host_to_pqu_32(pqu_metadata->window_num);
		pqu_metadata->global_frame_id = host_to_pqu_32(pqu_metadata->global_frame_id);
	}
#endif
	// copy current pqu_metadata to correct shared memory and reset it
	memcpy(context->pqu_metadata_ptr[context->pqu_metadata_ptr_index],
		&context->pqu_metadata, sizeof(struct drm_mtk_tv_pqu_metadata));
	memset(&context->pqu_metadata, 0, sizeof(struct drm_mtk_tv_pqu_metadata));

	// get dummy reg
	dummy_reg_in.RIU = 0;
	dummy_reg_in.dummy_type = DUMMY_TYPE_PQU_METADATA_INDEX;
	dummy_reg_in.dummy_value = host_to_pqu_16(context->pqu_metadata_ptr_index);
	dummy_reg_out.reg = reg;
	drv_hwreg_render_display_set_dummy_value(dummy_reg_in, &dummy_reg_out);

	// send dummy reg
	if (!dummy_reg_in.RIU) {
		mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
			dummy_reg_out.regCount, (struct sm_reg *)reg);
	}
	//LOG("set dummy reg to %d", context->pqu_metadata_ptr_index);

	// update shared memory index
	context->pqu_metadata_ptr_index =
		(context->pqu_metadata_ptr_index + 1) % MTK_TV_DRM_PQU_METADATA_COUNT;

	return 0;
}

int mtk_tv_drm_pqu_metadata_get_copy_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_tv_kms_context *kms = NULL;
	struct mtk_pqu_metadata_context *context = NULL;
	struct mtk_tv_drm_metabuf pqu_metabuf;
	struct drm_mtk_tv_pqu_metadata pqu_metadata;
	uint16_t pqu_meta_index = MTK_TV_DRM_PQU_METADATA_COUNT;
	int ret = 0;

	// get context
	drm_for_each_crtc(crtc, drm) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		mplane = mtk_tv_crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO];
		kms = (struct mtk_tv_kms_context *)mplane->crtc_private;
		break;
	}
	context = &kms->pqu_metadata_priv;
	if (context->init == false) {
		ERR("pqu_metadata has not been inited");
		return -EINVAL;
	}

	if (data == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	// get pqu metadata index
	drv_hwreg_render_display_get_dummy_value(DUMMY_TYPE_PQU_METADATA_INDEX, &pqu_meta_index);
#if PQU_ENDIAN_CONVERT_ENABLE
	// if pqurv55 enable, convert endian
	if (bPquEnable)
		pqu_meta_index = pqu_to_host_16(pqu_meta_index);
#endif
	if (pqu_meta_index >= MTK_TV_DRM_PQU_METADATA_COUNT) {
		ERR("get meta index fail (%u)", pqu_meta_index);
		return -EPERM;
	}

	// get metabuf of pqu metadata mmap
	memset(&pqu_metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(
		&pqu_metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_MATADATA)) {
		ERR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_MATADATA);
		return -EPERM;
	}

	// copy current mmap pqu metadata to local buffer
	memcpy(&pqu_metadata, &((struct drm_mtk_tv_pqu_metadata *)pqu_metabuf.addr)[pqu_meta_index],
		sizeof(struct drm_mtk_tv_pqu_metadata));
#if PQU_ENDIAN_CONVERT_ENABLE
	// if pqurv55 enable, convert endian
	if (bPquEnable) {
		struct drm_mtk_tv_pqu_window_setting *setting = NULL;
		int i;

		pqu_metadata.window_num      = pqu_to_host_32(pqu_metadata.window_num);
		pqu_metadata.global_frame_id = pqu_to_host_32(pqu_metadata.global_frame_id);
		if (pqu_metadata.window_num > MTK_TV_DRM_WINDOW_NUM_MAX) {
			ERR("get window number fail (%u)", pqu_metadata.window_num);
			mtk_tv_drm_metabuf_free(&pqu_metabuf);
			return -EPERM;
		}
		for (i = 0; i < pqu_metadata.window_num; ++i) {
			setting = &pqu_metadata.window_setting[i];
			setting->mute          = pqu_to_host_32(setting->mute);
			setting->pq_id         = pqu_to_host_32(setting->pq_id);
			setting->frame_id      = pqu_to_host_32(setting->frame_id);
			setting->window_x      = pqu_to_host_32(setting->window_x);
			setting->window_y      = pqu_to_host_32(setting->window_y);
			setting->window_z      = pqu_to_host_32(setting->window_z);
			setting->window_width  = pqu_to_host_32(setting->window_width);
			setting->window_height = pqu_to_host_32(setting->window_height);
		}
	}
#endif
	// copy local pqu metadata buffer to output buffer
	memcpy(data, &pqu_metadata, sizeof(struct drm_mtk_tv_pqu_metadata));

	// free metabuf
	ret = mtk_tv_drm_metabuf_free(&pqu_metabuf);
	if (ret) {
		ERR("free mmap fail");
		return ret;
	}
	return 0;
}
