// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/delay.h>
#include "mtk_tv_drm_video_pixelshift.h"
#include "../mtk_tv_drm_kms.h"
#include "hwreg_render_video_pixelshift.h"
#include "mtk_tv_sm_ml.h"
#include "mtk_tv_drm_common.h"
#include "mtk_tv_drm_log.h"
#include <drm/mtk_tv_drm_panel.h>
#include "pqu_msg.h"
#include "ext_command_render_if.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_OLED

#define W_4K 3840
#define H_4K 2160
#define PST_TWICE 2
#define ORBIT_FLAG_SIZE 1
#define PIXELSHIFT_SHM_VER500 0x603000
#define PIXELSHIFT_SHM_VER200 0x1000
#define ORBIT_OFFSET 0x02
#define STILL_IMAGE 0x03
#define ORBITT_TIMEOUT 100

static void __iomem *u64OrbitVaddr;

int _mtk_video_pixelshift_demura_ctrl(uint32_t h_offset, uint32_t v_offset)
{
	struct msg_render_demura_orbit_info info;
	struct pqu_render_demura_orbit_param param;

	if (bPquEnable) { // RV55
		memset(&param, 0, sizeof(struct pqu_render_demura_orbit_param));
		param.orbit_h_offset = h_offset;
		param.orbit_v_offset = v_offset;
		DRM_INFO("[%s][%d] RV55\n", __func__, __LINE__);
		pqu_render_demura_orbit((const struct pqu_render_demura_orbit_param *)&param, NULL);
	} else { // PQU
		memset(&info, 0, sizeof(struct msg_render_demura_orbit_info));
		info.orbit_h_offset = h_offset;
		info.orbit_v_offset = v_offset;
		DRM_INFO("[%s][%d] PQU\n", __func__, __LINE__);
		pqu_msg_send(PQU_MSG_SEND_RENDER_DEMURA_ORBIT, (void *)&info);
	}
	DRM_INFO("[%s][%d] PQU send done\n", __func__, __LINE__);

	return 0;
}

