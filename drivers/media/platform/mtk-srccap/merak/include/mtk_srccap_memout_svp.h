/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_OPTEE)
#ifndef MTK_SRCCAP_MEMOUT_SVP_H
#define MTK_SRCCAP_MEMOUT_SVP_H

#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/tee_drv.h>
#include <uapi/linux/tee.h>
#include <tee_client_api.h>

/* ============================================================================================== */
/* ---------------------------------------- Definess -------------------------------------------- */
/* ============================================================================================== */
#define DISP_TA_UUID {0x4dd53ca0, 0x0248, 0x11e6, \
	{0x86, 0xc0, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b} }
#define SRCCAP_SOURCE_BUF_TAG (22)
#define SRCCAP_BUF_TAG_OFFSET (24)

/* ============================================================================================== */
/* ------------------------------------------ Enums --------------------------------------------- */
/* ============================================================================================== */

/* ============================================================================================== */
/* ------------------------------------------ Macro --------------------------------------------- */
/* ============================================================================================== */
#define MALLOC(size)	((size >= PAGE_SIZE) ? vmalloc(size) : kmalloc(size, GFP_KERNEL))
#define FREE_MEM(ptr, size)			\
	do {						\
		if (size >= PAGE_SIZE)	\
			vfree(ptr);			\
		else					\
			kfree(ptr);			\
	} while (0)

/* ============================================================================================== */
/* ----------------------------------------- Structs -------------------------------------------- */
/* ============================================================================================== */
struct srccap_memout_svp_info {
	int optee_version;
	TEEC_Context *pstContext;
	TEEC_Session *pstSession;
	bool svp_enable;
	bool force_svp;
	u8 aid;
	u32 video_pipeline_ID;
};

/* ============================================================================================== */
/* ---------------------------------------- Functions ------------------------------------------- */
/* ============================================================================================== */
int mtk_memout_svp_init(struct mtk_srccap_dev *srccap_dev);
void mtk_memout_svp_exit(struct mtk_srccap_dev *srccap_dev);
int mtk_memout_set_secure_mode(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_secure_info *secureinfo);
int mtk_memout_cfg_buf_security(
	struct mtk_srccap_dev *srccap_dev,
	int buf_fd,
	int meta_fd);
int mtk_memout_get_sst(
	struct mtk_srccap_dev *srccap_dev,
	struct v4l2_memout_sst_info *sst_info);
int mtk_memout_svp_show(
	struct mtk_srccap_dev *srccap_dev,
	char *buf);
int mtk_memout_svp_store(
	struct mtk_srccap_dev *srccap_dev,
	const char *buf);

#endif  // MTK_SRCCAP_MEMOUT_SVP_H
#endif // IS_ENABLED(CONFIG_OPTEE)
