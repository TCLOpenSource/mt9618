/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef _MTK_TV_DRM_PATTERN
#define _MTK_TV_DRM_PATTERN
//-------------------------------------------------------------------------------------------------
//  Driver Capability
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
//  Macro and Define
//-------------------------------------------------------------------------------------------------
#define MTK_PATTERN_BIT_INDEX_PURE_COLOR			(0)
#define MTK_PATTERN_BIT_INDEX_RAMP				(1)
#define MTK_PATTERN_BIT_INDEX_PURE_COLORBAR			(2)
#define MTK_PATTERN_BIT_INDEX_GRADIENT_COLORBAR			(3)
#define MTK_PATTERN_BIT_INDEX_BLACK_WHITE_COLORBAR		(4)
#define MTK_PATTERN_BIT_INDEX_CROSS				(5)
#define MTK_PATTERN_BIT_INDEX_PIP_WINDOW			(6)
#define MTK_PATTERN_BIT_INDEX_DOT_MATRIX			(7)
#define MTK_PATTERN_BIT_INDEX_CROSSHATCH			(8)
#define MTK_PATTERN_BIT_INDEX_RANDOM				(9)
#define MTK_PATTERN_BIT_INDEX_CHESSBOARD			(10)
#define MTK_PATTERN_BIT_INDEX_BOX					(11)
#define MTK_PATTERN_BIT_INDEX_HV_RAMP_MISC			(12)
#define MTK_PATTERN_BIT_INDEX_RGB_PURE_COLORBAR		(13)

#ifndef BIT
#define BIT(_bit_)				(1 << (_bit_))
#endif

#ifndef MTK_PATTERN_FLAG_PURE_COLOR
#define MTK_PATTERN_FLAG_PURE_COLOR		(BIT(MTK_PATTERN_BIT_INDEX_PURE_COLOR))
#endif

#ifndef MTK_PATTERN_FLAG_RAMP
#define MTK_PATTERN_FLAG_RAMP			(BIT(MTK_PATTERN_BIT_INDEX_RAMP))
#endif

#ifndef MTK_PATTERN_FLAG_PURE_COLORBAR
#define MTK_PATTERN_FLAG_PURE_COLORBAR		(BIT(MTK_PATTERN_BIT_INDEX_PURE_COLORBAR))
#endif

#ifndef MTK_PATTERN_FLAG_GRADIENT_COLORBAR
#define MTK_PATTERN_FLAG_GRADIENT_COLORBAR	(BIT(MTK_PATTERN_BIT_INDEX_GRADIENT_COLORBAR))
#endif

#ifndef MTK_PATTERN_FLAG_BLACK_WHITE_COLORBAR
#define MTK_PATTERN_FLAG_BLACK_WHITE_COLORBAR	(BIT(MTK_PATTERN_BIT_INDEX_BLACK_WHITE_COLORBAR))
#endif

#ifndef MTK_PATTERN_FLAG_CROSS
#define MTK_PATTERN_FLAG_CROSS			(BIT(MTK_PATTERN_BIT_INDEX_CROSS))
#endif

#ifndef MTK_PATTERN_FLAG_PIP_WINDOW
#define MTK_PATTERN_FLAG_PIP_WINDOW		(BIT(MTK_PATTERN_BIT_INDEX_PIP_WINDOW))
#endif

#ifndef MTK_PATTERN_FLAG_DOT_MATRIX
#define MTK_PATTERN_FLAG_DOT_MATRIX		(BIT(MTK_PATTERN_BIT_INDEX_DOT_MATRIX))
#endif

#ifndef MTK_PATTERN_FLAG_CROSSHATCH
#define MTK_PATTERN_FLAG_CROSSHATCH		(BIT(MTK_PATTERN_BIT_INDEX_CROSSHATCH))
#endif

#ifndef MTK_PATTERN_FLAG_RANDOM
#define MTK_PATTERN_FLAG_RANDOM			(BIT(MTK_PATTERN_BIT_INDEX_RANDOM))
#endif

#ifndef MTK_PATTERN_FLAG_CHESSBOARD
#define MTK_PATTERN_FLAG_CHESSBOARD		(BIT(MTK_PATTERN_BIT_INDEX_CHESSBOARD))
#endif

#ifndef MTK_PATTERN_FLAG_BOX
#define MTK_PATTERN_FLAG_BOX		(BIT(MTK_PATTERN_BIT_INDEX_BOX))
#endif

#ifndef MTK_PATTERN_FLAG_HV_RAMP_MISC
#define MTK_PATTERN_FLAG_HV_RAMP_MISC		(BIT(MTK_PATTERN_BIT_INDEX_HV_RAMP_MISC))
#endif

#ifndef MTK_PATTERN_FLAG_RGB_PURE_COLORBAR
#define MTK_PATTERN_FLAG_RGB_PURE_COLORBAR	(BIT(MTK_PATTERN_BIT_INDEX_RGB_PURE_COLORBAR))
#endif

#define MIN_DIFF_LEVEL				(4)

#define SRC_1	(1)
#define SRC_2	(2)
#define SRC_3	(3)

//-------------------------------------------------------------------------------------------------
//  enums
//-------------------------------------------------------------------------------------------------
enum drm_pattern_position {
	DRM_PATTERN_POSITION_OPDRAM,
	DRM_PATTERN_POSITION_IPBLEND,
	DRM_PATTERN_POSITION_MULTIWIN,
	DRM_PATTERN_POSITION_PQ_GAMMA,
	DRM_PATTERN_POSITION_TCON,
	DRM_PATTERN_POSITION_GFX,
	DRM_PATTERN_POSITION_PAFRC_POST,
	DRM_PATTERN_POSITION_SEC,
	DRM_PATTERN_POSITION_MAX,
};

