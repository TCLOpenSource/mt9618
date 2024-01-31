// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

//-------------------------------------------------------------------------------------------------
//  Include Files
//-------------------------------------------------------------------------------------------------
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/spi/spi.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/version.h>
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_ldm_leddevice.h"
#include "mtk_tv_drm_ld.h"

#include "drvLDM_sti.h"

//-----------------------------------------------------------------------------
//  Driver Compiler Options
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define NODE_PATH_MAX_LENGTH    16
#define MTK_DRM_MODEL MTK_DRM_MODEL_LD

//-----------------------------------------------------------------------------
//  Local Structurs
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Global Variables
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------




//////////////////AS3824 Define START///////////////////////
static char LED_Device_AS3824_unlock[] = {
	LDM_AS382X_BCAST_SINGLE_BYTE_SAME_DATA,
	LDM_AS382X_UNLOCK_ADDR,
	LDM_AS382X_UNLOCK_CMD,

	0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static char LED_Device_AS3824_lock[] = {
	LDM_AS382X_BCAST_SINGLE_BYTE_SAME_DATA,
	LDM_AS382X_UNLOCK_ADDR,
	LDM_AS382X_LOCK_CMD,

	0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

//////////////////////AS3824 Define END//////////////////////////


//////////////////////IW7039 Define START//////////////////////
//static u8 u8TailZero[IW7039_DEVICE_NUM]={0};



/////////////////////IW7039 Define END//////////////////////////


//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Debug Functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Global Functions
//-----------------------------------------------------------------------------

struct spi_device *km_spidev_node_get(char channel);

int32_t spi_xfer(unsigned char channel, struct spi_transfer *spixfer)
{

	struct spi_device *spi_node;
	int status;
	struct spi_message msg;

	spi_node = km_spidev_node_get(channel);
	spi_message_init(&msg);
	spi_message_add_tail(spixfer, &msg);
	status = spi_sync(spi_node, &msg);
	if (status) {
		DRM_ERROR(
		"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  status);

		return -1;
	}
	return 0;

}

char ldm_mspi_read_as3824_single_data(
	unsigned char channel,
	char u8deviceidx,
	char u8RegisterAddr,
	u8 u8SPI_Devicenum,
	u32 u32MspiClk)
{
	int32_t ret = 0;
	//char tx_cmd[TXSIZE] = {0};
	char rx_data[RXSIZE] = {0};
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];
	char LED_Device_AS3824_read_single_data[] = {
		LDM_AS382X_SINGLE_BYTE_SAME_DATA, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));

	LED_Device_AS3824_read_single_data[E_MSPI_DATA0] =
	(LDM_AS382X_SINGLE_BYTE_SAME_DATA | u8deviceidx);
	LED_Device_AS3824_read_single_data[E_MSPI_DATA1] =
	u8RegisterAddr;
	LED_Device_AS3824_read_single_data[E_MSPI_DATA2] =
	0x00;

	///////RX/////READ POWER

	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = u32MspiClk;
	spi_read[0].tx_buf = LED_Device_AS3824_read_single_data;
	spi_read[0].len = LDM_AS382X_SINGLE_READ_CMD_NUM + u8SPI_Devicenum;

	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = rx_data;
	spi_read[1].len = LDM_AS382X_SINGLE_READ_CMD_NUM + u8SPI_Devicenum;

	ret = spi_xfer(channel, (void *)&spi_read);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	return rx_data[0];

}

int32_t ldm_as3824_init(
	struct mtk_tv_kms_context *pctx,
	unsigned char channel,
	u8 u8SPI_Devicenum)
{
	int32_t ret = 0;
	int i = 0;
	char u8temp = 0;
	//char tx_cmd[TXSIZE] = {0};
	//char rx_data[RXSIZE] = {0};
	struct spi_transfer spi_write;
	u8 u8LDM_AS382X_Header_VDAC_num = 0;
	u8 u8LDM_AS382X_Header_FB_on1_num = 0;

	char LED_Device_AS3824_initial_Reg0x01to0x15[] = {
		LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA,
		Reg_0x01_to_0x15_NUM,
		LDM_AS382X_CUR_ON_1_ADDR,
		//addr 0x1~0x5 CUR_ON_1, CUR_ON_2 , FAULT_1, GPIO_CTIL, FB_SEL1
		0x00, 0x00, 0x00, 0x00, 0x00,
		 //addr 0x6~0x9 , FB_SEL2, CURRctrl, SHORTLED1, SHORTLED2
		0x00, 0x00, 0x00, 0x00,
		 //addr 0xA~0xD, OPENLED1, OPENLED2, VDAC_H, VDAC_L
		0x00, 0x00, 0x00, 0x00,
		 //addr 0xE~0x11, FB_ON_1, FB_ON_2, IDAC_FB1_COUNTER, IDAC_FB2_COUNTER
		0x00, 0x00, 0x00, 0x00,
		//addr 0x12~0x15, FBLOOP_CTRL, PWMCTRL, PWMperiodLSB, PWMperiodMSB
		0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_AS3824_initial_PLLmulti[] = {
		LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA,
		PLL_MULTI_BYTE_NUM,
		LDM_AS382X_PLLmulti1_ADDR,
		0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_AS3824_initial_HDR[] = {
		LDM_AS382X_BCAST_SINGLE_BYTE_SAME_DATA,
		LDM_AS382X_HDR_ADDR,
		LDM_AS382X_BDAC_ON,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_AS3824_initial_PWM_Delay[] = {
		LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA,
		LDM_AS382X_DELAY_NUM,
		LDM_AS382X_DELAY1_ADDR, //addr 0x16~0x35
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_AS3824_initial_PWM_Duty[] = {
		LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA,
		LDM_AS382X_PWM_HTIME_NUM,
		LDM_AS382X_PWM_HTIME1_ADDR, //addr 0x37~0x56
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_AS3824_initial_BDAC[] = {
		LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA,
		LDM_AS382X_BDAC_NUM,
		LDM_AS382X_BDAC1_ADDR, //addr 0x37~0x56
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_as3824_info *pld_led_device_as3824_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_as3824_info = &(pctx->ld_priv.ld_led_device_as3824_info);

	// Wait 3824 power on
	// check 0xff for Nova case
	DRM_INFO("Wait AS3824 Power");

	do {
		u8temp = ldm_mspi_read_as3824_single_data(channel,
			0x01,
			LDM_AS382X_STATUS_ADDR,
			u8SPI_Devicenum,
			pld_mspi_info->u32MspiClk);
		DRM_INFO("check as3824 power %x:", u8temp);
	} while ((u8temp == NT50516_POWER_ON) || (u8temp & BIT0) == 0);
	DRM_INFO("AS3824 power on");
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = pld_mspi_info->u32MspiClk;

	/*unlock device*/
	spi_write.tx_buf = LED_Device_AS3824_unlock;
	spi_write.len = LDM_AS382X_BCAST_SINGLE_WRITE_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial reg0x01~0x15*/
	for (i = 0; i < Reg_0x01_to_0x0B_NUM ; i++) {
		LED_Device_AS3824_initial_Reg0x01to0x15[LDM_AS382X_HEADER_NUM + i] =
		pld_led_device_as3824_info->u8AS3824_Reg_0x01_to_0x0B[i];
	}

	u8LDM_AS382X_Header_VDAC_num =
	LDM_AS382X_HEADER_NUM + LDM_AS382X_VDAC_LEDH_ADDR - 1;

	for (i = 0; i < VDAC_BYTE_NUM ; i++) {
		LED_Device_AS3824_initial_Reg0x01to0x15[u8LDM_AS382X_Header_VDAC_num + i] =
			pld_led_device_as3824_info->u8AS3824_Reg_0x0C_0x0D_VDAC_MBR_OFF[i];
	}

	u8LDM_AS382X_Header_FB_on1_num =
	LDM_AS382X_HEADER_NUM + LDM_AS382X_FB_ON1_ADDR - 1;

	for (i = 0; i < Reg_0x0E_to_0x15_NUM ; i++) {
		LED_Device_AS3824_initial_Reg0x01to0x15[u8LDM_AS382X_Header_FB_on1_num + i] =
			pld_led_device_as3824_info->u8AS3824_Reg_0x0E_to_0x15[i];
	}

	for (i = 0; i < LDM_AS382X_PWM_HTIME_NUM ; i++) {
		LED_Device_AS3824_initial_PWM_Duty[LDM_AS382X_HEADER_NUM + i] =
			(u8)pld_led_device_as3824_info->u16AS3824_PWM_Duty_Init & MASK_2BYTES;
		LED_Device_AS3824_initial_PWM_Duty[LDM_AS382X_HEADER_NUM + i + 1] =
			(u8)(pld_led_device_as3824_info->u16AS3824_PWM_Duty_Init >> SHIFT_2BYTES);
	}

	spi_write.tx_buf = LED_Device_AS3824_initial_Reg0x01to0x15;
	spi_write.len = LDM_AS382X_WRITE_SAME_DATA_REG_0x01_0x15_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial PLLmulti*/
	spi_write.tx_buf = LED_Device_AS3824_initial_PLLmulti;
	spi_write.len = LDM_AS382X_WRITE_SAME_DATA_PLL_MULTI_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial HDR*/
	spi_write.tx_buf = LED_Device_AS3824_initial_HDR;
	spi_write.len = LDM_AS382X_BCAST_SINGLE_WRITE_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial PWM_delay*/
	spi_write.tx_buf = LED_Device_AS3824_initial_PWM_Delay;
	spi_write.len = LDM_AS382X_WRITE_SAME_DATA_PWM_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial PWM_Duty*/
	spi_write.tx_buf = LED_Device_AS3824_initial_PWM_Duty;
	spi_write.len = LDM_AS382X_WRITE_SAME_DATA_PWM_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*initial BDAC*/
	spi_write.tx_buf = LED_Device_AS3824_initial_BDAC;
	spi_write.len = LDM_AS382X_WRITE_SAME_DATA_BDAC_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*lock device*/
	spi_write.tx_buf = LED_Device_AS3824_lock;
	spi_write.len = LDM_AS382X_BCAST_SINGLE_WRITE_CMD_NUM + u8SPI_Devicenum;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	return ret;

}

void _ldm_as3824_init(struct mtk_tv_kms_context *pctx, u8 u8Mspi_Max_Num)
{
	u8 i = 0;
	int32_t ret = 0;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return;
	}

	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);

	DRM_INFO("[LDM]LD Device AS3824 init start\n");
	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			ret = ldm_as3824_init(pctx, i, pld_led_device_info->u8Device_Num[i]);
			if (!ret)
				DRM_INFO("[LDM]AS3824 init spi ch = %d done\n", i);
		}
	}
}

void ldm_mspi_write_one_iw7039_single_data(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u16 u16InputdData,
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum / IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u8 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE >> SHIFT_2BYTES) + u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE & MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] = (u8)(u16Temp >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] = (u8)((u16Temp & MASK_2BYTES));

	// data
	pLocalBuffer[E_MSPI_DATA4] = (u8)(u16InputdData >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] = (u8)((u16InputdData  & MASK_2BYTES));

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA6 + BYTE2 * i] = 0;
		pLocalBuffer[E_MSPI_DATA6 + BYTE2 * i + 1] = 0;
	}

	// Send SPI command

	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA6 + BYTE2 * u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}

