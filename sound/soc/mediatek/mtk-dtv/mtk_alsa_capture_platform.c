// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_alsa_capture_platform.c  --  Mediatek DTV Capture function
 *
 * Copyright (c) 2020 MediaTek Inc.
 */


#include "mtk_alsa_chip.h"
#include "mtk_alsa_capture_platform.h"
#include "mtk-reserved-memory.h"

#define CAP_NAME		"dsp-cap-pcm-dai"

#define MAX_CAP_BUF_SIZE	(4096*1024)

DEFINE_MUTEX(audio_idma_mutex);
DEFINE_MUTEX(cap_device_mutex);

static const char * const CAPTURE_ID_STRING[] = {"[HDMI_RX1_DEV]",
						"[HDMI_RX2_DEV]",
						"[ATV_DEV]",
						"[ADC_DEV]",
						"[ADC1_DEV]",
						"[I2S_RX2_DEV]",
						"[LOOPBACK_DEV]",
						"[HFP_RX_DEV]"};

static const unsigned short ATV_PRESCALE_STEP_TBL[] = {
	0x7E2B,		// -0.125dB
	0x7C5E,		// -0.25dB
	0x78D6,		// -0.5dB
	0x7214,		// -1dB
	0x65AC,		// -2dB
	0x50C3,		// -4dB
};

static void atv_get_origain(void);
static u32 atv_set_cmd(enum atv_comm_cmd cmd, long param1, long param2);
static void atv_set_threshold(enum atv_thr_type thr_type, unsigned int value);
static void atv_set_prescale(enum atv_gain_type gain_type, signed long db_value);

static unsigned int g_atv_gain_addr[10];
static unsigned short g_atv_gain[10];
static unsigned short g_atv_shift[10];
static unsigned int g_atv_record_std;
static signed int g_atv_record_nicam_gain;

static struct capture_priv *tasklet_priv;
static struct timespec _irq0TimeStamp;
static struct timespec _stTimeStamp;

static struct capture_register REG;

static unsigned int rx1_lifecycle_flag;
static unsigned long long rx1_mute_end_size;
static unsigned long long rx1_dsd_end_size;
static unsigned long long rx1_fs_end_size;
static unsigned char rx1_fs_previous_count;
static unsigned int rx2_lifecycle_flag;
static unsigned long long rx2_mute_end_size;
static unsigned long long rx2_dsd_end_size;
static unsigned long long rx2_fs_end_size;
static unsigned char rx2_fs_previous_count;

static unsigned int cap_rates[] = {
	RATE_48000,
};

static unsigned int hdmi_cap_rates[] = {
	RATE_32000,
	RATE_44100,
	RATE_48000,
	RATE_88200,
	RATE_96000,
	RATE_176400,
	RATE_192000,
};

static unsigned int cap_channels[] = {
	CH_2,
};

static unsigned int hdmi_cap_channels[] = {
	CH_2, CH_8,
};

static const struct snd_pcm_hw_constraint_list cap_rates_list = {
	.count = ARRAY_SIZE(cap_rates),
	.list = cap_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list cap_channels_list = {
	.count = ARRAY_SIZE(cap_channels),
	.list = cap_channels,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list hdmi_cap_rates_list = {
	.count = ARRAY_SIZE(hdmi_cap_rates),
	.list = hdmi_cap_rates,
	.mask = 0,
};

static const struct snd_pcm_hw_constraint_list hdmi_cap_channels_list = {
	.count = ARRAY_SIZE(hdmi_cap_channels),
	.list = hdmi_cap_channels,
	.mask = 0,
};

/* Default hardware settings of ALSA PCM playback */
static struct snd_pcm_hardware mtk_cap_default_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.channels_min = CH_2,
	.channels_max = CH_8,
	.buffer_bytes_max = MAX_CAP_BUF_SIZE,
	.period_bytes_min = 64,
	.period_bytes_max = 1024 * 64,
	.periods_min = 2,
	.periods_max = 64,
	.fifo_size = 0,
};

//[Sysfs] HELP command
static ssize_t help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (check_sysfs_null(dev, attr, buf))
		return 0;

	return snprintf(buf, PAGE_SIZE, "Debug Help:\n"
		"- captur_log_level\n"
		"	<W>[id][level]: Set device id & debug log level.\n"
		"	(0:HDMI_RX1, 1:HDMI_RX2, 2:ATV, 3:ADC, 4:ADC1, 5:I2S_RX2, 6:LOOPBACK, 7:HFP_RX ...)\n"
		"	(0:emerg, 1:alert, 2:critical, 3:error, 4:warn, 5:notice, 6:info, 7:debug)\n"
		"	<R>: Read all device debug log level.\n");
}

static ssize_t capture_log_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct capture_priv *priv = NULL;
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

	if ((id < CAP_DEV_NUM) && (log_level <= LOGLEVEL_DEBUG))
		priv->pcm_device[id]->log_level = log_level;

	return count;
}

static ssize_t capture_log_level_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;
	unsigned int id;
	int count = 0;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	for (id = 0; id < CAP_DEV_NUM; id++) {
		count += snprintf(buf + count, PAGE_SIZE, "%slog_level:%d\n",
			CAPTURE_ID_STRING[id], priv->pcm_device[id]->log_level);
	}
	return count;
}

static ssize_t capture_status_show(struct device *dev, struct device_attribute *attr,
				char *buf, struct capture_priv *priv, unsigned int id)
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

	substream = dev_runtime->substreams.c_substream;
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

static ssize_t hdmi1_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, HDMI_RX1_DEV);
}

static ssize_t hdmi2_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, HDMI_RX2_DEV);
}

static ssize_t atv_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, ATV_DEV);
}

static ssize_t adc_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, ADC_DEV);
}

static ssize_t adc1_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, ADC1_DEV);
}

static ssize_t i2s_rx2_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, I2S_RX2_DEV);
}

static ssize_t loopback_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, LOOPBACK_DEV);
}

static ssize_t hfp_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct capture_priv *priv = NULL;

	if (check_sysfs_null(dev, attr, buf))
		return 0;

	priv = dev_get_drvdata(dev);
	if (priv == NULL)
		return 0;

	return capture_status_show(dev, attr, buf, priv, HFP_RX_DEV);
}

static DEVICE_ATTR_RO(help);
static DEVICE_ATTR_RW(capture_log_level);
static DEVICE_ATTR_RO(hdmi1_status);
static DEVICE_ATTR_RO(hdmi2_status);
static DEVICE_ATTR_RO(atv_status);
static DEVICE_ATTR_RO(adc_status);
static DEVICE_ATTR_RO(adc1_status);
static DEVICE_ATTR_RO(i2s_rx2_status);
static DEVICE_ATTR_RO(loopback_status);
static DEVICE_ATTR_RO(hfp_status);

static struct attribute *cap_attrs[] = {
	&dev_attr_help.attr,
	&dev_attr_capture_log_level.attr,
	&dev_attr_hdmi1_status.attr,
	&dev_attr_atv_status.attr,
	&dev_attr_adc_status.attr,
	&dev_attr_adc1_status.attr,
	NULL,
};

static struct attribute *hdmi2_attrs[] = {
	&dev_attr_hdmi2_status.attr,
	NULL,
};

static struct attribute *i2s_rx2_attrs[] = {
	&dev_attr_i2s_rx2_status.attr,
	NULL,
};

static struct attribute *loopback_attrs[] = {
	&dev_attr_loopback_status.attr,
	NULL,
};

static struct attribute *hfp_attrs[] = {
	&dev_attr_hfp_status.attr,
	NULL,
};

static const struct attribute_group cap_attr_group = {
	.name = "mtk_dbg",
	.attrs = cap_attrs,
};

static const struct attribute_group hdmi2_attr_group = {
	.name = "mtk_dbg",
	.attrs = hdmi2_attrs,
};

static const struct attribute_group i2s_rx2_attr_group = {
	.name = "mtk_dbg",
	.attrs = i2s_rx2_attrs,
};

static const struct attribute_group loopback_attr_group = {
	.name = "mtk_dbg",
	.attrs = loopback_attrs,
};

static const struct attribute_group hfp_attr_group = {
	.name = "mtk_dbg",
	.attrs = hfp_attrs,
};

static const struct attribute_group *cap_attr_groups[] = {
	&cap_attr_group,
	NULL,
};

static int capture_reboot_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x20, 0x00);
	mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x04, 0x00);
	mtk_alsa_write_reg_mask_byte(REG.ATV_PALY_CMD, 0x01, 0x00);
	mtk_alsa_write_reg_mask(REG.CAP_DMA2_CTRL, 0x0001, 0x0000);
	mtk_alsa_write_reg_mask(REG.CAP_DMA4_CTRL, 0x0001, 0x0000);
	mtk_alsa_write_reg_mask(REG.CAP_DMA7_CTRL, 0x0001, 0x0000);
	mtk_alsa_write_reg_mask(REG.CAP_DMA8_CTRL, 0x0001, 0x0000);
	mtk_alsa_write_reg_mask(REG.CAP_DMA9_CTRL, 0x0001, 0x0000);
	mtk_alsa_write_reg_mask(REG.CAP_DMA10_CTRL, 0x0001, 0x0000);

	mtk_hdmi_rx1_clk_set_disable();
	mtk_hdmi_rx2_clk_set_disable();
	mtk_atv_clk_set_disable();
	mtk_adc0_clk_set_disable();
	mtk_adc1_clk_set_disable();
	mtk_i2s_rx2_clk_set_disable();

	return NOTIFY_OK;
}

/* reboot notifier */
static struct notifier_block alsa_capture_notifier = {
	.notifier_call = capture_reboot_notify,
};

static int capture_probe(struct snd_soc_component *component)
{
	struct capture_priv *priv;
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
	dev = component->dev;
	priv = snd_soc_component_get_drvdata(component);

	if (priv == NULL) {
		pr_err("[AUDIO][ERROR][%s]priv can't be NULL!!!\n", __func__);
		return -EFAULT;
	}

	/*Set default log level*/
	priv->pcm_device[HDMI_RX1_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[HDMI_RX2_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[ATV_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[ADC_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[ADC1_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[I2S_RX2_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[LOOPBACK_DEV]->log_level = LOGLEVEL_DEBUG;
	priv->pcm_device[HFP_RX_DEV]->log_level = LOGLEVEL_DEBUG;

	/* Create device attribute files */
	ret = sysfs_create_groups(&dev->kobj, cap_attr_groups);
	if (ret)
		dev_err(dev, "Audio capture Sysfs init fail\n");

	for_each_card_rtds(component->card, rtd) {
		dai_link = rtd->dai_link;

		if (dai_link->ignore == 0) {
			if (!strncmp(dai_link->cpus->dai_name, "HDMI-RX2", strlen("HDMI-RX2")))
				ret = sysfs_merge_group(&dev->kobj, &hdmi2_attr_group);
			else if (!strncmp(dai_link->cpus->dai_name, "I2S-RX2", strlen("I2S-RX2")))
				ret = sysfs_merge_group(&dev->kobj, &i2s_rx2_attr_group);
			else if (!strncmp(dai_link->cpus->dai_name, "LOOPBACK", strlen("LOOPBACK")))
				ret = sysfs_merge_group(&dev->kobj, &loopback_attr_group);
			else if (!strncmp(dai_link->cpus->dai_name, "HFP-RX", strlen("HFP-RX")))
				ret = sysfs_merge_group(&dev->kobj, &hfp_attr_group);
			else
				ret = 0;

			if (ret)
				dev_info(dev, "[AUDIO][%s]sysfs_merge_groups fail!\n", __func__);
		}
	}

	ret = register_reboot_notifier(&alsa_capture_notifier);
	if (ret) {
		dev_err(dev, "[ALSA CAP]register reboot notify fail %d\n", ret);
		return ret;
	}

	return 0;
}

static void capture_remove(struct snd_soc_component *component)
{
	struct device *dev;

	dev = NULL;

	if (component != NULL)
		dev = component->dev;

	if (dev != NULL) {
		sysfs_unmerge_group(&dev->kobj, &hfp_attr_group);
		sysfs_unmerge_group(&dev->kobj, &loopback_attr_group);
		sysfs_unmerge_group(&dev->kobj, &i2s_rx2_attr_group);
		sysfs_unmerge_group(&dev->kobj, &hdmi2_attr_group);
		sysfs_remove_groups(&dev->kobj, cap_attr_groups);
	}
}

static unsigned int prv_irq_num;
static unsigned int irq_write_lv;
static unsigned int irq_write_lv2;
void _tasklet_verify_hw(unsigned long idx)
{
	struct capture_priv *priv = tasklet_priv;
	int irq_dev = mtk_alsa_read_reg_byte(REG.IRQ0_IDX);
	int irq_num = mtk_alsa_read_reg_byte(REG.IRQ0_IDX + 1);
	struct mtk_runtime_struct *dev_runtime = NULL;
	struct mtk_dma_struct *capture = NULL;
	struct mtk_snd_timestamp *dev_ts = NULL;
	struct mtk_snd_timestamp_info *tsinfo = NULL;
	unsigned int pts_l = 0;
	unsigned int pts_h = 0;
	int size = DEFAULT_TIMESTAMP_SIZE * 4;
	unsigned long long total_write_size = 0;
	unsigned char *new_w_ptr = NULL;
	int offset_bytes = 0;

	if (irq_num != prv_irq_num) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP] missing interrupt!\n");
		return;
	}

	switch (irq_dev) {
	case 2:
		dev_runtime = priv->pcm_device[ATV_DEV];
		capture = &dev_runtime->capture;
		if (capture->format == SNDRV_PCM_FORMAT_S24_LE)
			size = DEFAULT_TIMESTAMP_SIZE * 8;
		break;
	case 4:
		dev_runtime = priv->pcm_device[ADC_DEV];
		capture = &dev_runtime->capture;
		if (capture->format == SNDRV_PCM_FORMAT_S24_LE)
			size = DEFAULT_TIMESTAMP_SIZE * 8;
		break;
	case 7:
		dev_runtime = priv->pcm_device[ADC1_DEV];
		capture = &dev_runtime->capture;
		if (capture->format == SNDRV_PCM_FORMAT_S24_LE)
			size = DEFAULT_TIMESTAMP_SIZE * 8;
		break;
	case 16:
		dev_runtime = priv->pcm_device[HDMI_RX1_DEV];
		capture = &dev_runtime->capture;
		new_w_ptr = capture->r_buf.addr + irq_write_lv;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;

		if (offset_bytes < 0)
			offset_bytes += capture->r_buf.size;
		total_write_size = capture->r_buf.total_write_size + offset_bytes;
		pts_l = mtk_alsa_read_reg(REG.HWTSG_TS_L);
		pts_h = mtk_alsa_read_reg(REG.HWTSG_TS_H);
		break;
	case 17:
		dev_runtime = priv->pcm_device[HDMI_RX2_DEV];
		capture = &dev_runtime->capture;
		new_w_ptr = capture->r_buf.addr + irq_write_lv2;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;

		if (offset_bytes < 0)
			offset_bytes += capture->r_buf.size;
		total_write_size = capture->r_buf.total_write_size + offset_bytes;
		pts_l = mtk_alsa_read_reg(REG.HWTSG2_TS_L);
		pts_h = mtk_alsa_read_reg(REG.HWTSG2_TS_H);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP] %sinvlid irq number %d!\n", __func__, irq_dev);
		break;
	}

	if (!dev_runtime)
		return;

	dev_ts = &dev_runtime->timestamp;

	if (dev_ts->ts_count < 510) {
		dev_ts->ts_count += 1;
		tsinfo = &dev_ts->ts_info[dev_ts->wr_index];
		if ((irq_dev == 16) || (irq_dev == 17)) {
			// 0x15555555 /* Max us */
			dev_ts->total_size = total_write_size;
			tsinfo->hts = (pts_l + (pts_h << 16)) / 12;
		} else {
			dev_ts->total_size += size;
			tsinfo->hts = 0;
		}
		tsinfo->kts.tv_sec = _irq0TimeStamp.tv_sec;
		tsinfo->kts.tv_nsec = _irq0TimeStamp.tv_nsec;
		tsinfo->byte_count = dev_ts->total_size;
		dev_ts->wr_index += 1;
		if (dev_ts->wr_index == 512)
			dev_ts->wr_index = 0;
	}
}

static irqreturn_t irq0_handler(int irq, void *irq_priv)
{
	struct capture_priv *priv = irq_priv;
	int irq_num = mtk_alsa_read_reg_byte(REG.IRQ0_IDX + 1);

	tasklet_priv = irq_priv;

	if (irq_num == prv_irq_num)
		return IRQ_HANDLED;

	prv_irq_num = irq_num;
	irq_write_lv = mtk_alsa_read_reg(REG.PAS_CTRL_73) * BYTES_IN_MIU_LINE;
	irq_write_lv2 = mtk_alsa_read_reg(REG.PAS_CTRL_74) * BYTES_IN_MIU_LINE;
	ktime_get_ts(&_irq0TimeStamp);
	tasklet_schedule(&priv->irq0_tasklet);

	return IRQ_HANDLED;
}

static irqreturn_t irq1_handler(int irq, void *irq_priv)
{
	// update global variable _stTimeStamp after DSP write xxx frames data to DDR
	ktime_get_ts(&_stTimeStamp);

	return IRQ_HANDLED;
}

static irqreturn_t irq2_handler(int irq, void *irq_priv)
{
	struct capture_priv *priv = irq_priv;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[HDMI_RX1_DEV];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	struct mtk_snd_timestamp_info *tsinfo = NULL;
	unsigned int irq_write_lv = 0;
	unsigned char *new_w_ptr = NULL;
	int offset_bytes = 0;
	unsigned long long total_write_size = 0;
	unsigned int pts_l = 0;
	unsigned int pts_h = 0;

	if (dev_ts->ts_count < 510) {
		pts_l = mtk_alsa_read_reg(REG.HWTSG_TS_L);
		pts_h = mtk_alsa_read_reg(REG.HWTSG_TS_H);
		dev_ts->ts_count += 1;
		tsinfo = &dev_ts->ts_info[dev_ts->wr_index];
		ktime_get_ts(&tsinfo->kts);
		tsinfo->hts = (pts_l + (pts_h << 16)) / 12;
		irq_write_lv = mtk_alsa_read_reg(REG.PAS_CTRL_73) * BYTES_IN_MIU_LINE;
		new_w_ptr = capture->r_buf.addr + irq_write_lv;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		if (offset_bytes < 0)
			offset_bytes += capture->r_buf.size;
		total_write_size = capture->r_buf.total_write_size + offset_bytes;
		tsinfo->byte_count = total_write_size;
		dev_ts->total_size = total_write_size;
		dev_ts->wr_index += 1;
		if (dev_ts->wr_index == 512)
			dev_ts->wr_index = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t irq3_handler(int irq, void *irq_priv)
{
	struct capture_priv *priv = irq_priv;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[HDMI_RX2_DEV];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	struct mtk_snd_timestamp_info *tsinfo = NULL;
	unsigned int irq_write_lv = 0;
	unsigned char *new_w_ptr = NULL;
	int offset_bytes = 0;
	unsigned long long total_write_size = 0;
	unsigned int pts_l = 0;
	unsigned int pts_h = 0;

	if (dev_ts->ts_count < 510) {
		pts_l = mtk_alsa_read_reg(REG.HWTSG2_TS_L);
		pts_h = mtk_alsa_read_reg(REG.HWTSG2_TS_H);
		dev_ts->ts_count += 1;
		tsinfo = &dev_ts->ts_info[dev_ts->wr_index];
		ktime_get_ts(&tsinfo->kts);
		tsinfo->hts = (pts_l + (pts_h << 16)) / 12;
		irq_write_lv = mtk_alsa_read_reg(REG.PAS_CTRL_74) * BYTES_IN_MIU_LINE;
		new_w_ptr = capture->r_buf.addr + irq_write_lv;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		if (offset_bytes < 0)
			offset_bytes += capture->r_buf.size;
		total_write_size = capture->r_buf.total_write_size + offset_bytes;
		tsinfo->byte_count = total_write_size;
		dev_ts->total_size = total_write_size;
		dev_ts->wr_index += 1;
		if (dev_ts->wr_index == 512)
			dev_ts->wr_index = 0;
	}
	return IRQ_HANDLED;
}

void parser_init(struct capture_priv *priv, unsigned int id)
{
	// Set SE-DMA ES3 offset address & size
	if (id == HDMI_RX1_DEV) {
		mtk_alsa_write_reg_mask_byte(REG.SE_MCFG, 0xFF, 0x02);
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF,
			((priv->cap_dma_offset[id] >> BYTES_IN_MIU_LINE_LOG2) & 0xFF));
		mtk_alsa_write_reg(REG.SE_MBASE_H,
			((priv->cap_dma_offset[id] >> BYTES_IN_MIU_LINE_LOG2) >> 8));
		mtk_alsa_write_reg_mask_byte(REG.SE_MBASE_EXT, 0xFF,
			(((priv->cap_dma_offset[id] >> (BYTES_IN_MIU_LINE_LOG2 + 24)) & 0xFF)));
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF00,
			((priv->cap_dma_size[id] >> (BYTES_IN_MIU_LINE_LOG2)) - 1));

		// Switch to MCU control
		// [2] RIU control parser A enable
		// [6] CPU allow audio R2 control
		mtk_alsa_write_reg_mask(REG.PAS_CTRL_0, 0x0044, 0x0004);
		// [8] R2_ES1_SIGN: update lead counter increase/decrease
		// [15] R2ES1_MODE: 1: ES1_CNT is controlled by R2/MCU
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x81, 0x81);

		// SE-DSP(DEC3) DMA Switch to HDMI source
		mtk_alsa_write_reg_mask_byte(REG.DECODER1_CFG + 1, 0x87, 0x84);

		// STOP and reset: 0xB0008200
		// [5] ES1W disable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x20, 0x00);
		mtk_alsa_delaytask(1);
		// [3] MAD ES1 DRAM write high priority enable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x08, 0x08);

		// [12] clear ES Full
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x10, 0x10);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x10, 0x00);

		// [13] reset ES level
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x20, 0x20);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x20, 0x00);

		// [11] reset lockstatus
		mtk_alsa_write_reg_mask_byte(REG.HDMI_CFG_6 + 1, 0x08, 0x08);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.HDMI_CFG_6 + 1, 0x08, 0x00);
	} else {
		mtk_alsa_write_reg_mask_byte(REG.SE_MCFG, 0xFF, 0x03);
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF,
			((priv->cap_dma_offset[id] >> BYTES_IN_MIU_LINE_LOG2) & 0xFF));
		mtk_alsa_write_reg(REG.SE_MBASE_H,
			((priv->cap_dma_offset[id] >> BYTES_IN_MIU_LINE_LOG2) >> 8));
		mtk_alsa_write_reg_mask_byte(REG.SE_MBASE_EXT, 0xFF,
			(((priv->cap_dma_offset[id] >> (BYTES_IN_MIU_LINE_LOG2 + 24)) & 0xFF)));
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF00,
			((priv->cap_dma_size[id] >> (BYTES_IN_MIU_LINE_LOG2)) - 1));

		// Switch to MCU control
		// [3] RIU control parser B enable
		// [7] CPU allow audio R2 control
		mtk_alsa_write_reg_mask(REG.PAS_CTRL_0, 0x0088, 0x0008);
		// [8] R2_ES2_SIGN: update lead counter increase/decrease
		// [15] R2ES2_MODE: 1: ES2_CNT is controlled by R2/MCU
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x81, 0x81);

		// SE-DSP(DEC4) DMA Switch to HDMI2 source
		mtk_alsa_write_reg_mask_byte(REG.DECODER2_CFG + 1, 0x87, 0x86);

		// STOP and reset: 0xB0008200
		// [2] ES2W disable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x04, 0x00);
		mtk_alsa_delaytask(1);
		// [1] MAD ES2 DRAM write high priority enable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x02, 0x02);

		// [12] clear ES Full
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x10, 0x10);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x10, 0x00);

		// [13] reset ES level
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x20, 0x20);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x20, 0x00);

		// [13] reset lockstatus
		mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG1 + 1, 0x20, 0x20);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG1 + 1, 0x20, 0x00);
	}
}

