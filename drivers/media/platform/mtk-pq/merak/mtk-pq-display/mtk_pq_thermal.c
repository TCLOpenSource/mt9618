// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

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
#include <linux/workqueue.h>
#include "mtk_pq_thermal.h"
#include "mtk_pq_common.h"

//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define CDEV_NAME_LEN				(64)
#define MAX_STATE				(1UL)
#define MTK_THERMAL_DEFAULT_AISR_OFF_TEMP	(123000U)
#define MTK_THERMAL_DEFAULT_AISR_HALF_TEMP	(118000U)
#define MTK_THERMAL_DEFAULT_AISR_ON_TEMP	(105000U)
#define MTK_THERMAL_DEFAULT_OFF_DELAY		(30)
#define MTK_THERMAL_POLLING_TIME		(1000)

struct mtk_pq_cdev {
	unsigned long cur_state;
	unsigned long prev_state;
	struct thermal_zone_device *tz;
	struct delayed_work poll_queue;
};

struct mtk_pq_thermal _thermal_info;
static void mtk_pq_aisr_throttle_handle(struct work_struct *work);
static DECLARE_DELAYED_WORK(aisr_throttle_work_queue, mtk_pq_aisr_throttle_handle);

static int _mtk_thermal_aisr_off_temp = MTK_THERMAL_DEFAULT_AISR_OFF_TEMP;
module_param(_mtk_thermal_aisr_off_temp, int, 0644);

static int _mtk_thermal_aisr_half_temp = MTK_THERMAL_DEFAULT_AISR_HALF_TEMP;
module_param(_mtk_thermal_aisr_half_temp, int, 0644);

static int _mtk_thermal_aisr_on_temp = MTK_THERMAL_DEFAULT_AISR_ON_TEMP;
module_param(_mtk_thermal_aisr_on_temp, int, 0644);

static int _mtk_thermal_aisr_off_delay_sec = MTK_THERMAL_DEFAULT_OFF_DELAY;
module_param(_mtk_thermal_aisr_off_delay_sec, int, 0644);

static int mtk_pq_find_state(int temp)
{
	int state;

	if (temp >= _mtk_thermal_aisr_off_temp)
		state = MTK_PQ_THERMAL_STATE_AISR_OFF;
	else if (temp >= _mtk_thermal_aisr_half_temp &&
		temp < _mtk_thermal_aisr_off_temp)
		state = MTK_PQ_THERMAL_STATE_AISR_HALF;
	else if (temp >= _mtk_thermal_aisr_on_temp &&
			temp < _mtk_thermal_aisr_half_temp)
		state = MTK_PQ_THERMAL_STATE_AISR_KEEP;
	else
		state = MTK_PQ_THERMAL_STATE_AISR_ON;

	return state;
}

static void mtk_pq_update_cdev_state(struct mtk_pq_cdev *mtk_dev, int state)
{
	if ((mtk_dev == NULL) || (mtk_dev->prev_state == state))
		return;

	// aisr off
	if (state == MTK_PQ_THERMAL_STATE_AISR_OFF)
		schedule_delayed_work(&aisr_throttle_work_queue,
				HZ * _mtk_thermal_aisr_off_delay_sec);

	// aisr on
	if (state == MTK_PQ_THERMAL_STATE_AISR_ON)
		_thermal_info.thermal_state_aisr = MTK_PQ_THERMAL_STATE_AISR_ON;

	// 1/2 aisr
	if (state == MTK_PQ_THERMAL_STATE_AISR_HALF) {
		if (mtk_dev->prev_state == MTK_PQ_THERMAL_STATE_AISR_OFF)
			cancel_delayed_work_sync(&aisr_throttle_work_queue);
		else if (_thermal_info.thermal_state_aisr == MTK_PQ_THERMAL_STATE_AISR_ON)
			_thermal_info.thermal_state_aisr = MTK_PQ_THERMAL_STATE_AISR_HALF;
	}

	// not reach off cancel off work queue
	if (state == MTK_PQ_THERMAL_STATE_AISR_KEEP) {
		if (mtk_dev->prev_state == MTK_PQ_THERMAL_STATE_AISR_OFF)
			cancel_delayed_work_sync(&aisr_throttle_work_queue);
	}

	mtk_dev->prev_state = state;
}

static void mtk_pq_thermal_zone_check(struct work_struct *work)
{
	struct thermal_zone_device *tz;
	int temp = 0, state = MTK_PQ_THERMAL_STATE_AISR_OFF;
	struct mtk_pq_cdev *mtk_dev = container_of(work, struct mtk_pq_cdev,
			poll_queue.work);

	WARN(!mtk_dev, "invalid mtk_pq_cdev %p\n", mtk_dev);
	tz = mtk_dev->tz;
	WARN_ON(!tz);

	thermal_zone_get_temp(tz, &temp);
	pr_debug("thermal, read temp = %d\n", temp);
	state = mtk_pq_find_state(temp);
	mtk_pq_update_cdev_state(mtk_dev, state);
	mod_delayed_work(system_freezable_power_efficient_wq,
			&mtk_dev->poll_queue,
			msecs_to_jiffies(MTK_THERMAL_POLLING_TIME));
}

int mtk_pq_cdev_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct device *dev;
	struct mtk_pq_cdev *mtk_dev;
	struct thermal_zone_device *tz;

	if (!pdev) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	np = pdev->dev.of_node;
	dev = &pdev->dev;

	if (!np) {
		dev_err(dev, "DT node not found\n");
		return -ENODEV;
	}

	mtk_dev = devm_kzalloc(dev, sizeof(*mtk_dev), GFP_KERNEL);
	if (!mtk_dev) {
		dev_err(dev, "failed to alloc mtk_dev\n");
		return -ENOMEM;
	}

	tz = thermal_zone_get_zone_by_name("mtk-thermal");
	if (IS_ERR(tz)) {
		dev_err(dev, "failed to get thermal zone %s\n", "mtk-thermal");
		return PTR_ERR(tz);
	}

	mtk_dev->tz = tz;
	INIT_DELAYED_WORK(&mtk_dev->poll_queue, mtk_pq_thermal_zone_check);
	/* queue work to system_freezable_power_efficient_wq */
	mod_delayed_work(system_freezable_power_efficient_wq,
			&mtk_dev->poll_queue,
			msecs_to_jiffies(MTK_THERMAL_POLLING_TIME));

	return 0;
}

int mtk_pq_cdev_remove(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev;

	if (!pdev) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	cdev = platform_get_drvdata(pdev);
	if (cdev)
		thermal_cooling_device_unregister(cdev);

	return 0;
}

int mtk_pq_cdev_get_thermal_info(struct mtk_pq_thermal *thermal_info)
{
	if (!thermal_info) {
		PQ_CHECK_NULLPTR();
		return -EPERM;
	}

	thermal_info->thermal_state_aisr = _thermal_info.thermal_state_aisr;

	return 0;
}

void mtk_pq_aisr_throttle_handle(struct work_struct *pstWork)
{
	_thermal_info.thermal_state_aisr = MTK_PQ_THERMAL_STATE_AISR_OFF;
}
