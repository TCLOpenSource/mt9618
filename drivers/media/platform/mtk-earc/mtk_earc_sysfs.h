/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_EARC_SYSFS_H
#define MTK_EARC_SYSFS_H

#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>

/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */
#define STR_UNIT_LENGTH						60
#define CONVERT_STR_LENGTH					10
#define GENERIC_ERROR_CODE_MAX				255

#define EARC_DIFF_TERMINATION_MAX			1
#define EARC_DIFF_DRIVE_STRENGTH_MAX		3
#define EARC_DIFF_SKEW_MAX					2
#define EARC_COMM_TERMINATION_MAX			1
#define EARC_COMM_DRIVE_STRENGTH_MAX		3

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */
typedef enum {
	E_EARC_STATUS_IDLE1 = 1,
	E_EARC_STATUS_IDLE2 = 2,
	E_EARC_STATUS_DISC1 = 4,
	E_EARC_STATUS_DISC2 = 8,
	E_EARC_STATUS_EARC  = 16,
} E_EARC_STATUS;

/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */
struct mtk_earc_info {
	struct kobject earc_kobj;
};


/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */

#endif  //#ifndef MTK_EARC_SYSFS_H
