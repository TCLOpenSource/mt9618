// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include "../mtk_tv_drm_kms.h"
#include "mtk_tv_drm_oled.h"
#include "mtk_tv_drm_common.h"
#include "mtk_tv_drm_panel_common.h"
#include "mtk_tv_drm_log.h"

#include "mtk_tv_drm_video_pixelshift.h"
#include "hwreg_render_video_pnl.h"
#include "hwreg_render_video_pixelshift.h"
#include "drv_scriptmgt.h"

#define MTK_DRM_MODEL MTK_DRM_MODEL_OLED
#define PIXELSHIFT_SHM_VER500 0x603000
#define PIXELSHIFT_SHM_VER200 0x1000
#define ORBIT_OFFSET 0x02
#define STILL_IMAGE 0x03

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#endif

// Temperature
#define TEMP_ACCURATE 100	// leave 2 decimal point of temperature
#define TEMP_DIV      256	// decimal point temperature has 4 bit shift and then divided by 16

// Compensation
#define OLED_TEST_COMP_OP_TIME  5    /* test mode will fake a 10 seccond compensation monitor */
#define OLED_COMP_MONIT_TIME    5    // check OFFRS_JB_DONE gpio every 5 sec (when needed)

#define DEC_8 (8)
#define HEX_FF (0xFF)

#if !defined(LOBYTE)
#define LOBYTE(w)           ((unsigned char)(w))
#endif
#if !defined(HIBYTE)
#define HIBYTE(w)           ((unsigned char)(((unsigned short)(w) >> DEC_8) & HEX_FF))
#endif

static struct task_struct *thWaitComp;
static enum drm_mtk_oled_event eOLEDEvent = E_OLED_EVENT_NUM;
static void __iomem *u64StillImageVaddr;
static bool bpreStillImage;

static int _mtk_oled_get_memory_info(u32 *addr)
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

static struct mtk_oled_info *_mtk_oled_get_info(struct drm_device *drm_dev)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;

	if (!drm_dev)
		return NULL;

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	pctx_pnl = &pctx->panel_priv;

	return &pctx_pnl->oled_info;
}

unsigned char _mtk_oled_hwi2c_readbytes(unsigned short u16BusNum,
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
	if (!adap) {
		DRM_ERROR("%s No adapter available!\n", __func__);
		return FALSE;
	}
	num_xfer = i2c_transfer(adap, msgs, 2);
	i2c_put_adapter(adap);
	return (num_xfer == 2) ? TRUE : FALSE;
}

unsigned char _mtk_oled_hwi2c_writebytes(unsigned short u16BusNum,
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
		DRM_ERROR("%s No adapter available!\n", __func__);
		return FALSE;
	}
	num_xfer = i2c_transfer(adap, &msg, TRUE);
	i2c_put_adapter(adap);
	kfree(data);
	return (num_xfer == TRUE) ? TRUE : FALSE;
}

static int _mtk_oled_i2c_trans(
	struct mtk_oled_info *oled_info,
	unsigned char *pu8addr, unsigned short u16size, unsigned char *pBuf, bool bRead)
{
	int  s32Ret = 0;
	uint8_t   u8BusID = 0;
	uint8_t   u8SlaveID = 0;
	uint8_t   u8I2C_Mode = 0;

	u8BusID    = oled_info->oled_i2c.channel_id;
	/* shift right 1bit for 8bit slave addressing */
	u8SlaveID  = oled_info->oled_i2c.slave >> 1;
	u8I2C_Mode = oled_info->oled_i2c.i2c_mode;

	DRM_INFO("I2C BusID : %d, SlaveID : 0x%02x, RegAddr : 0x%x, data : 0x%x, data Size:%d\n",
		u8BusID, u8SlaveID, *pu8addr, *pBuf, (int)u16size);

	if (u8I2C_Mode == 1) {	/* hw i2c */
		if (bRead == 1) { /* Read i2c */
			s32Ret = _mtk_oled_hwi2c_readbytes(u8BusID,
				u8SlaveID, 1, pu8addr, u16size, pBuf);
		} else { /* Write i2c */
			s32Ret = _mtk_oled_hwi2c_writebytes(u8BusID,
				u8SlaveID, 1, pu8addr, u16size, pBuf);
		}

		if (s32Ret == 0)
			DRM_ERROR("HWI2C write fail. s32Ret=%d\n", s32Ret);

	} else {
		DRM_ERROR("SWI2C not support, use HWI2C\n");
	}

	return 0;
}

