// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_backlight.h"
#include "mtk_tv_drm_oled.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "hwreg_render_stub.h"
#include "ext_command_render_if.h"
#include "pqu_msg.h"
#include "pqu_render.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_GLOBAL_PQ
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define WRN(fmt, args...) DRM_WARN("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define LDM_MAX_LEVEL   0xFF
#define LDM_MIN_LEVEL   0x00

#define PWM_NS_TIMING 1000000000
#define PWM_SHIFT_MAX 100U
#define PWM_SHIFT_MIN 0U

#define INTERPOLATE(src_max, src_min, src_val, dst_max, dst_min) ({		\
	uint64_t src_base = (src_max)-(src_min);				\
	uint64_t dst_base = (dst_max)-(dst_min);				\
	uint64_t src_diff = (src_val)-(src_min);				\
	uint64_t dst_val = (src_diff * dst_base / src_base) + (dst_min);	\
	dst_val; })

#define FRAME_RATE_INTEGER(fr)	((fr)/1000)
#define FRAME_RATE_23		(23)
#define FRAME_RATE_24		(24)
#define FRAME_RATE_25		(25)
#define FRAME_RATE_26		(26)
#define FRAME_RATE_29		(29)
#define FRAME_RATE_30		(30)
#define FRAME_RATE_49		(49)
#define FRAME_RATE_50		(50)
#define FRAME_RATE_59		(59)
#define FRAME_RATE_60		(60)
#define FRAME_RATE_99		(99)
#define FRAME_RATE_100		(100)
#define FRAME_RATE_119		(119)
#define FRAME_RATE_120		(120)

