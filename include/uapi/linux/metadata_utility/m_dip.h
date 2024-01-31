/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __UAPI_MTK_M_DIP_H
#define __UAPI_MTK_M_DIP_H

#include <linux/types.h>
#ifndef __KERNEL__
#include <stdint.h>
#include <stdbool.h>
#endif

#ifndef E_COMPONENT12TAG_START
#define E_COMPONENT12_TAG_START 0x000c0000
enum EN_COMPONENT12_TAG { // range 0x00030000 ~ 0x0003FFFF
	M_DIP_IN_FRAME_INFO_META_TAG = E_COMPONENT12_TAG_START,
	E_DIP_COMPONENT12_TAG_MAX,
};
#endif

#define MAX_PLANE (5)

enum EN_M_DIP_COLOR_RANGE {
	E_M_DIP_COLOR_RANGE_LIMIT,
	E_M_DIP_COLOR_RANGE_FULL,
	E_M_DIP_COLOR_RANGE_MAX,
};

enum EN_M_DIP_COLOR_SPACE {
	E_M_DIP_COLOR_SPACE_BT601,
	E_M_DIP_COLOR_SPACE_BT709,
	E_M_DIP_COLOR_SPACE_BT2020,
	E_M_DIP_COLOR_SPACE_RESERVED_START,
};

struct meta_dip_frame_info {
	__u32 u32Width;
	__u32 u32Height;
	__u32 u32Bytesperline;
	__u32 u32Fourcc;
	__u32 u32Size[MAX_PLANE];
	__u64 u64Addr[MAX_PLANE];
	__u16 u16Rotation;
	enum EN_M_DIP_COLOR_RANGE eColorRange;
	enum EN_M_DIP_COLOR_SPACE eColorSpace;
};
#endif