void ldm_mspi_write_one_iw7039_single_data_chksum(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u16 u16InputdData,
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u8 i = 0;
	u16 u16ChkSum = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE >> SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
		(u8)(COMMAND_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE & MASK_2BYTES);
	u16Temp =
	pLocalBuffer[E_MSPI_DATA0]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[E_MSPI_DATA1];
	u16ChkSum += u16Temp;

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] = (u8)(u16Temp >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] = (u8)((u16Temp & MASK_2BYTES));
	u16ChkSum += u16Temp;

	// data
	pLocalBuffer[E_MSPI_DATA4] = (u8)(u16InputdData >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] = (u8)((u16InputdData & MASK_2BYTES));
	u16Temp =
	pLocalBuffer[E_MSPI_DATA4]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[E_MSPI_DATA5];
	u16ChkSum += u16Temp;

	u16ChkSum = ~u16ChkSum;
	pLocalBuffer[E_MSPI_DATA6] = (u8)(u16ChkSum >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA7] = (u8)((u16ChkSum & MASK_2BYTES));

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA8 + BYTE2 * i] = 0;
		pLocalBuffer[E_MSPI_DATA8 + BYTE2 * i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA8+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}


void ldm_mspi_write_one_iw7039_32word_array_data(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u16 a16Data[IW7039_32WORD],
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_WRITE_32_WORD_OF_ONE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
		(u8)(COMMAND_IW7039_WRITE_32_WORD_OF_ONE_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_32_WORD_OF_ONE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] = (u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] = (u8)((u16Temp&MASK_2BYTES));

	// data
	for (i = 0; i < IW7039_32WORD; i++) {
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i] = (u8)(a16Data[i]>>SHIFT_2BYTES);
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1] = (u8)((a16Data[i]&MASK_2BYTES));
	}

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}

void ldm_mspi_write_one_iw7039_32word_array_data_chk_sum(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u16 a16Data[IW7039_32WORD],
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	u16 u16ChkSum = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	// GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_WRITE_32_WORD_OF_ONE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_32_WORD_OF_ONE_DEVICE&MASK_2BYTES);
	u16Temp =
pLocalBuffer[E_MSPI_DATA0]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[E_MSPI_DATA1];
	u16ChkSum += u16Temp;

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_32_WORD_OF_ONE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));
	u16ChkSum += u16Temp;

	// data
	for (i = 0; i < IW7039_32WORD; i++) {
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i] = (u8)(a16Data[i]>>SHIFT_2BYTES);
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1] = (u8)((a16Data[i]&MASK_2BYTES));
		u16Temp =
