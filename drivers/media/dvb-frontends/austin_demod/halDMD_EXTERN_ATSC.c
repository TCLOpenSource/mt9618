// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

//-------------------------------------------------------------------------------------------------
//  Include Files
//-------------------------------------------------------------------------------------------------

//#include "drvDMD_ATSC.h"
#ifdef ADD_MERAK20_ATSC3
#include "../mtk-merak20-demod/demod_drv_atsc.h"
#else
#include "../mtk-merak-demod/demod_drv_atsc.h"
#endif

#if !defined MTK_PROJECT
#ifdef MSOS_TYPE_LINUX_KERNEL
#include <linux/string.h>
#include <linux/kernel.h>
#else
#include <stdio.h>
#include <math.h>
#endif
#endif

#if DMD_ATSC_3PARTY_EN
// Please add header files needed by 3 party platform
typedef u8 MS_BOOL;
typedef u8 MS_U8;
typedef u16 MS_U16;
typedef u32 MS_U32;
typedef s16 MS_S16;
typedef s32 MS_S32;
#endif

#ifdef MSOS_TYPE_LINUX_KERNEL
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
#endif
#endif

//-------------------------------------------------------------------------------------------------
//  Driver Compiler Options
//-------------------------------------------------------------------------------------------------

#ifndef BIN_RELEASE
#define BIN_RELEASE				 0
#endif

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------

#define USE_DEFAULT_CHECK_TIME	  1

#ifdef CONFIG_UTOPIA_PROC_DBG_SUPPORT
#define HAL_EXTERN_ATSC_DBINFO(y)   y
#else
#define HAL_EXTERN_ATSC_DBINFO(y)   //y
#endif

#define FW_PRI_XDATA_BASE	0x4008

#define REG_IIC_BYPASS	   0x0910
#define REG_ALL_PAD_IN	   0x0951
#define REG_TOP_BASE		 0x0900
#define REG_TOP_MCU_BASE	 0x3400
#define REG_DMD0_MCU2_BASE   0x3800
#define REG_DMD1_MCU2_BASE   0x3900
#define REG_DMD2_MCU2_BASE   0x3A00
#define REG_DMD3_MCU2_BASE   0x3B00
#define REG_DMD_SEL		  0x3C26
#define REG_GPIO_EN		  0x0951
#define REG_GPIO_LEVEL	   0x09CA
#define REG_GPIO_SET		 0x09C6
#define REG_GPIO_OUT		 0x09C8
#define REG_SSPI_EN		  0x0976
#define REG_PAD_MODE_SEL	 0x0A33
#define REG_ADC_OUT_SEL	  0x0A2E


#define REG_MCU_RST		  0x0B32

#define EXTERN_ATSC_VSB_TRAIN_SNR_LIMIT   0x05
#define EXTERN_ATSC_FEC_ENABLE			0x1F

#define VSB_ATSC		   0x04
#define QAM256_ATSC		0x02

#define QAM16_J83ABC	   0x00
#define QAM32_J83ABC	   0x01
#define QAM64_J83ABC	   0x02
#define QAM128_J83ABC	  0x03
#define QAM256_J83ABC	  0x04

#define LOAD_CODE_I2C_BLOCK_NUM  0x80

#define I2C_SLAVE_ID	(((pres->sdmd_atsc_initdata.u8I2CSlaveBus & 0x3F) << 8) | pres->sdmd_atsc_initdata.u8I2CSlaveAddr)

#define SPI_DEVICE_BUFFER_SIZE   256

//SSPI COMMAND
#define RIU_W_CMD   0x1A
#define RIU_W1_CMD  0x1D
#define RIU_R_CMD   0x18
#define RIU_R1_CMD  0x1C
#define RIU_RT_CMD  0x11
#define RIU_R1T_CMD 0x15
#define MIU_W_CMD   0x25
#define MIU_R_CMD   0x20
#define MIU_ST_CMD  0x21
#define CFG_W_CMD   0x05
#define CFG_R_CMD   0x00

 // Please set the following values according to 3 party platform
 #define IS_MULTI_DMD		 0
 #define IS_MULTI_MCU		 0
 #define DIGRF_INSIDE		 0
 #define J83ABC_ENABLE		0
 #define SOC_LIKE_EXT_DMD	 1
 #define USE_PSRAM_32KB	   1
 #define FS_RATE			  24000
 #define EXT_IOCTL_CMD		HAL_EXTERN_ATSC_IOCTL_CMD_0
 #define SUPPORT_ATSC3		1
 #define FW_RUN_ON_DRAM_MODE  0
 #define SSPI_USE_MPLL_CLK	1

#define MIX_RATE_BIT					  27

#if SOC_LIKE_EXT_DMD
 #define REG_TR_BASE					0x2100
 #define REG_FRONTEND_BASE			  0x2800
 #define REG_EQ_BASE					0x3300
 #define REG_RS_BASE					0x1F00
#elif J83ABC_ENABLE
 #define REG_TR_BASE					0x2900
 #define REG_FRONTEND_BASE			  0x2700
 #define REG_EQ_BASE					0x2A00
 #define REG_RS_BASE					0x2100
#else
 #define REG_TR_BASE					0x1400
 #define REG_FRONTEND_BASE			  0x3E00
 #define REG_EQ_BASE					0x1900
 #define REG_RS_BASE					0x0D00
#endif

//-------------------------------------------------------------------------------------------------
//  Local Variables
//-------------------------------------------------------------------------------------------------

static MS_U8  id;

static struct DMD_ATSC_RESDATA *pres;

static MS_U8			*pExtChipChNum;
static MS_U8			*pExtChipChId;
static MS_U8			*pExtChipI2cCh;

static struct DMD_ATSC_SYS_PTR sys;

#ifdef CONFIG_UTOPIA_PROC_DBG_SUPPORT
static DMD_ATSC_DEBUG_LEVEL * pATSC_DBGLV;
#endif

// Please set fw dat file used by 3 party platform
static const MS_U8  EXTERN_ATSC_table[] = {
	#include "UTOPIA_ENABLE.dat"
};

static const MS_U8  EXTERN_ATSC_U03_table[] = {
	#include "UTOPIA_ENABLE.dat"
};

#if !IS_MULTI_MCU || !DIGRF_INSIDE
static MS_U16 u16Lib_size = sizeof(EXTERN_ATSC_table);
#endif

#if DIGRF_INSIDE
#if IS_MULTI_MCU && DIGRF_INSIDE
const MS_U8 DIGRF_table[] = {
	#include "DIGRF_FW.dat"
};

static MS_U16 u16Lib_size = sizeof(DIGRF_table);

static MS_BOOL _isDigRfEn = FALSE;
#endif

static const MS_U16 _drf_atsc_aci_coef_6M[46] = {
	0x000A, 0x003C, 0x0097, 0x00CF, 0x0081, 0x7FDA, 0x7F93, 0x7FF8,
	0x0059, 0x0015, 0x7FAF, 0x7FE6, 0x0052, 0x001D, 0x7FA8, 0x7FE0,
	0x0062, 0x0024, 0x7F90, 0x7FD8, 0x0081, 0x002C, 0x7F6A, 0x7FD0,
	0x00AF, 0x0035, 0x7F33, 0x7FC7, 0x00F3, 0x003E, 0x7EDF, 0x7FBE,
	0x015D, 0x0046, 0x7E53, 0x7FB7, 0x021E, 0x004C, 0x7D35, 0x7FB2,
	0x03FE, 0x0050, 0x7943, 0x7FAF, 0x145B, 0x2051
};
#endif

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------

#ifndef MSOS_TYPE_LINUX_KERNEL
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
typedef MS_BOOL (*EXT_IOCTL)(DMD_ATSC_IOCTL_DATA*, DMD_ATSC_HAL_COMMAND, void*);

extern MS_U8 EXT_ATSC_CHIP[];
extern EXT_IOCTL EXT_ATSC_IOCTL[];
#endif
#endif // #ifndef MSOS_TYPE_LINUX_KERNEL

//-------------------------------------------------------------------------------------------------
//  Local Functions
//-------------------------------------------------------------------------------------------------
#if IS_MULTI_MCU
static MS_BOOL _I2C_Channel_Change(MS_U8 ch_num);
#endif

static MS_BOOL _I2C_READ_BYTE(MS_U32 u32Addr, MS_U8 *pu8Data)
{
	#if IS_MULTI_MCU
	MS_U8 i2cCh = *(pExtChipI2cCh);
	#endif

	MS_BOOL bRet = TRUE;
	MS_U8 u8MsbData[6] = {0};

	u8MsbData[0] = 0x10;
	u8MsbData[1] = 0x00;
	u8MsbData[2] = (u32Addr>>16)&0xff;
	u8MsbData[3] = (u32Addr>>8)&0xff;
	u8MsbData[4] = u32Addr&0xff;

	#if IS_MULTI_MCU
	#if DIGRF_INSIDE
	if (!_isDigRfEn && i2cCh != 1)
	#else
	if (i2cCh != 1)
	#endif
	{
		if (i2cCh != 3 && (u8MsbData[2] != 0x00 ||
			u8MsbData[3] < 0x10 || u8MsbData[3] > 0x2F))
			_I2C_Channel_Change(3); // For HK
		else if (i2cCh != 5 && (u8MsbData[2] == 0x00 &&
			u8MsbData[3] > 0x10 && u8MsbData[3] < 0x2F))
			_I2C_Channel_Change(5); // For DMD
	}
	#if DIGRF_INSIDE
	else if (i2cCh && i2cCh != 3 && _isDigRfEn)
		_I2C_Channel_Change(3); // For Digital RF
	#endif
	#endif

	u8MsbData[0] = 0x35;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, u8MsbData);

	u8MsbData[0] = 0x10;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 5, u8MsbData);
	bRet &= pres->sdmd_atsc_initdata.I2C_ReadBytes(I2C_SLAVE_ID, 0, 0, 1, pu8Data);

	u8MsbData[0] = 0x34;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, u8MsbData);

	return bRet;
}

static MS_BOOL _I2C_WRITE_BYTE(MS_U32 u32Addr, MS_U8 u8Data)
{
	#if IS_MULTI_MCU
	MS_U8 i2cCh = *(pExtChipI2cCh);
	#endif

	MS_BOOL bRet = TRUE;
	MS_U8 u8MsbData[6] = {0};

	u8MsbData[0] = 0x10;
	u8MsbData[1] = 0x00;
	u8MsbData[2] = (u32Addr>>16)&0xff;
	u8MsbData[3] = (u32Addr>>8)&0xff;
	u8MsbData[4] = u32Addr&0xff;
	u8MsbData[5] = u8Data;

	#if IS_MULTI_MCU
	#if DIGRF_INSIDE
	if (!_isDigRfEn && i2cCh != 1)
	#else
	if (i2cCh != 1)
	#endif
	{
		if (i2cCh != 3 && (u8MsbData[2] != 0x00 ||
			u8MsbData[3] < 0x10 || u8MsbData[3] > 0x2F))
			_I2C_Channel_Change(3); // For HK
		else if (i2cCh != 5 && (u8MsbData[2] == 0x00 &&
			u8MsbData[3] > 0x10 && u8MsbData[3] < 0x2F))
			_I2C_Channel_Change(5); // For DMD
	}
	#if DIGRF_INSIDE
	else if (i2cCh && i2cCh != 3 && _isDigRfEn)
		_I2C_Channel_Change(3); // For Digital RF
	#endif
	#endif

	u8MsbData[0] = 0x35;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, u8MsbData);
	u8MsbData[0] = 0x10;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 6, u8MsbData);
	u8MsbData[0] = 0x34;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, u8MsbData);

	return bRet;
}

static MS_BOOL _I2C_WRITE_BYTES(MS_U32 u32Addr, MS_U8 *u8Data, MS_U8 u8Len)
{
	#if IS_MULTI_MCU
	MS_U8 i2cCh = *(pExtChipI2cCh);
	#endif

	MS_BOOL bRet = TRUE;
	MS_U16 index;
	MS_U8 Data[0x80+5];

	Data[0] = 0x10;
	Data[1] = 0x00;
	Data[2] = (u32Addr>>16)&0xff;
	Data[3] = (u32Addr>>8)&0xff;
	Data[4] = u32Addr&0xff;

	for (index = 0; index < u8Len ; index++)
		Data[5+index] = u8Data[index];

	#if IS_MULTI_MCU
	#if DIGRF_INSIDE
	if (!_isDigRfEn && i2cCh != 1)
	#else
	if (i2cCh != 1)
	#endif
	{
		if (i2cCh != 3 && (Data[2] != 0x00 || Data[3] < 0x10 || Data[3] > 0x2F))
			_I2C_Channel_Change(3); // For HK
		else if (i2cCh != 5 && (Data[2] == 0x00 && Data[3] > 0x10 && Data[3] < 0x2F))
			_I2C_Channel_Change(5); // For DMD
	}
	#if DIGRF_INSIDE
	else if (i2cCh && i2cCh != 3 && _isDigRfEn)
		_I2C_Channel_Change(3); // For Digital RF
	#endif
	#endif

	Data[0] = 0x35;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);
	Data[0] = 0x10;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, (5 + u8Len), Data);
	Data[0] = 0x34;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);

	return bRet;
}

static MS_BOOL _I2C_Channel_Set(MS_U8 ch_num)
{
	MS_BOOL bRet = TRUE;
	MS_U8 Data[5] = {0x53, 0x45, 0x52, 0x44, 0x42};

	//Exit
	Data[0] = 0x34;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);
	Data[0] = (ch_num & 0x01) ? 0x36 : 0x45;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);

	//Init
	Data[0] = 0x53;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 5, Data);
	Data[0] = (ch_num & 0x04) ? 0x80 : 0x81;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);

	if ((ch_num == 4) || (ch_num == 5) || (ch_num == 1))
		Data[0] = 0x82;
	else
		Data[0] = 0x83;

	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);

	if ((ch_num == 4) || (ch_num == 5))
		Data[0] = 0x85;
	else
		Data[0] = 0x84;

	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);
	Data[0] = (ch_num & 0x01) ? 0x51 : 0x53;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);
	Data[0] = (ch_num & 0x01) ? 0x37 : 0x7F;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);
	Data[0] = 0x35;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);
	Data[0] = 0x71;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, NULL, 1, Data);

	#if IS_MULTI_MCU
	*(pExtChipI2cCh) = ch_num;
	#endif

	return bRet;
}

static MS_BOOL _I2C_Channel_Change(MS_U8 ch_num)
{
	MS_BOOL bRet = TRUE;
	MS_U8 Data[5] = {0x53, 0x45, 0x52, 0x44, 0x42};

	Data[0] = (ch_num & 0x01) ? 0x81 : 0x80;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);
	Data[0] = (ch_num & 0x02) ? 0x83 : 0x82;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);
	Data[0] = (ch_num & 0x04) ? 0x85 : 0x84;
	bRet &= pres->sdmd_atsc_initdata.I2C_WriteBytes(I2C_SLAVE_ID, 0, 0, 1, Data);

	#if IS_MULTI_MCU
	*(pExtChipI2cCh) = ch_num;
	#endif

	return bRet;
}

static MS_BOOL _WRITE_MBX(MS_U16 u16Addr, MS_U8 u8Data)
{
	MS_BOOL bResult = TRUE;
	MS_U8 u8ReadData, u8Cnt;

	#if SOC_LIKE_EXT_DMD
	bResult &= _I2C_WRITE_BYTE(0x098A, (MS_U8)u16Addr);		//Reg
	bResult &= _I2C_WRITE_BYTE(0x098B, (MS_U8)(u16Addr >> 8)); //Reg
	bResult &= _I2C_WRITE_BYTE(0x0989, 0x01);	//Write data length
	bResult &= _I2C_WRITE_BYTE(0x098C, u8Data);	//Write data
	bResult &= _I2C_WRITE_BYTE(0x0988, 0x01);	//Trigger write operation
	#else
	bResult &= _I2C_WRITE_BYTE(0x0C04, (MS_U8)u16Addr);		//Reg
	bResult &= _I2C_WRITE_BYTE(0x0C05, (MS_U8)(u16Addr >> 8));//Reg
	bResult &= _I2C_WRITE_BYTE(0x0C03, 0x01);	//Write data length
	bResult &= _I2C_WRITE_BYTE(0x0C06, u8Data);	//Write data
	bResult &= _I2C_WRITE_BYTE(0x0C02, 0x01);	//Trigger write operation
	#endif

	u8ReadData = 0x00;
	u8Cnt = 0;
	#if SOC_LIKE_EXT_DMD
	while ((_I2C_READ_BYTE(0x0988, &u8ReadData) == TRUE) && ((u8ReadData&0x01) == 0x01))
	#else
	while ((_I2C_READ_BYTE(0x0C02, &u8ReadData) == TRUE) && ((u8ReadData&0x01) == 0x01))
	#endif
	{
		if (u8Cnt > 100) {
			PRINT("ERROR: ATSC EXTERN DEMOD MBX WRITE TIME OUT!\n");
			return FALSE;
		}
		u8Cnt++;

		sys.delayms(1);
	}

	return bResult;
}

static MS_BOOL _READ_MBX(MS_U16 u16Addr, MS_U8 *pu8Data)
{
	MS_BOOL bResult = TRUE;
	MS_U8 u8ReadData, u8Cnt;

	#if SOC_LIKE_EXT_DMD
	bResult &= _I2C_WRITE_BYTE(0x098A, (MS_U8)u16Addr);		//Reg
	bResult &= _I2C_WRITE_BYTE(0x098B, (MS_U8)(u16Addr >> 8)); //Reg
	bResult &= _I2C_WRITE_BYTE(0x0989, 0x01);	//Read data length
	bResult &= _I2C_WRITE_BYTE(0x0988, 0x02);	//Trigger read operation
	#else
	bResult &= _I2C_WRITE_BYTE(0x0C04, (MS_U8)u16Addr);		//Reg
	bResult &= _I2C_WRITE_BYTE(0x0C05, (MS_U8)(u16Addr >> 8)); //Reg
	bResult &= _I2C_WRITE_BYTE(0x0C03, 0x01);	//Read data length
	bResult &= _I2C_WRITE_BYTE(0x0C02, 0x02);	//Trigger read operation
	#endif

	u8ReadData = 0x00;
	u8Cnt = 0;
	#if SOC_LIKE_EXT_DMD
	while ((_I2C_READ_BYTE(0x0988, &u8ReadData) == TRUE) && ((u8ReadData&0x02) == 0x02))
	#else
	while ((_I2C_READ_BYTE(0x0C02, &u8ReadData) == TRUE) && ((u8ReadData&0x02) == 0x02))
	#endif
	{
		if (u8Cnt > 100) {
			PRINT("ERROR: ATSC EXTERN DEMOD MBX READ TIME OUT!\n");
			return FALSE;
		}
		u8Cnt++;

		sys.delayms(1);
	}

	#if SOC_LIKE_EXT_DMD
	bResult &= _I2C_READ_BYTE(0x098C, pu8Data); //Read data
	#else
	bResult &= _I2C_READ_BYTE(0x0C06, pu8Data); //Read data
	#endif

	return bResult;
}

#if defined MTK_PROJECT
static MS_U32 _MSPI_Init_Ext(MS_U8 u8HWNum)
{
	MS_S32 ret = 0;
	PDWNC_GSPI_MODE_T paramGspiMode;

	paramGspiMode.u4Freq = 12000000; // Unit : Hz
	paramGspiMode.u4Mode = 0;

	paramGspiMode.u4Mode &= (~(GSPI_CPHA|GSPI_CPOL|GSPI_LSB_FIRST));

	paramGspiMode.u4CsLead = 0;
	paramGspiMode.u4CsLag  = 0;
	paramGspiMode.u4CsHold = 0;

	ret = PDWNC_GspiInit(&paramGspiMode);

	return ret;
}

static void _MSPI_SlaveEnable(MS_BOOL enable)
{
	MS_U32 u4DemodSPICSPin = 0xFFFF;

	DRVCUST_OptQuery(eDemodSPICSZPIN, &u4DemodSPICSPin);

	if (enable)
		GPIO_SetOut(u4DemodSPICSPin, 0);
	else
		GPIO_SetOut(u4DemodSPICSPin, 1);
}

static MS_U32 _MSPI_Write(MS_U8 *pData, MS_U16 u16Size)
{
	MS_U32 spi_length;
	MS_U32 dataLen = 0;
	MS_U32 AccuLen = 0;

	do {
		dataLen	= (u16Size > 32 ? 32:u16Size); //according MTK buffer size:32 bytes
		spi_length = dataLen*8-1;
		PDWNC_GspiReadWrite(GSPI_WRITE, spi_length, NULL, (pData+AccuLen));

		u16Size -= dataLen;
		AccuLen += dataLen;

	} while (u16Size);

	return TRUE;
}

static MS_U32 _MSPI_Read(MS_U8 *pData, MS_U16 u16Size)
{
	MS_U32 size_to_spi_drv = 0;

	size_to_spi_drv = ((u16Size)*8)-1;
	PDWNC_GspiReadWrite(GSPI_READ, size_to_spi_drv, pData, NULL);

	return TRUE;
}
#endif

