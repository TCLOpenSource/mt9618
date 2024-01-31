// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <drm/mtk_tv_drm.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "mtk_tv_drm_sync.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_plane.h"
#include "mtk_tv_drm_gop_wrapper.h"
#include "mtk_tv_drm_drv.h"
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_gop.h"
#include "mtk_tv_drm_encoder.h"
#include "mtk_tv_drm_gop_plane.h"
#include "mtk_tv_drm_kms.h"
#include "mapi_pq_if.h"
#include "mapi_pq_output_format_if.h"
#include "reg/mapi_pq_nts_reg_if.h"
#include "ext_command_render_if.h"
#include "mtk-reserved-memory.h"
#include "mdrv_gop.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_GOP
#define MTK_DRM_DTS_GOPNUM_TAG     "GRAPHIC_GOP_NUM"
#define MTK_DRM_DTS_HMIRROR_TAG     "GRAPHIC_HMIRROR"
#define MTK_DRM_DTS_VMIRROR_TAG     "GRAPHIC_VMIRROR"
#define MTK_DRM_DTS_HSTRETCH_INIT_MODE_TAG "GRAPHIC_HSTRETCH_INIT_MODE"
#define MTK_DRM_DTS_VSTRETCH_INIT_MODE_TAG "GRAPHIC_VSTRETCH_INIT_MODE"
#define MTK_DRM_DTS_IPVERSION_TAG     "ip-version"
#define MTK_DRM_DTS_ENGINE_PMODE_TAG     "GRAPHIC_ENGINE_PMODE"
#define MTK_DRM_DTS_AFBC_WIDTH_CAP_TAG     "GOP_AFBC_WIDTH_CAPS"
#define MTK_DRM_DTS_GOP_INPUT_MAX_WIDTH_CAP     "GOP_INPUT_MAX_WIDTH_CAP"
#define MTK_DRM_DTS_DUAL_GOP_LAYER_TAG     "GOP_DUAL_FOR_ONE_LAYER"
#define MTK_DRM_DTS_GOP_USE_ML		"GOP_USE_ML"
#define MTK_DRM_DTS_AUTO_DETECT_TAG     "GOP_AUTO_DETECT_CAP"
#define MTK_DRM_DTS_BLEND2OSDB0_TAG     "GOP_BLENDING_TO_OSDB0_CAP"
#define MTK_DRM_DTS_OSDB_LOC_TAG     "GOP_SELECT_OSDB_LOCATION_CAP"
#define MTK_DRM_DTS_GOP_BLEND_VIDEO_IRQ		"GOP_BLEND_VIDEO_IRQ_CAP"
#define MTK_DRM_DTS_GOP_MULTI_ALPHA_TAG     "GOP_MULTI_ALPHA_CAPS"
#define MTK_DRM_DTS_GOP_STEP2_HVSP_TAG     "GOP_STEP2_HVSP_CAPS"

/* panel info */
#define MTK_DRM_PANEL_COLOR_COMPATIBLE_STR "panel-color-info"
#define MTK_DRM_DTS_PANEL_CURLUM_TAG     "HDRPanelNits"

#define MTK_DRM_DTS_GOP_CAPABILITY_TAG     "capability"

#define MTK_DRM_GOP_ATTR_MODE     (0644)

#define GOP_MAX_ML_CMD_NUM	(PIM_GOP_DOMAIN_ML_MAX_SIZE + 500)
#define GOP_ML_BUF_CNT		(2)


#define CHANNEL_NUM	(3)
#define CHANNEL_0	(0)
#define CHANNEL_1	(1)
#define CHANNEL_2	(2)
#define IDX_1	(1)
#define IDX_2	(2)
#define IDX_3	(3)
#define US_PER_MS	(1000)
#define MAX_RETRY	(50)

int bootlogo_dual_idx = INVALID_NUM;
bool Debug_GOPOnOFF_Mode;

static uint32_t _gu32CapsAFBC[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_AFBC,
	MTK_GOP_CAPS_OFFSET_GOP_AFBC_COMMON};
static uint32_t _gu32CapsHMirror[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_HMIRROR,
	MTK_GOP_CAPS_OFFSET_GOP_HMIRROR_COMMON};
static uint32_t _gu32CapsVMirror[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_VMIRROR,
	MTK_GOP_CAPS_OFFSET_GOP_VMIRROR_COMMON};
static uint32_t _gu32CapsTransColor[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_TRANSCOLOR,
	MTK_GOP_CAPS_OFFSET_GOP_TRANSCOLOR_COMMON};
static uint32_t _gu32CapsScroll[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_SCROLL,
	MTK_GOP_CAPS_OFFSET_GOP_SCROLL_COMMON};
static uint32_t _gu32CapsBlendXcIp[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_BLEND_XCIP,
	MTK_GOP_CAPS_OFFSET_GOP_BLEND_XCIP_COMMON};
static uint32_t _gu32CapsCSC[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_CSC,
	MTK_GOP_CAPS_OFFSET_GOP_CSC_COMMON};
static uint32_t _gu32CapsHDRMode[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_HDR_MODE_MSK,
	MTK_GOP_CAPS_OFFSET_GOP_HDR_MODE_MSK_COMMON};
static uint32_t _gu32CapsHDRModeSft[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_HDR_MODE_SFT,
	MTK_GOP_CAPS_OFFSET_GOP_HDR_MODE_SFT_COMMON};
static uint32_t _gu32CapsHScaleMode[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_HSCALING_MODE_MSK,
	MTK_GOP_CAPS_OFFSET_GOP_HSCALING_MODE_MSK_COMMON};
static uint32_t _gu32CapsHScaleSft[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_HSCALING_MODE_SFT,
	MTK_GOP_CAPS_OFFSET_GOP_HSCALING_MODE_SFT_COMMON};
static uint32_t _gu32CapsVScaleMode[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_VSCALING_MODE_MSK,
	MTK_GOP_CAPS_OFFSET_GOP_VSCALING_MODE_MSK_COMMON};
static uint32_t _gu32CapsVScaleSft[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_VSCALING_MODE_SFT,
	MTK_GOP_CAPS_OFFSET_GOP_VSCALING_MODE_SFT_COMMON};
static uint32_t _gu32CapsH10V6Switch[2] = {
	MTK_GOP_CAPS_OFFSET_GOP0_H10V6_SWITCH,
	MTK_GOP_CAPS_OFFSET_GOP_H10V6_SWITCH_COMMON};
static uint32_t _gu32CapsXVYCC_Lite[MTK_GOP_CAPS_TYPE] = {
	MTK_GOP_CAPS_OFFSET_GOP0_XVYCC_LITE,
	MTK_GOP_CAPS_OFFSET_GOP_XVYCC_LITE};

static MS_BOOL _gbGOPCurrentBypass[MAX_GOP_PLANE_NUM] = {0};
static MS_BOOL _gbGOPCurrentOverScan[MAX_GOP_PLANE_NUM] = {0};

static void _delaytask(uint32_t ms)
{
	if (drm_can_sleep())
		usleep_range(ms * US_PER_MS, ms * US_PER_MS);
	else
		mdelay(ms);
}

static enum EN_GOP_WRAP_CSC _convertCsc(uint64_t u64CscVal)
{
	enum EN_GOP_WRAP_CSC eCsc;

	switch (u64CscVal) {
	case MTK_DRM_CSC_Y2R_709_F2F:
		eCsc = E_GOP_WRAP_CSC_Y2R_709_F2F;
		break;
	case MTK_DRM_CSC_Y2R_709_F2L:
		eCsc = E_GOP_WRAP_CSC_Y2R_709_F2L;
		break;
	case MTK_DRM_CSC_Y2R_709_L2F:
		eCsc = E_GOP_WRAP_CSC_Y2R_709_L2F;
		break;
	case MTK_DRM_CSC_Y2R_709_L2L:
		eCsc = E_GOP_WRAP_CSC_Y2R_709_L2L;
		break;
	case MTK_DRM_CSC_Y2R_601_F2F:
		eCsc = E_GOP_WRAP_CSC_Y2R_601_F2F;
		break;
	case MTK_DRM_CSC_Y2R_601_F2L:
		eCsc = E_GOP_WRAP_CSC_Y2R_601_F2L;
		break;
	case MTK_DRM_CSC_Y2R_601_L2F:
		eCsc = E_GOP_WRAP_CSC_Y2R_601_L2F;
		break;
	case MTK_DRM_CSC_Y2R_601_L2L:
		eCsc = E_GOP_WRAP_CSC_Y2R_601_L2L;
		break;
	case MTK_DRM_CSC_R2Y_709_F2F:
		eCsc = E_GOP_WRAP_CSC_R2Y_709_F2F;
		break;
	case MTK_DRM_CSC_R2Y_709_F2L:
		eCsc = E_GOP_WRAP_CSC_R2Y_709_F2L;
		break;
	case MTK_DRM_CSC_R2Y_709_L2F:
		eCsc = E_GOP_WRAP_CSC_R2Y_709_L2F;
		break;
	case MTK_DRM_CSC_R2Y_709_L2L:
		eCsc = E_GOP_WRAP_CSC_R2Y_709_L2L;
		break;
	case MTK_DRM_CSC_R2Y_601_F2F:
		eCsc = E_GOP_WRAP_CSC_R2Y_601_F2F;
		break;
	case MTK_DRM_CSC_R2Y_601_F2L:
		eCsc = E_GOP_WRAP_CSC_R2Y_601_F2L;
		break;
	case MTK_DRM_CSC_R2Y_601_L2F:
		eCsc = E_GOP_WRAP_CSC_R2Y_601_L2F;
		break;
	case MTK_DRM_CSC_R2Y_601_L2L:
		eCsc = E_GOP_WRAP_CSC_R2Y_601_L2L;
		break;
	default:
		eCsc = E_GOP_WRAP_CSC_NONE;
		break;
	}

	return eCsc;
}

static EN_GOP_WRAP_OPOSDB_DST _convertOpOsdbDst(uint64_t u64OpOsdbVal)
{
	EN_GOP_WRAP_OPOSDB_DST eOpOsdbDst;

	switch (u64OpOsdbVal) {
	case MTK_DRM_DST_OSDB0:
		eOpOsdbDst = E_GOP_WARP_OPDST_OSDB0;
		break;
	case MTK_DRM_DST_OSDB1:
		eOpOsdbDst = E_GOP_WARP_OPDST_OSDB1;
		break;
	default:
		eOpOsdbDst = E_GOP_WARP_OPDST_INVALID;
		break;
	}

	return eOpOsdbDst;
}

static UTP_EN_GOP_COLOR_TYPE _convertColorFmt(const u32 u32DrmFmt)
{
	UTP_EN_GOP_COLOR_TYPE utpWrapfmt;

	switch (u32DrmFmt) {
	case DRM_FORMAT_XRGB8888:
		utpWrapfmt = UTP_E_GOP_COLOR_XRGB8888;
		break;
	case DRM_FORMAT_ABGR8888:
		utpWrapfmt = UTP_E_GOP_COLOR_ABGR8888;
		break;
	case DRM_FORMAT_RGBA8888:
		utpWrapfmt = UTP_E_GOP_COLOR_RGBA8888;
		break;
	case DRM_FORMAT_BGRA8888:
		utpWrapfmt = UTP_E_GOP_COLOR_BGRA8888;
		break;
	case DRM_FORMAT_ARGB8888:
		utpWrapfmt = UTP_E_GOP_COLOR_ARGB8888;
		break;
	case DRM_FORMAT_RGB565:
		utpWrapfmt = UTP_E_GOP_COLOR_RGB565;
		break;
	case DRM_FORMAT_ARGB4444:
		utpWrapfmt = UTP_E_GOP_COLOR_ARGB4444;
		break;
	case DRM_FORMAT_ARGB1555:
		utpWrapfmt = UTP_E_GOP_COLOR_ARGB1555;
		break;
	case DRM_FORMAT_YUV422:
		utpWrapfmt = UTP_E_GOP_COLOR_YUV422;
		break;
	case DRM_FORMAT_RGBA5551:
		utpWrapfmt = UTP_E_GOP_COLOR_RGBA5551;
		break;
	case DRM_FORMAT_RGBA4444:
		utpWrapfmt = UTP_E_GOP_COLOR_RGBA4444;
		break;
	case DRM_FORMAT_BGR565:
		utpWrapfmt = UTP_E_GOP_COLOR_BGR565;
		break;
	case DRM_FORMAT_ABGR4444:
		utpWrapfmt = UTP_E_GOP_COLOR_ABGR4444;
		break;
	case DRM_FORMAT_ABGR1555:
		utpWrapfmt = UTP_E_GOP_COLOR_ABGR1555;
		break;
	case DRM_FORMAT_BGRA5551:
		utpWrapfmt = UTP_E_GOP_COLOR_BGRA5551;
		break;
	case DRM_FORMAT_BGRA4444:
		utpWrapfmt = UTP_E_GOP_COLOR_BGRA4444;
		break;
	case DRM_FORMAT_XBGR8888:
		utpWrapfmt = UTP_E_GOP_COLOR_XBGR8888;
		break;
	case DRM_FORMAT_UYVY:
		utpWrapfmt = UTP_E_GOP_COLOR_UYVY;
		break;
	case DRM_FORMAT_ARGB2101010:
		utpWrapfmt = UTP_E_GOP_COLOR_A2RGB10;
		break;
	case DRM_FORMAT_ABGR2101010:
		utpWrapfmt = UTP_E_GOP_COLOR_A2BGR10;
		break;
	case DRM_FORMAT_RGBA1010102:
		utpWrapfmt = UTP_E_GOP_COLOR_RGB10A2;
		break;
	case DRM_FORMAT_BGRA1010102:
		utpWrapfmt = UTP_E_GOP_COLOR_BGR10A2;
		break;
	case DRM_FORMAT_XRGB2101010:
		utpWrapfmt = UTP_E_GOP_COLOR_X2RGB10;
		break;
	case DRM_FORMAT_XBGR2101010:
		utpWrapfmt = UTP_E_GOP_COLOR_X2BGR10;
		break;
	case DRM_FORMAT_RGBX1010102:
		utpWrapfmt = UTP_E_GOP_COLOR_RGB10X2;
		break;
	case DRM_FORMAT_BGRX1010102:
		utpWrapfmt = UTP_E_GOP_COLOR_BGR10X2;
		break;
	case DRM_FORMAT_RGB888:
		utpWrapfmt = UTP_E_GOP_COLOR_RGB888;
		break;
	default:
		utpWrapfmt = UTP_E_GOP_COLOR_ARGB8888;
		break;
	}

	return utpWrapfmt;
}

static UTP_EN_GOP_STRETCH_HMODE _convertHStretchMode(const u32 u32HStretchMode)
{
	UTP_EN_GOP_STRETCH_HMODE mode;

	switch (u32HStretchMode) {
	case MTK_DRM_HSTRETCH_DUPLICATE:
		mode = UTP_E_GOP_HSTRCH_DUPLICATE;
		break;
	case MTK_DRM_HSTRETCH_6TAP8PHASE:
		mode = UTP_E_GOP_HSTRCH_6TAPE_LINEAR;
		break;
	case MTK_DRM_HSTRETCH_4TAP256PHASE:
		mode = UTP_E_GOP_HSTRCH_NEW4TAP;
		break;
	default:
		mode = UTP_E_GOP_HSTRCH_INVALID;
		break;
	}

	return mode;
}

static UTP_EN_GOP_STRETCH_VMODE _convertVStretchMode(const u32 u32VStretchMode)
{
	UTP_EN_GOP_STRETCH_VMODE mode;

	switch (u32VStretchMode) {
	case MTK_DRM_VSTRETCH_2TAP16PHASE:
		mode = UTP_E_GOP_VSTRCH_2TAP;
		break;
	case MTK_DRM_VSTRETCH_DUPLICATE:
		mode = UTP_E_GOP_VSTRCH_DUPLICATE;
		break;
	case MTK_DRM_VSTRETCH_BILINEAR:
		mode = UTP_E_GOP_VSTRCH_BILINEAR;
		break;
	case MTK_DRM_VSTRETCH_4TAP:
		mode = UTP_E_GOP_VSTRCH_4TAP;
		break;
	default:
		mode = UTP_E_GOP_VSTRCH_INVALID;
		break;
	}

	return mode;
}

static uint8_t _mtk_drm_graphic_plane_is_ext_prop_updated(
	struct gop_prop *gop_plane_properties,
	enum ext_graphic_plane_prop eprop)
{
	if (eprop < 0 || eprop >= E_GOP_PROP_MAX)
		return false;

	if (gop_plane_properties->prop_update[eprop] == 1) {
		gop_plane_properties->prop_update[eprop] = 0;
		return true;
	} else {
		return false;
	}
}

static uint8_t _mtk_drm_graphic_crtc_is_ext_prop_updated(
	struct ext_crtc_graphic_prop_info *ext_crtc_graphic_prop,
	enum ext_graphic_crtc_prop_type eprop)
{
	if (eprop < 0 || eprop >= E_EXT_CRTC_GRAPHIC_PROP_MAX)
		return false;

	if (ext_crtc_graphic_prop->prop_update[eprop] == 1) {
		return true;
	} else {
		return false;
	}
}

static uint8_t _mtk_drm_video_crtc_is_ext_prop_updated(
	struct ext_crtc_prop_info *ext_crtc_video_prop,
	enum ext_video_crtc_prop_type eprop)
{
	if (eprop < 0 || eprop >= E_EXT_CRTC_PROP_MAX)
		return false;

	if (ext_crtc_video_prop->prop_update[eprop] == 1)
		return true;
	else
		return false;
}

static int _mtk_drm_GOP_check_dts(int ret, const char *name)
{
	if (ret != 0) {
		DRM_ERROR("[GOP][%s, %d]of_property_read_u32 failed, name = %s\n",
				__func__, __LINE__, name);
		return -1;
	}

	return 0;
}

