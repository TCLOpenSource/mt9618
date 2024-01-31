// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_alsa_playback_platform.c  --  Mediatek DTV playback function
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mtk_alsa_chip.h"
#include "mtk_alsa_playback_platform.h"
#include "mtk-reserved-memory.h"

#define PLAYBACK_DMA_NAME		"snd-output-pcm-dai"

#define MAX_PCM_BUF_SIZE	(48*1024)

static const char * const PLAYBACK_ID_STRING[] = {"[SPEAKER_PLAYBACK]",
						"[HEADPHONE_PLAYBACK]",
						"[LINEOUT_PLAYBACK]",
						"[HFP_PLAYBACK]",
						"[TDM_PLAYBACK]"};

static struct playback_register REG;

/* Speaker */
static unsigned int pb_dma_playback_rates[] = {
	RATE_48000,
};

static unsigned int pb_dma_playback_channels[] = {
	CH_2, CH_8, CH_12
};
/* Headphone, Lineout */
static unsigned int pb_hp_playback_rates[] = {
	RATE_48000,
};

static unsigned int pb_hp_playback_channels[] = {
	CH_2,
};

static const struct snd_pcm_hw_constraint_list constraints_pb_dma_rates = {
	.count = ARRAY_SIZE(pb_dma_playback_rates),
	.list = pb_dma_playback_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_pb_dma_channels = {
	.count = ARRAY_SIZE(pb_dma_playback_channels),
	.list = pb_dma_playback_channels,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_pb_hp_rates = {
	.count = ARRAY_SIZE(pb_hp_playback_rates),
	.list = pb_hp_playback_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list constraints_pb_hp_channels = {
	.count = ARRAY_SIZE(pb_hp_playback_channels),
	.list = pb_hp_playback_channels,
	.mask = 0,
};

/* Default hardware settings of ALSA PCM playback */
static struct snd_pcm_hardware pb_dma_playback_default_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
	.channels_min = CH_2,
	.channels_max = CH_12,
	.buffer_bytes_max = MAX_PCM_BUF_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = 1024 * 8,
	.periods_min = 2,
	.periods_max = 8,
	.fifo_size = 0,
};

/* Headphone, Lineout hardware settings of ALSA PCM playback */
static struct snd_pcm_hardware pb_dma_playback_hp_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.buffer_bytes_max = MAX_PCM_BUF_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = (MAX_PCM_BUF_SIZE >> 2),
	.periods_min = 1,
	.periods_max = 1024,
	.fifo_size = 0,
};

static int playback_get_output_port(const char *sink)
{
	if (!strncmp(sink, "DAC0", strlen("DAC0")))
		return AUDIO_OUT_DAC0;
	else if (!strncmp(sink, "DAC1", strlen("DAC1")))
		return AUDIO_OUT_DAC1;
	else if (!strncmp(sink, "DAC2", strlen("DAC2")))
		return AUDIO_OUT_DAC2;
	else if (!strncmp(sink, "DAC3", strlen("DAC3")))
		return AUDIO_OUT_DAC3;
	else if (!strncmp(sink, "I2S0", strlen("I2S0")))
		return AUDIO_OUT_I2S0;
	else if (!strncmp(sink, "I2S-TX2", strlen("I2S-TX2")))
		return AUDIO_OUT_I2S_TX2;
	else if (!strncmp(sink, "I2S5", strlen("I2S5")))
		return AUDIO_OUT_I2S5;
	else
		return AUDIO_OUT_NULL;
}

static void charged_finished(struct mtk_runtime_struct *dev_runtime)
{
	unsigned int chg_reg_value;

	chg_reg_value = mtk_alsa_read_reg(REG.PB_CHG_FINISHED);

	if ((chg_reg_value & 0x4) && (dev_runtime->charged_finished == 0)) {
		mtk_alsa_write_reg_mask(REG.PB_HP_OPENED, 0x1000, 0x1000);
		dev_runtime->charged_finished = 1;
		apr_info(dev_runtime->log_level,
			"[%s] : Change charge state to finished!!\n", __func__);
	}
	if ((dev_runtime->sink_dev == AUDIO_OUT_DAC0 ||
			dev_runtime->sink_dev == AUDIO_OUT_DAC1) &&
			(dev_runtime->charged_finished == 0))
		apr_info(dev_runtime->log_level,
			"[%s] : Charge not finished!!\n", __func__);
}

static void dma_register_get_reg(unsigned int id,
	u32 *ctrl, u32 *wptr, u32 *level)
{
	switch (id)  {
	case SPEAKER_PLAYBACK:
		*ctrl = REG.PB_DMA1_CTRL;
		*wptr = REG.PB_DMA1_WPTR;
		*level = REG.PB_DMA1_DDR_LEVEL;
		break;
	case HEADPHONE_PLAYBACK:
		*ctrl = REG.PB_DMA2_CTRL;
		*wptr = REG.PB_DMA2_WPTR;
		*level = REG.PB_DMA2_DDR_LEVEL;
		break;
	case LINEOUT_PLAYBACK:
		*ctrl = REG.PB_DMA3_CTRL;
		*wptr = REG.PB_DMA3_WPTR;
		*level = REG.PB_DMA3_DDR_LEVEL;
		break;
	case HFP_PLAYBACK:
		*ctrl = REG.PB_DMA4_CTRL;
		*wptr = REG.PB_DMA4_WPTR;
		*level = REG.PB_DMA4_DDR_LEVEL;
		break;
	case TDM_PLAYBACK:
		*ctrl = REG.PB_DMA4_CTRL;
		*wptr = REG.PB_DMA4_WPTR;
		*level = REG.PB_DMA4_DDR_LEVEL;
		break;
	default:
		break;
	}
}

static int dma_reader_init(struct playback_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;
	ptrdiff_t dmaRdr_base_pa = 0;
	ptrdiff_t dmaRdr_base_ba = 0;
	ptrdiff_t dmaRdr_base_va = playback->str_mode_info.virtual_addr;
	int ret = 0;

	adev_info(priv->dev, dev_runtime->log_level,
		"%sInitiate MTK PCM Playback engine\n", PLAYBACK_ID_STRING[id]);

	playback->r_buf.size = priv->play_dma_size[id];
	playback->high_threshold =
		playback->r_buf.size - (playback->r_buf.size >> 3);

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	if (priv->play_dma_offset[id] == 0) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s fail to get DTS memory info\n",
			PLAYBACK_ID_STRING[id]);
		return -ENOMEM;
	}

	if ((playback->initialized_status != 1) ||
		(playback->status != CMD_RESUME)) {
		dmaRdr_base_pa = priv->mem_info.bus_addr -
			priv->mem_info.memory_bus_base + priv->play_dma_offset[id];
		dmaRdr_base_ba = priv->mem_info.bus_addr + priv->play_dma_offset[id];

		if ((dmaRdr_base_ba % 0x1000)) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA PB]%s invalid addr,should align 4KB\n",
				PLAYBACK_ID_STRING[id]);
			return -EFAULT;
		}

		/* convert Bus Address to Virtual Address */
		if (dmaRdr_base_va == 0) {
			dmaRdr_base_va = (ptrdiff_t)ioremap_wc(dmaRdr_base_ba,
						playback->r_buf.size);
			if (dmaRdr_base_va == 0) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA PB]%s fail to convert addr\n",
					PLAYBACK_ID_STRING[id]);
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

	/* reset PCM playback engine */
	mtk_alsa_write_reg(REG_CTRL, 0x0001);

	/* clear PCM playback write pointer */
	mtk_alsa_write_reg(REG_WPTR, 0x0000);

	/* reset written size */
	playback->copy_size = 0;

	/* clear PCM playback pcm buffer */
	memset((void *)playback->r_buf.addr, 0x00,
					playback->r_buf.size);
	smp_mb();

	if ((id == SPEAKER_PLAYBACK) && (priv->hfp_enable_flag)) {
		ret = mtk_i2s_tx_clk_set_enable(playback->format, 48000);
		if (playback->format == SNDRV_PCM_FORMAT_S24_LE)
			mtk_alsa_write_reg_mask(REG.PB_I2S_BCK_WIDTH, 0x0007, 0x0005);
		else if (playback->format == SNDRV_PCM_FORMAT_S32_LE)
			mtk_alsa_write_reg_mask(REG.PB_I2S_BCK_WIDTH, 0x0007, 0x0006);
		else
			mtk_alsa_write_reg_mask(REG.PB_I2S_BCK_WIDTH, 0x0007, 0x0004);
	}

	if (id == TDM_PLAYBACK)
		mtk_alsa_write_reg_mask(REG.PB_I2S_TX2_TDM_CFG, 0x9FDF, 0x1940);

	if (playback->format == SNDRV_PCM_FORMAT_S24_LE) {
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0x30, 0x30);
		playback->byte_length = 4;
	} else if (playback->format == SNDRV_PCM_FORMAT_S32_LE) {
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0x30, 0x20);
		playback->byte_length = 4;
	} else {
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0x30, 0x00);
		playback->byte_length = 2;
	}

	if ((playback->channel_mode == CH_2) || (priv->hfp_enable_flag))
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0xC0, 0x00);
	else if (playback->channel_mode == CH_8)
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0xC0, 0x80);
	else if (playback->channel_mode == CH_12)
		mtk_alsa_write_reg_mask_byte(REG_CTRL, 0xC0, 0x40);

	return ret;
}