static void _HAL_EXTERN_ATSC_InitClk(void)
{
	MS_U8 dmd_id  = (id&0x0F);
	MS_U8 chip_id = (id>>4);
	MS_U8 data;
	#if !defined(MSOS_TYPE_LINUX_KERNEL) && (DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN)
	MS_U8 i;
	DMD_ATSC_IOCTL_DATA ioData;
	#endif

	#if USE_DEFAULT_CHECK_TIME
	pres->sdmd_atsc_initdata.vsbagclockchecktime	= 100;
	pres->sdmd_atsc_initdata.vsbprelockchecktime	= 300;
	pres->sdmd_atsc_initdata.vsbfsynclockchecktime  = 600;
	pres->sdmd_atsc_initdata.vsbfeclockchecktime	= 4000;

	#if SUPPORT_ATSC3
	pres->sdmd_atsc_initdata.u16OFDMAGCLockCheckTime   = 100;
	pres->sdmd_atsc_initdata.u16OFDMPreLockCheckTime   = 300;
	pres->sdmd_atsc_initdata.u16OFDMFSyncLockCheckTime = 600;
	pres->sdmd_atsc_initdata.u16OFDMFECLockCheckTime   = 4000;
	#endif

	pres->sdmd_atsc_initdata.qamagclockchecktime	= 60;
	pres->sdmd_atsc_initdata.qamprelockchecktime	= 1000;
	pres->sdmd_atsc_initdata.qammainlockchecktime   = 2000;
	#endif

	if (pres->sdmd_atsc_pridata.bdownloaded)
		return;

	_I2C_Channel_Set(0);

	_I2C_Channel_Change(3);

	_I2C_WRITE_BYTE(REG_ALL_PAD_IN, 0x00);

	_I2C_READ_BYTE(REG_TOP_BASE, &data);

	if (data == 0xC7)
		*(pExtChipChNum) = 4;
	else if (data == 0xB5) {
		_I2C_READ_BYTE(REG_TOP_BASE+0x04, &data);

		if (data & 0x30)
			*(pExtChipChNum) = 2;
		else
			*(pExtChipChNum) = 1;
	} else
		*(pExtChipChNum) = 1;

	#if !defined(MSOS_TYPE_LINUX_KERNEL) && (DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN)
	for (i = 0; i < 4; i++) {
		if (EXT_ATSC_CHIP[i] == data) {
			if (pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd != EXT_ATSC_IOCTL[i]) {
				pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd = EXT_ATSC_IOCTL[i];

				ioData.id   = id;
				ioData.pRes = pres;
				ioData.sys  = sys;

				pres->sdmd_atsc_pridata.hal_dmd_atsc_ioctl_cmd(
				&ioData, DMD_ATSC_HAL_CMD_InitClk, NULL);

				return;
			}

			break;
		}
	}
	#endif

	while (chip_id)
		dmd_id -= *(pExtChipChNum - (chip_id--));

	*(pExtChipChId) = dmd_id;

	#if !FW_RUN_ON_DRAM_MODE
	#if IS_MULTI_DMD && !DIGRF_INSIDE
	// inivec.task
	_I2C_WRITE_BYTE(0x100A00+(0x53)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x53)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x4f)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x4f)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x4e)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x4e)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x33)*2, 0X01);
	_I2C_WRITE_BYTE(0x100A00+(0x33)*2+1, 0X12);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2, 0X03);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2+1, 0X18);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2, 0X03);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2, 0X03);
	_I2C_WRITE_BYTE(0x100A00+(0x35)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x30)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x30)*2+1, 0X22);
	_I2C_WRITE_BYTE(0x100A00+(0x01)*2, 0X40);
	_I2C_WRITE_BYTE(0x100A00+(0x01)*2+1, 0X04);
	_I2C_WRITE_BYTE(0x100A00+(0x45)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x45)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x0c)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x0c)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x0f)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x0f)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x20)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x20)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x0b)*2, 0X05);
	_I2C_WRITE_BYTE(0x100A00+(0x0b)*2+1, 0X05);
	_I2C_WRITE_BYTE(0x100A00+(0x2e)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x2e)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x2a)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x2a)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x2b)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x2b)*2+1, 0X0c);
	_I2C_WRITE_BYTE(0x100900+(0x0b)*2, 0X30);
	_I2C_WRITE_BYTE(0x100900+(0x0b)*2+1, 0X04);
	_I2C_WRITE_BYTE(0x100A00+(0x17)*2, 0X04);
	_I2C_WRITE_BYTE(0x100A00+(0x17)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x51)*2, 0X81);
	_I2C_WRITE_BYTE(0x100A00+(0x51)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x09)*2, 0X01);
	_I2C_WRITE_BYTE(0x100900+(0x09)*2+1, 0X01);
	_I2C_WRITE_BYTE(0x100900+(0x0a)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x0a)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x0b)*2, 0X30);
	_I2C_WRITE_BYTE(0x100900+(0x0b)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x12)*2, 0X18);
	_I2C_WRITE_BYTE(0x100900+(0x12)*2+1, 0X14);
	_I2C_WRITE_BYTE(0x100900+(0x13)*2, 0X20);
	_I2C_WRITE_BYTE(0x100900+(0x13)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x1b)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x1b)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x1c)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x1c)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x08)*2, 0X01);
	_I2C_WRITE_BYTE(0x100900+(0x08)*2+1, 0X0a);
	_I2C_WRITE_BYTE(0x100900+(0x28)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x28)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x51)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x51)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x52)*2, 0X40);
	_I2C_WRITE_BYTE(0x100900+(0x52)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x50)*2, 0X10);
	_I2C_WRITE_BYTE(0x100900+(0x50)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x57)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x57)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x55)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x55)*2+1, 0X01);
	_I2C_WRITE_BYTE(0x100900+(0x55)*2, 0X10);
	_I2C_WRITE_BYTE(0x100900+(0x55)*2+1, 0X01);
	_I2C_WRITE_BYTE(0x100900+(0x59)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x59)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x20)*2, 0X04);
	_I2C_WRITE_BYTE(0x100900+(0x20)*2+1, 0X00);

	// ini_demod_hkmcu
	_I2C_WRITE_BYTE(0x100900+(0x09)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x09)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x0a)*2, 0X10);
	_I2C_WRITE_BYTE(0x100900+(0x0a)*2+1, 0X11);
	_I2C_WRITE_BYTE(0x100900+(0x0d)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x0d)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x33)*2, 0X01);
	_I2C_WRITE_BYTE(0x100A00+(0x33)*2+1, 0X12);
	_I2C_WRITE_BYTE(0x100A00+(0x30)*2, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x30)*2+1, 0X22);
	_I2C_WRITE_BYTE(0x100900+(0x23)*2, 0X00);
	_I2C_WRITE_BYTE(0x100900+(0x23)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x51)*2, 0X81);
	_I2C_WRITE_BYTE(0x100A00+(0x51)*2+1, 0X00);
	_I2C_WRITE_BYTE(0x100A00+(0x18)*2, 0X01);
	_I2C_WRITE_BYTE(0x100A00+(0x18)*2+1, 0X01);
	#endif
	#else // #if !FW_RUN_ON_DRAM_MODE



	#endif // #if !FW_RUN_ON_DRAM_MODE
}

static void _MIU_INIT(void)
{
	// austin_inivec.v.cpp
	_I2C_WRITE_BYTE(0x000a9f, 0x00);
	_I2C_WRITE_BYTE(0x000a9e, 0x00);
	_I2C_WRITE_BYTE(0x000a9d, 0x00);
	_I2C_WRITE_BYTE(0x000a9c, 0x00);
	_I2C_WRITE_BYTE(0x000a67, 0x12);
	_I2C_WRITE_BYTE(0x000a66, 0x01);

	_I2C_WRITE_BYTE(0x000a6b, 0x18);
	_I2C_WRITE_BYTE(0x000a6a, 0x03);
	sys.delayms(1);
	_I2C_WRITE_BYTE(0x000a6b, 0x00);
	_I2C_WRITE_BYTE(0x000a6a, 0x03);
	#if !FW_RUN_ON_DRAM_MODE
	_I2C_WRITE_BYTE(0x000a60, 0x6b);

	_I2C_WRITE_BYTE(0x000937, 0x00);
	_I2C_WRITE_BYTE(0x000933, 0x00);
	#else
	_I2C_WRITE_BYTE(0x000a61, 0x24);
	_I2C_WRITE_BYTE(0x000a60, 0x00);

	_I2C_WRITE_BYTE(0x000913, 0x01);
	_I2C_WRITE_BYTE(0x000912, 0x01);
	_I2C_WRITE_BYTE(0x000915, 0x00);
	_I2C_WRITE_BYTE(0x000914, 0x00);
	_I2C_WRITE_BYTE(0x000917, 0x00);
	_I2C_WRITE_BYTE(0x000916, 0x00);

	_I2C_WRITE_BYTE(0x000927, 0x00);
	_I2C_WRITE_BYTE(0x000926, 0x20);
	_I2C_WRITE_BYTE(0x000937, 0x00);
	_I2C_WRITE_BYTE(0x000936, 0x11);
	_I2C_WRITE_BYTE(0x000933, 0x00);
	_I2C_WRITE_BYTE(0x000932, 0x00);

	_I2C_WRITE_BYTE(0x000910, 0x01);
	_I2C_WRITE_BYTE(0x000951, 0x00);

	_I2C_WRITE_BYTE(0x000937, 0x00);

	_I2C_WRITE_BYTE(0x000933, 0x10);
	_I2C_WRITE_BYTE(0x000932, 0x00);
	#endif  //#if !FW_RUN_ON_DRAM_MODE
	//================================================
	// Start MIU init !!!
	//================================================
	//miu init start
	_I2C_WRITE_BYTE(0x0D00+0x10*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x10*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x01*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x01*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x23*2+1, 0xFF);
	_I2C_WRITE_BYTE(0x0D00+0x23*2+0, 0xFE);

	_I2C_WRITE_BYTE(0x0D00+0x33*2+1, 0xFF);
	_I2C_WRITE_BYTE(0x0D00+0x33*2+0, 0xFF);

	_I2C_WRITE_BYTE(0x0D00+0x43*2+1, 0xFF);
	_I2C_WRITE_BYTE(0x0D00+0x43*2+0, 0xFF);

	_I2C_WRITE_BYTE(0x0D00+0x53*2+1, 0xFF);
	_I2C_WRITE_BYTE(0x0D00+0x53*2+0, 0xFF);

	_I2C_WRITE_BYTE(0x0E00+0x1B*2+1, 0x40);
	_I2C_WRITE_BYTE(0x0E00+0x1B*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x1A*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x1A*2+0, 0x00);

	//MIU clock setting
	//set DDRPLL0 2000MHZ
	_I2C_WRITE_BYTE(0x0E00+0x18*2+1, 0x8F);
	_I2C_WRITE_BYTE(0x0E00+0x18*2+0, 0x5B);
	_I2C_WRITE_BYTE(0x0E00+0x19*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x19*2+0, 0x22);

	_I2C_WRITE_BYTE(0x0E00+0x11*2+1, 0x20);
	_I2C_WRITE_BYTE(0x0E00+0x11*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x20);
	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x30);
	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x20);
	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);

	#if FW_RUN_ON_DRAM_MODE
	//miu_clk = 452838600/200;
	_I2C_WRITE_BYTE(0x0D00+0x01*2+1, 0x01);
	_I2C_WRITE_BYTE(0x0D00+0x01*2+0, 0x11);

	_I2C_WRITE_BYTE(0x0D00+0x02*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x02*2+0, 0x4B);

	_I2C_WRITE_BYTE(0x0D00+0x03*2+1, 0x15);
	_I2C_WRITE_BYTE(0x0D00+0x03*2+0, 0x3C);

	_I2C_WRITE_BYTE(0x0D00+0x04*2+1, 0x0A);
	_I2C_WRITE_BYTE(0x0D00+0x04*2+0, 0x44);

	_I2C_WRITE_BYTE(0x0D00+0x05*2+1, 0x0E);
	_I2C_WRITE_BYTE(0x0D00+0x05*2+0, 0x43);

	_I2C_WRITE_BYTE(0x0D00+0x06*2+1, 0x62);
	_I2C_WRITE_BYTE(0x0D00+0x06*2+0, 0x97);

	_I2C_WRITE_BYTE(0x0D00+0x07*2+1, 0x10);
	_I2C_WRITE_BYTE(0x0D00+0x07*2+0, 0xFF);

	_I2C_WRITE_BYTE(0x0D00+0x08*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x08*2+0, 0x31);

	_I2C_WRITE_BYTE(0x0D00+0x09*2+1, 0x40);
	_I2C_WRITE_BYTE(0x0D00+0x09*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x0A*2+1, 0x80);
	_I2C_WRITE_BYTE(0x0D00+0x0A*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x0B*2+1, 0xC0);
	_I2C_WRITE_BYTE(0x0D00+0x0B*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x01*2+1, 0xAA);
	_I2C_WRITE_BYTE(0x0E00+0x01*2+0, 0xAA);

	_I2C_WRITE_BYTE(0x0E00+0x02*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x02*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x1E*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x1E*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x1F*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x1F*2+0, 0x00);

	//#if !DMD_ATSC3_USE_ESMT
	_I2C_WRITE_BYTE(0x0E00+0x1D*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x1D*2+0, 0x54);
	//#else
	//_I2C_WRITE_BYTE(0x0E00+0x1D*2+1, 0x00);
	//_I2C_WRITE_BYTE(0x0E00+0x1D*2+0, 0x44);
	//#endif

	_I2C_WRITE_BYTE(0x0E5D, 0x00);
	_I2C_WRITE_BYTE(0x0E5C, 0x55);

	_I2C_WRITE_BYTE(0x0E15, 0x00);
	_I2C_WRITE_BYTE(0x0E14, 0x55);

	//#if !DMD_ATSC3_USE_ESMT
	_I2C_WRITE_BYTE(0x0E75, 0x00);
	_I2C_WRITE_BYTE(0x0E74, 0xAA);
	//#else
	//_I2C_WRITE_BYTE(0x0E75, 0x00);
	//_I2C_WRITE_BYTE(0x0E74, 0x55);
	//#endif

	_I2C_WRITE_BYTE(0x0E00+0x24*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x24*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x26*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x26*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x27*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x27*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x05*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x05*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x28*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x28*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x1C*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x1C*2+0, 0x04);

	_I2C_WRITE_BYTE(0x0E00+0x25*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x25*2+0, 0x02);

	_I2C_WRITE_BYTE(0x0E00+0x29*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x29*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x36*2+1, 0x55);
	_I2C_WRITE_BYTE(0x0E00+0x36*2+0, 0x55);

	_I2C_WRITE_BYTE(0x0E00+0x37*2+1, 0x55);
	_I2C_WRITE_BYTE(0x0E00+0x37*2+0, 0x55);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE5);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x3F*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x3F*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x2A*2+1, 0xC0);
	_I2C_WRITE_BYTE(0x0E00+0x2A*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x3C*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x3C*2+0, 0x01);

	_I2C_WRITE_BYTE(0x0E00+0x42*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x42*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x16*2+1, 0xA6);
	_I2C_WRITE_BYTE(0x0D00+0x16*2+0, 0x21);

	_I2C_WRITE_BYTE(0x0E00+0x40*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x40*2+0, 0x01);

	_I2C_WRITE_BYTE(0x0E00+0x04*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x04*2+0, 0x3F);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE9);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x20);
	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x10);

	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x3E*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x3E*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE9);

	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);

	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x00);

	sys.DelayMS(1);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x08);
	sys.DelayMS(1);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x0C);
	sys.DelayMS(1);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x0E);
	sys.DelayMS(1);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x1E);
	sys.DelayMS(1);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x1F);

	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x1A);
	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x08);

	_I2C_WRITE_BYTE(0x0D00+0x20*2+1, 0x80);
	_I2C_WRITE_BYTE(0x0D00+0x20*2+0, 0x15);

	_I2C_WRITE_BYTE(0x0D00+0x30*2+1, 0x80);
	_I2C_WRITE_BYTE(0x0D00+0x30*2+0, 0x15);

	_I2C_WRITE_BYTE(0x0D00+0x40*2+1, 0x80);
	_I2C_WRITE_BYTE(0x0D00+0x40*2+0, 0x15);

	_I2C_WRITE_BYTE(0x0D00+0x50*2+1, 0x80);
	_I2C_WRITE_BYTE(0x0D00+0x50*2+0, 0x15);

	_I2C_WRITE_BYTE(0x0D00+0x23*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x23*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x33*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x33*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x43*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x43*2+0, 0x00);

	_I2C_WRITE_BYTE(0x0D00+0x53*2+1, 0x00);
	_I2C_WRITE_BYTE(0x0D00+0x53*2+0, 0x00);
	//miu init end
	#endif

	_I2C_WRITE_BYTE(0x0933, 0x14);  //enable miu clock

	#if FW_RUN_ON_DRAM_MODE
	_I2C_WRITE_BYTE(0x1018, 0x0C);  //enable dram_en
	_I2C_WRITE_BYTE(0x1019, 0x00);
	_I2C_WRITE_BYTE(0x1008, 0x00);
	_I2C_WRITE_BYTE(0x1009, 0x00);
	_I2C_WRITE_BYTE(0x100a, 0x00);
	_I2C_WRITE_BYTE(0x100b, 0x00);
	_I2C_WRITE_BYTE(0x100c, 0x00);
	_I2C_WRITE_BYTE(0x100d, 0x00);
	_I2C_WRITE_BYTE(0x100e, 0x00);
	_I2C_WRITE_BYTE(0x100f, 0x8C);
	#endif
}
/*
 *{
 *	// austin_inivec.v.cpp
 *	_I2C_WRITE_BYTE( 0x000a9f, 0x00);
 *	_I2C_WRITE_BYTE( 0x000a9e, 0x00);
 *	_I2C_WRITE_BYTE( 0x000a9d, 0x00);
 *	_I2C_WRITE_BYTE( 0x000a9c, 0x00);
 *	_I2C_WRITE_BYTE( 0x000a67, 0x12);
 *	_I2C_WRITE_BYTE( 0x000a66, 0x01);
 *
 *	_I2C_WRITE_BYTE( 0x000a6b, 0x18);
 *	_I2C_WRITE_BYTE( 0x000a6a, 0x03);
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE( 0x000a6b, 0x00);
 *	_I2C_WRITE_BYTE( 0x000a6a, 0x03);
 *	_I2C_WRITE_BYTE( 0x000a61, 0x24);
 *	_I2C_WRITE_BYTE( 0x000a60, 0x00);
 *
 *	//_I2C_WRITE_BYTE( 0x000a03, 0x04);
 *	//_I2C_WRITE_BYTE( 0x000a02, 0x40);
 *	//_I2C_WRITE_BYTE( 0x000a8a, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a19, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a18, 0x02);
 *	//_I2C_WRITE_BYTE( 0x000a1e, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a41, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a40, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a17, 0x05);
 *	//_I2C_WRITE_BYTE( 0x000a16, 0x05);
 *	//_I2C_WRITE_BYTE( 0x000a5d, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a5c, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a55, 0x0c);
 *	//_I2C_WRITE_BYTE( 0x000a54, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a57, 0x0c);
 *	//_I2C_WRITE_BYTE( 0x000a56, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a2f, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000a2e, 0x04);
 *	_I2C_WRITE_BYTE( 0x000913, 0x01);
 *	_I2C_WRITE_BYTE( 0x000912, 0x01);
 *	_I2C_WRITE_BYTE( 0x000915, 0x00);
 *	_I2C_WRITE_BYTE( 0x000914, 0x00);
 *	_I2C_WRITE_BYTE( 0x000917, 0x00);
 *	_I2C_WRITE_BYTE( 0x000916, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000925, 0x14);
 *	//_I2C_WRITE_BYTE( 0x000924, 0x18);
 *	_I2C_WRITE_BYTE( 0x000927, 0x00);
 *	_I2C_WRITE_BYTE( 0x000926, 0x20);
 *	_I2C_WRITE_BYTE( 0x000937, 0x00);
 *	_I2C_WRITE_BYTE( 0x000936, 0x11);
 *	_I2C_WRITE_BYTE( 0x000933, 0x00);
 *	_I2C_WRITE_BYTE( 0x000932, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000939, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000938, 0x00);
 *	_I2C_WRITE_BYTE( 0x000910, 0x01);
 *	_I2C_WRITE_BYTE( 0x000951, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009a3, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009a2, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009a5, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009a4, 0x40);
 *	//_I2C_WRITE_BYTE( 0x0009a0, 0x10);
 *	//sys.DelayMS(1);
 *	//_I2C_WRITE_BYTE( 0x0009af, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009ae, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009ab, 0x01);
 *	//_I2C_WRITE_BYTE( 0x0009aa, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009aa, 0x10);
 *	//_I2C_WRITE_BYTE( 0x0009b3, 0x00);
 *	//_I2C_WRITE_BYTE( 0x0009b2, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000940, 0x04);
 *	_I2C_WRITE_BYTE( 0x000937, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000936, 0x11);
 *	//_I2C_WRITE_BYTE( 0x000933, 0x10);
 *	_I2C_WRITE_BYTE( 0x000933, 0x10);
 *	_I2C_WRITE_BYTE( 0x000932, 0x00);
 *	//_I2C_WRITE_BYTE( 0x000A30, 0x01);
 *
 *	//miu init start
 *	_I2C_WRITE_BYTE(0x0D00+0x10*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x10*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x01*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x01*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x23*2+1, 0xFF);
 *	_I2C_WRITE_BYTE(0x0D00+0x23*2+0, 0xFE);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x33*2+1, 0xFF);
 *	_I2C_WRITE_BYTE(0x0D00+0x33*2+0, 0xFF);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x43*2+1, 0xFF);
 *	_I2C_WRITE_BYTE(0x0D00+0x43*2+0, 0xFF);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x53*2+1, 0xFF);
 *	_I2C_WRITE_BYTE(0x0D00+0x53*2+0, 0xFF);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x1B*2+1, 0x40);
 *	_I2C_WRITE_BYTE(0x0E00+0x1B*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x1A*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x1A*2+0, 0x00);
 *
 *	//MIU clock setting
 *	//miu_clk = 452838600/200;
 *	_I2C_WRITE_BYTE(0x0E00+0x18*2+1, 0x8F);
 *	_I2C_WRITE_BYTE(0x0E00+0x18*2+0, 0x5B);
 *	_I2C_WRITE_BYTE(0x0E00+0x19*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x19*2+0, 0x22);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x11*2+1, 0x20);
 *	_I2C_WRITE_BYTE(0x0E00+0x11*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x20);
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x30);
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+1, 0x20);
 *	_I2C_WRITE_BYTE(0x0E00+0x12*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x01*2+1, 0x01);
 *	_I2C_WRITE_BYTE(0x0D00+0x01*2+0, 0x11);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x02*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x02*2+0, 0x4B);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x03*2+1, 0x15);
 *	_I2C_WRITE_BYTE(0x0D00+0x03*2+0, 0x3C);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x04*2+1, 0x0A);
 *	_I2C_WRITE_BYTE(0x0D00+0x04*2+0, 0x44);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x05*2+1, 0x0E);
 *	_I2C_WRITE_BYTE(0x0D00+0x05*2+0, 0x43);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x06*2+1, 0x62);
 *	_I2C_WRITE_BYTE(0x0D00+0x06*2+0, 0x97);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x07*2+1, 0x10);
 *	_I2C_WRITE_BYTE(0x0D00+0x07*2+0, 0xFF);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x08*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x08*2+0, 0x31);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x09*2+1, 0x40);
 *	_I2C_WRITE_BYTE(0x0D00+0x09*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0A*2+1, 0x80);
 *	_I2C_WRITE_BYTE(0x0D00+0x0A*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0B*2+1, 0xC0);
 *	_I2C_WRITE_BYTE(0x0D00+0x0B*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x01*2+1, 0xAA);
 *	_I2C_WRITE_BYTE(0x0E00+0x01*2+0, 0xAA);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x02*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x02*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x1E*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x1E*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x1F*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x1F*2+0, 0x00);
 *
 *	//#if !DMD_ATSC3_USE_ESMT
 *	_I2C_WRITE_BYTE(0x0E00+0x1D*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x1D*2+0, 0x54);
 *	//#else
 *	//_I2C_WRITE_BYTE(0x0E00+0x1D*2+1, 0x00);
 *	//_I2C_WRITE_BYTE(0x0E00+0x1D*2+0, 0x44);
 *	//#endif
 *
 *	_I2C_WRITE_BYTE(0x0E5D, 0x00);
 *	_I2C_WRITE_BYTE(0x0E5C, 0x55);
 *
 *	_I2C_WRITE_BYTE(0x0E15, 0x00);
 *	_I2C_WRITE_BYTE(0x0E14, 0x55);
 *
 *	//#if !DMD_ATSC3_USE_ESMT
 *	_I2C_WRITE_BYTE(0x0E75, 0x00);
 *	_I2C_WRITE_BYTE(0x0E74, 0xAA);
 *	//#else
 *	//_I2C_WRITE_BYTE(0x0E75, 0x00);
 *	//_I2C_WRITE_BYTE(0x0E74, 0x55);
 *	//#endif
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x24*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x24*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x26*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x26*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x27*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x27*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x05*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x05*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x28*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x28*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x1C*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x1C*2+0, 0x04);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x25*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x25*2+0, 0x02);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x29*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x29*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x36*2+1, 0x55);
 *	_I2C_WRITE_BYTE(0x0E00+0x36*2+0, 0x55);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x37*2+1, 0x55);
 *	_I2C_WRITE_BYTE(0x0E00+0x37*2+0, 0x55);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE5);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x3F*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x3F*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x2A*2+1, 0xC0);
 *	_I2C_WRITE_BYTE(0x0E00+0x2A*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x3C*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x3C*2+0, 0x01);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x42*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x42*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x16*2+1, 0xA6);
 *	_I2C_WRITE_BYTE(0x0D00+0x16*2+0, 0x21);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x40*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x40*2+0, 0x01);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x04*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x04*2+0, 0x3F);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE9);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x01);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x0C);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x20);
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x10);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x3E*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x3E*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x00*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xE9);
 *
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0E00+0x07*2+0, 0xA9);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x00);
 *
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x08);
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x0C);
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x0E);
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x1E);
 *	sys.DelayMS(1);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x00*2+0, 0x1F);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+1, 0x1A);
 *	_I2C_WRITE_BYTE(0x0D00+0x0F*2+0, 0x08);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x20*2+1, 0x80);
 *	_I2C_WRITE_BYTE(0x0D00+0x20*2+0, 0x15);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x30*2+1, 0x80);
 *	_I2C_WRITE_BYTE(0x0D00+0x30*2+0, 0x15);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x40*2+1, 0x80);
 *	_I2C_WRITE_BYTE(0x0D00+0x40*2+0, 0x15);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x50*2+1, 0x80);
 *	_I2C_WRITE_BYTE(0x0D00+0x50*2+0, 0x15);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x23*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x23*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x33*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x33*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x43*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x43*2+0, 0x00);
 *
 *	_I2C_WRITE_BYTE(0x0D00+0x53*2+1, 0x00);
 *	_I2C_WRITE_BYTE(0x0D00+0x53*2+0, 0x00);
 *	//miu init end
 *
 *	_I2C_WRITE_BYTE(0x0933, 0x14);  //enable miu clock
 *
 *	#if FW_RUN_ON_DRAM_MODE
 *	_I2C_WRITE_BYTE(0x1018, 0x0C);  //enable dram_en
 *	_I2C_WRITE_BYTE(0x1019, 0x00);
 *	_I2C_WRITE_BYTE(0x1008, 0x00);
 *	_I2C_WRITE_BYTE(0x1009, 0x00);
 *	_I2C_WRITE_BYTE(0x100a, 0x00);
 *	_I2C_WRITE_BYTE(0x100b, 0x00);
 *	_I2C_WRITE_BYTE(0x100c, 0x00);
 *	_I2C_WRITE_BYTE(0x100d, 0x00);
 *	_I2C_WRITE_BYTE(0x100e, 0x00);
 *	_I2C_WRITE_BYTE(0x100f, 0x8C);
 *	#endif
 *}
 */
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN
#if !IS_MULTI_MCU || (IS_MULTI_MCU && DIGRF_INSIDE)
static MS_BOOL _SSPI_Init(MS_U8 u8DeviceNum)
{
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
	if (sys.MSPI_Init_Ext(u8DeviceNum) == E_MSPI_OK)
		return TRUE;
	else
		return FALSE;
	#else
	sys.mspi_init_ext(u8DeviceNum);

	return TRUE;
	#endif
}