static int _mtk_oled_i2c_set_enableBit(
	struct mtk_oled_info *oled_info,
	uint8_t addr, uint8_t mask, bool bEnable)
{
	uint8_t u8Addr[] = {addr};
	uint8_t u8Data[] = {0};
	int ret = 0;
	bool stub = false;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get OLED test mode\n");

	if (stub) {
		DRM_INFO("OLED i2c set enableBit in stub mode\n");
		return 0;
	}

	ret = _mtk_oled_i2c_trans(oled_info, u8Addr, 1, u8Data, true);
	if (ret < 0) {
		DRM_ERROR("Fail to read OLED I2C slave=0x%X, channel=0x%X, addr=0x%X, val=0x%X\n",
			oled_info->oled_i2c.slave, oled_info->oled_i2c.channel_id,
			u8Addr[0], u8Data[0]);
		return ret;
	}

	if (bEnable)
		u8Data[0] = u8Data[0] | mask;
	else
		u8Data[0] = u8Data[0] & (~mask);

	ret = _mtk_oled_i2c_trans(oled_info, u8Addr, 1, u8Data, false);
	if (ret < 0) {
		DRM_ERROR("Fail to write OLED I2C slave=0x%X, channel=0x%X, addr=0x%X, val=0x%X\n",
			oled_info->oled_i2c.slave, oled_info->oled_i2c.channel_id,
			u8Addr[0], u8Data[0]);
	}

	return ret;
}

static int _mtk_oled_get_temperature(
	struct mtk_oled_info *oled_info,
	struct drm_mtk_tv_oled_temperature *oled_temp)
{
	int ret = 0;
	int i = 0;

	/* [TODO_OLED] check get temperature i2c */
	uint8_t u8Data[] = {oled_info->oled_seq.jb_temp_min, 0,
		oled_info->oled_seq.jb_temp_max, 0};

	ret = _mtk_oled_i2c_trans(oled_info, &(oled_info->oled_i2c.temp_addr),
					ARRAY_SIZE(u8Data), u8Data, true);
	if (ret < 0) {
		DRM_ERROR("Fail to read OLED I2C slave=0x%X, channel=0x%X, addr=0x%X, val=0x%X\n",
			oled_info->oled_i2c.slave, oled_info->oled_i2c.channel_id,
			oled_info->oled_i2c.temp_addr, u8Data[0]);
		return ret;
	}

	// Ex. temp = 25.5C -> data[0] = 25, data[1] = (0.5/16)<<4 = 128
	oled_temp->temp1 = u8Data[i++]*TEMP_ACCURATE;
	oled_temp->temp1 += u8Data[i++]*TEMP_ACCURATE/TEMP_DIV;
	oled_temp->temp2 = u8Data[i++]*TEMP_ACCURATE;
	oled_temp->temp2 += u8Data[i++]*TEMP_ACCURATE/TEMP_DIV;

	DRM_INFO("Get OLED temperature temp1 = %u, temp2 = %u\n",
		oled_temp->temp1, oled_temp->temp2);

	return ret;
}

