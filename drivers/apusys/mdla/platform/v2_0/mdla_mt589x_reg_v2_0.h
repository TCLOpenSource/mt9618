/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MDLA_MT589X_REG_H__
#define __MDLA_MT589X_REG_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>

#define APMCU_RIUBASE 0x1C000000
#define RIU_ADDR(bank, offset)		(APMCU_RIUBASE + ((bank<<8)<<1) + (offset<<2))
#define REG_OFFSET_SHIFT_BITS		2

#define CKGEN00	0x1020
#define X32_AIA_TOP 0x2901
#define TZPC_AID_0 0x1203
#define TZPC_AID_1 0x1204
#define IOMMU_LOCAL_SSC3 0x153b
#define IOMMU_LOCAL_SSC4 0x153c
#define IOMMU_GLOBAL 0x120e
#define MCUSYS_XIU_BRIDGE 0x2009
#define X32_APUSYS (0x2800)

#define X32_APUSYS_BASE (0x2800<<9)

#define FORCE_RESET	0
#define COLD_RESET	1
#define NORMAL_RESET	2

#endif
