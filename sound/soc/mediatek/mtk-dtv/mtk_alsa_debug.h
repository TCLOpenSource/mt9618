/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk_alsa_debug.h  --  Mediatek debug header
 *
 * Copyright (c) 2020 MediaTek Inc.
 *
 */
#ifndef _DEBUG_PLATFORM_HEADER
#define _DEBUG_PLATFORM_HEADER

#define	SINETONE_SIZE	96
#define	SINETONE_44_SIZE	88

#define NONE			"\033[m"
#define RED			"\033[0;32;31m"
#define LIGHT_RED		"\033[1;31m"
#define GREEN			"\033[0;32;32m"
#define LIGHT_GREEN		"\033[1;32m"
#define BLUE			"\033[0;32;34m"
#define LIGHT_BLUE		"\033[1;34m"
#define DARY_GRAY		"\033[1;30m"
#define CYAN			"\033[0;36m"
#define LIGHT_CYAN		"\033[1;36m"
#define PURPLE			"\033[0;35m"
#define LIGHT_PURPLE		"\033[1;35m"
#define BROWN			"\033[0;33m"
#define YELLOW			"\033[1;33m"
#define LIGHT_GRAY		"\033[0;37m"
#define WHITE			"\033[1;37m"

#define AUDIO_DEBUG_MODE	0
#define AUDIO_CONSOLE_MODE	1

#if AUDIO_DEBUG_MODE
#define MTK_ALSA_ERROR_FMT	LIGHT_RED"[AUDIO ERROR]"NONE
#define MTK_ALSA_INFO_FMT	LIGHT_GREEN"[AUDIO INFO]"NONE
#define MTK_ALSA_DEBUG_FMT	YELLOW"[AUDIO INFO]"NONE
#define MTK_ALSA_LEVEL_ERROR	LOGLEVEL_ERR
#define MTK_ALSA_LEVEL_INFO	LOGLEVEL_INFO
#define MTK_ALSA_LEVEL_DEBUG	LOGLEVEL_DEBUG
#else
#define MTK_ALSA_ERROR_FMT
#define MTK_ALSA_INFO_FMT
#define MTK_ALSA_DEBUG_FMT
#define MTK_ALSA_LEVEL_ERROR	LOGLEVEL_DEBUG
#define MTK_ALSA_LEVEL_INFO	LOGLEVEL_DEBUG
#define MTK_ALSA_LEVEL_DEBUG	LOGLEVEL_DEBUG
#endif

#if AUDIO_CONSOLE_MODE
#define apr_emerg(log_level, fmt, ...)	do {if (log_level == LOGLEVEL_EMERG) \
					pr_emerg(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_alert(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ALERT) \
					pr_emerg(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_crit(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_CRIT)  \
					pr_emerg(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_err(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ERR)  \
					pr_emerg(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_warn(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_WARNING) \
					pr_emerg(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_notice(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_NOTICE) \
					pr_emerg(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_info(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_INFO) \
					pr_emerg(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_debug(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_DEBUG) \
					pr_emerg(MTK_ALSA_DEBUG_FMT fmt, ##__VA_ARGS__); \
					} while (0)

#define adev_emerg(dev, log_level, fmt, ...)	do {if (log_level == LOGLEVEL_EMERG) \
					dev_emerg(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_alert(dev, priv, fmt, ...)	do {if (log_level <= LOGLEVEL_ALERT) \
					dev_emerg(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_crit(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_CRIT) \
					dev_emerg(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_err(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ERR) \
					dev_emerg(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_warn(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_WARNING) \
					dev_emerg(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_notice(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_NOTICE) \
					dev_emerg(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_info(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_INFO) \
					dev_emerg(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_dbg(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_DEBUG) \
					dev_emerg(dev, "\n"MTK_ALSA_DEBUG_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#else
#define apr_emerg(log_level, fmt, ...)	do {if (log_level == LOGLEVEL_EMERG) \
					pr_err(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_alert(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ALERT) \
					pr_err(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_crit(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_CRIT)  \
					pr_err(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_err(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ERR)  \
					pr_err(MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_warn(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_WARNING) \
					pr_err(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_notice(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_NOTICE) \
					pr_err(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_info(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_INFO) \
					pr_err(MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define apr_debug(log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_DEBUG) \
					pr_err(MTK_ALSA_DEBUG_FMT fmt, ##__VA_ARGS__); \
					} while (0)

#define adev_emerg(dev, log_level, fmt, ...)	do {if (log_level == LOGLEVEL_EMERG) \
					dev_err(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_alert(dev, priv, fmt, ...)	do {if (log_level <= LOGLEVEL_ALERT) \
					dev_err(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_crit(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_CRIT) \
					dev_err(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_err(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_ERR) \
					dev_err(dev, "\n"MTK_ALSA_ERROR_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_warn(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_WARNING) \
					dev_err(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_notice(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_NOTICE) \
					dev_err(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_info(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_INFO) \
					dev_err(dev, "\n"MTK_ALSA_INFO_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#define adev_dbg(dev, log_level, fmt, ...)	do {if (log_level <= LOGLEVEL_DEBUG) \
					dev_err(dev, "\n"MTK_ALSA_DEBUG_FMT fmt, ##__VA_ARGS__); \
					} while (0)
#endif
#endif /* _ALSA_PLATFORM_HEADER */
