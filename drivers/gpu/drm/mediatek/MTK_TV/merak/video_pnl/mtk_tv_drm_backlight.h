/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_BACKLIGHT_H_
#define _MTK_TV_DRM_BACKLIGHT_H_
#include <drm/mtk_tv_drm.h>
#include <linux/mutex.h>
#include "mtk_tv_drm_video_panel.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_panel_context;

struct mtk_tv_drm_backlight_info {
	int init;
	struct mutex mutex;

	// only record controllable pwm param
	uint32_t pwm_period_shift[MTK_BACKLIGHT_PORT_NUM_MAX];
	uint32_t pwm_period_multi[MTK_BACKLIGHT_PORT_NUM_MAX];
	uint32_t pwm_vsync_enable[MTK_BACKLIGHT_PORT_NUM_MAX];

	struct drm_mtk_range_value value[MTK_BACKLIGHT_PORT_NUM_MAX];
	uint32_t frame_rate; // only store integer part, ignore fractional part
	uint32_t vrr_enable;
	uint32_t pwm_enable;
	uint32_t freerun_enable;
};

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_backlight_init(
	struct mtk_panel_context *panel_ctx,
	bool force,
	bool pwm_enable);

int mtk_tv_drm_backlight_set_property(
	struct mtk_panel_context *panel_ctx,
	struct drm_mtk_range_value *range);

int mtk_tv_drm_backlight_set_frame_rate(
	struct mtk_panel_context *panel_ctx,
	uint32_t frame_rate);

int mtk_tv_drm_backlight_set_pwm_enable(
	struct mtk_panel_context *panel_ctx,
	bool pwm_enable);

int mtk_tv_drm_backlight_control(
	struct mtk_panel_context *panel_ctx,
	struct drm_mtk_tv_backight_control_param *param);

int mtk_tv_drm_backlight_ioctl(
	struct drm_device *drm_dev,
	void *data,
	struct drm_file *file_priv);

ssize_t mtk_tv_drm_backlight_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf);

ssize_t mtk_tv_drm_backlight_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

#endif //_MTK_TV_DRM_BACKLIGHT_H_