void parser_start(struct mtk_dma_struct *capture, int flag, unsigned int id)
{
	if (id == HDMI_RX1_DEV) {
		u16 pc_value = mtk_alsa_read_reg(REG.STATUS_HDMI_PC);

		// [5] Bypass mode
		mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x20, 0x20);

		// [4] Byte mode
		if ((pc_value == 0x11) || (pc_value == 0x15) || (pc_value == 0x16))
			mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x10, 0x10);
		else
			mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x10, 0x00);

		// [5] ES1W enable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x20, 0x20);

		if (flag) {
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL, 0x80, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG1, 0xC0, 0x00);
		} else {
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL, 0x80, 0x80);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG1, 0x40, 0x40);
			if (capture->channel_mode == CH_8)
				mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG2, 0x30, 0x30);
			else
				mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG2, 0x30, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG1, 0x80, 0x80);
		}
	} else {
		u16 pc_value = mtk_alsa_read_reg(REG.STATUS_HDMI2_PC);

		// [12] Bypass mode
		mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG1 + 1, 0x10, 0x10);

		// [6] Byte mode
		if ((pc_value == 0x11) || (pc_value == 0x15) || (pc_value == 0x16))
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG2, 0x40, 0x40);
		else
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG2, 0x40, 0x00);

		// [2] ES2W enable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x04, 0x04);

		if (flag) {
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL + 1, 0x80, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG1, 0xC0, 0x00);
		} else {
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL + 1, 0x80, 0x80);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG1, 0x40, 0x40);
			if (capture->channel_mode == CH_8)
				mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG2, 0x30, 0x30);
			else
				mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG2, 0x30, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG1, 0x80, 0x80);
		}
	}
}

void parser_stop(struct mtk_dma_struct *capture, unsigned int id)
{
	unsigned int tmp_bytes = 0;

	if (id == HDMI_RX1_DEV) {
		// [5] ES1W disable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x20, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x30, 0x00);

		tmp_bytes = mtk_alsa_read_reg(REG.PAS_CTRL_49);
		mtk_alsa_write_reg(REG.PAS_CTRL_50, (tmp_bytes / 2));

		//[9] R2_ES1_UPD
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x02, 0x02);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x02, 0x00);

		// stop interrupt
		mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL, 0xFF, 0x00);

		rx1_mute_end_size = 0;
		rx1_dsd_end_size = 0;
		rx1_fs_end_size = 0;
		rx1_fs_previous_count = 0;
	} else {
		// [2] ES2W disable
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_39, 0x04, 0x00);

		mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG1 + 1, 0x10, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG2, 0x40, 0x00);

		tmp_bytes = mtk_alsa_read_reg(REG.PAS_CTRL_65);
		mtk_alsa_write_reg(REG.PAS_CTRL_66, (tmp_bytes / 2));

		//[9] R2_ES1_UPD
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x02, 0x02);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x02, 0x00);

		// stop interrupt
		mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL + 1, 0xFF, 0x00);

		rx2_mute_end_size = 0;
		rx2_dsd_end_size = 0;
		rx2_fs_end_size = 0;
		rx2_fs_previous_count = 0;
	}
}

static unsigned int parser_read(struct mtk_dma_struct *capture,
				void *buffer, unsigned int bytes, unsigned int id)
{
	unsigned int rest_size_to_end = 0;
	unsigned int tmp_size = 0;
	unsigned int tmp_bytes = 0;

	rest_size_to_end = capture->r_buf.buf_end - capture->r_buf.r_ptr;
	tmp_bytes = (bytes >> 4) << 4;
	tmp_size = (rest_size_to_end > tmp_bytes) ? tmp_bytes : rest_size_to_end;
	if (id == HDMI_RX1_DEV)
		tmp_bytes = mtk_alsa_read_reg(REG.PAS_CTRL_49) * BYTES_IN_MIU_LINE;
	else
		tmp_bytes = mtk_alsa_read_reg(REG.PAS_CTRL_65) * BYTES_IN_MIU_LINE;
	tmp_size = (tmp_size > tmp_bytes) ? tmp_bytes : tmp_size;

	if (tmp_size == 0)
		return 0;

	memcpy(buffer, capture->r_buf.r_ptr, tmp_size);

	capture->r_buf.r_ptr += tmp_size;

	if (capture->r_buf.r_ptr == capture->r_buf.buf_end)
		capture->r_buf.r_ptr = capture->r_buf.addr;

	smp_mb();

	if (id == HDMI_RX1_DEV) {
		mtk_alsa_write_reg(REG.PAS_CTRL_50, (tmp_size / (2 * BYTES_IN_MIU_LINE)));
		//[9] R2_ES1_UPD
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x02, 0x02);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x02, 0x00);
	} else {
		mtk_alsa_write_reg(REG.PAS_CTRL_66, (tmp_size / (2 * BYTES_IN_MIU_LINE)));
		//[9] R2_ES1_UPD
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x02, 0x02);
		mtk_alsa_delaytask_us(50);
		mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x02, 0x00);
	}

	mtk_alsa_delaytask(1);

	capture->status = CMD_START;

	return tmp_size;
}

static unsigned int dma_read(struct mtk_dma_struct *capture,
					void *buffer, unsigned int bytes, unsigned int id)
{
	unsigned int rest_size_to_end = 0;
	unsigned int tmp_size = 0;
	unsigned int ptr_offset = 0;
	unsigned int tmp_bytes = 0;
	unsigned int REG_CAP_DMA_RPTR = 0;

	switch (id) {
	case ATV_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA2_RPTR;
		break;
	case ADC_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA4_RPTR;
		break;
	case ADC1_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA7_RPTR;
		break;
	case I2S_RX2_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA8_RPTR;
		break;
	case LOOPBACK_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA9_RPTR;
		break;
	case HFP_RX_DEV:
		REG_CAP_DMA_RPTR = REG.CAP_DMA10_RPTR;
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP] %sinvlid pcm device %u!\n", __func__, id);
		break;
	}

	rest_size_to_end = capture->r_buf.buf_end - capture->r_buf.r_ptr;
	tmp_bytes = (bytes >> 4) << 4;
	tmp_size = (rest_size_to_end > tmp_bytes) ? tmp_bytes : rest_size_to_end;

	if (tmp_size == 0)
		return 0;

	memcpy(buffer, capture->r_buf.r_ptr, tmp_size);

	capture->r_buf.r_ptr += tmp_size;

	if (capture->r_buf.r_ptr == capture->r_buf.buf_end)
		capture->r_buf.r_ptr = capture->r_buf.addr;

	smp_mb();

	ptr_offset = capture->r_buf.r_ptr - capture->r_buf.addr;
	mtk_alsa_write_reg(REG_CAP_DMA_RPTR, (ptr_offset / BYTES_IN_MIU_LINE));

	capture->status = CMD_START;

	return tmp_size;
}

void capture_init(struct mtk_dma_struct *capture, unsigned int id)
{
	unsigned int REG_CAP_DMA_CTRL = 0;

	switch (id) {
	case ATV_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA2_CTRL;
		// DSP ISR timestamp interval : 1024 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, 0x03);
		break;
	case ADC_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA4_CTRL;
		// DSP ISR timestamp interval : 1024 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, 0x03);
		break;
	case ADC1_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA7_CTRL;
		// DSP ISR timestamp interval : 1024 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, 0x03);
		break;
	case I2S_RX2_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA8_CTRL;
		// DSP ISR timestamp interval : 4096 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, 0x05);
		break;
	case LOOPBACK_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA9_CTRL;
		// DSP ISR timestamp interval : 256 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, 0x01);
		break;
	case HFP_RX_DEV:
		REG_CAP_DMA_CTRL = REG.CAP_DMA10_CTRL;
		// DSP ISR timestamp interval : 1024 frames
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL + 1, 0x0F, (AU_BIT0 | AU_BIT1));
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP] %sinvlid pcm device %u!\n", __func__, id);
		break;
	}

	if (capture->format == SNDRV_PCM_FORMAT_S24_LE)
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL, CAP_FORMAT_MASK, CAP_FORMAT_24BIT);
	else
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL, CAP_FORMAT_MASK, CAP_FORMAT_16BIT);

	if (capture->channel_mode == CH_2)
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL, CAP_CHANNEL_MASK, CAP_CHANNEL_2);
	else
		mtk_alsa_write_reg_mask_byte(REG_CAP_DMA_CTRL, CAP_CHANNEL_MASK, CAP_CHANNEL_8);
}

static int cap_dma_set_mem_info(struct capture_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *pcm_stream = &dev_runtime->capture;
	unsigned int buffer_size = 0;
	ptrdiff_t dma_base_pa = 0;
	ptrdiff_t dma_base_ba = 0;
	ptrdiff_t dma_base_va = pcm_stream->str_mode_info.virtual_addr;
	unsigned int offset = priv->cap_dma_offset[id];
	unsigned int size = priv->cap_dma_size[id];

	if ((offset == 0) || (size == 0))
		return -ENXIO;

	buffer_size = size;
	pcm_stream->r_buf.size = buffer_size;
	pcm_stream->high_threshold =
			pcm_stream->r_buf.size - (pcm_stream->r_buf.size >> 3);

	if ((pcm_stream->initialized_status != 1) ||
		(pcm_stream->status != CMD_RESUME)) {
		dma_base_pa = priv->mem_info.bus_addr -
				priv->mem_info.memory_bus_base + offset;
		dma_base_ba = priv->mem_info.bus_addr + offset;

		if (dma_base_ba % 0x1000) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA CAP]%sInvalid bus addr,should align 4KB\n",
				CAPTURE_ID_STRING[id]);
			return -EFAULT;
		}

		/* convert Bus Address to Virtual Address */
		if (dma_base_va == 0) {
			dma_base_va = (ptrdiff_t)ioremap_wc(dma_base_ba,
						pcm_stream->r_buf.size);
			if (dma_base_va == 0) {
				adev_err(priv->dev, dev_runtime->log_level,
					"[ALSA CAP]%sfail to convert Bus Addr\n",
					CAPTURE_ID_STRING[id]);
				return -ENOMEM;
			}
		}
		pcm_stream->str_mode_info.physical_addr = dma_base_pa;
		pcm_stream->str_mode_info.bus_addr = dma_base_ba;
		pcm_stream->str_mode_info.virtual_addr = dma_base_va;

		pcm_stream->initialized_status = 1;
	} else {
		dma_base_pa = pcm_stream->str_mode_info.physical_addr;
		dma_base_ba = pcm_stream->str_mode_info.bus_addr;
		dma_base_va = pcm_stream->str_mode_info.virtual_addr;
	}

	pcm_stream->r_buf.addr = (unsigned char *)dma_base_va;
	pcm_stream->r_buf.r_ptr = pcm_stream->r_buf.addr;
	pcm_stream->r_buf.w_ptr = pcm_stream->r_buf.addr;
	pcm_stream->r_buf.buf_end = pcm_stream->r_buf.addr + buffer_size;
	pcm_stream->r_buf.total_read_size = 0;
	pcm_stream->r_buf.total_write_size = 0;
	pcm_stream->copy_size = 0;

	memset((void *)pcm_stream->r_buf.addr, 0x00, pcm_stream->r_buf.size);
	smp_mb();

	return 0;
}

void cap_dma_init(struct capture_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	struct mtk_snd_timestamp_info *tsinfo = &dev_ts->ts_info[0];

	dev_ts->wr_index = 1;
	dev_ts->rd_index = 0;
	dev_ts->ts_count = 0;
	dev_ts->total_size = 0;
	tsinfo->byte_count = 0;

	switch (id) {
	case HDMI_RX1_DEV:
		// Mute event prepare flow
		mtk_alsa_write_reg_mask_byte(REG.HDMI_EVENT_2, 0x02, 0x02);
		parser_init(priv, id);
		parser_start(capture, rx1_lifecycle_flag, id);
		rx1_mute_end_size = 0;
		rx1_dsd_end_size = 0;
		rx1_fs_end_size = 0;
		rx1_fs_previous_count = 0;
		break;
	case HDMI_RX2_DEV:
		// Mute event prepare flow
		mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_2 + 1, 0x02, 0x02);
		parser_init(priv, id);
		parser_start(capture, rx2_lifecycle_flag, id);
		rx2_mute_end_size = 0;
		rx2_dsd_end_size = 0;
		rx2_fs_end_size = 0;
		rx2_fs_previous_count = 0;
		break;
	case ATV_DEV:
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA2_CTRL, 0x0001, 0x0001);
		break;
	case ADC_DEV:
		mtk_alsa_write_reg_mask_byte(REG.ANALOG_MUX + 1, 0xF0, 0x00);
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA4_CTRL, 0x0001, 0x0001);
		break;
	case ADC1_DEV:
		mtk_alsa_write_reg_mask_byte(REG.ANALOG_MUX, 0x0F, 0x01);
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA7_CTRL, 0x0001, 0x0001);
		break;
	case I2S_RX2_DEV:
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA8_CTRL, 0x0001, 0x0001);
		break;
	case LOOPBACK_DEV:
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x00);
		// I2S out(BCK and WS) clock bypass to I2S in
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG + 1, 0x10, 0x10);
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA9_CTRL, 0x0001, 0x0001);
		// For blocking mode, alsa core will only call snd_pcm_ops .pointer one times.
		// If the hw_ptr is 0 at that time then it will directly close device.
		// Let DSP buffer quene some datas to prevent the hw_ptr level is 0.
		mtk_alsa_delaytask(1);
		break;
	case HFP_RX_DEV:
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x00);
		// I2S TX wck and ws bypass to I2S RX
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG+1, 0x10, 0x10);
		capture_init(capture, id);
		mtk_alsa_write_reg_mask(REG.CAP_DMA10_CTRL, 0x0001, 0x0001);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]invalid pcm device init %u\n", id);
		break;
	}
}