#define PWM_DEBUG_SYSFS (1)
#define SYSFS_PRINT(len, buf, fmt, ...) \
	(len += snprintf(buf + len, PAGE_SIZE - len, fmt "\n", ##__VA_ARGS__))

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------
#if PWM_DEBUG_SYSFS
enum backlight_dbg_type {
	DBG_BYPASS,
	DBG_VRR_ENABLE,
	DBG_PWM_ENABLE,
	DBG_FRAME_RATE,
	DBG_SHIFT,
	DBG_VSYNC,
	DBG_MULTI,
	DBG_VALUE,
	DBG_MAX_TYPE
};

struct dbg_env {
	bool enable;
	char *name;
};
#endif

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------
struct mtk_tv_drm_backlight_info _backlight_info;
#if PWM_DEBUG_SYSFS
struct mtk_tv_drm_backlight_info _dbg_info;
static struct dbg_env _dbg_env[DBG_MAX_TYPE] = {
	[DBG_BYPASS]     = { .enable = false, .name = "bypass"},
	[DBG_VRR_ENABLE] = { .enable = false, .name = "vrr_enable"},
	[DBG_PWM_ENABLE] = { .enable = false, .name = "pwm_enable"},
	[DBG_FRAME_RATE] = { .enable = false, .name = "frame_rate"},
	[DBG_SHIFT]      = { .enable = false, .name = "shift"},
	[DBG_VSYNC]      = { .enable = false, .name = "vsync"},
	[DBG_MULTI]      = { .enable = false, .name = "multi"},
	[DBG_VALUE]      = { .enable = false, .name = "value"},
};
#endif

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static inline int _backlight_update_pqu(
	struct mtk_panel_context *panel_ctx)
{
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	struct drm_mtk_range_value *value = NULL;
	struct mtk_tv_drm_metabuf metabuf;
	struct pqu_render_shared_memory pqu_info;
	struct msg_render_shared_memory msg_info;
	struct pqu_render_backlight backlight;
	uint32_t pwm_duty, pwm_period, pwm_shift, period_multi, pwm_max, pwm_min, vsync_enable;
	bool ldm_update = false;
	uint8_t i;

#if PWM_DEBUG_SYSFS
	if (_dbg_env[DBG_BYPASS].enable)
		return 0;
#endif

	memset(&backlight, 0, sizeof(struct pqu_render_backlight));
	backlight.pwm_num = panel_ctx->cus.pwmport_num;
	backlight.control = !info->pwm_enable; // when pwm disable, force to set pwm
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		value = &info->value[i];

		// LDM use first valid PWM value
		if (ldm_update == false && value->valid == true) {
			backlight.ldm_level = INTERPOLATE(value->max_value,
							value->min_value,
							value->value,
							LDM_MAX_LEVEL,
							LDM_MIN_LEVEL);
			LOG("%s%x, %s%u, %s%u, %s%u, %s%u",
				"LDM:0x", backlight.ldm_level,
				"frame_rate:", info->frame_rate,
				"vrr_enable:", info->vrr_enable,
				"pwm_enable:", info->pwm_enable,
				"freerun_enable:", info->freerun_enable);
			ldm_update = true;
		}

		vsync_enable = (info->vrr_enable || info->freerun_enable) ?
					false : info->pwm_vsync_enable[i];

		period_multi = (info->pwm_period_multi[i] > 1) ?
					info->pwm_period_multi[i] : 1;

		pwm_period = (vsync_enable) ?
					PWM_NS_TIMING / (info->frame_rate * period_multi) :
					panel_ctx->cus.period_pwm[i] / period_multi;

		pwm_max = pwm_period;

		/* Interpolation
		 * from: 0       cus.min_pwm_val     cus.max_pwm_val
		 *       |--------------|-------------------|
		 *   to: 0          ?pwm_min?            pwm_max
		 */
		pwm_min = INTERPOLATE(panel_ctx->cus.max_pwm_val[i],
					0,
					panel_ctx->cus.min_pwm_val[i],
					pwm_max,
					0);

		/* Interpolation
		 * from: min_value       value           max_value
		 *          |--------------|----------------|
		 *   to: pwm_min       ?pwm_duty?        pwm_max
		 */
		pwm_duty = INTERPOLATE(value->max_value,
					value->min_value,
					value->value,
					pwm_max,
					pwm_min);

		/* Interpolation
		 * from: PWM_SHIFT_MIN   pwm_period_shift   PWM_SHIFT_MAX
		 *             |----------------|-----------------|
		 *   to:       0           ?pwm_shift?        pwm_period
		 */
		pwm_shift = (info->pwm_period_shift[i] == 0) ? 0 :
				INTERPOLATE(PWM_SHIFT_MAX,
					PWM_SHIFT_MIN,
					info->pwm_period_shift[i],
					pwm_period,
					0);

		LOG("value[%d] = %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u, %s:%u", i,
			"valid", value->valid,
			"value", value->value,
			"max_value", value->max_value,
			"min_value", value->min_value,
			"period_shift", info->pwm_period_shift[i],
			"period_multi", info->pwm_period_multi[i],
			"vsync_enable", info->pwm_vsync_enable[i]);

		if (pwm_duty > pwm_period) {
			ERR("pwm_duty(%x) > pwm_period(%x)", pwm_duty, pwm_period);
			pwm_duty = pwm_period;
		}
		backlight.pwm_info[i].port         = panel_ctx->cus.pwmport[i];
		backlight.pwm_info[i].period       = pwm_period;
		backlight.pwm_info[i].duty         = pwm_duty;
		backlight.pwm_info[i].shift        = pwm_shift;
		backlight.pwm_info[i].div          = panel_ctx->cus.div_pwm[i];
		backlight.pwm_info[i].polarity     = panel_ctx->cus.pol_pwm[i];
		backlight.pwm_info[i].enable       = value->valid && info->pwm_enable;
		backlight.pwm_info[i].vsync_enable = vsync_enable;
		LOG("PWM[%d] = %s%x, %s%x, %s%x, %s%x, %s%x, %s%x, %s%x, %s%x", i,
			"port:0x",   backlight.pwm_info[i].port,
			"period:0x", backlight.pwm_info[i].period,
			"duty:0x",   backlight.pwm_info[i].duty,
			"shift:0x",  backlight.pwm_info[i].shift,
			"div:0x",    backlight.pwm_info[i].div,
			"pol:0x",    backlight.pwm_info[i].polarity,
			"enable:0x", backlight.pwm_info[i].enable,
			"vsync:0x",  backlight.pwm_info[i].vsync_enable);
	}
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&metabuf,
			MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_BACKLIGHT)) {
		ERR("get metabuf MTK_TV_DRM_METABUF_MMAP_TYPE_PQU_BACKLIGHT fail");
		return -EINVAL;
	}
	memcpy(metabuf.addr, &backlight, sizeof(struct pqu_render_backlight));
	memset(&pqu_info, 0, sizeof(struct pqu_render_shared_memory));
	memset(&msg_info, 0, sizeof(struct msg_render_shared_memory));
	pqu_info.addr = msg_info.addr = metabuf.mmap_info.phy_addr;
	pqu_info.size = msg_info.size = metabuf.size;

	if (panel_ctx->oled_info.oled_support)
		mtk_oled_set_luminance(&panel_ctx->oled_info, (uint8_t)backlight.ldm_level);

	mtk_tv_drm_metabuf_free(&metabuf);
	return MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_BACKLIGHT, &msg_info, &pqu_info);
}

