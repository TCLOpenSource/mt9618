/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef _MTK_DRM_LDM_LEDDEVICE_H_
#define _MTK_DRM_LDM_LEDDEVICE_H_

//-----------------------------------------------------------------------------
//  Driver Capability
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//  Macro and Define
//-----------------------------------------------------------------------------

enum EN_LD_PWM_CH {
	E_LDPWM0,
	E_LDPWM1,
	E_LDPWM2,
	E_LDPWM_MAX = E_LDPWM2,
};

enum EN_LD_PWM_PERIOD {

	E_PWM_PERIOD_0 = 0,
	E_PWM_PERIOD_1 = 1,
	E_PWM_PERIOD_MAX = E_PWM_PERIOD_1,
};

enum EN_LD_PWM_SHIFT {

	E_PWM_SHIFT_0 = 0,
	E_PWM_SHIFT_1 = 1,
	E_PWM_SHIFT_MAX = E_PWM_SHIFT_1,
};

enum EN_LD_PWM_DUTY {

	E_PWM_DUTY_0 = 0,
	E_PWM_DUTY_1 = 1,
	E_PWM_DUTY_MAX = E_PWM_DUTY_1,
};

enum EN_LD_VSYNC_LIKE {

	E_VSYNC_LIKE_ST_0 = 0,
	E_VSYNC_LIKE_ST_1 = 1,
	E_VSYNC_LIKE_END_0  = 2,
	E_VSYNC_LIKE_END_1,
	E_VSYNC_LIKE_MAX = E_VSYNC_LIKE_END_1,
};

enum EN_LD_VSYNC_LIKE_SEL {

	E_VSYNC_LIKE_HW_DMA = 0,
	E_VSYNC_LIKE_PWM0 = 1,
	E_VSYNC_LIKE_LD_SPI_CS  = 2,
	E_VSYNC_LIKE_SEL_MAX = E_VSYNC_LIKE_LD_SPI_CS,
};

enum drm_en_mspi_data {
	E_MSPI_DATA0 = 0,
	E_MSPI_DATA1 = 1,
	E_MSPI_DATA2,
	E_MSPI_DATA3,
	E_MSPI_DATA4,
	E_MSPI_DATA5,
	E_MSPI_DATA6,
	E_MSPI_DATA7,
	E_MSPI_DATA8,
	E_MSPI_DATA9,
	E_MSPI_DATA10,
	E_MSPI_DATA11,
	E_MSPI_DATA12,
	E_MSPI_DATA13,
	E_MSPI_DATA14,
	E_MSPI_DATA15,
	E_MSPI_DATA_NUM,
	E_MSPI_DATA_MAX = E_MSPI_DATA_NUM,
};
#define RETRY_COUNTER		(200UL)
#define PWM_EN_CLK_ALL	0x07

#define FULL_DUTY_12BIT		0xFFF
#define SPI_READ_BUF_NUM	(0x02)
#if !defined LINE_BUFFERSIZE
#define LINE_BUFFERSIZE  4096
#endif
#define HIGH_BYTE_16BIT_BASE	0x100

#define CHECKSUM_ONEBYTE  0x01
#define CHECKSUM_TWOBYTE  0x02

#define ONE	0x01
#define TWO	0x02
#define THREE	0x03
#define FOUR	0x04


/* AMS 382X Define START*/
#define LDM_AS382X_CUR_ON_1_ADDR         0x01
#define LDM_AS382X_DELAY1_ADDR           0x16    // Defines the delay time of the PWM, 0x16 ~ 0x35,
#define LDM_AS382X_PWM_HTIME1_ADDR       0x37    // Defines PWM high time, 0x37 ~ 0x56,
#define LDM_AS382X_BDAC1_ADDR        0x70
#define LDM_AS382X_VDAC_LEDH_ADDR        0x0C
#define LDM_AS382X_FB_ON1_ADDR        0x0E


#define LDM_AS382X_STATUS_ADDR        0x60

#define LDM_AS382X_PLLmulti1_ADDR        0x61
#define LDM_AS382X_HDR_ADDR        0x67
#define LDM_AS382X_BDAC_ON        0x10

#define LDM_AS382X_UNLOCK_ADDR           0x36   // Defines unlock address,
#define LDM_AS382X_UNLOCK_CMD       0xCA
#define LDM_AS382X_LOCK_CMD         0xAC

#define LDM_AS382X_HEADER_NUM	0x03

#define LDM_AS382X_DELAY_NUM	0x20
#define LDM_AS382X_PWM_HTIME_NUM	0x20
#define LDM_AS382X_BDAC_NUM	0x10

#define LDM_AS382X_SINGLE_BYTE_SAME_DATA   0x40
#define LDM_AS382X_BCAST_SINGLE_BYTE_SAME_DATA   0xC0
#define LDM_AS382X_BCAST_MULTI_BYTE_SAME_DATA   0x80
#define LDM_AS382X_BCAST_MULTI_BYTE_DIFF_DATA   0xBF

