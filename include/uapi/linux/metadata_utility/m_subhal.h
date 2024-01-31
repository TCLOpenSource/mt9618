/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 * Copyright (c) 2020-2021 MediaTek Inc.
 */

#ifndef __UAPI_MTK_M_SBUHAL_H
#define __UAPI_MTK_M_SBUHAL_H

#include <linux/types.h>
#ifndef __KERNEL__
#include <stdint.h>
#include <stdbool.h>
#endif

/* ============================================================== */
/* ------------------------ Metadata Tag ------------------------ */
/* ============================================================== */

/* ============================================================== */
/* -------------------- Metadata Tag Version -------------------- */
/* ============================================================== */
#define META_SUBHAL_INPUTSRC_INFO_VERSION (1)
#define META_SUBHAL_VDEC_INFO_VERSION (1)
#define META_SUBHAL_RENDER_INFO_VERSION (1)

/* ============================================================== */
/* ---------------------- Metadata Content ---------------------- */
/* ============================================================== */

enum meta_mute_action {
	no_mute_action = 0,
	freeze = 1,
	mute = 2,
	panel_mute = 3,
	backlight_mute = 4,
	tx_mute = 5,
};

#ifndef E_COMPONENT8_TAG_START
#define E_COMPONENT8_TAG_START 0x00080000
enum EN_COMPONENT8_TAG { /* range 0x00080000 ~ 0x0008FFFF */
	META_SUBHAL_INPUTSRC_INFO = E_COMPONENT8_TAG_START,
	META_SUBHAL_VDEC_INFO,
	META_SUBHAL_RENDER_INFO,
	E_COMPONENT8_TAG_MAX,
};
#endif

struct meta_inputsrc_info {
	enum meta_mute_action mute_action;
	bool hdmi_mode;
};

struct meta_vdec_info {
	enum meta_mute_action mute_action;
};

struct meta_subhal_render_info {
	enum meta_mute_action mute_action;
};
#endif /*__UAPI_MTK_M_SBUHAL_H*/