static MS_BOOL _SSPI_MIU_Writes(MS_U32 u32Addr, MS_U8 *pdata, MS_U16 u16Size)
{
	MS_BOOL bRet = TRUE;
	MS_U8 Wdata[5];

	Wdata[0] = MIU_W_CMD;
	Wdata[1] = u32Addr & 0xFF;
	Wdata[2] = (u32Addr >> 8) & 0xFF;
	Wdata[3] = (u32Addr >> 16) & 0xFF;
	Wdata[4] = (u32Addr >> 24);

	// Write operation
	sys.mspi_slaveenable(TRUE);
	// send write address
	sys.mspi_write(Wdata, sizeof(Wdata));
	// send data
	sys.mspi_write(pdata, u16Size);
	sys.mspi_slaveenable(FALSE);

	return bRet;
}
/*
 *#if !defined MTK_PROJECT
 *static MS_BOOL _SSPI_MIU_Reads(MS_U32 u32Addr, MS_U8 *pdata, MS_U16 u16Size)
 *{
 *	MS_BOOL bRet = TRUE;
 *	MS_U8 Rdata[SPI_DEVICE_BUFFER_SIZE];
 *	MS_U16 dataLen, i, j=0;
 *
 *	do
 *	{
 *		dataLen = (u16Size>16?16:u16Size);//(len>24?24:len);
 *
 *		Rdata[0] = MIU_R_CMD;
 *		Rdata[1] = u32Addr & 0xFF;
 *		Rdata[2] = (u32Addr >> 8) & 0xFF;
 *		Rdata[3] = (u32Addr >> 16)& 0xFF;
 *		Rdata[4] = (u32Addr >> 24);
 *		Rdata[5] = dataLen+1;
 *
 *		// send read command to read data
 *		sys.MSPI_SlaveEnable(TRUE);
 *		sys.MSPI_Write(Rdata,6);
 *		sys.MSPI_SlaveEnable(FALSE);
 *
 *		// read operation
 *		Rdata[0] = MIU_ST_CMD;
 *		sys.MSPI_SlaveEnable(TRUE);
 *		sys.MSPI_Write(Rdata,1);
 *		sys.MSPI_Read(Rdata, dataLen+1);
 *		sys.MSPI_SlaveEnable(FALSE);
 *
 *		if(Rdata[0] != 0x0A)
 *		{
 *			PRINT("MDrv_SS_MIU_Reads fail, status=0x%x\n", Rdata[0] );
 *			return FALSE;
 *		}
 *
 *		for (i=1; i<dataLen+1; i++, j++)
 *		{
 *			pdata[j] = Rdata[i];
 *		}
 *
 *		u16Size -= dataLen;
 *		u32Addr += dataLen;
 *	} while(u16Size);
 *
 *	return bRet;
 *}
 *#endif
 */
static MS_BOOL _SSPI_RIU_Write8(MS_U16 u16Addr, MS_U8 data)
{
	MS_BOOL bRet = TRUE;
	MS_U8 Wdata[4];

	Wdata[0] = RIU_W1_CMD;
	Wdata[1] = u16Addr & 0xFF;
	Wdata[2] = (u16Addr >> 8) & 0xFF;
	Wdata[3] = data;

	// Write operation
	sys.mspi_slaveenable(TRUE);
	// send write address & data
	sys.mspi_write(Wdata, 4);
	sys.mspi_slaveenable(FALSE);

	return bRet;
}

static MS_BOOL _SSPI_RIU_Read8(MS_U16 u16Addr, MS_U8 *pdata)
{
	MS_BOOL bRet = TRUE;
	MS_U8 Rdata[4];

	Rdata[0] = RIU_R1T_CMD;
	Rdata[1] = u16Addr & 0xFF;
	Rdata[2] = (u16Addr >> 8) & 0xFF;
	Rdata[3] = 0x00;

	sys.mspi_slaveenable(TRUE);
	// send read command to read data
	sys.mspi_write(Rdata, 4);
	// read operation
	sys.mspi_read(pdata, 1);
	sys.mspi_slaveenable(FALSE);

	return bRet;
}
#endif // #if !IS_MULTI_MCU || (IS_MULTI_MCU && DIGRF_INSIDE)
#endif // #if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN

#if IS_MULTI_DMD
static MS_BOOL _SEL_DMD(void)
{
	return _I2C_WRITE_BYTE(REG_DMD_SEL, *(pExtChipChId));
}
#endif

static MS_BOOL _HAL_EXTERN_ATSC_Download(void)
{
	MS_BOOL bRet = TRUE;
	MS_U16 index;
	MS_U16 RAM_BASE, RAM_Address;
	const MS_U8 *ATSC_table;
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN
	#if !IS_MULTI_MCU || (IS_MULTI_MCU && DIGRF_INSIDE)
	MS_U32 RAM_SSPI_Address;
	#endif
	#endif
	MS_U8  u8TmpData;
	MS_U16 u16AddressOffset;
	#if IS_MULTI_MCU && DIGRF_INSIDE
	MS_U8  backendRegs[16] = {0};
	MS_U8  u8AciCoefTable[92];
	#endif
	#if IS_MULTI_MCU
	MS_U8  u8i;
	#endif


	if (pres->sdmd_atsc_pridata.bdownloaded) {
		#if !IS_MULTI_MCU
		// Enable MCU Reset
		bRet &= _I2C_WRITE_BYTE(REG_MCU_RST, 0x03);

		// Disable MCU Reset
		bRet &= _I2C_WRITE_BYTE(REG_MCU_RST, 0x00);
		#else
		// Enable MCU Reset
		bRet &= _I2C_WRITE_BYTE(REG_TOP_MCU_BASE+0x80+((*(pExtChipChId))*0x20), 0x01);

		// Disable MCU Reset
		bRet &= _I2C_WRITE_BYTE(REG_TOP_MCU_BASE+0x80+((*(pExtChipChId))*0x20), 0x00);
		#endif

		sys.delayms(20);

		return bRet;
	}

	bRet &= _I2C_READ_BYTE(0x0902, &u8TmpData);
	if (u8TmpData == 0x01) {
		ATSC_table = &EXTERN_ATSC_U03_table[0];
		#if !IS_MULTI_MCU || !DIGRF_INSIDE
		u16Lib_size = sizeof(EXTERN_ATSC_U03_table);
		#endif
	} else {
		ATSC_table = &EXTERN_ATSC_table[0];
		#if !IS_MULTI_MCU || !DIGRF_INSIDE
		u16Lib_size = sizeof(EXTERN_ATSC_table);
		#endif
	}

	#if FW_RUN_ON_DRAM_MODE
	RAM_BASE		 = 0x0000;
	RAM_SSPI_Address = 0x00000000;
	#else
	RAM_BASE		 = 0x8000;
	RAM_SSPI_Address = 0x80000000;
	#endif

	#if !IS_MULTI_MCU || (IS_MULTI_MCU && DIGRF_INSIDE)
	#if IS_MULTI_MCU && DIGRF_INSIDE
	_isDigRfEn = TRUE;
	#endif

	#if IS_MULTI_MCU && DIGRF_INSIDE
	// Enable MCU SRAM CLOCK
	bRet &= _I2C_WRITE_BYTE(0x0917, 0x1C);
	#endif

	// Enable MCU Reset
	bRet &= _I2C_WRITE_BYTE(REG_MCU_RST, 0x03);

	bRet &= _I2C_Channel_Change(0);

	// Disable Watch-Dog
	bRet &= _I2C_WRITE_BYTE(0x3008, 0x00);
	bRet &= _I2C_WRITE_BYTE(0x3009, 0x00);
	bRet &= _I2C_WRITE_BYTE(0x300A, 0x00);
	bRet &= _I2C_WRITE_BYTE(0x300B, 0x00);

	bRet &= _I2C_Channel_Change(3);

	#if !FW_RUN_ON_DRAM_MODE

	// Enable SRAM XDATA mapping
	bRet &= _I2C_WRITE_BYTE(0x10E1, 0x20); // Start address
	bRet &= _I2C_WRITE_BYTE(0x10E0, 0x3F); // End address
	#if USE_PSRAM_32KB
	bRet &= _I2C_WRITE_BYTE(0x10E6, 0x18);
	bRet &= _I2C_WRITE_BYTE(0x10E4, 0x20);
	#else
	bRet &= _I2C_WRITE_BYTE(0x10E6, 0x08);
	#endif

	#endif // #if !FW_RUN_ON_DRAM_MODE

	if (pres->sdmd_atsc_initdata.bIsUseSspiLoadCode == TRUE) {
		#if SSPI_USE_MPLL_CLK
		_MIU_INIT();
		#endif

		#ifdef MTK_PROJECT
		bRet &= _I2C_READ_BYTE(0x0975, &u8TmpData);
		u8TmpData &= ~0x03;
		bRet &= _I2C_WRITE_BYTE(0x0975, u8TmpData);
		#endif

		#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN
		//turn on all pad in
		bRet &= _I2C_WRITE_BYTE(0x0950, 0x00);
		bRet &= _I2C_WRITE_BYTE(REG_ALL_PAD_IN, 0x00);

		if (pres->sdmd_atsc_initdata.bIsSspiUseTsPin == TRUE) {
			PRINT("##### MSPI USE TS PIN : ENABLE #####\n");
			bRet &= _I2C_READ_BYTE(0x0976, &u8TmpData);
			u8TmpData &= ~0x03;
			u8TmpData |= 0x02;
			bRet &= _I2C_WRITE_BYTE(0x0976, u8TmpData);
		} else {
			PRINT("##### MSPI USE SPI PIN : ENABLE #####\n");
			bRet &= _I2C_READ_BYTE(0x0976, &u8TmpData);
			u8TmpData &= ~0x03;
			u8TmpData |= 0x01;
			bRet &= _I2C_WRITE_BYTE(0x0976, u8TmpData);
		}
		#endif
	}

	bRet &= _I2C_Channel_Change(0);

	if (pres->sdmd_atsc_initdata.bIsUseSspiLoadCode == FALSE) {
		for (index = 0; index < u16Lib_size; ) {
			RAM_Address = RAM_BASE + index;
			if (index + LOAD_CODE_I2C_BLOCK_NUM-1 < u16Lib_size) {
				#if !DIGRF_INSIDE
				bRet &= _I2C_WRITE_BYTES(RAM_Address,
				(MS_U8 *)(ATSC_table+index), LOAD_CODE_I2C_BLOCK_NUM);
				#else
				bRet &= _I2C_WRITE_BYTES(RAM_Address,
				(MS_U8 *)(DIGRF_table+index), LOAD_CODE_I2C_BLOCK_NUM);
				#endif
				index = index + LOAD_CODE_I2C_BLOCK_NUM - 1;
			} else {
				#if !DIGRF_INSIDE
				bRet &= _I2C_WRITE_BYTES(RAM_Address,
				(MS_U8 *)(ATSC_table+index), u16Lib_size-index);
				#else
				bRet &= _I2C_WRITE_BYTES(RAM_Address,
				(MS_U8 *)(DIGRF_table+index), u16Lib_size-index);
				#endif
				index = u16Lib_size;
			}
		}

		HAL_EXTERN_ATSC_DBINFO(PRINT("ATSC firmware code size = 0x[%x]\n", u16Lib_size));

		if (u16Lib_size >= 0x6b80) {
			HAL_EXTERN_ATSC_DBINFO(PRINT("Firmware code size over 0x6B80!!!\n"));
			bRet = FALSE;
		}
	}
	#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN
	else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("HAL EXTERN ATSC Download by MSPI\n"));

		_SSPI_Init(0);

		_SSPI_RIU_Read8(0x0974, &u8TmpData);
		u8TmpData |= 0x10;
		_SSPI_RIU_Write8(0x0974, u8TmpData);

		#if !DIGRF_INSIDE
		_SSPI_MIU_Writes(RAM_SSPI_Address, (MS_U8 *)ATSC_table, u16Lib_size);
		#else
		_SSPI_MIU_Writes(RAM_SSPI_Address, (MS_U8 *)DIGRF_table, u16Lib_size);
		#endif

		_SSPI_RIU_Read8(0x0974, &u8TmpData);
		u8TmpData &= ~0x10;
		_SSPI_RIU_Write8(0x0974, u8TmpData);
	}
	#endif

	#if !DIGRF_INSIDE
	if (EXTERN_ATSC_U03_table[0x3FC] == 0x55 && EXTERN_ATSC_U03_table[0x3FD] == 0x54 &&
		EXTERN_ATSC_U03_table[0x3FC] == 0x4F && EXTERN_ATSC_U03_table[0x3FC] == 0x46) {
		u16AddressOffset = (ATSC_table[0x400] << 8)|ATSC_table[0x401];

		//Set IF value
		RAM_Address = RAM_BASE + u16AddressOffset;
		u8TmpData = (pres->sdmd_atsc_initdata.if_khz) & 0xFF;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);
		RAM_Address = RAM_BASE + u16AddressOffset + 1;
		u8TmpData = (pres->sdmd_atsc_initdata.if_khz) >> 8;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

		//Set IQSwap mode
		RAM_Address = RAM_BASE + u16AddressOffset + 2;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, pres->sdmd_atsc_initdata.biqswap);
	} else {
		u16AddressOffset = 0x6B80;

		//Set IF value
		RAM_Address = RAM_BASE + u16AddressOffset + 0x0F;//BACKEND_IF_KHZ_0
		u8TmpData = (pres->sdmd_atsc_initdata.if_khz) & 0xFF;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

		RAM_Address = RAM_BASE + u16AddressOffset + 0x10;//BACKEND_IF_KHZ_1
		u8TmpData = (pres->sdmd_atsc_initdata.if_khz) >> 8;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

		//Set TS mode
		RAM_Address = RAM_BASE + u16AddressOffset + 0x0C;//TS_SERIAL MODE
		u8TmpData = (pres->sdmd_atsc_initdata.u5TsConfigByte_DivNum << 3) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_ClockInv << 2) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_DataSwap << 1) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_SerialMode);
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

		//Set IQSwap mode
		RAM_Address = RAM_BASE + u16AddressOffset + 0x0E;//BACKEND_DMD_IQ_SWAP
		u8TmpData = pres->sdmd_atsc_initdata.biqswap;
		bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);
	}
	#else // #if !DIGRF_INSIDE
	for (u8i = 0; u8i < 92; u8i = u8i + 2) {
		u8AciCoefTable[u8i]   = _drf_atsc_aci_coef_6M[u8i/2] >> 8;
		u8AciCoefTable[u8i+1] = _drf_atsc_aci_coef_6M[u8i/2];
	}

	if (pres->sdmd_atsc_initdata.if_khz == 5000) {
		backendRegs[0] = 0x88;
		backendRegs[1] = 0x13;
	} else if (pres->sdmd_atsc_initdata.if_khz == 6000) {
		// 6Mhz   7000
		backendRegs[0] = 0x70;
		backendRegs[1] = 0x17;
	} else if (pres->sdmd_atsc_initdata.if_khz == 7000) {
		// 7Mhz   7000
		backendRegs[0] = 0x58;
		backendRegs[1] = 0x1B;
	} else {
		//8 Mhz   8000
		backendRegs[0] = 0x40;
		backendRegs[1] = 0x1F;
	}

	backendRegs[2] = 6;  //ATSC use 6Mhz
	backendRegs[3] = 1;

	//Get address offset
	u16AddressOffset = (DIGRF_table[0x400] << 8)|DIGRF_table[0x401];

	//Set IF value
	RAM_Address = RAM_BASE + u16AddressOffset;
	u8TmpData = backendRegs[0];
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);
	RAM_Address = RAM_BASE + u16AddressOffset + 1;
	u8TmpData = backendRegs[1];
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

	//Set Bandwidth
	RAM_Address = RAM_BASE + u16AddressOffset + 2;
	u8TmpData = backendRegs[2];
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

	//Set tv mode
	RAM_Address = RAM_BASE + u16AddressOffset + 3;
	u8TmpData = backendRegs[3];
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

	//Get address offset find coef table address
	u16AddressOffset = (DIGRF_table[0x40C] << 8)|DIGRF_table[0x40D];

	if (u16AddressOffset) {
		u16AddressOffset = (DIGRF_table[u16AddressOffset] << 8) |
		DIGRF_table[u16AddressOffset + 1];
		RAM_Address = RAM_BASE + u16AddressOffset;
		bRet &= _I2C_WRITE_BYTES(RAM_Address, (MS_U8 *)(u8AciCoefTable), 92);
	}
	#endif // #if !DIGRF_INSIDE

	bRet &= _I2C_Channel_Change(3);

	if (pres->sdmd_atsc_initdata.bIsUseSspiLoadCode == TRUE) {
		#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN || DMD_ATSC_3PARTY_MSPI_EN
		if (pres->sdmd_atsc_initdata.bIsSspiUseTsPin == TRUE) {
			PRINT("##### MSPI USE TS PIN : DISABLE #####\n");
			bRet = _I2C_READ_BYTE(0x0976, &u8TmpData);
			u8TmpData &= ~0x02;
			bRet = _I2C_WRITE_BYTE(0x0976, u8TmpData);
		}
		#endif
	}

	#if !FW_RUN_ON_DRAM_MODE

	// Disable SRAM XDATA mapping
	bRet &= _I2C_WRITE_BYTE(0x10E6, 0x00);

	// Set program SRAM start address
	bRet &= _I2C_WRITE_BYTE(0x1004, 0x00);
	bRet &= _I2C_WRITE_BYTE(0x1005, 0x00);
	bRet &= _I2C_WRITE_BYTE(0x1000, 0x00);

	// Set program SRAM end address
	bRet &= _I2C_WRITE_BYTE(0x1006, 0x7F);
	bRet &= _I2C_WRITE_BYTE(0x1007, 0xFF);
	bRet &= _I2C_WRITE_BYTE(0x1002, 0x00);

	// Boot from SRAM
	bRet &= _I2C_WRITE_BYTE(0x1018, 0x01);

	#endif // #if !FW_RUN_ON_DRAM_MODE

	// Disable MCU Reset
	bRet &= _I2C_WRITE_BYTE(REG_MCU_RST, 0x00);

	#if IS_MULTI_MCU && DIGRF_INSIDE
	_isDigRfEn = FALSE;
	#endif

	#if DIGRF_INSIDE
	 //Reset tuner FSM
	 bRet &= _I2C_READ_BYTE(0x0990, &u8TmpData);
	 bRet &= _I2C_WRITE_BYTE(0x0990, u8TmpData&(~0x04));

	 sys.DelayMS(5);

	 bRet &= _I2C_READ_BYTE(0x0990, &u8TmpData);
	 bRet &= _I2C_WRITE_BYTE(0x0990, (u8TmpData | 0x04));

	 sys.DelayMS(5);
	#else // #if DIGRF_INSIDE
	pres->sdmd_atsc_pridata.bdownloaded = TRUE;
	#endif // #if DIGRF_INSIDE
	#endif // #if !IS_MULTI_MCU || (IS_MULTI_MCU && DIGRF_INSIDE)

	#if IS_MULTI_DMD
	for (u8i = 0, u8TmpData = *(pExtChipChNum); u8i < u8TmpData; u8i++) {
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x80+(u8i*0x20))), 0x01); // Enable MCU reset
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x86+(u8i*0x20))), 0x01); // Enable MCU periphal reset
		// SRAM enable
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x81 + (u8i * 0x20))), 0x00);
		// Select for xdata setting from top riu to dmd riu
		bRet &= _I2C_WRITE_BYTE(
		((REG_DMD0_MCU2_BASE+(u8i*0x100)) + 0x1C), 0x01);
		// Disable MCU periphal reset
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x86+(u8i*0x20))), 0x00);

		//set lower bound "xb_eram_hb[5:0]"
		// `DBG.iic_write( 2, (`RIUBASE_MCU + 8'he1), 8'h20); //xdata address[15:10]
		bRet &= _I2C_WRITE_BYTE(
		((REG_DMD0_MCU2_BASE + (u8i * 0x100)) + 0xe1), RAM_BASE >> 10);
		//set upper bound "xb_eram_lb[5:0]"
		// `DBG.iic_write( 2, (`RIUBASE_MCU + 8'he0), 8'h3f); //xdata address[15:10]
		bRet &= _I2C_WRITE_BYTE(
		((REG_DMD0_MCU2_BASE + (u8i * 0x100)) + 0xe0), (RAM_BASE+0x7FFF) >> 10);
		//set "reg_xd2eram_hk_mcu_psram_en"
		// `DBG.iic_write( 2, (`RIUBASE_MCU + 8'he6), 8'h08);
		// Enable SRAM XDATA mapping
		bRet &= _I2C_WRITE_BYTE(((REG_DMD0_MCU2_BASE+(u8i*0x100)) + 0xe6), 0x11);
	}

	//  01:all dmd, 00:single dmd
	bRet &= _I2C_WRITE_BYTE(0x3C28, 0x01);

	bRet &= _I2C_Channel_Change(1);

	//u16Lib_size = sizeof(EXTERN_ATSC_table);

	for (index = 0; index < u16Lib_size; ) {
		RAM_Address = RAM_BASE + index;
		if (index + LOAD_CODE_I2C_BLOCK_NUM - 1 < u16Lib_size) {
			bRet &= _I2C_WRITE_BYTES(RAM_Address,
			(MS_U8 *)(ATSC_table+index), LOAD_CODE_I2C_BLOCK_NUM);
			index = index + LOAD_CODE_I2C_BLOCK_NUM - 1;
		} else {
			bRet &= _I2C_WRITE_BYTES(RAM_Address,
			(MS_U8 *)(ATSC_table+index), u16Lib_size-index);
			index = u16Lib_size;
		}
	}

	HAL_EXTERN_ATSC_DBINFO(PRINT("ATSC firmware code size = 0x[%x]\n", u16Lib_size));

	if (u16Lib_size >= 0x6b80) {
		HAL_EXTERN_ATSC_DBINFO(PRINT("Firmware code size over 0x6B80!!!\n"));
		bRet = FALSE;
	}

	u16AddressOffset = (ATSC_table[0x400] << 8)|ATSC_table[0x401];

	//Set IF value
	RAM_Address = RAM_BASE + u16AddressOffset;
	u8TmpData = (pRes->sDMD_ATSC_InitData.u16IF_KHZ) & 0xFF;
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);
	RAM_Address = RAM_BASE + u16AddressOffset + 1;
	u8TmpData = (pRes->sDMD_ATSC_InitData.u16IF_KHZ) >> 8;
	bRet &= _I2C_WRITE_BYTE(RAM_Address, u8TmpData);

	//Set IQSwap mode
	RAM_Address = RAM_BASE + u16AddressOffset + 2;
	bRet &= _I2C_WRITE_BYTE(RAM_Address, pRes->sDMD_ATSC_InitData.bIQSwap);

	bRet &= _I2C_Channel_Change(3);

	for (u8i = 0, u8TmpData = *(pExtChipChNum); u8i < u8TmpData; u8i++) {
		// Select for xdata setting from dmd riu to top riu
		bRet &= _I2C_WRITE_BYTE(
		((REG_DMD0_MCU2_BASE+(u8i*0x100)) + 0x1C), 0x00);
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x81+(u8i*0x20))), 0x01); // SRAM disable
		bRet &= _I2C_WRITE_BYTE(
		(REG_TOP_MCU_BASE + (0x80+(u8i*0x20))), 0x00); // Disable MCU reset

		pRes[u8i].sDMD_ATSC_PriData.bDownloaded = TRUE;
	}
	#endif // #if IS_MULTI_DMD

	sys.delayms(20);
	return TRUE;
}