// return updated number or errno
static inline int _backlight_set_value(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;
	int ret = 0;

	if (param->port_num != panel_ctx->cus.pwmport_num) {
		ERR("Invalid PWM port num: input(%u) != expect(%u)",
			param->port_num, panel_ctx->cus.pwmport_num);
		return -EPERM;
	}
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		if (param->value[i].valid == true && (
		    param->value[i].value < param->value[i].min_value ||
		    param->value[i].value > param->value[i].max_value)) {
			ERR("Invalid range_value[%d] = valid:%d, value:%d, max:%d, min:%d", i,
				param->value[i].valid, param->value[i].value,
				param->value[i].max_value, param->value[i].min_value);
			return -EPERM;
		}
	}

#if PWM_DEBUG_SYSFS
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i)
		_dbg_info.value[i] = param->value[i];
#endif
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		LOG("range[%d] = valid:%u(%u), max:%u(%u), min:%u(%u), value:%u(%u)", i,
			param->value[i].valid,     info->value[i].valid,
			param->value[i].max_value, info->value[i].max_value,
			param->value[i].min_value, info->value[i].min_value,
			param->value[i].value,     info->value[i].value);
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_VALUE].enable))
			continue;
#endif
		if (!memcmp(&info->value[i], &param->value[i], sizeof(struct drm_mtk_range_value)))
			continue;
		info->value[i] = param->value[i];
		ret++;
	}
	return ret;
}

// return updated number or errno
static inline int _backlight_get_value(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;

	param->port_num = panel_ctx->cus.pwmport_num;
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_VALUE].enable))
			param->value[i] = _dbg_info.value[i];
		else
#endif
			param->value[i] = info->value[i];
		LOG("range_value[%d] = valid:%u, max:%u, min:%u, value:%u", i,
			param->value[i].valid,
			param->value[i].max_value,
			param->value[i].min_value,
			param->value[i].value);
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_set_vsync_enable(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;
	int ret = 0;

	if (param->port_num != panel_ctx->cus.pwmport_num) {
		ERR("Invalid PWM port num: input(%u) != expect(%u)",
			param->port_num, panel_ctx->cus.pwmport_num);
		return -EPERM;
	}

#if PWM_DEBUG_SYSFS
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i)
		_dbg_info.pwm_vsync_enable[i] = param->vsync_enable[i];
#endif
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		LOG("vsync_enable[%d] = %u(%u)", i,
			param->vsync_enable[i], info->pwm_vsync_enable[i]);

#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_VSYNC].enable))
			continue;