static int dma_reader_exit(struct mtk_runtime_struct *dev_runtime,
	unsigned int id)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	ptrdiff_t dmaRdr_base_va = playback->str_mode_info.virtual_addr;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	apr_info(dev_runtime->log_level,
		"%sExit MTK PCM Playback engine\n", PLAYBACK_ID_STRING[id]);

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	/* reset PCM playback engine */
	mtk_alsa_write_reg(REG_CTRL, 0x0001);

	/* clear PCM playback write pointer */
	mtk_alsa_write_reg(REG_WPTR, 0x0000);

	if (dmaRdr_base_va != 0) {
		if (playback->r_buf.addr) {
			iounmap((void *)playback->r_buf.addr);
			playback->r_buf.addr = 0;
		} else
			apr_err(dev_runtime->log_level,
				"[ALSA PB]%sbuf addr should not be 0\n", PLAYBACK_ID_STRING[id]);

		playback->str_mode_info.virtual_addr = 0;
	}

	playback->status = CMD_STOP;
	playback->initialized_status = 0;

	return 0;
}

static int dma_reader_start(struct snd_soc_component *component,
		struct mtk_runtime_struct *dev_runtime, unsigned int id)
{
	struct snd_soc_card *card = component->card;
	const struct snd_soc_dapm_route *routes = card->of_dapm_routes;
	const char *sink = NULL;
	int port;
	int i;
	const char *source;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	switch (id)  {
	case SPEAKER_PLAYBACK:
		source = "SPEAKER_PLAYBACK";
		break;
	case HEADPHONE_PLAYBACK:
		source = "HEADPHONE_PLAYBACK";
		break;
	case LINEOUT_PLAYBACK:
		source = "LINEOUT_PLAYBACK";
		break;
	case HFP_PLAYBACK:
		source = "HFP_PLAYBACK";
		break;
	case TDM_PLAYBACK:
		source = "TDM_PLAYBACK";
		break;
	default:
		source = "NO_THIS_PLAYBACK_TYPE";
		break;
	}

