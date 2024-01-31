/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_AVD_MUX_H
#define MTK_SRCCAP_AVD_MUX_H

#include <linux/videodev2.h>
#include <linux/types.h>
#include "mtk_srccap.h"
/* ============================================================================================== */
/* ------------------------------------------ Defines ------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------- Macros ------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------- Enums -------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------ Structs ------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
enum V4L2_AVD_RESULT mtk_avd_input(unsigned char *u8ScartFB, struct mtk_srccap_dev *srccap_dev);

#endif  // MTK_SRCCAP_AVD_MUX_H