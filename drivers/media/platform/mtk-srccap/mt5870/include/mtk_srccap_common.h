/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef MTK_SRCCAP_COMMON_H
#define MTK_SRCCAP_COMMON_H


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
struct mtk_srccap_dev;

struct srccap_common_info {
	struct task_struct *vsync_monitor_task;
};

/* ============================================================================================== */
/* ----------------------------------------- Functions ------------------------------------------ */
/* ============================================================================================== */
int mtk_common_subscribe_event_vsync(struct mtk_srccap_dev *srccap_dev);
int mtk_common_unsubscribe_event_vsync(struct mtk_srccap_dev *srccap_dev);
int mtk_srccap_register_common_subdev(
	struct v4l2_device *srccap_dev,
	struct v4l2_subdev *subdev_common,
	struct v4l2_ctrl_handler *common_ctrl_handler);
void mtk_srccap_unregister_common_subdev(
	struct v4l2_subdev *subdev_common);

#endif  // MTK_SRCCAP_COMMON_H