static int cap_dma_stop(struct capture_priv *priv, unsigned int id)
{
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	struct mtk_snd_timestamp_info *tsinfo = &dev_ts->ts_info[0];

	/* Reset some variables */
	dev_runtime->substreams.substream_status = CMD_STOP;
	dev_runtime->substreams.c_substream = NULL;
	dev_runtime->device_cycle_count = 0;

	dev_ts->wr_index = 1;
	dev_ts->rd_index = 0;
	dev_ts->ts_count = 0;
	dev_ts->total_size = 0;
	tsinfo->byte_count = 0;

	/* Stop DMA Writer  */
	switch (id) {
	case HDMI_RX1_DEV:
	case HDMI_RX2_DEV:
		parser_stop(capture, id);
		break;
	case ATV_DEV:
		/* ATV stop cmd */
		mtk_alsa_write_reg_mask_byte(REG.ATV_PALY_CMD, 0x01, 0x00);
		mtk_alsa_write_reg_mask(REG.CAP_DMA2_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA2_RPTR, 0x0000);
		break;
	case ADC_DEV:
		mtk_alsa_write_reg_mask(REG.CAP_DMA4_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA4_RPTR, 0x0000);
		break;
	case ADC1_DEV:
		mtk_alsa_write_reg_mask(REG.CAP_DMA7_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA7_RPTR, 0x0000);
		break;
	case I2S_RX2_DEV:
		mtk_alsa_write_reg_mask(REG.CAP_DMA8_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA8_RPTR, 0x0000);
		break;
	case LOOPBACK_DEV:
		mtk_alsa_write_reg_mask(REG.CAP_DMA9_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA9_RPTR, 0x0000);
		break;
	case HFP_RX_DEV:
		mtk_alsa_write_reg_mask(REG.CAP_DMA10_CTRL, 0x0001, 0x0000);
		mtk_alsa_write_reg(REG.CAP_DMA10_RPTR, 0x0000);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]invalid pcm device stop %u\n", id);
		break;
	}

	/* reset Read & Write Pointer */
	capture->r_buf.r_ptr = capture->r_buf.addr;
	capture->r_buf.w_ptr = capture->r_buf.addr;
	capture->r_buf.total_read_size = 0;
	capture->r_buf.total_write_size = 0;

	capture->status = CMD_STOP;

	return 0;
}

static int cap_dma_exit(struct mtk_dma_struct *pcm_stream)
{
	ptrdiff_t dma_base_va = pcm_stream->str_mode_info.virtual_addr;

	if (dma_base_va != 0) {
		if (pcm_stream->r_buf.addr) {
			iounmap((void *)pcm_stream->r_buf.addr);
			pcm_stream->r_buf.addr = 0;
			pcm_stream->r_buf.buf_end = 0;
		} else {
			apr_err(MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]buffer address should not be 0 !\n");
		}
		pcm_stream->str_mode_info.virtual_addr = 0;
	}
	pcm_stream->sample_rate = 0;
	pcm_stream->channel_mode = 0;
	pcm_stream->status = CMD_STOP;
	pcm_stream->initialized_status = 0;

	return 0;
}

static int cap_dma_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);
	return 0;
}

static int cap_dma_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int cap_dma_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);

	return 0;
}

static int cap_dma_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int err = 0;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);

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
		adev_info(priv->dev, MTK_ALSA_LEVEL_INFO,
			"[ALSA CAP]%sInvalid trigger command %d\n",
			CAPTURE_ID_STRING[id], cmd);
		err = -EINVAL;
		break;
	}

	return err;
}

static int cap_dma_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	int err = 0;

	dev_runtime->device_cycle_count = mtk_alsa_get_hw_reboot_cycle();

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);

	/*print hw parameter*/
	adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR, "%sparameter :\n"
								"format is %d\n"
								"rate is %d\n"
								"channels is %d\n"
								"sample_bits is %d\n",
								CAPTURE_ID_STRING[id],
								runtime->format,
								runtime->rate,
								runtime->channels,
								runtime->sample_bits);

	if ((capture->status != CMD_START) ||
		(capture->initialized_status == 0)) {
		capture->channel_mode = runtime->channels;
		capture->format = runtime->format;
		/* Open SW DMA */
		err = cap_dma_set_mem_info(priv, id);
		if (err != 0)
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sErr! prepare init fail\n",
				CAPTURE_ID_STRING[id]);
		else
			cap_dma_init(priv, id);

		capture->status = CMD_START;
	}

	return err;
}

static void cap_dma_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	unsigned long flags;

	adev_info(dai->dev, dev_runtime->log_level, "%s[%s:%d] !!!\n",
		CAPTURE_ID_STRING[id], __func__, __LINE__);

	timer_close(&dev_runtime->timer);

	spin_lock_irqsave(&spinlock->lock, flags);
	memset(&dev_runtime->monitor, 0x00, sizeof(struct mtk_monitor_struct));
	spin_unlock_irqrestore(&spinlock->lock, flags);

	cap_dma_stop(priv, id);
	cap_dma_exit(capture);
}

static const struct snd_soc_dai_ops cap_dma_dai_ops = {
	.startup	= cap_dma_startup,
	.shutdown	= cap_dma_shutdown,
	.hw_params	= cap_dma_hw_params,
	.hw_free	= cap_dma_hw_free,
	.prepare	= cap_dma_prepare,
	.trigger	= cap_dma_trigger,
};

int capture_dai_suspend(struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *capture = &dev_runtime->capture;

	adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
		"[ALSA CAP]%sdevice suspend\n", CAPTURE_ID_STRING[id]);

	if (capture->status != CMD_STOP) {
		cap_dma_stop(priv, id);
		capture->status = CMD_SUSPEND;
	}

	if (dev_runtime->clk_status) {
		switch (id) {
		case HDMI_RX1_DEV:
			mtk_hdmi_rx1_clk_set_disable();
			break;
		case HDMI_RX2_DEV:
			mtk_hdmi_rx2_clk_set_disable();
			break;
		case ATV_DEV:
			mtk_atv_clk_set_disable();
			break;
		case ADC_DEV:
			mtk_adc0_clk_set_disable();
			break;
		case ADC1_DEV:
			mtk_adc1_clk_set_disable();
			break;
		case I2S_RX2_DEV:
			mtk_i2s_rx2_clk_set_disable();
			break;
		case LOOPBACK_DEV:
			break;
		case HFP_RX_DEV:
			break;
		default:
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]invalid pcm device %u suspend\n", id);
			break;
		}
		dev_runtime->clk_status = 0;
	}

	return 0;
}

int capture_dai_resume(struct snd_soc_dai *dai)
{
	struct capture_priv *priv = snd_soc_dai_get_drvdata(dai);
	unsigned int id = dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	int err = 0;

	adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
		"[ALSA CAP]%sdevice resume\n", CAPTURE_ID_STRING[id]);

	if (capture->status == CMD_SUSPEND) {
		capture->status = CMD_RESUME;
		/* Open SW DMA */
		err = cap_dma_set_mem_info(priv, id);
		if (err != 0)
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sErr! set memory init fail\n",
				CAPTURE_ID_STRING[id]);
		cap_dma_init(priv, id);
		capture->status = CMD_START;
	}

	if (dev_runtime->clk_status == 0) {
		switch (id) {
			case HDMI_RX1_DEV:
				mtk_hdmi_rx1_clk_set_enable();
				break;
			case HDMI_RX2_DEV:
				mtk_hdmi_rx2_clk_set_enable();
				break;
			case ATV_DEV:
				mtk_atv_clk_set_enable();
				break;
			case ADC_DEV:
				mtk_adc0_clk_set_enable();
				break;
			case ADC1_DEV:
				mtk_adc1_clk_set_enable();
				break;
			case I2S_RX2_DEV:
				mtk_i2s_rx2_clk_set_enable();
				break;
			case LOOPBACK_DEV:
				break;
			case HFP_RX_DEV:
				break;
			default:
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]invalid pcm device %u resume\n", id);
				break;
		}
		dev_runtime->clk_status = 1;
	}

	return 0;
}

static struct snd_soc_dai_driver cap_dais[] = {
	{
		.name = "HDMI-RX1",
		.id = HDMI_RX1_DEV,
		.capture = {
			.stream_name = "HDMI-RX1 CAPTURE",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "HDMI-RX2",
		.id = HDMI_RX2_DEV,
		.capture = {
			.stream_name = "HDMI-RX2 CAPTURE",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "ATV",
		.id = ATV_DEV,
		.capture = {
			.stream_name = "ATV CAPTURE",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "ADC",
		.id = ADC_DEV,
		.capture = {
			.stream_name = "ADC Capture",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "ADC1",
		.id = ADC1_DEV,
		.capture = {
			.stream_name = "ADC1 Capture",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "I2S-RX2",
		.id = I2S_RX2_DEV,
		.capture = {
			.stream_name = "I2S_RX2 Capture",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "LOOPBACK",
		.id = LOOPBACK_DEV,
		.capture = {
			.stream_name = "LOOPBACK Capture",
			.channels_min = CH_2,
			.channels_max = CH_8,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
	{
		.name = "HFP-RX",
		.id = HFP_RX_DEV,
		.capture = {
			.stream_name = "Hand-Free Profile Capture",
			.channels_min = CH_2,
			.channels_max = CH_2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
						SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &cap_dma_dai_ops,
		.suspend = capture_dai_suspend,
		.resume = capture_dai_resume,
	},
};

static int cap_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, CAP_NAME);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	int ret;
	unsigned int time_out;
	unsigned int index_ga;
	unsigned int index_thr;

	/* Set specified information */
	dev_runtime->substreams.c_substream = substream;

	snd_soc_set_runtime_hwparams(substream, &mtk_cap_default_hw);

	if ((id == HDMI_RX1_DEV) || (id == HDMI_RX2_DEV)) {
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&hdmi_cap_rates_list);
		if (ret < 0)
			return ret;

		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&hdmi_cap_channels_list);
		if (ret < 0)
			return ret;
	} else {
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&cap_rates_list);
		if (ret < 0)
			return ret;

		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS,
			&cap_channels_list);
		if (ret < 0)
			return ret;
	}

	if (capture->c_buf.addr == NULL)
		return -ENOMEM;

	memset((void *)capture->c_buf.addr, 0x00, MAX_CAP_BUF_SIZE);
	capture->c_buf.size = MAX_CAP_BUF_SIZE;

	if (spinlock->init_status == 0) {
		spin_lock_init(&spinlock->lock);
		spinlock->init_status = 1;
	}

	timer_open(&dev_runtime->timer, (void *)timer_monitor);

	mutex_lock(&cap_device_mutex);
	// clock init
	switch (id) {
	case HDMI_RX1_DEV:
		mtk_hdmi_rx1_clk_set_enable();
		rx1_lifecycle_flag = priv->hdmi_rx1_nonpcm_flag;
		if (rx1_lifecycle_flag) {
			if (priv->irq2 < 0) {
				ret = -EINVAL;
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to get irq2\n", CAPTURE_ID_STRING[id]);
			} else {
				ret = devm_request_irq(priv->dev, priv->irq2, irq2_handler,
					IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
				if (ret < 0)
					adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
						"[ALSA CAP]%sfail to request irq2\n",
						CAPTURE_ID_STRING[id]);
			}
			// bit[14]: "1" 2byte mode, "0" PAPB
			// bit[12]: "1" interrupt with PD, "0" interrupt with [14]
			mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG4 + 1, 0x50, 0x50);
		} else {
			if (priv->irq0 < 0) {
				ret = -EINVAL;
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to get irq0\n", CAPTURE_ID_STRING[id]);
				break;
			} else if (priv->irq0_counter == 0) {
				// MB112E00
				ret = devm_request_irq(priv->dev, priv->irq0, irq0_handler,
					IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
				if (ret < 0)
					adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
						"[ALSA CAP]%sfail to request irq0\n",
						CAPTURE_ID_STRING[id]);
				tasklet_enable(&priv->irq0_tasklet);
			}
			priv->irq0_counter += 1;
			mtk_alsa_write_reg_mask_byte(REG.HWTSG_CFG4 + 1, 0x50, 0x40);
		}
		break;
	case HDMI_RX2_DEV:
		mtk_hdmi_rx2_clk_set_enable();
		rx2_lifecycle_flag = priv->hdmi_rx2_nonpcm_flag;
		if (rx2_lifecycle_flag) {
			if (priv->irq3 < 0) {
				ret = -EINVAL;
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to get irq3\n", CAPTURE_ID_STRING[id]);
			} else {
				ret = devm_request_irq(priv->dev, priv->irq3, irq3_handler,
					IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
				if (ret < 0)
					adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
						"[ALSA CAP]%sfail to request irq3\n",
						CAPTURE_ID_STRING[id]);
			}
			// bit[14]: "1" 2byte mode, "0" PAPB
			// bit[12]: "1" interrupt with PD, "0" interrupt with [14]
			mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG4 + 1, 0x50, 0x50);
		} else {
			if (priv->irq0 < 0) {
				ret = -EINVAL;
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to get irq0\n", CAPTURE_ID_STRING[id]);
				break;
			} else if (priv->irq0_counter == 0) {
				// MB112E00
				ret = devm_request_irq(priv->dev, priv->irq0, irq0_handler,
					IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
				if (ret < 0)
					adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
						"[ALSA CAP]%sfail to request irq0\n",
						CAPTURE_ID_STRING[id]);
				tasklet_enable(&priv->irq0_tasklet);
			}
			priv->irq0_counter += 1;
			mtk_alsa_write_reg_mask_byte(REG.HWTSG2_CFG4 + 1, 0x50, 0x40);
		}
		break;
	case ATV_DEV:
		mtk_atv_clk_set_enable();
		if (priv->irq0 < 0) {
			ret = -EINVAL;
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sfail to get irq0\n", CAPTURE_ID_STRING[id]);
			break;
		} else if (priv->irq0_counter == 0) {
			// MB112E00
			ret = devm_request_irq(priv->dev, priv->irq0, irq0_handler,
			IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
			if (ret < 0)
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to request irq0\n",
					CAPTURE_ID_STRING[id]);
			tasklet_enable(&priv->irq0_tasklet);
		}
		priv->irq0_counter += 1;

		/* SIFDMA config (SE-DSP) */
		mtk_alsa_write_reg_mask_byte(REG.SE_MCFG, 0xFF, 0x00);
		mtk_alsa_write_reg_mask_byte(REG.SE_MBASE_H, 0xFF, 0x08);
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF00, 0x700);

		mtk_alsa_write_reg_mask_byte(REG.SE_MCFG, 0xFF, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.SE_MBASE_H, 0xFF, 0x10);
		mtk_alsa_write_reg_mask(REG.SE_MSIZE_H, 0xFF00, 0x700);

		mtk_alsa_write_reg_mask(REG.DBG_CMD_1, 0xFFFF, 0xF900);

		// /* Trigger PIO[8] to inform DSP */
		mtk_alsa_write_reg_mask_byte(REG.SE_IDMA_CTRL_0, 0x20, 0x20);
		mtk_alsa_delaytask(2);
		mtk_alsa_write_reg_mask_byte(REG.SE_IDMA_CTRL_0, 0x20, 0x00);

		time_out = 0;
		while (time_out++ < 3000) {
			if (mtk_alsa_read_reg_byte(REG.DBG_RESULT_2) == 0x77)
				break;
			mtk_alsa_delaytask(1);
		}

		if (time_out >= 3000) {
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA ATV]DSP Reload timeOut2: \r\n");
		} else {
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA ATV]DSP Palsum Reload Code Success..\n");
			mtk_alsa_write_reg_mask(REG.DBG_CMD_1, 0xFFFF, 0x0);
		}

		if (priv->atv_gain_init_flag == false) {
			atv_get_origain();
			priv->atv_gain_init_flag = true;
		}

		/* init atv prescale setting */
		for (index_ga = ATV_PAL_GAIN_A2_FM_M; index_ga < ATV_GAIN_TYPE_NUM; index_ga++) {
			atv_set_prescale(ATV_PAL_GAIN_A2_FM_M + index_ga,
						*(&priv->atv_gain.pal_a2_fm_m + index_ga));
		}

		g_atv_record_nicam_gain = priv->atv_gain.pal_nicam;

		/* init atv palsum threshold setting */
		for (index_thr = A2_M_CARRIER1_ON_AMP; index_thr < PAL_THR_TYPE_END; index_thr++) {
			atv_set_threshold(A2_M_CARRIER1_ON_AMP + index_thr,
						*(&priv->atv_thr.a2_m_carrier1_on_amp + index_thr));

		}

		/* init atv btsc threshold setting */
		for (index_thr = BTSC_MONO_ON_NSR; index_thr < BTSC_THR_TYPE_END; index_thr++) {
			atv_set_threshold(A2_M_CARRIER1_ON_AMP + index_thr,
						*(&priv->atv_thr.a2_m_carrier1_on_amp + index_thr));

		}

		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x01, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x04, 0x04);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x08, 0x08);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x80, 0x80);

		/* ATV TriggerSifPLL init table */
		mtk_alsa_write_reg_mask_byte(REG.SE_INPUT_CFG, 0x04, 0x04);
		mtk_alsa_write_reg_mask_byte(REG.CLK_CFG6, 0x80, 0x80);
		mtk_alsa_write_reg_mask_byte(REG.CLK_CFG6, 0x60, 0x60);
		// SIF AGC control by DSP
		mtk_alsa_write_reg_mask_byte(REG.SIF_AGC_CFG6, 0x01, 0x01);
		mtk_alsa_write_reg_mask_byte(REG.ASIF_AMUX, 0xFF, 0xe0);
		 // Enable VIF
		mtk_alsa_write_reg_mask_byte(REG.ASIF_AMUX + 1, 0xFF, 0x3E);

		// SIF mode
		if (mtk_alsa_check_atv_input_type() == ATV_INPUT_SIF_EXTERNAL) {
			// 0x9503c2[10] = 0
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x04, 0x00);
			// 0x112cc8[12] = 0
			mtk_alsa_write_reg_mask_byte(REG.ASIF_AMUX + 1, 0x10, 0x00);
		}

		/* ATV play cmd */
		mtk_alsa_write_reg_mask_byte(REG.ATV_PALY_CMD, 0x01, 0x01);
		break;
	case ADC_DEV:
		mtk_adc0_clk_set_enable();
		if (priv->irq0 < 0) {
			ret = -EINVAL;
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sfail to get irq0\n", CAPTURE_ID_STRING[id]);
			break;
		} else if (priv->irq0_counter == 0) {
			// MB112E00
			ret = devm_request_irq(priv->dev, priv->irq0, irq0_handler,
			IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
			if (ret < 0)
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to request irq0\n",
					CAPTURE_ID_STRING[id]);
			tasklet_enable(&priv->irq0_tasklet);
		}
		priv->irq0_counter += 1;
		break;
	case ADC1_DEV:
		mtk_adc1_clk_set_enable();
		if (priv->irq0 < 0) {
			ret = -EINVAL;
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sfail to get irq0\n", CAPTURE_ID_STRING[id]);
			break;
		} else if (priv->irq0_counter == 0) {
			// MB112E00
			ret = devm_request_irq(priv->dev, priv->irq0, irq0_handler,
			IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
			if (ret < 0)
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to request irq0\n",
					CAPTURE_ID_STRING[id]);
			tasklet_enable(&priv->irq0_tasklet);
		}
		priv->irq0_counter += 1;
		break;
	case I2S_RX2_DEV:
		mtk_i2s_rx2_clk_set_enable();
		break;
	case LOOPBACK_DEV:
		if (priv->irq1 < 0)
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sfail to get irq1\n", CAPTURE_ID_STRING[id]);
		else {
			// MB112E10
			ret = devm_request_irq(priv->dev, priv->irq1, irq1_handler,
			IRQF_SHARED, "mediatek,mtk-capture-platform", priv);
			if (ret < 0)
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to request irq1\n",
					CAPTURE_ID_STRING[id]);
		}
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x00);
		// I2S out(BCK and WS) clock bypass to I2S in
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG + 1, 0x10, 0x10);
		break;
	case HFP_RX_DEV:
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x00);
		// I2S TX wck and ws bypass to I2S RX
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG+1, 0x10, 0x10);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]invalid pcm device %u open\n", id);
		ret = -ENODEV;
	}
	dev_runtime->clk_status = 1;
	mutex_unlock(&cap_device_mutex);

	return ret;
}

