// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#define MTK_DRM_MODEL MTK_DRM_MODEL_PANEL
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <drm/mtk_tv_drm.h>
#include <linux/delay.h> // mdelay
#include <linux/bitops.h>

#include "mtk_tv_drm_trigger_gen.h"
#include "../mtk_tv_lpll_tbl.h"
#include "../mtk_tv_drm_log.h"
#include "../mtk_tv_drm_kms.h"
#include "../metabuf/mtk_tv_drm_metabuf.h"
#include "hwreg_render_video_pnl.h"
#include "hwreg_render_common_trigger_gen.h"
#include "hwreg_srccap_common_trigger_gen.h"
#include "hwreg_pq_common_trigger_gen.h"
#include "common/hwreg_common_riu.h"
#include "pixelmonitor.h"
#include "mtk_tv_sm_ml.h"
#include "mtk_tv_drm_common.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "m_pqu_render.h"
#include "mtk_tv_drm_trigger_gen_tbl.h"
#include "hwreg_render_stub.h"

#define TG_DBG(x) x

#define TRIGGER_GEN_DEBUG (0)
#define B2R_SW_IRQ_DELAY_LINE_4K (1)
#define SRC0_PQ_IRQ_DELAY_LINE_4K (16)
#define IP_PQ_IRQ_DELAY_LINE_4K (16)
#define OP1_PQ_IRQ_DELAY_LINE_4K (16)
#define OP2_PQ_IRQ_DELAY_LINE_4K (16)
#define DISP_PQ_IRQ_DELAY_LINE_4K (16)

#define TGEN_XTAL_24M 24
#define IP_XTAL_12M 12
#define INPUT_4K2K60_ONE_LINE_TIME_USX10000 74074
#define INPUT_4K2K120_ONE_LINE_TIME_USX10000 37037
#define INPUT_4K2K240_ONE_LINE_TIME_USX10000 18518

#define XC_8K4K_WIDTH (7680)
#define XC_8K4K_HIGH (4320)
#define XC_4K2K_HTOTAL (4400)
#define XC_4K2K_WIDTH (3840)
#define XC_4K2K_HIGH (2160)
#define XC_4K1K_WIDTH (3840)
#define XC_4K1K_HIGH (1080)
#define XC_WQHD_WIDTH (2560)
#define XC_WQHD_HIGH  (1440)
#define XC_FHD_WIDTH (1920)
#ifndef XC_FHD_HIGH
#define XC_FHD_HIGH  (1080)
#endif
#define XC_720_HIGH (720)
#define XC_SD_WIDTH  (960)

#define JKKK_TODO 0

#define DEC_1000000 (1000000)
#define DEC_10000 (10000)
#define DEC_100 (100)
#define DEC_600 (600)

#define PQ_COMMON_TRIGEN_DOMAIN_OP1_PQ_IRQ_DLY_V    (0x680)

static uint32_t _s32FrameLockPhaseDiffValue;
static uint32_t _s32FrameLockSrcLockValue;
static uint32_t g_OuputFreqDivInputFreqx100;


static uint16_t g_u16CaseSelect = TGUC_A_0;

static int32_t g_src_htt2trig_httx100;
static int32_t g_out_htt2trig_httx100;
static int32_t g_4K_line2trig_httx100;
static int32_t g_Input4K_line2trig_httx100;
static int32_t g_src_htt2out_httx100;
static int32_t g_trig_htt2out_httx100;
static int32_t g_4k_line2out_httx100;

static int32_t g_out_htt2src_httx100;
static int32_t g_trig_htt2src_httx100;

static uint8_t s_u8delta_ip1;
static uint8_t s_u8delta_op1;
static uint8_t s_u8delta_op2;
static uint8_t s_u8delta_disp;

static st_trigger_gen_parameter *trigger_gen_parameter;
static mtktv_chip_series series = MTK_TV_SERIES_MERAK_1;
static atomic_t trig_setup_bypass;
static bool g_bIsUpdateRenderTG;
static atomic_t g_bIstgwork;

#define IP1_DELTA	2
#define OP1_DELTA       4
#define OP2_DELTA	6
#define DISP_DELTA	8

struct tg_record_info {
	struct drm_mtk_tv_trigger_gen_info input_info;
	int out_vtt;
	int out_de_start;
	int out_de_end;
	int out_xtail_cnt;
	int out_vfreq;
	uint16_t F1_D2_INPUT;
	uint16_t F1_D3_INPUT;
	uint8_t debug_setting;
	bool render_modify;
};
static struct tg_record_info tg_record;

static inline struct mtk_drm_plane * __maybe_unused
	__get_mtk_drm_plane(struct mtk_tv_kms_context *pctx)
{
	struct ext_crtc_prop_info *crtc_props;
	uint64_t plane_id;
	struct drm_mode_object *obj;
	struct drm_plane *plane;

	if (!pctx)
		return NULL;

	crtc_props = pctx->ext_crtc_properties;
	if (!crtc_props)
		return NULL;

	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
	if (!obj)
		return NULL;

	plane = obj_to_plane(obj);
	if (!plane)
		return NULL;

	return to_mtk_plane(plane);
}

static inline struct mtk_video_context * __maybe_unused
	__get_mtk_video_context(struct mtk_tv_kms_context *pctx)
{
	// already check the null in _sanity_check()
	struct mtk_drm_plane *mplane;
	struct mtk_tv_kms_context *pctx_plane;

	if (!pctx)
		return NULL;

	mplane = __get_mtk_drm_plane(pctx);
	if (!mplane)
		return NULL;

	pctx_plane = (struct mtk_tv_kms_context *)mplane->crtc_private;
	if (!pctx_plane)
		return NULL;

	return &pctx_plane->video_priv;
}

static void trigger_gen_overflowprotect(
	uint32_t u32TempA, uint32_t u32TempB, uint16_t *u16Value)
{
	if (u16Value == NULL)
		return;

	if (u32TempA >= u32TempB)
		*u16Value = (u32TempA - u32TempB) / 100;
	else
		*u16Value = 0;
}

static uint16_t _triggen_case_sel_ver1(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	bool is_gamemode;
	bool scmi_fbl;
	bool is_b2r;
	bool is_forceP;
	uint16_t input_vde;
	bool is_gamingMJC_en;

	is_gamemode = tg_info->LegacyMode;
	scmi_fbl = tg_info->IsSCMIFBL;
	is_b2r = tg_info->IsB2Rsource;
	is_forceP = tg_info->IsForceP;
	input_vde = tg_info->Vde;
	is_gamingMJC_en = tg_info->IsGamingMJCOn;

	DRM_INFO("[TG] scmi %s, game %s, b2r %s, gameMJC %s, in_vde %d\n",
				scmi_fbl ? "FBL" : "FB",
				is_gamemode ? "Y" : "N",
				is_b2r ? "Y" : "N",
				is_gamingMJC_en ? "Y" : "N",
				(int)input_vde);

	/* remove mdgw/frc dma case, the m6 series always enable */
	if (!scmi_fbl) {
		if ((is_gamemode) && (!is_gamingMJC_en))
			return TGUC_C_0; // Normal Legency
		else
			return TGUC_A_0; // Normal TS
	} else {
		if ((is_gamemode) && (!is_gamingMJC_en))
			return TGUC_D_0;
		else
			return TGUC_B_0;
	}

	return TGUC_MAX;
}

static uint16_t _triggen_case_ver2_forceP(
	uint32_t pnl_lib_version,
	bool frc_en,
	bool is_gamemode,
	uint16_t input_vde)
{
	if (pnl_lib_version == DRV_PNL_VERSION0600)
		if (frc_en == false)
			return TGUC_F_0;

	if (!is_gamemode)
		return TGUC_B_0;

	if (input_vde >= XC_FHD_HIGH)
		return TGUC_D_1;//special LL
	else
		return TGUC_D_0;//LL
}

static uint16_t _triggen_case_sel_ver2(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	bool frc_en;
	bool is_gamemode;
	bool scmi_fbl;
	bool is_b2r;
	bool is_forceP;
	bool is_pmode;
	bool is_gamingMJC_en;
	uint16_t input_vde;
	uint32_t Input_VFreqx1000;
	uint32_t max_output_freq;
	struct mtk_panel_context *pctx_pnl;

	frc_en = !tg_info->IsFrcMdgwFBL;
	is_gamemode = tg_info->IsLowLatency;
	scmi_fbl = tg_info->IsSCMIFBL;
	is_b2r = tg_info->IsB2Rsource;
	is_pmode = tg_info->IsPmodeEn;
	is_forceP = tg_info->IsForceP;
	input_vde = tg_info->Vde;
	Input_VFreqx1000 = tg_info->Input_Vfreqx1000;
	pctx_pnl = &pctx->panel_priv;
	is_gamingMJC_en = tg_info->IsGamingMJCOn;
	max_output_freq = pctx_pnl->outputTiming_info.u32OutputVfreq;

	DRM_INFO("[TG] scmi %s, frc %s, game %s, gameMJC %s, b2r %s, in_vde %d\n",
			scmi_fbl ? "FBL" : "FB",
			frc_en ? "FB" : "FBL",
			is_gamemode ? "Y" : "N",
			is_gamingMJC_en ? "Y" : "N",
			is_b2r ? "Y" : "N", (int)input_vde);

	/*
	 * Force P case
	 * SCMI FBL + FRC FB calculation for normal,
	 */
	if (is_forceP) {
		return _triggen_case_ver2_forceP(
			pctx_pnl->hw_info.pnl_lib_version,
			frc_en,
			is_gamemode,
			input_vde);
	}

	if (scmi_fbl) {
		if (frc_en) {
			if ((is_b2r && !is_gamemode) || (is_gamingMJC_en))
				return TGUC_B_0;

			if (input_vde >= XC_FHD_HIGH)
				return TGUC_D_1;//special LL
			else
				return TGUC_D_0;//LL
		} else
			return TGUC_F_0;
	} else {
		if (frc_en) {
			if ((is_gamemode) && (!is_gamingMJC_en)) {
				if (input_vde >= XC_FHD_HIGH)
					return TGUC_C_1; // special LL
				else
					return TGUC_C_0; //LL
			} else
				return TGUC_A_0;
		} else {
			if (input_vde >= XC_FHD_HIGH)
				return TGUC_E_1;
			else
				return TGUC_E_0;
		}
	}

	return TGUC_MAX;
}


static int trigger_gen_case_select(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info, uint16_t *case_select)
{
	uint16_t current_case = TGUC_MAX;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	//IdrframesInfo.height;//input_de_end - input_vsync_st + 1;

	pctx_pnl = &pctx->panel_priv;

	if (series == MTK_TV_SERIES_MERAK_1)
		current_case = _triggen_case_sel_ver1(pctx, tg_info);
	else if (series == MTK_TV_SERIES_MERAK_2)
		current_case = _triggen_case_sel_ver2(pctx, tg_info);
	else
		DRM_ERROR("%s: invalid chip version\n", __func__);

	if (current_case >= TGUC_MAX)
		return -EINVAL;

	TG_DBG(DRM_INFO(" <%s> current_case: %d\n", __func__, current_case));

	*case_select = current_case;

	return 0;
}

