/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_duplex_platform.h  --  Mediatek PCM device header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _DUPLEX_PLATFORM_HEADER
#define _DUPLEX_PLATFORM_HEADER

#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>

#include "mtk_alsa_pcm_common.h"

extern unsigned char sinetone_44[SINETONE_44_SIZE];

#define SOC_INTEGER_SRC(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = src_control_info, \
	.get = src_control_get_value, .put = src_control_put_value, \
	.private_value = (unsigned short)xvalue }

enum {
	SRC1_DEV,
	SRC2_DEV,
	SRC3_DEV,
	DUPLEX_DEV_NUM,
};

enum src_kcontrol_cmd {
	SRC1_RATE,
	SRC1_PB_BUF_LEVEL,    // unit: bytes
	SRC1_CAP_BUF_LEVEL,    // unit: bytes
	SRC2_RATE,
	SRC2_PB_BUF_LEVEL,    // unit: bytes
	SRC2_CAP_BUF_LEVEL,    // unit: bytes
	SRC3_RATE,
	SRC3_PB_BUF_LEVEL,    // unit: bytes
	SRC3_CAP_BUF_LEVEL,    // unit: bytes
};

struct duplex_register {
	u32 CHANNEL_M1_CFG;
	u32 CHANNEL_M2_CFG;
	u32 CHANNEL_M3_CFG;
	u32 CHANNEL_M4_CFG;
	u32 CHANNEL_M5_CFG;
	u32 CHANNEL_M6_CFG;
	u32 CHANNEL_M7_CFG;
	u32 CHANNEL_M8_CFG;
	u32 CHANNEL_M9_CFG;
	u32 CHANNEL_M10_CFG;
	u32 CHANNEL_M11_CFG;
	u32 CHANNEL_M12_CFG;
	u32 CHANNEL_M13_CFG;
	u32 CHANNEL_M14_CFG;
	u32 CHANNEL_M15_CFG;
	u32 CHANNEL_M16_CFG;
	u32 CHANNEL_5_CFG;
	u32 CHANNEL_6_CFG;
	u32 CHANNEL_7_CFG;
	u32 CHANNEL_8_CFG;
	u32 CHANNEL_9_CFG;
	u32 CHANNEL_10_CFG;
	u32 PB_DMA_SYNTH_UPDATE;
	// SRC1
	u32 PB_DMA_SYNTH_H;
	u32 PB_DMA_SYNTH_L;
	u32 PB_DMA_CFG_00;
	u32 PB_DMA_CFG_01;
	u32 PB_DMA_CFG_02;
	u32 PB_DMA_CFG_03;
	u32 PB_DMA_CFG_04;
	u32 PB_DMA_CFG_05;
	u32 PB_DMA_CFG_06;
	u32 PB_DMA_CFG_07;
	u32 PB_DMA_CFG_08;
	u32 PB_DMA_CFG_09;
	u32 PB_DMA_ENABLE;
	u32 PB_DMA_STATUS_00;
	u32 CAP_DMA_CTRL;
	u32 CAP_DMA_RPTR;
	u32 CAP_DMA_WPTR;
	// SRC2
	u32 SRC2_PB_DMA_CTRL;
	u32 SRC2_PB_DMA_WPTR;
	u32 SRC2_PB_DMA_DDR_LEVEL;
	u32 SRC2_PB_DMA_SYNTH_H;
	u32 SRC2_PB_DMA_SYNTH_L;
	u32 SRC2_PB_DMA_SYNTH_UPDATE;
	u32 SRC2_CAP_DMA_CTRL;
	u32 SRC2_CAP_DMA_RPTR;
	u32 SRC2_CAP_DMA_WPTR;
	// SRC3
	u32 SRC3_PB_DMA_SYNTH_H;
	u32 SRC3_PB_DMA_SYNTH_L;
	u32 SRC3_PB_DMA_CFG_00;
	u32 SRC3_PB_DMA_CFG_01;
	u32 SRC3_PB_DMA_CFG_02;
	u32 SRC3_PB_DMA_CFG_03;
	u32 SRC3_PB_DMA_CFG_04;
	u32 SRC3_PB_DMA_CFG_05;
	u32 SRC3_PB_DMA_CFG_06;
	u32 SRC3_PB_DMA_CFG_07;
	u32 SRC3_PB_DMA_CFG_08;
	u32 SRC3_PB_DMA_CFG_09;
	u32 SRC3_PB_DMA_ENABLE;
	u32 SRC3_PB_DMA_STATUS_00;
	u32 SRC3_CAP_DMA_CTRL;
	u32 SRC3_CAP_DMA_RPTR;
	u32 SRC3_CAP_DMA_WPTR;
};

struct duplex_priv {
	struct device *dev;
	struct mem_info mem_info;
	struct mtk_runtime_struct *pcm_device[DUPLEX_DEV_NUM];
	u32 play_dma_offset[DUPLEX_DEV_NUM];
	u32 play_dma_size[DUPLEX_DEV_NUM];
	u32 cap_dma_offset[DUPLEX_DEV_NUM];
	u32 cap_dma_size[DUPLEX_DEV_NUM];
};

int duplex_dev_probe(struct platform_device *pdev);
int duplex_dev_remove(struct platform_device *pdev);

#endif /* _DUPLEX_PLATFORM_HEADER */
