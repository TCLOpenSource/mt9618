// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * mtk_alsa_dplayback_platform.c  --  Mediatek dplayback function
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mtk_alsa_chip.h"
#include "mtk_alsa_dplayback_platform.h"
#include "mtk-reserved-memory.h"

#define DPLAYBACK_DMA_NAME	"snd-doutput-pcm-dai"

#define MAX_DIGITAL_BUF_SIZE	(12 * 4 * 2048 * 4)

static const char * const DPLAYBACK_ID_STRING[] = {"[SPDIF_PCM_PLAYBACK]",
						"[SPDIF_NONPCM_PLAYBACK]",
						"[ARC_PCM_PLAYBACK]",
						"[ARC_NONPCM_PLAYBACK]",
						"[EARC_PCM_PLAYBACK]",
						"[EARC_NONPCM_PLAYBACK]"};
static struct dplayback_register REG;

static unsigned int dplayback_pcm_rates[] = {
	RATE_32000,
	RATE_44100,
	RATE_48000,
	RATE_176400,
	RATE_192000,
};

static unsigned int spdif_nonpcm_rates[] = {
	RATE_32000,
	RATE_44100,
	RATE_48000,
};

static unsigned int arc_nonpcm_rates[] = {
	RATE_32000,
	RATE_44100,
	RATE_48000,
	RATE_88200,
	RATE_96000,
	RATE_176400,
	RATE_192000,
};

static unsigned int earc_nonpcm_rates[] = {
	RATE_32000,
	RATE_44100,
	RATE_48000,
	RATE_88200,
	RATE_96000,
	RATE_176400,
	RATE_192000,
};

static unsigned int dplayback_channels[] = {
	CH_2, CH_6, CH_8,
};

static const struct snd_pcm_hw_constraint_list constraints_dpb_pcm_rates = {
	.count = ARRAY_SIZE(dplayback_pcm_rates),
	.list = dplayback_pcm_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_spdif_nonpcm_rates = {
	.count = ARRAY_SIZE(spdif_nonpcm_rates),
	.list = spdif_nonpcm_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_arc_nonpcm_rates = {
	.count = ARRAY_SIZE(arc_nonpcm_rates),
	.list = arc_nonpcm_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_dpb_channels = {
	.count = ARRAY_SIZE(dplayback_channels),
	.list = dplayback_channels,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_earc_nonpcm_rates = {
	.count = ARRAY_SIZE(earc_nonpcm_rates),
	.list = earc_nonpcm_rates,
	.mask = 0,
};

/* Default hardware settings of ALSA PCM playback */
static struct snd_pcm_hardware dplayback_default_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.channels_min = CH_2,
	.channels_max = CH_8,
	.buffer_bytes_max = MAX_DIGITAL_BUF_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = MAX_DIGITAL_BUF_SIZE >> 3,
	.periods_min = 2,
	.periods_max = 8,
	.fifo_size = 0,
};

static int spdif_nonpcm_set_synthesizer(unsigned int sample_rate)
{
	mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG, 0x07, 0x02);
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG+1, 0x10, 0x10);

	switch (sample_rate) {
	case RATE_32000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]spdif synthesizer rate is 32 KHz\n");
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_H, 0x34BC);
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_L, 0x0000);
		break;
	case RATE_44100:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]spdif synthesizer rate is 44.1 KHz\n");
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_H, 0x2643);
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_L, 0xEB1A);
		break;
	case RATE_48000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]spdif synthesizer rate is 48 KHz\n");
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_L, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]Err! un-supported rate !!!\n");
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_SPDIF_SYNTH_L, 0x0000);
		break;
	}
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x20, 0x20);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x20, 0x00);
	return 0;
}

static int arc_nonpcm_set_synthesizer(unsigned int sample_rate)
{
	mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG, 0x07, 0x02);
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG+1, 0x20, 0x20);

	switch (sample_rate) {
	case RATE_32000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 32 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x34BC);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_44100:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 44.1 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2643);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0xEB1A);
		break;
	case RATE_48000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 48 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_96000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 96 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x1194);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_176400:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 176.4 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x0990);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0xFAC6);
		break;
	case RATE_192000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]arc synthesizer rate is 192 KHz\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x8CA);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]Err! un-supported rate !!!\n");
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	}
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x10, 0x10);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x10, 0x00);
	return 0;
}

static int earc_nonpcm_set_synthesizer(unsigned int sample_rate)
{
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10+1, 0x70, 0x20);  //NPCM SYNTH APN page 6
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10, 0x10, 0x10);  // RIU select

	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG+1, 0x70, 0x20);// RIU select

	switch (sample_rate) {
	case RATE_32000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 32 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x6978);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0x0000);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x34BC);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_44100:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 44.1 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x4C87);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0xD634);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2643);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0xEB1A);
		break;
	case RATE_48000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 48 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x4650);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0x0000);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_96000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 96 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x2328);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0x0000);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x1194);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	case RATE_176400:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 176.4 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x1321);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0xF58C);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x0990);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0xFAC6);
		break;
	case RATE_192000:
		apr_info(MTK_ALSA_LEVEL_INFO,
			"[ALSA DPB]earc synthesizer rate is 192 KHz\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x1194);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0x0000);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x8CA);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]Err! un-supported rate !!!\n");
		mtk_alsa_write_reg(REG.DPB_EARC_CFG12, 0x4650);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG11, 0x0000);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_H, 0x2328);
		mtk_alsa_write_reg(REG.DPB_ARC_SYNTH_L, 0x0000);
		break;
	}
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10, 0x20, 0x20);//update bit toggle
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10, 0x20, 0x00);


	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x10, 0x10); //update bit 0x112bce [4]
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_SYNTH_CFG, 0x10, 0x00); //update bit 0x112bce [4]
	return 0;
}

static int earc_atop_aupll(int rate, int channels)
{
	int error = 0;

	switch (rate) {
	case 0:
		error = 1;
		break;
	case RATE_32000:
	case RATE_44100:
	case RATE_48000:
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C, 0x03, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C+1, 0xFF, 0x28);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX30+1, 0x03, 0x00);
		if (channels == CH_2)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x50);
		else if (channels == CH_8)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x30);
		else
			error = 1;
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX28, 0x70, 0x30);
		break;
	case RATE_88200:
	case RATE_96000:
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C, 0x03, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C+1, 0xFF, 0x14);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX30+1, 0x03, 0x00);
		if (channels == CH_2)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x40);
		else if (channels == CH_8)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x20);
		else
			error = 1;
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX28, 0x70, 0x30);
		break;
	case RATE_176400:
	case RATE_192000:
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C, 0x03, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C+1, 0xFF, 0x14);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX30+1, 0x03, 0x01);
		if (channels == CH_2)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x30);
		else if (channels == CH_8)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x10);
		else
			error = 1;
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX28, 0x70, 0x30);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]Err! un-supported earc rate or channel !!!\n");
		error = 1;
		break;
	}

	if (error) {
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C, 0x03, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX2C+1, 0xFF, 0x04);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX30+1, 0x03, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX20+1, 0x70, 0x30);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX28, 0x70, 0x20);
	}
	return error;
}

static int earc_channel_status(struct mtk_runtime_struct *dev_runtime,
						unsigned int id)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	int datarate;

	/* set channel status */
	if ((id == EARC_NONPCM_PLAYBACK) || (id == ARC_NONPCM_PLAYBACK)) {
		//[5:3][1:0]unencrypted IEC61937
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x3F, 0x02);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x80, 0x80);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25, 0x0F, 0x00);
		//Table 9-27 ICE61937 Compressed Audio Layout A
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25+1, 0xF0, 0x00);

		datarate = (playback->channel_mode / 2) * playback->sample_rate;
		switch (datarate) {
		case 32000:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x03);
			break;
		case 44100:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x00);
			break;
		case 48000:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x02);
			break;
		case 176400:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0C);
			break;
		case 192000:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0E);
			break;
		case 768000:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x09);
			break;
		default:
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x02);
			apr_err(dev_runtime->log_level,
				"[ALSA DPB]%swrong nonpcm data rate\n", DPLAYBACK_ID_STRING[id]);
			break;
		}
		//[143:136] speaker map: NA
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0xFF, 0x00);
	} else {
		//PCM case
		apr_info(dev_runtime->log_level,
			"[ALSA DPB]earc cs info use hw param\n");

		if (playback->channel_mode == CH_2) {
			//[5:3][1:0]unencrypted 2-ch LPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x3F, 0x00);
			//[15:12]2-channel layout
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25+1, 0xF0, 0x00);
			//sample rate
			switch (playback->sample_rate) {
			case RATE_48000:
				//[11:8]48000Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x02);
				break;
			case RATE_44100:
				//[11:8]44100Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x00);
				break;
			case RATE_32000:
				//[11:8]32000Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x03);
				break;
			case RATE_88200:
				//[11:8]88200Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x08);
				break;
			case RATE_96000:
				//[11:8]96000Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0A);
				break;
			case RATE_176400:
				//[11:8]176400Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0C);
				break;
			case RATE_192000:
				//[11:8]192000Hz
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0E);
				break;
			default:
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x01);
				break;
			}
		} else if (playback->channel_mode == CH_8) {
			//[5:3][1:0]unencrypted 8-ch LPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x3F, 0x20);
			//[15:12]8-channel layout
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25+1, 0xF0, 0x70);
			//sample rate
			switch (playback->sample_rate) {
			case RATE_48000:
				//[11:8]48000Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0E);
				break;
			case RATE_44100:
				//[11:8]44100Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0C);
				break;
			case RATE_32000:
				//[11:8]32000Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x8B);
				break;
			case RATE_88200:
				//[11:8]88200Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x0D);
				break;
			case RATE_96000:
				//[11:8]96000Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x05);
				break;
			case RATE_176400:
				//[11:8]176400Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x8D);
				break;
			case RATE_192000:
				//[11:8]192000Hz*4
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x09);
				break;
			default:
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG26+1, 0x0F, 0x01);
				break;
			}
		}
		//mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x80, 0x80);//sync with utopia

		if (playback->channel_mode == CH_8) {
			//[15:8]FR/FL/LFE1/FC/LS/RS/RLC/RRC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0xFF, 0x13);
		} else if (playback->channel_mode == CH_6) {
			//[15:8]FR/FL/LFE1/FC/LS/RS/RLC/RRC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0xFF, 0x0B);
		} else {
			//[15:8]FR/FL/LFE1/FC/LS/RS/RLC/RRC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0xFF, 0);
		}

		//[35:32] bit length
		if (playback->format == SNDRV_PCM_FORMAT_S24_LE)
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25, 0x0F, 0x0B);
		else
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG25, 0x0F, 0x02);
	}

	return 0;
}

//[Sysfs] HELP command
static ssize_t help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (check_sysfs_null(dev, attr, buf))
		return 0;

	return snprintf(buf, PAGE_SIZE, "Debug Help:\n"
		"- dplayback_log_level\n"
		"	<W>[id][level]: Set device id & debug log level.\n"
		"	(0:spdif_pcm, 1:spdif_nonpcm, 2:arc_pcm, 3:arc_nonpcm, 4:earc_pcm, 5:earc_nonpcm ...)\n"
		"	(0:emerg, 1:alert, 2:critical, 3:error, 4:warn, 5:notice, 6:info, 7:debug)\n"
		"	<R>: Read all device debug log level.\n");
}

static ssize_t dplayback_log_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct dplayback_priv *priv = NULL;
	unsigned int id;
	unsigned int log_level;

	if (check_sysfs_null(dev, attr, (char *)buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	if (sscanf(buf, "%u %u", &id, &log_level) < 0) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	if ((id < DPLAYBACK_DEV_NUM) && (log_level <= LOGLEVEL_DEBUG))
		priv->pcm_device[id]->log_level = log_level;

	return count;
}

static ssize_t dplayback_log_level_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;
	unsigned int id;
	int count = 0;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	for (id = 0; id < DPLAYBACK_DEV_NUM; id++) {
		count += snprintf(buf + count, PAGE_SIZE, "%slog_level:%d\n",
			DPLAYBACK_ID_STRING[id], priv->pcm_device[id]->log_level);
	}
	return count;
}

static ssize_t cs_log_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct dplayback_priv *priv = NULL;
	unsigned int log_level;

	if (check_sysfs_null(dev, attr, (char *)buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	if (kstrtoint(buf, 10, &log_level)) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	if (log_level <= LOGLEVEL_DEBUG)
		priv->cs_log_level = log_level;

	return count;
}

static ssize_t cs_log_level_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return snprintf(buf, PAGE_SIZE, "cs_log_level:%d\n", priv->cs_log_level);
}

