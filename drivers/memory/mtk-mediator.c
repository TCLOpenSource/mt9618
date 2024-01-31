// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/time64.h>
#include <linux/pm_runtime.h>
#include <asm-generic/div64.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include "mtk-dramc.h"
#include "mtk-mediator.h"

#define MDMEDIATOR_IOC_MAGIC  'M'
#define MDMEDIATOR_SET_DRAM_FREQ	_IOWR(MDMEDIATOR_IOC_MAGIC, 0, int)
#define MDMEDIATOR_CHANGE_TOTAL_BW	_IOWR(MDMEDIATOR_IOC_MAGIC, 1, int)
#define MDMEDIATOR_SET_SCENARIO		_IOWR(MDMEDIATOR_IOC_MAGIC, 2, struct mtk_mediator_scenario)

#define DRAM4266	(4266)
#define DRAM3200	(3200)
#define MAX_WIN_NUM	(16)
#define RES_4K2K	(3840*2160)

static u32 bw_est_total;
static u32 bw_high_est_total;
static u32 bw_low_est_total;
static u32 bw_est_cpu;
static u32 bw_est_gpu;
static u32 bw_est_gop;
static u32 bw_est_dip;
static u32 dynamic_dram_freq;
static u32 dram_freq;
struct miscdevice misc_dev;

enum mtk_source_type {
	SOURCE_TYPE_HDMI,
	SOURCE_TYPE_VGA,
	SOURCE_TYPE_ATV,
	SOURCE_TYPE_YPBPR,
	SOURCE_TYPE_CVBS,
	SOURCE_TYPE_DTV,
	SOURCE_TYPE_MM,
	SOURCE_TYPE_PHOTO,
	SOURCE_TYPE_CAMERA,
	SOURCE_TYPE_NUM,
};

enum mtk_output_fmt {
	OUTPUT_FORMAT_ARGB8,
	OUTPUT_FORMAT_ABGR8,
	OUTPUT_FORMAT_RGBA8,
	OUTPUT_FORMAT_BGRA8,
	OUTPUT_FORMAT_RGB565,
	OUTPUT_FORMAT_ARGB10,
	OUTPUT_FORMAT_ABGR10,
	OUTPUT_FORMAT_RGBA10,
	OUTPUT_FORMAT_BGRA10,
	OUTPUT_FORMAT_YUV444_10,
	OUTPUT_FORMAT_YUV444_8,
	OUTPUT_FORMAT_YUV444_8_COMPRESS,
	OUTPUT_FORMAT_YUV422_10,
	OUTPUT_FORMAT_YUV422_8_1PLN,
	OUTPUT_FORMAT_YUV422_8_2PLN,
	OUTPUT_FORMAT_YUV422_8_COMPRESS,
	OUTPUT_FORMAT_YUV422_6_COMPRESS,
	OUTPUT_FORMAT_YUV420_10,
	OUTPUT_FORMAT_YUV420_8,
	OUTPUT_FORMAT_YUV420_8_COMPRESS,
	OUTPUT_FORMAT_YUV420_6_COMPRESS,
	OUTPUT_FORMAT_MOTION_RGB_444_6,
	OUTPUT_FORMAT_MOTION_YUV_444_6,
	OUTPUT_FORMAT_MOTION_YUV_422_6,
	OUTPUT_FORMAT_MOTION_YUV_420_6,
	OUTPUT_FORMAT_MOTION_RGB_444_8,
	OUTPUT_FORMAT_MOTION_YUV_444_8,
	OUTPUT_FORMAT_MOTION_YUV_422_8,
	OUTPUT_FORMAT_MOTION_YUV_420_8,
	OUTPUT_FORMAT_NUM
};

enum mtk_pq_level {
	PQ_LEVEL_BEST,
	PQ_LEVEL_FULL,
	PQ_LEVEL_NORMAL,
	PQ_LEVEL_LITE,
	PQ_LEVEL_ZERO,
	PQ_LEVEL_MAX,
};

struct mtk_stream_info {
	enum mtk_source_type	src_type;
	enum mtk_output_fmt	render_fmt;
	enum mtk_pq_level	pq_level;
	unsigned int		src_width;
	unsigned int		src_height;
	unsigned int		src_fps;
	unsigned int		render_width;
	unsigned int		render_height;
	unsigned int		render_fps;
};

struct mtk_dram_info {
	unsigned int dram_ch_num;
	unsigned int *dram_size;
	bool balance;
};