static int _video_pixel_get_memory_info(u32 *addr)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;

	// find memory_info node in dts
	target_memory_np = of_find_node_by_name(NULL, "memory_info");

	if (!target_memory_np)
		return -ENODEV;

	// query cpu_emi0_base value from memory_info node
	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);

	if (p != NULL) {
		*addr = be32_to_cpup(p);
		of_node_put(target_memory_np);
		p = NULL;
	} else {
		DRM_ERROR("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}


static void _mtk_video_set_pixelshift_ml_cmd(
	struct reg_info *reg,
	uint16_t RegCount,
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *video_priv = &pctx->video_priv;
	int ret = 0;
	int check = 0;
	bool stub, debug;

	check = drv_hwreg_common_get_stub(&stub);
	if (check == 0 && stub) {
		DRM_INFO("[%s][%d] Stub does not fire ml cmd\n",
			__func__, __LINE__);
		return;
	}

	debug = (video_priv->disp_ml.mem_idx == MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX);
	// get memory index if setting is triggered by debug cmd
	if (debug) {
		ret = mtk_tv_sm_ml_get_mem_index(&video_priv->disp_ml);
		if (ret) {
			DRM_ERROR("[%s, %d]: disp_ml get mem index fail", __func__, __LINE__);
			return;
		}
	}

	// Add register write command
	ret = mtk_tv_sm_ml_write_cmd(&video_priv->disp_ml, RegCount, (struct sm_reg *)reg);
	if (ret)
		DRM_ERROR("[%s, %d]: disp_ml append cmd fail", __func__, __LINE__);

	// fire ml and release memory index if it's debug cmd
	if (debug) {
		ret = mtk_tv_sm_ml_fire(&video_priv->disp_irq_ml);
		if (ret)
			DRM_ERROR("[%s, %d]: disp_irq_ml fire fail", __func__, __LINE__);
	}
}

void _mtk_video_set_pixelshift_post_justcan(
	struct mtk_pixelshift_context *ps_priv,
	struct ext_crtc_prop_info *crtc_props,
	struct hwregOut *paramOut,
	bool bRIU,
	struct reg_info *reg,
	uint16_t *RegCount)
{

	if (ps_priv->u8OLEDPixleshiftType == PIXELSHIFT_POST_JUSTSCAN) {
		ps_priv->bIsOLEDPixelshiftOsdAttached = true;
		crtc_props->prop_val[E_EXT_CRTC_PROP_PIXELSHIFT_OSD_ATTACH] = true;

		//BKA3A3_h7F[7:7] = vg_h_direction
		//BKA3A3_h7F[6:0] = vg_h_pixel
		drv_hwreg_render_pixelshift_set_vg_h(
			paramOut,
			bRIU,
			ps_priv->i8OLEDPixelshiftHDirection);
		*RegCount = *RegCount + paramOut->regCount;
		paramOut->reg = reg + *RegCount;

		//BKA3A3_h7F[15:15] = vg_v_direction
		//BKA3A3_h7F[14:8]  = vg_v_pixel
		drv_hwreg_render_pixelshift_set_vg_v(
			paramOut,
			bRIU,
			ps_priv->i8OLEDPixelshiftVDirection);
		*RegCount = *RegCount + paramOut->regCount;
		paramOut->reg = reg + *RegCount;
		// Shift demura table offset to opposite direction to align in final position
		_mtk_video_pixelshift_demura_ctrl((uint32_t)(ps_priv->i8OLEDPixelshiftHRangeMax-
		ps_priv->i8OLEDPixelshiftHDirection), (uint32_t)(ps_priv->i8OLEDPixelshiftVRangeMax-
		ps_priv->i8OLEDPixelshiftVDirection));
	}
}


static int _mtk_video_set_pixelshift_direction(struct mtk_tv_kms_context *pctx)
{
	bool bRIU = false;
	uint16_t RegCount = 0;
	uint8_t u8ThreadWaitTime = 0;
	uint32_t chip_miu0_bus_base = 0;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PIXELSHIFT];
	struct hwregOut paramOut;
	int ret = 0;

	DRM_INFO("[%s][%d] Enter\n", __func__, __LINE__);
	ret = _video_pixel_get_memory_info(&chip_miu0_bus_base);

	if (ret < 0) {
		DRM_ERROR("Failed to get memory info\n");
		return ret;
	}

	if ((pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200) ||
	(pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0500)) {
		if ((pctx->ld_priv.u64LDM_phyAddress != 0)
		&& (pctx->ld_priv.u32LDM_mmapsize != 0)) {
			if (u64OrbitVaddr == NULL) {
				u64OrbitVaddr = memremap((size_t)(pctx->ld_priv.u64LDM_phyAddress
					+ chip_miu0_bus_base),
					pctx->ld_priv.u32LDM_mmapsize,
					MEMREMAP_WB);
				DRM_INFO("[PIXELSHIFT][%s, %d] LDM phyAddress : 0x%llx\n",
					__func__, __LINE__, pctx->ld_priv.u64LDM_phyAddress);
				DRM_INFO("[PIXELSHIFT][%s, %d] LDM size : 0x%x\n",
					__func__, __LINE__, pctx->ld_priv.u32LDM_mmapsize);
				DRM_INFO("[PIXELSHIFT][%s, %d] LDM VirtAddress : 0x%p\n",
					__func__, __LINE__, u64OrbitVaddr);

				if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200)
					memset((u64OrbitVaddr + pctx->ld_priv.u32LDM_mmapsize -
						PIXELSHIFT_SHM_VER200 + ORBIT_OFFSET), 0, 1);
				else if (pctx->panel_priv.hw_info.pnl_lib_version ==
					DRV_PNL_VERSION0500)
					memset((u64OrbitVaddr +
						PIXELSHIFT_SHM_VER500 + ORBIT_OFFSET), 0, 1);
			}
		}
	}

	if (u64OrbitVaddr != NULL) {
		if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200) {
			memset((u64OrbitVaddr + pctx->ld_priv.u32LDM_mmapsize -
				PIXELSHIFT_SHM_VER200 + ORBIT_OFFSET), 1, ORBIT_FLAG_SIZE);

			while (*(uint8_t *)(u64OrbitVaddr + pctx->ld_priv.u32LDM_mmapsize -
				PIXELSHIFT_SHM_VER200 + ORBIT_OFFSET) != 0) {
				mdelay(1);
				u8ThreadWaitTime++;
				if (u8ThreadWaitTime == ORBITT_TIMEOUT) {
					DRM_INFO("[PIXELSHIFT] %s %d, share memory wait timeout\n",
					__func__, __LINE__);
					break;
				}
			}
		} else if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0500) {
			memset((u64OrbitVaddr +
				PIXELSHIFT_SHM_VER500 + ORBIT_OFFSET), 1, ORBIT_FLAG_SIZE);

			while (*(uint8_t *)(u64OrbitVaddr +
				PIXELSHIFT_SHM_VER500 + ORBIT_OFFSET) != 0) {
				mdelay(1);
				u8ThreadWaitTime++;
				if (u8ThreadWaitTime == ORBITT_TIMEOUT) {
					DRM_INFO("[PIXELSHIFT] %s %d, share memory wait timeout\n",
					__func__, __LINE__);
					break;
				}
			}
		}
	}

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	//BKA3A3_h7D[7:7] = video_h_direction
	//BKA3A3_h7D[6:0] = video_h_pixel
	drv_hwreg_render_pixelshift_set_video_h(
		&paramOut,
		bRIU,
		ps_priv->i8OLEDPixelshiftHDirection);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A3_h7D[15:15] = video_v_direction
	//BKA3A3_h7D[14:8] = video_v_pixel
	drv_hwreg_render_pixelshift_set_video_v(
		&paramOut,
		bRIU,
		ps_priv->i8OLEDPixelshiftVDirection);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	_mtk_video_set_pixelshift_post_justcan(
		ps_priv,
		crtc_props,
		&paramOut,
		bRIU,
		reg,
		&RegCount);

	if (ps_priv->bIsOLEDPixelshiftOsdAttached) {
		//BKA3A3_h7E[7:7] = graph_h_direction
		//BKA3A3_h7E[6:0] = graph_h_pixel
		drv_hwreg_render_pixelshift_set_graph_h(
			&paramOut,
			bRIU,
			ps_priv->i8OLEDPixelshiftHDirection);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;

		//BKA3A3_h7E[15:15] = graph_v_direction
		//BKA3A3_h7E[14:8] = graph_v_pixel
		drv_hwreg_render_pixelshift_set_graph_v(
			&paramOut,
			bRIU,
			ps_priv->i8OLEDPixelshiftVDirection);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
	}

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PIXELSHIFT);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_set_pixelshift_ml_cmd(reg, RegCount, pctx);

	DRM_INFO("[%s][%d] Fire ML\n", __func__, __LINE__);

	return 0;
}