	apr_info(dev_runtime->log_level,
		"%sStart MTK PCM Playback engine\n", PLAYBACK_ID_STRING[id]);

	for (i = 0; i < card->num_of_dapm_routes; i++)
		if (!strncmp(routes[i].source, source, strlen(source)))
			sink = routes[i].sink;

	if (sink != NULL) {
		/* set speaker dma to output port */
		port = playback_get_output_port(sink);
		mtk_alsa_write_reg_mask(REG_CTRL, 0xF000, port << 12);
		dev_runtime->sink_dev = port;
	} else {
		apr_err(dev_runtime->log_level,
			"[ALSA PB]%scan't get output port will no sound\n",
			PLAYBACK_ID_STRING[id]);
		dev_runtime->sink_dev = 0;
	}
	/* start PCM playback engine */
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0003, 0x0002);

	/* trigger DSP to reset DMA engine */
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0001, 0x0001);
	mtk_alsa_delaytask_us(50);
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0001, 0x0000);

	return 0;
}

static int dma_reader_stop(struct mtk_runtime_struct *dev_runtime, unsigned int id)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	apr_info(dev_runtime->log_level,
		"%sStop MTK PCM Playback engine\n", PLAYBACK_ID_STRING[id]);

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	/* reset PCM playback engine */
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0001, 0x0001);

	/* stop PCM playback engine */
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0002, 0x0000);

	/* clear PCM playback write pointer */
	mtk_alsa_write_reg(REG_WPTR, 0x0000);

	/* reset Write Pointer */
	playback->r_buf.r_ptr = playback->r_buf.addr;
	playback->r_buf.w_ptr = playback->r_buf.addr;
	playback->r_buf.avail_size = 0;
	playback->r_buf.remain_size = 0;
	playback->r_buf.total_read_size = 0;
	playback->r_buf.total_write_size = 0;

	/* reset written size */
	playback->copy_size = 0;

	playback->status = CMD_STOP;

	return 0;
}

static void dma_reader_set_hw_param(struct playback_priv *priv,
	struct snd_pcm_runtime *runtime, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(priv->dev, dev_runtime->log_level,
		"[ALSA PB]%s target channel mode is %u\n",
		PLAYBACK_ID_STRING[id], runtime->channels);

	playback->channel_mode = runtime->channels;
	playback->sample_rate = runtime->rate;
	playback->format = runtime->format;
}

static int dma_reader_get_inused_lines(unsigned int id)
{
	int inused_lines = 0;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	mtk_alsa_write_reg_mask(REG_CTRL, 0x0008, 0x0008);
	inused_lines = mtk_alsa_read_reg(REG_DDR_LEVEL);
	mtk_alsa_write_reg_mask(REG_CTRL, 0x0008, 0x0000);

	return inused_lines;
}

static void dma_reader_get_avail_bytes
			(struct mtk_runtime_struct *dev_runtime, unsigned int id)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	int inused_lines = 0;
	int avail_lines = 0;
	int avail_bytes = 0;

	inused_lines = dma_reader_get_inused_lines(id);
	avail_lines = (playback->r_buf.size / BYTES_IN_MIU_LINE) -
					inused_lines;

	if (avail_lines < 0) {
		apr_err(dev_runtime->log_level, "[ALSA PB]%s incorrect inused line %d\n",
			PLAYBACK_ID_STRING[id], inused_lines);
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

static unsigned int dma_reader_write_zero(struct mtk_runtime_struct *dev_runtime,
					unsigned int bytes)
{
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned int copy_size = bytes;
	int loop = 0;

	for (loop = 0; loop < copy_size; loop++) {
		*(playback->r_buf.w_ptr++) = 0;

		if (playback->r_buf.w_ptr >= playback->r_buf.buf_end)
			playback->r_buf.w_ptr = playback->r_buf.addr;
	}
	return copy_size;
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

static unsigned int dma_reader_write(struct playback_priv *priv,
					unsigned int id, void *buffer, unsigned int bytes)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned int w_ptr_offset = 0;
	unsigned int copy_sample = 0;
	unsigned int copy_size = 0;
	unsigned int copy_tmp = 0;
	int inused_bytes = 0;
	static unsigned int warning_cnt;
	u32 REG_CTRL = 0, REG_WPTR = 0, REG_DDR_LEVEL = 0;

	dma_register_get_reg(id, &REG_CTRL, &REG_WPTR, &REG_DDR_LEVEL);

	if ((playback->channel_mode == 0) || (playback->byte_length == 0)) {
		adev_err(priv->dev, dev_runtime->log_level,
			"%s%s[%d] devision by zero error!\n",
			PLAYBACK_ID_STRING[id], __func__, __LINE__);
		return 0;
	}

	copy_sample = bytes / (playback->channel_mode * playback->byte_length);
	copy_size = bytes;

	inused_bytes = dma_reader_get_inused_lines(id) * BYTES_IN_MIU_LINE;

	if ((inused_bytes == 0) && (playback->status == CMD_START)) {
		if (warning_cnt == 0)
			adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s*****Buf empty !!*****\n", PLAYBACK_ID_STRING[id]);
		warning_cnt++;
	} else if (warning_cnt != 0) {
		adev_info(priv->dev, dev_runtime->log_level,
		"[ALSA PB]%s*****Buf empty cnt = %d!!*****\n", PLAYBACK_ID_STRING[id], warning_cnt);
		warning_cnt = 0;
	}

	if ((inused_bytes + copy_size + playback->r_buf.remain_size) > playback->high_threshold) {
		adev_info(priv->dev, dev_runtime->log_level,
			"*****PCM Playback near Buffer threshold !!*****\n");
		return 0;
	}

	if ((dev_runtime->sink_dev == AUDIO_OUT_DAC0 ||
			dev_runtime->sink_dev == AUDIO_OUT_DAC1) &&
			(dev_runtime->charged_finished == 0)) {	/* charging is not completed  */
		copy_tmp = dma_reader_write_zero(dev_runtime, copy_size);
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s*****Output zero!!*****\n",
			PLAYBACK_ID_STRING[id]);
	} else if (dev_runtime->inject.status == 1) {	/* for inject debug mode */
		copy_sample = dma_reader_write_inject(dev_runtime, copy_sample);
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s*****Output sinetone!!*****\n",
			PLAYBACK_ID_STRING[id]);
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

	mtk_alsa_write_reg_mask(REG_WPTR, 0xFFFF,
		(w_ptr_offset / BYTES_IN_MIU_LINE));

	playback->copy_size += bytes;
	playback->status = CMD_START;

	/* ensure write pointer can be applied */
	mtk_alsa_delaytask(1);

	return bytes;
}

