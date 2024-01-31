/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_capture_platform.h  --  Mediatek PCM device header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _CAPTURE_PLATFORM_HEADER
#define _CAPTURE_PLATFORM_HEADER

#include <linux/interrupt.h>

#include "mtk_alsa_pcm_common.h"
#include "mtk_alsa_pcm_atv.h"

#define SOC_INTEGER_CAP(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = cap_control_info, \
	.get = cap_control_get_value, .put = cap_control_put_value, \
	.private_value = (unsigned short)xvalue }

#define SOC_TIMESTAMP_CAP(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = cap_timestamp_info, \
	.get = cap_timestamp_get_value, .put = cap_timestamp_put_value, \
	.private_value = (unsigned short)xvalue }

#define SOC_INTEGER_ATV(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = atv_control_info, \
	.get = atv_control_get_value, .put = atv_control_put_value, \
	.private_value = (unsigned short)xvalue }

enum {
	HDMI_RX1_DEV,
	HDMI_RX2_DEV,
	ATV_DEV,
	ADC_DEV,
	ADC1_DEV,
	I2S_RX2_DEV,
	LOOPBACK_DEV,
	HFP_RX_DEV,
	CAP_DEV_NUM,
};

enum {
	RX1_MUTE_EVENT,
	RX1_NONPCM_EVENT,
	RX1_NONPCM_FLAG,
	RX1_FASTLOCK,
	RX1_CS,
	RX2_MUTE_EVENT,
	RX2_NONPCM_EVENT,
	RX2_NONPCM_FLAG,
	RX2_FASTLOCK,
	RX2_CS,
};

