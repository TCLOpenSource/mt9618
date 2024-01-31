/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_TRIGGER_GEN_TBL_H_
#define _MTK_TV_DRM_TRIGGER_GEN_TBL_H_
#include "mtk_tv_drm_trigger_gen.h"
#include "hwreg_render_video_pnl.h"
#include "hwreg_render_common_trigger_gen.h"
#include "hwreg_srccap_common_trigger_gen.h"
#include "hwreg_pq_common_trigger_gen.h"

// TODO: should move to hwreg HAL

#ifdef CONFIG_MTK_DISP_BRINGUP
#define TRIGGER_GEN_SW_HCOUNT (20)
#define IPM_READ_DELAY (0)
#else
#define TRIGGER_GEN_SW_HCOUNT (88)
#define IPM_READ_DELAY (2)
#endif
#define TRIGGER_GEN_SW_HCOUNT_TS (200)
#define VSYNC_TRIG_LEN (TRIGGER_GEN_SW_HCOUNT * 2)
#define VSYNC_TRIG_LEN_TS (20)
#define SRCCAP_COMMON_SW_IRQ_DLY (20)
#define SRCCAP_COMMON_PQ_IRQ_DLY (20)

#ifdef CONFIG_MTK_DISP_BRINGUP
// create dummy TS table to make trigger gen drv happy
#define pq_trigger_gen_ts pq_trigger_gen
#define srccap_trigger_gen_ts srccap_trigger_gen
static struct reg_srccap_common_triggen_info
	srccap_trigger_gen[SRCCAP_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0, SRCCAP_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
	//sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1, SRCCAP_COMMON_TRIGEN_INPUT_IP1, 0, 0, 0,
	//sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },
};

static struct pq_common_triggen_info
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2R, PQ_COMMON_TRIGEN_INPUT_B2R0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 6, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_IP, PQ_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    2, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP1, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    12, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP2, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    19, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE0,
	(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 0, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 4,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 4, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, 4 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 6, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 7,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 7, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, 4 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 0, 0, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    2, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 6, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 7, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 135,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, 4 },
};

static struct render_common_triggen_info
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_DISP, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    20, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC1, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC2, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_TGEN, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 0, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 15, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },
};

#else	// manks
static struct reg_srccap_common_triggen_info
	srccap_trigger_gen[SRCCAP_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0, SRCCAP_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
	//sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1, SRCCAP_COMMON_TRIGEN_INPUT_IP1, 0, 0, 0,
	//sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },
};

static struct reg_srccap_common_triggen_info
	srccap_trigger_gen_ts[SRCCAP_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC0, SRCCAP_COMMON_TRIGEN_INPUT_IP0, 0, 1, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 1, 1, 1, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    1, 1, 3, 1, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    1, 4, 1, SRCCAP_COMMON_SW_IRQ_DLY,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    1, SRCCAP_COMMON_PQ_IRQ_DLY, 1, VSYNC_TRIG_LEN_TS },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ SRCCAP_COMMON_TRIGEN_DOMAIN_SRC1, SRCCAP_COMMON_TRIGEN_INPUT_IP1, 0, 1, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 1, 1, 1, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    1, 1, 3, 1, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    1, 4, 1, SRCCAP_COMMON_SW_IRQ_DLY,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    1, SRCCAP_COMMON_PQ_IRQ_DLY, 1, VSYNC_TRIG_LEN_TS },
};