static ssize_t earc_channel_freq_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;
	struct mtk_runtime_struct *dev_runtime = NULL;
	struct mtk_dma_struct *playback = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	dev_runtime = priv->pcm_device[EARC_NONPCM_PLAYBACK];
	if (dev_runtime == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]dev_runtime can't be NULL!!!\n", __func__);
		return 0;
	}

	playback = &dev_runtime->playback;
	if (playback == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]dma playback can't be NULL!!!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "earc nonpcm channel is :%u\n"
			"earc nonpcm freq is %u\n",
			playback->channel_mode, playback->sample_rate);
}

static ssize_t earc_channel_freq_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int channel, freq;
	struct dplayback_priv *priv = NULL;
	struct mtk_runtime_struct *dev_runtime = NULL;
	struct mtk_dma_struct *playback = NULL;

	if (check_sysfs_null(dev, attr, (char *)buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	dev_runtime = priv->pcm_device[EARC_NONPCM_PLAYBACK];
	if (dev_runtime == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]dev_runtime can't be NULL!!!\n", __func__);
		return 0;
	}

	playback = &dev_runtime->playback;
	if (playback == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]dma playback can't be NULL!!!\n", __func__);
		return 0;
	}

	if (sscanf(buf, "%u %u", &channel, &freq) < 0) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	if ((channel != 2) && (channel != 8)) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]channel overbound!!!\n", __func__);
		return 0;
	}

	playback->channel_mode = channel;
	playback->sample_rate = freq;

	//earc_apply_setting
	earc_nonpcm_set_synthesizer(playback->sample_rate);
	earc_channel_status(dev_runtime, EARC_NONPCM_PLAYBACK);
	// step6: start output audio stream(differential signal)
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x3F, 0x00);
	//update bit
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00, 0x08, 0x08);

	//utopia step7: when audio part is ready
	earc_atop_aupll(playback->sample_rate, playback->channel_mode);

	//clock
	mtk_earc_clk_set_enable(1);

	return count;
}

static ssize_t digital_out_cs_set_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return snprintf(buf, PAGE_SIZE, "c bit %d\n"
			"l bit %d\n"
			"category %d\n"
			"channel alloc %d\n",
			priv->C_bit_en,
			priv->L_bit_en,
			priv->category,
			priv->ch_alloc);
}

/*commad echo 1 1 10 >*/
/*sys/devices/platform/mtk-sound-dplayback-platform/mtk_dbg/digital_out_cs_set */
static ssize_t digital_out_cs_set_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct dplayback_priv *priv = NULL;
	int c_bit;
	int l_bit;
	int category;
	int ch_alloc;

	if (check_sysfs_null(dev, attr, (char *)buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	if (sscanf(buf, "%u %u %u %u", &c_bit, &l_bit, &category, &ch_alloc) < 0) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	priv->C_bit_en = c_bit;
	priv->L_bit_en = l_bit;
	priv->category = category;
	priv->ch_alloc = ch_alloc;

	mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x0020, priv->C_bit_en << 5);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x0001, priv->L_bit_en);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x00fe, priv->category);

	//update spdif channel status
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);//Tx1 Toggle bit[14]
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x40);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);

	//update arc channel status
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00); //Tx2 Toggle bit[12]
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x10);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);

	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x0004, priv->C_bit_en << 2);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x0080, priv->L_bit_en << 7);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x007f, priv->category);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0x00ff, priv->ch_alloc);

	//update  earc channel status
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00); //earc Toggle bit
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00);

	return count;
}

static ssize_t dplayback_status_show(struct device *dev, struct device_attribute *attr,
					char *buf, struct dplayback_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	const char *str;
	int count = 0;

	dev_runtime = priv->pcm_device[id];
	if (!dev_runtime) {
		str = "closed";
		return snprintf(buf, PAGE_SIZE, "%s\n", str);
	}
	substream = dev_runtime->substreams.p_substream;
	if (!substream) {
		str = "closed";
		return snprintf(buf, PAGE_SIZE, "%s\n", str);
	}
	runtime = substream->runtime;

	if (!runtime) {
		str = "closed";
		return snprintf(buf, PAGE_SIZE, "%s\n", str);
	}

	count += scnprintf(buf+count, PAGE_SIZE, "owner_pid   : %d\n", pid_vnr(substream->pid));
	count += scnprintf(buf+count, PAGE_SIZE, "avail_max   : %ld\n", runtime->avail_max);
	count += scnprintf(buf+count, PAGE_SIZE, "-----\n");
	count += scnprintf(buf+count, PAGE_SIZE, "hw_ptr      : %ld\n", runtime->status->hw_ptr);
	count += scnprintf(buf+count, PAGE_SIZE, "appl_ptr    : %ld\n", runtime->control->appl_ptr);

	return count;
}


//[Sysfs] Dplayback Sine tone
static ssize_t dplayback_sinetone_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	struct dplayback_priv *priv;
	unsigned int id;
	unsigned int enablesinetone = 0;

	if (dev == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[AUDIO][ERROR][%s]dev can't be NULL!!!\n", __func__);
		return 0;
	}
	priv = dev_get_drvdata(dev);

	if (priv == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return 0;
	}

	if (attr == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]attr can't be NULL!!!\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]buf can't be NULL!!!\n", __func__);
		return 0;
	}

	if (sscanf(buf, "%u %u", &id, &enablesinetone) < 0) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	if ((id < DPLAYBACK_DEV_NUM) && (enablesinetone <= 1)) {
		priv->pcm_device[id]->inject.status = enablesinetone;
		priv->pcm_device[id]->inject.r_ptr = 0;
	}

	return count;
}

static ssize_t dplayback_sinetone_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv;
	unsigned int id;
	int count = 0;

	if (dev == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[AUDIO][ERROR][%s]dev can't be NULL!!!\n", __func__);
		return 0;
	}
	priv = dev_get_drvdata(dev);

	if (priv == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return 0;
	}

	if (attr == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]attr can't be NULL!!!\n", __func__);
		return 0;
	}

	if (buf == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]buf can't be NULL!!!\n", __func__);
		return 0;
	}

	for (id = 0; id < DPLAYBACK_DEV_NUM; id++) {
		count += snprintf(buf + count, PAGE_SIZE, "%ssinetone status:%d\n",
			DPLAYBACK_ID_STRING[id], priv->pcm_device[id]->inject.status);
	}

	return count;
}


static ssize_t spdif_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, SPDIF_PCM_PLAYBACK);
}

static ssize_t spdif_npcm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, SPDIF_NONPCM_PLAYBACK);
}

static ssize_t arc_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, ARC_PCM_PLAYBACK);
}

static ssize_t arc_npcm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, ARC_NONPCM_PLAYBACK);
}

static ssize_t earc_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, EARC_PCM_PLAYBACK);
}

static ssize_t earc_npcm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dplayback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return dplayback_status_show(dev, attr, buf, priv, EARC_NONPCM_PLAYBACK);
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(dplayback_log_level);
static DEVICE_ATTR_RW(cs_log_level);
static DEVICE_ATTR_RW(earc_channel_freq);
static DEVICE_ATTR_RW(digital_out_cs_set);
static DEVICE_ATTR_RO(spdif_status);
static DEVICE_ATTR_RO(spdif_npcm_status);
static DEVICE_ATTR_RO(arc_status);
static DEVICE_ATTR_RO(arc_npcm_status);
static DEVICE_ATTR_RO(earc_status);
static DEVICE_ATTR_RO(earc_npcm_status);
static DEVICE_ATTR_RW(dplayback_sinetone);

static struct attribute *dplayback_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_dplayback_log_level.attr,
	&dev_attr_cs_log_level.attr,
	&dev_attr_earc_channel_freq.attr,
	&dev_attr_digital_out_cs_set.attr,
	&dev_attr_spdif_status.attr,
	&dev_attr_spdif_npcm_status.attr,
	&dev_attr_arc_status.attr,
	&dev_attr_arc_npcm_status.attr,
	&dev_attr_earc_status.attr,
	&dev_attr_earc_npcm_status.attr,
	&dev_attr_dplayback_sinetone.attr,
	NULL,
};

static const struct attribute_group dplayback_attr_group = {
	.name = "mtk_dbg",
	.attrs = dplayback_attrs,
};


static const struct attribute_group *dplayback_attr_groups[] = {
	&dplayback_attr_group,
	NULL,
};

static int dplayback_reboot_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0002, 0x0000);
	mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0002, 0x0000);
	mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0002, 0x0000);

	mtk_spdif_tx_clk_set_disable(0);
	mtk_spdif_tx_clk_set_disable(1);
	mtk_arc_clk_set_disable(0);
	mtk_arc_clk_set_disable(1);
	mtk_earc_clk_set_disable(0);
	mtk_earc_clk_set_disable(1);
	mtk_earc_clk_set_disable(2);

	return NOTIFY_OK;
}

/* reboot notifier */
static struct notifier_block alsa_dplayback_notifier = {
	.notifier_call = dplayback_reboot_notify,
};

