/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_SCRIPT_MANAGER_H_
#define _MTK_TV_DRM_SCRIPT_MANAGER_H_
#include <linux/list.h>
#include <linux/mutex.h>
#include <drm/mtk_tv_drm.h>
#include "drv_scriptmgt.h"


//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
#define MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX	(0xFF)
#define MTK_TV_DRM_SM_ML_CMD_SIZE		(8)

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------

struct mtk_sm_ml_context {
	// init env
	bool init;
	int ml_fd;
	enum sm_ml_sync ml_sync_id;
	uint8_t ml_buf_cnt;
	uint16_t ml_cmd_cnt;
	struct mutex mutex;

	// runtime env
	uint8_t mem_idx;
	uint16_t cmd_cnt;
	uint64_t cur_addr;
	uint64_t max_addr;
	uint32_t mem_fail_cnt;
	uint32_t list_cmd_cnt;
	struct list_head work_list;
};

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_sm_ml_init(
	struct mtk_sm_ml_context *context,
	enum sm_ml_sync sync_id,
	enum sm_ml_unique_id unique_id,
	uint8_t buffer_cnt,
	uint16_t cmd_cnt);

int mtk_tv_sm_ml_deinit(
	struct mtk_sm_ml_context *context);

// int mtk_tv_sm_ml_prepare(
int mtk_tv_sm_ml_get_mem_index(
	struct mtk_sm_ml_context *context);

int mtk_tv_sm_ml_write_cmd(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs);

int mtk_tv_sm_ml_write_cmd_delay(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs,
	uint8_t vsync_num);

int mtk_tv_sm_ml_fire(
	struct mtk_sm_ml_context *context);

int mtk_tv_sm_ml_get_mem_info(
	struct mtk_sm_ml_context *context,
	void **start_addr,
	void **max_addr);

int mtk_tv_sm_ml_put_mem_info(
	struct mtk_sm_ml_context *context,
	void *start_addr);

int mtk_tv_sm_ml_set_mem_cmds(
	struct mtk_sm_ml_context *context,
	void *start_addr,
	uint16_t cmd_cnt);

bool mtk_tv_sm_ml_is_valid(
	struct mtk_sm_ml_context *context);

#endif //_MTK_TV_DRM_SCRIPT_MANAGER_H_