static void _HAL_EXTERN_ATSC_FWVERSION(void)
{
	MS_U8 data1, data2, data3;

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &data1);
	#else
	_I2C_READ_BYTE(0x099F, &data1);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (data1 != 0x55) {
		// Check if FW is not UTOF
		_I2C_READ_BYTE(0x0995, &data1); // REG_BASE[DMDTOP_WR_DBG_04]
		_I2C_READ_BYTE(0x0B54, &data2); // REG_BASE[DMDTOP_WR_DBG_15]
		_I2C_READ_BYTE(0x0B55, &data3); // REG_BASE[DMDTOP_WR_DBG_16]
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C4, &data1);
		_I2C_READ_BYTE(0x20C5, &data2);
		_I2C_READ_BYTE(0x20C6, &data3);
		#else
		_I2C_READ_BYTE(0x0995, &data1);
		_I2C_READ_BYTE(0x0996, &data2);
		_I2C_READ_BYTE(0x0997, &data3);
		#endif
	}
	pr_err("EXTERN_ATSC_FW_VERSION:%x.%x.%x\n", data1, data2, data3);
}

static MS_BOOL _HAL_EXTERN_ATSC_Exit(void)
{
	MS_BOOL bRet = TRUE;
	MS_U8 u8Data = 0xFF;

	//Reset FSM
	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (u8Data != 0x55) {
		// Check if FW is not UTOF
		if (_I2C_WRITE_BYTE(0x0990, 0x00) == FALSE)
			return FALSE;

		while (!u8Data) {
			if (_I2C_READ_BYTE(0x0992, &u8Data) == FALSE)
				return FALSE;
		}
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C0, 0x00) == FALSE)
			return FALSE;

		while (u8Data != 0x02) {
			if (_I2C_READ_BYTE(0x20C1, &u8Data) == FALSE)
				return FALSE;
		}
		#else
		if (_I2C_WRITE_BYTE(0x0990, 0x00) == FALSE)
			return FALSE;

		while (u8Data != 0x02) {
			if (_I2C_READ_BYTE(0x0992, &u8Data) == FALSE)
				return FALSE;
		}
		#endif
	}

	#if !DIGRF_INSIDE
	// Disable TS pad
	bRet = _I2C_READ_BYTE(0x095A, &u8Data);
	u8Data = 0x00;
	bRet = _I2C_WRITE_BYTE(0x095A, u8Data);

	// [0][4] ifagc/rfagc disable
	bRet = _I2C_READ_BYTE(0x0a30, &u8Data);
	u8Data &= ~0x11;
	bRet = _I2C_WRITE_BYTE(0x0a30, u8Data);
	#endif

	return bRet;
}

#if DIGRF_INSIDE
static MS_BOOL _HAL_EXTERN_ATSC_SET_DIGRF_FREQ(MS_U32 u32RfFreq)
{
	MS_BOOL bRet = TRUE;

	#if IS_MULTI_MCU && DIGRF_INSIDE
	_isDigRfEn = TRUE;
	#endif

	u32RfFreq = (float)(u32RfFreq / 1000) * 4;

	// Set RF frequency
	if ((*(pExtChipChId)) == 0) {
		bRet = _I2C_WRITE_BYTE(0x1814, (u32RfFreq & 0xFF));  // DBB0
		bRet = _I2C_WRITE_BYTE(0x1815, ((u32RfFreq >> 8) & 0xFF));
	} else if ((*(pExtChipChId)) == 1) {
		bRet = _I2C_WRITE_BYTE(0x1816, (u32RfFreq & 0xFF));  // DBB1
		bRet = _I2C_WRITE_BYTE(0x1817, ((u32RfFreq >> 8) & 0xFF));
	} else if ((*(pExtChipChId)) == 2) {
		bRet = _I2C_WRITE_BYTE(0x1818, (u32RfFreq & 0xFF));  // DBB2
		bRet = _I2C_WRITE_BYTE(0x1819, ((u32RfFreq >> 8) & 0xFF));
	} else if ((*(pExtChipChId)) == 3) {
		bRet = _I2C_WRITE_BYTE(0x181A, (u32RfFreq & 0xFF));  // DBB3
		bRet = _I2C_WRITE_BYTE(0x181B, ((u32RfFreq >> 8) & 0xFF));
	} else {
		bRet = _I2C_WRITE_BYTE(0x1814, (u32RfFreq & 0xFF));  // DBB0
		bRet = _I2C_WRITE_BYTE(0x1815, ((u32RfFreq >> 8) & 0xFF));
	}

	#if IS_MULTI_MCU && DIGRF_INSIDE
	_isDigRfEn = FALSE;
	#endif

	HAL_EXTERN_ATSC_DBINFO(PRINT("demod id %d SetRfFreq done\r\n", (id&0x0F)));

	return bRet;
}
#endif

static MS_BOOL _HAL_EXTERN_ATSC_SoftReset(void)
{
	MS_U8 u8Data = 0xFF;

	//Reset FSM
	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (u8Data != 0x55) {
		// Check if FW is not UTOF
		if (_I2C_WRITE_BYTE(0x0990, 0x00) == FALSE)
			return FALSE;

		while (u8Data != 0x0) {
			if (_I2C_READ_BYTE(0x0992, &u8Data) == FALSE)
				return FALSE;
		}

		//Execute demod top reset
		_I2C_READ_BYTE(0x0B20, &u8Data);
		_I2C_WRITE_BYTE(0x0B20, (u8Data|0x01));
		return _I2C_WRITE_BYTE(0x0B20, (u8Data&(~0x01)));
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C0, 0x00) == FALSE)
			return FALSE;

		while (u8Data != 0x02) {
			if (_I2C_READ_BYTE(0x20C1, &u8Data) == FALSE)
				return FALSE;
		}

		return TRUE;
		#else
		if (_I2C_WRITE_BYTE(0x0990, 0x00) == FALSE)
			return FALSE;

		while (u8Data != 0x02) {
			if (_I2C_READ_BYTE(0x0992, &u8Data) == FALSE)
				return FALSE;
		}

		return TRUE;
		#endif
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_SetVsbMode(void)
{
	#if !J83ABC_ENABLE
	MS_U8 u8Data = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (u8Data != 0x55) {
		// Check if FW is not UTOF
		return _I2C_WRITE_BYTE(0x0990, 0x08);
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C2, 0x02) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x20C0, 0x04);
		#else
		if (_I2C_WRITE_BYTE(0x0993, 0x02) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
		#endif
	}
}

#if SUPPORT_ATSC3
static MS_BOOL _HAL_EXTERN_ATSC_SetOFDMMode(void)
{
	MS_U8 u8TmpData;
	MS_U16 u16PLPConfigDataAddr;
	MS_BOOL bRet = TRUE;

	//Get the base address of FW HAL_PRI_XDATA_BASE
	u16PLPConfigDataAddr = 0;
	bRet &= _READ_MBX(FW_PRI_XDATA_BASE, &u8TmpData); //0x4008
	u16PLPConfigDataAddr = u8TmpData << 8;
	bRet &= _READ_MBX(FW_PRI_XDATA_BASE+1, &u8TmpData); //0x4009
	u16PLPConfigDataAddr |= u8TmpData;

	//SET PLP data to u16PLPConfigDataAddr
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 0, pres->sdmd_atsc_initdata.u8PLP_num);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 1, pres->sdmd_atsc_initdata.u8PLP_id[0]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 2, pres->sdmd_atsc_initdata.u8PLP_id[1]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 3, pres->sdmd_atsc_initdata.u8PLP_id[2]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 4, pres->sdmd_atsc_initdata.u8PLP_id[3]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 5, pres->sdmd_atsc_initdata.u8PLP_layer[0]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 6, pres->sdmd_atsc_initdata.u8PLP_layer[1]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 7, pres->sdmd_atsc_initdata.u8PLP_layer[2]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 8, pres->sdmd_atsc_initdata.u8PLP_layer[3]);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 9, pres->sdmd_atsc_initdata.u8Minor_Version);
	bRet &= _WRITE_MBX(u16PLPConfigDataAddr + 10, pres->sdmd_atsc_initdata.u8IsKRMode);

	if (_I2C_WRITE_BYTE(0x20C2, 0x01) == FALSE)
		return FALSE;
	return _I2C_WRITE_BYTE(0x20C0, 0x04);
}

static MS_BOOL _HAL_EXTERN_ATSC_SetEASMode(void)
{
	if (_I2C_WRITE_BYTE(0x20C2, 0x11) == FALSE)
		return FALSE;
	else
		return _I2C_WRITE_BYTE(0x20C0, 0x04);
}
#endif

