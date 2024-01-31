// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_plane.h"
#include "mtk_tv_drm_gem.h"
#include "mtk_tv_drm_drv.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_video.h"
#include "mtk_tv_drm_encoder.h"
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_common.h"
#include "drv_scriptmgt.h"
#include "mtk_tv_drm_demura.h"
#include "mtk_tv_drm_backlight.h"
#include "mtk_tv_drm_log.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "hwreg_render_stub.h"
#include "mtk_tv_drm_video_frc.h"
#include "pixelmonitor.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_VIDEO
#define VFRQRATIO_1000 (1000)
#define SW_DELAY (20) // for LTP test
#define SW_FB_NUM (10) //for LTP test
#define MEMC_FB_NUM (10)
#define PXM_SLEEP_DIFF_TIME (1000) // 1ms=1000us

#define TO_PTR(v)		     ((void *)(long)(v))
#define TO_PTR_BACKLIGHT_RANGE(v)    ((struct drm_mtk_range_value *)(long)(v))
#define TO_PTR_PATTERN_GENERATE(v)    ((struct drm_mtk_pattern_config *)(long)(v))
#define TO_PTR_DEMURA_CONFIG(v)    ((struct drm_mtk_demura_config *)(long)(v))



#define PANEL_PROP(i) \
		((i >= E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID) && \
			(i <= E_EXT_CRTC_PROP_NO_SIGNAL)) \

#define PIXELSHIFT_PROP(i) \
		((i >= E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE) && \
			(i <= E_EXT_CRTC_PROP_PIXELSHIFT_V)) \

#define LDM_PROP(i) \
		((i >= E_EXT_CRTC_PROP_LDM_STATUS) && \
			(i <= E_EXT_CRTC_PROP_LDM_SUSPEND_RESUME_TEST))\

#define VIDEO_GOP_PROP(i) \
		((i >= E_EXT_CRTC_PROP_GOP_VIDEO_ORDER) && \
			(i <= E_EXT_CRTC_PROP_GOP_VIDEO_ORDER))\

static void _mtk_video_display_set_LiveTone(
	struct mtk_tv_kms_context *pctx,
	void *buffer_addr)
{
	struct drm_mtk_range_value *range = (struct drm_mtk_range_value *)buffer_addr;

	if (pctx == NULL || range == NULL) {
		DRM_ERROR("[%s][%d] Invalid input", __func__, __LINE__);
		return;
	}
	DRM_INFO("[%s][%d] valid     = %u", __func__, __LINE__, range->valid);
	DRM_INFO("[%s][%d] max_value = %u", __func__, __LINE__, range->max_value);
	DRM_INFO("[%s][%d] min_value = %u", __func__, __LINE__, range->min_value);
	DRM_INFO("[%s][%d] value     = %u", __func__, __LINE__, range->value);
	drv_STUB("LiveTone_valid",     range->valid);
	drv_STUB("LiveTone_max_value", range->max_value);
	drv_STUB("LiveTone_min_value", range->min_value);
	drv_STUB("LiveTone_value",     range->value);

	if (bPquEnable) { // RV55
		struct pqu_render_live_tone backlight;

		memset(&backlight, 0, sizeof(struct pqu_render_live_tone));
		backlight.usr_valid     = range->valid;
		backlight.usr_max_value = range->max_value;
		backlight.usr_min_value = range->min_value;
		backlight.usr_value     = range->value;
		pqu_render_live_tone((const struct pqu_render_live_tone *)&backlight, NULL);
	} else { // PQU
		struct msg_render_live_tone backlight;

		memset(&backlight, 0, sizeof(struct msg_render_live_tone));
		backlight.usr_valid     = range->valid;
		backlight.usr_max_value = range->max_value;
		backlight.usr_min_value = range->min_value;
		backlight.usr_value     = range->value;
		pqu_msg_send(PQU_MSG_SEND_RENDER_LIVE_TONE, (void *)&backlight);
	}
}

static void _mtk_video_display_set_BackgroundRGB(
	struct mtk_tv_kms_context *pctx,
	void *buffer_addr)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct drm_mtk_bg_color *bg_color = (struct drm_mtk_bg_color *)buffer_addr;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_BG_RGB];
	struct hwregPostFillBgARGBIn paramIn;
	struct hwregOut paramOut;
	int i;

	if (buffer_addr == NULL) {
		DRM_ERROR("[%s][%d] set background color fail\n", __func__, __LINE__);
		return;
	}

	DRM_INFO("[%s][%d] background GBR/YUV:[%u, %u, %u]\n", __func__, __LINE__,
		bg_color->green_Y,	bg_color->blue_Cb, bg_color->red_Cr);

	for (i = MTK_VPLANE_TYPE_MEMC; i < MTK_VPLANE_TYPE_MAX; ++i) {
		memset(reg, 0, sizeof(reg));
		memset(&paramIn, 0, sizeof(struct hwregPostFillBgARGBIn));
		memset(&paramOut, 0, sizeof(struct hwregOut));

		paramIn.RIU = 0;
		paramIn.planeType = (enum drm_hwreg_video_plane_type)i;
		paramIn.bgARGB.R = bg_color->red_Cr;
		paramIn.bgARGB.G = bg_color->green_Y;
		paramIn.bgARGB.B = bg_color->blue_Cb;
		paramOut.reg = reg;

		drv_hwreg_render_display_set_postFillBgRGB(paramIn, &paramOut);
		if (!paramIn.RIU) {
			mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)&reg);
		}
		DRM_INFO("[%s][%d]: plane_type: %d, RIU %d, regCount %d", __func__, __LINE__,
			i, paramIn.RIU, paramOut.regCount);
	}
}