static int cap_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, CAP_NAME);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	int ret = 0;

	mutex_lock(&cap_device_mutex);
	switch (id) {
	case HDMI_RX1_DEV:
		mtk_hdmi_rx1_clk_set_disable();
		if (rx1_lifecycle_flag)
			devm_free_irq(priv->dev, priv->irq2, priv);
		else if (priv->irq0_counter > 1)
			priv->irq0_counter -= 1;
		else {
			devm_free_irq(priv->dev, priv->irq0, priv);
			tasklet_disable(&priv->irq0_tasklet);
			priv->irq0_counter = 0;
		}
		break;
	case HDMI_RX2_DEV:
		mtk_hdmi_rx2_clk_set_disable();
		if (rx2_lifecycle_flag)
			devm_free_irq(priv->dev, priv->irq3, priv);
		else if (priv->irq0_counter > 1)
			priv->irq0_counter -= 1;
		else {
			devm_free_irq(priv->dev, priv->irq0, priv);
			tasklet_disable(&priv->irq0_tasklet);
			priv->irq0_counter = 0;
		}
		break;
	case ATV_DEV:
		mtk_atv_clk_set_disable();
		if (priv->irq0_counter > 1)
			priv->irq0_counter -= 1;
		else {
			devm_free_irq(priv->dev, priv->irq0, priv);
			tasklet_disable(&priv->irq0_tasklet);
			priv->irq0_counter = 0;
		}
		break;
	case ADC_DEV:
		mtk_adc0_clk_set_disable();
		if (priv->irq0_counter > 1)
			priv->irq0_counter -= 1;
		else {
			devm_free_irq(priv->dev, priv->irq0, priv);
			tasklet_disable(&priv->irq0_tasklet);
			priv->irq0_counter = 0;
		}
		break;
	case ADC1_DEV:
		mtk_adc1_clk_set_disable();
		if (priv->irq0_counter > 1)
			priv->irq0_counter -= 1;
		else {
			devm_free_irq(priv->dev, priv->irq0, priv);
			tasklet_disable(&priv->irq0_tasklet);
			priv->irq0_counter = 0;
		}
		break;
	case I2S_RX2_DEV:
		mtk_i2s_rx2_clk_set_disable();
		break;
	case LOOPBACK_DEV:
		devm_free_irq(priv->dev, priv->irq1, priv);
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x10);
		// I2S out(BCK and WS) clock bypass to I2S in
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG + 1, 0x10, 0x00);
		break;
	case HFP_RX_DEV:
		// I2S in format, (0:standard, 1:Left-justified)
		mtk_alsa_write_reg_mask_byte(REG.I2S_INPUT_CFG, 0x10, 0x10);
		// I2S TX wck and ws bypass to I2S RX
		mtk_alsa_write_reg_mask_byte(REG.I2S_OUT2_CFG+1, 0x10, 0x00);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]invalid pcm device %u close\n", id);
		ret = -ENODEV;
		break;
	}
	dev_runtime->clk_status = 0;
	mutex_unlock(&cap_device_mutex);

	return ret;
}

static void capture_get_avail_bytes(struct mtk_dma_struct *capture)
{
	int avail_bytes = 0;

	avail_bytes = capture->r_buf.w_ptr - capture->r_buf.r_ptr;

	if (avail_bytes < 0)
		avail_bytes += capture->r_buf.size;

	capture->r_buf.avail_size = avail_bytes;
}

static snd_pcm_uframes_t cap_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, CAP_NAME);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	int offset_bytes = 0;
	snd_pcm_uframes_t offset_pcm_samples = 0;
	snd_pcm_uframes_t new_hw_ptr = 0;
	snd_pcm_uframes_t new_hw_ptr_pos = 0;
	unsigned int new_w_ptr_offset = 0;
	unsigned char *new_w_ptr = NULL;

	switch (id) {
	case HDMI_RX1_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.PAS_CTRL_73) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		if ((offset_bytes == 0) &&
			(capture->r_buf.avail_size > (capture->r_buf.size >> 1))) {
			// [12] ES Full Clr
			mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x10, 0x10);
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sErr! ES Full error occur!\n", CAPTURE_ID_STRING[id]);
			mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_48 + 1, 0x10, 0x00);
		}
		break;
	case HDMI_RX2_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.PAS_CTRL_74) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		if ((offset_bytes == 0) &&
			(capture->r_buf.avail_size > (capture->r_buf.size >> 1))) {
			// [12] ES2 Full Clr
			mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x10, 0x10);
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]%sErr! ES Full error occur!\n", CAPTURE_ID_STRING[id]);
			mtk_alsa_write_reg_mask_byte(REG.PAS_CTRL_64 + 1, 0x10, 0x00);
		}
		break;
	case ATV_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA2_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	case ADC_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA4_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	case ADC1_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA7_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	case I2S_RX2_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA8_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	case LOOPBACK_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA9_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	case HFP_RX_DEV:
		new_w_ptr_offset =
			mtk_alsa_read_reg(REG.CAP_DMA10_WPTR) * BYTES_IN_MIU_LINE;
		if (!frame_aligned(runtime, new_w_ptr_offset))
			AUDIO_DO_ALIGNMENT(new_w_ptr_offset, runtime->byte_align);
		new_w_ptr = capture->r_buf.addr + new_w_ptr_offset;
		offset_bytes = new_w_ptr - capture->r_buf.w_ptr;
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]Err! un-supported cap device %u!!!\n", id);
		break;
	}

	if (offset_bytes < 0)
		offset_bytes += capture->r_buf.size;
	capture->r_buf.w_ptr = new_w_ptr;
	capture->r_buf.total_write_size += offset_bytes;
	capture_get_avail_bytes(capture);

	offset_pcm_samples = bytes_to_frames(runtime, offset_bytes);
	new_hw_ptr = runtime->status->hw_ptr + offset_pcm_samples;
	new_hw_ptr_pos = new_hw_ptr % runtime->buffer_size;
	return new_hw_ptr_pos;
}

static int cap_pcm_copy(struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(rtd, CAP_NAME);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = rtd->cpu_dai->id;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_spinlock_struct *spinlock = &dev_runtime->spinlock;
	struct mtk_dma_struct *capture = &dev_runtime->capture;
	unsigned char *buffer_tmp = NULL;
	unsigned int size_to_copy = 0;
	unsigned int request_size = 0;
	unsigned int copied_size = 0;
	unsigned int size = 0;
	int ret = 0;
	int retry_counter = 0;
	unsigned long flags;

	if (buf == NULL) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]%sErr! invalid memory address!\n",
			CAPTURE_ID_STRING[id]);
		return -EINVAL;
	}

	if (bytes == 0) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]%sErr! request bytes is zero!\n",
			CAPTURE_ID_STRING[id]);
		return -EINVAL;
	}

	if (mtk_alsa_get_hw_reboot_flag(dev_runtime->device_cycle_count)) {
		snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA CAP]%sErr! after reboot need re-open device!\n",
			CAPTURE_ID_STRING[id]);
		return -EINVAL;
	}

	if (bytes % BYTES_IN_MIU_LINE != 0) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]%sErr! request size is not multiple 16 bytes!\n",
			CAPTURE_ID_STRING[id]);
		return -EINVAL;
	}

	if ((capture->status == CMD_STOP) || (capture->status == CMD_SUSPEND)) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]%s Err! capture->status not ready!\n",
			CAPTURE_ID_STRING[id]);
		return -EAGAIN;
	}

	buffer_tmp = capture->c_buf.addr;
	if (buffer_tmp == NULL) {
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]%s Err! need to allocate copy buffer!\n",
			CAPTURE_ID_STRING[id]);
		return -ENXIO;
	}

	request_size = bytes;
	capture_get_avail_bytes(capture);

	if ((((substream->f_flags & O_NONBLOCK) > 0) &&
		(request_size > capture->r_buf.avail_size)) ||
		(request_size > capture->c_buf.size)) {
		adev_err(priv->dev, dev_runtime->log_level,
			"[ALSA CAP]%s copy fail\n"
			"request_size = %u, buf avail_size = %u, copy buffer size = %u\n",
			CAPTURE_ID_STRING[id], request_size,
			capture->r_buf.avail_size, capture->c_buf.size);
		return -EAGAIN;
	}

	/* Wake up Monitor if necessary */
	if (dev_runtime->monitor.monitor_status == CMD_STOP) {
		while (1) {
			if (timer_reset(&dev_runtime->timer) == 0) {
				spin_lock_irqsave(&spinlock->lock, flags);
				dev_runtime->monitor.monitor_status = CMD_START;
				spin_unlock_irqrestore(&spinlock->lock, flags);
				break;
			}

			if ((++retry_counter) > 50) {
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sfail to wakeup Monitor\n",
					CAPTURE_ID_STRING[id]);
				break;
			}
			mtk_alsa_delaytask(2);
		}
		retry_counter = 0;
	}

	do {
		size_to_copy = request_size - copied_size;

		switch (id) {
		case ATV_DEV:
		case ADC_DEV:
		case ADC1_DEV:
		case I2S_RX2_DEV:
		case LOOPBACK_DEV:
		case HFP_RX_DEV:
			size = dma_read(capture, (void *)buffer_tmp, size_to_copy, id);
			break;
		case HDMI_RX1_DEV:
		case HDMI_RX2_DEV:
			size = parser_read(capture, (void *)buffer_tmp, size_to_copy, id);
			break;
		default:
			adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
				"[ALSA CAP]Err! un-supported cap device %u!!!\n", id);
			break;
		}
		if (size > 0) {
			ret = copy_to_user((buf + copied_size), buffer_tmp, size);
			if (ret) {
				adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
					"[ALSA CAP]%sErr! %d byte not copied\n",
					CAPTURE_ID_STRING[id], ret);
				copied_size += (size - ret);
			} else {
				copied_size += size;
			}
		}
		capture->r_buf.total_read_size += copied_size;

		if (retry_counter > 500) {
			adev_err(priv->dev, dev_runtime->log_level,
				"[ALSA DUP]%scapture copy fail\n",
				CAPTURE_ID_STRING[id]);
			ret = EBUSY;
			break;
		}
		retry_counter += 1;
	} while (copied_size < request_size);

	return 0;
}

static int cap_pcm_gettimeinfo(struct snd_pcm_substream *substream,
		struct timespec *curr_tstamp, struct timespec *audio_tstamp,
		struct snd_pcm_audio_tstamp_config *audio_tstamp_config,
		struct snd_pcm_audio_tstamp_report *audio_tstamp_report)
{
	// get system ts and set to audio_tstamp to keep audio_tstamp change
	// Or curr_tstamp maynot updated in update_audio_tstamp() sometimes
	struct timespec systemts;

	ktime_get_ts(&systemts);

	audio_tstamp->tv_sec = systemts.tv_sec;
	audio_tstamp->tv_nsec = systemts.tv_nsec;

	// get timestamp that updated by DSP ISR and set to curr_tstamp
	curr_tstamp->tv_sec = _stTimeStamp.tv_sec;
	curr_tstamp->tv_nsec = _stTimeStamp.tv_nsec;

	return 0;
}

const struct snd_pcm_ops cap_pcm_ops = {
	.open = cap_pcm_open,
	.close = cap_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = cap_pcm_pointer,
	.copy_user = cap_pcm_copy,
	.get_time_info = cap_pcm_gettimeinfo,
};

static int cap_control_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 12;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 65535;

	return 0;
}

static int cap_control_get_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	struct mtk_runtime_struct *dev_runtime = NULL;
	struct mtk_dma_struct *capture = NULL;
	unsigned char reg_byte;

	switch (kcontrol->private_value) {
	case RX1_MUTE_EVENT:
		dev_runtime = priv->pcm_device[HDMI_RX1_DEV];
		capture = &dev_runtime->capture;

		reg_byte = mtk_alsa_read_reg_byte(REG.HDMI_EVENT_1);
		if (reg_byte) {
			rx1_mute_end_size = capture->r_buf.total_write_size + 960;
			apr_info(MTK_ALSA_LEVEL_INFO, "%s, mute event is %u\n",
				CAPTURE_ID_STRING[HDMI_RX1_DEV],
				mtk_alsa_read_reg(REG.HDMI_EVENT_1));
			reg_byte &= mtk_alsa_read_reg_byte(REG.HDMI_EVENT_1 + 1);
			if (reg_byte) {
				mtk_alsa_write_reg_mask_byte(REG.HDMI_EVENT_0, 0xFF, reg_byte);
				mtk_alsa_write_reg_mask_byte(REG.HDMI_EVENT_0 + 1, 0xFF, reg_byte);
				mtk_alsa_delaytask_us(40);
				mtk_alsa_write_reg(REG.HDMI_EVENT_0, 0x0000);
			}
		} else if (mtk_alsa_read_reg_byte(REG.HDMI_EVENT_1 + 1)) {
			apr_info(MTK_ALSA_LEVEL_INFO, "%s, un-mute event is %04X\n",
				CAPTURE_ID_STRING[HDMI_RX1_DEV],
				mtk_alsa_read_reg(REG.HDMI_EVENT_1));
			mtk_alsa_write_reg_mask_byte(REG.HDMI_EVENT_0 + 1, 0xFF, 0xFF);
			mtk_alsa_delaytask_us(40);
			mtk_alsa_write_reg_mask_byte(REG.HDMI_EVENT_0 + 1, 0xFF, 0x00);
		}

		if (mtk_alsa_read_reg_byte(REG.STATUS_HDMI_CS) & 0x80)
			rx1_dsd_end_size = capture->r_buf.total_write_size + 960;

		reg_byte = mtk_alsa_read_reg_byte(REG.CAP_HDMI_INFO);
		if (reg_byte != rx1_fs_previous_count) {
			rx1_fs_end_size = capture->r_buf.total_write_size + 960;
			rx1_fs_previous_count = reg_byte;
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL, 0x7F,
							rx1_fs_previous_count);
		}

		if (capture->r_buf.total_read_size > rx1_mute_end_size)
			ucontrol->value.integer.value[0] = 0;
		else
			ucontrol->value.integer.value[0] = 1;

		if (capture->r_buf.total_read_size > rx1_dsd_end_size)
			ucontrol->value.integer.value[1] = 0;
		else
			ucontrol->value.integer.value[1] = 1;

		if (capture->r_buf.total_read_size > rx1_fs_end_size)
			ucontrol->value.integer.value[2] = 0;
		else
			ucontrol->value.integer.value[2] = 1;
		break;
	case RX1_NONPCM_EVENT:
		if (mtk_alsa_read_reg_byte(REG.PAS_CTRL_9 + 1) & 0x20) {
			ucontrol->value.integer.value[0] = 1;
			ucontrol->value.integer.value[1] =
				mtk_alsa_read_reg_byte(REG.STATUS_HDMI_PC);
			if (mtk_alsa_read_reg_byte(REG.HDMI_TBUS_3) & 0x80)
				ucontrol->value.integer.value[3] = 1;
			else
				ucontrol->value.integer.value[3] = 0;
		} else {
			ucontrol->value.integer.value[0] = 0;
			ucontrol->value.integer.value[1] = 0;
			ucontrol->value.integer.value[3] = 0;
		}
		if (mtk_alsa_read_reg_byte(REG.STATUS_HDMI_CS) & 0x40)
			ucontrol->value.integer.value[2] = 1;
		else
			ucontrol->value.integer.value[2] = 0;
		break;
	case RX1_CS:
		ucontrol->value.integer.value[0] = mtk_alsa_read_reg(REG.HDMI_CS0);
		ucontrol->value.integer.value[1] = mtk_alsa_read_reg(REG.HDMI_CS1);
		ucontrol->value.integer.value[2] = mtk_alsa_read_reg(REG.HDMI_CS2);
		ucontrol->value.integer.value[3] = mtk_alsa_read_reg(REG.HDMI_CS3);
		ucontrol->value.integer.value[4] = mtk_alsa_read_reg(REG.HDMI_CS4);
		ucontrol->value.integer.value[5] = mtk_alsa_read_reg(REG.HDMI_CS5);
		ucontrol->value.integer.value[6] = mtk_alsa_read_reg(REG.HDMI_CS6);
		ucontrol->value.integer.value[7] = mtk_alsa_read_reg(REG.HDMI_CS7);
		ucontrol->value.integer.value[8] = mtk_alsa_read_reg(REG.HDMI_CS8);
		ucontrol->value.integer.value[9] = mtk_alsa_read_reg(REG.HDMI_CS9);
		ucontrol->value.integer.value[10] = mtk_alsa_read_reg(REG.HDMI_CS10);
		ucontrol->value.integer.value[11] = mtk_alsa_read_reg(REG.HDMI_CS11);
		break;
	case RX2_MUTE_EVENT:
		dev_runtime = priv->pcm_device[HDMI_RX2_DEV];
		capture = &dev_runtime->capture;

		reg_byte = mtk_alsa_read_reg_byte(REG.HDMI2_EVENT_3);
		if (reg_byte) {
			rx2_mute_end_size = capture->r_buf.total_write_size + 960;
			apr_info(MTK_ALSA_LEVEL_INFO, "%s, mute event is %u, %u\n",
				CAPTURE_ID_STRING[HDMI_RX2_DEV],
				mtk_alsa_read_reg_byte(REG.HDMI2_EVENT_3),
				mtk_alsa_read_reg_byte(REG.HDMI2_EVENT_4));
			reg_byte &= mtk_alsa_read_reg_byte(REG.HDMI2_EVENT_4);
			if (reg_byte) {
				mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_0, 0xFF, reg_byte);
				mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_1, 0xFF, reg_byte);
				mtk_alsa_delaytask_us(40);
				mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_0, 0xFF, 0x00);
				mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_1, 0xFF, 0x00);
			}
		} else if (mtk_alsa_read_reg_byte(REG.HDMI2_EVENT_4)) {
			apr_info(MTK_ALSA_LEVEL_INFO, "%s, un-mute event is %02X\n",
				CAPTURE_ID_STRING[HDMI_RX2_DEV],
				mtk_alsa_read_reg(REG.HDMI2_EVENT_4));
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_1, 0xFF, 0xFF);
			mtk_alsa_delaytask_us(40);
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_EVENT_1, 0xFF, 0x00);
		}

		if (mtk_alsa_read_reg_byte(REG.HDMI2_CFG_29 + 1) & 0x40)
			rx2_dsd_end_size = capture->r_buf.total_write_size + 960;

		reg_byte = mtk_alsa_read_reg_byte(REG.CAP_HDMI_INFO+1);
		if (reg_byte != rx2_fs_previous_count) {
			rx2_fs_end_size = capture->r_buf.total_write_size + 960;
			rx2_fs_previous_count = reg_byte;
			mtk_alsa_write_reg_mask_byte(REG.CAP_HDMI_CTRL+1, 0x7F,
							rx2_fs_previous_count);
		}

		if (capture->r_buf.total_read_size > rx2_mute_end_size)
			ucontrol->value.integer.value[0] = 0;
		else
			ucontrol->value.integer.value[0] = 1;

		if (capture->r_buf.total_read_size > rx2_dsd_end_size)
			ucontrol->value.integer.value[1] = 0;
		else
			ucontrol->value.integer.value[1] = 1;

		if (capture->r_buf.total_read_size > rx2_fs_end_size)
			ucontrol->value.integer.value[2] = 0;
		else
			ucontrol->value.integer.value[2] = 1;
		break;
	case RX2_NONPCM_EVENT:
		if (mtk_alsa_read_reg_byte(REG.HDMI2_CFG_29 + 1) & 0x2) {
			ucontrol->value.integer.value[0] = 1;
			ucontrol->value.integer.value[1] =
				mtk_alsa_read_reg_byte(REG.STATUS_HDMI2_PC);
			if (mtk_alsa_read_reg_byte(REG.HDMI2_CFG_29 + 1) & 0x10)
				ucontrol->value.integer.value[3] = 1;
			else
				ucontrol->value.integer.value[3] = 0;
		} else {
			ucontrol->value.integer.value[0] = 0;
			ucontrol->value.integer.value[1] = 0;
			ucontrol->value.integer.value[3] = 0;
		}
		if (mtk_alsa_read_reg_byte(REG.HDMI2_CFG_29 + 1) & 0x20)
			ucontrol->value.integer.value[2] = 1;
		else
			ucontrol->value.integer.value[2] = 0;
		break;
	case RX2_CS:
		ucontrol->value.integer.value[0] = mtk_alsa_read_reg(REG.HDMI2_CS0);
		ucontrol->value.integer.value[1] = mtk_alsa_read_reg(REG.HDMI2_CS1);
		ucontrol->value.integer.value[2] = mtk_alsa_read_reg(REG.HDMI2_CS2);
		ucontrol->value.integer.value[3] = mtk_alsa_read_reg(REG.HDMI2_CS3);
		ucontrol->value.integer.value[4] = mtk_alsa_read_reg(REG.HDMI2_CS4);
		ucontrol->value.integer.value[5] = mtk_alsa_read_reg(REG.HDMI2_CS5);
		ucontrol->value.integer.value[6] = mtk_alsa_read_reg(REG.HDMI2_CS6);
		ucontrol->value.integer.value[7] = mtk_alsa_read_reg(REG.HDMI2_CS7);
		ucontrol->value.integer.value[8] = mtk_alsa_read_reg(REG.HDMI2_CS8);
		ucontrol->value.integer.value[9] = mtk_alsa_read_reg(REG.HDMI2_CS9);
		ucontrol->value.integer.value[10] = mtk_alsa_read_reg(REG.HDMI2_CS10);
		ucontrol->value.integer.value[11] = mtk_alsa_read_reg(REG.HDMI2_CS11);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]Err! un-supported cap kcontrol !!!\n");
		break;
	}

	return 0;
}

