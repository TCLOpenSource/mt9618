/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * mtk_alsa_dplayback_platform.h  --  Mediatek DTV dplayback header
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _DPLAYBACK_DMA_PLATFORM_HEADER
#define _DPLAYBACK_DMA_PLATFORM_HEADER

#include "mtk_alsa_pcm_common.h"

extern unsigned char sinetone[SINETONE_SIZE];

#define SOC_CHAR_EARC(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = earc_control_info, \
	.get = earc_get_param, .put = earc_put_param, \
	.private_value = (unsigned short)xvalue }

#define SOC_EARC_SLT(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = earc_control_info, \
	.get = earc_get_slt_param, .put = earc_put_slt_param, \
	.private_value = (unsigned short)xvalue }

#define SOC_CHAR_CS(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = dplayback_cs_control_info, \
	.get = dplayback_get_cs, .put = dplayback_put_cs, \
	.private_value = (unsigned short)xvalue }

#define SOC_CHAR_EARC_HDMI_BYPASS(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = dplayback_hdmi_rx_bypass_info, \
	.get = dplayback_get_hdmi_rx_bypass, .put = dplayback_put_hdmi_rx_bypass, \
	.private_value = (unsigned short)xvalue }

enum {
	SPDIF = 0,
	ARC,
	EARC,
	DPLAYBACK_HW_NUM,
};

enum {
	SPDIF_PCM_PLAYBACK = 0,
	SPDIF_NONPCM_PLAYBACK,
	ARC_PCM_PLAYBACK,
	ARC_NONPCM_PLAYBACK,
	EARC_PCM_PLAYBACK,
	EARC_NONPCM_PLAYBACK,
	DPLAYBACK_DEV_NUM,
};

struct dplayback_register {
	u32 DPB_DMA5_CTRL;
	u32 DPB_DMA5_WPTR;
	u32 DPB_DMA5_DDR_LEVEL;
	u32 DPB_DMA6_CTRL;
	u32 DPB_DMA6_WPTR;
	u32 DPB_DMA6_DDR_LEVEL;
	u32 DPB_DMA7_CTRL;
	u32 DPB_DMA7_WPTR;
	u32 DPB_DMA7_DDR_LEVEL;
	u32 DPB_EARC_SINE_GEN;
	u32 DPB_CS0;
	u32 DPB_CS1;
	u32 DPB_CS2;
	u32 DPB_CS3;
	u32 DPB_CS4;
	u32 DPB_SPDIF_CFG;
	u32 DPB_ARC_CFG;
	u32 DPB_CS_TOGGLE;
	u32 DPB_SPDIF_SYNTH_H;
	u32 DPB_SPDIF_SYNTH_L;
	u32 DPB_ARC_SYNTH_H;
	u32 DPB_ARC_SYNTH_L;
	u32 DPB_SYNTH_CFG;
	u32 DPB_EARC_CFG00;
	u32 DPB_EARC_CFG01;
	u32 DPB_EARC_CFG02;
	u32 DPB_EARC_CFG03;
	u32 DPB_EARC_CFG04;
	u32 DPB_EARC_CFG05;
	u32 DPB_EARC_CFG06;
	u32 DPB_EARC_CFG07;
	u32 DPB_EARC_CFG08;
	u32 DPB_EARC_CFG09;
	u32 DPB_EARC_CFG10;
	u32 DPB_EARC_CFG11;
	u32 DPB_EARC_CFG12;
	u32 DPB_EARC_CFG13;
	u32 DPB_EARC_CFG14;
	u32 DPB_EARC_CFG15;
	u32 DPB_EARC_CFG16;
	u32 DPB_EARC_CFG17;
	u32 DPB_EARC_CFG18;
	u32 DPB_EARC_CFG19;
	u32 DPB_EARC_CFG20;
	u32 DPB_EARC_CFG21;
	u32 DPB_EARC_CFG22;
	u32 DPB_EARC_CFG23;
	u32 DPB_EARC_CFG24;
	u32 DPB_EARC_CFG25;
	u32 DPB_EARC_CFG26;
	u32 DPB_EARC_CFG27;
	u32 DPB_EARC_CFG28;
	u32 DPB_EARC_CFG29;
	u32 DPB_EARC_CFG30;
	u32 DPB_EARC_CFG31;
	u32 DPB_EARC_CFG32;
	u32 DPB_EARC_CFG33;
	u32 DPB_EARC_CFG34;
	u32 DPB_EARC_CFG35;
	u32 DPB_EARC_CFG36;
	u32 DPB_EARC_CFG37;
	u32 DPB_EARC_CFG38;
	u32 DPB_EARC_CFG39;
	u32 DPB_EARC_CFG40;
	u32 DPB_EARC_CFG41;
	u32 DPB_EARC_CFG42;
	u32 DPB_EARC_CFG43;
	u32 DPB_EARC_CFG44;
	u32 DPB_EARC_CFG45;
	u32 DPB_EARC_CFG46;
	u32 DPB_EARC_CFG47;
	u32 DPB_EARC_CFG48;
	u32 DPB_EARC_CFG49;
	u32 DPB_EARC_CFG50;
	u32 DPB_EARC_CFG51;
	u32 DPB_EARC_CFG52;
	u32 DPB_EARC_TX00;
	u32 DPB_EARC_TX18;
	u32 DPB_EARC_TX20;
	u32 DPB_EARC_TX24;
	u32 DPB_EARC_TX28;
	u32 DPB_EARC_TX2C;
	u32 DPB_EARC_TX30;
};

/* define a clk status structure */
struct mtk_dp_clk_struct {
	unsigned int spdif_clk;
	unsigned int arc_clk;
	unsigned int earc_clk;
};

struct dplayback_priv {
	struct device *dev;
	struct mem_info mem_info;
	struct mtk_runtime_struct *pcm_device[DPLAYBACK_DEV_NUM];
	struct mtk_dp_clk_struct dp_clk_status;
	u32 play_dma_offset[DPLAYBACK_HW_NUM];
	u32 play_dma_size[DPLAYBACK_HW_NUM];
	int ip_version;
	bool C_bit_en;
	bool L_bit_en;
	int category;
	bool hdmi_rx_bypass_flag;
	int ch_alloc;
	int slt_result;
	unsigned int cs_log_level;
};

int dplayback_dev_probe(struct platform_device *pdev);
int dplayback_dev_remove(struct platform_device *pdev);

#endif /* _DPLAYBACK_DMA_PLATFORM_HEADER */