static void _mtk_video_crtc_set_windowRGB(
	struct mtk_tv_kms_context *pctx,
	void *buffer_addr)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct drm_mtk_mute_color *mute_color = (struct drm_mtk_mute_color *)buffer_addr;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_WINDOW_RGB];
	struct hwregPostFillWindowARGBIn paramIn;
	struct hwregOut paramOut;
	int i;
	struct mtk_video_hw_version *video_hw_version = NULL;
	enum VIDEO_MUTE_VERSION_E video_mute_hw_ver = 0;

	if (buffer_addr == NULL) {
		DRM_ERROR("[%s][%d] set mute color fail\n", __func__, __LINE__);
		return;
	}

	DRM_INFO("[%s][%d] window GBR/YUV:[%u, %u, %u]\n", __func__, __LINE__,
		mute_color->green_Y, mute_color->blue_Cb, mute_color->red_Cr);

	video_hw_version = &pctx_video->video_hw_ver;
	video_mute_hw_ver = video_hw_version->video_mute;

	if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_VB) {
		for (i = MTK_VPLANE_TYPE_MEMC; i < MTK_VPLANE_TYPE_MAX; ++i) {
			memset(reg, 0, sizeof(reg));
			memset(&paramIn, 0, sizeof(struct hwregPostFillWindowARGBIn));
			memset(&paramOut, 0, sizeof(struct hwregOut));

			paramIn.RIU = 0;
			paramIn.planeType = (enum drm_hwreg_video_plane_type)i;
			paramIn.windowARGB.R = mute_color->red_Cr;
			paramIn.windowARGB.G = mute_color->green_Y;
			paramIn.windowARGB.B = mute_color->blue_Cb;
			paramOut.reg = reg;

			drv_hwreg_render_display_set_postFillWindowRGB(paramIn, &paramOut);
			DRM_INFO("[%s][%d]: plane_type: %d, RIU %d, regCount %d",
				__func__, __LINE__, i, paramIn.RIU, paramOut.regCount);

			if (!paramIn.RIU)
				mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
					paramOut.regCount, (struct sm_reg *)&reg);
		}
	} else if (video_mute_hw_ver == EN_VIDEO_MUTE_VERSION_OSDB) {
		memset(&paramIn, 0, sizeof(struct hwregPostFillWindowARGBIn));
		memset(&paramOut, 0, sizeof(struct hwregOut));

		paramIn.RIU = 0;
		paramIn.planeType = 0;	// don't care
		paramIn.windowARGB.R = mute_color->red_Cr;
		paramIn.windowARGB.G = mute_color->green_Y;
		paramIn.windowARGB.B = mute_color->blue_Cb;
		paramOut.reg = reg;
		drv_hwreg_render_display_set_mute_color(paramIn, &paramOut);

		if (!paramIn.RIU)
			mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml,
				paramOut.regCount, (struct sm_reg *)&reg);
	} else {
		DRM_ERROR("[%s, %d]: invalid mute version\n", __func__, __LINE__);
		return;
	}
}

static inline int _mtk_video_crtc_check_blob_prop_size(
	uint32_t propy_idx,
	void *buf,
	uint32_t size)
{
	int ret = 0; // default, we don't need to check BLOB

	switch (propy_idx) {
	case E_EXT_CRTC_PROP_PQMAP_NODE_ARRAY:
		ret = mtk_tv_drm_pqmap_check_buffer(buf, size);
		break;
	case E_EXT_CRTC_PROP_GLOBAL_PQ:
		ret = mtk_tv_drm_global_pq_check(buf, size);
		break;
	case E_EXT_CRTC_PROP_DEMURA_CONFIG:
		ret = mtk_tv_drm_demura_check_buffer(buf, size);
		break;
	default:
		break;
	}
	return ret;
}

static int _mtk_video_display_get_pixel_report(
	struct mtk_tv_kms_context *pctx,
	struct drm_mtk_pxm_get_report_size *pxm_report)
{
	int ret = 0;
	__u16 instance = 0;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct pxm_res_info info;
	struct pxm_report_info report_info;
	enum pxm_return pxm_ret = E_PXM_RET_OK;

	if (!pctx || !pxm_report)
		return -EINVAL;

	pctx_pnl = &(pctx->panel_priv);

	memset(&info, 0, sizeof(struct pxm_res_info));
	memset(&report_info, 0, sizeof(struct pxm_report_info));

	//assign resource information value
	info.win_id = pxm_report->win_id;
	info.display_en = pxm_report->display_en;
	info.point = (enum pxm_point)pxm_report->point;
	info.pos.x = pxm_report->pos_x;
	info.pos.y = pxm_report->pos_y;
	info.display_color.color_r = pxm_report->display_r;
	info.display_color.color_g = pxm_report->display_g;
	info.display_color.color_b = pxm_report->display_b;
	info.display_color.alpha_val = pxm_report->display_alpha;
	info.report_en = pxm_report->report_en;
	info.report_once = 0;
	info.h_size = pctx_pnl->info.de_width;
	info.v_size = pctx_pnl->info.de_height;

	DRM_INFO("[%s][%d]report_en=%d, report_once=%d, win id(%u)\n",
		__func__, __LINE__,
		info.report_en, info.report_once, info.win_id);
	DRM_INFO("[%s][%d]point(%d), pos(x:%u, y:%u), size(h:%u, v:%u)\n",
		__func__, __LINE__,
		info.point, info.pos.x, info.pos.y, info.h_size, info.v_size);
	DRM_INFO("[%s][%d]display_en=%d, display(r:%u, g:%u, b:%u, alpha:%u\n",
		__func__, __LINE__,
		info.display_en, info.display_color.color_r, info.display_color.color_g,
		info.display_color.color_b, info.display_color.alpha_val);

	//Create pixel monitor resource
	pxm_ret = pxm_create_resource(&instance, &info);
	if (pxm_ret != E_PXM_RET_OK) {
		DRM_ERROR("[%s][%d]Fail to create pixel monitor resource, ret=%d\n",
			__func__, __LINE__, pxm_ret);
		ret = -EINVAL;
		goto exit;
	}

	//Fire pixel monitor setting
	pxm_ret = pxm_fire(instance);
	if (pxm_ret != E_PXM_RET_OK) {
		DRM_ERROR("[%s][%d]Fail to fire pixel monitor, ret=%d\n",
			__func__, __LINE__, pxm_ret);
		ret = -EINVAL;
		goto exit;
	}

	//If report enable. Get pixel value
	if (info.report_en == 1) {

		//After update position, wait a frame time to update pixel value
		usleep_range(pxm_report->frame_us, (pxm_report->frame_us + PXM_SLEEP_DIFF_TIME));

		report_info.times = 1;
		report_info.mode = E_PXM_REPORT_MODE_LEGACY;
		report_info.color = kzalloc(sizeof(struct pxm_color), GFP_KERNEL);
		if (report_info.color == NULL) {
			DRM_ERROR("[%s][%d]Fail to allocate report color info memory\n",
				__func__, __LINE__);
			goto exit;
		}

		pxm_ret = pxm_get_report(instance, &report_info);
		if (pxm_ret != E_PXM_RET_OK) {
			DRM_ERROR("[%s][%d]Fail to get pixel monitor report, ret=%d\n",
				__func__, __LINE__, pxm_ret);
			ret = -EINVAL;
			goto exit;
		}

		pxm_report->r = report_info.color->color_r;
		pxm_report->g = report_info.color->color_g;
		pxm_report->b = report_info.color->color_b;
		pxm_report->alpha = report_info.color->alpha_val;
		DRM_INFO("[%s][%d]color (r:%u, g:%u, b:%u, alpha:%u)\n",
			__func__, __LINE__,
			pxm_report->r, pxm_report->g, pxm_report->b,
			pxm_report->alpha);
	}

exit:
	kfree(report_info.color);