static int dma_reader_fe_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	int ret;
	struct snd_pcm_hardware *pb_hw;
	const struct snd_pcm_hw_constraint_list *rate_hw, *channel_hw;

	dev_runtime->charged_finished = 0;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

	if ((id == SPEAKER_PLAYBACK) || (id == TDM_PLAYBACK)) {
		pb_hw = &pb_dma_playback_default_hw;
		rate_hw = &constraints_pb_dma_rates;
		channel_hw = &constraints_pb_dma_channels;
	} else {
		pb_hw = &pb_dma_playback_hp_hw;
		rate_hw = &constraints_pb_hp_rates;
		channel_hw = &constraints_pb_hp_channels;
	}

	/* Set specified information */
	dev_runtime->substreams.p_substream = substream;
	dev_runtime->device_cycle_count = mtk_alsa_get_hw_reboot_cycle();

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

	snd_soc_set_runtime_hwparams(substream, pb_hw);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE, rate_hw);
	if (ret < 0) {
		adev_err(priv->dev, dev_runtime->log_level,
		"%ssnd_pcm_hw_constraint_list RATE fail test\n", PLAYBACK_ID_STRING[id]);
		return ret;
	}
	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS, channel_hw);
	if (ret < 0) {
		adev_err(priv->dev, dev_runtime->log_level,
		"%ssnd_pcm_hw_constraint_list CHANNELS fail test\n", PLAYBACK_ID_STRING[id]);
		return ret;
	}

	if (playback->c_buf.addr == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB] %s[%d] id = %u c_buf is NULL\n",
			__func__, __LINE__, id);
		return -ENOMEM;
	}

	memset((void *)playback->c_buf.addr, 0x00, MAX_PCM_BUF_SIZE);
	playback->c_buf.size = MAX_PCM_BUF_SIZE;

	if (spinlock->init_status == 0) {
		spin_lock_init(&spinlock->lock);
		spinlock->init_status = 1;
	}

	timer_open(&dev_runtime->timer, (void *)timer_monitor);

	/* Check if charging is complete */
	charged_finished(dev_runtime);
	return 0;
}

static void dma_reader_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	unsigned long flags;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

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
	playback->byte_length = 0;
	playback->status = CMD_STOP;

	/* Stop DMA Reader  */
	dma_reader_stop(dev_runtime, id);
	mtk_alsa_delaytask(2);

	/* Close MTK Audio DSP */
	dma_reader_exit(dev_runtime, id);
	mtk_alsa_delaytask(2);
}

static int dma_reader_fe_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int dma_reader_fe_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int dma_reader_fe_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct snd_soc_component *component = dai->component;
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);
	/*print hw parameter*/

	adev_info(priv->dev, dev_runtime->log_level, "%sparameter :\n"
								"format is %d\n"
								"rate is %d\n"
								"channels is %d\n"
								"sample_bits is %d\n",
								PLAYBACK_ID_STRING[id],
								runtime->format,
								runtime->rate,
								runtime->channels,
								runtime->sample_bits);

	if (playback->status != CMD_START) {
		/* Configure SW DMA */
		dma_reader_set_hw_param(priv, runtime, id);
		/* Open SW DMA */
		dma_reader_init(priv, id);
		/* Start SW DMA */
		dma_reader_start(component, dev_runtime, id);
		/* Configure SW DMA device status */
		playback->status = CMD_START;
	} else {
		/* Stop SW DMA */
		dma_reader_stop(dev_runtime, id);
		mtk_alsa_delaytask(2);
		/* Start SW DMA */
		dma_reader_start(component, dev_runtime, id);
	}

	return 0;
}

static int dma_reader_fe_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int err = 0;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		PLAYBACK_ID_STRING[id], __func__, __LINE__);

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
		adev_info(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s invalid PB's trigger command %d\n",
			PLAYBACK_ID_STRING[id], cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

static int i2s_be_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	return 0;
}

static void i2s_be_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
}

static int i2s_be_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	return 0;
}

static int i2s_be_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	return 0;
}

static int i2s_be_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	return 0;
}

static int i2s_be_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	return 0;
}

/* FE DAIs */
static const struct snd_soc_dai_ops dma_reader_fe_dai_ops = {
	.startup	= dma_reader_fe_startup,
	.shutdown	= dma_reader_fe_shutdown,
	.hw_params	= dma_reader_fe_hw_params,
	.hw_free	= dma_reader_fe_hw_free,
	.prepare	= dma_reader_fe_prepare,
	.trigger	= dma_reader_fe_trigger,
};

/* BE DAIs */
static const struct snd_soc_dai_ops i2s_be_dai_ops = {
	.startup	= i2s_be_startup,
	.shutdown	= i2s_be_shutdown,
	.hw_params	= i2s_be_hw_params,
	.hw_free	= i2s_be_hw_free,
	.prepare	= i2s_be_prepare,
	.trigger	= i2s_be_trigger,
};