static int cap_control_put_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);

	switch (kcontrol->private_value) {
	case RX1_NONPCM_FLAG:
		priv->hdmi_rx1_nonpcm_flag = ucontrol->value.integer.value[0];
		break;
	case RX1_FASTLOCK:
		if (ucontrol->value.integer.value[0])
			mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x04, 0x04);
		else
			mtk_alsa_write_reg_mask_byte(REG.HDMI_MODE_CFG, 0x04, 0x00);
		break;
	case RX2_NONPCM_FLAG:
		priv->hdmi_rx2_nonpcm_flag = ucontrol->value.integer.value[0];
		break;
	case RX2_FASTLOCK:
		if (ucontrol->value.integer.value[0])
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG2, 0x10, 0x10);
		else
			mtk_alsa_write_reg_mask_byte(REG.HDMI2_MODE_CFG2, 0x10, 0x00);
		break;
	default:
		adev_err(priv->dev, MTK_ALSA_LEVEL_ERROR,
			"[ALSA CAP]Err! un-supported cap kcontrol !!!\n");
		break;
	}
	return 0;
}

static unsigned int mtk_alsa_read_dsp_sram(unsigned int dsp_addr,
				enum dsp_memory_type dsp_mem_type)
{
	unsigned char dat[3];
	unsigned int value = 0;
	bool ret;

	mutex_lock(&audio_idma_mutex);

	mtk_alsa_write_reg_mask(REG.SE_BDMA_CFG, 0x8080, 0x0000);
	mtk_alsa_write_reg_mask_byte(REG.FD230_SEL, 0x01, 0x01);

	ret = mtk_alsa_read_reg_byte_polling(REG.SE_IDMA_CTRL_0, 0x18, 0x00, 200);
	if (ret) {
		mutex_unlock(&audio_idma_mutex);
		return false;
	}

	if ((dsp_mem_type == DSP_MEM_TYPE_DM) && (dsp_addr >= 0x8000))
		mtk_alsa_write_reg_mask_byte(REG.SE_IDMA_CTRL_0, 0x40, 0x40);

	if (dsp_mem_type == DSP_MEM_TYPE_DM)
		dsp_addr |= 0x8000;

	mtk_alsa_write_reg(REG.SE_IDMA_RDBASE_ADDR_L, dsp_addr);
	mtk_alsa_write_reg_mask_byte(REG.SE_IDMA_CTRL_0, 0x08, 0x08);
	mtk_alsa_delaytask_us(40);

	if ((dsp_mem_type == DSP_MEM_TYPE_DM) && (dsp_addr >= 0x8000))
		mtk_alsa_write_reg_mask_byte(REG.SE_IDMA_CTRL_0, 0x40, 0x00);

	ret = mtk_alsa_read_reg_byte_polling(REG.SE_IDMA_CTRL_0, 0x18, 0x00, 200);
	if (ret) {
		mutex_unlock(&audio_idma_mutex);
		return false;
	}

	dat[2] = mtk_alsa_read_reg_byte(REG.SE_IDMA_RDDATA_H_1);
	dat[1] = mtk_alsa_read_reg_byte(REG.SE_IDMA_RDDATA_H_0);
	dat[0] = mtk_alsa_read_reg_byte(REG.SE_IDMA_RDDATA_L);

	value = (((unsigned char)dat[2] << 16)
		| ((unsigned char)dat[1] << 8)
		| ((unsigned char)dat[0]));

	mutex_unlock(&audio_idma_mutex);

	return value;
}

static bool mtk_alsa_write_dsp_sram(unsigned int dsp_addr,
				unsigned int value,
				enum dsp_memory_type dsp_mem_type)
{
	unsigned char dat[3];
	bool ret;

	mutex_lock(&audio_idma_mutex);

	mtk_alsa_write_reg_mask(REG.SE_BDMA_CFG, 0x8080, 0x0000);
	mtk_alsa_write_reg_mask_byte(REG.FD230_SEL, 0x01, 0x01);

	ret = mtk_alsa_read_reg_byte_polling(REG.SE_IDMA_CTRL_0, 0x18, 0x00, 200);
	if (ret) {
		mutex_unlock(&audio_idma_mutex);
		return false;
	}

	dat[2] = (unsigned char)(value / 0x10000);
	dat[1] = (unsigned char)((value >> 8) & 0x0000FF);
	dat[0] = (unsigned char)(value & 0x0000FF);

	if (dsp_mem_type == DSP_MEM_TYPE_DM)
		dsp_addr |= 0x8000;

	mtk_alsa_write_reg(REG.SE_IDMA_WRBASE_ADDR_L, dsp_addr);

	mtk_alsa_write_reg_byte(REG.SE_DSP_BRG_DATA_L, dat[1]);
	mtk_alsa_write_reg_byte(REG.SE_DSP_BRG_DATA_H, dat[2]);

	mtk_alsa_delaytask_us(40);

	mtk_alsa_write_reg_byte(REG.SE_DSP_BRG_DATA_L, dat[0]);
	mtk_alsa_write_reg_byte(REG.SE_DSP_BRG_DATA_H, 0x00);

	ret = mtk_alsa_read_reg_byte_polling(REG.SE_IDMA_CTRL_0, 0x18, 0x00, 200);
	if (ret) {
		mutex_unlock(&audio_idma_mutex);
		return false;
	}

	mutex_unlock(&audio_idma_mutex);

	return true;
}

static int atv_get_atv_system(void)
{
	if ((mtk_alsa_read_reg_byte(REG.ATV_STD_CMD) & 0xF) == ATV_STD_M_BTSC)
		return ATV_SYS_BTSC;
	else
		return ATV_SYS_PALSUM;
}

static u32 atv_set_cmd(enum atv_comm_cmd cmd, long param1, long param2)
{
	u32 u32_ret = 0;

	switch (cmd) {
	case ATV_SET_START:
		mtk_alsa_write_reg_mask_byte(REG.ATV_PALY_CMD, 0x01, 0x01);
		break;
	case ATV_SET_STOP:
		mtk_alsa_write_reg_mask_byte(REG.ATV_PALY_CMD, 0x01, 0x00);
		break;
	case ATV_ENABLE_HIDEV:
		if (param1 == true) {
			mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_HIDEV_MODE);
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x01, 0x01);
			mtk_alsa_delaytask(5);
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x01, 0x00);
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x02, 0x02);
			mtk_alsa_delaytask(1);
		} else {
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x02, 0x00);
			mtk_alsa_delaytask(1);
			mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_MONO_MODE);
		}
		break;
	case ATV_SET_HIDEV_BW:
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, param1);
		break;
	case ATV_ENABLE_AGC:
		if (param1 == true) {
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x01, 0x01);
			mtk_alsa_delaytask(3);
		} else {
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x01, 0x00);
			mtk_alsa_delaytask(3);
		}
		break;
	case ATV_RESET_AGC:
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x02, 0x02);
		mtk_alsa_delaytask(3);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG + 1, 0x02, 0x00);
		mtk_alsa_delaytask(3);
		break;
	case ATV_ENABLE_FC_TRACKING:
		if (param1 == true) {
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x02, 0x02);
			mtk_alsa_delaytask(1);
		} else {
			mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x02, 0x00);
			mtk_alsa_delaytask(1);
		}
		break;
	case ATV_RESET_FC_TRACKING:
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x01, 0x01);
		mtk_alsa_delaytask(5);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x01, 0x00);
		break;
	case ATV_PAL_GET_SC1_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_SC1_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_SC1_NSR:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_SC1_NSR_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_SC2_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_SC2_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_SC2_NSR:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_SC2_NSR_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_PILOT_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_PILOT_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_PILOT_PHASE:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_PILOT_PHASE_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_STEREO_DUAL_ID:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_STEREO_DUAL_ID_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_NICAM_PHASE_ERR:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_NICAM_PHASE_ERR_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_PAL_GET_BAND_STRENGTH:
		mtk_alsa_write_reg(REG.DBG_CMD_1, PAL_BAND_STRENGTH_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_BTSC_GET_SC1_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, BTSC_SC1_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_BTSC_GET_SC1_NSR:
		mtk_alsa_write_reg(REG.DBG_CMD_1, BTSC_SC1_NSR_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_BTSC_GET_STEREO_PILOT_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, BTSC_STEREO_PILOT_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_BTSC_GET_SAP_AMP:
		mtk_alsa_write_reg(REG.DBG_CMD_1, BTSC_SAP_AMP_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	case ATV_BTSC_GET_SAP_NSR:
		mtk_alsa_write_reg(REG.DBG_CMD_1, BTSC_SAP_NSR_DBG_CMD);
		mtk_alsa_write_reg(REG.DBG_CMD_2, 0x0000);
		mtk_alsa_delaytask(SIF_DSP_DET_TIME);
		u32_ret = (mtk_alsa_read_reg(REG.DBG_RESULT_2)
				+ (mtk_alsa_read_reg(REG.DBG_RESULT_1) << SHIFT_16));
		break;
	default:
		break;
	}

	return u32_ret;
}

static void atv_set_hidev(enum atv_hidev_mode hidev_mode)
{
	if (atv_get_atv_system() == ATV_SYS_BTSC)
		return;

	switch (hidev_mode) {
	case ATV_HIDEV_OFF:
		atv_set_cmd(ATV_ENABLE_HIDEV, false, (long) NULL);
		break;
	case ATV_HIDEV_L1:
		atv_set_cmd(ATV_ENABLE_HIDEV, true, (long) NULL);
		atv_set_cmd(ATV_SET_HIDEV_BW, ATV_HIDEV_BW_L1, (long) NULL);
		break;
	case ATV_HIDEV_L2:
		atv_set_cmd(ATV_ENABLE_HIDEV, true, (long) NULL);
		atv_set_cmd(ATV_SET_HIDEV_BW, ATV_HIDEV_BW_L2, (long) NULL);
		break;
	case ATV_HIDEV_L3:
		atv_set_cmd(ATV_ENABLE_HIDEV, true, (long) NULL);
		atv_set_cmd(ATV_SET_HIDEV_BW, ATV_HIDEV_BW_L3, (long) NULL);
		break;
	default:
		break;
	}
}

static enum atv_standard_type atv_get_main_standard(void)
{
	return (mtk_alsa_read_reg_byte(REG.ATV_STD_CMD) & 0xF);
}

static int atv_get_sub_standard(void)
{
	int ret;
	int sub_std_state;

	sub_std_state = (mtk_alsa_read_reg_byte(REG.ATV_STD_CMD) & 0xF0);

	if (sub_std_state == ATV_MONO_MODE) {
		ret = ATV_SUB_STD_MONO;
	} else if (sub_std_state == ATV_HIDEV_MODE) {
		ret = ATV_SUB_STD_HIDEV;
	} else if (sub_std_state == ATV_A2_MODE) {
		ret = ATV_SUB_STD_A2;
	} else if (sub_std_state == ATV_NICAM_MODE) {
		ret = ATV_SUB_STD_NICAM;
	} else {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s fail !\n", __func__);
		ret = ATV_SUB_STD_INVALID;
	}
	return ret;
}

static enum atv_carrier_status atv_get_audio_status(void)
{
	unsigned char ucValue = 0;
	unsigned char nicam_snd_info = _FM_MONO_NOT_EXIST;
	unsigned char nicam_state = _NICAM_UNLOCK_STATE;
	enum atv_carrier_status current_status = ATV_NO_CARRIER;

	ucValue = mtk_alsa_read_reg_byte(REG.ATV_SND_CARRIER_STATUS_1);

	if (atv_get_atv_system() == ATV_SYS_BTSC) {
		if (ucValue & _STATUS_MONO_EXIST)
			current_status |= ATV_PRIMARY_CARRIER;
		if (ucValue & _STATUS_STEREO_EXIST)
			current_status |= ATV_STEREO;
		if (ucValue & _STATUS_DUAL_EXIST)
			current_status |= ATV_BILINGUAL;

		return current_status;
	}

	if (atv_get_sub_standard() ==  ATV_SUB_STD_A2) {
		if ((ucValue & _STATUS_A2_PRIMARY_EXIST) != _STATUS_A2_PRIMARY_EXIST)
			return ATV_NO_CARRIER;

		current_status |= ATV_PRIMARY_CARRIER;

		if (ucValue & _STATUS_A2_SECONDARY_EXIST)
			current_status |= ATV_SECONDARY_CARRIER;
		if (ucValue & _STATUS_A2_PILOT_EXIST)
			current_status |= ATV_PILOT;
		if (ucValue & _STATUS_A2_DK2_EXIST)
			current_status |= ATV_DK2;
		if (ucValue & _STATUS_A2_DK3_EXIST)
			current_status |= ATV_DK3;
		if (ucValue & _STATUS_STEREO_EXIST)
			current_status |= ATV_STEREO;
		if (ucValue & _STATUS_DUAL_EXIST)
			current_status |= ATV_BILINGUAL;
	} else if (atv_get_sub_standard() == ATV_SUB_STD_NICAM) {
		ucValue = mtk_alsa_read_reg_byte(REG.ATV_SND_CARRIER_STATUS_2);
		nicam_snd_info = (ucValue & _MASK_NICAM_SOUND_MODE_INFO);
		nicam_state = (ucValue & _MASK_NICAM_STATUS_INFO);

		if (nicam_snd_info == _FM_MONO_NOT_EXIST)
			return ATV_NO_CARRIER;

		current_status |= ATV_PRIMARY_CARRIER;

		if (nicam_state != _NICAM_LOCK_STATE)
			return current_status;

		current_status |= ATV_NICAM;

		if (nicam_snd_info == _NICAM_STEREO)
			current_status |= ATV_STEREO;
		if (nicam_snd_info == _FM_MONO_n_NICAM_STEREO)
			current_status |= ATV_STEREO;
		if (nicam_snd_info == _NICAM_DUAL)
			current_status |= ATV_BILINGUAL;
		if (nicam_snd_info == _FM_MONO_n_NICAM_DUAL)
			current_status |= ATV_BILINGUAL;
	} else {
		if ((ucValue & _STATUS_A2_PRIMARY_EXIST) != _STATUS_A2_PRIMARY_EXIST)
			return ATV_NO_CARRIER;

		current_status |= ATV_PRIMARY_CARRIER;

		// try A2
		if (ucValue & _STATUS_A2_SECONDARY_EXIST) {
			current_status |= ATV_SECONDARY_CARRIER;

			if (ucValue & _STATUS_A2_PILOT_EXIST)
				current_status |= ATV_PILOT;
			if (ucValue & _STATUS_A2_DK2_EXIST)
				current_status |= ATV_DK2;
			if (ucValue & _STATUS_A2_DK3_EXIST)
				current_status |= ATV_DK3;
			if (ucValue & _STATUS_STEREO_EXIST)
				current_status |= ATV_STEREO;
			if (ucValue & _STATUS_DUAL_EXIST)
				current_status |= ATV_BILINGUAL;
		} else {
			// try Nicam
			ucValue = mtk_alsa_read_reg_byte(REG.ATV_SND_CARRIER_STATUS_2);
			nicam_snd_info = (ucValue & _MASK_NICAM_SOUND_MODE_INFO);
			nicam_state = (ucValue & _MASK_NICAM_STATUS_INFO);

			if (nicam_state != _NICAM_LOCK_STATE)
				return current_status;

			current_status |= ATV_NICAM;

			if (nicam_snd_info == _NICAM_STEREO)
				current_status |= ATV_STEREO;
			if (nicam_snd_info == _FM_MONO_n_NICAM_STEREO)
				current_status |= ATV_STEREO;
			if (nicam_snd_info == _NICAM_DUAL)
				current_status |= ATV_BILINGUAL;
			if (nicam_snd_info == _FM_MONO_n_NICAM_DUAL)
				current_status |= ATV_BILINGUAL;
		}
	}

	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s[%d] current_status = %d\n"
							, __func__, __LINE__, current_status);


	return current_status;
}

static int atv_get_cur_snd_mode(void)
{
	if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
		return mtk_alsa_read_reg_byte(REG.ATV_SND_MODE_2);
	else
		return mtk_alsa_read_reg_byte(REG.ATV_SND_MODE_1);
}

static enum atv_sound_mode_detect atv_get_support_snd_mode(void)
{
	enum atv_standard_type current_std_type;
	enum atv_carrier_status current_status;
	enum atv_sound_mode_detect ret_snd;

