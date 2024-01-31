/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_GLOBAL_PQ_H_
#define _MTK_TV_DRM_GLOBAL_PQ_H_
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_pqmap.h"

//-------------------------------------------------------------------------------------------------
//  Local Defines
//-------------------------------------------------------------------------------------------------
#define MTK_GLOBAL_PQ_STRING_LEN_MAX	2048

//-------------------------------------------------------------------------------------------------
//  Enum
//-------------------------------------------------------------------------------------------------
enum mtk_global_ambilight_buffer {
	EN_BUFFER_0 = 0,
	EN_BUFFER_1,
	EN_BUFFER_2,
	EN_BUFFER_3,
	EN_BUFFER_MAX,
};

//-------------------------------------------------------------------------------------------------
//  Structures
//-------------------------------------------------------------------------------------------------
struct mtk_global_pq_context {
	bool init;
	struct mtk_tv_drm_metabuf metabuf;
	uint64_t string_pa;
	char *string_va;
	uint32_t string_len;
};

struct mtk_global_pq_hw_version {
	int pnlgamma_version;
	int pqgamma_version;
	u32 ambient_version;
};

struct mtk_global_pq_amb_context {
	int32_t amb_aul_instance;
	bool ambient_light_init;
};

//-------------------------------------------------------------------------------------------------
//  Global Variables
//-------------------------------------------------------------------------------------------------
extern bool bPquEnable; // is pqurv55 enable ? declare at mtk_tv_drm_drv.c.

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
void mtk_tv_drm_global_pq_create_sysfs(struct platform_device *pdev);

void mtk_tv_drm_global_pq_remove_sysfs(struct platform_device *pdev);

int mtk_tv_drm_global_pq_parse_dts(
	struct mtk_tv_kms_context *pctx);

int mtk_tv_drm_global_pq_init(
	struct mtk_global_pq_context *ctx);

int mtk_tv_drm_global_pq_deinit(
	struct mtk_global_pq_context *ctx);

int mtk_tv_drm_global_pq_flush(
	struct mtk_global_pq_context *ctx,
	char *global_pq_string);

int mtk_tv_drm_global_pq_check(
	void *global_pq_buffer,
	uint32_t size);

int mtk_tv_drm_ambilight_get_data(
	struct mtk_tv_drm_crtc *mtk_tv_crtc,
	struct drm_mtk_tv_ambilight_data *data);

int mtk_tv_drm_get_ambilight_data_ioctl(
	struct drm_device *dev, void *data, struct drm_file *file_priv);

struct pqmap_version mtk_tv_drm_global_get_qmap_version(
	struct mtk_tv_kms_context *pctx,
	enum PQMAP_VERSION_TYPE_E qmap_ver,
	enum PQMAP_TYPE_E type);

int mtk_tv_drm_ambilight_debug(const char *buf, struct device *dev);


#endif
