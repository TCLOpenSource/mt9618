// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_pattern.h"
#include "mtk_tv_drm_kms.h"
#include "hwreg_render_pattern.h"
#include "mtk_tv_drm_log.h"
#include "mdrv_gop.h"

//-----------------------------------------------------------------------------
//  Local Defines
//-----------------------------------------------------------------------------
#define MTK_DRM_MODEL			MTK_DRM_MODEL_PATTERN
#define PATTERN_LOG_ERROR		BIT(0)
#define PATTERN_LOG_INFO		BIT(1)
#define PATTERN_LOG_FLOW		BIT(2)

#define PATTERN_LOG(_level, _fmt, _args...) { \
	if (_level & PATTERN_LOG_ERROR) \
		DRM_ERROR("[PATTERN ERROR]  [%s,%5d]"_fmt, \
		__func__, __LINE__, ##_args); \
	else if (_level & PATTERN_LOG_INFO) { \
		DRM_INFO("[PATTERN INFO]  [%s,%5d]"_fmt, \
		__func__, __LINE__, ##_args); \
	} \
	else if (_level & PATTERN_LOG_FLOW) { \
		DRM_WARN("[PATTERN FLOW]  [%s,%5d]"_fmt, \
		__func__, __LINE__, ##_args); \
	} \
}

#define ENTER()			PATTERN_LOG(PATTERN_LOG_INFO, ">>>>>ENTER>>>>>\n")
#define EXIT()			PATTERN_LOG(PATTERN_LOG_INFO, "<<<<<EXIT <<<<<\n")
#define LOG(fmt, args...)	PATTERN_LOG(PATTERN_LOG_INFO, fmt, ##args)
#define ERR(fmt, args...)	PATTERN_LOG(PATTERN_LOG_ERROR, fmt, ##args)

#define PATTERN_NUM		(11)
#define PATTERN_REG_NUM_MAX		(40)
#define SYSFS_MAX_BUF_COUNT	(0x1000)
#define INVALID_PATTERN_POSITION				(0xFF)
#define DEFAULT_COLOR_VALUE (1023)
#define DEFAULT_LINE_INTERVAL (100)
#define DEFAULT_POSITION_ST (100)
#define DEFAULT_LINE_WIDTH (5)
#define DEFAULT_COLOR_2 (1)
#define DEFAULT_COLOR_3 (2)
#define DEFAULT_COLOR_4 (3)
#define DEFAULT_DIFF_LEVEL (4)
#define DEFAULT_VALUE_3 (3)
#define DEFAULT_VALUE_4 (4)
#define DEFAULT_VALUE_13 (13)
#define DEFAULT_VALUE_2160 (2160)
#define DEFAULT_VALUE_3840 (3840)
#define DEFAULT_VALUE_4320 (4320)
#define DEFAULT_VALUE_7680 (7680)
/*#define PATTERN_TEST*/
#define PAT_HWVER_1	1
#define PAT_HWVER_2	2
#define PAT_HWVER_3	3
#define GOP_PAT_COLORBAR_W (12)
#define GOP_PAT_COLORBAR_H (12)
#define GOP_PAT_DISPLAY_W (0)
#define GOP_PAT_DISPLAY_H (3)

#define OPBLEND(i) \
		((i >= DRM_PATTERN_POSITION_MULTIWIN) && \
			(i <= DRM_PATTERN_POSITION_SEC)) \

//-------------------------------------------------------------------------------------------------
//  Local enums
//-------------------------------------------------------------------------------------------------
enum mtk_drm_pattern_type {
	E_MTK_DRM_PATTERN_TYPE_FILL_PURE_COLOR = 0,
	E_MTK_DRM_PATTERN_TYPE_FILL_RAMP,
	E_MTK_DRM_PATTERN_TYPE_FILL_PURE_COLORBAR,
	E_MTK_DRM_PATTERN_TYPE_FILL_GRADIENT_COLORBAR,
	E_MTK_DRM_PATTERN_TYPE_FILL_BLACK_WHITE_COLORBAR,
	E_MTK_DRM_PATTERN_TYPE_FILL_CROSS,
	E_MTK_DRM_PATTERN_TYPE_FILL_PIP_WINDOW,
	E_MTK_DRM_PATTERN_TYPE_FILL_DOT_MATRIX,
	E_MTK_DRM_PATTERN_TYPE_FILL_CROSSHATCH,
	E_MTK_DRM_PATTERN_TYPE_FILL_FLAG_RANDOM,
	E_MTK_DRM_PATTERN_TYPE_FILL_FLAG_CHESSBOARD,
	E_MTK_DRM_PATTERN_TYPE_FILL_BOX,
	E_MTK_DRM_PATTERN_TYPE_FILL_HV_RAMP_MISC,
	E_MTK_DRM_PATTERN_TYPE_FILL_RGB_PURE_COLORBAR,
	E_MTK_DRM_PATTERN_TYPE_FILL_MAX
};

//-----------------------------------------------------------------------------
//  Local Variables
//-----------------------------------------------------------------------------
static struct drm_pattern_env pattern_env[DRM_PATTERN_POSITION_MAX];
static unsigned short g_capability[DRM_PATTERN_POSITION_MAX];
static bool b_inited;
char *position_str[DRM_PATTERN_POSITION_MAX] = {
	" OP_DRAM  ", " IP_BLEND ", " MultiWin ", " PQ_GAMMA ", "   TCon   ",
	"   GFX    ", "PAFRC_POST", "   SEC    "};

char *type_str[PATTERN_NUM] = {
	"      Pure Color      ", "         Ramp         ", "     Pure Colorbar    ",
	"   Gradient Colorbar  ", " Black-White Colorbar ", "         Cross        ",
	"      PIP Window      ", "      Dot Matrix      ", "      Crosshatch      ",
	"        Random        ", "      Chessboard      "};

__u8 HW_VERSION;
//-----------------------------------------------------------------------------
//  Local Functions
//-----------------------------------------------------------------------------
static bool _mtk_drm_pattern_get_value_from_string(char *buf, char *name,
	unsigned int len, int *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	ENTER();

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return false;
	}

	*value = 0;
	cmd = buf;

	for (string = buf; string < (buf + len); ) {
		strsep(&cmd, " =\n\r");
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;
	}

	for (string = buf; string < (buf + len); ) {
		tmp_name = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		tmp_value = string;
		for (string += strlen(string); string < (buf + len) && !*string;)
			string++;

		if (strncmp(tmp_name, name, strlen(name) + 1) == 0) {
			if (kstrtoint(tmp_value, 0, value)) {
				PATTERN_LOG(PATTERN_LOG_ERROR,
					"kstrtoint fail!\n");
				return find;
			}
			find = true;
			PATTERN_LOG(PATTERN_LOG_INFO,
				"name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"name(%s) was not found!\n", name);

	EXIT();
	return find;
}

static int _mtk_drm_pattern_get_size(
	enum drm_pattern_position position,
	unsigned short *h_size, unsigned short *v_size)
{
	int ret = 0;

	ENTER();

	if ((h_size == NULL) || (v_size == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	*h_size = pattern_env[position].h_size;
	*v_size = pattern_env[position].v_size;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_get_capability(struct drm_pattern_capability *pattern_cap)
{
	int ret = 0;
	int position = 0;
	struct hwreg_render_pattern_capability hwreg_cap;

	ENTER();

	memset(&hwreg_cap, 0, sizeof(struct hwreg_render_pattern_capability));

	if (pattern_cap == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (!b_inited) {
		for (position = 0; position < DRM_PATTERN_POSITION_MAX; position++) {
			hwreg_cap.position = position;
			ret = drv_hwreg_render_pattern_get_capability(&hwreg_cap, HW_VERSION);
			if (ret) {
				PATTERN_LOG(PATTERN_LOG_ERROR,
					"drv_hwreg_render_pattern_get_capability fail, ret = %d!\n",
					ret);
				return ret;
			}

			g_capability[position] = hwreg_cap.pattern_type;

			PATTERN_LOG(PATTERN_LOG_INFO,
				"g_capability[%u] = 0x%x\n", position, g_capability[position]);
		}

		b_inited = true;
	}

	for (position = 0; position < DRM_PATTERN_POSITION_MAX; position++) {

		pattern_cap->pattern_type[position] = g_capability[position];

		PATTERN_LOG(PATTERN_LOG_INFO,
			"pattern_type[%u] = 0x%x\n", position, pattern_cap->pattern_type[position]);
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_check_capability_get_type(struct drm_pattern_capability *pattern_cap,
	enum drm_pattern_position position, char *cmd, unsigned int len, int *type)
{
	int ret = 0;
	bool find = false;
	int value = 0;

	ENTER();

	if ((!pattern_cap) || (!cmd) || (!type)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid pointer\n");
		ret = -EINVAL;
		goto exit;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	if (!(pattern_cap->pattern_type[position])) {
		pr_crit("[STI PQ ERROR] [%s,%5d]%s pattern not support!\n",
		__func__, __LINE__, position_str[position]);
		ret = -ENODEV;
		goto exit;
	}

	find = _mtk_drm_pattern_get_value_from_string(cmd, "type", len, &value);
	if (!find) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Cmdline format error, type should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}
	*type = value;

	if (!((pattern_cap->pattern_type[position] >> value) & 1)) {
		pr_crit("[STI PQ ERROR] [%s,%5d]Pattern type not support, position : %s, type = %d!\n",
		__func__, __LINE__, position_str[position], *type);
		ret = -ENODEV;
		goto exit;
	}

exit:
	EXIT();

	return ret;
}

static int _mtk_drm_pattern_check_color_range(
	struct drm_pattern_color *color)
{
	int ret = 0;

	ENTER();

	if (color == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if ((color->red & 0xFC00)
		|| (color->green & 0xFC00)
		|| (color->blue & 0xFC00)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid RGB value(0x%x, 0x%x, 0x%x), should be within 10 bits!\n",
			color->red, color->green, color->blue);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_check_color_value(
	struct drm_pattern_color *color)
{
	int ret = 0;
	unsigned short max = 0;

	ENTER();

	if (color == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	max = max_t(unsigned short, color->red, max_t(unsigned short, color->green, color->blue));

	if ((color->red != 0) && (color->red != max)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid RGB value(0x%x, 0x%x, 0x%x), the start value should be same or 0, and also the end value!\n",
			color->red, color->green, color->blue);
		return -EINVAL;
	}

	if ((color->green != 0) && (color->green != max)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid RGB value(0x%x, 0x%x, 0x%x), the start value should be same or 0, and also the end value!\n",
			color->red, color->green, color->blue);
		return -EINVAL;
	}

	if ((color->blue != 0) && (color->blue != max)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid RGB value(0x%x, 0x%x, 0x%x), the start value should be same or 0, and also the end value!\n",
			color->red, color->green, color->blue);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_check_gradient_color(
	struct drm_pattern_color *start, struct drm_pattern_color *end)
{
	int ret = 0;

	ENTER();

	if ((start == NULL) || (end == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(start);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(end);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_value(start);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_value fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_value(end);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_value fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	if (!(((start->red >= end->red)
		&& (start->green >= end->green)
		&& (start->blue >= end->blue))
		|| ((start->red <= end->red)
		&& (start->green <= end->green)
		&& (start->blue <= end->blue)))) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid RGB value(0x%x, 0x%x, 0x%x), (0x%x, 0x%x, 0x%x), the trend should be consistent!\n",
			start->red, start->green, start->blue,
			end->red, end->green, end->blue);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_check_level(
	enum drm_pattern_position position,
	unsigned short diff_level, bool b_vertical)
{
	int ret = 0;
	unsigned char count = 0;
	unsigned short temp = 0;
	unsigned short level = diff_level;
	unsigned short h_size = 0;
	unsigned short v_size = 0;

	ENTER();

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	h_size = pattern_env[position].h_size;
	v_size = pattern_env[position].v_size;

	if (!(h_size & v_size)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid h/v size, h = 0x%x, v = 0x%x\n", h_size, v_size);
		return -EINVAL;
	}

	if (diff_level < MIN_DIFF_LEVEL) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Diff level(0x%x) should be greater than 0x%x\n",
			diff_level, MIN_DIFF_LEVEL);
		return -EINVAL;
	}

	if (b_vertical) {
		if (diff_level > v_size) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"Invalid level, level mast be smaller than v size, level = 0x%x, v = 0x%x\n",
				diff_level, v_size);
			return -EINVAL;
		}
	} else {
		if (diff_level > h_size) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"Invalid level, level mast be smaller than h size, level = 0x%x, h = 0x%x\n",
				diff_level, h_size);
			return -EINVAL;
		}
	}

	for (count = 0; count < 16; count++) {
		temp = level & 0x1;

		if (temp) {
			if (count < 2) {
				PATTERN_LOG(PATTERN_LOG_ERROR,
					"Diff level(0x%x) should be the nth power of 2\n",
					diff_level);
				return -EINVAL;
			}

			level = level >> 1;

			if (level) {
				PATTERN_LOG(PATTERN_LOG_ERROR,
					"Diff level(0x%x) should be the nth power of 2\n",
					diff_level);
				return -EINVAL;
			}
			break;
		}
		level = level >> 1;
	}

	EXIT();
	return ret;
}

void _mtk_drm_pattern_rgb2yuv(unsigned short red, unsigned short green, unsigned short blue,
	unsigned long *Y, unsigned long *Cb, unsigned long *Cr)
{
	long Pb = 0, Pr = 0;
	long Luma = 0;
	const int C1 = 299, C2 = 587, C3 = 114, C4 = 500, C5 = 1000, C6 = 1023;
	const int D1 = 36, D2 = 128, D3 = 219, D4 = 224, D5 = 255;

	Luma = C1 * red + C2 * green + C3 * blue;
	Pb = (C4 / (C5 - C3)) * (blue * C5 * C6 - Luma);
	Pr = (C4 / (C5 - C1)) * (red * C5 * C6 - Luma);

	// Y = 16 + 219 * Y;
	Luma = D1 * C5 * C6 + D3 * Luma;
	Pb = D2 * C5 * C6 + D4 * Pb;
	Pr = D2 * C5 * C6 + D4 * Pr;

	// convert 8bits to 10bits
	*Y = (unsigned long) Luma / D5 / C5;
	*Cb = (unsigned long) Pb / D5 / C5;
	*Cr = (unsigned long) Pr / D5 / C5;
}

static int _mtk_drm_pattern_fill_common_info(
	char *buf, unsigned int len,
	struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	find = _mtk_drm_pattern_get_value_from_string(buf, "enable", len, &value);
	if (!find) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Cmdline format error, enable should be set, please echo help!\n");
		return -EINVAL;
	}

	pattern_info->enable = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "force_timing", len, &value);
	if (find)
		pattern_info->force_timing.enable = TRUE;
	else
		pattern_info->force_timing.enable = FALSE;

	find = _mtk_drm_pattern_get_value_from_string(buf, "force_width", len, &value);
	if (find)
		pattern_info->force_timing.force_h_size = value;
	else
		pattern_info->force_timing.force_h_size = 0;

	find = _mtk_drm_pattern_get_value_from_string(buf, "force_height", len, &value);
	if (find)
		pattern_info->force_timing.force_v_size = value;
	else
		pattern_info->force_timing.force_v_size = 0;


	if (pattern_info->enable) {
		ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
			return ret;
		}

		pattern_info->h_size = h_size;
		pattern_info->v_size = v_size;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_pure_color(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	struct drm_pattern_pure_color *pure_color = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pure_color = &pattern_info->m.pure_color;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_PURE_COLOR;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r", len, &value);
	if (!find)
		pure_color->color.red = DEFAULT_COLOR_VALUE;
	else
		pure_color->color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g", len, &value);
	if (!find)
		pure_color->color.green = DEFAULT_COLOR_VALUE;
	else
		pure_color->color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b", len, &value);
	if (!find)
		pure_color->color.blue = DEFAULT_COLOR_VALUE;
	else
		pure_color->color.blue = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_ramp(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	struct drm_pattern_ramp *ramp = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ramp = &pattern_info->m.ramp;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_RAMP;

	_mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	ramp->start.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	ramp->start.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	ramp->start.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "vertical", len, &value);
	ramp->b_vertical = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	if (!find)
		ramp->end.red = DEFAULT_COLOR_VALUE;
	else
		ramp->end.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	if (!find)
		ramp->end.green = DEFAULT_COLOR_VALUE;
	else
		ramp->end.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	if (!find)
		ramp->end.blue = DEFAULT_COLOR_VALUE;
	else
		ramp->end.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "level", len, &value);
	if (!find)
		ramp->diff_level = 4;
	else
		ramp->diff_level = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_pure_colorbar(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_pure_colorbar *pure_colorbar = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	if (pattern_info->position == DRM_PATTERN_POSITION_MULTIWIN) {
		h_size = pattern_info->h_size;
		v_size = pattern_info->v_size;
	}

	pure_colorbar = &pattern_info->m.pure_colorbar;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_PURE_COLORBAR;

	_mtk_drm_pattern_get_value_from_string(buf, "movable", len, &value);
	pure_colorbar->movable = value;

	_mtk_drm_pattern_get_value_from_string(buf, "speed", len, &value);
	pure_colorbar->shift_speed = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "width", len, &value);
	if (!find)
		pure_colorbar->colorbar_h_size = (h_size >> 3);
	else
		pure_colorbar->colorbar_h_size = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "height", len, &value);
	if (!find)
		pure_colorbar->colorbar_v_size = v_size;
	else
		pure_colorbar->colorbar_v_size = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_gradient_colorbar(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	struct drm_pattern_gradient_colorbar *gradient_colorbar = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	gradient_colorbar = &pattern_info->m.gradient_colorbar;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_GRADIENT_COLORBAR;

	_mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	gradient_colorbar->start.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	gradient_colorbar->start.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	gradient_colorbar->start.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "color_1", len, &value);
	gradient_colorbar->color_1st = value;

	_mtk_drm_pattern_get_value_from_string(buf, "vertical", len, &value);
	gradient_colorbar->b_vertical = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	if (!find)
		gradient_colorbar->end.red = DEFAULT_COLOR_VALUE;
	else
		gradient_colorbar->end.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	if (!find)
		gradient_colorbar->end.green = DEFAULT_COLOR_VALUE;
	else
		gradient_colorbar->end.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	if (!find)
		gradient_colorbar->end.blue = DEFAULT_COLOR_VALUE;
	else
		gradient_colorbar->end.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "color_2", len, &value);
	if (!find)
		gradient_colorbar->color_2nd = DEFAULT_COLOR_2;
	else
		gradient_colorbar->color_2nd = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "color_3", len, &value);
	if (!find)
		gradient_colorbar->color_3rd = DEFAULT_COLOR_3;
	else
		gradient_colorbar->color_3rd = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "color_4", len, &value);
	if (!find)
		gradient_colorbar->color_4th = DEFAULT_COLOR_4;
	else
		gradient_colorbar->color_4th = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "level", len, &value);
	if (!find)
		gradient_colorbar->diff_level = DEFAULT_DIFF_LEVEL;
	else
		gradient_colorbar->diff_level = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_black_white_colorbar(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_black_white_colorbar *black_white_colorbar = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	black_white_colorbar = &pattern_info->m.black_white_colorbar;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_BLACK_WHITE_COLORBAR;

	_mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	black_white_colorbar->first_color.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	black_white_colorbar->first_color.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	black_white_colorbar->first_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	if (!find)
		black_white_colorbar->second_color.red = DEFAULT_COLOR_VALUE;
	else
		black_white_colorbar->second_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	if (!find)
		black_white_colorbar->second_color.green = DEFAULT_COLOR_VALUE;
	else
		black_white_colorbar->second_color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	if (!find)
		black_white_colorbar->second_color.blue = DEFAULT_COLOR_VALUE;
	else
		black_white_colorbar->second_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "width", len, &value);
	if (!find)
		black_white_colorbar->colorbar_h_size = (h_size >> DEFAULT_VALUE_3);
	else
		black_white_colorbar->colorbar_h_size = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "height", len, &value);
	if (!find)
		black_white_colorbar->colorbar_v_size = v_size;
	else
		black_white_colorbar->colorbar_v_size = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_cross(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_cross *cross = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	cross = &pattern_info->m.cross;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_CROSS;

	_mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	cross->cross_color.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	cross->cross_color.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	cross->border_color.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	cross->border_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	cross->cross_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	cross->border_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "position_h", len, &value);
	if (!find)
		cross->h_position = (h_size >> 1);
	else
		cross->h_position = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "position_v", len, &value);
	if (!find)
		cross->v_position = (v_size >> 1);
	else
		cross->v_position = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_pip_window(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_pip_window *pip_window = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	pip_window = &pattern_info->m.pip_window;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_PIP_WINDOW;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	if (!find)
		pip_window->pip_color.red = DEFAULT_COLOR_VALUE;
	else
		pip_window->pip_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	if (!find)
		pip_window->pip_color.green = DEFAULT_COLOR_VALUE;
	else
		pip_window->pip_color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	if (!find)
		pip_window->pip_color.blue = DEFAULT_COLOR_VALUE;
	else
		pip_window->pip_color.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	pip_window->bg_color.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	pip_window->bg_color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	if (!find)
		pip_window->bg_color.blue = DEFAULT_COLOR_VALUE;
	else
		pip_window->bg_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "x", len, &value);
	if (!find)
		pip_window->window.x = (h_size >> 2);
	else
		pip_window->window.x = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "y", len, &value);
	if (!find)
		pip_window->window.y = (v_size >> 2);
	else
		pip_window->window.y = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "width", len, &value);
	if (!find)
		pip_window->window.width = (h_size >> 1);
	else
		pip_window->window.width = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "height", len, &value);
	if (!find)
		pip_window->window.height = (v_size >> 1);
	else
		pip_window->window.height = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_crosshatch(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_crosshatch *crosshatch = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	crosshatch = &pattern_info->m.crosshatch;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_CROSSHATCH;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	if (!find)
		crosshatch->line_color.red = DEFAULT_COLOR_VALUE;
	else
		crosshatch->line_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	if (!find)
		crosshatch->line_color.green = DEFAULT_COLOR_VALUE;
	else
		crosshatch->line_color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	if (!find)
		crosshatch->line_color.blue = DEFAULT_COLOR_VALUE;
	else
		crosshatch->line_color.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	crosshatch->bg_color.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	crosshatch->bg_color.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	crosshatch->bg_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_line_interval", len, &value);
	if (!find)
		crosshatch->h_line_interval = DEFAULT_LINE_INTERVAL;
	else
		crosshatch->h_line_interval = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_line_interval", len, &value);
	if (!find)
		crosshatch->v_line_interval = DEFAULT_LINE_INTERVAL;
	else
		crosshatch->v_line_interval = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_line_width", len, &value);
	if (!find)
		crosshatch->h_line_width = DEFAULT_LINE_WIDTH;
	else
		crosshatch->h_line_width = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_line_width", len, &value);
	if (!find)
		crosshatch->v_line_width = DEFAULT_LINE_WIDTH;
	else
		crosshatch->v_line_width = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_position_st", len, &value);
	if (!find)
		crosshatch->h_position_st = DEFAULT_POSITION_ST;
	else
		crosshatch->h_position_st = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_position_st", len, &value);
	if (!find)
		crosshatch->v_position_st = DEFAULT_POSITION_ST;
	else
		crosshatch->v_position_st = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_box(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_box *box = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	box = &pattern_info->m.box;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_BOX;

	find = _mtk_drm_pattern_get_value_from_string(buf, "r_st", len, &value);
	if (!find)
		box->line_color.red = DEFAULT_COLOR_VALUE;
	else
		box->line_color.red = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "g_st", len, &value);
	if (!find)
		box->line_color.green = DEFAULT_COLOR_VALUE;
	else
		box->line_color.green = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "b_st", len, &value);
	if (!find)
		box->line_color.blue = DEFAULT_COLOR_VALUE;
	else
		box->line_color.blue = value;

	_mtk_drm_pattern_get_value_from_string(buf, "r_end", len, &value);
	box->bg_color.red = value;

	_mtk_drm_pattern_get_value_from_string(buf, "g_end", len, &value);
	box->bg_color.green = value;

	_mtk_drm_pattern_get_value_from_string(buf, "b_end", len, &value);
	box->bg_color.blue = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_line_interval", len, &value);
	if (!find)
		box->h_line_interval = DEFAULT_LINE_INTERVAL;
	else
		box->h_line_interval = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_line_interval", len, &value);
	if (!find)
		box->v_line_interval = DEFAULT_LINE_INTERVAL;
	else
		box->v_line_interval = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_line_width", len, &value);
	if (!find)
		box->h_line_width = DEFAULT_LINE_WIDTH;
	else
		box->h_line_width = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_line_width", len, &value);
	if (!find)
		box->v_line_width = DEFAULT_LINE_WIDTH;
	else
		box->v_line_width = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "h_position_st", len, &value);
	if (!find)
		box->h_position_st = DEFAULT_POSITION_ST;
	else
		box->h_position_st = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "v_position_st", len, &value);
	if (!find)
		box->v_position_st = DEFAULT_POSITION_ST;
	else
		box->v_position_st = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_hv_ramp_misc(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;


	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}
	pattern_info->pattern_type = MTK_PATTERN_FLAG_HV_RAMP_MISC;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_rgb_pure_colorbar(char *buf,
	unsigned int len, struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	int value = 0;
	bool find = false;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	struct drm_pattern_rgb_pure_colorbar *rgb_pure_colorbar = NULL;

	ENTER();

	if ((buf == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_get_size(pattern_info->position, &h_size, &v_size);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_get_size fail, ret = %d!\n", ret);
		return ret;
	}

	rgb_pure_colorbar = &pattern_info->m.rgb_pure_colorbar;
	pattern_info->pattern_type = MTK_PATTERN_FLAG_RGB_PURE_COLORBAR;

	find = _mtk_drm_pattern_get_value_from_string(buf, "width", len, &value);
	if (!find)
		rgb_pure_colorbar->colorbar_h_size = (h_size >> DEFAULT_VALUE_3);
	else
		rgb_pure_colorbar->colorbar_h_size = value;

	find = _mtk_drm_pattern_get_value_from_string(buf, "height", len, &value);
	if (!find)
		rgb_pure_colorbar->colorbar_v_size = v_size;
	else
		rgb_pure_colorbar->colorbar_v_size = value;

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_gop_pat(bool enable)
{
	ST_HW_GOP_PATINFO gop_pattern_info;

	memset(&gop_pattern_info, 0, sizeof(ST_HW_GOP_PATINFO));
	gop_pattern_info.ColorbarW = GOP_PAT_COLORBAR_W;
	gop_pattern_info.ColorbarH = GOP_PAT_COLORBAR_H;
	gop_pattern_info.DisWidth = GOP_PAT_DISPLAY_W;
	gop_pattern_info.DisHeight = GOP_PAT_DISPLAY_H;
	gop_pattern_info.PatEnable = enable;

	if (!KDrv_HW_GOP_SetTestPattern(&gop_pattern_info)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "KDrv_HW_GOP_SetTestPattern fail\n");
		return -EINVAL;
	}

	return 0;
}

static int _mtk_drm_pattern_set_off(bool riu,
	enum hwreg_render_pattern_position point, struct hwregOut *pattern_reg)
{
	int ret = 0;

	if (point == HWREG_RENDER_PATTERN_POSITION_OPDRAM) {
		ret = drv_hwreg_render_pattern_opdram_set_off(riu);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"[drv_hwreg][%s %d] fail, position = %u, ret = %d!\n",
				__func__, __LINE__, point, ret);
			return -EINVAL;
		}
	} else if (point == HWREG_RENDER_PATTERN_POSITION_IPBLEND) {
		ret = drv_hwreg_render_pattern_ipblend_set_off(riu, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"[drv_hwreg][%s %d] fail, position = %u, ret = %d!\n",
			__func__, __LINE__, point, ret);
			return -EINVAL;
		}
	} else if (point == HWREG_RENDER_PATTERN_POSITION_GFX && HW_VERSION >= PAT_HWVER_3) {
		ret = _mtk_drm_pattern_set_gop_pat(false);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"[drv_hwreg][%s %d] fail, position = %u, ret = %d!\n",
			__func__, __LINE__, point, ret);
			return -EINVAL;
		}
	} else if (OPBLEND((enum drm_pattern_position)point)) {
		ret = drv_hwreg_render_pattern_opblend_set_off(riu, point, pattern_reg, HW_VERSION);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"[drv_hwreg][%s %d] fail, position = %u, ret = %d!\n",
				__func__, __LINE__, point, ret);
			return -EINVAL;
		}
	} else {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position(%u)!\n", point);
		return -EINVAL;
	}

	return ret;
}

