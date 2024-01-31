/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_PQMAP_H_
#define _MTK_TV_DRM_PQMAP_H_
#include <linux/dma-buf.h>
#include <drm/mtk_tv_drm.h>
#include "pqmap_utility.h"
#include "mtk_tv_sm_ml.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
#define IS_PQMAP_INVALID(x) \
	(((x >= EN_PQMAP_TYPE_MAX) || (x < EN_PQMAP_TYPE_MAIN)) ? (true) : (false))

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------
enum PQMAP_TYPE_E {
	EN_PQMAP_TYPE_MAIN = 0,
	EN_PQMAP_TYPE_MAIN_EX,
	EN_PQMAP_TYPE_MAX,
};

enum PQMAP_VERSION_TYPE_E {
	EN_PQMAP_VERSION_TYPE_TOTAL_VER = 0,
	EN_PQMAP_VERSION_TYPE_HWREG_VER,
	EN_PQMAP_VERSION_TYPE_SWREG_VER,
	EN_PQMAP_VERSION_TYPE_MAX,
};

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_pqmap_info {
	bool init;
	void *pim_handle;
	void *pim_va;
	uint32_t pim_size;
};
struct mtk_pqmap_context {
	struct mtk_pqmap_info pqmap[MTK_DRM_PQMAP_NUM_MAX];
};

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_pqmap_init(
	struct mtk_pqmap_context *pqmap_ctx,
	uint8_t pqmap_idx,
	struct drm_mtk_tv_pqmap_param *param);

int mtk_tv_drm_pqmap_deinit(
	struct mtk_pqmap_context *pqmap_ctx);

int mtk_tv_drm_pqmap_update(
	struct mtk_pqmap_context *pqmap_ctx,
	struct mtk_sm_ml_context *ml_ctx,
	struct drm_mtk_tv_pqmap_node_array *node_array);

int mtk_tv_drm_pqmap_write_ml(
	struct mtk_pqmap_context *pqmap_ctx,
	struct mtk_sm_ml_context *ml_ctx,
	uint16_t *node_num,
	uint16_t *node_buf);

int mtk_tv_drm_pqmap_check_buffer(
	void *buffer,
	uint32_t size);

int mtk_tv_drm_pqmap_set_info_ioctl(
	struct drm_device *drm,
	void *data,
	struct drm_file *file_priv);

int mtk_tv_drm_pqmap_get_version(
	struct mtk_pqmap_context *pqmap_ctx,
	enum PQMAP_VERSION_TYPE_E qmap_ver,
	enum PQMAP_TYPE_E type,
	struct pqmap_version *output_version);

#endif //_MTK_TV_DRM_PQMAP_H_