static void _mtk_oled_get_stillimage_event(struct mtk_tv_kms_context *pctx)
{
	int bStillImage = false;
	uint32_t chip_miu0_bus_base = 0;
	int ret = 0;

	ret = _mtk_oled_get_memory_info(&chip_miu0_bus_base);

	if (ret < 0) {
		DRM_ERROR("Failed to get memory info\n");
		return;
	}

	if ((pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200) ||
	(pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0500)) {
		if ((pctx->ld_priv.u64LDM_phyAddress != 0) &&
		(pctx->ld_priv.u32LDM_mmapsize != 0)) {
			if (u64StillImageVaddr == NULL) {
				u64StillImageVaddr =
				memremap((size_t)(pctx->ld_priv.u64LDM_phyAddress +
					chip_miu0_bus_base),
					pctx->ld_priv.u32LDM_mmapsize,
					MEMREMAP_WB);

				DRM_INFO("[OLED_STILL][%s, %d] LDM phyAddress : 0x%llx\n",
					__func__, __LINE__, pctx->ld_priv.u64LDM_phyAddress);
				DRM_INFO("[OLED_STILL][%s, %d] LDM size : 0x%x\n",
					__func__, __LINE__, pctx->ld_priv.u32LDM_mmapsize);
				DRM_INFO("[OLED_STILL][%s, %d] LDM VirtAddress : 0x%p\n",
					__func__, __LINE__, u64StillImageVaddr);

				if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200)
					memset((u64StillImageVaddr +
						pctx->ld_priv.u32LDM_mmapsize -
						PIXELSHIFT_SHM_VER200 + STILL_IMAGE), 0, 1);
				else if (pctx->panel_priv.hw_info.pnl_lib_version ==
				DRV_PNL_VERSION0500)
					memset((u64StillImageVaddr +
					PIXELSHIFT_SHM_VER500 + STILL_IMAGE), 0, 1);
			} else {
				if (pctx->panel_priv.hw_info.pnl_lib_version == DRV_PNL_VERSION0200)
					bStillImage = *(bool *)(u64StillImageVaddr +
							pctx->ld_priv.u32LDM_mmapsize -
							PIXELSHIFT_SHM_VER200 + STILL_IMAGE);
				else if (pctx->panel_priv.hw_info.pnl_lib_version ==
				DRV_PNL_VERSION0500)
					bStillImage = *(bool *)(u64StillImageVaddr +
							PIXELSHIFT_SHM_VER500 + STILL_IMAGE);

				if ((bStillImage != bpreStillImage) &&
				(eOLEDEvent == E_OLED_EVENT_NUM)) {
					bpreStillImage = bStillImage;
					eOLEDEvent = bStillImage ?
						E_OLED_EVENT_STILL_TRIG : E_OLED_EVENT_STILL_CANCEL;
				}

			}
		}
	}

}

//-----------------------------------------------------------------------------
//  OLED ioctl functions
//-----------------------------------------------------------------------------
int mtk_tv_drm_set_oled_callback_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv)
{
	DRM_INFO("OLED callback not support\n");

	return 0;
}

void mtk_tv_drm_send_oled_callback(
	struct mtk_oled_info *oled_info,
	enum drm_mtk_oled_event eEvent)
{
	// leave those action that always need to do in one place
	switch (eEvent) {
	case E_OLED_EVENT_JB_DONE:
	case E_OLED_EVENT_JB_FAIL:
	case E_OLED_EVENT_OFFRS_DONE:
	case E_OLED_EVENT_OFFRS_FAIL:
		oled_info->oled_status.bComp = false;
		oled_info->oled_status.bOffRs = false;
		oled_info->oled_status.bJb = false;
		break;
	case E_OLED_EVENT_ERR:
	case E_OLED_EVENT_EVDD:
		oled_info->oled_status.bErrDet = true;
		break;
	case E_OLED_EVENT_STILL_TRIG:
	case E_OLED_EVENT_STILL_CANCEL:
		oled_info->oled_status.bStill = !oled_info->oled_status.bStill;
		break;
	default:
		break;
	}

	eOLEDEvent = eEvent;
}

int mtk_tv_drm_get_oled_temperature_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv)
{
	struct mtk_oled_info *oled_info = _mtk_oled_get_info(drm_dev);

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform\n");
		return 0;
	}

	DRM_INFO("OLED Try to read OLED temperature\n");

	// JB also need to check temperature, so break I2C reading from ioctl
	return _mtk_oled_get_temperature(oled_info,
		(struct drm_mtk_tv_oled_temperature *)data);
}