static struct pq_common_triggen_info
	pq_trigger_gen[PQ_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2R, PQ_COMMON_TRIGEN_INPUT_B2R0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 6, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_IP, PQ_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    2, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP1, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    12, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP2, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    19, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE0,
		(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 0, 0,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 0, 0, 0,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 0, 0, 0, 0, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1,
		(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 0, 0,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 0, 0, 0,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 0, 0, 0, 0, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    0, 0, 0, VSYNC_TRIG_LEN },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 6, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 7, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	//swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN },
};

static struct pq_common_triggen_info
	pq_trigger_gen_ts[PQ_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2R, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 9, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, 4 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_IP, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    2, 2, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 8, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 9, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, 4 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP1, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    0xC, 2, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 8, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 9, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN_TS },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_OP2, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    0x13, 2, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 8, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 9, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN_TS },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE0,
		(enum pq_common_triggen_src_sel)RENDER_COMMON_TRIGEN_INPUT_IP0, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, 0, 0, 0, 0, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 2, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 5, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 6,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 6, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN_TS },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_B2RLITE1, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 0, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 7,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 7, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN_TS },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ PQ_COMMON_TRIGEN_DOMAIN_FRC_IPM, PQ_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 1,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT_TS, 1, 1, 1, 2,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    2, 2, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 3,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 8, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 9, PQ_COMMON_TRIGEN_TRIG_SEL_VS, 1,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel, sw_vsync_len
	    PQ_COMMON_TRIGEN_TRIG_SEL_VS, 0, 0, VSYNC_TRIG_LEN_TS },
};

static struct render_common_triggen_info
	render_trigger_gen[RENDER_COMMON_TRIGEN_DOMAIN_MAX] = {
	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_DISP, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    20, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC1, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 0xF, 0, 1, 0x10,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC2, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_TGEN, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 0, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_INPUT, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_STAGE, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 15, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_ME, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },

	//domain, src, trig_sel,trig_sel_separate,sw_trig_mode,
	{ RENDER_COMMON_TRIGEN_DOMAIN_FRC_MI, RENDER_COMMON_TRIGEN_INPUT_TGEN, 0, 0, 0,
    //sw_hcnt, sw_htt, sw_user_mode_h, sw_user_mode_v, vcnt_keep_en, vcnt_upd_mask_range,
	    0, TRIGGER_GEN_SW_HCOUNT, 1, 0, 1, 8,
	//RWBk_ud_dly_h, RWBk_ud_dly_v, rwbank_trig_sel, ds_dly_v, ds_trig_sel, ml_dly_v,
	    1, 0, 0, 1, 0, 1,
	//ml_trig_sel, vs_dly_h, vs_dly_v, vs_trig_sel, dma_r_dly_h, dma_r_dly_v
	    0, 1, 3, 0, 1, 9,
	//dmar_trig_sel, str_dly_v, str_trig_sel, sw_irq_dly_v,
	    0, 0, 0, 0,
	// swirq_trig_sel, pq_irq_dly_v, pqirq_trig_sel
	    0, 0, 0 },
};
#endif

typedef enum  {
	// 1280x720
	E_1280X720_48,      // 0
	E_1280X720_50,      // 1
	E_1280X720_60,
	E_1280X720_100,
	E_1280X720_120,
	// 2560x1080
	E_2560X1080_48,     // 5
	E_2560X1080_50,
	E_2560X1080_60,
	E_2560X1080_100,
	E_2560X1080_120,
	// 1920x1080
	E_1920X1080_48,     // 10
	E_1920X1080_50,
	E_1920X1080_60,
	E_1920X1080_100,
	E_1920X1080_120,
	// 4096x2160
	E_4096X2160_48,     // 15
	E_4096X2160_50,
	E_4096X2160_60,
	E_4096X2160_100,
	E_4096X2160_120,
	// 3840x2160
	E_3840X2160_48,     // 20
	E_3840X2160_50,
	E_3840X2160_60,
	E_3840X2160_100,
	E_3840X2160_120,
	E_3840X2160_144_1,
	E_3840X2160_144_2,
	E_3840X2160_144_3,
	E_3840X2160_144_4,
	// PC time
	E_2560X1440_60,     // 29
	E_2560X1440_120,
	// 1920x1080 and 2560x1440 144
	E_1920X1080_144,
	E_1920X1080_240,
	E_2560X1440_144,
	// 7680x4320
	E_7680X4320_48,     // 34
	E_7680X4320_50,
	E_7680X4320_60,
	E_7680X4320_100,
	E_7680X4320_120,

	E_TIMING_MAX,
} trigger_gen_timing_mapping;