struct capture_register {
	u32 PAS_CTRL_0;
	u32 PAS_CTRL_9;
	u32 PAS_CTRL_39;
	u32 PAS_CTRL_48;
	u32 PAS_CTRL_49;
	u32 PAS_CTRL_50;
	u32 PAS_CTRL_64;
	u32 PAS_CTRL_65;
	u32 PAS_CTRL_66;
	u32 PAS_CTRL_73;
	u32 PAS_CTRL_74;
	u32 HDMI_EVENT_0;
	u32 HDMI_EVENT_1;
	u32 HDMI_EVENT_2;
	u32 HDMI2_EVENT_0;
	u32 HDMI2_EVENT_1;
	u32 HDMI2_EVENT_2;
	u32 HDMI2_EVENT_3;
	u32 HDMI2_EVENT_4;
	u32 SE_IDMA_CTRL_0;
	u32 SE_BDMA_CFG;
	u32 FD230_SEL;
	u32 SE_DSP_BRG_DATA_L;
	u32 SE_DSP_BRG_DATA_H;
	u32 SE_IDMA_WRBASE_ADDR_L;
	u32 SE_IDMA_RDBASE_ADDR_L;
	u32 SE_IDMA_RDDATA_H_0;
	u32 SE_IDMA_RDDATA_H_1;
	u32 SE_IDMA_RDDATA_L;
	u32 SE_AUD_CTRL;
	u32 SE_MBASE_H;
	u32 SE_MSIZE_H;
	u32 SE_MCFG;
	u32 SE_MBASE_EXT;
	u32 SE_INPUT_CFG;
	u32 STATUS_HDMI_CS;
	u32 STATUS_HDMI_PC;
	u32 STATUS_HDMI2_PC;
	u32 HDMI_TBUS_3;
	u32 HDMI_CFG_6;
	u32 HDMI2_CFG_29;
	u32 HDMI_MODE_CFG;
	u32 HDMI2_MODE_CFG1;
	u32 HDMI2_MODE_CFG2;
	u32 DECODER1_CFG;
	u32 DECODER2_CFG;
	u32 CLK_CFG6;
	u32 SIF_AGC_CFG6;
	u32 ASIF_AMUX;
	u32 I2S_INPUT_CFG;
	u32 I2S_OUT2_CFG;
	u32 CAP_HDMI_CTRL;
	u32 CAP_HDMI_INFO;
	u32 CAP_DMA2_CTRL;
	u32 CAP_DMA2_RPTR;
	u32 CAP_DMA2_WPTR;
	u32 CAP_DMA4_CTRL;
	u32 CAP_DMA4_RPTR;
	u32 CAP_DMA4_WPTR;
	u32 CAP_DMA7_CTRL;
	u32 CAP_DMA7_RPTR;
	u32 CAP_DMA7_WPTR;
	u32 CAP_DMA8_CTRL;
	u32 CAP_DMA8_RPTR;
	u32 CAP_DMA8_WPTR;
	u32 CAP_DMA9_CTRL;
	u32 CAP_DMA9_RPTR;
	u32 CAP_DMA9_WPTR;
	u32 CAP_DMA10_CTRL;
	u32 CAP_DMA10_RPTR;
	u32 CAP_DMA10_WPTR;
	u32 CHANNEL_M7_CFG;
	u32 CHANNEL_M8_CFG;
	u32 CHANNEL_M15_CFG;
	u32 CHANNEL_M16_CFG;
	u32 ATV_PALY_CMD;
	u32 ATV_STD_CMD;
	u32 ATV_COMM_CFG;
	u32 ATV_SND_MODE_1;
	u32 ATV_SND_MODE_2;
	u32 ATV_DETECTION_RESULT;
	u32 ATV_SND_CARRIER_STATUS_1;
	u32 ATV_SND_CARRIER_STATUS_2;
	u32 DBG_CMD_1;
	u32 DBG_CMD_2;
	u32 DBG_RESULT_1;
	u32 DBG_RESULT_2;
	u32 SE_AID_CTRL;
	u32 ANALOG_MUX;
	u32 IRQ0_IDX;
	u32 IRQ1_IDX;
	u32 HDMI_CS0;
	u32 HDMI_CS1;
	u32 HDMI_CS2;
	u32 HDMI_CS3;
	u32 HDMI_CS4;
	u32 HDMI_CS5;
	u32 HDMI_CS6;
	u32 HDMI_CS7;
	u32 HDMI_CS8;
	u32 HDMI_CS9;
	u32 HDMI_CS10;
	u32 HDMI_CS11;
	u32 HDMI2_CS0;
	u32 HDMI2_CS1;
	u32 HDMI2_CS2;
	u32 HDMI2_CS3;
	u32 HDMI2_CS4;
	u32 HDMI2_CS5;
	u32 HDMI2_CS6;
	u32 HDMI2_CS7;
	u32 HDMI2_CS8;
	u32 HDMI2_CS9;
	u32 HDMI2_CS10;
	u32 HDMI2_CS11;
	u32 HWTSG_CFG1;
	u32 HWTSG_CFG2;
	u32 HWTSG_CFG4;
	u32 HWTSG_TS_L;
	u32 HWTSG_TS_H;
	u32 HWTSG2_CFG1;
	u32 HWTSG2_CFG2;
	u32 HWTSG2_CFG4;
	u32 HWTSG2_TS_L;
	u32 HWTSG2_TS_H;
};

struct capture_priv {
	struct device *dev;
	struct mem_info mem_info;
	struct mtk_runtime_struct *pcm_device[CAP_DEV_NUM];
	struct tasklet_struct irq0_tasklet;
	int irq0_counter;
	int irq0;
	int irq1;
	int irq2;
	int irq3;
	u32 cap_dma_offset[CAP_DEV_NUM];
	u32 cap_dma_size[CAP_DEV_NUM];
	int hdmi_rx1_nonpcm_flag;
	int hdmi_rx2_nonpcm_flag;
	struct atv_thr_tbl atv_thr;
	struct atv_gain_tbl atv_gain;
	bool atv_gain_init_flag;
};

int capture_dev_probe(struct platform_device *pdev);
int capture_dev_remove(struct platform_device *pdev);

#endif /* _CAPTURE_PLATFORM_HEADER */
