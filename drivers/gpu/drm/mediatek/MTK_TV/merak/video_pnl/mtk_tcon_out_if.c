// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek tcon output driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/time.h>

#include "mtk_tcon_out_if.h"
#include "mtk_tcon_dga.h"
#include "mtk_tcon_lineod.h"
#include "mtk_tcon_od.h"
#include "mtk_tcon_vac.h"
#include "pnl_cust.h"

#include "../metabuf/mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "pqu_msg.h"
#include "m_pqu_render.h"

#include "hwreg_render_video_tcon.h"
#include "hwreg_render_video_tcon_vac.h"
#include "hwreg_render_video_tcon_dga.h"
#include "hwreg_render_stub.h"

#define TCON_NODE             "tcon"
#define TCON_REG              "reg"

#define LANE_NUM_32 (32)
#define LANE_NUM_16 (16)
#define LANE_NUM_12 (12)
#define LANE_NUM_8 (8)
#define LANE_NUM_6 (6)
#define LANE_NUM_4 (4)
#define LANE_NUM_3 (3)
#define LANE_NUM_2 (2)

#define TCON_COMP_TIMEOUT (100)		//ms
#define TCON_TOGGLE_GPO_TIME (30)	//ms

//for panel tab
typedef enum {
	E_PNL_TCON_PANELINFO_PANELLINKTYPE = 0,
	E_PNL_TCON_PANELINFO_PANELLINKEXTTYPE,
	E_PNL_TCON_PANELINFO_FIXEDVABCKPORCH,
	E_PNL_TCON_PANELINFO_FIXEDHBACKPORCH,

	E_PNL_TCON_PANELINFO_VSYNC_START = 11,
	E_PNL_TCON_PANELINFO_VSYNC_END,
	E_PNL_TCON_PANELINFO_VDESTART,
	E_PNL_TCON_PANELINFO_VDEND,
	E_PNL_TCON_PANELINFO_PANELMAXVTOTAL,
	E_PNL_TCON_PANELINFO_PANELVTOTAL,
	E_PNL_TCON_PANELINFO_MINVTOTAL,
	E_PNL_TCON_PANELINFO_VDEHEADDUMMY,
	E_PNL_TCON_PANELINFO_VDETAILDUMMY,

	E_PNL_TCON_PANELINFO_HSYNC_START = 31,
	E_PNL_TCON_PANELINFO_HSYNC_END,
	E_PNL_TCON_PANELINFO_HDESTART,
	E_PNL_TCON_PANELINFO_HDEEND,
	E_PNL_TCON_PANELINFO_PANELMAXHTOTAL,
	E_PNL_TCON_PANELINFO_PANELHTOTAL,
	E_PNL_TCON_PANELINFO_PANELMINHTOTAL,
	E_PNL_TCON_PANELINFO_HDEHEADDUMMY,
	E_PNL_TCON_PANELINFO_HDETAILDUMMY,

	E_PNL_TCON_PANELINFO_PANELMAXDCLK = 51,
	E_PNL_TCON_PANELINFO_PANELDCLK,
	E_PNL_TCON_PANELINFO_PANELMINDCLK,
	E_PNL_TCON_PANELINFO_PANELMAXSET,
	E_PNL_TCON_PANELINFO_MINSET,
	E_PNL_TCON_PANELINFO_OUTTIMINGMODE,
	E_PNL_TCON_PANELINFO_TOTALPAIR,
	E_PNL_TCON_PANELINFO_PAIREVEN,
	E_PNL_TCON_PANELINFO_PAIRODD,
	E_PNL_TCON_PANELINFO_DUALPORT,

	E_PNL_TCON_PANELINFO_PANELHSYNCWIDTH = 71,
	E_PNL_TCON_PANELINFO_PANELHSYNCBACKPORCH,
	E_PNL_TCON_PANELINFO_PANELVSYNCWIDTH,
	E_PNL_TCON_PANELINFO_PANELVBACKPORCH,

	E_PNL_TCON_PANELINFO_SSC_ENABLE = 80,
	E_PNL_TCON_PANELINFO_SSC_FMODULATION,
	E_PNL_TCON_PANELINFO_SSC_PERCENTAGE,
	E_PNL_TCON_PANELINFO_SSC_BIN_CTRL,
	E_PNL_TCON_PANELINFO_DEMURA_SEL,
	E_PNL_TCON_PANELINFO_DATA_PATH_SEL,
	E_PNL_TCON_PANELINFO_VCOM_SEL,

	E_PNL_TCON_PANELINFO_DEMURA_ENABLE = 91,
	E_PNL_TCON_PANELINFO_OD_ENABLE,
	E_PNL_TCON_PANELINFO_OD_MODE_TYPE,
} E_PNL_TCON_PANELINFO;

typedef struct {
	uint32_t u32PanelLinkType;      //use for STI and uboot
	uint32_t u32PanelLinkExtType;   //use for mixed mode utopia
	uint32_t u32FixedVBackPorch;
	uint32_t u32FixedHBackPorch;
	uint32_t u32VsyncStart;
	uint32_t u32VsyncEnd;
	uint32_t u32VDEStart;
	uint32_t u32VDEEnd;
	uint32_t u32PanelMaxVTotal;
	uint32_t u32PanelVTotal;
	uint32_t u32PanelMinVTotal;
	uint32_t u32VDEHeadDummy;
	uint32_t u32VDETailDummy;
	uint32_t u32HsyncStart;
	uint32_t u32HsyncEnd;
	uint32_t u32HDEStart;
	uint32_t u32HDEEnd;
	uint32_t u32PanelMaxHTotal;
	uint32_t u32PanelHTotal;
	uint32_t u32PanelMinHTotal;
	uint32_t u32HDEHeadDummy;
	uint32_t u32HDETailDummy;
	uint32_t u32PanelMaxDCLK;
	uint32_t u32PanelDCLK;
	uint32_t u32PanelMinDCLK;
	uint32_t u32PanelMaxSET;
	uint32_t u32PanelMinSET;
	uint32_t u32OutTimingMode;
	uint32_t u32HTotalPair;
	uint32_t u32VTotalPair_Even;
	uint32_t u32VTotalPair_Odd;
	uint32_t u32DaulPort;
	uint32_t u32PanelHsyncWidth;
	uint32_t u32PanelHsyncBackPorch;
	uint32_t u32PanelVsyncWidth;
	uint32_t u32PanelVsyncBackPorch;
	uint32_t u32SSCEnable;
	uint32_t u32SSC_Fmodulation;
	uint32_t u32SSC_Percentage;
	uint32_t u32SSC_TconBinCtrl;
	uint32_t u32Demura_sel;
	uint32_t u32DataPath_sel;
	uint32_t u32Vcom_sel;
	uint32_t u32DemuraEnable;
	uint32_t u32OdEnable;
	uint32_t u32OdModeType;
} ST_TCON_PANELINFO;

struct tcon_powersequence_context {
	uint8_t *pu8Tab;
	uint16_t u16RegCount;
	bool bOn;
	struct completion *comp;
	struct completion *adl_comp;
};

ST_TCON_PANELINFO g_stPnlInfo;
bool g_bPreInitDone = FALSE;
bool g_bEnable = TRUE;
bool g_ModeChange = FALSE;
bool g_bGameDirectFramerateMode = FALSE;
bool g_bGpioSetting = FALSE;
bool g_bEnableSetting = FALSE;
bool g_bGpioComActive = FALSE;
bool g_bAdlComActive = FALSE;
static struct task_struct *g_gpio_worker;
static struct task_struct *g_enable_worker;
struct tcon_powersequence_context g_power_seq_con;
drm_update_output_timing_type g_out_upd_type = E_OUTPUT_TYPE_MAX;

struct completion g_gpio_comp;
struct completion g_adl_done_comp;
struct mutex mutex_tcon;

bool _tcon_path_patch(struct mtk_panel_context *pCon)
{
	int nRet = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	nRet = drv_hwreg_render_tcon_set_path_patch(
			(EN_DRV_TCON_PATH_MODE)g_stPnlInfo.u32DataPath_sel);

	return TCON_GET_RET(nRet);
}

bool _tcon_general_setting(
			struct mtk_panel_context *pCon,
			uint8_t *pu8TconTab,
			uint16_t u16RegisterCount)
{
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	drv_hwreg_render_tcon_write_general_setting(pu8TconTab, u16RegisterCount);

	bRet &= _tcon_path_patch(pCon);
	TCON_FUNC_EXIT(bRet);
	return bRet;
}