static int _mtk_video_set_pixelshift_osd_attach(struct mtk_tv_kms_context *pctx)
{
	bool bRIU = false;
	uint16_t RegCount = 0;
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PIXELSHIFT];
	struct hwregOut paramOut;

	DRM_INFO("[%s][%d] Enter\n", __func__, __LINE__);

	// Set OSD direction to the same as video or back to (0, 0)
	if (ps_priv->bIsOLEDPixelshiftOsdAttached) {
		_mtk_video_set_pixelshift_direction(pctx);
	} else {
		memset(&reg, 0, sizeof(reg));
		memset(&paramOut, 0, sizeof(struct hwregOut));
		paramOut.reg = reg;

		//BKA3A3_h7E[7:7] = graph_h_direction
		//BKA3A3_h7E[6:0] = graph_h_pixel
		drv_hwreg_render_pixelshift_set_graph_h(
			&paramOut,
			bRIU,
			0);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;

		//BKA3A3_h7E[15:15] = graph_v_direction
		//BKA3A3_h7E[14:8] = graph_v_pixel
		drv_hwreg_render_pixelshift_set_graph_v(
			&paramOut,
			bRIU,
			0);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
	}

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PIXELSHIFT);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_set_pixelshift_ml_cmd(reg, RegCount, pctx);

	DRM_INFO("[%s][%d] Fire ML\n", __func__, __LINE__);

	return 0;
}