int playback_dai_suspend(struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;

	if (playback->status != CMD_STOP) {
		dma_reader_stop(dev_runtime, id);
		playback->status = CMD_SUSPEND;
	}

	return 0;
}

int playback_dai_resume(struct snd_soc_dai *dai)
{
	struct playback_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *playback = &dev_runtime->playback;
	struct snd_soc_component *component = dai->component;

	if (playback->status == CMD_SUSPEND) {
		playback->status = CMD_RESUME;
		dma_reader_init(priv, id);
		dma_reader_start(component, dev_runtime, id);
		playback->status = CMD_START;
	}

	return 0;
}

int i2s0_dai_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

int i2s0_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}

int dac1_dai_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

int dac1_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_driver playback_dais[] = {
	/* FE DAIs */
	{
		.name = "SPEAKER",
		.id = SPEAKER_PLAYBACK,
		.playback = {
			.stream_name = "SPEAKER_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_12,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &dma_reader_fe_dai_ops,
		.suspend = playback_dai_suspend,
		.resume = playback_dai_resume,
	},
	{
		.name = "HEADPHONE",
		.id = HEADPHONE_PLAYBACK,
		.playback = {
			.stream_name = "HEADPHONE_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &dma_reader_fe_dai_ops,
		.suspend = playback_dai_suspend,
		.resume = playback_dai_resume,
	},
	{
		.name = "LINEOUT",
		.id = LINEOUT_PLAYBACK,
		.playback = {
			.stream_name = "LINEOUT_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &dma_reader_fe_dai_ops,
		.suspend = playback_dai_suspend,
		.resume = playback_dai_resume,
	},
	{
		.name = "HFP-TX",
		.id = HFP_PLAYBACK,
		.playback = {
			.stream_name = "HFP_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &dma_reader_fe_dai_ops,
		.suspend = playback_dai_suspend,
		.resume = playback_dai_resume,
	},
	{
		.name = "TDM-TX",
		.id = TDM_PLAYBACK,
		.playback = {
			.stream_name = "TDM_PLAYBACK",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &dma_reader_fe_dai_ops,
		.suspend = playback_dai_suspend,
		.resume = playback_dai_resume,
	},
	/* BE DAIs */
	{
		.name = "BE_I2S0",
		.id = I2S0_PLAYBACK,
		.playback = {
			.stream_name = "I2S0",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &i2s_be_dai_ops,
		.suspend = i2s0_dai_suspend,
		.resume = i2s0_dai_resume,
	},
	{
		.name = "BE_I2S5",
		.id = I2S5_PLAYBACK,
		.playback = {
			.stream_name = "I2S5",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &i2s_be_dai_ops,
		.suspend = i2s0_dai_suspend,
		.resume = i2s0_dai_resume,
	},
	{
		.name = "BE_DAC0",
		.id = DAC0_PLAYBACK,
		.playback = {
			.stream_name = "DAC0",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
		},
	},
	{
		.name = "BE_DAC1",
		.id = DAC1_PLAYBACK,
		.playback = {
			.stream_name = "DAC1",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
		},
		.suspend = dac1_dai_suspend,
		.resume = dac1_dai_resume,
	},
	{
		.name = "BE_I2S_TX2",
		.id = I2S_TX2_PLAYBACK,
		.playback = {
			.stream_name = "I2S-TX2",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &i2s_be_dai_ops,
		.suspend = i2s0_dai_suspend,
		.resume = i2s0_dai_resume,
	},
};

static snd_pcm_uframes_t playback_pcm_pointer
				(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, PLAYBACK_DMA_NAME);
	struct playback_priv *priv = snd_soc_component_get_drvdata(component);
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

static int playback_pcm_copy(struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, PLAYBACK_DMA_NAME);
	struct playback_priv *priv = snd_soc_component_get_drvdata(component);
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
	unsigned long receive_size = 0;
	unsigned long flags;

	if (buf == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%sErr! invalid memory address!\n", PLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	if (bytes == 0) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%sErr! request bytes is zero!\n", PLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	if (mtk_alsa_get_hw_reboot_flag(dev_runtime->device_cycle_count)) {
		snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%sErr! after reboot need re-open device!\n",
			PLAYBACK_ID_STRING[id]);
		return -EINVAL;
	}

	/* Check if charging is complete */
	charged_finished(dev_runtime);

	buffer_tmp = playback->c_buf.addr;
	if (buffer_tmp == NULL) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%sErr! need to alloc system mem for PCM buf\n",
			PLAYBACK_ID_STRING[id]);
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
					"[ALSA PB]%sfail to wakeup Monitor %d\n",
					PLAYBACK_ID_STRING[id], substream->pcm->device);
				break;
			}

			mtk_alsa_delaytask(1);
		}
		retry_counter = 0;
	}

	request_size = bytes;

	dma_reader_get_avail_bytes(dev_runtime, id);

	if ((((substream->f_flags & O_NONBLOCK) > 0) &&
		(request_size > playback->r_buf.avail_size)) ||
		(request_size > playback->c_buf.size)) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA PB]%s copy fail!\n"
			"request_size = %u, buf avail_size = %u, copy buffer size = %u\n",
			PLAYBACK_ID_STRING[id], request_size,
			playback->r_buf.avail_size, playback->c_buf.size);
		return -EAGAIN;
	}

	do {
		size_to_copy = request_size - copied_size;

		/* Deliver PCM data */
		receive_size = copy_from_user(buffer_tmp, (buf + copied_size),
						size_to_copy);
		if (receive_size) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA PB]%sErr! %lu return byte\n",
				PLAYBACK_ID_STRING[id], receive_size);
			size_to_copy -= receive_size;
		}

		size = dma_reader_write(priv, id, (void *)buffer_tmp, size_to_copy);
		if (size == 0) {
			if ((retry_counter % 50) == 0)
				adev_info(priv->dev, dev_runtime->log_level,
					"[ALSA PB]%sRetry write PCM\n", PLAYBACK_ID_STRING[id]);

			mtk_alsa_delaytask(1);

			if (retry_counter > 500) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA PB]%scopy fail\n", PLAYBACK_ID_STRING[id]);
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

