// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/kobject.h>
#include <linux/string.h>

#include <drm/mtk_tv_drm.h>
#include <linux/pinctrl/consumer.h>
#include "mtk_tv_drm_plane.h"
//#include "mtk_tv_drm_utpa_wrapper.h"
#include "mtk_tv_drm_drv.h"
#include "mtk_tv_drm_connector.h"
#include "mtk_tv_drm_crtc.h"
#include "mtk_tv_drm_ld.h"
#include "mtk_tv_drm_encoder.h"
//#include "mtk_tv_drm_osdp_plane.h"
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_pqu_agent.h"

#include "ext_command_render_if.h"

#include "pqu_msg.h"
#include "pqu_render.h"
#include "apiLDM.h"
#include "drvLDM_sti.h"

//#define MTK_DRM_LD_COMPATIBLE_STR "MTK-drm-ld"
#define MTK_DRM_DTS_LD_SUPPORT_TAG "LDM_SUPPORT"
#define MTK_DRM_DTS_LD_MMAP_ADDR_TAG "LDM_MMAP_ADDR"
#define MTK_DRM_DTS_LD_VERSION_TAG "MX_LDM_VERSION"
#define MTK_DRM_DTS_RENDER_QMAP_VERSION_TAG "MX_RENDER_QMAP_VERSION"
#define MTK_DRM_DTS_LD_UBOOT_SUPPORT_TAG "LED_MSPI_EN"
#define MTK_DRM_DTS_LD_DMA_MSPI_TEST "LDM_DMA_SPI_LTP_TEST"

#define MTK_DRM_DTS_MMAP_INFO_LD_MMAP_ADDR_TAG "MI_DISPOUT_LOCAL_DIMMING"
#define MTK_DRM_DTS_MMAP_INFO_LD_MMAP_ADDR_REG_TAG "reg"
#define MTK_DRM_MODEL MTK_DRM_MODEL_LD

#define BACKLIGHT 100
#define SLEEP_TIME 100
#define SLEEP_TIME_5SEC 5000
#define SLEEP_TIME_RE_INIT 17

#define MMAPINFO_OFFSET_0 0
#define MMAPINFO_OFFSET_1 1
#define MMAPINFO_OFFSET_2 2
#define MMAPINFO_OFFSET_3 3

#define STRING_MAX	(512)
#define VFRQRATIO_1000 (1000)

static struct device *_drm_dev;


static struct pqu_render_ldm_param ldm_param;
static struct pqu_render_ldm_reply_param ldm_reply_param;

static __u8 preLDMSupportType;

enum EN_SYSFS_LDM_CMD g_ldm_cmd_sysfs;
enum EN_LDM_GET_DATA_TYPE g_ldm_dump_data_sysfs;

static char *ld_config_cus_paths[] = {
	"/vendor/tvconfig/bsp/chip/mt5896/ldm/",
	"/vendor/tvconfig/bsp/chip/mt5897/ldm/",
	"/vendor/tvconfig/bsp/chip/mt5896/ldm/",
	"/vendor/tvconfig/bsp/chip/mt5876/ldm/",
	"/vendor/tvconfig/bsp/chip/mt5879/ldm/",
	"/vendor/tvconfig/bsp/chip/mt5873/ldm/",
};

struct LD_CUS_PATH stLD_Cus_Path_real;