struct mtk_mediator_scenario {
	struct mtk_stream_info	stream[MAX_WIN_NUM];
	unsigned int		win_num;
};

static void putbuf(char *buf, u32 *len, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	*len += vscnprintf(buf+(*len), PAGE_SIZE-(*len), fmt, args);
	va_end(args);
}

/* Mediator Cpu Freq Adapter callback*/
int (*mediator_cfa_decode)(unsigned int, bool) = NULL;
int (*mediator_cfa_non_decode)(unsigned int, bool) = NULL;

/* Mediator Cpu Freq Adapter register function*/
void register_cpu_freq_adapter_cb(enum mediator_cb_event cb_event,
				  int (*cpu_freq_adapter)(unsigned int, bool))
{
	if (cb_event == CPU_FREQ_ADAPTER_DECODER)
		mediator_cfa_decode = cpu_freq_adapter;
	else if (cb_event == CPU_FREQ_ADAPTER_NON_DECODER)
		mediator_cfa_non_decode = cpu_freq_adapter;
	else
		pr_err("[MTK_MEDIATOR] %s fail\n", __func__);
}
EXPORT_SYMBOL(register_cpu_freq_adapter_cb);

static int mtk_mediator_setup_scenario(struct mtk_mediator_scenario *scen)
{
	int i;
	unsigned int decode_win_num = 0;
	unsigned int non_decode_win_num = 0;
	bool decode_4k = false;
	bool non_decode_4k = false;

//	pr_info("[MTK_MEDIATOR] Set Up Scenario\n");
//	pr_info("[MTK_MEDIATOR] total win number: %d\n\n", scen->win_num);
	for (i = 0; i < scen->win_num; i++) {
//		pr_info("[MTK_MEDIATOR] stream : %d\n", i);
//		pr_info("[MTK_MEDIATOR] src type : %d\n", scen->stream[i].src_type);
//		pr_info("[MTK_MEDIATOR] src width : %u\n", scen->stream[i].src_width);
//		pr_info("[MTK_MEDIATOR] src height : %u\n", scen->stream[i].src_height);
//		pr_info("[MTK_MEDIATOR] src fps : %u\n", scen->stream[i].src_fps);
//		pr_info("[MTK_MEDIATOR] render fmt : %d\n", scen->stream[i].render_fmt);
//		pr_info("[MTK_MEDIATOR] render width : %u\n", scen->stream[i].render_width);
//		pr_info("[MTK_MEDIATOR] render heigth : %u\n", scen->stream[i].render_height);
//		pr_info("[MTK_MEDIATOR] render fps : %u\n", scen->stream[i].render_fps);
//		pr_info("[MTK_MEDIATOR] pq level : %d\n", scen->stream[i].pq_level);
//		pr_info("[MTK_MEDIATOR]\n");

		if (scen->stream[i].src_type == SOURCE_TYPE_DTV ||
		    scen->stream[i].src_type == SOURCE_TYPE_MM) {
			decode_win_num += 1;
			if (scen->stream[i].src_width * scen->stream[i].src_height == RES_4K2K)
				decode_4k = true;
		} else {
			non_decode_win_num += 1;
			if (scen->stream[i].src_width * scen->stream[i].src_height == RES_4K2K)
				non_decode_4k = true;
		}
	}
	if (mediator_cfa_decode) {
//		pr_info("[MTK_MEDIATOR] decode_win_num: %u\n", decode_win_num);
//		pr_info("[MTK_MEDIATOR] has decode 4K: %u\n", decode_4k);
		mediator_cfa_decode(decode_win_num, decode_4k);
	} else {
//		pr_info("[MTK_MEDIATOR] do not register mediator_cfa_decode");
	}

	if (mediator_cfa_non_decode) {
//		pr_info("[MTK_MEDIATOR] non_decode_win_num: %u\n", non_decode_win_num);
//		pr_info("[MTK_MEDIATOR] has non_decode 4K: %u\n", non_decode_4k);
		mediator_cfa_non_decode(non_decode_win_num, non_decode_4k);
	} else {
//		pr_info("[MTK_MEDIATOR] do not register mediator_cfa_non_decode");
	}

	return 0;
}