static void trigger_gen_srccap_formula(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	bool IS_B2RSource = false;
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t rwbank_upd, ds_mload, vsync, rd_trig;
	uint16_t b2r_rwbank_upd = 0, b2r_ds_mload = 0, b2r_vsync = 0, b2r_rd_trig = 0;
	uint16_t current_case = TGUC_A_0;
	uint16_t input_vtt;
	uint16_t input_vsyncwidth;
	uint32_t input_vbk;
	uint16_t input_vsync_st = 0;//todo ip1_04 input de start
	uint8_t u8DomainIdx = 0;
	uint16_t output_vtt = 0;
	uint16_t Out_div_In_factorx100;
	uint32_t u32TempA, u32TempB;
	uint8_t IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsSelSeperate = 1;
	uint8_t u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeVsync = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeRtrig = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeSwPqIrq = 1;  // 1: Vde falling , 0: Vsync rising
	uint32_t b2r_data_start = 0;
	uint16_t input_vde = 0;
	bool IS_Interlance = false;
	enum video_crtc_frame_sync_mode frame_sync_mode;
	uint32_t b2rvsyc_ipvsyc_diffx100;

	IS_B2RSource = tg_info->IsB2Rsource;
	pctx_pnl = &pctx->panel_priv;
	output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	input_vde = tg_info->Vde;
	IS_Interlance = !(tg_info->IsPmodeEn);
	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;
	current_case = g_u16CaseSelect;
	input_vsync_st = tg_info->Vde_start;
	input_vsyncwidth = tg_info->Vs_pulse_width;
	b2rvsyc_ipvsyc_diffx100 = 2 * g_src_htt2trig_httx100; // 2 line * output 1 line period

	input_vtt = tg_info->Vtotal - 2;

	if (IS_Interlance) {	// i mode case
		//nput_vde = input_vde/2;
		input_vtt = input_vtt - 2;
	}

	input_vbk = input_vtt - input_vde - IPM_READ_DELAY; //ip1_06 Blake discuss
	Out_div_In_factorx100 = output_vtt*100/input_vtt;
	b2r_data_start = (1 + trigger_gen_parameter[current_case].F1_D1_INPUT
				+ trigger_gen_parameter[current_case].F1_D2_INPUT
				+ trigger_gen_parameter[current_case].F1_D3_INPUT
				+ trigger_gen_parameter[current_case].F2_D5)
				* g_Input4K_line2trig_httx100;

	//position calculat
	if (frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) {
		IsVdeFallingmode = 0; // 1: Vde falling , 0: Vsync rising
		u8IsSelSeperate = 1;
		u8IsVdeFallingmode = 1 ;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeRtrig = 0;	// 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
	} else {
		IsVdeFallingmode = 1; // 1: Vde falling , 0: Vsync rising
		u8IsSelSeperate = 0;
		u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeRtrig = 1;	// 1: Vde falling , 0: Vsync rising
		u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
	}

	if (IS_B2RSource) {
		input_vbk = b2r_data_start;
		Out_div_In_factorx100 = 100;
		//b2r_rwbank_upd, b2r_ds_mload, b2r_vsync, b2r_rd_trig;
		b2r_rwbank_upd = 0;
		b2r_vsync = (b2r_data_start -
				(1 + trigger_gen_parameter[current_case].F1_D1_INPUT +
				trigger_gen_parameter[current_case].F1_D2_INPUT +
				trigger_gen_parameter[current_case].F1_D3_INPUT +
				trigger_gen_parameter[current_case].F2_D5) *
				g_Input4K_line2trig_httx100) / 100;
		b2r_rd_trig = (b2r_data_start
				- (trigger_gen_parameter[current_case].F2_D5
					* g_Input4K_line2trig_httx100)) / 100;
		rwbank_upd = (input_vbk
				- ((trigger_gen_parameter[current_case].F2_D5
						+ trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT
						+ trigger_gen_parameter[current_case].F1_D1_INPUT)
						* g_Input4K_line2trig_httx100)) / 100;
		ds_mload = (input_vbk
				- ((trigger_gen_parameter[current_case].F2_D5
						+ trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT)
						* g_Input4K_line2trig_httx100)) / 100;
		vsync = (input_vbk
				- ((trigger_gen_parameter[current_case].F2_D5
						+ trigger_gen_parameter[current_case].F1_D3_INPUT)
						* g_Input4K_line2trig_httx100)) / 100;
		rd_trig = (input_vbk
				- (trigger_gen_parameter[current_case].F2_D5
					* g_Input4K_line2trig_httx100)) / 100;
	} else {
		// rwbank_upd
		u32TempA = input_vbk * g_src_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D5
						+ trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT
						+ trigger_gen_parameter[current_case].F1_D1_INPUT)
						* g_Input4K_line2trig_httx100;
		trigger_gen_overflowprotect(u32TempA, u32TempB, &rwbank_upd);

		// ds_mload
		u32TempA = input_vbk * g_src_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D5
						+ trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT)
						* g_Input4K_line2trig_httx100;
		trigger_gen_overflowprotect(u32TempA, u32TempB, &ds_mload);

		// vsync
		u32TempA = input_vbk * g_src_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT)
						* g_Input4K_line2trig_httx100;
		trigger_gen_overflowprotect(u32TempA, u32TempB, &vsync);

		// rd_trig
		u32TempA = (u8IsVdeFallingmodeRtrig ? input_vbk
				: (input_vsync_st+input_vsyncwidth)) * g_src_htt2trig_httx100;
		u32TempB = trigger_gen_parameter[current_case].F2_D5
						* g_Input4K_line2trig_httx100;
		trigger_gen_overflowprotect(u32TempA, u32TempB, &rd_trig);
	}

	//position update into structure src0/src1/ip1
	#if (!TRIGGER_GEN_DEBUG)

	if (IS_B2RSource) {
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].RWBk_ud_dly_v = b2r_rwbank_upd;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].ds_dly_v = b2r_ds_mload;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].ml_dly_v = b2r_ds_mload;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].vs_dly_v =
			b2r_vsync + B2R_SW_IRQ_DELAY_LINE_4K * g_4K_line2trig_httx100 / 100;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].dma_r_dly_v = b2r_rd_trig;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].trig_sel = IsVdeFallingmode;

		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].sw_irq_dly_v = b2r_vsync;

		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].trig_sel_separate =
			u8IsSelSeperate;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].rwbank_trig_sel =
			u8IsVdeFallingmodeRwbk;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].ds_trig_sel =
			u8IsVdeFallingmodeDsMl;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].ml_trig_sel =
			u8IsVdeFallingmodeDsMl;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].vs_trig_sel =
			u8IsVdeFallingmodeVsync;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].dmar_trig_sel =
			u8IsVdeFallingmodeRtrig;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].swirq_trig_sel =
			u8IsVdeFallingmodeSwPqIrq;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].pqirq_trig_sel =
			u8IsVdeFallingmodeSwPqIrq;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_B2R].str_trig_sel =
			u8IsVdeFallingmodeSwPqIrq;
	} else {
		for (u8DomainIdx = 0; u8DomainIdx < SRCCAP_COMMON_TRIGEN_DOMAIN_MAX;
			u8DomainIdx++) {
			srccap_trigger_gen[u8DomainIdx].RWBk_ud_dly_v = rwbank_upd;
			srccap_trigger_gen[u8DomainIdx].ds_dly_v = ds_mload;
			srccap_trigger_gen[u8DomainIdx].ml_dly_v = ds_mload;
			srccap_trigger_gen[u8DomainIdx].vs_dly_v = vsync;
			srccap_trigger_gen[u8DomainIdx].dma_r_dly_v = rd_trig;
			srccap_trigger_gen[u8DomainIdx].trig_sel = IsVdeFallingmode;
			srccap_trigger_gen[u8DomainIdx].sw_irq_dly_v = vsync +
				SRC0_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/DEC_100;
			srccap_trigger_gen[u8DomainIdx].pq_irq_dly_v = vsync +
				SRC0_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/DEC_100;

			srccap_trigger_gen[u8DomainIdx].trig_sel_separate = u8IsSelSeperate;
			srccap_trigger_gen[u8DomainIdx].rwbank_trig_sel = u8IsVdeFallingmodeRwbk;
			srccap_trigger_gen[u8DomainIdx].ds_trig_sel = u8IsVdeFallingmodeDsMl;
			srccap_trigger_gen[u8DomainIdx].ml_trig_sel = u8IsVdeFallingmodeDsMl;
			srccap_trigger_gen[u8DomainIdx].vs_trig_sel = u8IsVdeFallingmodeVsync;
			srccap_trigger_gen[u8DomainIdx].dmar_trig_sel = u8IsVdeFallingmodeRtrig;
			srccap_trigger_gen[u8DomainIdx].swirq_trig_sel = u8IsVdeFallingmodeSwPqIrq;
			srccap_trigger_gen[u8DomainIdx].pqirq_trig_sel = u8IsVdeFallingmodeSwPqIrq;
			srccap_trigger_gen[u8DomainIdx].str_trig_sel = u8IsVdeFallingmodeSwPqIrq;
		}
	}
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].RWBk_ud_dly_v = rwbank_upd + s_u8delta_ip1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].ds_dly_v = ds_mload + s_u8delta_ip1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].ml_dly_v = ds_mload + s_u8delta_ip1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].vs_dly_v = vsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].dma_r_dly_v = rd_trig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].trig_sel = IsVdeFallingmode;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].pq_irq_dly_v = vsync +
		IP_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/100;

	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].trig_sel_separate = u8IsSelSeperate;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].rwbank_trig_sel = u8IsVdeFallingmodeRwbk;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].ds_trig_sel = u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].ml_trig_sel = u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].vs_trig_sel = u8IsVdeFallingmodeVsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].dmar_trig_sel = u8IsVdeFallingmodeRtrig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].swirq_trig_sel = u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].pqirq_trig_sel = u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].str_trig_sel = u8IsVdeFallingmodeSwPqIrq;
	#endif

	//debug message
	TG_DBG(DRM_INFO("<%s> current_case: %d\n", __func__, current_case));
	TG_DBG(DRM_INFO("input_vsync_st: %d\n", input_vsync_st));
	TG_DBG(DRM_INFO("input_vde: %d\n", input_vde));
	TG_DBG(DRM_INFO("input_vtt: %d\n", input_vtt));
	TG_DBG(DRM_INFO("input_vbk: %d\n", input_vbk));
	TG_DBG(DRM_INFO("input_vsyncwidth: %d\n", input_vsyncwidth));
	TG_DBG(DRM_INFO("b2r_data_start: %d\n", b2r_data_start));
	TG_DBG(DRM_INFO("Out_div_In_factorx100: %d\n", Out_div_In_factorx100));
	TG_DBG(DRM_INFO("IsVdeFallingmode: %d\n", IsVdeFallingmode));
	TG_DBG(DRM_INFO("rwbank_upd: %d\n", rwbank_upd));
	TG_DBG(DRM_INFO("ds_mload: %d\n", ds_mload));
	TG_DBG(DRM_INFO("vsync: %d\n", vsync));
	TG_DBG(DRM_INFO("rd_trig: %d\n", rd_trig));

	TG_DBG(DRM_INFO("b2r_rwbank_upd: %d\n", b2r_rwbank_upd));
	TG_DBG(DRM_INFO("b2r_ds_mload: %d\n", b2r_ds_mload));
	TG_DBG(DRM_INFO("b2r_vsync: %d\n", b2r_vsync));
	TG_DBG(DRM_INFO("b2r_rd_trig: %d\n", b2r_rd_trig));
}

static inline bool _need_pq_irq_patch(struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	/* only merak 2.0 need this patch */
	if (series != MTK_TV_SERIES_MERAK_2)
		return false;

	/*
	 * as request,
	 * op1/op2 pq irq posion in fbl is missing part,
	 * hardcode 0x400 until re-tune trigger gen formula
	 */
	if (tg_info->IsB2Rsource && tg_info->IsSCMIFBL)
		return true;

	return false;
}

static void trigger_gen_PQ_formula(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	bool IS_B2RSource = false;
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t rwbank_upd, ds_mload, vsync, rd_trig;
	uint16_t current_case = TGUC_A_0;
	uint16_t input_de_end;
	uint16_t input_vtt;
	uint32_t input_vbk;
	uint16_t input_vsync_st = 50;//todo ip1_04
	uint16_t input_vsyncwidth;
	uint16_t output_vtt = 0;
	uint16_t output_vde_start = 0;
	uint16_t Out_div_In_factorx100;
	uint32_t u32TempA, u32TempB;
	uint8_t IsVdeFallingmode = 0;  // 1: Vde falling , 2: Vsync rising
	uint8_t u8IsSelSeperate = 1;
	uint8_t u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeVsync = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeRtrig = 1;  // 1: Vde falling , 0: Vsync rising
	uint8_t u8IsVdeFallingmodeSwPqIrq = 1;  // 1: Vde falling , 0: Vsync rising
	uint32_t b2r_data_start = 0;
	uint16_t input_vde = 0;
	bool IS_Interlance = false;
	enum video_crtc_frame_sync_mode frame_sync_mode;

	IS_B2RSource = tg_info->IsB2Rsource;
	pctx_pnl = &pctx->panel_priv;
	output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	input_vde = tg_info->Vde;
	IS_Interlance = !(tg_info->IsPmodeEn);
	output_vde_start = pctx_pnl->info.de_vstart;
	//select case //common trigger select
	current_case = g_u16CaseSelect;
	input_vsync_st = tg_info->Vde_start;
	input_de_end = 2180;
	input_vsyncwidth = tg_info->Vs_pulse_width;
	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;

	input_vtt = tg_info->Vtotal - 2;

	if (IS_Interlance == true) {
		//input_vde = input_vde/2;
		input_vtt = input_vtt - 2;
	}

	input_vbk = input_vtt - input_vde - IPM_READ_DELAY;

	Out_div_In_factorx100 = output_vtt*100/input_vtt;
	b2r_data_start = (1 + trigger_gen_parameter[current_case].F1_D1_INPUT
		+ trigger_gen_parameter[current_case].F1_D2_INPUT
		+ trigger_gen_parameter[current_case].F1_D3_INPUT
				+ trigger_gen_parameter[current_case].F2_D5);

	//position calculat
	if (IS_B2RSource) {
		input_vbk = b2r_data_start;
		Out_div_In_factorx100 = 100;
		//IsVdeFallingmode = 1;
	}

	if (current_case == TGUC_E_0 ||
		current_case == TGUC_E_1 ||
		current_case == TGUC_F_0) {
		IsVdeFallingmode = 0;
		u8IsSelSeperate = 0;
		u8IsVdeFallingmode = 1;
		u8IsVdeFallingmodeRwbk = 1;
		u8IsVdeFallingmodeDsMl = 1;
		u8IsVdeFallingmodeVsync = 1;
		u8IsVdeFallingmodeRtrig = 1;
		u8IsVdeFallingmodeSwPqIrq = 1;

		// rwbank_upd
		u32TempA = output_vde_start * g_out_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D3 * g_out_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F1_D3
					+ trigger_gen_parameter[current_case].F1_D2
					+ trigger_gen_parameter[current_case].F1_D1)
					* g_4K_line2trig_httx100);
		trigger_gen_overflowprotect(u32TempA, u32TempB, &rwbank_upd);

		// ds_mload
		u32TempA = output_vde_start * g_out_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D3 * g_out_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F1_D3
					+ trigger_gen_parameter[current_case].F1_D2)
					* g_4K_line2trig_httx100);
		trigger_gen_overflowprotect(u32TempA, u32TempB, &ds_mload);

		// vsync
		u32TempA = output_vde_start * g_out_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D3 * g_out_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F1_D3)
					* g_4K_line2trig_httx100);
		trigger_gen_overflowprotect(u32TempA, u32TempB, &vsync);

		// rd_trig
		u32TempA = output_vde_start * g_out_htt2trig_httx100;
		u32TempB = (trigger_gen_parameter[current_case].F2_D3 * g_out_htt2trig_httx100);
		trigger_gen_overflowprotect(u32TempA, u32TempB, &rd_trig);
	} else if ((current_case == TGUC_B_0) ||
		(current_case == TGUC_D_0) || (current_case == TGUC_D_1)) {
		if (frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) {
			IsVdeFallingmode = 0; // 1: Vde falling , 0: Vsync rising
			u8IsSelSeperate = 1;
			u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRtrig = 0;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
		} else {
			IsVdeFallingmode = 1; // 1: Vde falling , 0: Vsync rising
			u8IsSelSeperate = 0;
			u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRtrig = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
		}

		if (IS_B2RSource) {
			rwbank_upd = (input_vbk * g_Input4K_line2trig_httx100
					- ((trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT
					+ trigger_gen_parameter[current_case].F1_D1_INPUT)
					* g_Input4K_line2trig_httx100)) / 100;

			ds_mload = (input_vbk * g_Input4K_line2trig_httx100
					- ((trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT)
					* g_Input4K_line2trig_httx100)) / 100;

			vsync = (input_vbk * g_Input4K_line2trig_httx100
					- ((trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT)
					* g_Input4K_line2trig_httx100)) / 100;
			rd_trig = (input_vbk * g_Input4K_line2trig_httx100
					- (trigger_gen_parameter[current_case].F2_D5
					* g_Input4K_line2trig_httx100)) / 100;
		} else {
			// rwbank_upd
			u32TempA = input_vbk * g_src_htt2trig_httx100;
			u32TempB = (trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT
					+ trigger_gen_parameter[current_case].F1_D1_INPUT)
						* g_Input4K_line2trig_httx100;
			trigger_gen_overflowprotect(u32TempA, u32TempB, &rwbank_upd);

			// ds_mload
			u32TempA = input_vbk * g_src_htt2trig_httx100;
			u32TempB = (trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT)
						* g_Input4K_line2trig_httx100;
			trigger_gen_overflowprotect(u32TempA, u32TempB, &ds_mload);

			// vsync
			u32TempA = input_vbk * g_src_htt2trig_httx100;
			u32TempB = (trigger_gen_parameter[current_case].F2_D5
					+ trigger_gen_parameter[current_case].F1_D3_INPUT)
						* g_Input4K_line2trig_httx100;
			trigger_gen_overflowprotect(u32TempA, u32TempB, &vsync);

			// rd_trig
			u32TempA = (u8IsVdeFallingmodeRtrig ? input_vbk :
				(input_vsync_st+input_vsyncwidth)) * g_src_htt2trig_httx100;
			u32TempB = (trigger_gen_parameter[current_case].F2_D5)
						* g_Input4K_line2trig_httx100;
			trigger_gen_overflowprotect(u32TempA, u32TempB, &rd_trig);
		}
	} else {
		if (frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) {
			IsVdeFallingmode = 0; // 1: Vde falling , 0: Vsync rising
			u8IsSelSeperate = 1;
			u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRtrig = 0;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
		} else {
			IsVdeFallingmode = 1; // 1: Vde falling , 0: Vsync rising
			u8IsSelSeperate = 0;
			u8IsVdeFallingmode = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRwbk = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeDsMl = 1;  // 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeVsync = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeRtrig = 1;	// 1: Vde falling , 0: Vsync rising
			u8IsVdeFallingmodeSwPqIrq = 1;	// 1: Vde falling , 0: Vsync rising
		}

		if (IS_B2RSource) {
			rwbank_upd = ((input_vbk * g_Input4K_line2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
						* g_Input4K_line2trig_httx100))
					- (trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT
					+ trigger_gen_parameter[current_case].F1_D1_INPUT)
					* g_Input4K_line2trig_httx100) / 100;

			ds_mload = ((input_vbk * g_Input4K_line2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
						* g_Input4K_line2trig_httx100))
					- (trigger_gen_parameter[current_case].F1_D3_INPUT
					+ trigger_gen_parameter[current_case].F1_D2_INPUT)
					* g_Input4K_line2trig_httx100) / 100;

			vsync = ((input_vbk * g_Input4K_line2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
						* g_Input4K_line2trig_httx100))
					- trigger_gen_parameter[current_case].F1_D3_INPUT
					* g_Input4K_line2trig_httx100) / 100;
			rd_trig = (input_vbk * g_Input4K_line2trig_httx100) / 100
				+ ((trigger_gen_parameter[current_case].F2_D1
					* g_Input4K_line2trig_httx100) / 100);
		} else {
			rwbank_upd = ((input_vbk*g_src_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
							* g_src_htt2trig_httx100))
					- (trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT
						+ trigger_gen_parameter[current_case].F1_D1_INPUT)
					* g_Input4K_line2trig_httx100) / 100;

			ds_mload = ((input_vbk*g_src_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
							* g_src_htt2trig_httx100))
					- (trigger_gen_parameter[current_case].F1_D3_INPUT
						+ trigger_gen_parameter[current_case].F1_D2_INPUT)
					* g_Input4K_line2trig_httx100) / 100;
			vsync = ((input_vbk*g_src_htt2trig_httx100)
					+ ((trigger_gen_parameter[current_case].F2_D1
							* g_src_htt2trig_httx100))
					- trigger_gen_parameter[current_case].F1_D3_INPUT
					* g_Input4K_line2trig_httx100) / 100;
			rd_trig = ((u8IsVdeFallingmodeRtrig ? input_vbk :
						(input_vsync_st + input_vsyncwidth))
					* g_src_htt2trig_httx100) / 100
					+ ((trigger_gen_parameter[current_case].F2_D1
					*g_src_htt2trig_httx100) / 100);
		}
	}

	//position update into structure op1/op2/frc1/frc_input
