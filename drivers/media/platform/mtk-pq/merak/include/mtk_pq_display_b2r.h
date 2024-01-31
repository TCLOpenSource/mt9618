/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_DISPLAY_B2R_H
#define _MTK_PQ_DISPLAY_B2R_H
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <uapi/linux/mtk-v4l2-pq.h>

#include "mtk_pq.h"

//----------------------------------------------------------------------------
//  Driver Capability
//----------------------------------------------------------------------------
#define DISPLAY_B2R_PPU_SHIFT       (27)
#define DISPLAY_B2R_PPU_MASK        BIT(3)
#define DISPLAY_B2R_UNCOM_SHIFT     (28)
#define DISPLAY_B2R_UNCOM_MASK      BIT(0)
#define DISPLAY_B2R_VP9_SHIFT       (29)
#define DISPLAY_B2R_VP9_MASK        BIT(1)
#define DISPLAY_B2R_AV1_SHIFT       (30)
#define DISPLAY_B2R_AV1_MASK        BIT(2)
#define DISPLAY_B2R_PITCH_SHIFT     (16)
#define DISPLAY_B2R_PITCH_MASK      (0xFF)
#define DISPLAY_B2R_SELECT_SHIFT    (16)
#define DISPLAY_B2R_SELECT_MASK     (0x3)

//----------------------------------------------------------------------------
//  Macro and Define
//----------------------------------------------------------------------------

int mtk_pq_display_b2r_qbuf(struct mtk_pq_device *pq_dev, struct mtk_pq_buffer *pq_buf);
#endif
