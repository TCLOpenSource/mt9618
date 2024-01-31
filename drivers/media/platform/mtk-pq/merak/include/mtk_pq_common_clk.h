/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_COMMON_CLK_H
#define _MTK_PQ_COMMON_CLK_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <uapi/linux/mtk-v4l2-pq.h>

/* function*/
int mtk_pq_common_clk_setup(struct mtk_pq_platform_device *pdev, bool enable);
int mtk_pq_common_clk_parse_dts(struct mtk_pq_platform_device *pdev);
void mtk_pq_common_clk_create_sysfs(struct device *pdv);
void mtk_pq_common_clk_remove_sysfs(struct device *pdv);

#endif
