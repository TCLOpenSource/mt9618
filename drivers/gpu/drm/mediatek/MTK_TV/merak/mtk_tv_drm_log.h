/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* * Copyright (c) 2020 MediaTek Inc. */

#ifndef _MTK_DRM_LOG_H_
#define _MTK_DRM_LOG_H_

#include <linux/module.h>
#include <linux/version.h>
#include <linux/jiffies.h>
#include <drm/drm_print.h>

/* How to use
 * 1. Include this header, ex: #include "mtk_tv_drm_log.h"
 * 2. Define this model type, ex #define MTK_DRM_MODEL MTK_DRM_MODEL_COMMON
 */

/* Redefine DRM_INFO to MTK version */
#ifdef DRM_INFO
#undef DRM_INFO
#endif

#define DRM_INFO(fmt, ...) do {					\
	if (mtk_drm_log_level & MTK_DRM_MODEL) {		\
		_DRM_PRINTK(, INFO, fmt, ##__VA_ARGS__);	\
	}							\
} while (0)

/* Redefine DRM_ERROR to MTK version */
#ifdef DRM_ERROR
#undef DRM_ERROR
#endif

#define DRM_ERROR_DELAY_SEC		(1)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
#define DRM_ERR_API			__drm_err
#else
#define DRM_ERR_API			drm_err
#endif
#define DRM_ERROR(fmt, ...) do {					\
	static unsigned long timeout;					\
	if (mtk_drm_log_level != 0 || time_after(jiffies, timeout)) {	\
		DRM_ERR_API(fmt, ##__VA_ARGS__);			\
		timeout = jiffies + (DRM_ERROR_DELAY_SEC * HZ);		\
	}								\
} while (0)

#define MTK_DRM_MODEL_COMMON		BIT(0)
#define MTK_DRM_MODEL_DISPLAY		BIT(1)
#define MTK_DRM_MODEL_PANEL		BIT(2)
#define MTK_DRM_MODEL_CONNECTOR		BIT(3)
#define MTK_DRM_MODEL_CRTC		BIT(4)
#define MTK_DRM_MODEL_DRV		BIT(5)
#define MTK_DRM_MODEL_ENCODER		BIT(6)
#define MTK_DRM_MODEL_FB		BIT(7)
#define MTK_DRM_MODEL_GEM		BIT(8)
#define MTK_DRM_MODEL_KMS		BIT(9)
#define MTK_DRM_MODEL_PATTERN		BIT(10)
#define MTK_DRM_MODEL_PLANE		BIT(11)
#define MTK_DRM_MODEL_VIDEO		BIT(12)
#define MTK_DRM_MODEL_GOP		BIT(13)
#define MTK_DRM_MODEL_DEMURA		BIT(14)
#define MTK_DRM_MODEL_PNLGAMMA		BIT(15)
#define MTK_DRM_MODEL_PQMAP		BIT(16)
#define MTK_DRM_MODEL_AUTOGEN		BIT(17)
#define MTK_DRM_MODEL_TVDAC		BIT(18)
#define MTK_DRM_MODEL_METABUF		BIT(19)
#define MTK_DRM_MODEL_FENCE		BIT(20)
#define MTK_DRM_MODEL_PQU_METADATA	BIT(21)
#define MTK_DRM_MODEL_OLED		BIT(22)
#define MTK_DRM_MODEL_LD		BIT(23)
#define MTK_DRM_MODEL_GLOBAL_PQ		BIT(24)
#define MTK_DRM_MODEL_SCRIPTMGT_ML	BIT(25)
#define MTK_DRM_MODEL_FRC		BIT(26)

extern uint32_t mtk_drm_log_level;

extern int boottime_print(const char *s);

#endif