static int _mtk_video_set_pixelshift_type(struct mtk_tv_kms_context *pctx)
{
	bool bRIU = false;
	uint16_t RegCount = 0;
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct mtk_panel_context *panel_priv = &pctx->panel_priv;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PIXELSHIFT];
	struct hwregOut paramOut;
	enum video_pixelshift_type eType =
		(enum video_pixelshift_type)ps_priv->u8OLEDPixleshiftType;
	struct pixelshift_overscan_info overscaninfo;

	DRM_INFO("[%s][%d] Enter\n", __func__, __LINE__);

	if (!panel_priv->oled_info.oled_pixelshift) {
		DRM_INFO("[%s][%d] Pixel shift type %d not support\n", __func__, __LINE__, eType);
		return -EINVAL;
	}

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	memset(&overscaninfo, 0, sizeof(struct pixelshift_overscan_info));
	overscaninfo.u16FrameVEndProtect =
		panel_priv->info.de_vstart + panel_priv->info.de_height - 1;
	overscaninfo.u16FrameGEndProtect =
		panel_priv->info.de_vstart + panel_priv->info.de_height - 1;
	if (eType == PIXELSHIFT_PRE_OVERSCAN) {
		overscaninfo.u32HStart =
			panel_priv->info.de_hstart - ps_priv->i8OLEDPixelshiftHRangeMax;
		overscaninfo.u32HEnd =
			panel_priv->info.de_hstart + panel_priv->info.de_width - 1
			+ ps_priv->i8OLEDPixelshiftHRangeMax;
		overscaninfo.u32VStart =
			panel_priv->info.de_vstart - ps_priv->i8OLEDPixelshiftVRangeMax;
		overscaninfo.u32VEnd =
			panel_priv->info.de_vstart + panel_priv->info.de_height - 1
			+ ps_priv->i8OLEDPixelshiftVRangeMax;
	} else {
		overscaninfo.u32HStart = panel_priv->info.de_hstart;
		overscaninfo.u32HEnd = panel_priv->info.de_hstart + panel_priv->info.de_width - 1;
		overscaninfo.u32VStart = panel_priv->info.de_vstart;
		overscaninfo.u32VEnd = panel_priv->info.de_vstart + panel_priv->info.de_height - 1;
	}

	//BKA3A3_h76[3:3] = hw_cal_v_v_end_en
	//BKA3A3_h76[11:11] = hw_cal_g_v_end_en
	drv_hwreg_render_pixelshift_set_type(
		&paramOut,
		bRIU,
		eType,
		overscaninfo);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (ps_priv->u8OLEDPixleshiftType != PIXELSHIFT_POST_JUSTSCAN) {
		_mtk_video_pixelshift_demura_ctrl((uint32_t)(ps_priv->i8OLEDPixelshiftHRangeMax),
			(uint32_t)(ps_priv->i8OLEDPixelshiftVRangeMax));
	}

	// Set FO_HVSP scaling
	// [OLED_TODO] : fix to use correct window in video path instead of fohvsp"in"window
	/*
	plane_ctx->fo_hvsp_in_window.w = W_4K;
	plane_ctx->fo_hvsp_in_window.h = H_4K;

	if (eType == PIXELSHIFT_PRE_OVERSCAN) {
		plane_ctx->fo_hvsp_out_window.w = W_4K +
			(ps_priv->i8OLEDPixelshiftHRangeMax * PST_TWICE);
		plane_ctx->fo_hvsp_out_window.h = H_4K +
			(ps_priv->i8OLEDPixelshiftVRangeMax * PST_TWICE);
	} else {
		plane_ctx->fo_hvsp_out_window.w = W_4K;
		plane_ctx->fo_hvsp_out_window.h = H_4K;
	}
	*/

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PIXELSHIFT);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_set_pixelshift_ml_cmd(reg, RegCount, pctx);

	DRM_INFO("[%s][%d] Fire ML\n", __func__, __LINE__);

	_mtk_video_set_pixelshift_direction(pctx);

	return 0;
}