pLocalBuffer[E_MSPI_DATA4+BYTE2*i]*HIGH_BYTE_16BIT_BASE +
pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1];
		u16ChkSum += u16Temp;
	}

	u16ChkSum = ~u16ChkSum;
	pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2] = (u8)(u16ChkSum>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+1] = (u8)((u16ChkSum&MASK_2BYTES));

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len =
	(E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
		"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}


void ldm_mspi_write_all_iw7039_same_32word_array_data(
	unsigned char channel,
	u8 u8ChipNum,
	u16 u16RegisterAddr,
	u16 a16Data[IW7039_32WORD],
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
		"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));

	// data
	for (i = 0; i < IW7039_32WORD; i++) {
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i] = (u8)(a16Data[i]>>SHIFT_2BYTES);
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1] = (u8)((a16Data[i]&MASK_2BYTES));
	}

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA4+IW7039_32WORD*BYTE2+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}



	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}

void ldm_mspi_write_all_iw7039_same_32word_array_data_chk_sum(
	unsigned char channel,
	u8 u8ChipNum,
	u16 u16RegisterAddr,
	u16 a16Data[IW7039_32WORD],
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	u16 u16ChkSum = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;


	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE&MASK_2BYTES);
	u16Temp = pLocalBuffer[0]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[1];
	u16ChkSum += u16Temp;

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));
	u16ChkSum += u16Temp;

	// data
	for (i = 0; i < IW7039_32WORD; i++) {
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i] = (u8)(a16Data[i]>>SHIFT_2BYTES);
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1] = (u8)((a16Data[i]&MASK_2BYTES));
		u16Temp =
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i]*HIGH_BYTE_16BIT_BASE +
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1];
		u16ChkSum += u16Temp;
	}

	u16ChkSum = ~u16ChkSum;
	pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2] = (u8)(u16ChkSum>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+1] = (u8)((u16ChkSum&MASK_2BYTES));

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA4+IW7039_32WORD*BYTE2+IW7039_CHK_SUM_NUM+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}


void ldm_mspi_write_all_iw7039_same_single_data(
	unsigned char channel,
	u8 u8ChipNum,
	u16 u16RegisterAddr,
	u16 u16InputdData,
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u8 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));

	// data
	pLocalBuffer[E_MSPI_DATA4] =
	(u8)(u16InputdData>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] =
	(u8)((u16InputdData&MASK_2BYTES));

	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA6+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}


u16 ldm_mspi_read_iw7039_single_data(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u32 u32MspiClk)
{
	u8 au8InputdData[IW7039_READ_BUF_NUM] = {0};
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 u16ReadVal = 0;
	u8 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];

	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData number of device wrong!\n");
		return 0;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return 0;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_READ_SINGLE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));

	// data dummy
	pLocalBuffer[E_MSPI_DATA4] =
	(u8)(DATA_DUMMY_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] =
	(u8)((DATA_DUMMY_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES));

	// device dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i+1] = 0;
	}


		///////RX/////READ POWER

	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = u32MspiClk;
	spi_read[0].tx_buf = pLocalBuffer;
	spi_read[0].len = (E_MSPI_DATA6+BYTE2*u8DummyNo);

	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = au8InputdData;
	spi_read[1].len = (E_MSPI_DATA6+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_read);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);

	u16ReadVal = au8InputdData[E_MSPI_DATA6];
	u16ReadVal = u16ReadVal*HIGH_BYTE_16BIT_BASE + au8InputdData[E_MSPI_DATA7];

	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);

	return u16ReadVal;
}

u16 ldm_mspi_read_iw7039_single_data_chk_sum(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u32 u32MspiClk)
{
	u8 au8InputdData[IW7039_READ_BUF_NUM] = {0};
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 u16ReadVal = 0;
	u8 i = 0;
	u16 u16ChkSum = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];

	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData number of device wrong!\n");
		return 0;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return 0;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES);
	u16Temp = pLocalBuffer[0]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[1];
	u16ChkSum += u16Temp;

	// address
	u16Temp = (ADDRESS_IW7039_READ_SINGLE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));
	u16ChkSum += u16Temp;

	u16ChkSum = ~u16ChkSum;
	pLocalBuffer[E_MSPI_DATA4] =
	(u8)(u16ChkSum>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] =
	(u8)((u16ChkSum&MASK_2BYTES));

	// read data dummy
	pLocalBuffer[E_MSPI_DATA6] =
	(u8)(DATA_DUMMY_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA7] =
	(u8)((DATA_DUMMY_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES));

	// device dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA8+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA8+BYTE2*i+1] = 0;
	}

	///////RX/////READ POWER

	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = u32MspiClk;
	spi_read[0].tx_buf = pLocalBuffer;
	spi_read[0].len = (E_MSPI_DATA8+BYTE2*u8DummyNo);

	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = au8InputdData;
	spi_read[1].len = (E_MSPI_DATA8+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_read);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);

	u16ReadVal = au8InputdData[E_MSPI_DATA6];
	u16ReadVal = u16ReadVal*HIGH_BYTE_16BIT_BASE + au8InputdData[E_MSPI_DATA7];

	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);

	return u16ReadVal;
}


u16 ldm_mspi_read_iw7039_single_data_v2(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u32 u32MspiClk)
{
	u8 au8InputdData[IW7039_READ_BUF_NUM] = {0};
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 u16ReadVal = 0;
	u8 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];

	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));


	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData number of device wrong!\n");
		return 0;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return 0;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_READ_SINGLE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] = (u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] = (u8)((u16Temp&MASK_2BYTES));

	// read data dummy
	//pLocalBuffer[4] = (u8)(DATA_DUMMY_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES);
	//pLocalBuffer[5] = (u8)((DATA_DUMMY_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES));

	// device dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA4+BYTE2*i+1] = 0;
	}

		///////RX/////READ POWER
	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = u32MspiClk;
	spi_read[0].tx_buf = pLocalBuffer;
	spi_read[0].len = (E_MSPI_DATA4+BYTE2*u8DummyNo);

	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = au8InputdData;
	spi_read[1].len = E_MSPI_DATA2;

	ret = spi_xfer(channel, (void *)&spi_read);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

// SPI SW mode, pull low VSync GPIO7
//GPIO_SetOut(7, 0);
/*
 *for(i=0 ; i < 20 ; i++)
 *{
 *	DRM_INFO("read iw7039[%x]= %x\n",i, au8InputdData[i]);
 *}
 */
	u16ReadVal = au8InputdData[1] | ((au8InputdData[0]&MASK_2BYTES)<<SHIFT_2BYTES);

	//u16ReadVal = au8InputdData[6];
	//u16ReadVal = u16ReadVal*0x100 + au8InputdData[7];
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);

	return u16ReadVal;

}

