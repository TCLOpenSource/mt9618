// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mtk_tv_drm_video_panel.h"
#include "../mtk_tv_lpll_tbl.h"
#include "../mtk_tv_drm_log.h"
#include "../mtk_tv_drm_kms.h"
#include "../metabuf/mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_backlight.h"
#include "mtk_tv_drm_trigger_gen.h"
#include "hwreg_render_video_pnl.h"
#include "hwreg_render_common_trigger_gen.h"
#include "common/hwreg_common_riu.h"
#include "pixelmonitor.h"
#include "mtk_tv_sm_ml.h"
#include "mtk_tv_drm_common.h"
#include <drm/mtk_tv_drm_panel.h>
#include "mtk_tcon_out_if.h"
#include "mtk_tcon_common.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "m_pqu_render.h"
#include "pnl_cust.h"
#include <linux/i2c.h>
#include "mtk_tv_drm_demura.h"
#include "mtk_tv_drm_ld.h"
#include "drvLDM_sti.h"
#include "apiLDM.h"


//reference from mtk_tv_drm_backlight.c
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/completion.h>
#include <drm/mtk_tv_drm.h>
#include "hwreg_render_stub.h"
//mdelay
#include <linux/delay.h>
//pm
#include <soc/mediatek/mtk-pm.h>

#define MTK_DRM_MODEL MTK_DRM_MODEL_PANEL
#define CONNECTID_MAIN (0)
#define CONNECTID_GFX (1)
#define CONNECTID_DELTA (2)
#define VFRQRATIO_10 (10)
#define VFRQRATIO_1000 (1000)
#define MOD_TOP_CLK (594)
#define MOD_TOP_CLK_720MHZ (720)
#define MOD_TOP_CLK_360MHZ (360)
#define P_250 (250)
#define P_500 (500)
#define P_1000 (1000)
#define P_2000 (2000)
#define P_4000 (4000)
#define P_8000 (8000)
#define REFRESH_144 (144)
#define PNL_WAIT_ACTIVE (20)

#define DEC_1 (1)
#define DEC_2 (2)
#define DEC_5 (5)
#define DEC_8 (8)
#define DEC_10 (10)
#define DEC_16 (16)
#define DEC_100 (100)
#define DEC_1000 (1000)
#define HEX_1 (0x1)
#define HEX_2 (0x2)
#define HEX_38 (0x38)
#define HEX_FF (0xFF)
#define I2C_HW_MODE (1)

#define IS_INPUT_B2R(x)     ((x == META_PQ_INPUTSRC_DTV) ||\
				(x == META_PQ_INPUTSRC_STORAGE) || \
				(x == META_PQ_INPUTSRC_STORAGE))
#define IS_INPUT_SRCCAP(x)     ((x == META_PQ_INPUTSRC_HDMI) ||\
				(x == META_PQ_INPUTSRC_VGA) || \
				(x == META_PQ_INPUTSRC_TV) ||\
				(x == META_PQ_INPUTSRC_YPBPR) || \
				(x == META_PQ_INPUTSRC_CVBS))

#define LANE_NUM_64 (64)
#define LANE_NUM_32 (32)
#define LANE_NUM_16 (16)
#define LANE_NUM_8 (8)
#define LANE_NUM_4 (4)
#define LANE_NUM_2 (2)

//chip version
#define LPLL_VER_2 (2)
#define LPLL_VER_203 (0x203)
#define MOD_VER_2 (2)
#define MOD_CLK_VER_0 (0)
#define MOD_CLK_VER_1 (1)
#define MOD_CLK_VER_2 (2)
#define MOD_CLK_VER_3 (3)
#define MOD_CLK_VER_4 (4)
#define MOD_CLK_VER_5 (5)

#define _READY_CB_COUNT 4
#define _CB_INC(x) ((x++)&(_READY_CB_COUNT-1))
#define _CB_DEC(x) (x == 0 ? 0 : --x)
#define MHZ (1000000ULL)

#if !defined(LOBYTE)
#define LOBYTE(w)           ((unsigned char)(w))
#endif
#if !defined(HIBYTE)
#define HIBYTE(w)           ((unsigned char)(((unsigned short)(w) >> DEC_8) & HEX_FF))
#endif

#define IS_TCON_FIFOCLK_SUPPORT(x)    (((x >= E_LINK_USIT_8BIT_6PAIR) &&\
					(x <= E_LINK_CHPI_10BIT_6PAIR)) || \
					((x >= E_LINK_USIT_8BIT_8PAIR) &&\
					(x <= E_LINK_USIT_10BIT_16PAIR)) || \
					((x >= E_LINK_MINILVDS_1BLK_3PAIR_6BIT) &&\
					(x <= E_LINK_MINILVDS_2BLK_6PAIR_8BIT)))

#define PNL_FREQ_GROUP1_LOWER (45000)
#define PNL_FREQ_GROUP1_UPPER (65000)
#define PNL_FREQ_GROUP2_LOWER (90000)
#define PNL_FREQ_GROUP2_UPPER (125000)
#define PNL_FREQ_GROUP3_LOWER (140000)
#define PNL_FREQ_GROUP3_UPPER (150000)
#define PNL_FREQ_GROUP4_LOWER (180000)
#define PNL_FREQ_GROUP4_UPPER (250000)

#define I2C_RETRY_COUNT (10)
#define I2C_DEFAULT_BUS (0x02)

#define VSYNC_FREQ_TO_CYCLE(f)		({__s32 _x = f; (_x) ? (1000000U / (_x)) : 0; })
#define VCNT_COUNTER			100
#define MTK_DRM_KMS_GFID_MAX		255
static time64_t g_u64time_resume;
static bool g_runtime_force_suspend_enable;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define DEV_PANEL_CTX_TO_KMS_CTX(pnl)	({							\
	struct mtk_panel_context *__pnl = pnl;							\
	struct drm_connector *__dcn = (__pnl) ? __pnl->connector : NULL;			\
	struct mtk_tv_drm_connector *__mcn = (__dcn) ? to_mtk_tv_connector(__dcn) : NULL;	\
	(__mcn) ? (struct mtk_tv_kms_context *)__mcn->connector_private : NULL;			})
#else
#define DEV_PANEL_CTX_TO_KMS_CTX(pnl)	({							\
	struct mtk_panel_context *__pnl = pnl;							\
	struct drm_connector *__dcn = (__pnl) ? __pnl->drmPanel.connector : NULL;		\
	struct mtk_tv_drm_connector *__mcn = (__dcn) ? to_mtk_tv_connector(__dcn) : NULL;	\
	(__mcn) ? (struct mtk_tv_kms_context *)__mcn->connector_private : NULL;			})
#endif

//-------------------------------------------------------------------------------------------------
// Prototype
//-------------------------------------------------------------------------------------------------
static int mtk_tv_drm_panel_disable(struct drm_panel *panel);
static int mtk_tv_drm_panel_unprepare(struct drm_panel *panel);
static int mtk_tv_drm_panel_prepare(struct drm_panel *panel);
static int mtk_tv_drm_panel_enable(struct drm_panel *panel);
static int mtk_tv_drm_panel_get_modes(struct drm_panel *panel);
static int mtk_tv_drm_panel_get_timings(struct drm_panel *panel,
						unsigned int num_timings,
						struct display_timing *timings);

static int mtk_tv_drm_panel_probe(struct platform_device *pdev);
static int mtk_tv_drm_panel_remove(struct platform_device *pdev);
static void mtk_tv_drm_panel_shutdown(struct platform_device *pdev);

static int mtk_video_panel_vsync_callback(void *priv_data);
static void _mtk_panel_power_control(struct drm_panel *pDrmPanel,	bool bOnOff);
static int mtk_tv_drm_panel_bind(
	struct device *compDev,
	struct device *master,
	void *masterData);
static void mtk_tv_drm_panel_unbind(
	struct device *compDev,
	struct device *master,
	void *masterData);
#ifdef CONFIG_PM
static int panel_runtime_suspend(struct device *dev);
static int panel_runtime_resume(struct device *dev);
static int panel_pm_runtime_force_suspend(struct device *dev);
static int panel_pm_runtime_force_resume(struct device *dev);
#endif
static __u64 u64PreTypDclk;
static drm_en_pnl_output_format ePreOutputFormat = E_PNL_OUTPUT_FORMAT_NUM;
static drm_en_vbo_bytemode ePreByteMode = E_VBO_MODE_MAX;

static bool bPnlInitDone;
static bool backlight_undo;
static bool bl_undo_en;
static struct task_struct *panel_moniter_worker;
static struct pqu_dd_update_pq_cfd_sharemem_param _cfd_sharemem_msg_info[_READY_CB_COUNT];
static struct pqu_dd_update_pq_info_cfd_context _cfd_context_msg_info[_READY_CB_COUNT];
static struct pqu_render_pixel_mute _pixel_mute_msg_info[_READY_CB_COUNT];
static int _cfd_sharemem_count;
static int _cfd_context_count;
static int _pixel_mute_count;
static bool bTconModeChange;
static bool bTconPreOutputEnable;
static DEFINE_STATIC_KEY_FALSE(tg_need_update);
static DEFINE_STATIC_KEY_FALSE(phasediff_need_update);

static __u16 u16GameDirectFrGroup = HEX_FF;
static __u16 u16DLG_On = HEX_FF;

static unsigned char ucChange_ModeID_state = E_MODE_ID_CHANGE_INIT;

static bool _bTCON_LC_SecToggle_Start = FALSE;
static bool _bTCON_LC_SecToggle_STR_Off = FALSE;
static __u16  _u16TCON_LC_SecToggle_Frame_Cnt;
static __u16  _u16TCON_LC_SecToggle_Frame_Cnt_Target = TCON_GPO_LC_Default_Frame_Period;
static __u32  _u16PreTCON_LC_TOGGLE_GPO_Status = TCON_GPO_LC_BitMap;
static __u32  _u16CurTCON_LC_TOGGLE_GPO_Status;

static struct drm_mtk_panel_max_framerate PanelMaxFramerate;
bool g_bGlobalMuteEn = FALSE;
struct mtk_global_mute_context g_global_mute_con;
static struct task_struct *g_global_unmute_worker;
struct mutex mutex_globalmute;

static bool _i2c_need_init;

static bool _bTrigger_panel_pw_off;
static bool _bTrigger_panel_pw_on;
struct mutex mutex_panel;
static __u8 tcon_i2c_count;
static __u8 tcon_i2c_sec_count;
static bool _bPreDLG_status = FALSE;
static bool _bBL_ON_status = TRUE;
static struct completion bl_on_comp;

//-------------------------------------------------------------------------------------------------
// Structure
//-------------------------------------------------------------------------------------------------
static const struct dev_pm_ops panel_pm_ops = {
	SET_RUNTIME_PM_OPS(panel_runtime_suspend,
			   panel_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_pm_runtime_force_suspend,
				panel_pm_runtime_force_resume)
};

static const struct drm_panel_funcs drmPanelFuncs = {
	.disable = mtk_tv_drm_panel_disable,
	.unprepare = mtk_tv_drm_panel_unprepare,
	.prepare = mtk_tv_drm_panel_prepare,
	.enable = mtk_tv_drm_panel_enable,
	.get_modes = mtk_tv_drm_panel_get_modes,
	.get_timings = mtk_tv_drm_panel_get_timings,
};

static const struct of_device_id mtk_tv_drm_panel_of_match_table[] = {
	{ .compatible = "mtk,mt5896-panel",},
	{/* sentinel */}
};
MODULE_DEVICE_TABLE(of, mtk_tv_drm_panel_of_match_table);

struct platform_driver mtk_tv_drm_panel_driver = {
	.probe		= mtk_tv_drm_panel_probe,
	.remove		= mtk_tv_drm_panel_remove,
	.driver = {
		.name = "video_out",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_tv_drm_panel_of_match_table),
		.pm = &panel_pm_ops,
	},
	.shutdown	= mtk_tv_drm_panel_shutdown,
};

static const struct component_ops mtk_tv_drm_panel_component_ops = {
	.bind	= mtk_tv_drm_panel_bind,
	.unbind = mtk_tv_drm_panel_unbind,
};

struct mtk_lpll_table_info {
	E_PNL_SUPPORTED_LPLL_TYPE table_start;
	E_PNL_SUPPORTED_LPLL_TYPE table_end;
	uint16_t panel_num;
	uint16_t lpll_reg_num;
	uint16_t mpll_reg_num;
	uint16_t moda_reg_num;
};

struct mtk_lpll_type {
	E_PNL_SUPPORTED_LPLL_TYPE lpll_type;
	E_PNL_SUPPORTED_LPLL_TYPE lpll_tbl_idx;
};

struct mtk_global_mute_context {
	struct mtk_panel_context *pctx_pnl;
	drm_st_backlight_mute_info backlight_mute_info;
	drm_st_pixel_mute_info pixel_mute_info;
};

struct mtk_tv_color_info {
	u8 color_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;
};

//mapping color space to VUI
static struct mtk_tv_color_info color_metry_map[5] = {
	{1, 11, 5},    //0:E_CFD_COLORSPACE_BT601
	{1, 1, 1},	   //1:E_CFD_COLORSPACE_BT709
	{9, 14, 9},    //2:E_CFD_COLORSPACE_BT2020
	{12, 13, 0},   //3:E_CFD_COLORSPACE_DCI_P3
	{0x80, 21, 0}, //4:E_CFD_COLORSPACE_ADOBE_RGB
};

//-------------------------------------------------------------------------------------------------
// Function
//-------------------------------------------------------------------------------------------------

//bring up start
static void _get_table_info(struct mtk_panel_context *pctx, struct mtk_lpll_table_info *tbl_info)
{
	en_drv_pnl_version lpll_version = DRV_PNL_VERSION0100;

	if (!pctx || !tbl_info) {
		DRM_ERROR("[%s, %d]: invalid input", __func__, __LINE__);
		return;
	}

	lpll_version = drv_hwreg_render_pnl_get_lpllversion();

	switch (lpll_version) {
	case DRV_PNL_VERSION0100:
		tbl_info->table_start = E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_5BYTE_OSD_5BYTE_300to300MHz;
		tbl_info->panel_num = PNL_NUM_VER1;
		tbl_info->lpll_reg_num = LPLL_REG_NUM;
		tbl_info->mpll_reg_num = 0;
		tbl_info->moda_reg_num = 0;
		break;
	case DRV_PNL_VERSION0200:
		tbl_info->table_start =
			E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz_VER2;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_5BYTE_OSD_5BYTE_Xtal_mode_0to583_2MHz_VER2;
		tbl_info->panel_num = PNL_NUM_VER0200;
		tbl_info->lpll_reg_num = LPLL_REG_NUM_VER0200;
		tbl_info->mpll_reg_num = MPLL_REG_NUM_VER0200;
		tbl_info->moda_reg_num = MODA_REG_NUM_VER0200;
		break;
	case DRV_PNL_VERSION0300:
		tbl_info->table_start =
			E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz_VER0300;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_CEDS_TEST_0to600MHz_VER0300;
		tbl_info->panel_num = PNL_NUM_VER0300;
		tbl_info->lpll_reg_num = LPLL_REG_NUM_VER0300;
		tbl_info->mpll_reg_num = MPLL_REG_NUM_VER0300;
		tbl_info->moda_reg_num = MODA_REG_NUM_VER0300;
		break;
	case DRV_PNL_VERSION0203:
		tbl_info->table_start =
			E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz_VER0203;
		tbl_info->table_end =
		E_PNL_SUPPORTED_LPLL_Vx1_Video_5BYTE_OSD_5BYTE_Xtal_mode_0to583_2MHz_VER0203;
		tbl_info->panel_num = PNL_NUM_VER0203;
		tbl_info->lpll_reg_num = LPLL_REG_NUM_VER0203;
		tbl_info->mpll_reg_num = MPLL_REG_NUM_VER0203;
		tbl_info->moda_reg_num = MODA_REG_NUM_VER0203;
		break;
	case DRV_PNL_VERSION0400:
		tbl_info->table_start =
			E_PNL_SUPPORTED_LPLL_TTL_450to600MHz_VER004;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1K144_180to180MHz_VER004;
		tbl_info->panel_num = PNL_NUM_VER004;
		tbl_info->lpll_reg_num = LPLL_REG_NUM_VER004;
		tbl_info->mpll_reg_num = MPLL_REG_NUM_VER004;
		tbl_info->moda_reg_num = MODA_REG_NUM_VER004;
		break;
	case DRV_PNL_VERSION0500:
		tbl_info->table_start =
			E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz_VER005;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_8bit_300to300MHz_VER005;
		tbl_info->panel_num = PNL_NUM_VER005;
		tbl_info->lpll_reg_num = LPLL_REG_NUM_VER005;
		tbl_info->mpll_reg_num = MPLL_REG_NUM_VER005;
		tbl_info->moda_reg_num = MODA_REG_NUM_VER005;
		break;

	default:
		tbl_info->table_start = E_PNL_SUPPORTED_LPLL_LVDS_1ch_450to600MHz;
		tbl_info->table_end =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_5BYTE_OSD_5BYTE_300to300MHz;
		tbl_info->panel_num = PNL_NUM_VER1;
		tbl_info->lpll_reg_num = LPLL_REG_NUM;
		tbl_info->mpll_reg_num = 0;
		tbl_info->moda_reg_num = 0;
		break;
	}
}

static E_PNL_SUPPORTED_LPLL_TYPE __get_ver5_link_idx(struct mtk_panel_context *pctx)
{
	uint64_t ldHz = pctx->info.typ_dclk;
	E_PNL_SUPPORTED_LPLL_TYPE idx = E_PNL_SUPPORTED_LPLL_MAX;

	switch (pctx->v_cfg.linkIF) {
	case E_LINK_HSLVDS_1CH:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_450to600MHz_VER005;
		break;

	case E_LINK_HSLVDS_2CH:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_450to600MHz_VER005;
		break;

	case E_LINK_MINILVDS_2BLK_3PAIR_8BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_8bit_300to300MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_8bit_300to700MHz_VER005;
		break;

	case E_LINK_EPI28_8BIT_6PAIR_4KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_CML_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 360 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_CML_300to360MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_CML_360to600MHz_VER005;
		break;

	case E_LINK_EPI28_8BIT_8PAIR_4KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_CML_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_CML_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_CML_450to600MHz_VER005;
		break;

	case E_LINK_EPI28_8BIT_12PAIR_4KCML:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K144_CML_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K144_CML_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180*MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_CML_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_CML_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_CML_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 360 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_CML_300to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_CML_360to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K60_CML_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K60_CML_300to600MHz_VER005;
		}
		break;

	case E_LINK_EPI28_8BIT_16PAIR_4KCML:
		if ((pctx->v_cfg.timing == E_4K2K_144HZ) || (pctx->v_cfg.timing == E_4K1K_144HZ)) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_CML_360to360MHz_VER005;
			else if ((ldHz >= 360 * MHZ) && (ldHz < 480 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_CML_360to480MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_CML_480to720MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_CML_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_CML_300to450MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_CML_450to600MHz_VER005;
		}
		break;

	case E_LINK_EPI24_10BIT_12PAIR_4KCML:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_CML_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_CML_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_CML_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K120_CML_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K120_CML_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K60_CML_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K60_CML_300to600MHz_VER005;
		}
		break;

	case E_LINK_EPI24_10BIT_16PAIR_4KCML:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K1K144_CML_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K1K144_CML_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_CML_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_CML_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_CML_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K60_CML_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K60_CML_300to600MHz_VER005;
		}
		break;

	case E_LINK_EPI28_8BIT_6PAIR_4KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_LVDS_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 360 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_LVDS_300to360MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K60_LVDS_360to600MHz_VER005;
		break;

	case E_LINK_EPI28_8BIT_8PAIR_4KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_LVDS_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_LVDS_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K60_LVDS_450to600MHz_VER005;
		break;

	case E_LINK_EPI28_8BIT_12PAIR_4KLVDS:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K144_LVDS_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K144_LVDS_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_LVDS_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_LVDS_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_LVDS_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 360 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_LVDS_300to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K120_LVDS_360to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K60_LVDS_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K60_LVDS_300to600MHz_VER005;
		}
		break;

	case E_LINK_EPI28_8BIT_16PAIR_4KLVDS:
		if ((pctx->v_cfg.timing == E_4K2K_144HZ) || (pctx->v_cfg.timing == E_4K1K_144HZ)) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_LVDS_360to360MHz_VER005;
			else if ((ldHz >= 360 * MHZ) && (ldHz < 480 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_LVDS_360to480MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K144_LVDS_480to720MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_LVDS_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_LVDS_300to450MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_16x1_4K120_LVDS_450to600MHz_VER005;
		}
		break;

	case E_LINK_EPI24_10BIT_12PAIR_4KLVDS:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_LVDS_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_LVDS_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_LVDS_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K120_LVDS_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K120_LVDS_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K60_LVDS_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K60_LVDS_300to600MHz_VER005;
		}
		break;

	case E_LINK_EPI24_10BIT_16PAIR_4KLVDS:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K1K144_LVDS_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K1K144_LVDS_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_LVDS_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_LVDS_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K120_LVDS_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K60_LVDS_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_16x1_4K60_LVDS_300to600MHz_VER005;
		}
		break;

	case E_LINK_CMPI27_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_4K60_300to400MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_4K60_400to600MHz_VER005;
		break;

	case E_LINK_CMPI27_8BIT_8PAIR:
		if (ldHz < 300*MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_4K60_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_4K60_450to600MHz_VER005;
		break;

	case E_LINK_CMPI27_8BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K144_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K144_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_8bit_12x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_8bit_12x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K120_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K120_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K120_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_CMPI27_10BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K1K144_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K1K144_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K1K144_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_USIT_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_4K60_300to400MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_4K60_400to600MHz_VER005;
		break;

	case E_LINK_USIT_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_8x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_8x1_4K60_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_8x1_4K60_450to600MHz_VER005;
		break;

	case E_LINK_USIT_8BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K144_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K144_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K120_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K120_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K120_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_USIT_8BIT_16PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K144_360to360MHz_VER005;
			else if ((ldHz >= 360 * MHZ) && (ldHz < 540 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K144_360to540MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K144_540to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K1K144_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K1K144_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K1K144_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K60_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K60_300to450MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_16x1_4K60_450to600MHz_VER005;
		}
		break;

	case E_LINK_USIT_10BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_USIT_10BIT_16PAIR:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K120_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K120_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K120_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_16x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_ISP_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_4K60_300to400MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_4K60_400to600MHz_VER005;
		break;

	case E_LINK_ISP_8BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x2_4K60_300to300MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x2_4K60_300to600MHz_VER005;
		break;

	case E_LINK_ISP_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_4K60_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_4K60_450to600MHz_VER005;
		break;

	case E_LINK_ISP_8BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K144_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K144_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K120_300to300MHz_VER005;
			else if ((ldHz >= 300 * MHZ) && (ldHz < 400 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K120_300to400MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K120_400to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_ISP_10BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_6x2_4K60_300to300MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_6x2_4K60_300to600MHz_VER005;
		break;

	case E_LINK_ISP_10BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K144_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K144_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K1K144_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K1K144_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K1K144_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_CHPI_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x1_4K60_300to300MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x1_4K60_300to600MHz_VER005;
		break;

	case E_LINK_CHPI_8BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x2_4K60_300to300MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x2_4K60_300to600MHz_VER005;
		break;

	case E_LINK_CHPI_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_4K60_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_4K60_450to600MHz_VER005;
		break;

	case E_LINK_CHPI_8BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K2K_144HZ) {
			if (ldHz < 360 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K144_360to360MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K144_360to720MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K1K144_180to180MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K1K144_180to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_CHPI_10BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_4K60_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 360 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_4K60_300to360MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_4K60_360to600MHz_VER005;
		break;

	case E_LINK_CHPI_10BIT_12PAIR:
		if (pctx->v_cfg.timing == E_4K1K_144HZ) {
			if (ldHz < 180 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K1K144_180to180MHz_VER005;
			else if ((ldHz >= 180 * MHZ) && (ldHz < 270 * MHZ))
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K1K144_180to270MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K1K144_270to360MHz_VER005;
		} else if (pctx->v_cfg.timing == E_4K2K_120HZ ||
				pctx->v_cfg.timing == E_4K1K_240HZ) {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K120_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K120_300to600MHz_VER005;
		} else {
			if (ldHz < 300 * MHZ)
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K60_300to300MHz_VER005;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K60_300to600MHz_VER005;
		}
		break;

	case E_LINK_LVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_LVDS_2ch_300to300MHz_VER005;
		else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
			idx = E_PNL_SUPPORTED_LPLL_LVDS_2ch_300to450MHz_VER005;
		else
			idx = E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER005;
		break;

	case E_LINK_VB1:
		switch (pctx->v_cfg.timing) {
		// Vx1 10bit/8bit format is depend on Vx1 byte_mode, not dither_depth
		case E_4K2K_120HZ:
		case E_4K1K_240HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_16_lane_4K120_300to300MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_16_lane_4K120_300to600MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K120_300to300MHz_VER005;
				else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K120_300to450MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K120_450to600MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;

		case E_4K2K_60HZ:
		case E_4K1K_120HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K60_300to300MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K60_300to600MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K60_300to300MHz_VER005;
				else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K60_300to450MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K60_450to600MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;

		case E_4K2K_30HZ:
		case E_FHD_120HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_4_lane_4K30_300to300MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_4_lane_4K30_300to600MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_4_lane_4K30_300to300MHz_VER005;
				else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_4_lane_4K30_300to450MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_4_lane_4K30_450to600MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;
		case E_FHD_60HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_2K60_300to300MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_2K60_300to600MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 300 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_2_lane_2K60_300to300MHz_VER005;
				else if ((ldHz >= 300 * MHZ) && (ldHz < 450 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_2_lane_2K60_300to450MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_2_lane_2K60_450to600MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;

		case E_4K2K_144HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 360 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_16_lane_4K144_360to360MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_16_lane_4K144_360to720MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 360 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K144_360to360MHz_VER005;
				else if ((ldHz >= 360 * MHZ) && (ldHz < 480 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K144_360to480MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_16_lane_4K144_480to720MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;
		case E_4K1K_144HZ:
			if (pctx->info.vbo_byte == E_VBO_4BYTE_MODE) {
				if (ldHz < 180 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K1k144_180to180MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K1k144_180to360MHz_VER005;
			} else if (pctx->info.vbo_byte == E_VBO_3BYTE_MODE) {
				if (ldHz < 180 * MHZ)
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1k144_180to180MHz_VER005;
				else if ((ldHz >= 180 * MHZ) && (ldHz < 240 * MHZ))
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1k144_180to240MHz_VER005;
				else
					idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1k144_240to360MHz_VER005;
			} else {
				DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
			}
			break;
		default:
			DRM_ERROR("[%s][%d] Not Support timming\n", __func__, __LINE__);
			break;
		}
		break;
	default:
		DRM_ERROR("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		break;
	}

	return idx;
}

static E_PNL_SUPPORTED_LPLL_TYPE __get_ver4_link_idx(struct mtk_panel_context *pctx)
{
	uint64_t ldHz = pctx->info.typ_dclk;
	E_PNL_SUPPORTED_LPLL_TYPE idx = E_PNL_SUPPORTED_LPLL_MAX;

	switch (pctx->v_cfg.linkIF) {
	case E_LINK_TTL:
		if (ldHz < 75 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_TTL_75to75MHz_VER004;
		else if (ldHz >= 75 * MHZ && ldHz < 225 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_TTL_75to225MHz_VER004;
		else if (ldHz >= 225 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_TTL_225to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_TTL_450to600MHz_VER004;
		break;
	case E_LINK_HSLVDS_1CH:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_1ch_450to600MHz_VER004;
		break;
	case E_LINK_HSLVDS_2CH:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_HS_LVDS_2ch_450to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_1BLK_3PAIR_6BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_3pair_6bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_3pair_6bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_1BLK_6PAIR_6BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_6pair_6bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_6pair_6bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_2BLK_3PAIR_6BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_6bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_6bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_2BLK_6PAIR_6BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_6pair_6bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_6pair_6bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_1BLK_3PAIR_8BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_3pair_8bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_3pair_8bit_300to600MHz_VER004;
		break;

	case E_LINK_MINILVDS_1BLK_6PAIR_8BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_6pair_8bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_1bk_6pair_8bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_2BLK_3PAIR_8BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_8bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_3pair_8bit_300to600MHz_VER004;
		break;
	case E_LINK_MINILVDS_2BLK_6PAIR_8BIT:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_6pair_8bit_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_MINILVDS_2bk_6pair_8bit_300to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_2PAIR_2KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_CML_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_CML_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_CML_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_4PAIR_2KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_CML_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_CML_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_CML_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_6PAIR_2KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6pair_2K_CML_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6pair_2K_CML_300to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_8PAIR_2KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_CML_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_CML_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_CML_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_6PAIR_4KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_CML_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 350 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_CML_300to350MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_CML_350to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_8PAIR_4KCML:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_CML_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_CML_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_CML_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_12PAIR_4KCML:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K_CML_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
			E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_CML_180to360MHz_VER004;
			else
				idx =
				E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K_CML_300to600MHz_VER004;
		}
		break;
	case E_LINK_EPI24_10BIT_12PAIR_4KCML:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K_CML_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
			E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_CML_250to360MHz_VER004;
			else
				idx =
				E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K_CML_300to600MHz_VER004;
		}
		break;
	case E_LINK_EPI28_8BIT_2PAIR_2KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_LVDS_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_LVDS_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_2pair_2K_LVDS_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_4PAIR_2KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_LVDS_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_LVDS_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_4pair_2K_LVDS_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_6PAIR_2KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6pair_2K_LVDS_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6pair_2K_LVDS_300to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_8PAIR_2KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_LVDS_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_LVDS_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8pair_2K_LVDS_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_6PAIR_4KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_LVDS_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 350 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_LVDS_300to350MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_6x1_4K_LVDS_350to600MHz_VER004;
		break;

	case E_LINK_EPI28_8BIT_8PAIR_4KLVDS:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_LVDS_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_LVDS_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_8x1_4K_LVDS_450to600MHz_VER004;
		break;
	case E_LINK_EPI28_8BIT_12PAIR_4KLVDS:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K_LVDS_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
			E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K1K144_LVDS_180to360MHz_VER004;
			else
				idx =
				E_PNL_SUPPORTED_LPLL_EPI_28_8bit_12x1_4K_LVDS_300to600MHz_VER004;
		}
		break;
	case E_LINK_EPI24_10BIT_12PAIR_4KLVDS:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K_LVDS_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
			E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K1K144_LVDS_250to360MHz_VER004;
			else
				idx =
				E_PNL_SUPPORTED_LPLL_EPI_24_10bit_12x1_4K_LVDS_300to600MHz_VER004;
		}
		break;
	case E_LINK_CMPI27_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 400 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_300to400MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_6x1_400to600MHz_VER004;
		break;
	case E_LINK_CMPI27_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_8x1_450to600MHz_VER004;
		break;
	case E_LINK_CMPI27_8BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_4K1K144_180to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_27_8bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_CMPI27_10BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 400 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_8x1_300to400MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_8x1_400to600MHz_VER004;
		break;
	case E_LINK_CMPI27_10BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_4K1K144_250to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_CMPI_24_10bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_USIT_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 400 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_300to400MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_6x1_400to600MHz_VER004;
		break;
	case E_LINK_USIT_8BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_4K1K144_180to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_8bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_USIT_10BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_6x1_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_6x1_300to600MHz_VER004;
		break;
	case E_LINK_USIT_10BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_4K1K144_300to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_USI_T_10bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_ISP_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 400 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_300to400MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x1_400to600MHz_VER004;
		break;
	case E_LINK_ISP_8BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x2_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_6x2_300to600MHz_VER004;
		break;
	case E_LINK_ISP_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_8x1_450to600MHz_VER004;
		break;
	case E_LINK_ISP_8BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_4K1K144_180to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_8bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_ISP_10BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_6x2_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_6x2_300to600MHz_VER004;
		break;
	case E_LINK_ISP_10BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 400 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_8x1_300to400MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_8x1_400to600MHz_VER004;
		break;
	case E_LINK_ISP_10BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				 E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_4K1K144_300to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_ISP_10bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_CHPI_8BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x1_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x1_300to600MHz_VER004;
		break;
	case E_LINK_CHPI_8BIT_6X2PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x2_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_6x2_300to600MHz_VER004;
		break;
	case E_LINK_CHPI_8BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 450 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_300to450MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_8x1_450to600MHz_VER004;
		break;
	case E_LINK_CHPI_8BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_4K1K144_180to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_8bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_CHPI_10BIT_8PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_300to300MHz_VER004;
		else if (ldHz >= 300 * MHZ && ldHz < 350 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_300to350MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_8x1_350to600MHz_VER004;
		break;
	case E_LINK_CHPI_10BIT_12PAIR:
		if (ldHz < 300 * MHZ) {
			idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_300to300MHz_VER004;
		} else {
			if (pctx->v_cfg.timing == E_4K1K_144HZ)
				idx =
				E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_4K1K144_250to360MHz_VER004;
			else
				idx = E_PNL_SUPPORTED_LPLL_CHPI_10bit_12x1_300to600MHz_VER004;
		}
		break;
	case E_LINK_CHPI_10BIT_6PAIR:
		if (ldHz < 300 * MHZ)
			idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_300to300MHz_VER004;
		else
			idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_300to600MHz_VER004;
		break;
	case E_LINK_LVDS:
		idx = E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER004;
		break;
	case E_LINK_VB1:
		switch (pctx->v_cfg.timing) {
		case E_4K2K_60HZ:
		case E_4K1K_120HZ:
			if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_10) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_300to600MHz_VER004;
			} else if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_8) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_450to600MHz_VER004;
			} else {
				pr_err("%s: link use VB1-4k2k60hz but dither undef\n", __func__);
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_300to600MHz_VER004;
			}
			break;
		case E_4K2K_30HZ:
		case E_FHD_120HZ:
			if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_10) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_4_lane_300to600MHz_VER004;
			} else if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_8) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_4_lane_300to450MHz_VER004;
			} else {
				DRM_ERROR("%s: link use VB1-FHD120hz but dither undef\n", __func__);
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_4_lane_300to600MHz_VER004;
			}
			break;
		case E_FHD_60HZ:
			if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_10) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_300to300MHz_VER004;
			} else if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_8) {
				idx = E_PNL_SUPPORTED_LPLL_Vx1_8bit_2_lane_300to300MHz_VER004;
			} else {
				DRM_ERROR("%s: link use VB1-FHD60hz but dither undef\n", __func__);
				idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_2_lane_300to300MHz_VER004;
			}
			break;
		case E_4K1K_144HZ:
			if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_10) {
				idx =
				E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K1K144_180to360MHz_VER004;
			} else if (pctx->dither_info.dither_depth == E_DITHER_DEPTH_8) {
				idx =
				E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1K144_200to360MHz_VER004;
			} else {
				DRM_ERROR("%s: link use VB1-4k1k144hz but dither undef\n",
						__func__);
				idx =
				E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K1K144_180to360MHz_VER004;
			}
			break;
		default:
			idx = E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_300to600MHz_VER004;
			break;
		}
		break;
	default:
		DRM_ERROR("%s(%d): Link type Not Support\n", __func__, __LINE__);
		break;
	}

	return idx;
}

static struct mtk_lpll_type _get_link_idx(struct mtk_panel_context *pctx)
{
	en_drv_pnl_version lpll_version = DRV_PNL_VERSION0100;
	struct mtk_lpll_type lpll_name;
	struct mtk_lpll_table_info lpll_tbl_info;

	lpll_name.lpll_tbl_idx = E_PNL_SUPPORTED_LPLL_MAX;
	lpll_name.lpll_type = E_PNL_SUPPORTED_LPLL_MAX;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		lpll_name.lpll_tbl_idx = E_PNL_SUPPORTED_LPLL_MAX;
		lpll_name.lpll_type = E_PNL_SUPPORTED_LPLL_MAX;
		return lpll_name;
	}

	lpll_version = drv_hwreg_render_pnl_get_lpllversion();

	switch (lpll_version) {
	case DRV_PNL_VERSION0100:
		if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz;
			lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz;
		} else if (pctx->v_cfg.linkIF == E_LINK_VB1) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz;
			lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz;
		} else {
			DRM_INFO("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		}
		break;
	case DRV_PNL_VERSION0200:
		_get_table_info(pctx, &lpll_tbl_info);

		if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
			lpll_name.lpll_tbl_idx =
					E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER2 -
					lpll_tbl_info.table_start;
			lpll_name.lpll_type =  E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER2;
		} else if (pctx->v_cfg.linkIF == E_LINK_VB1) {
			if (pctx->v_cfg.timing == E_4K2K_144HZ) {
				lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER2 -
					lpll_tbl_info.table_start;
				lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER2;
			} else {
				lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER2 -
					lpll_tbl_info.table_start;
				lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER2;
			}
		} else {
			DRM_INFO("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		}
		break;
	case DRV_PNL_VERSION0300:
		_get_table_info(pctx, &lpll_tbl_info);

		if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER0300 -
					lpll_tbl_info.table_start;
			lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER0300;
		} else if (pctx->v_cfg.linkIF == E_LINK_VB1) {
			if (pctx->v_cfg.timing == E_4K2K_144HZ) {
				lpll_name.lpll_tbl_idx =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_4K144_180to360MHz_VER0300 -
						lpll_tbl_info.table_start;

				lpll_name.lpll_type =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_4K144_180to360MHz_VER0300;
			} else if (pctx->v_cfg.timing == E_8K4K_144HZ) {
				lpll_name.lpll_tbl_idx =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_8K144_360to720MHz_VER0300 -
						lpll_tbl_info.table_start;

				lpll_name.lpll_type =
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_8K144_360to720MHz_VER0300;
			} else {
				lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER0300 -
						lpll_tbl_info.table_start;

				lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER0300;
			}
		} else {
			DRM_INFO("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		}
		break;
	case DRV_PNL_VERSION0203:
		_get_table_info(pctx, &lpll_tbl_info);

		if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER0203 -
				lpll_tbl_info.table_start;
			lpll_name.lpll_type = E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz_VER0203;
		} else if (pctx->v_cfg.linkIF == E_LINK_VB1) {
			if (pctx->v_cfg.timing == E_4K2K_144HZ) {
				lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER0203 -
					lpll_tbl_info.table_start;
				lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER0203;
			} else {
				lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER0203 -
					lpll_tbl_info.table_start;
				lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz_VER0203;
			}
		} else {
			DRM_INFO("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		}
		break;
	case DRV_PNL_VERSION0400:
	{
		E_PNL_SUPPORTED_LPLL_TYPE idx = __get_ver4_link_idx(pctx);

		_get_table_info(pctx, &lpll_tbl_info);
		lpll_name.lpll_tbl_idx = idx - lpll_tbl_info.table_start;
		lpll_name.lpll_type = idx;
		break;
	}
	case DRV_PNL_VERSION0500:
	{
		E_PNL_SUPPORTED_LPLL_TYPE idx = __get_ver5_link_idx(pctx);

		_get_table_info(pctx, &lpll_tbl_info);
		lpll_name.lpll_tbl_idx = idx - lpll_tbl_info.table_start;
		lpll_name.lpll_type = idx;
		break;
	}

	default:
		if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz;
			lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_LVDS_2ch_450to600MHz;
		} else if (pctx->v_cfg.linkIF == E_LINK_VB1) {
			lpll_name.lpll_tbl_idx =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz;

			lpll_name.lpll_type =
				E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_5BYTE_300to600MHz;
		} else {
			DRM_INFO("[%s][%d] Link type Not Support\n", __func__, __LINE__);
		}
		break;
	}

	return lpll_name;
}

static void _mtk_load_lpll_tbl(struct mtk_panel_context *pctx)
{
	uint32_t version = 0;
	uint16_t panelNum = 0;
	E_PNL_SUPPORTED_LPLL_TYPE tblidx = 0;
	struct mtk_lpll_table_info lpll_tbl_info;
	struct mtk_lpll_type lpll_name;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	version = pctx->hw_info.pnl_lib_version;

	lpll_name = _get_link_idx(pctx);
	tblidx = lpll_name.lpll_tbl_idx;

	_get_table_info(pctx, &lpll_tbl_info);
	panelNum = lpll_tbl_info.panel_num;

	DRM_INFO("[%s][%d] panel version = %d\n", __func__, __LINE__, version);
	DRM_INFO("[%s][%d] lpll_tbl_idx = %d\n", __func__, __LINE__, tblidx);
	DRM_INFO("[%s][%d] lpll_tbl_panelNum = %d\n", __func__, __LINE__, panelNum);

	// the version is useless
	drv_hwreg_render_pnl_load_lpll_tbl(version, tblidx, panelNum);
}

static void _mtk_load_mpll_tbl(struct mtk_panel_context *pctx)
{
	uint32_t version = 0;
	uint16_t regNum = 0;
	uint16_t panelNum = 0;
	E_PNL_SUPPORTED_LPLL_TYPE tblidx = 0;
	struct mtk_lpll_table_info lpll_tbl_info;
	struct mtk_lpll_type lpll_name;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	version = pctx->hw_info.pnl_lib_version;

	lpll_name = _get_link_idx(pctx);
	tblidx = lpll_name.lpll_tbl_idx;

	_get_table_info(pctx, &lpll_tbl_info);
	regNum = lpll_tbl_info.mpll_reg_num;
	panelNum = lpll_tbl_info.panel_num;

	DRM_INFO("[%s][%d] panel ver = %d\n", __func__, __LINE__, version);
	DRM_INFO("[%s][%d] mpll_tbl_idx = %d\n", __func__, __LINE__, tblidx);
	DRM_INFO("[%s][%d] mpll_tbl_regNum = %d\n", __func__, __LINE__, regNum);
	DRM_INFO("[%s][%d] mpll_tbl_panelNum = %d\n", __func__, __LINE__, panelNum);

	// the version is useless
	drv_hwreg_render_pnl_load_mpll_tbl(version, tblidx, regNum, panelNum);
}

static void _mtk_load_moda_tbl(struct mtk_panel_context *pctx)
{
	uint32_t version = 0;
	uint16_t regNum = 0;
	uint16_t panelNum = 0;
	E_PNL_SUPPORTED_LPLL_TYPE tblidx = 0;
	struct mtk_lpll_table_info lpll_tbl_info;
	struct mtk_lpll_type lpll_name;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	version = pctx->hw_info.pnl_lib_version;

	lpll_name = _get_link_idx(pctx);
	tblidx = lpll_name.lpll_tbl_idx;

	_get_table_info(pctx, &lpll_tbl_info);
	regNum = lpll_tbl_info.moda_reg_num;
	panelNum = lpll_tbl_info.panel_num;

	DRM_INFO("[%s][%d] panel ver = %d\n", __func__, __LINE__, version);
	DRM_INFO("[%s][%d] moda_tbl_idx = %d\n", __func__, __LINE__, tblidx);
	DRM_INFO("[%s][%d] moda_tbl_regNum = %d\n", __func__, __LINE__, regNum);
	DRM_INFO("[%s][%d] moda_tbl_panelNum = %d\n", __func__, __LINE__, panelNum);

	// the version is useless
	drv_hwreg_render_pnl_load_moda_tbl(version, tblidx, regNum, panelNum);
}

static void _mtk_odclk_setting(struct mtk_panel_context *pctx)
{
	uint32_t pnl_ver = 0;
	uint32_t odclk = 0;
	E_PNL_SUPPORTED_LPLL_TYPE tblidx = 0;
	E_PNL_SUPPORTED_LPLL_TYPE lpll_enum = 0;
	struct mtk_lpll_type lpll_name;
	drm_pnl_link_if linkIF;
	drm_output_mode timing;
	uint32_t p = P_1000;
	uint64_t refresh = 0;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pnl_ver = pctx->hw_info.pnl_lib_version;

	lpll_name = _get_link_idx(pctx);
	tblidx = lpll_name.lpll_tbl_idx;
	lpll_enum = lpll_name.lpll_type;
	linkIF = pctx->v_cfg.linkIF;
	timing = pctx->v_cfg.timing;

	DRM_INFO("[%s][%d] odclk_tbl_idx = %d\n", __func__, __LINE__, tblidx);
	DRM_INFO("[%s][%d] odclk_tbl_enum = %d\n", __func__, __LINE__, lpll_enum);
	DRM_INFO("[%s][%d] panel lib version = %d\n", __func__, __LINE__, pnl_ver);
	DRM_INFO("[%s][%d] linkIF=%d timing=%d\n", __func__, __LINE__, linkIF, timing);

	if (!(pctx->info.typ_htt && pctx->info.typ_vtt)) {
		DRM_ERROR("[%s, %d]: Panel info is error.\n",
			__func__, __LINE__);
		return;
	}

	refresh = (pctx->info.typ_dclk / pctx->info.typ_htt / pctx->info.typ_vtt);

	if (linkIF == E_LINK_VB1 || linkIF == E_LINK_LVDS) {
		if (pctx->v_cfg.lanes == LANE_NUM_2)
			p = P_250; //2k60
		else if (pctx->v_cfg.lanes == LANE_NUM_4)
			p = P_500; //2k120
		else if (pctx->v_cfg.lanes == LANE_NUM_8)
			p = P_1000; //4k60
		else if (pctx->v_cfg.lanes == LANE_NUM_16) {
			if (pctx->v_cfg.format == E_OUTPUT_YUV422)
				p = P_4000; //8k60_yuv422
			else
				p = P_2000; //4k120 & 4K144
			}
		else if (pctx->v_cfg.lanes == LANE_NUM_32)
			p = P_4000; //8k60
		else if (pctx->v_cfg.lanes == LANE_NUM_64)
			p = P_8000; //8k120 & 8K144

		switch (pnl_ver) {
		case DRV_PNL_VERSION0300:
			if (pctx->v_cfg.lanes == LANE_NUM_16) {
				if ((refresh == REFRESH_144) ||
					(pctx->v_cfg.format == E_OUTPUT_YUV422))
					p = P_4000; //4K144 & 8K60_YUV422
				else
					p = P_2000; //4K120
			}
			break;
		case DRV_PNL_VERSION0400:
		case DRV_PNL_VERSION0500:
			if (pctx->v_cfg.lanes == LANE_NUM_8) {
				if (refresh == REFRESH_144)
					p = P_2000; //4k1k144
				else
					p = P_1000; //4K60
			}
			break;
		}
	}  else {
		/* TCON-less case
		 * FHD	TCON-less case: not support now
		 */
		switch (timing) {
		case E_4K2K_144HZ:
		case E_4K2K_120HZ:
		case E_4K1K_144HZ:
		case E_4K1K_240HZ:
			p = P_2000;
			break;
		default:
			p = P_1000;
			break;
		}
	}

	odclk = pctx->info.typ_dclk / p; //(odclk KHz)
/*
	odclk = MOD_TOP_CLK;
	if ((version == LPLL_VER_2) || (version == LPLL_VER_203)) {
		if ((lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_3BYTE_OSD_3BYTE_340to720MHz_VER2) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_3BYTE_OSD_4BYTE_340to720MHz_VER2) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER2) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_340to720MHz_VER2) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_8K144_360to720MHz_VER0300) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_3BYTE_OSD_3BYTE_340to720MHz_VER0203) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_3BYTE_OSD_4BYTE_340to720MHz_VER0203) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_3BYTE_340to720MHz_VER0203) ||
			(lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_Video_4BYTE_OSD_4BYTE_340to720MHz_VER0203)) {
			odclk = MOD_TOP_CLK_720MHZ;
		}
	}

	if (pnl_ver == DRV_PNL_VERSION0400) {
		if ((lpll_enum ==
			E_PNL_SUPPORTED_LPLL_Vx1_10bit_8_lane_4K1K144_180to360MHz_VER004) ||
		(lpll_enum == E_PNL_SUPPORTED_LPLL_Vx1_8bit_8_lane_4K1K144_200to360MHz_VER004))
			odclk = MOD_TOP_CLK_360MHZ;
	}
*/
	pr_err("odclk_p = %d\n", p);
	pr_err("odclk = %dKHz\n", odclk);
	pr_err("htt = %d\n", pctx->info.typ_htt);
	pr_err("vtt = %d\n", pctx->info.typ_vtt);
	pr_err("lanes = %d\n", pctx->v_cfg.lanes);
	pr_err("refresh = %llu\n", pctx->info.typ_dclk / pctx->info.typ_htt / pctx->info.typ_vtt);
	// the version is useless
	drv_hwreg_render_pnl_set_odclk(pnl_ver, tblidx, odclk);
}

//bring up end

static void _mtk_moda_path_setting(struct mtk_panel_context *pctx)
{
	uint32_t pnl_ver;
	bool path1EN = false;
	bool path2EN = false;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pnl_ver = pctx->hw_info.pnl_lib_version;

	// only support mod version = 2 or panel version = 4
	if (pnl_ver != DRV_PNL_VERSION0200 &&
		pnl_ver != DRV_PNL_VERSION0203 &&
		pnl_ver != DRV_PNL_VERSION0400 &&
		pnl_ver != DRV_PNL_VERSION0500)
		return;

	if (pctx->v_cfg.linkIF == E_LINK_VB1) {
		if ((pctx->info.vbo_byte == E_VBO_5BYTE_MODE) &&
				(pctx->ext_grpah_combo_info.graph_vbo_byte_mode ==
					E_VBO_5BYTE_MODE))
			path1EN = false;
		else
			path1EN = true;

		if ((pctx->info.vbo_byte == E_VBO_5BYTE_MODE) ||
				(pctx->ext_grpah_combo_info.graph_vbo_byte_mode ==
						E_VBO_5BYTE_MODE))
			path2EN = true;
		else
			path2EN = false;

	} else {
		path1EN = true;
		path2EN = false;
	}

	drv_hwreg_render_pnl_analog_path_setting(path1EN, path2EN);
}

static struct mtk_drm_panel_context *_get_gfx_panel_ctx(void)
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

static struct mtk_drm_panel_context *_get_extdev_panel_ctx(void)
{
	struct device_node *deviceNodePanel = NULL;
	struct drm_panel *pDrmPanel = NULL;
	struct mtk_drm_panel_context *pStPanelCtx = NULL;

	deviceNodePanel = of_find_node_by_name(NULL, EXTDEV_NODE);
	if (deviceNodePanel != NULL && of_device_is_available(deviceNodePanel)) {
		pDrmPanel = of_drm_find_panel(deviceNodePanel);
		pStPanelCtx = container_of(pDrmPanel, struct mtk_drm_panel_context, drmPanel);
	}
	return pStPanelCtx;
}

static void _mtk_cfg_ckg_common(struct mtk_panel_context *pctx)
{
	if (pctx == NULL)
		return;

	if (pctx->v_cfg.linkIF != E_LINK_NONE) {
		//clock setting
		drv_hwreg_render_pnl_set_cfg_ckg_common();
	}
}

void _vb1_set_mft(struct mtk_panel_context *pctx)
{
	#define H_DIVIDER_FOR_YUV422 2
	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (pctx->v_cfg.format == E_OUTPUT_YUV422) {
		drv_hwreg_render_pnl_set_vby1_mft(
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			pctx->v_cfg.div_sec,
			pctx->v_cfg.lanes,
			(pctx->info.de_width)/H_DIVIDER_FOR_YUV422);
	} else {
		drv_hwreg_render_pnl_set_vby1_mft(
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			pctx->v_cfg.div_sec,
			pctx->v_cfg.lanes,
			pctx->info.de_width);
	}
	#undef H_DIVIDER_FOR_YUV422
}

void _vb1_v_8k_setting(struct mtk_panel_context *pctx)
{
	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (pctx->v_cfg.format == E_OUTPUT_YUV422)
		drv_hwreg_render_pnl_set_vby1_v_8k422_60HZ();
	else
		drv_hwreg_render_pnl_set_vby1_v_8k_60HZ();
}

void _vb1_seri_data_sel(struct mtk_panel_context *pctx)
{
	__u8 u8GfxLanes = 0;
	__u8 u8ExtdevLanes = 0;
	struct mtk_drm_panel_context *pStPanelCtx = NULL;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pStPanelCtx = _get_gfx_panel_ctx();
	if (pStPanelCtx != NULL)
		u8GfxLanes = pStPanelCtx->cfg.lanes;

	pStPanelCtx = _get_extdev_panel_ctx();
	if (pStPanelCtx != NULL)
		u8ExtdevLanes = pStPanelCtx->cfg.lanes;

	if (pctx->v_cfg.linkIF == E_LINK_LVDS)
		drv_hwreg_render_pnl_set_vby1_seri_data_sel(0x0);

	if (pctx->v_cfg.lanes == 32)
		drv_hwreg_render_pnl_set_vby1_seri_data_sel(0x1);

	if (pctx->v_cfg.lanes == 16 || pctx->v_cfg.lanes == 8 ||
		(u8ExtdevLanes == 16 &&
		u8GfxLanes == 8)) {
		drv_hwreg_render_pnl_set_vby1_seri_data_sel(0x2);
	}

	if (pctx->v_cfg.lanes == 64)
		drv_hwreg_render_pnl_set_vby1_seri_data_sel(0x3);
}

drm_en_vbo_bytemode _vb1_set_byte_mode_decision(struct mtk_panel_context *pctx)
{
	uint32_t pnl_ver = 0;
	drm_en_vbo_bytemode ret = E_VBO_NO_LINK;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return ret;
	}

	pnl_ver = pctx->hw_info.pnl_lib_version;

	if (pnl_ver == DRV_PNL_VERSION0200 ||
		pnl_ver == DRV_PNL_VERSION0203 ||
		pnl_ver == DRV_PNL_VERSION0400 ||
		pnl_ver == DRV_PNL_VERSION0500) {
		ret = pctx->info.vbo_byte;
	} else {
		if ((pctx->v_cfg.timing == E_8K4K_60HZ) &&
				(pctx->v_cfg.format == E_OUTPUT_YUV422)) {
			ret = E_VBO_5BYTE_MODE;
		} else {
			ret = E_VBO_4BYTE_MODE;
		}
	}
	return ret;
}

void _vb1_set_byte_mode(struct mtk_panel_context *pctx)
{
	drm_en_vbo_bytemode ret = _vb1_set_byte_mode_decision(pctx);

	drv_hwreg_render_pnl_set_vby1_byte_mode((en_drv_pnl_vbo_bytemode)ret);
}

static void _mtk_vby1_setting(struct mtk_panel_context *pctx)
{
	uint32_t pnl_ver = 0;
	uint8_t output_format = 0;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (pctx->cus.tcon_en) {
		DRM_INFO("Tcon related mod bank settings are set in tcon bin\n");
		return;
	}

	pnl_ver = pctx->hw_info.pnl_lib_version;
	output_format = (uint8_t)pctx->v_cfg.format;

	DRM_INFO("[%s][%d] pctx->v_cfg.linkIF= %d pctx->v_cfg.timing =%d\n",
			__func__, __LINE__, pctx->v_cfg.linkIF, pctx->v_cfg.timing);

	if (pctx->v_cfg.linkIF != E_LINK_NONE) {
		switch (pctx->v_cfg.timing) {

		case E_FHD_60HZ:
			if (pctx->v_cfg.linkIF == E_LINK_LVDS)
				drv_hwreg_render_pnl_set_lvds_v_fhd_60HZ(pnl_ver);
			else
				drv_hwreg_render_pnl_set_vby1_v_fhd_60HZ(pnl_ver);
		break;
		case E_FHD_120HZ:
			drv_hwreg_render_pnl_set_vby1_v_fhd_120HZ(pnl_ver);
		break;
		case E_4K2K_60HZ:
		case E_4K1K_120HZ:
			drv_hwreg_render_pnl_set_vby1_v_4k_60HZ(pnl_ver);
		break;
		case E_4K2K_120HZ:
		case E_4K2K_144HZ:
		case E_4K1K_240HZ:
			drv_hwreg_render_pnl_set_vby1_v_4k_120HZ(pnl_ver);
		break;
		case E_8K4K_60HZ:
			_vb1_v_8k_setting(pctx);
		break;
		case E_8K4K_120HZ:
			drv_hwreg_render_pnl_set_vby1_v_8k_120HZ();
		break;
		case E_8K4K_144HZ:
			drv_hwreg_render_pnl_set_vby1_v_8k_144HZ();
		break;
		default:
			DRM_INFO("[%s][%d] Video OUTPUT TIMING Not Support\n", __func__, __LINE__);
		break;
		}

		//ToDo:Add Fixed H-backporch code.

		//VBY1 byte-mode
		_vb1_set_byte_mode(pctx);
	}

	DRM_INFO("[%s][%d]pctx->v_cfg.linkIF = %d\n", __func__, __LINE__, pctx->v_cfg.linkIF);

	_vb1_set_mft(pctx);
	//set up pre-order and YUV by-pass
	drv_hwreg_render_pnl_set_sctcon_misc(output_format);

	//serializer data sel
	if (pctx->v_cfg.linkIF == E_LINK_LVDS)
		drv_hwreg_render_pnl_set_vby1_seri_data_sel(0x0);
	else
		_vb1_seri_data_sel(pctx);
}

void _mtk_vby1_lane_order(struct mtk_panel_context *pctx)
{
	uint32_t hwversion = 0;
	st_drv_out_lane_order stPnlOutLaneOrder = { };

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	hwversion = pctx->hw_info.pnl_lib_version;
	memcpy(&stPnlOutLaneOrder, &pctx->out_lane_info, sizeof(st_drv_out_lane_order));

	if (pctx->v_cfg.linkIF == E_LINK_VB1) {
		drv_hwreg_render_pnl_set_vby1_lane_order(
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			(en_drv_pnl_output_mode)pctx->v_cfg.timing,
			(en_drv_pnl_output_format)pctx->v_cfg.format,
			pctx->v_cfg.div_sec,
			pctx->v_cfg.lanes,
			&stPnlOutLaneOrder);
	}

	if (pctx->v_cfg.linkIF == E_LINK_LVDS) {
		drv_hwreg_render_pnl_set_lvds_lane_order(hwversion, 0x0);
	}
}

void mtk_render_output_en(struct mtk_panel_context *pctx, bool bEn)
{
	uint32_t hwversion = 0;
	bool bDuplicateEn = false;
	st_drv_out_lane_order stPnlOutLaneOrder = { };

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
				__func__, __LINE__);
		return;
	}

	if (pctx->v_cfg.linkIF == E_LINK_NONE)
		return;


	if (pctx->cus.tcon_en) {
		//The order required by the general panel should be load pmic first
		//and then load tcon bin power on
		mtk_tcon_enable(pctx, bEn);
		DRM_INFO("Tcon related output config settings are set in tcon bin...'%s'\n",
					bEn ? "ON" : "OFF");

		if (bEn == true)
			pctx->bVby1LocknProtectEn = true;
		return;
	}

	memcpy(&stPnlOutLaneOrder, &pctx->out_lane_info, sizeof(st_drv_out_lane_order));
	bDuplicateEn = pctx->lane_duplicate_en;
	hwversion = pctx->hw_info.pnl_lib_version;
	//video
	drv_hwreg_render_pnl_set_render_output_en_video(
			hwversion,
			bEn,
			bDuplicateEn,
			(en_drv_pnl_output_mode)pctx->v_cfg.timing,
			(en_drv_pnl_output_format)pctx->v_cfg.format,
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			&stPnlOutLaneOrder);
	// if output config enable, this protect will enable
	if (bEn == true)
		pctx->bVby1LocknProtectEn = true;
}

int mtk_render_set_gop_pattern_en(bool bEn)
{
	return drv_hwreg_render_pnl_set_gop_pattern(bEn);
}

int mtk_render_set_multiwin_pattern_en(bool bEn)
{
	return drv_hwreg_render_pnl_set_multiwin_pattern(bEn);
}

int mtk_render_set_pafrc_post_pattern_en(bool bEn)
{
	return drv_hwreg_render_pnl_set_pafrc_post_pattern(bEn);
}

int mtk_render_set_sec_pattern_en(bool bEn)
{
	return drv_hwreg_render_pnl_set_sec_pattern(bEn);
}

int mtk_render_set_tcon_pattern_en(bool bEn)
{
	return drv_hwreg_render_pnl_set_tcon_pattern(bEn);
}

int mtk_render_set_tcon_blackpattern(bool bEn)
{
	return drv_hwreg_render_pnl_set_tcon_blackpattern(bEn);
}

int mtk_render_set_output_hmirror_enable(struct mtk_panel_context *pctx)
{
	bool hmirror_en;
	bool setup_gfx = false;
	bool setup_deltav = false;	// M6 extension
	uint32_t pnl_ver;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	hmirror_en = pctx->cus.hmirror_en;
	pnl_ver = pctx->hw_info.pnl_lib_version;

	switch (pnl_ver) {
	case DRV_PNL_VERSION0100:
		setup_deltav = true;
		setup_gfx = true;
		break;
	case DRV_PNL_VERSION0200:
		setup_gfx = true;
		setup_deltav = false;
		break;
	case DRV_PNL_VERSION0203:
		setup_gfx = true;
		setup_deltav = false;
		break;
	case DRV_PNL_VERSION0300:
		setup_deltav = true;
		setup_gfx = true;
		break;
	case DRV_PNL_VERSION0400:
	case DRV_PNL_VERSION0500:
	default:
		setup_deltav = false;
		setup_gfx = false;
		break;
	}

	// setup
	drv_hwreg_render_pnl_set_vby1_mft_hmirror_video(hmirror_en);
	if (setup_deltav)
		drv_hwreg_render_pnl_set_vby1_mft_hmirror_deltav(hmirror_en);
	if (setup_gfx)
		drv_hwreg_render_pnl_set_vby1_mft_hmirror_gfx(hmirror_en);

	// check
	if (drv_hwreg_render_pnl_get_vby1_mft_hmirror_video() != hmirror_en)
		return -EINVAL;

	if (setup_deltav &&
		drv_hwreg_render_pnl_get_vby1_mft_hmirror_deltav() != hmirror_en)
		return -EINVAL;

	if (setup_gfx &&
		drv_hwreg_render_pnl_get_vby1_mft_hmirror_gfx() != hmirror_en)
		return -EINVAL;

	DRM_INFO("[%s, %d]: Mirror mode set done\n", __func__, __LINE__);

	return 0;
}

void mtk_render_set_pnl_init_done(bool bEn)
{
	bPnlInitDone = bEn;
}

bool mtk_render_get_pnl_init_done(void)
{
	return bPnlInitDone;
}

static int _mtk_tgen_VG_ratio(drm_output_mode out_timing)
{
	//For main video_out 120hz, GFX 60HZ output case,
	//need to reset gfx every 2 vsync

	struct hwregOut paramOut;
	struct reg_info reg[1];
	int ret = 0;
	bool bRIU = true, bEnable = 0;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	if ((out_timing == E_4K2K_120HZ) ||
		(out_timing == E_4K2K_144HZ))
		bEnable = 1;
	else
		bEnable = 0;

	DRM_INFO("[%s, %d]: out_timing:%d, bEnable:%d\n", __func__, __LINE__, out_timing, bEnable);

	paramOut.reg = reg;
	//enable GOP ref mask
	//BKA3A3_h13[0] = enable
	ret = drv_hwreg_render_pnl_tgen_set_gop_ref_mask(
		&paramOut,
		bRIU,
		bEnable);

	return ret;

}


static void _mtk_tgen_init(struct mtk_panel_context *pctx, uint8_t out_clk_if)
{
	st_drv_pnl_info stPnlInfoTmp = {0};
	uint32_t pnl_ver = 0;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	//reset to freerun mode before configuring tgen
	drv_hwreg_render_pnl_set_framesync_freerun_mode();
	/*
	 * consider the dlg mode, to avoid tgen lock
	 * the previous ivs/ovs lead to report the
	 * wrong vtt, should reset the odclk
	 */
	drv_hwreg_render_pnl_reset_vttv_cnt();

	pnl_ver = pctx->hw_info.pnl_lib_version;

	stPnlInfoTmp.version = pctx->info.version;
	stPnlInfoTmp.length = pctx->info.length;

	strncpy(stPnlInfoTmp.pnlname, pctx->info.pnlname, DRV_PNL_NAME_MAX_NUM-1);

	stPnlInfoTmp.linkIf = (en_drv_pnl_link_if)pctx->info.linkIF;
	stPnlInfoTmp.div_section = pctx->info.div_section;

	stPnlInfoTmp.ontiming_1 = pctx->info.ontiming_1;
	stPnlInfoTmp.ontiming_2 = pctx->info.ontiming_2;
	stPnlInfoTmp.offtiming_1 = pctx->info.offtiming_1;
	stPnlInfoTmp.offtiming_2 = pctx->info.offtiming_2;

	stPnlInfoTmp.hsync_st = pctx->info.hsync_st;
	stPnlInfoTmp.hsync_w = pctx->info.hsync_w;
	stPnlInfoTmp.hsync_pol = pctx->info.hsync_pol;

	stPnlInfoTmp.vsync_st = pctx->info.vsync_st;
	stPnlInfoTmp.vsync_w = pctx->info.vsync_w;
	stPnlInfoTmp.vsync_pol = pctx->info.vsync_pol;

	stPnlInfoTmp.de_hstart = pctx->info.de_hstart;
	stPnlInfoTmp.de_vstart = pctx->info.de_vstart;
	stPnlInfoTmp.de_width = pctx->info.de_width;
	stPnlInfoTmp.de_height = pctx->info.de_height;

	stPnlInfoTmp.max_htt = pctx->info.max_htt;
	stPnlInfoTmp.typ_htt = pctx->info.typ_htt;
	stPnlInfoTmp.min_htt = pctx->info.min_htt;

	stPnlInfoTmp.max_vtt = pctx->info.max_vtt;
	stPnlInfoTmp.typ_vtt = pctx->info.typ_vtt;
	stPnlInfoTmp.min_vtt = pctx->info.min_vtt;

	stPnlInfoTmp.max_vtt_panelprotect = pctx->tgen_info.pnl_tgen_max_vtotal;
	stPnlInfoTmp.min_vtt_panelprotect = pctx->tgen_info.pnl_tgen_max_vtotal;

	stPnlInfoTmp.typ_dclk = pctx->info.typ_dclk;

	stPnlInfoTmp.op_format = (en_drv_pnl_output_format)pctx->info.op_format;

	stPnlInfoTmp.vrr_en = pctx->info.vrr_en;
	stPnlInfoTmp.vrr_max_v = pctx->info.vrr_max_v;
	stPnlInfoTmp.vrr_min_v = pctx->info.vrr_min_v;

	stPnlInfoTmp.lane_num = out_clk_if;

	drv_hwreg_render_pnl_tgen_init(pnl_ver, &stPnlInfoTmp);

	if (pctx->out_upd_type == E_MODE_RESUME_TYPE)
		drv_hwreg_render_pnl_tgen_reset_de(&stPnlInfoTmp);

	_mtk_tgen_VG_ratio((drm_output_mode)pctx->v_cfg.timing);
}
static void _mtk_mft_hbkproch_protect_init(struct mtk_panel_context *pctx)
{
	st_drv_hprotect_info stHprotectInfoTmp = {0};
	st_drv_parse_out_info_from_dts_ans sthbproch_protect_ans;

	if (pctx->cus.tcon_en) {
		DRM_INFO("Tcon related H backporch settings are set in tcon bin\n");
		return;
	}

	stHprotectInfoTmp.hsync_w =  pctx->info.hsync_w;
	stHprotectInfoTmp.de_hstart =  pctx->info.de_hstart;
	stHprotectInfoTmp.lane_numbers = pctx->v_cfg.lanes;
	stHprotectInfoTmp.typ_htt = pctx->info.typ_htt;
	stHprotectInfoTmp.de_width = pctx->info.de_width;
	stHprotectInfoTmp.op_format = (en_drv_pnl_output_format)pctx->info.op_format;
	sthbproch_protect_ans.verify_enable = false;

	drv_hwreg_render_pnl_hbkproch_protect_init(&stHprotectInfoTmp, &sthbproch_protect_ans);

}

static void _dump_mod_cfg(struct mtk_panel_context *pctx)
{
	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	DRM_INFO("================V_MOD_CFG================\n");
	dump_mod_cfg(&pctx->v_cfg);
}

static void _init_mod_cfg(struct mtk_panel_context *pctx)
{
	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	init_mod_cfg(&pctx->v_cfg);
}

int mtk_tv_drm_check_out_mod_cfg(struct mtk_panel_context *pctx_pnl,
						drm_parse_dts_info *data)
{
	drm_mod_cfg out_cfg = {0};

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] param is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&out_cfg, 0, sizeof(drm_mod_cfg));

	set_out_mod_cfg(&data->src_info, &out_cfg);

	DRM_INFO("[%s] calculated out_cfg.lanes is %d target = %d\n"
			, __func__, out_cfg.lanes, data->out_cfg.lanes);
	DRM_INFO("[%s] calculated out_cfg.timing is %d target = %d\n"
			, __func__, out_cfg.timing, data->out_cfg.timing);

	if (out_cfg.lanes == data->out_cfg.lanes &&
		out_cfg.timing == data->out_cfg.timing) {

		return 0;
	} else {

		return -1;
	}

}
EXPORT_SYMBOL(mtk_tv_drm_check_out_mod_cfg);

int mtk_tv_drm_check_out_lane_cfg(struct mtk_panel_context *pctx_pnl)
{
	uint32_t sup_lanes = 0;
	uint32_t def_layout = 0;
	uint32_t hw_read_layout = 0;
	uint8_t index = 0;
	int ret = 0;

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] param is NULL\n", __func__);
		return -EINVAL;
	}
	sup_lanes = pctx_pnl->out_lane_info.sup_lanes;

	for (index = 0; index < sup_lanes; index++) {
		def_layout = pctx_pnl->out_lane_info.def_layout[index];
		hw_read_layout = drv_hwreg_render_pnl_get_vby1_swap(index);
		if (def_layout != hw_read_layout) {
			ret = -1;
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(mtk_tv_drm_check_out_lane_cfg);

int mtk_tv_drm_check_video_hbproch_protect(drm_parse_dts_info *data,
	st_drv_parse_out_info_from_dts_ans *sthbproch_protect_ans)
{
	st_drv_hprotect_info tmpVideoHbkprotectInfo;
	st_drv_hprotect_info tmpDeltaHbkprotectInfo;
	st_drv_hprotect_info tmpGfxHbkprotectInfo;
	int ret = 0;

	tmpVideoHbkprotectInfo.hsync_w = data->video_hprotect_info.hsync_w;
	tmpVideoHbkprotectInfo.de_hstart = data->video_hprotect_info.de_hstart;
	tmpVideoHbkprotectInfo.lane_numbers = data->video_hprotect_info.lane_numbers;
	tmpVideoHbkprotectInfo.op_format =
		(en_drv_pnl_output_format)data->video_hprotect_info.op_format;
	tmpDeltaHbkprotectInfo.hsync_w = data->delta_hprotect_info.hsync_w;
	tmpDeltaHbkprotectInfo.de_hstart = data->delta_hprotect_info.de_hstart;
	tmpDeltaHbkprotectInfo.lane_numbers = data->delta_hprotect_info.lane_numbers;
	tmpDeltaHbkprotectInfo.op_format =
		(en_drv_pnl_output_format)data->delta_hprotect_info.op_format;

	tmpGfxHbkprotectInfo.hsync_w = data->gfx_hprotect_info.hsync_w;
	tmpGfxHbkprotectInfo.de_hstart = data->gfx_hprotect_info.de_hstart;
	tmpGfxHbkprotectInfo.lane_numbers = data->gfx_hprotect_info.lane_numbers;
	tmpGfxHbkprotectInfo.op_format =
		(en_drv_pnl_output_format)data->gfx_hprotect_info.op_format;

	ret |= drv_hwreg_render_pnl_hbkproch_protect_init(
		&tmpVideoHbkprotectInfo, sthbproch_protect_ans);
	ret |= drv_hwreg_render_pnl_delta_hbkproch_protect_init(
		&tmpDeltaHbkprotectInfo, sthbproch_protect_ans);
	ret |= drv_hwreg_render_pnl_gfx_hbkproch_protect_init(
		&tmpGfxHbkprotectInfo, sthbproch_protect_ans);

	return ret;
}
EXPORT_SYMBOL(mtk_tv_drm_check_video_hbproch_protect);

int mtk_render_set_tx_mute(struct mtk_panel_context *pctx_pnl,
							drm_st_tx_mute_info tx_mute_info)
{
	#define PNL_V1 0
	#define PNL_V2 1
	#define PNL_OSD 2
	int Ret = -EINVAL;
	bool bReadBack = 0;
	uint32_t pnl_ver = 0;

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] param is NULL\n", __func__);
		return -EINVAL;
	}

	pnl_ver = pctx_pnl->hw_info.pnl_lib_version;

	if (tx_mute_info.u8ConnectorID == PNL_V1) {
		//video
		Ret = drv_hwreg_render_pnl_set_tx_mute_en_video(tx_mute_info.bEnable);
		bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_video();
		DRM_INFO("%s V1:%d\n", __func__, bReadBack);
	}

	if (pnl_ver == DRV_PNL_VERSION0200 ||
		pnl_ver == DRV_PNL_VERSION0203 ||
		pnl_ver == DRV_PNL_VERSION0400 ||
		pnl_ver == DRV_PNL_VERSION0500) {
		// not support
		Ret = 0;
	} else {
		if (tx_mute_info.u8ConnectorID == PNL_V2) {
			//deltav
			Ret = drv_hwreg_render_pnl_set_tx_mute_en_deltav(tx_mute_info.bEnable);
			bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_deltav();
			DRM_INFO("%s V2:%d\n", __func__, bReadBack);
		}
	}

	if (tx_mute_info.u8ConnectorID == PNL_OSD) {
		//gfx
		Ret = drv_hwreg_render_pnl_set_tx_mute_en_gfx(tx_mute_info.bEnable);
		bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_gfx();
		DRM_INFO("%s OSD:%d\n", __func__, bReadBack);
	}

	if ((pnl_ver != DRV_PNL_VERSION0200 &&
		pnl_ver != DRV_PNL_VERSION0203 &&
		pnl_ver != DRV_PNL_VERSION0400 &&
		pnl_ver != DRV_PNL_VERSION0500) || (tx_mute_info.u8ConnectorID != PNL_V2)) {
		if (tx_mute_info.bEnable != bReadBack) {
			DRM_ERROR("[%s, %d]: TX mute is incorrect.\n",
				__func__, __LINE__);
			Ret = -EINVAL;
		}
	}

	#undef PNL_V1
	#undef PNL_V2
	#undef PNL_OSD

	return Ret;
}

int mtk_render_set_tx_mute_common(struct mtk_panel_context *pctx_pnl,
	drm_st_tx_mute_info tx_mute_info,
	uint8_t u8connector_id)
{
	#define PNL_V1 0
	#define PNL_V2 1
	#define PNL_OSD 2
	int Ret = -EINVAL;
	bool bReadBack = 0;
	uint32_t pnl_ver = 0;

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] param is NULL\n", __func__);
		return -EINVAL;
	}

	pnl_ver = pctx_pnl->hw_info.pnl_lib_version;

	if (u8connector_id == MTK_DRM_CONNECTOR_TYPE_VIDEO) {
		//video
		Ret = drv_hwreg_render_pnl_set_tx_mute_en_video(tx_mute_info.bEnable);
		bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_video();
		DRM_INFO("%s V1:%d\n", __func__, bReadBack);
	}

	if (pnl_ver == DRV_PNL_VERSION0200 ||
		pnl_ver == DRV_PNL_VERSION0203 ||
		pnl_ver == DRV_PNL_VERSION0400 ||
		pnl_ver == DRV_PNL_VERSION0500) {
		// not support
		Ret = 0;
	} else {
		if (u8connector_id == MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO) {
			//deltav
			Ret = drv_hwreg_render_pnl_set_tx_mute_en_deltav(tx_mute_info.bEnable);
			bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_deltav();
			DRM_INFO("%s V2:%d\n", __func__, bReadBack);
		}
	}

	if (u8connector_id == MTK_DRM_CONNECTOR_TYPE_GRAPHIC) {
		//gfx
		Ret = drv_hwreg_render_pnl_set_tx_mute_en_gfx(tx_mute_info.bEnable);
		bReadBack = drv_hwreg_render_pnl_get_tx_mute_en_gfx();
		DRM_INFO("%s OSD:%d\n", __func__, bReadBack);
	}

	if ((pnl_ver != DRV_PNL_VERSION0200 &&
		pnl_ver != DRV_PNL_VERSION0203 &&
		pnl_ver != DRV_PNL_VERSION0400 &&
		pnl_ver != DRV_PNL_VERSION0500) ||
		(u8connector_id != MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO)) {
		if (tx_mute_info.bEnable != bReadBack) {
			DRM_ERROR("[%s, %d]: TX mute is incorrect.\n",
				__func__, __LINE__);
			Ret = -EINVAL;
		}
	}

	#undef PNL_V1
	#undef PNL_V2
	#undef PNL_OSD

	return Ret;
}

static void _ready_cb_pqu_render_pixel_mute(void)
{
	int ret = 0;

	DRM_INFO("pqu cpuif ready callback function\n");

	ret = pqu_render_pixel_mute(&_pixel_mute_msg_info[_CB_DEC(_pixel_mute_count)], NULL);
	if (ret != 0)
		DRM_ERROR("pqu_render_pixel_mute fail! (ret=%d)\n", ret);
}

static char *___strtok;

char *strtok_video_panel(char *s, const char *ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : ___strtok;
	if (!sbegin)
		return NULL;

	sbegin += strspn(sbegin, ct);
	if (*sbegin == '\0') {
		___strtok = NULL;
		return NULL;
	}
	send = strpbrk(sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';
	___strtok = send;
	return sbegin;
}

int mtk_video_panel_get_all_value_from_string(char *buf, char *delim,
	unsigned int len, uint32_t *value)
{
	char *token;
	char *endptr;

	if ((buf == NULL) || (delim == NULL) || (value == NULL)) {
		DRM_ERROR("[%s, %d]Pointer is NULL!\n", __func__, __LINE__);
		return -1;
	}

	token = strtok_video_panel(buf, (const char *)delim);
	while (token != NULL) {
		*value = strtoul(token, &endptr, 0);
		value++;
		token = strtok_video_panel(0, (const char *)delim);
	}

	return 0;
}

int mtk_render_set_pixel_mute_video(drm_st_pixel_mute_info pixel_mute_info)
{
	int Ret = -EINVAL;
	struct msg_render_pixel_mute msg_pixel_mute = { };
	struct pqu_render_pixel_mute pqu_pixel_mute = { };


	if (true == pixel_mute_info.bEnable) {
		msg_pixel_mute.bEnable = 1;
		pqu_pixel_mute.bEnable = 1;
	} else {
		msg_pixel_mute.bEnable = 0;
		pqu_pixel_mute.bEnable = 0;
	}

	if (true == pixel_mute_info.bLatchMode) {
		msg_pixel_mute.bLatchMode = 1;
		pqu_pixel_mute.bLatchMode = 1;
	} else {
		msg_pixel_mute.bLatchMode = 0;
		pqu_pixel_mute.bLatchMode = 0;
	}

	//VIDEO Path is MTK_DRM_CONNECTOR_TYPE_VIDEO (1)
	msg_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	msg_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	msg_pixel_mute.u32Blue       = pixel_mute_info.u32Blue;
	msg_pixel_mute.u8ConnectorID = MTK_DRM_CONNECTOR_TYPE_VIDEO;

	pqu_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	pqu_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	pqu_pixel_mute.u32Blue       = MTK_DRM_CONNECTOR_TYPE_VIDEO;
	pqu_pixel_mute.u8ConnectorID = 0;

	//PQ mute in PQU
	if (bPquEnable) { // RV55
		Ret = pqu_render_pixel_mute(
				(const struct pqu_render_pixel_mute *)&pqu_pixel_mute,
				NULL);
		if (Ret == -ENODEV) {
			memcpy(&_pixel_mute_msg_info[_CB_INC(_pixel_mute_count)],
				&pqu_pixel_mute,
				sizeof(struct pqu_render_pixel_mute));
			DRM_ERROR("pqu_render_pixel_mute register ready cb\n");
			Ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_render_pixel_mute);
		} else if (Ret != 0) {
			DRM_ERROR("pqu_render_pixel_mute fail (ret=%d)\n", Ret);
		}
		DRM_INFO("[%s, %d]: PQ Mute in RV55(PQU)\n", __func__, __LINE__);
		DRM_INFO("ret of pqu_render_pixel_mute: %d\n", Ret);
	} else { //
		pqu_msg_send(PQU_MSG_SEND_RENDER_PIXEL_MUTE, (void *)&msg_pixel_mute);
		DRM_INFO("[%s, %d]: PQ Mute in ARM(MSG)\n", __func__, __LINE__);
		Ret = 0;
	}
	return Ret;
}
EXPORT_SYMBOL(mtk_render_set_pixel_mute_video);

int mtk_render_set_pixel_mute_deltav(drm_st_pixel_mute_info pixel_mute_info)
{
	int Ret = -EINVAL;
	struct msg_render_pixel_mute msg_pixel_mute = { };
	struct pqu_render_pixel_mute pqu_pixel_mute = { };

	if (true == pixel_mute_info.bEnable) {
		msg_pixel_mute.bEnable = 1;
		pqu_pixel_mute.bEnable = 1;
	} else {
		msg_pixel_mute.bEnable = 0;
		pqu_pixel_mute.bEnable = 0;
	}

	if (true == pixel_mute_info.bLatchMode) {
		msg_pixel_mute.bLatchMode = 1;
		pqu_pixel_mute.bLatchMode = 1;
	} else {
		msg_pixel_mute.bLatchMode = 0;
		pqu_pixel_mute.bLatchMode = 0;
	}
	//DELTAV PATH is MTK_DRM_CONNECTOR_TYPE_GRAPHIC (1)
	msg_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	msg_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	msg_pixel_mute.u32Blue       = pixel_mute_info.u32Blue;
	msg_pixel_mute.u8ConnectorID = MTK_DRM_CONNECTOR_TYPE_GRAPHIC;

	pqu_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	pqu_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	pqu_pixel_mute.u32Blue       = pixel_mute_info.u32Blue;
	pqu_pixel_mute.u8ConnectorID = MTK_DRM_CONNECTOR_TYPE_GRAPHIC;

	//PQ mute in PQU
	if (bPquEnable) { // RV55
		Ret = pqu_render_pixel_mute(
				(const struct pqu_render_pixel_mute *)&pqu_pixel_mute,
				NULL);
		if (Ret == -ENODEV) {
			memcpy(&_pixel_mute_msg_info[_CB_INC(_pixel_mute_count)],
				&pqu_pixel_mute,
				sizeof(struct pqu_render_pixel_mute));
			DRM_ERROR("pqu_render_pixel_mute register ready cb\n");
			Ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_render_pixel_mute);
		} else if (Ret != 0) {
			DRM_ERROR("pqu_render_pixel_mute fail (ret=%d)\n", Ret);
		}
		DRM_INFO("[%s, %d]: PQ Mute in RV55(PQU)\n", __func__, __LINE__);
		DRM_INFO("ret of pqu_render_pixel_mute: %d\n", Ret);
	} else { //
		pqu_msg_send(PQU_MSG_SEND_RENDER_PIXEL_MUTE, (void *)&msg_pixel_mute);
		DRM_INFO("[%s, %d]: PQ Mute in ARM(MSG)\n", __func__, __LINE__);
		Ret = 0;
	}
	return Ret;
}
EXPORT_SYMBOL(mtk_render_set_pixel_mute_deltav);

int mtk_render_set_pixel_mute_gfx(drm_st_pixel_mute_info pixel_mute_info)
{
	int Ret = -EINVAL;
	struct msg_render_pixel_mute msg_pixel_mute = {0};
	struct pqu_render_pixel_mute pqu_pixel_mute = {0};

	if (true == pixel_mute_info.bEnable) {
		msg_pixel_mute.bEnable = 1;
		pqu_pixel_mute.bEnable = 1;
	} else {
		msg_pixel_mute.bEnable = 0;
		pqu_pixel_mute.bEnable = 0;
	}

	if (true == pixel_mute_info.bLatchMode) {
		msg_pixel_mute.bLatchMode = 1;
		pqu_pixel_mute.bLatchMode = 1;
	} else {
		msg_pixel_mute.bLatchMode = 0;
		pqu_pixel_mute.bLatchMode = 0;
	}
	//GFX PATH is MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO(2)
	msg_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	msg_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	msg_pixel_mute.u32Blue       = pixel_mute_info.u32Blue;
	msg_pixel_mute.u8ConnectorID = MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO;

	pqu_pixel_mute.u32Red        = pixel_mute_info.u32Red;
	pqu_pixel_mute.u32Green      = pixel_mute_info.u32Green;
	pqu_pixel_mute.u32Blue       = pixel_mute_info.u32Blue;
	pqu_pixel_mute.u8ConnectorID = MTK_DRM_CONNECTOR_TYPE_EXT_VIDEO;

	//PQ mute in PQU
	if (bPquEnable) { // RV55
		Ret = pqu_render_pixel_mute(
				(const struct pqu_render_pixel_mute *)&pqu_pixel_mute,
				NULL);
		if (Ret == -ENODEV) {
			memcpy(&_pixel_mute_msg_info[_CB_INC(_pixel_mute_count)],
				&pqu_pixel_mute,
				sizeof(struct pqu_render_pixel_mute));
			DRM_ERROR("pqu_render_pixel_mute register ready cb\n");
			Ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_render_pixel_mute);
		} else if (Ret != 0) {
			DRM_ERROR("pqu_render_pixel_mute fail (ret=%d)\n", Ret);
		}
		DRM_INFO("[%s, %d]: PQ Mute in RV55(PQU)\n", __func__, __LINE__);
		DRM_INFO("ret of pqu_render_pixel_mute: %d\n", Ret);
	} else { //ARM
		pqu_msg_send(PQU_MSG_SEND_RENDER_PIXEL_MUTE, (void *)&msg_pixel_mute);
		DRM_INFO("[%s, %d]: PQ Mute in ARM(MSG)\n", __func__, __LINE__);
		Ret = 0;
	}
	return Ret;
}
EXPORT_SYMBOL(mtk_render_set_pixel_mute_gfx);

static bool mtk_tv_drm_check_bypass_backlight(void)
{
	int32_t wakeup_reason;

	wakeup_reason = pm_get_wakeup_reason();
	DRM_INFO("[%s] wakeup_reason=[%d].\n", __func__, wakeup_reason);

	switch (wakeup_reason) {
	case PM_WK_RTC:
	case PM_WK_RTC2:
	case PM_WK_AVLINK:
	case PM_WK_VOICE:
	case PM_WK_VAD:
	case PM_WK_UART:
	case PM_WK_GPIO_WOWLAN:
	case PM_WK_GPIO_WOEWBS:
	case PM_WK_WOC:
	case PM_WK_NONE:
		return TRUE;
	default:
		return FALSE;
	}
}

int _set_backlight_mute(struct mtk_panel_context *pctx,
	drm_st_backlight_mute_info backlight_mute_info)
{
	uint32_t u32_ldm_support_type = E_LDM_UNSUPPORT;
	int ret = 0;

	if (!pctx)
		return -EINVAL;

	if (mtk_render_get_pnl_init_done()) {
		if (pctx->cus.vcc_bl_cusctrl == 0) {

			//ldm support, notify ldm turn off backlight(range is 0)
			u32_ldm_support_type = mtk_ldm_get_support_type();
			if (u32_ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU
				|| u32_ldm_support_type == E_LDM_SUPPORT_R2) {
				ret = mtk_ldm_set_backlight_off(backlight_mute_info.bEnable);

				if (ret)
					DRM_INFO("[%s][%d]LDM models BL_off fail\n", __func__, __LINE__);
			}
			else
			{
				// LDM models don't pull down BL_ON
				gpiod_set_value(pctx->gpio_backlight, !backlight_mute_info.bEnable);
				DRM_INFO("[%s][%d] not LDM models BL_off\n", __func__, __LINE__);
			}
		}
		// double check
		if (pctx->cus.vcc_bl_cusctrl == 0) {
			if (gpiod_get_value(pctx->gpio_backlight) == (!backlight_mute_info.bEnable))
				DRM_INFO("[%s][%d]BL mute pass\n", __func__, __LINE__);
		}
	} else {
		// if subhal want to mute/unmute BL but panel is not ready case
		DRM_INFO("[%s][%d]BL mute undo\n", __func__, __LINE__);
		backlight_undo = true;
		bl_undo_en = !backlight_mute_info.bEnable;
	}

	return 0;
}

int mtk_render_set_backlight_mute(struct mtk_panel_context *pctx,
	drm_st_backlight_mute_info backlight_mute_info)
{
	if (!pctx)
		return -EINVAL;

	if (mtk_render_get_pnl_init_done()) {
		if (pctx->cus.vcc_bl_cusctrl == 0) {
			mutex_lock(&mutex_globalmute);
			// if the global mute is in process, ignore the subhal backlight unmute
			if (g_bGlobalMuteEn && backlight_mute_info.bEnable == 0) {
				DRM_INFO("[%s] Global mute is in process. Keep BackLight mute\n",
					__func__);
				mutex_unlock(&mutex_globalmute);
				return 0;
			}

			_set_backlight_mute(pctx, backlight_mute_info);
			mutex_unlock(&mutex_globalmute);
		}
	} else {
		// if subhal want to mute/unmute BL but panel is not ready case
		DRM_INFO("[%s][%d]BL mute undo\n", __func__, __LINE__);
		backlight_undo = true;
		bl_undo_en = !backlight_mute_info.bEnable;
	}

	return 0;
}

void _mtk_render_powerdown(struct mtk_panel_context *pctx, bool bEn)
{
	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	if (bEn)
		mtk_render_output_en(pctx, false);
	else
		mtk_render_output_en(pctx, true);

}

int _mtk_tcon_update_ssc(struct mtk_panel_context *pctx_pnl)
{
	uint32_t u32Val = 0;
	int ret = 0;
	bool ssc_tcon_ctrl_en = 0;

	ret = mtk_tcon_get_property(E_TCON_PROP_MODULE_EN, &u32Val);

	if (ret == 0 && u32Val) {
		ret = mtk_tcon_get_property(E_TCON_PROP_SSC_BIN_CTRL, &u32Val);
		ssc_tcon_ctrl_en = (ret == 0 && u32Val == 0) ? 1 : 0;

		//force tcon bin control, if u32Val = 0
		if (ssc_tcon_ctrl_en) {
			ret = mtk_tcon_get_property(E_TCON_PROP_SSC_EN, &u32Val);
			pctx_pnl->stdrmSSC.ssc_setting.ssc_en = (ret == 0) ? u32Val : 0;
			u32Val = 0;

			ret = mtk_tcon_get_property(E_TCON_PROP_SSC_FMODULATION, &u32Val);
			pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation = (ret == 0) ? u32Val : 0;
			u32Val = 0;

			ret = mtk_tcon_get_property(E_TCON_PROP_SSC_PERCENTAGE, &u32Val);
			pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation = (ret == 0) ? u32Val : 0;
			// deviation: 0.1% for TCON_CTRL, 0.01% in render DD
			pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation *= 10;

			DRM_INFO("[%s,%d] Tcon control SSC. en=%X modulation=%X deviation=%X\n",
					__func__, __LINE__,
					pctx_pnl->stdrmSSC.ssc_setting.ssc_en,
					pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation,
					pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation);
		}
	}

	return ret;

}

void _mtk_tcon_preinit_proc(struct mtk_panel_context *pctx)
{
	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	if (pctx->cus.tcon_en && pctx->out_upd_type != E_NO_UPDATE_TYPE) {
		/* get change mode or not */
		bTconModeChange = mtk_tcon_is_mode_change(pctx);

		TCON_DEBUG("ModeChg=%d typ_dclk=%llu -> %llu\n",
				bTconModeChange,
				u64PreTypDclk,
				pctx->info.typ_dclk);

		/* power off */
		if (bTconModeChange) {
			bTconPreOutputEnable = mtk_tcon_is_enable();
			mtk_tcon_enable(pctx, false);
			_mtk_tcon_update_ssc(pctx);
		}
	}
}

void _mtk_tcon_init_proc(struct mtk_panel_context *pctx)
{
	__u32 u32tmp = 0, u32framerate = 0;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	TCON_DEBUG("ModeChg=%d PreOutputEn=%d upd_type=%d\n",
				bTconModeChange, bTconPreOutputEnable,
				pctx->out_upd_type);

	if (pctx->cus.tcon_en &&
		((pctx->out_upd_type == E_MODE_RESET_TYPE) ||
		(pctx->out_upd_type == E_MODE_RESUME_TYPE) || bTconModeChange)) {

		mtk_tcon_init(pctx); //tcon initialize

		u32tmp = pctx->info.typ_htt * pctx->info.typ_vtt / DEC_2;
		u32framerate = (__u32)((pctx->info.typ_dclk * DEC_100 + u32tmp) /
			(__u64)pctx->info.typ_htt / (__u64)pctx->info.typ_vtt);

		//only 120Hz output customize swing and pe level
		if (u32framerate >= 11900 && u32framerate <= 12100) {
			if (pctx->stdrmSwing.usr_swing_level)
				mtk_render_set_swing_vreg(&pctx->stdrmSwing, pctx->cus.tcon_en);
			if (pctx->stdrmPE.usr_pe_level)
				mtk_render_set_pre_emphasis(&pctx->stdrmPE);
		}

		/* power on */
		if (bTconModeChange) {
			if (bTconPreOutputEnable)
				mtk_tcon_enable(pctx, true);
			bTconModeChange = false;
		}
	}

	//only pqgamma enable
	if (pctx->cus.pgamma_en)
		mtk_tcon_set_pq_sharemem_context(pctx);

}

void _mtk_tcon_deinit_proc(struct mtk_panel_context *pctx)
{

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	if (pctx->cus.tcon_en)
		mtk_tcon_deinit(pctx); //tcon de-initialize
}

//Panel to CFD Coolor Format Coversion Table
uint8_t _panel_color_format_to_cfd_color_format(drm_en_pnl_output_format op_format)
{
	uint8_t cfd_color_fmt = op_format;

	DRM_INFO("[%s][%d] panel color format from dts is %d\n",
		__func__, __LINE__, op_format);

	if (op_format == E_OUTPUT_YUV444)
		cfd_color_fmt = E_OUTPUT_YUV422;
	else if (op_format == E_OUTPUT_YUV422)
		cfd_color_fmt = E_OUTPUT_YUV444;

	return cfd_color_fmt;
}

void mtk_video_display_set_panel_color_format(struct mtk_panel_context *pctx)
{
	struct mtk_cfd_set_CSC *mi_render_csc_input;
	struct mtk_cfd_set_CSC cfd_csc_input;
	struct mtk_cfd_set_CSC cfd_csc_video_output;
	struct mtk_cfd_set_CSC cfd_csc_delta_output;
	struct drm_connector *drm_conn = NULL;
	struct mtk_tv_drm_connector *mtk_conn = NULL;
	struct mtk_tv_kms_context *pctx_kms = NULL;
	struct mtk_video_context *pctx_video = NULL;


	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	mi_render_csc_input = mtk_video_display_GetCFD_RenderInCsc_Setting();
	if (mi_render_csc_input == NULL) {
		//set render input color format
		cfd_csc_input.u8Data_Format = pctx->cus.render_in_color_format;
		cfd_csc_input.u8ColorSpace = pctx->cus.render_in_color_space;
		cfd_csc_input.u8Range = pctx->cus.render_in_color_range;
	} else {
		cfd_csc_input.u8Data_Format = mi_render_csc_input->u8Data_Format;
		cfd_csc_input.u8ColorSpace = mi_render_csc_input->u8ColorSpace;
		cfd_csc_input.u8Range = mi_render_csc_input->u8Range;
	}

	//set video1 color format
	cfd_csc_video_output.u8Data_Format =
		_panel_color_format_to_cfd_color_format(pctx->info.op_format);
	cfd_csc_video_output.u8ColorSpace = pctx->info.op_color_space;
	cfd_csc_video_output.u8Range = pctx->info.op_color_range;
	//set video2 color format
	cfd_csc_delta_output.u8Data_Format =
		_panel_color_format_to_cfd_color_format(pctx->extdev_info.op_format);
	cfd_csc_delta_output.u8ColorSpace = pctx->extdev_info.op_color_space;
	cfd_csc_delta_output.u8Range = pctx->extdev_info.op_color_range;
	DRM_INFO("[%s][%d] input(%d,%d,%d), video_output(%d,%d,%d), delta_output(%d,%d,%d)\n",
		__func__, __LINE__,
		cfd_csc_input.u8Data_Format, cfd_csc_input.u8ColorSpace,
		cfd_csc_input.u8Range, cfd_csc_video_output.u8Data_Format,
		cfd_csc_video_output.u8ColorSpace, cfd_csc_video_output.u8Range,
		cfd_csc_delta_output.u8Data_Format, cfd_csc_delta_output.u8ColorSpace,
		cfd_csc_delta_output.u8Range);

	mtk_video_display_SetCFD_Setting(pctx->hw_info.pnl_lib_version,
		&cfd_csc_input, &cfd_csc_video_output, &cfd_csc_delta_output);

	drm_conn = pctx->drmPanel.connector;
	mtk_conn = (drm_conn) ? to_mtk_tv_connector(drm_conn) : NULL;
	pctx_kms = (mtk_conn) ? (struct mtk_tv_kms_context *)mtk_conn->connector_private : NULL;
	pctx_video = (pctx_kms) ? &pctx_kms->video_priv : NULL;
	mtk_video_display_set_defaultBackgroundColor(pctx_video);
}

unsigned char _PANEL_CUST_HWI2C_ReadBytes(unsigned short u16BusNum,
	unsigned char u8SlaveID,
	unsigned char ucSubAdr,
	unsigned char *paddr,
	unsigned short ucBufLen,
	unsigned char *pBuf)
{
	int num_xfer;
	unsigned short u16BusNumSlaveID = (u16BusNum << DEC_8) | (u8SlaveID);
	struct i2c_adapter *adap;
	struct i2c_msg msgs[] = {
		{
			.addr = LOBYTE(u16BusNumSlaveID & HEX_FF),
			.flags = 0,
			.len = ucSubAdr,
			.buf = paddr,
		},
		{
			.addr = LOBYTE(u16BusNumSlaveID & HEX_FF),
			.flags = I2C_M_RD,
			.len = ucBufLen,
			.buf = pBuf,
		},
	};

	adap = i2c_get_adapter(HIBYTE(u16BusNumSlaveID));
	if (!adap)
		return FALSE;
	num_xfer = i2c_transfer(adap, msgs, ARRAY_SIZE(msgs));
	i2c_put_adapter(adap);
	return (num_xfer == ARRAY_SIZE(msgs)) ? TRUE : FALSE;
}

unsigned char _PANEL_CUST_HWI2C_WriteBytes(unsigned short u16BusNum,
	unsigned char u8SlaveID,
	unsigned char AddrCnt,
	unsigned char *pu8addr,
	unsigned short u16size,
	unsigned char *pBuf)
{
	int num_xfer;
	unsigned short u16BusNumSlaveID = (u16BusNum << DEC_8) | (u8SlaveID);
	struct i2c_adapter *adap;
	struct i2c_msg msg = {
		.addr = LOBYTE(u16BusNumSlaveID & HEX_FF),
		.flags = 0,
		.len = (AddrCnt + u16size),
	};
	unsigned char *data;

	data = kzalloc(AddrCnt + u16size, GFP_KERNEL);
	if (!data)
		return FALSE;
	memcpy(data, pu8addr, AddrCnt);
	memcpy(data + AddrCnt, pBuf, u16size);
	msg.buf = data;
	adap = i2c_get_adapter(HIBYTE(u16BusNumSlaveID));
	if (!adap) {
		kfree(data);
		TCON_DEBUG("%s No adapter available!\n", __func__);
		return FALSE;
	}
	num_xfer = i2c_transfer(adap, &msg, TRUE);
	i2c_put_adapter(adap);
	kfree(data);
	return (num_xfer == TRUE) ? TRUE : FALSE;
}

unsigned char _PANEL_CUST_SWI2C_ReadBytes(unsigned short u16BusNum,
	unsigned char u8SlaveID,
	unsigned char ucSubAdr,
	unsigned char *paddr,
	unsigned short ucBufLen,
	unsigned char *pBuf)
{
	return FALSE;
}
unsigned char _PANEL_CUST_SWI2C_WriteBytes(unsigned short u16BusNum,
	unsigned char u8SlaveID,
	unsigned char AddrCnt,
	unsigned char *pu8addr,
	unsigned short u16size,
	unsigned char *pBuf)
{
	return FALSE;
}

static void _mtk_game_direct_send_tcon_i2c_cmd(struct mtk_panel_context *pctx)
{
	int  s32Ret = 0;
	uint16_t  u16Game_Direct_fr_group = 0;
	uint8_t   *u8I2C_Column_Num = NULL;
	uint8_t   u8I2C_Row = 0;
	uint8_t   *u8I2C_Data = NULL;
	uint8_t   u8BusID = 0;
	uint8_t   u8SlaveID = 0;
	uint8_t   u8Offset = 0;
	uint8_t   u8Size = 0;
	uint8_t   u8I2C_Mode = 0;
	struct drm_st_i2c_cmd_info i2c_cmd_info;
	uint8_t   i = 0, index = 0;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	u16Game_Direct_fr_group = pctx->info.game_direct_fr_group;
	if (u16Game_Direct_fr_group > tcon_i2c_count) {
		DRM_ERROR("[%s][%d] tcon_i2c_count < u16Game_Direct_fr_group!\n", __func__, __LINE__);
		return;
	}
	u8Size     = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_info_length;
	u8BusID    = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_bus;
	u8SlaveID  = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_dev_addr;
	u8Offset   = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_reg_offst;
	u8I2C_Mode = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_mode;
	u8I2C_Row  = pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_info_row;
	u8I2C_Column_Num = kzalloc(
				(u8I2C_Row * sizeof(uint8_t)), GFP_KERNEL);
	memcpy(u8I2C_Column_Num,
		pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_info_column_num,
		(pctx->cus.tcon_i2c_info[i].i2c_info_row * sizeof(uint8_t)));
	u8I2C_Data = kzalloc(
				(u8Size * sizeof(uint8_t)), GFP_KERNEL);
	memcpy(u8I2C_Data,
		pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_info_data,
		(pctx->cus.tcon_i2c_info[i].i2c_info_length * sizeof(uint8_t)));

	DRM_INFO("I2C BusID %d, SlaveID 0x%02x, Offset=%d, Size=%d\n",
		u8BusID, u8SlaveID, u8Offset, u8Size);

	if (u8I2C_Mode == 1) {  // hw i2c
		for (i = 0; i < u8I2C_Row; i++) {
			i2c_cmd_info.i2c_cmd_bus = u8BusID;
			//shift right 1bit for 8bit slave addressing
			i2c_cmd_info.i2c_cmd_dev_addr = u8SlaveID >> DEC_1;
			i2c_cmd_info.i2c_cmd_reg_offst = u8Offset;
			i2c_cmd_info.i2c_cmd_length = u8I2C_Column_Num[i];
			i2c_cmd_info.i2c_cmd_data =
				kzalloc(
				(i2c_cmd_info.i2c_cmd_length * sizeof(uint8_t)), GFP_KERNEL);
			memcpy(i2c_cmd_info.i2c_cmd_data,
			pctx->cus.tcon_i2c_info[u16Game_Direct_fr_group].i2c_info_data + index,
				i2c_cmd_info.i2c_cmd_length * sizeof(uint8_t));
			s32Ret = _PANEL_CUST_HWI2C_WriteBytes(i2c_cmd_info.i2c_cmd_bus,
			i2c_cmd_info.i2c_cmd_dev_addr, 0, &i2c_cmd_info.i2c_cmd_reg_offst,
			(unsigned int)i2c_cmd_info.i2c_cmd_length, i2c_cmd_info.i2c_cmd_data);
			if (s32Ret == 0)
				DRM_ERROR("HWI2C write fail. s32Ret=%d\n", s32Ret);
			kfree(i2c_cmd_info.i2c_cmd_data);
			index += i2c_cmd_info.i2c_cmd_length;
		}
	} else {
		s32Ret = _PANEL_CUST_SWI2C_WriteBytes(u8BusID,
			u8SlaveID, 0, &u8Offset, u8Size, u8I2C_Data);
		if (s32Ret == 0)
			DRM_ERROR("SWI2C write fail. s32Ret=%d\n", s32Ret);
	}

	kfree(u8I2C_Column_Num);
	kfree(u8I2C_Data);
}

static void _mtk_dlg_send_tcon_i2c_cmd(struct mtk_panel_context *pctx)
{
	int  s32Ret = 0;
	uint16_t  u16DLG_flag = 0;
	uint8_t   *u8I2C_Column_Num = NULL, *u8I2C_Sec_Column_Num = NULL;
	uint8_t   u8I2C_Row = 0, u8I2C_Sec_Row = 0;
	uint8_t   *u8I2C_Data = NULL, *u8I2C_Sec_Data = NULL;
	uint8_t   u8BusID = 0, u8BusID_Sec = 0;
	uint8_t   u8SlaveID = 0, u8SlaveID_Sec = 0;
	uint8_t   u8Offset = 0, u8Offset_Sec = 0;
	uint8_t   u8Size = 0, u8Size_Sec = 0;
	uint8_t   u8I2C_Mode = 0, u8I2C_Sec_Mode = 0;
	struct drm_st_i2c_cmd_info i2c_cmd_info, i2c_sec_cmd_info;
	uint8_t   i = 0, j = 0, index = 0;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	u16DLG_flag = pctx->info.dlg_on;
	if (u16DLG_flag > tcon_i2c_count) {
		DRM_ERROR("[%s][%d] tcon_i2c_count < u16DLG_flag!\n", __func__, __LINE__);
		return;
	}
	if (pctx->cus.dlg_i2c_sec_CustCtrl && (u16DLG_flag > tcon_i2c_sec_count)) {
		DRM_ERROR("[%s][%d] tcon_i2c_sec_count < u16DLG_flag!\n", __func__, __LINE__);
		return;
	}
	u8Size     = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_length;
	u8BusID    = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_bus;
	u8SlaveID  = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_dev_addr;
	u8Offset   = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_reg_offst;
	u8I2C_Mode = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_mode;
	u8I2C_Row  = pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_row;
	u8I2C_Column_Num = kzalloc(
				(u8I2C_Row * sizeof(uint8_t)), GFP_KERNEL);
	memcpy(u8I2C_Column_Num,
		pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_column_num,
		(pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_row * sizeof(uint8_t)));
	u8I2C_Data = kzalloc(
				(u8Size * sizeof(uint8_t)), GFP_KERNEL);
	memcpy(u8I2C_Data,
		pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_data,
		(pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_length * sizeof(uint8_t)));

	DRM_INFO("I2C BusID %d, SlaveID 0x%02x, Offset=%d, Size=%d\n",
		u8BusID, u8SlaveID, u8Offset, u8Size);

	if (pctx->cus.dlg_i2c_sec_CustCtrl) {
		u8Size_Sec     = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_length;
		u8BusID_Sec    = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_bus;
		u8SlaveID_Sec  = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_dev_addr;
		u8Offset_Sec   = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_reg_offst;
		u8I2C_Sec_Mode = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_mode;
		u8I2C_Sec_Row  = pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_row;
		u8I2C_Sec_Column_Num = kzalloc(
					(u8I2C_Sec_Row * sizeof(uint8_t)), GFP_KERNEL);
		memcpy(u8I2C_Sec_Column_Num,
			pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_column_num,
			(pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_row * sizeof(uint8_t)));
		u8I2C_Sec_Data = kzalloc(
					(u8Size * sizeof(uint8_t)), GFP_KERNEL);
		memcpy(u8I2C_Sec_Data,
			pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_data,
			(pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_length * sizeof(uint8_t)));

		DRM_INFO("I2C Sec BusID %d, SlaveID 0x%02x, Offset=%d, Size=%d\n",
			u8BusID_Sec, u8SlaveID_Sec, u8Offset_Sec, u8Size_Sec);
	}

	//set i2c wp off before sending i2c cmd
	if (pctx->cus.i2c_wp_CustCtrl == 1) {
		gpiod_set_value(pctx->cus.i2c_wp_gpio, pctx->cus.i2c_wp_off);
		if (gpiod_get_value(pctx->cus.i2c_wp_gpio) != pctx->cus.i2c_wp_off)
			DRM_ERROR("Change I2C WP OFF GPIO fail!!\n");
	}

	if (u8I2C_Mode == 1) {  // hw i2c
		for (i = 0; i < u8I2C_Row; i++) {
			i2c_cmd_info.i2c_cmd_bus = u8BusID;
			//shift right 1bit for 8bit slave addressing
			i2c_cmd_info.i2c_cmd_dev_addr = u8SlaveID >> DEC_1;
			i2c_cmd_info.i2c_cmd_reg_offst = u8Offset;
			i2c_cmd_info.i2c_cmd_length = u8I2C_Column_Num[i];
			i2c_cmd_info.i2c_cmd_data =
				kzalloc(
				(i2c_cmd_info.i2c_cmd_length * sizeof(uint8_t)), GFP_KERNEL);
			memcpy(i2c_cmd_info.i2c_cmd_data,
			pctx->cus.tcon_i2c_info[u16DLG_flag].i2c_info_data + index,
				i2c_cmd_info.i2c_cmd_length * sizeof(uint8_t));
			s32Ret = _PANEL_CUST_HWI2C_WriteBytes(i2c_cmd_info.i2c_cmd_bus,
			i2c_cmd_info.i2c_cmd_dev_addr, 0, &i2c_cmd_info.i2c_cmd_reg_offst,
			(unsigned int)i2c_cmd_info.i2c_cmd_length, i2c_cmd_info.i2c_cmd_data);
			if (s32Ret == 0) {
				for (j = 0; j < I2C_RETRY_COUNT; j++) {
					DRM_ERROR("HWI2C write fail. retry cnt=%d\n", j);
					msleep(20);
					s32Ret = _PANEL_CUST_HWI2C_WriteBytes(
					i2c_cmd_info.i2c_cmd_bus, i2c_cmd_info.i2c_cmd_dev_addr, 0,
					&i2c_cmd_info.i2c_cmd_reg_offst,
					(unsigned int)i2c_cmd_info.i2c_cmd_length,
					i2c_cmd_info.i2c_cmd_data);
					if (s32Ret) {
						DRM_INFO("HWI2C write done.\n");
						break;
					} else if (j == I2C_RETRY_COUNT - 1)
						DRM_ERROR("HWI2C retry fail. cnt=%d\n", j + 1);
				}
			}
			kfree(i2c_cmd_info.i2c_cmd_data);
			index += i2c_cmd_info.i2c_cmd_length;
		}
		index = 0;
		if (pctx->cus.dlg_i2c_sec_CustCtrl) {
			for (i = 0; i < u8I2C_Sec_Row; i++) {
				i2c_sec_cmd_info.i2c_cmd_bus = u8BusID_Sec;
				//shift right 1bit for 8bit slave addressing
				i2c_sec_cmd_info.i2c_cmd_dev_addr = u8SlaveID_Sec >> DEC_1;
				i2c_sec_cmd_info.i2c_cmd_reg_offst = u8Offset_Sec;
				i2c_sec_cmd_info.i2c_cmd_length = u8I2C_Sec_Column_Num[i];
				i2c_sec_cmd_info.i2c_cmd_data =
					kzalloc(
					(i2c_sec_cmd_info.i2c_cmd_length * sizeof(uint8_t)), GFP_KERNEL);
				memcpy(i2c_sec_cmd_info.i2c_cmd_data,
				pctx->cus.tcon_i2c_sec_info[u16DLG_flag].i2c_info_data + index,
					i2c_sec_cmd_info.i2c_cmd_length * sizeof(uint8_t));
				s32Ret = _PANEL_CUST_HWI2C_WriteBytes(i2c_sec_cmd_info.i2c_cmd_bus,
				i2c_sec_cmd_info.i2c_cmd_dev_addr, 0, &i2c_sec_cmd_info.i2c_cmd_reg_offst,
				(unsigned int)i2c_sec_cmd_info.i2c_cmd_length, i2c_sec_cmd_info.i2c_cmd_data);
				if (s32Ret == 0) {
					for (j = 0; j < I2C_RETRY_COUNT; j++) {
						DRM_ERROR("HWI2C Sec write fail. retry cnt=%d\n", j);
						msleep(20);
						s32Ret = _PANEL_CUST_HWI2C_WriteBytes(
						i2c_sec_cmd_info.i2c_cmd_bus, i2c_sec_cmd_info.i2c_cmd_dev_addr, 0,
						&i2c_sec_cmd_info.i2c_cmd_reg_offst,
						(unsigned int)i2c_sec_cmd_info.i2c_cmd_length,
						i2c_sec_cmd_info.i2c_cmd_data);
						if (s32Ret) {
							DRM_INFO("HWI2C Sec write done.\n");
							break;
						} else if (j == I2C_RETRY_COUNT - 1)
							DRM_ERROR("HWI2C retry fail. cnt=%d\n", j + 1);
					}
				}
				kfree(i2c_sec_cmd_info.i2c_cmd_data);
				index += i2c_sec_cmd_info.i2c_cmd_length;
			}
		}
	} else {
		s32Ret = _PANEL_CUST_SWI2C_WriteBytes(u8BusID,
			u8SlaveID, 0, &u8Offset, u8Size, u8I2C_Data);
		if (s32Ret == 0)
			DRM_ERROR("SWI2C write fail. s32Ret=%d\n", s32Ret);
	}

	//set i2c wp on after sending i2c cmd
	if (pctx->cus.i2c_wp_CustCtrl == 1) {
		gpiod_set_value(pctx->cus.i2c_wp_gpio, pctx->cus.i2c_wp_on);
		if (gpiod_get_value(pctx->cus.i2c_wp_gpio) != pctx->cus.i2c_wp_on)
			DRM_ERROR("Change I2C WP ON GPIO fail!!\n");
	}

	kfree(u8I2C_Column_Num);
	kfree(u8I2C_Data);
	if (pctx->cus.dlg_i2c_sec_CustCtrl) {
		kfree(u8I2C_Sec_Column_Num);
		kfree(u8I2C_Sec_Data);
	}
}

static void _mtk_vrr_hfr_send_i2c_cmd(struct mtk_panel_context *pctx)
{
	int  s32Ret = 0;
	uint16_t  u16DLG_flag = 0;
	uint8_t   *u8I2C_Data = NULL;
	uint8_t   *u8I2C_Addr = NULL;
	uint8_t   u8Offset = 0;
	uint8_t   u8BusID = 0;
	uint8_t   u8SlaveID = 0;
	uint8_t   u8DataSize = 0;
	uint8_t   u8AddrSize = 0;
	uint8_t   u8I2C_Mode = 0;
	uint8_t   i = 0;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	if (pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR == NULL ||
		pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA == NULL ||
		pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR == NULL ||
		pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA == NULL) {
		DRM_INFO("[%s][%d] VRR HFR I2C DATA is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	u16DLG_flag = pctx->info.dlg_on;
	u8I2C_Mode	= I2C_HW_MODE;
	u8BusID		= I2C_DEFAULT_BUS;
	u8SlaveID	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_PMIC_I2C_SLAVE_ADDR;
	if (u16DLG_flag) {
		u8AddrSize	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR_SIZE;
		u8DataSize	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA_SIZE;
		u8I2C_Addr	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR;
		u8I2C_Data	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA;
	} else {
		u8AddrSize	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR_SIZE;
		u8DataSize	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA_SIZE;
		u8I2C_Addr	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR;
		u8I2C_Data	= pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA;
	}

	DRM_INFO("[%s][%d] I2C BusID %d, SlaveID 0x%02x, Size=%d\n",
		 __func__, __LINE__, u8BusID, u8SlaveID, u8DataSize);

	//Pre delay
	if (pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_PMIC_I2C_PRE_DELAY)
		msleep(pctx->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_PMIC_I2C_PRE_DELAY);

	if (u8I2C_Mode == 1) {  // hw i2c
		s32Ret = _PANEL_CUST_HWI2C_WriteBytes(u8BusID, u8SlaveID >> DEC_1,
			u8AddrSize, u8I2C_Addr, u8DataSize, u8I2C_Data);
		if (s32Ret == 0) {
			for (i = 0; i < I2C_RETRY_COUNT; i++) {
				DRM_ERROR("HWI2C write fail. retry cnt=%d\n", i);
				msleep(20);
				s32Ret = _PANEL_CUST_HWI2C_WriteBytes(u8BusID, u8SlaveID >> DEC_1,
			u8AddrSize, u8I2C_Addr, u8DataSize, u8I2C_Data);
				if (s32Ret) {
					DRM_INFO("HWI2C write done.\n");
					break;
				} else if (i == I2C_RETRY_COUNT - 1) {
					DRM_ERROR("HWI2C retry fail. cnt=%d\n", i + 1);
				}
			}
		}
	} else {
		s32Ret = _PANEL_CUST_SWI2C_WriteBytes(u8BusID,
			u8SlaveID, 0, &u8Offset, u8DataSize, u8I2C_Data);
		if (s32Ret == 0)
			DRM_ERROR("SWI2C write fail. s32Ret=%d\n", s32Ret);
	}
}

static uint8_t _mtk_get_out_clk_if(struct mtk_panel_context *pctx)
{
	uint8_t out_clk_if = 0;
	uint16_t lanes = 0;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return false;
	}

	switch (pctx->v_cfg.timing) {
	case E_8K4K_120HZ:
	case E_8K4K_144HZ:
		out_clk_if = LANE_NUM_64;
	break;
	case E_8K4K_60HZ:
		if ((pctx->v_cfg.format == E_OUTPUT_YUV444)
			|| (pctx->v_cfg.format == E_OUTPUT_RGB))
			out_clk_if = LANE_NUM_32;	// 8K60 YUV444, 32p
		else
			out_clk_if = LANE_NUM_16;	//8K60 yuv422, 16p
	break;
	case E_4K2K_120HZ:
	case E_4K2K_144HZ:
	case E_4K1K_240HZ:
		out_clk_if = LANE_NUM_16;
	break;
	case E_4K2K_60HZ:
	case E_4K1K_120HZ:
		out_clk_if = LANE_NUM_8;
	break;
	case E_FHD_120HZ:
		out_clk_if = LANE_NUM_4;
	break;
	case E_FHD_60HZ:
		out_clk_if = LANE_NUM_2;
	break;
	default:
		out_clk_if = LANE_NUM_32;
	break;
	}

	/* for tconless model use */
	if (mtk_tcon_get_lanes_num_by_linktype(pctx->v_cfg.linkIF, &lanes) == 0) {
		DRM_INFO("[%s:%d] tconless lanes=%d -> %d\n",
			__func__, __LINE__, out_clk_if, lanes);
		out_clk_if = lanes;
	}

	return out_clk_if;
}

static void _mtk_tcon_cust_i2c_gpio_control(struct mtk_panel_context *pctx)
{
	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return;
	}

	if (!pctx->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet && pctx->cus.tcon_en) {
		DRM_INFO("[%s][%d]Tconless GPIO setting is set in tcon bin\n", __func__, __LINE__);
		return;
	}

	//send game direct tcon i2c cmd when fr group changed
	if ((pctx->cus.game_direct_framerate_mode == TRUE) &&
		 (u16GameDirectFrGroup != pctx->info.game_direct_fr_group) &&
		 !pctx->cus.tcon_en) {
		_mtk_game_direct_send_tcon_i2c_cmd(pctx);
		u16GameDirectFrGroup = pctx->info.game_direct_fr_group;
	}
	//send dlg tcon i2c cmd when dlg on/off changed
	if ((pctx->cus.dlg_en == true)
		&& (pctx->cus.dlg_i2c_CustCtrl == true)
		&& (!pctx->cus.tcon_en)
		&& ((u16DLG_On != pctx->info.dlg_on) || _i2c_need_init)) {
		_mtk_dlg_send_tcon_i2c_cmd(pctx);
		u16DLG_On = pctx->info.dlg_on;
		_i2c_need_init = false;
	}
	if ((pctx->cus.dlg_en) &&
		(pctx->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet) &&
		((u16DLG_On != pctx->info.dlg_on) || _i2c_need_init)) {
		_mtk_vrr_hfr_send_i2c_cmd(pctx);
		u16DLG_On = pctx->info.dlg_on;
		_i2c_need_init = false;
	}
	//set tcon gpio when dlg on/off changed
	if ((pctx->cus.dlg_en == true)
		&& (pctx->cus.dlg_gpio_CustCtrl == true)
		&& (gpiod_get_value(pctx->gpio_dlg) != pctx->info.dlg_on)) {
		DRM_INFO("Change DLG GPIO from %d -> %d\n",
			gpiod_get_value(pctx->gpio_dlg), pctx->info.dlg_on);
		gpiod_set_value(pctx->gpio_dlg, pctx->info.dlg_on);
		if (gpiod_get_value(pctx->gpio_dlg) != pctx->info.dlg_on)
			DRM_ERROR("Change DLG GPIO fail!!\n");
	}
}

static struct mtk_demura_context *_mtk_get_demura_context_by_kms(
	struct mtk_tv_kms_context *pctx_kms)
{
	struct mtk_demura_context *pctx_demura = NULL;

	if (pctx_kms == NULL) {
		DRM_INFO("[%s][%d]pctx_kms is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	pctx_demura = &pctx_kms->demura_priv;
	if (pctx_demura == NULL) {
		DRM_INFO("[%s][%d]pctx_demura is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	return pctx_demura;
}

static struct mtk_demura_context *_mtk_get_demura_context(
	struct mtk_panel_context *pctx)
{
	struct drm_connector *drm_conn = NULL;
	struct mtk_tv_drm_connector *mtk_conn = NULL;
	struct mtk_tv_kms_context *pctx_kms = NULL;
	struct mtk_demura_context *pctx_demura = NULL;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]pctx is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	drm_conn = pctx->drmPanel.connector;
	if (drm_conn == NULL) {
		DRM_INFO("[%s][%d]drm_conn is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	mtk_conn = to_mtk_tv_connector(drm_conn);
	if (mtk_conn == NULL) {
		DRM_INFO("[%s][%d]mtk_conn is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	pctx_kms = (struct mtk_tv_kms_context *)mtk_conn->connector_private;
	if (pctx_kms == NULL) {
		DRM_INFO("[%s][%d]pctx_kms is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	pctx_demura = &pctx_kms->demura_priv;
	if (pctx_demura == NULL) {
		DRM_INFO("[%s][%d]pctx_demura is NULL, FAIL\n", __func__, __LINE__);
		return NULL;
	}

	return pctx_demura;
}

bool _mtk_set_swing_biascon(struct mtk_panel_context *pctx)
{
	int ret;
	struct st_hw_info stHwInfo;
	uint16_t biasConVal = 0;
	uint16_t biasConOffsetVal = 0;

	if (!pctx) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return false;
	}

	memset(&stHwInfo, 0, sizeof(stHwInfo));

	stHwInfo.pnl_lib_version = pctx->hw_info.pnl_lib_version;
	stHwInfo.rcon_enable = pctx->hw_info.rcon_enable;
	stHwInfo.rcon_max = pctx->hw_info.rcon_max;
	stHwInfo.rcon_min = pctx->hw_info.rcon_min;
	stHwInfo.rcon_value = pctx->hw_info.rcon_value;
	stHwInfo.biascon_single_max = pctx->hw_info.biascon_single_max;
	stHwInfo.biascon_single_min = pctx->hw_info.biascon_single_min;
	stHwInfo.biascon_single_value = pctx->hw_info.biascon_single_value;
	stHwInfo.biascon_double_max = pctx->hw_info.biascon_double_max;
	stHwInfo.biascon_double_min = pctx->hw_info.biascon_double_min;
	stHwInfo.biascon_double_value = pctx->hw_info.biascon_double_value;
	stHwInfo.rint_enable = pctx->hw_info.rint_enable;
	stHwInfo.rint_max = pctx->hw_info.rint_max;
	stHwInfo.rint_min = pctx->hw_info.rint_min;
	stHwInfo.rint_value = pctx->hw_info.rint_value;

	DRM_INFO("[%s:%d] rcon en=%d %d, biascon sigle=%d double=%d, rint en=%d %d\n",
			__func__, __LINE__,
			stHwInfo.rcon_enable, stHwInfo.rcon_value,
			stHwInfo.biascon_single_value,
			stHwInfo.biascon_double_value,
			stHwInfo.rint_enable, stHwInfo.rint_value);

	ret = drv_hwreg_render_pnl_set_swing_biascon(
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			&stHwInfo,
			biasConVal,
			biasConOffsetVal);

	return (ret == 0) ? true : false;
}

bool mtk_render_cfg_connector(struct mtk_panel_context *pctx)
{
	uint8_t out_clk_if = 0;
	uint32_t hw_version = 0;
	bool bDuplicateEn = 0;
	st_drv_out_lane_order stPnlOutLaneOrder = { };
	struct mtk_demura_context *pctx_demura = NULL;
	uint8_t ldm_support_type = E_LDM_UNSUPPORT;

	if (pctx == NULL) {
		DRM_INFO("[%s][%d]param is NULL, FAIL\n", __func__, __LINE__);
		return false;
	}

	DRM_INFO("[%s][%d] Tcon preinit tcon_en=%d out_upd_type=%d\n",
					__func__, __LINE__, pctx->cus.tcon_en, pctx->out_upd_type);

	_mtk_tcon_preinit_proc(pctx);

	hw_version = pctx->hw_info.pnl_lib_version;
	memcpy(&stPnlOutLaneOrder, &pctx->out_lane_info, sizeof(st_drv_out_lane_order));

	bDuplicateEn = pctx->lane_duplicate_en;

	_init_mod_cfg(pctx);
	set_out_mod_cfg(&pctx->info, &pctx->v_cfg);
	_dump_mod_cfg(pctx);

	//for haps only case.
#ifdef CONFIG_MSTAR_ARM_BD_FPGA
	pctx->v_cfg.linkIF = E_LINK_LVDS;
	pctx->v_cfg.timing = E_FHD_60HZ;
	pctx->v_cfg.format = E_OUTPUT_RGB;
	pctx->v_cfg.lanes = 2;
#endif

	if ((pctx->out_upd_type == E_MODE_RESET_TYPE) ||
		(pctx->out_upd_type == E_MODE_RESUME_TYPE)) {
		drv_hwreg_render_pnl_analog_setting(
			hw_version,
			bDuplicateEn,
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			(en_drv_pnl_output_mode)pctx->v_cfg.timing,
			&stPnlOutLaneOrder,
			(en_drv_pnl_output_format)pctx->v_cfg.format);

		_mtk_load_mpll_tbl(pctx);//load mpll table after enable power regulator

		drv_hwreg_render_pnl_set_vby1_pll_powerdown(false);

		_mtk_cfg_ckg_common(pctx);

		_mtk_load_lpll_tbl(pctx);

		_mtk_load_moda_tbl(pctx);
		_mtk_moda_path_setting(pctx);
	}

	if (pctx->v_cfg.linkIF != E_LINK_NONE) {
		out_clk_if = _mtk_get_out_clk_if(pctx);

		if ((pctx->out_upd_type == E_MODE_RESET_TYPE) ||
			(pctx->out_upd_type == E_MODE_RESUME_TYPE)) {
			drm_en_vbo_bytemode vbo_bytemode = _vb1_set_byte_mode_decision(pctx);
			drv_hwreg_render_pnl_set_cfg_ckg_video(
						hw_version,
						(en_drv_pnl_vbo_bytemode)vbo_bytemode,
						MOD_IN_IF_VIDEO,
						out_clk_if,
						(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
						(en_drv_pnl_output_mode)pctx->v_cfg.timing,
						(en_drv_pnl_output_format)pctx->v_cfg.format,
						pctx->cus.tcon_en,
						IS_TCON_FIFOCLK_SUPPORT(pctx->v_cfg.linkIF));
		}
	}

	if ((pctx->out_upd_type == E_MODE_RESET_TYPE) ||
		(pctx->out_upd_type == E_MODE_RESUME_TYPE)) {
		uint32_t pnl_ver = pctx->hw_info.pnl_lib_version;


		_mtk_odclk_setting(pctx);
		_mtk_vby1_setting(pctx);
		_mtk_vby1_lane_order(pctx);
		drv_hwreg_render_pnl_set_video1_vby1_fifo_reset();

		drv_hwreg_render_pnl_disp_odclk_init();
		drv_hwreg_render_pnl_set_swing_rcon(
			(en_drv_pnl_link_if)pctx->v_cfg.linkIF,
			pnl_ver,
			pctx->hw_info.rcon_enable,
			pctx->hw_info.rcon_max,
			pctx->hw_info.rcon_min,
			pctx->hw_info.rcon_value);
		_mtk_set_swing_biascon(pctx);
		mtk_render_set_ssc(pctx);
		mtk_render_set_swing_vreg(&pctx->stdrmSwing, pctx->cus.tcon_en);
		mtk_render_set_pre_emphasis(&pctx->stdrmPE);
		mtk_render_set_output_hmirror_enable(pctx);
		mtk_render_set_panel_dither(&pctx->dither_info);
		drv_hwreg_render_pnl_set_scdisp_path_sel(
			(en_drv_pnl_scdisp_path_sel)pctx->cus.escdisp_path_sel);
	}

	mtk_video_display_set_panel_color_format(pctx);

	if (pctx->out_upd_type != E_NO_UPDATE_TYPE) {
		if (_bPreDLG_status != pctx->info.dlg_on){
			ldm_support_type = mtk_ldm_get_support_type();
			if (ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU || ldm_support_type == E_LDM_SUPPORT_R2) {
			MApi_ld_set_spi_self_trig_off(E_LDM_HW_VERSION_5);
			DRM_INFO("[%s][%d][LDM] Close LDM spi trigger : PreDLG_status:%d ,dlg_on :%d\n", __func__, __LINE__,_bPreDLG_status,pctx->info.dlg_on);
			}
		}
		_bPreDLG_status = pctx->info.dlg_on;
		if ((pctx->cus.dlg_en == 1) &&
			!drv_hwreg_render_pnl_get_demura_bypass()) {
			pctx_demura = _mtk_get_demura_context(pctx);
			if (pctx_demura != NULL)
				mtk_tv_drm_DLG_demura_set_config(pctx_demura, pctx->info.dlg_on);
		}
		_mtk_tcon_cust_i2c_gpio_control(pctx);
		_mtk_mft_hbkproch_protect_init(pctx);
		_mtk_tgen_init(pctx, out_clk_if);
		pctx->outputTiming_info.FrameSyncModeChgBypass = false;
		mtk_trigger_gen_set_bypass(false);
	}

	_mtk_tcon_init_proc(pctx);

	if (pctx->out_upd_type == E_MODE_RESET_TYPE)
		mtk_render_output_en(pctx, true);

       //set default vfreq
	if (pctx->info.typ_htt != 0 && pctx->info.typ_vtt != 0) {
		pctx->outputTiming_info.u32OutputVfreq =
			(pctx->info.typ_dclk / pctx->info.typ_htt / pctx->info.typ_vtt)
			*VFRQRATIO_1000;//ex:60000 = 60.000hz
		mtk_tv_drm_backlight_set_frame_rate(pctx, pctx->outputTiming_info.u32OutputVfreq);
	}

	pctx->u8controlbit_gfid = 0;//initial control bit gfid

	return true;
}
EXPORT_SYMBOL(mtk_render_cfg_connector);

void mtk_render_pnl_checklockprotect(bool *bVby1LocknProtectEn)
{
	drv_hwreg_render_pnl_checklockprotect(bVby1LocknProtectEn);
}

bool mtk_render_cfg_crtc(struct mtk_panel_context *pctx)
{
	if (!pctx) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return false;
	}

	//TODO:Refine
	return true;
}

static void dump_panel_dts_info(struct mtk_panel_context *pctx_pnl)
{
	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	DRM_INFO("================Video Path================\n");
	dump_panel_info(&pctx_pnl->info);
	DRM_INFO("================================================\n");
}

static void dump_dts_info(struct mtk_panel_context *pctx_pnl)
{
	int FRCType = 0;
	int i;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: Panel context is NULL.\n",
			__func__, __LINE__);
		return;
	}

	dump_panel_dts_info(pctx_pnl);

	DRM_INFO("bVRR_en = %d\n", pctx_pnl->info.vrr_en);
	DRM_INFO("u16VRRMaxRate = %d\n", pctx_pnl->info.vrr_max_v);
	DRM_INFO("u16VRRMinRate = %d\n", pctx_pnl->info.vrr_min_v);

	//pnl_cus_setting
	DRM_INFO("PanelGammaBinPath = %s\n", pctx_pnl->cus.pgamma_path);
	DRM_INFO("TCON bin path = %s\n", pctx_pnl->cus.tcon_path);
	DRM_INFO("bPanelGammaEnable = %d\n", pctx_pnl->cus.pgamma_en);
	DRM_INFO("bTCONBinEnable = %d\n", pctx_pnl->cus.tcon_en);
	DRM_INFO("enPanelMirrorMode = %d\n", pctx_pnl->cus.mirror_mode);
	DRM_INFO("enPanelOutputFormat = %d\n", pctx_pnl->cus.op_format);
	DRM_INFO("bFramesync_Mode = %d\n", pctx_pnl->cus.fs_mode);
	DRM_INFO("bOverDriveEnable = %d\n", pctx_pnl->cus.od_en);
	DRM_INFO("u64OverDrive_Buf_Addr = %llx\n", pctx_pnl->cus.od_buf_addr);
	DRM_INFO("u32OverDrive_Buf_Size = %u\n", pctx_pnl->cus.od_buf_size);
	DRM_INFO("u16VCC_BL_CusCtrl = %d\n", pctx_pnl->cus.vcc_bl_cusctrl);
	DRM_INFO("u16VCC_OffOn_Delay = %d\n", pctx_pnl->cus.vcc_offon_delay);
	DRM_INFO("u16GPIO_Backlight = %d\n", pctx_pnl->cus.gpio_bl);
	DRM_INFO("u16GPIO_VCC = %d\n", pctx_pnl->cus.gpio_vcc);
	DRM_INFO("u16M_delta = %d\n", pctx_pnl->cus.m_del);
	DRM_INFO("escdisp_path_sel = %d\n", pctx_pnl->cus.escdisp_path_sel);

	DRM_INFO("u16CustomerColorPrimaries = %d\n", pctx_pnl->cus.cus_color_prim);
	DRM_INFO("u16SourceWx = %d\n", pctx_pnl->cus.src_wx);
	DRM_INFO("u16SourceWy = %d\n", pctx_pnl->cus.src_wy);
	DRM_INFO("enVideoGFXDispMode = %d\n", pctx_pnl->cus.disp_mode);
	DRM_INFO("u16OSD_Height = %d\n", pctx_pnl->cus.osd_height);
	DRM_INFO("u16OSD_Width = %d\n", pctx_pnl->cus.osd_width);
	DRM_INFO("u16OSD_HsyncStart = %d\n", pctx_pnl->cus.osd_hs_st);
	DRM_INFO("u16OSD_HsyncEnd = %d\n", pctx_pnl->cus.osd_hs_end);
	DRM_INFO("u16OSD_HDEStart = %d\n", pctx_pnl->cus.osd_hde_st);
	DRM_INFO("u16OSD_HDEEnd = %d\n", pctx_pnl->cus.osd_hde_end);
	DRM_INFO("u16OSD_HTotal = %d\n", pctx_pnl->cus.osd_htt);
	DRM_INFO("u16OSD_VsyncStart = %d\n", pctx_pnl->cus.osd_vs_st);
	DRM_INFO("u16OSD_VsyncEnd = %d\n", pctx_pnl->cus.osd_vs_end);
	DRM_INFO("u16OSD_VDEStart = %d\n", pctx_pnl->cus.osd_vde_st);
	DRM_INFO("u16OSD_VDEEnd = %d\n", pctx_pnl->cus.osd_vde_end);
	DRM_INFO("u16OSD_VTotal = %d\n", pctx_pnl->cus.osd_vtt);

	DRM_INFO("PWM port num = %d\n", pctx_pnl->cus.pwmport_num);
	for (i = 0; i < pctx_pnl->cus.pwmport_num; ++i) {
		DRM_INFO("u16PWMPort     (%d) = %d\n", i, pctx_pnl->cus.pwmport[i]);
		DRM_INFO("u16MaxPWMvalue (%d) = %d\n", i, pctx_pnl->cus.max_pwm_val[i]);
		DRM_INFO("u16MinPWMvalue (%d) = %d\n", i, pctx_pnl->cus.min_pwm_val[i]);
		DRM_INFO("u32PeriodPWM   (%d) = %d\n", i, pctx_pnl->cus.period_pwm[i]);
		DRM_INFO("u32DutyPWM     (%d) = %d\n", i, pctx_pnl->cus.duty_pwm[i]);
		DRM_INFO("u16DivPWM      (%d) = %d\n", i, pctx_pnl->cus.div_pwm[i]);
		DRM_INFO("bPolPWM        (%d) = %d\n", i, pctx_pnl->cus.pol_pwm[i]);
		DRM_INFO("bBakclightFreq2Vfreq(%d) = %d\n", i, pctx_pnl->cus.pwm_period_multi[i]);
		DRM_INFO("u32PwmPeriodMulti(%d) = %d\n", i, pctx_pnl->cus.pwm_vsync_enable[i]);
	}

	//swing level control
	DRM_INFO("bSwingCtrlEn = %d\n", pctx_pnl->swing.en);
	DRM_INFO("u16SwingCtrlChs = %d\n", pctx_pnl->swing.ctrl_chs);
	DRM_INFO("u32Swing_Ch3_0 = %d\n", pctx_pnl->swing.ch3_0);
	DRM_INFO("u32Swing_Ch7_4 = %d\n", pctx_pnl->swing.ch7_4);
	DRM_INFO("u32Swing_Ch11_8 = %d\n", pctx_pnl->swing.ch11_8);
	DRM_INFO("u32Swing_Ch15_12 = %d\n", pctx_pnl->swing.ch15_12);
	DRM_INFO("u32Swing_Ch19_16 = %d\n", pctx_pnl->swing.ch19_16);
	DRM_INFO("u32Swing_Ch23_20 = %d\n", pctx_pnl->swing.ch23_20);
	DRM_INFO("u32Swing_Ch27_24 = %d\n", pctx_pnl->swing.ch27_24);
	DRM_INFO("u32Swing_Ch31_28 = %d\n", pctx_pnl->swing.ch31_28);
	// PE control
	DRM_INFO("bPECtrlEn = %d\n", pctx_pnl->pe.en);
	DRM_INFO("u16PECtrlChs = %d\n", pctx_pnl->pe.ctrl_chs);
	DRM_INFO("u32PE_Ch3_0 = %d\n", pctx_pnl->pe.ch3_0);
	DRM_INFO("u32PE_Ch7_4 = %d\n", pctx_pnl->pe.ch7_4);
	DRM_INFO("u32PE_Ch11_8 = %d\n", pctx_pnl->pe.ch11_8);
	DRM_INFO("u32PE_Ch15_12 = %d\n", pctx_pnl->pe.ch15_12);
	DRM_INFO("u32PE_Ch19_16 = %d\n", pctx_pnl->pe.ch19_16);
	DRM_INFO("u32PE_Ch23_20 = %d\n", pctx_pnl->pe.ch23_20);
	DRM_INFO("u32PE_Ch27_24 = %d\n", pctx_pnl->pe.ch27_24);
	DRM_INFO("u32PE_Ch31_28 = %d\n", pctx_pnl->pe.ch31_28);

	//lane order control
	//DRM_INFO("u16LaneOrder_Ch3_0 = %d\n", pctx_pnl->lane_order.ch3_0);
	//DRM_INFO("u16LaneOrder_Ch7_4 = %d\n", pctx_pnl->lane_order.ch7_4);
	//DRM_INFO("u16LaneOrder_Ch11_8 = %d\n", pctx_pnl->lane_order.ch11_8);
	//DRM_INFO("u16LaneOrder_Ch15_12 = %d\n", pctx_pnl->lane_order.ch15_12);
	//DRM_INFO("u16LaneOrder_Ch19_16 = %d\n", pctx_pnl->lane_order.ch19_16);
	//DRM_INFO("u16LaneOrder_Ch23_20 = %d\n", pctx_pnl->lane_order.ch23_20);
	//DRM_INFO("u16LaneOrder_Ch27_24 = %d\n", pctx_pnl->lane_order.ch27_24);
	//DRM_INFO("u16LaneOrder_Ch31_28 = %d\n", pctx_pnl->lane_order.ch31_28);

	//output config
	DRM_INFO("u16OutputCfg_Ch3_0 = %d\n", pctx_pnl->op_cfg.ch3_0);
	DRM_INFO("u16OutputCfg_Ch7_4 = %d\n", pctx_pnl->op_cfg.ch7_4);
	DRM_INFO("u16OutputCfg_Ch11_8 = %d\n", pctx_pnl->op_cfg.ch11_8);
	DRM_INFO("u16OutputCfg_Ch15_12 = %d\n", pctx_pnl->op_cfg.ch15_12);
	DRM_INFO("u16OutputCfg_Ch19_16 = %d\n", pctx_pnl->op_cfg.ch19_16);
	DRM_INFO("u16OutputCfg_Ch23_20 = %d\n", pctx_pnl->op_cfg.ch23_20);
	DRM_INFO("u16OutputCfg_Ch27_24 = %d\n", pctx_pnl->op_cfg.ch27_24);
	DRM_INFO("u16OutputCfg_Ch31_28 = %d\n", pctx_pnl->op_cfg.ch31_28);

	DRM_INFO("Color_Format = %d\n", pctx_pnl->color_info.format);
	DRM_INFO("Rx = %d\n", pctx_pnl->color_info.rx);
	DRM_INFO("Ry = %d\n", pctx_pnl->color_info.ry);
	DRM_INFO("Gx = %d\n", pctx_pnl->color_info.gx);
	DRM_INFO("Gy = %d\n", pctx_pnl->color_info.gy);
	DRM_INFO("Bx = %d\n", pctx_pnl->color_info.bx);
	DRM_INFO("By = %d\n", pctx_pnl->color_info.by);
	DRM_INFO("Wx = %d\n", pctx_pnl->color_info.wx);
	DRM_INFO("Wy = %d\n", pctx_pnl->color_info.wy);
	DRM_INFO("MaxLuminance = %d\n", pctx_pnl->color_info.maxlum);
	DRM_INFO("MedLuminance = %d\n", pctx_pnl->color_info.medlum);
	DRM_INFO("MinLuminance = %d\n", pctx_pnl->color_info.minlum);
	DRM_INFO("LinearRGB = %d\n", pctx_pnl->color_info.linear_rgb);
	DRM_INFO("HDRPanelNits = %d\n", pctx_pnl->color_info.hdrnits);

	//FRC table
	for (FRCType = 0;
		FRCType < (sizeof(pctx_pnl->frc_info)/sizeof(struct drm_st_pnl_frc_table_info));
		FRCType++) {
		DRM_INFO("index = %d, u16LowerBound = %d\n",
			FRCType,
			pctx_pnl->frc_info[FRCType].u16LowerBound);
		DRM_INFO("index = %d, u16HigherBound = %d\n",
			FRCType,
			pctx_pnl->frc_info[FRCType].u16HigherBound);
		DRM_INFO("index = %d, u8FRC_in = %d\n",
			FRCType,
			pctx_pnl->frc_info[FRCType].u8FRC_in);
		DRM_INFO("index = %d, u8FRC_out = %d\n",
			FRCType,
			pctx_pnl->frc_info[FRCType].u8FRC_out);
	}
}

void mtk_render_pnl_set_BFI_En(struct mtk_panel_context *pctx_pnl, bool bEn)
{
	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}

	pctx_pnl->bBFIEn = bEn;
}
EXPORT_SYMBOL(mtk_render_pnl_set_BFI_En);

void mtk_render_pnl_set_Bfi_handle(struct mtk_panel_context *pctx_pnl, uint8_t u8PlaneNum)
{
	#define OLED_DBGSTATUS_BFI 14

	bool bstatus = 0;
	uint32_t u32FrameID = 0;
	static bool s_bBFICheckFirstId = 1;
	static bool s_bBFIBlackFrame;
	static bool s_prelocked_flag;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}

	bstatus = (bool)((drv_hwreg_render_pnl_get_oled_DBG_Status()&
		(0x01<<OLED_DBGSTATUS_BFI))>>OLED_DBGSTATUS_BFI);

	if (((pctx_pnl->gu64panel_debug_log)&(0x01<<E_PNL_BFI_DBGLOG))
		== (0x01<<E_PNL_BFI_DBGLOG))
		DRM_INFO("[%s, %d]: 01 oled_DBG_Status = %d\n", __func__, __LINE__, bstatus);

	if (s_bBFIBlackFrame == true)
		s_bBFIBlackFrame = false;
	else
		s_bBFIBlackFrame = true;

	if ((s_bBFICheckFirstId == true) && (pctx_pnl->outputTiming_info.locked_flag == true)) {
		u32FrameID = pctx_pnl->gu32frameID;
		if ((u32FrameID != 0)) {
			s_bBFIBlackFrame = false;
			s_bBFICheckFirstId = false;
		}
	}

	if ((s_prelocked_flag == false) && (pctx_pnl->outputTiming_info.locked_flag == true))
		s_bBFICheckFirstId = true;

	if (((pctx_pnl->gu64panel_debug_log)&(0x01<<E_PNL_BFI_DBGLOG)) == (0x01<<E_PNL_BFI_DBGLOG))
		DRM_INFO("[%s, %d]: Black frame = %d\n", __func__, __LINE__, s_bBFIBlackFrame);

	s_prelocked_flag = pctx_pnl->outputTiming_info.locked_flag;
	drv_hwreg_render_pnl_set_Oled_Fid_User_Value(s_bBFIBlackFrame);
	if (((pctx_pnl->gu64panel_debug_log)&(0x01<<E_PNL_BFI_DBGLOG)) == (0x01<<E_PNL_BFI_DBGLOG))
		DRM_INFO("[%s, %d]: u8PlaneNum = %d\n", __func__, __LINE__, u8PlaneNum);

	if (u8PlaneNum == 0) // MTK_VPLANE_TYPE_MEMC
		drv_hwreg_render_pnl_set_Oled_Fid_User_En(true);
	else
		drv_hwreg_render_pnl_set_Oled_Fid_User_En(false);

	bstatus = (bool)((drv_hwreg_render_pnl_get_oled_DBG_Status()&(0x01<<OLED_DBGSTATUS_BFI))
		>>OLED_DBGSTATUS_BFI);
	if (((pctx_pnl->gu64panel_debug_log)&(0x01<<E_PNL_BFI_DBGLOG)) == (0x01<<E_PNL_BFI_DBGLOG))
		DRM_INFO("[%s, %d]: 02 oled_DBG_Status = %d\n", __func__, __LINE__, bstatus);

}
EXPORT_SYMBOL(mtk_render_pnl_set_Bfi_handle);


static int _parse_cus_setting_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np;
	int ret = 0, num = 0;
	const char *pnl_name;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_CUS_SETTING_NODE);
	if (np != NULL) {

		ret = of_property_read_string(np, GAMMA_BIN_TAG, &(pctx_pnl->cus.pgamma_path));
		if (ret != 0x0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, GAMMA_BIN_TAG);
			return ret;
		}

		ret = of_property_read_string(np, TCON_BIN_TAG, &(pctx_pnl->cus.tcon_path));
		if (ret != 0x0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, TCON_BIN_TAG);
			return ret;
		}

		ret = of_property_read_string(np, PANEL_NAME_TAG, &pnl_name);
		if (ret != 0x0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, PANEL_NAME_TAG);
			return ret;
		}
		memset(pctx_pnl->cus.panel_name, 0, DRM_NAME_MAX_NUM);
		strncpy(pctx_pnl->cus.panel_name, pnl_name, DRM_NAME_MAX_NUM - 1);
		pctx_pnl->cus.dtsi_file_type = get_dts_u32(np, DTSI_FILE_TYPE);
		pctx_pnl->cus.dtsi_file_ver = get_dts_u32(np, DTSI_FILE_VER);
		pctx_pnl->cus.pgamma_en = get_dts_u32(np, GAMMA_EN_TAG);
		pctx_pnl->cus.tcon_en = get_dts_u32(np, TCON_EN_TAG);
		pctx_pnl->cus.mirror_mode = get_dts_u32(np, MIRROR_TAG);
		pctx_pnl->cus.op_format = get_dts_u32(np, OUTPUT_FORMAT_TAG);
		pctx_pnl->cus.fs_mode = get_dts_u32(np, FS_MODE_TAG);
		pctx_pnl->cus.od_en = get_dts_u32(np, OD_EN_TAG);
		pctx_pnl->cus.od_buf_addr = get_dts_u32(np, OD_ADDR_TAG);
		pctx_pnl->cus.od_buf_size = get_dts_u32(np, OD_BUF_SIZE_TAG);
		pctx_pnl->cus.vcc_bl_cusctrl = get_dts_u32(np, VCC_BL_CUSCTRL_TAG);
		pctx_pnl->cus.vcc_offon_delay = get_dts_u32(np, VCC_OFFON_DELAY_TAG);
		pctx_pnl->cus.escdisp_path_sel = get_dts_u32(np, SCDISP_PATH_SEL_TAG);
		pctx_pnl->cus.force_free_run_en = get_dts_u32(np, FORCE_FREE_RUN_EN_TAG);
		pctx_pnl->cus.gpio_bl = get_dts_u32(np, GPIO_BL_TAG);
		pctx_pnl->cus.gpio_vcc = get_dts_u32(np, GPIO_VCC_TAG);
		pctx_pnl->cus.m_del = get_dts_u32(np, M_DELTA_TAG);
		pctx_pnl->cus.cus_color_prim = get_dts_u32(np, COLOR_PRIMA_TAG);
		pctx_pnl->cus.src_wx = get_dts_u32(np, SOURCE_WX_TAG);
		pctx_pnl->cus.src_wy = get_dts_u32(np, SOURCE_WY_TAG);
		pctx_pnl->cus.disp_mode = get_dts_u32(np, VG_DISP_TAG);
		pctx_pnl->cus.osd_height = get_dts_u32(np, OSD_HEIGHT_TAG);
		pctx_pnl->cus.osd_width = get_dts_u32(np, OSD_WIDTH_TAG);
		pctx_pnl->cus.osd_hs_st = get_dts_u32(np, OSD_HS_START_TAG);
		pctx_pnl->cus.osd_hs_end = get_dts_u32(np, OSD_HS_END_TAG);
		pctx_pnl->cus.osd_hde_st = get_dts_u32(np, OSD_HDE_START_TAG);
		pctx_pnl->cus.osd_hde_end = get_dts_u32(np, OSD_HDE_END_TAG);
		pctx_pnl->cus.osd_htt = get_dts_u32(np, OSD_HTOTAL_TAG);
		pctx_pnl->cus.osd_vs_st = get_dts_u32(np, OSD_VS_START_TAG);
		pctx_pnl->cus.osd_vs_end = get_dts_u32(np, OSD_VS_END_TAG);
		pctx_pnl->cus.osd_vde_st = get_dts_u32(np, OSD_VDE_START_TAG);
		pctx_pnl->cus.osd_vde_end = get_dts_u32(np, OSD_VDE_END_TAG);
		pctx_pnl->cus.osd_vtt = get_dts_u32(np, OSD_VTOTAL_TAG);
		pctx_pnl->cus.game_direct_framerate_mode = get_dts_u32(np, GAME_DIRECT_FR_MODE_TAG);
		pctx_pnl->cus.inch_size = get_dts_u32(np, INCH_SIZE_TAG);
		pctx_pnl->cus.pwm_to_bl_delay = get_dts_u32(np, PWM_TO_BL_DELAY_TAG);
		pctx_pnl->cus.bl_to_pwm_delay = get_dts_u32(np, BL_TO_PWM_DELAY_TAG);
		pctx_pnl->cus.ctrl_bit_en = get_dts_u32(np, CTRL_BIT_EN_TAG);

		// backlight
		num = get_dts_u32_elems(np, PWM_PORT_TAG);
		if (num <= 0 || num >= PWM_PORT_MAX_NUM) {
			DRM_ERROR("Too many PWM port, num(%u) > max(%u)", num, PWM_PORT_MAX_NUM);
			num = PWM_PORT_MAX_NUM - 1;
		}
		pctx_pnl->cus.pwmport_num = num;
		get_dts_u32_array(np, PWM_PORT_TAG,      pctx_pnl->cus.pwmport,     num);
		get_dts_u32_array(np, MAX_PWM_VALUE_TAG, pctx_pnl->cus.max_pwm_val, num);
		get_dts_u32_array(np, MIN_PWM_VALUE_TAG, pctx_pnl->cus.min_pwm_val, num);
		get_dts_u32_array(np, PERIOD_PWM_TAG,    pctx_pnl->cus.period_pwm,  num);
		get_dts_u32_array(np, DUTY_PWM_TAG,      pctx_pnl->cus.duty_pwm,    num);
		get_dts_u32_array(np, SHIFT_PWM_TAG,     pctx_pnl->cus.shift_pwm,   num);
		get_dts_u32_array(np, DIV_PWM_TAG,       pctx_pnl->cus.div_pwm,     num);
		get_dts_u32_array(np, POL_PWM_TAG,       pctx_pnl->cus.pol_pwm,     num);
		get_dts_u32_array(np, PERIOD_MULTI_TAG,  pctx_pnl->cus.pwm_period_multi, num);
		get_dts_u32_array(np, PWM_VSYNC_ENABLE_TAG, pctx_pnl->cus.pwm_vsync_enable, num);

		pctx_pnl->cus.hmirror_en = (pctx_pnl->cus.mirror_mode == E_PNL_MIRROR_H) ||
			(pctx_pnl->cus.mirror_mode == E_PNL_MIRROR_V_H);

		if (pctx_pnl->cus.tcon_en) {
			ret = of_property_read_string(np, PANEL_ID_STRING_TAG,
						&(pctx_pnl->cus.panel_id_str));
			if (ret != 0x0) {
				DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, PANEL_ID_STRING_TAG);
				return ret;
			}
			pctx_pnl->cus.overdrive_en = get_dts_u32(np, OVERDIRVE_ENABLE_TAG);
			pctx_pnl->cus.panel_index = get_dts_u32(np, PANEL_INDEX_TAG);
			pctx_pnl->cus.chassis_index = get_dts_u32(np, CHASSIS_INDEX_TAG);
			pctx_pnl->cus.vcom_pattern = get_dts_u32(np, VCOM_PATTERN_TAG);
			pctx_pnl->cus.vcom_type = get_dts_u32(np, VCOM_TYPE_TAG);
			pctx_pnl->cus.spread_permillage = get_dts_u32(np, SPREAD_PERMILLAGE_TAG);
			pctx_pnl->cus.spread_freq = get_dts_u32(np, SPREAD_FREQ_TAG);
			pctx_pnl->cus.ocell_demura_idx = get_dts_u32(np, OCELL_DEMURA_IDX_TAG);
			pctx_pnl->cus.tcon_index = get_dts_u32(np, TCON_INDEX_TAG);
			pctx_pnl->cus.current_max = get_dts_u32(np, CURRENT_MAX_TAG);
			pctx_pnl->cus.backlight_curr_freq = get_dts_u32(np, BL_CUR_FREQ_TAG);
			pctx_pnl->cus.backlight_pwm_freq = get_dts_u32(np, BL_PWM_FREQ_TAG);
			pctx_pnl->cus.backlight_wait_logo = get_dts_u32(np, BL_WAIT_LOGO_TAG);
			pctx_pnl->cus.backlight_control_if = get_dts_u32(np, BL_CONTROL_IF_TAG);
		}

		pctx_pnl->cus.render_in_color_format = get_dts_u32(np, RENDER_IN_COLOR_FORMAT_TAG);
		pctx_pnl->cus.render_in_color_range = get_dts_u32(np, RENDER_IN_COLOR_RANGE_TAG);
		pctx_pnl->cus.render_in_color_space = get_dts_u32(np, RENDER_IN_COLOR_SPACE_TAG);

		//DLG
		pctx_pnl->cus.dlg_en = get_dts_u32(np, DLG_ENABLE_TAG);
		pctx_pnl->cus.dlg_gpio_CustCtrl = get_dts_u32(np, DLG_GPIO_CUS_CTRL_TAG);
		pctx_pnl->cus.dlg_i2c_CustCtrl = get_dts_u32(np, DLG_I2C_CUS_CTRL_TAG);
		pctx_pnl->cus.dlg_i2c_sec_CustCtrl = get_dts_u32(np, DLG_I2C_SEC_CUS_CTRL_TAG);
		pctx_pnl->cus.dlg_acon_mem_enable = get_dts_u32(np, DLG_ACON_MEM_ENABLE_TAG);
		if (pctx_pnl->cus.dlg_en) {
			DRM_INFO("[%s, %d]: dlg_en=%d, dlg_gpio_custCtrl=%d\n",
					__func__, __LINE__, pctx_pnl->cus.dlg_en,
					pctx_pnl->cus.dlg_gpio_CustCtrl);
			DRM_INFO("[%s, %d]: dlg_i2c_custctrl=%d, dlg_acon_mem_enable=%d\n",
					__func__, __LINE__, pctx_pnl->cus.dlg_i2c_CustCtrl,
					pctx_pnl->cus.dlg_acon_mem_enable);
		}
		pctx_pnl->cus.global_mute_backlight_ignore =
				get_dts_u32(np, GLOBAL_MUTE_BACKLIGHT_IGNORE_TAG);
		//Global mute delay(ms)
		pctx_pnl->cus.global_mute_delay = get_dts_u32(np, GLOBAL_MUTE_DELAY_TAG);
		pctx_pnl->cus.global_unmute_delay = get_dts_u32(np, GLOBAL_UNMUTE_DELAY_TAG);

		pctx_pnl->cus.pmic_seq_CusCtrl = get_dts_u32(np, PMIC_SEQ_CUSCTRL_TAG);
		pctx_pnl->cus.pwr_on_to_pmic_dly = get_dts_u32(np, PWR_ON_TO_PMIC_DLY_TAG);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_CUS_SETTING_NODE);
	}

	return ret;
}

static int _parse_tconless_model_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np;
	int ret = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TCONLESS_INFO_NODE);
	if (np != NULL) {
		ret = of_property_read_string(
				np,
				HIGT_FRAME_RATE_BIN_PATH,
				&(pctx_pnl->tconless_model_info.higt_frame_rate_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, HIGT_FRAME_RATE_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				HIGT_PIXEL_CLOCK_BIN_PATH,
				&(pctx_pnl->tconless_model_info.higt_pixel_clock_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, HIGT_PIXEL_CLOCK_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				GAME_MODE_BIN_PATH,
				&(pctx_pnl->tconless_model_info.game_mode_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, GAME_MODE_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				HIGT_FRAME_RATE_GAMMA_BIN_PATH,
				&(pctx_pnl->tconless_model_info.higt_frame_rate_gamma_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, HIGT_FRAME_RATE_GAMMA_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				HIGT_PIXEL_CLOCK_GAMMA_BIN_PATH,
				&(pctx_pnl->tconless_model_info.higt_pixel_clock_gamma_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, HIGT_PIXEL_CLOCK_GAMMA_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				GAME_MODE_GAMMA_BIN_PATH,
				&(pctx_pnl->tconless_model_info.game_mode_gamma_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, GAME_MODE_GAMMA_BIN_PATH);
			return ret;
		}

		/* parsing power sequence on/off bin path */
		ret = of_property_read_string(
				np,
				POWER_SEQ_ON_BIN_PATH,
				&(pctx_pnl->tconless_model_info.power_seq_on_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, POWER_SEQ_ON_BIN_PATH);
			return ret;
		}

		ret = of_property_read_string(
				np,
				POWER_SEQ_OFF_BIN_PATH,
				&(pctx_pnl->tconless_model_info.power_seq_off_bin_path));

		if (ret != 0) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, POWER_SEQ_OFF_BIN_PATH);
			return ret;
		}
	}

	return ret;
}

static int _parse_spread_spectrum_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_SSC_INFO);
	if (np != NULL) {
		pctx_pnl->stdrmSSC.ssc_setting.ssc_en = get_dts_u32(np, SSC_EN);
		// modulation: 1Khz for property, 0.1Khz in render DD
		// deviation: 0.01% for property, 0.01% in render DD
		pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation =
			get_dts_u32(np, SSC_MODULATION) * DEC_10;
		pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation = get_dts_u32(np, SSC_DEVIATION);
		pctx_pnl->stdrmSSC.ssc_protect_by_user_en = get_dts_u32(np, SSC_PROTECT_BY_USER_EN);
		pctx_pnl->stdrmSSC.ssc_modulation_protect_value =
			get_dts_u32(np, SSC_MODULATION_PROTECT_VALUE);
		pctx_pnl->stdrmSSC.ssc_deviation_protect_value =
			get_dts_u32(np, SSC_DEVIATION_PROTECT_VALUE);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_SSC_INFO);
	}

	_mtk_tcon_update_ssc(pctx_pnl);

	return ret;
}


static int _parse_swing_level_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	int index = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_OUTPUT_SWING_INFO);
	if (np != NULL) {
		pctx_pnl->stdrmSwing.usr_swing_level = get_dts_u32(np, SWING_USER_DEF);
		pctx_pnl->stdrmSwing.common_swing = get_dts_u32(np, SWING_COMMON);
		pctx_pnl->stdrmSwing.ctrl_lanes = get_dts_u32(np, SWING_CTRL_LANE);
		if (pctx_pnl->stdrmSwing.common_swing == 1)
			pctx_pnl->stdrmSwing.swing_level[0] = get_dts_u32(np, SWING_LEVEL);
		else
			for (index = 0; index < pctx_pnl->stdrmSwing.ctrl_lanes; index++)
				pctx_pnl->stdrmSwing.swing_level[index] =
					get_dts_u32_index(np, SWING_LEVEL, index);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_OUTPUT_SWING_INFO);
	}

	return ret;
}

static int _parse_output_pre_emphsis_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	int index = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_OUTPUT_PRE_EMPHASIS);
	if (np != NULL) {
		pctx_pnl->stdrmPE.usr_pe_level = get_dts_u32(np, PE_USER_DEF);
		pctx_pnl->stdrmPE.common_pe = get_dts_u32(np, PE_COMMON);
		pctx_pnl->stdrmPE.ctrl_lanes = get_dts_u32(np, PE_CTRL_LANE);
		if (pctx_pnl->stdrmPE.common_pe == 1)
			pctx_pnl->stdrmPE.pe_level[0] = get_dts_u32(np, PE_LEVEL);
		else
			for (index = 0; index < pctx_pnl->stdrmPE.ctrl_lanes; index++)
				pctx_pnl->stdrmPE.pe_level[index] =
					get_dts_u32_index(np, PE_LEVEL, index);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_OUTPUT_PRE_EMPHASIS);
	}

	return ret;
}

static int _parse_pe_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}
	np = of_find_node_by_name(NULL, PNL_OUTPUT_PE_NODE);
	if (np != NULL) {

		pctx_pnl->pe.en = get_dts_u32(np, PE_EN_TAG);
		pctx_pnl->pe.ctrl_chs = get_dts_u32(np, PE_CHS_TAG);
		pctx_pnl->pe.ch3_0 = get_dts_u32(np, PE_CH3_0_TAG);
		pctx_pnl->pe.ch7_4 = get_dts_u32(np, PE_CH7_4_TAG);
		pctx_pnl->pe.ch11_8 = get_dts_u32(np, PE_CH11_8_TAG);
		pctx_pnl->pe.ch15_12 = get_dts_u32(np, PE_CH15_12_TAG);
		pctx_pnl->pe.ch19_16 = get_dts_u32(np, PE_CH19_16_TAG);
		pctx_pnl->pe.ch23_20 = get_dts_u32(np, PE_CH23_20_TAG);
		pctx_pnl->pe.ch27_24 = get_dts_u32(np, PE_CH27_24_TAG);
		pctx_pnl->pe.ch31_28 = get_dts_u32(np, PE_CH31_28_TAG);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_OUTPUT_PE_NODE);
	}

	return ret;
}

static int _parse_lane_order_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}
	np = of_find_node_by_name(NULL, PNL_LANE_ORDER_NODE);
	if (np != NULL) {

		pctx_pnl->lane_order.ch3_0 = get_dts_u32(np, LANE_CH3_0_TAG);
		pctx_pnl->lane_order.ch7_4 = get_dts_u32(np, LANE_CH7_4_TAG);
		pctx_pnl->lane_order.ch11_8 = get_dts_u32(np, LANE_CH11_8_TAG);
		pctx_pnl->lane_order.ch15_12 = get_dts_u32(np, LANE_CH15_12_TAG);
		pctx_pnl->lane_order.ch19_16 = get_dts_u32(np, LANE_CH19_16_TAG);
		pctx_pnl->lane_order.ch23_20 = get_dts_u32(np, LANE_CH23_20_TAG);
		pctx_pnl->lane_order.ch27_24 = get_dts_u32(np, LANE_CH27_24_TAG);
		pctx_pnl->lane_order.ch31_28 = get_dts_u32(np, LANE_CH31_28_TAG);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_LANE_ORDER_NODE);
	}

	return ret;
}

static int _parse_output_lane_order_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	int index;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_PKG_LANE_ORDER_NODE);
	if (np != NULL) {
		pctx_pnl->out_lane_info.sup_lanes = get_dts_u32(np, SUPPORT_LANES);

		for (index = 0; index < pctx_pnl->out_lane_info.sup_lanes; index++) {
			pctx_pnl->out_lane_info.def_layout[index] =
				get_dts_u32_index(np, LAYOUT_ORDER,	index);
			}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_PKG_LANE_ORDER_NODE);
		ret = -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_USR_LANE_ORDER_NODE);
	if (np != NULL) {
		pctx_pnl->out_lane_info.usr_defined = get_dts_u32(np, USR_DEFINE);
		pctx_pnl->out_lane_info.ctrl_lanes = get_dts_u32(np, CTRL_LANES);
		for (index = 0; index < pctx_pnl->out_lane_info.ctrl_lanes; index++) {
			pctx_pnl->out_lane_info.lane_order[index] =
				get_dts_u32_index(np, LANEORDER, index);
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_USR_LANE_ORDER_NODE);
		ret = -ENODEV;
	}
	return ret;
}

static int _parse_op_cfg_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}
	np = of_find_node_by_name(NULL, PNL_OUTPUT_CONFIG_NODE);
	if (np != NULL) {

		pctx_pnl->op_cfg.ch3_0 = get_dts_u32(np, OUTCFG_CH3_0_TAG);
		pctx_pnl->op_cfg.ch7_4 = get_dts_u32(np, OUTCFG_CH7_4_TAG);
		pctx_pnl->op_cfg.ch11_8 = get_dts_u32(np, OUTCFG_CH11_8_TAG);
		pctx_pnl->op_cfg.ch15_12 = get_dts_u32(np, OUTCFG_CH15_12_TAG);
		pctx_pnl->op_cfg.ch19_16 = get_dts_u32(np, OUTCFG_CH19_16_TAG);
		pctx_pnl->op_cfg.ch23_20 = get_dts_u32(np, OUTCFG_CH23_20_TAG);
		pctx_pnl->op_cfg.ch27_24 = get_dts_u32(np, OUTCFG_CH27_24_TAG);
		pctx_pnl->op_cfg.ch31_28 = get_dts_u32(np, OUTCFG_CH31_28_TAG);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_OUTPUT_CONFIG_NODE);
	}

	return ret;
}

static int _parse_panel_color_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}
	np = of_find_node_by_name(NULL, PNL_COLOR_INFO_NODE);
	if (np != NULL) {

		pctx_pnl->color_info.format = get_dts_u32(np, COLOR_FORMAT_TAG);
		pctx_pnl->color_info.rx = get_dts_u32(np, RX_TAG);
		pctx_pnl->color_info.ry = get_dts_u32(np, RY_TAG);
		pctx_pnl->color_info.gx = get_dts_u32(np, GX_TAG);
		pctx_pnl->color_info.gy = get_dts_u32(np, GY_TAG);
		pctx_pnl->color_info.bx = get_dts_u32(np, BX_TAG);
		pctx_pnl->color_info.by = get_dts_u32(np, BY_TAG);
		pctx_pnl->color_info.wx = get_dts_u32(np, WX_TAG);
		pctx_pnl->color_info.wy = get_dts_u32(np, WY_TAG);
		pctx_pnl->color_info.maxlum = get_dts_u32(np, MAX_LUMI_TAG);
		pctx_pnl->color_info.medlum = get_dts_u32(np, MED_LUMI_TAG);
		pctx_pnl->color_info.minlum = get_dts_u32(np, MIN_LUMI_TAG);
		pctx_pnl->color_info.linear_rgb = get_dts_u32(np, LINEAR_RGB_TAG);
		pctx_pnl->color_info.hdrnits = get_dts_u32(np, HDR_NITS_TAG);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_COLOR_INFO_NODE);
	}

	return ret;
}

static int _parse_frc_tbl_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np;
	int ret = 0, FRClen = 0, FRCType = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	//Get FRC table info
	np = of_find_node_by_name(NULL, PNL_FRC_TABLE_NODE);
	if (np != NULL) {
		DRM_INFO("[%s, %d]: find node success, name = %s\n",
			__func__, __LINE__, PNL_FRC_TABLE_NODE);

		//ret = of_get_property(np, FRC_TABLE_TAG, &FRClen);
		if (of_get_property(np, FRC_TABLE_TAG, &FRClen) == NULL) {
			DRM_INFO("[%s, %d]: read string failed, name = %s\n",
				__func__, __LINE__, FRC_TABLE_TAG);
			return -1;
		}
		for (FRCType = 0;
			FRCType < (FRClen / (sizeof(__u32) * PNL_FRC_TABLE_ARGS_NUM));
			FRCType++) {

			DRM_INFO("[%s, %d]: get_dts_u32_index, FRCType = %d\n",
				__func__, __LINE__, FRCType);
			pctx_pnl->frc_info[FRCType].u16LowerBound =
				get_dts_u32_index(np,
				FRC_TABLE_TAG,
				FRCType*PNL_FRC_TABLE_ARGS_NUM + 0);
			pctx_pnl->frc_info[FRCType].u16HigherBound =
				get_dts_u32_index(np,
				FRC_TABLE_TAG,
				FRCType*PNL_FRC_TABLE_ARGS_NUM + 1);
			pctx_pnl->frc_info[FRCType].u8FRC_in =
				get_dts_u32_index(np,
				FRC_TABLE_TAG,
				FRCType*PNL_FRC_TABLE_ARGS_NUM + 2);
			pctx_pnl->frc_info[FRCType].u8FRC_out =
				get_dts_u32_index(np,
				FRC_TABLE_TAG,
				FRCType*PNL_FRC_TABLE_ARGS_NUM + 3);
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_FRC_TABLE_NODE);
		ret = -ENODATA;
	}

	return ret;

}

static int _parse_hw_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	memset(&pctx_pnl->hw_info, 0, sizeof(drm_st_hw_info));
	np = of_find_node_by_name(NULL, PNL_HW_INFO_NODE);
	if (!np) {
		DRM_ERROR("[%s, %d]: coundn't find node<%s>\n",
			__func__, __LINE__, PNL_HW_INFO_NODE);
		return -ENODEV;
	}

	pctx_pnl->hw_info.pnl_lib_version = get_dts_u32(np, PNL_LIB_VER);
	pctx_pnl->hw_info.rcon_enable = get_dts_u32(np, RCON_ENABLE);
	pctx_pnl->hw_info.rcon_max = get_dts_u32(np, RCON_MAX);
	pctx_pnl->hw_info.rcon_min = get_dts_u32(np, RCON_MIN);
	pctx_pnl->hw_info.rcon_value = get_dts_u32(np, RCON_VALUE);

	pctx_pnl->hw_info.biascon_single_max =
		get_dts_u32(np, BIASCON_SINGLE_MAX);

	pctx_pnl->hw_info.biascon_single_min =
		get_dts_u32(np, BIASCON_SINGLE_MIN);

	pctx_pnl->hw_info.biascon_single_value =
		get_dts_u32(np, BIASCON_SINGLE_VALUE);

	pctx_pnl->hw_info.biascon_double_max =
		get_dts_u32(np, BIASCON_BIASCON_DOUBLE_MAX);

	pctx_pnl->hw_info.biascon_double_min =
		get_dts_u32(np, BIASCON_BIASCON_DOUBLE_MIN);

	pctx_pnl->hw_info.biascon_double_value =
		get_dts_u32(np, BIASCON_BIASCON_DOUBLE_VALUE);

	pctx_pnl->hw_info.rint_enable = get_dts_u32(np, RINT_ENABLE);
	pctx_pnl->hw_info.rint_max = get_dts_u32(np, RINT_MAX);
	pctx_pnl->hw_info.rint_min = get_dts_u32(np, RINT_MIN);
	pctx_pnl->hw_info.rint_value = get_dts_u32(np, RINT_VALUE);
	of_node_put(np);

	ret = drv_hwreg_render_pnl_version_initial(pctx_pnl->hw_info.pnl_lib_version);
	DRM_INFO("%s: pnl ver 0x%x\n", __func__, pctx_pnl->hw_info.pnl_lib_version);

	return ret;
}

static int _parse_ext_graph_combo_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	memset(&pctx_pnl->ext_grpah_combo_info, 0, sizeof(drm_st_ext_graph_combo_info));
	np = of_find_node_by_name(NULL, PNL_COMBO_INFO_NODE);
	if (np != NULL) {

		pctx_pnl->ext_grpah_combo_info.graph_vbo_byte_mode =
			get_dts_u32(np, GRAPH_VBO_BYTE_MODE);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_COMBO_INFO_NODE);
	}

	return ret;
}

static int _parse_panel_dither_info(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct device_node *np;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_DITHER_INFO_NODE);
	if (np != NULL) {
		pctx_pnl->dither_info.dither_pattern = get_dts_u32(np, DITHER_PATTERN);
		pctx_pnl->dither_info.dither_depth = get_dts_u32(np, DITHER_DEPTH);
		pctx_pnl->dither_info.dither_capability = get_dts_u32(np, DITHER_CAPABILITY);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_DITHER_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;
}

void dump_tcon_i2c_info(struct mtk_panel_context *pctx_pnl)
{
	int i = 0, count = 0;
	struct device_node *np = NULL;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}

	np = of_find_node_by_name(NULL, PNL_TCON_I2C_INFO_NODE);
	if (np != NULL) {
		count = of_property_count_strings(np, "tcon_i2c_name_list");
		for (i = 0; i < count; i++) {
			DRM_INFO("i2c_cmd_id = %d\n", i);
			DRM_INFO("i2c_bus = %d\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_bus);
			DRM_INFO("i2c_mode = %d\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_mode);
			DRM_INFO("i2c_dev_addr = %x\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_dev_addr);
			DRM_INFO("i2c_reg_offst = %x\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_reg_offst);
			DRM_INFO("i2c_info_length = %x\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_length);
			DRM_INFO("i2c_info_row = %d\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_row);
			DRM_INFO("i2c_info_column = %x\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_column_num[0]);
			DRM_INFO("i2c_info_data = %x\n",
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_data[0]);
		}
	}

	if (pctx_pnl->cus.dlg_i2c_sec_CustCtrl) {
		np = of_find_node_by_name(NULL, PNL_TCON_I2C_SEC_INFO_NODE);
		if (np != NULL) {
			count = of_property_count_strings(np, "tcon_i2c_sec_name_list");
			for (i = 0; i < count; i++) {
				DRM_INFO("i2c_sec_cmd_id = %d\n", i);
				DRM_INFO("i2c_sec_bus = %d\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_bus);
				DRM_INFO("i2c_sec_mode = %d\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_mode);
				DRM_INFO("i2c_sec_dev_addr = %x\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_dev_addr);
				DRM_INFO("i2c_sec_reg_offst = %x\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_reg_offst);
				DRM_INFO("i2c_sec_info_length = %x\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_length);
				DRM_INFO("i2c_sec_info_row = %d\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_row);
				DRM_INFO("i2c_sec_info_column = %x\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_column_num[0]);
				DRM_INFO("i2c_sec_info_data = %x\n",
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_data[0]);
			}
		}
	}
}

static int _parse_panel_tcon_i2c_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np = NULL;
	struct device_node *infoNp = NULL;
	struct device_node *tcon_info_np = NULL;
	int ret = 0, count = 0;
	int i = 0, j = 0, k = 0;
	const char *tconI2CName = NULL;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TCON_I2C_INFO_NODE);
	if (np != NULL) {
		count = of_property_count_strings(np, "tcon_i2c_name_list");
		tcon_i2c_count = count;
		pctx_pnl->cus.tcon_i2c_info =
					kzalloc(
					(sizeof(struct drm_st_tcon_i2c_info) * count), GFP_KERNEL);

		for (i = 0; i < count; i++) {
			of_property_read_string_index(np, "tcon_i2c_name_list", i, &tconI2CName);
			infoNp = of_find_node_by_name(np, tconI2CName);
			if (infoNp != NULL && of_device_is_available(infoNp)) {
				pctx_pnl->cus.tcon_i2c_info[i].i2c_bus =
					get_dts_u32(infoNp, TCON_I2C_BUS_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_mode =
					get_dts_u32(infoNp, TCON_I2C_MODE_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_dev_addr =
					get_dts_u32(infoNp, TCON_I2C_DEV_ADDR_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_reg_offst =
					get_dts_u32(infoNp, TCON_I2C_REG_OFFSET_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_length =
					get_dts_u32(infoNp, TCON_I2C_INFO_LENGTH_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_row =
					get_dts_u32(infoNp, TCON_I2C_INFO_ROW_TAG);
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_column_num =
					kzalloc(
					(pctx_pnl->cus.tcon_i2c_info[i].i2c_info_row *
					sizeof(uint8_t)), GFP_KERNEL);
				for (j = 0; j < pctx_pnl->cus.tcon_i2c_info[i].i2c_info_row; j++) {
					pctx_pnl->cus.tcon_i2c_info[i].i2c_info_column_num[j] =
						get_dts_u32_index(
						infoNp, TCON_I2C_INFO_COLUMN_NUM_TAG, j);
				}
				pctx_pnl->cus.tcon_i2c_info[i].i2c_info_data =
					kzalloc(
					(pctx_pnl->cus.tcon_i2c_info[i].i2c_info_length *
					sizeof(uint8_t)), GFP_KERNEL);
				for (k = 0;
					k < pctx_pnl->cus.tcon_i2c_info[i].i2c_info_length;
					k++) {
					pctx_pnl->cus.tcon_i2c_info[i].i2c_info_data[k] =
						get_dts_u32_index(
						infoNp, TCON_I2C_INFO_DATA_TAG, k);
				}
			}
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_TCON_I2C_INFO_NODE);
	}

	tcon_info_np = of_find_node_by_name(NULL, PNL_CUST_TCON_INFO_NODE);
	if (tcon_info_np != NULL) {
		pctx_pnl->cus.i2c_wp_CustCtrl =
			get_dts_u32(tcon_info_np, TCON_CUST_SUPPORT_I2C_WP_TAG);
		pctx_pnl->cus.i2c_wp_on =
			get_dts_u32(tcon_info_np, TCON_CUST_GPIO_TCON_I2C_WP_ON_TAG);
		pctx_pnl->cus.i2c_wp_off =
			get_dts_u32(tcon_info_np, TCON_CUST_GPIO_TCON_I2C_WP_OFF_TAG);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_CUST_TCON_INFO_NODE);
	}

	if (pctx_pnl->cus.dlg_i2c_sec_CustCtrl) {
		np = of_find_node_by_name(NULL, PNL_TCON_I2C_SEC_INFO_NODE);
		if (np != NULL) {
			count = of_property_count_strings(np, "tcon_i2c_sec_name_list");
			tcon_i2c_sec_count = count;
			pctx_pnl->cus.tcon_i2c_sec_info =
						kzalloc(
						(sizeof(struct drm_st_tcon_i2c_info) * count), GFP_KERNEL);

			for (i = 0; i < count; i++) {
				of_property_read_string_index(np, "tcon_i2c_sec_name_list", i, &tconI2CName);
				infoNp = of_find_node_by_name(np, tconI2CName);
				if (infoNp != NULL && of_device_is_available(infoNp)) {
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_bus =
						get_dts_u32(infoNp, TCON_I2C_SEC_BUS_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_mode =
						get_dts_u32(infoNp, TCON_I2C_SEC_MODE_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_dev_addr =
						get_dts_u32(infoNp, TCON_I2C_SEC_DEV_ADDR_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_reg_offst =
						get_dts_u32(infoNp, TCON_I2C_SEC_REG_OFFSET_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_length =
						get_dts_u32(infoNp, TCON_I2C_SEC_INFO_LENGTH_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_row =
						get_dts_u32(infoNp, TCON_I2C_SEC_INFO_ROW_TAG);
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_column_num =
						kzalloc(
						(pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_row *
						sizeof(uint8_t)), GFP_KERNEL);
					for (j = 0; j < pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_row; j++) {
						pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_column_num[j] =
							get_dts_u32_index(
							infoNp, TCON_I2C_SEC_INFO_COLUMN_NUM_TAG, j);
					}
					pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_data =
						kzalloc(
						(pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_length *
						sizeof(uint8_t)), GFP_KERNEL);
					for (k = 0;
						k < pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_length;
						k++) {
						pctx_pnl->cus.tcon_i2c_sec_info[i].i2c_info_data[k] =
							get_dts_u32_index(
							infoNp, TCON_I2C_SEC_INFO_DATA_TAG, k);
					}
				}
			}
		} else {
			DRM_INFO("[%s, %d]: find node failed, name = %s\n",
						__func__, __LINE__, PNL_TCON_I2C_SEC_INFO_NODE);
		}
	}

	dump_tcon_i2c_info(pctx_pnl);

	return ret;
}

void dump_tcon_vrr_od_info(struct mtk_panel_context *pctx_pnl)
{
	int i = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}

	DRM_INFO("VRR_OD_table_Total = %d\n",
				pctx_pnl->VRR_OD_info->u8VRR_OD_table_Total);

	for (i = 0; i < pctx_pnl->VRR_OD_info->u8VRR_OD_table_Total; i++) {
		DRM_INFO("VRR_OD_table_id = %d, i = %d\n",
			pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16Index, i);
		DRM_INFO("VRR_OD_Enable = %d\n",
			pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].bEnable);
		DRM_INFO("VRR_OD_FreqHighBound = %d\n",
			pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16FreqHighBound);
		DRM_INFO("VRR_OD_FreqLowBound = %d\n",
			pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16FreqLowBound);
		DRM_INFO("VRR_OD_table = %s\n",
			pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name);
	}
}

static int _parse_panel_tcon_vrr_od_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np = NULL;
	struct device_node *infoNp = NULL;
	int ret = 0, count = 0;
	int i = 0;
	const char *tcon_vrr_od_tab_Name = NULL;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TCON_VRR_OD_INFO_NODE);
	if (np != NULL) {
		pctx_pnl->vrr_od_En = get_dts_u32(np, VRR_OD_EN_TAG);

		/*parse VRR OD info when vrr_od_En is true*/
		if (pctx_pnl->vrr_od_En) {
			count = of_property_count_strings(np, "vrr_od_table_list");
			pctx_pnl->VRR_OD_info =
						kzalloc(
						(sizeof(drm_st_VRR_OD_info)), GFP_KERNEL);

			pctx_pnl->VRR_OD_info->u8VRR_OD_table_Total = count;
			for (i = 0; i < count; i++) {
				of_property_read_string_index(np, "vrr_od_table_list", i,
					&tcon_vrr_od_tab_Name);
				infoNp = of_find_node_by_name(np, tcon_vrr_od_tab_Name);
				if (infoNp != NULL && of_device_is_available(infoNp)) {
					pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].bEnable =
						get_dts_u32(infoNp, VRR_OD_TAB_EN_TAG);
					pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16Index =
						get_dts_u32(infoNp, VRR_OD_TAB_IDX_TAG);
					pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16FreqHighBound =
						get_dts_u32(infoNp, VRR_OD_HI_BOUND_TAG);
					pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].u16FreqLowBound =
						get_dts_u32(infoNp, VRR_OD_LO_BOUND_TAG);
					ret = of_property_read_string(infoNp, VRR_OD_FILE_TAG,
				&(pctx_pnl->VRR_OD_info->stVRR_OD_Table[i].s8VRR_OD_table_Name));
					if (ret != 0x0) {
						DRM_INFO("[%s, %d]: read string failed name = %s\n",
							__func__, __LINE__, VRR_OD_FILE_TAG);
						return ret;
					}
				}
			}
			dump_tcon_vrr_od_info(pctx_pnl);
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_TCON_VRR_OD_INFO_NODE);
	}

	return ret;
}

void dump_tcon_vrr_pga_info(struct mtk_panel_context *pctx_pnl)
{
	int i = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}

	DRM_INFO("VRR_PGA_table_Total = %d\n",
				pctx_pnl->VRR_PGA_info->u8VRR_PGA_table_Total);

	for (i = 0; i < pctx_pnl->VRR_PGA_info->u8VRR_PGA_table_Total; i++) {
		DRM_INFO("VRR_PGA_table_id = %d, i = %d\n",
			pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16Index, i);
		DRM_INFO("VRR_PGA_Enable = %d\n",
			pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].bEnable);
		DRM_INFO("VRR_PGA_FreqHighBound = %d\n",
			pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16FreqHighBound);
		DRM_INFO("VRR_PGA_FreqLowBound = %d\n",
			pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16FreqLowBound);
		DRM_INFO("VRR_PGA_table = %s\n",
			pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name);
	}
}

static int _parse_panel_tcon_vrr_pga_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np = NULL;
	struct device_node *infoNp = NULL;
	int ret = 0, count = 0;
	int i = 0;
	const char *tcon_vrr_pga_tab_Name = NULL;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TCON_VRR_PGA_INFO_NODE);
	if (np != NULL) {
		pctx_pnl->vrr_pga_En = get_dts_u32(np, VRR_PGA_EN_TAG);

		/*parse VRR PGA info when vrr_pga_En is true*/
		if (pctx_pnl->vrr_pga_En) {
			count = of_property_count_strings(np, "vrr_pga_table_list");
			pctx_pnl->VRR_PGA_info =
						kzalloc(
						(sizeof(drm_st_VRR_PGA_info)), GFP_KERNEL);

			pctx_pnl->VRR_PGA_info->u8VRR_PGA_table_Total = count;
			for (i = 0; i < count; i++) {
				of_property_read_string_index(np, "vrr_pga_table_list", i,
					&tcon_vrr_pga_tab_Name);
				infoNp = of_find_node_by_name(np, tcon_vrr_pga_tab_Name);
				if (infoNp != NULL && of_device_is_available(infoNp)) {
					pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].bEnable =
						get_dts_u32(infoNp, VRR_PGA_TAB_EN_TAG);
					pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16Index =
						get_dts_u32(infoNp, VRR_PGA_TAB_IDX_TAG);
					pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16FreqHighBound
						= get_dts_u32(infoNp, VRR_PGA_HI_BOUND_TAG);
					pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].u16FreqLowBound =
						get_dts_u32(infoNp, VRR_PGA_LO_BOUND_TAG);
					ret = of_property_read_string(infoNp, VRR_PGA_FILE_TAG,
				&(pctx_pnl->VRR_PGA_info->stVRR_PGA_Table[i].s8VRR_PGA_table_Name));
					if (ret != 0x0) {
						DRM_INFO("[%s, %d]: read string failed name = %s\n",
							__func__, __LINE__, VRR_PGA_FILE_TAG);
						return ret;
					}
				}
			}
			dump_tcon_vrr_pga_info(pctx_pnl);
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_TCON_VRR_PGA_INFO_NODE);
	}

	return ret;
}

static int _parse_panel_oled_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np = NULL;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_OLED_INFO_NODE);

	if (np != NULL) {
		pctx_pnl->oled_info.oled_support           = get_dts_u32(np, "OLED_Support");
		pctx_pnl->oled_info.oled_ip_bypass_cust    = get_dts_u32(np, "oled_ip_bypass_cust");
		pctx_pnl->oled_info.oled_pixelshift        = get_dts_u32(np, "OLED_PixelShift");

		if (pctx_pnl->oled_info.oled_support == 1) {
			/*oled i2c settings */
			pctx_pnl->oled_info.oled_i2c.slave         = get_dts_u32(np, "slave_addr");
			pctx_pnl->oled_info.oled_i2c.channel_id    = get_dts_u32(np, "channel_id");
			pctx_pnl->oled_info.oled_i2c.i2c_mode      = get_dts_u32(np, "i2c_mode");
			pctx_pnl->oled_info.oled_i2c.reg_offset    = get_dts_u32(np, "reg_offset");
			pctx_pnl->oled_info.oled_i2c.lumin_addr    =
				get_dts_u32(np, "lumin_gain_addr");
			pctx_pnl->oled_info.oled_i2c.temp_addr	   = get_dts_u32(np, "temp_addr");
			pctx_pnl->oled_info.oled_i2c.offrs_addr	   = get_dts_u32(np, "offrs_addr");
			pctx_pnl->oled_info.oled_i2c.offrs_mask	   = get_dts_u32(np, "offrs_mask");
			pctx_pnl->oled_info.oled_i2c.jb_addr       = get_dts_u32(np, "jb_addr");
			pctx_pnl->oled_info.oled_i2c.jb_mask       = get_dts_u32(np, "jb_mask");
			pctx_pnl->oled_info.oled_i2c.hdr_addr      = get_dts_u32(np, "hdr_addr");
			pctx_pnl->oled_info.oled_i2c.hdr_mask      = get_dts_u32(np, "hdr_mask");
			pctx_pnl->oled_info.oled_i2c.lea_addr      = get_dts_u32(np, "lea_addr");
			pctx_pnl->oled_info.oled_i2c.lea_mask      = get_dts_u32(np, "lea_mask");
			pctx_pnl->oled_info.oled_i2c.tpc_addr      = get_dts_u32(np, "tpc_addr");
			pctx_pnl->oled_info.oled_i2c.tpc_mask      = get_dts_u32(np, "tpc_mask");
			pctx_pnl->oled_info.oled_i2c.cpc_addr      = get_dts_u32(np, "cpc_addr");
			pctx_pnl->oled_info.oled_i2c.cpc_mask      = get_dts_u32(np, "cpc_mask");
			pctx_pnl->oled_info.oled_i2c.opt_p0_addr   =
				get_dts_u32(np, "opt_p0_addr");
			pctx_pnl->oled_info.oled_i2c.opt_start_addr =
				get_dts_u32(np, "opt_start_addr");
			pctx_pnl->oled_info.oled_i2c.opt_start_mask =
				get_dts_u32(np, "opt_start_mask");

			/* oled sequence */
			pctx_pnl->oled_info.oled_seq.onrf_op      = get_dts_u32(np, "onrf_op");
			pctx_pnl->oled_info.oled_seq.onrf_delay   = get_dts_u32(np, "onrf_delay");
			pctx_pnl->oled_info.oled_seq.offrs_op     = get_dts_u32(np, "offrs_op");
			pctx_pnl->oled_info.oled_seq.jb_op        = get_dts_u32(np, "jb_op");
			pctx_pnl->oled_info.oled_seq.jb_on_off    = get_dts_u32(np, "jb_on_off");
			pctx_pnl->oled_info.oled_seq.jb_temp_max  = get_dts_u32(np, "jb_temp_max");
			pctx_pnl->oled_info.oled_seq.jb_temp_min  = get_dts_u32(np, "jb_temp_min");
			pctx_pnl->oled_info.oled_seq.jb_cooldown  = get_dts_u32(np, "jb_cooldown");
			pctx_pnl->oled_info.oled_seq.qsm_on       = get_dts_u32(np, "qsm_on");
			pctx_pnl->oled_info.oled_seq.qsm_evdd_off = get_dts_u32(np, "qsm_evdd_off");
			pctx_pnl->oled_info.oled_seq.qsm_off      = get_dts_u32(np, "qsm_off");
			pctx_pnl->oled_info.oled_seq.qsm_evdd_on  = get_dts_u32(np, "qsm_evdd_on");
			pctx_pnl->oled_info.oled_seq.mon_delay    = get_dts_u32(np, "mon_delay");

		} else {
			DRM_INFO("[%s, %d]OLED is not supported\n",
					__func__, __LINE__);
		}
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_OLED_INFO_NODE);
	}

	return 0;
}

static int _parse_vrr_hfr_pmic_i2c_ctrl_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np;
	int ret = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TCON_VRR_HFR_PMIC_I2C_CTRL_NODE);
	if (np != NULL) {
		pctx_pnl->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet =
			get_dts_u32(np, "bSupportVrrHfrTconPmicI2cSet");
		if (pctx_pnl->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet) {
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_PMIC_I2C_PRE_DELAY =
				get_dts_u32(np, "ucVRR_HFR_PMIC_I2C_PRE_DELAY");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_PMIC_I2C_SLAVE_ADDR =
				get_dts_u32(np, "ucVRR_HFR_PMIC_I2C_SLAVE_ADDR");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR_SIZE =
				get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_ADDR_SIZE");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR =
				kzalloc(
					(pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR_SIZE *
					sizeof(uint8_t)), GFP_KERNEL);
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_ADDR[0] =
				get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_ADDR_0");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA_SIZE =
				get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_DATA_SIZE");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA =
				kzalloc(
					(pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA_SIZE *
					sizeof(uint8_t)), GFP_KERNEL);
			if (pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA_SIZE == DEC_1) {
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA[0] =
					get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_DATA_1");
			} else if (pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA_SIZE == DEC_2) {
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA[0] =
					get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_DATA_1");
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_ON_PMIC_I2C_DATA[DEC_1] =
					get_dts_u32(np, "ucVRR_HFR_ON_PMIC_I2C_DATA_2");
			}
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR_SIZE =
				get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_ADDR_SIZE");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR =
				kzalloc(
					(pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR_SIZE *
					sizeof(uint8_t)), GFP_KERNEL);
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_ADDR[0] =
				get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_ADDR_0");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA_SIZE =
				get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_DATA_SIZE");
			pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA =
				kzalloc(
					(pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA_SIZE *
					sizeof(uint8_t)), GFP_KERNEL);
			if (pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA_SIZE == DEC_1) {
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA[0] =
					get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_DATA_1");
			} else if (pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA_SIZE == DEC_2) {
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA[0] =
					get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_DATA_1");
				pctx_pnl->vrr_hfr_pmic_i2c_ctrl.VRR_HFR_OFF_PMIC_I2C_DATA[DEC_1] =
					get_dts_u32(np, "ucVRR_HFR_OFF_PMIC_I2C_DATA_2");
			}
		} else {
			DRM_INFO("[%s, %d]VRR HFR PMIC I2C ctrl is not supported\n",
				__func__, __LINE__);
		}
	}

	return ret;
}

static int _parse_panel_tgen_info(struct mtk_panel_context *pctx_pnl)
{
	struct device_node *np;
	int ret = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, PNL_TGEN_INFO_NODE);
	if (np != NULL) {
		pctx_pnl->tgen_info.pnl_tgen_max_vtotal =
			get_dts_u32(np, PNL_TGEN_MAX_VTOTAL);
		pctx_pnl->tgen_info.pnl_tgen_min_vtotal =
			get_dts_u32(np, PNL_TGEN_MIN_VTOTAL);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
					__func__, __LINE__, PNL_TGEN_INFO_NODE);
	}

	return ret;
}

int readDTB2PanelPrivate(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;

	if (pctx_pnl == NULL)
		return -ENODEV;

	//Get Board ini related info
	ret |= _parse_cus_setting_info(pctx_pnl);

	if (pctx_pnl->cus.tcon_en) {
		ret |= _parse_panel_tcon_vrr_od_info(pctx_pnl);
		ret |= _parse_panel_tcon_vrr_pga_info(pctx_pnl);

		/* If tcon enable, and then get tconless model info. */
		ret |= _parse_tconless_model_info(pctx_pnl);

		ret |= _parse_vrr_hfr_pmic_i2c_ctrl_info(pctx_pnl);

		/* Load tcon data into memory */
		pctx_pnl->cus.tcon_en = mtk_tcon_preinit(pctx_pnl);

		if (pctx_pnl->vrr_od_En)
			load_vrr_tcon_files(pctx_pnl);

		if (pctx_pnl->vrr_pga_En)
			load_vrr_pga_files(pctx_pnl);
	}

	//Get panel info
	ret |= _parse_panel_info(pctx_pnl->dev->of_node,
		&pctx_pnl->info,
		&pctx_pnl->panelInfoList);

	//Get MMAP related info
	ret |= _parse_swing_level_info(pctx_pnl);

	ret |= _parse_output_pre_emphsis_info(pctx_pnl);

	ret |= _parse_spread_spectrum_info(pctx_pnl);

	ret |= _parse_pe_info(pctx_pnl);

	ret |= _parse_lane_order_info(pctx_pnl);

	ret |= _parse_op_cfg_info(pctx_pnl);

	ret |= _parse_panel_color_info(pctx_pnl);

	ret |= _parse_frc_tbl_info(pctx_pnl);

	ret |= _parse_output_lane_order_info(pctx_pnl);

	ret |= _parse_hw_info(pctx_pnl);

	ret |= _parse_ext_graph_combo_info(pctx_pnl);

	ret |= _parse_panel_dither_info(pctx_pnl);

	ret |= _parse_panel_oled_info(pctx_pnl);

	ret |= _parse_panel_tgen_info(pctx_pnl);

	if ((pctx_pnl->cus.game_direct_framerate_mode == TRUE)
		|| (pctx_pnl->cus.dlg_i2c_CustCtrl == TRUE))
		ret |= _parse_panel_tcon_i2c_info(pctx_pnl);

	dump_dts_info(pctx_pnl);

	//Patch for GOP sets TGEN of GFX
	{
		struct mtk_drm_panel_context *pStPanelCtx = NULL;

		pStPanelCtx = _get_gfx_panel_ctx();
		if (pStPanelCtx != NULL) {
			memcpy(&pStPanelCtx->out_lane_info, &pctx_pnl->out_lane_info,
					sizeof(drm_st_out_lane_order));
			memcpy(&pctx_pnl->gfx_info, &pStPanelCtx->pnlInfo, sizeof(drm_st_pnl_info));
		}
	}

	//Patch for _mtk_render_check_vbo_ctrlbit_feature sets TGEN of EXTDEV
	{
		struct mtk_drm_panel_context *pStPanelCtx = NULL;

		pStPanelCtx = _get_extdev_panel_ctx();
		if (pStPanelCtx != NULL) {
			memcpy(&pStPanelCtx->out_lane_info, &pctx_pnl->out_lane_info,
					sizeof(drm_st_out_lane_order));
			memcpy(&pctx_pnl->extdev_info, &pStPanelCtx->pnlInfo,
					sizeof(drm_st_pnl_info));
		}
	}

	// Vby1locknProtectEn initial
	{
		pctx_pnl->bVby1LocknProtectEn = false;
	}
	//
	{
		if (pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0200 ||
			pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0203 ||
			pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0400 ||
			pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0500)
			pctx_pnl->bBFIEn = true;
		else
			pctx_pnl->bBFIEn = false;
	}

	drv_hwreg_render_pnl_set_Oled_BypassAll(pctx_pnl->oled_info.oled_ip_bypass_cust);

	// for lane duplicate
	pctx_pnl->lane_duplicate_en = get_dts_u32(pctx_pnl->dev->of_node, "lane_duplicate");

	return ret;
}
EXPORT_SYMBOL(readDTB2PanelPrivate);

static void _ready_cb_pqu_render_dd_pq_cfd_update_info(void)
{
	int ret = 0;

	DRM_INFO("pqu cpuif ready callback function\n");

	ret = pqu_render_dd_pq_cfd_update_info(
		&_cfd_context_msg_info[_CB_DEC(_cfd_context_count)], NULL);
	if (ret != 0)
		DRM_ERROR("pqu_render_dd_pq_cfd_update_info fail!\n");
}

int _mtk_video_panel_set_pq_context(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct meta_render_pqu_cfd_context updateInfo;
	struct pqu_dd_update_pq_info_cfd_context pqupdate;
	struct mtk_tv_drm_metabuf metabuf;

	memset(&updateInfo, 0, sizeof(struct msg_cfd_pq_update_info));

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_VDO_PNL)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_VDO_PNL);
		return -EINVAL;
	}

	updateInfo.pnl_color_info_bx = pctx_pnl->color_info.bx;
	updateInfo.pnl_color_info_by = pctx_pnl->color_info.by;
	updateInfo.pnl_color_info_format = pctx_pnl->color_info.format;
	updateInfo.pnl_color_info_gx = pctx_pnl->color_info.gx;
	updateInfo.pnl_color_info_gy = pctx_pnl->color_info.gy;
	updateInfo.pnl_color_info_hdrnits = pctx_pnl->color_info.hdrnits;
	updateInfo.pnl_color_info_length = pctx_pnl->color_info.length;
	updateInfo.pnl_color_info_linear_rgb = pctx_pnl->color_info.linear_rgb;
	updateInfo.pnl_color_info_maxlum = pctx_pnl->color_info.maxlum;
	updateInfo.pnl_color_info_medlum = pctx_pnl->color_info.medlum;
	updateInfo.pnl_color_info_minlum = pctx_pnl->color_info.minlum;
	updateInfo.pnl_color_info_rx = pctx_pnl->color_info.rx;
	updateInfo.pnl_color_info_ry = pctx_pnl->color_info.ry;
	updateInfo.pnl_color_info_version = pctx_pnl->color_info.version;
	updateInfo.pnl_color_info_wx = pctx_pnl->color_info.wx;
	updateInfo.pnl_color_info_wy = pctx_pnl->color_info.wy;

	pqupdate.pnl_color_info_bx = pctx_pnl->color_info.bx;
	pqupdate.pnl_color_info_by = pctx_pnl->color_info.by;
	pqupdate.pnl_color_info_format = pctx_pnl->color_info.format;
	pqupdate.pnl_color_info_gx = pctx_pnl->color_info.gx;
	pqupdate.pnl_color_info_gy = pctx_pnl->color_info.gy;
	pqupdate.pnl_color_info_hdrnits = pctx_pnl->color_info.hdrnits;
	pqupdate.pnl_color_info_length = pctx_pnl->color_info.length;
	pqupdate.pnl_color_info_linear_rgb = pctx_pnl->color_info.linear_rgb;
	pqupdate.pnl_color_info_maxlum = pctx_pnl->color_info.maxlum;
	pqupdate.pnl_color_info_medlum = pctx_pnl->color_info.medlum;
	pqupdate.pnl_color_info_minlum = pctx_pnl->color_info.minlum;
	pqupdate.pnl_color_info_rx = pctx_pnl->color_info.rx;
	pqupdate.pnl_color_info_ry = pctx_pnl->color_info.ry;
	pqupdate.pnl_color_info_version = pctx_pnl->color_info.version;
	pqupdate.pnl_color_info_wx = pctx_pnl->color_info.wx;
	pqupdate.pnl_color_info_wy = pctx_pnl->color_info.wy;

	if (bPquEnable) {
		ret = pqu_render_dd_pq_cfd_update_info(&pqupdate, NULL);
		if (ret == -ENODEV) {
			memcpy(&_cfd_context_msg_info[_CB_INC(_cfd_context_count)],
				&pqupdate,
				sizeof(struct pqu_dd_update_pq_info_cfd_context));
			DRM_ERROR("cfd_update_info register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_render_dd_pq_cfd_update_info);
		} else if (ret != 0) {
			DRM_ERROR("cfd_update_info fail (ret=%d)\n", ret);
		}
	}
	else
		pqu_msg_send(PQU_MSG_SEND_CFD_PQ_FIRE, &updateInfo);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;

}


static void _mtk_render_map_timing(
	drm_output_mode timing,
	struct pxm_size *size)
{
	if (size == NULL) {
		DRM_ERROR("[drm][pxm] size is NULL!\n");
		return;
	}

	switch (timing) {
	case E_8K4K_120HZ:
	case E_8K4K_60HZ:
	case E_8K4K_30HZ:
		size->h_size = 7680;
		size->v_size = 4320;
		break;
	case E_4K2K_144HZ:
	case E_4K2K_120HZ:
	case E_4K2K_60HZ:
	case E_4K2K_30HZ:
		size->h_size = 3840;
		size->v_size = 2160;
		break;
	case E_FHD_120HZ:
	case E_FHD_60HZ:
		size->h_size = 1920;
		size->v_size = 1080;
		break;
	case E_4K1K_240HZ:
	case E_4K1K_120HZ:
		size->h_size = UHD_4K_W;
		size->v_size = FHD_2K_H;
		break;
	default:
		size->h_size = 0;
		size->v_size = 0;
		DRM_INFO("[drm][pxm] timing(%d) is invalid!\n", timing);
		break;
	}
}

static void _mtk_render_set_pxm_size(struct mtk_panel_context *pctx)
{
	enum pxm_return ret = E_PXM_RET_FAIL;
	struct pxm_size_info info;
	struct mtk_drm_panel_context *pStPanelCtx = NULL;
	drm_output_mode extdevOutputMode = 0;

	memset(&info, 0, sizeof(struct pxm_size_info));

	if (pctx == NULL) {
		DRM_ERROR("[drm][pxm] pctx is NULL!\n");
		return;
	}

	info.win_id = 0;  // tcon not support time sharing, win id set 0

	info.point = E_PXM_POINT_TCON_AFTER_OSD;
	_mtk_render_map_timing(pctx->v_cfg.timing, &info.size);
	ret = pxm_set_size(&info);
	if (ret != E_PXM_RET_OK) {
		DRM_ERROR("[drm][pxm] set pxm size fail(%d)!\n", ret);
		return;
	}

	info.point = E_PXM_POINT_TCON_BEFORE_MOD;
	_mtk_render_map_timing(pctx->v_cfg.timing, &info.size);
	ret = pxm_set_size(&info);
	if (ret != E_PXM_RET_OK) {
		DRM_ERROR("[drm][pxm] set pxm size fail(%d)!\n", ret);
		return;
	}

	pStPanelCtx = _get_extdev_panel_ctx();
	if (pStPanelCtx != NULL)
		extdevOutputMode = pStPanelCtx->cfg.timing;

	info.point = E_PXM_POINT_TCON_DELTA_PATH;
	_mtk_render_map_timing(extdevOutputMode, &info.size);
	ret = pxm_set_size(&info);
	if (ret != E_PXM_RET_OK)
		DRM_ERROR("[drm][pxm] set pxm size fail(%d)!\n", ret);

}

int mtk_tv_drm_set_change_modeid_state_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	uint8_t *state = (uint8_t *)data;

	ucChange_ModeID_state = *state;

	return ret;
}

int mtk_tv_drm_get_change_modeid_state_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	uint8_t *state = (uint8_t *)data;

	if (!drm_dev || !data)
		return -EINVAL;

	*state = ucChange_ModeID_state;

	return ret;
}

static int mtk_panel_init(struct device *dev)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;
	uint32_t u32_ldm_support_type = E_LDM_UNSUPPORT;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pctx_pnl = dev->driver_data;
	if (!pctx_pnl) {
		pr_err("%s: missing driver data\n", __func__);
		return -ENODEV;
	}

	u64PreTypDclk = pctx_pnl->info.typ_dclk;
	ePreOutputFormat = pctx_pnl->info.op_format;
	ePreByteMode = pctx_pnl->info.vbo_byte;

	//External BE related
	if (mtk_render_cfg_connector(pctx_pnl) == false) {
		DRM_INFO("[%s][%d] mtk_plane_init failed!!\n", __func__, __LINE__);
		return -ENODEV;
	}

	//2. Config CRTC setting
	//update gamma lut size
	if (mtk_render_cfg_crtc(pctx_pnl) == false) {
		DRM_INFO("[%s][%d] mtk_plane_init failed!!\n", __func__, __LINE__);
		return -ENODEV;
	}

	//update gamma lut size
	//drm_mode_crtc_set_gamma_size(&mtkCrtc->base, MTK_GAMMA_LUT_SIZE);
	//set pixel monitor size
	_mtk_render_set_pxm_size(pctx_pnl);

	//ldm support, notify ldm re-init, when tgen change.
	u32_ldm_support_type = mtk_ldm_get_support_type();

	if (u32_ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU
	|| u32_ldm_support_type == E_LDM_SUPPORT_R2) {
		ret = mtk_ldm_re_init();
		if (ret)
			DRM_INFO("[%s][%d] mtk_ldm_re_init no re-init\n", __func__, __LINE__);
	}
	if (ucChange_ModeID_state == E_MODE_ID_CHANGE_DONE)
		DRM_INFO("[%s, %d]: Change Mode ID state is not reset by MI render!!\n",
			__func__, __LINE__);
	ucChange_ModeID_state = E_MODE_ID_CHANGE_DONE;

	DRM_INFO("[%s][%d] success!!\n", __func__, __LINE__);

	return 0;
}

static int _mtk_video_panel_add_framesyncML(
	struct mtk_tv_kms_context *pctx,
	struct reg_info *reg,
	uint32_t RegCount)
{
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	int ret = 0;

	if (!pctx || !reg) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx, reg=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx, (unsigned long long)reg);
		return -ENODEV;
	}

	switch (pctx_pnl->outputTiming_info.eFrameSyncState) {
	case E_PNL_FRAMESYNC_STATE_PROP_IN:
		mtk_tv_sm_ml_write_cmd(&pctx_video->disp_ml, RegCount, (struct sm_reg *)reg);
		break;
	case E_PNL_FRAMESYNC_STATE_IRQ_IN:
		mtk_tv_sm_ml_write_cmd(&pctx_video->disp_irq_ml, RegCount, (struct sm_reg *)reg);
		break;
	default:
		DRM_ERROR("[%s, %d] framesync_state = %d\n",
			__func__, __LINE__, pctx_pnl->outputTiming_info.eFrameSyncState);
		ret = -1;
		break;
	}

	return ret;
}

static int _mtk_video_get_vplane_buf_mode(
	struct mtk_tv_kms_context *pctx,
	enum drm_mtk_vplane_buf_mode *buf_mode)
{
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;
	struct drm_mode_object *obj = NULL;
	struct drm_plane *plane = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_tv_kms_context *pctx_plane = NULL;
	struct mtk_video_context *pctx_video = NULL;
	unsigned int plane_inx = 0;
	struct video_plane_prop *plane_props = NULL;

	if (!pctx || !buf_mode) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx, buf_mode=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx, (unsigned long long)buf_mode);
		return -ENODEV;
	}

	crtc_props = pctx->ext_crtc_properties;
	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);

	if (obj == NULL) {
		DRM_ERROR("[%s, %d]: null ptr! obj =0x%llx,\n",
			__func__, __LINE__, (unsigned long long)obj);
		return -ENODEV;
	}

	plane = obj_to_plane(obj);
	mplane = to_mtk_plane(plane);
	pctx_plane = (struct mtk_tv_kms_context *)mplane->crtc_private;
	pctx_video = &pctx_plane->video_priv;
	plane_inx = mplane->video_index;
	plane_props = (pctx_video->video_plane_properties + plane_inx);

	*buf_mode = plane_props->prop_val[EXT_VPLANE_PROP_BUF_MODE];

	return 0;
}

static int _mtk_video_panel_set_vttv_phase_diff(
	struct mtk_tv_kms_context *pctx,
	uint64_t *phase_diff)
{
	int ret = 0;
	enum drm_mtk_vplane_buf_mode vplane_buf_mode = MTK_VPLANE_BUF_MODE_MAX;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;
	struct drm_mode_object *obj = NULL;
	struct drm_plane *plane = NULL;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}
	ret = _mtk_video_get_vplane_buf_mode(pctx, &vplane_buf_mode);
	if (ret) {
		DRM_ERROR("[%s, %d]: get vplane buf mode error\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pctx_pnl = &pctx->panel_priv;
	crtc_props = pctx->ext_crtc_properties;
	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
	plane = obj_to_plane(obj);

	//set lock user phase diff
	if ((vplane_buf_mode == MTK_VPLANE_BUF_MODE_HW) &&
		((uint16_t)plane->state->crtc_w == pctx_pnl->info.de_width &&
		(uint16_t)plane->state->crtc_h == pctx_pnl->info.de_height)) {
		if ((pctx_pnl->outputTiming_info.u8FRC_in == 1 &&
			pctx_pnl->outputTiming_info.u8FRC_out == 1) &&
			(pctx_pnl->info.de_width == UHD_8K_W &&
			pctx_pnl->info.de_height == UHD_8K_H)) {
			*phase_diff = pctx_pnl->outputTiming_info.u16OutputVTotal*
				FRAMESYNC_VTTV_SHORTEST_LOW_LATENCY_PHASE_DIFF/
				FRAMESYNC_VTTV_PHASE_DIFF_DIV_1000;
		} else {
			*phase_diff = pctx_pnl->outputTiming_info.u16OutputVTotal*
				FRAMESYNC_VTTV_LOW_LATENCY_PHASE_DIFF/
				FRAMESYNC_VTTV_PHASE_DIFF_DIV_1000;
		}
	} else {
		*phase_diff = pctx_pnl->outputTiming_info.u16OutputVTotal*
			FRAMESYNC_VTTV_NORMAL_PHASE_DIFF/
			FRAMESYNC_VTTV_PHASE_DIFF_DIV_1000;
	}

	if (pctx->stub == false)
		*phase_diff = mtk_video_panel_get_FrameLockPhaseDiff();

	DRM_INFO("[%s][%d] phase_diff: %td\n",
			__func__, __LINE__, (ptrdiff_t)*phase_diff);

	return ret;
}

/*
static uint64_t _mtk_video_adjust_input_freq_stubMode(
	struct mtk_tv_kms_context *pctx,
	struct mtk_panel_context *pctx_pnl,
	uint64_t input_vfreq)
{
	//in stub mode
	//change the value of input frame rate to max output frame rate
	//to meet the ivs:ovs = 1:1
	if (pctx->stub == true)
		if ((pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt) != 0)
			input_vfreq = ((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
				((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));
	return input_vfreq;
}
*/

static uint16_t _mtk_video_adjust_output_vtt_ans_stubMode(
	struct mtk_tv_kms_context *pctx,
	struct mtk_panel_context *pctx_pnl,
	uint16_t output_vtt)
{
	if (pctx->stub == true) {
		if ((pctx_pnl->outputTiming_info.u16OutputVTotal != 0) &&
			(pctx_pnl->outputTiming_info.u16OutputVTotal <= pctx_pnl->info.max_vtt) &&
			(pctx_pnl->outputTiming_info.u16OutputVTotal >= pctx_pnl->info.min_vtt))
			output_vtt = 1;// right result
		else
			output_vtt = 0;// wrong result

		DRM_INFO("[%s][%d] stub result log : %td %td\n",
				__func__, __LINE__,
				(ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal,
				(ptrdiff_t)pctx_pnl->info.typ_vtt);
	}
	return output_vtt;
}



//Add code to remove the patch since user space commit change
/*
static void _mtk_video_i_mode_ivsovs_patch(
	struct mtk_tv_kms_context *pctx,
	uint8_t *ivs)
{
	bool bIsB2Rsource = false;
	bool IS_Interlance = false;
	unsigned long flags;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return;
	}

	if (!ivs) {
		DRM_ERROR("[%s, %d]: null ptr! ivs=0x%llx\n",
			__func__, __LINE__, (unsigned long long)ivs);
		return;
	}

	spin_lock_irqsave(&pctx->tg_lock, flags);
	bIsB2Rsource = pctx->trigger_gen_info.IsB2Rsource;
	IS_Interlance = (pctx->trigger_gen_info.IsPmodeEn)?0:1;
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	if ((bIsB2Rsource) && (IS_Interlance)) {
		*ivs = (*ivs)*2;
		DRM_INFO("[%s][%d] is B2R+I mode\n",
			__func__, __LINE__);
	}
}
*/

static inline void __tgen_src_sel(struct mtk_tv_kms_context *pctx, unsigned int plane_inx,
			drm_en_vttv_source_type *ivsSource,
			drm_en_vttv_source_type *ovsSource)
{
	// already check null
	struct mtk_video_context *pctx_video = &pctx->video_priv;
	struct mtk_video_plane_ctx *plane_ctx = (pctx_video->plane_ctx + plane_inx);
	struct mtk_panel_context *pctx_pnl = &pctx->panel_priv;
	enum drm_mtk_vplane_buf_mode vplane_buf_mode = MTK_VPLANE_BUF_MODE_MAX;

	/*
	 * consider the frc is enable or not
	 * frc on: source sel frc1/2
	 * frc off: source sel ip
	 */
	if ((pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0400) ||
		(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0500) ||
		(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0600)) {
		if (plane_ctx->memc_change_on == MEMC_CHANGE_ON) {
			*ivsSource = RW_BK_UPD_FRC1;
			*ovsSource = RW_BK_UPD_FRC2;
		} else {
			*ivsSource = RW_BK_UPD_IP_IPM_W;
			*ovsSource = RW_BK_UPD_FRC2;
		}
	} else {
		if (pctx->trigger_gen_info.IsLowLatency == true) {
			*ivsSource = RW_BK_UPD_OP2;
			*ovsSource = RW_BK_UPD_DISP;
		} else {
			*ivsSource = REF_PULSE;
			*ovsSource = REF_PULSE;
		}
	}

	DRM_INFO("[%s][%d]vplane_buf_mode: %d, ivsSource: %d, ovsSource: %d\n",
			__func__, __LINE__, vplane_buf_mode, *ivsSource, *ovsSource);
}

static void _mtk_video_get_ivsovs_ratio(
	struct mtk_panel_context *pctx_pnl,
	uint64_t input_vfreq,
	uint8_t *ivs, uint8_t *ovs)
{
	int FRCType = 0;
	uint64_t frc_table_upperbound = 0, frc_table_lowerbound = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return;
	}
	if (!ivs) {
		DRM_ERROR("[%s, %d]: null ptr! ivs=0x%llx\n",
			__func__, __LINE__, (unsigned long long)ivs);
		return;
	}
	if (!ovs) {
		DRM_ERROR("[%s, %d]: null ptr! ovs=0x%llx\n",
			__func__, __LINE__, (unsigned long long)ovs);
		return;
	}

	//find ivs/ovs
	*ivs = pctx_pnl->frcRatio_info.u8FRC_in;
	*ovs = pctx_pnl->frcRatio_info.u8FRC_out;

	if (*ivs == 0) {
		for (FRCType = 0;
			FRCType < (sizeof(pctx_pnl->frc_info)/
				sizeof(struct drm_st_pnl_frc_table_info));
			FRCType++) {
			// VFreq in FRC table is x10 base.
			frc_table_upperbound = (uint64_t)VFRQRATIO_1000/VFRQRATIO_10
				*pctx_pnl->frc_info[FRCType].u16HigherBound;
			frc_table_lowerbound = (uint64_t)VFRQRATIO_1000/VFRQRATIO_10
				*pctx_pnl->frc_info[FRCType].u16LowerBound;
			if (input_vfreq > frc_table_lowerbound &&
				input_vfreq <= frc_table_upperbound) {
				*ivs = pctx_pnl->frc_info[FRCType].u8FRC_in;
				*ovs = pctx_pnl->frc_info[FRCType].u8FRC_out;
				break;
			}
		}
	}
}

static int _mtk_video_set_vttv_mode(
	struct mtk_tv_kms_context *pctx,
	uint64_t input_vfreq, bool bLowLatencyEn)
{
	int ret = 0;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	drm_en_vttv_source_type ivsSource = REF_PULSE, ovsSource = REF_PULSE;
	uint8_t ivs = 0, ovs = 0;
	uint64_t output_vfreq = 0, default_vfreq = 0, outputVTT = 0, phase_diff = 0;
	bool bRIU = false;
	uint16_t vttv_m_delta = 0;
	uint16_t RegCount = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;
	struct drm_mode_object *obj = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct drm_plane *plane = NULL;
	unsigned int plane_inx;
	unsigned long flags;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	DRM_INFO("[%s][%d]input_vfreq: %lld, bLowLatencyEn: %d\n",
			__func__, __LINE__, input_vfreq, bLowLatencyEn);

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;
	vttv_m_delta = pctx_pnl->cus.m_del;
	crtc_props = pctx->ext_crtc_properties;
	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
	plane = obj_to_plane(obj);
	mplane = to_mtk_plane(plane);
	plane_inx = mplane->video_index;

	// decide ivs/ovs source
	__tgen_src_sel(pctx, plane_inx, &ivsSource, &ovsSource);

	//find ivs/ovs
	_mtk_video_get_ivsovs_ratio(pctx_pnl, input_vfreq, &ivs, &ovs);

	DRM_INFO("[%s][%d]ivs: %d, ovs: %d\n",
			__func__, __LINE__, ivs, ovs);

	//calc output vfreq
	if (ivs > 0) {
		output_vfreq = input_vfreq * ovs / ivs;
		pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		pctx_pnl->outputTiming_info.u8FRC_in = ivs;
		pctx_pnl->outputTiming_info.u8FRC_out = ovs;

		DRM_INFO("[%s][%d] ivs: %td, ovs: %td, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs,
			(ptrdiff_t)output_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc output vfreq fail, ivs: %td, ovs: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs);

		DRM_INFO("[%s][%d]calc output vfreq fail, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
	}

	//calc default vfreq
	if (pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt > 0) {
		default_vfreq =
			((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
			((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));

		DRM_INFO("[%s][%d]typ_htt:%td, typ_vtt:%td, typ_dclk:%td, default_vfreq:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc default vfreq fail,typ_htt:%td, typ_vtt:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt);

		DRM_INFO("[%s][%d]calc default vfreq fail,typ_dclk:%td, default_vfreq: %td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	}

	//calc output vtt
	if (output_vfreq > 0) {
		outputVTT = ((pctx_pnl->info.typ_vtt*default_vfreq) + (output_vfreq/2))
			/ output_vfreq;
		DRM_INFO("[%s][%d] cal outputVTT: %td, maxVTT: %td, minVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT,
			(ptrdiff_t)pctx_pnl->info.max_vtt, (ptrdiff_t)pctx_pnl->info.min_vtt);
		if (pctx_pnl->info.min_vtt > outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.min_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		} else if (pctx_pnl->info.max_vtt < outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.max_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		}
		DRM_INFO("[%s][%d] in tolerance range, outputVTT: %td\n",
		__func__, __LINE__, (ptrdiff_t)outputVTT);

		pctx_pnl->outputTiming_info.u16OutputVTotal = outputVTT;
	} else {
		DRM_INFO("[%s][%d] calc output vtt fail, outputVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT);
		pctx_pnl->outputTiming_info.u32OutputVfreq = default_vfreq;
		DRM_INFO("[%s][%d] output_vfreq: %td => free run with default vfreq\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
		return -EPERM;
	}

	//adjust output vtt in stub mode
	outputVTT = _mtk_video_adjust_output_vtt_ans_stubMode(
			pctx, pctx_pnl, pctx_pnl->outputTiming_info.u16OutputVTotal);

	//set lock user phase diff
	ret = _mtk_video_panel_set_vttv_phase_diff(pctx, &phase_diff);
	if (ret) {
		DRM_ERROR("[%s, %d]: set vttv phase diff error\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	// we cannot use VTTV in B2R mode at E1
	if (pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0100) {
		spin_lock_irqsave(&pctx->tg_lock, flags);
		if (pctx->trigger_gen_info.IsB2Rsource == true) {
			spin_unlock_irqrestore(&pctx->tg_lock, flags);
			return -EPERM;
		}
		spin_unlock_irqrestore(&pctx->tg_lock, flags);
	}

	//force Free Run Mode
	if (pctx_pnl->cus.force_free_run_en == true) {
		DRM_INFO("[%s][%d] Force Free Run Mode\n", __func__, __LINE__);
		return -EPERM;
	}

	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_VTTV;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	//h31[0] = 0
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A3_h16[4:0] = ivsSource
	//BKA3A3_h16[12:8] = ovsSource
	drv_hwreg_render_pnl_set_vttSourceSelect(
		&paramOut,
		bRIU,
		ivsSource,
		ovsSource);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h20[15] = 1
	//h20[14:8] = 1
	//h20[7:0] = 3
	drv_hwreg_render_pnl_set_centralVttChange(
		&paramOut,
		bRIU,
		false,
		FRAMESYNC_VTTV_FRAME_COUNT,
		FRAMESYNC_VTTV_THRESHOLD);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h22[15:3] = phase_diff
	//h23[15] = 1
	drv_hwreg_render_pnl_set_vttLockPhaseDiff(
		&paramOut,
		bRIU,
		1,
		phase_diff);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[4] = 1
	drv_hwreg_render_pnl_set_vttLockKeepSequenceEnable(
		&paramOut,
		bRIU,
		1,
		FRAMESYNC_VTTV_LOCKKEEPSEQUENCETH);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[10] = 1
	//h30[15] = 1
	//h30[14:0] = m_delta
	drv_hwreg_render_pnl_set_vttLockDiffLimit(
		&paramOut,
		bRIU,
		true,
		vttv_m_delta);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//VTT uper bound/lower bound is control by Tgen panel only
	//disable VTTV uper bound /lower bound
	//h33[11] = 0
	//h33[12] = 0
	//h36[15:0] = 0xffff
	//h37[15:0] = 0
	drv_hwreg_render_pnl_set_vttLockVttRange(
		&paramOut,
		bRIU,
		true,
		pctx_pnl->info.max_vtt,
		pctx_pnl->info.min_vtt);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[13] = 1
	drv_hwreg_render_pnl_set_vttLockVsMaskMode(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h32[7:0] = ivs
	//h32[15:8] = ovs

	//Add code to remove the patch since user space commit change
	//_mtk_video_i_mode_ivsovs_patch(pctx, &ivs);
	drv_hwreg_render_pnl_set_vttLockIvsOvs(
		&paramOut,
		bRIU,
		ivs,
		ovs);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h3e[15:0] = 9
	drv_hwreg_render_pnl_set_vttLockLockedInLimit(
		&paramOut,
		bRIU,
		FRAMESYNC_VTTV_LOCKED_LIMIT);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[13] = 1
	drv_hwreg_render_pnl_set_vttLockUdSwModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[9] = 0
	drv_hwreg_render_pnl_set_vttLockIvsShiftEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[8] = 1
	drv_hwreg_render_pnl_set_vttLockProtectEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h23[15:0] = output VTT
		drv_hwreg_render_pnl_set_vtt(
			&paramOut,
			bRIU,
			outputVTT);

	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[0] = 1
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[12] = 0
	drv_hwreg_render_pnl_set_vttLockLastVttLatchEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);

	return ret;
}

static int _mtk_video_set_vtt2fpll_mode(
	struct mtk_tv_kms_context *pctx,
	uint64_t input_vfreq, bool bLowLatencyEn)
{
	int ret = 0, FRCType = 0;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	drm_en_vttv_source_type ivsSource = REF_PULSE, ovsSource = REF_PULSE;
	uint8_t ivs = 0, ovs = 0;
	uint64_t output_vfreq = 0, default_vfreq = 0, outputVTT = 0, phase_diff = 0;
	uint64_t frc_table_upperbound = 0, frc_table_lowerbound = 0;
	bool bRIU = false;
	uint16_t vttv_m_delta = 0;
	uint16_t RegCount = 0;
	uint64_t lpll_set = drv_hwreg_render_pnl_get_lpllset();
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;
	enum drm_mtk_vplane_buf_mode vplane_buf_mode = MTK_VPLANE_BUF_MODE_MAX;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;

	//fpll clk setting
	uint8_t v1odclk = 0;
	uint8_t v1odclk_gate = 0;
	uint8_t v1odclk_inv = 0;
	uint8_t v1odclk_div = 0;

	//set lpll limit
	uint16_t offsetlimit = 0;

	//set lpll D5D6D7
	uint64_t lpll_limit = 0;
	uint8_t D5D6D7_H = 0;
	uint16_t D5D6D7_L = 0;
	uint8_t D5D6D7_RK_H = 0;
	uint16_t D5D6D7_RK_L = 0;

	DRM_INFO("[%s][%d]input_vfreq: %lld, bLowLatencyEn: %d\n",
			__func__, __LINE__, (unsigned long long)input_vfreq, bLowLatencyEn);

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;
	vttv_m_delta = pctx_pnl->cus.m_del;
	crtc_props = pctx->ext_crtc_properties;
	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];

	//decide ivs/ovs source
	_mtk_video_get_vplane_buf_mode(pctx, &vplane_buf_mode);
	switch (vplane_buf_mode) {
	case MTK_VPLANE_BUF_MODE_SW:
		ivsSource = REF_PULSE;
		ovsSource = REF_PULSE;
		break;
	case MTK_VPLANE_BUF_MODE_HW:
		ivsSource = RW_BK_UPD_OP2;
		ovsSource = RW_BK_UPD_DISP;
		break;
	case MTK_VPLANE_BUF_MODE_BYPASSS:
		ivsSource = RW_BK_UPD_IP_IPM_W;
		ovsSource = RW_BK_UPD_OP1_OPM_R;
		break;
	default:
		DRM_ERROR("[%s][%d] invalid video buffer mode!!\n",
			__func__, __LINE__);
	}
	DRM_INFO("[%s][%d]vplane_buf_mode: %d, ivsSource: %d, ovsSource: %d\n",
			__func__, __LINE__, vplane_buf_mode, ivsSource, ovsSource);

	//find ivs/ovs
	ivs = pctx_pnl->frcRatio_info.u8FRC_in;
	ovs = pctx_pnl->frcRatio_info.u8FRC_out;

	DRM_INFO("[%s][%d]ivs: %d, ovs: %d\n",
			__func__, __LINE__, ivs, ovs);

	if (ivs == 0) {
		for (FRCType = 0;
			FRCType < (sizeof(pctx_pnl->frc_info)/
				sizeof(struct drm_st_pnl_frc_table_info));
			FRCType++) {
			// VFreq in FRC table is x10 base.
			frc_table_upperbound = (uint64_t)VFRQRATIO_1000/VFRQRATIO_10
				*pctx_pnl->frc_info[FRCType].u16HigherBound;
			frc_table_lowerbound = (uint64_t)VFRQRATIO_1000/VFRQRATIO_10
				*pctx_pnl->frc_info[FRCType].u16LowerBound;
			if (input_vfreq > frc_table_lowerbound &&
				input_vfreq <= frc_table_upperbound) {
				ivs = pctx_pnl->frc_info[FRCType].u8FRC_in;
				ovs = pctx_pnl->frc_info[FRCType].u8FRC_out;
				break;
			}
		}
	}

	//calc output vfreq
	if (ivs > 0) {
		output_vfreq = input_vfreq * ovs / ivs;
		pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		pctx_pnl->outputTiming_info.u8FRC_in = ivs;
		pctx_pnl->outputTiming_info.u8FRC_out = ovs;

		DRM_INFO("[%s][%d] ivs: %td, ovs: %td, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs,
			(ptrdiff_t)output_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc output vfreq fail, ivs: %td, ovs: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs);

		DRM_INFO("[%s][%d]calc output vfreq fail, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
	}

	//calc default vfreq
	if (pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt > 0) {
		default_vfreq =
			((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
			((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));

		DRM_INFO("[%s][%d]typ_htt:%td, typ_vtt:%td, typ_dclk:%td, default_vfreq:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc default vfreq fail,typ_htt:%td, typ_vtt:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt);

		DRM_INFO("[%s][%d]calc default vfreq fail,typ_dclk:%td, default_vfreq: %td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	}

	//calc output vtt
	if (output_vfreq > 0) {
		outputVTT = ((pctx_pnl->info.typ_vtt*default_vfreq) + (output_vfreq/2))
			/ output_vfreq;
		DRM_INFO("[%s][%d] cal outputVTT: %td, maxVTT: %td, minVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT,
			(ptrdiff_t)pctx_pnl->info.max_vtt, (ptrdiff_t)pctx_pnl->info.min_vtt);
		if (pctx_pnl->info.min_vtt > outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.min_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		} else if (pctx_pnl->info.max_vtt < outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.max_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		}

		DRM_INFO("[%s][%d] in tolerance range, outputVTT: %td\n",
		__func__, __LINE__, (ptrdiff_t)outputVTT);
		pctx_pnl->outputTiming_info.u16OutputVTotal = outputVTT;
	} else {
		DRM_INFO("[%s][%d] calc output vtt fail, outputVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT);
		pctx_pnl->outputTiming_info.u32OutputVfreq = default_vfreq;
		DRM_INFO("[%s][%d] output_vfreq: %td => free run with default vfreq\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
		return -EPERM;
	}

	//adjust output vtt in stub mode
	outputVTT = _mtk_video_adjust_output_vtt_ans_stubMode(
			pctx, pctx_pnl, pctx_pnl->outputTiming_info.u16OutputVTotal);

	//set lock user phase diff
	ret = _mtk_video_panel_set_vttv_phase_diff(pctx, &phase_diff);
	if (ret) {
		DRM_ERROR("[%s, %d]: set vttv phase diff error\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	//force Free Run Mode
	if (pctx_pnl->cus.force_free_run_en == true) {
		DRM_INFO("[%s][%d] Force Free Run Mode\n", __func__, __LINE__);
		return -EPERM;
	}

	//fpll clk setting
	v1odclk = drv_hwreg_render_pnl_get_ckgV1odclk();
	v1odclk_gate = v1odclk & HEX_1; //[0]
	v1odclk_inv = v1odclk & HEX_2; //[1]
	v1odclk_div = v1odclk & HEX_38; //[5:3]

	v1odclk = (v1odclk_div / DEC_2) + v1odclk_inv + v1odclk_gate;

	drv_hwreg_render_pnl_set_idclk2fpllSWEN(true);
	drv_hwreg_render_pnl_set_odclkNodiv2fpllSWEN(true);
	drv_hwreg_render_pnl_set_odclk2fpllSWEN(true);
	drv_hwreg_render_pnl_set_ckglpllodclk(v1odclk);

	//set lpll limit
	//vby1 lpllLimit offset/org_set < 0.1%
	offsetlimit = lpll_set / DEC_16 * DEC_1 / DEC_1000;

	//set lpll D5D6D7
	//vby1 limit/org_set < 0.5%
	lpll_limit = lpll_set / DEC_16 * DEC_5 / DEC_1000;
	D5D6D7_H = high_16_bits(lpll_limit);
	D5D6D7_L = low_16_bits(lpll_limit);
	D5D6D7_RK_H = high_16_bits(lpll_limit);
	D5D6D7_RK_L = low_16_bits(lpll_limit);

	drv_hwreg_render_pnl_set_lpllLimit(offsetlimit);
	drv_hwreg_render_pnl_set_D5D6D7(D5D6D7_H, D5D6D7_L, D5D6D7_RK_H, D5D6D7_RK_L);


	// E1 dosn't support XC Bank use of memuload
	if (pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0100) {
		ret = _mtk_video_set_vttv_mode(pctx, input_vfreq, bLowLatencyEn);
		return ret;
	}

	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_VTTV;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	//drv_hwreg_render_pnl_set_vttfpllmode(false);
	//vtt2fpll mode
	//lpll_h4f[0] = 0
	drv_hwreg_render_pnl_set_vtt2setmode(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[0] = 0
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A3_h16[4:0] = ivsSource
	//BKA3A3_h16[12:8] = ovsSource
	drv_hwreg_render_pnl_set_vttSourceSelect(
		&paramOut,
		bRIU,
		ivsSource,
		ovsSource);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//change
	//h20[15] = 0
	//h20[14:8] = 1
	//h20[7:0] = 1
	drv_hwreg_render_pnl_set_centralVttChange(
		&paramOut,
		bRIU,
		false,
		1,
		1);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h22[15:3] = phase_diff
	//h23[15] = 1
	drv_hwreg_render_pnl_set_vttLockPhaseDiff(
		&paramOut,
		bRIU,
		true,
		phase_diff);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[4] = 1
	drv_hwreg_render_pnl_set_vttLockKeepSequenceEnable(
		&paramOut,
		bRIU,
		1,
		FRAMESYNC_VTTV_LOCKKEEPSEQUENCETH);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[10] = 1
	//h30[15] = 1
	//h30[14:0] = m_delta
	drv_hwreg_render_pnl_set_vttLockDiffLimit(
		&paramOut,
		bRIU,
		true,
		vttv_m_delta);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//VTT uper bound/lower bound is control by Tgen panel only
	//disable VTTV uper bound /lower bound
	//h33[11] = 0
	//h33[12] = 0
	//h36[15:0] = 0xffff
	//h37[15:0] = 0
	drv_hwreg_render_pnl_set_vttLockVttRange(
		&paramOut,
		bRIU,
		true,
		pctx_pnl->info.max_vtt,
		pctx_pnl->info.min_vtt);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[13] = 1
	drv_hwreg_render_pnl_set_vttLockVsMaskMode(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h32[7:0] = ivs
	//h32[15:8] = ovs

	//Add code to remove the patch since user space commit change
	//_mtk_video_i_mode_ivsovs_patch(pctx, &ivs);
	drv_hwreg_render_pnl_set_vttLockIvsOvs(
		&paramOut,
		bRIU,
		ivs,
		ovs);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h3e[15:0] = 9
	drv_hwreg_render_pnl_set_vttLockLockedInLimit(
		&paramOut,
		bRIU,
		FRAMESYNC_VTTV_LOCKED_LIMIT);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//change
	//h31[13] = 0
	drv_hwreg_render_pnl_set_vttLockUdSwModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[9] = 0
	drv_hwreg_render_pnl_set_vttLockIvsShiftEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[8] = 1
	drv_hwreg_render_pnl_set_vttLockProtectEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h23[15:0] = output VTT
		drv_hwreg_render_pnl_set_vtt(
			&paramOut,
			bRIU,
			outputVTT);

	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[12] = 0
	drv_hwreg_render_pnl_set_vttLockUdSwModeActsEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[0] = 1
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[12] = 0
	drv_hwreg_render_pnl_set_vttLockLastVttLatchEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//vtt2fpll mode enable
	//lpll_h4f[0] = 1
	drv_hwreg_render_pnl_set_vtt2setmode(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);

	return ret;
}

static void _mtk_video_set_IvsOvsSourceSel_phasediff(
	struct mtk_tv_kms_context *pctx,
	bool bRIU, bool bLowLatencyEn, bool bFrcFBL,
	uint32_t u32PhasediffValue)
{

	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;
	drm_en_vttv_source_type ivsSource = REF_PULSE, ovsSource = REF_PULSE;
	mtktv_chip_series series;
	unsigned long flags;
	uint16_t RegCount = 0;
	struct mtk_video_context *pctx_video = NULL;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;
	pctx_pnl = &pctx->panel_priv;
	pctx_video = &pctx->video_priv;

	if (pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0100) {
		spin_lock_irqsave(&pctx->tg_lock, flags);
		if (pctx->trigger_gen_info.IsB2Rsource == true) {
			spin_unlock_irqrestore(&pctx->tg_lock, flags);
			return;
		}
		spin_unlock_irqrestore(&pctx->tg_lock, flags);
	}

	if (pctx_pnl->cus.force_free_run_en == true) {
		DRM_INFO("[%s][%d] Force Free Run Mode\n", __func__, __LINE__);
		return;
	}

	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	if (series == MTK_TV_SERIES_MERAK_1) {
		if (bLowLatencyEn == true) {
			ivsSource = RW_BK_UPD_OP2;
			ovsSource = RW_BK_UPD_DISP;
		} else {
			ivsSource = REF_PULSE;
			ovsSource = REF_PULSE;
		}
	} else if (series == MTK_TV_SERIES_MERAK_2) {
		if (bFrcFBL) {	// frc bypass
			ivsSource = RW_BK_UPD_IP_IPM_W;
			ovsSource = RW_BK_UPD_FRC2;
		} else {
			ivsSource = RW_BK_UPD_FRC1;
			ovsSource = RW_BK_UPD_FRC2;
		}
	} else {
		DRM_ERROR("%s(%d): invalid chip series\n", __func__, __LINE__);
		return;
	}

	if (bRIU == false) {
		pctx_pnl->outputTiming_info.eFrameSyncState
			= E_PNL_FRAMESYNC_STATE_PROP_IN;
	}

	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	drv_hwreg_render_pnl_set_vttLockPhaseDiff(
		&paramOut,
		bRIU,
		true,
		u32PhasediffValue);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A3_h16[4:0] = ivsSource
	//BKA3A3_h16[12:8] = ovsSource
	drv_hwreg_render_pnl_set_vttSourceSelect(
		&paramOut,
		bRIU,
		ivsSource,
		ovsSource);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if ((pctx_pnl->outputTiming_info.eFrameSyncMode
		== VIDEO_CRTC_FRAME_SYNC_VTTV) ||
		(pctx_pnl->outputTiming_info.eFrameSyncMode
		== VIDEO_CRTC_FRAME_SYNC_VTTPLL)) {
		//h31[0] = 1
		drv_hwreg_render_pnl_set_vttLockModeEnable(
			&paramOut,
			bRIU,
			true);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
	}

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return;
	} else if ((bRIU == false) && (RegCount != 0)) {
		if (mtk_tv_sm_ml_get_mem_index(&pctx_video->disp_ml))
			DRM_INFO("mtk_tv_sm_ml_get_mem_index error");

		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);
	}
}

	//h24[12] = 0
	drv_hwreg_render_pnl_set_vttLockLastVttLatchEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return;
	} else if ((bRIU == false) && (RegCount != 0)) {
		if (mtk_tv_sm_ml_get_mem_index(&pctx_video->disp_ml))
			DRM_INFO("mtk_tv_sm_ml_get_mem_index error");

		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);
	}

}

int _mtk_video_set_low_latency_mode(
	struct mtk_tv_kms_context *pctx,
	uint64_t bLowLatencyEn)
{
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	drm_en_vttv_source_type ivsSource = REF_PULSE, ovsSource = REF_PULSE;
	uint64_t outputVTT = 0, phase_diff = 0;

	enum drm_mtk_vplane_buf_mode vplane_buf_mode = MTK_VPLANE_BUF_MODE_MAX;
	struct ext_crtc_prop_info *crtc_props = NULL;
	uint64_t plane_id = 0;
	int ret = 0;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;
	crtc_props = pctx->ext_crtc_properties;
	plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];

	//decide ivs/ovs source
	_mtk_video_get_vplane_buf_mode(pctx, &vplane_buf_mode);
	switch (vplane_buf_mode) {
	case MTK_VPLANE_BUF_MODE_SW:
		ivsSource = REF_PULSE;
		ovsSource = REF_PULSE;
		break;
	case MTK_VPLANE_BUF_MODE_HW:
		ivsSource = RW_BK_UPD_OP2;
		ovsSource = RW_BK_UPD_DISP;
		break;
	case MTK_VPLANE_BUF_MODE_BYPASSS:
		ivsSource = RW_BK_UPD_IP_IPM_W;
		ovsSource = RW_BK_UPD_OP1_OPM_R;
		break;
	default:
		DRM_ERROR("[%s][%d] invalid video buffer mode!!\n",
			__func__, __LINE__);
	}
	DRM_INFO("[%s][%d]vplane_buf_mode: %d, ivsSource: %d, ovsSource: %d\n",
			__func__, __LINE__, vplane_buf_mode, ivsSource, ovsSource);

	//get output VTT
	outputVTT = pctx_pnl->outputTiming_info.u16OutputVTotal;


	//set lock user phase diff
	ret = _mtk_video_panel_set_vttv_phase_diff(pctx, &phase_diff);
	if (ret) {
		DRM_ERROR("[%s, %d]: set vttv phase diff error\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	return 0;
}

static int _mtk_video_set_vrr_mode(
	struct mtk_tv_kms_context *pctx,
	uint64_t input_vfreq)
{
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	bool bRIU = false;
	uint16_t RegCount = 0;
	uint8_t ivs = 0, ovs = 0;
	uint64_t output_vfreq = 0;
	uint64_t default_vfreq = 0, outputVTT = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;
	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FRAMELOCK;

	//find ivs/ovs
	_mtk_video_get_ivsovs_ratio(pctx_pnl, input_vfreq, &ivs, &ovs);

	//calc output vfreq
	if (ivs > 0) {
		output_vfreq = input_vfreq * ovs / ivs;
		pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		pctx_pnl->outputTiming_info.u8FRC_in = ivs;
		pctx_pnl->outputTiming_info.u8FRC_out = ovs;

		DRM_INFO("[%s][%d] ivs: %td, ovs: %td, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs,
			(ptrdiff_t)output_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc output vfreq fail, ivs: %td, ovs: %td\n",
			__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs);

		DRM_INFO("[%s][%d]calc output vfreq fail, output_vfreq: %td\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
	}

	//calc default vfreq
	if (pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt > 0) {
		default_vfreq =
			((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
			((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));

		DRM_INFO("[%s][%d]typ_htt:%td, typ_vtt:%td, typ_dclk:%td, default_vfreq:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc default vfreq fail,typ_htt:%td, typ_vtt:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt);

		DRM_INFO("[%s][%d]calc default vfreq fail,typ_dclk:%td, default_vfreq: %td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	}

	//calc output vtt
	if (output_vfreq > 0) {
		outputVTT = ((pctx_pnl->info.typ_vtt*default_vfreq) + (output_vfreq/2))
			/ output_vfreq;
		DRM_INFO("[%s][%d] cal outputVTT: %td, maxVTT: %td, minVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT,
			(ptrdiff_t)pctx_pnl->info.max_vtt, (ptrdiff_t)pctx_pnl->info.min_vtt);
		if (pctx_pnl->info.min_vtt > outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.min_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		} else if (pctx_pnl->info.max_vtt < outputVTT) {
			pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.max_vtt;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td\n",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
			return -EPERM;
		}

		DRM_INFO("[%s][%d] in tolerance range, outputVTT: %td\n",
		__func__, __LINE__, (ptrdiff_t)outputVTT);
		pctx_pnl->outputTiming_info.u16OutputVTotal = outputVTT;
	} else {
		DRM_INFO("[%s][%d] calc output vtt fail, outputVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT);
		pctx_pnl->outputTiming_info.u32OutputVfreq = default_vfreq;
		DRM_INFO("[%s][%d] output_vfreq: %td => free run with default vfreq\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
		return -EPERM;
	}

	//enable rwdiff auto-trace
	//BKA38B_40[11] = 1
	drv_hwreg_render_pnl_set_rwDiffTraceEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA38B_44[4:0] = 1
	drv_hwreg_render_pnl_set_rwDiffTraceDepositCount(
		&paramOut,
		bRIU,
		FRAMESYNC_VRR_DEPOSIT_COUNT);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//Turn on Framelock 1:1
	//BKA3A2_01[1] = 1
	drv_hwreg_render_pnl_set_forceInsertBypass(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_01[0] = 1
	drv_hwreg_render_pnl_set_InsertRecord(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_11[0] = 1
	drv_hwreg_render_pnl_set_fakeInsertPEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_10[3] = 1
	drv_hwreg_render_pnl_set_stateSwModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_10[6:4] = 4
	drv_hwreg_render_pnl_set_stateSwMode(
		&paramOut,
		bRIU,
		FRAMESYNC_VRR_STATE_SW_MODE);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//VRR upper bound/lower bound is control by Tgen panel only
	//disable VRR uper bound /lower bound
	//BKA3A2_1e[0] = 1
	//BKA3A2_1e[1] = 0
	//BKA3A2_1e[2] = 0
	//BKA3A2_18[15:0] = 0
	//BKA3A2_19[15:0] = 0
	//BKA3A2_1a[15:0] = 0xffff
        //if enable lower bound usr mode, set protect_vend
	drv_hwreg_render_pnl_set_FrameLockVttRange(
		&paramOut,
		bRIU,
		true,
		true,
		true,
		0,
		pctx_pnl->info.min_vtt_panelprotect,
		pctx_pnl->info.max_vtt_panelprotect);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_23[15:0] = 2
	drv_hwreg_render_pnl_set_chaseTotalDiffRange(
		&paramOut,
		bRIU,
		FRAMESYNC_VRR_CHASE_TOTAL_DIFF_RANGE);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_31[15:0] = 2
	drv_hwreg_render_pnl_set_chasePhaseTarget(
		&paramOut,
		bRIU,
		FRAMESYNC_VRR_CHASE_PHASE_TARGET);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_32[15:0] = 2
	drv_hwreg_render_pnl_set_chasePhaseAlmost(
		&paramOut,
		bRIU,
		FRAMESYNC_VRR_CHASE_PHASE_ALMOST);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_10[15] = 1
	drv_hwreg_render_pnl_set_statusReset(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A1_24[13] = 0
	drv_hwreg_render_pnl_set_vttLockLastVttLatchToggle(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A0_22[0] = 1
	drv_hwreg_render_pnl_set_framelockVcntEnableDb(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A2_10[15] = 0
	drv_hwreg_render_pnl_set_statusReset(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);

	return 0;
}

static int _mtk_video_set_freerun_vfreq(
	struct mtk_tv_kms_context *pctx,
	uint64_t input_vfreq, uint64_t output_vfreq)
{
	int ret = 0;
	struct mtk_panel_context *pctx_pnl = NULL;
	drm_en_vttv_source_type ivsSource = REF_PULSE, ovsSource = REF_PULSE;
	uint8_t ivs = 0, ovs = 0;
	uint64_t default_vfreq = 0, outputVTT = 0, phase_diff = 0;
	uint16_t m_delta = 0;
	bool bRIU = false;
	uint16_t RegCount = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_pnl = &pctx->panel_priv;
	m_delta = pctx_pnl->cus.m_del;

	//decide ivs/ovs source
	ivsSource = REF_PULSE;
	ovsSource = REF_PULSE;
	DRM_INFO("[%s][%d] ivsSource: %d, ovsSource: %d\n",
			__func__, __LINE__, ivsSource, ovsSource);

	DRM_INFO("[%s][%d] input_vfreq: %lld, output_vfreq: %lld\n",
			__func__, __LINE__, input_vfreq, output_vfreq);

	//set output vfreq
	if (output_vfreq != 0)
		pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;

	DRM_INFO("[%s][%d] ivs: %td, ovs: %td, output_vfreq: %td\n",
		__func__, __LINE__, (ptrdiff_t)ivs, (ptrdiff_t)ovs,
		(ptrdiff_t)output_vfreq);

	//calc default vfreq
	if (pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt > 0) {
		default_vfreq =
			((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
			((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));

		DRM_INFO("[%s][%d]typ_htt:%td, typ_vtt:%td, typ_dclk:%td, default_vfreq:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	} else {
		DRM_INFO("[%s][%d]calc default vfreq fail,typ_htt:%td, typ_vtt:%td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_htt,
			(ptrdiff_t)pctx_pnl->info.typ_vtt);

		DRM_INFO("[%s][%d]calc default vfreq fail,typ_dclk:%td, default_vfreq: %td\n",
			__func__, __LINE__,
			(ptrdiff_t)pctx_pnl->info.typ_dclk,
			(ptrdiff_t)default_vfreq);
	}

	//calc output vtt
	if (output_vfreq > 0) {
		outputVTT = ((pctx_pnl->info.typ_vtt*default_vfreq) + (output_vfreq/2))
			/ output_vfreq;
		DRM_INFO("[%s][%d] input_vfreq: %lld, output_vfreq: %lld\n",
			__func__, __LINE__, input_vfreq, output_vfreq);
		if (pctx_pnl->info.min_vtt > outputVTT) {
			outputVTT = pctx_pnl->info.min_vtt;
			if (outputVTT == 0)
				outputVTT = 1;
			output_vfreq = ((pctx_pnl->info.typ_vtt*default_vfreq) + (outputVTT/2))
				/ outputVTT;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
		DRM_INFO(" output_vfreq: %td\n", (ptrdiff_t)output_vfreq);
		} else if (pctx_pnl->info.max_vtt < outputVTT) {
			outputVTT = pctx_pnl->info.max_vtt;
			if (outputVTT == 0)
				outputVTT = 1;
			output_vfreq = ((pctx_pnl->info.typ_vtt*default_vfreq) + (outputVTT/2))
				/ outputVTT;
			pctx_pnl->outputTiming_info.u32OutputVfreq = output_vfreq;
		DRM_INFO("[%s][%d] out of tolerance range free run with u16OutputVTotal: %td",
		__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
		DRM_INFO(" output_vfreq: %td\n", (ptrdiff_t)output_vfreq);
		} else {
			DRM_INFO("[%s][%d] in tolerance range, outputVTT: %td\n",
			__func__, __LINE__, (ptrdiff_t)outputVTT);
		}
		pctx_pnl->outputTiming_info.u16OutputVTotal = outputVTT;
		DRM_INFO("[%s][%d] pctx_pnl->outputTiming_info.u16OutputVTotal: %d\n",
			__func__, __LINE__, pctx_pnl->outputTiming_info.u16OutputVTotal);
	} else {
		pctx_pnl->outputTiming_info.u16OutputVTotal = pctx_pnl->info.typ_vtt;
		DRM_INFO("[%s][%d] calc output vtt fail, u16OutputVTotal: %td\n",
			__func__, __LINE__, (ptrdiff_t)pctx_pnl->outputTiming_info.u16OutputVTotal);
	}

	//set lock user phase diff
	phase_diff = outputVTT*FRAMESYNC_VTTV_NORMAL_PHASE_DIFF/
		FRAMESYNC_VTTV_PHASE_DIFF_DIV_1000;

	DRM_INFO("[%s][%d] phase_diff: %td\n",
		__func__, __LINE__, (ptrdiff_t)phase_diff);

	//adjust output vtt in stub mode
	outputVTT = _mtk_video_adjust_output_vtt_ans_stubMode(
			pctx, pctx_pnl, pctx_pnl->outputTiming_info.u16OutputVTotal);

	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FREERUN;
	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));

	paramOut.reg = reg;

	//h31[0] = 0
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A3_h16[4:0] = ivsSource
	//BKA3A3_h16[12:8] = ovsSource
	drv_hwreg_render_pnl_set_vttSourceSelect(
		&paramOut,
		bRIU,
		ivsSource,
		ovsSource);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h20[15] = 0
	//h20[14:8] = 0
	//h20[7:0] = 0
	drv_hwreg_render_pnl_set_centralVttChange(
		&paramOut,
		bRIU,
		false,
		0,
		0);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h22[15:3] = phase_diff
	//h23[15] = 1
	drv_hwreg_render_pnl_set_vttLockPhaseDiff(
		&paramOut,
		bRIU,
		1,
		phase_diff);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[4] = 0
	//h24[3:0] = LockKeepSequenceTh
	drv_hwreg_render_pnl_set_vttLockKeepSequenceEnable(
		&paramOut,
		bRIU,
		0,
		FRAMESYNC_RECOV_LOCKKEEPSEQUENCETH);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[10] = 1
	//h30[15] = 1
	//h30[14:0] = 0
	drv_hwreg_render_pnl_set_vttLockDiffLimit(
		&paramOut,
		bRIU,
		true,
		m_delta);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//VTT uper bound/lower bound is control by Tgen panel only
	//disable VTTV uper bound /lower bound
	//h33[11] = 0
	//h33[12] = 0
	//h36[15:0] = pctx_pnl->stPanelType.m_wPanelMaxVTotal
	//h37[15:0] = after y_trigger
	drv_hwreg_render_pnl_set_vttLockVttRange(
		&paramOut,
		bRIU,
		false,
		0xffff,
		0);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h33[13] = 0
	drv_hwreg_render_pnl_set_vttLockVsMaskMode(
		&paramOut,
		bRIU,
		0);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h32[7:0] = ivs
	//h32[15:8] = ovs
	drv_hwreg_render_pnl_set_vttLockIvsOvs(
		&paramOut,
		bRIU,
		0,
		0);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h3e[15:0] = 9
	drv_hwreg_render_pnl_set_vttLockLockedInLimit(
		&paramOut,
		bRIU,
		FRAMESYNC_VTTV_LOCKED_LIMIT);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[13] = 1
	drv_hwreg_render_pnl_set_vttLockUdSwModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[9] = 0
	drv_hwreg_render_pnl_set_vttLockIvsShiftEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[8] = 0
	drv_hwreg_render_pnl_set_vttLockProtectEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h23[15:0] = output VTT
		drv_hwreg_render_pnl_set_vtt(
			&paramOut,
			bRIU,
			outputVTT);

	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[0] = 0
	drv_hwreg_render_pnl_set_vttLockModeEnable(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h24[12] = 1
	drv_hwreg_render_pnl_set_vttLockLastVttLatchEnable(
		&paramOut,
		bRIU,
		1);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);

	return ret;
}

static int _mtk_video_turn_off_framesync_mode(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	bool bRIU = false;
	bool fs_mode = false;
	uint16_t RegCount = 0;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;
	fs_mode = pctx_pnl->cus.fs_mode;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	if (pctx_pnl->outputTiming_info.eFrameSyncMode ==
		VIDEO_CRTC_FRAME_SYNC_FRAMELOCK) {
		//turn off vrr mode
		//BKA3A1_24[13] = 1
		drv_hwreg_render_pnl_set_vttLockLastVttLatchToggle(
			&paramOut,
			bRIU,
			true);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
		//BKA3A0_22[0] = 0
		drv_hwreg_render_pnl_set_framelockVcntEnableDb(
			&paramOut,
			bRIU,
			false);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
	} else if (pctx_pnl->outputTiming_info.eFrameSyncMode ==
		VIDEO_CRTC_FRAME_SYNC_VTTV) {
		//turn off vttv mode
		//h20[15] = 0
		//h20[14:8] = 0
		//h20[7:0] = 0
		drv_hwreg_render_pnl_set_centralVttChange(
			&paramOut,
			bRIU,
			false,
			0,
			0);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
		//BKA3A1_31[0] = 0
		drv_hwreg_render_pnl_set_vttLockModeEnable(
			&paramOut,
			bRIU,
			false);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
		//BKA3A1_24[12] = 1
		drv_hwreg_render_pnl_set_vttLockLastVttLatchEnable(
			&paramOut,
			bRIU,
			true);
		RegCount = RegCount + paramOut.regCount;
		paramOut.reg = reg + RegCount;
		if (fs_mode) {
			//turn off vtt2fpll mode
			//lpll_h4f[0] = 0
			drv_hwreg_render_pnl_set_vtt2setmode(
				&paramOut,
				bRIU,
				false);
			RegCount = RegCount + paramOut.regCount;
			paramOut.reg = reg + RegCount;
			}
		}

	if (RegCount > HWREG_VIDEO_REG_NUM_PANEL) {
		DRM_ERROR("[%s, %d]: RegCount <%d> is over limit <%d>\n",
			__func__, __LINE__, RegCount, HWREG_VIDEO_REG_NUM_PANEL);
		return -EPERM;
	} else if ((bRIU == false) && (RegCount != 0))
		_mtk_video_panel_add_framesyncML(pctx, reg, RegCount);

	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FREERUN;

	return 0;
}

static void __dump_frc_table(struct drm_st_pnl_frc_table_info *table, size_t len)
{
	int FRCType;

	if (len != PNL_FRC_TABLE_MAX_INDEX)
		return;

	DRM_INFO("==== %s ====\n", __func__);
	for (FRCType = 0; FRCType < len; FRCType++) {
		DRM_INFO("index = %d, u16LowerBound = %d\n",
			FRCType, table[FRCType].u16LowerBound);
		DRM_INFO("index = %d, u16HigherBound = %d\n",
			FRCType, table[FRCType].u16HigherBound);
		DRM_INFO("index = %d, u8FRC_in = %d\n",
			FRCType, table[FRCType].u8FRC_in);
		DRM_INFO("index = %d, u8FRC_out = %d\n",
			FRCType, table[FRCType].u8FRC_out);
	}

}

int _mtk_video_set_customize_frc_table(
	struct mtk_panel_context *pctx_pnl,
	struct drm_st_pnl_frc_table_info *customizeFRCTableInfo)
{
	if (!pctx_pnl || !customizeFRCTableInfo) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx, customizeFRCTableInfo=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl,
			(unsigned long long)customizeFRCTableInfo);
		return -ENODEV;
	}

	memset(pctx_pnl->frc_info,
		0,
		sizeof(struct drm_st_pnl_frc_table_info)*PNL_FRC_TABLE_MAX_INDEX);
	memcpy(pctx_pnl->frc_info,
		customizeFRCTableInfo,
		sizeof(struct drm_st_pnl_frc_table_info)*PNL_FRC_TABLE_MAX_INDEX);

	__dump_frc_table(pctx_pnl->frc_info, PNL_FRC_TABLE_MAX_INDEX);

	return 0;
}

static int _mtk_video_get_customize_frc_table(struct mtk_panel_context *pctx_pnl,
		void *CurrentFRCTableInfo, size_t len)
{
	if (!pctx_pnl || !CurrentFRCTableInfo) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx, CurrentFRCTableInfo=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl,
			(unsigned long long)CurrentFRCTableInfo);
		return -ENODEV;
	}

	memset(CurrentFRCTableInfo, 0, len);
	memcpy(CurrentFRCTableInfo, pctx_pnl->frc_info, len);

	if (len != sizeof(struct drm_st_pnl_frc_table_info) * PNL_FRC_TABLE_MAX_INDEX)
		DRM_ERROR("%s(%d): invalid blobs size for frc table\n", __func__, __LINE__);
	else
		__dump_frc_table(CurrentFRCTableInfo, PNL_FRC_TABLE_MAX_INDEX);

	return 0;
}

int mtk_video_get_vttv_input_vfreq(
	struct mtk_tv_kms_context *pctx,
	uint64_t plane_id,
	uint64_t *input_vfreq)
{
	struct drm_mode_object *obj = NULL;
	struct drm_plane *plane = NULL;
	struct mtk_drm_plane *mplane = NULL;
	struct mtk_tv_kms_context *pctx_plane = NULL;
	struct mtk_video_context *pctx_video = NULL;
	unsigned int plane_inx = 0;
	struct video_plane_prop *plane_props = NULL;

	if (!pctx || !input_vfreq) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx, input_vfreq=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx,
			(unsigned long long)input_vfreq);
		return -ENODEV;
	}

	obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
	plane = obj_to_plane(obj);
	mplane = to_mtk_plane(plane);
	pctx_plane = (struct mtk_tv_kms_context *)mplane->crtc_private;
	pctx_video = &pctx_plane->video_priv;
	plane_inx = mplane->video_index;
	plane_props = (pctx_video->video_plane_properties + plane_inx);

	*input_vfreq = plane_props->prop_val[EXT_VPLANE_PROP_INPUT_SOURCE_VFREQ];

	return 0;
}

void _mtk_video_get_internal_freerun_update(
	struct mtk_tv_kms_context *pctx,
	uint64_t *framesync_mode,
	bool *bchangeToFreerun)
{
	if ((!pctx) || (!framesync_mode) || (!bchangeToFreerun))
		return;

	if (((!pctx->stub) && (mtk_video_panel_get_framesync_mode_en(pctx) == true) &&
		(IS_FRAMESYNC(*framesync_mode) == true))) {
		*framesync_mode = VIDEO_CRTC_FRAME_SYNC_FREERUN;
		*bchangeToFreerun = true;
		DRM_INFO("internal change freerun = %d\n",
			*bchangeToFreerun);
	}
}

int _mtk_video_set_framesync_mode(
	struct mtk_tv_kms_context *pctx)
{
	int ret = 0;
	struct ext_crtc_prop_info *crtc_props = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	uint64_t input_vfreq = 0, framesync_mode = 0, output_vfreq = 0;
	uint64_t framesync_plane_id = 0;
	bool bLowLatencyEn = false;
	bool fs_mode = false;
	bool bNoSignal = false;
	bool bchangeToFreerun = false;
	struct drm_mode_object *obj = NULL;
	unsigned long flags;
	unsigned int plane_inx;
	uint64_t plane_id = 0;
	struct mtk_drm_plane *mplane = NULL;
	struct drm_plane *plane = NULL;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	crtc_props = pctx->ext_crtc_properties;
	pctx_pnl = &pctx->panel_priv;
	pctx_pnl->outputTiming_info.AutoForceFreeRun = false;
	fs_mode = pctx_pnl->cus.fs_mode;
	framesync_mode = crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
	bNoSignal = (bool)crtc_props->prop_val[E_EXT_CRTC_PROP_NO_SIGNAL];
	if (framesync_mode != VIDEO_CRTC_FRAME_SYNC_FREERUN) {
		framesync_plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
		obj = drm_mode_object_find(pctx->drm_dev, NULL, framesync_plane_id,
			DRM_MODE_OBJECT_ANY);
		if (!obj) {
			DRM_ERROR("[%s, %d]: null ptr! obj=0x%llx\n",
				__func__, __LINE__, (unsigned long long)obj);
			return -ENODEV;
		}
	}

	/* internal trigger freerun */
	/* set bchangeToFreerun to true to set framerate to typical */
	_mtk_video_get_internal_freerun_update(
		pctx, &framesync_mode, &bchangeToFreerun);

	if (pctx_pnl->outputTiming_info.FrameSyncModeChgBypass && bNoSignal)
		pctx_pnl->outputTiming_info.FrameSyncModeChgBypass = false;

	DRM_INFO("[%s][%d] framesync_mode = %lld, framesync_state = %d, Bypass/NoSignal = %d,%d\n",
		__func__, __LINE__, framesync_mode,
		pctx_pnl->outputTiming_info.eFrameSyncState,
		pctx_pnl->outputTiming_info.FrameSyncModeChgBypass,
		bNoSignal);

	spin_lock_irqsave(&pctx->tg_lock, flags);
	if (pctx->b2r_src_stubmode) {
		pctx->trigger_gen_info.IsB2Rsource = RENDER_COMMON_TRIGEN_INPUT_B2R0;
	} else if ((pctx->b2r_src_stubmode == false) && (pctx->stub == true)) {
		pctx->trigger_gen_info.IsB2Rsource = RENDER_COMMON_TRIGEN_INPUT_IP0;
	}
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	bLowLatencyEn =
		(bool)crtc_props->prop_val[E_EXT_CRTC_PROP_LOW_LATENCY_MODE];

	if ((pctx_pnl->outputTiming_info.FrameSyncModeChgBypass) && (bLowLatencyEn)) {
		DRM_INFO("[%s][%d] bypass frame sync mode chg.\n",
			__func__, __LINE__);
		return 0;
	}

	switch (framesync_mode) {
	case VIDEO_CRTC_FRAME_SYNC_VTTV:
		mtk_video_get_vttv_input_vfreq(pctx,
			crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID],
			&input_vfreq);
		DRM_INFO("[%s][%d] EXT_VPLANE_PROP_INPUT_SOURCE_VFREQ= %td\n",
			__func__, __LINE__, (ptrdiff_t)input_vfreq);

		bLowLatencyEn =
			(bool)crtc_props->prop_val[E_EXT_CRTC_PROP_LOW_LATENCY_MODE];
		_mtk_video_turn_off_framesync_mode(pctx);
		pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_VTTV;

		ret = mtk_trigger_gen_setup(pctx, true, true);
		if (ret != 0)
			DRM_ERROR("[%s, %d]: mtk_trigger_gen_setup error\n",
			__func__, __LINE__);

		if (fs_mode)
			ret = _mtk_video_set_vtt2fpll_mode(pctx, input_vfreq, bLowLatencyEn);
		else
			ret = _mtk_video_set_vttv_mode(pctx, input_vfreq, bLowLatencyEn);

		if (ret == -EPERM) {
			pctx_pnl->outputTiming_info.AutoForceFreeRun = true;
			output_vfreq = pctx_pnl->outputTiming_info.u32OutputVfreq;
			ret = _mtk_video_set_freerun_vfreq(pctx, input_vfreq, output_vfreq);
		}
		mtk_tv_panel_debugvttvinfo_print(pctx, 1);
		break;
	case VIDEO_CRTC_FRAME_SYNC_FRAMELOCK:
		mtk_video_get_vttv_input_vfreq(pctx,
			crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID],
			&input_vfreq);
		_mtk_video_turn_off_framesync_mode(pctx);
		pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FRAMELOCK;
		ret = mtk_trigger_gen_setup(pctx, true, true);
		if (ret != 0)
			DRM_ERROR("[%s, %d]: mtk_trigger_gen_setup error\n",
			__func__, __LINE__);

		_mtk_video_set_vrr_mode(pctx, input_vfreq);
		mtk_tv_panel_debugvttvinfo_print(pctx, 2);

		/*
		 * XXX:
		 * in vrr case, enable trigger gen bypass here,
		 * consider vrr on->off should be shameless case,
		 * don't change the trigger gen setting or the video will flashed.
		 * Also the bypass flag disable in trigger gen ioctl,
		 * which means the video do stream off->on (mute), re-set the trigger gen
		 */
		pctx_pnl->outputTiming_info.FrameSyncModeChgBypass = true;
		mtk_trigger_gen_set_bypass(true);
		break;
	case VIDEO_CRTC_FRAME_SYNC_FREERUN:
		if ((bchangeToFreerun) &&
			((pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt) != 0))
			output_vfreq =
				((uint64_t)pctx_pnl->info.typ_dclk*VFRQRATIO_1000) /
				((uint32_t)(pctx_pnl->info.typ_htt*pctx_pnl->info.typ_vtt));
		else
			output_vfreq =
			crtc_props->prop_val[E_EXT_CRTC_PROP_FREERUN_VFREQ];

		DRM_INFO("[%s][%d] VIDEO_CRTC_FRAME_SYNC_FREERUN mode\n",
			__func__, __LINE__);
		DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_FREERUN_VFREQ= %td\n",
			__func__, __LINE__, (ptrdiff_t)output_vfreq);
		_mtk_video_turn_off_framesync_mode(pctx);
		pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FREERUN;
		_mtk_video_set_freerun_vfreq(pctx, input_vfreq, output_vfreq);
		mtk_tv_panel_debugvttvinfo_print(pctx, 3);
		break;
	default:
		DRM_ERROR("[%s][%d] Not Support framesync_mode.\n", __func__, __LINE__);
		break;
	}

	if (framesync_mode != VIDEO_CRTC_FRAME_SYNC_FREERUN) {
		plane_id = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
		obj = drm_mode_object_find(pctx->drm_dev, NULL, plane_id, DRM_MODE_OBJECT_ANY);
		if (!obj) {
			DRM_ERROR("[%s, %d]: null ptr! obj=0x%llx\n",
				__func__, __LINE__, (unsigned long long)obj);
			return -ENODEV;
		}

		plane = obj_to_plane(obj);
		if (!plane) {
			DRM_ERROR("[%s, %d]: null ptr! plane=0x%llx\n",
				__func__, __LINE__, (unsigned long long)plane);
			return -ENODEV;
		}

		mplane = to_mtk_plane(plane);
		if (!mplane) {
			DRM_ERROR("[%s, %d]: null ptr! mplane=0x%llx\n",
				__func__, __LINE__, (unsigned long long)mplane);
			return -ENODEV;
		}

		plane_inx = mplane->video_index;

		mtk_video_display_set_timing(plane_inx, pctx,
			(uint32_t)input_vfreq, (uint32_t)pctx_pnl->outputTiming_info.u32OutputVfreq);
	}

	return ret;
}

int mtk_get_framesync_locked_flag(
	struct mtk_tv_kms_context *pctx)
{
	bool bRIU = true, locked_total = false, locked_phase = false;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	if (pctx->stub) {
		pctx_pnl->outputTiming_info.locked_flag = true;
		return 0;
	}

	switch (pctx_pnl->outputTiming_info.eFrameSyncMode) {
	case VIDEO_CRTC_FRAME_SYNC_VTTV:
		drv_hwreg_render_pnl_get_vttvLockedFlag(&paramOut, bRIU);
		pctx_pnl->outputTiming_info.locked_flag = paramOut.reg[0].val;
		break;
	case VIDEO_CRTC_FRAME_SYNC_FRAMELOCK:
		drv_hwreg_render_pnl_get_chaseTotalStatus(&paramOut, bRIU);
		locked_total = paramOut.reg[0].val;
		drv_hwreg_render_pnl_get_chasePhaseStatus(&paramOut, bRIU);
		locked_phase = paramOut.reg[0].val;
		pctx_pnl->outputTiming_info.locked_flag = (locked_total && locked_phase);
		break;
	default:
		pctx_pnl->outputTiming_info.locked_flag = false;
		break;
	}

	return 0;
}

bool mtk_video_panel_get_framesync_mode_en(
	struct mtk_tv_kms_context *pctx)
{
	bool bRIU = true, framesync_en = false;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	switch (pctx_pnl->outputTiming_info.eFrameSyncMode) {
	case VIDEO_CRTC_FRAME_SYNC_VTTV:
		drv_hwreg_render_pnl_get_vttLockModeEnable(&paramOut, bRIU);
		framesync_en = paramOut.reg[0].val;
		break;
	case VIDEO_CRTC_FRAME_SYNC_FRAMELOCK:
		drv_hwreg_render_pnl_get_framelockVcntEnableDb(&paramOut, bRIU);
		framesync_en = paramOut.reg[0].val;
		break;
	default:
		framesync_en = false;
		break;
	}

	return framesync_en;
}

int mtk_video_panel_update_framesync_state(
	struct mtk_tv_kms_context *pctx)
{
	if (!pctx) {
		DRM_ERROR("[%s, %d]: null ptr! pctx=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx);
		return -ENODEV;
	}

	switch (pctx->panel_priv.outputTiming_info.eFrameSyncState) {
	case E_PNL_FRAMESYNC_STATE_PROP_IN:
		pctx->panel_priv.outputTiming_info.eFrameSyncState =
			E_PNL_FRAMESYNC_STATE_PROP_FIRE;
		break;
	case E_PNL_FRAMESYNC_STATE_IRQ_IN:
		pctx->panel_priv.outputTiming_info.eFrameSyncState =
			E_PNL_FRAMESYNC_STATE_IRQ_FIRE;
		break;
	default:
		DRM_INFO("[%s, %d]: FrameSyncState=%d\n",
			__func__, __LINE__,
			pctx->panel_priv.outputTiming_info.eFrameSyncState);
		break;
	}

	return 0;
}

int mtk_mod_clk_init(struct device *dev)
{
	int ret = 0;
	struct device_node *np = NULL;
	uint32_t clk_version = 0;
	struct mtk_tv_kms_context *pctx;

	np = of_find_compatible_node(NULL, NULL, "MTK-drm-tv-kms");
	clk_version = get_dts_u32(np, "Clk_Version");

	if (dev == NULL) {
		MOD_MSG_POINTER_CHECK();
		return -ENXIO;
	}

	pctx = dev_get_drvdata(dev);
	if (!pctx) {
		pr_err("%s: missing pctx\n", __func__);
		return -ENODEV;
	}

	switch (pctx->clk_version) {
	case MOD_CLK_VER_0:
		ret = mtk_mod_clk_initV1(dev);
		break;
	case MOD_CLK_VER_1:
		ret = mtk_mod_clk_initV2(dev);
		break;
	case MOD_CLK_VER_2:
		ret = mtk_mod_clk_initV3(dev);
		break;
	case MOD_CLK_VER_3:
		ret = mtk_mod_clk_initV4(dev);
		break;
	case MOD_CLK_VER_4:
		ret = mtk_mod_clk_initV5(dev);
		break;
	case MOD_CLK_VER_5:
		ret = mtk_mod_clk_initV6(dev);
		break;
	default:
		pr_err("%s: invalid mod clk version %d\n",
			__func__, pctx->clk_version);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_mod_clk_init);

int _mtk_render_check_vbo_ctrlbit_feature(struct mtk_panel_context *pctx_pnl,
		uint8_t u8connectid, en_drv_vbo_ctrlbit_feature enfeature)
{
	uint32_t retValue = 0;
	const uint8_t gfid_ratio = 2;
	uint32_t u32OutputVfreq = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	u32OutputVfreq = pctx_pnl->outputTiming_info.u32OutputVfreq/VFRQRATIO_1000;
	switch (enfeature) {
	case E_DRV_VBO_CTRLBIT_GLOBAL_FRAMEID:
		if (u8connectid == CONNECTID_MAIN)
			retValue = pctx_pnl->u8controlbit_gfid;
		else if (u8connectid == CONNECTID_DELTA)
			retValue = pctx_pnl->u8controlbit_gfid;
		else if (u8connectid == CONNECTID_GFX) {
			if (IS_VFREQ_120HZ_GROUP(u32OutputVfreq))
				retValue = pctx_pnl->u8controlbit_gfid+gfid_ratio;
			else
				retValue = pctx_pnl->u8controlbit_gfid;
		}
		break;
	case E_DRV_VBO_CTRLBIT_HTOTAL:
		if (u8connectid == CONNECTID_MAIN)
			retValue = pctx_pnl->info.typ_htt;
		else if (u8connectid == CONNECTID_DELTA)
			retValue = pctx_pnl->extdev_info.typ_htt;
		else if (u8connectid == CONNECTID_GFX)
			retValue = pctx_pnl->gfx_info.typ_htt;

		break;
	case E_DRV_VBO_CTRLBIT_VTOTAL:
		if (u8connectid == CONNECTID_MAIN)
			retValue = pctx_pnl->info.typ_vtt;
		else if (u8connectid == CONNECTID_DELTA)
			retValue = pctx_pnl->extdev_info.typ_vtt;
		else if (u8connectid == CONNECTID_GFX)
			retValue = pctx_pnl->gfx_info.typ_vtt;

		break;
	case E_DRV_VBO_CTRLBIT_HSTART_POS:
		if (u8connectid == CONNECTID_MAIN)
			retValue = pctx_pnl->info.de_hstart;
		else if (u8connectid == CONNECTID_DELTA)
			retValue = pctx_pnl->extdev_info.de_hstart;
		else if (u8connectid == CONNECTID_GFX)
			retValue = pctx_pnl->gfx_info.de_hstart;

		break;
	case E_DRV_VBO_CTRLBIT_VSTART_POS:
		if (u8connectid == CONNECTID_MAIN)
			retValue = pctx_pnl->info.de_vstart;
		else if (u8connectid == CONNECTID_DELTA)
			retValue = pctx_pnl->extdev_info.de_vstart;
		else if (u8connectid == CONNECTID_GFX)
			retValue = pctx_pnl->gfx_info.de_vstart - pctx_pnl->gfx_info.vsync_st;

		break;
	default:
		break;
	}

	return retValue;

}
int mtk_render_set_vbo_ctrlbit(
	struct mtk_panel_context *pctx_pnl,
	struct drm_st_ctrlbits *pctrlbits)
{
	uint16_t ctrlbitIndex = 0;

	st_drv_ctrlbits stPnlCtrlbits = {0};

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: null ptr! pctx_pnl=0x%llx\n",
			__func__, __LINE__, (unsigned long long)pctx_pnl);
		return -ENODEV;
	}

	//Disable META DATA(Control Bit)
	if (pctx_pnl->cus.ctrl_bit_en == 0) {
		DRM_INFO("[%s, %d]: ctrl_bit_en=%d disable meta data\n",
		__func__, __LINE__, pctx_pnl->cus.ctrl_bit_en);
		return 0;
	}

	stPnlCtrlbits.u8ConnectorID = pctrlbits->u8ConnectorID;
	stPnlCtrlbits.u8CopyType = pctrlbits->u8CopyType;

	for (ctrlbitIndex = 0; ctrlbitIndex < CTRLBIT_MAX_NUM; ctrlbitIndex++) {
		stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].bEnable =
				pctrlbits->ctrlbit_item[ctrlbitIndex].bEnable;
		stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].u8LaneID =
				pctrlbits->ctrlbit_item[ctrlbitIndex].u8LaneID;
		stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].u8Lsb =
				pctrlbits->ctrlbit_item[ctrlbitIndex].u8Lsb;
		stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].u8Msb =
				pctrlbits->ctrlbit_item[ctrlbitIndex].u8Msb;
		stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].enFeature =
				(en_drv_vbo_ctrlbit_feature)pctrlbits
				->ctrlbit_item[ctrlbitIndex].enFeature;

		if ((stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].enFeature !=
			E_DRV_VBO_CTRLBIT_NO_FEATURE) &&
			(stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].enFeature !=
			E_DRV_VBO_CTRLBIT_NUM)) {
			stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].u32Value =
				_mtk_render_check_vbo_ctrlbit_feature
				(pctx_pnl, stPnlCtrlbits.u8ConnectorID,
				stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].enFeature);
		} else {
			stPnlCtrlbits.ctrlbit_item[ctrlbitIndex].u32Value =
				pctrlbits->ctrlbit_item[ctrlbitIndex].u32Value;
		}
	}

	return drv_hwreg_render_pnl_set_vbo_ctrlbit(&stPnlCtrlbits);
}

int mtk_render_set_swing_vreg(struct drm_st_out_swing_level *stdrmSwing, bool tcon_en)
{
	uint16_t SwingIndex = 0;
	st_out_swing_level stSwing = {0};

	if (!stdrmSwing) {
		DRM_ERROR("[%s, %d]: stdrmSwing is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	stSwing.usr_swing_level = stdrmSwing->usr_swing_level;
	stSwing.common_swing = stdrmSwing->common_swing;
	stSwing.ctrl_lanes = stdrmSwing->ctrl_lanes;

	if ((stSwing.usr_swing_level != 0) && (stSwing.ctrl_lanes <= DRV_VBO_MAX_NUM)) {
		for (SwingIndex = 0 ; SwingIndex < stSwing.ctrl_lanes ; SwingIndex++)
			stSwing.swing_level[SwingIndex] = stdrmSwing->swing_level[SwingIndex];
	} else {
		DRM_INFO("[%s, %d]: usr_swing_level setting def=%d lan=%d\n",
			__func__, __LINE__, stSwing.usr_swing_level, stSwing.ctrl_lanes);
		return 0;
	}
	drv_hwreg_render_pnl_set_swing_vreg(&stSwing, tcon_en);

	return 0;
}

int mtk_render_get_swing_vreg(struct drm_st_out_swing_level *stdrmSwing, bool tcon_en)
{
	uint16_t SwingIndex = 0;
	st_out_swing_level stSwing = {0};

	if (!stdrmSwing) {
		DRM_ERROR("[%s, %d]: stdrmSwing is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_swing_vreg(&stSwing, tcon_en);

	for (SwingIndex = 0 ; SwingIndex < DRV_VBO_MAX_NUM ; SwingIndex++)
		stdrmSwing->swing_level[SwingIndex] = stSwing.swing_level[SwingIndex];

	return 0;
}

int mtk_render_set_pre_emphasis(struct drm_st_out_pe_level *stdrmPE)
{
	#define DEFAULT_PE_LEVEL 0x10

	uint16_t PEIndex = 0;
	st_out_pe_level stPE = {0};

	if (!stdrmPE) {
		DRM_ERROR("[%s, %d]: stdrmPE is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	stPE.usr_pe_level = stdrmPE->usr_pe_level;
	stPE.common_pe = stdrmPE->common_pe;
	stPE.ctrl_lanes = stdrmPE->ctrl_lanes;

	if (stPE.usr_pe_level == 1) {
		if (stPE.ctrl_lanes <= DRV_VBO_MAX_NUM) {
			for (PEIndex = 0 ; PEIndex < stPE.ctrl_lanes ; PEIndex++)
				stPE.pe_level[PEIndex] = stdrmPE->pe_level[PEIndex];
		} else {
			DRM_ERROR("[%s, %d]: usr_pe_level setting not crrect def=%d lan=%d\n",
				__func__, __LINE__, stPE.usr_pe_level, stPE.ctrl_lanes);
			return -ENODEV;
		}
	} else {
		stPE.pe_level[PEIndex] = DEFAULT_PE_LEVEL;
	}

	drv_hwreg_render_pnl_set_pre_emphasis_enable();
	drv_hwreg_render_pnl_set_pre_emphasis(&stPE);

	#undef DEFAULT_PE_LEVEL
	return 0;
}

int mtk_render_get_pre_emphasis(struct drm_st_out_pe_level *stdrmPE)
{
	uint16_t PEIndex = 0;
	st_out_pe_level stPE = {0};

	if (!stdrmPE) {
		DRM_ERROR("[%s, %d]: stdrmPE is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_pre_emphasis(&stPE);

	for (PEIndex = 0 ; PEIndex < DRV_VBO_MAX_NUM ; PEIndex++)
		stdrmPE->pe_level[PEIndex] = stPE.pe_level[PEIndex];

	return 0;
}

int _mtk_render_check_linkif_epi(drm_pnl_link_if linkIF)
{
	if (((linkIF >= E_LINK_EPI28_8BIT_2PAIR_2KCML) &&
		(linkIF <= E_LINK_EPI24_10BIT_12PAIR_4KLVDS)) ||
		((linkIF >= E_LINK_EPI28_8BIT_16PAIR_4KCML) &&
		(linkIF <= E_LINK_EPI24_10BIT_16PAIR_4KLVDS)))
		return true;
	else
		return false;
}

int _mtk_render_check_linkif_usit(drm_pnl_link_if linkIF)
{
	if (((linkIF >= E_LINK_USIT_8BIT_6PAIR) &&
		(linkIF <= E_LINK_USIT_10BIT_12PAIR)) ||
		((linkIF >= E_LINK_USIT_8BIT_8PAIR) &&
		(linkIF <= E_LINK_USIT_10BIT_16PAIR)))
		return true;
	else
		return false;
}

int _mtk_render_get_ssc_linkif(
	drm_pnl_link_if linkIF, enum drm_pnl_ssc_link_if_sel *pessc_linkif)
{
	uint32_t u32rint_cnt_value = 0;
	uint32_t u32rint_th = 0;

	if (pessc_linkif == NULL) {
		DRM_ERROR("[%s, %d]: pessc_linkif is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_hw_rint_cnt_value(&u32rint_cnt_value);
	drv_hwreg_render_pnl_get_hw_rint_th(&u32rint_th);

	if ((linkIF == E_LINK_VB1) || (linkIF == E_LINK_VB1_TO_HDMITX)) {
		*pessc_linkif = E_LINK_SSC_VBY1;
	} else if ((linkIF == E_LINK_LVDS) || (linkIF == E_LINK_HSLVDS_1CH) ||
				(linkIF == E_LINK_HSLVDS_2CH)) {
		*pessc_linkif = E_LINK_SSC_LVDS;
	} else if ((linkIF >= E_LINK_MINILVDS_1BLK_3PAIR_6BIT) &&
			(linkIF <= E_LINK_MINILVDS_2BLK_6PAIR_8BIT)) {
		*pessc_linkif = E_LINK_SSC_MINILVDS;
	} else if (_mtk_render_check_linkif_epi(linkIF)) {
		if (u32rint_cnt_value > u32rint_th)
			*pessc_linkif = E_LINK_SSC_EPI30;
		else
			*pessc_linkif = E_LINK_SSC_EPI15;
	} else if ((linkIF >= E_LINK_CMPI27_8BIT_6PAIR) &&
			(linkIF <= E_LINK_CMPI27_10BIT_12PAIR)) {
		*pessc_linkif = E_LINK_SSC_CMPI20;
	} else if (_mtk_render_check_linkif_usit(linkIF)) {
		if (u32rint_cnt_value > u32rint_th)
			*pessc_linkif = E_LINK_SSC_USIT31;
		else
			*pessc_linkif = E_LINK_SSC_USIT16;
	} else if ((linkIF >= E_LINK_ISP_8BIT_6PAIR) &&
			(linkIF <= E_LINK_ISP_10BIT_12PAIR)) {
		*pessc_linkif = E_LINK_SSC_ISP18;
	} else if ((linkIF >= E_LINK_CHPI_8BIT_6PAIR) &&
			(linkIF <= E_LINK_CHPI_10BIT_6PAIR)) {
		*pessc_linkif = E_LINK_SSC_CHPI;
	} else {
		*pessc_linkif = E_LINK_SSC_VBY1;
		DRM_ERROR("[%s, %d]: linkIF is not support.\n",
			__func__, __LINE__);
	}

	return 0;
}

int _mtk_render_get_ssc_limit(
	enum drm_pnl_ssc_link_if_sel pessc_linkif,
	uint32_t *u32modulation_limit,
	uint32_t *u32deviation_limit)
{
	if ((!u32modulation_limit) || (!u32deviation_limit)) {
		DRM_ERROR("[%s, %d]: u32modulation_limit is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	switch (pessc_linkif) {
	case E_LINK_SSC_VBY1:
		*u32modulation_limit = SSC_MODULATION_LIMIT_VBY1;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_VBY1;
		break;
	case E_LINK_SSC_LVDS:
		*u32modulation_limit = SSC_MODULATION_LIMIT_LVDS;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_LVDS;
		break;
	case E_LINK_SSC_EPI15:
		*u32modulation_limit = SSC_MODULATION_LIMIT_EPI15;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_EPI15;
		break;
	case E_LINK_SSC_EPI30:
		*u32modulation_limit = SSC_MODULATION_LIMIT_EPI30;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_EPI30;
		break;
	case E_LINK_SSC_CEDS1:
		*u32modulation_limit = SSC_MODULATION_LIMIT_CEDS1;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_CEDS1;
		break;
	case E_LINK_SSC_CEDS2:
		*u32modulation_limit = SSC_MODULATION_LIMIT_CEDS2;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_CEDS2;
		break;
	case E_LINK_SSC_CMPI20:
		*u32modulation_limit = SSC_MODULATION_LIMIT_CMPI20;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_CMPI20;
		break;
	case E_LINK_SSC_USIT16:
		*u32modulation_limit = SSC_MODULATION_LIMIT_USIT16;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_USIT16;
		break;
	case E_LINK_SSC_USIT31:
		*u32modulation_limit = SSC_MODULATION_LIMIT_USIT31;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_USIT31;
		break;
	case E_LINK_SSC_CSPI18:
		*u32modulation_limit = SSC_MODULATION_LIMIT_CSPI18;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_CSPI18;
		break;
	case E_LINK_SSC_ISP18:
		*u32modulation_limit = SSC_MODULATION_LIMIT_ISP18;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_ISP18;
		break;
	case E_LINK_SSC_PDI:
		*u32modulation_limit = SSC_MODULATION_LIMIT_PDI;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_PDI;
		break;
	case E_LINK_SSC_CHPI:
		*u32modulation_limit = SSC_MODULATION_LIMIT_CHPI;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_CHPI;
		break;
	case E_LINK_SSC_MINILVDS:
		*u32modulation_limit = SSC_MODULATION_LIMIT_MINILVDS;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_MINILVDS;
		break;
	default:
		*u32modulation_limit = SSC_MODULATION_LIMIT_VBY1;
		*u32deviation_limit = SSC_DEVIATION_LIMIT_VBY1;
		break;
	}

	return 0;
}

// Input parameter:
// modulation: 0.1Khz
// deviation: 0.01%
int mtk_render_set_ssc(struct mtk_panel_context *pctx_pnl)
{
	st_out_ssc_ctrl stSSC = {0};
	int nRet;
	enum drm_pnl_ssc_link_if_sel essc_linkif = E_LINK_SSC_NONE;
	uint32_t u32modulation_limit = 0;
	uint32_t u32deviation_limit = 0;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d]: pctx is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	if (pctx_pnl->stdrmSSC.ssc_protect_by_user_en) {
		u32modulation_limit = pctx_pnl->stdrmSSC.ssc_modulation_protect_value;
		u32deviation_limit = pctx_pnl->stdrmSSC.ssc_deviation_protect_value;
	} else {
		_mtk_render_get_ssc_linkif(pctx_pnl->v_cfg.linkIF, &essc_linkif);
		_mtk_render_get_ssc_limit(
			essc_linkif, &u32modulation_limit, &u32deviation_limit);
	}

	// modulation: DTS:1Khz, render DD: 0.1Khz
	u32modulation_limit *= 10;

	if ((pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation > u32modulation_limit) ||
		(pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation > u32deviation_limit)) {
		DRM_ERROR("[%s, %d]: stdrmSSC out of limit [Mod:%d/Max:%d, Dev:%d/Max:%d].\n",
			__func__, __LINE__,
			pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation, u32modulation_limit,
			pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation, u32deviation_limit);
		return -EINVAL;
	}

	stSSC.ssc_en = pctx_pnl->stdrmSSC.ssc_setting.ssc_en;
	stSSC.ssc_modulation = pctx_pnl->stdrmSSC.ssc_setting.ssc_modulation;
	stSSC.ssc_deviation = pctx_pnl->stdrmSSC.ssc_setting.ssc_deviation;
	stSSC.ssc_tcon_ctrl_en = 0;

	nRet = drv_hwreg_render_pnl_set_ssc(&stSSC);

	return nRet;
}

int mtk_render_get_ssc(struct drm_st_out_ssc *stdrmSSC)
{
	st_out_ssc_ctrl stSSC = {0};

	if (!stdrmSSC) {
		DRM_ERROR("[%s, %d]: stdrmSSC is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_ssc(&stSSC);

	stdrmSSC->ssc_en = stSSC.ssc_en;
	stdrmSSC->ssc_modulation = stSSC.ssc_modulation;
	stdrmSSC->ssc_deviation = stSSC.ssc_deviation;

	return 0;
}

int mtk_render_get_Vtt_info(struct drm_st_vtt_info *stdrmVTTInfo)
{
	st_Vtt_status stVTTInfo = {0};

	if (!stdrmVTTInfo) {
		DRM_ERROR("[%s, %d]: stdrmVTTInfo is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_Vttv_status(&stVTTInfo);

	stdrmVTTInfo->lock = stVTTInfo.lock_flag;
	stdrmVTTInfo->lock_en = stVTTInfo.lock_en;
	stdrmVTTInfo->input_vtt = stVTTInfo.Vtt_input;
	stdrmVTTInfo->output_vtt = stVTTInfo.Vcnt;

	return 0;
}

int mtk_render_get_mod_status(struct drm_st_mod_status *stdrmMod)
{
	st_drv_pnl_mod_status stdrvMod = {0};

	if (!stdrmMod) {
		DRM_ERROR("[%s, %d]: stdrmMod is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_mod_status(&stdrvMod);

	stdrmMod->vby1_locked = stdrvMod.vby1_locked;
	stdrmMod->vby1_unlockcnt = stdrvMod.vby1_unlockcnt;
	stdrmMod->vby1_htpdn = stdrvMod.vby1_htpdn;
	stdrmMod->vby1_lockn = stdrvMod.vby1_lockn;
	stdrmMod->vby1_htpdn = stdrvMod.vby1_htpdn;

	stdrmMod->outconfig.outconfig_ch0_ch7 = stdrvMod.outconfig.outconfig_ch0_ch7;
	stdrmMod->outconfig.outconfig_ch8_ch15 = stdrvMod.outconfig.outconfig_ch8_ch15;
	stdrmMod->outconfig.outconfig_ch16_ch19 = stdrvMod.outconfig.outconfig_ch16_ch19;

	stdrmMod->freeswap.freeswap_ch0_ch1 = stdrvMod.freeswap.freeswap_ch0_ch1;
	stdrmMod->freeswap.freeswap_ch2_ch3 = stdrvMod.freeswap.freeswap_ch2_ch3;
	stdrmMod->freeswap.freeswap_ch4_ch5 = stdrvMod.freeswap.freeswap_ch4_ch5;
	stdrmMod->freeswap.freeswap_ch6_ch7 = stdrvMod.freeswap.freeswap_ch6_ch7;
	stdrmMod->freeswap.freeswap_ch8_ch9 = stdrvMod.freeswap.freeswap_ch8_ch9;
	stdrmMod->freeswap.freeswap_ch10_ch11 = stdrvMod.freeswap.freeswap_ch10_ch11;
	stdrmMod->freeswap.freeswap_ch12_ch13 = stdrvMod.freeswap.freeswap_ch12_ch13;
	stdrmMod->freeswap.freeswap_ch14_ch15 = stdrvMod.freeswap.freeswap_ch14_ch15;
	stdrmMod->freeswap.freeswap_ch16_ch17 = stdrvMod.freeswap.freeswap_ch16_ch17;
	stdrmMod->freeswap.freeswap_ch18_ch19 = stdrvMod.freeswap.freeswap_ch18_ch19;
	stdrmMod->freeswap.freeswap_ch20_ch21 = stdrvMod.freeswap.freeswap_ch20_ch21;
	stdrmMod->freeswap.freeswap_ch22_ch23 = stdrvMod.freeswap.freeswap_ch22_ch23;
	stdrmMod->freeswap.freeswap_ch24_ch25 = stdrvMod.freeswap.freeswap_ch24_ch25;
	stdrmMod->freeswap.freeswap_ch26_ch27 = stdrvMod.freeswap.freeswap_ch26_ch27;
	stdrmMod->freeswap.freeswap_ch28_ch29 = stdrvMod.freeswap.freeswap_ch28_ch29;
	stdrmMod->freeswap.freeswap_ch30_ch31 = stdrvMod.freeswap.freeswap_ch30_ch31;

	return 0;
}

int mtk_render_set_panel_dither(struct drm_st_dither_info *stdrmDITHERInfo)
{
	st_pnl_dither stDITHER = {0};

	if (!stdrmDITHERInfo) {
		DRM_ERROR("[%s, %d]: stdrmDITHERInfo is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	stDITHER.dither_pattern = stdrmDITHERInfo->dither_pattern;
	stDITHER.dither_depth = stdrmDITHERInfo->dither_depth;
	stDITHER.dither_capability = stdrmDITHERInfo->dither_capability;

	drv_hwreg_render_pnl_set_dither(&stDITHER);

	return 0;
}

int mtk_render_get_panel_dither_reg(struct drm_st_dither_reg *stdrmDITHERReg)
{
	st_dither_reg stDITHERReg = {0};

	if (!stdrmDITHERReg) {
		DRM_ERROR("[%s, %d]: stdrmDITHERReg is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drv_hwreg_render_pnl_get_panel_dither_reg(&stDITHERReg);

	stdrmDITHERReg->dither_frc_on = stDITHERReg.dither_frc_on;
	stdrmDITHERReg->dither_noise_disable = stDITHERReg.dither_noise_disable;
	stdrmDITHERReg->dither_tail_cut = stDITHERReg.dither_tail_cut;
	stdrmDITHERReg->dither_box_freeze = stDITHERReg.dither_box_freeze;
	stdrmDITHERReg->dither_dith_bit = stDITHERReg.dither_dith_bit;
	stdrmDITHERReg->dither_12bit_bypass = stDITHERReg.dither_12bit_bypass;
	stdrmDITHERReg->dither_is_12bit = stDITHERReg.dither_is_12bit;

	return 0;
}

static int mtk_tv_drm_panel_disable(struct drm_panel *panel)
{
	//If needed, control backlight in panel_enable
	struct mtk_panel_context *pStPanelCtx = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);
	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	//control backlight in panel enable/disable depend on pm mode/ use case.
	if (pStPanelCtx->cus.vcc_bl_cusctrl == 0)
		gpiod_set_value(pStPanelCtx->gpio_backlight, 0);
	return 0;
}

static int mtk_tv_drm_panel_unprepare(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);
	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	if (pStPanelCtx->cus.vcc_bl_cusctrl == 0) {
		if (gpiod_get_value(pStPanelCtx->gpio_vcc) != 0)
			gpiod_set_value(pStPanelCtx->gpio_vcc, 0);
	} else {
		DRM_INFO("[%s]Panel VCC is already disabled\n", __func__);
	}
	return 0;
}

int mtk_tv_drm_panel_checkDTSPara(drm_st_pnl_info *pStPnlInfo)
{
	if (!pStPnlInfo) {
		DRM_ERROR("[%s, %d]: pStPnlInfo is NULL.\n",
			__func__, __LINE__);

		return -ENODEV;
	}

	// HTT < Hde_start + Hde
	if (pStPnlInfo->typ_htt <= pStPnlInfo->de_hstart + pStPnlInfo->de_width) {
		DRM_ERROR("[%s, %d]: HTT < Hde_start + Hde\n",
			__func__, __LINE__);
		return -EPERM;
	}

	// Hde_start < Hsync_s + Hsync_w
	if (pStPnlInfo->de_hstart < pStPnlInfo->hsync_st + pStPnlInfo->hsync_w) {
		DRM_ERROR("[%s, %d]: Hde_start < Hsync_s + Hsync_w\n",
			__func__, __LINE__);
		return -EPERM;
	}

    // VTT < Vde_start + Vde
	if (pStPnlInfo->typ_vtt <= pStPnlInfo->de_vstart + pStPnlInfo->de_height) {
		DRM_ERROR("[%s, %d]:  VTT < Vde_start + Vde\n",
			__func__, __LINE__);
		return -EPERM;
	}

	// Vde_start < Vsync_s + Vsync_w
	if (pStPnlInfo->de_vstart < pStPnlInfo->vsync_st + pStPnlInfo->vsync_w) {
		DRM_ERROR("[%s, %d]: Vde_start < Vsync_s + Vsync_w\n",
			__func__, __LINE__);
		return -EPERM;
	}

	// VTT_max < Vtt_typ
	if (pStPnlInfo->max_vtt < pStPnlInfo->typ_vtt) {
		DRM_ERROR("[%s, %d]: VTT_max < Vtt_typ\n",
			__func__, __LINE__);
		return -EPERM;
	}

	// VTT_min > Vtt_typ
	if (pStPnlInfo->min_vtt > pStPnlInfo->typ_vtt) {
		DRM_ERROR("[%s, %d]: VTT_min > Vtt_typ\n",
			__func__, __LINE__);
		return -EPERM;
	}

    // VTT_min < Vde + Vde_start
	if (pStPnlInfo->min_vtt < pStPnlInfo->de_vstart
		+ pStPnlInfo->de_height) {
		DRM_ERROR("[%s, %d]: VTT_min < Vde + Vde_start\n",
			__func__, __LINE__);
		return -EPERM;
	}

	return 0;
}

static int mtk_tv_drm_panel_get_fix_ModeID(struct drm_panel *panel)
{
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;
	struct mtk_tv_kms_context *pctx = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drm_con = panel->connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;

	return pctx->panel_priv.disable_ModeID_change;
}
static inline int mtk_tv_drm_panel_update_kms_pnl_output_timming_info(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_device *kms_drm_dev = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);

	mtk_tv_drm_panel_checkDTSPara(&pStPanelCtx->info);

	kms_drm_dev = panel->drm;

	drm_con = panel->connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;
	pctx->panel_priv.outputTiming_info.u32OutputVfreq
		= pStPanelCtx->outputTiming_info.u32OutputVfreq;

	return 0;
}

static int mtk_tv_drm_panel_update_kms_pnl_info(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_device *kms_drm_dev = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);

	mtk_tv_drm_panel_checkDTSPara(&pStPanelCtx->info);

	kms_drm_dev = panel->drm;

	drm_con = panel->connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;
	memcpy(&pctx->panel_priv.info, &pStPanelCtx->info, sizeof(drm_st_pnl_info));
	return 0;
}

static void _send_pnl_info_pqu(struct mtk_panel_context *pStPanelCtx)
{
#define TGEN_2P (2)
	struct msg_render_panel_info msg_info = { };
	mtktv_chip_series series;
	uint8_t out_clk_if = 0;
	uint8_t alignNum;

	if (!pStPanelCtx)
		return;

	/* only merak2.0 needed */
	series = get_mtk_chip_series(pStPanelCtx->hw_info.pnl_lib_version);
	if (series == MTK_TV_SERIES_MERAK_1)
		return;

	if (pStPanelCtx->hw_info.pnl_lib_version == DRV_PNL_VERSION0400) {
		msg_info.de_hstart = pStPanelCtx->info.de_hstart;
		msg_info.de_width = pStPanelCtx->info.de_width;
		msg_info.np_align = 1;
	} else if (pStPanelCtx->hw_info.pnl_lib_version == DRV_PNL_VERSION0500) {
		/* only miffy need to 2p aligned */
		out_clk_if = _mtk_get_out_clk_if(pStPanelCtx);
		alignNum = max_t(uint8_t, out_clk_if, TGEN_2P);
		msg_info.de_hstart = rounddown(pStPanelCtx->info.de_hstart, alignNum);
		msg_info.de_width = rounddown(pStPanelCtx->info.de_width, alignNum);
		msg_info.np_align = TGEN_2P;

		DRM_INFO("[%s:%d] de_hstart=%d -> %d de_width=%d -> %d\n",
			__func__, __LINE__,
			pStPanelCtx->info.de_hstart, msg_info.de_hstart,
			pStPanelCtx->info.de_width, msg_info.de_width);
	} else if (pStPanelCtx->hw_info.pnl_lib_version == DRV_PNL_VERSION0600) {
		msg_info.de_hstart = pStPanelCtx->info.de_hstart;
		msg_info.de_width = pStPanelCtx->info.de_width;
		msg_info.np_align = 1;
	} else {
		return;
	}

	msg_info.de_vstart = (uint16_t)pStPanelCtx->info.de_vstart;
	msg_info.de_height = (uint16_t)pStPanelCtx->info.de_height;

	pqu_msg_send(PQU_MSG_SEND_RENDER_PANEL_INFO, &msg_info);
}

static int mtk_tv_drm_panel_prepare(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct drm_crtc_state *drmCrtcState = NULL;
	int ret = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	panel_common_enable();

	drmCrtcState = panel->connector->state->crtc->state;

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);
	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	drm_con = pStPanelCtx->drmPanel.connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;

	//game direct tcon i2c cmd init
	if ((pStPanelCtx->cus.game_direct_framerate_mode == TRUE)
		&& (u16GameDirectFrGroup == HEX_FF)
		&& (!pStPanelCtx->cus.tcon_en)) {
		_mtk_game_direct_send_tcon_i2c_cmd(pStPanelCtx);
		u16GameDirectFrGroup = pStPanelCtx->info.game_direct_fr_group;
	}

	//dlg tcon i2c cmd init
	if ((pStPanelCtx->cus.dlg_en == true)
		&& (pStPanelCtx->cus.dlg_i2c_CustCtrl == true)
		&& (u16DLG_On == HEX_FF)
		&& (!pStPanelCtx->cus.tcon_en)) {
		u16DLG_On = pStPanelCtx->info.dlg_on;
	}
	if ((pStPanelCtx->cus.dlg_en == true) &&
		(pStPanelCtx->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet) &&
		(u16DLG_On == HEX_FF)) {
		u16DLG_On = pStPanelCtx->info.dlg_on;
	}

	if (drmCrtcState->mode_changed
		&& !(_isSameModeAndPanelInfo(&drmCrtcState->mode, &pStPanelCtx->info))) {

		ret = _copyPanelInfo(drmCrtcState, &pStPanelCtx->info, &pStPanelCtx->panelInfoList);

		if (ret) {
			DRM_ERROR("[%s, %d]: new panel mode does not set.\n",
				__func__, __LINE__);
		} else {
			mtk_tv_drm_panel_update_kms_pnl_info(panel);

			if (mtk_tv_drm_panel_get_fix_ModeID(panel)) {
				DRM_ERROR("[%s, %d]: fix_ModeID.\n",
				__func__, __LINE__);
				return 0;
				}

			if ((pStPanelCtx->info.typ_dclk != u64PreTypDclk) ||
				(pStPanelCtx->info.op_format != ePreOutputFormat) ||
				(pStPanelCtx->info.vbo_byte != ePreByteMode))
				pStPanelCtx->out_upd_type = E_MODE_RESET_TYPE;
			else
				pStPanelCtx->out_upd_type = E_MODE_CHANGE_TYPE;

			mtk_panel_init(panel->dev);
			/* restore the output_timming_info to kms for trigger gen reset calc */
			mtk_tv_drm_panel_update_kms_pnl_output_timming_info(panel);
			pStPanelCtx->out_upd_type = E_NO_UPDATE_TYPE;
			/*
			 * To support DLG on/off case w/o video, should re-calc trigger gen
			 * render doman part with different output timming/fps
			 * and move to next vsync to set
			 */
			static_branch_enable(&tg_need_update);
		}
	}

	/* send panel info to puq for setting display window by DS */
	_send_pnl_info_pqu(pStPanelCtx);

	return 0;
}

static int mtk_tv_drm_panel_enable(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;

	if (!panel) {
		DRM_ERROR("[%s, %d]: panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);
	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n", __func__, __LINE__);
		return -ENODEV;
	}

	//control backlight in panel enable/disable depend on pm mode/ use case.
	if (pStPanelCtx->cus.vcc_bl_cusctrl == 0)
		gpiod_set_value(pStPanelCtx->gpio_backlight, 1);
	return 0;
}

static int mtk_tv_drm_panel_get_modes(struct drm_panel *panel)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	int modeCouct = 0;

	if (!panel) {
		DRM_ERROR("[%s, %d]: drm_panel is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);

	_panelAddModes(panel,
		&pStPanelCtx->panelInfoList,
		&modeCouct);

	pStPanelCtx->u8TimingsNum = modeCouct;

	return pStPanelCtx->u8TimingsNum;
}

static int mtk_tv_drm_panel_get_timings(struct drm_panel *panel,
							unsigned int num_timings,
							struct display_timing *timings)
{
	uint32_t u32NumTiming = 1;
	struct mtk_panel_context *pStPanelCtx = NULL;

	pStPanelCtx = container_of(panel, struct mtk_panel_context, drmPanel);

	if (!timings) {
		DRM_WARN("\033[1;31m [%s][%d]pointer is NULL \033[0m\n",
							__func__, __LINE__);
		return u32NumTiming;
	}
	timings[0] = pStPanelCtx->info.displayTiming;
	return u32NumTiming;
}

void mtk_tv_drm_panel_set_VTT_debugmode(uint8_t u8DebugSelVal)
{
	drv_hwreg_render_pnl_set_VTT_debugmode(u8DebugSelVal);
}

uint16_t mtk_tv_drm_panel_get_VTT_debugValue(void)
{
	return drv_hwreg_render_pnl_get_VTT_debugValue();
}
int mtk_tv_drm_get_fps_value_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_mtk_range_value *fpsinfo = (struct drm_mtk_range_value *)data;
	st_fps_value stFPS = {0};

	if (!drm_dev || !data || !file_priv)
		return -EINVAL;

	drv_hwreg_render_pnl_get_fps(&stFPS);

	fpsinfo->value = stFPS.value;
	fpsinfo->max_value = stFPS.max_value;
	fpsinfo->min_value = stFPS.min_value;
	fpsinfo->valid = stFPS.valid;
	return 0;
}
uint16_t mtk_tv_drm_panel_get_TgenMainVTT(void)
{
	return drv_hwreg_render_pnl_get_TgenMainVTT();
}

int mtk_tv_drm_video_get_outputinfo_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_mtk_tv_dip_capture_info *outputinfo = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_cfd_set_CSC *mi_render_csc_tconin = NULL;
	struct mtk_cfd_set_CSC *mi_render_csc_mainout = NULL;
	struct mtk_cfd_set_CSC *mi_render_csc_deltaout = NULL;
	struct window dispWindow;

	memset(&dispWindow, 0, sizeof(struct window));
	struct mtk_tv_color_info mi_render_color_info = {0};

	if (!drm_dev || !data || !file_priv)
		return -EINVAL;

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	outputinfo = (struct drm_mtk_tv_dip_capture_info *)data;
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_video = &pctx->video_priv;
	pctx_pnl = &pctx->panel_priv;

	outputinfo->video.p_engine = pctx->hw_caps.render_p_engine;
	outputinfo->video.color_fmt = (enum drm_en_pnl_output_format)pctx_pnl->v_cfg.format;
	outputinfo->video_osdb.p_engine = pctx->hw_caps.render_p_engine;
	outputinfo->video_osdb.color_fmt = (enum drm_en_pnl_output_format)pctx_pnl->v_cfg.format;
	outputinfo->video_post.p_engine = pctx->hw_caps.render_p_engine;
	outputinfo->video_post.color_fmt = (enum drm_en_pnl_output_format)pctx_pnl->v_cfg.format;

	if (pctx->chip_series == CHIP_SERIES_MAIN_TS) {
		outputinfo->video.width = pctx_pnl->info.de_width;
		outputinfo->video.height = pctx_pnl->info.de_height;
	} else { /* 2.0 series*/
		drv_hwreg_render_display_get_dispWindow(0, &dispWindow);

		outputinfo->video.width = dispWindow.w;
		outputinfo->video.height = dispWindow.h;
	}


	mi_render_csc_tconin = mtk_video_display_cfd_get_tcon_in_csc_setting();
	mi_render_csc_mainout = mtk_video_display_cfd_get_rendor_main_out_csc_setting();
	mi_render_csc_deltaout = mtk_video_display_cfd_get_render_delta_out_csc_setting();

	memset(&mi_render_color_info, 0, sizeof(struct mtk_tv_color_info));
	mi_render_color_info = color_metry_map[mi_render_csc_tconin->u8ColorSpace];

	outputinfo->video.data_format = mi_render_csc_tconin->u8Data_Format;
	outputinfo->video.data_range = (enum drm_video_data_range)mi_render_csc_tconin->u8Range;
	outputinfo->video.color_primary = mi_render_color_info.color_primaries;
	outputinfo->video.matrix_coefficient = mi_render_color_info.matrix_coefficients;
	outputinfo->video.transfer_characteristics = mi_render_color_info.transfer_characteristics;

	mi_render_color_info = color_metry_map[mi_render_csc_mainout->u8ColorSpace];

	outputinfo->video_post.data_format = mi_render_csc_mainout->u8Data_Format;
	outputinfo->video_post.data_range =
		(enum drm_video_data_range)mi_render_csc_mainout->u8Range;
	outputinfo->video_post.color_primary = mi_render_color_info.color_primaries;
	outputinfo->video_post.matrix_coefficient = mi_render_color_info.matrix_coefficients;
	outputinfo->video_post.transfer_characteristics =
		mi_render_color_info.transfer_characteristics;

	memset(&mi_render_color_info, 0, sizeof(struct mtk_tv_color_info));
	mi_render_color_info = color_metry_map[mi_render_csc_deltaout->u8ColorSpace];

	outputinfo->video_osdb.data_format = mi_render_csc_deltaout->u8Data_Format;
	outputinfo->video_osdb.data_range =
		(enum drm_video_data_range)mi_render_csc_deltaout->u8Range;
	outputinfo->video_osdb.color_primary = mi_render_color_info.color_primaries;
	outputinfo->video_osdb.matrix_coefficient = mi_render_color_info.matrix_coefficients;
	outputinfo->video_osdb.transfer_characteristics =
		mi_render_color_info.transfer_characteristics;


	DRM_INFO("[DIP INFO][P9] p_engine = %d color_fmt:%d data_format:%d data_range:%d\n",
		outputinfo->video.p_engine, outputinfo->video.color_fmt,
		outputinfo->video.data_format, outputinfo->video.data_range);
	DRM_INFO("[DIP INFO][P9] color_primary = %d matrix_coefficient:%d transfer_characteristics:%d\n",
		outputinfo->video.color_primary, outputinfo->video.matrix_coefficient,
		outputinfo->video.transfer_characteristics);

	DRM_INFO("[DIP INFO][P9] width = %d height:%d\n",
		outputinfo->video.width, outputinfo->video.height);

	DRM_INFO("[DIP INFO][P10] p_engine = %d color_fmt:%d data_format:%d data_range:%d\n",
		outputinfo->video_osdb.p_engine, outputinfo->video_osdb.color_fmt,
		outputinfo->video_osdb.data_format, outputinfo->video_osdb.data_range);
	DRM_INFO("[DIP INFO][P10] color_primary = %d matrix_coefficient:%d transfer_characteristics:%d\n",
		outputinfo->video_osdb.color_primary, outputinfo->video_osdb.matrix_coefficient,
		outputinfo->video_osdb.transfer_characteristics);

	DRM_INFO("[DIP INFO][P11] p_engine = %d color_fmt:%d data_format:%d data_range:%d\n",
		outputinfo->video_post.p_engine, outputinfo->video_post.color_fmt,
		outputinfo->video_post.data_format, outputinfo->video_post.data_range);
	DRM_INFO("[DIP INFO][P11] color_primary = %d matrix_coefficient:%d transfer_characteristics:%d\n",
		outputinfo->video_post.color_primary, outputinfo->video_post.matrix_coefficient,
		outputinfo->video_post.transfer_characteristics);

	return 0;
}

int mtk_tv_drm_set_oled_offrs_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	bool bEnable = false;
	int ret = 0;

	if (!drm_dev || !data)
		return -EINVAL;

	DRM_INFO("Try to set OLED OFF-RS = %d\n", bEnable);

	bEnable = *((bool *)data);

	// Get OLED structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	// Set OFF-RS
	ret = mtk_oled_set_offrs(&pctx->panel_priv, bEnable);
	if (ret)
		DRM_INFO("Fail to set OLED OFF-RS = %d\n", bEnable);

	return ret;
}

int mtk_tv_drm_set_oled_jb_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	bool bEnable = false;
	int ret = 0;

	if (!drm_dev || !data)
		return -EINVAL;

	bEnable = *((bool *)data);

	// Get OLED structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	// Set JB
	ret = mtk_oled_set_jb(&pctx->panel_priv, bEnable);
	if (ret)
		DRM_INFO("Fail to set OLED JB = %d\n", bEnable);

	return ret;
}

static void _ready_cb_pqu_dd_update_pq_cfd_sharemem_param(void)
{
	int ret = 0;

	DRM_INFO("pqu cpuif ready callback function\n");

	ret = pqu_render_dd_pq_cfd_update_sharemem(
		&_cfd_sharemem_msg_info[_CB_DEC(_cfd_sharemem_count)], NULL);
	if (ret != 0)
		DRM_ERROR("pqu_render_dd_pq_cfd_update_sharemem fail!\n");
}

static int mtk_tv_drm_send_vdo_pnl_sharememtopqu(struct mtk_panel_context *pctx_pnl)
{
	int ret = 0;
	struct mtk_tv_drm_metabuf metabuf;
	struct msg_render_pq_sharemem_info pq_info;
	struct pqu_dd_update_pq_cfd_sharemem_param shareinfo;
	struct meta_render_pqu_cfd_context *pqu_panel_info_data = NULL;

	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_VDO_PNL)) {
		DRM_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_VDO_PNL);
		return -1;
	}

	pqu_panel_info_data = metabuf.addr;

	pqu_panel_info_data->pnl_color_info_bx = pctx_pnl->color_info.bx;
	pqu_panel_info_data->pnl_color_info_by = pctx_pnl->color_info.by;
	pqu_panel_info_data->pnl_color_info_format = pctx_pnl->color_info.format;
	pqu_panel_info_data->pnl_color_info_gx = pctx_pnl->color_info.gx;
	pqu_panel_info_data->pnl_color_info_gy = pctx_pnl->color_info.gy;
	pqu_panel_info_data->pnl_color_info_hdrnits = pctx_pnl->color_info.hdrnits;
	pqu_panel_info_data->pnl_color_info_length = pctx_pnl->color_info.length;
	pqu_panel_info_data->pnl_color_info_linear_rgb = pctx_pnl->color_info.linear_rgb;
	pqu_panel_info_data->pnl_color_info_maxlum = pctx_pnl->color_info.maxlum;
	pqu_panel_info_data->pnl_color_info_medlum = pctx_pnl->color_info.medlum;
	pqu_panel_info_data->pnl_color_info_minlum = pctx_pnl->color_info.minlum;
	pqu_panel_info_data->pnl_color_info_rx = pctx_pnl->color_info.rx;
	pqu_panel_info_data->pnl_color_info_ry = pctx_pnl->color_info.ry;
	pqu_panel_info_data->pnl_color_info_version = pctx_pnl->color_info.version;
	pqu_panel_info_data->pnl_color_info_wx = pctx_pnl->color_info.wx;
	pqu_panel_info_data->pnl_color_info_wy = pctx_pnl->color_info.wy;

	pq_info.address = metabuf.mmap_info.phy_addr;
	pq_info.size = sizeof(struct meta_render_pqu_cfd_context);

	shareinfo.address  = metabuf.mmap_info.phy_addr;
	shareinfo.size = sizeof(struct meta_render_pqu_cfd_context);

	if (bPquEnable) {
		ret = pqu_render_dd_pq_cfd_update_sharemem(
		(const struct pqu_dd_update_pq_cfd_sharemem_param *)&shareinfo, NULL);
		if (ret == -ENODEV) {
			memcpy(&_cfd_sharemem_msg_info[_CB_INC(_cfd_sharemem_count)],
				&shareinfo,
				sizeof(struct pqu_dd_update_pq_cfd_sharemem_param));
			DRM_ERROR("pqu_render_dd_pq_cfd_update_sharemem register ready cb\n");
			ret = fw_ext_command_register_notify_ready_callback(0,
				_ready_cb_pqu_dd_update_pq_cfd_sharemem_param);
		} else if (ret != 0) {
			DRM_ERROR("pqu_render_dd_pq_cfd_update_sharemem fail (ret=%d)\n", ret);
		}
	} else
		pqu_msg_send(PQU_MSG_SEND_CFD_PQSHAREMEM_INFO, &pq_info);

	ret = mtk_tv_drm_metabuf_free(&metabuf);
	return ret;
}

int mtk_tv_drm_set_trigger_gen_debug(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	static bool s_bLegacyMode;
	static bool s_bIsLowLatency;
	static bool s_bIsSCMIFBL;
	static bool s_bIsFrcMdgwFBL;
	static bool s_debugEn;
	unsigned long flags;
	bool is_low_latency = false;
	bool is_frcmdgw_fbl = false;
	enum EN_TRIGGER_DEBUG_MODE eTriggerDebugModeSel
		= E_TRIGGER_DEBUG_OFF;
	mtktv_chip_series series;

	if (!pctx)
		return -ENODEV;

	pctx_pnl = &pctx->panel_priv;
	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	DRM_INFO(" [%s, %d] 1 s_debugEn=%x\n", __func__, __LINE__, s_debugEn);

	eTriggerDebugModeSel =
		(enum EN_TRIGGER_DEBUG_MODE)pctx_pnl->gu8Triggergendebugsetting;

	spin_lock_irqsave(&pctx->tg_lock, flags);

	switch (eTriggerDebugModeSel) {
	case E_TRIGGER_DEBUG_OFF: /*DRBUG = OFF*/
		if (s_debugEn == true) {
			pctx->trigger_gen_info.IsLowLatency = s_bLegacyMode;
			pctx->trigger_gen_info.LegacyMode = s_bIsLowLatency;
			pctx->trigger_gen_info.IsSCMIFBL = s_bIsSCMIFBL;
			pctx->trigger_gen_info.IsFrcMdgwFBL = s_bIsFrcMdgwFBL;
			s_debugEn = false;
		}
		break;
	case E_TRIGGER_DEBUG_LEGACY: /*Trigger gen -> Legacy mode on*/
		if (s_debugEn == false) {
			s_bLegacyMode = pctx->trigger_gen_info.IsLowLatency;
			s_bIsLowLatency = pctx->trigger_gen_info.LegacyMode;
			s_bIsSCMIFBL = pctx->trigger_gen_info.IsSCMIFBL;
			s_bIsFrcMdgwFBL = pctx->trigger_gen_info.IsFrcMdgwFBL;
			s_debugEn = true;
		}
		// Trigger gen -> Legacy mode on
		if (series == MTK_TV_SERIES_MERAK_2) {
			pctx->trigger_gen_info.LegacyMode = true;
		} else {
			pctx->trigger_gen_info.IsSCMIFBL = true;
			pctx->trigger_gen_info.IsFrcMdgwFBL = false;
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = true;
		}
		break;
	case E_TRIGGER_DEBUG_TS: /*Trigger gen -> Legacy mode off*/
		/* merak 2.0 chips should never go here */
		if (s_debugEn == false) {
			s_bLegacyMode = pctx->trigger_gen_info.IsLowLatency;
			s_bIsLowLatency = pctx->trigger_gen_info.LegacyMode;
			s_bIsSCMIFBL = pctx->trigger_gen_info.IsSCMIFBL;
			s_bIsFrcMdgwFBL = pctx->trigger_gen_info.IsFrcMdgwFBL;
			s_debugEn = true;
		}
		pctx->trigger_gen_info.IsSCMIFBL = false;
		pctx->trigger_gen_info.IsFrcMdgwFBL = false;
		pctx->trigger_gen_info.LegacyMode = false;
		pctx->trigger_gen_info.IsLowLatency = false;
		break;
	default:
		break;
	}

	is_low_latency = pctx->trigger_gen_info.IsLowLatency;
	is_frcmdgw_fbl = pctx->trigger_gen_info.IsFrcMdgwFBL;
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	// donot bypass trigger gen setting
	pctx_pnl->outputTiming_info.FrameSyncModeChgBypass = false;
	mtk_trigger_gen_set_bypass(false);

	DRM_INFO(" [%s, %d] 2 s_debugEn=%x\n", __func__, __LINE__, s_debugEn);
	DRM_INFO(" [%s, %d] s_bLegacyMode=%x\n", __func__, __LINE__, s_bLegacyMode);
	DRM_INFO(" [%s, %d] s_bIsLowLatency=%x\n", __func__, __LINE__, s_bIsLowLatency);
	DRM_INFO(" [%s, %d] s_bIsSCMIFBL=%x\n", __func__, __LINE__, s_bIsSCMIFBL);
	DRM_INFO(" [%s, %d] s_bIsFrcMdgwFBL=%x\n", __func__, __LINE__, s_bIsFrcMdgwFBL);

	mtk_trigger_gen_setup(pctx, true, true);
	_mtk_video_set_IvsOvsSourceSel_phasediff(pctx, true, is_low_latency, is_frcmdgw_fbl,
		mtk_video_panel_get_FrameLockPhaseDiff());

	return 0;
}

static void _mtk_video_panel_turn_off_chg_bypass(
	struct mtk_tv_kms_context *pctx)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	uint16_t output_vtt = 0;
	uint16_t RegCount = 0;
	bool bRIU = true;
	struct reg_info reg[HWREG_VIDEO_REG_NUM_PANEL];
	struct hwregOut paramOut;

	memset(&reg, 0, sizeof(reg));
	memset(&paramOut, 0, sizeof(struct hwregOut));
	paramOut.reg = reg;

	pctx_pnl = &pctx->panel_priv;

	//1. turn off FLock
	//BKA3A1_24[13] = 1
	drv_hwreg_render_pnl_set_vttLockLastVttLatchToggle(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//BKA3A0_22[0] = 0
	drv_hwreg_render_pnl_set_framelockVcntEnableDb(
		&paramOut,
		bRIU,
		false);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//2. turn on vtt recovery
	//h33[10] = 1
	//h30[15] = 1
	//h30[14:0] = m_delta
	drv_hwreg_render_pnl_set_vttLockDiffLimit(
		&paramOut,
		bRIU,
		true,
		pctx_pnl->cus.m_del);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	//h31[13] = 1
	drv_hwreg_render_pnl_set_vttLockUdSwModeEnable(
		&paramOut,
		bRIU,
		true);
	RegCount = RegCount + paramOut.regCount;
	paramOut.reg = reg + RegCount;

	if(pctx_pnl->outputTiming_info.u16OutputVTotal > 0)
		output_vtt = pctx_pnl->outputTiming_info.u16OutputVTotal;
	else
		output_vtt = pctx_pnl->info.typ_vtt;

	//3. set vtt
	drv_hwreg_render_pnl_set_vtt(
		&paramOut,
		bRIU,
		output_vtt);

	pctx_pnl->outputTiming_info.eFrameSyncMode = VIDEO_CRTC_FRAME_SYNC_FREERUN;
	pctx_pnl->outputTiming_info.FrameSyncModeChgBypass = FALSE;
	DRM_INFO("output_vtt = %d\n", output_vtt);
}


int mtk_tv_drm_set_trigger_gen_reset_to_default(
	struct mtk_tv_kms_context *pctx,
	bool bResetToDefaultEn,
	bool render_modify)
{
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;
	enum EN_TRIGGER_DEBUG_MODE eTriggerDebugModeSel
		= E_TRIGGER_DEBUG_OFF;
	mtktv_chip_series series;
	unsigned long flags;
	bool is_low_latency = false;
	bool is_frcmdgw_fbl = false;
	uint16_t input_vtt = 0;

	if (bResetToDefaultEn == false)
		return 0;

	if (!pctx)
		return -ENODEV;

	pctx_pnl = &pctx->panel_priv;

	eTriggerDebugModeSel =
		(enum EN_TRIGGER_DEBUG_MODE)pctx_pnl->gu8Triggergendebugsetting;

	series = get_mtk_chip_series(pctx_pnl->hw_info.pnl_lib_version);

	if(pctx_pnl->outputTiming_info.FrameSyncModeChgBypass)
		_mtk_video_panel_turn_off_chg_bypass(pctx);

////////////////only for sysfs debug////////////////////
	spin_lock_irqsave(&pctx->tg_lock, flags);

	switch (eTriggerDebugModeSel) {
	case E_TRIGGER_DEBUG_OFF: /*DRBUG = OFF*/
		if (series == MTK_TV_SERIES_MERAK_1) {
			pctx->trigger_gen_info.LegacyMode = false;
			pctx->trigger_gen_info.IsLowLatency = false;
			pctx->trigger_gen_info.Input_Vfreqx1000 = 60000;
			pctx->trigger_gen_info.eSourceType = MTK_SOURCE_TYPE_MM;
			pctx->trigger_gen_info.IsB2Rsource = true;
		} else {	// 2.0
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = false;
			pctx->trigger_gen_info.Input_Vfreqx1000 = 60000;
			pctx->trigger_gen_info.eSourceType = MTK_SOURCE_TYPE_MM;
			pctx->trigger_gen_info.IsB2Rsource = true;
			pctx->trigger_gen_info.IsFrcMdgwFBL = true;
			pctx->trigger_gen_info.IsSCMIFBL = false;
		}
		break;
	case E_TRIGGER_DEBUG_LEGACY: /*Trigger gen -> Legacy mode on*/
		if (series == MTK_TV_SERIES_MERAK_2) {
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = true;
		} else {
			pctx->trigger_gen_info.IsSCMIFBL = true;
			pctx->trigger_gen_info.IsFrcMdgwFBL = false;
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = true;
		}
		render_modify = true;
		break;
	case E_TRIGGER_DEBUG_TS: /*Trigger gen -> Legacy mode off*/
		if (series == MTK_TV_SERIES_MERAK_2) {
			pctx->trigger_gen_info.LegacyMode = false;
			pctx->trigger_gen_info.IsLowLatency = false;
		} else {
			pctx->trigger_gen_info.IsSCMIFBL = false;
			pctx->trigger_gen_info.IsFrcMdgwFBL = false;
			pctx->trigger_gen_info.LegacyMode = false;
			pctx->trigger_gen_info.IsLowLatency = false;
		}
		render_modify = true;
		break;
	default:
		break;
	}
	is_low_latency = pctx->trigger_gen_info.IsLowLatency;
	is_frcmdgw_fbl = pctx->trigger_gen_info.IsFrcMdgwFBL;
	input_vtt = pctx->trigger_gen_info.Vtotal;

	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	/* restored trigger gen setting, should disable bypass */
	mtk_trigger_gen_set_bypass(false);
////////////////only for sysfs debug////////////////////

	DRM_INFO("Try to Reset TRIGGER GEN\n");


	/* VTT protect end */
	mtk_trigger_gen_setup(pctx, true, render_modify);
	//_mtk_video_set_IvsOvsSourceSel_phasediff(pctx, false, is_low_latency, is_frcmdgw_fbl,
	//	mtk_video_panel_get_FrameLockPhaseDiff());

	return ret;
}

int mtk_tv_drm_set_trigger_gen_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	int ret = 0;
	enum EN_TRIGGER_DEBUG_MODE eTriggerDebugModeSel
		= E_TRIGGER_DEBUG_OFF;
	struct drm_mtk_tv_trigger_gen_info *trigger_gen_info =
		NULL;
	unsigned long flags;

	if (!drm_dev || !data)
		return -EINVAL;

	// Get kms structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	trigger_gen_info =
		(struct drm_mtk_tv_trigger_gen_info *)data;
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (!pctx)
		return -ENODEV;

	pctx_pnl = &pctx->panel_priv;
	pctx_pnl->outputTiming_info.FrameSyncModeChgBypass = false;
	mtk_trigger_gen_set_bypass(false);


	spin_lock_irqsave(&pctx->tg_lock, flags);
	// update trigger gen info
	pctx->trigger_gen_info.Vde_start = trigger_gen_info->Vde_start;
	pctx->trigger_gen_info.Vde = trigger_gen_info->Vde;
	pctx->trigger_gen_info.Vtotal = trigger_gen_info->Vtotal;
	pctx->trigger_gen_info.Vs_pulse_width = trigger_gen_info->Vs_pulse_width;
	pctx->trigger_gen_info.fdet_vtt0 = trigger_gen_info->fdet_vtt0;
	pctx->trigger_gen_info.fdet_vtt1 = trigger_gen_info->fdet_vtt1;
	pctx->trigger_gen_info.Input_Vfreqx1000 = trigger_gen_info->Input_Vfreqx1000;
	pctx->trigger_gen_info.IsSCMIFBL = trigger_gen_info->IsSCMIFBL;
	pctx->trigger_gen_info.IsFrcMdgwFBL = trigger_gen_info->IsFrcMdgwFBL;
	pctx->trigger_gen_info.IsPmodeEn = trigger_gen_info->IsPmodeEn;
	pctx->trigger_gen_info.LegacyMode = trigger_gen_info->LegacyMode;
	pctx->trigger_gen_info.IsLowLatency = trigger_gen_info->IsLowLatency;
	pctx->trigger_gen_info.IsForceP = trigger_gen_info->IsForceP;
	pctx->trigger_gen_info.eSourceType = trigger_gen_info->eSourceType;
	pctx->trigger_gen_info.IsB2Rsource =
		((trigger_gen_info->eSourceType) > MTK_SOURCE_TYPE_CVBS)?1:0;
	pctx->trigger_gen_info.u8Ivs = trigger_gen_info->u8Ivs;
	pctx->trigger_gen_info.u8Ovs = trigger_gen_info->u8Ovs;
	pctx->trigger_gen_info.IsVrr = trigger_gen_info->IsVrr;
	pctx->trigger_gen_info.IsGamingMJCOn = trigger_gen_info->IsGamingMJCOn;
	eTriggerDebugModeSel =
		(enum EN_TRIGGER_DEBUG_MODE)pctx_pnl->gu8Triggergendebugsetting;
////////////////only for sysfs debug////////////////////

	switch (eTriggerDebugModeSel) {
	case E_TRIGGER_DEBUG_OFF: /*DRBUG = OFF*/
		break;
	case E_TRIGGER_DEBUG_LEGACY: /*Trigger gen -> Legacy mode on*/
		if ((pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0400) ||
			(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0500) ||
			(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0600)) {
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = true;
		} else {
			pctx->trigger_gen_info.IsSCMIFBL = true;
			pctx->trigger_gen_info.IsFrcMdgwFBL = false;
			pctx->trigger_gen_info.LegacyMode = true;
			pctx->trigger_gen_info.IsLowLatency = true;
		}
		break;
	case E_TRIGGER_DEBUG_TS: /*Trigger gen -> Legacy mode off*/
		if ((pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0400) ||
			(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0500) ||
			(pctx_pnl->hw_info.pnl_lib_version == DRV_PNL_VERSION0600)) {
			pctx->trigger_gen_info.LegacyMode = false;
			pctx->trigger_gen_info.IsLowLatency = false;
		} else {
			pctx->trigger_gen_info.IsSCMIFBL = false;
			pctx->trigger_gen_info.IsFrcMdgwFBL = false;
			pctx->trigger_gen_info.LegacyMode = false;
			pctx->trigger_gen_info.IsLowLatency = false;
		}
		break;
	default:
		break;
	}
////////////////only for sysfs debug////////////////////
	spin_unlock_irqrestore(&pctx->tg_lock, flags);

	DRM_INFO("Try to set TRIGGER GEN INFO\n");
	DRM_INFO("gu8Triggergendebugsetting = %d\n",
		pctx_pnl->gu8Triggergendebugsetting);
	DRM_INFO("Vde_start = %d\n", pctx->trigger_gen_info.Vde_start);
	DRM_INFO("Vde = %d\n", pctx->trigger_gen_info.Vde);
	DRM_INFO("Vtotal = %d\n", pctx->trigger_gen_info.Vtotal);
	DRM_INFO("Vs_pulse_width = %d\n", pctx->trigger_gen_info.Vs_pulse_width);
	DRM_INFO("Input_Vfreqx1000 = %d\n", pctx->trigger_gen_info.Input_Vfreqx1000);
	DRM_INFO("fdet_vtt0 = %d\n", pctx->trigger_gen_info.fdet_vtt0);
	DRM_INFO("fdet_vtt1 = %d\n", pctx->trigger_gen_info.fdet_vtt1);
	DRM_INFO("IsSCMIFBL = %d\n", pctx->trigger_gen_info.IsSCMIFBL);
	DRM_INFO("IsFrcMdgwFBL = %d\n", pctx->trigger_gen_info.IsFrcMdgwFBL);
	DRM_INFO("IsPmodeEn = %d\n", pctx->trigger_gen_info.IsPmodeEn);
	DRM_INFO("IsLowLatency = %d\n", pctx->trigger_gen_info.IsLowLatency);
	DRM_INFO("LegacyMode = %d\n", pctx->trigger_gen_info.LegacyMode);
	DRM_INFO("Force P = %d\n", pctx->trigger_gen_info.IsForceP);
	DRM_INFO("eSourceType = %d\n", pctx->trigger_gen_info.eSourceType);
	DRM_INFO("IVS = %d\n", pctx->trigger_gen_info.u8Ivs);
	DRM_INFO("OVS = %d\n", pctx->trigger_gen_info.u8Ovs);
	DRM_INFO("Gaming MJC = %d\n", pctx->trigger_gen_info.IsGamingMJCOn);
	DRM_INFO("frame_sync_mode_Cur = %d\n",
		pctx_pnl->outputTiming_info.eFrameSyncMode);

	/* VTT protect end */
	mtk_trigger_gen_setup(pctx, true, true);
	/* setup phase diff in next vsync or ml got failed */
	static_branch_enable(&phasediff_need_update);

	mtk_tv_panel_debugvttvinfo_print(pctx, 0);

	return ret;
}

int mtk_tv_drm_get_phase_diff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	#define TGEN_XTAL_24M 24
	#define TEN_THOUSANDS 10000

	uint32_t *get_phase_diff_data = NULL;
	int ret = 0;

	if (!drm_dev || !data || !file_priv) {
		ret = -ENODEV;
		return ret;
	}

	get_phase_diff_data = (uint32_t *)data;

	//return u32(unit:us/microseconds )
	ret =  mtk_tv_drm_get_phase_diff_us(get_phase_diff_data);

	if (ret) {
		DRM_ERROR("[%s, %d]: Get phase diff IOCTL failed\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	return ret;
}

int mtk_tv_drm_get_phase_diff_us(uint32_t *get_phase_diff_val)
{
	#define TGEN_XTAL_24M 24
	#define TEN_THOUSANDS 10000

	uint64_t phase_diff = 0;
	int ret = 0;
	int64_t OutputOneLinePeriodUsx10000 = 0;
	uint32_t OutputFrameClkCount = 0;
	uint32_t OutputVtt = 0;

	if (ret) {
		DRM_ERROR("[%s, %d]: set vttv phase diff error\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	phase_diff = mtk_video_panel_get_FrameLockPhaseDiff();
	DRM_INFO("[%s, %d] phase_diff = %lld\n", __func__, __LINE__, phase_diff);
	//Get OneLinePeriodUs x 10000
	OutputVtt = drv_hwreg_render_pnl_get_Vcnt();

	if (OutputVtt == 0) {
		ret = -ENODEV;
		return ret;
	}

	OutputFrameClkCount = drv_hwreg_render_pnl_get_xtail_cnt();
	OutputOneLinePeriodUsx10000 =
		OutputFrameClkCount * TEN_THOUSANDS / TGEN_XTAL_24M / OutputVtt;
	DRM_INFO("[%s, %d] OutputOneLinePeriodUsx10000 = %lld\n",
			__func__,
			__LINE__,
			OutputOneLinePeriodUsx10000);
	//return U32(unit:us) = OutputOneLinePeriodU * _set_vttv_phase_diff / TEN_THOUSANDS
	*get_phase_diff_val = phase_diff * OutputOneLinePeriodUsx10000 / TEN_THOUSANDS;
	#undef TGEN_XTAL_24M
	#undef TEN_THOUSANDS

	return ret;
}

int mtk_tv_drm_get_ssc_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_st_out_ssc *stdrmSSC = NULL;

	if (!drm_dev || !data)
		return -EINVAL;

	stdrmSSC =
		(struct drm_st_out_ssc *)data;

	mtk_render_get_ssc(stdrmSSC);

	return ret;
}

static int mtk_tv_drm_panel_bind(
	struct device *compDev,
	struct device *master,
	void *masterData)
{
	struct mtk_tv_kms_vsync_callback_param vsync_callback_param;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!compDev) {
		DRM_ERROR("[%s, %d]: compDev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	init_completion(&bl_on_comp);

	mtk_tcon_pmic_lsic_init();
	mtk_tcon_store_isp_cmd_gamma();
	mtk_panel_init(compDev);

	// panel vsync callback
	memset(&vsync_callback_param, 0, sizeof(struct mtk_tv_kms_vsync_callback_param));
	vsync_callback_param.thread_name       = "panel_vsync_thread";
	vsync_callback_param.mi_rt_domain_name = NULL;
	vsync_callback_param.mi_rt_class_name  = NULL;
	vsync_callback_param.priv_data         = dev_get_drvdata(compDev);
	vsync_callback_param.callback_func     = mtk_video_panel_vsync_callback;
	mtk_tv_kms_register_vsync_callback(&vsync_callback_param);

	memset(&vsync_callback_param, 0, sizeof(struct mtk_tv_kms_vsync_callback_param));
	vsync_callback_param.thread_name       = "triggen vsync";
	vsync_callback_param.mi_rt_domain_name = NULL;
	vsync_callback_param.mi_rt_class_name  = NULL;
	vsync_callback_param.priv_data         = dev_get_drvdata(compDev);
	vsync_callback_param.callback_func     = mtk_trigger_gen_vsync_callback;
	mtk_tv_kms_register_vsync_callback(&vsync_callback_param);

	pctx_pnl = compDev->driver_data;
	if (!pctx_pnl) {
		pr_err("%s: missing driver data\n", __func__);
		return -ENODEV;
	}

	if (pctx_pnl->vrr_od_En)
		mtk_vrr_tcon_init(pctx_pnl, DEFAULT_OD_MODE_TYPE);

	if (pctx_pnl->vrr_pga_En)
		mtk_vrr_pnlgamma_init(pctx_pnl);

	return 0;
}

static void mtk_tv_drm_panel_unbind(
	struct device *compDev,
	struct device *master,
	void *masterData)
{

}

static int mtk_video_panel_vsync_tracker(
	struct mtk_tv_kms_context *pctx_kms,
	struct mtk_panel_context *pctx_pnl)
{
	static bool vttv_lock_en;
	static bool vttv_lock_status;
	static uint16_t vcnt_value;
	static uint16_t vcnt_count;
	static uint32_t vcnt_sum;
	uint16_t u16VdeEnd;
	uint16_t u16currentVtt;
	struct drm_st_vtt_info stdrmVTTInfo = {0};
	// __s32 ktime_diff, vsync_cycle;
	// __s64 ktime_curr;
	// static __s64 ktime_prev;

	switch (pctx_pnl->hw_info.pnl_lib_version) {
	case DRV_PNL_VERSION0200:
	case DRV_PNL_VERSION0203:
	case DRV_PNL_VERSION0400:
	case DRV_PNL_VERSION0500:
		if (pctx_pnl->bBFIEn == true) {
			mtk_render_pnl_set_Bfi_handle(pctx_pnl,
				pctx_kms->video_priv.plane_num[MTK_VPLANE_TYPE_MEMC]);
		}
		break;
	default:
		break;
	}

	mtk_render_get_Vtt_info(&stdrmVTTInfo);
	// vsync_cycle = VSYNC_FREQ_TO_CYCLE(pctx_kms->panel_priv.outputTiming_info.u32OutputVfreq);
	// ktime_curr = ktime_get();
	// ktime_diff = (__s32)ktime_ms_delta(ktime_curr, ktime_prev);
	// ktime_prev = ktime_curr;
	// if (abs(vsync_cycle - ktime_diff) > 1)
	//	DRM_WARN("vsync warning! VTT(lock %d, in %u, out %u) Cycle(cur %d, expect %d)",
	//		stdrmVTTInfo.lock, stdrmVTTInfo.input_vtt, stdrmVTTInfo.output_vtt,
	//		ktime_diff, vsync_cycle);

	//check vttv_lock_en
	if (vttv_lock_en != stdrmVTTInfo.lock_en) {
		DRM_WARN("vttv lock_en change (%d -> %d)\n",
			vttv_lock_en, stdrmVTTInfo.lock_en);
			vttv_lock_en = stdrmVTTInfo.lock_en;
	}

	//check vttv_lock_status
	if (vttv_lock_status != stdrmVTTInfo.lock) {
		DRM_WARN("vttv lock status change (%d -> %d)\n",
			vttv_lock_status, stdrmVTTInfo.lock);
			vttv_lock_status = stdrmVTTInfo.lock;
	}

	//print real vtotal
	if ((pctx_pnl->gu64panel_debug_log) & (0x01<<E_PNL_VTT_VALUE_DBGLOG)) {
		vcnt_value = drv_hwreg_render_pnl_get_Vcnt();
		DRM_WARN("output Vtotal : (%d)\n", vcnt_value);
	}

	//print avg vtotal of 100 frames
	if ((pctx_pnl->gu64panel_debug_log) & (0x01<<E_PNL_VTT_AVG_DBGLOG)) {
		if (vcnt_count < VCNT_COUNTER) {
			vcnt_sum += drv_hwreg_render_pnl_get_Vcnt();
			vcnt_count++;
		} else {
			DRM_WARN("100 frames Vtotal Sum : (%d)\n", vcnt_sum);
			vcnt_count = 0;
			vcnt_sum = 0;
		}
	} else {
		if ((vcnt_count != 0) || (vcnt_sum != 0)) {
			vcnt_count = 0;
			vcnt_sum = 0;
		}
	}

	/*check VTT lower than Vde End or not */
	drv_hwreg_render_pnl_get_vde_frame_v_end(&u16VdeEnd);
	u16currentVtt = drv_hwreg_render_pnl_get_Vcnt();
	if (u16VdeEnd > u16currentVtt) {
		DRM_WARN("Output VTT lower than VdeEnd\n");
		DRM_WARN("u16currentVtt = %d, u16VdeEnd=%d\n", u16currentVtt, u16VdeEnd);
	}

	return 0;
}

static inline void defered_setup_ivsovs(struct mtk_tv_kms_context *pctx)
{
	// already checked the pctx
	bool is_low_latency;
	bool is_frcmdgw_fbl;
	unsigned long flags;
	uint64_t phase_diff;

	spin_lock_irqsave(&pctx->tg_lock, flags);
	is_low_latency = pctx->trigger_gen_info.IsLowLatency;
	is_frcmdgw_fbl = pctx->trigger_gen_info.IsFrcMdgwFBL;
	spin_unlock_irqrestore(&pctx->tg_lock, flags);
	phase_diff = mtk_video_panel_get_FrameLockPhaseDiff();

	_mtk_video_set_IvsOvsSourceSel_phasediff(pctx, false,
			is_low_latency, is_frcmdgw_fbl, phase_diff);
}

static int mtk_video_panel_vsync_callback(void *priv_data)
{
	struct mtk_panel_context *pctx_pnl = (struct mtk_panel_context *)priv_data;
	struct mtk_tv_kms_context *pctx_kms = DEV_PANEL_CTX_TO_KMS_CTX(pctx_pnl);
	struct ext_crtc_prop_info *crtc_props = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_plane_ctx *plane_ctx = NULL;
	unsigned int plane_index = 0;
	int ret = 0;

	if (!pctx_pnl || !pctx_kms)
		return -EINVAL;

	crtc_props = pctx_kms->ext_crtc_properties;
	pctx_video = &pctx_kms->video_priv;

	mtk_trigger_gen_init_render(pctx_kms);

	mtk_video_panel_vsync_tracker(pctx_kms, pctx_pnl);

	/* deferred setup trigger gen if panel mode id changed */
	if (static_branch_unlikely(&tg_need_update)) {
		mtk_trigger_gen_setup(pctx_kms, true, true);
		static_branch_disable(&tg_need_update);
	}

	/* deferred setup phase diff */
	if (static_branch_unlikely(&phasediff_need_update)) {
		defered_setup_ivsovs(pctx_kms);
		static_branch_disable(&phasediff_need_update);
	}

	mtk_get_framesync_locked_flag(pctx_kms);

	ret = mtk_tv_sm_ml_get_mem_index(&pctx_video->disp_irq_ml);
	if (ret == 0) {
		if ((mtk_video_panel_get_framesync_mode_en(pctx_kms) == false) &&
			(pctx_kms->panel_priv.outputTiming_info.eFrameSyncState ==
				E_PNL_FRAMESYNC_STATE_PROP_FIRE) &&
			(IS_FRAMESYNC(
				crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE])
				== true) &&
			(pctx_pnl->outputTiming_info.AutoForceFreeRun
				== false)) {
			pctx_kms->panel_priv.outputTiming_info.eFrameSyncState =
				E_PNL_FRAMESYNC_STATE_IRQ_IN;
			_mtk_video_set_framesync_mode(pctx_kms);
		}

		for (plane_index = 0; plane_index < MAX_WINDOW_NUM; plane_index++) {
			plane_ctx = (pctx_video->plane_ctx + plane_index);

			mtk_video_display_get_rbank_IRQ(plane_index, pctx_video);
		}

		mtk_tv_sm_ml_fire(&pctx_video->disp_irq_ml);
		if (pctx_kms->panel_priv.outputTiming_info.eFrameSyncState
			== E_PNL_FRAMESYNC_STATE_IRQ_IN)
			mtk_video_panel_update_framesync_state(pctx_kms);

	}

	/*for control bit vsync gfid count */
	if (pctx_kms->panel_priv.u8controlbit_gfid < MTK_DRM_KMS_GFID_MAX) { //id 0~255
		pctx_kms->panel_priv.u8controlbit_gfid = pctx_kms->panel_priv.u8controlbit_gfid + 1;
	} else {
		pctx_kms->panel_priv.u8controlbit_gfid = 0;
	}

	if (pctx_pnl->bVby1LocknProtectEn == true)
		mtk_render_pnl_checklockprotect(&pctx_pnl->bVby1LocknProtectEn);

	API_VBYO_Enable(pctx_pnl->info.typ_htt, pctx_pnl->info.typ_vtt, pctx_pnl->info.typ_dclk);

	// pqu_metadata set global frame id
	mtk_tv_drm_pqu_metadata_set_attr(&pctx_kms->pqu_metadata_priv,
		MTK_PQU_METADATA_ATTR_GLOBAL_FRAME_ID, &pctx_kms->panel_priv.u8controlbit_gfid);

	return 0;
}

static int mtk_tv_drm_panel_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtk_panel_context *pctx_pnl = NULL;
	const struct of_device_id *deviceId = NULL;
	struct device *dev = NULL;

	if (!pdev) {
		DRM_ERROR("[%s, %d]: platform_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	dev = &pdev->dev;
	bPquEnable |= of_property_read_bool(dev->of_node, "rv55_boot");

	deviceId = of_match_node(mtk_tv_drm_panel_of_match_table, pdev->dev.of_node);

	if (!deviceId) {
		DRM_WARN("of_match_node failed\n");
		return -EINVAL;
	}

	pctx_pnl = devm_kzalloc(&pdev->dev, sizeof(struct mtk_panel_context), GFP_KERNEL);
	INIT_LIST_HEAD(&pctx_pnl->panelInfoList);
	pctx_pnl->dev = &pdev->dev;
	dev_set_drvdata(pctx_pnl->dev, pctx_pnl);

	drm_panel_init(&pctx_pnl->drmPanel);
	pctx_pnl->drmPanel.dev = pctx_pnl->dev;
	pctx_pnl->drmPanel.funcs = &drmPanelFuncs;
	ret = drm_panel_add(&pctx_pnl->drmPanel);
	ret = component_add(&pdev->dev, &mtk_tv_drm_panel_component_ops);

	if (ret) {
		DRM_ERROR("[%s, %d]: component_add fail\n",
			__func__, __LINE__);
	}

	/*read panel related data from dtb*/
	ret = readDTB2PanelPrivate(pctx_pnl);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: readDTB2PanelPrivate  failed\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_tv_drm_send_vdo_pnl_sharememtopqu(pctx_pnl);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: mtk_tv_drm_send_vdo_pnl_sharememtopqu failed\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	ret = _mtk_video_panel_set_pq_context(pctx_pnl);
	if (ret != 0x0) {
		DRM_ERROR("[%s, %d]: _mtk_video_panel_set_pq_context failed\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	mtk_render_set_pnl_init_done(true);

	//check customer vcc control flag
	if (pctx_pnl->cus.vcc_bl_cusctrl == 0) {
		pctx_pnl->gpio_vcc       = devm_gpiod_get(&pdev->dev, "vcc", GPIOD_ASIS);
		pctx_pnl->gpio_backlight = devm_gpiod_get(&pdev->dev, "backlight", GPIOD_ASIS);
		//Error Handling to check gpio_vcc
		if (IS_ERR(pctx_pnl->gpio_vcc))
			DRM_ERROR("get gpio_vcc gpio desc failed\n");
		//Error Handling to check gpio_backlight
		if (IS_ERR(pctx_pnl->gpio_backlight))
			DRM_ERROR("get gpio_backlight gpio desc failed\n");

		if (gpiod_get_value(pctx_pnl->gpio_vcc) != 1)
			mtk_render_set_pnl_init_done(false);

		if (mtk_tv_drm_backlight_init(pctx_pnl, false,
				gpiod_get_value(pctx_pnl->gpio_vcc)) != 0) {
			DRM_ERROR("Backlight init failed\n");
		}
	}
	//check dlg gpio cust ctrl flag
	if ((pctx_pnl->cus.dlg_en == 1)
		&& (pctx_pnl->cus.dlg_gpio_CustCtrl == 1)
		&& (pctx_pnl->cus.tcon_en == 0))
		pctx_pnl->gpio_dlg = devm_gpiod_get(&pdev->dev, "tcon_dlg", GPIOD_ASIS);

	if (IS_ERR(pctx_pnl->gpio_dlg))
		DRM_ERROR("get gpio_dlg gpio desc failed\n");

	//check i2c wp gpio cust ctrl flag
	if ((pctx_pnl->cus.i2c_wp_CustCtrl == 1)
		&& (pctx_pnl->cus.tcon_en == 0))
		pctx_pnl->cus.i2c_wp_gpio = devm_gpiod_get(&pdev->dev, "tcon_i2c_wp", GPIOD_ASIS);

	if (IS_ERR(pctx_pnl->cus.i2c_wp_gpio))
		DRM_ERROR("get i2c_wp_gpio_num gpio desc failed\n");

#ifdef LC_TOGGLE_USE_LINUX_GPIO
	//check lc toggle gpio ctrl flag
	if (pctx_pnl->cus.tcon_en == 1) {
		pctx_pnl->gpio_lc_toggle =
		devm_gpiod_get(&pdev->dev, "tcon_lc_toggle", GPIOD_OUT_LOW);
	}

	if (IS_ERR(pctx_pnl->gpio_lc_toggle))
		DRM_ERROR("get gpio_lc_toggle gpio desc failed\n");
#endif

	// OLED GPIO and init
	mtk_oled_init(pctx_pnl, pdev);

	mutex_init(&mutex_panel);
	mutex_init(&mutex_globalmute);

	return ret;
}

static int mtk_tv_drm_panel_remove(struct platform_device *pdev)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct list_head *tmpInfoNode = NULL, *n;
	drm_st_pnl_info *tmpInfo = NULL;

	if (!pdev) {
		DRM_ERROR("[%s, %d]: platform_device is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	//free link list of drm_st_pnl_info
	pStPanelCtx = pdev->dev.driver_data;
	list_for_each_safe(tmpInfoNode, n, &pStPanelCtx->panelInfoList) {
		tmpInfo = list_entry(tmpInfoNode, drm_st_pnl_info, list);
		list_del(&tmpInfo->list);
		kfree(tmpInfo);
		tmpInfo = NULL;
	}

	component_del(&pdev->dev, &mtk_tv_drm_panel_component_ops);
	mtk_render_set_pnl_init_done(false);

	return 0;
}

// long press power key case
static void mtk_tv_drm_panel_shutdown(struct platform_device *pdev)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;
	struct drm_panel *pDrmPanel = NULL;

	if (!pdev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
				__func__, __LINE__);
		return;
	}

	pStPanelCtx = pdev->dev.driver_data;
	drm_con = pStPanelCtx->drmPanel.connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;

	pDrmPanel = pctx->connector[0].base.panel;

	//avoid od trigger gals slpport time out
	_mtk_tcon_deinit_proc(pStPanelCtx);

	// disable BL & VCC
	_mtk_panel_power_control(pDrmPanel, false);
}

int mtk_video_panel_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct ext_crtc_prop_info *crtc_props = NULL;
	struct drm_property_blob *blob = NULL;

	if (!crtc || !state || !property)
		return -EFAULT;

	pctx = (struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	crtc_props = pctx->ext_crtc_properties;

	switch (prop_index) {
	case E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO:	// copy ivs/ovs drirectly
		blob = drm_property_lookup_blob(property->dev, val);
		if (!blob) {
			DRM_ERROR("[%s][%d]prop '%s' has invalid blob id: 0x%llx\n",
				__func__, __LINE__, property->name, val);
			return -EINVAL;
		}

		if (blob->length != sizeof(struct drm_st_pnl_frc_ratio_info)) {
			DRM_ERROR("[%s][%d]prop '%s' has invalid blob size: %zu\n",
				__func__, __LINE__, property->name,
				blob->length);
			return -EINVAL;

		}
		memset(&pctx->panel_priv.frcRatio_info, 0, blob->length);
		memcpy(&pctx->panel_priv.frcRatio_info, blob->data,
						blob->length);
		drm_property_blob_put(blob);	// we don't need this blob anymore
		break;
	default:
		break;
	}

	return ret;
}

int mtk_video_panel_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val,
	int prop_index)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx =
		(struct mtk_tv_kms_context *)crtc->plane[MTK_DRM_PLANE_TYPE_VIDEO]->crtc_private;
	struct ext_crtc_prop_info *crtc_props = pctx->ext_crtc_properties;
	struct drm_property_blob *property_blob = NULL;

	switch (prop_index) {
	case E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID:
		if (val != NULL) {
			*val = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID];
			DRM_INFO("[%s][%d] get framesync plane id = %td\n",
				__func__, __LINE__, (ptrdiff_t)*val);
		}
		break;
	case E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO:
		if (val != NULL) {
			*val = crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_FRC_RATIO];
			DRM_INFO("[%s][%d] get frc ratio = %td\n",
				__func__, __LINE__, (ptrdiff_t)*val);
		}
		break;
	case E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE:
		if (val != NULL) {
			*val = crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];
			DRM_INFO("[%s][%d] get frame sync mode = %td\n",
				__func__, __LINE__, (ptrdiff_t)*val);
		}
		break;
	case E_EXT_CRTC_PROP_LOW_LATENCY_MODE:
		if (val != NULL) {
			*val = crtc_props->prop_val[E_EXT_CRTC_PROP_LOW_LATENCY_MODE];
			DRM_INFO("[%s][%d] get low latency mode status = %td\n",
				__func__, __LINE__, (ptrdiff_t)*val);
		}
		break;
	case E_EXT_CRTC_PROP_CUSTOMIZE_FRC_TABLE:
		if ((property_blob == NULL) && (val != NULL)) {
			property_blob = drm_property_lookup_blob(property->dev, *val);
			if (property_blob == NULL) {
				DRM_INFO("[%s][%d] unknown FRC table status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			} else {
				ret = _mtk_video_get_customize_frc_table(
					&pctx->panel_priv,
					property_blob->data,
					property_blob->length);
				drm_property_blob_put(property_blob);
				DRM_INFO("[%s][%d] get FRC table status = %td!!\n",
					__func__, __LINE__, (ptrdiff_t)*val);
			}
		}
		break;
	}

	return ret;
}

static void _mtk_panel_vcc_offon_delay(struct mtk_panel_context *pStPanelCtx, bool bEnable)
{
	time64_t u64time_now = 0;
	time64_t u64system_time_diff = 0;

	if (pStPanelCtx == NULL) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (bEnable) {
		DRM_INFO(" g_runtime_force_suspend_enable = %d\n", g_runtime_force_suspend_enable);
		DRM_INFO(" pStPanelCtx->cus.vcc_offon_delay = %d\n",
			pStPanelCtx->cus.vcc_offon_delay);
		if (g_runtime_force_suspend_enable == false) {
			// if it's fake sleep, we will control VCC off/on time
			while (u64system_time_diff < pStPanelCtx->cus.vcc_offon_delay) {
				u64time_now = ktime_to_ms(ktime_get());
				if (u64time_now >= g_u64time_resume) {
					u64system_time_diff = u64time_now - g_u64time_resume;
				} else {
					g_u64time_resume = 0;
				DRM_ERROR("[%s, %d]: vcc u64time_now/g_u64time_resume abnormal\n",
				__func__, __LINE__);
				}
				DRM_INFO(" u64system_time_diff = %lld\n", u64system_time_diff);
			}
			DRM_INFO(" g_u64time_resume = %lld\n", g_u64time_resume);
			DRM_INFO(" u64time_now = %lld\n", u64time_now);
		}
		g_runtime_force_suspend_enable = false;
	} else {
		g_u64time_resume = ktime_to_ms(ktime_get());
		DRM_INFO(" g_u64time_resume = %lld\n", g_u64time_resume);
	}
}


static void _mtk_panel_power_control(struct drm_panel *pDrmPanel,
	bool bOnOff)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	ktime_t offtiming1 = 0;
	ktime_t offtiming2 = 0;
	ktime_t ontiming1 = 0;
	ktime_t ontiming2 = 0;
	ktime_t pwm_to_bl_delay = 0;
	ktime_t bl_to_pwm_delay = 0;
	st_pnl_cust_extic_info stExtIcInfo;
	uint32_t u32Val = 0;
	uint32_t u32_ldm_support_type = E_LDM_UNSUPPORT;
	int ret = 0;
	static bool lbBL_status;
	unsigned int timeout = DEC_100; //1000ms
	time64_t ktime_before, ktime_after;

	if (!pDrmPanel) {
		DRM_ERROR("[%s, %d]: pDrmPanel is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pStPanelCtx = container_of(pDrmPanel, struct mtk_panel_context, drmPanel);
	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: pStPanelCtx is NULL.\n",
			__func__, __LINE__);
		return;
	}

	drm_con = pStPanelCtx->drmPanel.connector;
	if (!drm_con) {
		DRM_ERROR("[%s, %d]: drm_con is NULL.\n",
			__func__, __LINE__);
		return;
	}

	mtk_connector = to_mtk_tv_connector(drm_con);
	if (!mtk_connector) {
		DRM_ERROR("[%s, %d]: mtk_connector is NULL.\n",
			__func__, __LINE__);
		return;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;
	if (!pctx) {
		DRM_ERROR("[%s, %d]: pctx is NULL.\n",
			__func__, __LINE__);
		return;
	}

	if (bOnOff && (mtk_render_get_pnl_init_done() == FALSE)) {
		ontiming1 = pStPanelCtx->info.ontiming_1;
		ontiming2 = pStPanelCtx->info.ontiming_2;
		pwm_to_bl_delay = pStPanelCtx->cus.pwm_to_bl_delay;

		DRM_INFO("[%s][%d] Turn on panel power and backlight start.\n", __func__, __LINE__);

		// delay to protect VCC off to on
		_mtk_panel_vcc_offon_delay(pStPanelCtx, true);

		//Setting before VCC customization
		PNL_Cust_Resume_Setting_Before_VCC();

		// check VCC status first, then enable VCC
		if (gpiod_get_value(pStPanelCtx->gpio_vcc) != 1)
			gpiod_set_value(pStPanelCtx->gpio_vcc, 1);
		else
			DRM_INFO("[%s] Panel VCC is already enabled\n", __func__);

		//Setting VCC ontiming1 customization
		PNL_Cust_Resume_Setting_VCC_OnTiming1();

		// wait ontiming1
		msleep(ontiming1);

		//dlg tcon i2c cmd resume
		//some slave device will resume after panel enable VCC
		//so it needs to send i2c cmd again
		if ((pStPanelCtx->cus.dlg_en == true)
			&& (pStPanelCtx->cus.dlg_i2c_CustCtrl == true)
			&& (!pStPanelCtx->cus.tcon_en)) {
			_mtk_dlg_send_tcon_i2c_cmd(pStPanelCtx);
		}
		if (pStPanelCtx->cus.dlg_en &&
			pStPanelCtx->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet) {
			_mtk_vrr_hfr_send_i2c_cmd(pStPanelCtx);
		}

		if (pStPanelCtx->cus.pmic_seq_CusCtrl) {
			// enable VBO
			mtk_render_output_en(pStPanelCtx, true);

			msleep(pStPanelCtx->cus.pwr_on_to_pmic_dly);

			//PMIC & LSIC write
			stExtIcInfo.bTcon_en = pStPanelCtx->cus.tcon_en;
			stExtIcInfo.u32Vcom_sel =
				(mtk_tcon_get_property(E_TCON_PROP_VCOM_SEL, &u32Val) == 0) ?
				u32Val : 0;
			PNL_Cust_Resume_Setting_ExtIC_PowerOn(&stExtIcInfo);
		} else {
			//PMIC & LSIC write
			stExtIcInfo.bTcon_en = pStPanelCtx->cus.tcon_en;
			stExtIcInfo.u32Vcom_sel =
				(mtk_tcon_get_property(E_TCON_PROP_VCOM_SEL, &u32Val) == 0) ?
				u32Val : 0;
			PNL_Cust_Resume_Setting_ExtIC_PowerOn(&stExtIcInfo);

			// enable VBO
			mtk_render_output_en(pStPanelCtx, true);
		}

		//Setting data ontiming2 customization
		PNL_Cust_Resume_Setting_Data_OnTiming2();

		// wait ontiming2
		msleep(ontiming2);

		//Setting ontiming2 BL customization
		PNL_Cust_Resume_Setting_OnTiming2_Backlight();
		//ldm support, notify ldm turn on backlight
		u32_ldm_support_type = mtk_ldm_get_support_type();
		if (u32_ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU
			||u32_ldm_support_type == E_LDM_SUPPORT_R2) {
			ktime_before = ktime_to_ms(ktime_get());
			ret = mtk_ldm_set_backlight_off(false);
			//wait for ldm spi ready
			while (!MApi_ld_get_spi_ready_flag() && timeout) {
				msleep(10);
				timeout--;
			}
			ktime_after = ktime_to_ms(ktime_get());
			DRM_INFO("[DBG] wait for ldm spi ready timeout = %d, time diff = %lldms",
				timeout, ktime_after - ktime_before);
			if (ret)
				DRM_INFO("[%s][%d] ld backlight on fail \n", __func__, __LINE__);
		}

		//Setting PWM Backlight customization
		mtk_tv_drm_backlight_set_pwm_enable(&pctx->panel_priv, true);
		PNL_Cust_Resume_Setting_PWM_Backlight();

		msleep(pwm_to_bl_delay);
		DRM_INFO("[DBG] delay %d ms between PWM and BL", (__u32)pwm_to_bl_delay);

		// enable BL
		pDrmPanel->funcs->enable(pDrmPanel);
		_bBL_ON_status = TRUE;

		if (backlight_undo) {
			// satisfy the subhal want to control BL
			//ldm support, notify ldm turn off backlight(range is 0)
			if (u32_ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU
				|| u32_ldm_support_type == E_LDM_SUPPORT_R2) {
				if (bl_undo_en) {
					ret = mtk_ldm_set_backlight_off(false);
					DRM_INFO("[%s][%d] mtk_ldm_set_backlight_off : false", __func__, __LINE__);
				} else {
					ret = mtk_ldm_set_backlight_off(true);
					DRM_INFO("[%s][%d] mtk_ldm_set_backlight_off : true", __func__, __LINE__);
				}
				if (ret)
					DRM_INFO("[%s][%d]LDM models BL_off fail\n", __func__, __LINE__);
			}
			else
			{
				if (bl_undo_en) {
					pDrmPanel->funcs->enable(pDrmPanel);
					_bBL_ON_status = TRUE;
				} else {
					pDrmPanel->funcs->disable(pDrmPanel);
					_bBL_ON_status = FALSE;
				}
				DRM_INFO("[%s][%d] BL GPIO : %d", __func__, __LINE__, bl_undo_en);
			}
			backlight_undo = false;
		}

		//Setting after BL customization
		PNL_Cust_Resume_Setting_After_Backlight();

		// set bPnlInitDone = 1.
		mtk_render_set_pnl_init_done(true);

		DRM_INFO("[%s][%d] Turn on panel power and backlight done.\n", __func__, __LINE__);
	} else if (!bOnOff && (mtk_render_get_pnl_init_done() == true)) {
		offtiming1 = pctx->panel_priv.info.offtiming_1;
		offtiming2 = pctx->panel_priv.info.offtiming_2;
		bl_to_pwm_delay = pStPanelCtx->cus.bl_to_pwm_delay;

		DRM_INFO("[%s] offtiming1=%lld,offtiming2=%lld\n",
			__func__, offtiming1, offtiming2);

		//Backlight
		PNL_Cust_Suspend_Setting_Before_Backlight();

		//disable BL
		pDrmPanel->funcs->disable(pDrmPanel);
		_bBL_ON_status = FALSE;

		msleep(bl_to_pwm_delay);
		DRM_INFO("[DBG] delay %d ms between BL and PWM", (__u32)bl_to_pwm_delay);

		//disable PWM
		mtk_tv_drm_backlight_set_pwm_enable(&pctx->panel_priv, false);
		PNL_Cust_Suspend_Backlight_PWM();

		//offTiming
		PNL_Cust_Suspend_Setting_Backlight_OffTiming1();

		//offtiming1 delay
		msleep(offtiming1);

		//Data
		PNL_Cust_Suspend_Setting_OffTiming1_Data();

		// disable VBO
		mtk_render_output_en(&pctx->panel_priv, false);

		//offTiming2
		PNL_Cust_Suspend_Setting_Data_OffTiming2();

		//offTiming2 delay
		msleep(offtiming2);

		//VCC
		PNL_Cust_Suspend_Setting_OffTiming2_VCC();

		//disable VCC
		pDrmPanel->funcs->unprepare(pDrmPanel);

		// get VCC off timer
		_mtk_panel_vcc_offon_delay(pStPanelCtx, false);

		//After VCC
		PNL_Cust_Suspend_Setting_After_VCC();

		// set bPnlInitDone = 0.
		mtk_render_set_pnl_init_done(false);

		DRM_INFO("[%s][%d] Turn off panel done.\n", __func__, __LINE__);
	}

	if (lbBL_status != _bBL_ON_status) {
		DRM_INFO("BL_ON status change! %d -> %d", lbBL_status, _bBL_ON_status);
		//BL_ON status change done, set complete to return ioctl
		lbBL_status = _bBL_ON_status;
		complete(&bl_on_comp);
	}
}

#ifdef CONFIG_PM
static int panel_resume_debugtestpattern(struct device *dev)
{
	#define PATTERN_RED_VAL (0xFFF)
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;
	drm_st_pixel_mute_info pixel_mute_info = {false};
	bool bTconPatternEn = false;
	bool bGopPatternEn = false;
	bool bMultiwinPatternEn = false;
	bool bPafrcpostPatternEn = false;
	bool bSecPatternEn = false;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = dev->driver_data;

	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: panel context is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	drm_con = pStPanelCtx->drmPanel.connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;

	switch (pctx->eResumePatternselect) {
	case E_RESUME_PATTERN_SEL_DEFAULT:
		break;

	case E_RESUME_PATTERN_SEL_MOD:
		{
			pixel_mute_info.bEnable = true;
			pixel_mute_info.u32Red = PATTERN_RED_VAL;
			pixel_mute_info.u32Green = false;
			pixel_mute_info.u32Blue = false;
		}
	break;

	case E_RESUME_PATTERN_SEL_TCON:
		{
			bTconPatternEn = true;
		}
		break;

	case E_RESUME_PATTERN_SEL_GOP:
		{
			bGopPatternEn = true;
		}
		break;

	case E_RESUME_PATTERN_SEL_MULTIWIN:
		{
			bMultiwinPatternEn = true;
		}
		break;

	case E_RESUME_PATTERN_SEL_PAFRC_POST:
		{
			bPafrcpostPatternEn = true;
		}
		break;

	case E_RESUME_PATTERN_SEL_SEC:
		{
			bSecPatternEn = true;
		}
		break;

	default:
		break;
	}

	mtk_render_set_pixel_mute_video(pixel_mute_info);
	mtk_render_set_tcon_blackpattern(bTconPatternEn);
	mtk_render_set_tcon_pattern_en(bTconPatternEn);
	mtk_render_set_gop_pattern_en(bGopPatternEn);
	mtk_render_set_multiwin_pattern_en(bMultiwinPatternEn);
	mtk_render_set_pafrc_post_pattern_en(bPafrcpostPatternEn);
	mtk_render_set_sec_pattern_en(bSecPatternEn);
	#undef PATTERN_RED_VAL
	return 0;
}

static int panel_runtime_suspend(struct device *dev)
{
	panel_common_disable();

	return 0;
}

static int panel_runtime_resume(struct device *dev)
{
	struct mtk_panel_context *pStPanelCtx = NULL;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pStPanelCtx = dev->driver_data;

	pStPanelCtx->out_upd_type = E_MODE_RESET_TYPE;

	panel_common_enable();
	mtk_panel_init(dev);

	return 0;
}

static int panel_pm_runtime_force_suspend(struct device *dev)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;
	struct drm_panel *pDrmPanel = NULL;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	pStPanelCtx = dev->driver_data;
	drm_con = pStPanelCtx->drmPanel.connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;
	pDrmPanel = pctx->connector[0].base.panel;
	//_mtk_render_powerdown(pStPanelCtx, true);

	_mtk_tcon_deinit_proc(pStPanelCtx);

	g_runtime_force_suspend_enable = true;
	mutex_lock(&mutex_panel);
	_mtk_panel_power_control(pDrmPanel, false);
	mutex_unlock(&mutex_panel);

	panel_common_disable();

	if ((pStPanelCtx->cus.dlg_en) &&
		((pStPanelCtx->cus.dlg_i2c_CustCtrl) ||
		(pStPanelCtx->vrr_hfr_pmic_i2c_ctrl.SupportVrrHfrTconPmicI2cSet)))
		_i2c_need_init = true;

	switch (pctx->clk_version) {
	case MOD_CLK_VER_0:
		panel_init_clk_V1(false);
		break;
	case MOD_CLK_VER_1:
		panel_init_clk_V2(false);
		break;
	case MOD_CLK_VER_2:
		panel_init_clk_V3(false);
		break;
	case MOD_CLK_VER_3:
		panel_init_clk_V4(false);
		break;
	case MOD_CLK_VER_4:
		panel_init_clk_V5(false);
		break;
	case MOD_CLK_VER_5:
		panel_init_clk_V6(false);
		break;
	default:
		DRM_ERROR("%s: invalid mod clk version %d\n",
			__func__, pctx->clk_version);
		return -EINVAL;
	}

	return pm_runtime_force_suspend(dev);
}

static int mtk_tv_drm_panel_handler(void *data)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc_state *cur_state = NULL;
	struct drm_panel *pDrmPanel = NULL;

	pctx = (struct mtk_tv_kms_context *)(data);
	pDrmPanel = pctx->connector[0].base.panel;

	while (!kthread_should_stop()) {
		boottime_print("Panel resume power seq [start]\n");
		msleep(PNL_WAIT_ACTIVE);
		cur_state = pctx->crtc[MTK_DRM_CRTC_TYPE_VIDEO].base.state;
		if (cur_state->active == 1) {
			mutex_lock(&mutex_panel);
			_mtk_panel_power_control(pDrmPanel, true);
			mutex_unlock(&mutex_panel);
			/*
			 * restore trigger gen setting
			 * put here to make sure get the right TGEN vtt report
			 */
			mtk_tv_drm_trigger_gen_init(pctx, true);
			break;
		}
	}
	boottime_print("Panel resume power seq [end]\n");
	DRM_INFO("[%s][%d] Turn on panel done.\n", __func__, __LINE__);

	return 0;
}

int mtk_tv_drm_panel_task_handler(void *data)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_panel *pDrmPanel = NULL;

	pctx = (struct mtk_tv_kms_context *)(data);
	pDrmPanel = pctx->connector[0].base.panel;

	while (!kthread_should_stop()) {
		mutex_lock(&mutex_panel);
		if (_bTrigger_panel_pw_on) {
			_mtk_panel_power_control(pDrmPanel, true);
			_bTrigger_panel_pw_on = false;
		} else if (_bTrigger_panel_pw_off) {
			_mtk_panel_power_control(pDrmPanel, false);
			_bTrigger_panel_pw_off = false;
		}
		mutex_unlock(&mutex_panel);
		msleep(PNL_WAIT_ACTIVE);
	}

	return 0;
}

uint8_t _mtk_render_get_panel_group(__u32 vfreq)
{
	uint8_t vfreq_group = 0;

	if ((vfreq > PNL_FREQ_GROUP1_LOWER)
		&& (vfreq < PNL_FREQ_GROUP1_UPPER))
		vfreq_group = E_PNL_FREQ_GROUP1;
	else if ((vfreq > PNL_FREQ_GROUP2_LOWER)
		&& (vfreq < PNL_FREQ_GROUP2_UPPER))
		vfreq_group = E_PNL_FREQ_GROUP2;
	else if ((vfreq > PNL_FREQ_GROUP3_LOWER)
		&& (vfreq < PNL_FREQ_GROUP3_UPPER))
		vfreq_group = E_PNL_FREQ_GROUP3;
	else if ((vfreq > PNL_FREQ_GROUP4_LOWER)
		&& (vfreq < PNL_FREQ_GROUP4_UPPER))
		vfreq_group = E_PNL_FREQ_GROUP4;
	else {
		DRM_ERROR("[%s][%d] undefined pre pnl vfreq group [%d]\n",
			__func__, __LINE__, vfreq);
		vfreq_group = E_PNL_FREQ_GROUP_UNDEFINED;
	}
	return vfreq_group;
}

void mtk_render_get_global_mute_ctrl(
	struct drm_mtk_global_mute_ctrl_info *global_mute_ctrl)
{
	uint8_t pre_mode_vfreq_group = 0, new_mode_vfreq_group = 0;

	DRM_INFO("[%s][%d] typ_dclk=%llu->%llu de_width=%d->%d de_height=%d->%d vfreq=%d->%d\n",
	__func__, __LINE__,
	global_mute_ctrl->pre_mode_info.typ_dclk, global_mute_ctrl->new_mode_info.typ_dclk,
	global_mute_ctrl->pre_mode_info.de_width, global_mute_ctrl->new_mode_info.de_width,
	global_mute_ctrl->pre_mode_info.de_height, global_mute_ctrl->new_mode_info.de_height,
	global_mute_ctrl->pre_mode_info.vfreq, global_mute_ctrl->new_mode_info.vfreq);

	/* if dclk changed, need do global mute */
	if ((global_mute_ctrl->pre_mode_info.typ_dclk) !=
		(global_mute_ctrl->new_mode_info.typ_dclk)) {
		global_mute_ctrl->global_mute_enable = TRUE;
		return;
	}

	/* if output size changed (DLG on/off), need do global mute */
	if (((global_mute_ctrl->pre_mode_info.de_width) !=
		(global_mute_ctrl->new_mode_info.de_width)) ||
		((global_mute_ctrl->pre_mode_info.de_height) !=
		(global_mute_ctrl->new_mode_info.de_height))) {
		global_mute_ctrl->global_mute_enable = TRUE;
		return;
	}

	pre_mode_vfreq_group =
		_mtk_render_get_panel_group(global_mute_ctrl->pre_mode_info.vfreq);
	new_mode_vfreq_group =
		_mtk_render_get_panel_group(global_mute_ctrl->new_mode_info.vfreq);

	/* if output vfreq group undefined or changed, need do global mute */
	if ((pre_mode_vfreq_group == E_PNL_FREQ_GROUP_UNDEFINED) ||
		(new_mode_vfreq_group == E_PNL_FREQ_GROUP_UNDEFINED)) {
		global_mute_ctrl->global_mute_enable = TRUE;
		DRM_ERROR("[%s][%d] pre_group[%d], new_group[%d], enable[%d]\n", __func__, __LINE__,
		pre_mode_vfreq_group, new_mode_vfreq_group, global_mute_ctrl->global_mute_enable);
		return;
	} else if (pre_mode_vfreq_group != new_mode_vfreq_group) {
		global_mute_ctrl->global_mute_enable = TRUE;
		DRM_ERROR("[%s][%d] pre_group[%d], new_group[%d], enable[%d]\n", __func__, __LINE__,
		pre_mode_vfreq_group, new_mode_vfreq_group, global_mute_ctrl->global_mute_enable);
		return;
	}

	global_mute_ctrl->global_mute_enable = FALSE;
	DRM_INFO("[%s][%d] pre_group[%d], new_group[%d], enable[%d]\n", __func__, __LINE__,
		pre_mode_vfreq_group, new_mode_vfreq_group, global_mute_ctrl->global_mute_enable);
}

int mtk_tv_drm_get_global_mute_ctrl_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_global_mute_ctrl_info *global_mute_ctrl = NULL;

	if (!drm_dev || !data)
		return -EINVAL;

	global_mute_ctrl =
		(struct drm_mtk_global_mute_ctrl_info *)data;

	mtk_render_get_global_mute_ctrl(global_mute_ctrl);

	return ret;
}

int mtk_tv_drm_set_vac_enable_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	bool bIsenable = false;

	if (!drm_dev || !data)
		return -EINVAL;

	bIsenable = *((bool *)data);

	ret = mtk_tcon_vac_enable(bIsenable);

	return ret;
}

static int panel_pm_runtime_force_resume(struct device *dev)
{
	struct mtk_panel_context *pStPanelCtx = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_connector *mtk_connector = NULL;
	struct drm_connector *drm_con = NULL;

	if (!dev) {
		DRM_ERROR("[%s, %d]: dev is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	pStPanelCtx = dev->driver_data;

	if (!pStPanelCtx) {
		DRM_ERROR("[%s, %d]: panel context is NULL.\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	boottime_print("Panel resume [begin]\n");

	pStPanelCtx->out_upd_type = E_MODE_RESUME_TYPE;
	drm_con = pStPanelCtx->drmPanel.connector;
	mtk_connector = to_mtk_tv_connector(drm_con);
	pctx = (struct mtk_tv_kms_context *)mtk_connector->connector_private;

	switch (pctx->clk_version) {
	case MOD_CLK_VER_0:
		panel_init_clk_V1(true);
		break;
	case MOD_CLK_VER_1:
		panel_init_clk_V2(true);
		break;
	case MOD_CLK_VER_2:
		panel_init_clk_V3(true);
		break;
	case MOD_CLK_VER_3:
		panel_init_clk_V4(true);
		break;
	case MOD_CLK_VER_4:
		panel_init_clk_V5(true);
		break;
	case MOD_CLK_VER_5:
		panel_init_clk_V6(true);
		break;
	default:
		DRM_ERROR("%s: invalid mod clk version %d\n",
			__func__, pctx->clk_version);
		return -EINVAL;
	}

	/* set video mute before panel backlight on */
	mtk_video_display_mute_resume(pctx);

	panel_common_enable();

	panel_resume_debugtestpattern(dev);

	mtk_panel_init(dev);

	if (bPquEnable) {
		int ret = 0;

		ret = mtk_tv_drm_send_vdo_pnl_sharememtopqu(pStPanelCtx);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]: mtk_tv_drm_send_vdo_pnl_sharememtopqu failed\n",
				__func__, __LINE__);
			return -ENODEV;
		}

		ret = _mtk_video_panel_set_pq_context(pStPanelCtx);
		if (ret != 0x0) {
			DRM_ERROR("[%s, %d]: _mtk_video_panel_set_pq_context failed\n",
				__func__, __LINE__);
			return -ENODEV;
		}
	}

	if ((mtk_tv_drm_check_bypass_backlight() == FALSE) &&
		(false == mtk_render_get_pnl_init_done())) {
		//create mtk_panel_handler for panel sequence control
		panel_moniter_worker = kthread_run(mtk_tv_drm_panel_handler, pctx, "panel_thread");
		DRM_INFO("[%s] mtk_panel_handler was created\n", __func__);
	}
	boottime_print("Panel resume [end]\n");

	return pm_runtime_force_resume(dev);
}
#endif

int mtk_tv_drm_InitMODInterrupt(void)
{
	drv_hwreg_render_pnl_InitMODInterrupt();
	return 0;
}

bool mtk_tv_drm_GetMODInterrupt(en_drv_pnl_mod_irq_bit_map eModIrqType)
{
	return drv_hwreg_render_pnl_GetMODInterrupt(eModIrqType);
}

void mtk_tv_drm_ClearMODInterrupt(en_drv_pnl_mod_irq_bit_map eModIrqType)
{
	drv_hwreg_render_pnl_ClearMODInterrupt(eModIrqType);
}

bool mtk_tv_drm_Is_Support_TCON_LC_SecToggle(void)
{
	bool bRet;
	uint16_t u16EnableBitMap = 0;
	uint16_t u16TogglePeriod = 0;

	drv_hwreg_render_pnl_get_tcon_gpo_info(&u16EnableBitMap, &u16TogglePeriod);
	if ((u16TogglePeriod > 0) && (u16EnableBitMap & BIT(3))) {
		if (u16TogglePeriod != _u16TCON_LC_SecToggle_Frame_Cnt_Target) {
			_u16TCON_LC_SecToggle_Frame_Cnt = 0;
			_u16TCON_LC_SecToggle_Frame_Cnt_Target = u16TogglePeriod;
		}
		bRet = TRUE;
	} else {
		bRet = FALSE;
	}
	return bRet;
}
void _mtk_tv_drm_TCON_LC_SecToggle_Status_Reset(void)
{
	_bTCON_LC_SecToggle_Start = FALSE;
	_u16PreTCON_LC_TOGGLE_GPO_Status = TCON_GPO_LC_BitMap;
	_u16CurTCON_LC_TOGGLE_GPO_Status = 0;
	_u16TCON_LC_SecToggle_Frame_Cnt = 0;
}

void mtk_tv_drm_tcon_LC_SecToggle_Set(struct mtk_panel_context *pctx_pnl,
	bool bPreStatus, bool bForce, bool bForceValue)
{
	if (bForce)	{
		gpiod_set_value(pctx_pnl->gpio_lc_toggle, bForceValue);
	} else {
		if (bPreStatus) {
			//set low
			gpiod_set_value(pctx_pnl->gpio_lc_toggle, 0);
		} else {
			//set high
			gpiod_set_value(pctx_pnl->gpio_lc_toggle, 1);
		}
	}
}

void mtk_tv_drm_TCON_LC_SecToggle_Control(struct mtk_panel_context *pctx_pnl)
{
	uint16_t tmp;
	uint16_t TCON_LC_TOGGLE_GPO_Status;

	if (_bTCON_LC_SecToggle_Start == FALSE)	{
		_u16CurTCON_LC_TOGGLE_GPO_Status =
			drv_hwreg_render_pnl_get_tcon_lc_toggle_gpo_status();
		//for uboot to kernel stage stably convert
		if ((!(_u16PreTCON_LC_TOGGLE_GPO_Status & TCON_GPO_LC_BitMap)) &&
			(_u16CurTCON_LC_TOGGLE_GPO_Status & TCON_GPO_LC_BitMap)) {
			//rasing edge to trig starting...
			_bTCON_LC_SecToggle_Start = TRUE;
			_u16TCON_LC_SecToggle_Frame_Cnt = 0;
			//for auto update gpo setting
			drv_hwreg_render_pnl_tcon_LC_auto_upd_gpo_setting();
			//force logic high
#ifdef LC_TOGGLE_USE_LINUX_GPIO
			mtk_tv_drm_tcon_LC_SecToggle_Set(pctx_pnl, FALSE, TRUE, TRUE);
#else
			drv_hwreg_render_pnl_tcon_LC_SecToggle_Set(FALSE, TRUE, TRUE);
#endif
			DRM_INFO("TCON_LC_SecToggle_Start\n");
		}
		//update gpo status
		_u16PreTCON_LC_TOGGLE_GPO_Status = _u16CurTCON_LC_TOGGLE_GPO_Status;
	} else	{
		//read gpo ps global frame count reset
		//Bank0x2444_0x50[0]:GPO_PS_FCNT_RST
		tmp = drv_hwreg_render_pnl_get_tcon_gpo_ps_fcnt_rst();
		if (tmp && (_bTCON_LC_SecToggle_STR_Off == FALSE)) { //GPO all off state
			_u16TCON_LC_SecToggle_Frame_Cnt = 0;
			_bTCON_LC_SecToggle_STR_Off = TRUE;
			//force set low
#ifdef LC_TOGGLE_USE_LINUX_GPIO
			mtk_tv_drm_tcon_LC_SecToggle_Set(pctx_pnl, FALSE, TRUE, FALSE);
#else
			drv_hwreg_render_pnl_tcon_LC_SecToggle_Set(FALSE, TRUE, FALSE);
#endif
		} else if (_bTCON_LC_SecToggle_STR_Off == TRUE) {
			if (!tmp) {
				//for auto update gpo setting
				drv_hwreg_render_pnl_tcon_LC_auto_upd_gpo_setting();
				//force set high
#ifdef LC_TOGGLE_USE_LINUX_GPIO
				mtk_tv_drm_tcon_LC_SecToggle_Set(pctx_pnl,
				FALSE, TRUE, TRUE);
#else
				drv_hwreg_render_pnl_tcon_LC_SecToggle_Set(FALSE, TRUE, TRUE);
#endif
				_bTCON_LC_SecToggle_STR_Off = FALSE;
				_u16TCON_LC_SecToggle_Frame_Cnt = 0;
			}
		} else {
			//start to frame count toggle
			if (_u16TCON_LC_SecToggle_Frame_Cnt <
				_u16TCON_LC_SecToggle_Frame_Cnt_Target) {
				_u16TCON_LC_SecToggle_Frame_Cnt++;
			} else {
				_u16TCON_LC_SecToggle_Frame_Cnt = 0; //clear frame counter
				TCON_LC_TOGGLE_GPO_Status =
					drv_hwreg_render_pnl_get_tcon_lc_toggle_gpo_status();
				if (TCON_LC_TOGGLE_GPO_Status & TCON_GPO_LC_BitMap) {
#ifdef LC_TOGGLE_USE_LINUX_GPIO
					mtk_tv_drm_tcon_LC_SecToggle_Set(pctx_pnl,
					TRUE, FALSE, FALSE);
#else
					drv_hwreg_render_pnl_tcon_LC_SecToggle_Set(TRUE,
						FALSE, FALSE);
#endif
				} else {
#ifdef LC_TOGGLE_USE_LINUX_GPIO
					mtk_tv_drm_tcon_LC_SecToggle_Set(pctx_pnl,
					FALSE, FALSE, FALSE);
#else
					drv_hwreg_render_pnl_tcon_LC_SecToggle_Set(FALSE,
						FALSE, FALSE);
#endif
				}
			}
		}
	}
}

int _mtk_render_set_ldm_backlight(struct mtk_panel_context *pctx, bool bEnable)
{
	uint32_t u32_ldm_support_type = E_LDM_UNSUPPORT;
	int ret = 0;

	if (!pctx) {
		DRM_ERROR("[%s, %d] pctx is NULL.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (mtk_render_get_pnl_init_done()) {
		if (pctx->cus.vcc_bl_cusctrl == 0) {
			//ldm support, notify ldm turn off backlight(range is 0)
			u32_ldm_support_type = mtk_ldm_get_support_type();
			if (u32_ldm_support_type == E_LDM_SUPPORT_TRUNK_PQU
				|| u32_ldm_support_type == E_LDM_SUPPORT_R2) {
				ret = mtk_ldm_set_backlight_off(bEnable);

				DRM_INFO("[%s][%d] ldm set backlight=%d ret=%d\n",
						__func__, __LINE__, bEnable, ret);
			}
		}
	}

	return ret;
}

static int _set_global_unmute_handler(void *data)
{
	int Ret = 0;
	struct mtk_global_mute_context *pCon = NULL;
	struct mtk_panel_context *pctx_pnl;
	drm_st_backlight_mute_info backlight_mute_info;
	drm_st_pixel_mute_info pixel_mute_info;
	__u32 global_unmute_delay;
	int64_t tCur;

	pCon = (struct mtk_global_mute_context *)(data);

	if (!pCon) {
		DRM_ERROR("[%s, %d] pCon is NULL.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	pctx_pnl = pCon->pctx_pnl;

	if (!pctx_pnl) {
		DRM_ERROR("[%s, %d] pctx_pnl is NULL.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	backlight_mute_info = pCon->backlight_mute_info;
	pixel_mute_info = pCon->pixel_mute_info;
	global_unmute_delay = pctx_pnl->cus.global_unmute_delay;

	//get current time
	tCur = ktime_get();

	mutex_lock(&mutex_globalmute);

	//Depending on the hardware characteristics of different panels,
	//it may take a few milliseconds to delay
	if (global_unmute_delay)
		msleep(global_unmute_delay);

	if (pixel_mute_info.u8ConnectorID == 0)
		Ret = mtk_render_set_pixel_mute_video(pixel_mute_info);
	else if (pixel_mute_info.u8ConnectorID == 1)
		Ret = mtk_render_set_pixel_mute_deltav(pixel_mute_info);
	else
		Ret = mtk_render_set_pixel_mute_gfx(pixel_mute_info);

	if (!pctx_pnl->cus.global_mute_backlight_ignore)
		Ret = _set_backlight_mute(pctx_pnl, backlight_mute_info);
	else
		Ret = _mtk_render_set_ldm_backlight(pctx_pnl, backlight_mute_info.bEnable);

	DRM_INFO("[%s] BackLight unmute done. Total time=%lld ms\n", __func__,
		ktime_ms_delta(ktime_get(), tCur));

	g_bGlobalMuteEn = FALSE;
	mutex_unlock(&mutex_globalmute);
	return Ret;
}

int mtk_render_set_global_mute(struct mtk_panel_context *pctx_pnl,
	struct mtk_tv_kms_context *pctx_kms,
	drm_st_global_mute_info global_mute_info,
	uint8_t u8connector_id)
{
	int Ret = 0;
	drm_st_tx_mute_info tx_mute_info;
	drm_st_backlight_mute_info backlight_mute_info;
	drm_st_pixel_mute_info pixel_mute_info;

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] pctx_pnl is NULL\n", __func__);
		return -EINVAL;
	}

	if (pctx_kms == NULL) {
		DRM_INFO("[%s] pctx_kms  is NULL\n", __func__);
		return -EINVAL;
	}

	if (pctx_kms->out_model == E_OUT_MODEL_VG_BLENDED) {
		DRM_INFO("[%s] BackLight mute en = %d mute delay=%d ms unmute delay=%d ms\n",
			__func__, global_mute_info.bEnable,
			pctx_pnl->cus.global_mute_delay, pctx_pnl->cus.global_unmute_delay);
		DRM_INFO("[%s] BackLight mute ignore=%d\n",
			__func__, pctx_pnl->cus.global_mute_backlight_ignore);

		//backlight mute properties
		backlight_mute_info.bEnable = global_mute_info.bEnable;
		backlight_mute_info.u8ConnectorID = u8connector_id;

		//pixel mute properties
		pixel_mute_info.bEnable = global_mute_info.bEnable;
		pixel_mute_info.bLatchMode = global_mute_info.bLatchMode;
		pixel_mute_info.u32Blue = global_mute_info.u32Blue;
		pixel_mute_info.u32Green = global_mute_info.u32Green;
		pixel_mute_info.u32Red = global_mute_info.u32Red;
		pixel_mute_info.u8ConnectorID = u8connector_id;

		//force color set by the pixel_mute_rgb sysfs
		if (pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_RED] != 0) {
			DRM_INFO("[%s] Pixel color R=%d -> %d\n",
				__func__, pixel_mute_info.u32Red,
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_RED]);
			pixel_mute_info.u32Red =
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_RED];
		}

		if (pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_GREEN] != 0) {
			DRM_INFO("[%s] Pixel color G=%d -> %d\n",
				__func__, pixel_mute_info.u32Green,
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_GREEN]);
			pixel_mute_info.u32Green =
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_GREEN];
		}

		if (pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_BLUE] != 0) {
			DRM_INFO("[%s] Pixel color B=%d -> %d\n",
				__func__, pixel_mute_info.u32Blue,
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_BLUE]);
			pixel_mute_info.u32Blue =
				pctx_pnl->pixel_mute_rgb_info[MTK_DRM_PIXEL_MUTE_BLUE];
		}

		if (global_mute_info.bEnable) {
			mutex_lock(&mutex_globalmute);
			if (!pctx_pnl->cus.global_mute_backlight_ignore)
				Ret = _set_backlight_mute(pctx_pnl, backlight_mute_info);
			else
				Ret = _mtk_render_set_ldm_backlight(pctx_pnl,
							backlight_mute_info.bEnable);

			if (u8connector_id == 0)
				Ret = mtk_render_set_pixel_mute_video(pixel_mute_info);
			else if (u8connector_id == 1)
				Ret = mtk_render_set_pixel_mute_deltav(pixel_mute_info);
			else
				Ret = mtk_render_set_pixel_mute_gfx(pixel_mute_info);

			//Depending on the hardware characteristics of different panels,
			//it may take a few milliseconds to delay
			if (pctx_pnl->cus.global_mute_delay && !g_bGlobalMuteEn)
				msleep(pctx_pnl->cus.global_mute_delay);

			g_bGlobalMuteEn = TRUE;
			mutex_unlock(&mutex_globalmute);
		} else {
			g_global_mute_con.pctx_pnl = pctx_pnl;
			g_global_mute_con.backlight_mute_info = backlight_mute_info;
			g_global_mute_con.pixel_mute_info = pixel_mute_info;

			if (pctx_pnl->cus.global_unmute_delay == 0) {
				_set_global_unmute_handler((void *)&g_global_mute_con);
				DRM_INFO("[%s] set_global_unmute_handler was called directly.\n",
					__func__);
			} else {
				g_global_unmute_worker = kthread_run(_set_global_unmute_handler,
						(void *)&g_global_mute_con,
						"global_unmute_thread");

				if (IS_ERR(g_global_unmute_worker)) {
					TCON_ERROR("create set_global_unmute_handler failed!\n");
					_set_global_unmute_handler((void *)&g_global_mute_con);
				} else
					DRM_INFO(
					"[%s] set_global_unmute_handler thread was created.(PID=%d)\n",
					__func__, g_global_unmute_worker->pid);
			}
		}
		DRM_INFO("[%s] BackLight mute done\n", __func__);
	} else {
		DRM_INFO("[%s] TX mute en = %d\n", __func__, global_mute_info.bEnable);
		tx_mute_info.bEnable = global_mute_info.bEnable;
		tx_mute_info.u8ConnectorID = u8connector_id;
		Ret = mtk_render_set_tx_mute_common(pctx_pnl, tx_mute_info, u8connector_id);
	}

	return Ret;
}

int mtk_tv_drm_set_dispout_pw_onoff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int Ret = 0;
	bool bEnable = false;
	struct drm_crtc *crtc = NULL;
	struct drm_panel *pDrmPanel = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	time64_t ktime_before, ktime_after, ktime_diff;

	if (!drm_dev || !data)
		return -EINVAL;

	// Get kms structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (pctx != NULL)
		pctx_pnl = &pctx->panel_priv;
	else {
		DRM_INFO("[%s] pctx is NULL\n", __func__);
		return -ENODEV;
	}

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] pctx_pnl is NULL\n", __func__);
		return -ENODEV;
	}
	ktime_before = ktime_to_ms(ktime_get());

	pDrmPanel = pctx->connector[0].base.panel;

	bEnable = *((bool *)data);

	if (bEnable) {
		_bTrigger_panel_pw_on = true;
		_bTrigger_panel_pw_off = false;
	} else {
		_bTrigger_panel_pw_on = false;
		_bTrigger_panel_pw_off = true;
	}

	ktime_after = ktime_to_ms(ktime_get());
	ktime_diff = ktime_after - ktime_before;
	DRM_INFO(" ktime_diff = %lld\n", ktime_diff);

	return Ret;
}

int mtk_tv_drm_get_panel_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int Ret = 0;
	struct drm_mtk_panel_info *panel_info = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!drm_dev || !data)
		return -EINVAL;

	// Get kms structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (pctx != NULL)
		pctx_pnl = &pctx->panel_priv;
	else {
		DRM_INFO("[%s] pctx is NULL\n", __func__);
		return -ENODEV;
	}

	if (pctx_pnl == NULL) {
		DRM_INFO("[%s] pctx_pnl is NULL\n", __func__);
		return -ENODEV;
	}

	panel_info = (struct drm_mtk_panel_info *)data;
	memset(panel_info->panel_name, 0, DRM_NAME_MAX_NUM);
	strncpy(panel_info->panel_name, pctx_pnl->cus.panel_name, DRM_NAME_MAX_NUM - 1);
	panel_info->panel_inch_size = pctx_pnl->cus.inch_size;
	DRM_INFO("[%s] panel_info (%s, %d)\n",
		__func__, panel_info->panel_name, panel_info->panel_inch_size);

	return Ret;
}

int mtk_tv_drm_set_demura_onoff_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int Ret = 0;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx_kms = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_demura_context *pctx_demura = NULL;
	bool bEnable = false;

	if (!drm_dev || !data)
		return -EINVAL;

	// Get kms structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx_kms = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	bEnable = *((bool *)data);
	DRM_INFO("[%s] bEnable = %d\n", __func__, bEnable);

	pctx_demura = _mtk_get_demura_context_by_kms(pctx_kms);
	if (pctx_demura == NULL) {
		DRM_ERROR("pctx_demura is NULL!!!");
		return -EINVAL;
	}
	mtk_tv_drm_set_demura_enable(pctx_demura, bEnable);

	return Ret;
}

void mtk_panel_set_max_framerate(__u32 u32TypMaxFramerate, __u32 u32DlgMaxFramerate)
{
	PanelMaxFramerate.typ_max_framerate = u32TypMaxFramerate;
	PanelMaxFramerate.dlg_max_framerate = u32DlgMaxFramerate;
	DRM_INFO("[%s] Set panel max framerate = %d, %d\n",
		__func__, u32TypMaxFramerate, u32DlgMaxFramerate);
}

struct drm_mtk_panel_max_framerate mtk_panel_get_max_framerate(void)
{
	return PanelMaxFramerate;
}

int mtk_tv_drm_get_panel_max_framerate_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_panel_max_framerate *MaxFramerate = 0;

	if (!drm_dev || !data)
		return -EINVAL;

	MaxFramerate =
		(struct drm_mtk_panel_max_framerate *)data;

	*MaxFramerate = mtk_panel_get_max_framerate();

	return ret;
}

bool mtk_tv_drm_Is_Support_PatDetectFunc(void)
{
	bool bSupport = FALSE;
	__u16 u16SupportBitMap = E_PNL_TCON_PDF_MODE_CSOT_4K1K_HSR |
							  E_PNL_TCON_PDF_MODE_BOE_4K1K_HSR |
							  E_PNL_TCON_PDF_MODE_HKC |
							  E_PNL_TCON_PDF_MODE_AUO_4K1K_HSR;
	__u16 u16EnableBitMap = 0;
	//use modv11 bank0x2427_0x7B[15:0] to store switch information
	//[7]:CSOT PDF enable for 4K1K HSR
	//[5]:BOE PDF enable for 4K1K HSR
	//[4]:HKC pattern_det enable for inversion enable
	//[1]:AUO PDF enable for 4K1K HSR
	u16EnableBitMap = drv_hwreg_render_pnl_get_Support_PatDetectFunc();
	if(u16EnableBitMap&u16SupportBitMap)
	{
		bSupport = TRUE;
	}
	else
	{
		bSupport = FALSE;
	}
	return bSupport;
}

void mtk_tv_drm_TCON_PatDetectFunc_Control(void)
{
	__u16 u16PDF_TypeOut = 0;
	__u16 u16EnableBitMap = 0;
	u16EnableBitMap = drv_hwreg_render_pnl_get_Support_PatDetectFunc();
	u16PDF_TypeOut = drv_hwreg_render_pnl_get_PDF_TypeOut();

#if 0 //There is no requirement for this type PDF now, please handle it if get spec.
	if(u16EnableBitMap&E_PNL_TCON_PDF_MODE_CSOT_4K1K_HSR)
	{

	}

	if(u16EnableBitMap&E_PNL_TCON_PDF_MODE_BOE_4K1K_HSR)
	{

	}
#endif

	if(u16EnableBitMap & E_PNL_TCON_PDF_MODE_HKC)
	{
		if(u16PDF_TypeOut & (BIT(14) | BIT(0)))//2P2Q or 2Q2P || 0P0Q or 0Q0P
		{
			//write 0x2442_0x3E[12] = 1'b1
			drv_hwreg_render_pnl_set_tcon_pdf_enable(TRUE);
		}
		else
		{
			//write 0x2442_0x3E[12] = 1'b0
			drv_hwreg_render_pnl_set_tcon_pdf_enable(FALSE);
		}
	}

#if 0
	if(u16EnableBitMap&E_PNL_TCON_PDF_MODE_AUO_4K1K_HSR)
	{

	}
#endif
}

uint32_t mtk_tv_drm_get_fpsx100_value(void)
{
	st_fps_value stFPS = {0};

	drv_hwreg_render_pnl_get_fps(&stFPS);
	return stFPS.value / 10;
}

int mtk_tv_drm_get_blon_status_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	uint8_t *BL_status = 0;

	if (!drm_dev || !data)
		return -EINVAL;

	DRM_INFO("[%s] IN, _bBL_ON_status = %d", __func__, _bBL_ON_status);

	//sleep for BL_ON change event
	wait_for_completion_interruptible(&bl_on_comp);

	BL_status =	(uint8_t *)data;
	*BL_status = (uint8_t)_bBL_ON_status;

	DRM_INFO("[%s] OUT, _bBL_ON_status = %d", __func__, _bBL_ON_status);

	return ret;
}

void mtk_panel_get_pnl_timing_info(drm_st_pnl_mode_timing *pnl_mode_timing,
	struct mtk_panel_context *pctx_pnl)
{
	drm_st_pnl_info *tmpInfo = NULL;

	list_for_each_entry(tmpInfo, &pctx_pnl->panelInfoList, list) {
		if (tmpInfo->de_width == 0 || tmpInfo->de_height == 0) {
			DRM_INFO("[Eric] end of list\n");
			goto FINISH;
		}
		if ((tmpInfo->de_width == pnl_mode_timing->de_width) &&
			(tmpInfo->de_height == pnl_mode_timing->de_height) &&
			(tmpInfo->typ_dclk == (pnl_mode_timing->typ_dclk*1000))) {
			pnl_mode_timing->de_hstart = tmpInfo->de_hstart;
			pnl_mode_timing->de_vstart = tmpInfo->de_vstart;
			DRM_INFO("pnl_mode_timing de_hstart = %d, de_vstart = %d\n",
				pnl_mode_timing->de_hstart, pnl_mode_timing->de_vstart);
			return;
		}
	}

	FINISH:
	if ((pnl_mode_timing->de_hstart == 0) || (pnl_mode_timing->de_vstart == 0)) {
		pnl_mode_timing->de_hstart = pctx_pnl->info.de_hstart;
		pnl_mode_timing->de_vstart = pctx_pnl->info.de_vstart;
		DRM_ERROR("[%s] do not match timing, protect de_hstart = %d, de_vstart = %d\n", __func__,
			pnl_mode_timing->de_hstart, pnl_mode_timing->de_vstart);
	}
}

int mtk_tv_drm_get_pnl_timing_info_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	drm_st_pnl_mode_timing *pnl_mode_timing = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!drm_dev || !data)
		return -EINVAL;

	// Get kms structure
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (pctx != NULL)
		pctx_pnl = &pctx->panel_priv;
	else {
		DRM_ERROR("[%s] pctx is NULL\n", __func__);
		return -ENODEV;
	}

	if (pctx_pnl == NULL) {
		DRM_ERROR("[%s] pctx_pnl is NULL\n", __func__);
		return -ENODEV;
	}

	DRM_INFO("[%s] IN\n", __func__);

	pnl_mode_timing = (drm_st_pnl_mode_timing *)data;
	mtk_panel_get_pnl_timing_info(pnl_mode_timing, pctx_pnl);

	DRM_INFO("[%s] pnl_mode_timing de_hstart=%d, de_vstart=%d\n", __func__, pnl_mode_timing->de_hstart, pnl_mode_timing->de_vstart);

	return ret;
}


MODULE_AUTHOR("Mediatek TV group");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