static int dplayback_probe(struct snd_soc_component *component)
{
	struct dplayback_priv *priv;
	struct device *dev;
	int ret;

	if (component == NULL) {
		pr_err("[AUDIO][ERROR][%s]component can't be NULL!!!\n", __func__);
		return -EINVAL;
	}

	if (component->dev == NULL) {
		pr_err("[AUDIO][ERROR][%s]component->dev can't be NULL!!!\n", __func__);
		return -EINVAL;
	}
	dev = component->dev;

	priv = snd_soc_component_get_drvdata(component);
	if (priv == NULL) {
		dev_err(dev, "[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return -EFAULT;
	}

	ret = 0;

	/*Set default log level*/
	priv->pcm_device[SPDIF_PCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[SPDIF_NONPCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[ARC_PCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[ARC_NONPCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[EARC_PCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[EARC_NONPCM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->cs_log_level = LOGLEVEL_DEBUG;

	/*ip-version */
	priv->ip_version = mtk_alsa_read_chip();

	/* clk status */
	priv->dp_clk_status.spdif_clk = 0;
	priv->dp_clk_status.arc_clk = 0;
	priv->dp_clk_status.earc_clk = 0;

	/* Create device attribute files */
	ret = sysfs_create_groups(&dev->kobj, dplayback_attr_groups);
	if (ret)
		dev_err(dev, "[AUDIO][ERROR][%s]sysfs_create_groups can't fail!!!\n", __func__);

	ret = register_reboot_notifier(&alsa_dplayback_notifier);
	if (ret) {
		dev_err(dev, "[ALSA DPB]register reboot notify fail %d\n", ret);
		return ret;
	}

	return 0;
}

static void dplayback_remove(struct snd_soc_component *component)
{
	struct device *dev;

	dev = NULL;

	if (component == NULL)
		pr_err("[AUDIO][ERROR][%s]component can't be NULL!!!\n", __func__);
	else
		dev = component->dev;

	if (dev != NULL)
		sysfs_remove_groups(&dev->kobj, dplayback_attr_groups);
}

static void dma_register_get_reg(unsigned int id, u32 *ctrl, u32 *wptr, u32 *level)
{
	switch (id)  {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
		*ctrl = REG.DPB_DMA5_CTRL;
		*wptr = REG.DPB_DMA5_WPTR;
		*level = REG.DPB_DMA5_DDR_LEVEL;
		break;
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		*ctrl = REG.DPB_DMA6_CTRL;
		*wptr = REG.DPB_DMA6_WPTR;
		*level = REG.DPB_DMA6_DDR_LEVEL;
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		*ctrl = REG.DPB_DMA7_CTRL;
		*wptr = REG.DPB_DMA7_WPTR;
		*level = REG.DPB_DMA7_DDR_LEVEL;
		break;
	default:
		break;
	}
}

static int dma_reader_init(struct dplayback_priv *priv,
			struct snd_pcm_runtime *runtime, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	ptrdiff_t dmaRdr_base_pa = 0;
	ptrdiff_t dmaRdr_base_ba = 0;
	ptrdiff_t dmaRdr_base_va = playback->str_mode_info.virtual_addr;
	unsigned int device_id = id/2;

	playback->r_buf.size = priv->play_dma_size[device_id];
	playback->high_threshold =
		playback->r_buf.size - (playback->r_buf.size >> 3);

	if ((playback->initialized_status != 1) ||
		(playback->status != CMD_RESUME)) {
		dmaRdr_base_pa = priv->mem_info.bus_addr -
				priv->mem_info.memory_bus_base + priv->play_dma_offset[device_id];
		dmaRdr_base_ba = priv->mem_info.bus_addr + priv->play_dma_offset[device_id];

		if ((dmaRdr_base_ba % 0x1000)) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%s invalid addr,should align 4KB\n",
				DPLAYBACK_ID_STRING[id]);
			return -EFAULT;
		}

		/* convert Bus Address to Virtual Address */
		if (dmaRdr_base_va == 0) {
			dmaRdr_base_va = (ptrdiff_t)ioremap_wc(dmaRdr_base_ba,
						playback->r_buf.size);
			if (dmaRdr_base_va == 0) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA DPB]%s fail to convert addr\n",
					DPLAYBACK_ID_STRING[id]);
				return -ENOMEM;
			}
		}

		playback->str_mode_info.physical_addr = dmaRdr_base_pa;
		playback->str_mode_info.bus_addr = dmaRdr_base_ba;
		playback->str_mode_info.virtual_addr = dmaRdr_base_va;

		playback->initialized_status = 1;
	} else {
		dmaRdr_base_pa = playback->str_mode_info.physical_addr;
		dmaRdr_base_ba = playback->str_mode_info.bus_addr;
		dmaRdr_base_va = playback->str_mode_info.virtual_addr;
	}

	/* init PCM playback buffer address */
	playback->r_buf.addr = (unsigned char *)dmaRdr_base_va;
	playback->r_buf.r_ptr = playback->r_buf.addr;
	playback->r_buf.w_ptr = playback->r_buf.addr;
	playback->r_buf.remain_size = 0;
	playback->r_buf.buf_end = playback->r_buf.addr + playback->r_buf.size;

	runtime->dma_area =  (unsigned char *)dmaRdr_base_va;
	runtime->dma_addr = dmaRdr_base_ba;
	runtime->dma_bytes =  playback->r_buf.size;

	/* clear PCM playback dma buffer */
	memset((void *)runtime->dma_area, 0x00, runtime->dma_bytes);

	/* reset copy size */
	playback->copy_size = 0;

	/* clear PCM playback pcm buffer */
	memset((void *)playback->r_buf.addr, 0x00, playback->r_buf.size);
	smp_mb();

	return 0;
}

static int dma_reader_exit(unsigned int id, struct mtk_runtime_struct *dev_runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	ptrdiff_t dmaRdr_base_va = playback->str_mode_info.virtual_addr;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
		/* reset PCM playback engine */
		mtk_alsa_write_reg(REG.DPB_DMA5_CTRL, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA5_WPTR, 0x0000);
		break;
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		/* reset PCM playback engine */
		mtk_alsa_write_reg(REG.DPB_DMA6_CTRL, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA6_WPTR, 0x0000);
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		/* reset PCM playback engine */
		mtk_alsa_write_reg(REG.DPB_DMA7_CTRL, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA7_WPTR, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA DPB] set wrong device exit\n");
		break;
	}

	if (dmaRdr_base_va != 0) {
		if (playback->r_buf.addr) {
			iounmap((void *)playback->r_buf.addr);
			playback->r_buf.addr = 0;
		} else
			apr_err(dev_runtime->log_level,
				"[ALSA DPB]%sdigital buf addr should not be 0\n",
				DPLAYBACK_ID_STRING[id]);

		playback->str_mode_info.virtual_addr = 0;
	}

	playback->status = CMD_STOP;
	playback->initialized_status = 0;

	return 0;
}

static int spdif_dma_start(unsigned int id, struct mtk_runtime_struct *dev_runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	/* set channel status */
	if (id == SPDIF_NONPCM_PLAYBACK) {
		mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG+1, 0x80, 0x80);
		apr_info(dev_runtime->log_level,
			"[ALSA DPB]spdif cs info use hw param\n");
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x60, 0x40);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x01, 0x01);
		switch (playback->sample_rate) {
		case RATE_32000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]spdif cs sample rate is 32 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0xC0);
			break;
		case RATE_44100:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]spdif cs sample rate is 44.1 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x00);
			break;
		case RATE_48000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]spdif cs sample rate is 48 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
			break;
		default:
			apr_err(dev_runtime->log_level,
				"[ALSA DPB]%sErr! un-supported rate !!!\n",
				DPLAYBACK_ID_STRING[id]);
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
			break;
		}
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0x00);
	} else {
		mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG+1, 0x80, 0x00);
		apr_info(dev_runtime->log_level,
			"[ALSA DPB]spdif cs info use hw param\n");
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x60, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x01, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
		/* set bit length */
		if (playback->format == SNDRV_PCM_FORMAT_S24_LE)
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0xD0);
		else
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0x40);
	}

	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0xA0, 0xA0);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x40);
	mtk_alsa_delaytask(5);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);

	/* start dplayback playback engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0003, 0x0002);

	/* trigger DSP to reset DMA engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0001, 0x0001);
	mtk_alsa_delaytask_us(50);
	mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0001, 0x0000);

	return 0;
}

void hdmi_arc_pin_control(int status)
{

	if (status == CMD_START) {
		//earc_tx_00[8]:reg_dati_arc_oven
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00+1, 0x01, 0x00);
	} else {
		//earc_tx_00[9]:reg_dati_arc
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00+1, 0x02, 0x00);
		//earc_tx_00[8]:reg_dati_arc_oven
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00+1, 0x01, 0x01);
	}

	apr_info(MTK_ALSA_LEVEL_INFO, "HDMI ARC PIN is %s\r\n",
		status?"Enable":"Disable");
}

static int arc_dma_start(unsigned int id,
	struct mtk_runtime_struct *dev_runtime, int ip_version)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	/* set channel status */
	if (id == ARC_NONPCM_PLAYBACK) {
		mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG+1, 0x80, 0x80);
		apr_info(dev_runtime->log_level,
			"[ALSA DPB]arc cs info use hw param\n");
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x60, 0x40);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x01, 0x01);
		switch (playback->sample_rate) {
		case RATE_32000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 32 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0xC0);
			break;
		case RATE_44100:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 44.1 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x00);
			break;
		case RATE_48000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 48 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
			break;
		case RATE_88200:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 88.2 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x10);
			break;
		case RATE_96000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 96 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x50);
			break;
		case RATE_176400:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 176 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x30);
			break;
		case RATE_192000:
			apr_info(dev_runtime->log_level,
				"[ALSA DPB]arc cs sample rate is 192 KHz\n");
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x70);
			break;
		default:
			apr_err(dev_runtime->log_level,
				"[ALSA DPB]%sErr! un-supported rate !!!\n",
				DPLAYBACK_ID_STRING[id]);
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
			break;
		}
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0x00);
	} else {
		mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG+1, 0x80, 0x00);
		apr_info(dev_runtime->log_level,
			"[ALSA DPB]arc cs info use hw param\n");
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x60, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x01, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
		/* set bit length */
		if (playback->format == SNDRV_PCM_FORMAT_S24_LE)
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0xD0);
		else
			mtk_alsa_write_reg_mask_byte(REG.DPB_CS4, 0xF0, 0x40);
	}

	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE, 0xA0, 0xA0);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE, 0x10, 0x00);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE, 0x10, 0x10);
	mtk_alsa_delaytask(5);
	mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE, 0x10, 0x00);

	if (ip_version == 0x1) {
		earc_channel_status(dev_runtime, id);
		//[15]REG_SWRSTZ_64XCHXFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x80, 0x80);
		//[14]REG_SWRSTZ_32XCHXFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x40, 0x40);
		//[13]REG_SWRSTZ_32XFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x20, 0x20);
		//[12]REG_SWRSTZ_SOURCE
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x10, 0x10);
		mtk_alsa_delaytask(1);
		//[09]REG_IEC60958_BIPHASEMARK_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x02, 0x02);
		//[08]REG_PARITY_BIT_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x01, 0x01);
		//[07]REG_ECC_ENCODE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x80, 0x80);
		//[06]REG_MULTI2DOUBLE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x40, 0x40);
		//[11]REG_RST_AFIFO_RIU
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x08);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
		//[04]REG_VALID_BIT_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x10, 0x10);
		//[03]REG_USER_STATUS_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x08, 0x08);
		//[02]REG_USER_STATUS_CRC_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x04, 0x04);
		//[01]REG_CHANNEL_STATUS_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x02, 0x02);
		//[05]REG_MIXER_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x20, 0x20);
		//[00]REG_MIXER_SOURCE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x01, 0x01);
		mtk_alsa_delaytask(1);
	}
	/* start dplayback playback engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0003, 0x0002);

	/* trigger DSP to reset DMA engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0001, 0x0001);
	mtk_alsa_delaytask_us(50);
	mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0001, 0x0000);

	if (ip_version == 0x1) {
		//utopia step7: when audio part is ready
		//arc always 2 ch
		earc_atop_aupll(playback->sample_rate, CH_2);

		// step8: clear AVMUTE in channel status.
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG18, 0x04, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);//update
		mtk_alsa_delaytask(1);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00);
	}

	return 0;
}

static int earc_dma_start(unsigned int id,
	struct mtk_runtime_struct *dev_runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	//[15]REG_SWRSTZ_64XCHXFS
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x80, 0x80);
	//[14]REG_SWRSTZ_32XCHXFS
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x40, 0x40);
	//[13]REG_SWRSTZ_32XFS
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x20, 0x20);
	//[12]REG_SWRSTZ_SOURCE
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x10, 0x10);
	mtk_alsa_delaytask(1);
	//[09]REG_IEC60958_BIPHASEMARK_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x02, 0x02);
	//[08]REG_PARITY_BIT_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x01, 0x01);
	//[07]REG_ECC_ENCODE_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x80, 0x80);
	//[06]REG_MULTI2DOUBLE_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x40, 0x40);
	//[11]REG_RST_AFIFO_RIU
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x08);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
	//[04]REG_VALID_BIT_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x10, 0x10);
	//[03]REG_USER_STATUS_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x08, 0x08);
	//[02]REG_USER_STATUS_CRC_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x04, 0x04);
	//[01]REG_CHANNEL_STATUS_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x02, 0x02);
	//[05]REG_MIXER_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x20, 0x20);
	//[00]REG_MIXER_SOURCE_EN
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x01, 0x01);
	mtk_alsa_delaytask(1);

	/* start dplayback engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0003, 0x0002);

	/* trigger DSP to reset DMA engine */
	mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0001, 0x0001);
	mtk_alsa_delaytask_us(50);
	mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0001, 0x0000);

	// step6: start output audio stream(differential signal)
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00, 0x08, 0x08);

	//utopia step7: when audio part is ready
	earc_atop_aupll(playback->sample_rate, playback->channel_mode);

	// step8: clear AVMUTE in channel status.
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG18, 0x04, 0x00);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);//update
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00);
	return 0;
}

static int dma_reader_stop(unsigned int id,
	struct mtk_runtime_struct *dev_runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG, 0x07, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG+1, 0x80, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x40, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0xA0, 0xA0);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);
		mtk_alsa_delaytask(1);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x40);
		mtk_alsa_delaytask(5);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);
		/* reset PCM playback engine */
		mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0003, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA5_WPTR, 0x0000);
		break;
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG, 0x07, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG+1, 0x80, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x40, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS3, 0xF0, 0x40);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0xA0, 0xA0);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x10, 0x00);
		mtk_alsa_delaytask(1);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x10, 0x10);
		mtk_alsa_delaytask(5);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x10, 0x00);
		/* reset PCM playback engine */
		mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0003, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA6_WPTR, 0x0000);
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		// utopia step1: set AVMUTE bit in channel status
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG18, 0x04, 0x04);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);
		mtk_alsa_delaytask(1);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00);
		// stop HDMI differential clock setting
		mtk_alsa_delaytask(10);
		earc_atop_aupll(0, 0);
		mtk_alsa_delaytask(10);
		// utopia step2: stop output audio stream(differential signal)
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_TX00, 0x08, 0x00);
		mtk_alsa_delaytask(125);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10+1, 0x70, 0x00);
		//[00]REG_MIXER_SOURCE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x01, 0x00);
		//[01]REG_CHANNEL_STATUS_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x02, 0x00);
		//[02]REG_USER_STATUS_CRC_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x04, 0x00);
		//[03]REG_USER_STATUS_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x08, 0x00);
		//[04]REG_VALID_BIT_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x10, 0x00);
		//[11]REG_RST_AFIFO_RIU
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x08);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x08, 0x00);
		//[05]REG_MIXER_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x20, 0x00);
		//[06]REG_MULTI2DOUBLE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x40, 0x00);
		//[07]REG_ECC_ENCODE_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02, 0x80, 0x00);
		//[08]REG_PARITY_BIT_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x01, 0x00);
		//[09]REG_IEC60958_BIPHASEMARK_EN
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x02, 0x00);

		//[12]REG_SWRSTZ_SOURCE
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x10, 0x00);
		//[13]REG_SWRSTZ_32XFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x20, 0x00);
		//[14]REG_SWRSTZ_32XCHXFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x40, 0x00);
		//[15]REG_SWRSTZ_64XCHXFS
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG02+1, 0x80, 0x00);
		mtk_alsa_delaytask(1);

		/* reset PCM playback engine */
		mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0003, 0x0001);
		/* clear PCM playback write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA7_WPTR, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA DPB] set wrong device stop\n");
		break;
	}

	/* reset read & write Pointer */
	playback->r_buf.r_ptr = playback->r_buf.addr;
	playback->r_buf.w_ptr = playback->r_buf.addr;
	playback->r_buf.avail_size = 0;
	playback->r_buf.remain_size = 0;
	playback->r_buf.total_read_size = 0;
	playback->r_buf.total_write_size = 0;

	/* reset copy size */
	playback->copy_size = 0;

	playback->status = CMD_STOP;

	#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	//ARC pin control
	if (id == ARC_PCM_PLAYBACK ||  id == ARC_NONPCM_PLAYBACK)
		hdmi_arc_pin_control(playback->status);
	#endif
	return 0;
}