u16 ldm_mspi_read_iw7039_single_data_v2_chk_sum(
	unsigned char channel,
	u8 u8ChipNum,
	u8 u8DeviceIdx,
	u16 u16RegisterAddr,
	u32 u32MspiClk)
{
	u8 au8InputdData[IW7039_READ_BUF_NUM] = {0};
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 u16ReadVal = 0;
	u8 i = 0;
	u16 u16ChkSum = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];

	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData number of device wrong!\n");
		return 0;
	}

	if (u8DeviceIdx > IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Read_Iw7039_SingleData read device no. wrong!\n");
		return 0;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)((COMMAND_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES)+u8DeviceIdx);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES);
	u16Temp = pLocalBuffer[0]*HIGH_BYTE_16BIT_BASE + pLocalBuffer[1];
	u16ChkSum += u16Temp;

	// address
	u16Temp = (ADDRESS_IW7039_READ_SINGLE_DEVICE + u16RegisterAddr);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));
	u16ChkSum += u16Temp;

	u16ChkSum = ~u16ChkSum;
	pLocalBuffer[E_MSPI_DATA4] =
	(u8)(u16ChkSum>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA5] =
	(u8)((u16ChkSum&MASK_2BYTES));

	// read data dummy
	//pLocalBuffer[6] = (u8)(DATA_DUMMY_IW7039_READ_SINGLE_DEVICE>>SHIFT_2BYTES);
	//pLocalBuffer[7] = (u8)((DATA_DUMMY_IW7039_READ_SINGLE_DEVICE&MASK_2BYTES));

	// device dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA6+BYTE2*i+1] = 0;
	}

		///////RX/////READ POWER
	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = u32MspiClk;
	spi_read[0].tx_buf = pLocalBuffer;
	spi_read[0].len = (E_MSPI_DATA6 + BYTE2 * u8DummyNo);

	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = au8InputdData;
	spi_read[1].len = (E_MSPI_DATA6 + BYTE2 * u8DummyNo);

		ret = spi_xfer(channel, (void *)&spi_read);
		if (ret) {
			DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
		}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);

	u16ReadVal = au8InputdData[E_MSPI_DATA6];
	u16ReadVal = u16ReadVal * HIGH_BYTE_16BIT_BASE + au8InputdData[E_MSPI_DATA7];

	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);

	return u16ReadVal;
}


void ldm_mspi_write_iw7039_set_daisy_chain_chip_num(
	unsigned char channel,
	u8 u8ChipNum,
	u32 u32MspiClk)
{
	u8 u8DummyNo = u8ChipNum/IW7039_DUMMY_CAL_NUM+1;
	u8 i = 0;
	char *pLocalBuffer = NULL;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	if (u8ChipNum > IW7039_DAISY_CHAIN_CHIP_NUM_MAX) {
		DRM_INFO(
"[MSPI_INIT]MSPI_Write_Iw7039_SetDaisyChainChipNum number of device wrong!\n");
		return;
	}

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);
	if (pLocalBuffer == NULL) {
		DRM_ERROR("error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// SPI SW mode, pull high VSync GPIO7
	//GPIO_SetOut(7, 1);

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_SET_NUM_OF_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(u8ChipNum);

	// device dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA2+BYTE2*i] = 0;
		pLocalBuffer[E_MSPI_DATA2+BYTE2*i+1] = 0;
	}

	// Send SPI command
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA2+BYTE2*u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	// SPI SW mode, pull low VSync GPIO7
	//GPIO_SetOut(7, 0);
	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);
}


u8 ldm_iw7039_send_duty_by_sw_spi(
	unsigned char channel,
	unsigned long u32VAddr,
	unsigned long u32DataNum,
	u8 u8Devicenum,
	u32 u32MspiClk)
{
	u8 *pu8Inputdata;
	char *pLocalBuffer = NULL;
	u8 u8DummyNo = u8Devicenum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	pu8Inputdata = (u8 *)u32VAddr;

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);

	if (pLocalBuffer == NULL) {
		DRM_ERROR(
"error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE + IW7039_PWM_HTIME1_ADDR);
	pLocalBuffer[E_MSPI_DATA2] =
	(u8)(u16Temp>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] =
	(u8)((u16Temp&MASK_2BYTES));

	// data
	for (i = 0; i < u32DataNum; i++) {
		pLocalBuffer[E_MSPI_DATA4 + BYTE2 * i] =
		(u8)((pu8Inputdata[BYTE2 * i + 1] & MASK_2BYTES));//high byte
		pLocalBuffer[E_MSPI_DATA4 + BYTE2 * i + 1] =
		(u8)(pu8Inputdata[BYTE2 * i] & MASK_2BYTES);  //low byte
	}
	// dummy
	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * i] = 0;
		pLocalBuffer[E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * i + 1] = 0;
	}

	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	return ret;
}


u8 ldm_iw7039_send_iset_by_sw_spi(
	unsigned char channel,
	unsigned long u32VAddr,
	unsigned long u32DataNum,
	u8 u8Devicenum,
	u32 u32MspiClk)
{
	u8 *pu8Inputdata;
	char *pLocalBuffer = NULL;
	u8 u8DummyNo = u8Devicenum/IW7039_DUMMY_CAL_NUM+1;
	u16 u16Temp = 0;
	u16 i = 0;
	int32_t ret = 0;
	struct spi_transfer spi_write;

	pu8Inputdata = (u8 *)u32VAddr;

	pLocalBuffer = kmalloc_array(LINE_BUFFERSIZE, sizeof(char), GFP_KERNEL);

	if (pLocalBuffer == NULL) {
		DRM_ERROR("error! %s:%d, malloc allocate fail\n", __func__, __LINE__);
		return -1;
	}
	memset(pLocalBuffer, 0, LINE_BUFFERSIZE * sizeof(char));

	// command
	pLocalBuffer[E_MSPI_DATA0] =
	(u8)(COMMAND_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE>>SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA1] =
	(u8)(COMMAND_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE&MASK_2BYTES);

	// address
	u16Temp = (ADDRESS_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE + IW7039_ISET_ADDR);
	pLocalBuffer[E_MSPI_DATA2] = (u8)(u16Temp >> SHIFT_2BYTES);
	pLocalBuffer[E_MSPI_DATA3] = (u8)((u16Temp & MASK_2BYTES));

	// data
	for (i = 0; i < u32DataNum; i++) {
		pLocalBuffer[E_MSPI_DATA4 + BYTE2 * i] =
		(u8)((pu8Inputdata[BYTE2 * i + 1] & MASK_2BYTES));//high byte
		pLocalBuffer[E_MSPI_DATA4 + BYTE2 * i + 1] =
		(u8)(pu8Inputdata[BYTE2 * i] & MASK_2BYTES);  //low byte
	}

	for (i = 0; i < u8DummyNo; i++) {
		pLocalBuffer[E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * i] = 0;
		pLocalBuffer[E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * i + 1] = 0;
	}

	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.rx_buf = NULL;
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz = u32MspiClk;
	spi_write.tx_buf = pLocalBuffer;
	spi_write.len = (E_MSPI_DATA4 + u32DataNum * BYTE2 + BYTE2 * u8DummyNo);

	ret = spi_xfer(channel, (void *)&spi_write);

	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	if (pLocalBuffer != NULL)
		kfree((void *)pLocalBuffer);

	return ret;
}


int32_t ldm_iw7039_init_turn_on_backlight(
	struct mtk_tv_kms_context *pctx,
	unsigned char channel,
	u8 u8SPI_Devicenum)
{
	int32_t ret = 0;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);