static int _mtk_video_set_pixelshift_enable(struct mtk_tv_kms_context *pctx)
{
	bool bRIU = false;
	uint16_t RegCount = 0;
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PIXELSHIFT];
	struct hwregOut paramOut;

	crtc_props->prop_val[E_EXT_CRTC_PROP_PIXELSHIFT_H] = 0;
	crtc_props->prop_val[E_EXT_CRTC_PROP_PIXELSHIFT_V] = 0;
	ps_priv->i8OLEDPixelshiftHDirection = 0;
	ps_priv->i8OLEDPixelshiftVDirection = 0;
	_mtk_video_set_pixelshift_direction(pctx);

	if (ps_priv->bIsOLEDPixelshiftEnable == false) {
		ps_priv->u8OLEDPixleshiftType = PIXELSHIFT_PRE_JUSTSCAN;
		_mtk_video_set_pixelshift_type(pctx);
	}

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	//BKA3A0_h70[5:4] = v0_frame_color_de_sel
	//BKA3A0_h70[7:7] = v0_en
	//BKA3A0_h71[7:7] = g0_en
	//BKA3A0_h71[15:15] = g1_en
	DRM_INFO("[%s][%d] Enter\n", __func__, __LINE__);
	drv_hwreg_render_pixelshift_set_enable(
		&paramOut,
		bRIU,
		ps_priv->bIsOLEDPixelshiftEnable);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PIXELSHIFT);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_set_pixelshift_ml_cmd(reg, RegCount, pctx);

	DRM_INFO("[%s][%d] Fire ML\n", __func__, __LINE__);

	return 0;
}

/* ------- above is static function -------*/
int mtk_video_pixelshift_init(struct device *dev)
{
	struct mtk_tv_kms_context *pctx = dev_get_drvdata(dev);
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;

	// start pixelshift with disable even if SoC support
	ps_priv->bIsOLEDPixelshiftEnable = false;
	ps_priv->bIsOLEDPixelshiftOsdAttached = true;
	ps_priv->u8OLEDPixleshiftType = PIXELSHIFT_PRE_JUSTSCAN;
	ps_priv->i8OLEDPixelshiftHDirection = 0;
	ps_priv->i8OLEDPixelshiftVDirection = 0;

	if (!pctx_pnl->oled_info.oled_pixelshift)
		return 0;

	_mtk_video_set_pixelshift_enable(pctx);
	DRM_INFO("[%s][%d] Pixel shift init done\n", __func__, __LINE__);

	return 0;
}

int mtk_video_pixelshift_suspend(struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_pixelshift_context *ps_priv = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!pdev) {
		DRM_INFO("[%s, %d]: platform_device is NULL.\n", __func__, __LINE__);
		return -ENODEV;
	}

	dev = &pdev->dev;
	pctx = dev_get_drvdata(dev);
	ps_priv = &pctx->pixelshift_priv;
	pctx_pnl = &pctx->panel_priv;

	// not support
	if (!pctx_pnl->oled_info.oled_pixelshift)
		return 0;

	DRM_INFO("[%s][%d] enable, type, attach, h, v = %d, %d, %d, %d, %d\n",
		__func__, __LINE__,
		ps_priv->bIsOLEDPixelshiftEnable,
		ps_priv->u8OLEDPixleshiftType,
		ps_priv->bIsOLEDPixelshiftOsdAttached,
		ps_priv->i8OLEDPixelshiftHDirection,
		ps_priv->i8OLEDPixelshiftVDirection);

	return 0;
}

int mtk_video_pixelshift_resume(struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_pixelshift_context *ps_priv = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!pdev) {
		DRM_INFO("[%s, %d]: platform_device is NULL.\n", __func__, __LINE__);
		return -ENODEV;
	}

	dev = &pdev->dev;
	pctx = dev_get_drvdata(dev);
	ps_priv = &pctx->pixelshift_priv;
	pctx_pnl = &pctx->panel_priv;

	// not support
	if (!pctx_pnl->oled_info.oled_pixelshift)
		return 0;

	_mtk_video_set_pixelshift_enable(pctx);
	DRM_INFO("[%s][%d] enable, type, attach, h, v = %d, %d, %d, %d, %d\n",
		__func__, __LINE__,
		ps_priv->bIsOLEDPixelshiftEnable,
		ps_priv->u8OLEDPixleshiftType,
		ps_priv->bIsOLEDPixelshiftOsdAttached,
		ps_priv->i8OLEDPixelshiftHDirection,
		ps_priv->i8OLEDPixelshiftVDirection);

	return 0;
}