static int dplayback_show_channel_status(unsigned int id,
	struct mtk_runtime_struct *dev_runtime, unsigned int cs_log_level)
{
	unsigned int control_bit = 0;
	unsigned int cs_pcm = 0;
	unsigned int sampling_freq = 0;
	unsigned int sampling_frequency = 0;
	unsigned int x_mit = 0;
	unsigned int channel_num = 0;
	unsigned int channel_alloc = 0;
	unsigned int mute = 0;
	unsigned int c_bit = 0;
	unsigned int l_bit = 0;
	unsigned int category_code = 0;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		// Control bit (channel status[5:0])
		control_bit = (mtk_alsa_read_reg_byte(REG.DPB_CS0) & 0x00fc);
		cs_pcm = ((control_bit & 0x40) >> 6);
		c_bit = ((control_bit & 0x20) >> 5);
		// Category code (channel status[14:8])
		category_code = (mtk_alsa_read_reg_byte(REG.DPB_CS1) & 0x00fe);
		// L bit (channel status[15])
		l_bit = (mtk_alsa_read_reg_byte(REG.DPB_CS1) & 0x0001);
		// Sampling frequency (channel status[27:24])
		sampling_freq = (mtk_alsa_read_reg_byte(REG.DPB_CS3) & 0x00f0);
		// Transmitted bit (Always 2CH)
		channel_num = CH_2;
		break;

	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		// Control bit (IEC60985 channel status[5:0])
		control_bit = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27) & 0x003f);
		cs_pcm = ((control_bit & 0x2) >> 1);
		c_bit = ((control_bit & 0x4) >> 2);
		// Category code (channel status[14:8])
		category_code = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27 + 1) & 0x007f);
		// L bit (channel status[15])
		l_bit = ((mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27 + 1) & 0x0080) >> 7);
		// Sampling frequency (IEC60985 channel status[27:24])
		sampling_freq = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG26 + 1) & 0x000f);
		// Transmitted bit (IEC60985 channel status[47:44])
		x_mit = ((mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG25 + 1) & 0x00f0) >> 4);
		// Channel Allocation (IEC60985 channel status[143:136])
		channel_alloc = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG19 + 1) & 0x00ff);
		// Mute (IEC60985 channel status[146])
		mute = ((mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG18) & 0x0004) >> 2);
		break;

	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA DPB] set wrong device\n");
		break;
	}

	// Sampling frequency
	if ((sampling_freq == 0x03) || (sampling_freq == 0xc0))
		sampling_frequency = RATE_32000;
	else if (sampling_freq == 0x00)
		sampling_frequency = RATE_44100;
	else if ((sampling_freq == 0x02) || (sampling_freq == 0x40))
		sampling_frequency = RATE_48000;
	else if ((sampling_freq == 0x0a) || (sampling_freq == 0x50))
		sampling_frequency = RATE_96000;
	else if ((sampling_freq == 0x0c) || (sampling_freq == 0x30))
		sampling_frequency = RATE_176400;
	else if ((sampling_freq == 0x0e) || (sampling_freq == 0x70))
		sampling_frequency = RATE_192000;
	else if ((sampling_freq == 0x09) || (sampling_freq == 0x60))
		sampling_frequency = RATE_768000;
	else
		apr_err(dev_runtime->log_level,
			"[ALSA DPB]%sUnknow frequency %x\n",
			DPLAYBACK_ID_STRING[id], sampling_freq);

	// Transmitted bit
	if (x_mit == 0x0)
		channel_num = CH_2;
	else if (x_mit == 0x7)
		channel_num = CH_8;
	else
		apr_err(dev_runtime->log_level,
			"[ALSA DPB]%sUnknow CH mode %x\n",
			DPLAYBACK_ID_STRING[id], x_mit);

	apr_err(cs_log_level,
		"[ALSA DPB]%s: Channel Status: **********************\n"
		"%u Hz %s, %uch layout, %s\n"
		"Channel Status; Control Bits: 0x%02x Sampling Freq: 0x%02x\n"
		"X-Mit: 0x%02x Ch-Alloc: 0x%02x Mute: %u C-bit: %u L-bit: %u Category: 0x%02x\n",
		DPLAYBACK_ID_STRING[id], sampling_frequency, cs_pcm ? "NPCM":"LPCM", channel_num,
		mute ? "muted":"unmuted", control_bit, sampling_freq,
		x_mit, channel_alloc, mute, c_bit, l_bit, category_code);

	return 0;
}

static void dma_reader_set_hw_param(struct mtk_runtime_struct *dev_runtime,
		struct snd_pcm_runtime *runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	apr_info(dev_runtime->log_level,
		"[ALSA DPB]digital target channel mode is %u\n", runtime->channels);

	playback->channel_mode = runtime->channels;
	playback->sample_rate = runtime->rate;
	playback->format = runtime->format;
}

static int dma_reader_get_inused_lines(unsigned int id)
{
	int inused_lines = 0;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0008, 0x0008);
		inused_lines = mtk_alsa_read_reg(REG.DPB_DMA5_DDR_LEVEL);
		mtk_alsa_write_reg_mask(REG.DPB_DMA5_CTRL, 0x0008, 0x0000);
		break;
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0008, 0x0008);
		inused_lines = mtk_alsa_read_reg(REG.DPB_DMA6_DDR_LEVEL);
		mtk_alsa_write_reg_mask(REG.DPB_DMA6_CTRL, 0x0008, 0x0000);
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0008, 0x0008);
		inused_lines = mtk_alsa_read_reg(REG.DPB_DMA7_DDR_LEVEL);
		mtk_alsa_write_reg_mask(REG.DPB_DMA7_CTRL, 0x0008, 0x0000);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA DPB] get wrong device level\n");
		break;
	}
	return inused_lines;
}

static void dma_reader_get_avail_bytes(unsigned int id,
			struct mtk_runtime_struct *dev_runtime)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	int inused_lines = 0;
	int avail_lines = 0;
	int avail_bytes = 0;

	inused_lines = dma_reader_get_inused_lines(id);
	avail_lines = (playback->r_buf.size / BYTES_IN_MIU_LINE) -
					inused_lines;

	if (avail_lines < 0) {
		apr_err(dev_runtime->log_level,
			"[ALSA DPB]%sincorrect inused line %d\n",
			DPLAYBACK_ID_STRING[id], inused_lines);
		avail_lines = 0;
	}

	avail_bytes = avail_lines * BYTES_IN_MIU_LINE;

	playback->r_buf.avail_size = avail_bytes;
}

static int dma_reader_get_consumed_bytes
	(struct snd_pcm_runtime *runtime, struct mtk_dma_struct *playback, unsigned int id)
{
	int inused_bytes = 0;
	int consumed_bytes = 0;

	inused_bytes = dma_reader_get_inused_lines(id) * BYTES_IN_MIU_LINE;

	if (!frame_aligned(runtime, inused_bytes))
		AUDIO_DO_ALIGNMENT(inused_bytes, runtime->byte_align);

	consumed_bytes = playback->copy_size - inused_bytes;
	if (consumed_bytes < 0)
		consumed_bytes = 0;
	else
		playback->copy_size = inused_bytes;

	return consumed_bytes;
}

static unsigned int dma_reader_write_inject(struct mtk_runtime_struct *dev_runtime,
					unsigned int samples)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned char *buffer_tmp;
	unsigned char byte_hi = 0;
	unsigned char byte_lo = 0;
	unsigned int copy_sample = samples;
	unsigned int frame_size = playback->channel_mode * playback->byte_length;
	int loop = 0;
	int i = 0;

	if ((dev_runtime->inject.r_ptr + sizeof(short)) > SINETONE_SIZE)
		dev_runtime->inject.r_ptr = 0;

	if ((playback->r_buf.w_ptr + frame_size) > (playback->r_buf.buf_end))
		playback->r_buf.w_ptr = playback->r_buf.addr;

	for (loop = 0; loop < copy_sample; loop++) {
		buffer_tmp = sinetone + dev_runtime->inject.r_ptr;
		if (buffer_tmp >= (sinetone + SINETONE_SIZE))
			buffer_tmp = sinetone;
		byte_lo = *(buffer_tmp++);
		if (buffer_tmp >= (sinetone + SINETONE_SIZE))
			buffer_tmp = sinetone;
		byte_hi = *(buffer_tmp++);
		for (i = 0; i < playback->channel_mode; i++) {
			if (playback->byte_length == 4) {
				*(playback->r_buf.w_ptr++) = byte_lo;
				*(playback->r_buf.w_ptr++) = byte_hi;
				*(playback->r_buf.w_ptr++) = 0;
				*(playback->r_buf.w_ptr++) = 0;
			} else {
				*(playback->r_buf.w_ptr++) = byte_lo;
				*(playback->r_buf.w_ptr++) = byte_hi;
			}
		}

		if (playback->r_buf.w_ptr >= playback->r_buf.buf_end)
			playback->r_buf.w_ptr = playback->r_buf.addr;

		dev_runtime->inject.r_ptr += sizeof(short);
		if (dev_runtime->inject.r_ptr >= SINETONE_SIZE)
			dev_runtime->inject.r_ptr = 0;
	}

	return copy_sample;
}

static unsigned int dma_reader_write(struct dplayback_priv *priv,
				unsigned int id, void *buffer, unsigned int bytes)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned int w_ptr_offset = 0;
	unsigned int copy_sample = 0;
	unsigned int copy_size = 0;
	unsigned int copy_tmp = 0;
	int inused_bytes = 0;

	copy_sample = bytes / (playback->channel_mode * playback->byte_length);
	copy_size = bytes;
	inused_bytes = dma_reader_get_inused_lines(id) * BYTES_IN_MIU_LINE;

	if ((inused_bytes == 0) && (playback->status == CMD_START))
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%s*****dplayback buffer empty !!*****\n",
			DPLAYBACK_ID_STRING[id]);

	if ((inused_bytes + copy_size + playback->r_buf.remain_size) > playback->high_threshold) {
		adev_info(priv->dev, dev_runtime->log_level,
			"*****PCM Playback near Buffer threshold !!*****\n");
		return 0;
	}

	if (dev_runtime->inject.status == 1) {	/* for inject debug mode */
		copy_sample = dma_reader_write_inject(dev_runtime, copy_sample);
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%s*****Output sinetone!!*****\n",
			DPLAYBACK_ID_STRING[id]);
	} else {

		do {
			copy_tmp = playback->r_buf.buf_end - playback->r_buf.w_ptr;
			copy_tmp = (copy_tmp > copy_size) ? copy_size : copy_tmp;

			memcpy(playback->r_buf.w_ptr, buffer, copy_tmp);
			playback->r_buf.w_ptr += copy_tmp;
			buffer += copy_tmp;

			if (playback->r_buf.w_ptr == playback->r_buf.buf_end)
				playback->r_buf.w_ptr = playback->r_buf.addr;

			copy_size -= copy_tmp;
		} while (copy_size > 0);

	}

	/* flush MIU */
	smp_mb();

	/* update PCM playback write pointer */
	w_ptr_offset = playback->r_buf.w_ptr - playback->r_buf.addr;
	w_ptr_offset += playback->r_buf.remain_size;
	playback->r_buf.remain_size = w_ptr_offset % BYTES_IN_MIU_LINE;
	AUDIO_DO_ALIGNMENT(w_ptr_offset, BYTES_IN_MIU_LINE);
	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case SPDIF_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA5_WPTR, 0xFFFF,
				(w_ptr_offset / BYTES_IN_MIU_LINE));
		break;
	case ARC_PCM_PLAYBACK:
	case ARC_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA6_WPTR, 0xFFFF,
				(w_ptr_offset / BYTES_IN_MIU_LINE));
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		mtk_alsa_write_reg_mask(REG.DPB_DMA7_WPTR, 0xFFFF,
				(w_ptr_offset / BYTES_IN_MIU_LINE));
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB] wrong device write\n");
		break;
	}
	playback->copy_size += bytes;
	playback->status = CMD_START;
	/* ensure write pointer can be applied */
	mtk_alsa_delaytask(1);

	return bytes;
}

