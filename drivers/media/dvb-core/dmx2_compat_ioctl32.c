// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define file_name "dmx2_compat_ioctl32"
#define pr_fmt(fmt) file_name ": " fmt
#define debug_flag dmx2_compat_ioctl32_debug

#include <linux/module.h>
#include <media/dvb_demux2.h>
#include <media/dvb_ctrls.h>
#include <media/dmx2dev.h>

#include "dmx2_compat_ioctl32.h"

#define dprintk(fmt, arg...) do { \
	if (debug_flag)	\
		pr_debug(pr_fmt("%s: " fmt), \
		       __func__, ##arg); \
} while (0)

static int debug_flag;
module_param(debug_flag, int, 0644);
MODULE_PARM_DESC(debug_flag, "Turn on/off " file_name " debugging (default:off).");

#ifdef CONFIG_COMPAT
/**
 * do_dvb_demux2_compat_ioctl()
 *     Ancillary function handles a compat32 ioctl call
 *
 * @file: pointer to &struct file with the file handler
 * @cmd: ioctl to be called
 * @arg: arguments passed from/to the ioctl handler
 *
 * This function is called when a 32 bits application calls a dvb demux2 ioctl
 * and the Kernel is compiled with 64 bits.
 *
 * It converts a 32-bits struct into a 64 bits one, calls the native 64-bits
 * ioctl handler and fills back the 32-bits struct with the results of the
 * native call.
 */
long do_dvb_demux2_compat_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	unsigned int ncmd;
	struct dmx2_controls32 __user *ucontrol = compat_ptr(arg);
	struct dmx2_control32 __user *ucontrols;
	struct dmx2_controls ctrl = { 0 };
	struct dmx2_control *temp, *base = NULL;
	compat_caddr_t ptr = 0;
	u32 i, count;
	long err = 0;
	struct dvb_fh *fh = file->private_data;
	struct dmx2dev *dmx2dev = fh_to_dmx2dev(fh);

	switch (cmd) {
	case DMX2_SET_CTRLS32:
	case DMX2_GET_CTRLS32:
		ncmd = (cmd == DMX2_SET_CTRLS32) ? DMX2_SET_CTRLS : DMX2_GET_CTRLS;

		if (!access_ok(ucontrol, sizeof(*ucontrol)) ||
			get_user(count, &ucontrol->count) ||
			get_user(ptr, &ucontrol->controls))
			return -EFAULT;

		base = temp = kzalloc(count * sizeof(struct dmx2_control), GFP_KERNEL);
		if (!base)
			return -ENOMEM;

		ctrl.count = count;
		ctrl.controls = base;
		ucontrols = compat_ptr(ptr);
		for (i = 0; i < count; i++, temp++)
			if (copy_from_user(temp, (ucontrols + i), sizeof(struct dmx2_control))) {
				err = -EFAULT;
				goto out;
			}

		break;
	default:
		ncmd = cmd;
		break;
	}

	err = dmx2dev->dmx_fops->compat_ioctl(file, ncmd, (unsigned long)(&ctrl));

	switch (cmd) {
	case DMX2_SET_CTRLS32:
	case DMX2_GET_CTRLS32:
		if (put_user(ctrl.error_idx, &ucontrol->error_idx)) {
			err = -EFAULT;
			goto out;
		}
		break;
	default:
		break;
	}
out:
	kfree(base);
	return err;
}

/**
 * do_dvb_dvr2_compat_ioctl()
 *     Ancillary function handles a compat32 ioctl call
 *
 * @file: pointer to &struct file with the file handler
 * @cmd: ioctl to be called
 * @arg: arguments passed from/to the ioctl handler
 *
 * This function is called when a 32 bits application calls a dvb dvr2 ioctl
 * and the Kernel is compiled with 64 bits.
 *
 * It converts a 32-bits struct into a 64 bits one, calls the native 64-bits
 * ioctl handler and fills back the 32-bits struct with the results of the
 * native call.
 */
long do_dvb_dvr2_compat_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	unsigned int ncmd;
	struct dmx2_controls32 __user *ucontrol = compat_ptr(arg);
	struct dmx2_control32 __user *ucontrols;
	struct dmx2_controls ctrl = { 0 };
	struct dmx2_control *temp, *base = NULL;
	compat_caddr_t ptr = 0;
	u32 i, count;
	long err = 0;
	struct dvb_fh *fh = file->private_data;
	struct dmx2dev *dmx2dev = fh_to_dmx2dev(fh);

	switch (cmd) {
	case DMX2_SET_CTRLS32:
	case DMX2_GET_CTRLS32:
		ncmd = (cmd == DMX2_SET_CTRLS32) ? DMX2_SET_CTRLS : DMX2_GET_CTRLS;

		if (!access_ok(ucontrol, sizeof(*ucontrol)) ||
			get_user(count, &ucontrol->count) ||
			get_user(ptr, &ucontrol->controls))
			return -EFAULT;

		base = temp = kzalloc(count * sizeof(struct dmx2_control), GFP_KERNEL);
		if (!base)
			return -ENOMEM;

		ctrl.count = count;
		ctrl.controls = base;
		ucontrols = compat_ptr(ptr);
		for (i = 0; i < count; i++, temp++)
			if (copy_from_user(temp, (ucontrols + i), sizeof(struct dmx2_control))) {
				err = -EFAULT;
				goto out;
			}

		break;
	default:
		ncmd = cmd;
		break;
	}

	err = dmx2dev->dvr_fops->compat_ioctl(file, ncmd, (unsigned long)(&ctrl));

	switch (cmd) {
	case DMX2_SET_CTRLS32:
	case DMX2_GET_CTRLS32:
		if (put_user(ctrl.error_idx, &ucontrol->error_idx)) {
			err = -EFAULT;
			goto out;
		}
		break;
	default:
		break;
	}
out:
	kfree(base);
	return err;
}

#endif //#ifdef CONFIG_COMPAT
