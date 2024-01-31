/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_DEMURA_H_
#define _MTK_TV_DRM_DEMURA_H_
#include <drm/mtk_tv_drm.h>
#include "mtk_tv_drm_metabuf.h"

//-------------------------------------------------------------------------------------------------
//  Defines
//-------------------------------------------------------------------------------------------------
#define MTK_DEMURA_SLOT_NUM		(2)
#define DEMURA_BUFFER_DESC_SIZE (0x400)  // 1024 Byte

#define HEADER_GAIN_NUM (8)
#define HEADER_OFFSET_NUM (8)
#define HEADER_PLANE_NUM (8)
#define HEADER_NODE_NUM (8)
#define HEADER_FORMAT_NUM (16)
#define HEADER_PADDING_NUM (28)
#define HEADER_U32_BYTES (4)
#define HEADER_U16_BYTES (2)

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_demura_slot {
	void    *bin_va;
	uint64_t bin_pa;
	uint32_t bin_len;
	bool     disable;
	uint8_t  id;
};

struct mtk_demura_context {
	int init; // 0: not init, <0: error code when init, 1: init success
	struct mtk_tv_drm_metabuf metabuf;
	struct mtk_demura_slot config_slot[MTK_DEMURA_SLOT_NUM];
	uint32_t bin_max_size;
	int8_t curr_slot_idx;
};

struct mtk_demura_header {
	int8_t format[HEADER_FORMAT_NUM];
	int8_t version[HEADER_U32_BYTES];
	int8_t header_size[HEADER_U32_BYTES]; /* 20 ~ 23 */
	int8_t lut_size[HEADER_U32_BYTES];  /* 24 ~ 27 */
	int8_t h_size[HEADER_U16_BYTES]; /* 28 29  need to remap */
	int8_t v_size[HEADER_U16_BYTES]; /* 30 31  need to remap */
	int8_t mode; /* 32 */
	int8_t plane_num; /* 33 */
	int8_t gain_r[HEADER_GAIN_NUM]; /* 34 ~ 41 */
	int8_t gain_g[HEADER_GAIN_NUM]; /* 42 ~ 49 */
	int8_t gain_b[HEADER_GAIN_NUM]; /* 50 ~ 57 */
	int8_t offset_r[HEADER_OFFSET_NUM][HEADER_U16_BYTES]; /* 58 ~ 73 */
	int8_t offset_g[HEADER_OFFSET_NUM][HEADER_U16_BYTES]; /* 74 ~ 89 */
	int8_t offset_b[HEADER_OFFSET_NUM][HEADER_U16_BYTES]; /* 90 ~ 105 */
	int8_t h_blk_size; /* 106 */
	int8_t v_blk_size; /* 107 */
	int8_t black_limit[HEADER_U16_BYTES]; /* 108 ~ 109 */
	int8_t plane[HEADER_PLANE_NUM][HEADER_U16_BYTES]; /* 110 ~ 125 */
	int8_t white_limit[HEADER_U16_BYTES]; /* 126 ~ 127 */
	int8_t padding[HEADER_PADDING_NUM]; /* 128 ~ 155 */
	int8_t header_crc[HEADER_U32_BYTES];
	/* offset DMC_BIN_HEADER_CRC_OFFSET : 156 ~ 159  need to remap */
};

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_demura_init(
	struct mtk_demura_context *demura, struct mtk_panel_context *pctx_pnl);

int mtk_tv_drm_demura_deinit(
	struct mtk_demura_context *demura);

int mtk_tv_drm_demura_set_config(
	struct mtk_demura_context *demura_ctx,
	struct drm_mtk_demura_config *config);

int mtk_tv_drm_demura_check_buffer(
	void *buffer,
	uint32_t size);

int mtk_tv_drm_demura_suspend(
	struct mtk_demura_context *demura);

int mtk_tv_drm_demura_resume(
	struct mtk_demura_context *demura);

int mtk_tv_drm_DLG_demura_slot_init(
	struct mtk_demura_context *demura, struct mtk_panel_context *pctx_pnl);

int mtk_tv_drm_DLG_demura_set_config(
	struct mtk_demura_context *demura, bool bDLG_mode);

int mtk_tv_drm_demura_slot_init(
	struct mtk_demura_context *demura);

int mtk_tv_drm_set_demura_enable(
	struct mtk_demura_context *demura, bool bEnable);
#endif //_MTK_TV_DRM_DEMURA_H_