#define LDM_AS382X_SINGLE_READ_CMD_NUM   0x03
#define LDM_AS382X_BCAST_SINGLE_WRITE_CMD_NUM   0x03
#define LDM_AS382X_WRITE_SAME_DATA_REG_0x01_0x15_CMD_NUM   0x18
#define LDM_AS382X_WRITE_SAME_DATA_PLL_MULTI_CMD_NUM   0x05
#define LDM_AS382X_WRITE_SAME_DATA_PWM_CMD_NUM   0x23
#define LDM_AS382X_WRITE_SAME_DATA_BDAC_CMD_NUM	0x13

#define LDM_AS3824_BDAC_Vref_800 80000
#define LDM_AS3824_BDAC_Vref_500 50000

#define LDM_AS3824_TOTAL_CH 0x10

/* AMS 382X Define END*/
#define NT50516_POWER_ON	0xFF

/* IW7039 Define START*/
#define IW7039_SIMPLE_CHKSUM   0
#define IW7039_DEVICE_NUM   16
#define MAX_LDM_SW_SPI_CMD_SIZE 514
//.Set number of device
#define COMMAND_IW7039_SET_NUM_OF_DEVICE 0xFE00
//.Read single device single register
#define COMMAND_IW7039_READ_SINGLE_DEVICE 0x8001
#define ADDRESS_IW7039_READ_SINGLE_DEVICE 0x8000
#define DATA_DUMMY_IW7039_READ_SINGLE_DEVICE 0x0000
//.Write all device same one address
#define COMMAND_IW7039_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE 0x8001
#define ADDRESS_IW7039_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE 0x0000
//.Write all device diff 32 word data
#define COMMAND_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE 0xFF20
#define ADDRESS_IW7039_WRITE_DIFFERNET_DATA_OF_ALL_DEVICE 0x0000
//.Write all device same 32 word data
#define COMMAND_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE 0x8020
#define ADDRESS_IW7039_WRITE_SAME_32_WORD_OF_ALL_DEVICE 0x0000
//.Write one device 32 word data
#define COMMAND_IW7039_WRITE_32_WORD_OF_ONE_DEVICE 0x8020
#define ADDRESS_IW7039_WRITE_32_WORD_OF_ONE_DEVICE 0x0000
//.Write one device one address
#define COMMAND_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE 0x8001
#define ADDRESS_IW7039_WRITE_ONE_WORD_OF_ONE_DEVICE 0x0000
#define IW7039_ISET_ADDR             0x020    // Defines ISET 0x20 ~ 0x3F
#define IW7039_DT_ADDR             0x040
#define IW7039_PWM_HTIME1_ADDR       0x060    // Defines PWM high time, 0x60~ 0x56,

#define IW7039_0x344_ADDR		0x344

#define IW7039_PWM_HTIME_NUM		0x20
#define IW7039_PWM_ISET_NUM		0x20
#define IW7039_PWM_DT_NUM		0x20

#define IW7039_DAISY_CHAIN_CHIP_NUM_MAX		0x7F
#define IW7039_SINGLE_DATA_READ_DEVICE_NUM_MAX		0x7D

#define IW7039_INIT_REG0_VALUE		0x802
#define IW7039_FINISH_INIT_REG0_WO_CHK		0x803
#define IW7039_FINISH_INIT_REG0_W_CHK		0x813

#define IW7039_DUMMY_CAL_NUM	(16)//one device one bit
#define IW7039_POWER_ON	0x08

#define IW7039_READ_BUF_NUM	(20)
#define IW7039_CHK_SUM_NUM	(2)

#define IW7039_32WORD		(32)
/* IW7039 Define END*/

/* MBI6353 Define START*/

#define MBI6353_HEADER_NUM		0x03
#define MBI6353_CHECKSUM_NUM		0x01

#define MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_H		0x80
#define MBI6353_WRITE_SAME_ONE_WORD_OF_ALL_DEVICE_L		0x00
#define MBI6353_MASK_ADDR_H		0x04
#define MBI6353_MASK_ADDR_L		0x01
#define MBI6353_CFG_ADDR_H		0x00
#define MBI6353_CFG_ADDR_L		0x00
#define MBI6353_SCAN1_DUTY_H		0x00
#define MBI6353_SCAN1_DUTY_L		0x20


/* MBI6353 Define END*/

/*MCU Define START*/
#define MCU_READ_BUF_NUM	(20)
#define MCU_READ_BUF_NUM1	(10)

#define MCU_1BYTE	(9)
#define MCU_2BYTE	(16)

/*MCU Define END*/

#define BYTE1	1
#define BYTE2	2