void mtk_tcon_recover_isp_cmd_gamma(void)
{
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!g_stPnlInfo.u32PanelLinkExtType) {
		TCON_ERROR("u32PanelLinkExtType is not set\n");
		return;
	}

	drv_hwreg_render_tcon_RecoverIspCmdGamma(g_stPnlInfo.u32PanelLinkExtType);

	TCON_FUNC_EXIT(bRet);
}

void mtk_tcon_store_isp_cmd_gamma(void)
{
	bool bRet = TRUE;
	static bool IsInitIspCmdGamma;

	TCON_FUNC_ENTER();

	if (!g_stPnlInfo.u32PanelLinkExtType) {
		TCON_ERROR("u32PanelLinkExtType is not set\n");
		return;
	}

	if (!IsInitIspCmdGamma) {
		drv_hwreg_render_tcon_StoreIspCmdGamma(g_stPnlInfo.u32PanelLinkExtType);
		IsInitIspCmdGamma = TRUE;
	}
	TCON_FUNC_EXIT(bRet);
}

static int _tcon_gpio_setting_handler(void *data)
{
	int64_t tCur;
	int64_t tDiff = 0;
	struct completion *comp = (struct completion *)data;

	//get current time
	tCur = ktime_get();

	//do REG_0140_TCON_GPO_COM_15_00_REG_GPO_SW_PARA_UPDATE_0140
	TCON_W1BYTEMSK(0x2444A0, BIT(1), BIT(1));

	while (tDiff < TCON_TOGGLE_GPO_TIME) {
		TCON_DEBUG("Toggle gpo para setting. Diff=%lld ms\n", tDiff);
		mdelay(8); //need to use mdelay , udelay didn't work
		tDiff = ktime_ms_delta(ktime_get(), tCur);
	}

	TCON_W1BYTEMSK(0x2444A0, 0, BIT(1));
	TCON_DEBUG("Toggle gpo para setting done. Total time=%lld ms\n",
		ktime_ms_delta(ktime_get(), tCur));

	if (comp)
		complete(comp);
	else
		TCON_ERROR("comp is NULL\n");

	g_bGpioSetting = FALSE;
	return 0;
}

