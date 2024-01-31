/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_AUTOGEN_H_
#define _MTK_TV_DRM_AUTOGEN_H_
#include <drm/mtk_tv_drm.h>
#include <linux/mutex.h>
#include "mtk_tv_drm_metabuf.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
#define AUTOGEN_CAPABILITY_SIZE_MAX	4

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_autogen_context {
	int init;

	// ALGO parameters from DTS
	// Don't reset those data after @autogen fail because others module maybe use them.
	uint32_t header_size;
	uint16_t capability_size;
	uint32_t capability[AUTOGEN_CAPABILITY_SIZE_MAX];

	// ALGO context elements
	void *ctx;
	void *metadata;
	void *hw_report;
	void *sw_reg;
	void *func_table;
	struct mtk_tv_drm_metabuf metabuf; // Content: [header data][swreg data]
	struct mutex mutex;
};

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_autogen_init(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_deinit(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_suspend(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_resume(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_lock(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_unlock(
	struct mtk_autogen_context *autogen_ctx);

int mtk_tv_drm_autogen_set_mem_info(
	struct mtk_autogen_context *autogen_ctx,
	void *start_addr,
	void *max_addr);

int mtk_tv_drm_autogen_get_cmd_size(
	struct mtk_autogen_context *autogen_ctx,
	uint32_t *cmd_size);

int mtk_tv_drm_autogen_set_nts_hw_reg(
	struct mtk_autogen_context *autogen_ctx,
	uint32_t reg_idx,
	uint32_t reg_value);

int mtk_tv_drm_autogen_set_sw_reg(
	struct mtk_autogen_context *autogen_ctx,
	uint32_t reg_idx,
	uint32_t reg_value);

#endif //_MTK_TV_DRM_AUTOGEN_H_