static MS_BOOL _HAL_EXTERN_ATSC_Set64QamMode(void)
{
	#if !J83ABC_ENABLE
	MS_U8 u8Data = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	// Check if FW is not UTOF
	if (u8Data != 0x55) {
		_I2C_WRITE_BYTE(0x0C1B, 0x00); // Clear QAM&SR force mode

		if (_I2C_WRITE_BYTE(0x0994, 0x00) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C2, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x20C0, 0x04);
		#else
		if (_I2C_WRITE_BYTE(0x0993, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
		#endif
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_Set256QamMode(void)
{
	#if !J83ABC_ENABLE
	MS_U8 u8Data = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	// Check if FW is not UTOF
	if (u8Data != 0x55) {
		_I2C_WRITE_BYTE(0x0C1B, 0x00); // Clear QAM&SR force mode

		if (_I2C_WRITE_BYTE(0x0994, 0x01) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C2, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x20C0, 0x04);
		#else
		if (_I2C_WRITE_BYTE(0x0993, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
		#endif
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_SetModeClean(void)
{
	#if !J83ABC_ENABLE
	MS_U8 u8Data = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &u8Data);
	#else
	_I2C_READ_BYTE(0x099F, &u8Data);
	#endif
	#endif

	#if !J83ABC_ENABLE
	 // Check if FW is not UTOF
	if (u8Data != 0x55) {
		_I2C_WRITE_BYTE(0x0C1B, 0x00); // Clear QAM&SR force mode

		return _I2C_WRITE_BYTE(0x0990, 0x00);
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		if (_I2C_WRITE_BYTE(0x20C2, 0x00) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x20C0, 0x00);
		#else
		if (_I2C_WRITE_BYTE(0x0993, 0x00) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x00);
		#endif
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_Set_QAM_SR(void)
{
	#if !J83ABC_ENABLE
	MS_U8 data1, data2, data3;
	#endif

	PRINT("QAM type 0x%x SR %d\n", pres->sdmd_atsc_pridata.elasttype-1,
	pres->sdmd_atsc_pridata.symrate);

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU
	_I2C_READ_BYTE(0x20CA, &data1);
	#else
	_I2C_READ_BYTE(0x099F, &data1);
	#endif
	#endif

	#if !J83ABC_ENABLE
	// Check if FW is not UTOF
	if (data1 != 0x55) {
		data1 = (pres->sdmd_atsc_pridata.elasttype-1) | 0x80;
		data2 = pres->sdmd_atsc_pridata.symrate & 0xFF;
		data3 = (pres->sdmd_atsc_pridata.symrate>>8) & 0xFF;

		_I2C_WRITE_BYTE(0x0C1B, data1);
		_I2C_WRITE_BYTE(0x0C1C, data2);
		_I2C_WRITE_BYTE(0x0C1D, data3);

		return _I2C_WRITE_BYTE(0x0990, 0x04);
	} else
	#endif
	{
		#if IS_MULTI_MCU
		if (_I2C_WRITE_BYTE(0x20C2, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x20C0, 0x04);
		#else
		if (_I2C_WRITE_BYTE(0x0993, 0x07) == FALSE)
			return FALSE;
		else
			return _I2C_WRITE_BYTE(0x0990, 0x04);
		#endif
	}
}

static enum DMD_ATSC_DEMOD_TYPE _HAL_EXTERN_ATSC_CheckType(void)
{
	MS_U8 mode = 0;
	#if SUPPORT_ATSC3
	MS_U8 flg = 0;
	#endif

	#if J83ABC_ENABLE
	_I2C_READ_BYTE(0x2A02, &mode); //EQ mode check

	mode &= 0x07;

	if (mode == QAM16_J83ABC)
		return DMD_ATSC_DEMOD_ATSC_16QAM;
	else if (mode == QAM32_J83ABC)
		return DMD_ATSC_DEMOD_ATSC_32QAM;
	else if (mode == QAM64_J83ABC)
		return DMD_ATSC_DEMOD_ATSC_64QAM;
	else if (mode == QAM128_J83ABC)
		return DMD_ATSC_DEMOD_ATSC_128QAM;
	else if (mode == QAM256_J83ABC)
		return DMD_ATSC_DEMOD_ATSC_256QAM;
	else
		return DMD_ATSC_DEMOD_ATSC_256QAM;
	#else // #if J83ABC_ENABLE
	#if SUPPORT_ATSC3
	_I2C_READ_BYTE(0x20C2, &mode);
	_I2C_READ_BYTE(0x20C3, &flg);
	#endif

	#if SUPPORT_ATSC3
	if ((mode&0x0F) == 0x01 && !(flg&0x04))
		return DMD_ATSC_DEMOD_ATSC_OFDM;
	else
	#endif
	{
		#if SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x3800, &mode); //mode check
		#else
		_I2C_READ_BYTE(0x2900, &mode); //mode check
		#endif

		if ((mode&VSB_ATSC) == VSB_ATSC)
			return DMD_ATSC_DEMOD_ATSC_VSB;
		else if ((mode&QAM256_ATSC) == QAM256_ATSC)
			return DMD_ATSC_DEMOD_ATSC_256QAM;
		else
			return DMD_ATSC_DEMOD_ATSC_64QAM;
	}
	#endif // #if J83ABC_ENABLE
}

static MS_BOOL _HAL_EXTERN_ATSC_AGCLock(void)
{
	MS_U8 data1 = 0;

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &data1);
	#else
	_I2C_READ_BYTE(0x099F, &data1);
	#endif
	#endif

	#if !J83ABC_ENABLE
	// Check if FW is not UTOF
	if (data1 != 0x55) {
		_I2C_READ_BYTE(0x0C1A, &data1);

		if ((data1 == 0x03) || (data1 == 0x00))
			return TRUE;
		else
			return FALSE;
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C3, &data1);
		#else
		_I2C_READ_BYTE(0x0994, &data1);
		#endif

		if (data1 & 0x01)
			return FALSE;
		else
			return TRUE;
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_PreLock(void)
{
	#if !J83ABC_ENABLE
	MS_U8 data1 = 0;
	#endif
	#if SUPPORT_ATSC3
	enum DMD_ATSC_DEMOD_TYPE eMode;
	#endif

	#if SUPPORT_ATSC3
	eMode = _HAL_EXTERN_ATSC_CheckType();
	#endif

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		_I2C_READ_BYTE(0x1AC0, &data1);

		if ((data1&0x01) == 0x01)
			return TRUE;
		else
			return FALSE;
	} else
	#endif
	{
		#if !J83ABC_ENABLE
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20CA, &data1);
		#else
		_I2C_READ_BYTE(0x099F, &data1);
		#endif
		#endif

		#if !J83ABC_ENABLE
		// Check if FW is not UTOF
		if (data1 != 0x55) {
			_I2C_READ_BYTE(0x0993, &data1);

			if ((data1&0x02) == 0x02)
				return TRUE;
			else
				return FALSE;
		} else
		#endif
		{
			return TRUE;
		}
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_FSync_Lock(void)
{
	MS_U8 data1 = 0;
	#if SUPPORT_ATSC3
	enum DMD_ATSC_DEMOD_TYPE eMode;
	#endif

	#if SUPPORT_ATSC3
	eMode = _HAL_EXTERN_ATSC_CheckType();
	#endif

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		_I2C_READ_BYTE(0x172C, &data1);

		if ((data1&0x01) == 0x01)
			return TRUE;
		else
			return FALSE;
	} else
	#endif
	{
		#if !J83ABC_ENABLE
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20CA, &data1);
		#else
		_I2C_READ_BYTE(0x099F, &data1);
		#endif
		#endif

		#if !J83ABC_ENABLE
		// Check if FW is not UTOF
		if (data1 != 0x55) {
			#if SOC_LIKE_EXT_DMD
			_I2C_READ_BYTE(0x3928, &data1);
			#else
			_I2C_READ_BYTE(0x2828, &data1);
			#endif

			if ((data1&0x80) == 0x80)
				return TRUE;
			else
				return FALSE;
		} else
		#endif
		{
			#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
			_I2C_READ_BYTE(0x20C3, &data1);
			#else
			_I2C_READ_BYTE(0x0994, &data1);
			#endif

			if (data1 & 0x04)
				return TRUE;
			else
				return FALSE;
		}
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_CE_Lock(void)
{
	return TRUE;
}

#if SUPPORT_ATSC3
static MS_BOOL _OFDM_FEC_Lock(void)
{
	MS_U8 data1 = 0;

	_I2C_READ_BYTE(0x20C1, &data1);

	if ((pres->sdmd_atsc_initdata.u8PLP_id[0] == 0xFF &&
		data1 >= 0x50) || (data1 == 0xF0)) {
		HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC Lock"));
		return TRUE;
	} else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC unLock"));
		return FALSE;
	}
}
#endif

static MS_BOOL _VSB_FEC_Lock(void)
{
	MS_U8 data1 = 0;
	#if !J83ABC_ENABLE
	MS_U8 data2 = 0, data3 = 0, data4 = 0, data5 = 0, data6 = 0, data7 = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &data1);
	#else
	_I2C_READ_BYTE(0x099F, &data1);
	#endif
	#endif

	#if !J83ABC_ENABLE
	// Check if FW is not UTOF
	if (data1 != 0x55) {
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C1, &data1);
		#else
		_I2C_READ_BYTE(0x0992, &data1);
		#endif

		_I2C_READ_BYTE(0x1917, &data2);//AD_NOISE_PWR_TRAIN1
		_I2C_READ_BYTE(0x1917, &data2);//AD_NOISE_PWR_TRAIN1
		_I2C_READ_BYTE(0x0994, &data3);
		_I2C_READ_BYTE(0x2601, &data4);//FEC_EN_CTL
		_I2C_READ_BYTE(0x2767, &data5);//EQEXT_REG_BASE//addy
		_I2C_READ_BYTE(0x0D01, &data6);
		_I2C_READ_BYTE(0x0D40, &data7);

		if (data1 == 0x80 && (data2 <= EXTERN_ATSC_VSB_TRAIN_SNR_LIMIT ||
			data5 <= EXTERN_ATSC_VSB_TRAIN_SNR_LIMIT) &&
			(data3&0x02) == 0x02 && (data4&EXTERN_ATSC_FEC_ENABLE) ==
			EXTERN_ATSC_FEC_ENABLE && ((data6&0x10) == 0x10) &&
			((data7&0x01) == 0x01)) {
			HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC Lock"));
			return TRUE;
		} else {
			HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC unLock"));
			return FALSE;
		}
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C1, &data1);
		#else
		_I2C_READ_BYTE(0x0992, &data1);
		#endif

		if (data1 == 0xF0) {
			HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC Lock"));
			return TRUE;
		} else {
			HAL_EXTERN_ATSC_DBINFO(PRINT("\nFEC unLock"));
			return FALSE;
		}
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_FEC_Lock(void)
{
	#if SUPPORT_ATSC3
	enum DMD_ATSC_DEMOD_TYPE eMode;
	#endif

	#if SUPPORT_ATSC3
	eMode = _HAL_EXTERN_ATSC_CheckType();
	#endif

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		return _OFDM_FEC_Lock();
	} else
	#endif
	{
		return _VSB_FEC_Lock();
	}
}

static MS_BOOL _HAL_EXTERN_ATSC_EAS_Lock(void)
{
	MS_U8 data1 = 0, data2 = 0;

	_I2C_READ_BYTE(0x1A67, &data1);
	_I2C_READ_BYTE(0x1A68, &data2);

	if (data1 || data2)
		return TRUE;
	else
		return FALSE;

}

static MS_BOOL _HAL_EXTERN_ATSC_QAM_PreLock(void)
{
	MS_U8 data1 = 0;
	MS_BOOL result = FALSE;

	#if J83ABC_ENABLE
	_I2C_READ_BYTE((REG_TR_BASE + 0x0050), &data1);//TR_LOCK
	#else
	_I2C_READ_BYTE((REG_TR_BASE + 0x0015), &data1);//TR_LOCK
	#endif

	#if J83ABC_ENABLE
	if ((data1&0x01) == 0x01) {
	#else
	if ((data1&0x10) == 0x10) {
	#endif
		HAL_EXTERN_ATSC_DBINFO(PRINT("	QAM preLock OK\n"));
		result = TRUE;
	} else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("	QAM preLock NOT OK\n"));
		result = FALSE;
	}
	return result;
}

static MS_BOOL _HAL_EXTERN_ATSC_QAM_Main_Lock(void)
{
	MS_U8 data1 = 0;
	#if !J83ABC_ENABLE
	MS_U8 data2 = 0, data3 = 0, data4 = 0, data5 = 0, data6 = 0;
	#endif

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20CA, &data1);
	#else
	_I2C_READ_BYTE(0x099F, &data1);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (data1 != 0x55) {
		// Check if FW is not UTOF
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C1, &data1);
		#else
		_I2C_READ_BYTE(0x0992, &data1);
		#endif

		_I2C_READ_BYTE(0x2618, &data2); //boundary detected
		_I2C_READ_BYTE(0x1415, &data3); //TR_LOCK
		_I2C_READ_BYTE(0x2601, &data4); //FEC_EN_CTL
		_I2C_READ_BYTE(0x0D01, &data5); //RS_backend
		_I2C_READ_BYTE(0x0D40, &data6); //RS_backend

		if (data1 == 0x80 && (data2&0x01) == 0x01 &&
			data4 == EXTERN_ATSC_FEC_ENABLE && (data3&0x10) == 0x10 &&
			((data5&0x10) == 0x10) && ((data6&0x01) == 0x01))
			return TRUE;
		else
			return FALSE;
	} else
	#endif
	{
		#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C1, &data1);
		#else
		_I2C_READ_BYTE(0x0992, &data1);
		#endif

		if (data1 >= 0x52)
			return TRUE;
		else
			return FALSE;
	}

	#if IS_MULTI_MCU || SOC_LIKE_EXT_DMD
	_I2C_READ_BYTE(0x20C1, &data1);
	#else
	_I2C_READ_BYTE(0x0992, &data1);
	#endif
}

static MS_U8 _HAL_EXTERN_ATSC_ReadIFAGC(void)
{
	MS_U8 data = 0;

	#if J83ABC_ENABLE
	_I2C_WRITE_BYTE(0x2716, 0x03); //debug select
	_I2C_READ_BYTE(0x2719, (MS_U8 *)(&data));
	#elif SOC_LIKE_EXT_DMD
	_I2C_WRITE_BYTE(0x2822, 0x03); //debug select
	_I2C_READ_BYTE(0x2825, (MS_U8 *)(&data));
	#else
	_I2C_WRITE_BYTE(0x3E16, 0x03); //debug select
	_I2C_READ_BYTE(0x3E19, (MS_U8 *)(&data));
	#endif

	return data;
}

static void _HAL_EXTERN_ATSC_CheckSignalCondition(enum DMD_ATSC_SIGNAL_CONDITION *pstatus)
{
	#if J83ABC_ENABLE
	MS_U8 u8NoisePowerH = 0, u8NoisePowerL = 0;
	static MS_U8 u8NoisePowerL_Last = 0xff;
	#else
	MS_U8 u8NoisePowerH = 0;
	#endif
	static MS_U8 u8NoisePowerH_Last = 0xff;
	enum DMD_ATSC_DEMOD_TYPE eMode;

	eMode = _HAL_EXTERN_ATSC_CheckType();

	#if J83ABC_ENABLE
	_I2C_READ_BYTE((REG_EQ_BASE + 0x00BE), &u8NoisePowerL);
	_I2C_READ_BYTE((REG_EQ_BASE + 0x00BF), &u8NoisePowerH);
	#else
	_I2C_READ_BYTE((REG_EQ_BASE + 0x0015), &u8NoisePowerH);
	#endif

	if (eMode == DMD_ATSC_DEMOD_ATSC_VSB) {
		//VSB mode//SNR=10*log10((1344<<10)/noisepower)
		if (!_VSB_FEC_Lock())
			u8NoisePowerH = 0xFF;
		else if (abs(u8NoisePowerH_Last-u8NoisePowerH) > 5)
			u8NoisePowerH_Last = u8NoisePowerH;
		else
			u8NoisePowerH = u8NoisePowerH_Last;

		if (u8NoisePowerH > 0xBE) //SNR<14.5
			*pstatus = DMD_ATSC_SIGNAL_NO;
		else if (u8NoisePowerH > 0x4D) //SNR<18.4
			*pstatus = DMD_ATSC_SIGNAL_WEAK;
		else if (u8NoisePowerH > 0x23) //SNR<21.8
			*pstatus = DMD_ATSC_SIGNAL_MODERATE;
		else if (u8NoisePowerH > 0x0A) //SNR<26.9
			*pstatus = DMD_ATSC_SIGNAL_STRONG;
		else
			*pstatus = DMD_ATSC_SIGNAL_VERY_STRONG;
	} else {
		//QAM MODE
		#if J83ABC_ENABLE
		if (!_HAL_EXTERN_ATSC_QAM_Main_Lock() || u8NoisePowerH)
			u8NoisePowerL = 0xFF;
		else if (abs(u8NoisePowerL_Last-u8NoisePowerL) > 5)
			u8NoisePowerL_Last = u8NoisePowerL;
		else
			u8NoisePowerL = u8NoisePowerL_Last;

		//SNR=10*log10(65536/noisepower)
		if (eMode == DMD_ATSC_DEMOD_ATSC_256QAM) {
			if (u8NoisePowerL > 0x71) //SNR<27.6
				*pstatus = DMD_ATSC_SIGNAL_NO;
			else if (u8NoisePowerL > 0x31) //SNR<31.2
				*pstatus = DMD_ATSC_SIGNAL_WEAK;
			else if (u8NoisePowerL > 0x25) //SNR<32.4
				*pstatus = DMD_ATSC_SIGNAL_MODERATE;
			else if (u8NoisePowerL > 0x17) //SNR<34.4
				*pstatus = DMD_ATSC_SIGNAL_STRONG;
			else
				*pstatus = DMD_ATSC_SIGNAL_VERY_STRONG;
		} else {
			if (u8NoisePowerL > 0x1D) //SNR<21.5
				*pstatus = DMD_ATSC_SIGNAL_NO;
			else if (u8NoisePowerL > 0x14) //SNR<25.4
				*pstatus = DMD_ATSC_SIGNAL_WEAK;
			else if (u8NoisePowerL > 0x0F) //SNR<27.8
				*pstatus = DMD_ATSC_SIGNAL_MODERATE;
			else if (u8NoisePowerL > 0x0B) //SNR<31.4
				*pstatus = DMD_ATSC_SIGNAL_STRONG;
			else
				*pstatus = DMD_ATSC_SIGNAL_VERY_STRONG;
		}
		#else // #if J83ABC_ENABLE
		if (!_HAL_EXTERN_ATSC_QAM_Main_Lock())
			u8NoisePowerH = 0xFF;
		else if (abs(u8NoisePowerH_Last-u8NoisePowerH) > 5)
			u8NoisePowerH_Last = u8NoisePowerH;
		else
			u8NoisePowerH = u8NoisePowerH_Last;

		if (eMode == DMD_ATSC_DEMOD_ATSC_256QAM) {
			//256QAM//SNR=10*log10((2720<<10)/noisepower)
			if (u8NoisePowerH > 0x13) //SNR<27.5
				*pstatus = DMD_ATSC_SIGNAL_NO;
			else if (u8NoisePowerH > 0x08) //SNR<31.2
				*pstatus = DMD_ATSC_SIGNAL_WEAK;
			else if (u8NoisePowerH > 0x06) //SNR<32.4
				*pstatus = DMD_ATSC_SIGNAL_MODERATE;
			else if (u8NoisePowerH > 0x04) //SNR<34.2
				*pstatus = DMD_ATSC_SIGNAL_STRONG;
			else
				*pstatus = DMD_ATSC_SIGNAL_VERY_STRONG;
		} else {
			//64QAM//SNR=10*log10((2688<<10)/noisepower)
			if (u8NoisePowerH > 0x4C) //SNR<21.5
				*pstatus = DMD_ATSC_SIGNAL_NO;
			else if (u8NoisePowerH > 0x1F) //SNR<25.4
				*pstatus = DMD_ATSC_SIGNAL_WEAK;
			else if (u8NoisePowerH > 0x11) //SNR<27.8
				*pstatus = DMD_ATSC_SIGNAL_MODERATE;
			else if (u8NoisePowerH > 0x07) //SNR<31.4
				*pstatus = DMD_ATSC_SIGNAL_STRONG;
			else
				*pstatus = DMD_ATSC_SIGNAL_VERY_STRONG;
		}
		#endif // #if J83ABC_ENABLE
	}
}

static MS_U8 _HAL_EXTERN_ATSC_ReadSNRPercentage(void)
{
	MS_U8 u8NoisePowerH = 0, u8NoisePowerL = 0;
	MS_U16 u16NoisePower;
	#if SUPPORT_ATSC3
	MS_U32 u32SnrSumOut = 0;
	#endif
	enum DMD_ATSC_DEMOD_TYPE eMode;

	eMode = _HAL_EXTERN_ATSC_CheckType();

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		_I2C_WRITE_BYTE(0x1540, 0x00);
		_I2C_WRITE_BYTE(0x15FF, 0x01);
		_I2C_WRITE_BYTE(0x1540, 0x01);
		_I2C_WRITE_BYTE(0x15FF, 0x01);
		_I2C_READ_BYTE(0x1542, ((MS_U8 *)(&u32SnrSumOut)));
		_I2C_READ_BYTE(0x1543, ((MS_U8 *)(&u32SnrSumOut))+1);
		_I2C_READ_BYTE(0x1544, ((MS_U8 *)(&u32SnrSumOut))+2);
		_I2C_WRITE_BYTE(0x1540, 0x00);
		_I2C_WRITE_BYTE(0x15FF, 0x01);

		//ATSC3 mode//SNR=10*log10(SnrSumOut/128)

		if (!_OFDM_FEC_Lock())
			return 0;//SNR=0;
		else if (u32SnrSumOut >= 0x138800) //SNR=40.0dB
			return 100;//SNR=MAX_SNR;
		else if (u32SnrSumOut >= 0x103ED2) //SNR=39.2dB
			return 98;
		else if (u32SnrSumOut >= 0x0D8328) //SNR=38.4dB
			return 96;
		else if (u32SnrSumOut >= 0x0B3D33) //SNR=37.6dB
			return 94;
		else if (u32SnrSumOut >= 0x095927) //SNR=36.8dB
			return 92;
		else if (u32SnrSumOut >= 0x07C689) //SNR=36.0dB
			return 90;
		else if (u32SnrSumOut >= 0x0677A8) //SNR=35.2dB
			return 88;
		else if (u32SnrSumOut >= 0x05611D) //SNR=34.4dB
			return 86;
		else if (u32SnrSumOut >= 0x04796F) //SNR=33.6dB
			return 84;
		else if (u32SnrSumOut >= 0x03B8BB) //SNR=32.8dB
			return 82;
		else if (u32SnrSumOut >= 0x031872) //SNR=32.0dB
			return 80;
		else if (u32SnrSumOut >= 0x029321) //SNR=31.2dB
			return 78;
		else if (u32SnrSumOut >= 0x02243D) //SNR=30.4dB
			return 76;
		else if (u32SnrSumOut >= 0x01C801) //SNR=29.6dB
			return 74;
		else if (u32SnrSumOut >= 0x017B4A) //SNR=28.8dB
			return 72;
		else if (u32SnrSumOut >= 0x013B7B) //SNR=28.0dB
			return 70;
		else if (u32SnrSumOut >= 0x010667) //SNR=27.2dB
			return 68;
		else if (u32SnrSumOut >= 0x00DA42) //SNR=26.4dB
			return 66;
		else if (u32SnrSumOut >= 0x00B58A) //SNR=25.6dB
			return 64;
		else if (u32SnrSumOut >= 0x0096FF) //SNR=24.8dB
			return 62;
		else if (u32SnrSumOut >= 0x007D98) //SNR=24.0dB
			return 60;
		else if (u32SnrSumOut >= 0x006877) //SNR=23.2dB
			return 58;
		else if (u32SnrSumOut >= 0x0056E4) //SNR=22.4dB
			return 56;
		else if (u32SnrSumOut >= 0x004846) //SNR=21.6dB
			return 54;
		else if (u32SnrSumOut >= 0x003C1D) //SNR=20.8dB
			return 52;
		else if (u32SnrSumOut >= 0x003200) //SNR=20.0dB
			return 50;
		else if (u32SnrSumOut >= 0x002997) //SNR=19.2dB
			return 48;
		else if (u32SnrSumOut >= 0x002297) //SNR=18.4dB
			return 46;
		else if (u32SnrSumOut >= 0x001CC6) //SNR=17.6dB
			return 44;
		else if (u32SnrSumOut >= 0x0017EE) //SNR=16.8dB
			return 42;
		else if (u32SnrSumOut >= 0x0013E8) //SNR=16.0dB
			return 40;
		else if (u32SnrSumOut >= 0x00108E) //SNR=15.2dB
			return 38;
		else if (u32SnrSumOut >= 0x000DC5) //SNR=14.4dB
			return 36;
		else if (u32SnrSumOut >= 0x000B74) //SNR=13.6dB
			return 34;
		else if (u32SnrSumOut >= 0x000987) //SNR=12.8dB
			return 32;
		else if (u32SnrSumOut >= 0x0007ED) //SNR=12.0dB
			return 30;
		else if (u32SnrSumOut >= 0x000697) //SNR=11.2dB
			return 28;
		else if (u32SnrSumOut >= 0x00057B) //SNR=10.4dB
			return 26;
		else if (u32SnrSumOut >= 0x00048F) //SNR=9.60dB
			return 24;
		else if (u32SnrSumOut >= 0x0003CB) //SNR=8.80dB
			return 22;
		else if (u32SnrSumOut >= 0x000328) //SNR=8.00dB
			return 20;
		else if (u32SnrSumOut >= 0x0002A0) //SNR=7.20dB
			return 18;
		else if (u32SnrSumOut >= 0x00022F) //SNR=6.40dB
			return 16;
		else if (u32SnrSumOut >= 0x0001D1) //SNR=5.60dB
			return 14;
		else if (u32SnrSumOut >= 0x000183) //SNR=4.80dB
			return 12;
		else if (u32SnrSumOut >= 0x000142) //SNR=4.00dB
			return 10;
		else if (u32SnrSumOut >= 0x00010B) //SNR=3.20dB
			return 8;
		else if (u32SnrSumOut >= 0x0000DE) //SNR=2.40dB
			return 6;
		else if (u32SnrSumOut >= 0x0000B9) //SNR=1.60dB
			return 4;
		else if (u32SnrSumOut >= 0x00009A) //SNR=0.80dB
			return 2;
		else//SNR<0.80dB
			return 0;//
	} else
	#endif
	{
		#if J83ABC_ENABLE
		_I2C_READ_BYTE((REG_EQ_BASE + 0x00BE), &u8NoisePowerL);
		_I2C_READ_BYTE((REG_EQ_BASE + 0x00BF), &u8NoisePowerH);
		#else
		_I2C_READ_BYTE((REG_EQ_BASE + 0x0014), &u8NoisePowerL);
		_I2C_READ_BYTE((REG_EQ_BASE + 0x0015), &u8NoisePowerH);
		#endif

		u16NoisePower = (u8NoisePowerH<<8) | u8NoisePowerL;

		if (eMode == DMD_ATSC_DEMOD_ATSC_VSB) {
			//VSB mode//SNR=10*log10((1344<<10)/noisepower)
			if (!_VSB_FEC_Lock())
				return 0;//SNR=0;
			else if (u16NoisePower <= 0x008A)//SNR>=40dB
				return 100;//SNR=MAX_SNR;
			else if (u16NoisePower <= 0x0097)//SNR>=39.6dB
				return 99;//
			else if (u16NoisePower <= 0x00A5)//SNR>=39.2dB
				return 98;//
			else if (u16NoisePower <= 0x00B5)//SNR>=38.8dB
				return 97;//
			else if (u16NoisePower <= 0x00C7)//SNR>=38.4dB
				return 96;//
			else if (u16NoisePower <= 0x00DA)//SNR>=38.0dB
				return 95;//
			else if (u16NoisePower <= 0x00EF)//SNR>=37.6dB
				return 94;//
			else if (u16NoisePower <= 0x0106)//SNR>=37.2dB
				return 93;//
			else if (u16NoisePower <= 0x0120)//SNR>=36.8dB
				return 92;//
			else if (u16NoisePower <= 0x013B)//SNR>=36.4dB
				return 91;//
			else if (u16NoisePower <= 0x015A)//SNR>=36.0dB
				return 90;//
			else if (u16NoisePower <= 0x017B)//SNR>=35.6dB
				return 89;//
			else if (u16NoisePower <= 0x01A0)//SNR>=35.2dB
				return 88;//
			else if (u16NoisePower <= 0x01C8)//SNR>=34.8dB
				return 87;//
			else if (u16NoisePower <= 0x01F4)//SNR>=34.4dB
				return 86;//
			else if (u16NoisePower <= 0x0224)//SNR>=34.0dB
				return 85;//
			else if (u16NoisePower <= 0x0259)//SNR>=33.6dB
				return 84;//
			else if (u16NoisePower <= 0x0293)//SNR>=33.2dB
				return 83;//
			else if (u16NoisePower <= 0x02D2)//SNR>=32.8dB
				return 82;//
			else if (u16NoisePower <= 0x0318)//SNR>=32.4dB
				return 81;//
			else if (u16NoisePower <= 0x0364)//SNR>=32.0dB
				return 80;//
			else if (u16NoisePower <= 0x03B8)//SNR>=31.6dB
				return 79;//
			else if (u16NoisePower <= 0x0414)//SNR>=31.2dB
				return 78;//
			else if (u16NoisePower <= 0x0479)//SNR>=30.8dB
				return 77;//
			else if (u16NoisePower <= 0x04E7)//SNR>=30.4dB
				return 76;//
			else if (u16NoisePower <= 0x0560)//SNR>=30.0dB
				return 75;//
			else if (u16NoisePower <= 0x05E5)//SNR>=29.6dB
				return 74;//
			else if (u16NoisePower <= 0x0677)//SNR>=29.2dB
				return 73;//
			else if (u16NoisePower <= 0x0716)//SNR>=28.8dB
				return 72;//
			else if (u16NoisePower <= 0x07C5)//SNR>=28.4dB
				return 71;//
			else if (u16NoisePower <= 0x0885)//SNR>=28.0dB
				return 70;//
			else if (u16NoisePower <= 0x0958)//SNR>=27.6dB
				return 69;//
			else if (u16NoisePower <= 0x0A3E)//SNR>=27.2dB
				return 68;//
			else if (u16NoisePower <= 0x0B3B)//SNR>=26.8dB
				return 67;//
			else if (u16NoisePower <= 0x0C51)//SNR>=26.4dB
				return 66;//
			else if (u16NoisePower <= 0x0D81)//SNR>=26.0dB
				return 65;//
			else if (u16NoisePower <= 0x0ECF)//SNR>=25.6dB
				return 64;//
			else if (u16NoisePower <= 0x103C)//SNR>=25.2dB
				return 63;//
			else if (u16NoisePower <= 0x11CD)//SNR>=24.8dB
				return 62;//
			else if (u16NoisePower <= 0x1385)//SNR>=24.4dB
				return 61;//
			else if (u16NoisePower <= 0x1567)//SNR>=24.0dB
				return 60;//
			else if (u16NoisePower <= 0x1778)//SNR>=23.6dB
				return 59;//
			else if (u16NoisePower <= 0x19BB)//SNR>=23.2dB
				return 58;//
			else if (u16NoisePower <= 0x1C37)//SNR>=22.8dB
				return 57;//
			else if (u16NoisePower <= 0x1EF0)//SNR>=22.4dB
				return 56;//
			else if (u16NoisePower <= 0x21EC)//SNR>=22.0dB
				return 55;//
			else if (u16NoisePower <= 0x2531)//SNR>=21.6dB
				return 54;//
			else if (u16NoisePower <= 0x28C8)//SNR>=21.2dB
				return 53;//
			else if (u16NoisePower <= 0x2CB7)//SNR>=20.8dB
				return 52;//
			else if (u16NoisePower <= 0x3108)//SNR>=20.4dB
				return 51;//
			else if (u16NoisePower <= 0x35C3)//SNR>=20.0dB
				return 50;//
			else if (u16NoisePower <= 0x3AF2)//SNR>=19.6dB
				return 49;//
			else if (u16NoisePower <= 0x40A2)//SNR>=19.2dB
				return 48;//
			else if (u16NoisePower <= 0x46DF)//SNR>=18.8dB
				return 47;//
			else if (u16NoisePower <= 0x4DB5)//SNR>=18.4dB
				return 46;//
			else if (u16NoisePower <= 0x5534)//SNR>=18.0dB
				return 45;//
			else if (u16NoisePower <= 0x5D6D)//SNR>=17.6dB
				return 44;//
			else if (u16NoisePower <= 0x6670)//SNR>=17.2dB
				return 43;//
			else if (u16NoisePower <= 0x7052)//SNR>=16.8dB
				return 42;//
			else if (u16NoisePower <= 0x7B28)//SNR>=16.4dB
				return 41;//
			else if (u16NoisePower <= 0x870A)//SNR>=16.0dB
				return 40;//
			else if (u16NoisePower <= 0x9411)//SNR>=15.6dB
				return 39;//
			else if (u16NoisePower <= 0xA25A)//SNR>=15.2dB
				return 38;//
			else if (u16NoisePower <= 0xB204)//SNR>=14.8dB
				return 37;//
			else if (u16NoisePower <= 0xC331)//SNR>=14.4dB
				return 36;//
			else if (u16NoisePower <= 0xD606)//SNR>=14.0dB
				return 35;//
			else if (u16NoisePower <= 0xEAAC)//SNR>=13.6dB
				return 34;//
			else// if (u16NoisePower>=0xEAAC)//SNR<13.6dB
				return 33;//
		} else {
			//QAM MODE
			if (eMode == DMD_ATSC_DEMOD_ATSC_256QAM) {
				//256QAM//SNR=10*log10((2720<<10)/noisepower)
				if (!_HAL_EXTERN_ATSC_QAM_Main_Lock())
					return 0;//SNR=0;
				else if (u16NoisePower <= 0x0117)//SNR>=40dB
					return 100;//
				else if (u16NoisePower <= 0x0131)//SNR>=39.6dB
					return 99;//
				else if (u16NoisePower <= 0x014F)//SNR>=39.2dB
					return 98;//
				else if (u16NoisePower <= 0x016F)//SNR>=38.8dB
					return 97;//
				else if (u16NoisePower <= 0x0193)//SNR>=38.4dB
					return 96;//
				else if (u16NoisePower <= 0x01B9)//SNR>=38.0dB
					return 95;//
				else if (u16NoisePower <= 0x01E4)//SNR>=37.6dB
					return 94;//
				else if (u16NoisePower <= 0x0213)//SNR>=37.2dB
					return 93;//
				else if (u16NoisePower <= 0x0246)//SNR>=36.8dB
					return 92;//
				else if (u16NoisePower <= 0x027E)//SNR>=36.4dB
					return 91;//
				else if (u16NoisePower <= 0x02BC)//SNR>=36.0dB
					return 90;//
				else if (u16NoisePower <= 0x02FF)//SNR>=35.6dB
					return 89;//
				else if (u16NoisePower <= 0x0349)//SNR>=35.2dB
					return 88;//
				else if (u16NoisePower <= 0x039A)//SNR>=34.8dB
					return 87;//
				else if (u16NoisePower <= 0x03F3)//SNR>=34.4dB
					return 86;//
				else if (u16NoisePower <= 0x0455)//SNR>=34.0dB
					return 85;//
				else if (u16NoisePower <= 0x04C0)//SNR>=33.6dB
					return 84;//
				else if (u16NoisePower <= 0x0535)//SNR>=33.2dB
					return 83;//
				else if (u16NoisePower <= 0x05B6)//SNR>=32.8dB
					return 82;//
				else if (u16NoisePower <= 0x0643)//SNR>=32.4dB
					return 81;//
				else if (u16NoisePower <= 0x06DD)//SNR>=32.0dB
					return 80;//
				else if (u16NoisePower <= 0x0787)//SNR>=31.6dB
					return 79;//
				else if (u16NoisePower <= 0x0841)//SNR>=31.2dB
					return 78;//
				else if (u16NoisePower <= 0x090D)//SNR>=30.8dB
					return 77;//
				else if (u16NoisePower <= 0x09EC)//SNR>=30.4dB
					return 76;//
				else if (u16NoisePower <= 0x0AE1)//SNR>=30.0dB
					return 75;//
				else if (u16NoisePower <= 0x0BEE)//SNR>=29.6dB
					return 74;//
				else if (u16NoisePower <= 0x0D15)//SNR>=29.2dB
					return 73;//
				else if (u16NoisePower <= 0x0E58)//SNR>=28.8dB
					return 72;//
				else if (u16NoisePower <= 0x0FBA)//SNR>=28.4dB
					return 71;//
				else if (u16NoisePower <= 0x113E)//SNR>=28.0dB
					return 70;//
				else if (u16NoisePower <= 0x12E8)//SNR>=27.6dB
					return 69;//
				else if (u16NoisePower <= 0x14BB)//SNR>=27.2dB
					return 68;//
				else if (u16NoisePower <= 0x16BB)//SNR>=26.8dB
					return 67;//
				else if (u16NoisePower <= 0x18ED)//SNR>=26.4dB
					return 66;//
				else if (u16NoisePower <= 0x1B54)//SNR>=26.0dB
					return 65;//
				else if (u16NoisePower <= 0x1DF7)//SNR>=25.6dB
					return 64;//
				else if (u16NoisePower <= 0x20DB)//SNR>=25.2dB
					return 63;//
				else if (u16NoisePower <= 0x2407)//SNR>=24.8dB
					return 62;//
				else if (u16NoisePower <= 0x2781)//SNR>=24.4dB
					return 61;//
				else if (u16NoisePower <= 0x2B50)//SNR>=24.0dB
					return 60;//
				else if (u16NoisePower <= 0x2F7E)//SNR>=23.6dB
					return 59;//
				else if (u16NoisePower <= 0x3413)//SNR>=23.2dB
					return 58;//
				else if (u16NoisePower <= 0x3919)//SNR>=22.8dB
					return 57;//
				else if (u16NoisePower <= 0x3E9C)//SNR>=22.4dB
					return 56;//
				else if (u16NoisePower <= 0x44A6)//SNR>=22.0dB
					return 55;//
				else if (u16NoisePower <= 0x4B45)//SNR>=21.6dB
					return 54;//
				else if (u16NoisePower <= 0x5289)//SNR>=21.2dB
					return 53;//
				else if (u16NoisePower <= 0x5A7F)//SNR>=20.8dB
					return 52;//
				else if (u16NoisePower <= 0x633A)//SNR>=20.4dB
					return 51;//
				else if (u16NoisePower <= 0x6CCD)//SNR>=20.0dB
					return 50;//
				else if (u16NoisePower <= 0x774C)//SNR>=19.6dB
					return 49;//
				else if (u16NoisePower <= 0x82CE)//SNR>=19.2dB
					return 48;//
				else if (u16NoisePower <= 0x8F6D)//SNR>=18.8dB
					return 47;//
				else if (u16NoisePower <= 0x9D44)//SNR>=18.4dB
					return 46;//
				else if (u16NoisePower <= 0xAC70)//SNR>=18.0dB
					return 45;//
				else if (u16NoisePower <= 0xBD13)//SNR>=17.6dB
					return 44;//
				else if (u16NoisePower <= 0xCF50)//SNR>=17.2dB
					return 43;//
				else if (u16NoisePower <= 0xE351)//SNR>=16.8dB
					return 42;//
				else if (u16NoisePower <= 0xF93F)//SNR>=16.4dB
					return 41;//
				else// if (u16NoisePower>=0xF93F)//SNR<16.4dB
					return 40;//
			} else {
				//64QAM//SNR=10*log10((2688<<10)/noisepower)
				if (!_HAL_EXTERN_ATSC_QAM_Main_Lock())
					return 0;//SNR=0;
				else if (u16NoisePower <= 0x0113)//SNR>=40dB
					return 100;//
				else if (u16NoisePower <= 0x012E)//SNR>=39.6dB
					return 99;//
				else if (u16NoisePower <= 0x014B)//SNR>=39.2dB
					return 98;//
				else if (u16NoisePower <= 0x016B)//SNR>=38.8dB
					return 97;//
				else if (u16NoisePower <= 0x018E)//SNR>=38.4dB
					return 96;//
				else if (u16NoisePower <= 0x01B4)//SNR>=38.0dB
					return 95;//
				else if (u16NoisePower <= 0x01DE)//SNR>=37.6dB
					return 94;//
				else if (u16NoisePower <= 0x020C)//SNR>=37.2dB
					return 93;//
				else if (u16NoisePower <= 0x023F)//SNR>=36.8dB
					return 92;//
				else if (u16NoisePower <= 0x0277)//SNR>=36.4dB
					return 91;//
				else if (u16NoisePower <= 0x02B3)//SNR>=36.0dB
					return 90;//
				else if (u16NoisePower <= 0x02F6)//SNR>=35.6dB
					return 89;//
				else if (u16NoisePower <= 0x033F)//SNR>=35.2dB
					return 88;//
				else if (u16NoisePower <= 0x038F)//SNR>=34.8dB
					return 87;//
				else if (u16NoisePower <= 0x03E7)//SNR>=34.4dB
					return 86;//
				else if (u16NoisePower <= 0x0448)//SNR>=34.0dB
					return 85;//
				else if (u16NoisePower <= 0x04B2)//SNR>=33.6dB
					return 84;//
				else if (u16NoisePower <= 0x0525)//SNR>=33.2dB
					return 83;//
				else if (u16NoisePower <= 0x05A5)//SNR>=32.8dB
					return 82;//
				else if (u16NoisePower <= 0x0630)//SNR>=32.4dB
					return 81;//
				else if (u16NoisePower <= 0x06C9)//SNR>=32.0dB
					return 80;//
				else if (u16NoisePower <= 0x0770)//SNR>=31.6dB
					return 79;//
				else if (u16NoisePower <= 0x0828)//SNR>=31.2dB
					return 78;//
				else if (u16NoisePower <= 0x08F1)//SNR>=30.8dB
					return 77;//
				else if (u16NoisePower <= 0x09CE)//SNR>=30.4dB
					return 76;//
				else if (u16NoisePower <= 0x0AC1)//SNR>=30.0dB
					return 75;//
				else if (u16NoisePower <= 0x0BCA)//SNR>=29.6dB
					return 74;//
				else if (u16NoisePower <= 0x0CED)//SNR>=29.2dB
					return 73;//
				else if (u16NoisePower <= 0x0E2D)//SNR>=28.8dB
					return 72;//
				else if (u16NoisePower <= 0x0F8B)//SNR>=28.4dB
					return 71;//
				else if (u16NoisePower <= 0x110A)//SNR>=28.0dB
					return 70;//
				else if (u16NoisePower <= 0x12AF)//SNR>=27.6dB
					return 69;//
				else if (u16NoisePower <= 0x147D)//SNR>=27.2dB
					return 68;//
				else if (u16NoisePower <= 0x1677)//SNR>=26.8dB
					return 67;//
				else if (u16NoisePower <= 0x18A2)//SNR>=26.4dB
					return 66;//
				else if (u16NoisePower <= 0x1B02)//SNR>=26.0dB
					return 65;//
				else if (u16NoisePower <= 0x1D9D)//SNR>=25.6dB
					return 64;//
				else if (u16NoisePower <= 0x2078)//SNR>=25.2dB
					return 63;//
				else if (u16NoisePower <= 0x239A)//SNR>=24.8dB
					return 62;//
				else if (u16NoisePower <= 0x270A)//SNR>=24.4dB
					return 61;//
				else if (u16NoisePower <= 0x2ACE)//SNR>=24.0dB
					return 60;//
				else if (u16NoisePower <= 0x2EEF)//SNR>=23.6dB
					return 59;//
				else if (u16NoisePower <= 0x3376)//SNR>=23.2dB
					return 58;//
				else if (u16NoisePower <= 0x386D)//SNR>=22.8dB
					return 57;//
				else if (u16NoisePower <= 0x3DDF)//SNR>=22.4dB
					return 56;//
				else if (u16NoisePower <= 0x43D7)//SNR>=22.0dB
					return 55;//
				else if (u16NoisePower <= 0x4A63)//SNR>=21.6dB
					return 54;//
				else if (u16NoisePower <= 0x5190)//SNR>=21.2dB
					return 53;//
				else if (u16NoisePower <= 0x596E)//SNR>=20.8dB
					return 52;//
				else if (u16NoisePower <= 0x620F)//SNR>=20.4dB
					return 51;//
				else if (u16NoisePower <= 0x6B85)//SNR>=20.0dB
					return 50;//
				else if (u16NoisePower <= 0x75E5)//SNR>=19.6dB
					return 49;//
				else if (u16NoisePower <= 0x8144)//SNR>=19.2dB
					return 48;//
				else if (u16NoisePower <= 0x8DBD)//SNR>=18.8dB
					return 47;//
				else if (u16NoisePower <= 0x9B6A)//SNR>=18.4dB
					return 46;//
				else if (u16NoisePower <= 0xAA68)//SNR>=18.0dB
					return 45;//
				else if (u16NoisePower <= 0xBAD9)//SNR>=17.6dB
					return 44;//
				else if (u16NoisePower <= 0xCCE0)//SNR>=17.2dB
					return 43;//
				else if (u16NoisePower <= 0xE0A4)//SNR>=16.8dB
					return 42;//
				else if (u16NoisePower <= 0xF650)//SNR>=16.4dB
					return 41;//
				else// if (u16NoisePower>=0xF650)//SNR<16.4dB
					return 40;//
			}
		}
	}
}

static MS_U16 _HAL_EXTERN_ATSC_ReadPKTERR(void)
{
	MS_U16 data = 0;
	MS_U8 reg = 0, reg_frz = 0;
	#if SUPPORT_ATSC3
	MS_U16 sum = 0;
	#endif
	enum DMD_ATSC_DEMOD_TYPE eMode;

	eMode = _HAL_EXTERN_ATSC_CheckType();

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		if (_OFDM_FEC_Lock()) {
			_I2C_READ_BYTE(0x16E1, &reg);
			data = reg;
			_I2C_READ_BYTE(0x16E0, &reg);
			sum += (data << 8)|reg;

			if (pres->sdmd_atsc_initdata.u8PLP_num >= 2) {
				_I2C_READ_BYTE(0x16E3, &reg);
				data = reg;
				_I2C_READ_BYTE(0x16E2, &reg);
				sum += (data << 8)|reg;
			}

			if (pres->sdmd_atsc_initdata.u8PLP_num >= 3) {
				_I2C_READ_BYTE(0x16E5, &reg);
				data = reg;
				_I2C_READ_BYTE(0x16E4, &reg);
				sum += (data << 8)|reg;
			}

			if (pres->sdmd_atsc_initdata.u8PLP_num == 4) {
				_I2C_READ_BYTE(0x16E7, &reg);
				data = reg;
				_I2C_READ_BYTE(0x16E6, &reg);
				sum += (data << 8)|reg;
			}
		}

		data = sum;
	} else
	#endif
	{

		_I2C_READ_BYTE((REG_RS_BASE+0x0003), &reg_frz);
		_I2C_WRITE_BYTE((REG_RS_BASE+0x0003), reg_frz|0x03);

		if (eMode == DMD_ATSC_DEMOD_ATSC_VSB) {
			if (!_VSB_FEC_Lock())
				data = 0;
			else {
				_I2C_READ_BYTE((REG_RS_BASE+0x0067), &reg);
				data = reg;
				_I2C_READ_BYTE((REG_RS_BASE+0x0066), &reg);
				data = (data << 8) | reg;
			}
		} else {
			if (!_HAL_EXTERN_ATSC_QAM_Main_Lock())
				data = 0;
			else {
				#if J83ABC_ENABLE
				_I2C_READ_BYTE(0x2167, &reg);
				data = reg;
				_I2C_READ_BYTE(0x2166, &reg);
				data = (data << 8)|reg;
				#else
				_I2C_READ_BYTE((REG_RS_BASE+0x0067), &reg);
				data = reg;
				_I2C_READ_BYTE((REG_RS_BASE+0x0066), &reg);
				data = (data << 8) | reg;
				#endif
			}
		}

		#if J83ABC_ENABLE
		reg_frz = reg_frz&(~0x03);
		_I2C_WRITE_BYTE(0x2103, reg_frz);
		#else
		reg_frz = reg_frz&(~0x03);
		_I2C_WRITE_BYTE(0x0D03, reg_frz);
		#endif
	}

	return data;
}

#ifdef UTPA2
static MS_BOOL _HAL_EXTERN_ATSC_ReadBER(MS_U32 *pBitErr,
MS_U16 *pError_window, MS_U32 *pWin_unit)
#else
static MS_BOOL _HAL_EXTERN_ATSC_ReadBER(float *pBer)
#endif
{
	MS_BOOL status = TRUE;
	MS_U8 reg = 0, reg_frz = 0;
	#if SUPPORT_ATSC3
	MS_U8 i = 0, FEC_type = 0;
	#endif
	MS_U16 BitErrPeriod;
	MS_U32 BitErr;
	enum DMD_ATSC_DEMOD_TYPE eMode;

	eMode = _HAL_EXTERN_ATSC_CheckType();

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		if (_OFDM_FEC_Lock()) {
			//scan 4 PLPs
			for (i = 0; i < 4; i++) {
				status &= _I2C_READ_BYTE(0x1702+i, &reg);
				if (reg & 0x40) {
					status &= _I2C_WRITE_BYTE(0x17E2, 0x80+0x10*(i+1));
					status &= _I2C_READ_BYTE(0x17E5, &reg);
					FEC_type = (reg >> 4) & 0x0F;
					break;
				}
			}

			status &= _I2C_READ_BYTE(0x1C67, &reg);
			BitErr = reg;
			status &= _I2C_READ_BYTE(0x1C66, &reg);
			BitErr = (BitErr << 8)|reg;
			status &= _I2C_READ_BYTE(0x1C65, &reg);
			BitErr = (BitErr << 8)|reg;
			status &= _I2C_READ_BYTE(0x1C64, &reg);
			BitErr = (BitErr << 8)|reg;

			status &= _I2C_READ_BYTE(0x1C25, &reg);
			BitErrPeriod = reg;
			status &= _I2C_READ_BYTE(0x1C24, &reg);
			BitErrPeriod = (BitErrPeriod << 8)|reg;

			if (BitErrPeriod == 0)	//protect 0
				BitErrPeriod = 1;

			#ifdef UTPA2
			*pBitErr = BitErr;
			*pError_window = BitErrPeriod;

			if ((FEC_type&0x01) == 0x01)
				*pWin_unit = 64800;
			else
				*pWin_unit = 16200;
			#else
			if (FEC_type&0x01 == 0x01)
				*pBer = (float)BitErr / ((float)BitErrPeriod*64800);
			else
				*pBer = (float)BitErr / ((float)BitErrPeriod*16200);
			#endif
		} else {
			#ifdef UTPA2
			*pBitErr = 0;
			*pError_window = 0;
			*pWin_unit = 0;
			#else
			*pBer = 0;
			#endif
		}
	} else
	#endif
	{
		if (eMode == DMD_ATSC_DEMOD_ATSC_VSB) {
			if (!_VSB_FEC_Lock()) {
				#ifdef UTPA2
				*pBitErr = 0;
				*pError_window = 0;
				*pWin_unit = 0;
				#else
				*pBer = 0;
				#endif
			} else {
				_I2C_READ_BYTE((REG_RS_BASE+0x0003), &reg_frz);
				_I2C_WRITE_BYTE((REG_RS_BASE+0x0003), reg_frz|0x03);

				_I2C_READ_BYTE((REG_RS_BASE+0x0047), &reg);
				BitErrPeriod = reg;
				_I2C_READ_BYTE((REG_RS_BASE+0x0046), &reg);
				BitErrPeriod = (BitErrPeriod << 8)|reg;

				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006d), &reg);
				BitErr = reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006C), &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006b), &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006a), &reg);
				BitErr = (BitErr << 8)|reg;

				// bank 0D 0x03 [1:0] reg_bit_err_num_freeze
				reg_frz = reg_frz&(~0x03);
				_I2C_WRITE_BYTE(0x0D03, reg_frz);

				if (BitErrPeriod == 0)//protect 0
					BitErrPeriod = 1;

				#ifdef UTPA2
				*pBitErr = BitErr;
				*pError_window = BitErrPeriod;
				*pWin_unit = 8*187*128;
				#else
				if (BitErr <= 0)
					*pBer = 0.5f / ((float)BitErrPeriod*8*187*128);
				else
					*pBer = (float)BitErr / ((float)BitErrPeriod*8*187*128);
				#endif
			}
		} else {
			if (!_HAL_EXTERN_ATSC_QAM_Main_Lock()) {
				#ifdef UTPA2
				*pBitErr = 0;
				*pError_window = 0;
				*pWin_unit = 0;
				#else
				*pBer = 0;
				#endif
			} else {
				#if J83ABC_ENABLE
				_I2C_READ_BYTE(0x2103, &reg_frz);
				_I2C_WRITE_BYTE(0x2103, reg_frz|0x03);

				_I2C_READ_BYTE(0x2147, &reg);
				BitErrPeriod = reg;
				_I2C_READ_BYTE(0x2146, &reg);
				BitErrPeriod = (BitErrPeriod << 8)|reg;

				status &= _I2C_READ_BYTE(0x216d, &reg);
				BitErr = reg;
				status &= _I2C_READ_BYTE(0x216c, &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE(0x216b, &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE(0x216a, &reg);
				BitErr = (BitErr << 8)|reg;

				reg_frz = reg_frz&(~0x03);
				_I2C_WRITE_BYTE(0x2103, reg_frz);

				if (BitErrPeriod == 0)	//protect 0
					BitErrPeriod = 1;

				#ifdef UTPA2
				*pBitErr = BitErr;
				*pError_window = BitErrPeriod;
				*pWin_unit = 8*188*128;
				#else
				if (BitErr <= 0)
					*pBer = 0.5f / ((float)BitErrPeriod*8*188*128);
				else
					*pBer = (float)BitErr / ((float)BitErrPeriod*8*188*128);
				#endif
				#else // #if J83ABC_ENABLE
				_I2C_READ_BYTE((REG_RS_BASE+0x0003), &reg_frz);
				_I2C_WRITE_BYTE((REG_RS_BASE+0x0003), reg_frz|0x03);

				_I2C_READ_BYTE((REG_RS_BASE+0x0047), &reg);
				BitErrPeriod = reg;
				_I2C_READ_BYTE((REG_RS_BASE+0x0046), &reg);
				BitErrPeriod = (BitErrPeriod << 8)|reg;

				// bank 0D 0x6a [7:0] reg_bit_err_num_7_0
				//		 0x6b [15:8] reg_bit_err_num_15_8
				// bank 0D 0x6c [7:0] reg_bit_err_num_23_16
				//		 0x6d [15:8] reg_bit_err_num_31_24
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006d), &reg);
				BitErr = reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006c), &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006b), &reg);
				BitErr = (BitErr << 8)|reg;
				status &= _I2C_READ_BYTE((REG_RS_BASE+0x006a), &reg);
				BitErr = (BitErr << 8)|reg;

				// bank 0D 0x03 [1:0] reg_bit_err_num_freeze
				reg_frz = reg_frz&(~0x03);
				_I2C_WRITE_BYTE((REG_RS_BASE+0x0003), reg_frz);

				if (BitErrPeriod == 0)	//protect 0
					BitErrPeriod = 1;

				#ifdef UTPA2
				*pBitErr = BitErr;
				*pError_window = BitErrPeriod;
				*pWin_unit = 7*122*128;
				#else
				if (BitErr <= 0)
					*pBer = 0.5f / ((float)BitErrPeriod*7*122*128);
				else
					*pBer = (float)BitErr / ((float)BitErrPeriod*7*122*128);
				#endif
				#endif // #if J83ABC_ENABLE
			}
		}
	}

	return status;
}