#if IW7039_SIMPLE_CHKSUM
	//12.Soft start IW7039 calibration process
	ldm_mspi_write_all_iw7039_same_single_data(channel,
	u8SPI_Devicenum,
	0x000,
	IW7039_FINISH_INIT_REG0_W_CHK,
	pld_mspi_info->u32MspiClk);
#else
	//12.Soft start IW7039 calibration process
	ldm_mspi_write_all_iw7039_same_single_data(channel,
	u8SPI_Devicenum,
	0x000,
	IW7039_FINISH_INIT_REG0_WO_CHK,
	pld_mspi_info->u32MspiClk);
	//msleep(SLEEP_TIME_1);
#endif

	DRM_INFO("pqu_ldm_iw7039_init_turn_on_backlight end\n");

	return ret;

}

int32_t ldm_iw7039_init(
	struct mtk_tv_kms_context *pctx,
	unsigned char channel,
	u8 u8SPI_Devicenum)
{
	int32_t ret = 0;
	u16 u16ReadVal = 0;
	u8 i = 0;
	u8 u8retry = 0;

	u16 intReg_Conf[Reg_0x00_to_0x1F_NUM] = {
/*0X00 ~ 0X07 , reg 0x001 config to use global ISET with 30mA,
 *reg 0x05 config to 4x Vsync
 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0X08 ~ 0X0F ( 0x20 set ILED range as 60mA)*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0X10 ~ 0X17 , Reg 0x10 , 0x11 enable 32 CHs, Reg 0x14 turn-off all fault,
 *ADJ-short does not turn off
 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*0X18 ~0X1F , reg 0x18 and 0x19 enable ADP for 32 CHs*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	/*Initializing*/

	/*Setting individual ILED*/
	u16 intReg_ISET[IW7039_PWM_ISET_NUM] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x20 ~ 0x27
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x28 ~ 0x2F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x30 ~ 0x37
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x38 ~ 0x3F
	};
	/*0x20 to 0x3F*/

	u16 intReg_HT[IW7039_PWM_HTIME_NUM] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x60 ~ 0x67
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x68 ~ 0x6F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x70 ~ 0x77
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,// 0x78 ~ 0x7F
	};
	//0x60 to 0x7F

	/*Delay time of each CH*/
	u16 intReg_DT[IW7039_PWM_DT_NUM] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x40 ~0x47
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x48 ~ 0x4F
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x50 ~ 0x57
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //0x58 ~ 0x5F
	};//0x40 to 0x5F

	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);

	msleep(SLEEP_TIME_100);

	//2.Set EN(OPCTRL23) of IW7039 to high

	//3.Dealy >= 8.9 ms
	DRM_INFO("pqu_ldm_iw7039_init start\n");
	msleep(SLEEP_TIME_10);

/*6.Supply stable and correct Vsync to IW7039, write/read function generate vsync
 *4.1 Send data 0xFE07 to the daisy chain,
 *Set the align command by daisy chain,
 *depended on how many device.
 */
	ldm_mspi_write_iw7039_set_daisy_chain_chip_num(channel,
	u8SPI_Devicenum,
	pld_mspi_info->u32MspiClk);
	msleep(SLEEP_TIME_10);

/*4.2/4.3 read back the chip 1 SN number in 0x344[15:8] is 0x8 or not?
 *default SPI mode is normal mode
 */

	DRM_INFO("check IW7039 1 power\n");
	do {
		u16ReadVal = ldm_mspi_read_iw7039_single_data_v2(channel,
			u8SPI_Devicenum,
			1,
			IW7039_0x344_ADDR,
			pld_mspi_info->u32MspiClk);
		u16ReadVal = (u16ReadVal>>SHIFT_2BYTES)&MASK_2BYTES;
		u8retry++;
	} while ((u16ReadVal != IW7039_POWER_ON) && (u8retry <= RETRY_COUNTER));
	DRM_INFO("IW7039 1 power: %x\n", u16ReadVal);

/*5.Supply stable and correct Hsync to IW7039
 *no need after discuss with SP
 */

/*7.Configure SPI verification mode, set SPI mode to normal mode*/
	ldm_mspi_write_all_iw7039_same_single_data(channel,
	u8SPI_Devicenum,
	0x000,
	IW7039_INIT_REG0_VALUE,
	pld_mspi_info->u32MspiClk);
	msleep(SLEEP_TIME_10);

/*8.Unlock, release protected register, 0xA5FF also is default
 *	pqu_ldm_mspi_write_all_iw7039_same_single_data(u8SPI_Devicenum, 0x0A0, 0xA5FF);
 */
	DRM_INFO("IW7039 init table\n");
	for (i = 0; i < Reg_0x00_to_0x1F_NUM ; i++)
		intReg_Conf[i] = pld_led_device_iw7039_info->u16IW7039_Reg_0x000_to_0x01F[i];

	ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
		u8SPI_Devicenum,
		0x000,
		intReg_Conf,
		pld_mspi_info->u32MspiClk);
	msleep(SLEEP_TIME_10);

	DRM_INFO("IW7039 init ISET\n");
	for (i = 0; i < IW7039_PWM_ISET_NUM ; i++)
		intReg_ISET[i] = pld_led_device_iw7039_info->u16IW7039_ISET_MBR_OFF;


/*Init ISET 0x20 ~ 0x3F*/
	ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
	u8SPI_Devicenum,
	IW7039_ISET_ADDR,
	intReg_ISET,
	pld_mspi_info->u32MspiClk);
	msleep(SLEEP_TIME_10);

/*Init HT 0x60 ~ 0x7F to 0 before read calibration state*/
	DRM_INFO("IW7039 init DUTY\n");

	if (1) {
		for (i = 0; i < IW7039_PWM_HTIME_NUM ; i++)
			intReg_HT[i] = pld_led_device_iw7039_info->u16IW7039_PWM_Duty_Init;

		ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
			u8SPI_Devicenum,
			IW7039_PWM_HTIME1_ADDR,
			intReg_HT,
			pld_mspi_info->u32MspiClk);
		msleep(SLEEP_TIME_10);
	} else {
		ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
			u8SPI_Devicenum,
			IW7039_PWM_HTIME1_ADDR,
			intReg_HT,
			pld_mspi_info->u32MspiClk);

	}

/*Init DT 0x40 ~ 0x5F*/
		ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
			u8SPI_Devicenum,
			IW7039_DT_ADDR,
			intReg_DT,
			pld_mspi_info->u32MspiClk);

/*10.Lock register: enable the DT register write protection
 *	ldm_mspi_write_all_iw7039_same_single_data(
 *	u8SPI_Devicenum,
 *	0x0A0,
 *	0xA5FF,
 *	pld_mspi_info->u32MspiClk);
 */


/*9.init CONFIG, ISET, DT, HT
 *Init CONFIG 0x0 ~ 0x1F
 */


/*11.Delay >= 100 ms*/
	msleep(SLEEP_TIME_100);


#if IW7039_SIMPLE_CHKSUM
/*12.Soft start IW7039 calibration process
 *
 *	ldm_mspi_write_all_iw7039_same_single_data(
 *	channel,
 *	u8SPI_Devicenum,
 *	0x000,
 *	IW7039_FINISH_INIT_REG0_W_CHK,
 *	pld_mspi_info->u32MspiClk);
 */