#if (!TRIGGER_GEN_DEBUG)
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].RWBk_ud_dly_v = rwbank_upd + s_u8delta_op1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].ds_dly_v = ds_mload + s_u8delta_op1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].ml_dly_v = ds_mload + s_u8delta_op1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].vs_dly_v = vsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].dma_r_dly_v = rd_trig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].trig_sel = IsVdeFallingmode;
	if (_need_pq_irq_patch(tg_info))
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].pq_irq_dly_v =
			PQ_COMMON_TRIGEN_DOMAIN_OP1_PQ_IRQ_DLY_V;
	else
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].pq_irq_dly_v = vsync +
			OP1_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/100;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].trig_sel_separate =
			u8IsSelSeperate;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].rwbank_trig_sel =
		u8IsVdeFallingmodeRwbk;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].ds_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].ml_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].vs_trig_sel =
		u8IsVdeFallingmodeVsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].dmar_trig_sel =
		u8IsVdeFallingmodeRtrig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].swirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].pqirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].str_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;

	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].RWBk_ud_dly_v = rwbank_upd + s_u8delta_op2;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].ds_dly_v = ds_mload + s_u8delta_op2;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].ml_dly_v = ds_mload + s_u8delta_op2;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].vs_dly_v = vsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].dma_r_dly_v = rd_trig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].trig_sel = IsVdeFallingmode;
	if (_need_pq_irq_patch(tg_info))
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].pq_irq_dly_v =
			PQ_COMMON_TRIGEN_DOMAIN_OP1_PQ_IRQ_DLY_V;
	else
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].pq_irq_dly_v = vsync +
			OP2_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/100;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].trig_sel_separate =
		u8IsSelSeperate;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].rwbank_trig_sel =
		u8IsVdeFallingmodeRwbk;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].ds_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].ml_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].vs_trig_sel =
		u8IsVdeFallingmodeVsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].dmar_trig_sel =
		u8IsVdeFallingmodeRtrig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].swirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].pqirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].str_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;

	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].RWBk_ud_dly_v = rwbank_upd;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].ds_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].ml_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].vs_dly_v = vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].dma_r_dly_v = rd_trig;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].trig_sel = IsVdeFallingmode;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].trig_sel_separate =
		u8IsSelSeperate;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].rwbank_trig_sel =
		u8IsVdeFallingmodeRwbk;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].ds_trig_sel =
		u8IsVdeFallingmodeDsMl;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].ml_trig_sel =
		u8IsVdeFallingmodeDsMl;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].vs_trig_sel =
		u8IsVdeFallingmodeVsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].dmar_trig_sel =
		u8IsVdeFallingmodeRtrig;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].swirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].pqirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].str_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;

	// FRC_INPUT and FRC_IPM sould be same stuff
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].RWBk_ud_dly_v = rwbank_upd;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].ds_dly_v = ds_mload;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].ml_dly_v = ds_mload;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].vs_dly_v = vsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].dma_r_dly_v = rd_trig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].trig_sel = IsVdeFallingmode;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].sw_irq_dly_v = rwbank_upd+1;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].trig_sel_separate =
		u8IsSelSeperate;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].rwbank_trig_sel =
		u8IsVdeFallingmodeRwbk;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].ds_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].ml_trig_sel =
		u8IsVdeFallingmodeDsMl;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].vs_trig_sel =
		u8IsVdeFallingmodeVsync;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].dmar_trig_sel =
		u8IsVdeFallingmodeRtrig;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].swirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].pqirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].str_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;

	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].RWBk_ud_dly_v = rwbank_upd;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].ds_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].ml_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].vs_dly_v = vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].dma_r_dly_v = rd_trig;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].trig_sel = IsVdeFallingmode;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].sw_irq_dly_v = rwbank_upd+1;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].trig_sel_separate =
		u8IsSelSeperate;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].rwbank_trig_sel =
		u8IsVdeFallingmodeRwbk;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].ds_trig_sel =
		u8IsVdeFallingmodeDsMl;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].ml_trig_sel =
		u8IsVdeFallingmodeDsMl;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].vs_trig_sel =
		u8IsVdeFallingmodeVsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].dmar_trig_sel =
		u8IsVdeFallingmodeRtrig;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].swirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].pqirq_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].str_trig_sel =
		u8IsVdeFallingmodeSwPqIrq;
#endif
	//debug message
	TG_DBG(DRM_INFO("<%s> current_case: %d\n", __func__, current_case));
	TG_DBG(DRM_INFO("input_vsync_st: %d\n", input_vsync_st));
	TG_DBG(DRM_INFO("input_vde: %d\n", input_vde));
	TG_DBG(DRM_INFO("input_vtt: %d\n", input_vtt));
	TG_DBG(DRM_INFO("input_vbk: %d\n", input_vbk));
	TG_DBG(DRM_INFO("input_vsyncwidth: %d\n", input_vsyncwidth));
	TG_DBG(DRM_INFO("output_vde_start: %d\n", output_vde_start));
	TG_DBG(DRM_INFO("Out_div_In_factorx100: %d\n", Out_div_In_factorx100));
	TG_DBG(DRM_INFO("IsVdeFallingmode: %d\n", IsVdeFallingmode));
	TG_DBG(DRM_INFO("rwbank_upd: %d\n", rwbank_upd));
	TG_DBG(DRM_INFO("ds_mload: %d\n", ds_mload));
	TG_DBG(DRM_INFO("vsync: %d\n", vsync));
	TG_DBG(DRM_INFO("rd_trig: %d\n", rd_trig));
}

static void trigger_gen_render_formula(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t rwbank_upd, ds_mload, vsync, rd_trig;
	uint16_t current_case = TGUC_A_0;
	uint16_t input_de_end;
	uint16_t input_vtt;
	uint16_t input_vbk;
	uint16_t input_vsync_st = 50;//todo ip1_04
	uint16_t input_vsyncwidth;
	uint16_t output_vtt = 0;
	uint16_t output_vde_start = 0;
	enum video_crtc_frame_sync_mode frame_sync_mode;
	uint16_t Out_div_In_factorx100;
	uint16_t input_vde = 0;
	uint8_t IsVdeFallingmode = 0;  // 1: Vde falling , 2: Vsync rising
	bool IS_Interlance = false;

	pctx_pnl = &pctx->panel_priv;
	output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	output_vde_start = pctx_pnl->info.de_vstart;
	input_vde = tg_info->Vde;
	IS_Interlance = !(tg_info->IsPmodeEn);
	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;
	current_case = g_u16CaseSelect;
	input_vsync_st = tg_info->Vde_start;
	input_de_end = 2180;
	input_vsyncwidth = tg_info->Vs_pulse_width;

	input_vtt = tg_info->Vtotal - 2;

	if (IS_Interlance) {
		//input_vde = input_vde/2;
		input_vtt = input_vtt - 2;
	}

	input_vbk = input_vtt - input_vde - IPM_READ_DELAY;
	Out_div_In_factorx100 = output_vtt * 100 / input_vtt;

	//position calculat

	rwbank_upd = (output_vde_start * g_out_htt2trig_httx100
					- trigger_gen_parameter[current_case].F2_D3
					* g_out_htt2trig_httx100
					- trigger_gen_parameter[current_case].F1_D3
					* g_4K_line2trig_httx100
					- trigger_gen_parameter[current_case].F1_D2
					* g_4K_line2trig_httx100
					- trigger_gen_parameter[current_case].F1_D1
					* g_4K_line2trig_httx100) / 100;
	ds_mload = (output_vde_start * g_out_htt2trig_httx100
				- trigger_gen_parameter[current_case].F2_D3
				* g_out_htt2trig_httx100
				- trigger_gen_parameter[current_case].F1_D3
				* g_4K_line2trig_httx100
				- trigger_gen_parameter[current_case].F1_D2
				* g_4K_line2trig_httx100) / 100;
	vsync = (output_vde_start*g_out_htt2trig_httx100
			- trigger_gen_parameter[current_case].F2_D3*g_out_htt2trig_httx100
			- trigger_gen_parameter[current_case].F1_D3*g_4K_line2trig_httx100)/100;
	rd_trig = (output_vde_start*g_out_htt2trig_httx100
				- trigger_gen_parameter[current_case].F2_D3
				* g_out_htt2trig_httx100) / 100;
	//position update into structure display/frc2

#if (!TRIGGER_GEN_DEBUG)
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].RWBk_ud_dly_v = rwbank_upd +
		s_u8delta_disp;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].ds_dly_v = ds_mload + s_u8delta_disp;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].ml_dly_v = ds_mload + s_u8delta_disp;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].vs_dly_v = vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].dma_r_dly_v = rd_trig;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_DISP].pq_irq_dly_v = vsync +
		DISP_PQ_IRQ_DELAY_LINE_4K*g_4K_line2trig_httx100/100;

	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].RWBk_ud_dly_v = rwbank_upd;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].ds_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].ml_dly_v = ds_mload;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].vs_dly_v = vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].dma_r_dly_v = rd_trig;
#endif

	//debug message
	TG_DBG(DRM_INFO("<%s> current_case: %d\n", __func__, current_case));
	TG_DBG(DRM_INFO("input_vsync_st: %d\n", input_vsync_st));
	TG_DBG(DRM_INFO("input_vde: %d\n", input_vde));
	TG_DBG(DRM_INFO("input_vtt: %d\n", input_vtt));
	TG_DBG(DRM_INFO("input_vbk: %d\n", input_vbk));
	TG_DBG(DRM_INFO("input_vsyncwidth: %d\n", input_vsyncwidth));
	TG_DBG(DRM_INFO("output_vde_start: %d\n", output_vde_start));
	TG_DBG(DRM_INFO("Out_div_In_factorx100: %d\n", Out_div_In_factorx100));
	TG_DBG(DRM_INFO("IsVdeFallingmode: %d\n", IsVdeFallingmode));
	TG_DBG(DRM_INFO("rwbank_upd: %d\n", rwbank_upd));
	TG_DBG(DRM_INFO("ds_mload: %d\n", ds_mload));
	TG_DBG(DRM_INFO("vsync: %d\n", vsync));
	TG_DBG(DRM_INFO("rd_trig: %d\n", rd_trig));
}

