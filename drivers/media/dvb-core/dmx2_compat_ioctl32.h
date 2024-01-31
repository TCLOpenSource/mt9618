/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef DMX2_COMPAT_IOCTL32_H
#define DMX2_COMPAT_IOCTL32_H

#include <stdbool.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/compat.h>
#include <linux/fs.h>

#ifdef CONFIG_COMPAT

#define DMX2_CID_MAX_CTRLS	1024

struct dmx2_controls32 {
	__u32 count;
	__u32 error_idx;
	__u32 reserved[2];
	compat_caddr_t controls;
};

struct dmx2_control32 {
	__u32 id;
	__u32 size;
	union {
		__s32 value;
		__s64 value64;
		compat_caddr_t string;
	};
} __packed;

/*
 * List of ioctls that require 32-bits/64-bits conversion
 *
 * The V4L2 ioctls that aren't listed there don't have pointer arguments
 * and the struct size is identical for both 32 and 64 bits versions, so
 * they don't need translations.
 */
#define DMX2_SET_CTRLS32	_IOWR('o', 78, struct dmx2_controls32)
#define DMX2_GET_CTRLS32	_IOWR('o', 79, struct dmx2_controls32)

long do_dvb_demux2_compat_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg);

long do_dvb_dvr2_compat_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg);

#endif //#ifdef CONFIG_COMPAT

#endif /* DMX2_COMPAT_IOCTL32_H */