static int _mtk_drm_pattern_set_pure_color(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_pure_color data;
	struct drm_pattern_pure_color *pure_color = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_pure_color));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	pure_color = &(pattern_info->m.pure_color);

	ret = _mtk_drm_pattern_check_color_range(&pure_color->color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,	"[drv_hwreg][%s %d] fail, ret = %d!\n",
				__func__, __LINE__, ret);
		return -EINVAL;
	}

	if (position == DRM_PATTERN_POSITION_OPDRAM) {
		data.common.position = (enum hwreg_render_pattern_position)position;
		data.common.enable = true;
		data.color.red = pure_color->color.red;
		data.color.green = pure_color->color.green;
		data.color.blue = pure_color->color.blue;

		ret = drv_hwreg_render_pattern_set_opdram_pure_color(riu, &data, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,	"[drv_hwreg][%s %d] fail, ret = %d!\n",
					__func__, __LINE__, ret);
			return -EINVAL;
		}
	} else if (position == DRM_PATTERN_POSITION_IPBLEND) {
		data.common.hsk_force_signal = pattern_info->force_timing.enable;
		data.common.h_size = pattern_info->h_size;
		data.common.v_size = pattern_info->v_size;
		ret = drv_hwreg_render_pattern_set_ipblend_pure_color(riu,
			pattern_reg, &(data.common));
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR, "[drv_hwreg][%s %d] fail, ret = %d!\n",
					__func__, __LINE__, ret);
			return -EINVAL;
		}
	} else if (OPBLEND(position)) {
		data.common.position = (enum hwreg_render_pattern_position)position;
		data.common.enable = true;
		data.common.h_size = pattern_info->h_size;
		data.common.v_size = pattern_info->v_size;
		data.common.color_space =
			(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;
		data.color.red = pure_color->color.red;
		data.color.green = pure_color->color.green;
		data.color.blue = pure_color->color.blue;

		ret = drv_hwreg_render_pattern_set_opblend_pure_color(
			riu, &data, pattern_reg, HW_VERSION);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR, "[drv_hwreg][%s %d] fail, ret = %d!\n",
					__func__, __LINE__, ret);
			return -EINVAL;
		}
	} else {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position, position = %u\n"
				, position);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_ramp(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	unsigned short color_st = 0;
	unsigned short color_end = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	unsigned int diff_level = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_ramp data;
	struct drm_pattern_ramp *ramp = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_ramp));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	h_size = pattern_env[position].h_size;
	v_size = pattern_env[position].v_size;

	if (!(h_size & v_size)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid h/v size, h = 0x%x, v = 0x%x\n", h_size, v_size);
		return -EINVAL;
	}

	ramp = &(pattern_info->m.ramp);

	ret = _mtk_drm_pattern_check_level(position, ramp->diff_level, ramp->b_vertical);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_level fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_gradient_color(&ramp->start, &ramp->end);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_gradient_color fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	color_st = max_t(unsigned short, ramp->start.red,
		max_t(unsigned short, ramp->start.green, ramp->start.blue));

	color_end = max_t(unsigned short, ramp->end.red,
		max_t(unsigned short, ramp->end.green, ramp->end.blue));

	diff_level = ramp->diff_level;
	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;
	data.start.red_integer = ramp->start.red;
	data.start.green_integer = ramp->start.green;
	data.start.blue_integer = ramp->start.blue;
	data.end.red_integer = ramp->end.red;
	data.end.green_integer = ramp->end.green;
	data.end.blue_integer = ramp->end.blue;
	data.b_vertical = ramp->b_vertical;

	if (ramp->b_vertical) {
		data.diff_h = h_size / 4;
		data.diff_v = (abs(color_st - color_end) << 13) / (diff_level - 1);
		data.ratio_h = 1;
		data.ratio_v = data.diff_v * diff_level / v_size;
	} else {
		data.diff_v = v_size / 4;
		data.diff_h = (abs(color_st - color_end) << 13) / (diff_level - 1);
		data.ratio_v = 1;
		data.ratio_h = data.diff_h * diff_level / h_size;
	}

	ret = drv_hwreg_render_pattern_set_ramp(riu, &data, pattern_reg, HW_VERSION);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_ramp fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_pure_colorbar(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_pure_colorbar data;
	struct drm_pattern_pure_colorbar *pure_colorbar = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_pure_colorbar));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	if (position == DRM_PATTERN_POSITION_GFX && HW_VERSION >= PAT_HWVER_3) {
		ret = _mtk_drm_pattern_set_gop_pat(true);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_gop_pat fail, ret = %d!\n", ret);
			return -EINVAL;
		}
	} else {
		pure_colorbar = &(pattern_info->m.pure_colorbar);
		data.common.position = (enum hwreg_render_pattern_position)position;
		data.common.enable = true;
		data.common.h_size = pattern_info->h_size;
		data.common.v_size = pattern_info->v_size;
		data.common.hsk_force_signal = pattern_info->force_timing.enable;
		data.common.force_h_size = pattern_info->force_timing.force_h_size;
		data.common.force_v_size = pattern_info->force_timing.force_v_size;
		data.common.color_space =
			(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;
		data.diff_h = pure_colorbar->colorbar_h_size;
		data.diff_v = pure_colorbar->colorbar_v_size;
		data.movable = pure_colorbar->movable;

		if (pure_colorbar->movable)
			data.shift_speed = pure_colorbar->shift_speed;

		ret = drv_hwreg_render_pattern_set_pure_colorbar(riu, &data, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"drv_hwreg_render_pattern_set_pure_colorbar fail, ret = %d!\n",
				ret);
			return -EINVAL;
		}
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_gradient_colorbar(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	unsigned short color_st = 0;
	unsigned short color_end = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	unsigned int diff_level = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_MAX;
	enum drm_pattern_color_space color_space = DRM_PATTERN_COLOR_SPACE_MAX;
	struct hwreg_render_pattern_gradient_colorbar data;
	struct drm_pattern_gradient_colorbar *gradient_colorbar = NULL;
	unsigned long Y = 0, Cb = 0, Cr = 0;

	memset(&data, 0, sizeof(struct hwreg_render_pattern_gradient_colorbar));

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	position = pattern_info->position;

	if ((position >= DRM_PATTERN_POSITION_MAX) || (position < 0)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid win_id or position, position = %u\n", position);
		return -EINVAL;
	}

	color_space = pattern_env[position].color_space;

	h_size = pattern_env[position].h_size;
	v_size = pattern_env[position].v_size;

	if (!(h_size & v_size)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid h/v size, h = 0x%x, v = 0x%x\n", h_size, v_size);
		return -EINVAL;
	}

	gradient_colorbar = &(pattern_info->m.gradient_colorbar);

	ret = _mtk_drm_pattern_check_level(position,
		gradient_colorbar->diff_level, gradient_colorbar->b_vertical);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_level fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_gradient_color(&gradient_colorbar->start,
		&gradient_colorbar->end);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_gradient_color fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	color_st = max_t(unsigned short, gradient_colorbar->start.red,
		max_t(unsigned short, gradient_colorbar->start.green,
		gradient_colorbar->start.blue));

	color_end = max_t(unsigned short, gradient_colorbar->end.red,
		max_t(unsigned short, gradient_colorbar->end.green,
		gradient_colorbar->end.blue));

	diff_level = gradient_colorbar->diff_level;
	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space = (enum hwreg_render_pattern_color_space)color_space;
	data.color0 = (enum hwreg_render_pattern_colorbar_color)gradient_colorbar->color_1st;
	data.color1 = (enum hwreg_render_pattern_colorbar_color)gradient_colorbar->color_2nd;
	data.color2 = (enum hwreg_render_pattern_colorbar_color)gradient_colorbar->color_3rd;
	data.color3 = (enum hwreg_render_pattern_colorbar_color)gradient_colorbar->color_4th;

	if (color_space == (enum drm_pattern_color_space) 0) {
		data.start.red_integer = gradient_colorbar->start.red;
		data.start.green_integer = gradient_colorbar->start.green;
		data.start.blue_integer = gradient_colorbar->start.blue;
		data.end.red_integer = gradient_colorbar->end.red;
		data.end.green_integer = gradient_colorbar->end.green;
		data.end.blue_integer = gradient_colorbar->end.blue;
	} else {
		_mtk_drm_pattern_rgb2yuv(gradient_colorbar->start.red,
			gradient_colorbar->start.green,
			gradient_colorbar->start.blue, &Y, &Cb, &Cr);
		data.start.red_integer = gradient_colorbar->start.red;
		data.start.green_integer = gradient_colorbar->start.green;
		data.start.blue_integer = gradient_colorbar->start.blue;
		_mtk_drm_pattern_rgb2yuv(gradient_colorbar->end.red,
			gradient_colorbar->end.green,
			gradient_colorbar->end.blue, &Y, &Cb, &Cr);
		data.end.red_integer = gradient_colorbar->end.red;
		data.end.green_integer = gradient_colorbar->end.green;
		data.end.blue_integer = gradient_colorbar->end.blue;
	}

	data.b_vertical = gradient_colorbar->b_vertical;

	if (gradient_colorbar->b_vertical) {
		data.diff_h = h_size / DEFAULT_VALUE_4;
		data.diff_v = (abs(color_st - color_end) << DEFAULT_VALUE_13) / (diff_level - 1);
		data.ratio_h = 1;
		data.ratio_v = data.diff_v * diff_level / v_size;
	} else {
		data.diff_v = v_size / DEFAULT_VALUE_4;
		data.diff_h = (abs(color_st - color_end) << DEFAULT_VALUE_13) / (diff_level - 1);
		data.ratio_v = 1;
		data.ratio_h = data.diff_h * diff_level / h_size;
	}

	ret = drv_hwreg_render_pattern_set_gradient_colorbar(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_gradient_colorbar fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_black_white_colorbar(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_MAX;
	enum drm_pattern_color_space color_space = DRM_PATTERN_COLOR_SPACE_MAX;
	struct hwreg_render_pattern_black_white_colorbar data;
	struct drm_pattern_black_white_colorbar *bw_colorbar = NULL;
	unsigned long Y = 0, Cb = 0, Cr = 0;

	memset(&data, 0, sizeof(struct hwreg_render_pattern_black_white_colorbar));

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	position = pattern_info->position;

	if ((position >= DRM_PATTERN_POSITION_MAX) || (position < 0)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid win_id or position, position = %u\n", position);
		return -EINVAL;
	}

	color_space = pattern_env[position].color_space;

	bw_colorbar = &(pattern_info->m.black_white_colorbar);

	ret = _mtk_drm_pattern_check_color_range(&bw_colorbar->first_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(&bw_colorbar->second_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space = (enum hwreg_render_pattern_color_space)color_space;

	if (color_space == (enum drm_pattern_color_space) 0) {
		data.first_color.red = bw_colorbar->first_color.red;
		data.first_color.green = bw_colorbar->first_color.green;
		data.first_color.blue = bw_colorbar->first_color.blue;
		data.second_color.red = bw_colorbar->second_color.red;
		data.second_color.green = bw_colorbar->second_color.green;
		data.second_color.blue = bw_colorbar->second_color.blue;
	} else {
		_mtk_drm_pattern_rgb2yuv(bw_colorbar->first_color.red,
			bw_colorbar->first_color.green,
			bw_colorbar->first_color.blue, &Y, &Cb, &Cr);
		data.first_color.red = Cr;
		data.first_color.green = Y;
		data.first_color.blue = Cb;
		_mtk_drm_pattern_rgb2yuv(bw_colorbar->second_color.red,
			bw_colorbar->second_color.green,
			bw_colorbar->second_color.blue, &Y, &Cb, &Cr);
		data.second_color.red = Cr;
		data.second_color.green = Y;
		data.second_color.blue = Cb;
	}

	data.diff_h = bw_colorbar->colorbar_h_size;
	data.diff_v = bw_colorbar->colorbar_v_size;

	ret = drv_hwreg_render_pattern_set_black_white_colorbar(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_black_white_colorbar fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_cross(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_MAX;
	enum drm_pattern_color_space color_space = DRM_PATTERN_COLOR_SPACE_MAX;
	struct hwreg_render_pattern_cross data;
	struct drm_pattern_cross *cross = NULL;
	unsigned long Y = 0, Cb = 0, Cr = 0;

	memset(&data, 0, sizeof(struct hwreg_render_pattern_cross));

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	position = pattern_info->position;

	if ((position >= DRM_PATTERN_POSITION_MAX) || (position < 0)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid win_id or position, position = %u\n", position);
		return -EINVAL;
	}

	color_space = pattern_env[position].color_space;

	h_size = pattern_env[position].h_size;
	v_size = pattern_env[position].v_size;

	if (!(h_size & v_size)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid h/v size, h = 0x%x, v = 0x%x\n", h_size, v_size);
		return -EINVAL;
	}

	cross = &(pattern_info->m.cross);

	ret = _mtk_drm_pattern_check_color_range(&cross->cross_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(&cross->border_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.color_space = (enum hwreg_render_pattern_color_space)color_space;
	data.common.h_size = h_size;
	data.common.v_size = v_size;
	data.cross_position_h = cross->h_position;
	data.cross_position_v = cross->v_position;

	if (color_space == (enum drm_pattern_color_space) 0) {
		data.cross_color.red = cross->cross_color.red;
		data.cross_color.green = cross->cross_color.green;
		data.cross_color.blue = cross->cross_color.blue;
		data.border_color.red = cross->border_color.red;
		data.border_color.green = cross->border_color.green;
		data.border_color.blue = cross->border_color.blue;
	} else {
		_mtk_drm_pattern_rgb2yuv(cross->cross_color.red,
			cross->cross_color.green,
			cross->cross_color.blue, &Y, &Cb, &Cr);
		data.cross_color.red = Cr;
		data.cross_color.green = Y;
		data.cross_color.blue = Cb;
		_mtk_drm_pattern_rgb2yuv(cross->border_color.red,
			cross->border_color.green,
			cross->border_color.blue, &Y, &Cb, &Cr);
		data.border_color.red = Cr;
		data.border_color.green = Y;
		data.border_color.blue = Cb;
	}

	ret = drv_hwreg_render_pattern_set_cross(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_cross fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_pip_window(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_pip_window data;
	struct drm_pattern_window *window = NULL;
	struct drm_pattern_pip_window *pip_window = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_pip_window));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	h_size = pattern_env[position].h_size;
	v_size = pattern_env[position].v_size;

	pip_window = &(pattern_info->m.pip_window);
	window = &(pip_window->window);

	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;

	ret = _mtk_drm_pattern_check_color_range(&pip_window->pip_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(&pip_window->bg_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	data.pip.red_integer = pip_window->pip_color.red;
	data.pip.green_integer = pip_window->pip_color.green;
	data.pip.blue_integer = pip_window->pip_color.blue;
	data.bg.red_integer = pip_window->bg_color.red;
	data.bg.green_integer = pip_window->bg_color.green;
	data.bg.blue_integer = pip_window->bg_color.blue;

	if ((window->x >= h_size) || (window->x + window->width >= h_size)
		|| (window->y >= v_size) || (window->y + window->height >= v_size)
		|| (window->width == 0) || (window->height == 0)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid pip window size(%u, %u, %u, %u), ret = %d!\n",
			window->x, window->y, window->width, window->height, ret);
		return -EINVAL;
	}

	data.window.h_start = window->x;
	data.window.h_end = window->x + window->width;
	data.window.v_start = window->y;
	data.window.v_end = window->y + window->height;

	ret = drv_hwreg_render_pattern_set_pip_window(riu, &data, pattern_reg, HW_VERSION);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_pip_window fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_crosshatch(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_crosshatch data;
	struct drm_pattern_crosshatch *crosshatch = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_crosshatch));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	crosshatch = &(pattern_info->m.crosshatch);

	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;

	ret = _mtk_drm_pattern_check_color_range(&crosshatch->line_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(&crosshatch->bg_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	data.line.red_integer = crosshatch->line_color.red;
	data.line.green_integer = crosshatch->line_color.green;
	data.line.blue_integer = crosshatch->line_color.blue;
	data.bg.red_integer = crosshatch->bg_color.red;
	data.bg.green_integer = crosshatch->bg_color.green;
	data.bg.blue_integer = crosshatch->bg_color.blue;

	data.h_line_interval = crosshatch->h_line_interval;
	data.v_line_interval = crosshatch->v_line_interval;
	data.h_position_st = crosshatch->h_position_st;
	data.h_line_width = crosshatch->h_line_width;
	data.v_position_st = crosshatch->v_position_st;
	data.v_line_width = crosshatch->v_line_width;

	ret = drv_hwreg_render_pattern_set_crosshatch(riu, &data, pattern_reg, HW_VERSION);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_crosshatch fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_box(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_box data;
	struct drm_pattern_box *box = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_box));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	box = &(pattern_info->m.box);
	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;

	ret = _mtk_drm_pattern_check_color_range(&box->line_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	ret = _mtk_drm_pattern_check_color_range(&box->bg_color);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_check_color_range fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	data.line.red_integer = box->line_color.red;
	data.line.green_integer = box->line_color.green;
	data.line.blue_integer = box->line_color.blue;
	data.bg.red_integer = box->bg_color.red;
	data.bg.green_integer = box->bg_color.green;
	data.bg.blue_integer = box->bg_color.blue;

	data.h_line_interval = box->h_line_interval;
	data.v_line_interval = box->v_line_interval;
	data.h_position_st = box->h_position_st;
	data.h_line_width = box->h_line_width;
	data.v_position_st = box->v_position_st;
	data.v_line_width = box->v_line_width;

	ret = drv_hwreg_render_pattern_set_box(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_box fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_hv_ramp_misc(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_hv_ramp_misc data;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_hv_ramp_misc));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.hsk_force_signal = pattern_info->force_timing.enable;
	data.common.force_h_size = pattern_info->force_timing.force_h_size;
	data.common.force_v_size = pattern_info->force_timing.force_v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;

	ret = drv_hwreg_render_pattern_set_hv_ramp_misc(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_hv_ramp_misc fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_rgb_pure_colorbar(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;
	enum drm_pattern_position position;
	struct hwreg_render_pattern_rgb_pure_colorbar data;
	struct drm_pattern_rgb_pure_colorbar *rgb_pure_colorbar = NULL;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	memset(&data, 0, sizeof(struct hwreg_render_pattern_rgb_pure_colorbar));
	position = pattern_info->position;

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position,position = %u\n", position);
		return -EINVAL;
	}

	rgb_pure_colorbar = &(pattern_info->m.rgb_pure_colorbar);
	data.common.position = (enum hwreg_render_pattern_position)position;
	data.common.enable = true;
	data.common.h_size = pattern_info->h_size;
	data.common.v_size = pattern_info->v_size;
	data.common.hsk_force_signal = pattern_info->force_timing.enable;
	data.common.force_h_size = pattern_info->force_timing.force_h_size;
	data.common.force_v_size = pattern_info->force_timing.force_v_size;
	data.common.color_space =
		(enum hwreg_render_pattern_color_space)pattern_env[position].color_space;
	data.diff_h = rgb_pure_colorbar->colorbar_h_size;
	data.diff_v = rgb_pure_colorbar->colorbar_v_size;

	ret = drv_hwreg_render_pattern_set_rgb_pure_colorbar(riu, &data, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"drv_hwreg_render_pattern_set_rgb_pure_colorbar fail, ret = %d!\n", ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_random(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	ret = drv_hwreg_render_pattern_set_random(riu, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"[drv_hwreg][%s %d] fail, ret = %d!\n",
			__func__, __LINE__, ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_set_chessboard(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;

	ENTER();

	if ((pattern_info == NULL) || (pattern_reg == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO,
		"Set Pattern %s\n", pattern_info->enable ? "ON" : "OFF");

	ret = drv_hwreg_render_pattern_set_chessboard(riu, pattern_reg);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"[drv_hwreg][%s %d] fail, ret = %d!\n",
			__func__, __LINE__, ret);
		return -EINVAL;
	}

	EXIT();
	return ret;
}

static ssize_t _mtk_drm_pattern_show_status(
	char *buf, ssize_t used_count, enum drm_pattern_position position,
	struct mtk_tv_kms_context *drm_ctx)
{
	int ret = 0;
	ssize_t count = 0;
	ssize_t total_count = 0;
	unsigned short h_size = 0;
	unsigned short v_size = 0;

	ENTER();

	if (buf == NULL || drm_ctx == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position(%u)\n", position);
		return 0;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"DRM Pattern Status:\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		" ------------------------------\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"| Enable | Type |  Size(h, v)  |\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		" ------------------------------\n");
	total_count += count;

	if (true) {
		if (position == drm_ctx->pattern_status.pattern_enable_position) {
			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				"|   ON   |");
			total_count += count;

			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				"   %u  |", drm_ctx->pattern_status.pattern_type);
			total_count += count;
		} else {
			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				"|   OFF  |");
			total_count += count;

			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				" NULL |");
			total_count += count;
		}

		ret = _mtk_drm_pattern_get_size(position, &h_size, &v_size);
		if (ret)
			PATTERN_LOG(PATTERN_LOG_ERROR, "Get DRM Pattern Size Fail!\n");

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			" (%04u, %04u) |\n", h_size, v_size);
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			" ------------------------------\n");
		total_count += count;
	} else {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"Window Is Not Opened!\n");
		total_count += count;
	}

	EXIT();
	return total_count;
}

static ssize_t _mtk_drm_pattern_show_capbility(
	char *buf, ssize_t used_count, enum drm_pattern_position position)
{
	int ret = 0;
	int index = 0;
	ssize_t count = 0;
	ssize_t total_count = 0;
	struct drm_pattern_capability cap;

	ENTER();

	if (buf == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position(%u)\n", position);
		return 0;
	}

	memset(&cap, 0, sizeof(struct drm_pattern_capability));

	ret = _mtk_drm_pattern_get_capability(&cap);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Get DRM Pattern Capability Fail!\n");
		return 0;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\nPattery Capability Table:\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		" ---------------------");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"-------");
		total_count += count;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\n|    Position\\Type    |");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"  %2u  |", index);
		total_count += count;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\n ---------------------");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"-------");
		total_count += count;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\n|      %s     |", position_str[position]);
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		if (((cap.pattern_type[position] >> index) & 1)) {
			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				"   O  |");
			total_count += count;
		} else {
			count = scnprintf(buf + total_count + used_count,
				SYSFS_MAX_BUF_COUNT - total_count - used_count,
				"   X  |");
			total_count += count;
		}
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\n ---------------------");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"-------");
		total_count += count;
	}

	EXIT();
	return total_count;
}

static ssize_t _mtk_drm_pattern_show_help(
	char *buf, ssize_t used_count, unsigned int pattern_type)
{
	ssize_t count = 0;
	ssize_t total_count = 0;

	ENTER();

	if (buf == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	switch (pattern_type) {
	case MTK_PATTERN_FLAG_PURE_COLOR:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nPure Color Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r: red color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g: green color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b: blue color value(10 bit)\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_RAMP:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nRame Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"vertical: ramp is vertical or not.[0, 1]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"level: color granularity, should be the nth power of 2\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_PURE_COLORBAR:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nPure Colorbar Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"movable: colorbar is movable or not.[0, 1]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"speed: move a pixel after x frame\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"width: colorbar width\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"height: colorbar height\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_GRADIENT_COLORBAR:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nGradient Colorbar Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue start color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue end color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"vertical: ramp is vertical or not.[0, 1]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"level: color granularity, should be the nth power of 2\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"color_1: color of colorbar 1.[0, 3]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"color_2: color of colorbar 2.[0, 3]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"color_3: color of colorbar 3.[0, 3]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"color_4: color of colorbar 4.[0, 3]\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_BLACK_WHITE_COLORBAR:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nBlack White Colorbar Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red first color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green first color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue first color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red second color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green second color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue second color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"width: colorbar width\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"height: colorbar height\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_CROSS:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nCross Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red cross color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green cross color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue cross color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red border color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green border color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue border color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"position_h: corss center h position\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"position_v: corss center v position\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_PIP_WINDOW:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nPIP Window Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red PIP color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green PIP color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue PIP color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"x: PIP window h start position\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"y: PIP window v start position\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"width: PIP window width.\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"height: PIP window height\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_DOT_MATRIX:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nDot Matrix Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"No parameter need to set\n");
		total_count += count;
		break;
	case MTK_PATTERN_FLAG_CROSSHATCH:
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\nCrosshatch Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_st: red line color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_st: green line color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_st: blue line color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"r_end: red background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"g_end: green background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"b_end: blue background color value(10 bit)\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"h_line_interval: h line interval\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"v_line_interval: v line interval\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"h_line_width: h line width\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"v_line_width: v line width\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"h_position_st: h start position\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"v_position_st: v start position\n");
		total_count += count;
		break;
	default:
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid pattern type = 0x%x!\n", pattern_type);
		break;
	}

	EXIT();
	return total_count;
}

static ssize_t _mtk_drm_pattern_show_pattern_cmd(
	char *buf, ssize_t used_count, unsigned int position)
{
	ssize_t count = 0;
	ssize_t total_count = 0;

	ENTER();

	if (buf == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	if (position == DRM_PATTERN_POSITION_MULTIWIN) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\npattern_multiwin Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"force_timing: force timing enable or not.[0, 1]\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"force_width: force timing width\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"force_height: force timing height\n");
		total_count += count;
	} else if (position == DRM_PATTERN_POSITION_IPBLEND) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"\npattern_ipblend Cmd:\n");
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"force_timing: force timing enable or not.[0, 1]\n");
		total_count += count;
	}

	EXIT();
	return total_count;
}