static void trigger_gen_frc_formula(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t stage_rwbank_upd, me_rwbank_upd, mi_vsync;
	uint16_t current_case = 0;
	uint16_t input_de_end;
	uint16_t input_vtt;
	uint16_t input_vbk;
	uint16_t input_vsync_st = 0;//todo ip1_04
	uint16_t input_vsyncwidth;
	uint16_t output_vtt = 0;
	uint16_t output_vde_start = 0;
	uint16_t output_vde_end = 0;
	uint16_t A3A3_05 = 0;
	uint16_t Out_div_In_factorx100;
	uint32_t vtt_var_diff = 0;

	int64_t In_out_de_phase_diff = 0;
	uint16_t src_lock = 0;
	uint16_t de_limit = 0;
	uint16_t de_limit1 = 0;
	uint16_t de_limit2 = 0;
	uint16_t input_vde = 0;
	uint8_t u8VttvIvs = 0;
	uint8_t u8VttvOvs = 0;
	uint16_t outputVttDiff = 0;
	uint32_t Input_VFreqx1000 = 0;

	bool IS_B2RSource = false;
	bool IS_Interlance = false;
	enum video_crtc_frame_sync_mode frame_sync_mode;
	mtktv_chip_series series;
	//select case //common trigger select

	pctx_pnl = &pctx->panel_priv;
	output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	output_vde_start = pctx_pnl->info.de_vstart;
	output_vde_end = pctx_pnl->info.de_vstart+pctx_pnl->info.de_height;
	input_vde = tg_info->Vde;
	IS_B2RSource = tg_info->IsB2Rsource;
	IS_Interlance = !(tg_info->IsPmodeEn);
	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;
	current_case = g_u16CaseSelect;
	input_vsync_st = tg_info->Vde_start;
	Input_VFreqx1000 = tg_info->Input_Vfreqx1000;
	input_de_end = 2180;
	input_vsyncwidth = tg_info->Vs_pulse_width;
	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	input_vtt = tg_info->Vtotal;

	if (IS_Interlance == true) {
		//input_vde = input_vde/2;
		if (input_vtt >= 2)
			input_vtt = input_vtt - 2;
	}

	input_vbk = input_vtt - input_vde;
	if (input_vtt == 0)
		input_vtt = 1;

	Out_div_In_factorx100 = output_vtt*100/input_vtt;

	u8VttvIvs = tg_info->u8Ivs;
	u8VttvOvs = tg_info->u8Ovs;
	if (u8VttvOvs == 0) {
		u8VttvOvs = 1;
	}
	//position calculat
	stage_rwbank_upd = (output_vde_start * g_out_htt2trig_httx100
						- trigger_gen_parameter[current_case].F2_D4
						* g_out_htt2trig_httx100
						- trigger_gen_parameter[current_case].F1_D5
						* g_out_htt2trig_httx100
						- trigger_gen_parameter[current_case].F1_D4
						* g_out_htt2trig_httx100) / 100;
	me_rwbank_upd = (output_vde_start*g_out_htt2trig_httx100
					- trigger_gen_parameter[current_case].F2_D4
					* g_out_htt2trig_httx100
					- trigger_gen_parameter[current_case].F1_D5
				* g_out_htt2trig_httx100) / 100;
	mi_vsync = (output_vde_start * g_out_htt2trig_httx100
				- trigger_gen_parameter[current_case].F2_D4
				* g_out_htt2trig_httx100) / 100;

	//position update into structure frc_stage/frc_me/frc_mi
#if (!TRIGGER_GEN_DEBUG)
	if ((pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0100) ||
			(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0300)) {
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE].vs_dly_v = me_rwbank_upd;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME].vs_dly_v = stage_rwbank_upd;
	} else {
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE].vs_dly_v =
			stage_rwbank_upd;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME].vs_dly_v = me_rwbank_upd;
	}

	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI].vs_dly_v = mi_vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI].sw_irq_dly_v = mi_vsync;
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI].dma_r_dly_v = mi_vsync + 1;
#endif

	if (current_case != TGUC_A_0 &&
		current_case != TGUC_C_0 &&
		current_case != TGUC_C_1) {
		vtt_var_diff = ((int32_t)trigger_gen_parameter[current_case].F1_D1
			* g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F1_D2 * g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F1_D3 * g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F2_D5 * g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F2_D1 * g_src_htt2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F2_D2 * 100
			- (int32_t)trigger_gen_parameter[current_case].F1_D3 * g_4k_line2out_httx100
			- (int32_t)trigger_gen_parameter[current_case].F1_D2 * g_4k_line2out_httx100
			- (int32_t)trigger_gen_parameter[current_case].F1_D1
			* g_4k_line2out_httx100) / 100;
	} else {
		vtt_var_diff = ((int32_t)trigger_gen_parameter[current_case].F1_D1
			* g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F1_D2 * g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F1_D3 * g_4k_line2out_httx100
			+ (int32_t)trigger_gen_parameter[current_case].F2_D2 * 100
			+ (int32_t)trigger_gen_parameter[current_case].F2_D3 * 100
			- (int32_t)trigger_gen_parameter[current_case].F2_D4 * 100
			- (int32_t)trigger_gen_parameter[current_case].F1_D5 * 100
			- (int32_t)trigger_gen_parameter[current_case].F1_D4 * 100) / 100;
	}

	if (vtt_var_diff < 3)
		vtt_var_diff = 3;

	//VRR source lock value
	if (IS_B2RSource ||
		((Input_VFreqx1000 % 50000 != 0) &&
			(u8VttvIvs == 1 && (u8VttvOvs == 1 || u8VttvOvs == 2)) &&
				(frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK)))
		In_out_de_phase_diff = 0; /* only b2r or vrr + in:out not 1:1 */
	else {
		In_out_de_phase_diff = ((int64_t)((int64_t)input_vde * g_src_htt2out_httx100 *
			u8VttvIvs) / 100 -
			(int64_t)((int64_t)(output_vde_end-output_vde_start + 1) *
			u8VttvOvs)) / u8VttvOvs;
		if (In_out_de_phase_diff < 0)
			In_out_de_phase_diff = 0;
	}

	//0.5F latency(1:2 VRR case), src_lock need additional phase diff 0.5F
	if ((u8VttvIvs == 1 && u8VttvOvs == 2) &&
	(frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK))
	{
		outputVttDiff = input_vtt / u8VttvOvs;
	} else {
		outputVttDiff = 0;
	}

	_s32FrameLockPhaseDiffValue = vtt_var_diff + In_out_de_phase_diff;

	de_limit = ((((input_vsync_st+input_vsyncwidth)*g_src_htt2trig_httx100))
				+ trigger_gen_parameter[current_case].F2_D1*g_src_htt2trig_httx100
				+ trigger_gen_parameter[current_case].F2_D2*g_out_htt2trig_httx100
				+ trigger_gen_parameter[current_case].F2_D3*g_out_htt2trig_httx100
				- output_vde_start*g_out_htt2trig_httx100
				- A3A3_05*g_out_htt2trig_httx100)/100;
	de_limit1 = trigger_gen_parameter[current_case].F1_D1*g_4K_line2trig_httx100
				+ trigger_gen_parameter[current_case].F1_D2*g_4K_line2trig_httx100
				+ trigger_gen_parameter[current_case].F1_D3*g_4K_line2trig_httx100
				+ trigger_gen_parameter[current_case].F2_D5*g_4K_line2trig_httx100
				+ trigger_gen_parameter[current_case].F2_D1*g_src_htt2trig_httx100
				+ trigger_gen_parameter[current_case].F2_D2*g_out_htt2trig_httx100
				- trigger_gen_parameter[current_case].F1_D3*g_4K_line2trig_httx100
				- trigger_gen_parameter[current_case].F1_D2*g_4K_line2trig_httx100
				- trigger_gen_parameter[current_case].F1_D1*g_4K_line2trig_httx100;

	de_limit2 = trigger_gen_parameter[current_case].F1_D1*g_4K_line2trig_httx100
			+ trigger_gen_parameter[current_case].F1_D2*g_4K_line2trig_httx100
			+ trigger_gen_parameter[current_case].F1_D3*g_4K_line2trig_httx100
			+ trigger_gen_parameter[current_case].F2_D5*g_4K_line2trig_httx100
			+ trigger_gen_parameter[current_case].F2_D1*g_src_htt2trig_httx100
			+ trigger_gen_parameter[current_case].F2_D2*g_out_htt2trig_httx100
			+ trigger_gen_parameter[current_case].F2_D3*g_out_htt2trig_httx100
			- trigger_gen_parameter[current_case].F2_D4*g_out_htt2trig_httx100
			- trigger_gen_parameter[current_case].F1_D5*g_out_htt2trig_httx100
			- trigger_gen_parameter[current_case].F1_D4*g_out_htt2trig_httx100;

	src_lock = outputVttDiff + ( de_limit +
		(In_out_de_phase_diff * g_out_htt2trig_httx100) / 100 + 1 ) *
							g_trig_htt2src_httx100 / 100;
	// de_limit + min (0, de_limit, de_limit1, de_limit2)

	_s32FrameLockSrcLockValue = src_lock;

	//debug message
	TG_DBG(DRM_INFO("<%s> current_case: %d\n", __func__, current_case));
	TG_DBG(DRM_INFO("input_vsync_st: %d\n", input_vsync_st));
	TG_DBG(DRM_INFO("input_vde: %d\n", input_vde));
	TG_DBG(DRM_INFO("input_vtt: %d\n", input_vtt));
	TG_DBG(DRM_INFO("input_vbk: %d\n", input_vbk));
	TG_DBG(DRM_INFO("input_vsyncwidth: %d\n", input_vsyncwidth));
	TG_DBG(DRM_INFO("output_vde_start: %d\n", output_vde_start));
	TG_DBG(DRM_INFO("Out_div_In_factorx100: %d\n", Out_div_In_factorx100));
	TG_DBG(DRM_INFO("stage_rwbank_upd: %d\n", stage_rwbank_upd));
	TG_DBG(DRM_INFO("me_rwbank_upd: %d\n", me_rwbank_upd));
	TG_DBG(DRM_INFO("mi_vsync: %d\n", mi_vsync));
	TG_DBG(DRM_INFO("vtt_var_diff: %d\n", vtt_var_diff));
	TG_DBG(DRM_INFO("de_limit: %d\n", de_limit));
	TG_DBG(DRM_INFO("de_limit1: %d\n", de_limit1));
	TG_DBG(DRM_INFO("de_limit2: %d\n", de_limit2));
	TG_DBG(DRM_INFO("src_lock: %d\n", src_lock));
	TG_DBG(DRM_INFO("In_out_de_phase_diff: %lld\n", In_out_de_phase_diff));
	TG_DBG(DRM_INFO("_s32FrameLockPhaseDiffValue: %d\n", _s32FrameLockPhaseDiffValue));
}