#else
/*12.Soft start IW7039 calibration process
 *	ldm_mspi_write_all_iw7039_same_single_data(
 *	channel,
 *	u8SPI_Devicenum,
 *	0x000,
 *	IW7039_FINISH_INIT_REG0_WO_CHK,
 *	pld_mspi_info->u32MspiClk);
 *
 *msleep(SLEEP_TIME_1);
 */
#endif

	DRM_INFO("IW7039 Calibration\n");
/*13.Calibration done or not?
 *
 *	for(j=1; j<8; j++)
 *	{
 *		for(i=0; i<20; i++)
 *		{
 *#if IW7039_SIMPLE_CHKSUM
 *			u16ReadVal = ldm_mspi_read_iw7039_single_data_v2_chk_sum(
 *			channel,
 *			u8SPI_Devicenum,
 *			j,
 *			0x34A,
 *			pld_mspi_info->u32MspiClk);
 *#else
 *			u16ReadVal = ldm_mspi_read_iw7039_single_data_v2(
 *			channel,
 *			u8SPI_Devicenum,
 *			j, 0x34A,
 *			pld_mspi_info->u32MspiClk);
 *#endif
 *			u16ReadVal = (u16ReadVal>>9)&0x3F;
 *			DRM_INFO("check Calibration %x:", u16ReadVal);
 *
 *			if(0x10 == u16ReadVal)
 *			{
 *				DRM_INFO(
 *"[MSPI_INIT]kernel device[%d] calibration done, break, i = %d\n", j, i);
 *				break;
 *			}
 *		}
 *	}
 *
 *	DRM_INFO("IW7039 init duty");
 *
 *ldm_mspi_write_all_iw7039_same_32word_array_data(channel,
 *	u8SPI_Devicenum,
 *	IW7039_PWM_HTIME1_ADDR,
 *	intReg_HT,
 *	pld_mspi_info->u32MspiClk);
 *
 *14.Enter OTF Status: MCU can start to update the local dimming data
 */

	DRM_INFO("pqu_ldm_iw7039_init end\n");

	return ret;

}

void _ldm_iw7039_init(struct mtk_tv_kms_context *pctx, u8 u8Mspi_Max_Num)
{
	u8 i = 0;
	int32_t ret = 0;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return;
	}

	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);

	DRM_INFO("[LDM]LD Device IW7039 init start\n");
	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			ret = ldm_iw7039_init(pctx,
				i, pld_led_device_info->u8Device_Num[i]);
			if (!ret)
				DRM_INFO("[LDM]IW7039 init spi ch = %d done\n", i);
		}
	}
	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			ret = ldm_iw7039_init_turn_on_backlight(pctx,
				i, pld_led_device_info->u8Device_Num[i]);
			if (!ret)
				DRM_INFO("[LDM]IW7039 init turn on spi ch = %d done\n", i);
		}
	}
}

int32_t ldm_mbi6353_init(
	struct mtk_tv_kms_context *pctx,
	unsigned char channel,
	u8 u8SPI_Devicenum)
{
	int32_t ret = 0;
	struct spi_transfer spi_write;
	int i = 0;

	char LED_Device_MBI6353_initial_Mask[] = {
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_H,
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_L,
		0x00,
		MBI6353_MASK_NUM, MBI6353_MASK_ADDR_H,
		MBI6353_MASK_ADDR_L,//header

		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //data

		0x00, 0x00, //check sum

		0x00, 0x00, 0x00, 0x00, //dummy
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_MBI6353_initial_Config[] = {
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_H,
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_L,
		0x00,
		MBI6353_CFG_NUM,
		MBI6353_CFG_ADDR_H,
		MBI6353_CFG_ADDR_L,//header

		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, //check sum

		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	char LED_Device_MBI6353_initial_Duty[] = {
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_H,
		MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_L,
		0x00,
		MBI6353_DUTY_NUM,
		MBI6353_SCAN1_DUTY_H,
		MBI6353_SCAN1_DUTY_L,//header

		// 192 = 48 X 4 SCAN
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, //check sum
		0x00, 0x00, 0x00, 0x00, 0x00, //Dummy*25
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
	};

	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_mbi6353_info *pld_led_device_mbi6353_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &pctx->ld_priv;
	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_mbi6353_info = &(pctx->ld_priv.ld_led_device_mbi6353_info);

/*initial Mask*/
	for (i = 0; i < MBI6353_MASK_NUM * BYTE2; i++) {
		LED_Device_MBI6353_initial_Mask[(MBI6353_HEADER_NUM * BYTE2) + i] =
			pld_led_device_mbi6353_info->u8MBI6353_Mask[i];
	}

	for (i = 0; i < MBI6353_CFG_NUM * BYTE2 ; i++) {
		LED_Device_MBI6353_initial_Config[(MBI6353_HEADER_NUM * BYTE2) + i] =
			pld_led_device_mbi6353_info->u8MBI6353_Config[i];
	}

	for (i = 0; i < MBI6353_DUTY_NUM; i++) {
		LED_Device_MBI6353_initial_Duty[(MBI6353_HEADER_NUM * BYTE2) + (BYTE2 * i)] =
			(u8)(pld_led_device_mbi6353_info->u16MBI6353_PWM_Duty_Init >> SHIFT_2BYTES);
		LED_Device_MBI6353_initial_Duty[(MBI6353_HEADER_NUM * BYTE2) + (BYTE2 * i) + 1] =
			(u8)(pld_led_device_mbi6353_info->u16MBI6353_PWM_Duty_Init & MASK_2BYTES);
	}

	/*WRITE SPI*/
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz	  = pld_mspi_info->u32MspiClk;
	spi_write.rx_buf = NULL;

	/*SEND MASK*/
	spi_write.tx_buf = LED_Device_MBI6353_initial_Mask;
	spi_write.len = (MBI6353_HEADER_NUM + MBI6353_MASK_NUM +
		MBI6353_CHECKSUM_NUM + u8SPI_Devicenum - 1) * BYTE2;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
		"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*SEND CONFIG*/
	spi_write.tx_buf = LED_Device_MBI6353_initial_Config;
	spi_write.len = (MBI6353_HEADER_NUM + MBI6353_CFG_NUM +
		MBI6353_CHECKSUM_NUM + u8SPI_Devicenum - 1) * BYTE2;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
		"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	/*SEND DUTY INIT*/
	spi_write.tx_buf = LED_Device_MBI6353_initial_Duty;
	spi_write.len = (MBI6353_HEADER_NUM + MBI6353_DUTY_NUM +
		MBI6353_CHECKSUM_NUM + u8SPI_Devicenum - 1) * BYTE2;
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
		"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}


	return ret;

}

void _ldm_mbi6353_init(
	struct mtk_tv_kms_context *pctx,
	u8 u8Mspi_Max_Num)
{
	u8 i = 0;
	int32_t ret = 0;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return;
	}

	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);

	DRM_INFO("[LDM]LD Device MBI6353 init start\n");
	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			ret = ldm_mbi6353_init(pctx, i, pld_led_device_info->u8Device_Num[i]);
			if (!ret)
				DRM_INFO("[LDM]MBI6353nit spi ch = %d done\n", i);
		}
	}
}