__u32 get_dts_u32_ldm(struct device_node *np, const char *name)
{
	__u32 u32Tmp = 0x0;
	int ret;

	if (np == NULL || name == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	ret = of_property_read_u32(np, name, &u32Tmp);
	if (ret != 0x0) {

		DRM_ERROR(
		"[%s, %d]: LDM of_property_read_u32 failed, name = %s \r\n",
		__func__, __LINE__,
		name);
	}

	return u32Tmp;
}

__u32 get_dts_array_u8_ldm(struct device_node *np,
	const char *name,
	__u8 *ret_array,
	int ret_array_length)
{
	int ret, i;
	__u32 *u32TmpArray = NULL;

	if (np == NULL || name == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	if (ret_array == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	u32TmpArray = kmalloc_array(ret_array_length, sizeof(__u32), GFP_KERNEL);
	if (u32TmpArray == NULL) {
		DRM_ERROR("[LDM][%s, %d]: malloc failed \r\n", __func__, __LINE__);
		ret = -ENOMEM;
		kfree(u32TmpArray);
		return ret;
	}
	ret = of_property_read_u32_array(np, name, u32TmpArray, ret_array_length);
	if (ret != 0x0) {
		DRM_ERROR("[LDM][%s, %d]: of_property_read_u32_array failed: %s, ret =%d\n",
		__func__, __LINE__, name, ret);
		kfree(u32TmpArray);
		return ret;
	}
	for (i = 0; i < ret_array_length; i++)
		*(ret_array + i) = u32TmpArray[i];
	kfree(u32TmpArray);
	return 0;
}

__u32 get_dts_array_u16_ldm(struct device_node *np,
	const char *name,
	__u16 *ret_array,
	int ret_array_length)
{
	int ret, i;
	__u32 *u32TmpArray = NULL;

	if (np == NULL || name == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	if (ret_array == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	u32TmpArray = kmalloc_array(ret_array_length, sizeof(__u32), GFP_KERNEL);
	if (u32TmpArray == NULL) {
		DRM_ERROR("[LDM][%s, %d]: malloc failed \r\n", __func__, __LINE__);
		ret = -ENOMEM;
		kfree(u32TmpArray);
		return ret;
	}
	ret = of_property_read_u32_array(np, name, u32TmpArray, ret_array_length);
	if (ret != 0x0) {
		DRM_ERROR("[LDM][%s, %d]: of_property_read_u32_array failed: %s, ret =%d\n",
		__func__, __LINE__, name, ret);
		kfree(u32TmpArray);
		return ret;
	}
	for (i = 0; i < ret_array_length; i++)
		*(ret_array + i) = u32TmpArray[i];
	kfree(u32TmpArray);
	return 0;
}

__u32 get_dts_array_u32_ldm(struct device_node *np,
	const char *name,
	__u32 *ret_array,
	int ret_array_length)
{
	int ret, i;
	__u32 *u32TmpArray = NULL;

	if (np == NULL || name == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	if (ret_array == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return 0;
	}

	u32TmpArray = kmalloc_array(ret_array_length, sizeof(__u32), GFP_KERNEL);
	if (u32TmpArray == NULL) {
		DRM_ERROR("[LDM][%s, %d]: malloc failed \r\n", __func__, __LINE__);
		ret = -ENOMEM;
		kfree(u32TmpArray);
		return ret;
	}
	ret = of_property_read_u32_array(np, name, u32TmpArray, ret_array_length);
	if (ret != 0x0) {
		DRM_ERROR("[LDM][%s, %d]: of_property_read_u32_array failed: %s, ret =%d\n",
		__func__, __LINE__, name, ret);
		kfree(u32TmpArray);
		return ret;
	}
	for (i = 0; i < ret_array_length; i++)
		*(ret_array + i) = u32TmpArray[i];
	kfree(u32TmpArray);
	return 0;
}



void _dump_ldm_as3824_led_device_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]u16Rsense = %d\n", pctx->ld_led_device_as3824_info.u16Rsense);

	for (i = 0; i < Reg_0x01_to_0x0B_NUM; i++)
		DRM_INFO("[LDM]u8AS3824_Reg_0x01_to_0x0B[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x01_to_0x0B[i]);

	for (i = 0; i < Reg_0x0E_to_0x15_NUM; i++)
		DRM_INFO("[LDM]u8AS3824_Reg_0x0E_to_0x15[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0E_to_0x15[i]);

	DRM_INFO("[LDM]u8AS3824_Reg_0x67 = 0x%x\n",
	pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x67);
	DRM_INFO("[LDM]u16AS3824_PWM_Duty_Init = 0x%x\n",
	pctx->ld_led_device_as3824_info.u16AS3824_PWM_Duty_Init);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16AS3824_PWM_Phase_MBR_OFF[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u16AS3824_PWM_Phase_MBR_OFF[i]);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16AS3824_PWM_Phase_MBR_ON[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u16AS3824_PWM_Phase_MBR_ON[i]);

	for (i = 0; i < VDAC_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF[i]);

	for (i = 0; i < VDAC_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON[i]);

	for (i = 0; i < PLL_MULTI_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8AS3824_Reg_0x61_0x62_PLL_multi[%d] = 0x%x\n",
		i, pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x61_0x62_PLL_multi[i]);

	DRM_INFO("[LDM]u8AS3824_BDAC_High_Limit = 0x%x\n",
	pctx->ld_led_device_as3824_info.u8AS3824_BDAC_High_Limit);
	DRM_INFO("[LDM]u8AS3824_BDAC_Low_Limit = 0x%x\n",
	pctx->ld_led_device_as3824_info.u8AS3824_BDAC_Low_Limit);

}

void _dump_ldm_nt50585_led_device_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}


	DRM_INFO("[LDM]E_LD_DEVICE_NT50585\n");

	for (i = 0; i < Reg_0x01_to_0x0B_NUM; i++)
		DRM_INFO("u8NT50585_Reg_0x01_to_0x0B[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x01_to_0x0B[i]);

	for (i = 0; i < Reg_0x1D_to_0x1F_NUM; i++)
		DRM_INFO("[LDM]u8NT50585_Reg_0x1D_to_0x1F[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x1D_to_0x1F[i]);

	DRM_INFO("[LDM]u8NT50585_Reg_0x60 = 0x%x\n",
	pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x60);
	DRM_INFO("[LDM]u8NT50585_Reg_0x68 = 0x%x\n",
	pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x68);
	DRM_INFO("[LDM]u16NT50585_PWM_Duty_Init = 0x%x\n",
	pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Duty_Init);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16NT50585_PWM_Phase_MBR_OFF[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Phase_MBR_OFF[i]);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16NT50585_PWM_Phase_MBR_ON[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Phase_MBR_ON[i]);

	for (i = 0; i < VDAC_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8NT50585_Reg_0x14_0x15_IDAC_MBR_OFF[%d] = 0x%x\n",
		i,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x14_0x15_IDAC_MBR_OFF[i]);

	for (i = 0; i < VDAC_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8NT50585_Reg_0x14_0x15_IDAC_MBR_ON[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x14_0x15_IDAC_MBR_ON[i]);

	for (i = 0; i < PLL_MULTI_BYTE_NUM; i++)
		DRM_INFO("[LDM]u8NT50585_Reg_0x66_0x67_PLL_multi[%d] = 0x%x\n",
		i, pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x66_0x67_PLL_multi[i]);

	DRM_INFO("[LDM]u16NT50585_BDAC_High_Limit = 0x%x\n",
	pctx->ld_led_device_nt50585_info.u16NT50585_BDAC_High_Limit);
	DRM_INFO("[LDM]u16NT50585_BDAC_Low_Limit = 0x%x\n",
	pctx->ld_led_device_nt50585_info.u16NT50585_BDAC_Low_Limit);

}

void _dump_ldm_iw7039_led_device_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]E_LD_DEVICE_IW7039\n");

	for (i = 0; i < Reg_0x00_to_0x1F_NUM; i++)
		DRM_INFO("[LDM]u16IW7039_Reg_0x000_to_0x01F[%d] = 0x%x\n",
		i, pctx->ld_led_device_iw7039_info.u16IW7039_Reg_0x000_to_0x01F[i]);

	DRM_INFO("[LDM]u16IW7039_PWM_Duty_Init = 0x%x\n",
	pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Duty_Init);
	DRM_INFO("[LDM]u16IW7039_ISET_MBR_OFF = 0x%x\n",
	pctx->ld_led_device_iw7039_info.u16IW7039_ISET_MBR_OFF);
	DRM_INFO("[LDM]u16IW7039_ISET_MBR_ON = 0x%x\n",
	pctx->ld_led_device_iw7039_info.u16IW7039_ISET_MBR_ON);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16IW7039_PWM_Phase_MBR_OFF[%d] = 0x%x\n",
		i, pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Phase_MBR_OFF[i]);

	for (i = 0; i < pctx->ld_panel_info.u16LEDHeight; i++)
		DRM_INFO("[LDM]u16IW7039_PWM_Phase_MBR_ON[%d] = 0x%x\n",
		i, pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Phase_MBR_ON[i]);

	DRM_INFO("[LDM]u16IW7039_ISET_High_Limit = 0x%x\n",
	pctx->ld_led_device_iw7039_info.u16IW7039_ISET_High_Limit);
	DRM_INFO("[LDM]u16IW7039_ISET_Low_Limit = 0x%x\n",
	pctx->ld_led_device_iw7039_info.u16IW7039_ISET_Low_Limit);

}

void _dump_ldm_mbi6353_led_device_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]E_LD_DEVICE_MBI6353\n");

	for (i = 0; i < (MBI6353_MASK_NUM * TWO_BYTE); i++)
		DRM_INFO("[LDM]u8MBI6353_Mask[%d] = 0x%x\n",
		i, pctx->ld_led_device_mbi6353_info.u8MBI6353_Mask[i]);
	for (i = 0; i < (MBI6353_CFG_NUM * TWO_BYTE); i++)
		DRM_INFO("[LDM]u8MBI6353_Config[%d] = 0x%x\n",
		i, pctx->ld_led_device_mbi6353_info.u8MBI6353_Config[i]);
	for (i = 0; i < (MBI6353_CFG_NUM * TWO_BYTE); i++)
		DRM_INFO("[LDM]u8MBI6353_Config_1[%d] = 0x%x\n",
		i, pctx->ld_led_device_mbi6353_info.u8MBI6353_Config_1[i]);

	DRM_INFO("[LDM]u16MBI6353_PWM_Duty_Init = 0x%x\n",
	pctx->ld_led_device_mbi6353_info.u16MBI6353_PWM_Duty_Init);

}


void _dump_ldm_known_led_device_info(struct mtk_ld_context *pctx)
{


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

/*LED DEVICE*/
	switch (pctx->ld_led_device_info.eLEDDeviceType) {
	case E_LD_DEVICE_UNSUPPORT:
		DRM_INFO("E_LD_DEVICE_UNSUPPORT\n");
		break;
	case E_LD_DEVICE_AS3824:
		DRM_INFO("E_LD_DEVICE_AS3824\n");
		_dump_ldm_as3824_led_device_info(pctx);
		break;
	case E_LD_DEVICE_NT50585:
		DRM_INFO("E_LD_DEVICE_NT50585\n");
		_dump_ldm_nt50585_led_device_info(pctx);
		break;
	case E_LD_DEVICE_IW7039:
		DRM_INFO("E_LD_DEVICE_IW7039\n");
		_dump_ldm_iw7039_led_device_info(pctx);
		break;
	case E_LD_DEVICE_MCU:
		DRM_INFO("E_LD_DEVICE_MCU\n");
		break;
	case E_LD_DEVICE_CUS:
		DRM_INFO("E_LD_DEVICE_CUS\n");
		break;
	case E_LD_DEVICE_MBI6353:
		DRM_INFO("E_LD_DEVICE_MBI6353\n");
		_dump_ldm_mbi6353_led_device_info(pctx);
		break;
	default:
		DRM_INFO("E_LD_DEVICE_UNSUPPORT.\n");
		break;
	}


}


void _dump_ldm_led_device_info(struct mtk_ld_context *pctx)
{
	int i;
	__u16 u16ExtendData_Count = 0;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

/*LED parameter*/
	DRM_INFO("[LDM]eLEDDeviceType = %d\n",
	pctx->ld_led_device_info.eLEDDeviceType);
	DRM_INFO("[LDM]bLedDevice_VRR_Support = %d\n",
	pctx->ld_led_device_info.bLedDevice_VRR_Support);
	DRM_INFO("[LDM]u8SPIbits = %d\n", pctx->ld_led_device_info.u8SPIbits);

	for (i = 0; i < DUTY_HEADER_NUM; i++)
		DRM_INFO("[LDM]u8DutyHeader[%d] = 0x%x\n",
		i, pctx->ld_led_device_info.u8DutyHeader[i]);

	for (i = 0; i < CURRENT_HEADER_NUM; i++)
		DRM_INFO("[LDM]u8CurrentHeader[%d] = 0x%x\n",
		i, pctx->ld_led_device_info.u8CurrentHeader[i]);

	DRM_INFO("[LDM]u8SpiChannelNum = %d\n",
	pctx->ld_led_device_info.u8SpiChannelNum);
	DRM_INFO("[LDM]u8DutyHeaderByte = %d\n",
	pctx->ld_led_device_info.u8DutyHeaderByte);
	DRM_INFO("[LDM]u8DutyPerDataByte = %d\n",
	pctx->ld_led_device_info.u8DutyPerDataByte);
	DRM_INFO("[LDM]u8DutyCheckSumByte = %d\n",
	pctx->ld_led_device_info.u8DutyCheckSumByte);
	DRM_INFO("[LDM]u8CurrentHeaderByte = %d\n",
	pctx->ld_led_device_info.u8CurrentHeaderByte);
	DRM_INFO("[LDM]u8CurrentPerDataByte = %d\n",
	pctx->ld_led_device_info.u8CurrentPerDataByte);
	DRM_INFO("[LDM]u8CurrentCheckSumByte = %d\n",
	pctx->ld_led_device_info.u8CurrentCheckSumByte);

	for (i = 0; i < DUTY_HEADER_NUM; i++)
		DRM_INFO("[LDM]u8SpiChannelList[%d] = %d\n",
		i, pctx->ld_led_device_info.u8SpiChannelList[i]);

	for (i = 0; i < MSPI_MAX_NUM; i++)
		DRM_INFO("[LDM]u8Device_Num[%d] = %d\n",
		i, pctx->ld_led_device_info.u8Device_Num[i]);

	for (i = 0; i < DMA_INFO_NUM * MSPI_MAX_NUM; i++)
		DRM_INFO("[LDM]u16DutyDramFormat[%d] = %d\n",
		i, pctx->ld_led_device_info.u16DutyDramFormat[i]);

	for (i = 0; i < DMA_INFO_NUM * MSPI_MAX_NUM; i++)
		DRM_INFO("[LDM]u16CurrentDramFormat[%d] = %d\n",
		i, pctx->ld_led_device_info.u16CurrentDramFormat[i]);

	for (i = 0; i < (pctx->ld_led_device_info.u8DutyHeaderByte
	* MSPI_MAX_NUM); i++)
		DRM_INFO("[LDM]u16Mapping_Table_Header_Duty[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_Header_Duty[i]);

	for (i = 0; i < DUTY_HEADER_NUM; i++)
		DRM_INFO("[LDM]u16Mapping_Table_Duty[%d]  %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_Duty[i]);

	for (i = 0; i < (pctx->ld_led_device_info.u8DutyCheckSumByte
	* MSPI_MAX_NUM); i++)
		DRM_INFO("[LDM]u16Mapping_Table_CheckSum_Duty[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_CheckSum_Duty[i]);

	for (i = 0; i < MSPI_MAX_NUM; i++) {
		if (pctx->ld_led_device_info.u8Device_Num[i] > 0){
			u16ExtendData_Count = u16ExtendData_Count+ pctx->ld_led_device_info.u16ExtendDataByte[i];
		}
	}
	DRM_INFO("[LDM] u16ExtendData_Count = %d\n",u16ExtendData_Count);


	for (i = 0; i < u16ExtendData_Count; i++)
		DRM_INFO("[LDM]u16Mapping_Table_ExtendData_Duty[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_ExtendData_Duty[i]);


	for (i = 0; i < (pctx->ld_led_device_info.u8CurrentHeaderByte
	* MSPI_MAX_NUM); i++)
		DRM_INFO("[LDM]u16Mapping_Table_Header_Current[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_Header_Current[i]);

	for (i = 0; i < CURRENT_HEADER_NUM; i++)
		DRM_INFO("[LDM]u16Mapping_Table_Current[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_Current[i]);

	for (i = 0; i < (pctx->ld_led_device_info.u8CurrentCheckSumByte
	* MSPI_MAX_NUM); i++)
		DRM_INFO("[LDM]u16Mapping_Table_CheckSum_Current[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_CheckSum_Current[i]);

	for (i = 0; i < u16ExtendData_Count; i++)
		DRM_INFO("[LDM]u16Mapping_Table_ExtendData_Current[%d] = %d\n",
		i, pctx->ld_led_device_info.u16Mapping_Table_ExtendData_Current[i]);

	for (i = 0; i < MSPI_MAX_NUM; i++)
		DRM_INFO("[LDM]u16ExtendDataByte[%d] = %d\n",
		i, pctx->ld_led_device_info.u16ExtendDataByte[i]);

}

void _dump_ldm_boosting_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	/*Boosting*/
	DRM_INFO("[LDM]u8MaxPowerRatio = %d\n", pctx->ld_boosting_info.u8MaxPowerRatio);

	for (i = 0; i < INIT_OPEN_AREA_NUM; i++)
		DRM_INFO("[LDM]u16OpenAreaDuty_100[%d] = %d\n",
		i, pctx->ld_boosting_info.u16OpenAreaDuty_100[i]);

	for (i = 0; i < INIT_OPEN_AREA_NUM; i++)
		DRM_INFO("[LDM]u16OpenAreaDuty_AnchorPoints[%d] = %d\n",
		i, pctx->ld_boosting_info.u16OpenAreaDuty_AnchorPoints[i]);

	for (i = 0; i < DECAY_WEI_NUM; i++)
		DRM_INFO("[LDM]u16DecayWei[%d] = %d\n",
		i, pctx->ld_boosting_info.u16DecayWei[i]);

	for (i = 0; i < INIT_OPEN_AREA_NUM; i++)
		DRM_INFO("[LDM]u16OpenAreaCurrentAnchorPoints[%d] = %d\n",
		i, pctx->ld_boosting_info.u16OpenAreaCurrentAnchorPoints[i]);

	for (i = 0; i < INIT_OPEN_AREA_NUM; i++)
		DRM_INFO("[LDM]u16OpenAreaCurrent[%d] = %d\n",
		i, pctx->ld_boosting_info.u16OpenAreaCurrent[i]);


}

void _dump_ldm_dma_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]u8LDMAchannel = 0x%x\n", pctx->ld_dma_info.u8LDMAchannel);
	DRM_INFO("[LDM]u8LDMATrimode = 0x%x\n", pctx->ld_dma_info.u8LDMATrimode);
	DRM_INFO("[LDM]u8LDMACheckSumMode = 0x%x\n", pctx->ld_dma_info.u8LDMACheckSumMode);
	DRM_INFO("[LDM]u8cmdlength = 0x%x\n", pctx->ld_dma_info.u8cmdlength);
	DRM_INFO("[LDM]u8BLWidth = 0x%x\n", pctx->ld_dma_info.u8BLWidth);
	DRM_INFO("[LDM]u8BLHeight = 0x%x\n", pctx->ld_dma_info.u8BLHeight);
	DRM_INFO("[LDM]u16LedNum = 0x%x\n", pctx->ld_dma_info.u16LedNum);
	DRM_INFO("[LDM]u16DMAPackLength = %d\n", pctx->ld_dma_info.u16DMAPackLength);
	DRM_INFO("[LDM]u8DataInvert = 0x%x\n", pctx->ld_dma_info.u8DataInvert);
	DRM_INFO("[LDM]u8ExtDataLength = 0x%x\n", pctx->ld_dma_info.u8ExtDataLength);
	DRM_INFO("[LDM]u16BDACNum = %d\n", pctx->ld_dma_info.u16BDACNum);
	DRM_INFO("[LDM]u8BDACcmdlength = 0x%x\n", pctx->ld_dma_info.u8BDACcmdlength);

	DRM_INFO("[LDM]u8Same_tri_for_Vsync_and_DMA = %d\n",
		pctx->ld_dma_info.u8Same_tri_for_Vsync_and_DMA);
	DRM_INFO("[LDM]u16First_trig = 0x%x\n", pctx->ld_dma_info.u16First_trig);

	DRM_INFO("[LDM]bClosed_12bit_duty_mode = 0x%x\n",
		pctx->ld_dma_info.bClosed_12bit_duty_mode);

	for (i = 0; i < MSPI_HEADER_NUM; i++)
		DRM_INFO("[LDM]u16MspiHead[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16MspiHead[i]);

	for (i = 0; i < DMA_DELAY_NUM; i++)
		DRM_INFO("[LDM]u16DMADelay[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16DMADelay[i]);

	for (i = 0; i < EXT_DATA_NUM; i++)
		DRM_INFO("[LDM]u8ExtData[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u8ExtData[i]);

	for (i = 0; i < BDAC_HEADER_NUM; i++)
		DRM_INFO("[LDM]u16BDACHead[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16BDACHead[i]);

	for (i = 0; i < VSYNC_WIDTH_NUM; i++)
		DRM_INFO("[LDM]u16VsyncWidth[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16VsyncWidth[i]);

	for (i = 0; i < PWM_PERIO_NUM; i++)
		DRM_INFO("[LDM]u16PWM0_Period[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM0_Period[i]);

	for (i = 0; i < PWM_DUTY_NUM; i++)
		DRM_INFO("[LDM]u16PWM0_Duty[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM0_Duty[i]);

	for (i = 0; i < PWM_SHIFT_NUM; i++)
		DRM_INFO("[LDM]u16PWM0_Shift[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM0_Shift[i]);

	for (i = 0; i < PWM_PERIO_NUM; i++)
		DRM_INFO("[LDM]u16PWM1_Period[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM1_Period[i]);

	for (i = 0; i < PWM_DUTY_NUM; i++)
		DRM_INFO("[LDM]u16PWM1_Duty[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM1_Duty[i]);

	for (i = 0; i < PWM_SHIFT_NUM; i++)
		DRM_INFO("[LDM]u16PWM1_Shift[%d] = 0x%x\n",
		i, pctx->ld_dma_info.u16PWM1_Shift[i]);


}

void _dump_ldm_mspi_info(struct mtk_ld_context *pctx)
{
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]u8MspiChannel = %d\n", pctx->ld_mspi_info.u8MspiChannel);
	DRM_INFO("[LDM]u8MspiMode = %d\n", pctx->ld_mspi_info.u8MspiMode);
	DRM_INFO("[LDM]u8TrStart = %d\n", pctx->ld_mspi_info.u8TrStart);
	DRM_INFO("[LDM]u8TrEnd = %d\n", pctx->ld_mspi_info.u8TrEnd);
	DRM_INFO("[LDM]u8TB = %d\n", pctx->ld_mspi_info.u8TB);
	DRM_INFO("[LDM]u8TRW = %d\n", pctx->ld_mspi_info.u8TRW);
	DRM_INFO("[LDM]u32MspiClk = %d\n", pctx->ld_mspi_info.u32MspiClk);
	DRM_INFO("[LDM]BClkPolarity = %d\n", pctx->ld_mspi_info.BClkPolarity);
	DRM_INFO("[LDM]BClkPhase = %d\n", pctx->ld_mspi_info.BClkPhase);
	DRM_INFO("[LDM]u32MAXClk = %d\n", pctx->ld_mspi_info.u32MAXClk);
	DRM_INFO("[LDM]u8MspiBuffSizes = 0x%x\n", pctx->ld_mspi_info.u8MspiBuffSizes);

	for (i = 0; i < W_BIT_CONFIG_NUM; i++)
		DRM_INFO("[LDM]u8WBitConfig[%d] = 0x%x\n",
	i, pctx->ld_mspi_info.u8WBitConfig[i]);

	for (i = 0; i < R_BIT_CONFIG_NUM; i++)
		DRM_INFO("[LDM]u8RBitConfig[%d] = 0x%x\n",
		i, pctx->ld_mspi_info.u8RBitConfig[i]);
}

void _dump_ldm_misc_info(struct mtk_ld_context *pctx)
{
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]bLDEn = %d\n", pctx->ld_misc_info.bLDEn);
	DRM_INFO("[LDM]bCompensationEn = %d\n", pctx->ld_misc_info.bCompensationEn);
	DRM_INFO("[LDM]eLD_cus_alg_mode = %d\n", pctx->ld_misc_info.eLD_cus_alg_mode);
	DRM_INFO("[LDM]bOLEDEn = %d\n", pctx->ld_misc_info.bOLEDEn);
	DRM_INFO("[LDM]bOLED_LSP_En = %d\n", pctx->ld_misc_info.bOLED_LSP_En);
	DRM_INFO("[LDM]bOLED_GSP_En = %d\n", pctx->ld_misc_info.bOLED_GSP_En);
	DRM_INFO("[LDM]bOLED_HBP_En = %d\n", pctx->ld_misc_info.bOLED_HBP_En);
	DRM_INFO("[LDM]bOLED_CRP_En = %d\n", pctx->ld_misc_info.bOLED_CRP_En);
	DRM_INFO("[LDM]bOLED_LSPHDR_En = %d\n", pctx->ld_misc_info.bOLED_LSPHDR_En);
	DRM_INFO("[LDM]bOLED_YUVHist_En = %d\n", pctx->ld_misc_info.bOLED_YUVHist_En);
	DRM_INFO("[LDM]bOLED_LGE_GSR_En = %d\n", pctx->ld_misc_info.bOLED_LGE_GSR_En);
	DRM_INFO("[LDM]bOLED_LGE_GSR_HBP_En = %d\n", pctx->ld_misc_info.bOLED_LGE_GSR_HBP_En);
}

void _dump_ldm_panel_info(struct mtk_ld_context *pctx)
{
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("[LDM]u16PanelWidth = %d\n", pctx->ld_panel_info.u16PanelWidth);
	DRM_INFO("[LDM]u16PanelHeight = %d\n", pctx->ld_panel_info.u16PanelHeight);
	DRM_INFO("[LDM]eLEDType = %d\n", pctx->ld_panel_info.eLEDType);
	DRM_INFO("[LDM]u16LDFWidth = %d\n", pctx->ld_panel_info.u16LDFWidth);
	DRM_INFO("[LDM]u16LDFHeight = %d\n", pctx->ld_panel_info.u16LDFHeight);
	DRM_INFO("[LDM]u16LEDWidth = %d\n", pctx->ld_panel_info.u16LEDWidth);
	DRM_INFO("[LDM]u16LEDHeight = %d\n", pctx->ld_panel_info.u16LEDHeight);
	DRM_INFO("[LDM]u8LSFWidth = %d\n", pctx->ld_panel_info.u8LSFWidth);
	DRM_INFO("[LDM]u8LSFHeight = %d\n", pctx->ld_panel_info.u8LSFHeight);
	DRM_INFO("[LDM]u8Edge2D_Local_h = %d\n", pctx->ld_panel_info.u8Edge2D_Local_h);
	DRM_INFO("[LDM]u8Edge2D_Local_v = %d\n", pctx->ld_panel_info.u8Edge2D_Local_v);
	DRM_INFO("[LDM]u8PanelHz = %d\n", pctx->ld_panel_info.u8PanelHz);
	DRM_INFO("[LDM]u8MirrorPanel = %d\n", pctx->ld_panel_info.u8MirrorPanel);
	DRM_INFO("[LDM]bSupport_DV_GD = %d\n", pctx->ld_panel_info.bSupport_DV_GD);
}

static void dump_dts_ld_info(struct mtk_ld_context *pctx)
{


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  fail  !!\n",
		__func__, __LINE__);
		return;
	}

	DRM_INFO("================LDM INFO================\n");
	_dump_ldm_panel_info(pctx);
	_dump_ldm_misc_info(pctx);
	_dump_ldm_mspi_info(pctx);
	_dump_ldm_dma_info(pctx);
	_dump_ldm_boosting_info(pctx);
	_dump_ldm_led_device_info(pctx);
	_dump_ldm_known_led_device_info(pctx);
	DRM_INFO("[LDM]path:%s, real pathU:%s\n", stLD_Cus_Path_real.aCusPath, stLD_Cus_Path_real.aCusPathU);

	DRM_INFO("================LDM INFO END================\n");

}

static int _mtk_ldm_select_pins_to_func(struct mtk_tv_kms_context *pctx, uint8_t val)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *default_state;
	int ret = 0;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pinctrl = devm_pinctrl_get(pctx->dev);
	if (IS_ERR(pinctrl)) {
		pinctrl = NULL;
		DRM_ERROR("[DRM][LDM][%s][%d] Cannot find pinctrl!\n",
		__func__, __LINE__);
		return 1;
	}
	if (pinctrl) {
		if (val == 1)
			default_state = pinctrl_lookup_state(pinctrl, "fun_ldc_tmon_pmux");
		else
			default_state = pinctrl_lookup_state(pinctrl, "fun_ldc_tmon_gpio_pmux");
		if (IS_ERR(default_state)) {
			default_state = NULL;
			DRM_ERROR("[DRM][LDM][%s][%d] Cannot find pinctrl_state!\n",
			__func__, __LINE__);
			return 1;
		}
		if (default_state)
			ret = pinctrl_select_state(pinctrl, default_state);
	}

	return ret;
}
int _mtk_ld_atomic_set_crtc_property_trunk_ldm_sw_ctrl(
	struct mtk_tv_kms_context *pctx,
	struct mtk_ld_context *pctx_ld,
	struct hwregSWSetCtrlIn *stApiLDMSWSetCtrl
)
{
	int ret_api = 0;
	int ret_rtos = 0;

	switch (pctx_ld->u8LDM_Version) {
	case E_LDM_HW_VERSION_1:
	case E_LDM_HW_VERSION_2:
	case E_LDM_HW_VERSION_3:
	{

	if (stApiLDMSWSetCtrl->enLDMSwSetCtrlType == E_LDM_SW_SET_LD_SUPPORT) {
		pctx_ld->u8LDMSupport = stApiLDMSWSetCtrl->u8Value;
		ret_api = MApi_ld_Init_SetLDC(stApiLDMSWSetCtrl->u8Value);

		ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;

		if (bPquEnable)
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

	} else if (stApiLDMSWSetCtrl->enLDMSwSetCtrlType == E_LDM_SW_SET_CUS_PROFILE) {
		ret_api = MApi_ld_Init_UpdateProfile(
					pctx_ld->u64LDM_phyAddress,
					pctx);
	} else {
		ret_api = MApi_ld_Init_SetSwSetCtrl(stApiLDMSWSetCtrl);
	}
	break;
	}
	default:
	{
	if (stApiLDMSWSetCtrl->enLDMSwSetCtrlType == E_LDM_SW_SET_LD_SUPPORT
		&& (stApiLDMSWSetCtrl->u8Value == E_LDM_SUPPORT_R2
			|| stApiLDMSWSetCtrl->u8Value == E_LDM_SUPPORT_TRUNK_PQU
			|| stApiLDMSWSetCtrl->u8Value == E_LDM_UNSUPPORT)) {
		preLDMSupportType = pctx_ld->u8LDMSupport;
		pctx_ld->u8LDMSupport = stApiLDMSWSetCtrl->u8Value;
		ret_api = MApi_ld_Init_SetLDC(stApiLDMSWSetCtrl->u8Value);

		ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;

		if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2
			|| (preLDMSupportType == E_LDM_SUPPORT_R2
				&& pctx_ld->u8LDMSupport == E_LDM_UNSUPPORT)) {
			DRM_INFO(
			"[DRM][LDM][%s][%d] E_LDM_SUPPORT_R2 !!\n",
			__func__,
			__LINE__);
		} else {
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		}
	}
	break;
	}
	}
	return ret_api;
}


int _mtk_ld_atomic_set_crtc_property_trunk(
	struct mtk_tv_kms_context *pctx,
	struct ext_crtc_prop_info *crtc_props,
	struct mtk_ld_context *pctx_ld,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	int ret_api = 0;
	int ret_rtos = 0;
	int first_Globalstr = 0;

	struct drm_property_blob *property_blob;
	struct hwregSWSetCtrlIn stApiSWSetCtrl;
	struct LD_CUS_PATH stLD_Cus_Path;


	switch (prop_index) {
	case E_EXT_CRTC_PROP_LDM_SW_REG_CTRL:
	{
	memset(
	&stApiSWSetCtrl,
	0,
	sizeof(struct hwregSWSetCtrlIn));

	property_blob = drm_property_lookup_blob(property->dev, val);
	if (property_blob == NULL) {
		DRM_INFO("[%s][%d] unknown SW_REG_CTRL = %td!!\n",
			__func__, __LINE__, (ptrdiff_t)val);
	} else {
		memcpy(
		&stApiSWSetCtrl,
		property_blob->data,
		sizeof(struct hwregSWSetCtrlIn));

		ret_api = _mtk_ld_atomic_set_crtc_property_trunk_ldm_sw_ctrl(
					pctx,
					pctx_ld,
					&stApiSWSetCtrl);

		drm_property_blob_put(property_blob);
	}
	break;
	}
	case E_EXT_CRTC_PROP_LDM_STATUS:
	{
	DRM_INFO("[DRM][LDM][%s][%d] set LDM_STATUSe = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		break;
	}
	case E_EXT_CRTC_PROP_LDM_LUMA:
	DRM_INFO("[DRM][LDM][%s][%d] set LDM_LUMA = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		break;
	case E_EXT_CRTC_PROP_LDM_ENABLE:
	{

	DRM_INFO("[DRM][LDM][%s][%d] set LDM_ENABLE or Disable = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
////remove-start trunk ld need refactor
//		if (crtc_props->prop_val[prop_index]) {
//			ret_api = MApi_LDM_Enable();
//			DRM_INFO("[DRM][LDM] set LDM_ENABLE  %d\n", ret_api);
//		} else {
//			ret_api = MApi_LDM_Disable(
//				(__u8)crtc_props->prop_val[prop_index-1]);
//
//			DRM_INFO("[DRM][LDM] set LDM_Disable ,luma=%tx\n",
//				(ptrdiff_t)crtc_props->prop_val[prop_index-1]);
//		}
////remove-end trunk ld need refactor
		break;
	}
	case E_EXT_CRTC_PROP_LDM_STRENGTH:
	{
	DRM_INFO("[DRM][LDM][%s][%d] set LDM_STRENGTH = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
////remove-start trunk ld need refactor
//		ret_api = MApi_LDM_SetStrength(
//			(__u8)crtc_props->prop_val[prop_index]);
////remove-end trunk ld need refactor
		break;
	}
	case E_EXT_CRTC_PROP_LDM_DATA:
	{
////remove-start trunk ld need refactor
//		ST_LDM_GET_DATA stApiLDMData;
//
//
//		memset(
//			&stApiLDMData,
//			0,
//			sizeof(ST_LDM_GET_DATA));
//
//		property_blob = drm_property_lookup_blob(property->dev, val);
//
//		memcpy(
//			&stApiLDMData,
//			property_blob->data,
//			sizeof(ST_LDM_GET_DATA));
//
//		ret_api =  MApi_LDM_GetData(&stApiLDMData);
//
//		memcpy(
//			property_blob->data,
//			&stApiLDMData,
//			sizeof(ST_LDM_GET_DATA));
//
//		DRM_INFO(
//			"[DRM][LDM][%s][%d] set LDM_DATA = %d  %tx	%d %td\n",
//			__func__, __LINE__,
//			stApiLDMData.enDataType,
//			(ptrdiff_t)stApiLDMData.phyAddr,
//			ret_api,
//			(ptrdiff_t)val);
////remove-end trunk ld need refactor

		break;
	}
	case E_EXT_CRTC_PROP_LDM_DEMO_PATTERN:
	{
////remove-start trunk ld need refactor
//		ret_api = MApi_LDM_SetDemoPattern(
//			(__u32)crtc_props->prop_val[prop_index]);
//
//		DRM_INFO(
//			"[DRM][LDM][%s][%d] set DEMO_PATTERN = %td\n",
//			__func__, __LINE__,
//			(ptrdiff_t)crtc_props->prop_val[prop_index]);
////remove-start trunk ld need refactor

		break;
	}
	case E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS:
	{
		ldm_param.u8Seamless_flag =
			crtc_props->prop_val[E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS];
		ldm_param.u8VRR_flag =
			crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];

		if (bPquEnable)
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

		DRM_INFO(
		"[DRM][LDM][%s][%d] UI reply %d %x !!\n",
		__func__,
		__LINE__,
		prop_index,
		ldm_reply_param.ret);

		DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE= %td, %td\n",
			__func__, __LINE__,
			(ptrdiff_t)crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE],
			(ptrdiff_t)crtc_props->prop_val[E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS]);
		break;
	}
	case E_EXT_CRTC_PROP_LDM_INIT:
	{
		memset(
		&stLD_Cus_Path,
		0,
		sizeof(struct LD_CUS_PATH));

		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown LD_CUS_PATH = %td!!\n",
			__func__, __LINE__, (ptrdiff_t)val);
			break;
		}

		memcpy(
		&stLD_Cus_Path,
		property_blob->data,
		property_blob->length);

		drm_property_blob_put(property_blob);

		memcpy(&stLD_Cus_Path_real,&stLD_Cus_Path,sizeof(stLD_Cus_Path));

		DRM_INFO("[DRM][LDM][%s][%d] set LDM_INIT = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;
		if (bPquEnable)
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_version_reply1 %d %x !!\n",
		__func__,
		__LINE__,
		ret_rtos,
		ldm_reply_param.ret);

		switch (pctx_ld->u8LDMSupport) {
		case E_LDM_UNSUPPORT:
			break;
		case E_LDM_SUPPORT_TRUNK_PQU:
		case E_LDM_SUPPORT_TRUNK_FRC:
		case E_LDM_SUPPORT_R2:
			ret_api = MApi_ld_Init(
			pctx, &stLD_Cus_Path);
			DRM_INFO("[DRM][LDM] MApi_ld_Init %d\n", ret_api);
			break;
		case E_LDM_SUPPORT_CUS:
			DRM_INFO("[LDM]LDM support CUS\n");
			break;
		case E_LDM_SUPPORT_BE:
			DRM_INFO("[LDM]LDM support CUS BE\n");
			break;
		default:
			DRM_INFO("[LDM]LDM is not support.\n");
			break;
		}
		/*check init function*/
		if (ret_api != E_LDM_RET_SUCCESS)
			break;

		// set default gd str form DecayWei[4] covert AC on case
		first_Globalstr =pctx_ld->ld_boosting_info.u16DecayWei[4];
		MApi_ld_set_GD_strength(pctx_ld->u8LDM_Version,first_Globalstr);
		printk("[DRM][LDM]Default global strength from dtso  %d\n", first_Globalstr);

		/*send command to diff cpu*/
		ret_api = MApi_ld_Init_to_CPUIF();
		DRM_INFO("[DRM][LDM] MApi_ld_Init_to_CPUIF %d\n", ret_api);
		if (ret_api != E_LDM_RET_SUCCESS)
			break;


		if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
			printk("[LDM]R2 use mailbox\n");
			ret_api = MApi_ld_Init_to_Mailbox();
			if (ret_api != E_LDM_RET_SUCCESS)
				break;
		}

		if (pctx_ld->bDMA_SPI_Test) {
			msleep(SLEEP_TIME_5SEC);

			ret_api = MApi_ld_Init_Check_En(pctx_ld->u8LDM_Version);
			DRM_INFO("[DRM][LDM] MApi_ld_Init_Check_En%d\n", ret_api);
			if (ret_api != E_LDM_RET_SUCCESS)
				break;

			ret_api = MApi_ld_Init_Check_SPI_Num(pctx_ld->u8LDM_Version);
			DRM_INFO("[DRM][LDM] MApi_ld_Init_Check_SPI_Num %d\n", ret_api);
		}
		break;
	}
	case E_EXT_CRTC_PROP_LDM_SUSPEND:
	{
		DRM_INFO("[DRM][LDM][%s][%d] set LDM_SUSPEND = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		ret_api = mtk_ldm_suspend(pctx->dev);
		DRM_INFO("[DRM][LDM] set LDM_SUSPEND  %d\n", ret_api);
		break;
	}
	case E_EXT_CRTC_PROP_LDM_RESUME:
	{
		DRM_INFO("[DRM][LDM][%s][%d] set LDM_RESUME = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		ret_api = mtk_ldm_resume(pctx->dev);
		DRM_INFO("[DRM][LDM] set LDM_RESUME  %d\n", ret_api);
		break;
	}
	case E_EXT_CRTC_PROP_LDM_SUSPEND_RESUME_TEST:
	{
		DRM_INFO("[DRM][LDM][%s][%d] set SUSPEND_RESUME_TEST = %td\n",
		__func__, __LINE__,
		(ptrdiff_t)crtc_props->prop_val[prop_index]);
		ret_api = mtk_ldm_suspend(pctx->dev);
		if (ret_api != E_LDM_RET_SUCCESS)
			DRM_INFO("[DRM][LDM] set SUSPEND_RESUME_TEST %d\n", ret_api);

		msleep(SLEEP_TIME);
		ret_api = mtk_ldm_resume(pctx->dev);
		DRM_INFO("[DRM][LDM] set SUSPEND_RESUME_TEST %d\n", ret_api);
		break;
	}
	default:
		break;
	}
	return ret_api;

}

int _mtk_ld_atomic_set_crtc_property_cus(
	struct mtk_tv_kms_context *pctx,
	struct ext_crtc_prop_info *crtc_props,
	struct mtk_ld_context *pctx_ld,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	int ret_api = 0;
	int ret_rtos = 0;
	struct drm_property_blob *property_blob;
	struct hwregSWSetCtrlIn stApiSWSetCtrl;


	switch (prop_index) {
	case E_EXT_CRTC_PROP_LDM_SW_REG_CTRL:
		{
		memset(
			&stApiSWSetCtrl,
			0,
			sizeof(struct hwregSWSetCtrlIn));

		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown SW_REG_CTRL = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			memcpy(
			&stApiSWSetCtrl,
			property_blob->data,
			sizeof(struct hwregSWSetCtrlIn));
		if (stApiSWSetCtrl.enLDMSwSetCtrlType == E_LDM_SW_SET_LD_SUPPORT) {
			pctx_ld->u8LDMSupport = stApiSWSetCtrl.u8Value;
			ret_api = MApi_ld_Init_SetLDC(stApiSWSetCtrl.u8Value);

			ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;

			if (bPquEnable)
				ret_rtos = pqu_render_ldm(&ldm_param, NULL);
			else
				pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		} else if (stApiSWSetCtrl.enLDMSwSetCtrlType == E_LDM_SW_SET_CUS_PROFILE) {
			ret_api = MApi_ld_Init_UpdateProfile(
						pctx_ld->u64LDM_phyAddress,
						pctx);
		} else if (stApiSWSetCtrl.enLDMSwSetCtrlType == E_LDM_SW_SET_TMON_DEBUG) {
			ret_api = _mtk_ldm_select_pins_to_func(
						pctx,
						stApiSWSetCtrl.u8Value
						);

		} else {
			ret_api = MApi_ld_Init_SetSwSetCtrl(&stApiSWSetCtrl);
		}
			drm_property_blob_put(property_blob);
		}
		break;
	}
	case E_EXT_CRTC_PROP_LDM_STRENGTH:
	case E_EXT_CRTC_PROP_LDM_AUTO_LD:
	case E_EXT_CRTC_PROP_LDM_XTendedRange:
		break;
	case E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS:
	{
		ldm_param.u8Seamless_flag =
			crtc_props->prop_val[E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS];
		ldm_param.u8VRR_flag =
			crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE];

		if (bPquEnable)
			ret_api = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

		DRM_INFO(
		"[DRM][LDM][%s][%d] UI reply %d %x !!\n",
		__func__,
		__LINE__,
		prop_index,
		ldm_reply_param.ret);
		DRM_INFO("[%s][%d] E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE= %td, %td\n",
			__func__, __LINE__,
			(ptrdiff_t)crtc_props->prop_val[E_EXT_CRTC_PROP_SET_FRAMESYNC_MODE],
			(ptrdiff_t)crtc_props->prop_val[E_EXT_CRTC_PROP_LDM_VRR_SEAMLESS]);
		break;
	}
	case E_EXT_CRTC_PROP_MAX:
		DRM_INFO(
		"[DRM][LDM][%s][%d] invalid property!!\n",
		__func__, __LINE__);
		break;
	default:
		break;

	}

	return ret_api;
}

int _mtk_ld_atomic_set_crtc_property_non(
	struct mtk_tv_kms_context *pctx,
	struct ext_crtc_prop_info *crtc_props,
	struct mtk_ld_context *pctx_ld,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	int ret_api = 0;
	int ret_rtos = 0;
	struct drm_property_blob *property_blob;
	struct hwregSWSetCtrlIn stApiSWSetCtrl;


	switch (prop_index) {
	case E_EXT_CRTC_PROP_LDM_SW_REG_CTRL:
	{
		memset(
			&stApiSWSetCtrl,
			0,
			sizeof(struct hwregSWSetCtrlIn));

		property_blob = drm_property_lookup_blob(property->dev, val);
		if (property_blob == NULL) {
			DRM_INFO("[%s][%d] unknown SW_REG_CTRL = %td!!\n",
				__func__, __LINE__, (ptrdiff_t)val);
		} else {
			memcpy(
			&stApiSWSetCtrl,
			property_blob->data,
			sizeof(struct hwregSWSetCtrlIn));

			switch (pctx_ld->u8LDM_Version) {
			case E_LDM_HW_VERSION_1:
			case E_LDM_HW_VERSION_2:
			case E_LDM_HW_VERSION_3:
			{
			if (stApiSWSetCtrl.enLDMSwSetCtrlType == E_LDM_SW_SET_LD_SUPPORT) {
				pctx_ld->u8LDMSupport = stApiSWSetCtrl.u8Value;
				ret_api = MApi_ld_Init_SetLDC(stApiSWSetCtrl.u8Value);

				ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;

				if (bPquEnable)
					ret_rtos = pqu_render_ldm(&ldm_param, NULL);
				else
					pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

			} else if (stApiSWSetCtrl.enLDMSwSetCtrlType
						== E_LDM_SW_SET_CUS_PROFILE) {
				ret_api = MApi_ld_Init_UpdateProfile(
							pctx_ld->u64LDM_phyAddress,
							pctx);
			} else {
				ret_api = MApi_ld_Init_SetSwSetCtrl(&stApiSWSetCtrl);
			}
			break;
			}
			default:
			{
			if (stApiSWSetCtrl.enLDMSwSetCtrlType == E_LDM_SW_SET_LD_SUPPORT
				&& (stApiSWSetCtrl.u8Value == E_LDM_SUPPORT_R2
					|| stApiSWSetCtrl.u8Value == E_LDM_SUPPORT_TRUNK_PQU
					|| stApiSWSetCtrl.u8Value == E_LDM_UNSUPPORT)) {

				pctx_ld->u8LDMSupport = stApiSWSetCtrl.u8Value;
				ret_api = MApi_ld_Init_SetLDC(stApiSWSetCtrl.u8Value);

				ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;

			if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
				DRM_INFO(
				"[DRM][LDM][%s][%d] E_LDM_SUPPORT_R2 !!\n",
				__func__,
				__LINE__);
			} else {
				pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
			}
			}
			break;
			}
			}
				drm_property_blob_put(property_blob);
		}
		break;
	}
	case E_EXT_CRTC_PROP_LDM_AUTO_LD:
		break;
	case E_EXT_CRTC_PROP_LDM_XTendedRange:
		break;
	default:
		break;
	}
	return ret_api;
}

int mtk_ld_atomic_set_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val,
	int prop_index)
{
	// int ret = 0;
	int ret_api = 0;
//	int ret_rtos = 0;
//	struct drm_property_blob *property_blob;
	struct mtk_tv_kms_context *pctx;
	struct ext_crtc_prop_info *crtc_props;
	struct mtk_ld_context *pctx_ld;
//	struct hwregSWSetCtrlIn stApiSWSetCtrl;

	if (crtc == NULL || state == NULL || property == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	crtc_props = pctx->ext_crtc_properties;
	if (crtc_props == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;

	switch (pctx_ld->u8LDMSupport) {
	case E_LDM_UNSUPPORT:
	{
		ret_api = _mtk_ld_atomic_set_crtc_property_non(
			pctx,
			crtc_props,
			pctx_ld,
			property,
			val,
			prop_index);
	break;
	}
	case E_LDM_SUPPORT_TRUNK_PQU:
	case E_LDM_SUPPORT_R2:
	{
		ret_api = _mtk_ld_atomic_set_crtc_property_trunk(
			pctx,
			crtc_props,
			pctx_ld,
			property,
			val,
			prop_index);
	break;
	}
	case E_LDM_SUPPORT_CUS:
	{
		ret_api = _mtk_ld_atomic_set_crtc_property_cus(
			pctx,
			crtc_props,
			pctx_ld,
			property,
			val,
			prop_index);
	break;
	}
	default:
	break;
	}
	return ret_api;
}

int mtk_ld_atomic_get_crtc_property(
	struct mtk_tv_drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val,
	int prop_index)
{
	struct mtk_tv_kms_context *pctx;
	struct ext_crtc_prop_info *crtc_props;
	int ret = 0;
	struct mtk_ld_context *pctx_ld;

	if (crtc == NULL || state == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	if (property == NULL || val == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = (struct mtk_tv_kms_context *)crtc->crtc_private;
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	crtc_props = pctx->ext_crtc_properties;
	if (crtc_props == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_TRUNK_PQU
		|| pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {


		switch (prop_index) {
		case E_EXT_CRTC_PROP_LDM_STATUS:
		{
////remove-start trunk ld need refactor
//		crtc_props->prop_val[prop_index] = MApi_LDM_GetStatus();
//
//		DRM_INFO(
//			"[DRM][LDM][%s][%d] get LDM_STATUS = %td\n",
//			__func__, __LINE__,
//			(ptrdiff_t)crtc_props->prop_val[prop_index]);
////remove-end trunk ld need refactor

		break;
		}
		case E_EXT_CRTC_PROP_MAX:
		DRM_INFO("[DRM][LDM][%s][%d] invalid property!!\n",
			__func__, __LINE__);
		break;
		default:
		break;
		}

		return ret;
	} else if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_CUS) {

		return ret;

	} else {

		return 0;
	}

}

int _parse_ldm_led_device_mbi6353_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm LED DEVICE MBI6353 info
	np = of_find_node_by_name(NULL, LDM_LED_DEVICE_MBI6353_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_led_device_mbi6353_info.u16MBI6353_PWM_Duty_Init
		= get_dts_u32_ldm(np, MBI6353_PWM_Duty_Init_TAG);

		name = MBI6353_MASK_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_mbi6353_info.u8MBI6353_Mask,
		(MBI6353_MASK_NUM * TWO_BYTE));

		name = MBI6353_CFG_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_mbi6353_info.u8MBI6353_Config,
		(MBI6353_CFG_NUM * TWO_BYTE));

		name = MBI6353_CFG_1_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_mbi6353_info.u8MBI6353_Config_1,
		(MBI6353_CFG_NUM * TWO_BYTE));

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_LED_DEVICE_IW7039_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;

}

int _parse_ldm_led_device_iw7039_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm LED DEVICE IW7039 info
	np = of_find_node_by_name(NULL, LDM_LED_DEVICE_IW7039_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Duty_Init
		= get_dts_u32_ldm(np, IW7039_PWM_Duty_Init_TAG);
		pctx->ld_led_device_iw7039_info.u16IW7039_ISET_MBR_OFF
		= get_dts_u32_ldm(np, IW7039_ISET_MBR_OFF_TAG);
		pctx->ld_led_device_iw7039_info.u16IW7039_ISET_MBR_ON
		= get_dts_u32_ldm(np, IW7039_ISET_MBR_ON_TAG);
		pctx->ld_led_device_iw7039_info.u16IW7039_ISET_High_Limit
		= get_dts_u32_ldm(np, IW7039_ISET_High_Limit_TAG);
		pctx->ld_led_device_iw7039_info.u16IW7039_ISET_Low_Limit
		= get_dts_u32_ldm(np, IW7039_ISET_Low_Limit_TAG);

		name = IW7039_Reg_0x000_to_0x01F_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_iw7039_info.u16IW7039_Reg_0x000_to_0x01F,
		Reg_0x00_to_0x1F_NUM);

		name = IW7039_PWM_Phase_MBR_OFF_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Phase_MBR_OFF,
		pctx->ld_panel_info.u16LEDHeight);

		name = IW7039_PWM_Phase_MBR_ON_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_iw7039_info.u16IW7039_PWM_Phase_MBR_ON,
		pctx->ld_panel_info.u16LEDHeight);


	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_LED_DEVICE_IW7039_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;

}

int _parse_ldm_led_device_nt50585_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm LED DEVICE NT50585 info
	np = of_find_node_by_name(NULL, LDM_LED_DEVICE_NT50585_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x60
		= get_dts_u32_ldm(np, NT50585_Reg_0x60_TAG);
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x68
		= get_dts_u32_ldm(np, NT50585_Reg_0x68_TAG);
		pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Duty_Init
		= get_dts_u32_ldm(np, NT50585_PWM_Duty_Init_TAG);
		pctx->ld_led_device_nt50585_info.u16NT50585_BDAC_High_Limit
		= get_dts_u32_ldm(np, NT50585_BDAC_High_Limit_TAG);
		pctx->ld_led_device_nt50585_info.u16NT50585_BDAC_Low_Limit
		= get_dts_u32_ldm(np, NT50585_BDAC_Low_Limit_TAG);

		name = NT50585_Reg_0x01_to_0x0B_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x01_to_0x0B,
		Reg_0x01_to_0x0B_NUM);

		name = NT50585_Reg_0x1D_to_0x1F_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x1D_to_0x1F,
		Reg_0x1D_to_0x1F_NUM);


		name = NT50585_PWM_Phase_MBR_OFF_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Phase_MBR_OFF,
		pctx->ld_panel_info.u16LEDHeight);

		name = NT50585_PWM_Phase_MBR_ON_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u16NT50585_PWM_Phase_MBR_ON,
		pctx->ld_panel_info.u16LEDHeight);

		name = NT50585_Reg_0x14_0x15_IDAC_MBR_OFF_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x14_0x15_IDAC_MBR_OFF,
		VDAC_BYTE_NUM);

		name = NT50585_Reg_0x14_0x15_IDAC_MBR_ON_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x14_0x15_IDAC_MBR_ON,
		VDAC_BYTE_NUM);

		name = NT50585_Reg_0x66_0x67_PLL_multi_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_nt50585_info.u8NT50585_Reg_0x66_0x67_PLL_multi,
		PLL_MULTI_BYTE_NUM);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_LED_DEVICE_NT50585_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;


}