static ssize_t _mtk_drm_pattern_show_mapping_table(
	char *buf, ssize_t used_count)
{
	int index = 0;
	ssize_t count = 0;
	ssize_t total_count = 0;

	ENTER();

	if (buf == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"\n\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"Pattern Type Mapping Table:\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		" -----------------------------\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		"| Index|     Pattern Type     |\n");
	total_count += count;

	count = scnprintf(buf + total_count + used_count,
		SYSFS_MAX_BUF_COUNT - total_count - used_count,
		" -----------------------------\n");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			"|  %2u  |%s|\n", index, type_str[index]);
		total_count += count;

		count = scnprintf(buf + total_count + used_count,
			SYSFS_MAX_BUF_COUNT - total_count - used_count,
			" -----------------------------\n");
		total_count += count;
	}

	EXIT();
	return total_count;
}

static int _mtk_drm_pattern_set_pattern_info(bool riu,
	struct drm_pattern_info *pattern_info, struct hwregOut *pattern_reg)
{
	int ret = 0;

	switch (pattern_info->pattern_type) {
	case MTK_PATTERN_FLAG_PURE_COLOR:
		ret = _mtk_drm_pattern_set_pure_color(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_pure_color fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_RAMP:
		ret = _mtk_drm_pattern_set_ramp(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_ramp fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_PURE_COLORBAR:
		ret = _mtk_drm_pattern_set_pure_colorbar(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_pure_colorbar fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_GRADIENT_COLORBAR:
		ret = _mtk_drm_pattern_set_gradient_colorbar(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_gradient_colorbar fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_BLACK_WHITE_COLORBAR:
		ret = _mtk_drm_pattern_set_black_white_colorbar(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_black_white_colorbar fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_CROSS:
		ret = _mtk_drm_pattern_set_cross(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_cross fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_PIP_WINDOW:
		ret = _mtk_drm_pattern_set_pip_window(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_pip_window fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_CROSSHATCH:
		ret = _mtk_drm_pattern_set_crosshatch(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_crosshatch fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_BOX:
		ret = _mtk_drm_pattern_set_box(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_box fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_HV_RAMP_MISC:
		ret = _mtk_drm_pattern_set_hv_ramp_misc(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_hv_ramp_misc fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_RGB_PURE_COLORBAR:
		ret = _mtk_drm_pattern_set_rgb_pure_colorbar(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_rgb_pure_colorbar fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_RANDOM:
		ret = _mtk_drm_pattern_set_random(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_random fail, ret = %d!\n",
				ret);
		}
		break;
	case MTK_PATTERN_FLAG_CHESSBOARD:
		ret = _mtk_drm_pattern_set_chessboard(riu,
			pattern_info, pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_chessboard fail, ret = %d!\n",
				ret);
		}
		break;
	default:
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Pattern type not support, pattern_type = 0x%x!\n",
			pattern_info->pattern_type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int _mtk_drm_pattern_set_info(struct drm_pattern_info *pattern_info)
{
	int ret = 0;
	bool riu = true;
	struct hwregOut pattern_reg;
	enum hwreg_render_pattern_position position;

	ENTER();

	memset(&pattern_reg, 0, sizeof(struct hwregOut));

	if (pattern_info == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PATTERN_LOG(PATTERN_LOG_INFO, "riu : %s!\n", riu ? "ON" : "OFF");

	pattern_reg.reg = vmalloc(sizeof(struct reg_info) * PATTERN_REG_NUM_MAX);
	if (pattern_reg.reg == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "vmalloc pattern reg fail!\n");
		return -EINVAL;
	}

	for (position = HWREG_RENDER_PATTERN_POSITION_OPDRAM;
		position < HWREG_RENDER_PATTERN_POSITION_MAX; position++) {

		if (!g_capability[position]) {
			PATTERN_LOG(PATTERN_LOG_INFO,
				"Pattern type not support, position = %d!\n", position);
			continue;
		}

		ret = _mtk_drm_pattern_set_off(riu, position, &pattern_reg);

		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_off fail, position = %u, ret = %d!\n",
				position, ret);
			goto exit;
		}

		pattern_reg.regCount = 0;
	}

	if (pattern_info->enable) {
		if ((uint32_t)pattern_info->position >= DRM_PATTERN_POSITION_MAX) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"Invalid position, position = %u\n", pattern_info->position);
			ret = -EINVAL;
			goto exit;
		}

		if (!(pattern_info->pattern_type & g_capability[pattern_info->position])) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"pattern type not support, please check it!\n");
			ret = -EINVAL;
			goto exit;
		}

		ret = _mtk_drm_pattern_set_pattern_info(riu, pattern_info, &pattern_reg);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR,
				"_mtk_drm_pattern_set_pattern_info fail, ret = %d!\n",
				ret);
			goto exit;
		}
	} else
		PATTERN_LOG(PATTERN_LOG_INFO, "Set Pattern OFF\n");

exit:
	vfree(pattern_reg.reg);

	EXIT();
	return ret;
}

static int _mtk_drm_pattern_fill_type(char *cmd, unsigned int len,
	struct drm_pattern_info *pattern_info, int type)
{
	int ret = 0;

	if ((cmd == NULL) || (pattern_info == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"NULL command or pattern info\n");
		return -EINVAL;
	}

	switch (type) {
	case E_MTK_DRM_PATTERN_TYPE_FILL_PURE_COLOR:
		ret = _mtk_drm_pattern_fill_pure_color(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_RAMP:
		ret = _mtk_drm_pattern_fill_ramp(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_PURE_COLORBAR:
		/* For test item, keep to set default H/V value. */
		if (pattern_info->position == DRM_PATTERN_POSITION_MULTIWIN) {
			if (HW_VERSION == PAT_HWVER_3) {
				pattern_info->h_size = DEFAULT_VALUE_3840;
				pattern_info->v_size = DEFAULT_VALUE_2160;
			} else {
				pattern_info->h_size = DEFAULT_VALUE_7680;
				pattern_info->v_size = DEFAULT_VALUE_4320;
			}
		}
		ret = _mtk_drm_pattern_fill_pure_colorbar(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_GRADIENT_COLORBAR:
		ret = _mtk_drm_pattern_fill_gradient_colorbar(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_BLACK_WHITE_COLORBAR:
		ret = _mtk_drm_pattern_fill_black_white_colorbar(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_CROSS:
		ret = _mtk_drm_pattern_fill_cross(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_PIP_WINDOW:
		ret = _mtk_drm_pattern_fill_pip_window(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_CROSSHATCH:
		ret = _mtk_drm_pattern_fill_crosshatch(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_FLAG_RANDOM:
		pattern_info->pattern_type = MTK_PATTERN_FLAG_RANDOM;
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_FLAG_CHESSBOARD:
		pattern_info->pattern_type = MTK_PATTERN_FLAG_CHESSBOARD;
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_BOX:
		ret = _mtk_drm_pattern_fill_box(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_HV_RAMP_MISC:
		ret = _mtk_drm_pattern_fill_hv_ramp_misc(cmd, len, pattern_info);
		break;
	case E_MTK_DRM_PATTERN_TYPE_FILL_RGB_PURE_COLORBAR:
		ret = _mtk_drm_pattern_fill_rgb_pure_colorbar(cmd, len, pattern_info);
		break;
	default:
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Pattern Type::%d Not Support!\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int mtk_drm_pattern_set_env(
	enum drm_pattern_position position,
	struct drm_pattern_env *env)
{
	int ret = 0;

	ENTER();

	if (env == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"Invalid position, position = %u\n", position);
		return -EINVAL;
	}

	pattern_env[position].h_size = env->h_size;
	pattern_env[position].v_size = env->v_size;
	pattern_env[position].color_space = env->color_space;

	PATTERN_LOG(PATTERN_LOG_INFO, "h = 0x%x, v = 0x%x, color space = %u\n",
		env->h_size, env->v_size, env->color_space);

	EXIT();
	return ret;
}

ssize_t mtk_drm_pattern_show(char *buf,
	struct device *dev, enum drm_pattern_position position)
{
	int index;
	int ret = 0;
	ssize_t count = 0;
	ssize_t total_count = 0;
	struct drm_pattern_capability cap;
	struct mtk_tv_kms_context *drm_ctx = NULL;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_ver = NULL;

	ENTER();

	if ((buf == NULL) || (dev == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position(%u)\n", position);
		return -EINVAL;
	}

	memset(&cap, 0, sizeof(struct drm_pattern_capability));

	drm_ctx = dev_get_drvdata(dev);
	if (!drm_ctx) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "drm_ctx is NULL!\n");
		return -EINVAL;
	}

	pctx_video = &(drm_ctx->video_priv);
	if (!pctx_video) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "pctx_video is NULL!\n");
		return -EINVAL;
	}
	video_hw_ver = &(pctx_video->video_hw_ver);
	if (!video_hw_ver) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "video_hw_ver is NULL!\n");
		return -EINVAL;
	}
	HW_VERSION = video_hw_ver->pattern;

	pctx_pnl = &(drm_ctx->panel_priv);
	if (!pctx_pnl) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "pctx_pnl is NULL!\n");
		return -EINVAL;
	}

#ifdef PATTERN_TEST
	if (HW_VERSION == PAT_HWVER_3) {
		pattern_env[position].h_size = DEFAULT_VALUE_3840;
		pattern_env[position].v_size = DEFAULT_VALUE_2160;
	} else {
		pattern_env[position].h_size = DEFAULT_VALUE_7680;
		pattern_env[position].v_size = DEFAULT_VALUE_4320;
	}
#else
	pattern_env[position].h_size = pctx_pnl->info.de_width;
	pattern_env[position].v_size = pctx_pnl->info.de_height;
#endif


	ret = _mtk_drm_pattern_get_capability(&cap);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Get DRM Pattern Capability Fail!\n");
		return -EINVAL;
	}

	count = scnprintf(buf + total_count, SYSFS_MAX_BUF_COUNT - total_count,
		"Position: %s\n\n", position_str[position]);
	total_count += count;

	count = _mtk_drm_pattern_show_status(buf, total_count, position, drm_ctx);
	total_count += count;

	count = _mtk_drm_pattern_show_capbility(buf, total_count, position);
	total_count += count;

	count = scnprintf(buf + total_count, SYSFS_MAX_BUF_COUNT - total_count,
		"\nCommon Cmd:\n");
	total_count += count;

	count = scnprintf(buf + total_count, SYSFS_MAX_BUF_COUNT - total_count,
		"enable: echo enable=1 type=1 > pattern node\n");
	total_count += count;

	count = scnprintf(buf + total_count, SYSFS_MAX_BUF_COUNT - total_count,
		"disable: echo enable=0 > pattern node\n");
	total_count += count;

	count = scnprintf(buf + total_count, SYSFS_MAX_BUF_COUNT - total_count,
		"\nOptional Cmd:\n");
	total_count += count;

	for (index = 0; index < PATTERN_NUM; index++) {
		if (((cap.pattern_type[position] >> index) & 1)) {
			count = _mtk_drm_pattern_show_help(buf, total_count, BIT(index));
			total_count += count;
		}
	}

	count = _mtk_drm_pattern_show_pattern_cmd(buf, total_count, position);
	total_count += count;

	count = _mtk_drm_pattern_show_mapping_table(buf, total_count);
	total_count += count;

	EXIT();

	PATTERN_LOG(PATTERN_LOG_INFO, "count = %zd!\n", total_count);

	return total_count;
}

int mtk_drm_pattern_store(const char *buf,
	struct device *dev, enum drm_pattern_position position)
{
	int ret = 0;
	int len = 0;
	int type = 0;
	char *cmd = NULL;
	struct drm_pattern_info pattern_info;
	struct mtk_tv_kms_context *drm_ctx = NULL;
	struct drm_pattern_capability cap;
	struct mtk_panel_context *pctx_pnl = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_ver = NULL;

	ENTER();

	if ((buf == NULL) || (dev == NULL)) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	if (position >= DRM_PATTERN_POSITION_MAX || position < 0) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "Invalid position(%u)\n", position);
		return -EINVAL;
	}

	drm_ctx = dev_get_drvdata(dev);
	if (!drm_ctx) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "drm_ctx is NULL!\n");
		return -EINVAL;
	}

	pctx_video = &(drm_ctx->video_priv);
	if (!pctx_video) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "pctx_video is NULL!\n");
		return -EINVAL;
	}
	video_hw_ver = &(pctx_video->video_hw_ver);
	if (!video_hw_ver) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "video_hw_ver is NULL!\n");
		return -EINVAL;
	}
	HW_VERSION = video_hw_ver->pattern;

	pctx_pnl = &(drm_ctx->panel_priv);
	if (!pctx_pnl) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "pctx_pnl is NULL!\n");
		return -EINVAL;
	}

	len = strlen(buf);
	cmd = kvmalloc(len + 1, GFP_KERNEL);
	if (cmd == NULL) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "vmalloc cmd fail!\n");
		return -EINVAL;
	}
	memset(cmd, 0, len + 1);
	memset(&pattern_info, 0, sizeof(struct drm_pattern_info));
	memset(&cap, 0, sizeof(struct drm_pattern_capability));