static int digital_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	int ret;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	/* Set specified information */
	dev_runtime->substreams.p_substream = substream;
	dev_runtime->device_cycle_count = mtk_alsa_get_hw_reboot_cycle();

	snd_soc_set_runtime_hwparams(substream, &dplayback_default_hw);

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
	case ARC_PCM_PLAYBACK:
	case EARC_PCM_PLAYBACK:
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sset constraint rate list\n", DPLAYBACK_ID_STRING[id]);
		ret = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_dpb_pcm_rates);
		break;
	case SPDIF_NONPCM_PLAYBACK:
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sset constraint rate list\n", DPLAYBACK_ID_STRING[id]);
		ret = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_spdif_nonpcm_rates);
		break;
	case ARC_NONPCM_PLAYBACK:
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sset constraint rate list\n", DPLAYBACK_ID_STRING[id]);
		ret = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_arc_nonpcm_rates);
		break;
	case EARC_NONPCM_PLAYBACK:
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sset constraint rate list\n", DPLAYBACK_ID_STRING[id]);
		ret = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &constraints_earc_nonpcm_rates);
		break;
	default:
		ret = -1;
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB] set wrong device hardware rate\n");
		break;
	}
	if (ret < 0)
		return ret;

	ret = snd_pcm_hw_constraint_list(runtime, 0,
		SNDRV_PCM_HW_PARAM_CHANNELS, &constraints_dpb_channels);
	if (ret < 0)
		return ret;

	if (playback->c_buf.addr == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB] %s[%d] id = %u c_buf is NULL\n",
			__func__, __LINE__, id);
		return -ENOMEM;
	}

	memset((void *)playback->c_buf.addr, 0x00, MAX_DIGITAL_BUF_SIZE);
	playback->c_buf.size = MAX_DIGITAL_BUF_SIZE;

	if (spinlock->init_status == 0) {
		spin_lock_init(&spinlock->lock);
		spinlock->init_status = 1;
	}

	timer_open(&dev_runtime->timer, (void *)timer_monitor);

	return 0;
}

static void digital_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned long flags;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	/* Reset some variables */
	dev_runtime->substreams.substream_status = CMD_STOP;
	dev_runtime->substreams.p_substream = NULL;
	dev_runtime->device_cycle_count = 0;

	timer_close(&dev_runtime->timer);

	spin_lock_irqsave(&spinlock->lock, flags);
	memset(&dev_runtime->monitor, 0x00,
			sizeof(struct mtk_monitor_struct));
	spin_unlock_irqrestore(&spinlock->lock, flags);

	/* Reset PCM configurations */
	playback->sample_rate = 0;
	playback->channel_mode = 0;
	playback->status = CMD_STOP;

	/* Stop DMA Reader  */
	dma_reader_stop(id, dev_runtime);
	mtk_alsa_delaytask(2);

	/* Close MTK Audio DSP */
	dma_reader_exit(id, dev_runtime);
	mtk_alsa_delaytask(2);
}

static int digital_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int digital_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int spdif_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	/*print hw parameter*/
	adev_info(priv->dev, dev_runtime->log_level, "%sparameter :\n"
								"format is %d\n"
								"rate is %d\n"
								"channels is %d\n"
								"sample_bits is %d\n",
								DPLAYBACK_ID_STRING[id],
								runtime->format,
								runtime->rate,
								runtime->channels,
								runtime->sample_bits);

	if (playback->status != CMD_START) {
		/* Configure SW DMA sample rate */
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%s target sample rate is %u\n",
			DPLAYBACK_ID_STRING[id], runtime->rate);
		/* Configure SW DMA channel mode */
		dma_reader_set_hw_param(dev_runtime, runtime);

		if (id == SPDIF_NONPCM_PLAYBACK)
			spdif_nonpcm_set_synthesizer(runtime->rate);
		else
			mtk_alsa_write_reg_mask_byte(REG.DPB_SPDIF_CFG, 0x07, 0x00);

		/* Init SW DMA */
		dma_reader_init(priv, runtime, id);

		/* reset SW DMA engine */
		mtk_alsa_write_reg(REG.DPB_DMA5_CTRL, 0x0001);

		if (playback->format == SNDRV_PCM_FORMAT_S24_LE) {
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA5_CTRL, 0x30, 0x30);
			playback->byte_length = 4;
		} else {
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA5_CTRL, 0x30, 0x00);
			playback->byte_length = 2;
		}

		/* clear SW DMA write pointer */
		mtk_alsa_write_reg(REG.DPB_DMA5_WPTR, 0x0000);

		/* Start SW DMA */
		spdif_dma_start(id, dev_runtime);
		/* Configure SW DMA device status */
		playback->status = CMD_START;

		/* Show Channel Status */
		dplayback_show_channel_status(id, dev_runtime, priv->cs_log_level);
	} else {
		/* Stop SW DMA */
		dma_reader_stop(id, dev_runtime);
		mtk_alsa_delaytask(2);
		/* Start SW DMA */
		spdif_dma_start(id, dev_runtime);
	}
	return 0;
}

static int arc_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	/*print hw parameter*/
	adev_info(priv->dev, dev_runtime->log_level, "%sparameter :\n"
								"format is %d\n"
								"rate is %d\n"
								"channels is %d\n"
								"sample_bits is %d\n",
								DPLAYBACK_ID_STRING[id],
								runtime->format,
								runtime->rate,
								runtime->channels,
								runtime->sample_bits);

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	if (playback->status != CMD_START) {
		/* Configure SW DMA sample rate */
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%s target sample rate is %u\n",
			DPLAYBACK_ID_STRING[id], runtime->rate);
		/* Configure SW DMA channel mode */
		dma_reader_set_hw_param(dev_runtime, runtime);

		if (priv->ip_version == 1) {
			//m6L arc data from earc ip
			/* Stop SW DMA */
			//utopia step1,2,3: StopSequence
			dma_reader_stop(id, dev_runtime);

			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG51, 0x10, 0x10);

			earc_nonpcm_set_synthesizer(runtime->rate);

			//utopia step4: SetSequence
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10+1, 0x70, 0x00);
			//[14]REG_SWRSTZ_64XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x00);
			//[14]REG_SWRSTZ_32XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x00);
			//[06]REG_SWRSTZ_32XFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x00);
			mtk_alsa_delaytask(1);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x40);

			//[2:0]REG_EARC_TX_CH
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x07, 0x00);
			//[2:0]REG_EARC_TX_MIXER_CH
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x07, 0x00);
			//[6:4]REG_EARC_TX_MULTI2DOUBLE_CH
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x70, 0x00);
			//[10:8]REG_EARC_TX_USER_STATUS_CH
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x07, 0x00);
			//[14:12]REG_EARC_TX_CHANNEL_STATUS_CH
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x70, 0x00);

			//[12:8]REG_64XCHXFS_DFS_DIV
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x1F, 0x01);
			//[4:0]REG_32XFS_DFS_DIV
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x1F, 0x0F);
			//[12:8]REG_32XCHXFS_DFS_DIV
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x1F, 0x0F);

			//[13]REG_64XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x20);
			//[05]REG_32XFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x20);
			//[13]REG_32XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x20);

			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x00);

			//[04]REG_EARC_TX_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x10, 0x00);
			//[07]REG_EARC_TX_MIXER_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x80, 0x00);
			//[08]REG_EARC_TX_ECC_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08+1, 0x01, 0x00);
			//[05]REG_USER_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x20, 0x00);
			//[06]REG_CHANNEL_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x40, 0x00);

			//[04]REG_EARC_TX_CFG_DATA_SEL = 1'b1
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG03, 0x10, 0x10);

			//[06]REG_CHANNEL_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x40, 0x40);

			//[03]REG_EARC_TX_VALID_BIT_SEL
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG07, 0x08, 0x00);
			//[09]REG_EARC_TX_VALID_BIT_IN
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08+1, 0x02, 0x00);
		} else {
			//m6 arc ip from arc ip
			if (id == ARC_NONPCM_PLAYBACK)
				arc_nonpcm_set_synthesizer(runtime->rate);
			else
				mtk_alsa_write_reg_mask_byte(REG.DPB_ARC_CFG, 0x07, 0x00);
		}

		/* Init SW DMA */
		dma_reader_init(priv, runtime, id);

		/* reset SW DMA engine */
		mtk_alsa_write_reg(REG_CTRL, 0x0001);

		if (playback->format == SNDRV_PCM_FORMAT_S24_LE) {
			mtk_alsa_write_reg_mask_byte(REG_CTRL, 0x30, 0x30);
			playback->byte_length = 4;
		} else {
			mtk_alsa_write_reg_mask_byte(REG_CTRL, 0x30, 0x00);
			playback->byte_length = 2;
		}

		/* clear SW DMA write pointer */
		mtk_alsa_write_reg(REG_WPTR, 0x0000);

		/* Start SW DMA */
		arc_dma_start(id, dev_runtime, priv->ip_version);
		/* Configure SW DMA device status */
		playback->status = CMD_START;

		/* Show Channel Status */
		dplayback_show_channel_status(id, dev_runtime, priv->cs_log_level);
	} else {
		/* Stop SW DMA */
		dma_reader_stop(id, dev_runtime);
		mtk_alsa_delaytask(2);
		/* Start SW DMA */
		arc_dma_start(id, dev_runtime, priv->ip_version);
	}

	#ifndef CONFIG_MSTAR_ARM_BD_FPGA
	//ARC pin control
	hdmi_arc_pin_control(playback->status);
	#endif

	return 0;
}