bool _tcon_gpio_setting(uint8_t *pu8TconTab, uint16_t u16RegisterCount)
{
	uint32_t u32tabIdx = 0;
	uint32_t u32Addr;
	uint8_t u8Mask;
	uint8_t u8Value;
	//uint16_t u16TconSubBank;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	while (u16RegisterCount--) {
		u32Addr = ((pu8TconTab[u32tabIdx]<<24) + (pu8TconTab[(u32tabIdx+1)]<<16) +
			(pu8TconTab[(u32tabIdx+2)]<<8) + pu8TconTab[(u32tabIdx+3)]) & 0xFFFFFFFF;
		u8Mask  = pu8TconTab[(u32tabIdx+4)] & 0xFF;
		u8Value = pu8TconTab[(u32tabIdx+5)] & 0xFF;

		TCON_INFO("[addr=%06x, msk=%02x, val=%02x]\n", u32Addr, u8Mask, u8Value);

		//Update to support ext register.
		TCON_W1BYTEMSK(u32Addr, u8Value, u8Mask);
		u32tabIdx = u32tabIdx + 6;
	}

	reinit_completion(&g_gpio_comp);

	if (!g_bGpioSetting) {
		g_bGpioSetting = TRUE;
		g_bGpioComActive = TRUE;
		//create _tcon_gpio_setting_handler for gpio toggle control
		g_gpio_worker = kthread_run(_tcon_gpio_setting_handler,
				(void *)&g_gpio_comp,
				"tcon_gpio_setting_thread");

		if (IS_ERR(g_gpio_worker)) {
			TCON_ERROR("create thread failed!(err=%d)\n",
				(int)PTR_ERR(g_gpio_worker));
			g_gpio_worker = NULL;
			_tcon_gpio_setting_handler((void *)&g_gpio_comp);
		} else
			TCON_DEBUG("thread was created.(PID=%d)\n", g_gpio_worker->pid);
	} else
		TCON_ERROR("The latest action is still going on.\n");

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool _tcon_scaler_setting(uint8_t *pu8TconTab, uint16_t u16RegisterCount)
{
	uint32_t u32tabIdx = 0;
	uint32_t u32Addr;
	uint16_t u16Mask;
	uint16_t u16Value;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	while (u16RegisterCount--) {
		u32Addr = ((pu8TconTab[u32tabIdx]<<24) + (pu8TconTab[(u32tabIdx+1)]<<16) +
			(pu8TconTab[(u32tabIdx+2)]<<8) + pu8TconTab[(u32tabIdx+3)]) & 0xFFFFFFFF;
		u16Mask  = pu8TconTab[(u32tabIdx+4)] & 0xFF;
		u16Value = pu8TconTab[(u32tabIdx+5)] & 0xFF;

		TCON_INFO("[addr=%06x, msk=%02x, val=%02x]\n", u32Addr, u16Mask, u16Value);

		TCON_W1BYTEMSK(u32Addr, u16Value, u16Mask);
		u32tabIdx = u32tabIdx + 6;
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool _tcon_mod_setting(uint8_t *pu8TconTab, uint16_t u16RegisterCount)
{
	uint32_t u32tabIdx = 0;
	uint32_t u32Addr;
	uint16_t u16Mask;
	uint16_t u16Value;
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	while (u16RegisterCount--) {
		u32Addr = ((pu8TconTab[u32tabIdx]<<24) + (pu8TconTab[(u32tabIdx+1)]<<16) +
			(pu8TconTab[(u32tabIdx+2)]<<8) + pu8TconTab[(u32tabIdx+3)]) & 0xFFFFFFFF;
		u16Mask  = pu8TconTab[(u32tabIdx+4)] & 0xFF;
		u16Value = pu8TconTab[(u32tabIdx+5)] & 0xFF;

		TCON_INFO("[addr=%06x, msk=%02x, val=%02x]\n", u32Addr, u16Mask, u16Value);

		//Update to support ext register.
		TCON_W1BYTEMSK(u32Addr, u16Value, u16Mask);
		u32tabIdx = u32tabIdx + 6;
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

void _update_tcon_bin_setting(drm_st_pnl_info *pInfo, uint8_t *pu8TconTab)
{
	uint16_t u16Vfreq = 0;

	//u32PanelLinkType: for STI and uboot
	TCON_CHECK_EQUAL_AND_ASSIGN("linkIF", pInfo->linkIF, g_stPnlInfo.u32PanelLinkType);

	if (pInfo->typ_vtt == 0) {
		TCON_ERROR("pInfo->typ_vtt is 0\n");
		return;
	}
	if (pInfo->typ_htt == 0) {
		TCON_ERROR("pInfo->typ_htt is 0\n");
		return;
	}

	/* calculate the current vfreq. */
	u16Vfreq = pInfo->typ_dclk / pInfo->typ_htt / pInfo->typ_vtt;

	TCON_DEBUG("PanelLinkType=%u PanelLinkExtType=%u linkIF=%u Vfreq=%u"
		" typ_dclk=%u typ_htt=%u typ_vtt=%u\n",
			g_stPnlInfo.u32PanelLinkType,
			g_stPnlInfo.u32PanelLinkExtType,
			(uint32_t)pInfo->linkIF,
			u16Vfreq,
			(uint32_t)pInfo->typ_dclk, pInfo->typ_htt, pInfo->typ_vtt);

	TCON_CHECK_EQUAL_AND_ASSIGN("hsync_st", pInfo->hsync_st, g_stPnlInfo.u32HsyncStart);
	TCON_CHECK_EQUAL_AND_ASSIGN("hsync_w", pInfo->hsync_w,
			(g_stPnlInfo.u32HsyncEnd - g_stPnlInfo.u32HsyncStart + 1));
	TCON_CHECK_EQUAL_AND_ASSIGN("vsync_st", pInfo->vsync_st, g_stPnlInfo.u32VsyncStart);
	TCON_CHECK_EQUAL_AND_ASSIGN("vsync_w", pInfo->vsync_w,
			(g_stPnlInfo.u32VsyncEnd - g_stPnlInfo.u32VsyncStart + 1));

	TCON_CHECK_EQUAL_AND_ASSIGN("de_hstart", pInfo->de_hstart, g_stPnlInfo.u32HDEStart);
	TCON_CHECK_EQUAL_AND_ASSIGN("de_vstart", pInfo->de_vstart, g_stPnlInfo.u32VDEStart);
	TCON_CHECK_EQUAL_AND_ASSIGN("de_width", pInfo->de_width,
			(g_stPnlInfo.u32HDEEnd - g_stPnlInfo.u32HDEStart + 1));
	TCON_CHECK_EQUAL_AND_ASSIGN("de_height", pInfo->de_height,
			(g_stPnlInfo.u32VDEEnd - g_stPnlInfo.u32VDEStart + 1));

	// max_htt / min_htt / max_vtt / min_vtt follow dtsi value.
	TCON_CHECK_EQUAL_AND_ASSIGN("typ_htt", pInfo->typ_htt, g_stPnlInfo.u32PanelHTotal);
	TCON_CHECK_EQUAL_AND_ASSIGN("typ_dclk", pInfo->typ_dclk, g_stPnlInfo.u32PanelDCLK);

	if (u16Vfreq == 0) {
		TCON_ERROR("u16Vfreq is 0\n");
		return;
	}
	if (g_stPnlInfo.u32PanelVTotal == 0) {
		TCON_ERROR("g_stPnlInfo.u32PanelVTotal is 0\n");
		return;
	}

	/* calculate new vtt */
	pInfo->typ_vtt = pInfo->typ_dclk / pInfo->typ_htt / u16Vfreq;
	pInfo->max_vtt = (__u16)((__u32)pInfo->typ_vtt * g_stPnlInfo.u32PanelMaxVTotal
				/ g_stPnlInfo.u32PanelVTotal);
	pInfo->min_vtt = (__u16)((__u32)pInfo->typ_vtt * g_stPnlInfo.u32PanelMinVTotal
				/ g_stPnlInfo.u32PanelVTotal);
	//TCON_CHECK_EQUAL_AND_ASSIGN(pInfo->typ_vtt, g_stPnlInfo.u32PanelVTotal);

	TCON_DEBUG(
		"Vtt=%u->%u Vtt range=%d ~ %d dclk range=%d ~ %d\n",
		g_stPnlInfo.u32PanelVTotal, pInfo->typ_vtt,
		pInfo->min_vtt, pInfo->max_vtt,
		g_stPnlInfo.u32PanelMinDCLK, g_stPnlInfo.u32PanelMaxDCLK);

}

bool _tcon_panelinfo_setting(drm_st_pnl_info *pInfo, uint8_t *pu8TconTab,
				uint32_t u32RegisterlistSize)
{
	uint16_t u16PanelInfoID;
	uint32_t u32PanelInfoValue;
	uint16_t u16Count = 0;
	uint16_t u16MaxRegisterlistSize = u32RegisterlistSize/6;
	bool bRet = TRUE;

	if (!pInfo) {
		TCON_ERROR("pInfo is NULL parameter\n");
		return FALSE;
	}
	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	memset(&g_stPnlInfo, 0, sizeof(ST_TCON_PANELINFO));

	for (u16Count = 0; u16Count < u16MaxRegisterlistSize; u16Count++) {
		u16PanelInfoID = (pu8TconTab[1+6*u16Count]<<8) + (pu8TconTab[6*u16Count]);
		u32PanelInfoValue = (pu8TconTab[5+6*u16Count]<<24) +
					(pu8TconTab[4+6*u16Count]<<16) +
					(pu8TconTab[3+6*u16Count]<<8) + (pu8TconTab[2+6*u16Count]);
		TCON_INFO("u16PanelInfoID=%d u32PanelInfoValue=%d\n",
					u16PanelInfoID, u32PanelInfoValue);

		switch (u16PanelInfoID) {
		case E_PNL_TCON_PANELINFO_PANELLINKTYPE:
			g_stPnlInfo.u32PanelLinkType = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELLINKEXTTYPE:
			g_stPnlInfo.u32PanelLinkExtType = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_FIXEDVABCKPORCH:
			g_stPnlInfo.u32FixedVBackPorch = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_FIXEDHBACKPORCH:
			g_stPnlInfo.u32FixedHBackPorch = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VSYNC_START:
			g_stPnlInfo.u32VsyncStart = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VSYNC_END:
			g_stPnlInfo.u32VsyncEnd = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VDESTART:
			g_stPnlInfo.u32VDEStart = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VDEND:
			g_stPnlInfo.u32VDEEnd = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMAXVTOTAL:
			g_stPnlInfo.u32PanelMaxVTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELVTOTAL:
			g_stPnlInfo.u32PanelVTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_MINVTOTAL:
			g_stPnlInfo.u32PanelMinVTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VDEHEADDUMMY:
			g_stPnlInfo.u32VDEHeadDummy = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VDETAILDUMMY:
			g_stPnlInfo.u32VDETailDummy = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HSYNC_START:
			g_stPnlInfo.u32HsyncStart = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HSYNC_END:
			g_stPnlInfo.u32HsyncEnd = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HDESTART:
			g_stPnlInfo.u32HDEStart = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HDEEND:
			g_stPnlInfo.u32HDEEnd = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMAXHTOTAL:
			g_stPnlInfo.u32PanelMaxHTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELHTOTAL:
			g_stPnlInfo.u32PanelHTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMINHTOTAL:
			g_stPnlInfo.u32PanelMinHTotal = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HDEHEADDUMMY:
			g_stPnlInfo.u32HDEHeadDummy = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_HDETAILDUMMY:
			g_stPnlInfo.u32HDETailDummy = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMAXDCLK:
			g_stPnlInfo.u32PanelMaxDCLK = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELDCLK:
			g_stPnlInfo.u32PanelDCLK = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMINDCLK:
			g_stPnlInfo.u32PanelMinDCLK = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELMAXSET:
			g_stPnlInfo.u32PanelMaxSET = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_MINSET:
			g_stPnlInfo.u32PanelMinSET = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_OUTTIMINGMODE:
			g_stPnlInfo.u32OutTimingMode = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_TOTALPAIR:
			g_stPnlInfo.u32HTotalPair = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PAIREVEN:
			g_stPnlInfo.u32VTotalPair_Even = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PAIRODD:
			g_stPnlInfo.u32VTotalPair_Odd = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_DUALPORT:
			g_stPnlInfo.u32DaulPort = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELHSYNCWIDTH:
			g_stPnlInfo.u32PanelHsyncWidth = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELHSYNCBACKPORCH:
			g_stPnlInfo.u32PanelHsyncBackPorch = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELVSYNCWIDTH:
			g_stPnlInfo.u32PanelVsyncWidth = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_PANELVBACKPORCH:
			g_stPnlInfo.u32PanelVsyncBackPorch = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_SSC_ENABLE:
			g_stPnlInfo.u32SSCEnable = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_SSC_FMODULATION:
			g_stPnlInfo.u32SSC_Fmodulation = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_SSC_PERCENTAGE:
			g_stPnlInfo.u32SSC_Percentage = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_SSC_BIN_CTRL:
			g_stPnlInfo.u32SSC_TconBinCtrl = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_DEMURA_SEL:
			g_stPnlInfo.u32Demura_sel = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_DATA_PATH_SEL:
			g_stPnlInfo.u32DataPath_sel = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_VCOM_SEL:
			g_stPnlInfo.u32Vcom_sel = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_DEMURA_ENABLE:
			g_stPnlInfo.u32DemuraEnable = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_OD_ENABLE:
			g_stPnlInfo.u32OdEnable = u32PanelInfoValue;
			break;
		case E_PNL_TCON_PANELINFO_OD_MODE_TYPE:
			g_stPnlInfo.u32OdModeType = u32PanelInfoValue;
			break;
		}
	}
	pr_crit("[EricDBG][%s] g_stPnlInfo.u32OdModeType = %d", __func__, g_stPnlInfo.u32OdModeType);

	TCON_DEBUG(
		"SSC Ctrl=%d En=%d Fmod=%x Percentage=%x VcomSel=%u DemuraEn=%u Datapath=%u\n",
		g_stPnlInfo.u32SSC_TconBinCtrl, g_stPnlInfo.u32SSCEnable,
		g_stPnlInfo.u32SSC_Fmodulation, g_stPnlInfo.u32SSC_Percentage,
		g_stPnlInfo.u32Vcom_sel, g_stPnlInfo.u32DemuraEnable, g_stPnlInfo.u32DataPath_sel);

	return bRet;
}

int _tcon_wait_gpio_setting_done(struct tcon_powersequence_context *pCon)
{
	unsigned long leave_time;

	if (!pCon) {
		TCON_ERROR("Input parameter is null.\n");
		return -EINVAL;
	}

	if (!g_bGpioComActive) {
		TCON_DEBUG("No need to wait gpio thread completion\n");
		return 0;
	}

	//It will block until gpio setting thread runs complete()
	if (pCon->comp) {
		TCON_DEBUG("Start waiting gpio thread completion\n");

		leave_time = wait_for_completion_timeout(pCon->comp,
				msecs_to_jiffies(TCON_COMP_TIMEOUT));
		TCON_DEBUG("Wait gpio thread completion time=%d ms\n",
				TCON_COMP_TIMEOUT - jiffies_to_msecs(leave_time));

		g_bGpioComActive = FALSE;
	} else {
		msleep(TCON_COMP_TIMEOUT);
		TCON_ERROR("gpio completion is null. Default wait %d ms\n",
			TCON_COMP_TIMEOUT);
	}

	return 0;
}

int _tcon_wait_adl_setting_done(struct tcon_powersequence_context *pCon)
{
	unsigned long leave_time;

	if (!pCon) {
		TCON_ERROR("Input parameter is null.\n");
		return -EINVAL;
	}

	if (!pCon->bOn || !g_bAdlComActive) {
		TCON_DEBUG("No need to wait adl thread completion\n");
		return 0;
	}

	//It will block until adl setting thread runs complete()
	if (pCon->adl_comp) {
		TCON_DEBUG("Start waiting adl thread completion\n");

		leave_time = wait_for_completion_timeout(pCon->adl_comp,
				msecs_to_jiffies(TCON_COMP_TIMEOUT));
		TCON_DEBUG("Wait adl thread completion time=%d ms\n",
				TCON_COMP_TIMEOUT - jiffies_to_msecs(leave_time));

		g_bAdlComActive = FALSE;
	} else {
		msleep(TCON_COMP_TIMEOUT);
		TCON_ERROR("adl completion is null. Default wait %d ms\n",
			TCON_COMP_TIMEOUT);
	}

	return 0;
}

static int _tcon_powerseq_setting_handler(void *data)
{
	uint32_t u32tabIdx = 0;
	uint32_t u32SubAddr;
	uint8_t u8SubMask;
	uint8_t u8SubNum;
	uint32_t u32Addr;
	uint16_t u16Mask;
	uint16_t u16Value;
	uint16_t u16Delay;
	int64_t tCur;
	struct tcon_powersequence_context *pCon = NULL;
	uint8_t *pu8TconTab = NULL;
	uint16_t u16RegisterCount;

	pCon = (struct tcon_powersequence_context *)(data);

	if (!pCon) {
		TCON_ERROR("Input parameter is null.\n");
		return -EINVAL;
	}

	_tcon_wait_gpio_setting_done(pCon);
	_tcon_wait_adl_setting_done(pCon);

	//get current time
	tCur = ktime_get();

	pu8TconTab = pCon->pu8Tab;
	u16RegisterCount = pCon->u16RegCount;

	while (pu8TconTab && u16RegisterCount--) {
		u32SubAddr = ((pu8TconTab[u32tabIdx+2]<<16) +
				(pu8TconTab[(u32tabIdx+1)]<<8) + pu8TconTab[(u32tabIdx)]);
		u8SubMask = pu8TconTab[(u32tabIdx+3)];
		u8SubNum = pu8TconTab[(u32tabIdx+4)];
		u32Addr = ((pu8TconTab[u32tabIdx+7]<<16) +
			(pu8TconTab[(u32tabIdx+6)]<<8) + pu8TconTab[(u32tabIdx+5)]);
		u16Mask = ((pu8TconTab[u32tabIdx+9]<<8) + pu8TconTab[(u32tabIdx+8)]);
		u16Value = ((pu8TconTab[u32tabIdx+11]<<8) + pu8TconTab[(u32tabIdx+10)]);
		u16Delay = ((pu8TconTab[u32tabIdx+13]<<8) + pu8TconTab[(u32tabIdx+12)]);
		//u8Reserved = pu8TconTab[(u32tabIdx+14)];

		TCON_INFO("u32SubAddr=%06x u8SubMask=%02x u8SubNum=%02x\n",
				u32SubAddr, u8SubMask, u8SubNum);
		TCON_INFO("u32Addr=%06x u16Mask=%02x u16Value=%02x u8Delay=%d\n",
				u32Addr, u16Mask, u16Value, u16Delay);

		if (u32SubAddr != 0)//Change sub bank
			TCON_W2BYTEMSK(u32SubAddr, u8SubNum, u8SubMask);

		if (u32Addr != 0)
			TCON_W2BYTEMSK(u32Addr, u16Value, u16Mask);

		mdelay(u16Delay);

		u32tabIdx = u32tabIdx + 15;
	}

	TCON_DEBUG("Tcon powersequence setting done. Total time=%lld ms\n",
		ktime_ms_delta(ktime_get(), tCur));

	g_bEnableSetting = FALSE;
	return 0;
}

bool _tcon_powersequence_setting(
	uint8_t *pu8TconTab, uint16_t u16RegisterCount, bool bOn, struct mtk_panel_context *pCon)
{
	bool bRet = TRUE;

	TCON_FUNC_ENTER();

	if (!pu8TconTab) {
		TCON_ERROR("pu8TconTab is NULL parameter\n");
		return FALSE;
	}

	g_power_seq_con.pu8Tab = pu8TconTab;
	g_power_seq_con.u16RegCount = u16RegisterCount;
	g_power_seq_con.bOn = bOn;
	g_power_seq_con.comp = &g_gpio_comp;
	g_power_seq_con.adl_comp = &g_adl_done_comp;

	if (g_out_upd_type == E_MODE_RESUME_TYPE || !bOn) {
		TCON_DEBUG("tcon_powerseq_setting_handler was called directly.\n");
		_tcon_powerseq_setting_handler(&g_power_seq_con);
	} else {
		TCON_DEBUG("upd_type=%d EnableSetting=%d\n", g_out_upd_type, g_bEnableSetting);
		if (!g_bEnableSetting) {
			g_bEnableSetting = TRUE;
			//create _tcon_powersequence_setting_handler for power sequence control
			g_enable_worker = kthread_run(_tcon_powerseq_setting_handler,
					&g_power_seq_con,
					"tcon_powerseq_setting_handler");

			if (IS_ERR(g_enable_worker)) {
				TCON_ERROR("create theard failed!(err=%d)\n",
					(int)PTR_ERR(g_enable_worker));
				g_enable_worker = NULL;
				_tcon_powerseq_setting_handler(&g_power_seq_con);
			} else
				TCON_DEBUG("thread was created.(PID=%d)\n",
					g_enable_worker->pid);
		}
	}

	TCON_FUNC_EXIT(bRet);
	return bRet;
}

bool _tcon_dump_table(struct mtk_panel_context *pCon, uint8_t *pu8Table, uint8_t u8TconType)
{
	bool bRet;
	st_tcon_tab_info stInfo;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}
	if (!pu8Table) {
		TCON_ERROR("pu8Table is NULL parameter\n");
		return FALSE;
	}

	memset(&stInfo, 0, sizeof(st_tcon_tab_info));
	stInfo.pu8Table = pu8Table;
	stInfo.u8TconType = u8TconType;

	bRet = get_tcon_dump_table(&stInfo);

	if (!bRet) {
		TCON_ERROR("get_tcon_dump_table return false\n");
		return bRet;
	}

	switch (u8TconType) {
	case E_TCON_TAB_TYPE_GENERAL:
		bRet = _tcon_general_setting(pCon, stInfo.pu8Table, stInfo.u16RegCount);
		mtk_tcon_recover_isp_cmd_gamma();
		break;
	case E_TCON_TAB_TYPE_GPIO:
		bRet = _tcon_gpio_setting(stInfo.pu8Table, stInfo.u16RegCount);
		break;
	case E_TCON_TAB_TYPE_SCALER:
		bRet = _tcon_scaler_setting(stInfo.pu8Table, stInfo.u16RegCount);
		break;
	case E_TCON_TAB_TYPE_MOD:
		bRet = _tcon_mod_setting(stInfo.pu8Table, stInfo.u16RegCount);
		break;
	case E_TCON_TAB_TYPE_POWER_SEQUENCE_ON:
		if (stInfo.u8Version > TCON20_VERSION)
			bRet = _tcon_powersequence_setting(stInfo.pu8Table, stInfo.u16RegCount,
				u8TconType == E_TCON_TAB_TYPE_POWER_SEQUENCE_ON, pCon);
		mtk_tcon_recover_isp_cmd_gamma();
		break;
	case E_TCON_TAB_TYPE_POWER_SEQUENCE_OFF:
		if (stInfo.u8Version > TCON20_VERSION)
			bRet = _tcon_powersequence_setting(stInfo.pu8Table, stInfo.u16RegCount,
				u8TconType == E_TCON_TAB_TYPE_POWER_SEQUENCE_ON, pCon);
		//_tcon_store_isp_cmd_gamma();
		break;
	case E_TCON_TAB_TYPE_GAMMA:
		break;
	case E_TCON_TAB_TYPE_PANEL_INFO:
		if (stInfo.u8Version > TCON20_VERSION)
			bRet = _tcon_panelinfo_setting(&pCon->info,
					stInfo.pu8Table, stInfo.u32ReglistSize);
		break;
	case E_TCON_TAB_TYPE_OVERDRIVER:
		bRet = mtk_tcon_od_setting(pCon, g_stPnlInfo.u32OdModeType);
		break;
	case E_TCON_TAB_TYPE_VAC_REG:
		if (stInfo.u8Version > TCON20_VERSION)
			bRet = mtk_tcon_vac_reg_setting(stInfo.pu8Table, stInfo.u16RegCount);
		break;
	default:
		TCON_DEBUG("GetTable: unknown tcon type=%u\n", u8TconType);
		bRet = FALSE;
		break;
	}

	return bRet;
}

bool _mtk_tcon_setup(void)
{
	bool bRet = FALSE;

	//log level initialize
	bRet = init_log_level(DEFAULT_TCON_LOG_LEVEL);

	return bRet;
}

bool _tcon_set_riu_addr(void)
{
	bool bRet = TRUE;
	struct device_node *np = NULL;
	uint32_t u32RegAddr = 0x0;
	uint32_t u32Size = 0x0;
	void __iomem *pVaddr = NULL;

	np = of_find_node_by_name(NULL, TCON_NODE);

	if (np) {
		u32RegAddr = get_dts_u32_index(np, TCON_REG, 1);
		u32Size = get_dts_u32_index(np, TCON_REG, 3);
		pVaddr = ioremap(u32RegAddr, u32Size);

		if (pVaddr) {
			TCON_DEBUG("Phy addr add (RegAddr=%X Size=%X)\n", u32RegAddr, u32Size);

			drv_hwreg_common_setRIUaddr(
					(u32RegAddr - 0x1c000000),
					u32Size,
					(uint64_t)pVaddr);
			drv_hwreg_common_setRIUaddr(
					0x00a32400,
					0x200,
					(uint64_t)ioremap(0x1ca32400, 0x200));
		} else
			TCON_ERROR("Find reg index failed, name = '%s'\n", TCON_REG);
	} else
		TCON_ERROR("Find node failed, name = '%s'\n", TCON_NODE);

	return bRet;
}

bool _tcon_check_mode_change(drm_st_pnl_info *pInfo)
{
	EN_TCON_MODE enMode = EN_TCON_MODE_DEFAULT;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	EN_TCON_MODE enPrevMode = get_tcon_mode();

	if (!pInfo) {
		TCON_ERROR("input pInfo is NULL parameter\n");
		return FALSE;
	}

	if (pInfo->dlg_on)
		enMode = EN_TCON_MODE_HFR;
	else if (pInfo->hpc_mode_en)
		enMode = EN_TCON_MODE_HPC;
	else if (g_bGameDirectFramerateMode && pInfo->game_direct_fr_group == 0)
		enMode = EN_TCON_MODE_GAME;

	TCON_DEBUG("dlg_on=%d, hpc_mode_en=%d, game_direct_fr_group=%d\n",
				pInfo->dlg_on,
				pInfo->hpc_mode_en,
				pInfo->game_direct_fr_group);

	/* check mode change */
	g_ModeChange = set_tcon_mode(enMode);

	/* check tcon bin exists or not */
	if (!is_tcon_data_exist(&pdata_buf, &data_len)) {
		set_tcon_mode(EN_TCON_MODE_DEFAULT);
		g_ModeChange = (enPrevMode != get_tcon_mode());
		TCON_DEBUG("Not exist tcon mode(%d) bin. Switch to tcon default mode\n", enMode);
	}

	return g_ModeChange;
}

bool _tcon_reload_panel_info(drm_st_pnl_info *pInfo)
{
	bool bRet = FALSE;
	bool data_exist;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	int64_t tCur;
	st_tcon_tab_info stInfo;

	if (!pInfo) {
		TCON_ERROR("pInfo is NULL parameter\n");
		return FALSE;
	}

	tCur = ktime_get();

	//update panel info from new mode tcon bin
	data_exist = is_tcon_data_exist(&pdata_buf, &data_len);

	if (data_exist) {
		memset(&stInfo, 0, sizeof(st_tcon_tab_info));
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_PANEL_INFO;

		bRet = get_tcon_dump_table(&stInfo);

		if (!bRet) {
			TCON_ERROR("get_tcon_dump_table return false\n");
			return bRet;
		}

		if (stInfo.u8Version > TCON20_VERSION)
			_tcon_panelinfo_setting(pInfo,
					stInfo.pu8Table, stInfo.u32ReglistSize);

		TCON_DEBUG("Mode change(%d) done. TotTime=%lld ms\n",
			get_tcon_mode(), ktime_ms_delta(ktime_get(), tCur));
	} else
		TCON_ERROR("This tcon mode(%d) data buffer is not exist.\n", get_tcon_mode());

	return bRet;
}

const char *_get_tcon_mode_str(void)
{
	EN_TCON_MODE enMode = get_tcon_mode();

	if (enMode == EN_TCON_MODE_DEFAULT)
		return "Default";
	else if (enMode == EN_TCON_MODE_HFR)
		return "High frame rate, ex:240hz";
	else if (enMode == EN_TCON_MODE_HPC)
		return "High pixel colck, ex:144hz";
	else if (enMode == EN_TCON_MODE_GAME)
		return "Direct game (force 60hz)";

	TCON_ERROR("Not support this mode(%d)\n", enMode);
	return "Not support this mode";
}

int _reload_pmic_lsic(struct mtk_panel_context *pCon)
{
	st_pnl_cust_extic_info stExtIcInfo;
	uint32_t u32Val = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	//PMIC & LSIC write
	stExtIcInfo.bTcon_en = pCon->cus.tcon_en;
	stExtIcInfo.u32Vcom_sel =
		(mtk_tcon_get_property(E_TCON_PROP_VCOM_SEL, &u32Val) == 0) ?
		u32Val : 0;
	PNL_Cust_Resume_Setting_ExtIC_PowerOn(&stExtIcInfo);

	return 0;
}

int mtk_tcon_get_lanes_num_by_linktype(
			drm_pnl_link_if enLinktype, uint16_t *pu16Lanes)
{
	uint16_t lanes = 0;

	if (!pu16Lanes) {
		TCON_ERROR("pu16Lanes is NULL parameter\n");
		return -ENODEV;
	}

	if (!g_bPreInitDone) {
		TCON_DEBUG("This is not the tconless model or pre-init first.\n");
		return -ENODEV;
	}

	if ((enLinktype < E_LINK_MINILVDS_1BLK_3PAIR_6BIT) ||
		(enLinktype > E_LINK_USIT_10BIT_16PAIR)) {
		TCON_ERROR("Unknown linktype=%d\n", enLinktype);
		return -ENODEV;
	}

	switch (enLinktype) {
	case E_LINK_EPI28_8BIT_2PAIR_2KCML:
	case E_LINK_EPI28_8BIT_2PAIR_2KLVDS:
		lanes = LANE_NUM_2;
		break;
	case E_LINK_MINILVDS_1BLK_3PAIR_6BIT:
	case E_LINK_MINILVDS_2BLK_3PAIR_6BIT:
	case E_LINK_MINILVDS_1BLK_3PAIR_8BIT:
	case E_LINK_MINILVDS_2BLK_3PAIR_8BIT:
		lanes = LANE_NUM_3;
		break;
	case E_LINK_EPI28_8BIT_4PAIR_2KCML:
	case E_LINK_EPI28_8BIT_4PAIR_2KLVDS:
		lanes = LANE_NUM_4;
		break;
	case E_LINK_MINILVDS_1BLK_6PAIR_6BIT:
	case E_LINK_MINILVDS_2BLK_6PAIR_6BIT:
	case E_LINK_MINILVDS_1BLK_6PAIR_8BIT:
	case E_LINK_MINILVDS_2BLK_6PAIR_8BIT:
	case E_LINK_EPI28_8BIT_6PAIR_2KCML:
	case E_LINK_EPI28_8BIT_6PAIR_4KCML:
	case E_LINK_EPI28_8BIT_6PAIR_2KLVDS:
	case E_LINK_EPI28_8BIT_6PAIR_4KLVDS:
	case E_LINK_CMPI27_8BIT_6PAIR:
	case E_LINK_USIT_8BIT_6PAIR:
	case E_LINK_USIT_10BIT_6PAIR:
	case E_LINK_ISP_8BIT_6PAIR:
	case E_LINK_CHPI_8BIT_6PAIR:
	case E_LINK_CHPI_10BIT_6PAIR:
		lanes = LANE_NUM_6;
		break;
	case E_LINK_EPI28_8BIT_8PAIR_2KCML:
	case E_LINK_EPI28_8BIT_8PAIR_4KCML:
	case E_LINK_EPI28_8BIT_8PAIR_2KLVDS:
	case E_LINK_EPI28_8BIT_8PAIR_4KLVDS:
	case E_LINK_CMPI27_8BIT_8PAIR:
	case E_LINK_CMPI27_10BIT_8PAIR:
	case E_LINK_ISP_8BIT_8PAIR:
	case E_LINK_ISP_10BIT_8PAIR:
	case E_LINK_CHPI_8BIT_8PAIR:
	case E_LINK_CHPI_10BIT_8PAIR:
	case E_LINK_USIT_8BIT_8PAIR:
		lanes = LANE_NUM_8;
		break;
	case E_LINK_EPI28_8BIT_12PAIR_4KCML:
	case E_LINK_EPI24_10BIT_12PAIR_4KCML:
	case E_LINK_EPI28_8BIT_12PAIR_4KLVDS:
	case E_LINK_EPI24_10BIT_12PAIR_4KLVDS:
	case E_LINK_CMPI27_8BIT_12PAIR:
	case E_LINK_CMPI27_10BIT_12PAIR:
	case E_LINK_USIT_8BIT_12PAIR:
	case E_LINK_USIT_10BIT_12PAIR:
	case E_LINK_ISP_8BIT_12PAIR:
	case E_LINK_ISP_8BIT_6X2PAIR:
	case E_LINK_ISP_10BIT_6X2PAIR:
	case E_LINK_ISP_10BIT_12PAIR:
	case E_LINK_CHPI_8BIT_12PAIR:
	case E_LINK_CHPI_8BIT_6X2PAIR:
	case E_LINK_CHPI_10BIT_12PAIR:
		lanes = LANE_NUM_12;
		break;
	case E_LINK_EPI28_8BIT_16PAIR_4KCML:
	case E_LINK_EPI24_10BIT_16PAIR_4KCML:
	case E_LINK_EPI28_8BIT_16PAIR_4KLVDS:
	case E_LINK_EPI24_10BIT_16PAIR_4KLVDS:
	case E_LINK_USIT_8BIT_16PAIR:
	case E_LINK_USIT_10BIT_16PAIR:
		lanes = LANE_NUM_16;
		break;
	default:
		TCON_ERROR("Unknown linktype=%d\n", enLinktype);
		return -ENODEV;
	}

	*pu16Lanes = lanes;
	TCON_DEBUG("linktype=%d lanes=%d\n", enLinktype, lanes);

	return 0;
}

bool mtk_tcon_update_panel_info(drm_st_pnl_info *pInfo)
{
	bool bRet = FALSE;
	bool data_exist;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	int64_t tCur;
	st_tcon_tab_info stInfo;
	EN_TCON_MODE enMode;
	bool bModeChange;

	if (!pInfo) {
		TCON_ERROR("pInfo is NULL parameter\n");
		return FALSE;
	}

	if (!g_bPreInitDone) {
		TCON_DEBUG("This is not the tconless model or pre-init first.\n");
		return FALSE;
	}

	tCur = ktime_get();

	TCON_DEBUG("Start to update the panel(%s) info.\n", pInfo->pnlname);

	/* record the current tcon mode */
	enMode = get_tcon_mode();

	/* check and switch tcon mode */
	_tcon_check_mode_change(pInfo);

	data_exist = is_tcon_data_exist(&pdata_buf, &data_len);

	if (data_exist) {
		memset(&stInfo, 0, sizeof(st_tcon_tab_info));
		stInfo.pu8Table = pdata_buf;
		stInfo.u8TconType = E_TCON_TAB_TYPE_PANEL_INFO;

		bRet = get_tcon_dump_table(&stInfo);

		if (!bRet) {
			TCON_ERROR("get_tcon_dump_table return false\n");
			return bRet;
		}

		if (stInfo.u8Version > TCON20_VERSION) {
			bRet = _tcon_panelinfo_setting(pInfo,
					stInfo.pu8Table, stInfo.u32ReglistSize);
			_update_tcon_bin_setting(pInfo,
					stInfo.pu8Table);
		}
		TCON_DEBUG("Update end. TotTime=%lld ms\n", ktime_ms_delta(ktime_get(), tCur));
	} else
		TCON_ERROR("This tcon mode(%d) data buffer is not exist.\n", get_tcon_mode());

	/* recover the current tcon mode */
	bModeChange = set_tcon_mode(enMode);

	/* recover the current tcon mode panel info */
	if (bModeChange)
		_tcon_reload_panel_info(pInfo);

	return bRet;
}

int mtk_tcon_show_info(
	struct mtk_panel_context *pCon, char *pInfo, const uint32_t u32InfoSize)
{
	int len = 0, totlen = 0;
	uint32_t u32MaxLen = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}

	if (!pInfo || u32InfoSize == 0) {
		TCON_ERROR("pInfo is NULL or info size is zero\n");
		return -EINVAL;
	}

	u32MaxLen = u32InfoSize - 1;

	len = snprintf(pInfo, u32MaxLen,
		"Tcon info:\n"
		"	- Module support = %d\n"
		"	- Mode = %s\n"
		"	- Link type = %u -> cur: %d\n"
		"	- hsync_st = %u -> cur: %u\n"
		"	- hsync_w = %u -> cur: %u\n"
		"	- vsync_st = %u -> cur: %u\n"
		"	- vsync_w = %u -> cur: %u\n"
		"	- de_hstart = %u -> cur: %u\n"
		"	- de_vstart = %u -> cur: %u\n"
		"	- de_width = %u -> cur: %u\n"
		"	- de_height = %u -> cur: %u\n"
		"	- typ_htt = %u -> cur: %u\n"
		"	- typ_dclk = %u -> cur: %u\n"
		"	- max_vtt = %u -> cur: %u\n"
		"	- typ_vtt = %u -> cur: %u\n"
		"	- min_vtt = %u -> cur: %u\n"
		"	- SSC Ctrl = %u\n"
		"	- SSC_En = %u\n"
		"	- SSC_Fmodulation = %u\n"
		"	- SSC_Percentage = %u\n"
		"	- Vcom_sel = %u\n"
		"	- DemuraEnable = %u\n"
		"	- DatPath = %u\n",
		drv_hwreg_render_tcon_is_module_enable(),
		_get_tcon_mode_str(),
		g_stPnlInfo.u32PanelLinkType, (uint32_t)pCon->info.linkIF,
		g_stPnlInfo.u32HsyncStart, pCon->info.hsync_st,
		(g_stPnlInfo.u32HsyncEnd - g_stPnlInfo.u32HsyncStart + 1), pCon->info.hsync_w,
		g_stPnlInfo.u32VsyncStart, pCon->info.vsync_st,
		(g_stPnlInfo.u32VsyncEnd - g_stPnlInfo.u32VsyncStart + 1), pCon->info.vsync_w,
		g_stPnlInfo.u32HDEStart, pCon->info.de_hstart,
		g_stPnlInfo.u32VDEStart, pCon->info.de_vstart,
		(g_stPnlInfo.u32HDEEnd - g_stPnlInfo.u32HDEStart + 1), pCon->info.de_width,
		(g_stPnlInfo.u32VDEEnd - g_stPnlInfo.u32VDEStart + 1), pCon->info.de_height,
		g_stPnlInfo.u32PanelHTotal, pCon->info.typ_htt,
		g_stPnlInfo.u32PanelDCLK, (uint32_t)pCon->info.typ_dclk,
		g_stPnlInfo.u32PanelMaxVTotal, pCon->info.max_vtt,
		g_stPnlInfo.u32PanelVTotal, pCon->info.typ_vtt,
		g_stPnlInfo.u32PanelMinVTotal, pCon->info.min_vtt,
		g_stPnlInfo.u32SSC_TconBinCtrl, g_stPnlInfo.u32SSCEnable,
		g_stPnlInfo.u32SSC_Fmodulation, g_stPnlInfo.u32SSC_Percentage,
		g_stPnlInfo.u32Vcom_sel, g_stPnlInfo.u32DemuraEnable,
		g_stPnlInfo.u32DataPath_sel);

	totlen += len;

	//get tcon common info
	len = get_tcon_common_info(pCon, pInfo + totlen, u32MaxLen - totlen);
	if (len > 0)
		totlen += len;

	//get tcon dga info
	len = mtk_tcon_panelgamma_get_info(pCon, pInfo + totlen, u32MaxLen - totlen);
	if (len > 0)
		totlen += len;

	//get tcon lineod info
	len = mtk_tcon_lineod_get_info(pCon, pInfo + totlen, u32MaxLen - totlen);
	if (len > 0)
		totlen += len;

	//get tcon od info
	len = mtk_tcon_od_get_info(pCon, pInfo + totlen, u32MaxLen - totlen);
	if (len > 0)
		totlen += len;

	//get tcon vac info
	len = mtk_tcon_vac_get_info(pCon, pInfo + totlen, u32MaxLen - totlen);
	if (len > 0)
		totlen += len;

	return ((totlen < 0) || (totlen > u32MaxLen)) ? -EINVAL : totlen;
}

int mtk_tcon_set_cmd(
	struct mtk_panel_context *pCon, enum EN_TCON_CMD_TYPE enType, const uint16_t u16Val)
{
	int ret = 0;

	switch (enType) {
	case E_TCON_CMD_OD_EN:
	case E_TCON_CMD_OD_BYPASS:
		ret = mtk_tcon_od_set_cmd(pCon, enType, u16Val, g_stPnlInfo.u32OdModeType);
		break;
	case E_TCON_CMD_LINEOD_EN:
	case E_TCON_CMD_PCID_EN:
	case E_TCON_CMD_PCID_BYPASS:
		ret = mtk_tcon_lineod_set_cmd(pCon, enType, u16Val);
		break;
	case E_TCON_CMD_VAC_EN:
	case E_TCON_CMD_VAC_BYPASS:
		ret = mtk_tcon_vac_set_cmd(pCon, enType, u16Val);
		break;
	case E_TCON_CMD_DGA_EN:
	case E_TCON_CMD_DGA_BYPASS:
		ret = mtk_tcon_panelgamma_set_cmd(pCon, enType, u16Val);
		break;
	case E_TCON_CMD_DGA_RESET:
		ret = (pCon && pCon->vrr_pga_En) ?
			mtk_vrr_pnlgamma_init(pCon) : mtk_tcon_panelgamma_setting(pCon);

		reinit_completion(&g_adl_done_comp);
		g_bAdlComActive = TRUE;
		wait_tcon_setting_done(pCon, &g_adl_done_comp);
		break;
	case E_TCON_CMD_LOG_LEVEL:
		ret = init_log_level((uint32_t)u16Val);
		break;
	case E_TCON_CMD_ONOFF:
		ret = mtk_tcon_enable(pCon, u16Val);
		break;
	case E_TCON_CMD_PMIC_LSIC_RELOAD:
		ret = _reload_pmic_lsic(pCon);
		break;
	default:
		TCON_ERROR("Unknown type=%d\n", enType);
		return -ENODEV;
	}

	return 0;
}

bool mtk_tcon_preinit(struct mtk_panel_context *pCon)
{
	bool setup_status = FALSE;
	int64_t tCur;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	tCur = ktime_get();
	setup_status = _mtk_tcon_setup();

	//determines whether the tcon module is enabled
	if (!drv_hwreg_render_tcon_is_module_enable()) {
		TCON_ERROR("Tcon module not supported\n");
		return FALSE;
	}

	if (!g_bPreInitDone) {
		g_bGameDirectFramerateMode = pCon->cus.game_direct_framerate_mode;
		init_completion(&g_gpio_comp);
		init_completion(&g_adl_done_comp);
		mutex_init(&mutex_tcon);

		_tcon_set_riu_addr();
		load_tcon_files(pCon);
	}

	g_bPreInitDone = TRUE;

	TCON_DEBUG("tcon preinit panel info done. Setup status=%d TotTime=%lld ms\n",
			setup_status, ktime_ms_delta(ktime_get(), tCur));
	return TRUE;
}

bool mtk_tcon_init(struct mtk_panel_context *pCon)
{
	bool data_exist = FALSE;
	bool bRet = TRUE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	uint8_t u8Version = 0;
	int64_t tCur;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	/* get current time */
	tCur = ktime_get();
	mutex_lock(&mutex_tcon);

	data_exist = is_tcon_data_exist(&pdata_buf, &data_len);

	if (data_exist) {
		if (!get_tcon_version(pdata_buf, &u8Version)) {
			TCON_ERROR("get tcon version return failed\n");
			mutex_unlock(&mutex_tcon);
			return FALSE;
		}

		g_out_upd_type = pCon->out_upd_type;
		TCON_DEBUG("Update type=%d\n", g_out_upd_type);

		bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_GENERAL);
		bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_GPIO);
		bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_SCALER);
		bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_MOD);
		// version 2 new panel table
		//bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_PANEL_INFO);
		bRet &= _tcon_dump_table(pCon, pdata_buf, E_TCON_TAB_TYPE_VAC_REG);

		if (u8Version > TCON20_VERSION)
			bRet &= mtk_tcon_lineod_setting(pCon);
	}

	if (!pCon->vrr_od_En)
		bRet &= mtk_tcon_od_setting(pCon, g_stPnlInfo.u32OdModeType);
	else
		bRet &= mtk_vrr_tcon_init(pCon, g_stPnlInfo.u32OdModeType);

	bRet &= mtk_tcon_vac_setting();

	if (!pCon->vrr_pga_En)
		bRet &= mtk_tcon_panelgamma_setting(pCon);
	else
		bRet &= mtk_vrr_pnlgamma_init(pCon);

	mutex_unlock(&mutex_tcon);

	reinit_completion(&g_adl_done_comp);
	g_bAdlComActive = TRUE;
	wait_tcon_setting_done(pCon, &g_adl_done_comp);

	g_ModeChange = FALSE;

	TCON_DEBUG("Total time=%lld ms\n", ktime_ms_delta(ktime_get(), tCur));

	return bRet;
}