	current_status = atv_get_audio_status();
	current_std_type = atv_get_main_standard();
	ret_snd = ATV_SOUND_MODE_DET_UNKNOWN;

	switch (current_std_type) {
	case ATV_STD_M_A2:
	case ATV_STD_M_EIAJ:
		if (current_status == ATV_PRIMARY_CARRIER)
			return ATV_SOUND_MODE_DET_MONO;
		if ((current_status & ATV_SECONDARY_CARRIER) != ATV_SECONDARY_CARRIER)
			return ATV_SOUND_MODE_DET_MONO;
		if ((current_status & ATV_STEREO) == ATV_STEREO)
			return ATV_SOUND_MODE_DET_STEREO;
		if ((current_status & ATV_BILINGUAL) == ATV_BILINGUAL)
			return ATV_SOUND_MODE_DET_DUAL;
		break;
	case ATV_STD_M_BTSC:
		if (current_status == ATV_PRIMARY_CARRIER)
			return ATV_SOUND_MODE_DET_MONO;
		if ((current_status & ATV_STEREO) == ATV_STEREO) {
			if ((current_status & ATV_BILINGUAL) == ATV_BILINGUAL)
				return ATV_SOUND_MODE_DET_STEREO_SAP;
			else
				return ATV_SOUND_MODE_DET_STEREO;
		}
		if ((current_status & ATV_BILINGUAL) == ATV_BILINGUAL)
			return ATV_SOUND_MODE_DET_MONO_SAP;
		break;
	default:
		if (current_status == ATV_PRIMARY_CARRIER)
			return ATV_SOUND_MODE_DET_MONO;
		if ((current_status & ATV_NICAM) == ATV_NICAM) {
			if ((current_status & ATV_STEREO) == ATV_STEREO)
				return ATV_SOUND_MODE_DET_NICAM_STEREO;
			else if ((current_status & ATV_BILINGUAL) == ATV_BILINGUAL)
				return ATV_SOUND_MODE_DET_NICAM_DUAL;
			else
				return ATV_SOUND_MODE_DET_NICAM_MONO;
		}
		if ((current_status & ATV_SECONDARY_CARRIER) != ATV_SECONDARY_CARRIER)
			return ATV_SOUND_MODE_DET_MONO;
		if ((current_status & ATV_STEREO) == ATV_STEREO)
			return ATV_SOUND_MODE_DET_STEREO;
		if ((current_status & ATV_BILINGUAL) == ATV_BILINGUAL)
			return ATV_SOUND_MODE_DET_DUAL;
		break;
	}

	return ret_snd;
}

static int atv_get_auto_detection_result(void)
{
	unsigned short i = 0;
	unsigned char dsp_result = 0;

	mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xFF, 0x00);
	mtk_alsa_delaytask(1);
	mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, 0x00);

	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s, start to detect !\n", __func__);

	mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xFF, ATV_STD_SEL_AUTO);

	dsp_result = mtk_alsa_read_reg_byte(REG.ATV_DETECTION_RESULT);

	while ((dsp_result == ATV_STD_BUSY) || (dsp_result == ATV_STD_NOT_READY)) {
		if (i++ > 1000) {
			apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s timeout !\n", __func__);
			break;
		}
		mtk_alsa_delaytask(1);
		dsp_result = mtk_alsa_read_reg_byte(REG.ATV_DETECTION_RESULT);
	}

	mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xFF, 0x00);

	g_atv_record_std = ATV_STD_NOT_READY;

	apr_info(MTK_ALSA_LEVEL_INFO,
		"[ALSA ATV] %s, detection time = %d(ms), result = %x\n",
		__func__, i, dsp_result);

	return dsp_result;
}

static int atv_get_sys_capability(void)
{
	//TODO: wait for TIS layer ready
	return true;
}

static void atv_set_standard(enum atv_standard_type std_type)
{
	if (std_type > ATV_STD_MAX)
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s fail !\n", __func__);

	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s[%d] std_type = %d\n"
							, __func__, __LINE__, std_type);

	if (g_atv_record_std == std_type)
		return;

	/* re standard */
	mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0x00, 0x00);
	mtk_alsa_delaytask(10);

	if (std_type == ATV_STD_L_NICAM) {
		atv_set_cmd(ATV_ENABLE_AGC, false, (long) NULL);
		atv_set_cmd(ATV_RESET_AGC, (long) NULL, (long) NULL);
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0x0F, std_type);
	} else {
		atv_set_cmd(ATV_ENABLE_AGC, true, (long) NULL);
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0x0F, std_type);
	}

	if (std_type == ATV_STD_I_NICAM)
		atv_set_prescale(ATV_PAL_GAIN_NICAM, g_atv_record_nicam_gain + PRESCALE_NICAM_I);
	else
		atv_set_prescale(ATV_PAL_GAIN_NICAM, g_atv_record_nicam_gain);

	g_atv_record_std = std_type;
}

static void atv_set_sub_standard(enum atv_sub_std sub_std_type)
{
	if (sub_std_type > ATV_SUB_STD_NICAM)
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s fail 1!\n", __func__);

	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s[%d] sub_std_type = %d\n"
							, __func__, __LINE__, sub_std_type);

	if (atv_get_atv_system() != ATV_SYS_PALSUM)
		sub_std_type = ATV_SUB_STD_MONO;

	switch (sub_std_type) {
	case ATV_SUB_STD_MONO:
		atv_set_cmd(ATV_ENABLE_FC_TRACKING, false, (long) NULL);
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_MONO_MODE);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, 0x00);
		break;
	case ATV_SUB_STD_HIDEV:
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_HIDEV_MODE);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, 0x10);
		atv_set_cmd(ATV_RESET_FC_TRACKING, (long) NULL, (long) NULL);
		atv_set_cmd(ATV_ENABLE_FC_TRACKING, true, (long) NULL);
		break;
	case ATV_SUB_STD_A2:
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_A2_MODE);
		atv_set_cmd(ATV_ENABLE_FC_TRACKING, false, (long) NULL);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, 0x00);
		break;
	case ATV_SUB_STD_NICAM:
		mtk_alsa_write_reg_mask_byte(REG.ATV_STD_CMD, 0xF0, ATV_NICAM_MODE);
		atv_set_cmd(ATV_ENABLE_FC_TRACKING, false, (long) NULL);
		mtk_alsa_write_reg_mask_byte(REG.ATV_COMM_CFG, 0x30, 0x00);
		mtk_alsa_delaytask(50);
		break;
	default:
		break;
	}
}

static void atv_set_soundmode(enum atv_sound_mode snd_mode)
{
	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s[%d] snd_mode = %d\n"
							, __func__, __LINE__, snd_mode);

	switch (snd_mode) {
	case ATV_SOUND_MODE_MONO:
		if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
			mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x01);
		else
			mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x00);
		break;
	case ATV_SOUND_MODE_STEREO:
	case ATV_SOUND_MODE_NICAM_STEREO:
		if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
			mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x02);
		else
			mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x01);
		break;
	case ATV_SOUND_MODE_SAP:
		mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x02);
		break;
	case ATV_SOUND_MODE_A2_LANG_A:
	case ATV_SOUND_MODE_NICAM_DUAL_A:
		if (atv_get_atv_system() == ATV_SYS_PALSUM) {
			if (atv_get_sub_standard() == ATV_SUB_STD_A2)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x00);
			else if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x06);
		} else {
			apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV]set snd mode fail 1!\n");
		}
		break;
	case ATV_SOUND_MODE_A2_LANG_B:
	case ATV_SOUND_MODE_NICAM_DUAL_B:
		if (atv_get_atv_system() == ATV_SYS_PALSUM) {
			if (atv_get_sub_standard() == ATV_SUB_STD_A2)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x02);
			else if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x07);
		} else {
			apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV]set snd mode fail 2!\n");
		}
		break;
	case ATV_SOUND_MODE_A2_LANG_AB:
	case ATV_SOUND_MODE_NICAM_DUAL_AB:
		if (atv_get_atv_system() == ATV_SYS_PALSUM) {
			if (atv_get_sub_standard() == ATV_SUB_STD_A2)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0xFF, 0x03);
			else if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
				mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x05);
		} else {
			apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV]set snd mode fail 3!\n");
		}
		break;
	case ATV_SOUND_MODE_NICAM_MONO:
		if (atv_get_sub_standard() == ATV_SUB_STD_NICAM)
			mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_2, 0xFF, 0x08);
		else
			apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV]set snd mode fail 4!\n");
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV]set snd mode fail 5!\n");
		break;
	}
}

static void atv_set_auto_soundmode(unsigned char enable)
{
	apr_info(MTK_ALSA_LEVEL_INFO, "[ALSA ATV] %s[%d] enable = %d\n"
							, __func__, __LINE__, enable);

	if (enable == true)
		mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0x80, 0x80);
	else
		mtk_alsa_write_reg_mask_byte(REG.ATV_SND_MODE_1, 0x80, 0x00);
}

static void atv_set_threshold(enum atv_thr_type thr_type, unsigned int value)
{
	unsigned int dsp_sram_base_addr;
	unsigned short offset;

	if ((thr_type > BTSC_THR_TYPE_END) || (value > 0xFFFFFF)) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA ATV] %s param out of range fail!\n", __func__);
		return;
	}

	if (thr_type < PAL_THR_TYPE_END) {
		dsp_sram_base_addr = ATV_PAL_THR_BASE_ADDR;
		offset = (thr_type - A2_M_CARRIER1_ON_AMP);
	} else {
		dsp_sram_base_addr = ATV_BTSC_THR_BASE_ADDR;
		offset = (thr_type - BTSC_MONO_ON_NSR);
	}

	mtk_alsa_write_dsp_sram((dsp_sram_base_addr + offset), value, DSP_MEM_TYPE_PM);
}

static void atv_get_origain(void)
{
	unsigned int pal_gain_base;
	unsigned int btsc_total_gain_base;
	unsigned int btsc_mts_gain_base;
	unsigned int addr;
	int i;

	pal_gain_base = ATV_PAL_TYPE_GAIN_BASE_ADDR;  // len 12
	btsc_total_gain_base = ATV_BTSC_TOTAL_GAIN_BASE_ADDR;  // len 2
	btsc_mts_gain_base = ATV_BTSC_MTS_GAIN_BASE_ADDR;  // len 6

	for (i = 0; i < ATV_GAIN_TYPE_NUM; i++) {
		if (i <= ATV_PAL_GAIN_AM)
			addr = pal_gain_base + (2 * i);
		else if (i == ATV_BTSC_GAIN_TOTAL)
			addr = btsc_total_gain_base;
		else if (i == ATV_BTSC_GAIN_MONO)
			addr = btsc_mts_gain_base;
		else if (i == ATV_BTSC_GAIN_STEREO)
			addr = btsc_mts_gain_base + 2;
		// ATV_BTSC_GAIN_SAP
		else
			addr = btsc_mts_gain_base + 4;

		g_atv_gain_addr[i] = addr;
		g_atv_gain[i] = (mtk_alsa_read_dsp_sram(addr, DSP_MEM_TYPE_PM) >> 8);
		g_atv_shift[i] = mtk_alsa_read_dsp_sram(addr + 1, DSP_MEM_TYPE_PM);
	}
}

static void atv_set_prescale(enum atv_gain_type gain_type, signed long db_value)
{
	unsigned short atv_gain;
	unsigned short atv_shift;
	unsigned short shift_value;
	unsigned char i, i_min, i_max;
	signed int normalized_db;
	unsigned int gain;
	unsigned int gain_addr;

	if ((gain_type >= ATV_GAIN_TYPE_NUM) ||
		(db_value > PRESCALE_UPPER_BOUND) ||
		(db_value < PRESCALE_LOWER_BOUND)) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA ATV] %s param out of range fail!\n", __func__);
		return;
	}

	atv_gain = g_atv_gain[gain_type];
	atv_shift = g_atv_shift[gain_type];

	i_min = ((8/PRESCALE_STEP_ONE_DB) - 1);
	i_max = (sizeof(ATV_PRESCALE_STEP_TBL)/sizeof(unsigned short));

	if (db_value > (18 * PRESCALE_STEP_ONE_DB)) {
		shift_value = 5;
		db_value = 0;
	} else if (db_value > (12 * PRESCALE_STEP_ONE_DB)) {
		shift_value = 5;
		db_value = db_value - (18 * PRESCALE_STEP_ONE_DB);
	} else if (db_value > (6 * PRESCALE_STEP_ONE_DB)) {
		shift_value = 4;
		db_value = db_value - (12 * PRESCALE_STEP_ONE_DB);
	} else if (db_value > 0) {
		shift_value = 3;
		db_value = db_value - (6 * PRESCALE_STEP_ONE_DB);
	} else if (db_value > (-6 * PRESCALE_STEP_ONE_DB)) {
		shift_value = 2;
		//db_value = db_value + (0 * PRESCALE_STEP_ONE_DB);
	} else if (db_value > (-12 * PRESCALE_STEP_ONE_DB)) {
		shift_value = 1;
		db_value = db_value + (6 * PRESCALE_STEP_ONE_DB);
	} else {
		shift_value = 0;
		db_value = 0;
	}

	gain = 0x7FFF;
	normalized_db = -db_value;

	for (i = i_min; i < i_max; i++) {
		if (normalized_db & (1 << (i - i_min))) {
			gain = (gain * ((unsigned int)ATV_PRESCALE_STEP_TBL[i]));
			gain = (gain >> 15);
		}
	}

	while ((atv_shift + shift_value) < 2) {
		gain = (gain * 0x4000);
		gain = (gain >> 15);
		shift_value++;
	}

	gain = (gain * ((unsigned int)atv_gain)) >> 15;
	atv_gain = (unsigned short)gain;
	atv_shift = (atv_shift + shift_value - 2);

	gain_addr = g_atv_gain_addr[gain_type];

	mtk_alsa_write_dsp_sram(gain_addr,
				((unsigned int)atv_gain << 8),
				DSP_MEM_TYPE_PM);
	mtk_alsa_write_dsp_sram(gain_addr + 1,
				(unsigned int)atv_shift,
				DSP_MEM_TYPE_PM);
}

static int atv_control_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 4;
	uinfo->value.integer.min = -60;
	uinfo->value.integer.max = 0xFFFFFF;

	return 0;
}

static int atv_control_get_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	unsigned int u32_ret_0 = 0;
	unsigned int u32_ret_1 = 0;
	unsigned int u32_ret_2 = 0;
	unsigned int u32_ret_3 = 0;

	switch (kcontrol->private_value) {
	case ATV_STANDARD:
		u32_ret_0 = atv_get_main_standard();
		u32_ret_1 = atv_get_sub_standard();
		break;
	case ATV_SOUNDMODE:
		u32_ret_2 = atv_get_cur_snd_mode();
		u32_ret_3 = atv_get_support_snd_mode();
		break;
	case ATV_CARRIER_STATUS:
		u32_ret_0 = atv_get_audio_status();
		break;
	case ATV_AUTO_DET_STD_RESULT:
		u32_ret_0 = atv_get_auto_detection_result();
		break;
	case ATV_SYS_CAPABILITY:
		u32_ret_0 = atv_get_sys_capability();
		break;
	case ATV_PAL_SC1_AMP:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_SC1_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_SC1_NSR:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_SC1_NSR, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_SC2_AMP:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_SC2_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_SC2_NSR:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_SC2_NSR, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_PILOT_AMP:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_PILOT_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_PILOT_PHASE:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_PILOT_PHASE, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_STEREO_DUAL_ID:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_STEREO_DUAL_ID, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_NICAM_PHASE_ERR:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_NICAM_PHASE_ERR, (long) NULL, (long) NULL);
		break;
	case ATV_PAL_BAND_STRENGTH:
		u32_ret_0 = atv_set_cmd(ATV_PAL_GET_BAND_STRENGTH, (long) NULL, (long) NULL);
		break;
	case ATV_BTSC_SC1_AMP:
		u32_ret_0 = atv_set_cmd(ATV_BTSC_GET_SC1_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_BTSC_SC1_NSR:
		u32_ret_0 = atv_set_cmd(ATV_BTSC_GET_SC1_NSR, (long) NULL, (long) NULL);
		break;
	case ATV_BTSC_PILOT_AMP:
		u32_ret_0 = atv_set_cmd(ATV_BTSC_GET_STEREO_PILOT_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_BTSC_SAP_AMP:
		u32_ret_0 = atv_set_cmd(ATV_BTSC_GET_SAP_AMP, (long) NULL, (long) NULL);
		break;
	case ATV_BTSC_SAP_NSR:
		u32_ret_0 = atv_set_cmd(ATV_BTSC_GET_SAP_NSR, (long) NULL, (long) NULL);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s fail !!!\n", __func__);
		break;
	}

	ucontrol->value.integer.value[0] = u32_ret_0;
	ucontrol->value.integer.value[1] = u32_ret_1;
	ucontrol->value.integer.value[2] = u32_ret_2;
	ucontrol->value.integer.value[3] = u32_ret_3;
	return 0;
}

static int atv_control_put_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	long param1 = ucontrol->value.integer.value[0];
	long param2 = ucontrol->value.integer.value[1];

	switch (kcontrol->private_value) {
	case ATV_HIDEV:
		atv_set_hidev((enum atv_hidev_mode) param1);
		break;
	case ATV_PRESCALE:
		atv_set_prescale((enum atv_gain_type) param1, param2);
		break;
	case ATV_STANDARD:
		atv_set_standard((enum atv_standard_type) param1);
		atv_set_sub_standard((enum atv_sub_std) param2);
		break;
	case ATV_SOUNDMODE:
		if (param1 != ATV_SOUND_MODE_AUTO)
			atv_set_soundmode((enum atv_sound_mode) param1);
		else
			atv_set_auto_soundmode(param2);
		break;
	case ATV_THRESHOLD:
		atv_set_threshold((enum atv_thr_type) param1, param2);
		break;
	default:
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA ATV] %s fail !!!\n", __func__);
		break;
	}

	return 0;
}

static int cap_timestamp_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER64;
	uinfo->count = 5;
	return 0;
}

static int cap_timestamp_get_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->private_value;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	int rd_index = dev_ts->rd_index;
	struct mtk_snd_timestamp_info *tsinfo = &dev_ts->ts_info[rd_index];

	ucontrol->value.integer64.value[1] = tsinfo->byte_count;
	ucontrol->value.integer64.value[2] = tsinfo->kts.tv_sec;
	ucontrol->value.integer64.value[3] = tsinfo->kts.tv_nsec;
	ucontrol->value.integer64.value[4] = tsinfo->hts;
	return 0;
}

static int cap_timestamp_put_value(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct capture_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->private_value;
	struct mtk_runtime_struct *dev_runtime = priv->pcm_device[id];
	struct mtk_snd_timestamp *dev_ts = &dev_runtime->timestamp;
	unsigned int rd_index = dev_ts->rd_index;
	struct mtk_snd_timestamp_info *tsinfo = &dev_ts->ts_info[rd_index];
	unsigned int byte_count = tsinfo->byte_count;

	while ((ucontrol->value.integer64.value[0] > byte_count) &&
						(dev_ts->ts_count > 0)) {
		rd_index += 1;
		if (rd_index == 512)
			rd_index = 0;
		tsinfo = &dev_ts->ts_info[rd_index];
		byte_count = tsinfo->byte_count;
		if (ucontrol->value.integer64.value[0] < byte_count)
			break;
		dev_ts->ts_count -= 1;
		dev_ts->rd_index = rd_index;
	}
	return 0;
}