#ifdef PATTERN_TEST
	if (HW_VERSION == PAT_HWVER_3) {
		pattern_env[position].h_size = DEFAULT_VALUE_3840;
		pattern_env[position].v_size = DEFAULT_VALUE_2160;
	} else {
		pattern_env[position].h_size = DEFAULT_VALUE_7680;
		pattern_env[position].v_size = DEFAULT_VALUE_4320;
	}
#else
	pattern_env[position].h_size = pctx_pnl->info.de_width;
	pattern_env[position].v_size = pctx_pnl->info.de_height;
#endif


	memcpy(cmd, buf, len);
	pattern_info.position = position;

	ret = _mtk_drm_pattern_fill_common_info(cmd, len, &pattern_info);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_fill_common_info fail, ret = %d!\n", ret);
		goto exit;
	}

	if ((pattern_info.force_timing.force_h_size == 0) ||
		(pattern_info.force_timing.force_v_size == 0)) {
		pattern_info.force_timing.force_h_size = pctx_pnl->info.de_width;
		pattern_info.force_timing.force_v_size = pctx_pnl->info.de_height;
	}

	PATTERN_LOG(PATTERN_LOG_INFO, "Force H/V=%u/%u, DE H/V=%u/%u\n",
		pattern_info.force_timing.force_h_size, pattern_info.force_timing.force_v_size,
		pctx_pnl->info.de_width, pctx_pnl->info.de_height);

	if (!pattern_info.enable) {
		drm_ctx->pattern_status.pattern_enable_position = INVALID_PATTERN_POSITION;
	} else {
		ret = _mtk_drm_pattern_get_capability(&cap);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR, "Fail get capability, ret=%d\n", ret);
			ret = -EINVAL;
			goto exit;
		}

		ret = _mtk_drm_pattern_check_capability_get_type(&cap, position, cmd, len, &type);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR, "Fail check capability, ret=%d\n", ret);
			goto exit;
		}

		ret = _mtk_drm_pattern_fill_type(cmd, len, &pattern_info, type);
		if (ret) {
			PATTERN_LOG(PATTERN_LOG_ERROR, "Fail fill type, ret=%d\n", ret);
			ret = -EINVAL;
			goto exit;
		}

		drm_ctx->pattern_status.pattern_enable_position = position;
		drm_ctx->pattern_status.pattern_type = type;
	}

	memcpy(&drm_ctx->pattern_info, &pattern_info, sizeof(struct drm_pattern_info));
	ret = _mtk_drm_pattern_set_info(&pattern_info);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_set_info fail, ret = %d!\n", ret);
		goto exit;
	}