bool mtk_tcon_deinit(struct mtk_panel_context *pCon)
{
	bool bRet = TRUE;
	int64_t tCur;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	/* get current time */
	tCur = ktime_get();
	mutex_lock(&mutex_tcon);

	//disable od
	bRet = (mtk_tcon_set_cmd(pCon, E_TCON_CMD_OD_EN, E_TCON_OD_VAL_OFF) == 0);

	mutex_unlock(&mutex_tcon);
	TCON_DEBUG("Total time=%lld ms\n", ktime_ms_delta(ktime_get(), tCur));

	return bRet;
}

bool mtk_vrr_tcon_init(struct mtk_panel_context *pCon, uint32_t u32OdModeType)
{
	bool bRet = TRUE;
	int64_t tCur;
	unsigned char i = 0;
	uint16_t u16EnableTblCnt = 0;
	uint16_t u16ApplyTblIdx = 0;

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	// count the number of od table enabled
	for (i = 0; i < pCon->VRR_OD_info->u8VRR_OD_table_Total; i++) {
		if (pCon->VRR_OD_info->stVRR_OD_Table[i].bEnable) {
			u16EnableTblCnt++;
			u16ApplyTblIdx = i;
		}
	}

	TCON_DEBUG("Total od table num=%d Apply index=%d\n", u16EnableTblCnt, u16ApplyTblIdx);

	for (i = 0; i < pCon->VRR_OD_info->u8VRR_OD_table_Total; i++) {
		if (pCon->VRR_OD_info->stVRR_OD_Table[i].bEnable) {
			/* get current time */
			tCur = ktime_get();

			bRet &= mtk_tcon_vrr_od_setting(pCon, i, (i == u16ApplyTblIdx), u32OdModeType);

			TCON_DEBUG("Total time=%lld ms\n", ktime_ms_delta(ktime_get(), tCur));
		}
	}

	TCON_FUNC_EXIT(bRet);

	return bRet;
}