int mtk_tv_drm_set_oled_hdr_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv)
{
	struct mtk_oled_info *oled_info = _mtk_oled_get_info(drm_dev);
	bool bEnable = *((bool *)data);

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform\n");
		return 0;
	}

	DRM_INFO("OLED Try to set OLED HDR = %d\n", bEnable);

	// Althought TCON HDR can be enable any time, but LGD prefer
	// keep SDR luminance be limited.
	return _mtk_oled_i2c_set_enableBit(oled_info, oled_info->oled_i2c.hdr_addr,
		oled_info->oled_i2c.hdr_mask, bEnable);
}

int mtk_tv_drm_get_oled_event_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv)
{
	bool stub = false;
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct mtk_oled_info *oled_info = _mtk_oled_get_info(drm_dev);

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform\n");
		*(int *)data = E_OLED_EVENT_NUM;
		return 0;
	}

	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get OLED test mode\n");

	/* Still image detection*/
	_mtk_oled_get_stillimage_event(pctx);

	/* GPIO Error Detection*/
	if (gpiod_get_value(oled_info->oled_gpio.gpio_err_det) == 1) {
		if (thWaitComp) {
			DRM_ERROR("OLED power abnormal, compensation fail\n");
			oled_info->oled_status.bComp = false;
			kthread_stop(thWaitComp);
			thWaitComp = NULL;
		}
		DRM_ERROR("OLED ERR_DET happened, power off immediately !!\n");
		eOLEDEvent = E_OLED_EVENT_ERR;
	} else if (gpiod_get_value(oled_info->oled_gpio.gpio_evdd_det) == 0) {

	}

	/* GPIO COMP done */
	if ((gpiod_get_value(oled_info->oled_gpio.gpio_comp_done) == 1) &&
		(oled_info->oled_status.bComp == true)) {
		DRM_INFO("Receive OLED OFFRS_JB_DONE signal\n");

		if (thWaitComp) {
			kthread_stop(thWaitComp);
			thWaitComp = NULL;
		}
	}

	*(int *)data = (int)eOLEDEvent;

	if (!stub)
		eOLEDEvent = E_OLED_EVENT_NUM;

	return 0;
}

int mtk_oled_set_luminance(struct mtk_oled_info *oled_info, uint8_t u8Lumin)
{
	int ret = 0;
	uint8_t u8Data[] = {u8Lumin};

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform\n");
		return 0;
	}

	ret = _mtk_oled_i2c_trans(oled_info, &(oled_info->oled_i2c.lumin_addr), 1, u8Data, false);
	if (ret < 0) {
		DRM_INFO("Fail to write OLED I2C slave=0x%X, channel=0x%X, addr=0x%X, val=0x%X\n",
			oled_info->oled_i2c.slave, oled_info->oled_i2c.channel_id,
			oled_info->oled_i2c.lumin_addr, u8Data[0]);
		return -EINVAL;
	}
	DRM_INFO("Set OLED global_luminance_gain = %d\n", u8Lumin);

	return ret;
}


