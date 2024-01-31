// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mtk_tv_drm_atrace.h"
#include "mtk_tv_drm_kms.h"

void mtk_drm_atrace_begin(const char *name)
{
	if (gMtkDrmDbg.atrace_enable)
		atrace_begin(name);
}

void mtk_drm_atrace_end(const char *name)
{
	if (gMtkDrmDbg.atrace_enable)
		atrace_end(name);
}

void mtk_drm_atrace_async_begin(const char *name, int32_t cookie)
{
	if (gMtkDrmDbg.atrace_enable)
		atrace_async_begin(name, cookie);
}

void mtk_drm_atrace_async_end(const char *name, int32_t cookie)
{
	if (gMtkDrmDbg.atrace_enable)
		atrace_async_end(name, cookie);
}

void mtk_drm_atrace_int(const char *name, int32_t value)
{
	if (gMtkDrmDbg.atrace_enable)
		atrace_int(name, value);
}