exit:
	kvfree(cmd);

	EXIT();
	return ret;
}

int mtk_drm_pattern_set_param(
	struct drm_pattern_status *pattern,
	struct drm_mtk_pattern_config *config)
{
	struct drm_pattern_info pattern_info;
	struct drm_pattern_capability cap;
	struct drm_pattern_env env;
	unsigned short h_size = 0;
	unsigned short v_size = 0;
	int ret = 0;

	ENTER();

	if ((pattern == NULL) || (config == NULL)) {
		ERR("Pointer is NULL!\n");
		return -EINVAL;
	}
	ret = _mtk_drm_pattern_get_capability(&cap);
	if (ret) {
		ERR("pattern get capability fail, ret = %d\n", ret);
		return ret;
	}
	ret = _mtk_drm_pattern_get_size((enum drm_pattern_position)config->pattern_position,
			&h_size, &v_size);
	if (ret) {
		ERR("pattern get size fail, ret = %d!\n", ret);
		return ret;
	}
	if (h_size == 0 || v_size == 0) { // invalid H/V, set default value
		env.h_size = h_size = UHD_8K_W;
		env.v_size = v_size = UHD_8K_H;
		env.color_space = DRM_PATTERN_COLOR_SPACE_RGB;
		ret = mtk_drm_pattern_set_env((enum drm_pattern_position)config->pattern_position,
				&env);
		if (ret) {
			ERR("pattern set env fail, ret = %d\n", ret);
			return ret;
		}
	}