static ssize_t mtk_drm_capability_GOP_Num_show(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n", pctx_gop->gop_plane_capability.u8GOPNum);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_VOP_PATH_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVopPathSel);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_AFBC_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportAFBC[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_HMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHMirror[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_VMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVMirror[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_TransColor_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportTransColor[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_Scroll_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportScroll[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_BlendXcIp_show(struct device *dev,
						      struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlendXcIP[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_CSC_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportCSC[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_HDRMode_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHDRMode[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_HScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHScalingMode[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_VScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVScalingMode[0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_H10V6_Switch_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportH10V6Switch[0]);

	return ssize;
}

static ssize_t mtk_drm_query_GOP_CRC_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	int ssize = 0;
	uint16_t u16Size = 255;
	__u32 CRCvalue = 0;

	gop_wrapper_get_CRC(&CRCvalue);

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%x\n", CRCvalue);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_ENGINE_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;
	int GopEngineMode = 0;

	GopEngineMode = pctx_gop->GopEngineMode;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n", GopEngineMode);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_XVYCC_Lite_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOP0]);

	return ssize;
}

static ssize_t mtk_drm_debug_GOP_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;

	return gop_wrapper_print_status(pctx_gop->GopNum);
}

static ssize_t mtk_drm_GOP_enable_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_GOP_fence_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_AFBC_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportAFBCWidth[GOP0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP0_Max_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportMaxWidth[GOP0]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_Dual_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8bNeedDualGOP);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_Blend2OSDB0_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlend2OSDB0);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_auto_detect_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
		snprintf(buf + ssize, u16Size - ssize, "%d\n",
			pctx_gop->gop_plane_capability.u8SupportAutoDetect);

	return ssize;
}

static ssize_t mtk_drm_GOP_ChipVersion_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->ChipVer);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_OSDB_Location_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	if (!buf) {
		DRM_ERROR("[GOP][%s, %d]null pointer of buf\n",
				__func__, __LINE__);
		return 0;
	}

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportOSDBLocSwitch);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_Multi_Alpha_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	if (!dev) {
		DRM_ERROR("[GOP][%s, %d]null pointer of dev\n",
				__func__, __LINE__);
		return 0;
	}

	if (!buf) {
		DRM_ERROR("[GOP][%s, %d]null pointer of buf\n",
				__func__, __LINE__);
		return 0;
	}
	pctx = dev_get_drvdata(dev);

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx->gop_priv.gop_plane_capability.u8SupportGOPMultiApha);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP_Step2HVSP_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	if (!dev) {
		DRM_ERROR("[GOP][%s, %d]null pointer of dev\n",
				__func__, __LINE__);
		return 0;
	}

	if (!buf) {
		DRM_ERROR("[GOP][%s, %d]null pointer of buf\n",
				__func__, __LINE__);
		return 0;
	}
	pctx = dev_get_drvdata(dev);

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx->gop_priv.gop_plane_capability.u8SupportGOPStep2HVSP);

	return ssize;
}

static ssize_t mtk_drm_GOP_BlendingEn_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_GOP_ForceConstantAlpha_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_GOP_PQEn_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_Num_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_VOP_PATH_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_AFBC_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_HMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_VMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_TransColor_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_Scroll_store(struct device *dev,
						    struct device_attribute *attr, const char *buf,
						    size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_BlendXcIp_store(struct device *dev,
						       struct device_attribute *attr,
						       const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_CSC_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_HDRMode_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_HScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_VScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_H10V6_Switch_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_query_GOP_CRC_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_ENGINE_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_XVYCC_Lite_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_debug_GOP_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_GOP_XVYCC_SRAM_Val_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mtk_drm_GOP_enable_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	char cmd[SYSFILE_MAX_SIZE];
	char *psep, *tmp;
	bool enable = false;
	int len, GOPIdx = 0, InVal = 0, ret = 0, length_needed = 0;
	ST_HW_GOP_SYS_DEBUG_INFO stGOPDebugInfo;

	memset(&stGOPDebugInfo, 0, sizeof(ST_HW_GOP_SYS_DEBUG_INFO));
	len = strlen(buf);

	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n",
			__func__, __LINE__, length_needed);
		return 0;
	}
	tmp = cmd;
	psep = strsep(&tmp, ",");
	ret = kstrtoint(psep, 0, &InVal);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		Debug_GOPOnOFF_Mode = false;
	} else {
		Debug_GOPOnOFF_Mode = (bool)InVal;
	}

	psep = strsep(&tmp, ",");
	ret = kstrtoint(psep, 0, &GOPIdx);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		GOPIdx = 0;
	}

	psep = strsep(&tmp, "\n\r");
	ret = kstrtoint(psep, 0, &InVal);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		enable = false;
	} else {
		enable = (bool)InVal;
	}

	DRM_INFO("[%s][%d]Debug_GOPOnOFF_Mode=%d, GOPIdx=%d, enable=%d\n",
		__func__, __LINE__, Debug_GOPOnOFF_Mode, GOPIdx, enable);

	if (enable == true)
		pctx_gop->DebugForceOn[GOPIdx] = true;
	else
		pctx_gop->DebugForceOn[GOPIdx] = false;

	stGOPDebugInfo.u32SetMode = MTK_GOP_DEBUG_MODE_ENG_EN;
	stGOPDebugInfo.bEnable = enable;
	stGOPDebugInfo.GopIdx = (MS_U8)GOPIdx;
	KDrv_HW_GOP_SetSysDebugInfo(&stGOPDebugInfo);

	return count;
}

static ssize_t mtk_drm_GOP_fence_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	char *psep, *cmd;
	int len = 0, ret = 0, length_needed = 0;
	MS_U32 GOPIdx = 0;

	len = strlen(buf);
	cmd = vmalloc(sizeof(char) * SYSFILE_MAX_SIZE);
	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n",
			__func__, __LINE__, length_needed);
		vfree(cmd);
		return 0;
	}

	psep = strsep(&cmd, "\n\r");
	ret = kstrtoint(psep, 0, &GOPIdx);
	if (GOPIdx >= MAX_GOP_PLANE_NUM) {
		DRM_ERROR("[GOP][%s, %d]Invalid GOP number %d\r\n",
			__func__, __LINE__, GOPIdx);
		vfree(cmd);
		return 0;
	}

	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		DRM_ERROR("[GOP][%s, %d]timeline:NA, timeline value:NA, fence seqno:NA\n",
		__func__, __LINE__);
	} else {
		DRM_INFO("[%s][%d]GOP:%d,timeline name:%s,timeline val:%d,fence seq:%d\n",
		__func__, __LINE__,
		GOPIdx,
		pctx_gop->gop_sync_timeline[GOPIdx]->name,
		pctx_gop->gop_sync_timeline[GOPIdx]->value,
		pctx_gop->mfence[GOPIdx].fence_seqno);
	}

	vfree(cmd);

	return count;
}

static ssize_t mtk_drm_capability_GOP0_AFBC_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP0_Max_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_Dual_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_GOP_BlendingEn_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	char cmd[SYSFILE_MAX_SIZE];
	char *psep, *tmp;
	int len = 0, ret = 0, length_needed = 0, enable = 0;

	len = strlen(buf);
	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n",
			__func__, __LINE__, length_needed);
		return 0;
	}

	tmp = cmd;
	psep = strsep(&tmp, "\n\r");
	ret = kstrtoint(psep, 0, &enable);

	if (ret)
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
	else
		KDrv_HW_GOP_SetOSDBEn((MS_BOOL)enable);

	return count;
}
static ssize_t mtk_drm_GOP_ChipVersion_store(struct device *dev,
								struct device_attribute *attr,
								const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_auto_detect_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_Blend2OSDB0_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_OSDB_Location_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static void mtk_drm_GOP_dump_xvycc_sram(uint8_t *Degammadata, uint8_t *gammadata)
{
	int i = 0, j = 0, channel = 0, dataIndex = 0;

	if (!Degammadata || !gammadata) {
		DRM_ERROR("[GOP][%s, %d]Null pointer fail\n",
			__func__, __LINE__);
		return;
	}

	for (i = 0; i < CHANNEL_NUM; i++) {
		channel = i * DEGAMMA_ENTRY_SIZE * DEGAMMA_ENTRY;
		for (j = 0; j < DEGAMMA_ENTRY; j++) {
			dataIndex = j * DEGAMMA_ENTRY_SIZE;
			if (i == CHANNEL_0) {
				pr_emerg("degamma R[0x%x]=0x%02x%02x%02x%02x\n", j,
					Degammadata[channel + dataIndex + IDX_3],
					Degammadata[channel + dataIndex + IDX_2],
					Degammadata[channel + dataIndex + IDX_1],
					Degammadata[channel + dataIndex]);
			} else if (i == CHANNEL_1) {
				pr_emerg("degamma G[0x%x]=0x%02x%02x%02x%02x\n", j,
					Degammadata[channel + dataIndex + IDX_3],
					Degammadata[channel + dataIndex + IDX_2],
					Degammadata[channel + dataIndex + IDX_1],
					Degammadata[channel + dataIndex]);
			} else if (i == CHANNEL_2) {
				pr_emerg("degamma B[0x%x]=0x%02x%02x%02x%02x\n", j,
					Degammadata[channel + dataIndex + IDX_3],
					Degammadata[channel + dataIndex + IDX_2],
					Degammadata[channel + dataIndex + IDX_1],
					Degammadata[channel + dataIndex]);
			}
		}
	}

	for (i = 0; i < CHANNEL_NUM; i++) {
		channel = i * GAMMA_ENTRY_SIZE * GAMMA_ENTRY;
		for (j = 0; j < GAMMA_ENTRY; j++) {
			dataIndex = j * GAMMA_ENTRY_SIZE;
			if (i == CHANNEL_0)
				pr_emerg("gamma R[0x%x]=0x%02x%02x\n", j,
				gammadata[channel + dataIndex + IDX_1],
				gammadata[channel + dataIndex]);
			else if (i == CHANNEL_1)
				pr_emerg("gamma G[0x%x]=0x%02x%02x\n", j,
				gammadata[channel + dataIndex + IDX_1],
				gammadata[channel + dataIndex]);
			else if (i == CHANNEL_2)
				pr_emerg("gamma B[0x%x]=0x%02x%02x\n", j,
				gammadata[channel + dataIndex + IDX_1],
				gammadata[channel + dataIndex]);
		}
	}
}

static ssize_t mtk_drm_GOP_XVYCC_SRAM_Val_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct mtk_tv_kms_context *pctx;
	struct mtk_gop_context *pctx_gop;
	char cmd[SYSFILE_MAX_SIZE];
	char *psep, *tmp;
	int len, GOPIdx = 0, ret = 0, length_needed = 0;
	uint8_t *Degammadata = NULL, *gammadata = NULL;

	if (!dev) {
		DRM_ERROR("[GOP][%s, %d]Null pointer of dev\n",
			__func__, __LINE__);
		return count;
	}
	pctx = dev_get_drvdata(dev);
	if (!pctx) {
		DRM_ERROR("[GOP][%s, %d]Null pointer of pctx\n",
			__func__, __LINE__);
		return count;
	}
	pctx_gop = &pctx->gop_priv;

	len = strlen(buf);

	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n", __func__,
			__LINE__, length_needed);
		return count;
	}
	tmp = cmd;
	psep = strsep(&tmp, "\n\r");
	ret = kstrtoint(psep, 0, &GOPIdx);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		return count;
	}

	if (GOPIdx >= MAX_GOP_PLANE_NUM || GOPIdx < 0) {
		DRM_ERROR("[GOP][%s, %d]Invalid GOP index:%d\r\n",  __func__, __LINE__, GOPIdx);
		return count;
	}

	if (pctx_gop->gop_plane_capability.u8SupportHDRMode[GOPIdx] == 0 ||
		pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOPIdx] == 1) {
		return count;
	}

	Degammadata = vmalloc(DEGAMMA_SIZE);
	if (Degammadata == NULL) {
		DRM_ERROR("[GOP][%s, %d]Failed to allocate buffer\r\n",  __func__, __LINE__);
		return count;
	}

	gammadata = vmalloc(GAMMA_SIZE);
	if (gammadata == NULL)	{
		vfree(Degammadata);
		DRM_ERROR("[GOP][%s, %d]Failed to allocate buffer\r\n",  __func__, __LINE__);
		return count;
	}

	if (!KDrv_HW_GOP_XVYCC_readSram_degamma((uint8_t)GOPIdx, Degammadata)) {
		vfree(Degammadata);
		vfree(gammadata);
		DRM_ERROR("[GOP][%s, %d]Failed to read degamma\r\n",  __func__, __LINE__);
		return count;
	}

	if (!KDrv_HW_GOP_XVYCC_readSram_gamma((uint8_t)GOPIdx, gammadata)) {
		vfree(Degammadata);
		vfree(gammadata);
		DRM_ERROR("[GOP][%s, %d]Failed to read gamma\r\n",  __func__, __LINE__);
		return count;
	}

	mtk_drm_GOP_dump_xvycc_sram(Degammadata, gammadata);

	vfree(Degammadata);
	vfree(gammadata);

	return count;
}

static ssize_t mtk_drm_capability_GOP_Multi_Alpha_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP_Step2HVSP_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_GOP_ForceConstantAlpha_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	char cmd[SYSFILE_MAX_SIZE];
	char *psep, *tmp;
	bool bEnable;
	int len = 0, GOPIdx = 0, AlphaVal = 0, ForceEn = 0, ret = 0, length_needed = 0;
	ST_HW_GOP_SYS_DEBUG_INFO stGOPDebugInfo;

	memset(&stGOPDebugInfo, 0, sizeof(ST_HW_GOP_SYS_DEBUG_INFO));
	len = strlen(buf);
	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n",
			__func__, __LINE__, length_needed);
		return 0;
	}
	tmp = cmd;

	psep = strsep(&tmp, ",");
	ret = kstrtoint(psep, 0, &GOPIdx);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		return 0;
	}

	psep = strsep(&tmp, ",");
	ret = kstrtoint(psep, 0, &ForceEn);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		return 0;
	}

	bEnable = (bool)ForceEn;

	psep = strsep(&tmp, "\n\r");
	ret = kstrtoint(psep, 0, &AlphaVal);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
		return 0;
	}

	stGOPDebugInfo.u32SetMode = MTK_GOP_DEBUG_MODE_ALPHAVAL;
	stGOPDebugInfo.bForceConstantAlphaEn = bEnable;
	stGOPDebugInfo.GopIdx = (MS_U8)GOPIdx;
	stGOPDebugInfo.u8AlphaVal = (MS_U8)AlphaVal;

	KDrv_HW_GOP_SetSysDebugInfo(&stGOPDebugInfo);

	return count;
}

static ssize_t mtk_drm_GOP_PQEn_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	char cmd[SYSFILE_MAX_SIZE];
	char *psep, *tmp;
	int len = 0, ret = 0, length_needed = 0, enable = 0;
	ST_HW_GOP_SYS_DEBUG_INFO stGOPDebugInfo;

	memset(&stGOPDebugInfo, 0, sizeof(ST_HW_GOP_SYS_DEBUG_INFO));
	len = strlen(buf);
	length_needed = snprintf(cmd, len + 1, "%s", buf);
	if (length_needed < 0 || length_needed >= SYSFILE_MAX_SIZE) {
		DRM_ERROR("[GOP][%s, %d]buffer to small, it need %d\r\n",
			__func__, __LINE__, length_needed);
		return 0;
	}
	tmp = cmd;

	psep = strsep(&tmp, "\n\r");
	ret = kstrtoint(psep, 0, &enable);

	if (!ret) {
		stGOPDebugInfo.u32SetMode = MTK_GOP_DEBUG_MODE_PQEN;
		stGOPDebugInfo.bEnable = (MS_BOOL)enable;
		KDrv_HW_GOP_SetSysDebugInfo(&stGOPDebugInfo);
	} else {
		DRM_ERROR("[GOP][%s, %d]kstrtoint fail, ret:%d\r\n",  __func__, __LINE__, ret);
	}

	return count;
}

static DEVICE_ATTR_RW(mtk_drm_capability_GOP_Num);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_VOP_PATH);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_AFBC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_HMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_VMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_TransColor);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_Scroll);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_BlendXcIp);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_CSC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_HDRMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_HScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_VScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_H10V6_Switch);
static DEVICE_ATTR_RW(mtk_drm_query_GOP_CRC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_ENGINE);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_XVYCC_Lite);
static DEVICE_ATTR_RW(mtk_drm_debug_GOP);
static DEVICE_ATTR_RW(mtk_drm_GOP_enable);
static DEVICE_ATTR_RW(mtk_drm_GOP_fence);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_AFBC_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP0_Max_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_Dual);
static DEVICE_ATTR_RW(mtk_drm_GOP_BlendingEn);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_Blend2OSDB0);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_auto_detect);
static DEVICE_ATTR_RW(mtk_drm_GOP_ChipVersion);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_OSDB_Location);
static DEVICE_ATTR_RW(mtk_drm_GOP_XVYCC_SRAM_Val);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_Multi_Alpha);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP_Step2HVSP);
static DEVICE_ATTR_RW(mtk_drm_GOP_ForceConstantAlpha);
static DEVICE_ATTR_RW(mtk_drm_GOP_PQEn);

static struct attribute *mtk_tv_drm_gop0_attrs[] = {
	&dev_attr_mtk_drm_capability_GOP_Num.attr,
	&dev_attr_mtk_drm_capability_GOP_VOP_PATH.attr,
	&dev_attr_mtk_drm_capability_GOP0_AFBC.attr,
	&dev_attr_mtk_drm_capability_GOP0_HMirror.attr,
	&dev_attr_mtk_drm_capability_GOP0_VMirror.attr,
	&dev_attr_mtk_drm_capability_GOP0_TransColor.attr,
	&dev_attr_mtk_drm_capability_GOP0_Scroll.attr,
	&dev_attr_mtk_drm_capability_GOP0_BlendXcIp.attr,
	&dev_attr_mtk_drm_capability_GOP0_CSC.attr,
	&dev_attr_mtk_drm_capability_GOP0_HDRMode.attr,
	&dev_attr_mtk_drm_capability_GOP0_HScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP0_VScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP0_H10V6_Switch.attr,
	&dev_attr_mtk_drm_query_GOP_CRC.attr,
	&dev_attr_mtk_drm_capability_GOP_ENGINE.attr,
	&dev_attr_mtk_drm_capability_GOP0_XVYCC_Lite.attr,
	&dev_attr_mtk_drm_capability_GOP0_AFBC_Width.attr,
	&dev_attr_mtk_drm_capability_GOP_Dual.attr,
	&dev_attr_mtk_drm_debug_GOP.attr,
	&dev_attr_mtk_drm_GOP_enable.attr,
	&dev_attr_mtk_drm_GOP_fence.attr,
	&dev_attr_mtk_drm_GOP_BlendingEn.attr,
	&dev_attr_mtk_drm_capability_GOP_Blend2OSDB0.attr,
	&dev_attr_mtk_drm_capability_GOP_auto_detect.attr,
	&dev_attr_mtk_drm_GOP_ChipVersion.attr,
	&dev_attr_mtk_drm_capability_GOP_OSDB_Location.attr,
	&dev_attr_mtk_drm_capability_GOP0_Max_Width.attr,
	&dev_attr_mtk_drm_GOP_XVYCC_SRAM_Val.attr,
	&dev_attr_mtk_drm_capability_GOP_Multi_Alpha.attr,
	&dev_attr_mtk_drm_capability_GOP_Step2HVSP.attr,
	&dev_attr_mtk_drm_GOP_ForceConstantAlpha.attr,
	&dev_attr_mtk_drm_GOP_PQEn.attr,
	NULL,
};