#define DUTYDRAMFORMAT_INTERVAL		(3)
#define DUTYDRAMFORMAT_INTERVAL_YOFF_END_OFFSET		(1)
#define DUTYDRAMFORMAT_INTERVAL_NUM_OFFSET		(2)
#define CURRENTDRAMFORMAT_INTERVAL		(3)
#define CURRENTDRAMFORMAT_INTERVAL_YOFF_END_OFFSET		(1)
#define CURRENTDRAMFORMAT_INTERVAL_NUM_OFFSET		(2)

#define HISTOGRAM_NUM  0x20

#define MSPI_BIT_MASK_NUM 8
#define Reg_0x01_to_0x0B_NUM       0x0B
#define Reg_0x0E_to_0x15_NUM       0x08
#define Reg_0x1D_to_0x1F_NUM       0x03
#define Reg_0x01_to_0x15_NUM       0x15
#define Reg_0x00_to_0x1F_NUM       0x20
#define VDAC_BYTE_NUM        2
#define PLL_MULTI_BYTE_NUM       2

#define MBI6353_MASK_NUM		0x03
#define MBI6353_CFG_NUM		0x10
#define MBI6353_DUTY_NUM		0xC0

#define DMA_MIU_PACK_LENGTH  32
#define PACKLENGTH (32UL)
#define MSPI_OLD			0x00
#define MSPI_NEW			0x01
#define ALG_TIME_TEST			0
#define FIRST_MSPI_TRIG		0xA0
#define BDMA_MSPI_HSIZE	0X01
#define BDMA_MSPI_BIT_MODE	0X0A
#define BDMA_MSPI_TR_DELAY_H_1	0x30
#define BDMA_MSPI_TR_DELAY_V_1	0x01
#define SLEEP_TIME_1			1
#define SLEEP_TIME_10			10
#define SLEEP_TIME_30			30
#define SLEEP_TIME_100			100
#define SLEEP_TIME_200			200

#define TWICE	0x02
#define TRIPLE	0x03
#define QUADRUPLE	0x04

#define MASK_BYTES_LEFT		(0xF0)
#define MASK_BYTES_RIGHT	(0x0F)
#define MASK_BYTES		(0xFF)
#define SHIFT_BYTES		(4UL)
#define MASK_2BYTES		(0xFF)
#define SHIFT_2BYTES	(8UL)
#define MASK_4BYTES		(0xFFFF)
#define SHIFT_4BYTES	(16UL)
#define THOUSAND  1000
#define MILLION 1000000
#define TXSIZE	0x08
#define RXSIZE	0x08
#define PINGPONG_IDX0	0x00
#define PINGPONG_IDX1	0x01
#define SHFT8 0x08
#define SHFT12 0x0C

/// @name BIT#
/// definition of one bit mask
/// @{
#if !defined(BIT0) && !defined(BIT1)
#define BIT0	0x00000001
#define BIT1	0x00000002
#define BIT2	0x00000004
#define BIT3	0x00000008
#define BIT4	0x00000010
#define BIT5	0x00000020
#define BIT6	0x00000040
#define BIT7	0x00000080
#define BIT8	0x00000100
#define BIT9	0x00000200
#define BIT10	0x00000400
#define BIT11	0x00000800
#define BIT12	0x00001000
#define BIT13	0x00002000
#define BIT14	0x00004000
#define BIT15	0x00008000
#define BIT16	0x00010000
#define BIT17	0x00020000
#define BIT18	0x00040000
#define BIT19	0x00080000
#define BIT20	0x00100000
#define BIT21	0x00200000
#define BIT22	0x00400000
#define BIT23	0x00800000
#define BIT24	0x01000000
#define BIT25	0x02000000
#define BIT26	0x04000000
#define BIT27	0x08000000
#define BIT28	0x10000000
#define BIT29	0x20000000
#define BIT30	0x40000000
#define BIT31	0x80000000
#endif


int32_t ldm_set_ld_pwm_vsync_setting(struct mtk_tv_kms_context *pctx);

int32_t ldm_as3824_init(struct mtk_tv_kms_context *pctx,
unsigned char channel, u8 u8SPI_Devicenum);
int32_t ldm_iw7039_init_turn_on_backlight(struct mtk_tv_kms_context *pctx,
	unsigned char channel, u8 u8SPI_Devicenum);
int32_t ldm_iw7039_init(struct mtk_tv_kms_context *pctx,
	unsigned char channel, u8 u8SPI_Devicenum);
int32_t ldm_mbi6353_init(struct mtk_tv_kms_context *pctx,
	unsigned char channel, u8 u8SPI_Devicenum);
int32_t ldm_vsync_bug_patch(struct mtk_tv_kms_context *pctx);

int32_t ldm_leddevice_init(struct mtk_tv_kms_context *pctx);

#endif // _PQU_LDM_LEDDEVICE_H_