int _parse_ldm_led_device_as3824_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm LED DEVICE AS3824 info
	np = of_find_node_by_name(NULL, LDM_LED_DEVICE_AS3824_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_led_device_as3824_info.u16Rsense
		= get_dts_u32_ldm(np, RSENSE_TAG);
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x67
		= get_dts_u32_ldm(np, AS3824_Reg_0x67_TAG);
		pctx->ld_led_device_as3824_info.u16AS3824_PWM_Duty_Init
		= get_dts_u32_ldm(np, AS3824_PWM_Duty_Init_TAG);
		pctx->ld_led_device_as3824_info.u8AS3824_BDAC_High_Limit
		= get_dts_u32_ldm(np, AS3824_BDAC_High_Limit_TAG);
		pctx->ld_led_device_as3824_info.u8AS3824_BDAC_Low_Limit
		= get_dts_u32_ldm(np, AS3824_BDAC_Low_Limit_TAG);

		name = AS3824_Reg_0x01_to_0x0B_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x01_to_0x0B,
		Reg_0x01_to_0x0B_NUM);

		name = AS3824_Reg_0x0E_to_0x15_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0E_to_0x15,
		Reg_0x0E_to_0x15_NUM);

		name = AS3824_PWM_Phase_MBR_OFF_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_as3824_info.u16AS3824_PWM_Phase_MBR_OFF,
		pctx->ld_panel_info.u16LEDHeight);

		name = AS3824_PWM_Phase_MBR_ON_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_as3824_info.u16AS3824_PWM_Phase_MBR_ON,
		pctx->ld_panel_info.u16LEDHeight);

		name = AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF,
		VDAC_BYTE_NUM);

		name = AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_ON,
		VDAC_BYTE_NUM);

		name = AS3824_Reg_0x61_0x62_PLL_multi_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_as3824_info.u8AS3824_Reg_0x61_0x62_PLL_multi,
		PLL_MULTI_BYTE_NUM);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_LED_DEVICE_AS3824_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;

}

