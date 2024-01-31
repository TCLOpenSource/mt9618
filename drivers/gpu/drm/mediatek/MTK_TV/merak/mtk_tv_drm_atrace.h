/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* * Copyright (c) 2020 MediaTek Inc. */

#ifndef _MTK_DRM_ATRACE_H_
#define _MTK_DRM_ATRACE_H_

#include <linux/mtk-tv-atrace.h>

void mtk_drm_atrace_begin(const char *name);
void mtk_drm_atrace_end(const char *name);
void mtk_drm_atrace_async_begin(const char *name, int32_t cookie);
void mtk_drm_atrace_async_end(const char *name, int32_t cookie);
void mtk_drm_atrace_int(const char *name, int32_t value);

#endif
