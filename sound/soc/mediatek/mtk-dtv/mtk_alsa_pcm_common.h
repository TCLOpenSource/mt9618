/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_pcm_common.h  --  Mediatek pcm common header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _ALSA_PLATFORM_HEADER
#define _ALSA_PLATFORM_HEADER

#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>
#include <sound/soc.h>

#include "mtk_alsa_debug.h"

#define EXPIRATION_TIME	500
#define DEFAULT_TIMESTAMP_SIZE	1024

/* channel mode */
#define CH_2		2
#define CH_6		6
#define CH_8		8
#define CH_12		12

#define RATE_8000	8000
#define RATE_11025	11025
#define RATE_12000	12000
#define RATE_16000	16000
#define RATE_22050	22050
#define RATE_24000	24000
#define RATE_32000	32000
#define RATE_44100	44100
#define RATE_48000	48000
#define RATE_64000	64000
#define RATE_88200	88200
#define RATE_96000	96000
#define RATE_176400	176400
#define RATE_192000	192000
#define RATE_768000	768000

#define AU_BIT0		(1<<0)
#define AU_BIT1		(1<<1)
#define AU_BIT2		(1<<2)
#define AU_BIT3		(1<<3)
#define AU_BIT4		(1<<4)
#define AU_BIT5		(1<<5)
#define AU_BIT6		(1<<6)
#define AU_BIT7		(1<<7)
#define AU_BIT8		(1<<8)
#define AU_BIT9		(1<<9)
#define AU_BIT10	(1<<10)
#define AU_BIT11	(1<<11)
#define AU_BIT12	(1<<12)
#define AU_BIT13	(1<<13)
#define AU_BIT14	(1<<14)
#define AU_BIT15	(1<<15)

#define CAP_CHANNEL_MASK	(AU_BIT6 | AU_BIT7)
#define CAP_CHANNEL_2		0
#define CAP_CHANNEL_8		AU_BIT6
#define CAP_CHANNEL_12		AU_BIT7

#define CAP_FORMAT_MASK		(AU_BIT4 | AU_BIT5)
#define CAP_FORMAT_16BIT	0
#define CAP_FORMAT_24BIT_3LE	AU_BIT4
#define CAP_FORMAT_24BIT	(AU_BIT4 | AU_BIT5)

#define	BYTES_IN_MIU_LINE	16
#define	BYTES_IN_MIU_LINE_LOG2	4
#define	ICACHE_MEMOFFSET_LOG2	12

#define BITS_IN_BYTE		8

#define AUDIO_DO_ALIGNMENT(val, alignment_size)	(val = (val / alignment_size) * alignment_size)

enum {
	E_PLAYBACK,
	E_CAPTURE,
};

enum {
	CMD_STOP = 0,
	CMD_START,
	CMD_PAUSE,
	CMD_PAUSE_RELEASE,
	CMD_PREPARE,
	CMD_SUSPEND,
	CMD_RESUME,
};

struct mem_info {
	u32 miu;
	u64 bus_addr;
	u64 buffer_size;
	u64 memory_bus_base;
};

/* define a STR (suspend to ram) data structure */
struct mtk_str_mode_struct {
	ptrdiff_t physical_addr;
	ptrdiff_t bus_addr;
	ptrdiff_t virtual_addr;
};

/* define a monitor data structure */
struct mtk_monitor_struct {
	unsigned int monitor_status;
	unsigned int expiration_counter;
	snd_pcm_uframes_t last_appl_ptr;
	snd_pcm_uframes_t last_hw_ptr;
};

/* define a substream data structure */
struct mtk_substream_struct {
	struct snd_pcm_substream *p_substream;
	struct snd_pcm_substream *c_substream;
	unsigned int substream_status;
};

struct mtk_snd_timestamp_info {
	unsigned long long byte_count;
	struct timespec kts;
	unsigned long hts;
};

struct mtk_snd_timestamp {
	struct mtk_snd_timestamp_info ts_info[512];
	unsigned int wr_index;
	unsigned int rd_index;
	unsigned int ts_count;
	unsigned long long total_size;
};

/* define a ring buffer structure */
struct mtk_ring_buffer_struct {
	unsigned char *addr;
	unsigned int size;
	unsigned char *w_ptr;
	unsigned char *r_ptr;
	unsigned char *buf_end;
	unsigned int avail_size;
	unsigned int remain_size;
	unsigned long long total_read_size;
	unsigned long long total_write_size;
};

/* define a segment buffer structure */
struct mtk_copy_buffer_struct {
	unsigned char *addr;
	unsigned int size;
};

struct mtk_inject_struct {
	bool status;
	unsigned int r_ptr;
};

struct mtk_spinlock_struct {
	unsigned int init_status;
	spinlock_t lock;
};

/* define a DMA data structure */
struct mtk_dma_struct {
	struct mtk_ring_buffer_struct r_buf;
	struct mtk_copy_buffer_struct c_buf;
	struct mtk_str_mode_struct str_mode_info;
	unsigned int initialized_status;
	unsigned int high_threshold;
	unsigned int copy_size;
	unsigned int status;
	unsigned int channel_mode;
	unsigned int sample_rate;
	unsigned int sample_bits;
	unsigned int byte_length;
	snd_pcm_format_t format;
};

/* define a runtime data structure */
struct mtk_runtime_struct {
	struct mtk_substream_struct substreams;
	struct mtk_dma_struct playback;
	struct mtk_dma_struct capture;
	struct mtk_inject_struct inject;
	struct mtk_snd_timestamp timestamp;
	struct mtk_monitor_struct monitor;
	struct mtk_spinlock_struct spinlock;
	struct timer_list timer;
	unsigned int log_level;
	unsigned int clk_status;
	unsigned int charged_finished;
	unsigned int suspend_flag;
	unsigned int sink_dev;
	int device_cycle_count;
};

void timer_open(struct timer_list *timer, void *func);
void timer_close(struct timer_list *timer);
int timer_reset(struct timer_list *timer);
void timer_monitor(struct timer_list *t);
int check_sysfs_null(struct device *dev, struct device_attribute *attr, char *buf);
int pcm_fill_silence(struct snd_pcm_substream *substream, int channel,
				unsigned long pos, unsigned long bytes);

#endif /* _ALSA_PLATFORM_HEADER */