#endif
		if (info->pwm_vsync_enable[i] == param->vsync_enable[i])
			continue;
		info->pwm_vsync_enable[i] = param->vsync_enable[i];
		ret++;
	}
	return ret;
}

// return updated number or errno
static inline int _backlight_get_vsync_enable(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;

	param->port_num = panel_ctx->cus.pwmport_num;
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_VSYNC].enable))
			param->vsync_enable[i] = _dbg_info.pwm_vsync_enable[i];
		else
#endif
			param->vsync_enable[i] = (bool)info->pwm_vsync_enable[i];
		LOG("vsync_enable[%d] = %u", i, param->vsync_enable[i]);
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_set_period_multi(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;
	int ret = 0;

	if (param->port_num != panel_ctx->cus.pwmport_num) {
		ERR("Invalid PWM port num: input(%u) != expect(%u)",
			param->port_num, panel_ctx->cus.pwmport_num);
		return -EPERM;
	}
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		if (param->period_multi[i] == 0) {
			ERR("Invalid period_multi[%u] = %u", i, param->period_multi[i]);
			return -EPERM;
		}
	}

#if PWM_DEBUG_SYSFS
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i)
		_dbg_info.pwm_period_multi[i] = param->period_multi[i];
#endif
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		LOG("period_multi[%d] = %u(%u)", i,
			param->period_multi[i], info->pwm_period_multi[i]);
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_MULTI].enable))
			continue;
#endif
		if (info->pwm_period_multi[i] == param->period_multi[i])
			continue;
		info->pwm_period_multi[i] = param->period_multi[i];
		ret++;
	}
	return ret;
}

// return updated number or errno
static inline int _backlight_get_period_multi(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;

	param->port_num = panel_ctx->cus.pwmport_num;
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_MULTI].enable))
			param->period_multi[i] = _dbg_info.pwm_period_multi[i];
		else
#endif
			param->period_multi[i] = info->pwm_period_multi[i];
		LOG("period_multi[%d] = %u", i, param->period_multi[i]);
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_set_vrr_enable(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	LOG("vrr_enable = %u(%u)", param->vrr_enable, info->vrr_enable);
#if PWM_DEBUG_SYSFS
	_dbg_info.vrr_enable = param->vrr_enable;
	if (unlikely(_dbg_env[DBG_VRR_ENABLE].enable))
		return 0;
#endif
	if (info->vrr_enable != param->vrr_enable) {
		info->vrr_enable = param->vrr_enable;
		return 1;
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_get_vrr_enable(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
#if PWM_DEBUG_SYSFS
	if (_dbg_env[DBG_VRR_ENABLE].enable)
		param->vrr_enable = (bool)_dbg_info.vrr_enable;
	else
#endif
		param->vrr_enable = (bool)info->vrr_enable;
	LOG("vrr_enable = %u", param->vrr_enable);
	return 0;
}

// return updated number or errno
static inline int _backlight_set_period_shift(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;
	int ret = 0;

	if (param->port_num != panel_ctx->cus.pwmport_num) {
		ERR("Invalid PWM port num: input(%u) != expect(%u)",
			param->port_num, panel_ctx->cus.pwmport_num);
		return -EPERM;
	}
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		if (param->period_shift[i] > PWM_SHIFT_MAX) { // unsigned must >= 0
			ERR("Invalid period_shift[%d] = %u", i, param->period_shift[i]);
			return -EPERM;
		}
	}

#if PWM_DEBUG_SYSFS
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i)
		_dbg_info.pwm_period_shift[i] = param->period_shift[i];
#endif

	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		LOG("period_shift[%d] = %u(%u)", i,
			param->period_shift[i], info->pwm_period_shift[i]);
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_SHIFT].enable))
			continue;
#endif
		if (info->pwm_period_shift[i] == param->period_shift[i])
			continue;
		info->pwm_period_shift[i] = param->period_shift[i];
		ret++;
	}
	return ret;
}