int _parse_ldm_led_device_info(struct mtk_ld_context *pctx)
{
	int ret = 0, i = 0;
	struct device_node *np;
	const char *name;
//	__u32 *u32TmpArray = NULL;
	__u16 u16ExtendData_Count = 0;
	__u16 u16SpiChNum = 0;
	__u16 u16DutyMappingNum = 0;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm LED DEVICE info
	np = of_find_node_by_name(NULL, LDM_LED_DEVICE_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_led_device_info.eLEDDeviceType
		= get_dts_u32_ldm(np, LEDDEVICE_TYPE_TAG);
		pctx->ld_led_device_info.bLedDevice_VRR_Support
		= get_dts_u32_ldm(np, LEDDEVICE_VRR_SUPPORT);
		pctx->ld_led_device_info.u8SPIbits
		= get_dts_u32_ldm(np, SPI_BITS_TAG);
		pctx->ld_led_device_info.u8SpiChannelNum
		= get_dts_u32_ldm(np, SPI_CHANNEL_NUM_TAG);
		pctx->ld_led_device_info.u8DutyHeaderByte
		= get_dts_u32_ldm(np, DUTY_HEADER_BYTE_TAG);
		pctx->ld_led_device_info.u8DutyPerDataByte
		= get_dts_u32_ldm(np, DUTY_PER_DATA_BYTE_TAG);
		pctx->ld_led_device_info.u8DutyCheckSumByte
		= get_dts_u32_ldm(np, DUTY_CHECKSUM_BYTE_TAG);
		pctx->ld_led_device_info.u8CurrentHeaderByte
		= get_dts_u32_ldm(np, CURRENT_HEADER_BYTE_TAG);
		pctx->ld_led_device_info.u8CurrentPerDataByte
		= get_dts_u32_ldm(np, CURRENT_PER_DATA_BYTE_TAG);
		pctx->ld_led_device_info.u8CurrentCheckSumByte
		= get_dts_u32_ldm(np, CURRENT_CHECKSUM_BYTE_TAG);

		name = SPI_CHANNEL_LIST_TAG;

		if (pctx->ld_dma_info.bClosed_12bit_duty_mode == TRUE)
			u16SpiChNum = ((pctx->ld_panel_info.u16LEDWidth * TRIPLE / TWICE)
				* pctx->ld_panel_info.u16LEDHeight);
		else
			u16SpiChNum = (pctx->ld_panel_info.u16LEDWidth
		* pctx->ld_panel_info.u16LEDHeight);

		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_info.u8SpiChannelList,
		u16SpiChNum);

		name = DEVICE_NUM_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_info.u8Device_Num, MSPI_MAX_NUM);

		name = DUTY_DRAM_FORMAT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16DutyDramFormat,
		DMA_INFO_NUM * MSPI_MAX_NUM);

		name = DUTY_HEADER_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_info.u8DutyHeader,
		DUTY_HEADER_NUM);

		name = MAPPING_TABLE_HEADER_DUTY_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_Header_Duty,
		(pctx->ld_led_device_info.u8DutyHeaderByte * MSPI_MAX_NUM));

		name = MAPPING_TABLE_DUTY_TAG;

		if (pctx->ld_dma_info.bClosed_12bit_duty_mode == TRUE)
			u16DutyMappingNum = ((pctx->ld_panel_info.u16LEDWidth * TRIPLE / TWICE)
				* pctx->ld_panel_info.u16LEDHeight);
		else
		{
			if(pctx->ld_led_device_info.eLEDDeviceType == E_LD_DEVICE_TL03)
				u16DutyMappingNum = (pctx->ld_panel_info.u16LEDWidth* (pctx->ld_panel_info.u16LEDHeight + TWICE)
				* pctx->ld_led_device_info.u8DutyPerDataByte);
			else if(pctx->ld_led_device_info.eLEDDeviceType == E_LD_DEVICE_TL04)
				u16DutyMappingNum = (pctx->ld_panel_info.u16LEDWidth* (pctx->ld_panel_info.u16LEDHeight + QUADRUPLE)
				* pctx->ld_led_device_info.u8DutyPerDataByte);
			else if(pctx->ld_led_device_info.eLEDDeviceType == E_LD_DEVICE_TL05)
				u16DutyMappingNum = (pctx->ld_panel_info.u16LEDHeight* (pctx->ld_panel_info.u16LEDWidth + QUADRUPLE)
				* pctx->ld_led_device_info.u8DutyPerDataByte);
			else if(pctx->ld_led_device_info.eLEDDeviceType == E_LD_DEVICE_TL06)
				u16DutyMappingNum = (pctx->ld_panel_info.u16LEDHeight* (pctx->ld_panel_info.u16LEDWidth + TWICE)
				* pctx->ld_led_device_info.u8DutyPerDataByte);
			else if(pctx->ld_led_device_info.eLEDDeviceType == E_LD_DEVICE_TL07)
				u16DutyMappingNum = ((pctx->ld_panel_info.u16LEDWidth+ pctx->ld_boosting_info.u16DecayWei[30])
				* (pctx->ld_panel_info.u16LEDHeight + pctx->ld_boosting_info.u16DecayWei[31]) * pctx->ld_led_device_info.u8DutyPerDataByte);
			else
				u16DutyMappingNum = (pctx->ld_panel_info.u16LEDWidth* pctx->ld_panel_info.u16LEDHeight
				* pctx->ld_led_device_info.u8DutyPerDataByte);
		}
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_Duty,
		u16DutyMappingNum);

		name = MAPPING_TABLE_CHECKSUM_DUTY_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_CheckSum_Duty,
		(pctx->ld_led_device_info.u8DutyCheckSumByte
		* MSPI_MAX_NUM));

		name = EXTEND_DATA_BYTE_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16ExtendDataByte,
		MSPI_MAX_NUM);

		switch (pctx->ld_led_device_info.eLEDDeviceType) {
		case E_LD_DEVICE_AS3824:
		{
			for (i = 0; i < MSPI_MAX_NUM; i++) {
				if (pctx->ld_led_device_info.u8Device_Num[i] == 0)
					u16ExtendData_Count++;
				else
					u16ExtendData_Count = u16ExtendData_Count
					+ pctx->ld_led_device_info.u8Device_Num[i];
			}
			break;
		}
		case E_LD_DEVICE_NT50585:
		{
			for (i = 0; i < MSPI_MAX_NUM; i++) {
				if (pctx->ld_led_device_info.u8Device_Num[i] == 0)
					u16ExtendData_Count++;
				else
					u16ExtendData_Count = u16ExtendData_Count
					+ (pctx->ld_led_device_info.u8Device_Num[i] * TWICE);
			}
			break;
		}
		case E_LD_DEVICE_IW7039:
		{
			for (i = 0; i < MSPI_MAX_NUM; i++) {
				if (pctx->ld_led_device_info.u8Device_Num[i] == 0)
					u16ExtendData_Count++;
				else if (pctx->ld_led_device_info.u8Device_Num[i]
					% FOUR_BYTE_BIT_NUM == 0)
					u16ExtendData_Count = u16ExtendData_Count
					+ ((pctx->ld_led_device_info.u8Device_Num[i]
					/ FOUR_BYTE_BIT_NUM) * TWICE);
				else
					u16ExtendData_Count = u16ExtendData_Count
					+ (((pctx->ld_led_device_info.u8Device_Num[i]
					/ FOUR_BYTE_BIT_NUM) * TWICE) + 1);
			}
			break;
		}
		case E_LD_DEVICE_MBI6353:
		{
			for (i = 0; i < MSPI_MAX_NUM; i++) {
				if (pctx->ld_led_device_info.u8Device_Num[i] == 0)
					u16ExtendData_Count++;
				else
					u16ExtendData_Count = u16ExtendData_Count
					+((pctx->ld_led_device_info.u8Device_Num[i] - 1) * TWICE);
			}
			break;
		}
		default:
		{
			for (i = 0; i < MSPI_MAX_NUM; i++) {
				if (pctx->ld_led_device_info.u8Device_Num[i] > 0){
					u16ExtendData_Count = u16ExtendData_Count+ pctx->ld_led_device_info.u16ExtendDataByte[i];
				}
			}
			break;
		}
		}


		name = MAPPING_TABLE_EXTEND_DATA_DUTY_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_ExtendData_Duty,
		u16ExtendData_Count);

		name = CURRENT_DRAM_FORMAT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16CurrentDramFormat,
		DMA_INFO_NUM * MSPI_MAX_NUM);

		name = CURRENT_HEADER_TAG;
		get_dts_array_u8_ldm(np, name,
		pctx->ld_led_device_info.u8CurrentHeader, CURRENT_HEADER_NUM);

		name = MAPPING_TABLE_HEADER_CURRENT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_Header_Current,
		(pctx->ld_led_device_info.u8CurrentHeaderByte * MSPI_MAX_NUM));

		name = MAPPING_TABLE_CURRENT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_Current,
		(pctx->ld_panel_info.u16LEDWidth
		* pctx->ld_panel_info.u16LEDHeight
		* pctx->ld_led_device_info.u8CurrentPerDataByte));

		name = MAPPING_TABLE_CHECKSUM_CURRENT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_CheckSum_Current,
		(pctx->ld_led_device_info.u8CurrentCheckSumByte * MSPI_MAX_NUM));

		name = MAPPING_TABLE_EXTEND_DATA_CURRENT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_led_device_info.u16Mapping_Table_ExtendData_Current,
		u16ExtendData_Count);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_LED_DEVICE_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;

}