static int mtk_mediator_setup_est_dram_bw(void)
{
	struct device_node *np = NULL;
	struct mtk_dram_info dram_info;
	int i;


	dram_info.dram_ch_num = mtk_dramc_get_support_channel_num();
	dram_info.dram_size = kmalloc_array(dram_info.dram_ch_num, sizeof(u32), GFP_KERNEL);
	if (!dram_info.dram_size)
		return -1;
	dram_info.balance = true;
	pr_info("[MTK_MEDIATOR] dram channel num: %u\n", dram_info.dram_ch_num);

	for (i = 0; i < dram_info.dram_ch_num; i++) {
		dram_info.dram_size[i] = mtk_dramc_get_channel_size(i);
		pr_info("[MTK_MEDIATOR] Ch %u dram_size: %u (Gb)\n"
				, i, dram_info.dram_size[i]);
		if (i > 0)
			if (dram_info.dram_size[i] != dram_info.dram_size[i-1])
				dram_info.balance = false;
	}
	if (dram_info.balance) {
		pr_info("[MTK_MEDIATOR] balance mode\n");

		np = of_find_node_by_name(NULL, "dram_bw_est_balance");
		if (!np)
			goto SETUP_BW_FAIL;
	} else {
		pr_info("[MTK_MEDIATOR] unbalance mode\n");

		np = of_find_node_by_name(NULL, "dram_bw_est_unbalance");
		if (!np)
			goto SETUP_BW_FAIL;
	}
	if (of_property_read_u32(np, "est_bw_total", &bw_est_total) < 0)
		goto SETUP_BW_FAIL;

	if (of_property_read_u32(np, "est_bw_cpu", &bw_est_cpu) < 0)
		goto SETUP_BW_FAIL;

	if (of_property_read_u32(np, "est_bw_gpu", &bw_est_gpu) < 0)
		goto SETUP_BW_FAIL;

	if (of_property_read_u32(np, "est_bw_gop", &bw_est_gop) < 0)
		goto SETUP_BW_FAIL;

	if (of_property_read_u32(np, "est_bw_dip", &bw_est_dip) < 0)
		goto SETUP_BW_FAIL;

	if (dynamic_dram_freq) {
		if (of_property_read_u32(np, "est_high_bw_total", &bw_high_est_total) < 0)
			goto SETUP_BW_FAIL;

		if (of_property_read_u32(np, "est_low_bw_total", &bw_low_est_total) < 0)
			goto SETUP_BW_FAIL;
	}
	kfree(dram_info.dram_size);

	return 0;

SETUP_BW_FAIL:
	kfree(dram_info.dram_size);

	return -1;
}

static int mtk_mediator_dynamic_dram_freq(void)
{
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "dram_config");
	if (!np)
		return -1;

	if (of_property_read_bool(np, "dynamic_dram_freq"))
		dynamic_dram_freq = true;
	else
		dynamic_dram_freq = false;

	return 0;
}

static ssize_t est_total_bw_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", bw_est_total);

	return len;
}

static ssize_t est_cpu_bw_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", bw_est_cpu);

	return len;
}

static ssize_t est_gpu_bw_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", bw_est_gpu);

	return len;
}

static ssize_t est_gop_bw_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", bw_est_gop);

	return len;
}

static ssize_t est_dip_bw_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", bw_est_dip);

	return len;
}

static ssize_t dynamic_dram_freq_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	putbuf(buf, &len, "%u\n", dynamic_dram_freq);

	return len;
}

static ssize_t dram_freq_show(struct device_driver *drv, char *buf)
{
	u32 len = 0;

	dram_freq = mtk_dramc_get_dfs_freq();

	putbuf(buf, &len, "%u\n", dram_freq);

	return len;
}

static DRIVER_ATTR_RO(est_total_bw);
static DRIVER_ATTR_RO(est_cpu_bw);
static DRIVER_ATTR_RO(est_gpu_bw);
static DRIVER_ATTR_RO(est_gop_bw);
static DRIVER_ATTR_RO(est_dip_bw);
static DRIVER_ATTR_RO(dynamic_dram_freq);
static DRIVER_ATTR_RO(dram_freq);

static struct platform_driver mtk_mediator_driver = {
	.driver	= {
		.name = "mtk-mediator",
	}
};