static struct ST_VRR_BASE_VFREQ_MAPPING_TBL
	stVrrBaseFreqMappingTbl[E_TIMING_MAX] = {
	//idx  width height FR   HTT   VTT  CLK
	//1280x720
	{E_1280X720_48,  1280, 720,  48,   2500, 750, 90},   //1280x720@48
	{E_1280X720_50,  1280, 720,  50,   1980, 750, 74},   //1280x720@50
	{E_1280X720_60,  1280, 720,  60,   1650, 750, 74},   //1280x720@59.94/60
	{E_1280X720_100,  1280, 720,  100,  1980, 750, 148},  //1280x720@100
	{E_1280X720_120,  1280, 720,  120,  1650, 750, 148},  //1280x720@119.88/120
	//2560x1080
	{E_2560X1080_48,  2560, 1080,  48,  3750, 1100, 198},
	{E_2560X1080_50,  2560, 1080,  50,  3300, 1125, 185},
	{E_2560X1080_60,  2560, 1080,  60,  3000, 1100, 198},
	{E_2560X1080_100,  2560, 1080,  100, 2970, 1250, 371},
	{E_2560X1080_120,  2560, 1080,  120, 3300, 1250, 495},
	//1920x1080
	{E_1920X1080_48, 1920, 1080,  48,  2750, 1125, 148},
	{E_1920X1080_50, 1920, 1080,  50,  2640, 1125, 148},
	{E_1920X1080_60, 1920, 1080,  60,  2200, 1125, 148},
	{E_1920X1080_100, 1920, 1080,  100, 2640, 1125, 297},
	{E_1920X1080_120, 1920, 1080,  120, 2200, 1125, 297},
	//4096x2160
	{E_4096X2160_48, 4096, 2160,  48,  5500, 2250, 594},
	{E_4096X2160_50, 4096, 2160,  50,  5280, 2250, 594},
	{E_4096X2160_60, 4096, 2160,  60,  4400, 2250, 594},
	{E_4096X2160_100, 4096, 2160,  100, 5280, 2250, 1188},
	{E_4096X2160_120, 4096, 2160,  120, 4400, 2250, 1188},
	//3840x2160
	{E_3840X2160_48, 3840, 2160,  48,  5500, 2250, 594},
	{E_3840X2160_50, 3840, 2160,  50,  5280, 2250, 594},
	{E_3840X2160_60, 3840, 2160,  60,  4400, 2250, 594},
	{E_3840X2160_100, 3840, 2160,  100, 5280, 2250, 1188},
	{E_3840X2160_120, 3840, 2160,  120, 4400, 2250, 1188},

	{E_3840X2160_144_1, 3840, 2160,  144, 3920, 2314, 1306},
	{E_3840X2160_144_2, 3840, 2160,  144, 4032, 2314, 1343},
	{E_3840X2160_144_3, 3840, 2160,  144, 4400, 2250, 1426},
	{E_3840X2160_144_4, 3840, 2160,  144, 5408, 2350, 1830},
	//PC timing
	{E_2560X1440_60, 2560, 1440,  60,  2720, 1481, 241},
	{E_2560X1440_120, 2560, 1440,  120, 2720, 1525, 497},
	//1920x1080 144, 2560x1080 144
	{E_1920X1080_144, 1920, 1080,  144, 2672, 1177, 452},
	{E_1920X1080_240, 1920, 1080,  240, 2200, 1125, 594},
	{E_2560X1440_144, 2560, 1440,  144, 3584, 1568, 809},
	//7680x4320
	{E_7680X4320_48, 7680, 4320,  48,  11000, 4450, 2350},
	{E_7680X4320_50, 7680, 4320,  50,  10560, 4450, 2350},
	{E_7680X4320_60, 7680, 4320,  60,  8800, 4450, 2350},
	{E_7680X4320_100, 7680, 4320,  100, 10560, 4450, 4699},
	{E_7680X4320_120, 7680, 4320,  120, 8800, 4450, 4699},
};


#endif

