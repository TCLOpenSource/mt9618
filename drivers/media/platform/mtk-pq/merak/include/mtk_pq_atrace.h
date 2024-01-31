/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_ATRACE_H
#define _MTK_PQ_ATRACE_H

#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
#include <linux/mtk-tv-atrace.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TV_ATRACE)
#define PQ_ATRACE_STR_SIZE 64
#define PQ_ATRACE_ASYNC_BEGIN(name, cookie) \
	do {\
		if (atrace_enable_pq == 1) \
			atrace_async_begin(name, cookie);\
	} while (0)
#define PQ_ATRACE_ASYNC_END(name, cookie) \
	do {\
		if (atrace_enable_pq == 1) \
			atrace_async_end(name, cookie);\
	} while (0)
#define PQ_ATRACE_BEGIN(name) \
	do {\
		if (atrace_enable_pq == 1) \
			atrace_begin(name);\
	} while (0)
#define PQ_ATRACE_END(name) \
	do {\
		if (atrace_enable_pq == 1) \
			atrace_end(name);\
	} while (0)
#define PQ_ATRACE_INT(name, value) \
	do {\
		if (atrace_enable_pq == 1) \
			atrace_int(name, value);\
	} while (0)
#define PQ_ATRACE_INT_FORMAT(val, format, ...) \
	do {\
		if (atrace_enable_pq == 1) { \
			char m_str[PQ_ATRACE_STR_SIZE];\
			int len = snprintf(m_str, PQ_ATRACE_STR_SIZE, format, __VA_ARGS__);\
			if ((len >= 0) && ((unsigned int) len < PQ_ATRACE_STR_SIZE)) {\
				atrace_int(m_str, val);\
			} \
		} \
	} while (0)

#else
#define PQ_ATRACE_ASYNC_BEGIN(name, cookie)
#define PQ_ATRACE_ASYNC_END(name, cookie)
#define PQ_ATRACE_BEGIN(name)
#define PQ_ATRACE_END(name)
#define PQ_ATRACE_INT(name, value)
#define PQ_ATRACE_INT_FORMAT(val, format, ...)
#endif

#endif  //#ifndef _MTK_PQ_ATRACE_H