	pxm_ret = pxm_destroy_resource(instance);
	if (pxm_ret != E_PXM_RET_OK) {
		DRM_ERROR("[%s][%d]Fail to destroy pixel monitor resource, ret=%d\n",
			__func__, __LINE__, pxm_ret);
		ret = -EINVAL;
	}

	return ret;
}

int mtk_video_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;
	int i = 0;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;

	for (i = 0; i < E_EXT_CRTC_PROP_MAX; i++) {
		if (property == pctx->drm_crtc_prop[i]) {
			ret = 0x0;
			break;
		}
	}
	if (ret != 0) {
		DRM_ERROR("[%s, %d]: unknown CRTC property %s!!\n",
			__func__, __LINE__, property->name);
		return ret;
	}

	if (PANEL_PROP(i)) { //pnl property
		crtc_props->prop_val[i] = val;
		crtc_props->prop_update[i] = 1;
		ret = mtk_video_panel_atomic_set_crtc_property(
			crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:set crtc PNL prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (PIXELSHIFT_PROP(i)) { //pixel shift property
		crtc_props->prop_val[i] = val;
		crtc_props->prop_update[i] = 1;
		ret = mtk_video_pixelshift_atomic_set_crtc_property(
			crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:set crtc pixelshift prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (LDM_PROP(i)) { //ldm property
		crtc_props->prop_val[i] = val;
		crtc_props->prop_update[i] = 1;
		ret = mtk_ld_atomic_set_crtc_property(crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:set crtc LDM prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (IS_BLOB_PROP(property)) {
		// Avoid non-block mode maybe free the BLOB before commit_tail.
		// Alloc a memory to store the BLOB and update at mtk_video_update_crtc.
		struct drm_property_blob *property_blob = NULL;
		void *buffer = NULL;

		if (crtc_props->prop_update[i] == 1) {
			// DRM_INFO("[%s][%d]prop '%s' is not completion last commit\n",
			//     __func__, __LINE__, property->name);
			return 0;
		}
		// free the previous drm commit blob buffer
		if (crtc_props->prop_val[i])
			kvfree(TO_PTR(crtc_props->prop_val[i])); // free old buffer

		crtc_props->prop_val[i] = 0; // reset

		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_ERROR("[%s][%d]prop '%s' has invalid blob id: 0x%llx\n",
				__func__, __LINE__, property->name, val);
			return -EINVAL;
		}
		if (_mtk_video_crtc_check_blob_prop_size(i,
				property_blob->data, property_blob->length)) {
			DRM_ERROR("[%s][%d] prop %s check buffer fail",
				__func__, __LINE__, property->name);
			drm_property_blob_put(property_blob);
			return -EINVAL;
		}
		buffer = kvmalloc(property_blob->length, GFP_KERNEL);
		if (IS_ERR_OR_NULL(buffer)) {
			DRM_ERROR("[%s][%d] prop '%s' alloc buffer fail, size = %zd\n",
				__func__, __LINE__, property->name, property_blob->length);
			drm_property_blob_put(property_blob);
			return -ENOMEM;
		}
		memcpy(buffer, property_blob->data, property_blob->length);
		crtc_props->prop_val[i] = (uintptr_t)buffer;
		crtc_props->prop_update[i] = 1;
		drm_property_blob_put(property_blob);
		DRM_INFO("[%s][%d] prop '%s': blob id = %llu, blob length = %zd\n",
			__func__, __LINE__, property->name, val, property_blob->length);
	} else { // remained property
		crtc_props->prop_val[i] = val;
		crtc_props->prop_update[i] = 1;
	}

	return ret;
}

int mtk_video_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = 0;
	int i = 0;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;

	for (i = 0; i < E_EXT_CRTC_PROP_MAX; i++) {
		if (property == pctx->drm_crtc_prop[i]) {
			*val = crtc_props->prop_val[i];
			ret = 0x0;
			break;
		}
	}

	if (PANEL_PROP(i)) {
		//pnl property
		ret = mtk_video_panel_atomic_get_crtc_property(
			crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:get crtc PNL prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (PIXELSHIFT_PROP(i)) {
		//pixel shift property
		ret = mtk_video_pixelshift_atomic_get_crtc_property(
			crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:get crtc pixelshift prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (LDM_PROP(i)) {
		//ldm property
		ret = mtk_ld_atomic_get_crtc_property(crtc,
			state,
			property,
			val,
			i);
		if (ret != 0) {
			DRM_ERROR("[%s, %d]:get crtc LDM prop fail!!\n",
				__func__, __LINE__);
			return ret;
		}
	} else if (IS_BLOB_PROP(property)) {
		// TODO
		// If the property is BLOB, create a blob and return the blob id.
		// Otherwise, set @val from @prop_val directly.
	} else {
		switch (i) {
		case E_EXT_CRTC_PROP_DEMURA_CONFIG:
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_DEMURA_CUC, %td!!\n",
				__func__, __LINE__, (ptrdiff_t)*val);
			break;
		case E_EXT_CRTC_PROP_PATTERN_GENERATE:
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_PATTERN_GENERATE, %td!!\n",
				__func__, __LINE__, (ptrdiff_t)*val);
			break;
		case E_EXT_CRTC_PROP_MAX:
			DRM_ERROR("[%s][%d] invalid property!!\n", __func__, __LINE__);
			ret = -EINVAL;
			break;
		default:
			//DRM_INFO("[DRM][VIDEO][%s][%d]default\n", __func__, __LINE__);
			break;
		}
	}

	return ret;
}

void mtk_video_update_crtc(
	struct mtk_tv_drm_crtc *crtc)
{
	int i = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct ext_crtc_prop_info *crtc_prop = NULL;
	uint64_t framesync_mode = 0;

	if (!crtc) {
		DRM_ERROR("[%s, %d]: null ptr! crtc=%p\n", __func__, __LINE__, crtc);
		return;
	}

	pctx = (struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	crtc_prop = pctx->ext_crtc_properties;

	for (i = 0; i < E_EXT_CRTC_PROP_MAX; i++) {
		if (crtc_prop->prop_update[i] == 0) {
			// no update, no work
			continue;
		}
		switch (i) {
		case E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID:
			//stTimingInfo.u32HighAccurateInputVFreq = (uint32_t)val;
			//stTimingInfo.u16InputVFreq = (uint32_t)val /100;
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			framesync_mode =
				crtc_prop->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
			if (framesync_mode == VIDEO_CRTC_FRAME_SYNC_VTTV) {
				pctx->panel_priv.outputTiming_info.eFrameSyncState =
					E_PNL_FRAMESYNC_STATE_PROP_IN;
				_mtk_video_set_framesync_mode(pctx);
			}
			break;

		case E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO:
			/* move to mtk_video_panel_atomic_set_crtc_property */
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE:
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			pctx->panel_priv.outputTiming_info.eFrameSyncState =
				E_PNL_FRAMESYNC_STATE_PROP_IN;
			_mtk_video_set_framesync_mode(pctx);
			break;

		case E_EXT_CRTC_PROP_LOW_LATENCY_MODE:
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_LOW_LATENCY_MODE= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			framesync_mode =
				crtc_prop->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
			if (framesync_mode == VIDEO_CRTC_FRAME_SYNC_VTTV) {
				pctx->panel_priv.outputTiming_info.eFrameSyncState =
					E_PNL_FRAMESYNC_STATE_PROP_IN;
				_mtk_video_set_low_latency_mode(
					pctx,
					crtc_prop->prop_val[i]);
			}
			break;

		case E_EXT_CRTC_PROP_CUSTOMIZE_FRC_TABLE:
			// no use
			DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_CUSTOMIZE_FRC_TABLE= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_PANEL_TIMING:
			// no use
			DRM_INFO("[%s][%d] PANEL_TIMING= 0x%tx\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_SET_FREERUN_TIMING:
			// no use
			DRM_INFO("[%s][%d] SET_FREERUN_TIMING= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_FORCE_FREERUN:
			// no use
			DRM_INFO("[%s][%d] FORCE_FREERUN= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_FREERUN_VFREQ:
			// no use
			DRM_INFO("[%s][%d] FREERUN_VFREQ= %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_VIDEO_LATENCY:
			// no use
			DRM_INFO("[%s][%d] VIDEO_LATENCY = %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_DEMURA_CONFIG:
			DRM_INFO("[%s][%d] (crtc prop) DEMURA_CONFIG", __func__, __LINE__);
			if (crtc_prop->prop_val[i] != 0) {
				mtk_tv_drm_demura_set_config(&pctx->demura_priv,
				TO_PTR_DEMURA_CONFIG(crtc_prop->prop_val[i]));
			}
			break;

		case E_EXT_CRTC_PROP_PATTERN_GENERATE:
			DRM_INFO("[%s][%d] (crtc prop) PATTERN_GENERATE", __func__, __LINE__);
			if (crtc_prop->prop_val[i] != 0) {
				mtk_drm_pattern_set_param(&pctx->pattern_status,
				TO_PTR_PATTERN_GENERATE(crtc_prop->prop_val[i]));
			}
			break;

		case E_EXT_CRTC_PROP_MUTE_COLOR:
			DRM_INFO("[%s][%d] (crtc prop) MUTE COLOR", __func__, __LINE__);
			if (crtc_prop->prop_val[i] != 0) {
				_mtk_video_crtc_set_windowRGB(pctx,
					TO_PTR(crtc_prop->prop_val[i]));
			}
			break;

		case E_EXT_CRTC_PROP_BACKGROUND_COLOR:
			DRM_INFO("[%s][%d] (crtc prop) BACKGROUND COLOR", __func__, __LINE__);
			_mtk_video_display_set_BackgroundRGB(pctx, TO_PTR(crtc_prop->prop_val[i]));
			break;

		case E_EXT_CRTC_PROP_GLOBAL_PQ:
			mtk_tv_drm_global_pq_flush(
				&pctx->global_pq_priv,
				TO_PTR(crtc_prop->prop_val[i]));
			break;

		case E_EXT_CRTC_PROP_BACKLIGHT_RANGE:
			mtk_tv_drm_backlight_set_property(&pctx->panel_priv,
				TO_PTR_BACKLIGHT_RANGE(crtc_prop->prop_val[i]));
			break;

		case E_EXT_CRTC_PROP_LIVETONE_RANGE:
			_mtk_video_display_set_LiveTone(pctx, TO_PTR(crtc_prop->prop_val[i]));
			break;

		case E_EXT_CRTC_PROP_PQMAP_NODE_ARRAY:
			mtk_tv_drm_pqmap_update(&pctx->pqmap_priv, &pctx->video_priv.disp_ml,
				(struct drm_mtk_tv_pqmap_node_array *)crtc_prop->prop_val[i]);
			break;

		case E_EXT_CRTC_PROP_CFD_CSC_Render_In:
			mtk_video_display_SetCFD_RenderinCsc_Setting(
				pctx, TO_PTR(crtc_prop->prop_val[i]));
			break;

		case E_EXT_CRTC_PROP_NO_SIGNAL:
			DRM_INFO("[%s][%d] NO SIGNAL = %td\n",
				__func__, __LINE__, (ptrdiff_t)crtc_prop->prop_val[i]);
			if (crtc_prop->prop_val[i] != 0)
				pctx->panel_priv.outputTiming_info.FrameSyncModeChgBypass = false;
			break;

		default:
			break;
		}
		crtc_prop->prop_update[i] = 0;
	}
}

/*Connector*/
int mtk_video_atomic_set_connector_property(
	struct mtk_tv_drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t val)
{
	int ret = -EINVAL;
	int i;
	struct drm_property_blob *property_blob;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)connector->connector_private;
	struct ext_connector_prop_info *connector_props = pctx->ext_connector_properties;
	struct mtk_panel_context *pctx_pnl = NULL;

	for (i = 0; i < E_EXT_CONNECTOR_PROP_MAX; i++) {
		if (property == pctx->drm_connector_prop[i]) {
			connector_props->prop_val[i] = val;
			ret = 0x0;
			break;
		}
	}
	switch (i) {
	case E_EXT_CONNECTOR_PROP_PNL_DBG_LEVEL:
		//MApi_PNL_SetDbgLevel(val);
		DRM_INFO("[%s][%d] PNL_DBG_LEVEL = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_OUTPUT:
		//MApi_PNL_SetOutput(val);
		DRM_INFO("[%s][%d] PNL_OUTPUT = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SWING_LEVEL:
		//MApi_PNL_Control_Out_Swing(val);
		DRM_INFO("[%s][%d] PNL_SWING_LEVEL = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_FORCE_PANEL_DCLK:
		//MApi_PNL_ForceSetPanelDCLK(val , TRUE);
		DRM_INFO("[%s][%d] PNL_FORCE_PANEL_DCLK = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_PNL_FORCE_PANEL_HSTART:
		//MApi_PNL_ForceSetPanelHStart(val , TRUE);
		DRM_INFO("[%s][%d] PNL_PNL_FORCE_PANEL_HSTART = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_PNL_OUTPUT_PATTERN:
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob != NULL) {
			drm_st_pnl_output_pattern stOutputPattern;

			memset(&stOutputPattern, 0, sizeof(drm_st_pnl_output_pattern));
			memcpy(&stOutputPattern,
				property_blob->data,
				sizeof(drm_st_pnl_output_pattern));
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_OUTPUT_PATTERN = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}

		break;
	case E_EXT_CONNECTOR_PROP_PNL_MIRROR_STATUS:
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob != NULL) {
			drm_st_pnl_mirror_info stMirrorInfo;

			memset(&stMirrorInfo, 0, sizeof(drm_st_pnl_mirror_info));
			memcpy(&stMirrorInfo, property_blob->data, sizeof(drm_st_pnl_mirror_info));

			//MApi_PNL_GetMirrorStatus(&stMirrorInfo);
			//Update blob id memory
			memcpy(property_blob->data, &stMirrorInfo, sizeof(drm_st_pnl_mirror_info));
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_MIRROR_STATUS = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SSC_EN:
		//MApi_PNL_SetSSC_En(val);
		DRM_INFO("[%s][%d] PNL_SSC_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SSC_FMODULATION:
		//MApi_PNL_SetSSC_Fmodulation(val);
		DRM_INFO("[%s][%d] PNL_SSC_FMODULATION = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SSC_RDEVIATION:
		//MApi_PNL_SetSSC_Rdeviation(val);
		DRM_INFO("[[%s][%d] PNL_SSC_RDEVIATION = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_OVERDRIVER_ENABLE:
		//MApi_PNL_OverDriver_Enable(val);
		DRM_INFO("[%s][%d] PNL_OVERDRIVER_ENABLE = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		break;
	case E_EXT_CONNECTOR_PROP_PNL_PANEL_SETTING:
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob != NULL) {
			drm_st_pnl_ini_para stPnlSetting;

			memset(&stPnlSetting, 0, sizeof(drm_st_pnl_ini_para));

			memcpy(&stPnlSetting, property_blob->data, sizeof(drm_st_pnl_ini_para));
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_PANEL_SETTING = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PARSE_OUT_INFO_FROM_DTS:
		/*
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PARSE_OUT_INFO_FROM_DTS = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown output info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_parse_dts_info param;
			drm_parse_dts_info *drmParseDtsInfo
					= ((drm_parse_dts_info *)(property_blob->data));
			drm_en_pnl_output_model output_model_cal;

			memset(&param, 0, sizeof(drm_parse_dts_info));
			//memcpy(&param, property_blob->data, sizeof(drm_parse_dts_info));

			param.src_info.linkIF = drmParseDtsInfo->src_info.linkIF;
			param.src_info.div_section = drmParseDtsInfo->src_info.div_section;
			param.src_info.hsync_st = drmParseDtsInfo->src_info.hsync_st;
			param.src_info.hsync_w = drmParseDtsInfo->src_info.hsync_w;
			param.src_info.vsync_st = drmParseDtsInfo->src_info.vsync_st;
			param.src_info.vsync_w = drmParseDtsInfo->src_info.vsync_w;

			param.src_info.de_hstart = drmParseDtsInfo->src_info.de_hstart;
			param.src_info.de_vstart = drmParseDtsInfo->src_info.de_vstart;
			param.src_info.de_width = drmParseDtsInfo->src_info.de_width;
			param.src_info.de_height = drmParseDtsInfo->src_info.de_height;
			param.src_info.typ_htt = drmParseDtsInfo->src_info.typ_htt;

			param.src_info.typ_vtt = drmParseDtsInfo->src_info.typ_vtt;
			param.src_info.typ_dclk = drmParseDtsInfo->src_info.typ_dclk;

			param.src_info.op_format = drmParseDtsInfo->src_info.op_format;

			param.out_cfg.linkIF = drmParseDtsInfo->out_cfg.linkIF;
			param.out_cfg.lanes = drmParseDtsInfo->out_cfg.lanes;
			param.out_cfg.div_sec = drmParseDtsInfo->out_cfg.div_sec;
			param.out_cfg.timing = drmParseDtsInfo->out_cfg.timing;
			param.out_cfg.format = drmParseDtsInfo->out_cfg.format;

			param.v_path_en = drmParseDtsInfo->v_path_en;
			param.extdev_path_en = drmParseDtsInfo->extdev_path_en;
			param.gfx_path_en = drmParseDtsInfo->gfx_path_en;
			param.output_model_ref = drmParseDtsInfo->output_model_ref;

			param.video_hprotect_info.hsync_w =
				drmParseDtsInfo->video_hprotect_info.hsync_w;
			param.video_hprotect_info.de_hstart =
				drmParseDtsInfo->video_hprotect_info.de_hstart;
			param.video_hprotect_info.lane_numbers =
				drmParseDtsInfo->video_hprotect_info.lane_numbers;
			param.delta_hprotect_info.hsync_w =
				drmParseDtsInfo->delta_hprotect_info.hsync_w;
			param.delta_hprotect_info.de_hstart =
				drmParseDtsInfo->delta_hprotect_info.de_hstart;
			param.delta_hprotect_info.lane_numbers =
				drmParseDtsInfo->delta_hprotect_info.lane_numbers;
			param.gfx_hprotect_info.hsync_w =
				drmParseDtsInfo->gfx_hprotect_info.hsync_w;
			param.gfx_hprotect_info.de_hstart =
				drmParseDtsInfo->gfx_hprotect_info.de_hstart;
			param.gfx_hprotect_info.lane_numbers =
				drmParseDtsInfo->gfx_hprotect_info.lane_numbers;

			ret |= mtk_tv_drm_check_out_mod_cfg(&pctx->panel_priv, &param);
			ret |= mtk_tv_drm_check_out_lane_cfg(&pctx->panel_priv);
			ret |= mtk_tv_drm_check_video_hbproch_protect(&param);

			output_model_cal = mtk_tv_drm_set_output_model(param.v_path_en,
				param.extdev_path_en, param.gfx_path_en);

			DRM_INFO("[%s][%d] param.output_model_ref = %d!!\n",
				__func__, __LINE__, param.output_model_ref);

			if (output_model_cal == param.output_model_ref)
				ret |= 0;
			else
				ret |= -1;
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set out mod cfg return = %td!!\n",
				__func__, __LINE__, ret);
		}
		*/
		break;
	case E_EXT_CONNECTOR_PROP_PNL_VBO_CTRLBIT:
		property_blob = drm_property_lookup_blob(property->dev, val);
		if ((property_blob != NULL) && (val != 0)) {
			struct drm_st_ctrlbits stCtrlbits;

			memset(&stCtrlbits, 0, sizeof(struct drm_st_ctrlbits));
			memcpy(&stCtrlbits, property_blob->data, sizeof(struct drm_st_ctrlbits));

			mtk_render_set_vbo_ctrlbit(&pctx->panel_priv, &stCtrlbits);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_VBO_CTRLBIT = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_VBO_TX_MUTE_EN:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_VBO_TX_MUTE_EN = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);

		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown tx_mute_info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_tx_mute_info param;

			memset(&param, 0, sizeof(drm_st_tx_mute_info));
			memcpy(&param, property_blob->data, sizeof(drm_st_tx_mute_info));

			ret = mtk_render_set_tx_mute(&pctx->panel_priv, param);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] set TX mute return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SWING_VREG:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_SWING_VREG = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);
		pctx_pnl = &pctx->panel_priv;

		if (property_blob != NULL) {
			struct drm_st_out_swing_level stSwing;
			bool tcon_enable = 0;

			memset(&stSwing, 0, sizeof(struct drm_st_out_swing_level));
			memcpy(&stSwing, property_blob->data,
				sizeof(struct drm_st_out_swing_level));

			if (pctx_pnl)
				tcon_enable = pctx_pnl->cus.tcon_en;
			mtk_render_set_swing_vreg(&stSwing, tcon_enable);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_SWING_VREG = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_PRE_EMPHASIS:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_PRE_EMPHASIS = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		property_blob = drm_property_lookup_blob(property->dev, val);

		if (property_blob != NULL) {
			struct drm_st_out_pe_level stPE;

			memset(&stPE, 0, sizeof(struct drm_st_out_pe_level));
			memcpy(&stPE, property_blob->data, sizeof(struct drm_st_out_pe_level));

			mtk_render_set_pre_emphasis(&stPE);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_SE = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SSC:
		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob != NULL) {
			struct drm_st_out_ssc stSSC;

			memset(&stSSC, 0, sizeof(struct drm_st_out_ssc));
			memcpy(&stSSC, property_blob->data, sizeof(struct drm_st_out_ssc));

			pctx_pnl = &pctx->panel_priv;

			if (!pctx_pnl) {
				DRM_ERROR("[%s, %d]: pctx_pnl is NULL.\n",
				__func__, __LINE__);
				return -ENODEV;
			}

			memcpy(&(pctx_pnl->stdrmSSC.ssc_setting), &stSSC,
				sizeof(struct drm_st_out_ssc));
			// modulation: 1Khz for property, 0.1Khz in render DD
			// deviation: 0.01% for property, 0.01% in render DD
			pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation *= 10;
			mtk_render_set_ssc(pctx_pnl);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] PNL_SSC = %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_CHECK_DTS_OUTPUT_TIMING:
		DRM_INFO("[%s][%d] E_EXT_CONNECTOR_PROP_PNL_CHECK_DTS_OUTPUT_TIMING = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);

		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown output info status = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			drm_st_pnl_info *pStPnlInfo
					= ((drm_st_pnl_info *)(property_blob->data));

			ret = mtk_tv_drm_panel_checkDTSPara(pStPnlInfo);
			drm_property_blob_put(property_blob);
			DRM_INFO("[%s][%d] mtk_tv_drm_panel_checkDTSPara return = %d!!\n",
				__func__, __LINE__, ret);
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_FRAMESYNC_MLOAD:
		break;
	case E_EXT_CONNECTOR_PROP_MAX:
		DRM_INFO("[%s][%d] invalid property!!\n",
			__func__, __LINE__);
		break;
	default:
		break;
	}

	return ret;
}

int mtk_video_atomic_get_connector_property(
	struct mtk_tv_drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	int ret = -EINVAL;
	int i;
	drm_pnl_info stPanelInfo;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)connector->connector_private;
	struct ext_connector_prop_info *connector_props = pctx->ext_connector_properties;
	struct drm_property_blob *property_blob;
	struct mtk_panel_context *pctx_pnl = NULL;

	memset(&stPanelInfo, 0, sizeof(drm_pnl_info));
	property_blob = NULL;

	for (i = 0; i < E_EXT_CONNECTOR_PROP_MAX; i++) {
		if (property == pctx->drm_connector_prop[i]) {
			*val = connector_props->prop_val[i];
			ret = 0x0;
			break;
		}
	}

	switch (i) {
	case E_EXT_CONNECTOR_PROP_PNL_PANEL_INFO:
		//stPanelInfo.u16PnlHstart = MAPI_PNL_GetPNLHstart_U2();
		//stPanelInfo.u16PnlVstart = MAPI_PNL_GetPNLVstart_U2();
		//stPanelInfo.u16PnlWidth = MAPI_PNL_GetPNLWidth_U2();
		//stPanelInfo.u16PnlHeight = MAPI_PNL_GetPNLHeight_U2();
		//stPanelInfo.u16PnlHtotal = MAPI_PNL_GetPNLHtotal_U2();
		//stPanelInfo.u16PnlVtotal = MAPI_PNL_GetPNLVtotal_U2();
		//stPanelInfo.u16PnlHsyncWidth = MAPI_PNL_GetPNLHsyncWidth_U2();
		//stPanelInfo.u16PnlHsyncBackPorch = MAPI_PNL_GetPNLHsyncBackPorch_U2();
		//stPanelInfo.u16PnlVsyncBackPorch = MAPI_PNL_GetPNLVsyncBackPorch_U2();
		//stPanelInfo.u16PnlDefVfreq = MApi_PNL_GetDefVFreq();
		//stPanelInfo.u16PnlLPLLMode = MApi_PNL_GetLPLLMode();
		//stPanelInfo.u16PnlLPLLType = MApi_PNL_GetLPLLType_U2();
		//stPanelInfo.u16PnlMinSET = MApi_PNL_GetMinSET_U2();
		//stPanelInfo.u16PnlMaxSET = MApi_PNL_GetMaxSET_U2();
		break;
	case E_EXT_CONNECTOR_PROP_PNL_VBO_CTRLBIT:
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SWING_VREG:
		if ((property_blob == NULL) && (val != NULL)) {
			property_blob = drm_property_lookup_blob(property->dev, *val);
			if (property_blob == NULL) {
				DRM_INFO("[%s][%d] unknown swing vreg status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			} else {
				struct drm_st_out_swing_level stSwing;
				size_t len;
				bool tcon_enable = 0;

				len = min(sizeof(struct drm_st_out_swing_level),
						property_blob->length);
				memset(&stSwing, 0, sizeof(struct drm_st_out_swing_level));
				memcpy(&stSwing, property_blob->data, len);
				pctx_pnl = &pctx->panel_priv;
				if (pctx_pnl)
					tcon_enable = pctx_pnl->cus.tcon_en;
				mtk_render_get_swing_vreg(&stSwing, tcon_enable);
				drm_property_blob_put(property_blob);
				DRM_INFO("[%s][%d] get swing verg status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			}
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_PRE_EMPHASIS:
		if ((property_blob == NULL) && (val != NULL)) {
			property_blob = drm_property_lookup_blob(property->dev, *val);
			if (property_blob == NULL) {
				DRM_INFO("[%s][%d] unknown pre emphasis status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			} else {
				struct drm_st_out_pe_level stPE;
				size_t len;

				len = min(sizeof(struct drm_st_out_pe_level),
						property_blob->length);
				memset(&stPE, 0, sizeof(struct drm_st_out_pe_level));
				memcpy(&stPE, property_blob->data, len);
				mtk_render_get_pre_emphasis(&stPE);
				drm_property_blob_put(property_blob);
				DRM_INFO("[%s][%d] get pre emphasis status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			}
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_SSC:
		if ((property_blob == NULL) && (val != NULL)) {
			property_blob = drm_property_lookup_blob(property->dev, *val);
			if (property_blob == NULL) {
				DRM_INFO("[%s][%d] unknown ssc status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			} else {
				struct drm_st_out_ssc stSSC;
				size_t len;

				len = min(sizeof(struct drm_st_out_ssc),
						property_blob->length);
				memset(&stSSC, 0, sizeof(struct drm_st_out_ssc));
				memcpy(&stSSC, property_blob->data, len);
				mtk_render_get_ssc(&stSSC);
				drm_property_blob_put(property_blob);
				DRM_INFO("[%s][%d] get ssc status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			}
		}
		break;
	case E_EXT_CONNECTOR_PROP_PNL_OUT_MODEL:
		*val = pctx->out_model;
		break;
	case E_EXT_CONNECTOR_PROP_PNL_FRAMESYNC_MLOAD:
		 *val = pctx->panel_priv.outputTiming_info.eFrameSyncState;
		break;
	case E_EXT_CONNECTOR_PROP_MAX:
		DRM_INFO("[%s][%d] invalid property!!\n",
			__func__, __LINE__);
		break;
	default:
		break;
	}

	return ret;
}

drm_en_pnl_output_model mtk_tv_drm_set_output_model(bool v_path_en,
						bool extdev_path_en, bool gfx_path_en)
{
	drm_en_pnl_output_model ret = E_PNL_OUT_MODEL_MAX;

	if (v_path_en) {
		if (extdev_path_en && gfx_path_en)
			ret = E_PNL_OUT_MODEL_VG_SEPARATED_W_EXTDEV;

		if (!extdev_path_en && gfx_path_en)
			ret = E_PNL_OUT_MODEL_VG_SEPARATED;

		if (!extdev_path_en && !gfx_path_en)
			ret = E_PNL_OUT_MODEL_VG_BLENDED;
	} else {
		ret = E_PNL_OUT_MODEL_ONLY_EXTDEV;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_tv_drm_set_output_model);

void set_panel_context(struct mtk_tv_kms_context *pctx)
{
	struct device_node *np = NULL;
	bool v_path_en = false;
	bool extdev_path_en = false;
	bool gfx_path_en = false;
	uint8_t out_model_idx = E_EXT_CONNECTOR_PROP_PNL_OUT_MODEL;

	np = of_find_node_by_name(NULL, "video_out");
	if (np != NULL && of_device_is_available(np)) {
		struct mtk_panel_context *pStPanelCtx = NULL;
		struct drm_panel *pDrmPanel = NULL;

		pDrmPanel = of_drm_find_panel(np);
		pStPanelCtx = container_of(pDrmPanel, struct mtk_panel_context, drmPanel);
		memcpy(&pctx->panel_priv, pStPanelCtx, sizeof(struct mtk_panel_context));

		v_path_en = true;
	}

	np = of_find_node_by_name(NULL, "ext_video_out");
	if (np != NULL && of_device_is_available(np))
		extdev_path_en = true;

	np = of_find_node_by_name(NULL, "graphic_out");
	if (np != NULL && of_device_is_available(np))
		gfx_path_en = true;

	pctx->out_model = (enum OUTPUT_MODEL) mtk_tv_drm_set_output_model(
		v_path_en,
		extdev_path_en,
		gfx_path_en);

	pctx->ext_connector_properties->prop_val[out_model_idx] = pctx->out_model;

	drm_mode_crtc_set_gamma_size(&pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].base, MTK_GAMMA_LUT_SIZE);

	DRM_INFO("[%s][%d]Output Model is %d\n",
		__func__, __LINE__, pctx->out_model);
}

int mtk_tv_drm_video_get_delay_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_tv_video_delay *delay = (struct drm_mtk_tv_video_delay *)data;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_frc_context *pctx_frc = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	int i = 0;
	uint16_t cur_rwdiff = 0;
	uint16_t r_protect_rwdiff = 0;
	uint8_t win_num = 0;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_video = &pctx->video_priv;
	pctx_frc = &pctx->frc_priv;
	pctx_pnl = &pctx->panel_priv;

	win_num = pctx->hw_caps.window_num;

	if (pctx->stub)
		delay->sw_delay_ms = SW_DELAY;
	else
		delay->sw_delay_ms = pctx_video->sw_delay;

	DRM_INFO("\n[%s]frc_latency=%d", __func__, pctx_frc->frc_latency);

	_mtk_video_frc_get_latency_preplay(delay->memc_pre_input_vfreq, pctx);

	delay->memc_delay_ms = pctx_frc->frc_latency;
	delay->memc_rw_diff  = pctx_frc->frc_rwdiff;

	DRM_INFO("\n[%s]frc_latency=%d frc_rwdiff=%d", __func__, pctx_frc->frc_latency,
		pctx_frc->frc_rwdiff);

	for (i = 0; i < win_num; i++) {
		plane_ctx = pctx_video->plane_ctx + i;
		//add delay for protect > 0 case.
		if (plane_ctx->protectVal > 0)
			r_protect_rwdiff = 1;
		else
			r_protect_rwdiff = 0;

		delay->mdgw_delay_ms[i] =
			(VFRQRATIO_1000/pctx_pnl->outputTiming_info.u32OutputVfreq)*
			(plane_ctx->rwdiff + r_protect_rwdiff);

		drv_hwreg_render_display_get_rwDiff(i, &cur_rwdiff);
		delay->mdgw_rw_diff[i] = (cur_rwdiff + r_protect_rwdiff);

		DRM_INFO("\n[%s]win[%d] mdgw_latency = %d(ms) mdgw_diff = %d protectVal = %d\n",
			__func__, i, delay->mdgw_delay_ms[i],
			cur_rwdiff, plane_ctx->protectVal);
	}

	return 0;
}

int mtk_tv_drm_video_get_fb_num_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_tv_video_fb_num *fb = (struct drm_mtk_tv_video_fb_num *)data;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	if (pctx->stub) {
		fb->memc_fb_num = SW_FB_NUM;
		fb->mdgw_fb_num = SW_FB_NUM;
	}	else {
		if (fb->mdgw_fb_mode == MTK_VPLANE_BUF_MODE_SW)
			fb->mdgw_fb_num = 1;
		else if (fb->mdgw_fb_mode == MTK_VPLANE_BUF_MODE_HW)
			fb->mdgw_fb_num = 1;
		else
			fb->mdgw_fb_num = 0;

		fb->memc_fb_num = 0;
	}

	return 0;
}

int mtk_tv_drm_video_set_frc_mode_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_tv_video_frc_mode *frc_mode = (struct drm_mtk_tv_video_frc_mode *)data;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_frc_context *pctx_frc = NULL;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_frc = &pctx->frc_priv;
	pctx_pnl = &pctx->panel_priv;

	pctx_frc->video_memc_mode = frc_mode->video_memc_mode;
	pctx_frc->video_frc_mode = frc_mode->video_frc_mode;

	return 0;
}

int mtk_tv_drm_video_set_extra_video_latency_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_tv_video_extra_latency *latency =
		(struct drm_mtk_tv_video_extra_latency *)data;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	int ret = 0;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	ret = mtk_video_display_frc_set_extra_latency(pctx, latency);
	if (ret < 0) {
		DRM_INFO("[%s][%d] frc_set_extra_latency failed!!\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}

int mtk_tv_drm_video_get_expect_pq_out_size_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_video_expect_pq_out_size *expect_pq_out_size =
		(struct drm_mtk_video_expect_pq_out_size *)data;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;
	enum DRM_VIDEO_MEM_MODE_E mem_mode = EN_VIDEO_MEM_MODE_SW;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	if (pctx->sw_caps.enable_expect_pqout_size == 1) {
		mem_mode = expect_pq_out_size->mem_mode;

		if (mem_mode == EN_VIDEO_MEM_MODE_MAX) {
			DRM_WARN("Invalid mem_mode:%d\n", mem_mode);
			return -EINVAL;
		}

		if (mem_mode == EN_VIDEO_MEM_MODE_HW) {
			expect_pq_out_size->width = pctx_pnl->info.de_width;
			expect_pq_out_size->height = pctx_pnl->info.de_height;
		} else {
			expect_pq_out_size->width = 0;
			expect_pq_out_size->height = 0;
		}
	} else {
		expect_pq_out_size->width = 0;
		expect_pq_out_size->height = 0;
	}
/*
	DRM_INFO("expect_pq_out_size:[width,height] = [%d, %d]\n",
		expect_pq_out_size->width,
		expect_pq_out_size->height);
*/
	return 0;
}

int mtk_tv_drm_video_set_live_tone_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct drm_mtk_range_value *range = (struct drm_mtk_range_value *)data;

	if (!drm_dev || !data || !file_priv)
		return -EINVAL;

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	_mtk_video_display_set_LiveTone(pctx, range);
	return 0;
}



int mtk_tv_drm_video_get_pxm_report_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct drm_mtk_pxm_get_report_size *pxm_report = (struct drm_mtk_pxm_get_report_size *)data;
	int ret = 0;

	if (!drm_dev || !data || !file_priv)
		return -EINVAL;

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	ret = _mtk_video_display_get_pixel_report(pctx, pxm_report);
	if (ret < 0) {
		DRM_ERROR("[%s][%d] Get pixel report failed!!\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}

int mtk_tv_drm_video_set_factory_mode_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct drm_mtk_video_factory_mode  *factory_mode_ctrl =
		(struct drm_mtk_video_factory_mode  *)data;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_FACTORY_MODE];
	struct hwregFacrotyModeIn paramIn;
	struct hwregOut paramOut;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	memset(reg, 0, sizeof(reg));
	memset(&paramIn, 0, sizeof(struct hwregFacrotyModeIn));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramIn.RIU = 1;
	paramIn.factory_mode =
		(enum drm_hwreg_factory_mode) factory_mode_ctrl->factory_mode;
	paramOut.reg = reg;

	drv_hwreg_render_display_set_factory_mode(paramIn, &paramOut);

	return 0;
}

int mtk_tv_drm_video_get_frc_algo_version_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	uint32_t *u32pVersion = (uint32_t *)data;
	int ret = 0;

	if (!drm_dev || !data || !file_priv) {
		DRM_WARN("[%s][%d] Invalid input null ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_video_display_frc_get_algo_version(u32pVersion);
	DRM_INFO("MEMC Version INFO: %d\n", *u32pVersion);

	return ret;
}