#ifdef UTPA2
static MS_BOOL _HAL_EXTERN_ATSC_ReadFrequencyOffset(MS_U8 *pMode, MS_S16 *pFF, MS_S16 *pRate)
#else
static MS_S16 _HAL_EXTERN_ATSC_ReadFrequencyOffset(void)
#endif
{
	enum DMD_ATSC_DEMOD_TYPE eMode;
	MS_U8 u8PTK_LOOP_FF_R3 = 0, u8PTK_LOOP_FF_R2 = 0;
	MS_U8 u8PTK_RATE_2 = 0, u8PTK_RATE_1 = 0;
	MS_U8 u8AD_CRL_LOOP_VALUE0 = 0, u8AD_CRL_LOOP_VALUE1 = 0;
	MS_U8 u8MIX_RATE_0 = 0, u8MIX_RATE_1 = 0, u8MIX_RATE_2 = 0, u8MIX_RATE_3 = 0;
	MS_S16 PTK_LOOP_FF, PTK_RATE, PTK_RATE_BASE;
	MS_S16 AD_CRL_LOOP_VALUE;
	MS_S32 MIX_RATE, MIXER_RATE_BASE;
	#if SUPPORT_ATSC3
	MS_U8 reg = 0, u8BW = 0;
	MS_S32 BsTdpFcfo = 0, FDPFCFO = 0, FineIcfo = 0;
	MS_S32 IcfoNorm = 0, FdSyncCfo = 0, FdSyncICfo = 0;
	#endif
	MS_S16 FreqOffset = 0;//kHz

	eMode = _HAL_EXTERN_ATSC_CheckType();

	#ifdef UTPA2
	*pMode = eMode;
	#endif

	#if SUPPORT_ATSC3
	if (eMode == DMD_ATSC_DEMOD_ATSC_OFDM) {
		//BS TDP latch
		_I2C_READ_BYTE(0x1A04, &reg);
		_I2C_WRITE_BYTE(0x1A04, reg | 0x01);

		_I2C_READ_BYTE(0x1ACF, &u8BW);
		_I2C_READ_BYTE(0x1A2A, ((MS_U8 *)(&BsTdpFcfo)));
		_I2C_READ_BYTE(0x1A2B, ((MS_U8 *)(&BsTdpFcfo))+1);
		_I2C_READ_BYTE(0x1ADA, ((MS_U8 *)(&FDPFCFO)));
		_I2C_READ_BYTE(0x1ADB, ((MS_U8 *)(&FDPFCFO))+1);
		_I2C_READ_BYTE(0x1ADA, ((MS_U8 *)(&FineIcfo)));
		_I2C_READ_BYTE(0x1ADB, ((MS_U8 *)(&FineIcfo))+1);

		//BS TDP release
		_I2C_READ_BYTE(0x1A04, &reg);
		_I2C_WRITE_BYTE(0x1A04, reg & (~0x01));

		//FDP freeze and load
		_I2C_WRITE_BYTE(0x15FE, 0x01);
		_I2C_WRITE_BYTE(0x15FF, 0x01);

		_I2C_READ_BYTE(0x1ADA, ((MS_U8 *)(&IcfoNorm)));
		_I2C_READ_BYTE(0x15A7, ((MS_U8 *)(&FdSyncCfo)));
		_I2C_READ_BYTE(0x15A8, ((MS_U8 *)(&FdSyncCfo))+1);
		_I2C_READ_BYTE(0x1ADA, ((MS_U8 *)(&FdSyncICfo)));

		//FDP release and load
		_I2C_WRITE_BYTE(0x15FE, 0x00);
		_I2C_WRITE_BYTE(0x15FF, 0x01);

		u8BW  = (u8BW >> 4) & 0x03;
		BsTdpFcfo = (((MS_S32)((BsTdpFcfo&0x00003F80)<<18))>>25)*47;
		FDPFCFO   = (((MS_S32)((FDPFCFO&0x00000FC0)<<20))>>26)*47;
		FineIcfo  = (((MS_S32)((FineIcfo&0x000007FF)<<21))>>21)*3000;
		IcfoNorm  = ((MS_S32)((IcfoNorm&0x0000003F)<<26))>>26;
		FdSyncCfo = ((MS_S32)((FdSyncCfo&0x000007FF)<<21))>>21;
		FdSyncICfo = ((MS_S32)((FdSyncICfo&0x0000000F)<<28))>>28;

		if (u8BW == 0) {
			IcfoNorm *= 3375;
			FdSyncCfo *= 53;
			FdSyncICfo *= 6750;
		} else if (u8BW == 1) {
			IcfoNorm *= 3938;
			FdSyncCfo *= 62;
			FdSyncICfo *= 7875;
		} else {
			IcfoNorm *= 4500;
			FdSyncCfo *= 70;
			FdSyncICfo *= 9000;
		}

		_I2C_READ_BYTE(0x15D0, &reg);
		if (reg & 0x02)
			IcfoNorm *= -1;

		FreqOffset = (MS_S16)(((MS_S32)(FDPFCFO + FineIcfo +
			(((MS_S32)(IcfoNorm + FdSyncCfo + FdSyncICfo))>>5) + BsTdpFcfo))>>10);

		#ifdef UTPA2
		*pRate = FreqOffset;
		#endif
	} else
	#endif
	{
		if (eMode == DMD_ATSC_DEMOD_ATSC_VSB) {
			#if SOC_LIKE_EXT_DMD
			_I2C_READ_BYTE(0x2d8E, &u8PTK_LOOP_FF_R2);
			_I2C_READ_BYTE(0x2d8F, &u8PTK_LOOP_FF_R3);
			#else
			_I2C_WRITE_BYTE(0x1303, 0x80);
			_I2C_READ_BYTE(0x1342, &u8PTK_LOOP_FF_R2);
			_I2C_READ_BYTE(0x1343, &u8PTK_LOOP_FF_R3);
			_I2C_WRITE_BYTE(0x1303, 0x00);
			#endif

			PTK_LOOP_FF = (u8PTK_LOOP_FF_R3<<8) | u8PTK_LOOP_FF_R2;

			#ifdef UTPA2
			*pFF = PTK_LOOP_FF;
			#endif

			FreqOffset = PTK_LOOP_FF*FS_RATE/0x80000; // FF * (Fs/2^19) Fs = ASIC FS KHz

			PTK_LOOP_FF = (u8PTK_LOOP_FF_R3<<8) | u8PTK_LOOP_FF_R2;

			_I2C_READ_BYTE(0x3EA2, &u8PTK_RATE_2);
			_I2C_READ_BYTE(0x3EA1, &u8PTK_RATE_1);

			PTK_RATE = (u8PTK_RATE_2<<8) | u8PTK_RATE_1;
			// 10762.238/4/Fs*2^14 Fs = ASIC FS KHz
			PTK_RATE_BASE = 10762 * 0x4000 / (4 * FS_RATE);

			FreqOffset = (-1)*(FreqOffset + ((PTK_RATE-PTK_RATE_BASE) *
			FS_RATE/0x4000)); // mixer_rate*(Fs/2^14) Fs = ASIC FS KHz

			#ifdef UTPA2
			*pRate = FreqOffset;
			#endif
		} else {
			#if J83ABC_ENABLE

			_I2C_READ_BYTE((REG_EQ_BASE + 0x0040), &u8AD_CRL_LOOP_VALUE0);
			_I2C_READ_BYTE((REG_EQ_BASE + 0x0041), &u8AD_CRL_LOOP_VALUE1);

			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0054), &u8MIX_RATE_0);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0055), &u8MIX_RATE_1);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0056), &u8MIX_RATE_2);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0057), &u8MIX_RATE_2);

			AD_CRL_LOOP_VALUE = (u8AD_CRL_LOOP_VALUE1 << 8) | u8AD_CRL_LOOP_VALUE0;

			if (eMode == DMD_ATSC_DEMOD_ATSC_256QAM)
				FreqOffset = (AD_CRL_LOOP_VALUE*5360)/0x10000000;
			else if (eMode == DMD_ATSC_DEMOD_ATSC_64QAM)
				FreqOffset = (AD_CRL_LOOP_VALUE*5056)/0x200000;

			#else // #if J83ABC_ENABLE
			_I2C_READ_BYTE((REG_EQ_BASE + 0x0004), &u8AD_CRL_LOOP_VALUE0);
			_I2C_READ_BYTE((REG_EQ_BASE + 0x0005), &u8AD_CRL_LOOP_VALUE1);

			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0054), &u8MIX_RATE_0);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0055), &u8MIX_RATE_1);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0056), &u8MIX_RATE_2);
			_I2C_READ_BYTE((REG_FRONTEND_BASE + 0x0057), &u8MIX_RATE_3);

			AD_CRL_LOOP_VALUE = (u8AD_CRL_LOOP_VALUE1<<8) | u8AD_CRL_LOOP_VALUE0;

			MIX_RATE = (u8MIX_RATE_3 << 24) | (u8MIX_RATE_2 << 16) |
				(u8MIX_RATE_1 << 8) | u8MIX_RATE_0;

			if (eMode == DMD_ATSC_DEMOD_ATSC_256QAM)
				FreqOffset = (AD_CRL_LOOP_VALUE*5360)/0x80000; // 5.360537/2^19*1000
			else if (eMode == DMD_ATSC_DEMOD_ATSC_64QAM)
				FreqOffset = (AD_CRL_LOOP_VALUE*5056)/0x80000; // 5.056941/2^19*1000

			#endif // #if J83ABC_ENABLE
			MIX_RATE = (u8MIX_RATE_3 << 24) | (u8MIX_RATE_2 << 16) |
				(u8MIX_RATE_1 << 8) | u8MIX_RATE_0;

			MIXER_RATE_BASE = ((0x01<<MIX_RATE_BIT)/FS_RATE) *
				pres->sdmd_atsc_initdata.if_khz;

			if (pres->sdmd_atsc_initdata.biqswap)
				FreqOffset -= (MIX_RATE-MIXER_RATE_BASE) /
				((0x01<<MIX_RATE_BIT)/FS_RATE);
			else
				FreqOffset += (MIX_RATE-MIXER_RATE_BASE) /
				((0x01<<MIX_RATE_BIT)/FS_RATE);

			#ifdef UTPA2
			*pFF = AD_CRL_LOOP_VALUE;
			*pRate = FreqOffset;
			#endif
		}
	}

	#ifdef UTPA2
	return TRUE;
	#else
	return FreqOffset;
	#endif
}