static int earc_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	/*print hw parameter*/
	adev_info(priv->dev, dev_runtime->log_level, "%sparameter :\n"
								"format is %d\n"
								"rate is %d\n"
								"channels is %d\n"
								"sample_bits is %d\n",
								DPLAYBACK_ID_STRING[id],
								runtime->format,
								runtime->rate,
								runtime->channels,
								runtime->sample_bits);

	if (playback->status != CMD_START) {
		/* Configure SW DMA sample rate */
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%starget sample rate is %u\n",
			DPLAYBACK_ID_STRING[id], runtime->rate);
		/* Configure SW DMA channel mode */
		dma_reader_set_hw_param(dev_runtime, runtime);
		//hdmi rx bypass clock
		if (priv->hdmi_rx_bypass_flag == 1) {
			// 0x951168[08] = 1 //iir synthesizer EN
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG52+1, 0x01, 0x01);
		}
		/* Stop SW DMA */
		//utopia step1,2,3: StopSequence
		dma_reader_stop(id, dev_runtime);

		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG51, 0x10, 0x10);

		earc_nonpcm_set_synthesizer(runtime->rate);

		//utopia step4: SetSequence
		if (id == EARC_NONPCM_PLAYBACK) {
			//[14]REG_SWRSTZ_64XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x00);
			//[14]REG_SWRSTZ_32XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x00);
			//[06]REG_SWRSTZ_32XFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x00);
			mtk_alsa_delaytask(1);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x40);
			if (playback->channel_mode == CH_2) {
				//[2:0]REG_EARC_TX_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x07, 0x00);
				//[2:0]REG_EARC_TX_MIXER_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x07, 0x00);
				//[6:4]REG_EARC_TX_MULTI2DOUBLE_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x70, 0x00);
				//[10:8]REG_EARC_TX_USER_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x07, 0x00);
				//[14:12]REG_EARC_TX_CHANNEL_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x70, 0x00);
				//[12:8]REG_64XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x1F, 0x01);
				//[4:0]REG_32XFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x1F, 0x0F);
				//[12:8]REG_32XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x1F, 0x0F);
			} else if (playback->channel_mode == CH_8) {
				//[2:0]REG_EARC_TX_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x07, 0x01);
				//[2:0]REG_EARC_TX_MIXER_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x07, 0x01);
				//[6:4]REG_EARC_TX_MULTI2DOUBLE_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x70, 0x10);
				//[10:8]REG_EARC_TX_USER_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x07, 0x01);
				//[14:12]REG_EARC_TX_CHANNEL_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x70, 0x10);
				//[12:8]REG_64XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x1F, 0x07);
				//[4:0]REG_32XFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x1F, 0x0F);
				//[12:8]REG_32XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x1F, 0x03);
			}
			//[13]REG_64XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x20);
			//[05]REG_32XFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x20);
			//[13]REG_32XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x20);

			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x00);

			//[04]REG_EARC_TX_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x10, 0x10);

			//[04]REG_EARC_TX_CFG_DATA_SEL = 1'b1
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG03, 0x10, 0x10);

			//[06]REG_CHANNEL_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x40, 0x40);

			earc_channel_status(dev_runtime, id);

			//[03]REG_EARC_TX_VALID_BIT_SEL
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG07, 0x08, 0x08);
			//[09]REG_EARC_TX_VALID_BIT_IN
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08+1, 0x02, 0x02);
		} else {
			//mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG10+1, 0x70, 0x00);
			//[14]REG_SWRSTZ_64XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x00);
			//[14]REG_SWRSTZ_32XCHXFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x00);
			//[06]REG_SWRSTZ_32XFS_DFS
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x00);
			mtk_alsa_delaytask(1);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x40, 0x40);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x40, 0x40);

			if (playback->channel_mode == CH_2) {
				//[2:0]REG_EARC_TX_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x07, 0x00);
				//[2:0]REG_EARC_TX_MIXER_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x07, 0x00);
				//[6:4]REG_EARC_TX_MULTI2DOUBLE_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x70, 0x00);
				//[10:8]REG_EARC_TX_USER_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x07, 0x00);
				//[14:12]REG_EARC_TX_CHANNEL_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x70, 0x00);

				//[12:8]REG_64XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x1F, 0x01);
				//[4:0]REG_32XFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x1F, 0x0F);
				//[12:8]REG_32XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x1F, 0x0F);
			} else if (playback->channel_mode == CH_8) {  //multi ch pcm 8ch
				//[2:0]REG_EARC_TX_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x07, 0x01);
				//[2:0]REG_EARC_TX_MIXER_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x07, 0x01);
				//[6:4]REG_EARC_TX_MULTI2DOUBLE_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09, 0x70, 0x10);
				//[10:8]REG_EARC_TX_USER_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x07, 0x01);
				//[14:12]REG_EARC_TX_CHANNEL_STATUS_CH
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG09+1, 0x70, 0x10);
				//[12:8]REG_64XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x1F, 0x07);
				//[4:0]REG_32XFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x1F, 0x0F);
				//[12:8]REG_32XCHXFS_DFS_DIV
				mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x1F, 0x03);
			}

			//[13]REG_64XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x20);
			//[05]REG_32XFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x20);
			//[13]REG_32XCHXFS_DFS_UPDATE
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x20);

			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG00+1, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01, 0x20, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG01+1, 0x20, 0x00);

			//[04]REG_EARC_TX_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x10, 0x00);
			//[07]REG_EARC_TX_MIXER_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x80, 0x00);
			//[08]REG_EARC_TX_ECC_NPCM
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08+1, 0x01, 0x00);
			//[05]REG_USER_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x20, 0x00);
			//[06]REG_CHANNEL_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x40, 0x00);

			earc_channel_status(dev_runtime, id);

			//[04]REG_EARC_TX_CFG_DATA_SEL = 1'b1
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG03, 0x10, 0x10);

			//[06]REG_CHANNEL_STATUS_ARC
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08, 0x40, 0x40);

			//[03]REG_EARC_TX_VALID_BIT_SEL
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG07, 0x08, 0x00);
			//[09]REG_EARC_TX_VALID_BIT_IN
			mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG08+1, 0x02, 0x00);
		}
		/* Init SW DMA */
		dma_reader_init(priv, runtime, id);
		/* reset SW DMA engine */
		mtk_alsa_write_reg(REG.DPB_DMA7_CTRL, 0x0001);

		if (playback->channel_mode == CH_8)
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA7_CTRL, 0xC0, 0x80);
		else
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA7_CTRL, 0xC0, 0x00);

		if (playback->format == SNDRV_PCM_FORMAT_S24_LE) {
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA7_CTRL, 0x30, 0x30);
			playback->byte_length = 4;
		} else {
			mtk_alsa_write_reg_mask_byte(REG.DPB_DMA7_CTRL, 0x30, 0x00);
			playback->byte_length = 2;
		}

		mtk_alsa_write_reg(REG.DPB_DMA7_WPTR, 0x0000);

		/* Start SW DMA */
		//utopia step5,6,7,8: PlaySequence
		earc_dma_start(id, dev_runtime);
		/* Configure SW DMA device status */
		playback->status = CMD_START;

		/* Show Channel Status */
		dplayback_show_channel_status(id, dev_runtime, priv->cs_log_level);
	} else {
		/* Stop SW DMA */
		dma_reader_stop(id, dev_runtime);
		mtk_alsa_delaytask(2);
		/* Start SW DMA */
		earc_dma_start(id, dev_runtime);
	}
	return 0;
}

static int digital_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int err = 0;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		DPLAYBACK_ID_STRING[id], __func__, __LINE__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_STOP:
		dev_runtime->substreams.substream_status = CMD_STOP;
		break;
	case SNDRV_PCM_TRIGGER_START:
		dev_runtime->substreams.substream_status = CMD_START;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev_runtime->substreams.substream_status = CMD_PAUSE;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev_runtime->substreams.substream_status = CMD_PAUSE_RELEASE;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		dev_runtime->substreams.substream_status = CMD_SUSPEND;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		dev_runtime->substreams.substream_status = CMD_RESUME;
		break;
	default:
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sdplayback invalid PB's trigger command %d\n",
			DPLAYBACK_ID_STRING[id], cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct snd_soc_dai_ops spdif_dai_ops = {
	.startup	= digital_startup,
	.shutdown	= digital_shutdown,
	.hw_params	= digital_hw_params,
	.hw_free	= digital_hw_free,
	.prepare	= spdif_prepare,
	.trigger	= digital_trigger,
};

static const struct snd_soc_dai_ops arc_dai_ops = {
	.startup	= digital_startup,
	.shutdown	= digital_shutdown,
	.hw_params	= digital_hw_params,
	.hw_free	= digital_hw_free,
	.prepare	= arc_prepare,
	.trigger	= digital_trigger,
};

static const struct snd_soc_dai_ops earc_dai_ops = {
	.startup	= digital_startup,
	.shutdown	= digital_shutdown,
	.hw_params	= digital_hw_params,
	.hw_free	= digital_hw_free,
	.prepare	= earc_prepare,
	.trigger	= digital_trigger,
};