int _parse_ldm_boosting_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;



	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm boosting info
	np = of_find_node_by_name(NULL, LDM_BOOSTING_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_boosting_info.u8MaxPowerRatio = get_dts_u32_ldm(
		np, MAX_POWER_RATIO_TAG);

		name = OPEN_AREA_DUTY_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_boosting_info.u16OpenAreaDuty_100, INIT_OPEN_AREA_NUM);

		name = OPEN_AREA_DUTY_ANCHOR_POINT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_boosting_info.u16OpenAreaDuty_AnchorPoints, INIT_OPEN_AREA_NUM);

		name = DECAY_WEI_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_boosting_info.u16DecayWei, DECAY_WEI_NUM);

		name = OPEN_AREA_CURRENT_ANCHOR_POINT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_boosting_info.u16OpenAreaCurrentAnchorPoints, INIT_OPEN_AREA_NUM);

		name = OPEN_AREA_CURRENT_TAG;
		get_dts_array_u16_ldm(np, name,
		pctx->ld_boosting_info.u16OpenAreaCurrent, INIT_OPEN_AREA_NUM);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_MSPI_INFO_NODE);
		ret = -ENODATA;

	}

	return ret;

}


int _parse_ldm_dma_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;


	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm DMA info
	np = of_find_node_by_name(NULL, LDM_DMA_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_dma_info.u8LDMAchannel
		= get_dts_u32_ldm(np, LDMA_CHANNEL_TAG);
		pctx->ld_dma_info.u8LDMATrimode
		= get_dts_u32_ldm(np, LDMA_TRIMODE_TAG);
		pctx->ld_dma_info.u8LDMACheckSumMode
		= get_dts_u32_ldm(np, LDMA_CHECK_SUM_MODE_TAG);
		pctx->ld_dma_info.u8cmdlength = get_dts_u32_ldm(np, CMD_LENGTH_TAG);
		pctx->ld_dma_info.u8BLWidth = get_dts_u32_ldm(np, BL_WIDTH_TAG);
		pctx->ld_dma_info.u8BLHeight = get_dts_u32_ldm(np, BL_HEIGHT_TAG);
		pctx->ld_dma_info.u16LedNum = get_dts_u32_ldm(np, LED_NUM_TAG);
		pctx->ld_dma_info.u8DataPackMode
		= get_dts_u32_ldm(np, DATA_PACK_MODE_TAG);
		pctx->ld_dma_info.u16DMAPackLength
		= get_dts_u32_ldm(np, DMA_PACK_LENGTH_TAG);
		pctx->ld_dma_info.u8DataInvert = get_dts_u32_ldm(np, DATA_INVERT_TAG);
		pctx->ld_dma_info.u32DMABaseOffset
		= get_dts_u32_ldm(np, DMA_BASE_OFFSET_TAG);
		pctx->ld_dma_info.u8ExtDataLength
		= get_dts_u32_ldm(np, EXT_DATA_LENGTH_TAG);
		pctx->ld_dma_info.u16BDACNum
		= get_dts_u32_ldm(np, BDAC_NUM_TAG);
		pctx->ld_dma_info.u8BDACcmdlength
		= get_dts_u32_ldm(np, BDAC_CMD_LENGTH_TAG);
		pctx->ld_dma_info.u8Same_tri_for_Vsync_and_DMA
		= get_dts_u32_ldm(np, LDM_SAME_TRIG_VSYNC_DMA_TAG);
		pctx->ld_dma_info.u16First_trig
		= get_dts_u32_ldm(np, LDM_FIRST_TRIG_TAG);
		pctx->ld_dma_info.bClosed_12bit_duty_mode
		= get_dts_u32_ldm(np, LDM_CLOSED_12BIT_DUTY_TAG);

		name = MSPI_HEAD_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16MspiHead, MSPI_HEADER_NUM);

		name = DMA_DELAY_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16DMADelay, DMA_DELAY_NUM);

		name = EXT_DATA_TAG;
		get_dts_array_u8_ldm(np, name, pctx->ld_dma_info.u8ExtData, EXT_DATA_NUM);

		name = BDAC_HEAD_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16BDACHead, BDAC_HEADER_NUM);

		name = LDM_VSYNC_WIDTH_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16VsyncWidth, VSYNC_WIDTH_NUM);

		name = LDM_PWM0_PERIOD_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM0_Period, PWM_PERIO_NUM);

		name = LDM_PWM0_DUTY_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM0_Duty, PWM_DUTY_NUM);

		name = LDM_PWM0_SHIFT_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM0_Shift, PWM_SHIFT_NUM);

		name = LDM_PWM1_PERIOD_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM1_Period, PWM_PERIO_NUM);

		name = LDM_PWM1_DUTY_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM1_Duty, PWM_DUTY_NUM);

		name = LDM_PWM1_SHIFT_TAG;
		get_dts_array_u16_ldm(np, name, pctx->ld_dma_info.u16PWM1_Shift, PWM_SHIFT_NUM);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_DMA_INFO_NODE);
		ret = -ENODATA;

	}

	return ret;

}


int _parse_ldm_mspi_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	const char *name;



	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm MSPI info
	np = of_find_node_by_name(NULL, LDM_MSPI_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_mspi_info.u8MspiChannel = get_dts_u32_ldm(np, MSPI_CHANNEL_TAG);
		pctx->ld_mspi_info.u8MspiMode = get_dts_u32_ldm(np, MSPI_MODE_TAG);
		pctx->ld_mspi_info.u8TrStart = get_dts_u32_ldm(np, TR_START_TAG);
		pctx->ld_mspi_info.u8TrEnd = get_dts_u32_ldm(np, TR_END_TAG);
		pctx->ld_mspi_info.u8TB = get_dts_u32_ldm(np, TB_TAG);
		pctx->ld_mspi_info.u8TRW = get_dts_u32_ldm(np, TRW_TAG);
		pctx->ld_mspi_info.u32MspiClk = get_dts_u32_ldm(np, MSPI_CLK_TAG);
		pctx->ld_mspi_info.BClkPolarity = get_dts_u32_ldm(np, BCLKPOLARITY_TAG);
		pctx->ld_mspi_info.BClkPhase = get_dts_u32_ldm(np, BCLKPHASE_TAG);
		pctx->ld_mspi_info.u32MAXClk = get_dts_u32_ldm(np, MAX_CLK_TAG);
		pctx->ld_mspi_info.u8MspiBuffSizes
		= get_dts_u32_ldm(np, MSPI_BUFFSIZES_TAG);

		name = W_BIT_CONFIG_TAG;
		get_dts_array_u8_ldm(np, name, pctx->ld_mspi_info.u8WBitConfig, W_BIT_CONFIG_NUM);

		name = R_BIT_CONFIG_TAG;
		get_dts_array_u8_ldm(np, name, pctx->ld_mspi_info.u8RBitConfig, R_BIT_CONFIG_NUM);

	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_MSPI_INFO_NODE);
		ret = -ENODATA;

	}

	return ret;

}


int _parse_ldm_misc_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	//const char *name;
	//int i;
	//__u32 u32Tmp = 0x0;
	//__u32 *u32TmpArray = NULL;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm Misc info
	np = of_find_node_by_name(NULL, LDM_MISC_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_misc_info.bLDEn = get_dts_u32_ldm(np, LDEN_TAG);
		pctx->ld_misc_info.bCompensationEn = get_dts_u32_ldm(np, COMPENSATIONEN_TAG);
		pctx->ld_misc_info.eLD_cus_alg_mode = get_dts_u32_ldm(np, LD_CUS_ALG_MODE_TAG);
		pctx->ld_misc_info.bOLEDEn = get_dts_u32_ldm(np, OLED_EN_TAG);
		pctx->ld_misc_info.bOLED_LSP_En = get_dts_u32_ldm(np, OLED_LSP_EN_TAG);
		pctx->ld_misc_info.bOLED_GSP_En = get_dts_u32_ldm(np, OLED_GSP_EN_TAG);
		pctx->ld_misc_info.bOLED_HBP_En = get_dts_u32_ldm(np, OLED_HBP_EN_TAG);
		pctx->ld_misc_info.bOLED_CRP_En = get_dts_u32_ldm(np, OLED_CRP_EN_TAG);
		pctx->ld_misc_info.bOLED_LSPHDR_En
		= get_dts_u32_ldm(np, OLED_LSPHDR_EN_TAG);
		pctx->ld_misc_info.bOLED_YUVHist_En
		= get_dts_u32_ldm(np, OLED_YUVHIST_EN_TAG);
		pctx->ld_misc_info.bOLED_LGE_GSR_En
		= get_dts_u32_ldm(np, OLED_LGE_GSR_EN_TAG);
		pctx->ld_misc_info.bOLED_LGE_GSR_HBP_En
		= get_dts_u32_ldm(np, OLED_LGE_GSR_HBP_EN_TAG);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_MISC_INFO_NODE);
		ret = -ENODATA;

	}

	return ret;
}