	memset(&pattern_info, 0, sizeof(struct drm_pattern_info));
	if (config->pattern_type < MTK_PATTERN_MAX) {
		pattern->pattern_enable_position = config->pattern_position;
		pattern_info.position = (enum drm_pattern_position)config->pattern_position;
		pattern_info.enable = true;
		pattern_info.h_size = h_size;
		pattern_info.v_size = v_size;

		switch (config->pattern_type) {
		case MTK_PATTERN_PURE_COLOR: {
			struct drm_pattern_pure_color *pure_color = &pattern_info.m.pure_color;
			struct drm_mtk_pattern_pure_color *mtk_pure_color = &config->u.pure_color;

			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_PURE_COLOR;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_PURE_COLOR;
			pure_color->color.red   = mtk_pure_color->color.red;
			pure_color->color.green = mtk_pure_color->color.green;
			pure_color->color.blue  = mtk_pure_color->color.blue;
			LOG("MTK_PATTERN_PURE_COLOR");
			LOG("  red = %u", pure_color->color.red);
			LOG("  red = %u", pure_color->color.green);
			LOG("  red = %u", pure_color->color.blue);
			break;
		}

		case MTK_PATTERN_PURE_COLOR_BAR: {
			struct drm_pattern_pure_color *pure_color = &pattern_info.m.pure_color;
			struct drm_mtk_pattern_pure_color *mtk_pure_color = &config->u.pure_color;

			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_PURE_COLORBAR;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_PURE_COLORBAR;
			pure_color->color.red   = mtk_pure_color->color.red;
			pure_color->color.green = mtk_pure_color->color.green;
			pure_color->color.blue  = mtk_pure_color->color.blue;
			LOG("MTK_PATTERN_PURE_COLOR_BAR");
			LOG("  red = %u", pure_color->color.red);
			LOG("  red = %u", pure_color->color.green);
			LOG("  red = %u", pure_color->color.blue);
			break;
		}
		case MTK_PATTERN_RAMP: {
			struct drm_pattern_ramp *ramp = &pattern_info.m.ramp;
			struct drm_mtk_pattern_ramp *mtk_ramp = &config->u.ramp;

			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_RAMP;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_RAMP;
			ramp->start.red   = mtk_ramp->color_begin.red;
			ramp->start.green = mtk_ramp->color_begin.green;
			ramp->start.blue  = mtk_ramp->color_begin.blue;
			ramp->end.red     = mtk_ramp->color_end.red;
			ramp->end.green   = mtk_ramp->color_end.green;
			ramp->end.blue    = mtk_ramp->color_end.blue;
			ramp->b_vertical  = mtk_ramp->vertical;
			ramp->diff_level  = mtk_ramp->level;
			LOG("MTK_PATTERN_RAMP");
			LOG("  start red   = %u", ramp->start.red);
			LOG("  start green = %u", ramp->start.green);
			LOG("  start blue  = %u", ramp->start.blue);
			LOG("  end red     = %u", ramp->end.red);
			LOG("  end green   = %u", ramp->end.green);
			LOG("  end blue    = %u", ramp->end.blue);
			LOG("  vertical    = %u", ramp->b_vertical);
			LOG("  level       = %u", ramp->diff_level);
			break;
		}
		case MTK_PATTERN_PIP_WINDOW: {
			struct drm_pattern_pip_window *pip_window = &pattern_info.m.pip_window;
			struct drm_mtk_pattern_pip_window *mtk_pip_window = &config->u.pip_window;

			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_PIP_WINDOW;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_PIP_WINDOW;
			pip_window->pip_color.red   = mtk_pip_window->window_color.red;
			pip_window->pip_color.green = mtk_pip_window->window_color.green;
			pip_window->pip_color.blue  = mtk_pip_window->window_color.blue;
			pip_window->bg_color.red    = mtk_pip_window->background_color.red;
			pip_window->bg_color.green  = mtk_pip_window->background_color.green;
			pip_window->bg_color.blue   = mtk_pip_window->background_color.blue;
			pip_window->window.x        = mtk_pip_window->window_x;
			pip_window->window.y        = mtk_pip_window->window_y;
			pip_window->window.width    = mtk_pip_window->window_width;
			pip_window->window.height   = mtk_pip_window->window_height;
			LOG("MTK_PATTERN_PIP_WINDOW");
			LOG("  pip red   = %u", pip_window->pip_color.red);
			LOG("  pip green = %u", pip_window->pip_color.green);
			LOG("  pip blue  = %u", pip_window->pip_color.blue);
			LOG("  bg red    = %u", pip_window->bg_color.red);
			LOG("  bg green  = %u", pip_window->bg_color.green);
			LOG("  bg blue   = %u", pip_window->bg_color.blue);
			LOG("  window x  = %u", pip_window->window.x);
			LOG("  window y  = %u", pip_window->window.y);
			LOG("  window w  = %u", pip_window->window.width);
			LOG("  window h  = %u", pip_window->window.height);
			break;
		}
		case MTK_PATTERN_CROSSHATCH: {
			struct drm_pattern_crosshatch *crosshatch = &pattern_info.m.crosshatch;
			struct drm_mtk_pattern_crosshatch *mtk_crosshatch = &config->u.crosshatch;

			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_CROSSHATCH;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_CROSSHATCH;
			crosshatch->line_color.red   = mtk_crosshatch->line_color.red;
			crosshatch->line_color.green = mtk_crosshatch->line_color.green;
			crosshatch->line_color.blue  = mtk_crosshatch->line_color.blue;
			crosshatch->bg_color.red     = mtk_crosshatch->background_color.red;
			crosshatch->bg_color.green   = mtk_crosshatch->background_color.green;
			crosshatch->bg_color.blue    = mtk_crosshatch->background_color.blue;
			crosshatch->h_position_st    = mtk_crosshatch->line_start_y;
			crosshatch->v_position_st    = mtk_crosshatch->line_start_x;
			crosshatch->h_line_width     = mtk_crosshatch->line_width_h;
			crosshatch->v_line_width     = mtk_crosshatch->line_width_v;
			crosshatch->h_line_interval  = mtk_crosshatch->line_interval_h;
			crosshatch->v_line_interval  = mtk_crosshatch->line_interval_v;
			LOG("MTK_PATTERN_CROSSHATCH");
			LOG("  line red   = %u", crosshatch->line_color.red);
			LOG("  line green = %u", crosshatch->line_color.green);
			LOG("  line blue  = %u", crosshatch->line_color.blue);
			LOG("  bg red     = %u", crosshatch->bg_color.red);
			LOG("  bg green   = %u", crosshatch->bg_color.green);
			LOG("  bg blue    = %u", crosshatch->bg_color.blue);
			LOG("  start y    = %u", crosshatch->h_position_st);
			LOG("  start x    = %u", crosshatch->v_position_st);
			LOG("  width h    = %u", crosshatch->h_line_width);
			LOG("  width v    = %u", crosshatch->v_line_width);
			LOG("  interval h = %u", crosshatch->h_line_interval);
			LOG("  interval v = %u", crosshatch->v_line_interval);
			break;
		}
		case MTK_PATTERN_RADOM: {
			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_RANDOM;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_RANDOM;
			LOG("MTK_PATTERN_RADOM");
			break;
		}
		case MTK_PATTERN_CHESSBOARD: {
			pattern->pattern_type = MTK_PATTERN_BIT_INDEX_CHESSBOARD;
			pattern_info.pattern_type = MTK_PATTERN_FLAG_CHESSBOARD;
			LOG("MTK_PATTERN_CHESSBOARD");
			break;
		}
		default:
			return -EINVAL;
		}
	} else {
		pattern->pattern_enable_position = INVALID_PATTERN_POSITION;
	}
	ret = _mtk_drm_pattern_set_info(&pattern_info);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "_mtk_drm_pattern_set_info fail, ret = %d!\n", ret);
		return ret;
	}
	EXIT();
	return ret;
}