int digital_dai_suspend(struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(priv->dev, dev_runtime->log_level,
		"[ALSA DPB]%sdevice suspend\n", DPLAYBACK_ID_STRING[id]);

	if (playback->status != CMD_STOP) {
		dma_reader_stop(id, dev_runtime);
		playback->status = CMD_SUSPEND;
		dev_runtime->device_cycle_count = 0;
	}

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 1) {
			mtk_spdif_tx_clk_set_disable(0);
			priv->dp_clk_status.spdif_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case SPDIF_NONPCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 2) {
			mtk_spdif_tx_clk_set_disable(1);
			priv->dp_clk_status.spdif_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_PCM_PLAYBACK:
		if (priv->ip_version == 1 && priv->dp_clk_status.earc_clk == 1) {
			mtk_earc_clk_set_disable(0);
			priv->dp_clk_status.earc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else if (priv->ip_version == 0 && priv->dp_clk_status.arc_clk == 1) {
			mtk_arc_clk_set_disable(0);
			priv->dp_clk_status.arc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_NONPCM_PLAYBACK:
		if (priv->ip_version == 1  && priv->dp_clk_status.earc_clk == 2) {
			mtk_earc_clk_set_disable(1);
			priv->dp_clk_status.earc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else if (priv->ip_version == 0 && priv->dp_clk_status.arc_clk == 2) {
			mtk_arc_clk_set_disable(1);
			priv->dp_clk_status.arc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		if (priv->hdmi_rx_bypass_flag == 1 && priv->dp_clk_status.earc_clk == 3) {
			mtk_earc_clk_set_disable(2);
			priv->dp_clk_status.earc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else if (priv->dp_clk_status.earc_clk == 2) {
			mtk_earc_clk_set_disable(1);
			priv->dp_clk_status.earc_clk = 0;
			dev_runtime->suspend_flag = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]invalid pcm device clock disable %u\n", id);
		break;
	}

	return 0;
}

int digital_dai_resume(struct snd_soc_dai *dai)
{
	struct dplayback_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(priv->dev, dev_runtime->log_level,
		"[ALSA DPB]%sdevice resume\n", DPLAYBACK_ID_STRING[id]);

	if (playback->status == CMD_SUSPEND)
		playback->status = CMD_RESUME;

	return 0;
}

static struct snd_soc_dai_driver dplayback_dais[] = {
	{
		.name = "SPDIF-TX",
		.id = SPDIF_PCM_PLAYBACK,
		.playback = {
			.stream_name = "SPDIF_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &spdif_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
	{
		.name = "SPDIF-TX-NONPCM",
		.id = SPDIF_NONPCM_PLAYBACK,
		.playback = {
			.stream_name = "SPDIF_NONPCM_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &spdif_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
	{
		.name = "ARC",
		.id = ARC_PCM_PLAYBACK,
		.playback = {
			.stream_name = "ARC_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &arc_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
	{
		.name = "ARC-NONPCM",
		.id = ARC_NONPCM_PLAYBACK,
		.playback = {
			.stream_name = "ARC_NONPCM_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
					SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
					SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &arc_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
	{
		.name = "EARC",
		.id = EARC_PCM_PLAYBACK,
		.playback = {
			.stream_name = "EARC_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |
					SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |
					SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &earc_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
	{
		.name = "EARC-NONPCM",
		.id = EARC_NONPCM_PLAYBACK,
		.playback = {
			.stream_name = "EARC_NONPCM_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
					SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |
					SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &earc_dai_ops,
		.suspend = digital_dai_suspend,
		.resume = digital_dai_resume,
	},
};

static int dplayback_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, DPLAYBACK_DMA_NAME);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int ret = 0;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 0) {
			mtk_spdif_tx_clk_set_enable(0);
			priv->dp_clk_status.spdif_clk = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sis busy", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case SPDIF_NONPCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 0) {
			mtk_spdif_tx_clk_set_enable(1);
			priv->dp_clk_status.spdif_clk = 2;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sis busy", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_PCM_PLAYBACK:
		if (priv->ip_version == 1 &&
			priv->dp_clk_status.earc_clk == 0) {
			mtk_earc_clk_set_enable(0);
			priv->dp_clk_status.earc_clk = 1;
		} else if (priv->ip_version == 0 &&
			priv->dp_clk_status.arc_clk == 0) {
			mtk_arc_clk_set_enable(0);
			priv->dp_clk_status.arc_clk = 1;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sis busy", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_NONPCM_PLAYBACK:
		if (priv->ip_version == 1 &&
			priv->dp_clk_status.earc_clk == 0) {
			mtk_earc_clk_set_enable(1);
			priv->dp_clk_status.earc_clk = 2;
		} else if (priv->ip_version == 0 &&
			priv->dp_clk_status.arc_clk == 0) {
			mtk_arc_clk_set_enable(1);
			priv->dp_clk_status.arc_clk = 2;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sis busy", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}

		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		if (priv->hdmi_rx_bypass_flag == 1 &&
			priv->dp_clk_status.earc_clk == 0) {  //hdmi rx bypass
			mtk_earc_clk_set_enable(2);
			priv->dp_clk_status.earc_clk = 3;
		} else if (priv->dp_clk_status.earc_clk == 0) {
			mtk_earc_clk_set_enable(1);
			priv->dp_clk_status.earc_clk = 2;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sis busy", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]invalid DPB device open %u\n", id);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static int dplayback_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, DPLAYBACK_DMA_NAME);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int ret = 0;

	switch (id) {
	case SPDIF_PCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 1) {
			mtk_spdif_tx_clk_set_disable(0);
			priv->dp_clk_status.spdif_clk = 0;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case SPDIF_NONPCM_PLAYBACK:
		if (priv->dp_clk_status.spdif_clk == 2) {
			mtk_spdif_tx_clk_set_disable(1);
			priv->dp_clk_status.spdif_clk = 0;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_PCM_PLAYBACK:
		if (priv->ip_version == 1 && priv->dp_clk_status.earc_clk == 1) {
			mtk_earc_clk_set_disable(0);
			priv->dp_clk_status.earc_clk = 0;
		} else if (priv->ip_version == 0 && priv->dp_clk_status.arc_clk == 1) {
			mtk_arc_clk_set_disable(0);
			priv->dp_clk_status.arc_clk = 0;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case ARC_NONPCM_PLAYBACK:
		if (priv->ip_version == 1  && priv->dp_clk_status.earc_clk == 2) {
			mtk_earc_clk_set_disable(1);
			priv->dp_clk_status.earc_clk = 0;
		} else if (priv->ip_version == 0 && priv->dp_clk_status.arc_clk == 2) {
			mtk_arc_clk_set_disable(1);
			priv->dp_clk_status.arc_clk = 0;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	case EARC_PCM_PLAYBACK:
	case EARC_NONPCM_PLAYBACK:
		if (priv->hdmi_rx_bypass_flag == 1 && priv->dp_clk_status.earc_clk == 3) {
			mtk_earc_clk_set_disable(2);
			priv->dp_clk_status.earc_clk = 0;
		} else if (priv->dp_clk_status.earc_clk == 2) {
			mtk_earc_clk_set_disable(1);
			priv->dp_clk_status.earc_clk = 0;
		} else {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%scan not close", DPLAYBACK_ID_STRING[id]);
			//return -EBUSY;
		}
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA DPB]invalid DPB device close %u\n", id);
		ret = -ENODEV;
		break;
	}
	dev_runtime->suspend_flag = 0;

	return ret;
}

static snd_pcm_uframes_t dplayback_pcm_pointer
				(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, DPLAYBACK_DMA_NAME);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int consumed_bytes = 0;
	snd_pcm_uframes_t consumed_pcm_samples = 0;
	snd_pcm_uframes_t new_hw_ptr = 0;
	snd_pcm_uframes_t new_hw_ptr_pos = 0;

	if (playback->status == CMD_START) {
		consumed_bytes = dma_reader_get_consumed_bytes(runtime, playback, id);
		consumed_pcm_samples = bytes_to_frames(runtime, consumed_bytes);
	}

	new_hw_ptr = runtime->status->hw_ptr + consumed_pcm_samples;
	new_hw_ptr_pos = new_hw_ptr % runtime->buffer_size;

	return new_hw_ptr_pos;
}

static int dplayback_pcm_copy(struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, DPLAYBACK_DMA_NAME);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned char *buffer_tmp = NULL;
	unsigned int request_size = 0;
	unsigned int copied_size = 0;
	unsigned int size_to_copy = 0;
	unsigned int size = 0;
	int retry_counter = 0;
	unsigned long flags;

	if (buf == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sErr! invalid memory address!\n",
			DPLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	if (bytes == 0) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sErr! request bytes is zero!\n",
			DPLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	if (mtk_alsa_get_hw_reboot_flag(dev_runtime->device_cycle_count)) {
		snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sErr! after reboot need re-open device!\n",
			DPLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	if (dev_runtime->suspend_flag == 1) {
		snd_pcm_stop(substream, SNDRV_PCM_STATE_SUSPENDED);
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sErr! after suspend need re-open device!\n",
			DPLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	buffer_tmp = playback->c_buf.addr;
	if (buffer_tmp == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%sErr! need to alloc system mem for PCM buf\n",
			DPLAYBACK_ID_STRING[id]);
		return -ENXIO;
	}

	/* Wake up Monitor if necessary */
	if (dev_runtime->monitor.monitor_status == CMD_STOP) {
		adev_info(priv->dev, dev_runtime->log_level,
			"Wake up Playback Monitor %d\n", substream->pcm->device);
		while (1) {
			if (timer_reset(&dev_runtime->timer) == 0) {
				spin_lock_irqsave(&spinlock->lock, flags);
				dev_runtime->monitor.monitor_status = CMD_START;
				spin_unlock_irqrestore(&spinlock->lock, flags);
				break;
			}

			if ((++retry_counter) > 50) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA DPB]%sfail to wakeup Monitor %d\n",
					DPLAYBACK_ID_STRING[id], substream->pcm->device);
				break;
			}
			mtk_alsa_delaytask(1);
		}
		retry_counter = 0;
	}

	request_size = bytes;

	dma_reader_get_avail_bytes(id, dev_runtime);

	if ((((substream->f_flags & O_NONBLOCK) > 0) &&
		(request_size > playback->r_buf.avail_size)) ||
		(request_size > playback->c_buf.size)) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA DPB]%s copy fail!\n"
			"request_size = %u, buf avail_size = %u, copy buffer size = %u!!!\n",
			DPLAYBACK_ID_STRING[id], request_size,
			playback->r_buf.avail_size, playback->c_buf.size);
		return -EAGAIN;
	}

	do {
		unsigned long receive_size = 0;

		size_to_copy = request_size - copied_size;

		/* Deliver PCM data */
		receive_size = copy_from_user(buffer_tmp, (buf + copied_size),
						size_to_copy);
		if (receive_size) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DPB]%sErr! %lu return byte\n",
				DPLAYBACK_ID_STRING[id], receive_size);
			size_to_copy -= receive_size;
		}

		size = dma_reader_write(priv, id, (void *)buffer_tmp, size_to_copy);
		if (size == 0) {
			if ((retry_counter % 50) == 0)
				adev_info(priv->dev, dev_runtime->log_level,
					"[ALSA DPB]%sRetry write PCM\n", DPLAYBACK_ID_STRING[id]);

			mtk_alsa_delaytask(1);

			if (retry_counter > 500) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA DPB]%scopy fail\n", DPLAYBACK_ID_STRING[id]);
				break;
			}
		}
		retry_counter += 1;
		copied_size += size;
	} while (copied_size < request_size);


	if (!size)
		return -EBUSY;
	else
		return 0;
}

const struct snd_pcm_ops dplayback_pcm_ops = {
	.open = dplayback_pcm_open,
	.close = dplayback_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = dplayback_pcm_pointer,
	.copy_user = dplayback_pcm_copy,
};

static int earc_control_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 16;

	return 0;
}

static int earc_get_param(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int earc_put_param(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int id = kcontrol->private_value;
	unsigned short temp_val = 0;
	int i;

	if (id == 0) {
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG19+1,
			ucontrol->value.bytes.data[3]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG18,
			ucontrol->value.bytes.data[4]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG18+1,
			ucontrol->value.bytes.data[5]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG17,
			ucontrol->value.bytes.data[6]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG17+1,
			ucontrol->value.bytes.data[7]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG16,
			ucontrol->value.bytes.data[8]);
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG16+1,
			ucontrol->value.bytes.data[9]);
	}

	if ((id == 4) || (id == 5) || (id == 6)) {
		mtk_alsa_write_reg_byte(REG.DPB_EARC_CFG34, id);
		mtk_alsa_write_reg(REG.DPB_EARC_CFG35, 0x0000);

		for (i = 0; i < 8; i++) {
			temp_val = (unsigned short)((ucontrol->value.bytes.data[i*2]<<8) |
				(ucontrol->value.bytes.data[(i*2)+1]&0xFF));
			mtk_alsa_write_reg((REG.DPB_EARC_CFG36 + (i*2)), temp_val);
		}
		//for (i = 0; i < 6; i++)
		//	mtk_alsa_write_reg((REG.DPB_EARC_CFG44 + (i*2)), 0x0000);
	}

	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG34+1, 0x01, 0x01);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG34+1, 0x01, 0x00);

	return 0;
}

static int earc_get_slt_param(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.bytes.data[1] = priv->slt_result;

	return 0;
}


static int earc_put_slt_param(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);

	if (ucontrol->value.bytes.data[0] == 1)
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_SINE_GEN+1, 0x20, 0x20);
	else
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_SINE_GEN+1, 0x20, 0x00);

	priv->slt_result = ucontrol->value.bytes.data[1];

	return 0;
}

// Cbit Lbit category CH alloc kcontrol
static int dplayback_cs_control_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 4;

	return 0;
}
static int dplayback_get_cs(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	ucontrol->value.integer.value[3] = 0x0;

	switch (kcontrol->private_value) {

	case 0: //spdif pcm
	case 1: //spdif nonpcm
	case 2: //arc pcm
	case 3: //arc nonpcm
		//C bit
		ucontrol->value.integer.value[0] =
		ret = ((mtk_alsa_read_reg_byte(REG.DPB_CS0) & 0x0020) >> 5);
		//L bit
		ucontrol->value.integer.value[1] =
		ret = (mtk_alsa_read_reg_byte(REG.DPB_CS1) & 0x0001);
		//Category bit
		ucontrol->value.integer.value[2] =
		ret = (mtk_alsa_read_reg_byte(REG.DPB_CS1) & 0x00fe);
		//Channel alloc
		ucontrol->value.integer.value[3] = 0x0;
		break;
	case 4:
	case 5:
		//C bit
		ucontrol->value.integer.value[0] =
		ret = ((mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27) & 0x0004) >> 2);
		//L bit
		ucontrol->value.integer.value[1] =
		ret = ((mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27+1) & 0x0080) >> 7);
		//Category bit
		ucontrol->value.integer.value[2] =
		ret = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG27+1) & 0x007f);
		//Channel alloc
		ucontrol->value.integer.value[3] =
		ret = (mtk_alsa_read_reg_byte(REG.DPB_EARC_CFG19+1) & 0x00FF);
		break;
	}

	return 0;
}

static int dplayback_put_cs(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->private_value;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	priv->C_bit_en = ucontrol->value.integer.value[0];
	priv->L_bit_en = ucontrol->value.integer.value[1];
	priv->category = ucontrol->value.integer.value[2];
	priv->ch_alloc = ucontrol->value.integer.value[3];

	switch (id) {
	case 0: //spdif pcm
	case 1: //spdif nonpcm
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x0020, priv->C_bit_en << 5);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x0001, priv->L_bit_en);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x00fe, priv->category);
		//update  spdif channel status
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);//Tx1 Toggle bit[14]
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x40);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);
		break;
	case 2: //arc pcm
	case 3: //arc nonpcm
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS0, 0x0020, priv->C_bit_en << 5);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x0001, priv->L_bit_en);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS1, 0x00fe, priv->category);
		//update  arc channel status
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00); //Tx2 Toggle bit[12]
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x10);
		mtk_alsa_write_reg_mask_byte(REG.DPB_CS_TOGGLE+1, 0x40, 0x00);
		break;
	case 4: //earc pcm
	case 5: //earc nonpcm
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27, 0x0004, priv->C_bit_en << 2);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x0080, priv->L_bit_en << 7);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG27+1, 0x007f, priv->category);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG19+1, 0xFF, priv->ch_alloc);
		//update  earc channel status
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00); //earc Toggle bit
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x01);
		mtk_alsa_delaytask(1);
		mtk_alsa_write_reg_mask_byte(REG.DPB_EARC_CFG29+1, 0x01, 0x00);
		break;
	}


	/* Show Channel Status */
	dplayback_show_channel_status(id, dev_runtime, priv->cs_log_level);

	return 0;
}

static int dplayback_hdmi_rx_bypass_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int dplayback_get_hdmi_rx_bypass(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)

{
	return 0;
}