int _parse_ldm_panel_info(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	//const char *name;
	//int i;
	//__u32 *u32TmpArray = NULL;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	//Get ldm Panel info
	np = of_find_node_by_name(NULL, LDM_PANEL_INFO_NODE);
	if (np != NULL && of_device_is_available(np)) {

		pctx->ld_panel_info.u16PanelWidth = get_dts_u32_ldm(np, PANEL_WIDTH_TAG);
		pctx->ld_panel_info.u16PanelHeight = get_dts_u32_ldm(np, PANEL_HEIGHT_TAG);
		pctx->ld_panel_info.eLEDType = get_dts_u32_ldm(np, LED_TYPE_TAG);
		pctx->ld_panel_info.u16LDFWidth = get_dts_u32_ldm(np, LDF_WIDTH_TAG);
		pctx->ld_panel_info.u16LDFHeight = get_dts_u32_ldm(np, LDF_HEIGHT_TAG);
		pctx->ld_panel_info.u16LEDWidth = get_dts_u32_ldm(np, LED_WIDTH_TAG);
		pctx->ld_panel_info.u16LEDHeight = get_dts_u32_ldm(np, LED_HEIGHT_TAG);
		pctx->ld_panel_info.u8LSFWidth = get_dts_u32_ldm(np, LSF_WIDTH_TAG);
		pctx->ld_panel_info.u8LSFHeight = get_dts_u32_ldm(np, LSF_HEIGHT_TAG);
		pctx->ld_panel_info.u8Edge2D_Local_h
		= get_dts_u32_ldm(np, EDGE2D_LOCAL_H_TAG);
		pctx->ld_panel_info.u8Edge2D_Local_v
		= get_dts_u32_ldm(np, EDGE2D_LOCAL_V_TAG);
		pctx->ld_panel_info.u8PanelHz = get_dts_u32_ldm(np, PANEL_TAG);
		pctx->ld_panel_info.u8MirrorPanel = get_dts_u32_ldm(np, MIRROR_PANEL_TAG);
		pctx->ld_panel_info.u8LSFHeight = get_dts_u32_ldm(np, LSF_HEIGHT_TAG);
		pctx->ld_panel_info.bSupport_DV_GD = get_dts_u32_ldm(np, SUPPORT_DV_GD);
	} else {
		DRM_INFO("[%s, %d]: find node failed, name = %s\n",
		__func__, __LINE__, LDM_PANEL_INFO_NODE);
		ret = -ENODATA;
	}

	return ret;
}

static int get_memory_info(u32 *addr)
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
		pr_err("can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}
	return 0;
}

void _parse_ldm_trunk_info(struct mtk_ld_context *pctx)
{
	int ret = 0;

	ret = _parse_ldm_panel_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_panel_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_misc_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_misc_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_mspi_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_mspi_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_dma_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_dma_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_boosting_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
		"[%s, %d]: _parse_ldm_boosting_info failed\n",
		__func__, __LINE__);
	}
	ret = _parse_ldm_led_device_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_led_device_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_led_device_as3824_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_led_device_as3824_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_led_device_nt50585_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_led_device_nt50585_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_led_device_iw7039_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_led_device_iw7039_info failed\n",
			__func__, __LINE__);
	}
	ret = _parse_ldm_led_device_mbi6353_info(pctx);
	if (ret != 0x0) {

		DRM_INFO(
			"[%s, %d]: _parse_ldm_led_device_iw7039_info failed\n",
			__func__, __LINE__);
	}

	dump_dts_ld_info(pctx);

}

uint32_t mtk_ldm_get_support_type(void)
{
	struct device_node *np;
	const char *name;
	uint32_t u32Tmp = 0x0;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, LDM_COMPATIBLE_STR);

	if (np != NULL) {
		name = MTK_DRM_DTS_LD_SUPPORT_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {
			DRM_INFO(
				"[%s, %d]: get LDM_SUPPORT fail , name = %s \r\n",
				__func__, __LINE__,
				name);
			return E_LDM_UNSUPPORT;
		}
	}
	return u32Tmp;
}



int readDTB2LDMPrivate(struct mtk_ld_context *pctx)
{
	int ret = 0;
	struct device_node *np;
	__u32 u32Tmp = 0x0;
	const char *name;
	//int *tmpArray = NULL, i;
	__u32 *u32TmpArray = NULL;
	int i;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ret = get_memory_info(&u32Tmp);
	if (ret) {
		pr_err("no defined miu0_base\n");
		ret = -EINVAL;
	} else {
		pctx->u32Cpu_emi0_base = u32Tmp;
		DRM_INFO("dts miu0_base 0x%x\n",
			pctx->u32Cpu_emi0_base);
	}

	np = of_find_compatible_node(NULL, NULL, LDM_COMPATIBLE_STR);

	if (np != NULL) {
		name = MTK_DRM_DTS_LD_SUPPORT_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: LDM of_property_read_u8 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}
		pctx->u8LDMSupport = u32Tmp;

		DRM_INFO(
			"LDM_support=%x %x \r\n",
			u32Tmp, pctx->u8LDMSupport);

		name = MTK_DRM_DTS_LD_MMAP_ADDR_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);

		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: of_property_read_u32 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}

		pctx->u64LDM_phyAddress = (__u64)u32Tmp;
		DRM_INFO("LDM_phyaddress=%x  %tx \r\n",
			u32Tmp, (ptrdiff_t)pctx->u64LDM_phyAddress);

		name = MTK_DRM_DTS_LD_VERSION_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: LDM of_property_read_u8 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}
		pctx->u8LDM_Version = u32Tmp;
		DRM_INFO(
			"LDM_Version=%x %x \r\n",
			u32Tmp, pctx->u8LDM_Version);

		name = MTK_DRM_DTS_RENDER_QMAP_VERSION_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: LDM of_property_read_u8 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}
		pctx->u8RENDER_QMAP_Version = u32Tmp;
		DRM_INFO(
			"RENDER_QMAP_Version=%x %x \r\n",
			u32Tmp, pctx->u8RENDER_QMAP_Version);

		name = MTK_DRM_DTS_LD_UBOOT_SUPPORT_TAG;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: LDM of_property_read_u8 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}
		pctx->bLDM_uboot = u32Tmp;

		DRM_INFO(
			"bLDM_uboot=%x %x \r\n",
			u32Tmp, pctx->bLDM_uboot);

		name = MTK_DRM_DTS_LD_DMA_MSPI_TEST;
		ret = of_property_read_u32(np, name, &u32Tmp);
		if (ret != 0x0) {

			DRM_INFO(
				"[%s, %d]: LDM of_property_read_u8 failed, name = %s \r\n",
				__func__, __LINE__,
				name);
			return ret;
		}
		pctx->bDMA_SPI_Test = u32Tmp;

		DRM_INFO(
			"bDMA_SPI_Test=%x %x \r\n",
			u32Tmp, pctx->bDMA_SPI_Test);

	}

	np = of_find_node_by_name(NULL, MTK_DRM_DTS_MMAP_INFO_LD_MMAP_ADDR_TAG);
	if (np != NULL) {
		DRM_INFO("LDM mmap info node find success \r\n");
		name = MTK_DRM_DTS_MMAP_INFO_LD_MMAP_ADDR_REG_TAG;

	u32TmpArray = kmalloc(sizeof(__u32) * 4, GFP_KERNEL);

	if (u32TmpArray == NULL) {
		DRM_ERROR("[LDM][%s, %d]: malloc failed \r\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto ERROR;
	}

	ret = of_property_read_u32_array(np, name, u32TmpArray, 4);
	if (ret != 0x0) {
		kfree(u32TmpArray);
		return ret;
	}
	for (i = 0; i < 4; i++)
		DRM_INFO("u32TmpArray[%d]  %x \r\n",
			i, u32TmpArray[i]);


	pctx->u64LDM_phyAddress = (__u64)(u32TmpArray[MMAPINFO_OFFSET_1] - pctx->u32Cpu_emi0_base);
	pctx->u32LDM_mmapsize = u32TmpArray[MMAPINFO_OFFSET_3];
	DRM_INFO("From mmap LDM_phyaddress=%x  %tx \r\n",
		u32Tmp, (ptrdiff_t)pctx->u32LDM_mmapsize);

	kfree(u32TmpArray);

	}

	_parse_ldm_trunk_info(pctx);

	return ret;

ERROR:
	kfree(u32TmpArray);

	return ret;
}

static void pqu_ready_ldm(void)
{
	int ret = 0;

	DRM_INFO("[%s,%5d] pqu notify callback\n", __func__, __LINE__);
	ret = pqu_render_ldm(&ldm_param, NULL);
	if (ret != 0)
		DRM_ERROR("cpuif ready buf API fail");
}

int mtk_ldm_set_dev(struct device *dev)
{
	if (dev == NULL) {
		DRM_ERROR("drm_device is NULL");
		return -EINVAL;
	}
	_drm_dev = dev;
	return 0;
}

struct device *mtk_ldm_get_dev(void)
{
	return _drm_dev;
}

int mtk_ldm_init(struct device *dev)
{

	int ret = 0, ret_rtos = 0;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ret = mtk_ldm_set_dev(dev);
	if (ret != 0x0) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ret = readDTB2LDMPrivate(pctx_ld);
	if (ret != 0x0) {

		DRM_ERROR(
			"[%s, %d]: readDTB2LDMPrivate failed\n",
			__func__, __LINE__);

		return -ENODEV;
	}

//ret = MApi_ld_Init_SetLDC(pctx_ld->u8LDMSupport); //move to uboot
	ldm_param.u8ld_support_type = pctx_ld->u8LDMSupport;
	ldm_param.u8ld_version = pctx_ld->u8LDM_Version;
	ldm_param.u8Seamless_flag = 0;
	ldm_param.u8VRR_flag = 0;
	ldm_param.u32ld_mmapsize = pctx_ld->u32LDM_mmapsize;
	ldm_param.u8ld_PQparam_type = E_LD_Param_MAX;
	ldm_param.u32Cpu_emi0_base = pctx_ld->u32Cpu_emi0_base;


	if (bPquEnable) {
//		ret_rtos = pqu_render_ldm(&ldm_param, &ldm_reply_param);
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_version_reply1 %d %x !!\n",
		__func__,
		__LINE__,
		ret_rtos,
		ldm_reply_param.ret);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
		DRM_INFO(
		"[DRM][LDM][%s][%d] E_LDM_SUPPORT_R2 !!\n",
		__func__,
		__LINE__);
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_version_reply %d %x !!\n",
		__func__,
		__LINE__,
		ret,
		ldm_reply_param.ret);
	}

	return ret;
}

int mtk_ldm_set_led_check(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_led_check *ldm_led_check)
{
	int ret_rtos = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_ld_context *pctx_ld = NULL;


	if (!mtk_tv_crtc || !ldm_led_check) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, data=%p\n",
				__func__, __LINE__, mtk_tv_crtc, ldm_led_check);
			return -EINVAL;
	}
	ldm_param.u8ld_ledcheck_en = (uint8_t)ldm_led_check->bEn;
	ldm_param.u8ld_ledcheck_type = (uint8_t)ldm_led_check->enldm_led_check_type;
	ldm_param.u16ld_ledcheck_num = ldm_led_check->u16LEDNum;


	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail	!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}



	if (bPquEnable) {
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] rv55 ld_param %d!!\n",
		__func__,
		__LINE__,
		ret_rtos);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_param  !!\n",
		__func__,
		__LINE__);
	}

//printk("mtk_ldm_set_led_check %d\n",ldm_led_check->bEn);


	return 0;
}

int mtk_tv_drm_set_ldm_led_check_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_tv_ldm_led_check *args = (struct drm_mtk_tv_ldm_led_check *)data;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;

	drm_for_each_crtc(crtc, dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		if (mtk_tv_crtc->ops->ldm_set_led_check) {
			ret = mtk_tv_crtc->ops->ldm_set_led_check(mtk_tv_crtc, args);
			break;
		}
	}
	return ret;

}


int mtk_ldm_set_black_insert(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_black_insert_enable *ldm_black_insert_en)
{

	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_ld_context *pctx_ld = NULL;
	struct ext_crtc_prop_info *crtc_props = NULL;
	int ret = 0;
	int ret_rtos = 0;

	uint64_t input_vfreqx1000 = 0;
	uint32_t input_freq = 0;
	uint32_t output_freq = 0;


	if (!mtk_tv_crtc || !ldm_black_insert_en) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, data=%p\n",
			__func__, __LINE__, mtk_tv_crtc, ldm_black_insert_en);
		return -EINVAL;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}


	crtc_props = pctx->ext_crtc_properties;
	if (crtc_props == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}



	if (crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID] != 0) {
		ret = mtk_video_get_vttv_input_vfreq(pctx,
			crtc_props->prop_val[E_EXT_CRTC_PROP_FRAMESYNC_PLANE_ID],
			&input_vfreqx1000);
	}

input_freq	= (uint32_t) DIV64_U64_ROUND_CLOSEST(input_vfreqx1000, VFRQRATIO_1000);
output_freq = DIV_ROUND_CLOSEST(pctx->panel_priv.outputTiming_info.u32OutputVfreq, VFRQRATIO_1000);



	ldm_param.u8ld_BFI_en = ldm_black_insert_en->bEn;
	ldm_param.u8ld_BFI_inputFreq = input_freq;
	ldm_param.u8ld_BFI_outputFreq = output_freq;



	if (bPquEnable) {
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] rv55 ld_param %d!!\n",
		__func__,
		__LINE__,
		ret_rtos);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_param  !!\n",
		__func__,
		__LINE__);
	}



	return ret;
}


int mtk_tv_drm_set_ldm_black_insert_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_tv_ldm_black_insert_enable *args =
		(struct drm_mtk_tv_ldm_black_insert_enable *)data;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;

	drm_for_each_crtc(crtc, dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		if (mtk_tv_crtc->ops->ldm_set_black_insert_enable) {
			ret = mtk_tv_crtc->ops->ldm_set_black_insert_enable(mtk_tv_crtc, args);
			break;
		}
	}
	return ret;

}

int mtk_ldm_set_pq_param(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_pq_param *ldm_pq_param)
{
	int ret_rtos = 0;
	struct mtk_tv_kms_context *pctx = NULL;
	struct mtk_ld_context *pctx_ld = NULL;


	if (!mtk_tv_crtc || !ldm_pq_param) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, data=%p\n",
				__func__, __LINE__, mtk_tv_crtc, ldm_pq_param);
			return -EINVAL;
	}

	ldm_param.u8ld_PQparam_type = (uint8_t)ldm_pq_param->e_ldm_pq_param_type;
	ldm_param.u8ld_PQparam_value = ldm_pq_param->u8value;

	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;

	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d]  null pointer fail	!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	if (bPquEnable) {
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] rv55 ld_param %d!!\n",
		__func__,
		__LINE__,
		ret_rtos);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_param  !!\n",
		__func__,
		__LINE__);
	}
	ldm_param.u8ld_PQparam_type = E_LD_Param_MAX;



	return 0;
}


int mtk_tv_drm_set_ldm_pq_param_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_tv_ldm_pq_param *args =
		(struct drm_mtk_tv_ldm_pq_param *)data;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;

	drm_for_each_crtc(crtc, dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		if (mtk_tv_crtc->ops->ldm_set_pq_param) {
			ret = mtk_tv_crtc->ops->ldm_set_pq_param(mtk_tv_crtc, args);
			break;
		}
	}
	return ret;

}




int mtk_ldm_set_ldm_VCOM_enable(struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ldm_VCOM_enable *ldm_VCOM_en)
{

//	struct mtk_tv_kms_context *pctx = NULL;
//	struct mtk_ld_context *pctx_ld = NULL;
	int ret = 0;
	int ret_rtos = 0;


	if (!mtk_tv_crtc || !ldm_VCOM_en) {
		DRM_ERROR("[%s][%d] null ptr, crtc=%p, data=%p\n",
			__func__, __LINE__, mtk_tv_crtc, ldm_VCOM_en);
		return -EINVAL;
	}

	ldm_param.u8ld_VCOM_en = ldm_VCOM_en->bEn;

	if (bPquEnable) {
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] rv55 ld_param %d!!\n",
		__func__,
		__LINE__,
		ret_rtos);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_param  !!\n",
		__func__,
		__LINE__);
	}



	return ret;
}



