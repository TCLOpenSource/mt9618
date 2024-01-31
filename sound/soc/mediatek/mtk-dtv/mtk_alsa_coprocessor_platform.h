/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_coprocessor_platform.h  --  Mediatek DTV coprocessor header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#ifndef _COPROCESSOR_PLATFORM_HEADER
#define _COPROCESSOR_PLATFORM_HEADER

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>

#include "mtk_alsa_pcm_common.h"

enum {
	AU_SE_SYSTEM = 0,
	AU_DVB2_NONE,
	AU_SIF,
	AU_CODE_SEGMENT_MAX,
};

enum {
	DSP_MAIN_COUNTER = 0,
	DSP_ISR_COUNTER,
	DSP_DISABLE,
	DSP_REBOOT,
	DSP_END,
};

enum {
	COPRO_DEVICE,
	COPRO_DEVICE_END,
};

#define SOC_REBOOT(xname, xvalue) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.info = reboot_control_info, \
	.get = reboot_get_info, .put = reboot_put_info, \
	.private_value = (unsigned short)xvalue }

#define REBOOT_COUNT 10

#define MST_CODEC_SYSTEM_VERSION_CHAR1            0xA3
#define MST_CODEC_SYSTEM_VERSION_CHAR2            0x0E
#define MST_CODEC_SYSTEM_VERSION_CHAR3            0x88

#define MST_CODEC_PM1_FLASH_ADDR                  0x00000000
#define MST_CODEC_PM2_FLASH_ADDR                  0x000026e8
#define MST_CODEC_PM3_FLASH_ADDR                  0x00002a48
#define MST_CODEC_PM4_FLASH_ADDR                  0x00004818
#define MST_CODEC_DEC_PM1_FLASH_ADDR              0x00004830
#define MST_CODEC_DEC_PM2_FLASH_ADDR              0x00004848
#define MST_CODEC_DEC_PM3_FLASH_ADDR              0x00004878
#define MST_CODEC_DEC_PM4_FLASH_ADDR              0x00004890
#define MST_CODEC_SIF_PM1_FLASH_ADDR              0x000048a8
#define MST_CODEC_SIF_PM2_FLASH_ADDR              0x00004950
#define MST_CODEC_SIF_PM3_FLASH_ADDR              0x00005fe8

#define MST_CODEC_PM1_ADDR                        0x00000000
#define MST_CODEC_PM1_SIZE                        0x000026df
#define MST_CODEC_PM2_ADDR                        0x00001e40
#define MST_CODEC_PM2_SIZE                        0x0000035a
#define MST_CODEC_PM3_ADDR                        0x00008000
#define MST_CODEC_PM3_SIZE                        0x00001dc4
#define MST_CODEC_PM4_ADDR                        0x0000c000
#define MST_CODEC_PM4_SIZE                        0x00000003
#define MST_CODEC_DEC_PM1_ADDR                    0x00001e00
#define MST_CODEC_DEC_PM1_SIZE                    0x00000012
#define MST_CODEC_DEC_PM2_ADDR                    0x00002040
#define MST_CODEC_DEC_PM2_SIZE                    0x00000021
#define MST_CODEC_DEC_PM3_ADDR                    0x00009000
#define MST_CODEC_DEC_PM3_SIZE                    0x00000003
#define MST_CODEC_DEC_PM4_ADDR                    0x0000c800
#define MST_CODEC_DEC_PM4_SIZE                    0x00000012
#define MST_CODEC_SIF_PM1_ADDR                    0x00001e00
#define MST_CODEC_SIF_PM1_SIZE                    0x00000096
#define MST_CODEC_SIF_PM2_ADDR                    0x00002040
#define MST_CODEC_SIF_PM2_SIZE                    0x00001698
#define MST_CODEC_SIF_PM3_ADDR                    0x00009000
#define MST_CODEC_SIF_PM3_SIZE                    0x0000722d

struct dsp_alg_info {
	unsigned int	cm_addr;
	unsigned int	cm_len;
	unsigned char	*cm_buf;

	unsigned int	pm_addr;
	unsigned int	pm_len;
	unsigned char	*pm_buf;

	unsigned int	cache_addr;
	unsigned int	cache_len;
	unsigned char	*cache_buf;

	unsigned int	prefetch_addr;
	unsigned int	prefetch_len;
	unsigned char	*prefetch_buf;
};

struct copr_register {
	u32 DEC_AUD_CTRL;
	u32 DEC_MAD_OFFSET_BASE_L;
	u32 DEC_MAD_OFFSET_BASE_H;
	u32 DEC_MAD_OFFSET_BASE_EXT;
	u32 SE_AUD_CTRL;
	u32 SE_MAD_OFFSET_BASE_L;
	u32 SE_MAD_OFFSET_BASE_H;
	u32 SE_MAD_OFFSET_BASE_EXT;
	u32 SE_IDMA_CTRL0;
	u32 SE_DSP_ICACHE_BASE_L;
	u32 SE_BDMA_CFG;
	u32 FD230_SEL;
	u32 SE_DSP_BRG_DATA_L;
	u32 SE_DSP_BRG_DATA_H;
	u32 SE_IDMA_WRBASE_ADDR_L;
	u32 SE_IDMA_RDBASE_ADDR_L;
	u32 SE_IDMA_RDDATA_H_0;
	u32 SE_IDMA_RDDATA_H_1;
	u32 SE_IDMA_RDDATA_L;
	u32 RIU_MAIL_00;
	u32 DSP_POWER_DOWN_H;
	u32 DSP_DBG_CMD1;
	u32 DSP_ISR_CNTR;
	u32 DSP_WHILE1_CNTR;
	u32 DSP_DBG_RESULT1;
	u32 DSP_TEST_MBX;
	u32 DSP_TEST_STATUS;
};

struct reboot_info {
	int disable_flag;
	int fail_cycle_count;
	unsigned int fail_cntr_count;
	unsigned char prev_while_cntr;
	unsigned char prev_isr_cntr;
};

struct copr_priv {
	struct device *dev;
	struct mem_info mem_info;
	struct timer_list timer;
	struct reboot_info reboot_info;
	struct snd_soc_component *component;
	const char *fw_name;
	unsigned int fw_size;
	unsigned int log_level;
	bool suspended;
};

#endif /* _COPROCESSOR_PLATFORM_HEADER */