// return updated number or errno
static inline int _backlight_get_period_shift(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	struct drm_mtk_tv_backight_control_param *param)
{
	uint8_t i;

	param->port_num = panel_ctx->cus.pwmport_num;
	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
#if PWM_DEBUG_SYSFS
		if (unlikely(_dbg_env[DBG_SHIFT].enable))
			param->period_shift[i] = _dbg_info.pwm_period_shift[i];
		else
#endif
			param->period_shift[i] = info->pwm_period_shift[i];
		LOG("period_shift[%d] = %u", i, param->period_multi[i]);
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_set_frame_rate(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	uint32_t frame_rate_pnl)
{
	uint32_t frame_rate_pwm;
	bool freerun_enable;

	switch (frame_rate_pnl) {
	case FRAME_RATE_23:
	case FRAME_RATE_24:
		frame_rate_pwm = FRAME_RATE_24;
		freerun_enable = false;
		break;
	case FRAME_RATE_25:
	case FRAME_RATE_26:
	case FRAME_RATE_49:
	case FRAME_RATE_50:
		frame_rate_pwm = FRAME_RATE_50;
		freerun_enable = false;
		break;
	case FRAME_RATE_29:
	case FRAME_RATE_30:
	case FRAME_RATE_59:
	case FRAME_RATE_60:
		frame_rate_pwm = FRAME_RATE_60;
		freerun_enable = false;
		break;
	case FRAME_RATE_99:
	case FRAME_RATE_100:
		frame_rate_pwm = FRAME_RATE_100;
		freerun_enable = false;
		break;
	case FRAME_RATE_119:
	case FRAME_RATE_120:
		frame_rate_pwm = FRAME_RATE_120;
		freerun_enable = false;
		break;
	default:
		frame_rate_pwm = frame_rate_pnl;
		freerun_enable = true;
		break;
	}

	LOG("frame_rate = %u -> %u(%u), free_run = %u(%u)", frame_rate_pnl,
		frame_rate_pwm, info->frame_rate, freerun_enable, info->freerun_enable);
#if PWM_DEBUG_SYSFS
	_dbg_info.frame_rate = frame_rate_pwm;
	_dbg_info.freerun_enable = freerun_enable;
	if (unlikely(_dbg_env[DBG_FRAME_RATE].enable))
		return 0;
#endif
	if (info->frame_rate != frame_rate_pwm) {
		info->frame_rate = frame_rate_pwm;
		info->freerun_enable = freerun_enable;
		return 1;
	}
	return 0;
}

// return updated number or errno
static inline int _backlight_set_pwm_enable(
	struct mtk_panel_context *panel_ctx,
	struct mtk_tv_drm_backlight_info *info,
	bool pwm_enable)
{
	LOG("pwm_enable = %u(%u)", pwm_enable, info->pwm_enable);
#if PWM_DEBUG_SYSFS
	_dbg_info.pwm_enable = pwm_enable;
	if (unlikely(_dbg_env[DBG_PWM_ENABLE].enable))
		return 0;
#endif
	if (info->pwm_enable != pwm_enable) {
		info->pwm_enable = pwm_enable;
		return 1;
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_backlight_init(
	struct mtk_panel_context *panel_ctx,
	bool force,
	bool pwm_enable)
{
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	int i;

	if (panel_ctx == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}

	LOG("init:%d, force:%d, pwm_enable:%d", info->init, force, pwm_enable);
	if (info->init == true) {
		if (force == false) {
			WRN("Backlight is inited but not force re-init");
			return 0;
		}
		info->init = false;
		mutex_destroy(&info->mutex);
	}
	memset(info, 0, sizeof(struct mtk_tv_drm_backlight_info));

	for (i = 0; i < panel_ctx->cus.pwmport_num; ++i) {
		// use dtso as default pwm value
		info->value[i].valid = true;
		info->value[i].max_value = panel_ctx->cus.max_pwm_val[i];
		info->value[i].min_value = panel_ctx->cus.min_pwm_val[i];
		info->value[i].value     = panel_ctx->cus.duty_pwm[i];

		// store pwm default setting
		info->pwm_period_shift[i] = panel_ctx->cus.shift_pwm[i];
		info->pwm_period_multi[i] = panel_ctx->cus.pwm_period_multi[i];
		info->pwm_vsync_enable[i] = panel_ctx->cus.pwm_vsync_enable[i];
		LOG("DTS[%d] =%s:%x,%s:%x,%s:%x,%s:%x,%s:%x,%s:%x,%s:%x,%s:%x,%s:%x,%s:%x", i,
			" port",   panel_ctx->cus.pwmport[i],
			" period", panel_ctx->cus.period_pwm[i],
			" duty",   panel_ctx->cus.duty_pwm[i],
			" shift",  panel_ctx->cus.shift_pwm[i],
			" div",    panel_ctx->cus.div_pwm[i],
			" pol",    panel_ctx->cus.pol_pwm[i],
			" vsync",  panel_ctx->cus.pwm_vsync_enable[i],
			" multi",  panel_ctx->cus.pwm_period_multi[i],
			" max",    panel_ctx->cus.max_pwm_val[i],
			" min",    panel_ctx->cus.min_pwm_val[i]);
	}
	mutex_init(&info->mutex);
	info->frame_rate = 0;
	info->vrr_enable = false;
	info->pwm_enable = pwm_enable;
	info->init = true;
#if PWM_DEBUG_SYSFS
	memcpy(&_dbg_info, info, sizeof(struct mtk_tv_drm_backlight_info));
#endif
	return 0;
}

int mtk_tv_drm_backlight_set_property(
	struct mtk_panel_context *panel_ctx,
	struct drm_mtk_range_value *value)
{
	struct drm_mtk_tv_backight_control_param param;

	if (panel_ctx == NULL || value == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}

	drv_STUB("Backlight_valid",     value->valid);
	drv_STUB("Backlight_max_value", value->max_value);
	drv_STUB("Backlight_min_value", value->min_value);
	drv_STUB("Backlight_value",     value->value);

	memset(&param, 0, sizeof(struct drm_mtk_tv_backight_control_param));
	param.action    = E_MTK_TV_BACKLIGHT_CONTROL_SET_VALUE_ALL;
	param.value_all = *value;

	return mtk_tv_drm_backlight_control(panel_ctx, &param);
}

int mtk_tv_drm_backlight_set_frame_rate(
	struct mtk_panel_context *panel_ctx,
	uint32_t frame_rate)
{
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	int ret = 0;

	if (panel_ctx == NULL || frame_rate == 0) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (unlikely(info->init == false)) {
		ERR("Not init");
		return -EINVAL;
	}

	mutex_lock(&info->mutex);
	ret = _backlight_set_frame_rate(panel_ctx, info, FRAME_RATE_INTEGER(frame_rate));
	if (ret > 0) { // updated number > 0
		ret = _backlight_update_pqu(panel_ctx);
		if (ret < 0)
			ERR("_backlight_update_pqu fail, ret = %d", ret);
	}
	mutex_unlock(&info->mutex);
	return ret;
}

int mtk_tv_drm_backlight_set_pwm_enable(
	struct mtk_panel_context *panel_ctx,
	bool pwm_enable)
{
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	int ret = 0;

	if (panel_ctx == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (unlikely(info->init == false)) {
		ERR("Not init");
		return -EINVAL;
	}

	mutex_lock(&info->mutex);
	ret = _backlight_set_pwm_enable(panel_ctx, info, pwm_enable);
	if (ret > 0) { // updated number > 0
		ret = _backlight_update_pqu(panel_ctx);
		if (ret < 0)
			ERR("_backlight_update_pqu fail, ret = %d", ret);
	}
	mutex_unlock(&info->mutex);
	return ret;
}

int mtk_tv_drm_backlight_control(
	struct mtk_panel_context *panel_ctx,
	struct drm_mtk_tv_backight_control_param *param)
{
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	struct drm_mtk_range_value value;
	uint8_t i;
	int ret = 0;

	if (panel_ctx == NULL || param == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (unlikely(info->init == false)) {
		ERR("Not init");
		return -EINVAL;
	}

	mutex_lock(&info->mutex);
	switch (param->action) {
	case E_MTK_TV_BACKLIGHT_CONTROL_SET_VALUE_ALL:
		value = param->value_all;
		param->port_num = panel_ctx->cus.pwmport_num;
		for (i = 0; i < panel_ctx->cus.pwmport_num; ++i)
			param->value[i] = value;
		ret = _backlight_set_value(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_VALUE_ALL:
		param->port_num = panel_ctx->cus.pwmport_num;
		ret = _backlight_get_value(panel_ctx, info, param);
		if (ret)
			break;
		value = param->value[0];
		param->value_all = value;
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_SET_VALUE:
		ret = _backlight_set_value(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_VALUE:
		ret = _backlight_get_value(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_SET_VSYNC_ENABLE:
		ret = _backlight_set_vsync_enable(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_VSYNC_ENABLE:
		ret = _backlight_get_vsync_enable(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_SET_PERIOD_MULTI:
		ret = _backlight_set_period_multi(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_PERIOD_MULTI:
		ret = _backlight_get_period_multi(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_SET_VRR_ENABLE:
		ret = _backlight_set_vrr_enable(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_VRR_ENABLE:
		ret = _backlight_get_vrr_enable(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_SET_PERIOD_SHIFT:
		ret = _backlight_set_period_shift(panel_ctx, info, param);
		break;

	case E_MTK_TV_BACKLIGHT_CONTROL_GET_PERIOD_SHIFT:
		ret = _backlight_get_period_shift(panel_ctx, info, param);
		break;

	default:
		mutex_unlock(&info->mutex);
		ERR("Invalid action %d", param->action);
		return -EINVAL;
	}

	if (ret > 0) // updated number > 0
		ret = _backlight_update_pqu(panel_ctx);
	mutex_unlock(&info->mutex);
	return ret;
}

int mtk_tv_drm_backlight_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct mtk_tv_kms_context *pctx = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_tv_drm_crtc *mtk_tv_crtc = NULL;
	struct drm_mtk_tv_backight_control_param *param = NULL;

	if (!drm_dev || !data || !file_priv) {
		ERR("Invalid input");
		return -EINVAL;
	}
	drm_for_each_crtc(crtc, drm_dev) {
		mtk_tv_crtc = to_mtk_tv_crtc(crtc);
		break;
	}
	pctx = (struct mtk_tv_kms_context *)mtk_tv_crtc->crtc_private;
	param = (struct drm_mtk_tv_backight_control_param *)data;
	return mtk_tv_drm_backlight_control(&pctx->panel_priv, param);
}

ssize_t mtk_tv_drm_backlight_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if PWM_DEBUG_SYSFS
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	struct dbg_env *e = NULL;
	uint32_t dbg, val;
	int len = 0;
	uint8_t i;

	SYSFS_PRINT(len, buf, "Controlled variable:");
	SYSFS_PRINT(len, buf, "  bypass=[1,0]     : enable/disable bypass pwm");
	SYSFS_PRINT(len, buf, "  vrr_enable=[1,0] : enable/disable vrr mode");
	SYSFS_PRINT(len, buf, "  pwm_enable=[1,0] : enable/disable pwm");
	SYSFS_PRINT(len, buf, "  frame_rate=[num] : set frame_rate");
	SYSFS_PRINT(len, buf, "  shift=[num]      : %% of period");
	SYSFS_PRINT(len, buf, "  vsync=[1,0]      : enable/disable pwm vsync align");
	SYSFS_PRINT(len, buf, "  multi=[num]      : set period multi");
	SYSFS_PRINT(len, buf, "  value=[num]      : set value of backlight value");
	SYSFS_PRINT(len, buf, "  reset=[1]        : reset to normal mode");
	SYSFS_PRINT(len, buf, "");

	SYSFS_PRINT(len, buf, "Debug environment:");
	for (i = 0; i < DBG_MAX_TYPE; ++i) {
		e = &_dbg_env[i];
		switch (i) {
		case DBG_VRR_ENABLE:
			val = info->vrr_enable;
			dbg = _dbg_info.vrr_enable;
			break;
		case DBG_PWM_ENABLE:
			val = info->pwm_enable;
			dbg = _dbg_info.pwm_enable;
			break;
		case DBG_FRAME_RATE:
			val = info->frame_rate;
			dbg = _dbg_info.frame_rate;
			break;
		case DBG_VSYNC:
			val = info->pwm_vsync_enable[0];
			dbg = _dbg_info.pwm_vsync_enable[0];
			break;
		case DBG_MULTI:
			val = info->pwm_period_multi[0];
			dbg = _dbg_info.pwm_period_multi[0];
			break;
		case DBG_SHIFT:
			val = info->pwm_period_shift[0];
			dbg = _dbg_info.pwm_period_shift[0];
			break;
		case DBG_VALUE:
			val = info->value[0].value;
			dbg = _dbg_info.value[0].value;
			break;
		case DBG_BYPASS:
			val = e->enable;
			dbg = 0;
		default:
			break;
		}
		if (e->enable)
			SYSFS_PRINT(len, buf, "  %-10s = %u(%u)", e->name, val, dbg);
		else
			SYSFS_PRINT(len, buf, "  %-10s = NA(%u)", e->name, val);
	}
	return len;
#else
	return 0;
#endif
}

ssize_t mtk_tv_drm_backlight_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#if PWM_DEBUG_SYSFS
	struct mtk_tv_kms_context *kms = dev_get_drvdata(dev);
	struct mtk_panel_context *pnl = &kms->panel_priv;
	struct mtk_tv_drm_backlight_info *info = &_backlight_info;
	struct dbg_env *e = NULL;
	uint64_t value;
	uint8_t i, j;

	if (mtk_render_common_parser_cmd_int(buf, "reset", &value) == 0) {
		// restore all variable
		// disable all debug env
		memcpy(info, &_dbg_info, sizeof(struct mtk_tv_drm_backlight_info));
		for (i = 0; i < DBG_MAX_TYPE; ++i)
			_dbg_env[i].enable = false;
	}
	for (i = 0; i < DBG_MAX_TYPE; ++i) {
		e = &_dbg_env[i];
		if (mtk_render_common_parser_cmd_int(buf, e->name, &value) != 0)
			continue;
		e->enable = true;
		switch (i) {
		case DBG_VRR_ENABLE:
			info->vrr_enable = value;
			break;
		case DBG_PWM_ENABLE:
			info->pwm_enable = value;
			break;
		case DBG_FRAME_RATE:
			info->frame_rate = value;
			info->freerun_enable = false;
			break;
		case DBG_VSYNC:
			for (j = 0; j < pnl->cus.pwmport_num; ++j)
				info->pwm_vsync_enable[j] = value;
			break;
		case DBG_MULTI:
			for (j = 0; j < pnl->cus.pwmport_num; ++j)
				info->pwm_period_multi[j] = value;
			break;
		case DBG_SHIFT:
			for (j = 0; j < pnl->cus.pwmport_num; ++j)
				info->pwm_period_shift[j] = value;
			break;
		case DBG_VALUE:
			for (j = 0; j < pnl->cus.pwmport_num; ++j)
				info->value[j].value = value;
			break;
		case DBG_BYPASS:
			e->enable = value;
			break;
		default:
			break;
		}
	}
	_backlight_update_pqu(pnl);
#endif
	return count;
}