const struct snd_pcm_ops playback_pcm_ops = {
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = playback_pcm_pointer,
	.copy_user = playback_pcm_copy,
};

static int dma_inject_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct playback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int val = ucontrol->value.integer.value[0];
	unsigned int xdata = kcontrol->private_value;

	if (val < 0)
		return -EINVAL;

	adev_info(priv->dev, MTK_ALSA_LEVEL_INFO,
		"[ALSA PB]Inject spk with Sinetone %d\n", val);

	priv->pcm_device[xdata]->inject.status = val;
	priv->pcm_device[xdata]->inject.r_ptr = 0;

	return 0;
}

static int dma_inject_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct playback_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int xdata = kcontrol->private_value;

	adev_info(priv->dev, MTK_ALSA_LEVEL_INFO,
		"[ALSA PB]get spk inject status\n");

	ucontrol->value.integer.value[0] =
			priv->pcm_device[xdata]->inject.status;

	return 0;
}

static const struct snd_kcontrol_new playback_controls[] = {
	SOC_SINGLE_BOOL_EXT("Inject Speaker Sinetone", 0,
			dma_inject_get, dma_inject_put),
};

//[Sysfs] HELP command
static ssize_t help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (check_sysfs_null(dev, attr, buf))
		return 0;

	return snprintf(buf, PAGE_SIZE, "Debug Help:\n"
		"- playback_log_level\n"
		"	<W>[id][level]: Set device id & debug log level.\n"
		"	(0:speaker, 1:headphone, 2:lineout, 3:HFP, 4:TDM ...)\n"
		"	(0:emerg, 1:alert, 2:critical, 3:error, 4:warn, 5:notice, 6:info, 7:debug)\n"
		"	<R>: Read all device debug log level.\n");
}

//[Sysfs] Speaker Log level
static ssize_t playback_log_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct playback_priv *priv = NULL;
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

	if ((id < PLAYBACK_DEV_NUM) && (log_level <= LOGLEVEL_DEBUG))
		priv->pcm_device[id]->log_level = log_level;

	return count;
}

static ssize_t playback_log_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;
	unsigned int id;
	int count = 0;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	for (id = 0; id < PLAYBACK_DEV_NUM; id++) {
		count += snprintf(buf + count, PAGE_SIZE, "%slog_level:%d\n",
			PLAYBACK_ID_STRING[id], priv->pcm_device[id]->log_level);
	}
	return count;
}

//[Sysfs] Playback Sine tone
static ssize_t playback_sinetone_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct playback_priv *priv = NULL;
	unsigned int id;
	unsigned int enablesinetone = 0;

	if (check_sysfs_null(dev, attr, (char *)buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	if (sscanf(buf, "%u %u", &id, &enablesinetone) < 0) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]input value wrong\n", __func__);
		return 0;
	}

	if ((id < PLAYBACK_DEV_NUM) && (enablesinetone <= 1)) {
		priv->pcm_device[id]->inject.status = enablesinetone;
		priv->pcm_device[id]->inject.r_ptr = 0;
	}

	return count;
}

static ssize_t playback_sinetone_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;
	unsigned int id;
	int count = 0;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	for (id = 0; id < PLAYBACK_DEV_NUM; id++) {
		count += snprintf(buf + count, PAGE_SIZE, "%ssinetone status:%d\n",
			PLAYBACK_ID_STRING[id], priv->pcm_device[id]->inject.status);
	}

	return count;
}

static ssize_t playback_status_show(struct device *dev, struct device_attribute *attr,
					char *buf, struct playback_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	const char *str = NULL;
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

static ssize_t spk_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return playback_status_show(dev, attr, buf, priv, SPEAKER_PLAYBACK);
}

static ssize_t hp_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return playback_status_show(dev, attr, buf, priv, HEADPHONE_PLAYBACK);
}

static ssize_t lineout_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return playback_status_show(dev, attr, buf, priv, LINEOUT_PLAYBACK);
}

static ssize_t hfp_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return playback_status_show(dev, attr, buf, priv, HFP_PLAYBACK);
}

static ssize_t tdm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct playback_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return playback_status_show(dev, attr, buf, priv, TDM_PLAYBACK);
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(playback_log_level);
static DEVICE_ATTR_RW(playback_sinetone);
static DEVICE_ATTR_RO(spk_status);
static DEVICE_ATTR_RO(hp_status);
static DEVICE_ATTR_RO(lineout_status);
static DEVICE_ATTR_RO(hfp_status);
static DEVICE_ATTR_RO(tdm_status);

static struct attribute *playback_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_playback_log_level.attr,
	&dev_attr_playback_sinetone.attr,
	&dev_attr_spk_status.attr,
	&dev_attr_hp_status.attr,
	&dev_attr_lineout_status.attr,
	NULL,
};

static struct attribute *hfp_attrs[] = {
	&dev_attr_hfp_status.attr,
	NULL,
};

static struct attribute *tdm_attrs[] = {
	&dev_attr_tdm_status.attr,
	NULL,
};

static const struct attribute_group playback_attr_group = {
	.name = "mtk_dbg",
	.attrs = playback_attrs,
};

static const struct attribute_group hfp_attr_group = {
	.name = "mtk_dbg",
	.attrs = hfp_attrs,
};