bool mtk_tcon_enable(struct mtk_panel_context *pCon, bool bEn)
{
	bool data_exist = FALSE;
	bool bRet = TRUE;
	loff_t data_len = 0;
	unsigned char *pdata_buf = NULL;
	int64_t tCur;

	TCON_FUNC_ENTER();

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	/* get current time */
	tCur = ktime_get();
	mutex_lock(&mutex_tcon);

	//notify power is on/off to od module
	mtk_tcon_od_set_cmd(pCon, E_TCON_CMD_ONOFF, bEn, g_stPnlInfo.u32OdModeType);

	if (bEn) {
		data_exist = is_tcon_power_seq_data_exist(&pdata_buf, &data_len, true);

		if (data_exist) {
			TCON_DEBUG("Use separate power seq. on bin file setting.\n");
			bRet &= _tcon_dump_table(
					pCon, pdata_buf, E_TCON_TAB_TYPE_POWER_SEQUENCE_ON);
		} else {
			TCON_DEBUG("Use power seq. on setting in the tcon bin.\n");
			data_exist = is_tcon_data_exist(&pdata_buf, &data_len);

			if (data_exist)
				bRet &= _tcon_dump_table(
					pCon, pdata_buf, E_TCON_TAB_TYPE_POWER_SEQUENCE_ON);
		}
	} else {
		data_exist = is_tcon_power_seq_data_exist(&pdata_buf, &data_len, false);

		if (data_exist) {
			TCON_DEBUG("Use separate power seq. off bin file setting.\n");
			bRet &= _tcon_dump_table(
					pCon, pdata_buf, E_TCON_TAB_TYPE_POWER_SEQUENCE_OFF);
		} else {
			TCON_DEBUG("Use power seq. off setting in the tcon bin.\n");
			data_exist = is_tcon_data_exist(&pdata_buf, &data_len);

			if (data_exist)
				bRet &= _tcon_dump_table(
					pCon, pdata_buf, E_TCON_TAB_TYPE_POWER_SEQUENCE_OFF);
		}
	}

	g_bEnable = bEn;
	mutex_unlock(&mutex_tcon);

	TCON_DEBUG("En=%d Total time=%lld ms\n", g_bEnable, ktime_ms_delta(ktime_get(), tCur));
	//Since the Tcon bin data cannot be read again after STR resume,
	//the system uses the method that the data read at AC stage will not released.
	//bRet &= release_tcon_resource();

	TCON_FUNC_EXIT(bRet);

	return bRet;
}