#if IS_MULTI_DMD
static MS_BOOL _HAL_EXTERN_ATSC_SetSerialControl(void)
{
	if (pRes->sDMD_ATSC_InitData.u1TsConfigByte_SerialMode) {
		if (pRes->sDMD_ATSC_InitData.u1TsConfigByte_IsNormal) {
			/*if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 0)
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 1)
			 *else
			 */
			// Enable TS MUX
		} else if (pRes->sDMD_ATSC_InitData.u1TsConfigByte_Is3Wire) {
			/*if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 0)
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 1)
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 2)
			 *	// Enable TS MUX
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 3)
			 *	// Enable TS MUX
			 *else
			 */
		} else {
			/*if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 0)
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 1)
			 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 2)
			 *else
			 */
		}
	} else {
		/*if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 0)
		 *else if (pRes->sDMD_ATSC_InitData.u2TsConfigByte_TSMode == 1)
		 *else
		 */
		// Enable TS MUX
	}
}
#else // #if IS_MULTI_DMD
static MS_BOOL _HAL_EXTERN_ATSC_SetSerialControl(void)
{
	MS_BOOL bResult = TRUE;
	MS_U8   u8TsConfigData, u8TmpData, u8ReadData;
	MS_U16  u16TSConfigDataAddr;

	#if !J83ABC_ENABLE
	#if IS_MULTI_MCU
	_I2C_READ_BYTE(0x20CA, &u8ReadData);
	#else
	_I2C_READ_BYTE(0x099F, &u8ReadData);
	#endif
	#endif

	#if !J83ABC_ENABLE
	if (u8ReadData != 0x55) {
		return bResult;
	} else
	#endif
	{
		u8TsConfigData = (pres->sdmd_atsc_initdata.u5TsConfigByte_DivNum << 3) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_ClockInv << 2) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_DataSwap << 1) |
					(pres->sdmd_atsc_initdata.u1TsConfigByte_SerialMode);

		#if SOC_LIKE_EXT_DMD
		//Set Data swap
		u8TmpData = (u8TsConfigData & 0x02) >> 1;
		bResult &= _I2C_READ_BYTE(0x3440, &u8ReadData);
		if (u8TmpData == 0x01)
			u8ReadData = u8ReadData | 0x20;
		else
			u8ReadData = u8ReadData & ~0x20;
		bResult &= _I2C_WRITE_BYTE(0x3440, u8ReadData);

		//Set Serial mode
		u8TmpData = u8TsConfigData & 0x01;
		bResult &= _I2C_READ_BYTE(0x3440, &u8ReadData);
		if (u8TmpData == 0x01)
			u8ReadData = u8ReadData | 0x01;
		else
			u8ReadData = u8ReadData & ~0x01;
		bResult &= _I2C_WRITE_BYTE(0x3440, u8ReadData);

		//Set Div Num
		u8TmpData = (u8TsConfigData & 0xF8) >> 3;
		bResult &= _I2C_WRITE_BYTE(0x0925, u8TmpData);

		//Set Clk phase
		bResult &= _I2C_READ_BYTE(0x0924, &u8ReadData);
		u8TmpData = (u8TsConfigData & 0x04) >> 2;
		if (u8TmpData == 0x01)  //Invert
			u8ReadData = u8ReadData | 0x20;
		else
			u8ReadData = u8ReadData & ~0x20;
		bResult &= _I2C_WRITE_BYTE(0x0924, u8ReadData);
		#else // #if SOC_LIKE_EXT_DMD
		//Set Data swap
		u8TmpData = (u8TsConfigData & 0x02) >> 1;
		bResult &= _I2C_READ_BYTE(0x0D22, &u8ReadData);
		if (u8TmpData == 0x01)
			u8ReadData = u8ReadData | 0x20;
		else
			u8ReadData = u8ReadData & ~0x20;
		bResult &= _I2C_WRITE_BYTE(0x0D22, u8ReadData);

		//Set Serial mode
		u8TmpData = u8TsConfigData & 0x01;
		bResult &= _I2C_READ_BYTE(0x0D22, &u8ReadData);
		if (u8TmpData == 0x01)
			u8ReadData = u8ReadData | 0x02;
		else
			u8ReadData = u8ReadData & ~0x02;
		bResult &= _I2C_WRITE_BYTE(0x0D22, u8ReadData);

		//Set Div Num
		u8TmpData = (u8TsConfigData & 0xF8) >> 3;
		bResult &= _I2C_WRITE_BYTE(0x0900+0x19, u8TmpData);

		//Set Clk phase
		bResult &= _I2C_READ_BYTE(0x0900+0x18, &u8ReadData);
		u8TmpData = (u8TsConfigData & 0x04) >> 2;
		//Invert
		if (u8TmpData == 0x01)
			u8ReadData = u8ReadData | 0x20;
		else
			u8ReadData = u8ReadData & ~0x20;
		bResult &= _I2C_WRITE_BYTE(0x0900+0x18, u8ReadData);
		#endif // #if SOC_LIKE_EXT_DMD

		//Set TS config data to Xdata
		u16TSConfigDataAddr = 0;
		bResult &= _READ_MBX(0x0080, &u8ReadData);
		u16TSConfigDataAddr = u8ReadData << 8;
		bResult &= _READ_MBX(0x0081, &u8ReadData);
		u16TSConfigDataAddr |= u8ReadData;

		bResult &= _WRITE_MBX(u16TSConfigDataAddr, u8TsConfigData);

		return bResult;
	}
}
#endif // #if IS_MULTI_DMD

