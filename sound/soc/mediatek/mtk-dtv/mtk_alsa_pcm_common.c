// SPDX-License-Identifier: GPL-2.0
/*
 * mtk_alsa_pcm_common.c  --  Mediatek DTV platform
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <sound/soc.h>

#include "mtk_alsa_pcm_common.h"

unsigned char sinetone[SINETONE_SIZE] = {
0x01, 0x00, 0x1A, 0x02, 0x2B, 0x04, 0x2B, 0x06, 0x0E, 0x08,
0xCF, 0x09, 0x65, 0x0B, 0xC8, 0x0C, 0xF4, 0x0D, 0xE3, 0x0E,
0x91, 0x0F, 0xFA, 0x0F, 0x1E, 0x10, 0xFA, 0x0F, 0x91, 0x0F,
0xE3, 0x0E, 0xF4, 0x0D, 0xC9, 0x0C, 0x64, 0x0B, 0xD0, 0x09,
0x0F, 0x08, 0x2B, 0x06, 0x2C, 0x04, 0x1B, 0x02, 0x00, 0x00,
0xE6, 0xFD, 0xD5, 0xFB, 0xD6, 0xF9, 0xF2, 0xF7, 0x31, 0xF6,
0x9B, 0xF4, 0x38, 0xF3, 0x0B, 0xF2, 0x1D, 0xF1, 0x70, 0xF0,
0x06, 0xF0, 0xE3, 0xEF, 0x06, 0xF0, 0x6F, 0xF0, 0x1D, 0xF1,
0x0B, 0xF2, 0x37, 0xF3, 0x9B, 0xF4, 0x31, 0xF6, 0xF1, 0xF7,
0xD5, 0xF9, 0xD5, 0xFB, 0xE6, 0xFD,
};

unsigned char sinetone_44[SINETONE_44_SIZE] = {
0x01, 0x00, 0x8B, 0x00, 0x13, 0x01, 0x94, 0x01, 0x0F, 0x02,
0x7E, 0x02, 0xE0, 0x02, 0x34, 0x03, 0x77, 0x03, 0xA7, 0x03,
0xC6, 0x03, 0xD0, 0x03, 0xC7, 0x03, 0xAA, 0x03, 0x79, 0x03,
0x38, 0x03, 0xE5, 0x02, 0x82, 0x02, 0x14, 0x02, 0x9B, 0x01,
0x19, 0x01, 0x92, 0x00, 0x07, 0x00, 0x7C, 0xFF, 0xF4, 0xFE,
0x72, 0xFE, 0xF7, 0xFD, 0x88, 0xFD, 0x24, 0xFD, 0xD0, 0xFC,
0x8C, 0xFC, 0x5A, 0xFC, 0x3C, 0xFC, 0x30, 0xFC, 0x39, 0xFC,
0x55, 0xFC, 0x83, 0xFC, 0xC5, 0xFC, 0x17, 0xFD, 0x78, 0xFD,
0xE5, 0xFD, 0x60, 0xFE, 0xDF, 0xFE, 0x67, 0xFF,
};

void timer_open(struct timer_list *timer, void *func)
{
	if (timer == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA]timer pointer is invalid\n");
		return;
	} else if (func == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA]timer callback function is invalid\n");
		return;
	}

	timer_setup(timer, func, 0);

	timer->expires = 0;
}

void timer_close(struct timer_list *timer)
{
	if (timer == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA]timer pointer is invalid\n");
		return;
	}

	del_timer_sync(timer);
	memset(timer, 0x00, sizeof(struct timer_list));
}

int timer_reset(struct timer_list *timer)
{
	if (timer == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA]timer pointer is invalid\n");
		return -EINVAL;
	} else if (timer->function == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA]timer callback function is invalid\n");
		return -EINVAL;
	}

	mod_timer(timer, (jiffies + msecs_to_jiffies(20)));

	return 0;
}

int timer_update(struct timer_list *timer,
				unsigned long time_interval)
{
	if (timer == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA]timer pointer is invalid\n");
		return -EINVAL;
	} else if (timer->function == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[ALSA]timer callback function is invalid\n");
		return -EINVAL;
	}

	mod_timer(timer, (jiffies + time_interval));

	return 0;
}

void timer_monitor(struct timer_list *t)
{
	struct mtk_runtime_struct *dev_runtime = NULL;
	struct mtk_spinlock_struct *spinlock = NULL;
	struct mtk_monitor_struct *monitor = NULL;
	struct snd_pcm_substream *substream[2];
	struct snd_pcm_runtime *runtime = NULL;
	char time_interval = 1;
	unsigned int expiration_counter = EXPIRATION_TIME;
	int i;

	dev_runtime = from_timer(dev_runtime, t, timer);

	if (dev_runtime == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR, "[ALSA]invalid structure!\n");
		return;
	}

	monitor = &dev_runtime->monitor;
	spinlock = &dev_runtime->spinlock;

	if (monitor->monitor_status == CMD_STOP) {
		apr_info(dev_runtime->log_level,
			"[ALSA]No action is required, exit Monitor\n");
		spin_lock(&spinlock->lock);
		memset(monitor, 0x00, sizeof(struct mtk_monitor_struct));
		spin_unlock(&spinlock->lock);
		return;
	}

	substream[E_PLAYBACK] = dev_runtime->substreams.p_substream;
	substream[E_CAPTURE] = dev_runtime->substreams.c_substream;

	for (i = 0; i < 2; i++) {
		if (substream[i] != NULL) {
			snd_pcm_period_elapsed(substream[i]);

			runtime = substream[i]->runtime;
			if (runtime != NULL) {
				/* If nothing to do, increase "expiration_counter" */
				if ((monitor->last_appl_ptr == runtime->control->appl_ptr) &&
					(monitor->last_hw_ptr == runtime->status->hw_ptr)) {
					monitor->expiration_counter++;

					if (monitor->expiration_counter >= expiration_counter) {
						apr_info(dev_runtime->log_level,
							"[ALSA]exit Monitor\n");
						snd_pcm_period_elapsed(substream[i]);
						spin_lock(&spinlock->lock);
						memset(monitor, 0x00,
							sizeof(struct mtk_monitor_struct));
						spin_unlock(&spinlock->lock);
						return;
					}
				} else {
					monitor->last_appl_ptr = runtime->control->appl_ptr;
					monitor->last_hw_ptr = runtime->status->hw_ptr;
					monitor->expiration_counter = 0;
				}
			}
		}
	}

	if (timer_update(&dev_runtime->timer,
			msecs_to_jiffies(time_interval)) != 0) {
		apr_err(dev_runtime->log_level,
			"[ALSA]fail to update timer for Monitor!\n");
		spin_lock(&spinlock->lock);
		memset(monitor, 0x00, sizeof(struct mtk_monitor_struct));
		spin_unlock(&spinlock->lock);
	}
}

int check_sysfs_null(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		apr_err(MTK_ALSA_LEVEL_ERROR,
			"[AUDIO][ERROR][%s]dev can't be NULL!!!\n", __func__);
		return -1;
	}

	if (attr == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]attr can't be NULL!!!\n", __func__);
		return -1;
	}

	if (buf == NULL) {
		adev_err(dev, MTK_ALSA_LEVEL_ERROR,
			"[%s]buf can't be NULL!!!\n", __func__);
		return -1;
	}

	return 0;
}

int pcm_fill_silence(struct snd_pcm_substream *substream, int channel,
				unsigned long pos, unsigned long bytes)
{
	return 0;
}