bool mtk_tcon_is_mode_change(struct mtk_panel_context *pCon)
{
	bool bChange = FALSE;
	int64_t tCur;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	tCur = ktime_get();

	/* check and switch tcon mode */
	bChange = _tcon_check_mode_change(&pCon->info);

	if (!bChange)
		return bChange;

	//update panel info from new mode tcon bin
	_tcon_reload_panel_info(&pCon->info);

	return bChange;
}

bool mtk_tcon_is_enable(void)
{
	return g_bEnable;
}

int mtk_tcon_vac_enable(bool bEn)
{
	bool bRet = false;

	bRet = drv_hwreg_render_tcon_vac_get_vac_enable();
	TCON_DEBUG("VAC bEn:%d bRet:%d\n", bEn, bRet);

	if (bEn) {
		if (bRet) {
			TCON_DEBUG("VAC already enabled!!\n");
			return 0;
		}
	} else {
		if (!bRet) {
			TCON_DEBUG("VAC already disabled!!\n");
			return 0;
		}
	}

	drv_hwreg_render_tcon_vac_set_vac_enable(bEn);
	TCON_DEBUG("VAC set to %d success!!\n", bEn);

	bRet = drv_hwreg_render_tcon_vac_get_vac_enable();

	if (bEn) {
		if (bRet) {
			TCON_DEBUG("VAC enabled check ok!!\n");
			return 0;
		}
	} else {
		if (!bRet) {
			TCON_DEBUG("VAC disabled check ok!!\n");
			return 0;
		}
	}

	TCON_DEBUG("VAC set to %d Failed!!\n", bEn);
	return -EINVAL;

}