static MS_BOOL _HAL_EXTERN_ATSC_IIC_Bypass_Mode(MS_BOOL bEnable)
{
	MS_U8 u8Retry = 10;
	MS_U8 u8Data = 0;
	MS_BOOL bRet = FALSE;

	if (bEnable) {
		#if SOC_LIKE_EXT_DMD
		_I2C_READ_BYTE(0x20C2, &u8Data);
		#else
		_I2C_READ_BYTE(0x0993, &u8Data);
		#endif

		// For EAS mode
		if ((u8Data & 0x11) == 0x11) {
			#if SOC_LIKE_EXT_DMD
			if (_I2C_WRITE_BYTE(0x20C2, 0x00) == FALSE)
				return FALSE;
			if (_I2C_WRITE_BYTE(0x20C0, 0x00) == FALSE)
				return FALSE;
			#else
			if (_I2C_WRITE_BYTE(0x0993, 0x00) == FALSE)
				return FALSE;
			if (_I2C_WRITE_BYTE(0x0990, 0x00) == FALSE)
				return FALSE;
			#endif

			// Waiting to enter FW idle state
			while (u8Data != 0x02) {
				#if SOC_LIKE_EXT_DMD
				if (_I2C_READ_BYTE(0x20C1, &u8Data) == FALSE)
					return FALSE;
				#else
				if (_I2C_READ_BYTE(0x0992, &u8Data) == FALSE)
					return FALSE;
				#endif
			}
		}
	}

	while ((u8Retry--) && (bRet == FALSE)) {
		if (bEnable)
			bRet = _I2C_WRITE_BYTE(REG_IIC_BYPASS, 0x10);// IIC by-pass mode on
		else
			bRet = _I2C_WRITE_BYTE(REG_IIC_BYPASS, 0x00);// IIC by-pass mode off
	}

	return bRet;
}

static MS_BOOL _HAL_EXTERN_ATSC_SW_SSPI_GPIO(MS_BOOL bEnable)
{
	MS_BOOL bRet = TRUE;
	MS_U8 u8Data;

	//switch to GPIO
	if (bEnable == TRUE) {
		bRet = _I2C_READ_BYTE(REG_SSPI_EN, &u8Data);
		u8Data &= ~0x01;
		bRet = _I2C_WRITE_BYTE(REG_SSPI_EN, u8Data);   // [0] reg_en_sspi_pad = 0
	} else {
		bRet = _I2C_READ_BYTE(REG_SSPI_EN, &u8Data);
		u8Data |= 0x01;
		bRet = _I2C_WRITE_BYTE(REG_SSPI_EN, u8Data);   // [0] reg_en_sspi_pad = 1
	}

	return bRet;
}

static MS_BOOL _HAL_EXTERN_ATSC_GPIO_GET_LEVEL(MS_U8 u8Pin, MS_BOOL *bLevel)
{
	MS_BOOL bRet = TRUE;
	MS_U8 u8Data;

	bRet = _I2C_READ_BYTE(REG_GPIO_LEVEL, &u8Data);
	if (u8Pin == 0) {
		if ((u8Data & 0x01) == 0x01)
			*bLevel = TRUE;
		else
			*bLevel = FALSE;
	} else if (u8Pin == 1) {
		if ((u8Data & 0x02) == 0x02)
			*bLevel = TRUE;
		else
			*bLevel = FALSE;
	} else if (u8Pin == 2) {
		if ((u8Data & 0x04) == 0x04)
			*bLevel = TRUE;
		else
			*bLevel = FALSE;
	} else if (u8Pin == 5) {
		if ((u8Data & 0x20) == 0x20)
			*bLevel = TRUE;
		else
			*bLevel = FALSE;
	} else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("Can not use this pin as GPIO[%d]\n", u8Pin));
		bRet = FALSE;

		return bRet;
	}

	return bRet;
}

static MS_BOOL _HAL_EXTERN_ATSC_GPIO_SET_LEVEL(MS_U8 u8Pin, MS_BOOL bLevel)
{
	MS_BOOL bRet = TRUE;
	MS_U8 u8Data;

	bRet = _I2C_READ_BYTE(REG_GPIO_SET, &u8Data);
	if (u8Pin == 0) {
		if (bLevel == FALSE)
			u8Data &= ~0x01;
		else
			u8Data |= 0x01;
	} else if (u8Pin == 1) {
		if (bLevel == FALSE)
			u8Data &= ~0x02;
		else
			u8Data |= 0x02;
	} else if (u8Pin == 2) {
		if (bLevel == FALSE)
			u8Data &= ~0x04;
		else
			u8Data |= 0x04;
	} else if (u8Pin == 5) {
		if (bLevel == FALSE)
			u8Data &= ~0x20;
		else
			u8Data |= 0x20;
	} else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("Can not use this pin as GPIO[%d]\n", u8Pin));
		bRet = FALSE;

		return bRet;
	}

	bRet = _I2C_WRITE_BYTE(REG_GPIO_SET, u8Data);

	return bRet;
}

static MS_BOOL _HAL_EXTERN_ATSC_GPIO_OUT_ENABLE(MS_U8 u8Pin, MS_BOOL bEnableOut)
{
	MS_BOOL bRet = TRUE;
	MS_U8 u8Data;

	bRet = _I2C_READ_BYTE(REG_GPIO_OUT, &u8Data);
	if (u8Pin == 0) {
		if (bEnableOut == TRUE)
			u8Data &= ~0x01;
		else
			u8Data |= 0x01;
	} else if (u8Pin == 1) {
		if (bEnableOut == TRUE)
			u8Data &= ~0x02;
		else
			u8Data |= 0x02;
	} else if (u8Pin == 2) {
		if (bEnableOut == TRUE)
			u8Data &= ~0x04;
		else
			u8Data |= 0x04;
	} else if (u8Pin == 5) {
		if (bEnableOut == TRUE)
			u8Data &= ~0x20;
		else
			u8Data |= 0x20;
	} else {
		HAL_EXTERN_ATSC_DBINFO(PRINT("Can not use this pin as GPIO[%d]\n", u8Pin));

		bRet = FALSE;

		return bRet;
	}

	bRet = _I2C_WRITE_BYTE(0x09C8, u8Data);

	return bRet;
}

static MS_BOOL _HAL_EXTERN_ATSC_DoIQSwap(MS_BOOL bIsQPad)
{
	pres->sdmd_atsc_pridata.bis_qpad = bIsQPad;

	//Set I&Q pad
	if (pres->sdmd_atsc_pridata.bis_qpad) {
		_I2C_WRITE_BYTE(REG_PAD_MODE_SEL, 0x2E);
		_I2C_WRITE_BYTE(REG_ADC_OUT_SEL, 0x01);

		HAL_EXTERN_ATSC_DBINFO(PRINT("select Q pad source\n"));
	} else {
		_I2C_WRITE_BYTE(REG_PAD_MODE_SEL, 0x1E);
		_I2C_WRITE_BYTE(REG_ADC_OUT_SEL, 0x00);

		HAL_EXTERN_ATSC_DBINFO(PRINT("select I pad source\n"));
	}

	return TRUE;
}

static MS_BOOL _HAL_EXTERN_ATSC_GetReg(MS_U16 u16Addr, MS_U8 *pu8Data)
{
	return _I2C_READ_BYTE(u16Addr, pu8Data);
}

static MS_BOOL _HAL_EXTERN_ATSC_SetReg(MS_U16 u16Addr, MS_U8 u8Data)
{
	return _I2C_WRITE_BYTE(u16Addr, u8Data);
}

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
#ifndef MSOS_TYPE_LINUX_KERNEL
MS_BOOL EXT_IOCTL_CMD(DMD_ATSC_IOCTL_DATA *pData,
DMD_ATSC_HAL_COMMAND eCmd, void *pArgs)
#else
MS_BOOL HAL_EXTERN_ATSC_IOCTL_CMD(DMD_ATSC_IOCTL_DATA *pData,
DMD_ATSC_HAL_COMMAND eCmd, void *pArgs)
#endif
#else // #if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
MS_BOOL hal_extern_atsc_ioctl_cmd(struct ATSC_IOCTL_DATA *pData,
enum DMD_ATSC_HAL_COMMAND eCmd, void *pArgs)
#endif // #if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
{
	MS_BOOL bResult = TRUE;

	id   = pData->id;
	pres = pData->pres;
	// pr_err("[%s][%d] eCmd = %d , pres = %p\n", __func__, __LINE__, eCmd, pres);
	pExtChipChNum = pData->pextchipchnum;
	pExtChipChId  = pData->pextchipchid;
	pExtChipI2cCh = pData->pextchipi2cch;

	sys  = pData->sys;

	#ifdef CONFIG_UTOPIA_PROC_DBG_SUPPORT
	pATSC_DBGLV = &(pData->DbgLv);
	#endif

	#if IS_MULTI_DMD
	_SEL_DMD();
	#endif

	switch (eCmd) {
	case DMD_ATSC_HAL_CMD_EXIT:
		bResult = _HAL_EXTERN_ATSC_Exit();
		break;
	case DMD_ATSC_HAL_CMD_INITCLK:
		#if defined MTK_PROJECT
		pData->sys.MSPI_Init_Ext	= _MSPI_Init_Ext;
		pData->sys.MSPI_SlaveEnable = _MSPI_SlaveEnable;
		pData->sys.MSPI_Write	   = _MSPI_Write;
		pData->sys.MSPI_Read		= _MSPI_Read;
		#endif
		_HAL_EXTERN_ATSC_InitClk();
		break;
	case DMD_ATSC_HAL_CMD_DOWNLOAD:
		bResult = _HAL_EXTERN_ATSC_Download();
		break;
	case DMD_ATSC_HAL_CMD_FWVERSION:
		_HAL_EXTERN_ATSC_FWVERSION();
		break;
	case DMD_ATSC_HAL_CMD_SOFTRESET:
		bResult = _HAL_EXTERN_ATSC_SoftReset();
		break;
	case DMD_ATSC_HAL_CMD_SETVSBMODE:
		bResult = _HAL_EXTERN_ATSC_SetVsbMode();
		break;
	#if SUPPORT_ATSC3
	case DMD_ATSC_HAL_CMD_SetOFDMMode:
		bResult = _HAL_EXTERN_ATSC_SetOFDMMode();
		break;
	case DMD_ATSC_HAL_CMD_SetEASMode:
		bResult = _HAL_EXTERN_ATSC_SetEASMode();
		break;
	#endif
	case DMD_ATSC_HAL_CMD_SET64QAMMODE:
		bResult = _HAL_EXTERN_ATSC_Set64QamMode();
		break;
	case DMD_ATSC_HAL_CMD_SET256QAMMODE:
		bResult = _HAL_EXTERN_ATSC_Set256QamMode();
		break;
	case DMD_ATSC_HAL_CMD_SETMODECLEAN:
		bResult = _HAL_EXTERN_ATSC_SetModeClean();
		break;
	case DMD_ATSC_HAL_CMD_SET_QAM_SR:
		bResult = _HAL_EXTERN_ATSC_Set_QAM_SR();
		break;
	case DMD_ATSC_HAL_CMD_ACTIVE:
		break;
	case DMD_ATSC_HAL_CMD_CheckType:
		*((enum DMD_ATSC_DEMOD_TYPE *)pArgs) = _HAL_EXTERN_ATSC_CheckType();
		break;
	case DMD_ATSC_HAL_CMD_AGCLOCK:
		bResult = _HAL_EXTERN_ATSC_AGCLock();
		break;
	case DMD_ATSC_HAL_CMD_PreLock:
		bResult = _HAL_EXTERN_ATSC_PreLock();
		break;
	case DMD_ATSC_HAL_CMD_FSync_Lock:
		bResult = _HAL_EXTERN_ATSC_FSync_Lock();
		break;
	case DMD_ATSC_HAL_CMD_CE_Lock:
		bResult = _HAL_EXTERN_ATSC_CE_Lock();
		break;
	case DMD_ATSC_HAL_CMD_FEC_Lock:
		bResult = _HAL_EXTERN_ATSC_FEC_Lock();
		break;
	case DMD_ATSC_HAL_CMD_EAS_Lock:
		bResult = _HAL_EXTERN_ATSC_EAS_Lock();
		break;
	case DMD_ATSC_HAL_CMD_QAM_PRELOCK:
		bResult = _HAL_EXTERN_ATSC_QAM_PreLock();
		break;
	case DMD_ATSC_HAL_CMD_QAM_MAIN_LOCK:
		bResult = _HAL_EXTERN_ATSC_QAM_Main_Lock();
		break;
	case DMD_ATSC_HAL_CMD_READIFAGC:
		*((MS_U16 *)pArgs) = _HAL_EXTERN_ATSC_ReadIFAGC();
		break;
	case DMD_ATSC_HAL_CMD_CHECKSIGNALCONDITION:
		_HAL_EXTERN_ATSC_CheckSignalCondition((enum DMD_ATSC_SIGNAL_CONDITION *)pArgs);
		break;
	case DMD_ATSC_HAL_CMD_READSNRPERCENTAGE:
		*((MS_U8 *)pArgs) = _HAL_EXTERN_ATSC_ReadSNRPercentage();
		break;
	case DMD_ATSC_HAL_CMD_READPKTERR:
		*((MS_U16 *)pArgs) = _HAL_EXTERN_ATSC_ReadPKTERR();
		break;
	case DMD_ATSC_HAL_CMD_GETPREVITERBIBER:
		break;
	/*	case DMD_ATSC_HAL_CMD_GetPostViterbiBer:
	 *	#ifdef UTPA2
	 *	bResult = _HAL_EXTERN_ATSC_ReadBER(&((
	 *	*((DMD_ATSC_BER_DATA *)pArgs)).BitErr),
	 *	&((*((DMD_ATSC_BER_DATA *)pArgs)).Error_window),
	 *	&((*((DMD_ATSC_BER_DATA *)pArgs)).Win_unit));
	 *	#else
	 *	bResult = _HAL_EXTERN_ATSC_ReadBER((float *)pArgs);
	 *	#endif
	 *	break;
	 *case DMD_ATSC_HAL_CMD_ReadFrequencyOffset:
	 *	#ifdef UTPA2
	 *	bResult = _HAL_EXTERN_ATSC_ReadFrequencyOffset(
	 *	&((*((DMD_ATSC_CFO_DATA *)pArgs)).Mode),
	 *	&((*((DMD_ATSC_CFO_DATA *)pArgs)).FF),
	 *	&((*((DMD_ATSC_CFO_DATA *)pArgs)).Rate));
	 *	#else
	 *	*((MS_S16 *)pArgs) = _HAL_EXTERN_ATSC_ReadFrequencyOffset();
	 *	#endif
	 *	break;
	 */
	case DMD_ATSC_HAL_CMD_TS_INTERFACE_CONFIG:
		bResult = _HAL_EXTERN_ATSC_SetSerialControl();
		break;
	case DMD_ATSC_HAL_CMD_IIC_BYPASS_MODE:
		bResult = _HAL_EXTERN_ATSC_IIC_Bypass_Mode(*((MS_BOOL *)pArgs));
		break;
	case DMD_ATSC_HAL_CMD_SSPI_TO_GPIO:
		bResult = _HAL_EXTERN_ATSC_SW_SSPI_GPIO(*((MS_BOOL *)pArgs));
		break;
	case DMD_ATSC_HAL_CMD_GPIO_GET_LEVEL:
		bResult = _HAL_EXTERN_ATSC_GPIO_GET_LEVEL((
		*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).pin_8,
			&((*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).blevel));
		break;
	case DMD_ATSC_HAL_CMD_GPIO_SET_LEVEL:
		bResult = _HAL_EXTERN_ATSC_GPIO_SET_LEVEL((
		*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).pin_8,
			(*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).blevel);
		break;
	case DMD_ATSC_HAL_CMD_GPIO_OUT_ENABLE:
		bResult = _HAL_EXTERN_ATSC_GPIO_OUT_ENABLE((
			*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).pin_8,
			(*((struct DMD_ATSC_GPIO_PIN_DATA *)pArgs)).bisout);
		break;
	case DMD_ATSC_HAL_CMD_DOIQSWAP:
		bResult = _HAL_EXTERN_ATSC_DoIQSwap(*((MS_BOOL *)pArgs));
		break;
	case DMD_ATSC_HAL_CMD_GET_REG:
		bResult = _HAL_EXTERN_ATSC_GetReg((*((struct DMD_ATSC_REG_DATA *)pArgs)).addr_16,
			&((*((struct DMD_ATSC_REG_DATA *)pArgs)).data_8));
		break;
	case DMD_ATSC_HAL_CMD_SET_REG:
		bResult = _HAL_EXTERN_ATSC_SetReg((*((struct DMD_ATSC_REG_DATA *)pArgs)).addr_16,
			(*((struct DMD_ATSC_REG_DATA *)pArgs)).data_8);
		break;
	#if DIGRF_INSIDE
	case DMD_ATSC_HAL_CMD_SET_DIGRF_FREQ:
		bResult = _HAL_EXTERN_ATSC_SET_DIGRF_FREQ(*((MS_U32 *)pArgs));
		break;
	#endif
	default:
		break;
	}

	return bResult;
}
#if	defined(MSOS_TYPE_LINUX_KERNEL) && defined(CONFIG_CC_IS_CLANG) && defined(CONFIG_MSTAR_CHIP)

EXPORT_SYMBOL(hal_extern_atsc_ioctl_cmd);

/*static int mstar_atsc_ext_dmd_init(void)
 *{
 *	PRINT(KERN_ALERT "MSTAR ATSC EXT DMD KERNEL MODULE INIT\n");
 *	return 0;
 *}
 *
 *static void mstar_atsc_ext_dmd_exit(void)
 *{
 *	PRINT(KERN_ALERT "MSTAR ATSC EXT DMD KERNEL MODULE EXIT\n");
 *}
 *
 *module_init(mstar_atsc_ext_dmd_init);
 *module_exit(mstar_atsc_ext_dmd_exit);
 */
#else
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
#else
EXPORT_SYMBOL(hal_extern_atsc_ioctl_cmd);
#endif
#endif

#ifndef MSOS_TYPE_LINUX_KERNEL
#if DMD_ATSC_UTOPIA_EN || DMD_ATSC_UTOPIA2_EN
MS_BOOL HAL_EXTERN_ATSC_IOCTL_CMD(DMD_ATSC_IOCTL_DATA *pData,
DMD_ATSC_HAL_COMMAND eCmd, void *pArgs)
{
	return EXT_IOCTL_CMD(pData, eCmd, pArgs);
}
#endif
#endif // #ifndef MSOS_TYPE_LINUX_KERNEL

static MS_U8 _DEFAULT_CMD(void)
{
	return TRUE;
}

MS_BOOL MDrv_DMD_ATSC_Initial_Hal_Interface(void)  __attribute__((weak, alias("_DEFAULT_CMD")));