int mtk_video_pixelshift_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_pixelshift_context *ps_priv = NULL;
	int8_t hv_direction = 0;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!crtc) {
		DRM_INFO("[%s][%d] NULL pointer\n", __func__, __LINE__);
		return 0;
	}

	pctx = (struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	ps_priv = &pctx->pixelshift_priv;
	pctx_pnl = &pctx->panel_priv;

	// Error handling
	if ((prop_index < E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE) ||
		(prop_index > E_EXT_CRTC_PROP_PIXELSHIFT_V)) {
		DRM_INFO("[%s][%d] Property %d invalid\n", __func__, __LINE__, prop_index);
		return 0;
	}
	prop_index = prop_index - E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE;

	if (!pctx_pnl->oled_info.oled_pixelshift) {
		DRM_INFO("[%s][%d] OLED pixel shift not support\n", __func__, __LINE__);
		ps_priv->prop_update[prop_index] = 0;
		return 0;
	}

	// Property is set in "mtk_tv_kms_atomic_set_crtc_property", not here
	// The setting (menuload) is triggered by "mtk_video_pixelshift_atomic_crtc_flush", not here
	// Here only check the update value.
	ps_priv->prop_update[prop_index] = true;
	switch (prop_index) {
	case E_PIXELSHIFT_PRIV_PROP_ENABLE:
		DRM_INFO("[%s][%d] Set pixelshift enable = %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		val = val > 0 ? 1 : 0;
		ps_priv->bIsOLEDPixelshiftEnable = val;
		break;
	case E_PIXELSHIFT_PRIV_PROP_ATTACH:
		DRM_INFO("[%s][%d] Set pixelshift osd attached= %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		val = val > 0 ? 1 : 0;
		// Post-JustScan cannot detach OSD
		if (ps_priv->u8OLEDPixleshiftType == PIXELSHIFT_POST_JUSTSCAN)
			val = 1;
		ps_priv->bIsOLEDPixelshiftOsdAttached = val;
		break;
	case E_PIXELSHIFT_PRIV_PROP_TYPE:
		DRM_INFO("[%s][%d] Set pixelshift type= %td\n",
			__func__, __LINE__, (ptrdiff_t)val);
		if (val > PIXELSHIFT_POST_JUSTSCAN ||
			!pctx_pnl->oled_info.oled_pixelshift) {
			DRM_INFO("[%s][%d] Not supported pixel shift type %td\n",
				__func__, __LINE__, (ptrdiff_t)val);
			return -EINVAL;
		}
		ps_priv->u8OLEDPixleshiftType = val;
		break;
	case E_PIXELSHIFT_PRIV_PROP_H:
		DRM_INFO("[%s][%d] Set H direction = %td, min~max = %td~%td\n",
		__func__, __LINE__, (ptrdiff_t)((int8_t)val),
		(ptrdiff_t)ps_priv->i8OLEDPixelshiftHRangeMin,
		(ptrdiff_t)ps_priv->i8OLEDPixelshiftHRangeMax);
		// Check range
		if ((int8_t)val < ps_priv->i8OLEDPixelshiftHRangeMin)
			hv_direction = ps_priv->i8OLEDPixelshiftHRangeMin;
		else if ((int8_t)val > ps_priv->i8OLEDPixelshiftHRangeMax)
			hv_direction = ps_priv->i8OLEDPixelshiftHRangeMax;
		else
			hv_direction = (int8_t)(val);
		// Update value
		ps_priv->i8OLEDPixelshiftHDirection = hv_direction;
		DRM_INFO("[%s][%d] Set H direction = %td\n",
		__func__, __LINE__, (ptrdiff_t)ps_priv->i8OLEDPixelshiftHDirection);
		break;
	case E_PIXELSHIFT_PRIV_PROP_V:
		DRM_INFO("[%s][%d] Set V direction = %td, min~max = %td~%td\n",
		__func__, __LINE__, (ptrdiff_t)((int8_t)val),
		(ptrdiff_t)ps_priv->i8OLEDPixelshiftVRangeMin,
		(ptrdiff_t)ps_priv->i8OLEDPixelshiftVRangeMax);
		// Check range
		if ((int8_t)val < ps_priv->i8OLEDPixelshiftVRangeMin)
			hv_direction = ps_priv->i8OLEDPixelshiftVRangeMin;
		else if ((int8_t)val > ps_priv->i8OLEDPixelshiftVRangeMax)
			hv_direction = ps_priv->i8OLEDPixelshiftVRangeMax;
		else
			hv_direction = (int8_t)(val);
		// Update value
		ps_priv->i8OLEDPixelshiftVDirection = hv_direction;
		DRM_INFO("[%s][%d] Set V direction = %td\n",
		__func__, __LINE__, (ptrdiff_t)ps_priv->i8OLEDPixelshiftVDirection);
		break;
	default:
		DRM_INFO("[%s][%d] invalid property!!\n",
			__func__, __LINE__);
		break;
	}

	return 0;
}
int mtk_video_pixelshift_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val,
	int prop_index)
{
	//int ret = -EINVAL;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	struct mtk_pixelshift_context *ps_priv = &pctx->pixelshift_priv;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	//struct drm_property_blob *property_blob = NULL;
	//int64_t int64Value = 0;

	// Error handling
	if (!pctx_pnl->oled_info.oled_pixelshift) {
		DRM_INFO("[%s][%d] OLED pixel shift not support\n", __func__, __LINE__);
		return 0;
	}

	if (val == NULL) {
		DRM_INFO("[%s][%d] OLED pixel shift val pointer is NULL\n", __func__, __LINE__);
		return 0;
	}
	if ((prop_index < E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE) ||
		(prop_index > E_EXT_CRTC_PROP_PIXELSHIFT_V)) {
		DRM_INFO("[%s][%d] Property %d invalid\n", __func__, __LINE__, prop_index);
		return 0;
	}

	prop_index = prop_index - E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE;
	switch (prop_index) {
	case E_PIXELSHIFT_PRIV_PROP_ENABLE:
		*val = (uint64_t)ps_priv->bIsOLEDPixelshiftEnable;
		DRM_INFO("[%s][%d] get pixel shift enable = %td\n",
			__func__, __LINE__, (ptrdiff_t)*val);
		break;
	case E_PIXELSHIFT_PRIV_PROP_ATTACH:
		*val = (uint64_t)ps_priv->bIsOLEDPixelshiftOsdAttached;
		DRM_INFO("[%s][%d] get pixel shift attach status = %td\n",
			__func__, __LINE__, (ptrdiff_t)*val);
		break;
	case E_PIXELSHIFT_PRIV_PROP_TYPE:
		*val = (uint64_t)ps_priv->u8OLEDPixleshiftType;
		DRM_INFO("[%s][%d] get pixel shift type = %td\n",
			__func__, __LINE__, (ptrdiff_t)*val);
		break;
	case E_PIXELSHIFT_PRIV_PROP_H:
		*val = (uint64_t)ps_priv->i8OLEDPixelshiftHDirection;
		DRM_INFO("[%s][%d] get pixel shift h = %td\n",
			__func__, __LINE__, (ptrdiff_t)*val);
		break;
	case E_PIXELSHIFT_PRIV_PROP_V:
		*val = (uint64_t)ps_priv->i8OLEDPixelshiftVDirection;
		DRM_INFO("[%s][%d] get pixel shift v = %td\n",
			__func__, __LINE__, (ptrdiff_t)*val);
		break;
	default:
		DRM_INFO("[DRM][VIDEO][%s][%d] Pixel shift not support getting property %d\n",
			__func__, __LINE__, prop_index);
		break;
	}

	return 0;
}
void mtk_video_pixelshift_atomic_crtc_flush(
	struct mtk_tv_drm_crtc *crtc)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_pixelshift_context *ps_priv = NULL;
	int prop_index = 0;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!crtc) {
		DRM_INFO("[%s][%d] NULL pointer\n", __func__, __LINE__);
		return;
	}
	pctx = (struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	ps_priv = &pctx->pixelshift_priv;
	pctx_pnl = &pctx->panel_priv;

	// Error handling
	if (!pctx_pnl->oled_info.oled_pixelshift)
		goto CLEAN_FLAG;

	// Set register value
	if (ps_priv->prop_update[E_PIXELSHIFT_PRIV_PROP_TYPE])
		_mtk_video_set_pixelshift_type(pctx);
	else if (ps_priv->prop_update[E_PIXELSHIFT_PRIV_PROP_H] ||
		ps_priv->prop_update[E_PIXELSHIFT_PRIV_PROP_V])
		_mtk_video_set_pixelshift_direction(pctx);
	else if (ps_priv->prop_update[E_PIXELSHIFT_PRIV_PROP_ATTACH])
		_mtk_video_set_pixelshift_osd_attach(pctx);
	if (ps_priv->prop_update[E_PIXELSHIFT_PRIV_PROP_ENABLE])
		_mtk_video_set_pixelshift_enable(pctx);

CLEAN_FLAG:
	// Clean update flag
	for (prop_index = E_PIXELSHIFT_PRIV_PROP_ENABLE;
	     prop_index <= E_PIXELSHIFT_PRIV_PROP_V; prop_index++) {
		ps_priv->prop_update[prop_index] = 0;
	}
}