int mtk_tcon_get_property(drm_en_tcon_property_type enType, uint32_t *pu32Val)
{
	int nRet = 0;

	if (!pu32Val) {
		TCON_ERROR("input value is NULL parameter\n");
		return -ENODEV;
	}

	switch (enType) {
	case E_TCON_PROP_MODULE_EN:
		*pu32Val = g_bPreInitDone;
		TCON_DEBUG("Tcon pre-initialized done=%d\n", *pu32Val);
		break;
	case E_TCON_PROP_SSC_EN:
		*pu32Val = g_stPnlInfo.u32SSCEnable;
		break;
	case E_TCON_PROP_SSC_FMODULATION:
		*pu32Val = g_stPnlInfo.u32SSC_Fmodulation;
		break;
	case E_TCON_PROP_SSC_PERCENTAGE:
		*pu32Val = g_stPnlInfo.u32SSC_Percentage;
		break;
	case E_TCON_PROP_SSC_BIN_CTRL:
		*pu32Val = g_stPnlInfo.u32SSC_TconBinCtrl;
		break;
	case E_TCON_PROP_VCOM_SEL:
		*pu32Val = g_stPnlInfo.u32Vcom_sel;
		break;
	case E_TCON_PROP_DEMURA_EN:
		*pu32Val = g_stPnlInfo.u32DemuraEnable;
		break;
	default:
		TCON_ERROR("Invalid type %u!\n", enType);
		nRet = -ENODEV;
		break;
	}

	return nRet;
}

