/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_THERMAL_H
#define _MTK_PQ_THERMAL_H

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/string.h>

enum mtk_pq_thermal_state {
	MTK_PQ_THERMAL_STATE_AISR_ON,
	MTK_PQ_THERMAL_STATE_AISR_KEEP,
	MTK_PQ_THERMAL_STATE_AISR_HALF,
	MTK_PQ_THERMAL_STATE_AISR_OFF,
};

enum mtk_pq_algo_thermal_state {
	MTK_PQ_ALGO_THERMAL_AISR_ON = 0,
	MTK_PQ_ALGO_THERMAL_AISR_OFF,
	MTK_PQ_ALGO_THERMAL_AISR_HALF,
	MTK_PQ_ALGO_THERMAL_AISR_PROCESS,
	MTK_PQ_ALGO_THERMAL_AISR_NONE,
	MTK_PQ_ALGO_THERMAL_AISR_MAX
};

struct mtk_pq_thermal {
	enum mtk_pq_thermal_state thermal_state_aisr;
	enum mtk_pq_algo_thermal_state thermal_algo_state_aisr;
};

int mtk_pq_cdev_probe(struct platform_device *pdev);
int mtk_pq_cdev_remove(struct platform_device *pdev);
int mtk_pq_cdev_get_thermal_info(struct mtk_pq_thermal *thermal_info);

#endif