int mtk_tv_drm_set_ldm_VCOM_enable_ioctl(
	struct drm_device *dev,
	void *data,
	struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_tv_ldm_VCOM_enable *args =
		(struct drm_mtk_tv_ldm_VCOM_enable *)data;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;

	drm_for_each_crtc(crtc, dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		if (mtk_tv_crtc->ops->ldm_set_VCOM_enable) {
			ret = mtk_tv_crtc->ops->ldm_set_VCOM_enable(mtk_tv_crtc, args);
			break;
		}
	}
	return ret;

}

int mtk_ldm_suspend(struct device *dev)
{

	int ret = 0;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	DRM_INFO("\033[1;34m [%s][%d] \033[m\n", __func__, __LINE__);

	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}


	if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_TRUNK_PQU) {
		DRM_INFO("LDM support TRUNK \r\n");
		ret = MApi_ld_Suspend(pctx_ld->u8LDM_Version, pctx);

	} else if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_CUS) {
		DRM_INFO("LDM support CUS \r\n");

	} else if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
		DRM_INFO("LDM support R2 \r\n");
		ret = MApi_ld_Suspend(pctx_ld->u8LDM_Version, pctx);
	} else {
		DRM_INFO("Not support LDM \r\n");

	}

	return ret;
}

int mtk_ldm_resume(struct device *dev)
{

	int ret = 0;
	int ret_rtos = 0;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;
	struct drm_st_ld_qmap_info *ld_qmap = NULL;

	DRM_INFO("\033[1;34m [%s][%d] \033[m\n", __func__, __LINE__);

	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	ret = MApi_ld_Init_SetLDC(pctx_ld->u8LDMSupport);

	// restore qmap
	ld_qmap = &pctx_ld->ld_qmap_info;
	if (ld_qmap->node_buf != NULL) {
		ret = mtk_tv_drm_pqmap_write_ml(&pctx->pqmap_priv, &pctx->video_priv.disp_ml,
						(uint16_t *)ld_qmap->node_num, ld_qmap->node_buf);
		if (ret)
			DRM_ERROR("[%s:%d] qmap write fail, ret:%d", __func__, __LINE__, ret);
	}

	DRM_INFO(
	"[DRM][LDM][%s][%d] ld resume set LDC %d %x !!\n",
	__func__,
	__LINE__,
	pctx_ld->u8LDMSupport,
	ldm_param.u8ld_support_type);

	if (bPquEnable) {
//	ret_rtos = pqu_render_ldm(&ldm_param, &ldm_reply_param);
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);

		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_version_reply1 %d %x !!\n",
		__func__,
		__LINE__,
		ret_rtos,
		ldm_reply_param.ret);
		if (ret_rtos) {
			DRM_ERROR("CPUIF not ready, wait it ready and retry");
			fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
		}
	} else if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
		DRM_INFO(
		"[DRM][LDM][%s][%d] E_LDM_SUPPORT_R2 !!\n",
		__func__,
		__LINE__);
	} else {
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
		DRM_INFO(
		"[DRM][LDM][%s][%d] ld_version_reply %d %x !!\n",
		__func__,
		__LINE__,
		ret,
		ldm_reply_param.ret);

	}
/*
	switch (pctx_ld->u8LDMSupport) {
	case E_LDM_UNSUPPORT:
		break;
	case E_LDM_SUPPORT_TRUNK_PQU:
	case E_LDM_SUPPORT_TRUNK_FRC:
	case E_LDM_SUPPORT_R2:
		DRM_INFO("LDM support TRUNK\n");
		ret = MApi_ld_Init(pctx);
		if (ret != E_LDM_RET_SUCCESS) {
			DRM_ERROR(
			"[DRM][LDM][%s][%d] MApi_ld_Init fail %d !!\n",
			__func__, __LINE__, ret);
		}
		break;
	case E_LDM_SUPPORT_CUS:
		DRM_INFO("LDM support CUS\n");
		break;
	case E_LDM_SUPPORT_BE:
		DRM_INFO("LDM support CUS BE\n");
		break;
	default:
		DRM_INFO("LDM is not support.\n");
		break;
	}
*/

// reinit led driver
	DRM_INFO("MApi_ld_Resume\n");
	ret = MApi_ld_Resume(pctx);
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] MApi_ld_Resume fail %d !!\n",
		__func__, __LINE__, ret);
	}

/*send command to diff cpu*/

	DRM_INFO("MApi_ld_Init_to_CPUIF\n");
	ret = MApi_ld_Init_to_CPUIF();
	if (ret != E_LDM_RET_SUCCESS) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] MApi_ld_Init_to_CPUIF fail %d !!\n",
		__func__, __LINE__, ret);
	}

	if (pctx_ld->u8LDMSupport == E_LDM_SUPPORT_R2) {
		DRM_INFO("R2 use mailbox\n");
		ret = MApi_ld_Init_to_Mailbox();
		if (ret != E_LDM_RET_SUCCESS) {
			DRM_ERROR(
			"[DRM][LDM][%s][%d] MApi_ld_Init_to_Mailbox fail %d !!\n",
			__func__, __LINE__, ret);
		}
	}

	return ret;
}

int mtk_ldm_re_init(void)
{
	int ret = 0;
	struct device *dev;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	if ((MApi_ld_Get_En_Status(pctx_ld->u8LDM_Version))
		&& (MApi_ld_Check_Tgen_Changed(pctx_ld->u8LDM_Version))
		) {
		MApi_ld_set_spi_self_trig_off(pctx_ld->u8LDM_Version);

		msleep(SLEEP_TIME_RE_INIT);

		ret = MApi_ld_Init_to_CPUIF();
		if (ret != E_LDM_RET_SUCCESS) {
			DRM_ERROR(
			"[DRM][LDM][%s][%d] MApi_ld_Init_to_CPUIF fail %d !!\n",
			__func__, __LINE__, ret);
		}
		DRM_INFO("[DRM][LDM][%s][%d] ldm re init ok !!\n",__func__,__LINE__);
	}
	return ret;
}

int mtk_ldm_set_backlight_off(bool bEn)
{
	int ret = 0;
	int ret_rtos = 0;
	struct device *dev;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	if (MApi_ld_Get_En_Status(pctx_ld->u8LDM_Version)) {
		ldm_param.u8ld_backlight_off = (uint8_t)bEn;

		if (bPquEnable) {
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);

			DRM_INFO(
			"[DRM][LDM][%s][%d] rv55 ld_param %d!!\n",
			__func__,
			__LINE__,
			ret_rtos);
			if (ret_rtos) {
				DRM_ERROR("CPUIF not ready, wait it ready and retry");
				fw_ext_command_register_notify_ready_callback(0, pqu_ready_ldm);
			}
		} else {
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);
			DRM_INFO(
			"[DRM][LDM][%s][%d] ld_param  !!\n",
			__func__,
			__LINE__);
		}
	}
	return ret;
}


/*----------------------SYSFS-------------------*/
int _mtk_ldm_cmd_sysfs(enum EN_SYSFS_LDM_CMD val)
{
	int ret_api = 0;
	int ret_rtos = 0;
	struct device *dev;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;
	struct hwregSWSetCtrlIn stApiSWSetCtrl;
	struct LD_CUS_PATH stLD_Cus_Path;

	memset(
	&stApiSWSetCtrl,
	0,
	sizeof(struct hwregSWSetCtrlIn));

	memset(
	&stLD_Cus_Path,
	0,
	sizeof(struct LD_CUS_PATH));

	if (ldm_param.u8ld_version - 1 > 0) {
		strncpy(stLD_Cus_Path.aCusPath,
		ld_config_cus_paths[(ldm_param.u8ld_version - 1)],
		MAX_PATH_LEN - 1);
		stLD_Cus_Path.aCusPath[sizeof(stLD_Cus_Path.aCusPath) - 1] = '\0';
	}
	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	switch (val) {
	case E_LDM_CMD_HOST_INIT:
	{
		pctx_ld->u8LDMSupport = E_LDM_SUPPORT_TRUNK_PQU;
		ret_api = MApi_ld_Init_SetLDC(E_LDM_SUPPORT_TRUNK_PQU);
		DRM_INFO("[DRM][LDM] MApi_ld_Init_SetLDC %d\n", ret_api);

		ldm_param.u8ld_support_type = E_LDM_SUPPORT_TRUNK_PQU;
		if (bPquEnable)
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

		ret_api = MApi_ld_Init(
		pctx, &stLD_Cus_Path_real);
		DRM_INFO("[DRM][LDM] MApi_ld_Init %d\n", ret_api);

		/*check init function*/
		if (ret_api != E_LDM_RET_SUCCESS) {
			DRM_ERROR("LDM INIT FAIL\n");
			break;
		}

		/*send command to diff cpu*/
		ret_api = MApi_ld_Init_to_CPUIF();
		DRM_INFO("[DRM][LDM] MApi_ld_Init_to_CPUIF %d\n", ret_api);
		break;
	}
	case E_LDM_CMD_R2_INIT:
	{
		pctx_ld->u8LDMSupport = E_LDM_SUPPORT_R2;
		ret_api = MApi_ld_Init_SetLDC(E_LDM_SUPPORT_TRUNK_PQU);
		DRM_INFO("[DRM][LDM] MApi_ld_Init_SetLDC %d\n", ret_api);

		ldm_param.u8ld_support_type = E_LDM_SUPPORT_R2;
		if (bPquEnable)
			ret_rtos = pqu_render_ldm(&ldm_param, NULL);
		else
			pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

		ret_api = MApi_ld_Init(
		pctx, &stLD_Cus_Path_real);
		DRM_INFO("[DRM][LDM] MApi_ld_Init%d\n", ret_api);

		/*check init function*/
		if (ret_api != E_LDM_RET_SUCCESS) {
			DRM_ERROR("LDM INIT FAIL\n");
			break;
		}
		/*send command to diff cpu*/
		ret_api = MApi_ld_Init_to_CPUIF();
		DRM_INFO("[DRM][LDM] MApi_ld_Init_to_CPUIF %d\n", ret_api);

		/*send command to diff cpu*/
		DRM_INFO("[DRM][LDM]R2 use mailbox\n");
		ret_api = MApi_ld_Init_to_Mailbox();
		break;
	}
	case E_LDM_CMD_SUSPEND:
	{
		ret_api = mtk_ldm_suspend(dev);
		DRM_INFO("[DRM][LDM] set LDM_SUSPEND %d\n", ret_api);
		break;
	}
	case E_LDM_CMD_RESUME:
	{
		ret_api = mtk_ldm_resume(dev);
		DRM_INFO("[DRM][LDM] set LDM_RESUME %d\n", ret_api);
		break;
	}

	case E_LDM_CMD_BYPASS:
	{
		ret_api = MApi_ld_Init_SetLDC(E_LDM_UNSUPPORT);
		DRM_INFO("[DRM][LDM] set LDM BYPASS %d\n", ret_api);
		break;
	}
	case E_LDM_CMD_CUS_INIT:
	{
		pctx_ld->u8LDMSupport = E_LDM_SUPPORT_CUS;
		switch (pctx_ld->u8LDM_Version) {
		case E_LDM_HW_VERSION_1:
		case E_LDM_HW_VERSION_2:
		case E_LDM_HW_VERSION_3:
		{
			ret_api = MApi_ld_Init_SetLDC(E_LDM_SUPPORT_CUS);
			DRM_INFO("[DRM][LDM] MApi_ld_Init_SetLDC %d\n", ret_api);
			ret_api = MApi_ld_Init_UpdateProfile(
						pctx_ld->u64LDM_phyAddress,
						pctx);
			DRM_INFO("[DRM][LDM] MApi_ld_Init_UpdateProfile %d\n", ret_api);
			break;
		}
		default:
			DRM_ERROR("[DRM][LDM] This chip don't support this function\n");
			break;
		}
		break;
	}
	default:
		DRM_ERROR("[DRM][LDM] error cmd\n");
		break;
	}
	return ret_api;

}


int _mtk_ldm_dump_data_sysfs(enum EN_LDM_GET_DATA_TYPE val)
{
	int ret_api = 0;
	int ret_rtos = 0;
	struct device *dev;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ldm_param.u8ld_dump_data = val;
	if (bPquEnable)
		ret_rtos = pqu_render_ldm(&ldm_param, NULL);
	else
		pqu_msg_send(PQU_MSG_SEND_LDM, &ldm_param);

// clean ldm dump data every time
	ldm_param.u8ld_dump_data = 0;

	return ret_api;

}

static ssize_t help_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "Debug Help:\n"
				"- ldm_cmd <RW>: To control ldm cmd\n"
				"	---------------LDM TRUNK---------------\n"
				"		LDM_HOST_INIT      -- 0x01\n"
				"		LDM_R2_INIT        -- 0x02\n"
				"		LDM_SUSPEND        -- 0x03\n"
				"		LDM_RESUME         -- 0x04\n"
				"		LDM_BYPSS          -- 0x05\n"
				"	---------------LDM CUS-----------------\n"
				"		LDM_CUS_INIT       -- 0x06\n"
				"	Example:\n"
				"		echo 0x01 > ldm_cmd\n"
				"		then LDM_HOST_INIT will be execute once\n"
				"		echo 0x02 > ldm_cmd\n"
				"		then LDM_R2_INIT will be execute once\n"
				"		echo 0x06 > ldm_cmd\n"
				"		then LDM_CUS_INIT will be execute once\n"
				"	---------------------------------------\n"
				"- ldm_dump_data <RW>: To control ldm dump data from DARM\n"
				"		LDF       --  0x1\n"
				"		LDB       --  0x2\n"
				"		SPI       --  0x3\n"
				"		HISTOGRAM --  0x4\n"
				"		LD_PARAM  --  0x5\n"
				"	Example:\n"
				"		First  => echo 0x01 > ldm_dump_data\n"
				"		Second => cat ldm_dump_data\n"
				"		then LDF DATA will be Print once\n"
				"		First  => echo 0x02 > ldm_dump_data\n"
				"		Second => cat ldm_dump_data\n"
				"		then LDB DATA will be Print once\n"
				"		First  => echo 0x03 > ldm_dump_data\n"
				"		Second => cat ldm_dump_data\n"
				"		then SPI DATA will be Print once\n"
				"	---------------------------------------\n"
				"- ldm_dts <RO>: To print LDM DTS parameter\n"
				"	Example:\n"
				"		cat ldm_dts\n"
				"		then LDM DTS DATA will be Print once\n"
				"	---------------------------------------\n"
				"- ldm_chk_reg <RO>: To print LDM REG\n"
				"	Example:\n"
				"		cat ldm_chk_reg\n"
				"		then LDM CHK REG will be Print once\n"
				"		Local_dimming_path         0:TRUNK 1:CUS\n"
				"		Cus Local_dimming_bypass   0:ACT 1:BYPASS\n"
				"		Trunk Local_dimming_bypass 0:ACT 1:BPASS\n"
				"		Trunk Local_dimming_en     0:DISABLE 1:ENABLE\n"
				"	---------------------------------------\n"
				"- ldm_bin_info <RO>: To print BIN INFO\n"
				"	Example:\n"
				"		cat ldm_bin_info\n");

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ldm_dts_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ret = scnprintf(buf, PAGE_SIZE, "LDM DTS:\n");
	dump_dts_ld_info(pctx_ld);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ldm_chk_reg_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int ret = 0;
	struct mtk_tv_kms_context *pctx;
	struct mtk_ld_context *pctx_ld;

	dev = mtk_ldm_get_dev();
	if (dev == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] device is NULL!!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	pctx = dev_get_drvdata(dev);
	if (pctx == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}
	pctx_ld = &pctx->ld_priv;
	if (pctx_ld == NULL) {
		DRM_ERROR(
		"[DRM][LDM][%s][%d] null pointer fail  !!\n",
		__func__, __LINE__);
		return -ENODEV;
	}

	ret = scnprintf(buf, PAGE_SIZE, "LDM CHK REG:\n");

	MApi_ld_Check_Reg_Sysfs(pctx_ld->u8LDM_Version);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ldm_cmd_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;

	switch (g_ldm_cmd_sysfs) {
	case E_LDM_CMD_HOST_INIT:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_HOST_INIT\n", g_ldm_cmd_sysfs);
		break;
	}
	case E_LDM_CMD_R2_INIT:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_R2_INIT\n", g_ldm_cmd_sysfs);
		break;
	}
	case E_LDM_CMD_SUSPEND:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_SUSPEND\n", g_ldm_cmd_sysfs);
		break;
	}
	case E_LDM_CMD_RESUME:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_RESUME\n", g_ldm_cmd_sysfs);
		break;
	}
	case E_LDM_CMD_BYPASS:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_BYPASS\n", g_ldm_cmd_sysfs);
		break;
	}
	case E_LDM_CMD_CUS_INIT:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d E_LDM_CUS_INIT\n", g_ldm_cmd_sysfs);
		break;
	}
	default:
		ret = scnprintf(buf, PAGE_SIZE, "ldm_init: %d\n", g_ldm_cmd_sysfs);
		break;
	}

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ldm_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{

	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	g_ldm_cmd_sysfs = val;

	_mtk_ldm_cmd_sysfs(g_ldm_cmd_sysfs);

	return count;
}