static int create_sysfs_node(void)
{
	struct device_driver *drv;
	int err = 0;

	drv = driver_find("mtk-mediator", &platform_bus_type);

	err = driver_create_file(drv, &driver_attr_est_total_bw);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_est_total_bw error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_est_cpu_bw);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_est_cpu_bw error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_est_gpu_bw);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_est_gpu_bw attribute error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_est_gop_bw);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_est_gop_bw error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_est_dip_bw);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_est_dip_bw error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dynamic_dram_freq);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_dynamic_dram_freq error\n");
		return err;
	}
	err = driver_create_file(drv, &driver_attr_dram_freq);
	if (err) {
		pr_err("[MTK_MEDIATOR] create driver_attr_dram_freq error\n");
		return err;
	}

	return err;
}

static long mdmediator_userdev_ioctl(struct file *file,
				     unsigned int cmd,
				     unsigned long arg)
{
	int ret = 0;
	int dir = _IOC_DIR(cmd);
	union ioctl_data {
		unsigned int ioctl_dram_freq;
		struct mtk_mediator_scenario ioctl_scen;
	} data = { };

	if (arg == 0)
		return -EINVAL;
	if (dir & _IOC_WRITE) {
		if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto exit;
		}
	}
	switch (cmd) {
	case MDMEDIATOR_SET_DRAM_FREQ:
		dram_freq = data.ioctl_dram_freq;
		pr_info("[MTK_MEDIATOR] set DRAM Freq : %u\n", dram_freq);
		ret = mtk_dramc_set_dfs_freq(dram_freq);
		if (!ret)
			pr_info("[MTK_MEDIATOR] fail to set dram_freq");
		break;
	case MDMEDIATOR_CHANGE_TOTAL_BW:
		dram_freq = data.ioctl_dram_freq;
		pr_info("[MTK_MEDIATOR] change total bw by DRAM freq : %u\n", dram_freq);
		if (dram_freq == DRAM4266)
			bw_est_total = bw_high_est_total;
		else if (dram_freq == DRAM3200)
			bw_est_total = bw_low_est_total;
		else
			pr_info("[MTK_MEDIATOR] target DRAM freq not support %u\n", dram_freq);
		break;
	case MDMEDIATOR_SET_SCENARIO:
		ret = mtk_mediator_setup_scenario(&data.ioctl_scen);
		if (ret)
			pr_info("[MTK_MEDIATOR] mtk_mediator_setup_scenario fail");
		break;
	default:
		ret = -ENOTTY;
		goto exit;
	}
exit:
	return ret;
}

static int mdmediator_userdev_open(struct inode *inode, struct file *file)
{
	/* do nothing */
	return 0;
}

static int mdmediator_userdev_release(struct inode *inode, struct file *file)
{
	/* do nothing */
	return 0;
}

static const struct file_operations mdmediator_userdev_fops = {
	.owner = THIS_MODULE,
	.open = mdmediator_userdev_open,
	.release = mdmediator_userdev_release,
	.unlocked_ioctl = mdmediator_userdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdmediator_userdev_ioctl,
#endif
};

static int register_misc_dev(void)
{
	misc_dev.minor = MISC_DYNAMIC_MINOR;
	misc_dev.name = "mediator_mtkd";
	misc_dev.fops = &mdmediator_userdev_fops;

	return misc_register(&misc_dev);
}

static int __init mtk_mediator_init(void)
{
	int ret;

	pr_info("[MTK_MEDIATOR] Init\n");

	ret = platform_driver_register(&mtk_mediator_driver);
	if (ret != 0) {
		pr_err("[MTK_MEDIATOR] Failed to register mtk_mediator driver\n");
		return ret;
	}
	ret = mtk_mediator_dynamic_dram_freq();
	if (ret) {
		pr_err("[MTK_MEDIATOR]Failed to init mtk_mediator_dynamic_dram_freq\n");
		return ret;
	}
	/* setup dram estimated bandwidth */
	ret = mtk_mediator_setup_est_dram_bw();
	if (ret) {
		pr_err("[MTK_MEDIATOR]Failed to init mtk_mediator_setup_est_dram_bw\n");
		return ret;
	}
	ret = create_sysfs_node();
	if (ret) {
		pr_err("[MTK_MEDIATOR] Failed to create_sysfs_node\n");
		return ret;
	}
	ret = register_misc_dev();
	if (ret) {
		pr_err("[MTK_MEDIATOR] register msic dev fail\n");
		return ret;
	}

	return ret;
}

static void __exit mtk_mediator_exit(void)
{
	pr_info("[MTK_MEDIATOR] exit\n");
	platform_driver_unregister(&mtk_mediator_driver);
}

module_init(mtk_mediator_init);
module_exit(mtk_mediator_exit);

MODULE_LICENSE("GPL");
