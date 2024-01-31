/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_playback_platform.h  --  Mediatek DTV playback header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _PLAYBACK_DMA_PLATFORM_HEADER
#define _PLAYBACK_DMA_PLATFORM_HEADER

#include "mtk_alsa_pcm_common.h"

extern unsigned char sinetone[SINETONE_SIZE];

enum {
	SPEAKER_PLAYBACK = 0,
	HEADPHONE_PLAYBACK,
	LINEOUT_PLAYBACK,
	HFP_PLAYBACK,
	TDM_PLAYBACK,
	PLAYBACK_DEV_NUM,
	I2S0_PLAYBACK,
	I2S5_PLAYBACK,
	DAC0_PLAYBACK,
	DAC1_PLAYBACK,
	I2S_TX2_PLAYBACK,
	PLAYBACK_DEV_END,
};

enum {
	AUDIO_OUT_NULL = 0,
	AUDIO_OUT_DAC0,
	AUDIO_OUT_DAC1,
	AUDIO_OUT_DAC2,
	AUDIO_OUT_DAC3,
	AUDIO_OUT_I2S0,
	AUDIO_OUT_I2S1,
	AUDIO_OUT_I2S5,
	AUDIO_OUT_I2S_TX2,
};

struct playback_register {
	u32 PB_DMA1_CTRL;
	u32 PB_DMA1_WPTR;
	u32 PB_DMA1_DDR_LEVEL;
	u32 PB_DMA2_CTRL;
	u32 PB_DMA2_WPTR;
	u32 PB_DMA2_DDR_LEVEL;
	u32 PB_DMA3_CTRL;
	u32 PB_DMA3_WPTR;
	u32 PB_DMA3_DDR_LEVEL;
	u32 PB_DMA4_CTRL;
	u32 PB_DMA4_WPTR;
	u32 PB_DMA4_DDR_LEVEL;
	u32 PB_CHG_FINISHED;
	u32 PB_HP_OPENED;
	u32 PB_I2S_BCK_WIDTH;
	u32 PB_I2S_TX2_TDM_CFG;
};

struct playback_priv {
	struct device *dev;
	struct mem_info mem_info;
	struct mtk_runtime_struct *pcm_device[PLAYBACK_DEV_NUM];
	u32 play_dma_offset[PLAYBACK_DEV_NUM];
	u32 play_dma_size[PLAYBACK_DEV_NUM];
	u32 hfp_enable_flag;
};

int playback_dev_probe(struct platform_device *pdev);
int playback_dev_remove(struct platform_device *pdev);

#endif /* _PLAYBACK_DMA_PLATFORM_HEADER */