//-----------------------------------------------------------------------------
//  OLED compensation functions
//-----------------------------------------------------------------------------
static int _mtk_oled_check_power(struct mtk_panel_context *pctx_pnl)
{
	struct mtk_oled_info *oled_info = &pctx_pnl->oled_info;
	struct drm_panel *panel = &pctx_pnl->drmPanel;
	struct mtk_panel_context *pStPanelCtx =
		container_of(panel, struct mtk_panel_context, drmPanel);
	uint16_t onrf_timeout = 0;
	bool stub = false;
	int ret = -1;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get OLED test mode\n");

	if (stub || gpiod_get_value(oled_info->oled_gpio.gpio_evdd_det) == 1) {
		DRM_INFO("Skip OLED power on when test mode, or it is already power on\n");
		return 0;
	}

	DRM_INFO("OLED panel is powered off, try power on to run OFF-RS\n");
	// TODO : Wait VCC off/on delay
	gpiod_set_value(pStPanelCtx->gpio_vcc, 1);
	// TODO : VCC > EVDD delay
	gpiod_set_value(pStPanelCtx->gpio_backlight, 1);
	DRM_INFO("OLED panel is powered on\n");

	// OLED I2C cmd needs to wait ON-RF finish a few ms, or it won't be processed
	onrf_timeout = oled_info->oled_seq.onrf_op;
	while (onrf_timeout > 0) {
		if (gpiod_get_value(oled_info->oled_gpio.gpio_evdd_det) == 1) {
			DRM_INFO("OLED ON-RF done, wait %u ms for I2C\n",
				oled_info->oled_seq.onrf_delay);
			msleep(oled_info->oled_seq.onrf_delay);
			ret = 0;
			break;
		}
		msleep(oled_info->oled_seq.mon_delay);
		onrf_timeout -= oled_info->oled_seq.mon_delay;
	}

	return ret;
}

static void _mtk_oled_qsm(struct mtk_panel_context *pctx_pnl)
{
	struct mtk_oled_info *oled_info = &pctx_pnl->oled_info;
	struct drm_panel *panel = &pctx_pnl->drmPanel;
	struct mtk_panel_context *pStPanelCtx =
		container_of(panel, struct mtk_panel_context, drmPanel);
	bool stub = false;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get OLED test mode\n");

	if (thWaitComp) {
		DRM_INFO("Stop waiting OLED compensation done\n");
		oled_info->oled_status.bComp = false;
		kthread_stop(thWaitComp);
		thWaitComp = NULL;
	}

	DRM_INFO("Start OLED QSM to cancel compensation\n");

	// New OLED TCON support QSM to cancel compensation
	// Old TCON just do power off/on to cancel them
	// EVDD _____          ____
	//           |_________|
	// QSM    _________
	//      __|        |_______
	if (!stub && oled_info->oled_seq.qsm_evdd_off > 0) {
		gpiod_set_value(oled_info->oled_gpio.gpio_qsm_en, 1);
		msleep(oled_info->oled_seq.qsm_evdd_off);
		gpiod_set_value(pStPanelCtx->gpio_backlight, 0);
		msleep(oled_info->oled_seq.qsm_off);
		gpiod_set_value(oled_info->oled_gpio.gpio_qsm_en, 0);
		msleep(oled_info->oled_seq.qsm_evdd_on);
		gpiod_set_value(pStPanelCtx->gpio_backlight, 1);
	} else {
		DRM_INFO("OLED test mode do not change GPIO, just do delay\n");
		msleep(oled_info->oled_seq.qsm_evdd_off +
			oled_info->oled_seq.qsm_off +
			oled_info->oled_seq.qsm_evdd_on);
	}
	DRM_INFO("OLED QSM finished\n");
}

static int _mtk_oled_wait_comp(void *data)
{
	struct mtk_oled_info *oled_info = (struct mtk_oled_info *)data;
	uint16_t u16Timeout = 0;
	bool stub = false;

	if (drv_hwreg_common_get_stub(&stub))
		DRM_INFO("Fail to get OLED test mode\n");

	// decide waiting for how long time
	if (stub)
		u16Timeout = OLED_TEST_COMP_OP_TIME;
	else if (oled_info->oled_status.bOffRs)
		u16Timeout = oled_info->oled_seq.offrs_op + OLED_COMP_MONIT_TIME;
	else
		u16Timeout = oled_info->oled_seq.jb_op + OLED_COMP_MONIT_TIME;

	// Start waiting the result
	while (!kthread_should_stop() && u16Timeout > 0) {
		u16Timeout -= OLED_COMP_MONIT_TIME;
		DRM_INFO("OLED compensation time left: %d sec\n", u16Timeout);
		ssleep(OLED_COMP_MONIT_TIME);
	}

	// Seccess
	if (oled_info->oled_status.bComp && (stub || u16Timeout > 0)) {
		DRM_INFO("OLED Copensation success\n");
		if (oled_info->oled_status.bJb)
			mtk_tv_drm_send_oled_callback(oled_info, E_OLED_EVENT_JB_DONE);
		else
			mtk_tv_drm_send_oled_callback(oled_info, E_OLED_EVENT_OFFRS_DONE);
		return 0;
	}

	// Fail
	if (!oled_info->oled_status.bComp)
		DRM_INFO("Compensation has been cancelled\n");
	else
		DRM_INFO("OLED compensation timeout\n");

	if (oled_info->oled_status.bJb)
		mtk_tv_drm_send_oled_callback(oled_info, E_OLED_EVENT_JB_FAIL);
	else
		mtk_tv_drm_send_oled_callback(oled_info, E_OLED_EVENT_OFFRS_FAIL);

	return 0;
}