int32_t ldm_cus_led_device_init(
	struct mtk_tv_kms_context *pctx,
	unsigned char channel,
	u8 u8SPI_Devicenum)
{
	int32_t ret = 0;
	char tx_cmd[] = {0};
	char rx_data[] = {0};
	struct spi_transfer spi_read[SPI_READ_BUF_NUM];
	struct spi_transfer spi_write;

	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_as3824_info *pld_led_device_as3824_info = NULL;
	struct drm_st_ld_led_device_nt50585_info *pld_led_device_nt50585_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;
	struct drm_st_ld_led_device_mbi6353_info *pld_led_device_mbi6353_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &pctx->ld_priv;
	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_as3824_info = &(pctx->ld_priv.ld_led_device_as3824_info);
	pld_led_device_nt50585_info = &(pctx->ld_priv.ld_led_device_nt50585_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);
	pld_led_device_mbi6353_info = &(pctx->ld_priv.ld_led_device_mbi6353_info);


	/*WRITE SPI*/
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz	  = pld_mspi_info->u32MspiClk;
	spi_write.tx_buf = tx_cmd;
	spi_write.rx_buf = NULL;
	spi_write.len = ARRAY_SIZE(tx_cmd);
	ret = spi_xfer(channel, &spi_write);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

/*READ SPI*/
	memset((void *)&spi_read, 0, sizeof(struct spi_transfer));
	spi_read[0].rx_buf = NULL;
	spi_read[0].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[0].tx_dma = 0x00;
	spi_read[0].speed_hz = pld_mspi_info->u32MspiClk;
	spi_read[0].tx_buf = tx_cmd;
	spi_read[0].len = ARRAY_SIZE(tx_cmd);
	spi_read[1].tx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].rx_nbits = SPI_NBITS_SINGLE;
	spi_read[1].tx_dma = 0x00;
	spi_read[1].speed_hz = pld_mspi_info->u32MspiClk;
	spi_read[1].tx_buf = NULL;
	spi_read[1].rx_buf = rx_data;
	spi_read[1].len = ARRAY_SIZE(rx_data);
	ret = spi_xfer(channel, (void *)&spi_read);
	if (ret) {
		DRM_ERROR(
"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
	}

	return ret;

}

void  _ldm_cus_led_device_init(
	struct mtk_tv_kms_context *pctx,
	u8 u8Mspi_Max_Num)
{
	u8 i = 0;
	int32_t ret = 0;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return;
	}

	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);

	DRM_INFO("[LDM]LD Device CUS init start\n");
	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			ret = ldm_cus_led_device_init(pctx,
				i,
				pld_led_device_info->u8Device_Num[i]);
			if (!ret)
				DRM_INFO("[LDM]CUS_LED_Device_Init spi ch = %d done\n", i);
		}
	}
}