int mtk_tcon_set_pq_sharemem_context(struct mtk_panel_context *pCon)
{
	struct msg_render_pq_update_info updateInfo;
	struct pqu_dd_update_pq_info_param pqupdate;
	struct mtk_tv_drm_metabuf metabuf;
	struct meta_render_pqu_pq_info *pqu_gamma_data;
	int ret;
	bool bPnlGammaEnable;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return -EINVAL;
	}


	if (!pCon->cus.tcon_en)
		return -EINVAL;

	bPnlGammaEnable = drv_hwreg_render_tcon_dga_get_enable();
	TCON_DEBUG("tcon_en=%d pgamma_en=%d\n", pCon->cus.tcon_en, bPnlGammaEnable);

	/* update panel gamma status to pqu share memory */
	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	if (mtk_tv_drm_metabuf_alloc_by_mmap(
		&metabuf, MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA)) {
		TCON_ERROR("get mmap %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_GAMMA);
		return -EINVAL;
	}
	pqu_gamma_data = metabuf.addr;
	pqu_gamma_data->pnlgamma.pnlgamma_enable = bPnlGammaEnable;

	memset(&updateInfo, 0, sizeof(struct msg_render_pq_update_info));
	updateInfo.sharemem_info.address = (uintptr_t)metabuf.addr;
	updateInfo.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	updateInfo.pnlgamma_enable = TRUE;

	memset(&pqupdate, 0, sizeof(struct pqu_dd_update_pq_info_param));
	pqupdate.sharemem_info.address  = metabuf.mmap_info.bus_addr;
	pqupdate.sharemem_info.size = sizeof(struct meta_render_pqu_pq_info);
	pqupdate.pnlgamma_enable = TRUE;

	MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_PQ_FIRE, &updateInfo, &pqupdate);

	ret = mtk_tv_drm_metabuf_free(&metabuf);

	TCON_DEBUG("update drm meta buf done. ret=%d\n", ret);
	return ret;
}

bool mtk_vrr_pnlgamma_init(struct mtk_panel_context *pCon)
{
	bool data_exist = FALSE;
	bool bRet = TRUE;
	loff_t data_len[VRR_OD_TABLE_NUM_MAX];
	unsigned char *pdata_buf[VRR_OD_TABLE_NUM_MAX];
	unsigned char i = 0;

	if (!pCon) {
		TCON_ERROR("pCon is NULL parameter\n");
		return FALSE;
	}

	for (i = 0; i < pCon->VRR_PGA_info->u8VRR_PGA_table_Total; i++) {
		if (pCon->VRR_PGA_info->stVRR_PGA_Table[i].bEnable) {
			data_exist = is_vrr_pnlgamma_data_exist(&pdata_buf[i], &data_len[i], i);
			bRet &= mtk_tcon_vrr_panelgamma_setting(pCon, i);
		}
	}

	if ((pCon->hw_info.pnl_lib_version == VERSION5) && bRet) {
		if (pCon->vrr_pga_En) {
			TCON_DEBUG("VRR Gamma set DGA set HW MODE\n");
			drv_hwreg_render_tcon_dga_set_sw_mode(FALSE);
		} else {
			TCON_DEBUG("Miffy set DGA set SW MODE\n");
			drv_hwreg_render_tcon_dga_set_sw_mode(TRUE);
		}
	}

	return bRet;
}