/*--------------------------------------------------------------------------*/
/*                              Test Function                               */
/*--------------------------------------------------------------------------*/
static bool _mtk_video_get_value_from_string(char *buf, char *name,
	unsigned int len, int *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		DRM_INFO("Pointer is NULL!\n");
		return false;
	}

	*value = 0;
	cmd = buf;

	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0) {
			if (kstrtoint(tmp_value, 0, value)) {
				DRM_INFO("kstrtoint fail!\n");
				return find;
			}
			find = true;
			DRM_INFO("name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	DRM_INFO("name(%s) was not found!\n", name);

	return find;
}

int mtk_video_pixelshift_debug(const char *buf, struct device *dev)
{
	int len = 0, n = 0;
	int value = 0;
	bool find = false;
	char *cmd = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *crtc = NULL;

	DRM_INFO("[%s][%d] Enter\n", __func__, __LINE__);

	if ((buf == NULL) || (dev == NULL)) {
		DRM_INFO("Pointer is NULL!\n");
		return -EINVAL;
	}

	pctx = dev_get_drvdata(dev);
	crtc = &pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO];
	if (!pctx) {
		DRM_INFO("drm_ctx is NULL!\n");
		return -EINVAL;
	}

	cmd = vmalloc(sizeof(char) * 0x1000);
	if (cmd == NULL) {
		DRM_INFO("vmalloc cmd fail!\n");
		return -EINVAL;
	}

	memset(cmd, 0, sizeof(char) * 0x1000);
	len = strlen(buf);

	n = snprintf(cmd, len + 1, "%s", buf);
	if (n < 0) {
		pr_err("snprintf :%d", n);
		return -EPERM;
	}

	if (_mtk_video_get_value_from_string(cmd, "enable", len, &value)) {
		find = true;
		DRM_INFO("enable: %d\n", value);
		mtk_video_pixelshift_atomic_set_crtc_property(crtc, NULL, NULL,
			(uint64_t)value, E_EXT_CRTC_PROP_PIXELSHIFT_ENABLE);
	}

	if (_mtk_video_get_value_from_string(cmd, "attach", len, &value)) {
		find = true;
		DRM_INFO("attach: %d\n", value);
		mtk_video_pixelshift_atomic_set_crtc_property(crtc, NULL, NULL,
			(uint64_t)value, E_EXT_CRTC_PROP_PIXELSHIFT_OSD_ATTACH);
	}

	if (_mtk_video_get_value_from_string(cmd, "type", len, &value)) {
		find = true;
		DRM_INFO("type: %d\n", value);
		mtk_video_pixelshift_atomic_set_crtc_property(crtc, NULL, NULL,
			(uint64_t)value, E_EXT_CRTC_PROP_PIXELSHIFT_TYPE);
	}

	if (_mtk_video_get_value_from_string(cmd, "h", len, &value)) {
		find = true;
		DRM_INFO("h: %d\n", value);
		mtk_video_pixelshift_atomic_set_crtc_property(crtc, NULL, NULL,
			(uint64_t)value, E_EXT_CRTC_PROP_PIXELSHIFT_H);
	}

	if (_mtk_video_get_value_from_string(cmd, "v", len, &value)) {
		find = true;
		DRM_INFO("v: %d\n", value);
		mtk_video_pixelshift_atomic_set_crtc_property(crtc, NULL, NULL,
			(uint64_t)value, E_EXT_CRTC_PROP_PIXELSHIFT_V);
	}

	if (find)
		mtk_video_pixelshift_atomic_crtc_flush(crtc);
	else
		DRM_INFO("Please enter at least one parameter: enable/attach/type/h/v\n");

	vfree(cmd);
	return 0;
}