static const struct snd_kcontrol_new cap_controls[] = {
	SOC_INTEGER_CAP("HDMI-RX1 MUTE EVENT", RX1_MUTE_EVENT),
	SOC_INTEGER_CAP("HDMI-RX1 NONPCM EVENT", RX1_NONPCM_EVENT),
	SOC_INTEGER_CAP("HDMI-RX1 NONPCM FLAG", RX1_NONPCM_FLAG),
	SOC_INTEGER_CAP("HDMI-RX1 FASTLOCK MODE", RX1_FASTLOCK),
	SOC_INTEGER_CAP("HDMI-RX1 CHANNEL STATUS", RX1_CS),
	SOC_TIMESTAMP_CAP("HDMI-RX1 TIMESTAMP", HDMI_RX1_DEV),
	SOC_INTEGER_CAP("HDMI-RX2 MUTE EVENT", RX2_MUTE_EVENT),
	SOC_INTEGER_CAP("HDMI-RX2 NONPCM EVENT", RX2_NONPCM_EVENT),
	SOC_INTEGER_CAP("HDMI-RX2 NONPCM FLAG", RX2_NONPCM_FLAG),
	SOC_INTEGER_CAP("HDMI-RX2 FASTLOCK MODE", RX2_FASTLOCK),
	SOC_INTEGER_CAP("HDMI-RX2 CHANNEL STATUS", RX2_CS),
	SOC_TIMESTAMP_CAP("HDMI-RX2 TIMESTAMP", HDMI_RX2_DEV),
	SOC_INTEGER_ATV("ATV HIDEV", ATV_HIDEV),
	SOC_INTEGER_ATV("ATV PRESCALE", ATV_PRESCALE),
	SOC_INTEGER_ATV("ATV THRESHOLD", ATV_THRESHOLD),
	SOC_INTEGER_ATV("ATV CARRIER STATUS", ATV_CARRIER_STATUS),
	SOC_INTEGER_ATV("ATV STANDARD", ATV_STANDARD),
	SOC_INTEGER_ATV("ATV SOUNDMODE", ATV_SOUNDMODE),
	SOC_INTEGER_ATV("ATV SYS CAPABILITY", ATV_SYS_CAPABILITY),
	SOC_INTEGER_ATV("ATV AUTO DET STD RESULT", ATV_AUTO_DET_STD_RESULT),
	SOC_INTEGER_ATV("ATV PAL SC1 AMP", ATV_PAL_SC1_AMP),
	SOC_INTEGER_ATV("ATV PAL SC1 NSR", ATV_PAL_SC1_NSR),
	SOC_INTEGER_ATV("ATV PAL SC2 AMP", ATV_PAL_SC2_AMP),
	SOC_INTEGER_ATV("ATV PAL SC2 NSR", ATV_PAL_SC2_NSR),
	SOC_INTEGER_ATV("ATV PAL PILOT AMP", ATV_PAL_PILOT_AMP),
	SOC_INTEGER_ATV("ATV PAL PILOT PHASE", ATV_PAL_PILOT_PHASE),
	SOC_INTEGER_ATV("ATV PAL STEREO DUAL ID", ATV_PAL_STEREO_DUAL_ID),
	SOC_INTEGER_ATV("ATV PAL NICAM PHASE ERR", ATV_PAL_NICAM_PHASE_ERR),
	SOC_INTEGER_ATV("ATV PAL BAND STRENGTH", ATV_PAL_BAND_STRENGTH),
	SOC_INTEGER_ATV("ATV BTSC SC1 AMP", ATV_BTSC_SC1_AMP),
	SOC_INTEGER_ATV("ATV BTSC SC1 NSR", ATV_BTSC_SC1_NSR),
	SOC_INTEGER_ATV("ATV BTSC PILOT AMP", ATV_BTSC_PILOT_AMP),
	SOC_INTEGER_ATV("ATV BTSC SAP AMP", ATV_BTSC_SAP_AMP),
	SOC_INTEGER_ATV("ATV BTSC SAP NSR", ATV_BTSC_SAP_NSR),
	SOC_TIMESTAMP_CAP("ATV TIMESTAMP", ATV_DEV),
	SOC_TIMESTAMP_CAP("ADC TIMESTAMP", ADC_DEV),
	SOC_TIMESTAMP_CAP("ADC1 TIMESTAMP", ADC1_DEV)
};

static const struct snd_soc_component_driver cap_dai_component = {
	.name			= CAP_NAME,
	.probe			= capture_probe,
	.remove			= capture_remove,
	.ops			= &cap_pcm_ops,
	.controls		= cap_controls,
	.num_controls	= ARRAY_SIZE(cap_controls),
};

static unsigned int cap_get_dts_u32(struct device_node *np, const char *name)
{
	unsigned int value;
	int ret;

	ret = of_property_read_u32(np, name, &value);
	if (ret)
		WARN_ON("[ALSA CAP]Can't get DTS property\n");

	return value;
}

static int cap_get_dts_s32(struct device_node *np, const char *name)
{
	int value;
	int ret;

	ret = of_property_read_s32(np, name, &value);
	if (ret)
		WARN_ON("[ALSA CAP]Can't get DTS property\n");

	return value;
}

static int capture_parse_registers(void)
{
	struct device_node *np;

	memset(&REG, 0x0, sizeof(struct capture_register));

	np = of_find_node_by_name(NULL, "capture-register");
	if (!np)
		return -ENODEV;

	REG.PAS_CTRL_0 = cap_get_dts_u32(np, "reg_pas_ctrl_0");
	REG.PAS_CTRL_9 = cap_get_dts_u32(np, "reg_pas_ctrl_9");
	REG.PAS_CTRL_39 = cap_get_dts_u32(np, "reg_pas_ctrl_39");
	REG.PAS_CTRL_48 = cap_get_dts_u32(np, "reg_pas_ctrl_48");
	REG.PAS_CTRL_49 = cap_get_dts_u32(np, "reg_pas_ctrl_49");
	REG.PAS_CTRL_50 = cap_get_dts_u32(np, "reg_pas_ctrl_50");
	REG.PAS_CTRL_64 = cap_get_dts_u32(np, "reg_pas_ctrl_64");
	REG.PAS_CTRL_65 = cap_get_dts_u32(np, "reg_pas_ctrl_65");
	REG.PAS_CTRL_66 = cap_get_dts_u32(np, "reg_pas_ctrl_66");
	REG.PAS_CTRL_73 = cap_get_dts_u32(np, "reg_pas_ctrl_73");
	REG.PAS_CTRL_74 = cap_get_dts_u32(np, "reg_pas_ctrl_74");
	REG.HDMI_EVENT_0 = cap_get_dts_u32(np, "reg_hdmi_event_0");
	REG.HDMI_EVENT_1 = cap_get_dts_u32(np, "reg_hdmi_event_1");
	REG.HDMI_EVENT_2 = cap_get_dts_u32(np, "reg_hdmi_event_2");
	REG.HDMI2_EVENT_0 = cap_get_dts_u32(np, "reg_hdmi2_event_0");
	REG.HDMI2_EVENT_1 = cap_get_dts_u32(np, "reg_hdmi2_event_1");
	REG.HDMI2_EVENT_2 = cap_get_dts_u32(np, "reg_hdmi2_event_2");
	REG.HDMI2_EVENT_3 = cap_get_dts_u32(np, "reg_hdmi2_event_3");
	REG.HDMI2_EVENT_4 = cap_get_dts_u32(np, "reg_hdmi2_event_4");
	REG.SE_IDMA_CTRL_0 = cap_get_dts_u32(np, "reg_se_idma_ctrl_0");
	REG.SE_BDMA_CFG = cap_get_dts_u32(np, "reg_se_bdma_cfg");
	REG.FD230_SEL = cap_get_dts_u32(np, "reg_fd230_sel");
	REG.SE_DSP_BRG_DATA_L = cap_get_dts_u32(np, "reg_se_dsp_brg_data_l");
	REG.SE_DSP_BRG_DATA_H = cap_get_dts_u32(np, "reg_se_dsp_brg_data_h");
	REG.SE_IDMA_WRBASE_ADDR_L = cap_get_dts_u32(np, "reg_se_idma_wrbase_addr_l");
	REG.SE_IDMA_RDBASE_ADDR_L = cap_get_dts_u32(np, "reg_se_idma_rdbase_addr_l");
	REG.SE_IDMA_RDDATA_H_0 = cap_get_dts_u32(np, "reg_se_idma_rddata_h_0");
	REG.SE_IDMA_RDDATA_H_1 = cap_get_dts_u32(np, "reg_se_idma_rddata_h_1");
	REG.SE_IDMA_RDDATA_L = cap_get_dts_u32(np, "reg_se_idma_rddata_l");
	REG.SE_AUD_CTRL = cap_get_dts_u32(np, "reg_se_aud_ctrl");
	REG.SE_MBASE_H = cap_get_dts_u32(np, "reg_se_mbase_h");
	REG.SE_MSIZE_H = cap_get_dts_u32(np, "reg_se_msize_h");
	REG.SE_MCFG = cap_get_dts_u32(np, "reg_se_mcfg");
	REG.SE_MBASE_EXT = cap_get_dts_u32(np, "reg_se_mbase_ext");
	REG.SE_INPUT_CFG = cap_get_dts_u32(np, "reg_input_cfg");
	REG.STATUS_HDMI_CS = cap_get_dts_u32(np, "reg_status_hdmi_cs");
	REG.STATUS_HDMI_PC = cap_get_dts_u32(np, "reg_status_hdmi_pc");
	REG.STATUS_HDMI2_PC = cap_get_dts_u32(np, "reg_status_hdmi2_pc");
	REG.HDMI_TBUS_3 = cap_get_dts_u32(np, "reg_hdmi_tbus_3");
	REG.HDMI_CFG_6 = cap_get_dts_u32(np, "reg_hdmi_cfg_6");
	REG.HDMI2_CFG_29 = cap_get_dts_u32(np, "reg_hdmi2_cfg_29");
	REG.HDMI_MODE_CFG = cap_get_dts_u32(np, "reg_hdmi_mode_cfg");
	REG.HDMI2_MODE_CFG1 = cap_get_dts_u32(np, "reg_hdmi2_mode_cfg1");
	REG.HDMI2_MODE_CFG2 = cap_get_dts_u32(np, "reg_hdmi2_mode_cfg2");
	REG.DECODER1_CFG = cap_get_dts_u32(np, "reg_decoder1_cfg");
	REG.DECODER2_CFG = cap_get_dts_u32(np, "reg_decoder2_cfg");
	REG.CLK_CFG6 = cap_get_dts_u32(np, "reg_clk_cfg6");
	REG.SIF_AGC_CFG6 = cap_get_dts_u32(np, "reg_sif_agc_cfg6");
	REG.ASIF_AMUX = cap_get_dts_u32(np, "reg_asif_amux");
	REG.I2S_INPUT_CFG = cap_get_dts_u32(np, "reg_i2s_input_cfg");
	REG.I2S_OUT2_CFG = cap_get_dts_u32(np, "reg_i2s_out2_cfg");
	REG.CAP_HDMI_CTRL = cap_get_dts_u32(np, "reg_cap_hdmi_ctrl");
	REG.CAP_HDMI_INFO = cap_get_dts_u32(np, "reg_cap_hdmi_info");
	REG.CAP_DMA2_CTRL = cap_get_dts_u32(np, "reg_cap_dma2_ctrl");
	REG.CAP_DMA2_RPTR = cap_get_dts_u32(np, "reg_cap_dma2_rptr");
	REG.CAP_DMA2_WPTR = cap_get_dts_u32(np, "reg_cap_dma2_wptr");
	REG.CAP_DMA4_CTRL = cap_get_dts_u32(np, "reg_cap_dma4_ctrl");
	REG.CAP_DMA4_RPTR = cap_get_dts_u32(np, "reg_cap_dma4_rptr");
	REG.CAP_DMA4_WPTR = cap_get_dts_u32(np, "reg_cap_dma4_wptr");
	REG.CAP_DMA7_CTRL = cap_get_dts_u32(np, "reg_cap_dma7_ctrl");
	REG.CAP_DMA7_RPTR = cap_get_dts_u32(np, "reg_cap_dma7_rptr");
	REG.CAP_DMA7_WPTR = cap_get_dts_u32(np, "reg_cap_dma7_wptr");
	REG.CAP_DMA8_CTRL = cap_get_dts_u32(np, "reg_cap_dma8_ctrl");
	REG.CAP_DMA8_RPTR = cap_get_dts_u32(np, "reg_cap_dma8_rptr");
	REG.CAP_DMA8_WPTR = cap_get_dts_u32(np, "reg_cap_dma8_wptr");
	REG.CAP_DMA9_CTRL = cap_get_dts_u32(np, "reg_cap_dma9_ctrl");
	REG.CAP_DMA9_RPTR = cap_get_dts_u32(np, "reg_cap_dma9_rptr");
	REG.CAP_DMA9_WPTR = cap_get_dts_u32(np, "reg_cap_dma9_wptr");
	REG.CAP_DMA10_CTRL = cap_get_dts_u32(np, "reg_cap_dma10_ctrl");
	REG.CAP_DMA10_RPTR = cap_get_dts_u32(np, "reg_cap_dma10_rptr");
	REG.CAP_DMA10_WPTR = cap_get_dts_u32(np, "reg_cap_dma10_wptr");
	REG.ATV_PALY_CMD = cap_get_dts_u32(np, "reg_atv_play_cmd");
	REG.ATV_STD_CMD = cap_get_dts_u32(np, "reg_atv_std_cmd");
	REG.ATV_COMM_CFG = cap_get_dts_u32(np, "reg_atv_comm_cfg");
	REG.ATV_SND_MODE_1 = cap_get_dts_u32(np, "reg_atv_snd_mode_1");
	REG.ATV_SND_MODE_2 = cap_get_dts_u32(np, "reg_atv_snd_mode_2");
	REG.ATV_DETECTION_RESULT = cap_get_dts_u32(np, "reg_atv_detection_result");
	REG.ATV_SND_CARRIER_STATUS_1 = cap_get_dts_u32(np, "reg_atv_snd_carrier_status_1");
	REG.ATV_SND_CARRIER_STATUS_2 = cap_get_dts_u32(np, "reg_atv_snd_carrier_status_2");
	REG.DBG_CMD_1 = cap_get_dts_u32(np, "reg_dbg_cmd_1");
	REG.DBG_CMD_2 = cap_get_dts_u32(np, "reg_dbg_cmd_2");
	REG.DBG_RESULT_1 = cap_get_dts_u32(np, "reg_dbg_result_1");
	REG.DBG_RESULT_2 = cap_get_dts_u32(np, "reg_dbg_result_2");
	REG.SE_AID_CTRL = cap_get_dts_u32(np, "reg_se_aid_ctrl");
	REG.ANALOG_MUX = cap_get_dts_u32(np, "reg_analog_mux");
	REG.IRQ0_IDX = cap_get_dts_u32(np, "reg_irq0_idx");
	REG.IRQ1_IDX = cap_get_dts_u32(np, "reg_irq1_idx");
	REG.HDMI_CS0 = cap_get_dts_u32(np, "reg_hdmi_status_0");
	REG.HDMI_CS1 = cap_get_dts_u32(np, "reg_hdmi_status_1");
	REG.HDMI_CS2 = cap_get_dts_u32(np, "reg_hdmi_status_2");
	REG.HDMI_CS3 = cap_get_dts_u32(np, "reg_hdmi_status_3");
	REG.HDMI_CS4 = cap_get_dts_u32(np, "reg_hdmi_status_4");
	REG.HDMI_CS5 = cap_get_dts_u32(np, "reg_hdmi_status_5");
	REG.HDMI_CS6 = cap_get_dts_u32(np, "reg_hdmi_status_6");
	REG.HDMI_CS7 = cap_get_dts_u32(np, "reg_hdmi_status_7");
	REG.HDMI_CS8 = cap_get_dts_u32(np, "reg_hdmi_status_8");
	REG.HDMI_CS9 = cap_get_dts_u32(np, "reg_hdmi_status_9");
	REG.HDMI_CS10 = cap_get_dts_u32(np, "reg_hdmi_status_10");
	REG.HDMI_CS11 = cap_get_dts_u32(np, "reg_hdmi_status_11");
	REG.HWTSG_CFG1 = cap_get_dts_u32(np, "reg_hwtsg_cfg1");
	REG.HWTSG_CFG2 = cap_get_dts_u32(np, "reg_hwtsg_cfg2");
	REG.HWTSG_CFG4 = cap_get_dts_u32(np, "reg_hwtsg_cfg4");
	REG.HWTSG_TS_L = cap_get_dts_u32(np, "reg_hwtsg_ts_l");
	REG.HWTSG_TS_H = cap_get_dts_u32(np, "reg_hwtsg_ts_h");
	REG.HDMI2_CS0 = cap_get_dts_u32(np, "reg_hdmi2_status_0");
	REG.HDMI2_CS1 = cap_get_dts_u32(np, "reg_hdmi2_status_1");
	REG.HDMI2_CS2 = cap_get_dts_u32(np, "reg_hdmi2_status_2");
	REG.HDMI2_CS3 = cap_get_dts_u32(np, "reg_hdmi2_status_3");
	REG.HDMI2_CS4 = cap_get_dts_u32(np, "reg_hdmi2_status_4");
	REG.HDMI2_CS5 = cap_get_dts_u32(np, "reg_hdmi2_status_5");
	REG.HDMI2_CS6 = cap_get_dts_u32(np, "reg_hdmi2_status_6");
	REG.HDMI2_CS7 = cap_get_dts_u32(np, "reg_hdmi2_status_7");
	REG.HDMI2_CS8 = cap_get_dts_u32(np, "reg_hdmi2_status_8");
	REG.HDMI2_CS9 = cap_get_dts_u32(np, "reg_hdmi2_status_9");
	REG.HDMI2_CS10 = cap_get_dts_u32(np, "reg_hdmi2_status_10");
	REG.HDMI2_CS11 = cap_get_dts_u32(np, "reg_hdmi2_status_11");
	REG.HWTSG2_CFG1 = cap_get_dts_u32(np, "reg_hwtsg2_cfg1");
	REG.HWTSG2_CFG2 = cap_get_dts_u32(np, "reg_hwtsg2_cfg2");
	REG.HWTSG2_CFG4 = cap_get_dts_u32(np, "reg_hwtsg2_cfg4");
	REG.HWTSG2_TS_L = cap_get_dts_u32(np, "reg_hwtsg2_ts_l");
	REG.HWTSG2_TS_H = cap_get_dts_u32(np, "reg_hwtsg2_ts_h");

	np = of_find_node_by_name(NULL, "channel-mux-register");
	if (!np)
		return -ENODEV;

	// TODO:
	REG.CHANNEL_M7_CFG = cap_get_dts_u32(np, "reg_ch_m7_cfg");
	REG.CHANNEL_M8_CFG = cap_get_dts_u32(np, "reg_ch_m8_cfg");
	REG.CHANNEL_M15_CFG = cap_get_dts_u32(np, "reg_ch_m15_cfg");
	REG.CHANNEL_M16_CFG = cap_get_dts_u32(np, "reg_ch_m16_cfg");

	return 0;
}