static int dplayback_put_hdmi_rx_bypass(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct dplayback_priv *priv = snd_soc_component_get_drvdata(component);

	priv->hdmi_rx_bypass_flag = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new cs_controls[] = {
	SOC_CHAR_EARC("EARC INFOFRAME", 0),
	SOC_CHAR_EARC("EARC ACP", 4),
	SOC_CHAR_EARC("EARC ISRC1", 5),
	SOC_CHAR_EARC("EARC ISRC2", 6),
	SOC_EARC_SLT("EARC PLAYBACK SLT SINEGEN TX", EARC_PCM_PLAYBACK),
	SOC_CHAR_CS("SPDIF PLAYBACK CS", SPDIF_PCM_PLAYBACK),
	SOC_CHAR_CS("SPDIF NONPCM PLAYBACK CS", SPDIF_NONPCM_PLAYBACK),
	SOC_CHAR_CS("ARC PLAYBACK CS", ARC_PCM_PLAYBACK),
	SOC_CHAR_CS("ARC NONPCM PLAYBACK CS", ARC_NONPCM_PLAYBACK),
	SOC_CHAR_CS("EARC PLAYBACK CS", EARC_PCM_PLAYBACK),
	SOC_CHAR_CS("EARC NONPCM PLAYBACK CS", EARC_NONPCM_PLAYBACK),
	SOC_CHAR_EARC_HDMI_BYPASS("EARC PCM PLAYBACK HDMI RX BYPASS", EARC_PCM_PLAYBACK),
	SOC_CHAR_EARC_HDMI_BYPASS("EARC NONPCM PLAYBACK HDMI RX BYPASS", EARC_NONPCM_PLAYBACK),
};

static const struct snd_soc_component_driver dplayback_dai_component = {
	.name			= DPLAYBACK_DMA_NAME,
	.probe			= dplayback_probe,
	.remove			= dplayback_remove,
	.ops			= &dplayback_pcm_ops,
	.controls		= cs_controls,
	.num_controls		= ARRAY_SIZE(cs_controls),
};

unsigned int dpb_get_dts_u32(struct device_node *np, const char *name)
{
	unsigned int value;
	int ret;

	ret = of_property_read_u32(np, name, &value);
	if (ret)
		WARN_ON("[ALSA DPB]Can't get DTS property\n");

	return value;
}

static int parse_registers(void)
{
	struct device_node *np;

	memset(&REG, 0x0, sizeof(struct dplayback_register));

	np = of_find_node_by_name(NULL, "dplayback-register");
	if (!np)
		return -ENODEV;

	REG.DPB_DMA5_CTRL = dpb_get_dts_u32(np, "reg_dpb_dma5_ctrl");
	REG.DPB_DMA5_WPTR = dpb_get_dts_u32(np, "reg_dpb_dma5_wptr");
	REG.DPB_DMA5_DDR_LEVEL = dpb_get_dts_u32(np, "reg_dpb_dma5_ddr_level");
	REG.DPB_DMA6_CTRL = dpb_get_dts_u32(np, "reg_dpb_dma6_ctrl");
	REG.DPB_DMA6_WPTR = dpb_get_dts_u32(np, "reg_dpb_dma6_wptr");
	REG.DPB_DMA6_DDR_LEVEL = dpb_get_dts_u32(np, "reg_dpb_dma6_ddr_level");
	REG.DPB_DMA7_CTRL = dpb_get_dts_u32(np, "reg_dpb_dma7_ctrl");
	REG.DPB_DMA7_WPTR = dpb_get_dts_u32(np, "reg_dpb_dma7_wptr");
	REG.DPB_DMA7_DDR_LEVEL = dpb_get_dts_u32(np, "reg_dpb_dma7_ddr_level");

	REG.DPB_CS0 = dpb_get_dts_u32(np, "reg_digital_out_cs0");
	REG.DPB_CS1 = dpb_get_dts_u32(np, "reg_digital_out_cs1");
	REG.DPB_CS2 = dpb_get_dts_u32(np, "reg_digital_out_cs2");
	REG.DPB_CS3 = dpb_get_dts_u32(np, "reg_digital_out_cs3");
	REG.DPB_CS4 = dpb_get_dts_u32(np, "reg_digital_out_cs4");
	REG.DPB_SPDIF_CFG = dpb_get_dts_u32(np, "reg_digital_spdif_out_cfg");
	REG.DPB_ARC_CFG = dpb_get_dts_u32(np, "reg_digital_arc_out_cfg");
	REG.DPB_CS_TOGGLE = dpb_get_dts_u32(np, "reg_digital_out_cs_toggle");

	REG.DPB_SPDIF_SYNTH_H = dpb_get_dts_u32(np, "reg_digital_out_spdif_synth_h");
	REG.DPB_SPDIF_SYNTH_L = dpb_get_dts_u32(np, "reg_digital_out_spdif_synth_l");
	REG.DPB_ARC_SYNTH_H = dpb_get_dts_u32(np, "reg_digital_out_arc_synth_h");
	REG.DPB_ARC_SYNTH_L = dpb_get_dts_u32(np, "reg_digital_out_arc_synth_l");
	REG.DPB_SYNTH_CFG = dpb_get_dts_u32(np, "reg_digital_out_synth_cfg");

	REG.DPB_EARC_CFG00 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg00");
	REG.DPB_EARC_CFG01 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg01");
	REG.DPB_EARC_CFG02 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg02");
	REG.DPB_EARC_CFG03 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg03");
	REG.DPB_EARC_CFG04 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg04");
	REG.DPB_EARC_CFG05 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg05");
	REG.DPB_EARC_CFG06 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg06");
	REG.DPB_EARC_CFG07 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg07");
	REG.DPB_EARC_CFG08 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg08");
	REG.DPB_EARC_CFG09 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg09");
	REG.DPB_EARC_CFG10 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg10");
	REG.DPB_EARC_CFG11 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg11");
	REG.DPB_EARC_CFG12 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg12");
	REG.DPB_EARC_CFG13 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg13");
	REG.DPB_EARC_CFG14 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg14");
	REG.DPB_EARC_CFG15 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg15");
	REG.DPB_EARC_CFG16 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg16");
	REG.DPB_EARC_CFG17 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg17");
	REG.DPB_EARC_CFG18 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg18");
	REG.DPB_EARC_CFG19 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg19");
	REG.DPB_EARC_CFG20 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg20");
	REG.DPB_EARC_CFG21 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg21");
	REG.DPB_EARC_CFG22 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg22");
	REG.DPB_EARC_CFG23 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg23");
	REG.DPB_EARC_CFG24 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg24");
	REG.DPB_EARC_CFG25 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg25");
	REG.DPB_EARC_CFG26 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg26");
	REG.DPB_EARC_CFG27 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg27");
	REG.DPB_EARC_CFG28 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg28");
	REG.DPB_EARC_CFG29 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg29");
	REG.DPB_EARC_CFG30 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg30");
	REG.DPB_EARC_CFG31 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg31");
	REG.DPB_EARC_CFG32 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg32");
	REG.DPB_EARC_CFG33 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg33");
	REG.DPB_EARC_CFG34 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg34");
	REG.DPB_EARC_CFG35 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg35");
	REG.DPB_EARC_CFG36 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg36");
	REG.DPB_EARC_CFG37 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg37");
	REG.DPB_EARC_CFG38 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg38");
	REG.DPB_EARC_CFG39 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg39");
	REG.DPB_EARC_CFG40 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg40");
	REG.DPB_EARC_CFG41 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg41");
	REG.DPB_EARC_CFG42 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg42");
	REG.DPB_EARC_CFG43 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg43");
	REG.DPB_EARC_CFG44 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg44");
	REG.DPB_EARC_CFG45 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg45");
	REG.DPB_EARC_CFG46 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg46");
	REG.DPB_EARC_CFG47 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg47");
	REG.DPB_EARC_CFG48 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg48");
	REG.DPB_EARC_CFG49 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg49");
	REG.DPB_EARC_CFG50 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg50");
	REG.DPB_EARC_CFG51 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg51");
	REG.DPB_EARC_CFG52 = dpb_get_dts_u32(np, "reg_digital_earc_out_cfg52");
	REG.DPB_EARC_TX00 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx00");
	REG.DPB_EARC_TX18 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx18");
	REG.DPB_EARC_TX20 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx20");
	REG.DPB_EARC_TX24 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx24");
	REG.DPB_EARC_TX28 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx28");
	REG.DPB_EARC_TX2C = dpb_get_dts_u32(np, "reg_digital_earc_out_tx2C");
	REG.DPB_EARC_TX30 = dpb_get_dts_u32(np, "reg_digital_earc_out_tx30");
	REG.DPB_EARC_SINE_GEN = dpb_get_dts_u32(np, "reg_dpb_earc_sine_gen");

	return 0;
}

static int parse_mmap(struct device_node *np,
				struct dplayback_priv *priv)
{
	struct of_mmap_info_data of_mmap_info = {};
	int ret;

	ret = of_mtk_get_reserved_memory_info_by_idx(np,
						0, &of_mmap_info);
	if (ret) {
		pr_err("[ALSA DPB]get audio mmap fail\n");
		return -EINVAL;
	}

	priv->mem_info.miu = of_mmap_info.miu;
	priv->mem_info.bus_addr = of_mmap_info.start_bus_address;
	priv->mem_info.buffer_size = of_mmap_info.buffer_size;

	return 0;
}

int parse_memory(struct device_node *np, struct dplayback_priv *priv)
{
	int ret;

	ret = of_property_read_u32(np, "spdif_dma_offset",
					&priv->play_dma_offset[SPDIF]);
	if (ret) {
		pr_err("[ALSA DPB]can't get spdif dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "spdif_dma_size",
					&priv->play_dma_size[SPDIF]);
	if (ret) {
		pr_err("[ALSA DPB]can't get spdif dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "arc_dma_offset",
					&priv->play_dma_offset[ARC]);
	if (ret) {
		pr_err("[ALSA DPB]can't get arc dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "arc_dma_size",
					&priv->play_dma_size[ARC]);
	if (ret) {
		pr_err("[ALSA DPB]can't get arc dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "earc_dma_offset",
					&priv->play_dma_offset[EARC]);
	if (ret) {
		pr_err("[ALSA DPB]can't get earc dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "earc_dma_size",
					&priv->play_dma_size[EARC]);
	if (ret) {
		pr_err("[ALSA DPB]can't get earc dma size\n");
		return -EINVAL;
	}

	return 0;
}

int dplayback_dev_probe(struct platform_device *pdev)
{
	struct dplayback_priv *priv;
	struct device *dev = &pdev->dev;
	struct device_node *node;
	int ret, i;

	node = dev->of_node;
	if (!node)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;

	for (i = 0; i < DPLAYBACK_DEV_NUM; i++) {
		priv->pcm_device[i] = devm_kzalloc(&pdev->dev,
				sizeof(struct mtk_runtime_struct), GFP_KERNEL);
		/* Allocate system memory for Specific Copy */
		priv->pcm_device[i]->playback.c_buf.addr = vmalloc(MAX_DIGITAL_BUF_SIZE);
		if ((!priv->pcm_device[i]) ||
			(!priv->pcm_device[i]->playback.c_buf.addr))
			return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);

	ret = devm_snd_soc_register_component(&pdev->dev,
					&dplayback_dai_component,
					dplayback_dais,
					ARRAY_SIZE(dplayback_dais));
	if (ret) {
		dev_err(dev, "[ALSA DPB]soc_register_component fail %d\n", ret);
		return ret;
	}

	/* parse dts */
	ret = of_property_read_u64(node, "memory_bus_base",
					&priv->mem_info.memory_bus_base);
	if (ret) {
		dev_err(dev, "[ALSA DPB]can't get miu bus base\n");
		return -EINVAL;
	}

	/* parse registers */
	ret = parse_registers();
	if (ret) {
		dev_err(dev, "[ALSA DPB]parse register fail %d\n", ret);
		return ret;
	}

	/* parse dma memory info */
	ret = parse_memory(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA DPB]parse memory fail %d\n", ret);
		return ret;
	}
	/* parse mmap */
	ret = parse_mmap(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA DPB]parse mmap fail %d\n", ret);
		return ret;
	}

	return 0;
}

int dplayback_dev_remove(struct platform_device *pdev)
{
	struct dplayback_priv *priv;
	struct device *dev = &pdev->dev;
	int i;

	if (dev == NULL) {
		dev_err(dev, "[AUDIO][ERROR][%s]dev can't be NULL!!!\n", __func__);
		return 0;
	}

	priv = dev_get_drvdata(dev);
	if (priv == NULL) {
		dev_err(dev, "[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return 0;
	}

	for (i = 0; i < DPLAYBACK_DEV_NUM; i++) {
		/* Free allocated memory */
		if (priv->pcm_device[i]->playback.c_buf.addr != NULL) {
			vfree(priv->pcm_device[i]->playback.c_buf.addr);
			priv->pcm_device[i]->playback.c_buf.addr = NULL;
		}
	}
	return 0;
}

MODULE_DESCRIPTION("Mediatek ALSA SoC DPlayback DMA platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dplayback");