static ssize_t ldm_dump_data_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;

	switch (g_ldm_dump_data_sysfs) {
	case E_LDM_DATA_TYPE_LDF:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x.LDF\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	case E_LDM_DATA_TYPE_LDB:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x.LDB\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	case E_LDM_DATA_TYPE_SPI:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x.SPI\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	case E_LDM_DATA_TYPE_HISTO:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x.HSITO\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	case E_LDM_DATA_TYPE_LD_PARA:
	{
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x.LD_PARA\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	default:
		ret = scnprintf(buf, PAGE_SIZE, "ldm_debug_dump_dram:0x%x\n",
		g_ldm_dump_data_sysfs);
		break;
	}
	_mtk_ldm_dump_data_sysfs(g_ldm_dump_data_sysfs);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t ldm_dump_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	g_ldm_dump_data_sysfs = val;

	_mtk_ldm_dump_data_sysfs(g_ldm_dump_data_sysfs);

	return count;
}

static ssize_t ldm_bin_info_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int ret = 0;
	int count = 0, ScnSize = 0;
	char string[STRING_MAX] = {0};
	uint8_t u8ver1 = 0, u8ver2 = 0;
	char name[MAX_PATH_LEN] = {0};
	char *pname = NULL;

	pname = MDrv_LDM_Get_BIN_Name(E_LDM_BIN_TYPE_COMPENSATION);
	if (sizeof(pname) >= MAX_PATH_LEN)
		return -EINVAL;
	strncpy(name, pname, MAX_PATH_LEN - 1);
	u8ver1 = MDrv_LDM_Get_BIN_Version1(E_LDM_BIN_TYPE_COMPENSATION);
	u8ver2 = MDrv_LDM_Get_BIN_Version2(E_LDM_BIN_TYPE_COMPENSATION);
	ScnSize = scnprintf(string + count, STRING_MAX - count, "Compensation:\n");
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count, "Name:%s\n", name);
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count,
		"version:%02X.%02X\n\n", u8ver1, u8ver2);
	count += ScnSize;
	pname = MDrv_LDM_Get_BIN_Name(E_LDM_BIN_TYPE_BACKLIGHTGAMMA);
	if (sizeof(pname) >= MAX_PATH_LEN)
		return -EINVAL;
	strncpy(name, pname, MAX_PATH_LEN - 1);
	u8ver1 = MDrv_LDM_Get_BIN_Version1(E_LDM_BIN_TYPE_BACKLIGHTGAMMA);
	u8ver2 = MDrv_LDM_Get_BIN_Version2(E_LDM_BIN_TYPE_BACKLIGHTGAMMA);

	ScnSize = scnprintf(string + count, STRING_MAX - count, "BacklightGamma:\n");
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count, "Name:%s\n", name);
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count,
		"version:%02X.%02X\n\n", u8ver1, u8ver2);
	count += ScnSize;
	pname = MDrv_LDM_Get_BIN_Name(E_LDM_BIN_TYPE_EDGE2D);
	if (sizeof(pname) >= MAX_PATH_LEN)
		return -EINVAL;
	strncpy(name, pname, MAX_PATH_LEN - 1);
	u8ver1 = MDrv_LDM_Get_BIN_Version1(E_LDM_BIN_TYPE_EDGE2D);
	u8ver2 = MDrv_LDM_Get_BIN_Version2(E_LDM_BIN_TYPE_EDGE2D);

	ScnSize = scnprintf(string + count, STRING_MAX - count, "Edge2D:\n");
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count, "Name:%s\n", name);
	count += ScnSize;
	ScnSize = scnprintf(string + count, STRING_MAX - count,
		"version:%02X.%02X\n\n", u8ver1, u8ver2);
	count += ScnSize;

	ret = scnprintf(buf, PAGE_SIZE, "%s", string);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RO(ldm_dts);
static DEVICE_ATTR_RO(ldm_chk_reg);
static DEVICE_ATTR_RW(ldm_cmd);
static DEVICE_ATTR_RW(ldm_dump_data);
static DEVICE_ATTR_RO(ldm_bin_info);

static struct attribute *mtk_drm_ldm_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_ldm_dts.attr,
	&dev_attr_ldm_chk_reg.attr,
	&dev_attr_ldm_cmd.attr,
	&dev_attr_ldm_dump_data.attr,
	&dev_attr_ldm_bin_info.attr,
	NULL
};

static const struct attribute_group mtk_drm_ldm_attr_group = {
	.name = "Localdimming",
	.attrs = mtk_drm_ldm_attrs
};

void mtk_drm_ldm_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_drm_ldm_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}
}

void mtk_drm_ldm_remove_sysfs(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Remove device attribute files");
	sysfs_remove_group(&pdev->dev.kobj, &mtk_drm_ldm_attr_group);
}

static int _mtk_tv_drm_ldm_ioctl_set_qmap(
	struct mtk_tv_kms_context *kms,
	struct drm_mtk_tv_ldm_param *param)
{
	struct mtk_pqmap_context *pqmap = &kms->pqmap_priv;
	struct mtk_sm_ml_context *ml = &kms->video_priv.disp_irq_ml;
	struct drm_st_ld_qmap_info *ld_qmap = &kms->ld_priv.ld_qmap_info;
	struct drm_mtk_tv_ldm_qmap_nodes *nodes = &param->qmap_nodes;
	struct mtk_tv_drm_metabuf metabuf = {0};
	uint32_t qmap_buf_size = 0;
	uint8_t i;
	int ret = 0;

	// covert dma_fd into metabuf
	memset(&metabuf, 0, sizeof(struct mtk_tv_drm_metabuf));
	ret = mtk_tv_drm_metabuf_alloc_by_ion(&metabuf, nodes->qmap_dma_fd);
	if (ret) {
		DRM_ERROR("[%s:%d] Convert DMA_FD %d fail, ret:%d", __func__, __LINE__,
			nodes->qmap_dma_fd, ret);
		return -EINVAL;
	}

	// check dma buffer size and expected size
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i)
		qmap_buf_size += nodes->qmap_num[i] * sizeof(uint16_t);

	if (metabuf.size < qmap_buf_size) {
		DRM_ERROR("[%s:%d] DMA size error, %u < %u", __func__, __LINE__,
			metabuf.size, qmap_buf_size);
		ret = -EINVAL;
		goto EXIT;
	}

	// use pqmap to convert cmds and write into ml
	ret = mtk_tv_drm_pqmap_write_ml(pqmap, ml, (uint16_t *)nodes->qmap_num, metabuf.addr);
	if (ret) {
		DRM_ERROR("[%s:%d] PQMAP fire , ret:%d", __func__, __LINE__, ret);
		ret = -EINVAL;
		goto EXIT;
	}

	// store qmap for str resume
	if (ld_qmap->node_buf != NULL) {
		kvfree(ld_qmap->node_buf);
		ld_qmap->node_buf = NULL;
	}
	for (i = 0; i < MTK_DRM_PQMAP_NUM_MAX; ++i)
		ld_qmap->node_num[i] = nodes->qmap_num[i];
	ld_qmap->buf_size = qmap_buf_size;
	ld_qmap->node_buf = kvmalloc(qmap_buf_size, GFP_KERNEL);
	if (ld_qmap->node_buf == NULL) {
		DRM_ERROR("[%s:%d] alloc %u fail", __func__, __LINE__, qmap_buf_size);
		ret = -ENOMEM;
		goto EXIT;
	}
	memcpy(ld_qmap->node_buf, metabuf.addr, qmap_buf_size);
EXIT:
	// destroy allocated resource
	mtk_tv_drm_metabuf_free(&metabuf);
	DRM_INFO("[%s:%d] ret:%d, dma_fd:%d", __func__, __LINE__, ret, nodes->qmap_dma_fd);
	return ret;
}

int mtk_tv_drm_ldm_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct mtk_tv_kms_context *pctx_kms = NULL;
	struct drm_mtk_tv_ldm_param *param = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct pqu_render_agent_param pqu_agent_input = {0};
	struct pqu_render_agent_param pqu_agent_output = {0};
	struct pqu_render_ldm_action_param *ldm_in = &pqu_agent_input.ldm_param;
	struct pqu_render_ldm_action_param *ldm_out = &pqu_agent_output.ldm_param;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_metabuf metabuf = {0};
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
	pctx_kms = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	param = (struct drm_mtk_tv_ldm_param *)data;

	BUILD_BUG_ON(E_MTK_TV_LDM_BLOCK_NUM_TYPE_LDF != E_PQU_RENDER_LDM_BLOCK_NUM_TYPE_LDF);
	BUILD_BUG_ON(E_MTK_TV_LDM_BLOCK_NUM_TYPE_LED != E_PQU_RENDER_LDM_BLOCK_NUM_TYPE_LED);
	BUILD_BUG_ON(E_MTK_TV_LDM_BLOCK_TYPE_LDF_MAX != E_PQU_RENDER_LDM_BLOCK_TYPE_LDF_MAX);
	BUILD_BUG_ON(E_MTK_TV_LDM_BLOCK_TYPE_LDF_AVG != E_PQU_RENDER_LDM_BLOCK_TYPE_LDF_AVG);
	BUILD_BUG_ON(E_MTK_TV_LDM_BLOCK_TYPE_LED     != E_PQU_RENDER_LDM_BLOCK_TYPE_LED);

	switch (param->action) {
	case E_MTK_TV_LDM_ACTION_GET_BLOCK_NUM:		/* struct drm_mtk_tv_ldm_block_num  */
		ldm_in->action = E_PQU_RENDER_LDM_GET_BLOCK_NUM;
		ldm_in->block_num.num_type =
			(enum pqu_render_ldm_block_num_type)param->block_num_info.num_type;
		ret = mtk_tv_drm_pqu_agent_sync_reply(E_PQU_RENDER_AGENT_CMD_LDM,
							&pqu_agent_input, &pqu_agent_output);
		param->block_num_info.width  = ldm_out->block_num.width;
		param->block_num_info.height = ldm_out->block_num.height;
		break;

	case E_MTK_TV_LDM_ACTION_GET_BLOCK_DATA:	/* struct drm_mtk_tv_ldm_block_data */
		ret = mtk_tv_drm_metabuf_alloc_by_ion(&metabuf, param->block_data_info.data_dma_fd);
		if (ret) {
			DRM_ERROR("[%s][%d] metabuf fail, fd = %d, ret = %d\n",
				__func__, __LINE__, param->block_data_info.data_dma_fd, ret);
			return ret;
		}
		ldm_in->action = E_PQU_RENDER_LDM_GET_BLOCK_DATA;
		ldm_in->block_data.data_type = (enum pqu_render_ldm_block_data_type)
						param->block_data_info.data_type;
		ldm_in->block_data.data_dma_addr = (uint64_t)(size_t)metabuf.addr;
		ret = mtk_tv_drm_pqu_agent_sync_reply(E_PQU_RENDER_AGENT_CMD_LDM,
							&pqu_agent_input, &pqu_agent_output);
		ret = pqu_agent_output.result;
		mtk_tv_drm_metabuf_free(&metabuf);
		break;

	case E_MTK_TV_LDM_ACTION_SET_DUTY:		/* __u8 duty */
		ldm_in->action = E_PQU_RENDER_LDM_SET_DUTY;
		ldm_in->duty   = param->duty;
		ret = mtk_tv_drm_pqu_agent_no_reply(E_PQU_RENDER_AGENT_CMD_LDM, &pqu_agent_input);
		break;

	case E_MTK_TV_LDM_ACTION_GET_DUTY:		/* __u8 duty */
		ldm_in->action = E_PQU_RENDER_LDM_GET_DUTY;
		ret = mtk_tv_drm_pqu_agent_sync_reply(E_PQU_RENDER_AGENT_CMD_LDM,
							&pqu_agent_input, &pqu_agent_output);
		param->duty = ldm_out->duty;
		break;

	case E_MTK_TV_LDM_ACTION_SET_VSYNC_FREQ:	/* __u32 vsync_freq */
		ldm_in->action     = E_PQU_RENDER_LDM_SET_VSYNC_FREQ;
		ldm_in->vsync_freq = param->vsync_freq;
		ret = mtk_tv_drm_pqu_agent_no_reply(E_PQU_RENDER_AGENT_CMD_LDM, &pqu_agent_input);
		break;

	case E_MTK_TV_LDM_ACTION_GET_VSYNC_FREQ:	/* __u32 vsync_freq */
		ldm_in->action = E_PQU_RENDER_LDM_GET_VSYNC_FREQ;
		ret = mtk_tv_drm_pqu_agent_sync_reply(E_PQU_RENDER_AGENT_CMD_LDM,
							&pqu_agent_input, &pqu_agent_output);
		param->vsync_freq = ldm_out->vsync_freq;
		break;

	case E_MTK_TV_LDM_ACTION_SET_QMAP:		/* struct drm_mtk_tv_ldm_qmap_nodes */
		ret = _mtk_tv_drm_ldm_ioctl_set_qmap(pctx_kms, param);
		break;

	case E_MTK_TV_LDM_ACTION_SET_STATUS:		/* int status */
		ldm_in->action = E_PQU_RENDER_LDM_SET_STATUS;
		ldm_in->status = param->status;
		ret = mtk_tv_drm_pqu_agent_no_reply(E_PQU_RENDER_AGENT_CMD_LDM, &pqu_agent_input);
		break;

	case E_MTK_TV_LDM_ACTION_GET_STATUS:		/* int status */
		ldm_in->action = E_PQU_RENDER_LDM_GET_STATUS;
		ret = mtk_tv_drm_pqu_agent_sync_reply(E_PQU_RENDER_AGENT_CMD_LDM,
							&pqu_agent_input, &pqu_agent_output);
		param->status = ldm_out->status;
		break;

	case E_MTK_TV_LDM_ACTION_SET_BOOST_EN:		/* bool boost_en */
		ldm_in->action = E_PQU_RENDER_LDM_SET_BOOST_EN;
		ldm_in->boost_en = param->boost_en;
		ret = mtk_tv_drm_pqu_agent_no_reply(E_PQU_RENDER_AGENT_CMD_LDM, &pqu_agent_input);
		break;

	default:
		ret = -ENODEV;
		break;
	}
	return ret;
}