static const struct attribute_group tdm_attr_group = {
	.name = "mtk_dbg",
	.attrs = tdm_attrs,
};

static const struct attribute_group *playback_attr_groups[] = {
	&playback_attr_group,
	NULL,
};

static int playback_reboot_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	mtk_alsa_write_reg_mask(REG.PB_DMA1_CTRL, 0x0002, 0x0000);
	mtk_alsa_write_reg_mask(REG.PB_DMA2_CTRL, 0x0002, 0x0000);
	mtk_alsa_write_reg_mask(REG.PB_DMA3_CTRL, 0x0002, 0x0000);
	mtk_alsa_write_reg_mask(REG.PB_DMA4_CTRL, 0x0002, 0x0000);

	return NOTIFY_OK;
}

/* reboot notifier */
static struct notifier_block alsa_playback_notifier = {
	.notifier_call = playback_reboot_notify,
};

static int playback_probe(struct snd_soc_component *component)
{
	struct playback_priv *priv;
	struct device *dev;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai_link *dai_link;
	int ret;

	if (component == NULL) {
		pr_err("[AUDIO][ERROR][%s]component can't be NULL!!!\n", __func__);
		return -EINVAL;
	}

	if (component->dev == NULL) {
		pr_err("[AUDIO][ERROR][%s]component->dev can't be NULL!!!\n", __func__);
		return -EINVAL;
	}

	priv = snd_soc_component_get_drvdata(component);

	if (priv == NULL) {
		pr_err("[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return -EFAULT;
	}

	dev = component->dev;
	ret = 0;

	/*Set default log level*/
	priv->pcm_device[SPEAKER_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[HEADPHONE_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[LINEOUT_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[HFP_PLAYBACK]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[TDM_PLAYBACK]->log_level = LOGLEVEL_DEBUG;

	/* Create device attribute files */
	ret = sysfs_create_groups(&dev->kobj, playback_attr_groups);
	if (ret)
		dev_err(dev, "Audio playback Sysfs init fail\n");

	for_each_card_rtds(component->card, rtd) {
		dai_link = rtd->dai_link;

		if (dai_link->ignore == 0) {
			if (!strncmp(dai_link->cpus->dai_name, "HFP-TX", strlen("HFP-TX")))
				ret = sysfs_merge_group(&dev->kobj, &hfp_attr_group);
			else if (!strncmp(dai_link->cpus->dai_name, "TDM-TX", strlen("TDM-TX")))
				ret = sysfs_merge_group(&dev->kobj, &tdm_attr_group);
			else
				ret = 0;

			if (ret)
				dev_info(dev, "[AUDIO][%s]sysfs_merge_groups fail!\n", __func__);
		}

		/* set pcm_fill_silence to all playback dai */
		if (rtd->cpu_dai->driver->playback.stream_name != NULL)
			rtd->ops.fill_silence = &pcm_fill_silence;
	}

	/* Register reboot notify. */
	ret = register_reboot_notifier(&alsa_playback_notifier);
	if (ret) {
		dev_err(dev, "[ALSA PB]register reboot notify fail %d\n", ret);
		return ret;
	}

	return 0;
}

static void playback_remove(struct snd_soc_component *component)
{
	struct device *dev;

	dev = NULL;

	if (component != NULL)
		dev = component->dev;

	if (dev != NULL) {
		sysfs_unmerge_group(&dev->kobj, &hfp_attr_group);
		sysfs_remove_groups(&dev->kobj, playback_attr_groups);
}
}

static const struct snd_soc_component_driver playback_dai_component = {
	.name			= PLAYBACK_DMA_NAME,
	.probe			= playback_probe,
	.remove			= playback_remove,
	.ops			= &playback_pcm_ops,
	.controls		= playback_controls,
	.num_controls		= ARRAY_SIZE(playback_controls),
};

unsigned int playback_get_dts_u32(struct device_node *np, const char *name)
{
	unsigned int value;
	int ret;

	ret = of_property_read_u32(np, name, &value);
	if (ret)
		WARN_ON("[ALSA PB]Can't get DTS property\n");

	return value;
}

static int playback_parse_capability(struct device_node *node,
				struct playback_priv *priv)
{
	struct device_node *np;
	const char *device_status = NULL;

	np = of_find_node_by_name(NULL, "mtk-sound-capability");
	if (!np)
		return -ENODEV;

	priv->hfp_enable_flag = 1;

	if (!of_property_read_bool(np, "hfp"))
		priv->hfp_enable_flag = 0;
	else {
		of_property_read_string(np, "hfp", &device_status);

		if (!strncmp(device_status, "disable", strlen("disable")))
			priv->hfp_enable_flag = 0;
	}

	return 0;
}

static int playback_parse_registers(void)
{
	struct device_node *np;

	memset(&REG, 0x0, sizeof(struct playback_register));

	np = of_find_node_by_name(NULL, "playback-register");
	if (!np)
		return -ENODEV;

	REG.PB_DMA1_CTRL = playback_get_dts_u32(np, "reg_pb_dma1_ctrl");
	REG.PB_DMA1_WPTR = playback_get_dts_u32(np, "reg_pb_dma1_wptr");
	REG.PB_DMA1_DDR_LEVEL = playback_get_dts_u32(np, "reg_pb_dma1_ddr_level");
	REG.PB_DMA2_CTRL = playback_get_dts_u32(np, "reg_pb_dma2_ctrl");
	REG.PB_DMA2_WPTR = playback_get_dts_u32(np, "reg_pb_dma2_wptr");
	REG.PB_DMA2_DDR_LEVEL = playback_get_dts_u32(np, "reg_pb_dma2_ddr_level");
	REG.PB_DMA3_CTRL = playback_get_dts_u32(np, "reg_pb_dma3_ctrl");
	REG.PB_DMA3_WPTR = playback_get_dts_u32(np, "reg_pb_dma3_wptr");
	REG.PB_DMA3_DDR_LEVEL = playback_get_dts_u32(np, "reg_pb_dma3_ddr_level");
	REG.PB_DMA4_CTRL = playback_get_dts_u32(np, "reg_pb_dma4_ctrl");
	REG.PB_DMA4_WPTR = playback_get_dts_u32(np, "reg_pb_dma4_wptr");
	REG.PB_DMA4_DDR_LEVEL = playback_get_dts_u32(np, "reg_pb_dma4_ddr_level");
	REG.PB_CHG_FINISHED = playback_get_dts_u32(np, "reg_pb_chg_finished");
	REG.PB_HP_OPENED = playback_get_dts_u32(np, "reg_pb_hp_opened");
	REG.PB_I2S_BCK_WIDTH = playback_get_dts_u32(np, "reg_pb_i2s_bck_width");
	REG.PB_I2S_TX2_TDM_CFG = playback_get_dts_u32(np, "reg_pb_i2s_tx2_tdm_cfg");

	return 0;
}

static int playback_parse_mmap(struct device_node *np,
				struct playback_priv *priv)
{
	struct of_mmap_info_data of_mmap_info = {};
	int ret;

	ret = of_mtk_get_reserved_memory_info_by_idx(np,
						0, &of_mmap_info);
	if (ret) {
		pr_err("[ALSA PB]get audio mmap fail\n");
		return -EINVAL;
	}

	priv->mem_info.miu = of_mmap_info.miu;
	priv->mem_info.bus_addr = of_mmap_info.start_bus_address;
	priv->mem_info.buffer_size = of_mmap_info.buffer_size;

	return 0;
}

static void playback_hfp_active_check(struct snd_soc_dai_driver *dai_driver,
	struct playback_priv *priv)
{
	struct snd_soc_pcm_stream *stream = &dai_driver[SPEAKER_PLAYBACK].playback;

	if (priv->hfp_enable_flag) {
		stream->channels_max = CH_8;

		pr_err("[ALSA PB]spk channel max is set to 8 channels, since HFP is enable\n");
	}
}

int playback_parse_memory(struct device_node *np, struct playback_priv *priv)
{
	int ret;

	ret = of_property_read_u32(np, "speaker_dma_offset",
					&priv->play_dma_offset[SPEAKER_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get speaker dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "speaker_dma_size",
					&priv->play_dma_size[SPEAKER_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get speaker dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "headphone_dma_offset",
					&priv->play_dma_offset[HEADPHONE_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get headphone dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "headphone_dma_size",
					&priv->play_dma_size[HEADPHONE_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get headphone dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "lineout_dma_offset",
					&priv->play_dma_offset[LINEOUT_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get lineout dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "lineout_dma_size",
					&priv->play_dma_size[LINEOUT_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get lineout dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "bt_dma_offset",
					&priv->play_dma_offset[HFP_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get hfp dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "bt_dma_size",
					&priv->play_dma_size[HFP_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get hfp dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "bt_dma_offset",
					&priv->play_dma_offset[TDM_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get TDM dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "bt_dma_size",
					&priv->play_dma_size[TDM_PLAYBACK]);
	if (ret) {
		pr_err("[ALSA PB]can't get TDM dma size\n");
		return -EINVAL;
	}

	return 0;
}

int playback_dev_probe(struct platform_device *pdev)
{
	struct playback_priv *priv;
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

	for (i = 0; i < PLAYBACK_DEV_NUM; i++) {
		priv->pcm_device[i] = devm_kzalloc(&pdev->dev,
				sizeof(struct mtk_runtime_struct), GFP_KERNEL);
		/* Allocate system memory for Specific Copy */
		priv->pcm_device[i]->playback.c_buf.addr = vmalloc(MAX_PCM_BUF_SIZE);
		if ((!priv->pcm_device[i]) ||
			(!priv->pcm_device[i]->playback.c_buf.addr))
			return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);

	ret = devm_snd_soc_register_component(&pdev->dev,
					&playback_dai_component,
					playback_dais,
					ARRAY_SIZE(playback_dais));
	if (ret) {
		dev_err(dev, "[ALSA PB]soc_register_component fail %d\n", ret);
		return ret;
	}
	/* parse dts */
	ret = of_property_read_u64(node, "memory_bus_base",
					&priv->mem_info.memory_bus_base);
	if (ret) {
		dev_err(dev, "[ALSA PB]can't get miu bus base\n");
		return -EINVAL;
	}
	/* parse registers */
	ret = playback_parse_registers();
	if (ret) {
		dev_err(dev, "[ALSA PB]parse register fail %d\n", ret);
		return ret;
	}
	/* parse dma memory info */
	ret = playback_parse_memory(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA PB]parse memory fail %d\n", ret);
		return ret;
	}
	/* parse mmap */
	ret = playback_parse_mmap(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA PB]parse mmap fail %d\n", ret);
		return ret;
	}
	/* parse capability */
	ret = playback_parse_capability(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA PB]parse capability fail %d\n", ret);
		return ret;
	}
	/* Check SPK channel max setting*/
	playback_hfp_active_check(playback_dais, priv);

	return 0;
}

int playback_dev_remove(struct platform_device *pdev)
{
	struct playback_priv *priv;
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

	for (i = 0; i < PLAYBACK_DEV_NUM; i++) {
		/* Free allocated memory */
		if (priv->pcm_device[i]->playback.c_buf.addr != NULL) {
			vfree(priv->pcm_device[i]->playback.c_buf.addr);
			priv->pcm_device[i]->playback.c_buf.addr = NULL;
		}
	}
	return 0;
}

MODULE_DESCRIPTION("Mediatek ALSA SoC Playback DMA platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:snd-evb");