int mtk_oled_set_offrs(void *data, bool bEnable)
{
	struct mtk_panel_context *pctx_pnl = (struct mtk_panel_context *)data;
	struct mtk_oled_info *oled_info = &pctx_pnl->oled_info;
	static __s64 tStart;
	__s64 tCur, tDiff;
	int ret = 0;

	DRM_INFO("OLED Try to set OLED OFF-RS = %d\n", bEnable);

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform, OFF-RS should not be triggered\n");
		return 0;
	}

	if (bEnable) {
		// Make sure panel is powered on
		ret = _mtk_oled_check_power(pctx_pnl);
		if (ret) {
			DRM_ERROR("OLED power abnormal, fail to run OFF-RS\n");
			return ret;
		}

		// Send OFF-RS I2C cmd and start wait it finish
		ret = _mtk_oled_i2c_set_enableBit(oled_info, oled_info->oled_i2c.offrs_addr,
			oled_info->oled_i2c.offrs_mask, true);
		if (ret == 0) {
			tStart = ktime_get();
			oled_info->oled_status.bComp = true;
			oled_info->oled_status.bOffRs = true;
			thWaitComp = kthread_run(_mtk_oled_wait_comp, oled_info, "oled_comp_wait");
			if (!thWaitComp) {
				DRM_ERROR("Fail to start waiting OLED compensation\n");
				return -ECOMM;
			}
			DRM_INFO("OLED OFF-RS is triggered, wait it finish\n");
		}
	} else {
		// QSM can only run after compensation start more than 3 sec
		if (oled_info->oled_seq.qsm_on > 0) {
			tCur = ktime_get();
			tDiff = oled_info->oled_seq.qsm_on - ktime_ms_delta(tCur, tStart);
			if (tDiff > 0)
				msleep(tDiff);
		}

		_mtk_oled_qsm(pctx_pnl);
	}

	return ret;
}

int mtk_oled_set_jb(void *data, bool bEnable)
{
	struct mtk_panel_context *pctx_pnl = (struct mtk_panel_context *)data;
	struct mtk_oled_info *oled_info = &pctx_pnl->oled_info;
	static __s64 tStart;
	__s64 tCur, tDiff;
	int ret = 0;

	DRM_INFO("OLED Try to set OLED JB = %d\n", bEnable);

	// Check OLED condition
	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform, JB should not be triggered\n");
		return 0;
	}

	if (bEnable) {
		// Make sure panel is powered on
		ret = _mtk_oled_check_power(pctx_pnl);
		if (ret) {
			DRM_ERROR("OLED power abnormal, fail to run JB\n");
			return ret;
		}

		// Send JB I2C cmd and start wait it finish
		ret = _mtk_oled_i2c_set_enableBit(oled_info, oled_info->oled_i2c.jb_addr,
			oled_info->oled_i2c.jb_mask, true);
		if (ret == 0) {
			tStart = ktime_get();
			oled_info->oled_status.bComp = true;
			oled_info->oled_status.bJb = true;
			thWaitComp = kthread_run(_mtk_oled_wait_comp, oled_info, "oled_comp_wait");
			if (!thWaitComp) {
				DRM_ERROR("Fail to start waiting OLED compensation\n");
				return -ECOMM;
			}
			DRM_INFO("OLED JB is triggered, wait it finish\n");
		}

		// Old OLED TCON need to send JB disable cmd "during enable sequence"
		if (oled_info->oled_seq.jb_on_off > 0) {
			msleep(oled_info->oled_seq.jb_on_off);
			if (_mtk_oled_i2c_set_enableBit(oled_info, oled_info->oled_i2c.jb_addr,
				oled_info->oled_i2c.jb_mask, false))
				DRM_INFO("Fail to send JB disable cmd, but still wait it run\n");
		}
	} else {
		// QSM can only run after compensation start more than 3 sec
		tCur = ktime_get();
		tDiff = oled_info->oled_seq.qsm_on - ktime_ms_delta(tCur, tStart);
		if (tDiff > 0)
			msleep(tDiff);

		_mtk_oled_qsm(pctx_pnl);
	}

	return 0;
}