static ssize_t pattern_hw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct mtk_tv_kms_context *drm_ctx = NULL;
	struct mtk_video_context *pctx_video = NULL;
	struct mtk_video_hw_version *video_hw_ver = NULL;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	drm_ctx = dev_get_drvdata(dev);
	if (!drm_ctx) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "drm_ctx is NULL!\n");
		return -EINVAL;
	}

	pctx_video = &(drm_ctx->video_priv);
	if (!pctx_video) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "pctx_video is NULL!\n");
		return -EINVAL;
	}
	video_hw_ver = &(pctx_video->video_hw_ver);
	if (!video_hw_ver) {
		PATTERN_LOG(PATTERN_LOG_ERROR, "video_hw_ver is NULL!\n");
		return -EINVAL;
	}

	count = scnprintf(buf, SYSFS_MAX_BUF_COUNT, "%u\n", video_hw_ver->pattern);

	return count;
}

static ssize_t pattern_tcon_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_TCON;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_tcon_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_TCON;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_gfx_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_GFX;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_gfx_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_GFX;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_multiwin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_MULTIWIN;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_multiwin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_MULTIWIN;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_opdram_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_OPDRAM;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_opdram_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_OPDRAM;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_ipblend_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_IPBLEND;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_ipblend_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_IPBLEND;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_pafrc_post_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_PAFRC_POST;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_pafrc_post_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_PAFRC_POST;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_sec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	enum drm_pattern_position position = DRM_PATTERN_POSITION_SEC;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_sec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_SEC;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t pattern_pq_gamma_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_PQ_GAMMA;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ret = mtk_drm_pattern_store(buf, dev, position);
	if (ret) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_store failed!\n",
			__func__, __LINE__);
		return ret;
	}

	return count;
}