static void trigger_gen_srccap_set(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info, bool bRIU)
{
	bool bGamemode = false;
	uint8_t u8DomainIdx = 0;
	// TODO: shrink stack
	struct reg_srccap_common_triggen_input_src_sel paramIn_inputSrcSel;
	struct reg_srccap_common_triggen_common_trig_sel paramIn_commonTrigSel;
	struct reg_srccap_common_triggen_trig_sel_sep paramIn_trigSelSep;
	struct reg_srccap_common_triggen_vs_sw_trig paramIn_vsSwTrig;
	struct reg_srccap_common_triggen_sw_hcnt_ini_val paramIn_swHcntIniVal;
	struct reg_srccap_common_triggen_sw_htt_size paramIn_swHttSize;
	struct reg_srccap_common_triggen_sw_user_mode paramIn_swUserMode;
	struct reg_srccap_common_triggen_vcnt_keep_enable paramIn_vcntKeepEnable;
	struct reg_srccap_common_triggen_rwbank_dly paramIn_rwbankTrig;
	struct reg_srccap_common_triggen_ds_dly paramIn_dsTrig;
	struct reg_srccap_common_triggen_ml_dly paramIn_mlTrig;
	struct reg_srccap_common_triggen_vs_dly paramIn_vsyncTrig;
	struct reg_srccap_common_triggen_dma_r_dly paramIn_dmaRdTrig;
	struct reg_srccap_common_triggen_str_dly paramIn_strTrig;
	struct reg_srccap_common_triggen_sw_irq_dly paramIn_swIrqTrig;
	struct reg_srccap_common_triggen_pq_irq_dly paramIn_pqIrqTrig;
	struct reg_srccap_common_triggen_sw_vsync_len paramIn_SwVsyncLen;
	struct reg_srccap_common_triggen_info *SrccapTGSetting;

	bGamemode = tg_info->LegacyMode;

	for (u8DomainIdx = 0; u8DomainIdx < SRCCAP_COMMON_TRIGEN_DOMAIN_MAX
		; u8DomainIdx++) {
		if (bGamemode)
			SrccapTGSetting = srccap_trigger_gen;
		else
			SrccapTGSetting = srccap_trigger_gen_ts;
		paramIn_inputSrcSel.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_inputSrcSel.src = SrccapTGSetting[u8DomainIdx].src;
		paramIn_commonTrigSel.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_commonTrigSel.trig_sel =
			SrccapTGSetting[u8DomainIdx].trig_sel;
		paramIn_trigSelSep.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_trigSelSep.trig_sel_separate =
			SrccapTGSetting[u8DomainIdx].trig_sel_separate;
		paramIn_vsSwTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_vsSwTrig.sw_trig_mode =
			SrccapTGSetting[u8DomainIdx].sw_trig_mode;
		paramIn_swHcntIniVal.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_swHcntIniVal.sw_hcnt = SrccapTGSetting[u8DomainIdx].sw_hcnt;
		paramIn_swHttSize.domain = SrccapTGSetting[u8DomainIdx].domain;
		if (SrccapTGSetting[u8DomainIdx].sw_htt >= 1)
			paramIn_swHttSize.sw_htt = SrccapTGSetting[u8DomainIdx].sw_htt - 1;
		else
			paramIn_swHttSize.sw_htt = SrccapTGSetting[u8DomainIdx].sw_htt;
		paramIn_swUserMode.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_swUserMode.sw_user_mode_h =
			SrccapTGSetting[u8DomainIdx].sw_user_mode_h;
		paramIn_swUserMode.sw_user_mode_v =
			SrccapTGSetting[u8DomainIdx].sw_user_mode_v;
		paramIn_vcntKeepEnable.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_vcntKeepEnable.vcnt_keep_en =
			SrccapTGSetting[u8DomainIdx].vcnt_keep_en;
		paramIn_vcntKeepEnable.vcnt_upd_mask_range =
			SrccapTGSetting[u8DomainIdx].vcnt_upd_mask_range;
		paramIn_rwbankTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_rwbankTrig.rwbank_trig_sel =
			SrccapTGSetting[u8DomainIdx].rwbank_trig_sel;
		paramIn_rwbankTrig.RWBk_ud_dly_h =
			SrccapTGSetting[u8DomainIdx].RWBk_ud_dly_h;
		paramIn_rwbankTrig.RWBk_ud_dly_v =
			SrccapTGSetting[u8DomainIdx].RWBk_ud_dly_v;
		paramIn_dsTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_dsTrig.ds_dly_v = SrccapTGSetting[u8DomainIdx].ds_dly_v;
		paramIn_dsTrig.ds_trig_sel = SrccapTGSetting[u8DomainIdx].ds_trig_sel;
		paramIn_mlTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_mlTrig.ml_dly_v = SrccapTGSetting[u8DomainIdx].ml_dly_v;
		paramIn_mlTrig.ml_trig_sel = SrccapTGSetting[u8DomainIdx].ml_trig_sel;
		paramIn_vsyncTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_vsyncTrig.vs_dly_h = SrccapTGSetting[u8DomainIdx].vs_dly_h;
		paramIn_vsyncTrig.vs_dly_v = SrccapTGSetting[u8DomainIdx].vs_dly_v;
		paramIn_vsyncTrig.vs_trig_sel =
			SrccapTGSetting[u8DomainIdx].vs_trig_sel;
		paramIn_dmaRdTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_dmaRdTrig.dma_r_dly_h =
			SrccapTGSetting[u8DomainIdx].dma_r_dly_h;
		paramIn_dmaRdTrig.dma_r_dly_v =
			SrccapTGSetting[u8DomainIdx].dma_r_dly_v;
		paramIn_dmaRdTrig.dmar_trig_sel =
			SrccapTGSetting[u8DomainIdx].dmar_trig_sel;
		paramIn_strTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_strTrig.str_dly_v = SrccapTGSetting[u8DomainIdx].str_dly_v;
		paramIn_strTrig.str_trig_sel =
			SrccapTGSetting[u8DomainIdx].str_trig_sel;
		paramIn_swIrqTrig.domain =
			SrccapTGSetting[u8DomainIdx].domain;
		paramIn_swIrqTrig.sw_irq_dly_v =
			SrccapTGSetting[u8DomainIdx].sw_irq_dly_v;
		paramIn_swIrqTrig.swirq_trig_sel =
			SrccapTGSetting[u8DomainIdx].swirq_trig_sel;
		paramIn_pqIrqTrig.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_pqIrqTrig.pq_irq_dly_v =
			SrccapTGSetting[u8DomainIdx].pq_irq_dly_v;
		paramIn_pqIrqTrig.pqirq_trig_sel =
			SrccapTGSetting[u8DomainIdx].pqirq_trig_sel;
		paramIn_SwVsyncLen.domain = SrccapTGSetting[u8DomainIdx].domain;
		paramIn_SwVsyncLen.sw_vsync_len =
			SrccapTGSetting[u8DomainIdx].sw_vsync_len;

		drv_hwreg_srccap_common_triggen_set_inputSrcSel(
			paramIn_inputSrcSel, bRIU, NULL, NULL);
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_commonTrigSel(
			paramIn_commonTrigSel, bRIU, NULL, NULL); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_trigSelSep(
			paramIn_trigSelSep, bRIU, NULL, NULL); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);

		/* only miffy/mokona need to set this */
		if (series == MTK_TV_SERIES_MERAK_2) {
			drv_hwreg_srccap_common_triggen_set_vsSwTrig(
			paramIn_vsSwTrig, bRIU, NULL, NULL); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
		}
		drv_hwreg_srccap_common_triggen_set_swHcntIniVal(
			paramIn_swHcntIniVal, bRIU, NULL, NULL); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_swHttSize(
			paramIn_swHttSize, bRIU, NULL, NULL); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_swUserMode(
			paramIn_swUserMode, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_vcntKeepEnable(
			paramIn_vcntKeepEnable, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_rwbankTrig(
			paramIn_rwbankTrig, bRIU, NULL, NULL); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_dsTrig(
			paramIn_dsTrig, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_mlTrig(
			paramIn_mlTrig, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_vsyncTrig(
			paramIn_vsyncTrig, bRIU, NULL, NULL); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_dmaRdTrig(
			paramIn_dmaRdTrig, bRIU, NULL, NULL); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_strTrig(
			paramIn_strTrig, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_swIrqTrig(
			paramIn_swIrqTrig, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_pqIrqTrig(
			paramIn_pqIrqTrig, bRIU, NULL, NULL); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_srccap_common_triggen_set_swVsyncLen(
			paramIn_SwVsyncLen, bRIU, NULL, NULL);
	}
#if JKKK_TODO
	if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pInstance, reg, RegCount);
#endif
}

static void trigger_gen_pq_set(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info, bool bRIU)
{
	bool bIsFrcEnable = false;
	uint8_t u8DomainIdx = 0;
	bool bGamemode = false;
	bool IS_B2RSource = false;
	struct pq_common_triggen_info *pqTGSetting;
	struct reg_info *reg;
	struct hwregOut paramOut = { };
	int ret = 0;
	bool stub = 0;
	// TODO: shrink stack
	struct pq_common_triggen_input_src_sel paramIn_inputSrcSel = { };
	struct pq_common_triggen_common_trig_sel paramIn_commonTrigSel = { };
	struct pq_common_triggen_trig_sel_sep paramIn_trigSelSep = { };
	struct pq_common_triggen_vs_sw_trig paramIn_vsSwTrig = { };
	struct pq_common_triggen_sw_hcnt_ini_val paramIn_swHcntIniVal = { };
	struct pq_common_triggen_sw_htt_size paramIn_swHttSize = { };
	struct pq_common_triggen_sw_user_mode paramIn_swUserMode = { };
	struct pq_common_triggen_vcnt_keep_enable paramIn_vcntKeepEnable = { };
	struct pq_common_triggen_rwbank_dly paramIn_rwbankTrig = { };
	struct pq_common_triggen_ds_dly paramIn_dsTrig = { };
	struct pq_common_triggen_ml_dly paramIn_mlTrig = { };
	struct pq_common_triggen_vs_dly paramIn_vsyncTrig = { };
	struct pq_common_triggen_dma_r_dly paramIn_dmaRdTrig = { };
	struct pq_common_triggen_str_dly paramIn_strTrig = { };
	struct pq_common_triggen_sw_vsync_len paramIn_swVsyncLen = { };
	struct pq_common_triggen_sw_irq_dly paramIn_swIrqTrig = { };
	struct pq_common_triggen_pq_irq_dly paramIn_pqIrqTrig = { };

	reg = kcalloc(HWREG_VIDEO_REG_NUM_RENDER_COMMON_TRIGGEN_SET,
			sizeof(*reg), _get_alloc_method());
	if (!reg)
		return;
	paramOut.reg = reg;

	IS_B2RSource = tg_info->IsB2Rsource;

	ret = drv_hwreg_common_get_stub(&stub);

	//Triger gen needs to use RIU, to prevent racing with menuload
	bRIU = TRUE;
	bGamemode = tg_info->LegacyMode;

	if (series == MTK_TV_SERIES_MERAK_1) {
		bIsFrcEnable = true;
	} else if (series == MTK_TV_SERIES_MERAK_2) {
		bIsFrcEnable = !tg_info->IsFrcMdgwFBL;
	} else {
		DRM_ERROR("%s: invalid chip version\n", __func__);
		return;
	}

	if (IS_B2RSource)
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].src =
			(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_B2R0;
	else
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].src =
			(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_IP0;

	if (bIsFrcEnable) {
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].src =
			pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].src;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].src =
			pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].src;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].src =
			pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_IP].src;
	} else {
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP1].src =
			PQ_COMMON_TRIGEN_INPUT_TGEN;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_OP2].src =
			PQ_COMMON_TRIGEN_INPUT_TGEN;
		pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM].src =
			PQ_COMMON_TRIGEN_INPUT_TGEN;
	}

	for (u8DomainIdx = 0; u8DomainIdx < PQ_COMMON_TRIGEN_DOMAIN_MAX; u8DomainIdx++) {
		if (bGamemode)
			pqTGSetting = pq_trigger_gen;
		else
			pqTGSetting = pq_trigger_gen_ts;

		paramIn_inputSrcSel.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_inputSrcSel.src = pqTGSetting[u8DomainIdx].src;
		paramIn_commonTrigSel.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_commonTrigSel.trig_sel = pqTGSetting[u8DomainIdx].trig_sel;
		paramIn_trigSelSep.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_trigSelSep.trig_sel_separate =
			pqTGSetting[u8DomainIdx].trig_sel_separate;
		paramIn_vsSwTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_vsSwTrig.sw_trig_mode = pqTGSetting[u8DomainIdx].sw_trig_mode;
		paramIn_swHcntIniVal.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_swHcntIniVal.sw_hcnt = pqTGSetting[u8DomainIdx].sw_hcnt;
		paramIn_swHttSize.domain = pqTGSetting[u8DomainIdx].domain;
		if (pqTGSetting[u8DomainIdx].sw_htt >= 1)
			paramIn_swHttSize.sw_htt = pqTGSetting[u8DomainIdx].sw_htt-1;
		else
			paramIn_swHttSize.sw_htt = pqTGSetting[u8DomainIdx].sw_htt;

		paramIn_swUserMode.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_swUserMode.sw_user_mode_h =
			pqTGSetting[u8DomainIdx].sw_user_mode_h;
		paramIn_swUserMode.sw_user_mode_v =
			pqTGSetting[u8DomainIdx].sw_user_mode_v;
		paramIn_vcntKeepEnable.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_vcntKeepEnable.vcnt_keep_en =
			pqTGSetting[u8DomainIdx].vcnt_keep_en;
		paramIn_vcntKeepEnable.vcnt_upd_mask_range =
			pqTGSetting[u8DomainIdx].vcnt_upd_mask_range;
		paramIn_rwbankTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_rwbankTrig.rwbank_trig_sel =
			pqTGSetting[u8DomainIdx].rwbank_trig_sel;
		paramIn_rwbankTrig.RWBk_ud_dly_h =
			pqTGSetting[u8DomainIdx].RWBk_ud_dly_h;
		paramIn_rwbankTrig.RWBk_ud_dly_v =
			pqTGSetting[u8DomainIdx].RWBk_ud_dly_v;
		paramIn_dsTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_dsTrig.ds_dly_v = pqTGSetting[u8DomainIdx].ds_dly_v;
		paramIn_dsTrig.ds_trig_sel = pqTGSetting[u8DomainIdx].ds_trig_sel;
		paramIn_mlTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_mlTrig.ml_dly_v = pqTGSetting[u8DomainIdx].ml_dly_v;
		paramIn_mlTrig.ml_trig_sel = pqTGSetting[u8DomainIdx].ml_trig_sel;
		paramIn_vsyncTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_vsyncTrig.vs_dly_h = pqTGSetting[u8DomainIdx].vs_dly_h;
		paramIn_vsyncTrig.vs_dly_v = pqTGSetting[u8DomainIdx].vs_dly_v;
		paramIn_vsyncTrig.vs_trig_sel = pqTGSetting[u8DomainIdx].vs_trig_sel;
		paramIn_dmaRdTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_dmaRdTrig.dma_r_dly_h = pqTGSetting[u8DomainIdx].dma_r_dly_h;
		paramIn_dmaRdTrig.dma_r_dly_v = pqTGSetting[u8DomainIdx].dma_r_dly_v;
		paramIn_dmaRdTrig.dmar_trig_sel = pqTGSetting[u8DomainIdx].dmar_trig_sel;
		paramIn_strTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_strTrig.str_dly_v = pqTGSetting[u8DomainIdx].str_dly_v;
		paramIn_strTrig.str_trig_sel = pqTGSetting[u8DomainIdx].str_trig_sel;
		paramIn_swIrqTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_swIrqTrig.sw_irq_dly_v = pqTGSetting[u8DomainIdx].sw_irq_dly_v;
		paramIn_swIrqTrig.swirq_trig_sel =
			pqTGSetting[u8DomainIdx].swirq_trig_sel;
		paramIn_pqIrqTrig.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_pqIrqTrig.pq_irq_dly_v = pqTGSetting[u8DomainIdx].pq_irq_dly_v;
		paramIn_pqIrqTrig.pqirq_trig_sel =
			pqTGSetting[u8DomainIdx].pqirq_trig_sel;
		paramIn_swVsyncLen.domain = pqTGSetting[u8DomainIdx].domain;
		paramIn_swVsyncLen.sw_vsync_len = pqTGSetting[u8DomainIdx].sw_vsync_len;

		drv_hwreg_pq_common_triggen_set_inputSrcSel(
			paramIn_inputSrcSel, bRIU, &paramOut, stub); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_commonTrigSel(
			paramIn_commonTrigSel, bRIU, &paramOut); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_trigSelSep(
			paramIn_trigSelSep, bRIU, &paramOut); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);

		if (series == MTK_TV_SERIES_MERAK_2) {
			drv_hwreg_pq_common_triggen_set_vsSwTrig(
			paramIn_vsSwTrig, bRIU, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
		}
		drv_hwreg_pq_common_triggen_set_swHcntIniVal(
			paramIn_swHcntIniVal, bRIU, &paramOut); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_swHttSize(
			paramIn_swHttSize, bRIU, &paramOut); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);

		// XXX: sw user setting move to PQU

		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_vcntKeepEnable(
			paramIn_vcntKeepEnable, bRIU, &paramOut); // 1
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_rwbankTrig(
			paramIn_rwbankTrig, bRIU, &paramOut); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_dsTrig(
			paramIn_dsTrig, bRIU, &paramOut); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_mlTrig(
			paramIn_mlTrig, bRIU, &paramOut); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_vsyncTrig(
			paramIn_vsyncTrig, bRIU, &paramOut); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_dmaRdTrig(
			paramIn_dmaRdTrig, bRIU, &paramOut); // 3
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_strTrig(
			paramIn_strTrig, bRIU, &paramOut); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_swVsyncLen(
			paramIn_swVsyncLen, bRIU, &paramOut);
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_swIrqTrig(
			paramIn_swIrqTrig, bRIU, &paramOut); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
		drv_hwreg_pq_common_triggen_set_pqIrqTrig(
			paramIn_pqIrqTrig, bRIU, &paramOut); // 2
		//CHECK_MLOAD_CNT(RegCountLimit);
	}

	kfree(reg);
}