//-----------------------------------------------------------------------------
//  OLED Init related functions
//-----------------------------------------------------------------------------
static int _parse_panel_oled_gpio(
	struct mtk_oled_info *oled_info,
	struct platform_device *pdev)
{
	int ret = 0;

	if (!oled_info) {
		DRM_ERROR("[%s, %d]: oled_info null ptr\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!oled_info->oled_support) {
		DRM_INFO("Not OLED platform, skip GPIO parsing\n");
		return 0;
	}

	oled_info->oled_gpio.gpio_err_det =
		devm_gpiod_get(&pdev->dev, "oled_err_det", GPIOD_IN);
	oled_info->oled_gpio.gpio_evdd_det =
		devm_gpiod_get(&pdev->dev, "oled_evdd_det", GPIOD_IN);
	oled_info->oled_gpio.gpio_on_rf =
		devm_gpiod_get(&pdev->dev, "oled_onrf", GPIOD_IN);
	oled_info->oled_gpio.gpio_qsm_en =
		devm_gpiod_get(&pdev->dev, "oled_qsm_en", GPIOD_OUT_LOW);
	oled_info->oled_gpio.gpio_comp_done =
		devm_gpiod_get(&pdev->dev, "oled_comp_done", GPIOD_IN);

	if (IS_ERR(oled_info->oled_gpio.gpio_err_det))
		DRM_ERROR("Get OLED Err_Det gpio desc failed\n");
	if (IS_ERR(oled_info->oled_gpio.gpio_evdd_det))
		DRM_ERROR("Get OLED EVDD_Det gpio desc failed\n");
	if (IS_ERR(oled_info->oled_gpio.gpio_on_rf))
		DRM_ERROR("Get OLED ON_RF gpio desc failed\n");
	if (IS_ERR(oled_info->oled_gpio.gpio_qsm_en))
		DRM_ERROR("Get OLED QSM_EN gpio desc failed\n");
	if (IS_ERR(oled_info->oled_gpio.gpio_comp_done))
		DRM_ERROR("Get OLED COMP gpio desc failed\n");

	return ret;
}

static void _dump_oled_info(struct mtk_oled_info *oled_info)
{
	// Show OLED I2C & Sequence timing info
	DRM_INFO("================OLED INFO================\n");
	DRM_INFO("support          = %d\n",   oled_info->oled_support);
	DRM_INFO("oled_ip_bypass   = %d\n",   oled_info->oled_ip_bypass_cust);
	DRM_INFO("slave_addr       = 0x%X\n", oled_info->oled_i2c.slave);
	DRM_INFO("channel_id       = 0x%X\n", oled_info->oled_i2c.channel_id);
	DRM_INFO("i2c_mode         = 0x%X\n", oled_info->oled_i2c.i2c_mode);
	DRM_INFO("reg_offset       = 0x%X\n", oled_info->oled_i2c.reg_offset);
	DRM_INFO("lumin_gain_addr  = 0x%X\n", oled_info->oled_i2c.lumin_addr);
	DRM_INFO("temp_addr        = 0x%X\n", oled_info->oled_i2c.temp_addr);
	DRM_INFO("offrs_addr       = 0x%X\n", oled_info->oled_i2c.offrs_addr);
	DRM_INFO("offrs_mask       = 0x%X\n", oled_info->oled_i2c.offrs_mask);
	DRM_INFO("jb_addr          = 0x%X\n", oled_info->oled_i2c.jb_addr);
	DRM_INFO("jb_mask          = 0x%X\n", oled_info->oled_i2c.jb_mask);
	DRM_INFO("hdr_addr         = 0x%X\n", oled_info->oled_i2c.hdr_addr);
	DRM_INFO("hdr_mask         = 0x%X\n", oled_info->oled_i2c.hdr_mask);
	DRM_INFO("lea_addr         = 0x%X\n", oled_info->oled_i2c.lea_addr);
	DRM_INFO("lea_mask         = 0x%X\n", oled_info->oled_i2c.lea_mask);
	DRM_INFO("tpc_addr         = 0x%X\n", oled_info->oled_i2c.tpc_addr);
	DRM_INFO("tpc_mask         = 0x%X\n", oled_info->oled_i2c.tpc_mask);
	DRM_INFO("cpc_addr         = 0x%X\n", oled_info->oled_i2c.cpc_addr);
	DRM_INFO("cpc_mask         = 0x%X\n", oled_info->oled_i2c.cpc_mask);
	DRM_INFO("opt_p0_addr      = 0x%X\n", oled_info->oled_i2c.opt_p0_addr);
	DRM_INFO("opt_start_addr   = 0x%X\n", oled_info->oled_i2c.opt_start_addr);
	DRM_INFO("opt_start_mask   = 0x%X\n", oled_info->oled_i2c.opt_start_mask);
	DRM_INFO("onrf_op          = %u\n",   oled_info->oled_seq.onrf_op);
	DRM_INFO("onrf_delay       = %u\n",   oled_info->oled_seq.onrf_delay);
	DRM_INFO("offrs_op         = %u\n",   oled_info->oled_seq.offrs_op);
	DRM_INFO("jb_op            = %u\n",   oled_info->oled_seq.jb_op);
	DRM_INFO("jb_on_off        = %u\n",   oled_info->oled_seq.jb_on_off);
	DRM_INFO("jb_temp_max      = %u\n",   oled_info->oled_seq.jb_temp_max);
	DRM_INFO("jb_temp_min      = %u\n",   oled_info->oled_seq.jb_temp_min);
	DRM_INFO("jb_cooldown      = %u\n",   oled_info->oled_seq.jb_cooldown);
	DRM_INFO("qsm_on           = %u\n",   oled_info->oled_seq.qsm_on);
	DRM_INFO("qsm_evdd_off     = %u\n",   oled_info->oled_seq.qsm_evdd_off);
	DRM_INFO("qsm_off          = %u\n",   oled_info->oled_seq.qsm_off);
	DRM_INFO("qsm_evdd_on      = %u\n",   oled_info->oled_seq.qsm_evdd_on);
	DRM_INFO("mon_delay        = %u\n",   oled_info->oled_seq.mon_delay);
	DRM_INFO("================OLED INFO END================\n");
}

int mtk_oled_init(void *data, struct platform_device *pdev)
{
	struct mtk_panel_context *pctx_pnl = (struct mtk_panel_context *)data;
	struct mtk_oled_info *oled_info = &pctx_pnl->oled_info;

	if (!oled_info->oled_support) {
		DRM_INFO("[%s, %d] oled_support:%d\n", __func__, __LINE__,
			oled_info->oled_support);
		return 0;
	}

	oled_info->oled_status.bErrDet = false;
	oled_info->oled_status.bComp = false;
	oled_info->oled_status.bOffRs = false;
	oled_info->oled_status.bJb = false;
	oled_info->oled_status.bStill = false;

	// Parse the rest info if OLED supported
	_parse_panel_oled_gpio(oled_info, pdev);
	_dump_oled_info(oled_info);


	DRM_INFO("OLED TCON init done\n");
	return 0;
}