int32_t ldm_set_ld_pwm_vsync_setting(struct mtk_tv_kms_context *pctx)
{
	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	u8 u8ld_version = 0;
	u16 u16PWM0_Period0 = 0;
	u16 u16PWM0_Period1 = 0;
	u16 u16PWM0_Shift0 = 0;
	u16 u16PWM0_Shift1 = 0;
	u16 u16PWM0_Duty0 = 0;
	u16 u16PWM0_Duty1 = 0;
	u16 u16PWM1_Period0 = 0;
	u16 u16PWM1_Period1 = 0;
	u16 u16PWM1_Shift0 = 0;
	u16 u16PWM1_Shift1 = 0;
	u16 u16PWM1_Duty0 = 0;
	u16 u16PWM1_Duty1 = 0;
	u16 u16Vsync_like_st_0 = 0;
	u16 u16Vsync_like_st_1 = 0;
	u16 u16Vsync_like_end_0 = 0;
	u16 u16Vsync_like_end_1 = 0;
	int32_t ret = 0;


	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &(pctx->ld_priv);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	u8ld_version = pctx_ld->u8LDM_Version;
	u16PWM0_Period0 = pld_dma_info->u16PWM0_Period[E_PWM_PERIOD_0];
	u16PWM0_Period1 = pld_dma_info->u16PWM0_Period[E_PWM_PERIOD_1];
	u16PWM0_Shift0 = pld_dma_info->u16PWM0_Shift[E_PWM_SHIFT_0];
	u16PWM0_Shift1 = pld_dma_info->u16PWM0_Shift[E_PWM_SHIFT_1];
	u16PWM0_Duty0 = pld_dma_info->u16PWM0_Duty[E_PWM_DUTY_0];
	u16PWM0_Duty1 = pld_dma_info->u16PWM0_Duty[E_PWM_DUTY_1];
	u16PWM1_Period0 = pld_dma_info->u16PWM1_Period[E_PWM_PERIOD_0];
	u16PWM1_Period1 = pld_dma_info->u16PWM1_Period[E_PWM_PERIOD_1];
	u16PWM1_Shift0 = pld_dma_info->u16PWM1_Shift[E_PWM_SHIFT_0];
	u16PWM1_Shift1 = pld_dma_info->u16PWM1_Shift[E_PWM_SHIFT_1];
	u16PWM1_Duty0 = pld_dma_info->u16PWM1_Duty[E_PWM_DUTY_0];
	u16PWM1_Duty1 = pld_dma_info->u16PWM1_Duty[E_PWM_DUTY_1];
	u16Vsync_like_st_0 = pld_dma_info->u16VsyncWidth[E_VSYNC_LIKE_ST_0];
	u16Vsync_like_st_1 = pld_dma_info->u16VsyncWidth[E_VSYNC_LIKE_ST_1];
	u16Vsync_like_end_0 = pld_dma_info->u16VsyncWidth[E_VSYNC_LIKE_END_0];
	u16Vsync_like_end_1 = pld_dma_info->u16VsyncWidth[E_VSYNC_LIKE_END_1];

	drv_hwreg_render_ldm_set_pwm_clken_bitmap(u8ld_version, PWM_EN_CLK_ALL);
	drv_hwreg_render_ldm_set_pwm0_vdben(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm0_reset_en(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm0_vdben_sw(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm0_period_0(u8ld_version, u16PWM0_Period0);
	drv_hwreg_render_ldm_set_pwm0_period_1(u8ld_version, u16PWM0_Period1);
	drv_hwreg_render_ldm_set_pwm0_shift_0(u8ld_version, u16PWM0_Shift0);
	drv_hwreg_render_ldm_set_pwm0_shift_1(u8ld_version, u16PWM0_Shift1);
	drv_hwreg_render_ldm_set_pwm0_duty_0(u8ld_version, u16PWM0_Duty0);
	drv_hwreg_render_ldm_set_pwm0_duty_1(u8ld_version, u16PWM0_Duty1);
	drv_hwreg_render_ldm_set_pwm1_vdben(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm1_reset_en(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm1_vdben_sw(u8ld_version, TRUE);
	drv_hwreg_render_ldm_set_pwm1_period_0(u8ld_version, u16PWM1_Period0);
	drv_hwreg_render_ldm_set_pwm1_period_1(u8ld_version, u16PWM1_Period1);
	drv_hwreg_render_ldm_set_pwm1_shift_0(u8ld_version, u16PWM1_Shift0);
	drv_hwreg_render_ldm_set_pwm1_shift_1(u8ld_version, u16PWM1_Shift1);
	drv_hwreg_render_ldm_set_pwm1_duty_0(u8ld_version, u16PWM1_Duty0);
	drv_hwreg_render_ldm_set_pwm1_duty_1(u8ld_version, u16PWM1_Duty1);
	drv_hwreg_render_ldm_set_ld_vsync_like_st_0(u8ld_version, u16Vsync_like_st_0);
	drv_hwreg_render_ldm_set_ld_vsync_like_st_1(u8ld_version, u16Vsync_like_st_1);
	drv_hwreg_render_ldm_set_ld_vsync_like_end_0(u8ld_version, u16Vsync_like_end_0);
	drv_hwreg_render_ldm_set_ld_vsync_like_end_1(u8ld_version, u16Vsync_like_end_1);
	drv_hwreg_render_ldm_set_vsync_like_src_sel(u8ld_version, E_VSYNC_LIKE_PWM0);

	return ret;
}

int32_t ldm_vsync_bug_patch(struct mtk_tv_kms_context *pctx)
{
	int32_t ret = 0;
	int32_t i = 0;
	struct spi_transfer spi_write;
	char tx_cmd[TXSIZE] = {1, 1, 1, 1, 1, 1, 1, 1};
	u8 u8Mspi_Max_Num = MSPI_MAX_NUM;

	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &(pctx->ld_priv);
	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);

	if (pctx_ld->u8LDM_Version > E_LD_VER3)
		u8Mspi_Max_Num = MSPI_MAX_NUM_2;

/*initial Mask*/

	/*WRITE SPI*/
	//spi owenr says reset parameter everytime
	memset((void *)&spi_write, 0, sizeof(struct spi_transfer));
	spi_write.tx_nbits = SPI_NBITS_SINGLE;
	spi_write.rx_nbits = SPI_NBITS_SINGLE;
	spi_write.tx_dma = 0x00;
	spi_write.speed_hz	  = pld_mspi_info->u32MspiClk;
	spi_write.rx_buf = NULL;


	for (i = 0; i < u8Mspi_Max_Num; i++) {
		if (pld_led_device_info->u8Device_Num[i] > 0) {
			/*SEND First*/
			spi_write.tx_buf = tx_cmd;
			spi_write.len = ARRAY_SIZE(tx_cmd);
			ret = spi_xfer(i, &spi_write);
			if (ret) {
				DRM_ERROR(
			"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
			}

			/*SEND Second*/
			spi_write.tx_buf = tx_cmd;
			spi_write.len = ARRAY_SIZE(tx_cmd);
			ret = spi_xfer(i, &spi_write);
			if (ret) {
				DRM_ERROR(
			"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
			}

			/*SEND Third*/
			spi_write.tx_buf = tx_cmd;
			spi_write.len = ARRAY_SIZE(tx_cmd);
			ret = spi_xfer(i, &spi_write);
			if (ret) {
				DRM_ERROR(
			"[%s,%5d] SPI transfer error = %d!!!\n", __func__, __LINE__,  ret);
			}
		}
	}
	return ret;

}

int32_t ldm_leddevice_init(struct mtk_tv_kms_context *pctx)
{
	int32_t ret = 0;
	u8 u8Mspi_Max_Num = MSPI_MAX_NUM;

	struct mtk_ld_context *pctx_ld = NULL;
	struct drm_st_ld_panel_info *pld_panel_info = NULL;
	struct drm_st_ld_misc_info *pld_misc_info = NULL;
	struct drm_st_ld_mspi_info *pld_mspi_info = NULL;
	struct drm_st_ld_dma_info  *pld_dma_info = NULL;
	struct drm_st_ld_boosting_info *pld_boosting_info = NULL;
	struct drm_st_ld_led_device_info *pld_led_device_info = NULL;
	struct drm_st_ld_led_device_as3824_info *pld_led_device_as3824_info = NULL;
	struct drm_st_ld_led_device_nt50585_info *pld_led_device_nt50585_info = NULL;
	struct drm_st_ld_led_device_iw7039_info *pld_led_device_iw7039_info = NULL;

	if (pctx == NULL) {
		DRM_ERROR("[LDM][%s,%5d] pctx is null\n", __func__, __LINE__);
		return E_LDM_RET_FAIL;
	}

	pctx_ld = &(pctx->ld_priv);
	pld_panel_info = &(pctx->ld_priv.ld_panel_info);
	pld_misc_info = &(pctx->ld_priv.ld_misc_info);
	pld_mspi_info = &(pctx->ld_priv.ld_mspi_info);
	pld_dma_info = &(pctx->ld_priv.ld_dma_info);
	pld_boosting_info = &(pctx->ld_priv.ld_boosting_info);
	pld_led_device_info = &(pctx->ld_priv.ld_led_device_info);
	pld_led_device_as3824_info = &(pctx->ld_priv.ld_led_device_as3824_info);
	pld_led_device_nt50585_info = &(pctx->ld_priv.ld_led_device_nt50585_info);
	pld_led_device_iw7039_info = &(pctx->ld_priv.ld_led_device_iw7039_info);

	if (pctx_ld->u8LDM_Version > E_LD_VER3)
		u8Mspi_Max_Num = MSPI_MAX_NUM_2;

	switch (pld_led_device_info->eLEDDeviceType) {
	case E_LD_DEVICE_UNSUPPORT:
		DRM_INFO("[LDM]LD Device is not support\n");
		break;
	case E_LD_DEVICE_AS3824:
		ret = ldm_vsync_bug_patch(pctx);
		if (!ret)
			DRM_INFO("ldm_vsync_bug_patch spi fail:%d\n", ret);
		_ldm_as3824_init(pctx, u8Mspi_Max_Num);
		break;
	case E_LD_DEVICE_NT50585:
		DRM_INFO("[LDM]LD Device NT50585 init start\n");
		break;
	case E_LD_DEVICE_IW7039:
		ret = ldm_vsync_bug_patch(pctx);
		if (!ret)
			DRM_INFO("ldm_vsync_bug_patch spi fail:%d\n", ret);
		_ldm_iw7039_init(pctx, u8Mspi_Max_Num);
		break;
	case E_LD_DEVICE_MCU:
		DRM_INFO("[LDM]LD Device MCU init start\n");
		break;
	case E_LD_DEVICE_CUS:
		_ldm_cus_led_device_init(pctx, u8Mspi_Max_Num);
		break;
	case E_LD_DEVICE_MBI6353:
		ret = ldm_vsync_bug_patch(pctx);
		if (!ret)
			DRM_INFO("ldm_vsync_bug_patch spi fail:%d\n", ret);
		_ldm_mbi6353_init(pctx, u8Mspi_Max_Num);
		break;
	default:
		DRM_INFO("[LDM]LD Device is not support\n");
		break;
	}

	return ret;
}