enum drm_pattern_color_space {
	DRM_PATTERN_COLOR_SPACE_RGB,
	DRM_PATTERN_COLOR_SPACE_YUV_FULL,
	DRM_PATTERN_COLOR_SPACE_YUV_LIMITED,
	DRM_PATTERN_COLOR_SPACE_MAX,
};

enum drm_pattern_colorbar_color {
	DRM_PATTERN_COLOR_WHITE,
	DRM_PATTERN_COLOR_BLUE,
	DRM_PATTERN_COLOR_GREEN,
	DRM_PATTERN_COLOR_RED,
	DRM_PATTERN_COLOR_MAX,
};

//-------------------------------------------------------------------------------------------------
//  structs
//-------------------------------------------------------------------------------------------------
struct drm_pattern_status {
	unsigned char pattern_enable_position;
	unsigned char pattern_type;
};

struct drm_pattern_env {
	unsigned short h_size;
	unsigned short v_size;
	enum drm_pattern_color_space color_space;
};

struct drm_pattern_color {
	unsigned short red;		//10 bits
	unsigned short green;	//10 bits
	unsigned short blue;	//10 bits
};

struct drm_pattern_window {
	unsigned short x;
	unsigned short y;
	unsigned short width;
	unsigned short height;
};

struct drm_pattern_pure_color {
	struct drm_pattern_color color;
};

struct drm_pattern_ramp {
	struct drm_pattern_color start;
	struct drm_pattern_color end;
	bool b_vertical;
	unsigned short diff_level;
};

struct drm_pattern_pure_colorbar {
	unsigned int colorbar_h_size;
	unsigned int colorbar_v_size;
	unsigned short shift_speed;
	bool movable;
};

struct drm_pattern_gradient_colorbar {
	struct drm_pattern_color start;
	struct drm_pattern_color end;
	enum drm_pattern_colorbar_color color_1st;
	enum drm_pattern_colorbar_color color_2nd;
	enum drm_pattern_colorbar_color color_3rd;
	enum drm_pattern_colorbar_color color_4th;
	unsigned short diff_level;
	bool b_vertical;
};

struct drm_pattern_black_white_colorbar {
	struct drm_pattern_color first_color;
	struct drm_pattern_color second_color;
	unsigned int colorbar_h_size;
	unsigned int colorbar_v_size;
};

struct drm_pattern_cross {
	struct drm_pattern_color cross_color;
	struct drm_pattern_color border_color;
	unsigned short h_position;
	unsigned short v_position;
};

struct drm_pattern_pip_window {
	struct drm_pattern_window window;
	struct drm_pattern_color pip_color;
	struct drm_pattern_color bg_color;
};

struct drm_pattern_crosshatch {
	struct drm_pattern_color line_color;
	struct drm_pattern_color bg_color;
	unsigned int h_line_interval;
	unsigned int v_line_interval;
	unsigned short h_position_st;
	unsigned short h_line_width;
	unsigned short v_position_st;
	unsigned short v_line_width;
};

struct drm_pattern_box {
	struct drm_pattern_color line_color;
	struct drm_pattern_color bg_color;
	unsigned int h_line_interval;
	unsigned int v_line_interval;
	unsigned short h_position_st;
	unsigned short h_line_width;
	unsigned short v_position_st;
	unsigned short v_line_width;
};

struct drm_pattern_rgb_pure_colorbar {
	unsigned int colorbar_h_size;
	unsigned int colorbar_v_size;
};

struct drm_pattern_force_timing {
	bool enable;
	unsigned short force_h_size;
	unsigned short force_v_size;
};

struct drm_pattern_info {
	enum drm_pattern_position position;
	unsigned short pattern_type;
	unsigned short h_size;
	unsigned short v_size;
	union {
		struct drm_pattern_pure_color pure_color;
		struct drm_pattern_ramp ramp;
		struct drm_pattern_pure_colorbar pure_colorbar;
		struct drm_pattern_gradient_colorbar gradient_colorbar;
		struct drm_pattern_black_white_colorbar black_white_colorbar;
		struct drm_pattern_cross cross;
		struct drm_pattern_pip_window pip_window;
		struct drm_pattern_crosshatch crosshatch;
		struct drm_pattern_box box;
		struct drm_pattern_rgb_pure_colorbar rgb_pure_colorbar;
	} m;
	struct drm_pattern_force_timing force_timing;
	bool enable;
};

struct drm_pattern_capability {
	unsigned short pattern_type[DRM_PATTERN_POSITION_MAX];
};

//-------------------------------------------------------------------------------------------------
//  function
//-------------------------------------------------------------------------------------------------
int mtk_drm_pattern_set_env(enum drm_pattern_position position,
	struct drm_pattern_env *env);

ssize_t mtk_drm_pattern_show(char *buf,
	struct device *dev, enum drm_pattern_position position);

int mtk_drm_pattern_store(const char *buf,
	struct device *dev, enum drm_pattern_position position);

int mtk_drm_pattern_set_param(
	struct drm_pattern_status *pattern,
	struct drm_mtk_pattern_config *config);

int mtk_drm_pattern_resume(struct drm_pattern_info *pattern_info);

void mtk_drm_pattern_create_sysfs(struct platform_device *pdev);

#endif