static void trigger_gen_render_set(struct mtk_tv_kms_context *pctx,
	struct drm_mtk_tv_trigger_gen_info *tg_info, bool bRIU)
{
	bool bIsFrcEnable = false;
	struct reg_info *reg;
	struct hwregOut paramOut = { };
	struct render_common_triggen_info *renderTGSetting = NULL;
	uint16_t u16InputVdeStart, u16InputVdeEnd;
	uint8_t u8DomainIdx = 0;
	bool bIsGameModeEnable = false;
	bool IS_B2RSource = false;
	struct mtk_panel_context *pctx_pnl = NULL;
	enum video_crtc_frame_sync_mode frame_sync_mode;

	struct hwregTrigGenInputSrcSelIn paramIn_inputSrcSel = { };
	struct hwregTrigGenCommonTrigSelIn paramIn_commonTrigSel = { };
	struct hwregTrigGenTrigSelSepIn paramIn_trigSelSep = { };
	struct hwregTrigGenVsSwTrigIn paramIn_vsSwTrig = { };
	struct hwregTrigGenSwHcntIniValIn paramIn_swHcntIniVal = { };
	struct hwregTrigGenSwHttSizeIn paramIn_swHttSize = { };
	struct hwregTrigGenSwUserModeIn paramIn_swUserMode = { };
	struct hwregTrigGenVcntKeepEnIn paramIn_vcntKeepEnable = { };
	struct hwregTrigGenRWbankDlyIn paramIn_rwbankTrig = { };
	struct hwregTrigGenDSDlyIn paramIn_dsTrig = { };
	struct hwregTrigGenMLDlyIn paramIn_mlTrig = { };
	struct hwregTrigGenVSDlyIn paramIn_vsyncTrig = { };
	struct hwregTrigGenDMARDlyIn paramIn_dmaRdTrig = { };
	struct hwregTrigGenSTRDlyIn paramIn_strTrig = { };
	struct hwregTrigGenSWIRQDlyIn paramIn_swIrqTrig = { };
	struct hwregTrigGenPQIRQDlyIn paramIn_pqIrqTrig = { };

	pctx_pnl = &pctx->panel_priv;

	reg = kcalloc(HWREG_VIDEO_REG_NUM_RENDER_COMMON_TRIGGEN_SET,
			sizeof(*reg), _get_alloc_method());
	if (!reg)
		return;
	paramOut.reg = reg;

	bIsGameModeEnable = tg_info->LegacyMode;
	IS_B2RSource = tg_info->IsB2Rsource;

	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;