static const struct attribute_group mtk_tv_drm_gop0_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_gop0_attrs
};


static ssize_t mtk_drm_capability_GOP1_AFBC_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportAFBC[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_HMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHMirror[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_VMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVMirror[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_TransColor_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportTransColor[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_Scroll_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportScroll[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_BlendXcIp_show(struct device *dev,
						      struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlendXcIP[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_CSC_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportCSC[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_HDRMode_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHDRMode[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_HScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHScalingMode[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_VScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVScalingMode[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_H10V6_Switch_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportH10V6Switch[1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_XVYCC_Lite_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOP1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_AFBC_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportAFBCWidth[GOP1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_Max_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportMaxWidth[GOP1]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP1_AFBC_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_HMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_VMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_TransColor_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_Scroll_store(struct device *dev,
						    struct device_attribute *attr, const char *buf,
						    size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_BlendXcIp_store(struct device *dev,
						       struct device_attribute *attr,
						       const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_CSC_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_HDRMode_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_HScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_VScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_H10V6_Switch_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_XVYCC_Lite_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_AFBC_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP1_Max_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_AFBC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_HMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_VMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_TransColor);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_Scroll);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_BlendXcIp);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_CSC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_HDRMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_HScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_VScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_H10V6_Switch);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_XVYCC_Lite);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_AFBC_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP1_Max_Width);

static struct attribute *mtk_tv_drm_gop1_attrs[] = {
	&dev_attr_mtk_drm_capability_GOP1_AFBC.attr,
	&dev_attr_mtk_drm_capability_GOP1_HMirror.attr,
	&dev_attr_mtk_drm_capability_GOP1_VMirror.attr,
	&dev_attr_mtk_drm_capability_GOP1_TransColor.attr,
	&dev_attr_mtk_drm_capability_GOP1_Scroll.attr,
	&dev_attr_mtk_drm_capability_GOP1_BlendXcIp.attr,
	&dev_attr_mtk_drm_capability_GOP1_CSC.attr,
	&dev_attr_mtk_drm_capability_GOP1_HDRMode.attr,
	&dev_attr_mtk_drm_capability_GOP1_HScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP1_VScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP1_H10V6_Switch.attr,
	&dev_attr_mtk_drm_capability_GOP1_XVYCC_Lite.attr,
	&dev_attr_mtk_drm_capability_GOP1_AFBC_Width.attr,
	&dev_attr_mtk_drm_capability_GOP1_Max_Width.attr,
	NULL,
};

static const struct attribute_group mtk_tv_drm_gop1_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_gop1_attrs
};

static ssize_t mtk_drm_capability_GOP2_AFBC_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportAFBC[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_HMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHMirror[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_VMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVMirror[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_TransColor_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportTransColor[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_Scroll_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportScroll[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_BlendXcIp_show(struct device *dev,
						      struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlendXcIP[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_CSC_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportCSC[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_HDRMode_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHDRMode[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_HScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHScalingMode[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_VScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVScalingMode[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_H10V6_Switch_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportH10V6Switch[2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_XVYCC_Lite_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOP2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_AFBC_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportAFBCWidth[GOP2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_Max_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportMaxWidth[GOP2]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP2_AFBC_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_HMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_VMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_TransColor_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_Scroll_store(struct device *dev,
						    struct device_attribute *attr, const char *buf,
						    size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_BlendXcIp_store(struct device *dev,
						       struct device_attribute *attr,
						       const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_CSC_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_HDRMode_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_HScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_VScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_H10V6_Switch_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_XVYCC_Lite_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_AFBC_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP2_Max_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_AFBC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_HMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_VMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_TransColor);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_Scroll);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_BlendXcIp);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_CSC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_HDRMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_HScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_VScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_H10V6_Switch);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_XVYCC_Lite);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_AFBC_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP2_Max_Width);

static struct attribute *mtk_tv_drm_gop2_attrs[] = {
	&dev_attr_mtk_drm_capability_GOP2_AFBC.attr,
	&dev_attr_mtk_drm_capability_GOP2_HMirror.attr,
	&dev_attr_mtk_drm_capability_GOP2_VMirror.attr,
	&dev_attr_mtk_drm_capability_GOP2_TransColor.attr,
	&dev_attr_mtk_drm_capability_GOP2_Scroll.attr,
	&dev_attr_mtk_drm_capability_GOP2_BlendXcIp.attr,
	&dev_attr_mtk_drm_capability_GOP2_CSC.attr,
	&dev_attr_mtk_drm_capability_GOP2_HDRMode.attr,
	&dev_attr_mtk_drm_capability_GOP2_HScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP2_VScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP2_H10V6_Switch.attr,
	&dev_attr_mtk_drm_capability_GOP2_XVYCC_Lite.attr,
	&dev_attr_mtk_drm_capability_GOP2_AFBC_Width.attr,
	&dev_attr_mtk_drm_capability_GOP2_Max_Width.attr,
	NULL,
};


static const struct attribute_group mtk_tv_drm_gop2_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_gop2_attrs
};

static ssize_t mtk_drm_capability_GOP3_AFBC_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportAFBC[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_HMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHMirror[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_VMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVMirror[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_TransColor_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportTransColor[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_Scroll_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportScroll[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_BlendXcIp_show(struct device *dev,
						      struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlendXcIP[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_CSC_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportCSC[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_HDRMode_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHDRMode[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_HScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHScalingMode[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_VScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVScalingMode[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_H10V6_Switch_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportH10V6Switch[3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_XVYCC_Lite_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOP3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_AFBC_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportAFBCWidth[GOP3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_Max_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportMaxWidth[GOP3]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP3_AFBC_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_HMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_VMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_TransColor_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_Scroll_store(struct device *dev,
						    struct device_attribute *attr, const char *buf,
						    size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_BlendXcIp_store(struct device *dev,
						       struct device_attribute *attr,
						       const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_CSC_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_HDRMode_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_HScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_VScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_H10V6_Switch_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_XVYCC_Lite_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_AFBC_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP3_Max_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_AFBC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_HMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_VMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_TransColor);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_Scroll);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_BlendXcIp);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_CSC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_HDRMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_HScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_VScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_H10V6_Switch);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_XVYCC_Lite);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_AFBC_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP3_Max_Width);

static struct attribute *mtk_tv_drm_gop3_attrs[] = {
	&dev_attr_mtk_drm_capability_GOP3_AFBC.attr,
	&dev_attr_mtk_drm_capability_GOP3_HMirror.attr,
	&dev_attr_mtk_drm_capability_GOP3_VMirror.attr,
	&dev_attr_mtk_drm_capability_GOP3_TransColor.attr,
	&dev_attr_mtk_drm_capability_GOP3_Scroll.attr,
	&dev_attr_mtk_drm_capability_GOP3_BlendXcIp.attr,
	&dev_attr_mtk_drm_capability_GOP3_CSC.attr,
	&dev_attr_mtk_drm_capability_GOP3_HDRMode.attr,
	&dev_attr_mtk_drm_capability_GOP3_HScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP3_VScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP3_H10V6_Switch.attr,
	&dev_attr_mtk_drm_capability_GOP3_XVYCC_Lite.attr,
	&dev_attr_mtk_drm_capability_GOP3_AFBC_Width.attr,
	&dev_attr_mtk_drm_capability_GOP3_Max_Width.attr,
	NULL,
};

static const struct attribute_group mtk_tv_drm_gop3_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_gop3_attrs
};

static ssize_t mtk_drm_capability_GOP4_AFBC_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportAFBC[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_HMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHMirror[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_VMirror_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVMirror[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_TransColor_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportTransColor[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_Scroll_show(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportScroll[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_BlendXcIp_show(struct device *dev,
						      struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportBlendXcIP[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_CSC_show(struct device *dev, struct device_attribute *attr,
						char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportCSC[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_HDRMode_show(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHDRMode[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_HScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportHScalingMode[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_VScaleMode_show(struct device *dev,
						       struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportVScalingMode[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_H10V6_Switch_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = 255;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportH10V6Switch[4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_XVYCC_Lite_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u8SupportXvyccLite[GOP4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_AFBC_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportAFBCWidth[GOP4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_Max_Width_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int ssize = 0;
	uint16_t u16Size = SYSFILE_MAX_SIZE;

	ssize +=
	    snprintf(buf + ssize, u16Size - ssize, "%d\n",
		     pctx_gop->gop_plane_capability.u32SupportMaxWidth[GOP4]);

	return ssize;
}

static ssize_t mtk_drm_capability_GOP4_AFBC_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_HMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_VMirror_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_TransColor_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_Scroll_store(struct device *dev,
						    struct device_attribute *attr, const char *buf,
						    size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_BlendXcIp_store(struct device *dev,
						       struct device_attribute *attr,
						       const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_CSC_store(struct device *dev, struct device_attribute *attr,
						 const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_HDRMode_store(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_HScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_VScaleMode_store(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_H10V6_Switch_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_XVYCC_Lite_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_AFBC_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static ssize_t mtk_drm_capability_GOP4_Max_Width_store(struct device *dev,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_AFBC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_HMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_VMirror);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_TransColor);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_Scroll);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_BlendXcIp);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_CSC);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_HDRMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_HScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_VScaleMode);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_H10V6_Switch);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_XVYCC_Lite);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_AFBC_Width);
static DEVICE_ATTR_RW(mtk_drm_capability_GOP4_Max_Width);

static struct attribute *mtk_tv_drm_gop4_attrs[] = {
	&dev_attr_mtk_drm_capability_GOP4_AFBC.attr,
	&dev_attr_mtk_drm_capability_GOP4_HMirror.attr,
	&dev_attr_mtk_drm_capability_GOP4_VMirror.attr,
	&dev_attr_mtk_drm_capability_GOP4_TransColor.attr,
	&dev_attr_mtk_drm_capability_GOP4_Scroll.attr,
	&dev_attr_mtk_drm_capability_GOP4_BlendXcIp.attr,
	&dev_attr_mtk_drm_capability_GOP4_CSC.attr,
	&dev_attr_mtk_drm_capability_GOP4_HDRMode.attr,
	&dev_attr_mtk_drm_capability_GOP4_HScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP4_VScaleMode.attr,
	&dev_attr_mtk_drm_capability_GOP4_H10V6_Switch.attr,
	&dev_attr_mtk_drm_capability_GOP4_XVYCC_Lite.attr,
	&dev_attr_mtk_drm_capability_GOP4_AFBC_Width.attr,
	&dev_attr_mtk_drm_capability_GOP4_Max_Width.attr,
	NULL,
};

static const struct attribute_group mtk_tv_drm_gop4_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_tv_drm_gop4_attrs
};

void mtk_drm_gop_get_capability(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	int ret = 0, i = 0;
	struct device_node *np;
	char cstr[100] = "";
	const char *name;
	uint32_t u32CapVal = 0;
	uint8_t u8GOPNum;

	snprintf(cstr, sizeof(cstr), "mediatek,mtk-dtv-gop%d", 0);
	np = of_find_compatible_node(NULL, NULL, cstr);
	if (np != NULL) {
		name = MTK_DRM_DTS_GOP_CAPABILITY_TAG;
		ret = of_property_read_u32(np, name, &u32CapVal);
		if (ret != 0x0)
			DRM_ERROR("[GOP][%s, %d]: of_property_read_u32 failed, name = %s \r\n",
				  __func__, __LINE__, name);
	}

	u8GOPNum = (uint8_t) (u32CapVal & MTK_GOP_CAPS_OFFSET_GOPNUM_MSK);
	pctx_gop->gop_plane_capability.u8GOPNum = u8GOPNum;

	pctx_gop->gop_plane_capability.u8IsDramAddrWordUnit =
	    (uint8_t) ((u32CapVal & BIT(MTK_GOP_CAPS_OFFSET_DRAMADDR_ISWORD_UNIT)) >>
		       MTK_GOP_CAPS_OFFSET_DRAMADDR_ISWORD_UNIT);
	pctx_gop->gop_plane_capability.u8IsPixelBaseHorizontal =
	    (uint8_t) ((u32CapVal & BIT(MTK_GOP_CAPS_OFFSET_H_PIXELBASE)) >>
		       MTK_GOP_CAPS_OFFSET_H_PIXELBASE);
	pctx_gop->gop_plane_capability.u8SupportVopPathSel =
	    (uint8_t) ((u32CapVal & BIT(MTK_GOP_CAPS_OFFSET_VOP_PATHSEL)) >>
		       MTK_GOP_CAPS_OFFSET_VOP_PATHSEL);

	name = MTK_DRM_DTS_GOP_CAPABILITY_TAG;
	for (i = 0; i < u8GOPNum; i++) {
		snprintf(cstr, sizeof(cstr), "mediatek,mtk-dtv-gop%d", i);
		np = of_find_compatible_node(NULL, NULL, cstr);
		if (of_property_read_u32(np, name, &u32CapVal) != 0)
			DRM_ERROR("[GOP][%s, %d]: of_property_read_u32 failed, name = %s \r\n",
				  __func__, __LINE__, name);

		pctx_gop->gop_plane_capability.u8SupportAFBC[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsAFBC[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsAFBC[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportHMirror[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsHMirror[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsHMirror[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportVMirror[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsVMirror[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsVMirror[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportTransColor[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsTransColor[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsTransColor[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportScroll[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsScroll[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsScroll[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportBlendXcIP[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsBlendXcIp[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsBlendXcIp[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportCSC[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsCSC[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsCSC[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportHDRMode[i] =
		    (uint8_t) ((u32CapVal & _gu32CapsHDRMode[((i != 0) ? 1 : 0)]) >>
			       _gu32CapsHDRModeSft[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportHScalingMode[i] =
		    (uint8_t) ((u32CapVal & _gu32CapsHScaleMode[((i != 0) ? 1 : 0)]) >>
			       _gu32CapsHScaleSft[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportVScalingMode[i] =
		    (uint8_t) ((u32CapVal & _gu32CapsVScaleMode[((i != 0) ? 1 : 0)]) >>
			       _gu32CapsVScaleSft[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportH10V6Switch[i] =
		    (uint8_t) ((u32CapVal & BIT(_gu32CapsH10V6Switch[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsH10V6Switch[((i != 0) ? 1 : 0)]);
		pctx_gop->gop_plane_capability.u8SupportXvyccLite[i] =
			(uint8_t) ((u32CapVal & BIT(_gu32CapsXVYCC_Lite[((i != 0) ? 1 : 0)])) >>
			       _gu32CapsXVYCC_Lite[((i != 0) ? 1 : 0)]);
	}

}

void mtk_drm_gop_CreateSysFile(struct device *dev)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int i = 0;
	int ret = 0;

	mtk_drm_gop_get_capability(dev, pctx_gop);

	for (i = 0; i < pctx_gop->gop_plane_capability.u8GOPNum; i++) {
		if (i == 0) {
			ret = sysfs_update_group(&dev->kobj,
				&mtk_tv_drm_gop0_attr_group);
			if (ret)
				dev_err(dev,
				"[%d]Fail to update sysfs files: %d\n",
				__LINE__, ret);
		} else if (i == 1) {
			ret = sysfs_update_group(&dev->kobj,
				&mtk_tv_drm_gop1_attr_group);
			if (ret)
				dev_err(dev,
				"[%d]Fail to update sysfs files: %d\n",
				__LINE__, ret);
		} else if (i == 2) {
			ret = sysfs_update_group(&dev->kobj,
				&mtk_tv_drm_gop2_attr_group);
			if (ret)
				dev_err(dev,
				"[%d]Fail to update sysfs files: %d\n",
				__LINE__, ret);
		} else if (i == 3) {
			ret = sysfs_update_group(&dev->kobj,
				&mtk_tv_drm_gop3_attr_group);
			if (ret)
				dev_err(dev,
				"[%d]Fail to update sysfs files: %d\n",
				__LINE__, ret);
		} else if (i == 4) {
			ret = sysfs_update_group(&dev->kobj,
				&mtk_tv_drm_gop4_attr_group);
			if (ret)
				dev_err(dev,
				"[%d]Fail to update sysfs files: %d\n",
				__LINE__, ret);
		}
	}
}

int mtk_gop_setVgRatio(__u32  GVreqDivRatio)
{
	struct hwregOut paramOut;
	struct reg_info reg[1];
	int ret = 0;
	bool bRIU = true, bEnable = 0;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	if (GVreqDivRatio == VG_REQ_DIV_RATIO_2)
		bEnable = 1;
	else
		bEnable = 0;

	paramOut.reg = reg;
	//enable GOP ref mask
	//BKA3A3_h13[0] = enable
	ret = drv_hwreg_render_pnl_tgen_set_gop_ref_mask(
		&paramOut,
		bRIU,
		bEnable);

	return ret;
}

int mtk_gop_setTgenFreeRun(struct mtk_gop_context *gop_priv,
	uint64_t prop_val, uint8_t mlResIdx)
{
	struct drm_mtk_tv_graphic_path_mode *pstVGSepMode;

	if (!gop_priv) {
		DRM_ERROR("[%s][%d]Null pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	pstVGSepMode = &gop_priv->stVGSepMode;
	if (gop_wrapper_set_TgenFreeRun(pstVGSepMode->VGsync_mode, mlResIdx)) {
		DRM_ERROR("[%s][%d]gop_wrapper_set_TgenFreeRun fail!\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static int _mtk_gop_get_ml_memIdx(uint8_t mlResIdx)
{

	if (gop_use_ml) {
		if (gop_wrapper_ml_get_mem_info(mlResIdx)) {
			DRM_ERROR("[GOP][%s, %d] gop_wrapper_all_ml_start fail\n",
						__func__, __LINE__);
			return -EINVAL;
		}
	}

	return 0;
}

static enum EN_GOP_RESULT _mtk_gop_ml_fire(uint8_t mlResIdx)
{
	enum EN_GOP_RESULT ret = E_GOP_WRAP_SUCCESS;

	if (gop_use_ml) {
		ret = gop_wrapper_ml_fire(mlResIdx);
		if (ret != E_GOP_WRAP_SUCCESS)
			DRM_ERROR("[GOP][%s, %d] gop_wrapper_ml_fire fail\n",
						__func__, __LINE__);
	}

	return ret;
}

int mtk_gop_set_video_crtc_property(
	struct ext_crtc_prop_info *video_crtc_properties,
	uint8_t mlResIdx)
{
	int ret = 0;
	EN_GOP_WRAP_VIDEO_ORDER_TYPE VGOrder = E_GOP_WRAP_VIDEO_OSDB0_OSDB1;
	EN_GOP_WRAP_OSDB_LOCATION_TYPE OsdbLoc = E_GOP_WRAP_PQGAMMA_OSDB_LD_PADJ;

	if (_mtk_drm_video_crtc_is_ext_prop_updated(
		video_crtc_properties, E_EXT_CRTC_PROP_GOP_VIDEO_ORDER)) {

		switch (video_crtc_properties->prop_val[E_EXT_CRTC_PROP_GOP_VIDEO_ORDER]) {
		case VIDEO_OSDB0_OSDB1:
			VGOrder = E_GOP_WRAP_VIDEO_OSDB0_OSDB1;
			break;
		case OSDB0_VIDEO_OSDB1:
			VGOrder = E_GOP_WRAP_OSDB0_VIDEO_OSDB1;
			break;
		case OSDB0_OSDB1_VIDEO:
			VGOrder = E_GOP_WRAP_OSDB0_OSDB1_VIDEO;
			break;
		default:
			VGOrder = E_GOP_WRAP_VIDEO_OSDB0_OSDB1;
			break;
		}
		ret |= gop_wrapper_set_vg_order(VGOrder, mlResIdx);
	}

	if (_mtk_drm_video_crtc_is_ext_prop_updated(
		video_crtc_properties, E_EXT_CRTC_PROP_GOP_OSDB_LOCATION)) {

		switch (video_crtc_properties->prop_val[E_EXT_CRTC_PROP_GOP_OSDB_LOCATION]) {
		case PQGAMMA_OSDB_LD_PADJ:
			OsdbLoc = E_GOP_WRAP_PQGAMMA_OSDB_LD_PADJ;
			break;
		case PQGAMMA_LD_OSDB_PADJ:
			OsdbLoc = E_GOP_WRAP_PQGAMMA_LD_OSDB_PADJ;
			break;
		case OSDB_PQGAMMA_LD_PADJ:
			OsdbLoc = E_GOP_WRAP_OSDB_PQGAMMA_LD_PADJ;
			break;
		case OSDB_LD_PQGAMMA_PADJ:
			OsdbLoc = E_GOP_WRAP_OSDB_LD_PQGAMMA_PADJ;
			break;
		case LD_PQGAMMA_OSDB_PADJ:
			OsdbLoc = E_GOP_WRAP_LD_PQGAMMA_OSDB_PADJ;
			break;
		case LD_OSDB_PQGAMMA_PADJ:
			OsdbLoc = E_GOP_WRAP_LD_OSDB_PQGAMMA_PADJ;
			break;
		default:
			OsdbLoc = E_GOP_WRAP_LOCATION_MAX;
			break;
		}
		ret |= gop_wrapper_set_osdb_loc(OsdbLoc, mlResIdx);
	}

	return ret;
}

int mtk_gop_set_graphic_crtc_property(
	struct mtk_tv_kms_context *pctx,
	struct ext_crtc_graphic_prop_info *graphic_crtc_properties,
	uint8_t mlResIdx)
{
	int ret = 0;

	if (!pctx) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return -ENXIO;
	}

	if (_mtk_drm_graphic_crtc_is_ext_prop_updated(
		graphic_crtc_properties, E_EXT_CRTC_GRAPHIC_PROP_MODE)) {

		ret |= mtk_gop_setTgenFreeRun(&pctx->gop_priv,
			graphic_crtc_properties->prop_val[E_EXT_CRTC_GRAPHIC_PROP_MODE],
			mlResIdx);
	}

	return ret;
}

bool mtk_gop_check_crtc_IsUpdated(struct mtk_tv_drm_crtc *mcrtc)
{
	struct mtk_tv_kms_context *pctx;
	struct ext_crtc_prop_info *video_crtc_properties;
	struct ext_crtc_graphic_prop_info *graphic_crtc_properties;
	int i = 0;
	bool bUpdate = false;

	if (!mcrtc) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return false;
	}
	pctx = (struct mtk_tv_kms_context *)mcrtc->crtc_private;

	if (!pctx) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return false;
	}
	video_crtc_properties = pctx->ext_crtc_properties;
	graphic_crtc_properties = pctx->ext_crtc_graphic_properties;

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		for (i = E_EXT_CRTC_PROP_GOP_VIDEO_ORDER;
			i <= E_EXT_CRTC_PROP_GOP_OSDB_LOCATION; i++) {
			if (_mtk_drm_video_crtc_is_ext_prop_updated(
				video_crtc_properties, i)) {
				return true;
			}
		}
	}

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		if (_mtk_drm_graphic_crtc_is_ext_prop_updated(
			graphic_crtc_properties, E_EXT_CRTC_GRAPHIC_PROP_MODE)) {
			return true;
		}
	}

	return bUpdate;
}

bool mtk_gop_check_plane_IsUpdated(
	struct mtk_tv_drm_crtc *mcrtc,
	struct drm_crtc_state *crtc_state)
{
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct mtk_drm_plane *mplane;
	bool bUpdate = false;
	unsigned int i = 0;

	if (!crtc_state) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return false;
	}

	for_each_oldnew_plane_in_state(crtc_state->state, plane,
		old_plane_state, new_plane_state, i) {
		mplane = to_mtk_plane(plane);
		mplane->gop_use_ml_res_idx = mtk_gop_get_ml_syncId(mcrtc);
		if ((mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC ||
			plane->type == DRM_PLANE_TYPE_PRIMARY)) {
			bUpdate = true;
		}
	}

	return bUpdate;
}

EN_GOP_WRAP_ML_SYNC mtk_gop_get_ml_syncId(
	struct mtk_tv_drm_crtc *mcrtc)
{
	EN_GOP_WRAP_ML_SYNC MlSyncId = E_GOP_WRAP_ML_DISP_SYNC;
	struct mtk_tv_kms_context *pkms = NULL;
	struct mtk_gop_context *pctx_gop = NULL;
	uint8_t u8SupportBlendVideoIrq = 0;

	if (!mcrtc) {
		DRM_ERROR("[GOP][%s, %d] null pointer of mcrtc\n", __func__, __LINE__);
		return MlSyncId;
	}

	pkms = (struct mtk_tv_kms_context *)mcrtc->crtc_private;
	if (!pkms) {
		DRM_ERROR("[GOP][%s, %d] null pointer of kms\n", __func__, __LINE__);
		return MlSyncId;
	}
	pctx_gop = &pkms->gop_priv;
	if (!pctx_gop) {
		DRM_ERROR("[GOP][%s, %d] null pointer of pctx_gop\n", __func__, __LINE__);
		return MlSyncId;
	}

	u8SupportBlendVideoIrq = pctx_gop->gop_plane_capability.u8SupportBlendVideoIrq;

	if (u8SupportBlendVideoIrq) {
		MlSyncId = E_GOP_WRAP_ML_OSD_SYNC;
	} else {
		if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO)
			MlSyncId = E_GOP_WRAP_ML_DISP_SYNC;
		else
			MlSyncId = E_GOP_WRAP_ML_OSD_SYNC;
	}

	return MlSyncId;
}

int mtk_gop_crtc_begin(struct mtk_tv_drm_crtc *mcrtc,
	struct drm_crtc_state *crtc_state)
{
	struct mtk_tv_kms_context *pctx;
	struct ext_crtc_prop_info *video_crtc_properties;
	struct ext_crtc_graphic_prop_info *graphic_crtc_properties;

	bool bCrtcPropUpdated = false, bPlanePropUpdated = false;
	int retry_count = 0;
	uint8_t mlResIdx = mtk_gop_get_ml_syncId(mcrtc);

	if (!mcrtc) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return -ENXIO;
	}
	pctx = (struct mtk_tv_kms_context *)mcrtc->crtc_private;

	if (!pctx) {
		DRM_ERROR("[GOP][%s, %d] null pointer\n", __func__, __LINE__);
		return -ENXIO;
	}
	video_crtc_properties = pctx->ext_crtc_properties;
	graphic_crtc_properties = pctx->ext_crtc_graphic_properties;

	bCrtcPropUpdated = mtk_gop_check_crtc_IsUpdated(mcrtc);
	bPlanePropUpdated = mtk_gop_check_plane_IsUpdated(
		mcrtc, crtc_state);

	if (bCrtcPropUpdated || bPlanePropUpdated) {
		while (_mtk_gop_get_ml_memIdx(mlResIdx) && retry_count < MAX_RETRY) {
			DRM_ERROR("[%s, %d] retry ml\n", __func__, __LINE__);
			retry_count++;
			_delaytask(1);
		}

		if (retry_count == MAX_RETRY)
			return -EINVAL;
	}

	return 0;
}

int  mtk_gop_crtc_flush(
	struct mtk_tv_drm_crtc *mcrtc,
	struct drm_crtc_state *crtc_state)
{
	int i = 0, ret = 0;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct mtk_drm_plane *mplane;
	uint8_t mlResIdx = mtk_gop_get_ml_syncId(mcrtc);
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mcrtc->crtc_private;
	struct ext_crtc_prop_info *v_crtc_pro =
		pctx->ext_crtc_properties;
	struct ext_crtc_graphic_prop_info *gfx_crtc_pro =
		pctx->ext_crtc_graphic_properties;
	bool bCrtcPropUpdated = false, bPlanePropUpdated = false;
	int retry_count = 0;


	for_each_oldnew_plane_in_state(crtc_state->state, plane,
		old_plane_state, new_plane_state, i) {
		mplane = to_mtk_plane(plane);
		if ((mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_GRAPHIC ||
			plane->type == DRM_PLANE_TYPE_PRIMARY)) {
			if (gop_wrapper_setLayer(mplane->gop_index, mlResIdx)) {
				DRM_ERROR("[GOP]gop set layer fail\n");
				ret = -EINVAL;
			}
			break;
		}
	}

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		ret |= mtk_gop_set_video_crtc_property(v_crtc_pro,
			mlResIdx);
	}

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		ret |= mtk_gop_set_graphic_crtc_property(pctx,
			gfx_crtc_pro, mlResIdx);
	}

	bCrtcPropUpdated = mtk_gop_check_crtc_IsUpdated(mcrtc);
	bPlanePropUpdated = mtk_gop_check_plane_IsUpdated(mcrtc, crtc_state);

	if (bCrtcPropUpdated || bPlanePropUpdated) {
		while ((_mtk_gop_ml_fire(mlResIdx) == E_GOP_WRAP_MENULOAD_RETRY)
			&& (retry_count < MAX_RETRY)) {
			DRM_ERROR("[%s, %d] retry ml fire\n", __func__, __LINE__);
			retry_count++;
			_delaytask(1);
		}
		if (retry_count == MAX_RETRY)
			return -EINVAL;
	}

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_VIDEO) {
		for (i = E_EXT_CRTC_PROP_GOP_VIDEO_ORDER;
				i <= E_EXT_CRTC_PROP_GOP_OSDB_LOCATION; i++) {
			if (_mtk_drm_video_crtc_is_ext_prop_updated(v_crtc_pro, i))
				v_crtc_pro->prop_update[i] = 0;
		}
	}

	if (mcrtc->mtk_crtc_type == MTK_DRM_CRTC_TYPE_GRAPHIC) {
		if (_mtk_drm_graphic_crtc_is_ext_prop_updated(
			gfx_crtc_pro, E_EXT_CRTC_GRAPHIC_PROP_MODE)) {
			gfx_crtc_pro->prop_update[E_EXT_CRTC_GRAPHIC_PROP_MODE]
				= 0;
		}
	}

	return ret;
}

int mtk_gop_enable_vblank(struct mtk_tv_drm_crtc *crtc)
{
	UTP_GOP_SET_ENABLE SetEnable;

	SetEnable.gop_idx = 0;
	SetEnable.bEn = true;

	utp_gop_intr_ctrl(&SetEnable);
	return 0;
}

int mtk_gop_get_alpha_blob(struct mtk_tv_kms_context *pctx, UTP_GOP_SET_WINDOW *pGOPInfo,
	uint64_t val)
{
	struct drm_property_blob *property_blob = NULL;
	struct drm_mtk_tv_graphic_alpha_mode stGfxAlphaInfo;

	memset(&stGfxAlphaInfo, 0, sizeof(struct drm_mtk_tv_graphic_alpha_mode));

	property_blob = drm_property_lookup_blob(pctx->drm_dev, val);
	if (property_blob != NULL) {
		memcpy(&stGfxAlphaInfo, property_blob->data, property_blob->length);
		pGOPInfo->stGopAlphaMode.bEnPixelAlpha = stGfxAlphaInfo.bEnPixelAlpha;
		pGOPInfo->stGopAlphaMode.u8Coef = stGfxAlphaInfo.u8ConstantAlphaVal;
		pGOPInfo->stGopAlphaMode.bEnMultiAlpha = stGfxAlphaInfo.bEnMultiAlpha;
		drm_property_blob_put(property_blob);
	} else {
		DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
			__func__, __LINE__, (ptrdiff_t)val);
		return -EINVAL;
	}

	return 0;
}

bool mtk_gop_check_overscan(struct mtk_tv_kms_context *pkms)
{
	struct mtk_pixelshift_context *pixelshift_priv = NULL;
	bool IsOverScan = false;

	if (!pkms) {
		DRM_ERROR("[GOP][%s, %d]: null pointer of pkms\n", __func__, __LINE__);
		return false;
	}
	pixelshift_priv = &pkms->pixelshift_priv;

	if (!pixelshift_priv) {
		DRM_ERROR("[GOP][%s, %d]: null pointer of pixelshift_priv\n", __func__, __LINE__);
		return false;
	}

	if (pixelshift_priv->bIsOLEDPixelshiftEnable &&
		pixelshift_priv->u8OLEDPixleshiftType == VIDEO_CRTC_PIXELSHIFT_PRE_OVERSCAN)
		IsOverScan = true;
	else
		IsOverScan = false;

	return IsOverScan;
}

void mtk_gop_set_display_coordinate(struct mtk_plane_state *state, UTP_GOP_SET_WINDOW *pGOPInfo,
	struct mtk_gop_context *pctx_gop, struct mtk_tv_kms_context *pkms,
	__u32 panelW, __u32 panelH)
{
	bool support_HMirror, support_VMirror, bOverScan;
	struct gop_prop *gop_properties;
	uint32_t overscan_w = 0, overscan_h = 0;
	uint32_t mixer4_w = 0, mixer4_h = 0, mixer2_w = 0, mixer2_h = 0;

	if (state == NULL || pGOPInfo == NULL || pctx_gop == NULL || pkms == NULL) {
		DRM_ERROR("[GOP][%s, %d]: null pointer\n", __func__, __LINE__);
		return;
	}

	if (pGOPInfo->gop_idx >= pctx_gop->GopNum ||
		pGOPInfo->gop_idx >= MAX_GOP_PLANE_NUM) {
		DRM_ERROR("[GOP][%s, %d]: Invalid GOP number %d\n",
			__func__, __LINE__, pGOPInfo->gop_idx);
		return;
	}
	gop_properties = (pctx_gop->gop_plane_properties + pGOPInfo->gop_idx);
	support_HMirror = pctx_gop->gop_plane_capability.u8SupportHMirror[pGOPInfo->gop_idx];
	support_VMirror = pctx_gop->gop_plane_capability.u8SupportVMirror[pGOPInfo->gop_idx];

	if (panelW == PANEL_8K_W &&
		pctx_gop->utpGopInit.panel_height == PANEL_8K_H) {
		mixer4_w = (panelW >> 1);
		mixer4_h = (panelH >> 1);
		mixer2_w = panelW;
		mixer2_h = panelH;
	} else {
		mixer4_w = panelW;
		mixer4_h = panelH;
		mixer2_w = panelW;
		mixer2_h = panelH;
	}

	if ((pctx_gop->utpGopInit.eGopDstPath == E_GOP_WARP_DST_OP) &&
		(mtk_gop_check_overscan(pkms) == true)) {
		overscan_w = (pkms->pixelshift_priv.i8OLEDPixelshiftHRangeMax << 1);
		overscan_h = (pkms->pixelshift_priv.i8OLEDPixelshiftVRangeMax << 1);
		bOverScan = true;
	} else {
		overscan_w = 0;
		overscan_h = 0;
		bOverScan = false;
	}

	if (bOverScan != _gbGOPCurrentOverScan[pGOPInfo->gop_idx]) {
		pGOPInfo->GopChangeStatus |= BIT(GOP_OVERSCAN_STATE_CHANGE);
		_gbGOPCurrentOverScan[pGOPInfo->gop_idx] = bOverScan;
	}
	pGOPInfo->bOverscanEn = bOverScan;
	pGOPInfo->gop_mixer4_w = mixer4_w + overscan_w;
	pGOPInfo->gop_mixer4_h = mixer4_h + overscan_h;
	pGOPInfo->gop_mixer2_w = mixer2_w + overscan_w;
	pGOPInfo->gop_mixer2_h = mixer2_h + overscan_h;
	pGOPInfo->stretchWinDstW = state->base.crtc_w + overscan_w;
	pGOPInfo->stretchWinDstH = state->base.crtc_h + overscan_h;

	if (support_HMirror && gop_properties->prop_val[E_GOP_PROP_HMIRROR])
		pGOPInfo->stretchWinX = panelW - state->pending.x - state->base.crtc_w;
	else
		pGOPInfo->stretchWinX = state->pending.x;

	if (support_VMirror && gop_properties->prop_val[E_GOP_PROP_VMIRROR])
		pGOPInfo->stretchWinY = panelH - state->pending.y - state->base.crtc_h;
	else
		pGOPInfo->stretchWinY = state->pending.y;

	if (Debug_GOPOnOFF_Mode) {
		if (pctx_gop->DebugForceOn[pGOPInfo->gop_idx])
			pGOPInfo->win_enable = true;
		else
			pGOPInfo->win_enable = false;
	} else {
		pGOPInfo->win_enable = true;
	}
}

void mtk_gop_check_property_update(struct mtk_tv_kms_context *pctx,
	struct gop_prop *gop_properties, UTP_GOP_SET_WINDOW *pGOPInfo)
{
	MS_U8 u8GopIdx = 0;

	if (!pGOPInfo) {
		DRM_ERROR("[GOP][%s, %d]: pGOPInfo is null\n",
			__func__, __LINE__);
		return;
	}

	u8GopIdx = pGOPInfo->gop_idx;

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_HMIRROR) == true) {
		pGOPInfo->hmirror =
		gop_properties->prop_val[E_GOP_PROP_HMIRROR];
		pGOPInfo->GopChangeStatus |= BIT(GOP_HMIRROR_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_VMIRROR) == true) {
		pGOPInfo->vmirror =
		gop_properties->prop_val[E_GOP_PROP_VMIRROR];
		pGOPInfo->GopChangeStatus |= BIT(GOP_VMIRROR_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_HSTRETCH) == true) {
		pGOPInfo->hstretch_mode =
			_convertHStretchMode(gop_properties->prop_val[E_GOP_PROP_HSTRETCH]);
		pGOPInfo->GopChangeStatus |= BIT(GOP_HSTRETCH_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_VSTRETCH) == true) {
		pGOPInfo->vstretch_mode =
			_convertVStretchMode(gop_properties->prop_val[E_GOP_PROP_VSTRETCH]);
		pGOPInfo->GopChangeStatus |= BIT(GOP_VSTRETCH_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_ALPHA_MODE) == true) {
		mtk_gop_get_alpha_blob(pctx, pGOPInfo,
			gop_properties->prop_val[E_GOP_PROP_ALPHA_MODE]);
		pGOPInfo->GopChangeStatus |= BIT(GOP_ALPHA_MODE_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_AID_TYPE) == true) {
		pGOPInfo->gop_aid_type = gop_properties->prop_val[E_GOP_PROP_AID_TYPE];
		pGOPInfo->GopChangeStatus |= BIT(GOP_AID_TYPE_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_OPBLEND_POS) == true) {
		pGOPInfo->eOpOsdbDst =
			_convertOpOsdbDst(gop_properties->prop_val[E_GOP_PROP_OPBLEND_POS]);
		pGOPInfo->GopChangeStatus |= BIT(GOP_OPBLEND_POS_STATE_CHANGE);
	}

	if ((_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_BYPASS) == true) ||
		(pGOPInfo->bypass_path != _gbGOPCurrentBypass[u8GopIdx])) {
		_gbGOPCurrentBypass[u8GopIdx] = pGOPInfo->bypass_path;
		pGOPInfo->GopChangeStatus |= BIT(GOP_BYPASS_STATE_CHANGE);
	}

	if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_properties,
		E_GOP_PROP_CSC) == true) {
		pGOPInfo->eCSC =
			_convertCsc(gop_properties->prop_val[E_GOP_PROP_CSC]);
		pGOPInfo->GopChangeStatus |= BIT(GOP_CSC_STATE_CHANGE);
	}
}

int mtk_gop_check_hw_support(struct mtk_gop_context *pctx_gop, UTP_GOP_SET_WINDOW *pGOPInfo)
{
	int ret = 0;

	if (pctx_gop == NULL || pGOPInfo == NULL) {
		DRM_ERROR("[GOP][%s, %d]: null pointer\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (pGOPInfo->GopChangeStatus & BIT(GOP_ALPHA_MODE_STATE_CHANGE)) {
		if (pGOPInfo->stGopAlphaMode.bEnMultiAlpha) {
			if (pctx_gop->gop_plane_capability.u8SupportGOPMultiApha == 0) {
				DRM_ERROR("[GOP][%s, %d]: not support multi alpha\n",
					__func__, __LINE__);
				pGOPInfo->stGopAlphaMode.bEnMultiAlpha = 0;
				ret = -EINVAL;
			}
		}
	}

	return ret;
}

void mtk_gop_update_plane(struct mtk_plane_state *state)
{
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	UTP_GOP_SET_WINDOW stGOPInfo;
	struct drm_framebuffer *fb = plane->state->fb;
	struct gop_prop *gop_properties;
	int VSyncRate = pctx->panel_priv.outputTiming_info.u32OutputVfreq / VFRQRATIO;
	bool HighWayPath = false;

	memset(&stGOPInfo, 0, sizeof(UTP_GOP_SET_WINDOW));

	if (pctx->out_model == E_OUT_MODEL_VG_BLENDED) {
		stGOPInfo.panel_w = pctx->panel_priv.info.de_width;
		stGOPInfo.panel_h = pctx->panel_priv.info.de_height;
	} else {
		stGOPInfo.panel_w = pctx->panel_priv.gfx_info.de_width;
		stGOPInfo.panel_h = pctx->panel_priv.gfx_info.de_height;
	}

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY)
		mplane->gop_index = 0;

	stGOPInfo.gop_idx = mplane->gop_index;
	if (stGOPInfo.gop_idx == INVALID_NUM) {
		DRM_ERROR("[GOP][%s, %d]: Invalid GOP number\r\n", __func__, __LINE__);
		return;
	}

	if (state->pending.addr == 0) {
		DRM_ERROR("[GOP][%s, %d]: Invalid buffer addr 0 of GOP%d\r\n",
			__func__, __LINE__, stGOPInfo.gop_idx);
		return;
	}

	stGOPInfo.pic_x_start = (__u16) state->base.src_x;
	stGOPInfo.pic_y_start = (__u16) state->base.src_y;
	stGOPInfo.pic_x_end = (__u16) (state->base.src_x + (state->base.src_w >> 16));
	stGOPInfo.pic_y_end = (__u16) (state->base.src_y + (state->base.src_h >> 16));
	stGOPInfo.fb_width = state->pending.fb_width;
	stGOPInfo.fb_height = state->pending.fb_height;
	stGOPInfo.fb_pitch = state->pending.pitch;
	stGOPInfo.fb_addr = state->pending.addr;
	stGOPInfo.u8zpos = state->base.zpos;

	mtk_gop_set_display_coordinate(state, &stGOPInfo, pctx_gop, pctx,
		stGOPInfo.panel_w, stGOPInfo.panel_h);

	gop_properties = (pctx_gop->gop_plane_properties + mplane->gop_index);

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY &&
		stGOPInfo.fb_width == BUF_4K_WIDTH &&
		stGOPInfo.fb_height == BUF_4K_HEIGHT &&
		VSyncRate == PANEL_120HZ &&
		pctx_gop->utpGopInit.eGopDstPath == E_GOP_WARP_DST_OP) {
		HighWayPath = true;
	} else {
		HighWayPath = false;
	}

	if (((stGOPInfo.fb_width == BUF_8K_WIDTH) &&
		(stGOPInfo.fb_height == BUF_8K_HEIGHT)) ||
			gop_properties->prop_val[E_GOP_PROP_BYPASS] == 1 ||
			HighWayPath == true)
		stGOPInfo.bypass_path = true;
	else
		stGOPInfo.bypass_path = false;

	stGOPInfo.fmt_type = _convertColorFmt(state->pending.format);

	mtk_gop_check_property_update(pctx, gop_properties, &stGOPInfo);

	mtk_gop_check_hw_support(pctx_gop, &stGOPInfo);

	stGOPInfo.afbc_buffer = (((fb->modifier >> 56) & 0xF) == DRM_FORMAT_MOD_VENDOR_ARM) ? 1 : 0;

	stGOPInfo.eGopType = (UTP_EN_GOP_TYPE)plane->type;

	stGOPInfo.gop_ml_res_idx = (__u8)mplane->gop_use_ml_res_idx;
	utp_gop_SetWindow(&stGOPInfo);

	pctx_gop->bSetPQBufByBlob[stGOPInfo.gop_idx] = false;
	pctx_gop->bSetPQBufByExtend[stGOPInfo.gop_idx] = false;
}

void mtk_gop_disable_plane(struct mtk_plane_state *state)
{
	UTP_GOP_SET_ENABLE stGopEn = {0};
	struct drm_plane *plane = state->base.plane;
	struct mtk_drm_plane *mplane = to_mtk_plane(plane);
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct gop_prop *gop_prop;
	uint8_t gop_ml_resIdx;

	if (mplane->mtk_plane_type == MTK_DRM_PLANE_TYPE_PRIMARY)
		mplane->gop_index = 0;

	if (plane->type == DRM_PLANE_TYPE_CURSOR)
		stGopEn.bIsCursor = true;
	else
		stGopEn.bIsCursor = false;

	stGopEn.gop_idx = mplane->gop_index;
	if (stGopEn.gop_idx == INVALID_NUM || stGopEn.gop_idx >= MAX_GOP_PLANE_NUM) {
		DRM_ERROR("[GOP][%s, %d]: Invalid GOP number\r\n", __func__, __LINE__);
		return;
	}

	gop_ml_resIdx = mplane->gop_use_ml_res_idx;

	if (Debug_GOPOnOFF_Mode) {
		if (pctx_gop->DebugForceOn[stGopEn.gop_idx])
			return;
	}

	if (state->pending.enable == false) {
		stGopEn.bEn = false;
		gop_prop = pctx_gop->gop_plane_properties + stGopEn.gop_idx;
		if (_mtk_drm_graphic_plane_is_ext_prop_updated(gop_prop, E_GOP_PROP_BYPASS)) {
			_gbGOPCurrentBypass[stGopEn.gop_idx] =
				(uint8_t)gop_prop->prop_val[E_GOP_PROP_BYPASS];
			stGopEn.bypass_path = (uint8_t)gop_prop->prop_val[E_GOP_PROP_BYPASS];
			stGopEn.GopChangeStatus |= BIT(GOP_BYPASS_STATE_CHANGE);
		}
		utp_gop_set_enable(&stGopEn, gop_ml_resIdx);
	}
}

void mtk_gop_disable_vblank(struct mtk_tv_drm_crtc *crtc)
{
	UTP_GOP_SET_ENABLE SetEnable;

	SetEnable.gop_idx = 0;
	SetEnable.bEn = false;
	utp_gop_intr_ctrl(&SetEnable);
}

int mtk_gop_get_roi(struct drm_mtk_tv_graphic_roi_info *RoiInfo)
{
	MTK_GOP_GETROI GopRoiInfo;
	int ret = 0;

	if (RoiInfo == NULL) {
		DRM_ERROR("[GOP][%s, %d]: RoiInfo null pointer\n", __func__, __LINE__);
		return -EINVAL;
	}
	memset(&GopRoiInfo, 0, sizeof(MTK_GOP_GETROI));

	ret = gop_wrapper_get_ROI(&GopRoiInfo);

	if (ret) {
		RoiInfo->RetCntOSDB0 = 0;
		RoiInfo->RetCntOSDB1 = 0;
	} else {
		RoiInfo->RetCntOSDB0 = GopRoiInfo.u64CntOSDB0;
		RoiInfo->RetCntOSDB1 = GopRoiInfo.u64CntOSDB1;
	}

	return ret;
}

int mtk_gop_set_roi(uint32_t threshold)
{
	if (!KDrv_HW_GOP_SetROI(threshold)) {
		DRM_ERROR("[GOP][%s, %d] KDrv_HW_GOP_SetROI fail\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}


int mtk_gop_set_pq_buf(struct mtk_gop_context *pctx_gop,
	struct drm_mtk_tv_graphic_pq_setting *PqBufInfo)
{
	MTK_GOP_PQ_CFD_ML_INFO GopPqCfdMl;
	MS_U32 GopIdx;

	if (PqBufInfo == NULL) {
		DRM_ERROR("[GOP][%s, %d]PqBufInfo is null\r\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (PqBufInfo->u32version < GOP_PQ_BUF_ST_IDX_VERSION) {
		DRM_ERROR("[GOP][%s, %d]version %d only support version %d\r\n",
			__func__, __LINE__, PqBufInfo->u32version, GOP_PQ_BUF_ST_IDX_VERSION);
		return -EINVAL;
	}

	if (PqBufInfo->u32GopIdx >= MAX_GOP_PLANE_NUM) {
		DRM_ERROR("[GOP][%s, %d]u32GopIdx %d is illegal\r\n",
			__func__, __LINE__, PqBufInfo->u32GopIdx);
		return -EINVAL;
	}

	if (pctx_gop->bSetPQBufByBlob[PqBufInfo->u32GopIdx]) {
		DRM_INFO("[%s][%d]use blob property to set PQ buf\n",
		__func__, __LINE__);
		return 0;
	}
	pctx_gop->bSetPQBufByExtend[PqBufInfo->u32GopIdx] = true;
	memset(&GopPqCfdMl, 0, sizeof(MTK_GOP_PQ_CFD_ML_INFO));
	GopIdx = PqBufInfo->u32GopIdx;

	if (PqBufInfo->u64buf_cfd_ml_addr != NULL) {
		GopPqCfdMl.u32_cfd_ml_bufsize = PqBufInfo->u32buf_cfd_ml_size;
		GopPqCfdMl.u64_cfd_ml_buf = u64_to_user_ptr(PqBufInfo->u64buf_cfd_ml_addr);
	} else {
		GopPqCfdMl.u32_cfd_ml_bufsize = 0;
		GopPqCfdMl.u64_cfd_ml_buf = NULL;
	}

	if (PqBufInfo->u64buf_pq_ml_addr != NULL) {
		GopPqCfdMl.u32_pq_ml_bufsize = PqBufInfo->u32buf_pq_ml_size;
		GopPqCfdMl.u64_pq_ml_buf = u64_to_user_ptr(PqBufInfo->u64buf_pq_ml_addr);
	} else {
		GopPqCfdMl.u32_pq_ml_bufsize = 0;
		GopPqCfdMl.u64_pq_ml_buf = NULL;
	}

	gop_wrapper_set_PqCfdMlInfo((__u8)GopIdx, &GopPqCfdMl);

	if (pctx_gop->gop_plane_capability.u8SupportHDRMode[GopIdx] != 0 &&
		pctx_gop->gop_plane_capability.u8SupportXvyccLite[GopIdx] == false) {
		if (PqBufInfo->u64buf_cfd_adl_addr != NULL) {
			pctx_gop->gop_adl[GopIdx].adl_bufsize = PqBufInfo->u32buf_cfd_adl_size;
			pctx_gop->gop_adl[GopIdx].u64_adl_buf =
				u64_to_user_ptr(PqBufInfo->u64buf_cfd_adl_addr);

			gop_wrapper_set_Adlnfo((__u8)GopIdx, &pctx_gop->gop_adl[GopIdx]);
		}
	}

	return 0;
}

int mtk_gop_set_pq_blob(struct mtk_tv_kms_context *pctx, struct mtk_drm_plane *mplane, uint64_t val)
{
	struct drm_property_blob *property_blob = NULL;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int plane_inx = mplane->gop_index;
	struct drm_mtk_tv_graphic_pq_setting stGfxPQInfo;
	MTK_GOP_PQ_CFD_ML_INFO GopPqCfdMl;

	memset(&stGfxPQInfo, 0, sizeof(struct drm_mtk_tv_graphic_pq_setting));
	memset(&GopPqCfdMl, 0, sizeof(MTK_GOP_PQ_CFD_ML_INFO));

	property_blob = drm_property_lookup_blob(pctx->drm_dev, val);
	if (property_blob != NULL) {
		if (pctx_gop->bSetPQBufByExtend[plane_inx]) {
			DRM_INFO("[%s][%d]use extend ioctl to set PQ buf\n",
			__func__, __LINE__);
			drm_property_blob_put(property_blob);
			return 0;
		}
		pctx_gop->bSetPQBufByBlob[plane_inx] = true;
		memcpy(&stGfxPQInfo, property_blob->data, property_blob->length);
		if (stGfxPQInfo.u64buf_cfd_ml_addr != NULL) {
			GopPqCfdMl.u32_cfd_ml_bufsize = stGfxPQInfo.u32buf_cfd_ml_size;
			GopPqCfdMl.u64_cfd_ml_buf = u64_to_user_ptr(stGfxPQInfo.u64buf_cfd_ml_addr);
		} else {
			GopPqCfdMl.u32_cfd_ml_bufsize = 0;
			GopPqCfdMl.u64_cfd_ml_buf = NULL;
		}

		if (stGfxPQInfo.u64buf_pq_ml_addr != NULL) {
			GopPqCfdMl.u32_pq_ml_bufsize = stGfxPQInfo.u32buf_pq_ml_size;
			GopPqCfdMl.u64_pq_ml_buf = u64_to_user_ptr(stGfxPQInfo.u64buf_pq_ml_addr);
		} else {
			GopPqCfdMl.u32_pq_ml_bufsize = 0;
			GopPqCfdMl.u64_pq_ml_buf = NULL;
		}

		gop_wrapper_set_PqCfdMlInfo((__u8)plane_inx, &GopPqCfdMl);

		if (pctx_gop->gop_plane_capability.u8SupportHDRMode[plane_inx] != 0 &&
			pctx_gop->gop_plane_capability.u8SupportXvyccLite[plane_inx] == false) {
			if (stGfxPQInfo.u64buf_cfd_adl_addr != NULL) {
				pctx_gop->gop_adl[plane_inx].adl_bufsize =
					stGfxPQInfo.u32buf_cfd_adl_size;
				pctx_gop->gop_adl[plane_inx].u64_adl_buf =
					u64_to_user_ptr(stGfxPQInfo.u64buf_cfd_adl_addr);
				gop_wrapper_set_Adlnfo((__u8)plane_inx,
					&pctx_gop->gop_adl[plane_inx]);
			}
		}
		drm_property_blob_put(property_blob);
	} else {
		DRM_INFO("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
			__func__, __LINE__, (ptrdiff_t)val);
		return -EINVAL;
	}

	return 0;
}

int mtk_gop_atomic_set_crtc_property(struct mtk_tv_drm_crtc *crtc, struct drm_crtc_state *state,
				     struct drm_property *property, uint64_t val)
{
	int i = 0;
	struct mtk_tv_kms_context *pctx;
	struct ext_crtc_graphic_prop_info *graphic_crtc_properties;
	struct drm_property_blob *property_blob = NULL;
	struct mtk_gop_context *pgop_priv;

	if (!crtc) {
		DRM_ERROR("[%s][%d]Null pointer\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;

	if (!pctx) {
		DRM_ERROR("[%s][%d]Null pointer\n", __func__, __LINE__);
		return -EINVAL;
	}
	graphic_crtc_properties = pctx->ext_crtc_graphic_properties;
	pgop_priv = &pctx->gop_priv;

	for (i = 0; i < E_EXT_CRTC_GRAPHIC_PROP_MAX; i++) {
		if (property == pctx->drm_crtc_graphic_prop[i]) {
			graphic_crtc_properties->prop_val[i] = val;
			graphic_crtc_properties->prop_update[i] = 1;
			if (i == E_EXT_CRTC_GRAPHIC_PROP_MODE) {
				property_blob = drm_property_lookup_blob(pctx->drm_dev, val);
				if (property_blob != NULL) {
					memcpy(&pgop_priv->stVGSepMode, property_blob->data,
						property_blob->length);
					drm_property_blob_put(property_blob);
				} else {
					DRM_ERROR("[%s][%d]blob id= 0x%tx, blob is NULL!!\n",
						__func__, __LINE__, (ptrdiff_t)val);
					return -EINVAL;
				}
			}
			break;
		}
	}

	return 0;
}

int mtk_gop_atomic_get_crtc_property(struct mtk_tv_drm_crtc *crtc,
				     const struct drm_crtc_state *state,
				     struct drm_property *property, uint64_t *val)
{
	int i = 0;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;
	struct ext_crtc_graphic_prop_info *graphic_crtc_properties =
		pctx->ext_crtc_graphic_properties;

	for (i = 0; i < E_EXT_CRTC_GRAPHIC_PROP_MAX; i++) {
		if (property == pctx->drm_crtc_graphic_prop[i]) {
			*val = graphic_crtc_properties->prop_val[i];
			break;
		}
	}
	return 0;
}

int mtk_gop_atomic_set_plane_property(struct mtk_drm_plane *mplane, struct drm_plane_state *state,
				      struct drm_property *property, uint64_t val)
{
	int ret = -EINVAL;
	int i;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int plane_inx = mplane->gop_index;
	struct gop_prop *plane_props = (pctx_gop->gop_plane_properties + plane_inx);

	for (i = 0; i < E_GOP_PROP_MAX; i++) {
		if (property == pctx_gop->drm_gop_plane_prop[i]) {
			plane_props->prop_val[i] = val;
			plane_props->prop_update[i] = 1;
			ret = 0x0;
			if (i == E_GOP_PROP_PQDATA)
				ret = mtk_gop_set_pq_blob(pctx, mplane, val);

			break;
		}
	}

	return ret;
}

int mtk_gop_atomic_get_plane_property(struct mtk_drm_plane *mplane,
				      const struct drm_plane_state *state,
				      struct drm_property *property, uint64_t *val)
{
	int ret = -EINVAL;
	int i;
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	int plane_inx = mplane->gop_index;
	struct gop_prop *plane_props = (pctx_gop->gop_plane_properties + plane_inx);

	for (i = 0; i < E_GOP_PROP_MAX; i++) {
		if (property == pctx_gop->drm_gop_plane_prop[i]) {
			if (i == E_GOP_PROP_PNL_CURLUM) {
				if (pctx_gop->pqu_shm_addr != NULL)
					*val = *pctx_gop->pqu_shm_addr;
				else
					*val = plane_props->prop_val[E_GOP_PROP_PNL_CURLUM];
			} else {
				*val = plane_props->prop_val[i];
			}
			ret = 0x0;
			break;
		}
	}

	return ret;
}

int mtk_gop_bootlogo_ctrl(struct mtk_tv_drm_crtc *crtc, unsigned int cmd, unsigned int *gop)
{
	struct mtk_drm_plane *mplane = crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC];
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;

	if (cmd == MTK_CTRL_BOOTLOGO_CMD_GETINFO) {
		*gop = pctx_gop->nBootlogoGop;
	} else if (cmd == MTK_CTRL_BOOTLOGO_CMD_DISABLE) {
		UTP_GOP_INIT_INFO utp_init_info;

		utp_init_info.gop_idx = pctx_gop->nBootlogoGop;
		utp_init_info.panel_width = pctx_gop->utpGopInit.panel_width;
		utp_init_info.panel_height = pctx_gop->utpGopInit.panel_height;
		utp_init_info.panel_hstart = pctx_gop->utpGopInit.panel_hstart;
		utp_init_info.ignore_level = E_GOP_WRAP_IGNORE_SWRST;
		if (pctx->out_model == E_OUT_MODEL_VG_BLENDED)
			utp_init_info.eGopDstPath = E_GOP_WARP_DST_OP;
		else
			utp_init_info.eGopDstPath = E_GOP_WARP_DST_VG_SEPARATE;

		if (utp_init_info.gop_idx < pctx_gop->GopNum) {
			utp_gop_InitByGop(&utp_init_info, pctx_gop->gop_ml);

			if (bootlogo_dual_idx != INVALID_NUM) {
				utp_init_info.gop_idx = bootlogo_dual_idx;
				utp_gop_InitByGop(&utp_init_info,
					pctx_gop->gop_ml);
			}
		} else {
			DRM_INFO("[%s][%d]not exist gop:%d\n", __func__, __LINE__,
				utp_init_info.gop_idx);
		}
	}
	return 0;
}

int mtk_gop_set_testpattern(struct mtk_tv_drm_crtc *crtc,
	struct drm_mtk_tv_graphic_testpattern *testpatternInfo)
{
	int ret;
	MTK_GOP_TESTPATTERN GopTestPatternInfo;

	memset(&GopTestPatternInfo, 0, sizeof(MTK_GOP_TESTPATTERN));
	GopTestPatternInfo.PatEnable = testpatternInfo->PatEnable;
	GopTestPatternInfo.HwAutoMode = testpatternInfo->HwAutoMode;
	GopTestPatternInfo.DisWidth = testpatternInfo->DisWidth;
	GopTestPatternInfo.DisHeight = testpatternInfo->DisHeight;
	GopTestPatternInfo.ColorbarW = testpatternInfo->ColorbarW;
	GopTestPatternInfo.ColorbarH = testpatternInfo->ColorbarH;
	GopTestPatternInfo.EnColorbarMove = testpatternInfo->EnColorbarMove;
	GopTestPatternInfo.MoveDir = (EN_GOP_WRAP_TESTPATTERN_MOVE_DIR)testpatternInfo->MoveDir;
	GopTestPatternInfo.MoveSpeed = testpatternInfo->MoveSpeed;

	ret = gop_wrapper_set_testpattern(&GopTestPatternInfo);

	return ret;
}

static size_t mtk_sm_pa2va(uint64_t phy_addr, uint32_t len)
{
	size_t vir_addr = 0;

	if (pfn_valid(__phys_to_pfn(phy_addr)))
		vir_addr = (size_t)__va(phy_addr);
	else
		vir_addr =  (size_t)ioremap_wc(phy_addr, len);

	return vir_addr;
}

int mtk_gop_start_gfx_pqudata(struct mtk_tv_drm_crtc *crtc, struct drm_mtk_tv_gfx_pqudata *pquifo)
{
	struct mtk_drm_plane *mplane = crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC];
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct pqu_gop_update_luminance_param pqu_param;
	struct pqu_gop_update_luminance_reply_param pqu_reply;
	struct device_node *np;
	struct of_mmap_info_data mmap_info;

	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_KMS_COMPATIBLE_STR);
	if (np == NULL) {
		DRM_ERROR("[GOP][%s, %d]: of_find_compatible_node failed \r\n", __func__,
			  __LINE__);
		return -1;
	}

	of_mtk_get_reserved_memory_info_by_idx(np, 0, &mmap_info);

	pqu_param.enable = TRUE;
	pqu_param.shm_addr = mmap_info.start_bus_address;
	pctx_gop->pqu_shm_addr =
		(uint32_t *)mtk_sm_pa2va(mmap_info.start_bus_address, mmap_info.buffer_size);
	if (pctx_gop->pqu_shm_addr == NULL) {
		DRM_ERROR("[GOP][%s, %d]: pqu_shm_addr is null pointer\r\n", __func__,
			  __LINE__);
		return -1;
	}
	*pctx_gop->pqu_shm_addr = 0;
	memset(&pqu_reply, 0, sizeof(struct pqu_gop_update_luminance_reply_param));

	if (pqu_render_gop_update_luminance(&pqu_param, &pqu_reply) != 0) {
		DRM_ERROR("[GOP][%s, %d]: pqu_render_gop_update_luminance failed \r\n", __func__,
			  __LINE__);
		return -1;
	}
	pquifo->RetValue = pqu_reply.ret;

	return 0;
}

int mtk_gop_stop_gfx_pqudata(struct mtk_tv_drm_crtc *crtc, struct drm_mtk_tv_gfx_pqudata *pquifo)
{
	struct mtk_drm_plane *mplane = crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC];
	struct mtk_tv_kms_context *pctx = (struct mtk_tv_kms_context *)mplane->crtc_private;
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct pqu_gop_update_luminance_param pqu_param;
	struct pqu_gop_update_luminance_reply_param pqu_reply;

	pqu_param.enable = FALSE;
	pqu_param.shm_addr = NULL;
	pctx_gop->pqu_shm_addr = NULL;
	memset(&pqu_reply, 0, sizeof(struct pqu_gop_update_luminance_reply_param));

	if (pqu_render_gop_update_luminance(&pqu_param, &pqu_reply) != 0) {
		DRM_ERROR("[GOP][%s, %d]: pqu_render_gop_update_luminance failed \r\n", __func__,
			  __LINE__);
		return -1;
	}
	pquifo->RetValue = pqu_reply.ret;

	return 0;
}


int mtk_drm_attach_props(struct mtk_gop_context *pctx_gop, struct mtk_drm_plane *mplane)
{
	int extend_prop_count = ARRAY_SIZE(gop_plane_props);
	struct drm_plane *plane_base = &mplane->base;
	struct drm_mode_object *plane_obj = &plane_base->base;
	int plane_inx = mplane->gop_index;
	struct gop_prop *plane_props = (pctx_gop->gop_plane_properties + plane_inx);
	int i;

	//////////////////////////////  PROPERTIES //////////////////////////////
	for (i = 0; i < extend_prop_count; i++) {
		drm_object_attach_property(plane_obj, pctx_gop->drm_gop_plane_prop[i],
					   plane_props->prop_val[i]);
	}

	return 0;
}

static int readDtbS32Array(struct device_node *np,
			   const char *tag_name, int **ret_array, int ret_array_length)
{
	int ret;
	int i;
	__u32 *u32TmpArray = NULL;
	int *array = NULL;

	if ((ret_array_length == 0) || (ret_array == NULL)) {
		DRM_ERROR("[GOP][%s, %d]: invalid param \r\n", __func__, __LINE__);
		ret = -EINVAL;
		goto ERROR;
	}

	u32TmpArray = kmalloc_array(ret_array_length, sizeof(__u32), GFP_KERNEL);
	array = kmalloc_array(ret_array_length, sizeof(int), GFP_KERNEL);
	if ((u32TmpArray == NULL) || (array == NULL)) {
		DRM_ERROR("[GOP][%s, %d]: malloc failed \r\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto ERROR;
	}
	ret = of_property_read_u32_array(np, tag_name, u32TmpArray, ret_array_length);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: of_property_read_u32_array failed \r\n", __func__,
			  __LINE__);
		goto ERROR;
	}
	for (i = 0; i < ret_array_length; i++)
		array[i] = u32TmpArray[i];

	kfree(u32TmpArray);
	*ret_array = array;
	return 0;

ERROR:
	kfree(u32TmpArray);
	kfree(array);

	return ret;
}

static int getDefaultPlaneProps(struct mtk_gop_context *pctx_gop,
				struct device_node *np, const char *tag_name, int prop_inx)
{
	int ret;
	int i;
	int *tmpArray = NULL;

	ret = readDtbS32Array(np, tag_name, &tmpArray, pctx_gop->GopNum);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: readDtbS32Array failed, name = %s \r\n", __func__,
			  __LINE__, tag_name);
		return ret;
	}

	if (tmpArray == NULL)
		ret = -ENOMEM;

	for (i = 0; i < pctx_gop->GopNum; i++)
		(pctx_gop->gop_plane_properties + i)->prop_val[prop_inx] = tmpArray[i];

	kfree(tmpArray);
	return ret;
}

static int readChipProp2GOPPrivate(struct mtk_gop_context *pctx_gop)
{
	int i, ret;

	for (i = 0; i < pctx_gop->GopNum; i++) {
		UTP_GOP_QUERY_FEATURE query_afbc;

		query_afbc.gop_idx = i;
		query_afbc.feature = UTP_E_GOP_CAP_AFBC_SUPPORT;
		ret = utp_gop_query_chip_prop(&query_afbc);
		if (ret != 0x0) {
			DRM_ERROR
			    ("[GOP][%s, %d]gop_query_chip_prop AFBC_SUPPORT failed\r\n",
			     __func__, __LINE__);
			return ret;
		}

		pctx_gop->gop_plane_properties[i].prop_val[E_GOP_PROP_AFBC_FEATURE] =
		    query_afbc.value;
	}
	return 0;
}

int readDTB2GOPPrivate(struct mtk_gop_context *pctx_gop)
{
	int ret = 0, dts_ret = 0;
	struct device_node *np;
	int u32Tmp = 0xFF, i = 0;
	int *tmpArray = NULL;
	const char *name;

	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_KMS_COMPATIBLE_STR);

	if (np != NULL) {
		name = MTK_DRM_DTS_GOP_PLANE_NUM_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->GopNum = u32Tmp;

		name = MTK_DRM_DTS_HMIRROR_TAG;
		ret = getDefaultPlaneProps(pctx_gop, np, name, E_GOP_PROP_HMIRROR);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);

		name = MTK_DRM_DTS_VMIRROR_TAG;
		ret = getDefaultPlaneProps(pctx_gop, np, name, E_GOP_PROP_VMIRROR);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);

		name = MTK_DRM_DTS_HSTRETCH_INIT_MODE_TAG;
		ret = getDefaultPlaneProps(pctx_gop, np, name, E_GOP_PROP_HSTRETCH);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);

		name = MTK_DRM_DTS_VSTRETCH_INIT_MODE_TAG;
		ret = getDefaultPlaneProps(pctx_gop, np, name, E_GOP_PROP_VSTRETCH);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);

		name = MTK_DRM_DTS_ENGINE_PMODE_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->GopEngineMode = u32Tmp;

	}

	np = of_find_node_by_name(NULL, MTK_DRM_PANEL_COLOR_COMPATIBLE_STR);

	if (np != NULL) {
		name = MTK_DRM_DTS_PANEL_CURLUM_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		for (i = 0; i < pctx_gop->GopNum; i++)
			(pctx_gop->gop_plane_properties + i)->prop_val[E_GOP_PROP_PNL_CURLUM] =
			    u32Tmp;
	}

	np = of_find_compatible_node(NULL, NULL, "mediatek,mtk-dtv-gop0");
	if (np != NULL) {
		name = MTK_DRM_DTS_IPVERSION_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->ChipVer = u32Tmp;
	}

	np = of_find_compatible_node(NULL, NULL, "mtk-drm-gop-drvconfig");
	if (np != NULL) {
		name = MTK_DRM_DTS_AFBC_WIDTH_CAP_TAG;
		readDtbS32Array(np, name, &tmpArray, pctx_gop->GopNum);
		if (tmpArray != NULL) {
			for (i = 0; i < pctx_gop->GopNum; i++) {
				pctx_gop->gop_plane_capability.u32SupportAFBCWidth[i] =
				(uint32_t)tmpArray[i];
			}
			kfree(tmpArray);
			tmpArray = NULL;
		}

		name = MTK_DRM_DTS_GOP_INPUT_MAX_WIDTH_CAP;
		readDtbS32Array(np, name, &tmpArray, pctx_gop->GopNum);
		if (tmpArray != NULL) {
			for (i = 0; i < pctx_gop->GopNum; i++) {
				pctx_gop->gop_plane_capability.u32SupportMaxWidth[i] =
				(uint32_t)tmpArray[i];
			}
			kfree(tmpArray);
			tmpArray = NULL;
		}

		name = MTK_DRM_DTS_DUAL_GOP_LAYER_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8bNeedDualGOP = (uint8_t)u32Tmp;

		// no matter what GOP_USE_ML define or not,
		// the default value could be used
		name = MTK_DRM_DTS_GOP_USE_ML;
		of_property_read_u32(np, name, &gop_use_ml);

		name = MTK_DRM_DTS_AUTO_DETECT_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportAutoDetect = (uint8_t)u32Tmp;

		name = MTK_DRM_DTS_BLEND2OSDB0_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportBlend2OSDB0 = (uint8_t)u32Tmp;

		name = MTK_DRM_DTS_OSDB_LOC_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportOSDBLocSwitch = (uint8_t)u32Tmp;

		name = MTK_DRM_DTS_GOP_BLEND_VIDEO_IRQ;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportBlendVideoIrq = (uint8_t)u32Tmp;

		name = MTK_DRM_DTS_GOP_MULTI_ALPHA_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportGOPMultiApha = (uint8_t)u32Tmp;

		name = MTK_DRM_DTS_GOP_STEP2_HVSP_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		dts_ret |= _mtk_drm_GOP_check_dts(ret, name);
		pctx_gop->gop_plane_capability.u8SupportGOPStep2HVSP = (uint8_t)u32Tmp;
	}

	return dts_ret;
}

static void clean_gop_ml_context(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	if (!pctx_gop || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx_gop and dev are null pointer\n",
			__func__, __LINE__);
		return;
	}

	if (pctx_gop->gop_ml)
		devm_kfree(dev, pctx_gop->gop_ml);
}

static void clean_gop_adl_context(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	if (!pctx_gop || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx_gop and dev are null pointer\n",
			__func__, __LINE__);
		return;
	}

	if (pctx_gop->gop_adl)
		devm_kfree(dev, pctx_gop->gop_adl);
}

static void clean_gop_pq_context(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	int i = 0;
	MTK_GOP_PQ_INFO *gop_pq;

	if (!pctx_gop || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx_gop and dev are null pointer\n",
			__func__, __LINE__);
		return;
	}

	gop_pq = pctx_gop->gop_pq;
	if (gop_pq) {
		for (i = 0; i < pctx_gop->GopNum; i++) {
			if ((gop_pq + i)->pCtx)
				devm_kfree(dev, (gop_pq + i)->pCtx);
			if ((gop_pq + i)->pSwReg)
				devm_kfree(dev, (gop_pq + i)->pSwReg);
			if ((gop_pq + i)->pMetadata)
				devm_kfree(dev, (gop_pq + i)->pMetadata);
			if ((gop_pq + i)->pHwReport)
				devm_kfree(dev, (gop_pq + i)->pHwReport);
		}
	}
}

void clean_gop_context(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	if (pctx_gop && dev) {
		if (pctx_gop->gop_plane_properties != NULL) {
			devm_kfree(dev, pctx_gop->gop_plane_properties);
			pctx_gop->gop_plane_properties = NULL;
		}
		clean_gop_pq_context(dev, pctx_gop);
		clean_gop_adl_context(dev, pctx_gop);
		clean_gop_ml_context(dev, pctx_gop);
	}
}

int mtk_gop_ml_destroy_resource(struct mtk_gop_context *pctx_gop)
{
	uint8_t u8SupportBlendVideoIrq = 0;
	int ret = 0;

	if (!pctx_gop) {
		DRM_ERROR("[GOP][%s, %d] null pointer of pctx_gop\n", __func__, __LINE__);
		return -ENXIO;
	}

	if (sm_ml_destroy_resource(pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_DISP_SYNC])
		!= E_SM_RET_OK) {
		DRM_ERROR("[GOP][%s, %d]destroy E_GOP_WRAP_ML_DISP_SYNC fail\n",
			__func__, __LINE__);
		ret = -EINVAL;
	}

	u8SupportBlendVideoIrq = pctx_gop->gop_plane_capability.u8SupportBlendVideoIrq;

	if (!u8SupportBlendVideoIrq) {
		if (sm_ml_destroy_resource(pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_OSD_SYNC])
			!= E_SM_RET_OK) {
			DRM_ERROR("[GOP][%s, %d]destroy E_GOP_WRAP_ML_OSD_SYNC fail\n",
				__func__, __LINE__);
			ret |= -EINVAL;
		}
	}

	return ret;
}

static int mtk_gop_get_bootlogo_gop(unsigned int gop_count, struct mtk_tv_kms_context *pctx)
{
	int i;
	UTP_GOP_SET_ENABLE en;
	int bootlogo_idx = INVALID_NUM;
	int VSyncRate = pctx->panel_priv.outputTiming_info.u32OutputVfreq / VFRQRATIO;
	bool bNeedCheckDualGop = false;
	enum OUTPUT_MODEL out_model = pctx->out_model;

	if (VSyncRate >= PANEL_120HZ && out_model == E_OUT_MODEL_VG_BLENDED)
		bNeedCheckDualGop = true;

	for (i = 0; i < gop_count; i++) {
		en.gop_idx = i;
		en.bEn = false;
		utp_gop_is_enable(&en);

		if (en.bEn == true) {
			if (bootlogo_idx == INVALID_NUM)
				bootlogo_idx = i;
			else
				bootlogo_dual_idx = i;

			if (!bNeedCheckDualGop || bootlogo_dual_idx != INVALID_NUM)
				break;
		}
	}

	return bootlogo_idx;
}

static void _mtk_drm_set_properties_init_value(struct mtk_tv_kms_context *pctx,
	struct mtk_gop_context *pctx_gop)
{
	struct mtk_panel_context *panel_priv = &pctx->panel_priv;
	int i = 0, j = 0;

	for (i = 0; i < pctx_gop->GopNum; i++) {
		for (j = 0; j < E_GOP_PROP_MAX; j++) {
			if (j == E_GOP_PROP_ALPHA_MODE || j == E_GOP_PROP_BYPASS
			   || j == E_GOP_PROP_CSC)
				(pctx_gop->gop_plane_properties + i)->prop_update[j] = 0;
			else
				(pctx_gop->gop_plane_properties + i)->prop_update[j] = 1;

			if (j == E_GOP_PROP_OPBLEND_POS) {
				(pctx_gop->gop_plane_properties + i)->prop_val[j] =
					MTK_DRM_DST_OSDB1;
			}

			if (j == E_GOP_PROP_HMIRROR) {
				if (panel_priv->cus.mirror_mode == E_PNL_MIRROR_H ||
					panel_priv->cus.mirror_mode == E_PNL_MIRROR_V_H) {
					(pctx_gop->gop_plane_properties + i)->prop_val[j] = 1;
				} else {
					(pctx_gop->gop_plane_properties + i)->prop_val[j] = 0;
				}
			}

			if (j == E_GOP_PROP_VMIRROR) {
				if (panel_priv->cus.mirror_mode == E_PNL_MIRROR_V ||
					panel_priv->cus.mirror_mode == E_PNL_MIRROR_V_H) {
					(pctx_gop->gop_plane_properties + i)->prop_val[j] = 1;
				} else {
					(pctx_gop->gop_plane_properties + i)->prop_val[j] = 0;
				}
			}
		}
	}
}

static int mtk_drm_create_properties(struct mtk_tv_kms_context *pctx)
{
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct drm_property *prop;
	const struct ext_prop_info *pprop;
	int extend_prop_count = ARRAY_SIZE(gop_plane_props);
	int i;

	for (i = 0; i < extend_prop_count; i++) {
		pprop = &gop_plane_props[i];
		if ((pprop->flag & DRM_MODE_PROP_ENUM) || (pprop->flag & DRM_MODE_PROP_IMMUTABLE)) {
			prop = drm_property_create_enum(pctx->drm_dev, pprop->flag,
							pprop->prop_name,
							pprop->enum_info.enum_list,
							pprop->enum_info.enum_length);
			if (prop == NULL) {
				DRM_ERROR("[GOP][%s, %d]: drm_property_create_enum fail!!\n",
					  __func__, __LINE__);
				return -ENOMEM;
			}
		} else if ((pprop->flag & DRM_MODE_PROP_RANGE)) {
			prop = drm_property_create_range(pctx->drm_dev, pprop->flag,
							 pprop->prop_name, pprop->range_info.min,
							 pprop->range_info.max);
		} else if (pprop->flag & DRM_MODE_PROP_BLOB) {
			prop = drm_property_create(pctx->drm_dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			pprop->prop_name, 0);
			if (prop == NULL) {
				DRM_ERROR("[%s, %d]: create ext blob prop fail!!\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
		} else {
			DRM_ERROR("[GOP][%s, %d]: unknown prop flag 0x%x !!\n", __func__,
				  __LINE__, pprop->flag);
			return -EINVAL;
		}

		pctx_gop->drm_gop_plane_prop[i] = prop;
	}

	_mtk_drm_set_properties_init_value(pctx, pctx_gop);

	return 0;
}

static int _mtk_gop_set_mux_clock(struct device *dev, int clk_index,
					char *dev_clk_name, bool bExistParent)
{
	int ret = 0;
	struct clk *dev_clk;
	struct clk_hw *clk_hw;
	struct clk_hw *parent_clk_hw;

	dev_clk = devm_clk_get(dev, dev_clk_name);
	if (IS_ERR(dev_clk)) {
		DRM_ERROR("[GOP][%s, %d]: failed to get %s\n", __func__, __LINE__, dev_clk_name);
		return -ENODEV;
	}
	DRM_INFO("[GOP][%s, %d]clk_name = %s\n", __func__, __LINE__, __clk_get_name(dev_clk));

	if (bExistParent == true) {
		clk_hw = __clk_get_hw(dev_clk);
		parent_clk_hw = clk_hw_get_parent_by_index(clk_hw, clk_index);
		if (IS_ERR(parent_clk_hw)) {
			DRM_ERROR("[GOP][%s, %d]failed to get parent_clk_hw\n", __func__, __LINE__);
			return -ENODEV;
		}

		//set_parent clk
		ret = clk_set_parent(dev_clk, parent_clk_hw->clk);
		if (ret) {
			DRM_ERROR("[GOP][%s, %d]failed to change clk_index [%d]\n",
				__func__, __LINE__, clk_index);
			return -ENODEV;
		}
	}

	//prepare and enable clk
	ret = clk_prepare_enable(dev_clk);
	if (ret) {
		DRM_ERROR("[GOP][%s, %d]failed to enable clk_index [%d]\n",
				__func__, __LINE__, clk_index);
		return -ENODEV;
	}

	return ret;
}

static struct mtk_drm_panel_context *_gfx_panel_ctx(void)
{
	struct device_node *deviceNodePanel = NULL;
	struct drm_panel *pDrmPanel = NULL;
	struct mtk_drm_panel_context *pStPanelCtx = NULL;

	deviceNodePanel = of_find_node_by_name(NULL, GFX_NODE);
	if (deviceNodePanel != NULL && of_device_is_available(deviceNodePanel)) {
		pDrmPanel = of_drm_find_panel(deviceNodePanel);
		pStPanelCtx = container_of(pDrmPanel, struct mtk_drm_panel_context, drmPanel);
	}
	return pStPanelCtx;
}

int mtk_gop_set_tgen(struct mtk_drm_panel_context *pStPanelCtx)
{
	MTK_GOP_TGEN_INFO stGop_tgen_info;
	drm_st_pnl_info *TgenInfo = NULL;
	int ret = 0;

	if (pStPanelCtx != NULL) {
		memset(&stGop_tgen_info, 0, sizeof(MTK_GOP_TGEN_INFO));
		TgenInfo = &pStPanelCtx->pnlInfo;
		stGop_tgen_info.Tgen_hs_st = TgenInfo->hsync_st;
		stGop_tgen_info.Tgen_hs_end = (TgenInfo->hsync_w
			+ TgenInfo->hsync_st - 1);
		stGop_tgen_info.Tgen_hfde_st = TgenInfo->de_hstart;
		stGop_tgen_info.Tgen_hfde_end = (TgenInfo->de_hstart +
						TgenInfo->de_width - 1);
		stGop_tgen_info.Tgen_htt = TgenInfo->typ_htt - 1;
		stGop_tgen_info.Tgen_vs_st = TgenInfo->vsync_st;
		stGop_tgen_info.Tgen_vs_end = (TgenInfo->vsync_st +
					TgenInfo->vsync_w - 1);
		stGop_tgen_info.Tgen_vfde_st = TgenInfo->de_vstart;
		stGop_tgen_info.Tgen_vfde_end = (TgenInfo->de_vstart +
			TgenInfo->de_height - 1);
		stGop_tgen_info.Tgen_vtt = TgenInfo->typ_vtt - 1;
		switch (TgenInfo->cop_format) {
		case E_OUTPUT_ARGB8101010:
			stGop_tgen_info.GOPOutMode = E_GOP_WARP_OUT_A8RGB10;
		break;
		case E_OUTPUT_ARGB8888_W_DITHER:
			stGop_tgen_info.GOPOutMode = E_GOP_WARP_OUT_ARGB8888_DITHER;
		break;
		case E_OUTPUT_ARGB8888_W_ROUND:
			stGop_tgen_info.GOPOutMode = E_GOP_WARP_OUT_ARGB8888_ROUND;
		break;
		default:
			stGop_tgen_info.GOPOutMode = E_GOP_WARP_OUT_A8RGB10;
		break;
		}
		ret = gop_wrapper_set_Tgen(&stGop_tgen_info);
	}

	return ret;
}

static int mtk_gop_init_clock(struct device *dev, enum OUTPUT_MODEL out_model,
	struct mtk_gop_context *pctx_gop)
{
	int clk_gop0_dst = 0;
	//0:xc_fd_ck, 1:xc_fn_ck
	int clk_gop_engine = 0;
	//0:xc_fd_ck
	int clk_gopc_dst = 0;
	//0:xc_fd_ck, 1:v1_odclk_int_ck
	int clk_gopg_dst = 0;
	//0:v1_odclk_int_ck, 1:o_odclk_int_ck
	int clk_gop0_sram = 0;
	//0:xc_fd_ck, 1:smi_ck
	int clk_gop1_sram = 0;
	//0:xc_fd_ck, 1:smi_ck
	int clk_gop2_sram = 0;
	//0:xc_fd_ck, 1:smi_ck
	int clk_gop3_sram = 0;
	//0:xc_fd_ck, 1:smi_ck
	int ChipMajor = (pctx_gop->ChipVer & MTK_GOP_MAJOR_IPVERSION_OFFSET_MSK) >>
		MTK_GOP_MAJOR_IPVERSION_OFFSET_SHT;
	int ChipMinor = (pctx_gop->ChipVer & MTK_GOP_MINOR_IPVERSION_OFFSET_MSK);

	//Machli E1 only
	if (ChipMajor == MTK_GOP_M6_SERIES_MAJOR && ChipMinor == MTK_GOP_M6_SERIES_MINOR) {
		if (out_model == E_OUT_MODEL_VG_BLENDED) {
			clk_gopg_dst = 0; //set odclk
		} else if (out_model == E_OUT_MODEL_VG_SEPARATED ||
			out_model == E_OUT_MODEL_VG_SEPARATED_W_EXTDEV) {
			clk_gopg_dst = 1; //set mod clk
		}

		if (_mtk_gop_set_mux_clock(dev, clk_gop0_dst, "clk_gop0_dst", true))
			return -ENODEV;
		if (_mtk_gop_set_mux_clock(dev, clk_gop_engine, "clk_gop_engine", true))
			return -ENODEV;
		if (_mtk_gop_set_mux_clock(dev, clk_gopc_dst, "clk_gopc_dst", true))
			return -ENODEV;
		if (_mtk_gop_set_mux_clock(dev, clk_gopg_dst, "clk_gopg_dst", true))
			return -ENODEV;
	}

	//xvycc clock
	if ((pctx_gop->gop_plane_capability.u8SupportHDRMode[GOP0]) &&
		(_mtk_gop_set_mux_clock(dev, clk_gop0_sram, "clk_gop0_sram", true)))
		return -ENODEV;
	if ((pctx_gop->gop_plane_capability.u8SupportHDRMode[GOP1]) &&
		(_mtk_gop_set_mux_clock(dev, clk_gop1_sram, "clk_gop1_sram", true)))
		return -ENODEV;
	if ((pctx_gop->gop_plane_capability.u8SupportHDRMode[GOP2]) &&
		(_mtk_gop_set_mux_clock(dev, clk_gop2_sram, "clk_gop2_sram", true)))
		return -ENODEV;
	if ((pctx_gop->gop_plane_capability.u8SupportHDRMode[GOP3]) &&
		(_mtk_gop_set_mux_clock(dev, clk_gop3_sram, "clk_gop3_sram", true)))
		return -ENODEV;

	return 0;
}

static int mtk_gop_setMixer_OutMode(struct mtk_tv_kms_context *pctx_kms)
{
	struct mtk_panel_context *panel_priv;
	int ret = 0;
	bool Mixer1_OutPreAlpha, Mixer2_OutPreAlpha;

	if (!pctx_kms) {
		DRM_ERROR("[GOP][%s, %d]pctx_kms is null pointer\n", __func__, __LINE__);
		return -ENXIO;
	}

	panel_priv = &pctx_kms->panel_priv;

	if (pctx_kms->out_model == E_OUT_MODEL_VG_BLENDED) {
		if (panel_priv->info.graphic_mixer1_out_prealpha == 1)
			Mixer1_OutPreAlpha = true;
		else
			Mixer1_OutPreAlpha = false;

		if (panel_priv->info.graphic_mixer2_out_prealpha == 1)
			Mixer2_OutPreAlpha = true;
		else
			Mixer2_OutPreAlpha = false;
	} else {
		if (panel_priv->gfx_info.graphic_mixer1_out_prealpha == 1)
			Mixer1_OutPreAlpha = true;
		else
			Mixer1_OutPreAlpha = false;

		if (panel_priv->gfx_info.graphic_mixer2_out_prealpha == 1)
			Mixer2_OutPreAlpha = true;
		else
			Mixer2_OutPreAlpha = false;
	}

	ret = gop_wrapper_set_MixerOutMode(Mixer1_OutPreAlpha, Mixer2_OutPreAlpha);

	return ret;
}

static int mtk_gop_pq_init(struct device *dev, struct mtk_tv_kms_context *pctx)
{
	MS_U8 i = 0;
	struct ST_PQ_CTX_SIZE_INFO PQSizeInfo;
	struct ST_PQ_CTX_SIZE_STATUS PQCtxSize;
	struct ST_PQ_FRAME_CTX_INFO FrameCtxInfo;
	struct ST_PQ_FRAME_CTX_STATUS FrameCtxStatus;
	struct ST_PQ_CTX_INFO PQCtxInfo;
	struct ST_PQ_CTX_STATUS PQCtxStatus;
	void *pCtx = NULL;
	struct ST_PQ_NTS_CAPABILITY_INFO PqNtsCapInfo;
	struct ST_PQ_NTS_CAPABILITY_STATUS PqNtsCapStatus;
	struct ST_PQ_NTS_INITHWREG_INFO PqInitHWRegInfo;
	struct ST_PQ_NTS_INITHWREG_STATUS PqInitHWRegStatus;
	struct mtk_gop_context *pctx_gop;
	struct mtk_autogen_context *autogen_priv;
	enum EN_PQAPI_RESULT_CODES enRet;

	if (!pctx || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx and dev are null pointer\n",
			__func__, __LINE__);
		return -ENXIO;
	}

	pctx_gop = &pctx->gop_priv;
	autogen_priv = &pctx->autogen_priv;

	pctx_gop->gop_pq =  devm_kzalloc(dev,
		sizeof(MTK_GOP_PQ_INFO) * pctx_gop->GopNum, GFP_KERNEL);
	if (!pctx_gop->gop_pq) {
		DRM_ERROR("[GOP][%s, %d]: gop_pq devm_kzalloc failed.\n",
			__func__, __LINE__);
		goto ERR;
	}

	memset(&PQSizeInfo, 0, sizeof(struct ST_PQ_CTX_SIZE_INFO));
	memset(&PQCtxSize, 0, sizeof(struct ST_PQ_CTX_SIZE_STATUS));
	MApi_PQ_GetCtxSize(NULL, &PQSizeInfo, &PQCtxSize);

	for (i = 0; i < pctx_gop->GopNum; i++) {
		memset(&FrameCtxInfo, 0, sizeof(struct ST_PQ_FRAME_CTX_INFO));
		memset(&FrameCtxStatus, 0, sizeof(struct ST_PQ_FRAME_CTX_STATUS));
		memset(&PQCtxInfo, 0, sizeof(struct ST_PQ_CTX_INFO));
		memset(&PQCtxStatus, 0, sizeof(struct ST_PQ_CTX_STATUS));
		memset(&PqNtsCapInfo, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_INFO));
		memset(&PqNtsCapStatus, 0, sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS));
		memset(&PqInitHWRegInfo, 0, sizeof(struct ST_PQ_NTS_INITHWREG_INFO));
		memset(&PqInitHWRegStatus, 0, sizeof(struct ST_PQ_NTS_INITHWREG_STATUS));
		pctx_gop->gop_pq[i].pCtx =
			devm_kzalloc(dev, PQCtxSize.u32PqCtxSize, GFP_KERNEL);
		if (!pctx_gop->gop_pq[i].pCtx) {
			DRM_ERROR("[GOP][%s, %d]: PQ ctx devm_kzalloc failed.\n",
				__func__, __LINE__);
			goto ERR;
		}
		memset(pctx_gop->gop_pq[i].pCtx, 0, PQCtxSize.u32PqCtxSize);

		pctx_gop->gop_pq[i].pSwReg =
			devm_kzalloc(dev, PQCtxSize.u32SwRegSize, GFP_KERNEL);
		if (!pctx_gop->gop_pq[i].pSwReg) {
			DRM_ERROR("[GOP][%s, %d]: PQ pSwReg devm_kzalloc failed.\n",
				__func__, __LINE__);
			goto ERR;
		}
		memset(pctx_gop->gop_pq[i].pSwReg, 0, PQCtxSize.u32SwRegSize);

		pctx_gop->gop_pq[i].pMetadata =
			devm_kzalloc(dev, PQCtxSize.u32MetadataSize, GFP_KERNEL);
		if (!pctx_gop->gop_pq[i].pMetadata) {
			DRM_ERROR("[GOP][%s, %d]: PQ pMetadata devm_kzalloc failed.\n",
				__func__, __LINE__);
			goto ERR;
		}
		memset(pctx_gop->gop_pq[i].pMetadata, 0, PQCtxSize.u32MetadataSize);

		pctx_gop->gop_pq[i].pHwReport =
			devm_kzalloc(dev, PQCtxSize.u32HwReportSize, GFP_KERNEL);
		if (!pctx_gop->gop_pq[i].pHwReport) {
			DRM_ERROR("[GOP][%s, %d]: PQ pHwReport devm_kzalloc failed.\n",
				__func__, __LINE__);
			goto ERR;
		}
		memset(pctx_gop->gop_pq[i].pHwReport, 0, PQCtxSize.u32HwReportSize);

		pCtx = pctx_gop->gop_pq[i].pCtx;
		FrameCtxInfo.pSwReg = pctx_gop->gop_pq[i].pSwReg;
		FrameCtxInfo.pMetadata = pctx_gop->gop_pq[i].pMetadata;
		FrameCtxInfo.pHwReport = pctx_gop->gop_pq[i].pHwReport;
		if (MApi_PQ_SetFrameCtx(pCtx, &FrameCtxInfo, &FrameCtxStatus)
			!= E_PQAPI_RC_SUCCESS) {
			DRM_ERROR("[GOP][%s, %d]: MApi_PQ_SetFrameCtx failed.\n",
				__func__, __LINE__);
			goto ERR;
		}

		PQCtxInfo.eWinID = E_PQ_MAIN_WINDOW;
		if (MApi_PQ_InitCtx(pCtx, &PQCtxInfo, &PQCtxStatus) != E_PQAPI_RC_SUCCESS) {
			DRM_ERROR("[GOP][%s, %d]: MApi_PQ_InitCtx failed.\n", __func__, __LINE__);
			goto ERR;
		}

		PqNtsCapInfo.eDomain = E_PQ_DOMAIN_GOP;
		PqNtsCapInfo.u16CapabilitySize = autogen_priv->capability_size;
		PqNtsCapInfo.pCapability = autogen_priv->capability;
		PqNtsCapInfo.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_INFO);
		PqNtsCapInfo.u32Version = API_VERSION_MAPI_PQ_SETCAPABILITY_NONTS;
		PqNtsCapStatus.u32Length = sizeof(struct ST_PQ_NTS_CAPABILITY_STATUS);
		if (MApi_PQ_SetCapability_NonTs(&PqNtsCapInfo, &PqNtsCapStatus)
			!= E_PQAPI_RC_SUCCESS) {
			DRM_ERROR("[GOP][%s, %d]: MApi_PQ_SetCapability_NonTs failed.\n",
				__func__, __LINE__);
			goto ERR;
		}

		PqInitHWRegInfo.eDomain = E_PQ_DOMAIN_GOP;
		enRet = MApi_PQ_InitHWRegTable_NonTs(&PqInitHWRegInfo, &PqInitHWRegStatus);
		if ((enRet != E_PQAPI_RC_SUCCESS) && (enRet != E_PQAPI_RC_FAIL_HW_NOT_SUPPORT)) {
			DRM_ERROR("[GOP][%s, %d]: MApi_PQ_InitHWRegTable_NonTs failed with %d\n",
				__func__, __LINE__, enRet);
			goto ERR;
		}

		if (gop_wrapper_set_PQCtxInfo(i, &pctx_gop->gop_pq[i])) {
			DRM_ERROR("[GOP][%s, %d]: gop_wrapper_set_PQCtxInfo failed.\n",
				__func__, __LINE__);
			goto ERR;
		}
	}

	return 0;

ERR:
	clean_gop_pq_context(dev, pctx_gop);

	return -1;
}

static int mtk_gop_adl_init(struct device *dev, struct mtk_gop_context *pctx_gop)
{
	struct sm_adl_res stRes;
	int i = 0, j = 0, gopIdx = 0;

	memset(&stRes, 0, sizeof(struct sm_adl_res));

	if (!pctx_gop || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx and dev are null pointer\n",
			__func__, __LINE__);
		return -ENXIO;
	}

	pctx_gop->gop_adl =  devm_kzalloc(dev,
		sizeof(MTK_GOP_ADL_INFO) * pctx_gop->GopNum, GFP_KERNEL);

	if (pctx_gop->gop_adl == NULL) {
		DRM_ERROR("[GOP][%s, %d]: devm_kzalloc failed.\n", __func__, __LINE__);
		goto ERR;
	}

	for (i = E_SM_TCON_ADL_GOP_0; i <= E_SM_TCON_ADL_GOP_3; i++) {
		stRes.client_type = i;
		stRes.trig_mode = E_SM_ADL_TRIGGER_MODE;
		switch (i) {
		case E_SM_TCON_ADL_GOP_0:
			gopIdx = GOP0;
			break;
		case E_SM_TCON_ADL_GOP_1:
			gopIdx = GOP1;
			break;
		case E_SM_TCON_ADL_GOP_2:
			gopIdx = GOP2;
			break;
		case E_SM_TCON_ADL_GOP_3:
			gopIdx = GOP3;
			break;
		default:
			break;
		}

		if (pctx_gop->gop_plane_capability.u8SupportHDRMode[gopIdx] != 0 &&
			pctx_gop->gop_plane_capability.u8SupportXvyccLite[gopIdx] == false) {
			if (sm_adl_create_resource(&pctx_gop->gop_adl[j].fd, &stRes) !=
				E_SM_RET_OK) {
				DRM_ERROR("[GOP][%s, %d]: sm_adl_create_resource %d failed.\n",
					__func__, __LINE__, i);
				goto ERR;
			}
		}
		j = j + 1;
	}

	return 0;

ERR:
	clean_gop_adl_context(dev, pctx_gop);

	return -1;
}

static int mtk_gop_ml_init(struct device *dev, enum OUTPUT_MODEL out_model,
	struct mtk_gop_context *pctx_gop)
{
	struct sm_ml_res stRes, stResVGSep;
	bool bNeedMlVGRes = false;

	if (!pctx_gop || !dev) {
		DRM_ERROR("[GOP][%s, %d]pctx and dev are null pointer\n",
			__func__, __LINE__);
		goto ERR;
	}

	memset(&stRes, 0, sizeof(struct sm_ml_res));
	memset(&stResVGSep, 0, sizeof(struct sm_ml_res));

	if (pctx_gop->gop_plane_capability.u8SupportBlendVideoIrq) {
		stRes.sync_id = E_SM_ML_OSD_SYNC;
	} else {
		stRes.sync_id = E_SM_ML_DISP_SYNC;
		stResVGSep.sync_id = E_SM_ML_OSD_SYNC;
		bNeedMlVGRes = true;
	}
	stRes.buffer_cnt = GOP_ML_BUF_CNT;
	stRes.cmd_cnt = GOP_MAX_ML_CMD_NUM;

	if (bNeedMlVGRes) {
		stResVGSep.buffer_cnt = GOP_ML_BUF_CNT;
		stResVGSep.cmd_cnt = GOP_MAX_ML_CMD_NUM;
	}

	pctx_gop->gop_ml =  devm_kzalloc(dev, sizeof(MTK_GOP_ML_INFO),
		GFP_KERNEL);
	if (pctx_gop->gop_ml == NULL) {
		DRM_ERROR("[GOP][%s, %d]: devm_kzalloc failed.\n", __func__, __LINE__);
		goto ERR;
	}

	if (gop_use_ml) {
		stRes.unique_id = E_SM_ML_UID_DISP_GOP;
		memcpy(&stRes.cl_name[0], "DISP_GOP", sizeof("DISP_GOP"));

		if (sm_ml_create_resource(&pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_DISP_SYNC],
			&stRes) != E_SM_RET_OK) {
			DRM_ERROR("[GOP][%s, %d]: sm_ml_create_resource 0 failed\n",
				__func__, __LINE__);
			goto ERR;
		}

		if (bNeedMlVGRes) {
			stResVGSep.unique_id = E_SM_ML_UID_VGSEP_GOP;
			memcpy(&stResVGSep.cl_name[0], "VGSEP_GOP", sizeof("VGSEP_GOP"));

			if (sm_ml_create_resource(&pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_OSD_SYNC],
				&stResVGSep) != E_SM_RET_OK) {
				DRM_ERROR("[GOP][%s, %d]: sm_ml_create_resource 1 failed\n",
					__func__, __LINE__);
				goto ERR;
			}
		} else {
			pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_OSD_SYNC] =
				pctx_gop->gop_ml->fd[E_GOP_WRAP_ML_DISP_SYNC];
		}
	}

	return 0;

ERR:
	gop_use_ml = 0;
	clean_gop_ml_context(dev, pctx_gop);

	return -1;
}

int mtk_gop_init(struct device *dev, struct device *master, void *data,
		 struct mtk_drm_plane **primary_plane, struct mtk_drm_plane **cursor_plane)
{
	int ret = 0, i = 0;
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_gop_context *pctx_gop = &pctx->gop_priv;
	struct mtk_tv_drm_crtc *crtc = &pctx->crtc[MTK_DRM_CRTC_TYPE_GRAPHIC];
	struct mtk_tv_drm_connector *connector = &pctx->connector[MTK_DRM_CONNECTOR_TYPE_GRAPHIC];
	struct mtk_drm_plane *mplane = NULL;
	struct drm_device *drm_dev = data;
	enum drm_plane_type drmPlaneType;
	unsigned int gop_idx;
	struct gop_prop *props = NULL;
	char timeline_name[MAX_TIMELINE_NAME] = "";
	struct mtk_drm_panel_context *pStPanelCtx = NULL;

	Debug_GOPOnOFF_Mode = false;
	utp_gop_InitByGop(NULL, NULL);	// get instance only
	pctx_gop->GopNum = pctx->plane_num[MTK_DRM_PLANE_TYPE_GRAPHIC];
	pctx_gop->nBootlogoGop = mtk_gop_get_bootlogo_gop(pctx_gop->GopNum, pctx);

	mplane = devm_kzalloc(dev, sizeof(struct mtk_drm_plane) * pctx_gop->GopNum, GFP_KERNEL);
	if (mplane == NULL) {
		DRM_ERROR("[GOP][%s, %d]: devm_kzalloc failed.\n", __func__, __LINE__);
		goto ERR;
	}

	for (i = 0; i < MTK_DRM_CRTC_TYPE_MAX; i++) {
		pctx->crtc[i].plane[MTK_DRM_PLANE_TYPE_GRAPHIC] = mplane;
		pctx->crtc[i].plane_count[MTK_DRM_PLANE_TYPE_GRAPHIC] = pctx_gop->GopNum;
	}

	connector->connector_private = pctx;

	props = devm_kzalloc(dev, sizeof(struct gop_prop) * pctx_gop->GopNum, GFP_KERNEL);
	if (props == NULL) {
		DRM_ERROR("[GOP][%s, %d]: devm_kzalloc failed.\n", __func__, __LINE__);
		goto ERR;
	}
	memset(props, 0, sizeof(struct gop_prop) * pctx_gop->GopNum);
	pctx_gop->gop_plane_properties = props;

	ret = readDTB2GOPPrivate(pctx_gop);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: readDTB2GOPPrivate failed.\n", __func__, __LINE__);
		goto ERR;
	}

	ret = readChipProp2GOPPrivate(pctx_gop);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: readChipProp2GOPPrivate failed.\n", __func__,
			  __LINE__);
		goto ERR;
	}

	if (mtk_gop_ml_init(dev, pctx->out_model, pctx_gop))
		DRM_ERROR("[GOP][%s, %d]: mtk_gop_ml_init failed.\n", __func__, __LINE__);

	ret = mtk_gop_init_clock(dev, pctx->out_model, pctx_gop);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: mtk_gop_init_clock failed.\n", __func__,
			  __LINE__);
		goto ERR;
	}

	pStPanelCtx = _gfx_panel_ctx();
	ret = mtk_gop_set_tgen(pStPanelCtx);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: mtk_gop_set_tgen failed.\n", __func__,
			  __LINE__);
		goto ERR;
	}

	ret = mtk_gop_setMixer_OutMode(pctx);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: mtk_gop_setMixer_OutMode failed.\n", __func__,
			  __LINE__);
		goto ERR;
	}

	ret = mtk_drm_create_properties(pctx);
	if (ret != 0x0) {
		DRM_ERROR("[GOP][%s, %d]: mtk_drm_create_properties failed.\n", __func__,
			  __LINE__);
		goto ERR;
	}

	mtk_drm_gop_CreateSysFile(dev);

	mtk_gop_adl_init(dev, pctx_gop);

	mtk_gop_pq_init(dev, pctx);

	for (gop_idx = 0; gop_idx < pctx_gop->GopNum; gop_idx++) {
		drmPlaneType =
		    (gop_idx ==
		     (pctx_gop->GopNum - 1)) ? DRM_PLANE_TYPE_CURSOR : DRM_PLANE_TYPE_OVERLAY;
		if (drmPlaneType == DRM_PLANE_TYPE_CURSOR)
			*cursor_plane = &crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx];

		ret =
		    mtk_plane_init(drm_dev, &crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].base,
				   MTK_PLANE_DISPLAY_PIPE, drmPlaneType);

		if (ret != 0x0) {
			DRM_ERROR("[GOP][%s, %d]: mtk_plane_init failed.\n", __func__,
				  __LINE__);
			goto ERR;
		}

		crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].crtc_base = &crtc->base;
		crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].crtc_private = pctx;
		crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].zpos = gop_idx;
		crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].gop_index = gop_idx;
		crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].mtk_plane_type =
		    MTK_DRM_PLANE_TYPE_GRAPHIC;

		if (mtk_drm_attach_props
		    (pctx_gop, &crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx]) < 0) {
			DRM_ERROR("[GOP][%s, %d]: mtk_drm_attach_props fai.\n", __func__,
				  __LINE__);
			goto ERR;
		}

		pctx_gop->utpGopInit.gop_idx =
		    crtc->plane[MTK_DRM_PLANE_TYPE_GRAPHIC][gop_idx].gop_index;

		snprintf(timeline_name, sizeof(timeline_name), "timeline-gop%d", gop_idx);
		pctx_gop->gop_sync_timeline[gop_idx] = mtk_tv_sync_timeline_create(timeline_name);
		DRM_INFO("[%s][%d]gop:%d, timeline=0x%p\n",
			__func__, __LINE__, gop_idx, pctx_gop->gop_sync_timeline[gop_idx]);

		if ((pctx_gop->utpGopInit.gop_idx == pctx_gop->nBootlogoGop) ||
			(pctx_gop->utpGopInit.gop_idx == bootlogo_dual_idx)) {
			continue;
		} else {
			pctx_gop->utpGopInit.ignore_level = E_GOP_WRAP_IGNORE_NORMAL;
		}

		if (pctx->out_model == E_OUT_MODEL_VG_BLENDED) {
			pctx_gop->utpGopInit.panel_width = pctx->panel_priv.info.de_width;
			pctx_gop->utpGopInit.panel_height = pctx->panel_priv.info.de_height;
			pctx_gop->utpGopInit.panel_hstart = pctx->panel_priv.info.de_hstart;
			pctx_gop->utpGopInit.eGopDstPath = E_GOP_WARP_DST_OP;
		} else {
			pctx_gop->utpGopInit.panel_width = pctx->panel_priv.gfx_info.de_width;
			pctx_gop->utpGopInit.panel_height = pctx->panel_priv.gfx_info.de_height;
			pctx_gop->utpGopInit.panel_hstart = pctx->panel_priv.gfx_info.de_hstart;
			pctx_gop->utpGopInit.eGopDstPath = E_GOP_WARP_DST_VG_SEPARATE;
		}

		ret = utp_gop_InitByGop(&pctx_gop->utpGopInit, pctx_gop->gop_ml);

		if (ret != 0x0) {
			DRM_ERROR("[GOP][%s, %d]: utp_gop_InitByGop failed.\n", __func__,
				  __LINE__);
			goto ERR;
		}
	}

	return 0;

ERR:
	clean_gop_context(dev, pctx_gop);
	return ret;
}

int mtk_drm_gop_suspend(struct platform_device *dev, pm_message_t state)
{
	if (utp_gop_power_state(UTP_E_GOP_POWER_SUSPEND) != 0)
		DRM_ERROR("[GOP][%s, %d]: suspend failed.\n", __func__, __LINE__);

	return 0;
}

int mtk_drm_gop_resume(struct platform_device *dev)
{
	if (utp_gop_power_state(UTP_E_GOP_POWER_RESUME) != 0)
		DRM_ERROR("[GOP][%s, %d]: resume failed.\n", __func__, __LINE__);

	return 0;
}