static int capture_parse_mmap(struct device_node *np, struct capture_priv *priv)
{
	struct of_mmap_info_data of_mmap_info = {};
	int ret;

	ret = of_mtk_get_reserved_memory_info_by_idx(np,
						0, &of_mmap_info);
	if (ret) {
		pr_err("[ALSA CAP]get audio mmap fail\n");
		return -EINVAL;
	}

	priv->mem_info.miu = of_mmap_info.miu;
	priv->mem_info.bus_addr = of_mmap_info.start_bus_address;
	priv->mem_info.buffer_size = of_mmap_info.buffer_size;

	return 0;
}

int capture_parse_memory(struct device_node *np, struct capture_priv *priv)
{
	int ret;

	ret = of_property_read_u32(np, "hdmi1_dma_offset",
					&priv->cap_dma_offset[HDMI_RX1_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get hdmi1 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "hdmi1_dma_size",
					&priv->cap_dma_size[HDMI_RX1_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get hdmi1 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "hdmi2_dma_offset",
					&priv->cap_dma_offset[HDMI_RX2_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get hdmi2 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "hdmi2_dma_size",
					&priv->cap_dma_size[HDMI_RX2_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get hdmi2 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture2_dma_offset",
					&priv->cap_dma_offset[ATV_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture2 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture2_dma_size",
					&priv->cap_dma_size[ATV_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture2 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture4_dma_offset",
					&priv->cap_dma_offset[ADC_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture4 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture4_dma_size",
					&priv->cap_dma_size[ADC_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture4 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture7_dma_offset",
					&priv->cap_dma_offset[ADC1_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture7 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture7_dma_size",
					&priv->cap_dma_size[ADC1_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture7 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture8_dma_offset",
					&priv->cap_dma_offset[I2S_RX2_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture8 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture8_dma_size",
					&priv->cap_dma_size[I2S_RX2_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture8 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture9_dma_offset",
					&priv->cap_dma_offset[LOOPBACK_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture9 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture9_dma_size",
					&priv->cap_dma_size[LOOPBACK_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture9 dma size\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture10_dma_offset",
					&priv->cap_dma_offset[HFP_RX_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture10 dma offset\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "capture10_dma_size",
					&priv->cap_dma_size[HFP_RX_DEV]);
	if (ret) {
		pr_err("[ALSA CAP]can't get capture10 dma size\n");
		return -EINVAL;
	}
	return 0;
}

int capture_parse_atv_info(struct device_node *np, struct capture_priv *priv)
{
	// ATV parser prescale
	priv->atv_gain.pal_a2_fm_m = cap_get_dts_s32(np, "pal_a2_fm_m");
	priv->atv_gain.pal_hidev_m = cap_get_dts_s32(np, "pal_hidev_m");
	priv->atv_gain.pal_a2_fm_x = cap_get_dts_s32(np, "pal_a2_fm_x");
	priv->atv_gain.pal_hidev_x = cap_get_dts_s32(np, "pal_hidev_x");
	priv->atv_gain.pal_nicam = cap_get_dts_s32(np, "pal_nicam");
	priv->atv_gain.pal_am = cap_get_dts_s32(np, "pal_am");

	priv->atv_gain.btsc_total = cap_get_dts_s32(np, "btsc_total");
	priv->atv_gain.btsc_mono = cap_get_dts_s32(np, "btsc_mono");
	priv->atv_gain.btsc_stereo = cap_get_dts_s32(np, "btsc_stereo");
	priv->atv_gain.btsc_sap = cap_get_dts_s32(np, "btsc_sap");

	// ATV parser threshold
	priv->atv_thr.a2_m_carrier1_on_amp = cap_get_dts_u32(np, "a2_m_carrier1_on_amp");
	priv->atv_thr.a2_m_carrier1_off_amp = cap_get_dts_u32(np, "a2_m_carrier1_off_amp");
	priv->atv_thr.a2_m_carrier1_on_nsr = cap_get_dts_u32(np, "a2_m_carrier1_on_nsr");
	priv->atv_thr.a2_m_carrier1_off_nsr = cap_get_dts_u32(np, "a2_m_carrier1_off_nsr");
	priv->atv_thr.a2_m_carrier2_on_amp = cap_get_dts_u32(np, "a2_m_carrier2_on_amp");
	priv->atv_thr.a2_m_carrier2_off_amp = cap_get_dts_u32(np, "a2_m_carrier2_off_amp");
	priv->atv_thr.a2_m_carrier2_on_nsr = cap_get_dts_u32(np, "a2_m_carrier2_on_nsr");
	priv->atv_thr.a2_m_carrier2_off_nsr = cap_get_dts_u32(np, "a2_m_carrier2_off_nsr");
	priv->atv_thr.a2_m_a2_pilot_on_amp = cap_get_dts_u32(np, "a2_m_a2_pilot_on_amp");
	priv->atv_thr.a2_m_a2_pilot_off_amp = cap_get_dts_u32(np, "a2_m_a2_pilot_off_amp");
	priv->atv_thr.a2_m_pilot_phase_on_thd = cap_get_dts_u32(np, "a2_m_pilot_phase_on_thd");
	priv->atv_thr.a2_m_pilot_phase_off_thd = cap_get_dts_u32(np, "a2_m_pilot_phase_off_thd");
	priv->atv_thr.a2_m_polit_mode_valid_ratio
					= cap_get_dts_u32(np, "a2_m_polit_mode_valid_ratio");
	priv->atv_thr.a2_m_polit_mode_invalid_ratio
					= cap_get_dts_u32(np, "a2_m_polit_mode_invalid_ratio");
	priv->atv_thr.a2_m_carrier2_output_thr
					= cap_get_dts_u32(np, "a2_m_carrier2_output_thr");

	priv->atv_thr.a2_bg_carrier1_on_amp = cap_get_dts_u32(np, "a2_bg_carrier1_on_amp");
	priv->atv_thr.a2_bg_carrier1_off_amp = cap_get_dts_u32(np, "a2_bg_carrier1_off_amp");
	priv->atv_thr.a2_bg_carrier1_on_nsr = cap_get_dts_u32(np, "a2_bg_carrier1_on_nsr");
	priv->atv_thr.a2_bg_carrier1_off_nsr = cap_get_dts_u32(np, "a2_bg_carrier1_off_nsr");
	priv->atv_thr.a2_bg_carrier2_on_amp = cap_get_dts_u32(np, "a2_bg_carrier2_on_amp");
	priv->atv_thr.a2_bg_carrier2_off_amp = cap_get_dts_u32(np, "a2_bg_carrier2_off_amp");
	priv->atv_thr.a2_bg_carrier2_on_nsr = cap_get_dts_u32(np, "a2_bg_carrier2_on_nsr");
	priv->atv_thr.a2_bg_carrier2_off_nsr = cap_get_dts_u32(np, "a2_bg_carrier2_off_nsr");
	priv->atv_thr.a2_bg_a2_pilot_on_amp = cap_get_dts_u32(np, "a2_bg_a2_pilot_on_amp");
	priv->atv_thr.a2_bg_a2_pilot_off_amp = cap_get_dts_u32(np, "a2_bg_a2_pilot_off_amp");
	priv->atv_thr.a2_bg_pilot_phase_on_thd = cap_get_dts_u32(np, "a2_bg_pilot_phase_on_thd");
	priv->atv_thr.a2_bg_pilot_phase_off_thd = cap_get_dts_u32(np, "a2_bg_pilot_phase_off_thd");
	priv->atv_thr.a2_bg_polit_mode_valid_ratio
					= cap_get_dts_u32(np, "a2_bg_polit_mode_valid_ratio");
	priv->atv_thr.a2_bg_polit_mode_invalid_ratio
					= cap_get_dts_u32(np, "a2_bg_polit_mode_invalid_ratio");
	priv->atv_thr.a2_bg_carrier2_output_thr
					= cap_get_dts_u32(np, "a2_bg_carrier2_output_thr");

	priv->atv_thr.a2_dk_carrier1_on_amp = cap_get_dts_u32(np, "a2_dk_carrier1_on_amp");
	priv->atv_thr.a2_dk_carrier1_off_amp = cap_get_dts_u32(np, "a2_dk_carrier1_off_amp");
	priv->atv_thr.a2_dk_carrier1_on_nsr = cap_get_dts_u32(np, "a2_dk_carrier1_on_nsr");
	priv->atv_thr.a2_dk_carrier1_off_nsr = cap_get_dts_u32(np, "a2_dk_carrier1_off_nsr");
	priv->atv_thr.a2_dk_carrier2_on_amp = cap_get_dts_u32(np, "a2_dk_carrier2_on_amp");
	priv->atv_thr.a2_dk_carrier2_off_amp = cap_get_dts_u32(np, "a2_dk_carrier2_off_amp");
	priv->atv_thr.a2_dk_carrier2_on_nsr = cap_get_dts_u32(np, "a2_dk_carrier2_on_nsr");
	priv->atv_thr.a2_dk_carrier2_off_nsr = cap_get_dts_u32(np, "a2_dk_carrier2_off_nsr");
	priv->atv_thr.a2_dk_a2_pilot_on_amp = cap_get_dts_u32(np, "a2_dk_a2_pilot_on_amp");
	priv->atv_thr.a2_dk_a2_pilot_off_amp = cap_get_dts_u32(np, "a2_dk_a2_pilot_off_amp");
	priv->atv_thr.a2_dk_pilot_phase_on_thd = cap_get_dts_u32(np, "a2_dk_pilot_phase_on_thd");
	priv->atv_thr.a2_dk_pilot_phase_off_thd = cap_get_dts_u32(np, "a2_dk_pilot_phase_off_thd");
	priv->atv_thr.a2_dk_polit_mode_valid_ratio
					= cap_get_dts_u32(np, "a2_dk_polit_mode_valid_ratio");
	priv->atv_thr.a2_dk_polit_mode_invalid_ratio
					= cap_get_dts_u32(np, "a2_dk_polit_mode_invalid_ratio");
	priv->atv_thr.a2_dk_carrier2_output_thr
					= cap_get_dts_u32(np, "a2_dk_carrier2_output_thr");

	priv->atv_thr.fm_i_carrier1_on_amp = cap_get_dts_u32(np, "fm_i_carrier1_on_amp");
	priv->atv_thr.fm_i_carrier1_off_amp = cap_get_dts_u32(np, "fm_i_carrier1_off_amp");
	priv->atv_thr.fm_i_carrier1_on_nsr = cap_get_dts_u32(np, "fm_i_carrier1_on_nsr");
	priv->atv_thr.fm_i_carrier1_off_nsr = cap_get_dts_u32(np, "fm_i_carrier1_off_nsr");

	priv->atv_thr.am_carrier1_on_amp = cap_get_dts_u32(np, "am_carrier1_on_amp");
	priv->atv_thr.am_carrier1_off_amp = cap_get_dts_u32(np, "am_carrier1_off_amp");
	priv->atv_thr.am_agc_ref = cap_get_dts_u32(np, "am_agc_ref");

	priv->atv_thr.hidev_m_carrier1_on_amp = cap_get_dts_u32(np, "hidev_m_carrier1_on_amp");
	priv->atv_thr.hidev_m_carrier1_off_amp = cap_get_dts_u32(np, "hidev_m_carrier1_off_amp");
	priv->atv_thr.hidev_m_carrier1_on_nsr = cap_get_dts_u32(np, "hidev_m_carrier1_on_nsr");
	priv->atv_thr.hidev_m_carrier1_off_nsr = cap_get_dts_u32(np, "hidev_m_carrier1_off_nsr");

	priv->atv_thr.hidev_bg_carrier1_on_amp = cap_get_dts_u32(np, "hidev_bg_carrier1_on_amp");
	priv->atv_thr.hidev_bg_carrier1_off_amp = cap_get_dts_u32(np, "hidev_bg_carrier1_off_amp");
	priv->atv_thr.hidev_bg_carrier1_on_nsr = cap_get_dts_u32(np, "hidev_bg_carrier1_on_nsr");
	priv->atv_thr.hidev_bg_carrier1_off_nsr = cap_get_dts_u32(np, "hidev_bg_carrier1_off_nsr");

	priv->atv_thr.hidev_dk_carrier1_on_amp = cap_get_dts_u32(np, "hidev_dk_carrier1_on_amp");
	priv->atv_thr.hidev_dk_carrier1_off_amp = cap_get_dts_u32(np, "hidev_dk_carrier1_off_amp");
	priv->atv_thr.hidev_dk_carrier1_on_nsr = cap_get_dts_u32(np, "hidev_dk_carrier1_on_nsr");
	priv->atv_thr.hidev_dk_carrier1_off_nsr = cap_get_dts_u32(np, "hidev_dk_carrier1_off_nsr");

	priv->atv_thr.hidev_i_carrier1_on_amp = cap_get_dts_u32(np, "hidev_i_carrier1_on_amp");
	priv->atv_thr.hidev_i_carrier1_off_amp = cap_get_dts_u32(np, "hidev_i_carrier1_off_amp");
	priv->atv_thr.hidev_i_carrier1_on_nsr = cap_get_dts_u32(np, "hidev_i_carrier1_on_nsr");
	priv->atv_thr.hidev_i_carrier1_off_nsr = cap_get_dts_u32(np, "hidev_i_carrier1_off_nsr");

	priv->atv_thr.nicam_bg_nicam_on_sigerr = cap_get_dts_u32(np, "nicam_bg_nicam_on_sigerr");
	priv->atv_thr.nicam_bg_nicam_off_sigerr = cap_get_dts_u32(np, "nicam_bg_nicam_off_sigerr");
	priv->atv_thr.nicam_bg_reset_hw_siger = cap_get_dts_u32(np, "nicam_bg_reset_hw_siger");

	priv->atv_thr.nicam_i_nicam_on_sigerr = cap_get_dts_u32(np, "nicam_i_nicam_on_sigerr");
	priv->atv_thr.nicam_i_nicam_off_sigerr = cap_get_dts_u32(np, "nicam_i_nicam_off_sigerr");
	priv->atv_thr.nicam_i_reset_hw_siger = cap_get_dts_u32(np, "nicam_i_reset_hw_siger");

	// pal end
	priv->atv_thr.pal_thr_type_end = cap_get_dts_u32(np, "pal_thr_type_end");

	priv->atv_thr.btsc_mono_on_nsr = cap_get_dts_u32(np, "btsc_mono_on_nsr");
	priv->atv_thr.btsc_mono_off_nsr = cap_get_dts_u32(np, "btsc_mono_off_nsr");
	priv->atv_thr.btsc_pilot_on_amplitude = cap_get_dts_u32(np, "btsc_pilot_on_amplitude");
	priv->atv_thr.btsc_pilot_off_amplitude = cap_get_dts_u32(np, "btsc_pilot_off_amplitude");
	priv->atv_thr.btsc_sap_on_nsr = cap_get_dts_u32(np, "btsc_sap_on_nsr");
	priv->atv_thr.btsc_sap_off_nsr = cap_get_dts_u32(np, "btsc_sap_off_nsr");
	priv->atv_thr.btsc_stereo_on = cap_get_dts_u32(np, "btsc_stereo_on");
	priv->atv_thr.btsc_stereo_off = cap_get_dts_u32(np, "btsc_stereo_off");
	priv->atv_thr.btsc_sap_on_amplitude = cap_get_dts_u32(np, "btsc_sap_on_amplitude");
	priv->atv_thr.btsc_sap_off_amplitude = cap_get_dts_u32(np, "btsc_sap_off_amplitude");

	// btsc end
	priv->atv_thr.btsc_thr_type_end = cap_get_dts_u32(np, "btsc_thr_type_end");

	return 0;
}

int capture_dev_probe(struct platform_device *pdev)
{
	struct capture_priv *priv;
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

	for (i = 0; i < CAP_DEV_NUM; i++) {
		priv->pcm_device[i] = devm_kzalloc(&pdev->dev,
				sizeof(struct mtk_runtime_struct), GFP_KERNEL);
		/* Allocate system memory for Specific Copy */
		priv->pcm_device[i]->capture.c_buf.addr = vmalloc(MAX_CAP_BUF_SIZE);
		if ((!priv->pcm_device[i]) ||
			(!priv->pcm_device[i]->capture.c_buf.addr))
			return -ENOMEM;
	}

	/* parse dts */
	platform_set_drvdata(pdev, priv);

	// MB112E00
	priv->irq0 = platform_get_irq(pdev, 3);
	if (priv->irq0 < 0)
		dev_err(dev, "[ALSA CAP]fail to get irq0\n");
	tasklet_init(&priv->irq0_tasklet, _tasklet_verify_hw, 0);
	tasklet_disable(&priv->irq0_tasklet);

	// MB112E10
	priv->irq1 = platform_get_irq(pdev, 2);
	if (priv->irq1 < 0)
		dev_err(dev, "[ALSA CAP]fail to get irq1\n");

	// HDMMI_RX1
	priv->irq2 = platform_get_irq(pdev, 0);
	if (priv->irq2 < 0)
		dev_err(dev, "[ALSA CAP]fail to get irq2\n");

	// HDMMI_RX2
	// TODO:
	priv->irq3 = platform_get_irq(pdev, 1);
	if (priv->irq3 < 0)
		dev_err(dev, "[ALSA CAP]fail to get irq3\n");

	ret = devm_snd_soc_register_component(&pdev->dev,
					&cap_dai_component,
					cap_dais,
					ARRAY_SIZE(cap_dais));
	if (ret) {
		dev_err(dev, "[ALSA CAP]soc_register_component fail %d\n", ret);
		return ret;
	}

	ret = of_property_read_u64(node, "memory_bus_base",
					&priv->mem_info.memory_bus_base);
	if (ret) {
		dev_err(dev, "[ALSA CAP]can't get miu bus base\n");
		return -EINVAL;
	}

	/* parse registers */
	ret = capture_parse_registers();
	if (ret) {
		dev_err(dev, "[ALSA CAP]parse register fail %d\n", ret);
		return ret;
	}

	/* parse dma memory info */
	ret = capture_parse_memory(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA CAP]parse memory fail %d\n", ret);
		return ret;
	}

	/* parse atv prescale/threshold info */
	ret = capture_parse_atv_info(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA CAP]parse atv info %d\n", ret);
		return ret;
	}

	/* parse mmap */
	ret = capture_parse_mmap(node, priv);
	if (ret) {
		dev_err(dev, "[ALSA CAP]parse mmap fail %d\n", ret);
		return ret;
	}

	return 0;
}

int capture_dev_remove(struct platform_device *pdev)
{
	struct capture_priv *priv = platform_get_drvdata(pdev);
	int i;

	tasklet_kill(&priv->irq0_tasklet);

	for (i = 0; i < CAP_DEV_NUM; i++) {
		if (priv->pcm_device[i]->capture.c_buf.addr != NULL) {
			vfree(priv->pcm_device[i]->capture.c_buf.addr);
			priv->pcm_device[i]->capture.c_buf.addr = NULL;
		}
	}

	return 0;
}

MODULE_DESCRIPTION("Mediatek ALSA SoC Capture platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dsp-cap");