	if (IS_B2RSource)
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_TGEN].src =
			RENDER_COMMON_TRIGEN_INPUT_B2R0;
	else
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_TGEN].src =
			RENDER_COMMON_TRIGEN_INPUT_IP0;

	if (frame_sync_mode == VIDEO_CRTC_FRAME_SYNC_FRAMELOCK || bIsGameModeEnable) {
		u16InputVdeStart = tg_info->Vde_start;
		u16InputVdeEnd = tg_info->Vde_start + 2160;
		drv_hwreg_render_common_triggen_set_trig_dly_V(_s32FrameLockSrcLockValue);
	} else {
		drv_hwreg_render_common_triggen_set_trig_dly_V(0);
	}

	if (series == MTK_TV_SERIES_MERAK_1) {
		bIsFrcEnable = true;
	} else if (series == MTK_TV_SERIES_MERAK_2) {
		bIsFrcEnable = !tg_info->IsFrcMdgwFBL;
	} else {
		DRM_ERROR("%s: invalid chip version\n", __func__);
		return;
	}

	if (bIsFrcEnable) {
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].src =
			render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_TGEN].src;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT].src =
			render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_TGEN].src;

		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
	} else {
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC1].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;

		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC2].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
		render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI].src =
			RENDER_COMMON_TRIGEN_INPUT_TGEN;
	}

	for (u8DomainIdx = 0; u8DomainIdx < RENDER_COMMON_TRIGEN_DOMAIN_MAX; u8DomainIdx++) {
		if (u8DomainIdx != RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT) {
			renderTGSetting = render_trigger_gen;

			paramIn_inputSrcSel.RIU = bRIU;
			paramIn_inputSrcSel.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_inputSrcSel.src = renderTGSetting[u8DomainIdx].src;
			paramIn_commonTrigSel.RIU = bRIU;
			paramIn_commonTrigSel.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_commonTrigSel.trig_sel = renderTGSetting[u8DomainIdx].trig_sel;
			paramIn_trigSelSep.RIU = bRIU;
			paramIn_trigSelSep.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_trigSelSep.trig_sel_separate =
				renderTGSetting[u8DomainIdx].trig_sel_separate;
			paramIn_vsSwTrig.RIU = bRIU;
			paramIn_vsSwTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_vsSwTrig.sw_trig_mode = renderTGSetting[u8DomainIdx].sw_trig_mode;
			paramIn_swHcntIniVal.RIU = bRIU;
			paramIn_swHcntIniVal.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_swHcntIniVal.sw_hcnt = renderTGSetting[u8DomainIdx].sw_hcnt;
			paramIn_swHttSize.RIU = bRIU;
			paramIn_swHttSize.domain = renderTGSetting[u8DomainIdx].domain;
			if (renderTGSetting[u8DomainIdx].sw_htt >= 1)
				paramIn_swHttSize.sw_htt = renderTGSetting[u8DomainIdx].sw_htt-1;
			else
				paramIn_swHttSize.sw_htt = renderTGSetting[u8DomainIdx].sw_htt;

			paramIn_swUserMode.RIU = bRIU;
			paramIn_swUserMode.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_swUserMode.sw_user_mode_h =
				renderTGSetting[u8DomainIdx].sw_user_mode_h;
			paramIn_swUserMode.sw_user_mode_v =
				renderTGSetting[u8DomainIdx].sw_user_mode_v;
			paramIn_vcntKeepEnable.RIU = bRIU;
			paramIn_vcntKeepEnable.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_vcntKeepEnable.vcnt_keep_en =
				renderTGSetting[u8DomainIdx].vcnt_keep_en;
			paramIn_vcntKeepEnable.vcnt_upd_mask_range =
				renderTGSetting[u8DomainIdx].vcnt_upd_mask_range;
			paramIn_rwbankTrig.RIU = bRIU;
			paramIn_rwbankTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_rwbankTrig.rwbank_trig_sel =
				renderTGSetting[u8DomainIdx].rwbank_trig_sel;
			paramIn_rwbankTrig.RWBk_ud_dly_h =
				renderTGSetting[u8DomainIdx].RWBk_ud_dly_h;
			paramIn_rwbankTrig.RWBk_ud_dly_v =
				renderTGSetting[u8DomainIdx].RWBk_ud_dly_v;
			paramIn_dsTrig.RIU = bRIU;
			paramIn_dsTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_dsTrig.ds_dly_v = renderTGSetting[u8DomainIdx].ds_dly_v;
			paramIn_dsTrig.ds_trig_sel = renderTGSetting[u8DomainIdx].ds_trig_sel;
			paramIn_mlTrig.RIU = bRIU;
			paramIn_mlTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_mlTrig.ml_dly_v = renderTGSetting[u8DomainIdx].ml_dly_v;
			paramIn_mlTrig.ml_trig_sel = renderTGSetting[u8DomainIdx].ml_trig_sel;
			paramIn_vsyncTrig.RIU = bRIU;
			paramIn_vsyncTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_vsyncTrig.vs_dly_h = renderTGSetting[u8DomainIdx].vs_dly_h;
			paramIn_vsyncTrig.vs_dly_v = renderTGSetting[u8DomainIdx].vs_dly_v;
			paramIn_vsyncTrig.vs_trig_sel = renderTGSetting[u8DomainIdx].vs_trig_sel;
			paramIn_dmaRdTrig.RIU = bRIU;
			paramIn_dmaRdTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_dmaRdTrig.dma_r_dly_h = renderTGSetting[u8DomainIdx].dma_r_dly_h;
			paramIn_dmaRdTrig.dma_r_dly_v = renderTGSetting[u8DomainIdx].dma_r_dly_v;
			paramIn_dmaRdTrig.dmar_trig_sel =
				renderTGSetting[u8DomainIdx].dmar_trig_sel;
			paramIn_strTrig.RIU = bRIU;
			paramIn_strTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_strTrig.str_dly_v = renderTGSetting[u8DomainIdx].str_dly_v;
			paramIn_strTrig.str_trig_sel = renderTGSetting[u8DomainIdx].str_trig_sel;
			paramIn_swIrqTrig.RIU = bRIU;
			paramIn_swIrqTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_swIrqTrig.sw_irq_dly_v = renderTGSetting[u8DomainIdx].sw_irq_dly_v;
			paramIn_swIrqTrig.swirq_trig_sel =
				renderTGSetting[u8DomainIdx].swirq_trig_sel;
			paramIn_pqIrqTrig.RIU = bRIU;
			paramIn_pqIrqTrig.domain = renderTGSetting[u8DomainIdx].domain;
			paramIn_pqIrqTrig.pq_irq_dly_v = renderTGSetting[u8DomainIdx].pq_irq_dly_v;
			paramIn_pqIrqTrig.pqirq_trig_sel =
				renderTGSetting[u8DomainIdx].pqirq_trig_sel;

			drv_hwreg_render_common_triggen_set_inputSrcSel(
				paramIn_inputSrcSel, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_commonTrigSel(
				paramIn_commonTrigSel, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_trigSelSep(
				paramIn_trigSelSep, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_swHcntIniVal(
				paramIn_swHcntIniVal, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_swHttSize(
				paramIn_swHttSize, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_swUserMode(
				paramIn_swUserMode, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_vcntKeepEnable(
				paramIn_vcntKeepEnable, &paramOut); // 1
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_rwbankTrig(
				paramIn_rwbankTrig, &paramOut); // 3
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_dsTrig(
				paramIn_dsTrig, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_mlTrig(
				paramIn_mlTrig, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_vsyncTrig(
				paramIn_vsyncTrig, &paramOut); // 3
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_dmaRdTrig(
				paramIn_dmaRdTrig, &paramOut); // 3
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_strTrig(
				paramIn_strTrig, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_swIrqTrig(
				paramIn_swIrqTrig, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_pqIrqTrig(
				paramIn_pqIrqTrig, &paramOut); // 2
			//CHECK_MLOAD_CNT(RegCountLimit);
			drv_hwreg_render_common_triggen_set_vsSwTrig(
				paramIn_vsSwTrig, &paramOut);	// 1
			//CHECK_MLOAD_CNT(RegCountLimit);
		}
	}

	kfree(reg);
}

static void trigger_gen_param_input_update(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
#define F1_D2_FACTORx100	60
#define F1_D3_FACTORx100	18
#define VFREQ_60		61000
#define VFREQ_120		119000
#define VFREQ_144		145000
#define F1D2_LOW		43
#define F1D2_LOW_GAME		21
#define F1D2_MID_1P		40
#define F1D2_MID_2P		53
#define F1D2_HIGH		53
#define F1D3_LOW		10
#define F1D3_LOW_GAME		10
#define F1D3_MID_1P		20
#define F1D3_MID_2P		10
#define F1D3_HIGH		20
	int i;
	uint16_t input_vde;
	uint32_t Input_VFreqx1000;
	int OutputVtt;
	int output_vde_start;
	struct mtk_panel_context *pctx_pnl;
	uint16_t F1_D2, F1_D3;
	bool bIsGameModeEnable = false;

	pctx_pnl = &pctx->panel_priv;
	output_vde_start = pctx_pnl->info.de_vstart;
	OutputVtt = drv_hwreg_render_pnl_get_Vcnt();
	input_vde = tg_info->Vde;
	Input_VFreqx1000 = tg_info->Input_Vfreqx1000;
	// miffy / mokona specific
	bIsGameModeEnable = tg_info->IsLowLatency;

	if (!trigger_gen_parameter) {
		DRM_ERROR("no trigger_gen_parameter available!\n");
		return;
	}

	/* only support merak2.0 */
	if (series != MTK_TV_SERIES_MERAK_2)
		return;

	DRM_INFO("output_vde_start: %d\n", output_vde_start);
//	F1_D2 = output_vde_start * F1_D2_FACTORx100 / DEC_100;
//	F1_D3 = output_vde_start * F1_D3_FACTORx100 / DEC_100;

	// TODO: vrr case

	if (Input_VFreqx1000 < VFREQ_60) {
		if (!bIsGameModeEnable) {
			F1_D2 = F1D2_LOW;	// normal PQ
			F1_D3 = F1D3_LOW;
		} else {
			F1_D2 = F1D2_LOW_GAME;	// Game PQ
			F1_D3 = F1D3_LOW_GAME;
		}
	} else if (Input_VFreqx1000 > VFREQ_60 && Input_VFreqx1000 < VFREQ_144) {
		if (tg_info->ip_path || tg_info->op_path) {
			/* 2P */
			F1_D2 = F1D2_MID_2P;
			F1_D3 = F1D3_MID_2P;
		} else {
			/* 1P */
			F1_D2 = F1D2_MID_1P;
			F1_D3 = F1D3_MID_1P;
		}
	} else {
		F1_D2 = F1D2_HIGH; //Game PQ
		F1_D3 = F1D3_HIGH;
	}

	// write back
	for (i = 0; i < TGUC_MAX; ++i) {
		trigger_gen_parameter[i].F1_D2_INPUT = F1_D2;
		trigger_gen_parameter[i].F1_D3_INPUT = F1_D3;
	}
}

static void trigger_gen_line_rate_cal(struct mtk_tv_kms_context *pctx,
			struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	bool IS_B2RSource = false;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint16_t input_vtt = 0;
	uint64_t InputFrameClkCount1 = 0;
	uint64_t InputFrameClkCount2 = 0;
	uint16_t output_vtt = 0;
	uint64_t trigger_gen_vttx100 = 0;
	uint64_t Output_VFreqx100 = 0;//100;
	uint64_t Input_VFreqx1000 = 0;
	uint64_t IFPS_DIV_OFPSx10000 = 0;
	uint64_t OutputFrameClkCount = 0;
	uint64_t OutputVtt = 0;
	uint64_t u64_10x6 = 0;
	uint64_t u64_10x4 = 0;
	uint64_t u64_10x2 = 0;
	uint64_t InputOneLinePeriodUsx10000 = 0;
	uint64_t OutputOneLinePeriodUsx10000 = 0;
	uint64_t TGOneLinePeriodUsx10000 = 0;
	bool bGamemode = false;
	enum video_crtc_frame_sync_mode frame_sync_mode;
	bool bIsPmodeEn = 0;

	IS_B2RSource = tg_info->IsB2Rsource;
	pctx_pnl = &pctx->panel_priv;
	crtc_props = pctx->ext_crtc_properties;
	InputFrameClkCount1 = tg_info->fdet_vtt0;
	InputFrameClkCount2 = tg_info->fdet_vtt1;
	bIsPmodeEn = tg_info->IsPmodeEn;

	frame_sync_mode = pctx_pnl->outputTiming_info.eFrameSyncMode;
	output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	trigger_gen_vttx100 =
		((uint64_t)output_vtt*XC_4K2K_HTOTAL*TGEN_XTAL_24M*DEC_100)
		/DEC_600/TRIGGER_GEN_SW_HCOUNT;
	Output_VFreqx100 = pctx_pnl->outputTiming_info.u32OutputVfreq;//100;
	Input_VFreqx1000 = tg_info->Input_Vfreqx1000;
	OutputFrameClkCount = drv_hwreg_render_pnl_get_xtail_cnt();
	OutputVtt = drv_hwreg_render_pnl_get_Vcnt();
	u64_10x6 = DEC_1000000;
	u64_10x4 = DEC_10000;
	u64_10x2 = DEC_100;
	bGamemode = tg_info->LegacyMode;

	// TODO:
	// temp solution for prevent 100/120hz input :
	//	4K2K_100/120hz output goes to 1:2
	// temp solution for prevent 200/240hz input :
	//	4K1K_200/240hz output goes to 1:2
	// ovs should be double

	input_vtt = tg_info->Vtotal;

	// Input_VFreqx100 = input_vfreq*100;
	if (Output_VFreqx100 != 0)
		IFPS_DIV_OFPSx10000 = Input_VFreqx1000*10000/Output_VFreqx100;

	if (OutputVtt == 0)
		OutputVtt = 1; //to avoid minus 0

	if (input_vtt == 0)
		input_vtt = 1; //to avoid minus 0

	if (Input_VFreqx1000 == 0)
		Input_VFreqx1000 = 1; //to avoid minus 0

	g_OuputFreqDivInputFreqx100 = Output_VFreqx100*100/Input_VFreqx1000;

	if (pctx_pnl->outputTiming_info.eFrameSyncMode ==
		VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) {
		InputOneLinePeriodUsx10000 = u64_10x4*u64_10x6*1000/Input_VFreqx1000/input_vtt;
	} else {
		if (bIsPmodeEn)
			InputOneLinePeriodUsx10000 =
				InputFrameClkCount1*10000/input_vtt/IP_XTAL_12M;
		else
			InputOneLinePeriodUsx10000 =
			(InputFrameClkCount1+InputFrameClkCount2)*10000/2/input_vtt/IP_XTAL_12M;
	}

	OutputOneLinePeriodUsx10000 =
		OutputFrameClkCount*10000/TGEN_XTAL_24M/OutputVtt;
	TGOneLinePeriodUsx10000 =
		TRIGGER_GEN_SW_HCOUNT*10000/TGEN_XTAL_24M;

	if (IS_B2RSource) {
		g_src_htt2trig_httx100 =
			OutputOneLinePeriodUsx10000*100/TGOneLinePeriodUsx10000;
		g_src_htt2out_httx100 = 100;
		// consider ifps & ofps
		// g_src_htt2trig_httx100 = g_src_htt2trig_httx100*10000/IFPS_DIV_OFPSx10000;
		// g_src_htt2out_httx100 = g_src_htt2out_httx100*10000/IFPS_DIV_OFPSx10000;
	} else {
		g_src_htt2trig_httx100 = InputOneLinePeriodUsx10000*100/TGOneLinePeriodUsx10000;
		g_src_htt2out_httx100 = InputOneLinePeriodUsx10000*100/OutputOneLinePeriodUsx10000;
	}

	g_out_htt2trig_httx100 = OutputOneLinePeriodUsx10000*100/TGOneLinePeriodUsx10000;
	g_4K_line2trig_httx100 = OutputOneLinePeriodUsx10000*100/TGOneLinePeriodUsx10000;
	if (Input_VFreqx1000 < 61000) {
		g_Input4K_line2trig_httx100 = INPUT_4K2K60_ONE_LINE_TIME_USX10000
			* 100 / TGOneLinePeriodUsx10000;
	} else if ((Input_VFreqx1000 > 61000) && (Input_VFreqx1000 < 121000)) {
		g_Input4K_line2trig_httx100 = INPUT_4K2K120_ONE_LINE_TIME_USX10000
			* 100 / TGOneLinePeriodUsx10000;
	} else {
		g_Input4K_line2trig_httx100 = INPUT_4K2K240_ONE_LINE_TIME_USX10000
			* 100 / TGOneLinePeriodUsx10000;
	}

	g_trig_htt2out_httx100 = TGOneLinePeriodUsx10000*100/OutputOneLinePeriodUsx10000;
	g_4k_line2out_httx100 = 100;//g_4K_line2trig_httx100 * g_trig_htt2out_httx100 /100;
  
	g_out_htt2src_httx100 = OutputOneLinePeriodUsx10000*100/InputOneLinePeriodUsx10000 ;
	g_trig_htt2src_httx100 = TGOneLinePeriodUsx10000*100/InputOneLinePeriodUsx10000;

	if (bGamemode == true) {
		s_u8delta_ip1 = IP1_DELTA;
		s_u8delta_op1 = OP1_DELTA;
		s_u8delta_op2 = OP2_DELTA;
		s_u8delta_disp = DISP_DELTA;
	} else {
		s_u8delta_ip1 = 0;
		s_u8delta_op1 = 0;
		s_u8delta_op2 = 0;
		s_u8delta_disp = 0;
	}

	TG_DBG(DRM_INFO("advance line rate\n"));
	TG_DBG(DRM_INFO("Input_VFreqx1000: %lld\n", Input_VFreqx1000));
	TG_DBG(DRM_INFO("Output_VFreqx100: %lld\n", Output_VFreqx100));
	TG_DBG(DRM_INFO("IFPS_DIV_OFPSx10000: %lld\n", IFPS_DIV_OFPSx10000));
	TG_DBG(DRM_INFO("OutputFrameClkCount: %lld\n", OutputFrameClkCount));
	TG_DBG(DRM_INFO("OutputVtt: %lld\n", OutputVtt));
	TG_DBG(DRM_INFO("InputFrameClkCount1: %lld\n", InputFrameClkCount1));
	TG_DBG(DRM_INFO("InputFrameClkCount2: %lld\n", InputFrameClkCount2));
	TG_DBG(DRM_INFO("InputOneLinePeriodUsx10000: %lld\n", InputOneLinePeriodUsx10000));
	TG_DBG(DRM_INFO("OutputOneLinePeriodUsx10000: %lld\n", OutputOneLinePeriodUsx10000));
	TG_DBG(DRM_INFO("TGOneLinePeriodUsx10000: %lld\n", TGOneLinePeriodUsx10000));
	TG_DBG(DRM_INFO("g_src_htt2trig_httx100: %d\n", g_src_htt2trig_httx100));
	TG_DBG(DRM_INFO("g_out_htt2trig_httx100: %d\n", g_out_htt2trig_httx100));
	TG_DBG(DRM_INFO("g_4K_line2trig_httx100: %d\n", g_4K_line2trig_httx100));
	TG_DBG(DRM_INFO("g_src_htt2out_httx100: %d\n", g_src_htt2out_httx100));
	TG_DBG(DRM_INFO("g_trig_htt2out_httx100: %d\n", g_trig_htt2out_httx100));
	TG_DBG(DRM_INFO("g_4k_line2out_httx100: %d\n", g_4k_line2out_httx100));
	TG_DBG(DRM_INFO("g_Input4K_line2trig_httx100: %d\n", g_Input4K_line2trig_httx100));
	TG_DBG(DRM_INFO("g_OuputFreqDivInputFreqx100: %d\n", g_OuputFreqDivInputFreqx100));
	TG_DBG(DRM_INFO("frame_sync_mode: %d\n", frame_sync_mode));
}

static inline int _sanity_check(struct mtk_tv_kms_context *pctx)
{
	struct mtk_panel_context *pctx_pnl;

	if ((!pctx) || (!(pctx->dev)) || (!(pctx->drm_dev))
		|| (!(pctx->ext_common_plane_properties))
		|| (!(pctx->ext_crtc_properties))
		|| (!(pctx->ext_crtc_graphic_properties))
		|| (!(pctx->ext_common_crtc_properties))
		|| (!(pctx->ext_connector_properties))
		|| (!(pctx->ext_common_connector_properties)))
		return -ENODEV;


	pctx_pnl = &pctx->panel_priv;
	if (!pctx_pnl) {
		DRM_ERROR("%s(%d) pctx_pnl=MULL\n", __func__, __LINE__);
		return -ENODEV;
	}

	return 0;
}

static inline void trigger_gen_record(struct mtk_tv_kms_context *pctx,
		struct drm_mtk_tv_trigger_gen_info *tg_info)
{
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;	// already checked

	// copy info data
	memcpy(&tg_record.input_info, tg_info, sizeof(tg_record.input_info));

	tg_record.out_vtt = drv_hwreg_render_pnl_get_Vcnt();
	tg_record.out_de_start = pctx_pnl->info.de_vstart;
	tg_record.out_de_end = pctx_pnl->info.de_vstart+pctx_pnl->info.de_height;
	tg_record.out_xtail_cnt = drv_hwreg_render_pnl_get_xtail_cnt();
	tg_record.out_vfreq = pctx_pnl->outputTiming_info.u32OutputVfreq;
	tg_record.F1_D2_INPUT = trigger_gen_parameter[0].F1_D2_INPUT;
	tg_record.F1_D3_INPUT = trigger_gen_parameter[0].F1_D3_INPUT;
	tg_record.debug_setting = pctx_pnl->gu8Triggergendebugsetting;
}

static inline void trigger_gen_get_param(struct mtk_tv_kms_context *pctx)
{
	struct mtk_panel_context *pctx_pnl;

	if (!pctx)
		return;

	pctx_pnl = &pctx->panel_priv;

	trigger_gen_parameter = drv_hwreg_render_common_triggen_get_param(
		pctx_pnl->info.de_height,
		pctx_pnl->info.de_width,
		pctx_pnl->outputTiming_info.u32OutputVfreq);
}

uint64_t mtk_video_panel_get_FrameLockPhaseDiff(void)
{
	return (uint64_t)_s32FrameLockPhaseDiffValue;
}

void mtk_trigger_gen_set_bypass(bool bypass)
{
	if (bypass)
		atomic_set(&trig_setup_bypass, 1);
	else
		atomic_set(&trig_setup_bypass, 0);
}

int mtk_trigger_gen_setup(
	struct mtk_tv_kms_context *pctx, bool bRIU, bool render_modify)
{
	int ret;
	unsigned long flags;
	struct drm_mtk_tv_trigger_gen_info tg_info = { };
	bool bypass = false;

	atomic_set(&g_bIstgwork, 1);
	TG_DBG(DRM_INFO("TG START\n"));
	ret = _sanity_check(pctx);
	if (ret)
		return ret;

	// update series
	series = get_mtk_chip_series(pctx->panel_priv.hw_info.pnl_lib_version);

	spin_lock_irqsave(&pctx->tg_lock, flags);
	memcpy(&tg_info, &pctx->trigger_gen_info, sizeof(tg_info));
	tg_record.render_modify = render_modify;
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	/*
	 * In vrr on -> off, keeping trigger gen vrr setting
	 * if in game mode (merak 2.0)
	 */
	if (atomic_read(&trig_setup_bypass) && tg_info.IsLowLatency &&
		series == MTK_TV_SERIES_MERAK_2) {
		bypass = true;
		DRM_INFO("%s(%d): bypass trigger gen setting\n",
			__func__, __LINE__);
	}

	// TG force to use RIU
	bRIU = true;

	trigger_gen_get_param(pctx);

	trigger_gen_param_input_update(pctx, &tg_info);

	// stored the tg info for sysfs
	trigger_gen_record(pctx, &tg_info);

	ret = trigger_gen_case_select(pctx, &tg_info, &g_u16CaseSelect);
	if (ret)
		return ret;

	trigger_gen_line_rate_cal(pctx, &tg_info);

	trigger_gen_srccap_formula(pctx, &tg_info);

	trigger_gen_PQ_formula(pctx, &tg_info);

	trigger_gen_render_formula(pctx, &tg_info);

	trigger_gen_frc_formula(pctx, &tg_info);

	if (!bypass) {
		if (render_modify)
			trigger_gen_render_set(pctx, &tg_info, bRIU);
		trigger_gen_srccap_set(pctx, &tg_info, bRIU);
		trigger_gen_pq_set(pctx, &tg_info, bRIU);
	}

out:
	atomic_set(&g_bIstgwork, 0);
	TG_DBG(DRM_INFO("TG END\n"));
	return 0;
}

int mtk_trigger_gen_show_param(char *buf)
{
#define NR_SRC_SEL	5
#define NR_TRIG_SEL	2
	int i, at = 0;
	enum pq_common_triggen_domain j;
	unsigned int src;
	bool trig;
	struct drm_mtk_tv_trigger_gen_info *t = &tg_record.input_info;
	const char *scenario_case[TGUC_MAX] = {
		"A-0", "A-2", "B-0", "C-0",
		"C-1", "C-2", "D-0", "D-1",
		"E-0", "E-1", "F-0", "Special",
	};
	const char *src_sel[NR_SRC_SEL] = {
		"ip0", "ip1",
		"b2r-global TGEN",
		"b2r-interal TGEN",
		"SC TGEN",
	};
	const char *trig_sel[NR_TRIG_SEL] = {
		"vsync raising",
		"vde falling",
	};
	const char *src_domain_name[SRCCAP_COMMON_TRIGEN_DOMAIN_MAX] = {
		"SRC-0", "SRC-1",
	};
	const char *pq_domain_name[PQ_COMMON_TRIGEN_DOMAIN_MAX] = {
		"B2R", "IP", "OP1", "OP2",
		"B2R-lite0", "B2R-lite1",
		"FRC-IPM",
	};
	const char *render_domain_name[RENDER_COMMON_TRIGEN_DOMAIN_MAX] = {
		"DISP", "FRC-1", "FRC-2", "TGEN",
		"FRC-input", "FRC-stage", "FRC-me",
		"FRC-mi",
	};

	at += sysfs_emit_at(buf, at, "*** scenario case: %s ***\n",
			scenario_case[g_u16CaseSelect]);
	at += sysfs_emit_at(buf, at, "debug mode: %d\n", (int)tg_record.debug_setting);
	// Input Param
	at += sysfs_emit_at(buf, at, "<Input info>\n");
	at += sysfs_emit_at(buf, at, "\tvsync start: %d\n", (int)t->Vde_start);
	at += sysfs_emit_at(buf, at, "\tvde: %d\n", (int)t->Vde);
	at += sysfs_emit_at(buf, at, "\tvtt: %d\n", (int)t->Vtotal);
	at += sysfs_emit_at(buf, at, "\tvsync width: %d\n", (int)t->Vs_pulse_width);
	at += sysfs_emit_at(buf, at, "\tvfreq*1000: %d\n", (int)t->Input_Vfreqx1000);
	at += sysfs_emit_at(buf, at, "\tframe clk 0: %d\n", (int)t->fdet_vtt0);
	at += sysfs_emit_at(buf, at, "\tframe clk 1: %d\n", (int)t->fdet_vtt1);
	at += sysfs_emit_at(buf, at, "\tSCMI in %s\n",
				t->IsSCMIFBL ? "FBL" : "FB");
	at += sysfs_emit_at(buf, at, "\tFrcMdgw in %s\n",
				t->IsFrcMdgwFBL ? "FBL" : "FB");
	at += sysfs_emit_at(buf, at, "\tForce P %s\n",
				t->IsForceP ? "Y" : "N");
	at += sysfs_emit_at(buf, at, "\t(ip/op)-2p Path %d/%d\n",
				t->ip_path, t->op_path);
	at += sysfs_emit_at(buf, at, "\tinput %s (%s-mode in %s-mode) in %s\n",
				t->IsB2Rsource ? "b2r" : "srccap",
				t->IsPmodeEn ? "P" : "I",
				t->LegacyMode ? "Legacy" : "TS",
				t->IsLowLatency ? "Low Latency" : "Normal");
	at += sysfs_emit_at(buf, at, "\tGamingMJC %s\n",
				t->IsGamingMJCOn ? "Y" : "N");
	at += sysfs_emit_at(buf, at, "\tVRR %d, VRR bypass setup: %d\n",
				(int)t->IsVrr, (int)atomic_read(&trig_setup_bypass));
	at += sysfs_emit_at(buf, at, "\trender domain setup: %d\n",
				(int)tg_record.render_modify);

	// Output Param
	at += sysfs_emit_at(buf, at, "<Output info>\n");
	at += sysfs_emit_at(buf, at, "\tvtt: %d\n", tg_record.out_vtt);
	at += sysfs_emit_at(buf, at, "\tde start: %d\n", tg_record.out_de_start);
	at += sysfs_emit_at(buf, at, "\tde end: %d\n", tg_record.out_de_end);
	at += sysfs_emit_at(buf, at, "\tvfrq*100: %d\n", tg_record.out_vfreq);
	at += sysfs_emit_at(buf, at, "\tframe clk cnt: %d\n", tg_record.out_xtail_cnt);

	// update input
	at += sysfs_emit_at(buf, at, "<Input param update>\n");
	at += sysfs_emit_at(buf, at, "\tf1_d2_in: %d\n", (int)tg_record.F1_D2_INPUT);
	at += sysfs_emit_at(buf, at, "\tf1_d3_in: %d\n", (int)tg_record.F1_D3_INPUT);

	// Htt
	at += sysfs_emit_at(buf, at, "<Htt * 100>\n");
	at += sysfs_emit_at(buf, at, "\tsrc htt2trig: %d\n", (int)g_src_htt2trig_httx100);
	at += sysfs_emit_at(buf, at, "\tout htt2trig: %d\n", (int)g_out_htt2trig_httx100);
	at += sysfs_emit_at(buf, at, "\t4K line2trig: %d\n", (int)g_4K_line2trig_httx100);
	at += sysfs_emit_at(buf, at, "\tin4K line2trig: %d\n", (int)g_Input4K_line2trig_httx100);
	at += sysfs_emit_at(buf, at, "\tsrc htt2out: %d\n", (int)g_src_htt2out_httx100);
	at += sysfs_emit_at(buf, at, "\ttrig htt2out: %d\n", (int)g_trig_htt2out_httx100);
	at += sysfs_emit_at(buf, at, "\t4k line2out: %d\n", (int)g_4k_line2out_httx100);
	at += sysfs_emit_at(buf, at, "\tb2r/ip vsync diff %d\n", (int)g_src_htt2trig_httx100 * 2);

	// framelock
	at += sysfs_emit_at(buf, at, "<Frame lock info>\n");
	at += sysfs_emit_at(buf, at, "\tphase diff: %d\n", (int)_s32FrameLockPhaseDiffValue);
	at += sysfs_emit_at(buf, at, "\tsrc lock: %d\n", (int)_s32FrameLockSrcLockValue);

	// input src
	for (i = 0; i < SRCCAP_COMMON_TRIGEN_DOMAIN_MAX; ++i) {
		if (t->IsB2Rsource)	// b2r case ignore srccap
			break;

		src = srccap_trigger_gen[i].src & (BIT(0) + BIT(1) + BIT(2));
		trig = srccap_trigger_gen[i].trig_sel & BIT(0);

		at += sysfs_emit_at(buf, at, "<%s>\n", src_domain_name[i]);

		at += sysfs_emit_at(buf, at, "\tsrc sel: %s\n",
			src < ARRAY_SIZE(src_sel) ? src_sel[src] : "Invalid");
		at += sysfs_emit_at(buf, at, "\ttrig sel: %s\n", trig_sel[trig]);

		at += sysfs_emit_at(buf, at, "\trw_bank_upd: %d\n",
					srccap_trigger_gen[i].RWBk_ud_dly_v);
		at += sysfs_emit_at(buf, at, "\tds/ml: %d\n", srccap_trigger_gen[i].ds_dly_v);
		at += sysfs_emit_at(buf, at, "\tvsync: %d\n", srccap_trigger_gen[i].vs_dly_v);
		at += sysfs_emit_at(buf, at, "\trd_trig: %d\n", srccap_trigger_gen[i].dma_r_dly_v);
	}

	// PQ
	for (j = 0; j < PQ_COMMON_TRIGEN_DOMAIN_MAX; ++j) {
		if (j == PQ_COMMON_TRIGEN_DOMAIN_B2RLITE0 || j == PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1)
			continue;

		// srccap case ignore b2r
		if (!t->IsB2Rsource) {
			if (j == PQ_COMMON_TRIGEN_DOMAIN_B2R)
				continue;
		}

		src = pq_trigger_gen[j].src & (BIT(0) + BIT(1) + BIT(2));
		trig = pq_trigger_gen[j].trig_sel & BIT(0);

		at += sysfs_emit_at(buf, at, "<%s>\n", pq_domain_name[j]);

		at += sysfs_emit_at(buf, at, "\tsrc sel: %s\n",
			src < ARRAY_SIZE(src_sel) ? src_sel[src] : "Invalid");
		at += sysfs_emit_at(buf, at, "\ttrig sel: %s\n", trig_sel[trig]);

		at += sysfs_emit_at(buf, at, "\trw_bank_upd: %d\n",
						pq_trigger_gen[j].RWBk_ud_dly_v);
		at += sysfs_emit_at(buf, at, "\tds/ml: %d\n", pq_trigger_gen[j].ds_dly_v);
		at += sysfs_emit_at(buf, at, "\tvsync: %d\n", pq_trigger_gen[j].vs_dly_v);
		at += sysfs_emit_at(buf, at, "\trd_trig: %d\n", pq_trigger_gen[j].dma_r_dly_v);
	}

	// render
	for (i = 0; i < RENDER_COMMON_TRIGEN_DOMAIN_MAX; ++i) {
		src = render_trigger_gen[i].src & (BIT(0) + BIT(1) + BIT(2));
		trig = render_trigger_gen[i].trig_sel & BIT(0);

		at += sysfs_emit_at(buf, at, "<%s>\n", render_domain_name[i]);

		at += sysfs_emit_at(buf, at, "\tsrc sel: %s\n",
			src < ARRAY_SIZE(src_sel) ? src_sel[src] : "Invalid");
		at += sysfs_emit_at(buf, at, "\ttrig sel: %s\n", trig_sel[trig]);

		at += sysfs_emit_at(buf, at, "\trw_bank_upd: %d\n",
						render_trigger_gen[i].RWBk_ud_dly_v);
		at += sysfs_emit_at(buf, at, "\tds/ml: %d\n", render_trigger_gen[i].ds_dly_v);
		at += sysfs_emit_at(buf, at, "\tvsync: %d\n", render_trigger_gen[i].vs_dly_v);
		at += sysfs_emit_at(buf, at, "\trd_trig: %d\n", render_trigger_gen[i].dma_r_dly_v);
	}

	return at;
}

static inline int _check_opt(char *cmd, const char *opt, bool *res)
{
	int ret;
	char *p;
	char *np;
	bool tmp;

	if (strstr(cmd, opt)) {
		p = (char *)cmd + strlen(opt) + 1;
		np = strsep(&p, "=");
		ret = kstrtobool(np, &tmp);
		if (ret)
			return ret;
		*res = tmp;
	}

	return 0;
}

int mtk_trigger_gen_init_render(struct mtk_tv_kms_context *pctx)
{
	int ret;
	struct drm_mtk_tv_trigger_gen_info tg_info = { };
	unsigned long flags;

	if (g_bIsUpdateRenderTG) {
		TG_DBG(DRM_INFO("TG Render Init START\n"));
		ret = _sanity_check(pctx);
		if (ret)
			return ret;
		spin_lock_irqsave(&pctx->tg_lock, flags);
		memcpy(&tg_info, &pctx->trigger_gen_info, sizeof(tg_info));
		spin_unlock_irqrestore(&pctx->tg_lock, flags);

		trigger_gen_get_param(pctx);
		trigger_gen_param_input_update(pctx, &tg_info);
		ret = trigger_gen_case_select(pctx, &tg_info, &g_u16CaseSelect);
		if (ret)
			return ret;

		trigger_gen_line_rate_cal(pctx, &tg_info);
		trigger_gen_render_formula(pctx, &tg_info);
		trigger_gen_frc_formula(pctx, &tg_info);

		trigger_gen_render_set(pctx, &tg_info, true);

		g_bIsUpdateRenderTG = false;
	}

	return 0;
}

int mtk_trigger_gen_set_param(struct mtk_tv_kms_context *pctx,
		const char *buf, size_t count)
{
	int ret = 0;
	char *tok;
	char *p;
	bool is_scmi_fb = false;
	bool is_frc_fb = false;
	bool is_low_latency = false;
	bool is_pmode = false;
	bool is_br2 = false;
	unsigned long flags;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!pctx)
		return -EINVAL;

	pctx_pnl = &pctx->panel_priv;

	// check dbg_off
	if (sysfs_streq(buf, "debug_off")) {
		pr_crit("%s: debbug off\n", __func__);
		pctx_pnl->gu8Triggergendebugsetting = E_TRIGGER_DEBUG_OFF;
		return 0;
	}

	p = kstrdup(buf, GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	while ((tok = strsep(&p, ","))) {
		ret = _check_opt(tok, "scmi_fb", &is_scmi_fb);
		if (ret) {
			pr_crit("%s: parse scmi_fb failed with %d\n", __func__, ret);
			goto out;
		}
		ret = _check_opt(tok, "frc_fb", &is_frc_fb);
		if (ret) {
			pr_crit("%s: parse frc_fb failed with %d\n", __func__, ret);
			goto out;
		}

		ret = _check_opt(tok, "ll", &is_low_latency);
		if (ret) {
			pr_crit("%s: parse ll failed with %d\n", __func__, ret);
			goto out;
		}

		ret = _check_opt(tok, "pmode", &is_pmode);
		if (ret) {
			pr_crit("%s: parse pmode failed with %d\n", __func__, ret);
			goto out;
		}

		ret = _check_opt(tok, "b2r", &is_br2);
		if (ret) {
			pr_crit("%s: parse is_br2 failed with %d\n", __func__, ret);
			goto out;
		}
	}

	spin_lock_irqsave(&pctx->tg_lock, flags);
	pctx->trigger_gen_info.IsLowLatency = is_low_latency;
	// XXX: only support merak 2.0
	pctx->trigger_gen_info.LegacyMode = true;
	pctx->trigger_gen_info.IsSCMIFBL = !is_scmi_fb;
	pctx->trigger_gen_info.IsFrcMdgwFBL = !is_frc_fb;
	pctx->trigger_gen_info.IsB2Rsource = is_br2;
	pctx->trigger_gen_info.IsPmodeEn = is_pmode;
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	pctx_pnl->gu8Triggergendebugsetting = E_TRIGGER_DEBUG_LEGACY;

	pr_crit("%s: ll:%s, scmi:%s, frc:%s, %s, %s-mode\n",
		__func__,
		pctx->trigger_gen_info.IsLowLatency ? "true" : "false",
		pctx->trigger_gen_info.IsSCMIFBL ? "fbl" : "fb",
		pctx->trigger_gen_info.IsFrcMdgwFBL ? "fbl" : "fb",
		pctx->trigger_gen_info.IsB2Rsource ? "b2r" : "ip",
		pctx->trigger_gen_info.IsPmodeEn ? "p" : "i");

out:
	kfree(p);

	return ret;
}

int mtk_trigger_gen_vsync_callback(void *p __maybe_unused)
{
	bool bIsTgBusy = true;

	if (atomic_read(&g_bIstgwork) == false)
		bIsTgBusy = false;

	drv_hwreg_render_pnl_set_trigger_gen_busy(bIsTgBusy);

	return 0;
}

int mtk_tv_drm_trigger_gen_init(
	struct mtk_tv_kms_context *pctx, bool render_modify)
{
	int ret = 0;

	ret |= mtk_tv_drm_set_trigger_gen_reset_to_default(pctx, true, render_modify);
	if (render_modify == false)
		g_bIsUpdateRenderTG = true;

	return ret;
}