static ssize_t pattern_pq_gamma_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	enum drm_pattern_position position = DRM_PATTERN_POSITION_PQ_GAMMA;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n",
			__func__, __LINE__);
		return 0;
	}

	count = mtk_drm_pattern_show(buf, dev, position);
	if (count < 0) {
		DRM_ERROR("[%s, %d]: mtk_drm_pattern_show failed!\n",
			__func__, __LINE__);
		return count;
	}

	return count;
}

static ssize_t panel_resume_no_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_DEFAULT;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_mod_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_MOD;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_tcon_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_TCON;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_gop_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_GOP;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_multiwin_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_MULTIWIN;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_pafrc_post_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_PAFRC_POST;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_sec_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_SEC;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static ssize_t panel_resume_pq_gamma_pattern_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mtk_tv_kms_context *pctx = NULL;
	int ret = 0;

	if ((dev == NULL) || (buf == NULL)) {
		DRM_ERROR("[%s, %d]: Pointer is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}
	pctx = dev_get_drvdata(dev);

	pctx->eResumePatternselect = E_RESUME_PATTERN_SEL_PQ_GAMMA;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", pctx->eResumePatternselect);

	return ((ret < 0) || (ret > PAGE_SIZE)) ? -EINVAL : ret;
}

static DEVICE_ATTR_RO(pattern_hw_ver);
static DEVICE_ATTR_RW(pattern_multiwin);
static DEVICE_ATTR_RW(pattern_tcon);
static DEVICE_ATTR_RW(pattern_gfx);
static DEVICE_ATTR_RW(pattern_opdram);
static DEVICE_ATTR_RW(pattern_ipblend);
static DEVICE_ATTR_RW(pattern_pafrc_post);
static DEVICE_ATTR_RW(pattern_sec);
static DEVICE_ATTR_RW(pattern_pq_gamma);
static DEVICE_ATTR_RO(panel_resume_no_pattern);
static DEVICE_ATTR_RO(panel_resume_mod_pattern);
static DEVICE_ATTR_RO(panel_resume_tcon_pattern);
static DEVICE_ATTR_RO(panel_resume_gop_pattern);
static DEVICE_ATTR_RO(panel_resume_multiwin_pattern);
static DEVICE_ATTR_RO(panel_resume_pafrc_post_pattern);
static DEVICE_ATTR_RO(panel_resume_sec_pattern);
static DEVICE_ATTR_RO(panel_resume_pq_gamma_pattern);

static struct attribute *mtk_drm_pattern_attrs[] = {
	&dev_attr_pattern_hw_ver.attr,
	&dev_attr_pattern_multiwin.attr,
	&dev_attr_pattern_tcon.attr,
	&dev_attr_pattern_gfx.attr,
	&dev_attr_pattern_opdram.attr,
	&dev_attr_pattern_ipblend.attr,
	&dev_attr_pattern_pafrc_post.attr,
	&dev_attr_pattern_sec.attr,
	&dev_attr_pattern_pq_gamma.attr,
	&dev_attr_panel_resume_no_pattern.attr,
	&dev_attr_panel_resume_mod_pattern.attr,
	&dev_attr_panel_resume_tcon_pattern.attr,
	&dev_attr_panel_resume_gop_pattern.attr,
	&dev_attr_panel_resume_multiwin_pattern.attr,
	&dev_attr_panel_resume_pafrc_post_pattern.attr,
	&dev_attr_panel_resume_sec_pattern.attr,
	&dev_attr_panel_resume_pq_gamma_pattern.attr,
	NULL
};

static const struct attribute_group mtk_drm_pattern_group = {
	.name = "mtk_dbg",
	.attrs = mtk_drm_pattern_attrs
};

void mtk_drm_pattern_create_sysfs(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_update_group(&pdev->dev.kobj, &mtk_drm_pattern_group);
	if (ret) {
		dev_err(&pdev->dev, "[%s, %d]Fail to create sysfs files: %d\n",
			__func__, __LINE__, ret);
	}
}

int mtk_drm_pattern_resume(struct drm_pattern_info *pattern_info)
{
	int ret = 0;

	ret = _mtk_drm_pattern_set_info(pattern_info);
	if (ret) {
		PATTERN_LOG(PATTERN_LOG_ERROR,
			"_mtk_drm_pattern_set_info fail, ret = %d!\n", ret);
	}
	return ret;
}
