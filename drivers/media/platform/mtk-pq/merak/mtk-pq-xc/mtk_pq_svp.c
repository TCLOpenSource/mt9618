// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_OPTEE)
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/err.h>
#include <asm/uaccess.h>

#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>

#include "mtk_pq.h"
#include "mtk_pq_common_ca.h"
#include "mtk_iommu_dtv_api.h"
#include "utpa2_XC.h"

#define SYSFS_MAX_BUF_COUNT (0x1000)

static inline bool _mtk_pq_svp_check_scnsize(int size, int max_size)
{
	if ((size < 0) || (size >= max_size)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "scan size %d overflow!\n", size);
		return false;
	}
	return true;
}

static bool _mtk_pq_svp_get_dbg_value_from_string(char *buf, char *name,
	unsigned int len, __u32 *value)
{
	bool find = false;
	char *string, *tmp_name, *cmd, *tmp_value;

	if ((buf == NULL) || (name == NULL) || (value == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
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
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
						"kstrtoint fail!\n");
				return find;
			}
			find = true;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
					"name = %s, value = %d\n", name, *value);
			return find;
		}
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_DBG,
			"name(%s) was not found!\n", name);

	return find;
}

static int _mtk_pq_teec_enable(struct mtk_pq_device *pq_dev)
{
	u32 tee_error = 0;
	int ret = 0;
	TEEC_UUID uuid = DISP_TA_UUID;
	TEEC_Context *pstContext = pq_dev->display_info.secure_info.pstContext;
	TEEC_Session *pstSession = pq_dev->display_info.secure_info.pstSession;

	if ((pstSession != NULL) && (pstContext != NULL)) {
		ret = mtk_pq_common_ca_teec_init_context(pq_dev, pstContext);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Init Context failed!\n");
			mtk_pq_common_ca_teec_finalize_context(pq_dev, pstContext);
			return -EPERM;
		}

		ret = mtk_pq_common_ca_teec_open_session(pq_dev, pstContext, pstSession, &uuid,
					NULL, &tee_error);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"TEEC Open pstSession failed with error(%u)\n", tee_error);
			mtk_pq_common_ca_teec_close_session(pq_dev, pstSession);
			mtk_pq_common_ca_teec_finalize_context(pq_dev, pstContext);
			return -EPERM;
		}

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "Enable disp teec success.\n");
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pstSession or pstContext is NULL.\n");
		return -EPERM;
	}

	return 0;
}

static void _mtk_pq_teec_disable(struct mtk_pq_device *pq_dev)
{
	TEEC_Context *pstContext = pq_dev->display_info.secure_info.pstContext;
	TEEC_Session *pstSession = pq_dev->display_info.secure_info.pstSession;

	if ((pstContext != NULL) && (pstSession != NULL)) {
		mtk_pq_common_ca_teec_close_session(pq_dev, pstSession);
		mtk_pq_common_ca_teec_finalize_context(pq_dev, pstContext);

		STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "Disable disp teec success.\n");
	} else {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pstSession or pstContext is NULL.\n");
	}
}

static int _mtk_pq_teec_smc_call(struct mtk_pq_device *pq_dev,
	EN_XC_OPTEE_ACTION action, TEEC_Operation *op)
{
	u32 error = 0;
	int ret = 0;
	TEEC_Session *pstSession = pq_dev->display_info.secure_info.pstSession;

	if (pstSession == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pstSession is NULL!\n");
		return -EPERM;
	}
	if (op == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "op is NULL!\n");
		return -EPERM;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQ SMC cmd:%u\n", action);

	ret = mtk_pq_common_ca_teec_invoke_cmd(pq_dev, pstSession, (u32)action, op, &error);
	if (ret) {
		if (action == E_XC_OPTEE_DESTROY_DISP_PIPELINE)
			ret = 0; //The cmd fail is acceptable while destroy
		else {
			ret = -EPERM;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"SMC call failed with error(%u)\n", error);
		}
	}

	return ret;
}

static int _mtk_pq_svp_create_disp_ppl(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Operation op = {0};

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));
	memset(&op, 0, sizeof(TEEC_Operation));

	/* create disp pipeline */
	optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
	optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
	optee_handler.enAID = (EN_XC_STI_AID)pq_dev->display_info.secure_info.aid;
	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
	op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_NONE, TEEC_NONE, TEEC_NONE);
	ret = _mtk_pq_teec_smc_call(pq_dev,
		E_XC_OPTEE_CREATE_DISP_PIPELINE,
		&op);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "smc call fail\n");
		return -EPERM;
	}

	pq_dev->display_info.secure_info.disp_pipeline_ID = optee_handler.u32DispPipelineID;
	pq_dev->display_info.secure_info.disp_pipeline_valid = true;
	return 0;
}

static int _mtk_pq_svp_auth_buf_with_errhandle(struct mtk_pq_device *pq_dev,
	u32 buf_tag, u64 iova, u32 size)
{
	int ret = 0;
	u32 pipelineID = 0;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	pipelineID = pq_dev->display_info.secure_info.disp_pipeline_ID;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP,
		"auth buffer: buf_tag=%u, size=%u, iova=0x%llx, pipelineID=0x%x\n",
		buf_tag, size, iova, pipelineID);

	ret = mtkd_iommu_buffer_authorize(buf_tag, iova, size, pipelineID);
	if (ret) {
		//Error handle for OPTEE auto release ppl
		if (ret == TEEC_ERROR_ITEM_NOT_FOUND) {
			// Re-create disp pipeline
			ret = _mtk_pq_svp_create_disp_ppl(pq_dev);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_svp_create_disp_ppl fail\n");
				return -EPERM;
			}

			pipelineID = pq_dev->display_info.secure_info.disp_pipeline_ID;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP,
				"re-auth buffer: buf_tag=%u, size=%u, iova=0x%llx, pipelineID=0x%x\n",
				buf_tag, size, iova, pipelineID);

			//Re-auth buffer
			ret = mtkd_iommu_buffer_authorize(buf_tag, iova, size, pipelineID);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"re-auth still fail, iova = %llu, ret = %d\n", iova, ret);
				return -EPERM;
			}
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"auth fail, iova = %llu, ret = %d\n", iova, ret);
			return -EPERM;
		}
	}

	return 0;
}

static int _mtk_pq_unauth_internal_buf(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	u8 idx = 0;
	u64 iova = 0;
	struct pq_buffer *pBufferTable = NULL;

	/* get device parameter */
	pBufferTable = pq_dev->display_info.BufferTable;

	if (pBufferTable == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pBufferTable = NULL\n");
		return -EPERM;
	}

	/* unauthorize pq buf */
	if (!pq_dev->display_info.secure_info.pq_buf_authed)
		goto already_unauth;

	for (idx = 0; idx < PQU_BUF_MAX; idx++) {
		if (pBufferTable[idx].valid && pBufferTable[idx].sec_buf) {
			iova = pBufferTable[idx].addr; //unlock secure buf only
			ret = mtkd_iommu_buffer_unauthorize(iova);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"unauthorize fail ret = %d\n", ret);
				return -EPERM;
			}

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP,
				"unauth pq_buf[%u]: iova=0x%llx\n", idx, iova);
		}
	}
	pq_dev->display_info.secure_info.pq_buf_authed = false;
	return ret;

already_unauth:
	return ret;
}

static int _mtk_pq_auth_internal_buf(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	u8 idx = 0;
	u32 size = 0, pipelineID = 0, buf_tag = 0;
	u64 iova = 0;
	bool secbuf_found = false;
	struct pq_buffer *pBufferTable = NULL;
	struct mtk_pq_platform_device *pqdev = dev_get_drvdata(pq_dev->dev);

	/* get device parameter */
	pipelineID = pq_dev->display_info.secure_info.disp_pipeline_ID;
	pBufferTable = pq_dev->display_info.BufferTable;

	/* authorize pq buf */
	if (pq_dev->display_info.secure_info.pq_buf_authed)
		goto already_auth;

	if ((pipelineID == 0) || (!pq_dev->display_info.secure_info.disp_pipeline_valid))
		goto auth_next_time;

	for (idx = 0; idx < PQU_BUF_MAX; idx++) {
		if (pBufferTable[idx].valid && pBufferTable[idx].sec_buf) {
			buf_tag = mtk_pq_buf_get_iommu_idx(pqdev, idx);
			size = pBufferTable[idx].size;
			iova = pBufferTable[idx].addr; //lock secure buf only

			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "auth pq internal buf[%u]\n", idx);
			ret = _mtk_pq_svp_auth_buf_with_errhandle(pq_dev, buf_tag, iova, size);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_svp_auth_buf_with_errhandle fail\n");
				return -EPERM;
			}
			secbuf_found = true;
		}
	}

	if (secbuf_found)
		pq_dev->display_info.secure_info.pq_buf_authed = true;

auth_next_time:
already_auth:
	return ret;
}

static int _mtk_pq_auth_win_buf(struct mtk_pq_device *pq_dev, struct mtk_pq_dma_buf *buf)
{
	int ret = 0;
	u32 buf_tag = 0;
	struct mtk_pq_platform_device *pqdev = NULL;

	if (buf == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "buf = NULL\n");
		return -EPERM;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "Auth window buf\n");

	/* authorize buf */
	pqdev = dev_get_drvdata(pq_dev->dev);
	buf_tag = pqdev->display_dev.pq_iommu_window << pqdev->display_dev.buf_iommu_offset;
	ret = _mtk_pq_svp_auth_buf_with_errhandle(pq_dev, buf_tag, buf->iova, buf->size);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pq_svp_auth_buf_with_errhandle fail\n");
		return -EPERM;
	}

	return ret;
}

static int _mtk_pq_parse_outbuf_meta(struct mtk_pq_device *pq_dev,
			int meta_fd, enum mtk_pq_aid *aid, u32 *pipeline_id)
{
	int ret = 0;
	struct vdec_dd_svp_info vdec_svp_md;
	struct meta_srccap_svp_info srccap_svp_md;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev = NULL\n");
		goto err_out;
	}
	if (aid == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "aid = NULL\n");
		goto err_out;
	}
	if (pipeline_id == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pipeline_id = NULL\n");
		goto err_out;
	}

	memset(&vdec_svp_md, 0, sizeof(struct vdec_dd_svp_info));
	memset(&srccap_svp_md, 0, sizeof(struct meta_srccap_svp_info));

	*aid = PQ_AID_NS;
	*pipeline_id = 0;

	if (IS_INPUT_B2R(pq_dev->common_info.input_source)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "MVOP source!!\n");

		ret = mtk_pq_common_read_metadata(meta_fd,
			EN_PQ_METATAG_VDEC_SVP_INFO, (void *)&vdec_svp_md);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "metadata do not has svp tag\n");
			goto non_svp_out;
		}

		if (!CHECK_AID_VALID(vdec_svp_md.aid)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Received invalid aid %u\n", vdec_svp_md.aid);
			goto err_out;
		}

		*aid = (enum mtk_pq_aid)vdec_svp_md.aid;
		*pipeline_id = vdec_svp_md.pipeline_id;

		ret = mtk_pq_common_delete_metadata(meta_fd,
			EN_PQ_METATAG_VDEC_SVP_INFO);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "delete svp tag fail\n");
			goto err_out;
		}
	}

	if (IS_INPUT_SRCCAP(pq_dev->common_info.input_source)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "EXTIN source!!\n");

		ret = mtk_pq_common_read_metadata(meta_fd,
			EN_PQ_METATAG_SRCCAP_SVP_INFO, (void *)&srccap_svp_md);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "metadata do not has svp tag\n");
			goto non_svp_out;
		}

		if (!CHECK_AID_VALID(srccap_svp_md.aid)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"Received invalid aid %u\n", srccap_svp_md.aid);
			goto err_out;
		}

		*aid = (enum mtk_pq_aid)srccap_svp_md.aid;
		*pipeline_id = srccap_svp_md.pipelineid;

		ret = mtk_pq_common_delete_metadata(meta_fd,
			EN_PQ_METATAG_SRCCAP_SVP_INFO);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "delete svp tag fail\n");
			goto err_out;
		}
	}
	return ret;

non_svp_out:
	return 0;

err_out:
	return -EPERM;
}

static int _mtk_pq_svp_check_outbuf_aid(struct mtk_pq_device *pq_dev,
	enum mtk_pq_aid bufaid, bool *spb)
{
	bool s_dev = false;
	enum mtk_pq_aid devaid = PQ_AID_NS;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}
	if (spb == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}
	if (!CHECK_AID_VALID(bufaid)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received invalid aid %u\n", bufaid);
		return -EINVAL;
	}

	s_dev = pq_dev->display_info.secure_info.svp_enable;
	s_dev |= pq_dev->display_info.secure_info.force_svp;

	if (pq_dev->display_info.secure_info.first_frame) {
		/* First frame just check dev condition */
		if ((bufaid != PQ_AID_NS) && (s_dev)) {
			*spb = true;
			if (bufaid == PQ_AID_SDC)
				pq_dev->display_info.secure_info.aid = PQ_AID_S;
			else
				pq_dev->display_info.secure_info.aid = bufaid;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "secure content playback!\n");
		}
		if ((bufaid != PQ_AID_NS) && (!s_dev)) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"PQ QBUF ERROR! s-buffer queue into ns-dev!\n");
			return -EPERM;
		}
		if ((bufaid == PQ_AID_NS) && (s_dev)) {
			*spb = true;
			pq_dev->display_info.secure_info.aid = PQ_AID_S;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "secure content playback!\n");
		}
		if ((bufaid == PQ_AID_NS) && (!s_dev)) {
			*spb = false;
			pq_dev->display_info.secure_info.aid = PQ_AID_NS;
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "non-secure content playback!\n");
		}
		pq_dev->display_info.secure_info.first_frame = false;
	} else {
		/* Dynamic stream aid change condition */
		devaid = pq_dev->display_info.secure_info.aid;
		if (!s_dev) {
			if (bufaid != PQ_AID_NS) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQ aid change, type = ns->s!\n");
				return -EADDRNOTAVAIL;
			}
		} else {
			if (bufaid > devaid) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQ aid change, type = ls->hs!\n");
				return -EADDRNOTAVAIL;
			} else if (bufaid < devaid) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP,
					"PQ aid change, type = hs->ls! Keep origin hs.\n");
				*spb = true;
			} else
				*spb = true;
		}
	}

	return 0;
}

int mtk_pq_svp_init(struct mtk_pq_device *pq_dev)
{
	TEEC_Context *pstContext = NULL;
	TEEC_Session *pstSession = NULL;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	pstContext = malloc(sizeof(TEEC_Context));
	if (pstContext == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pstContext malloc fail!\n");
		return -EPERM;
	}

	pstSession = malloc(sizeof(TEEC_Session));
	if (pstSession == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pstSession malloc fail!\n");
		free(pstContext);
		return -EPERM;
	}

	memset(pstContext, 0, sizeof(TEEC_Context));
	memset(pstSession, 0, sizeof(TEEC_Session));

	if (pq_dev->display_info.secure_info.pstContext != NULL) {
		free(pq_dev->display_info.secure_info.pstContext);
		pq_dev->display_info.secure_info.pstContext = NULL;
	}

	if (pq_dev->display_info.secure_info.pstSession != NULL) {
		free(pq_dev->display_info.secure_info.pstSession);
		pq_dev->display_info.secure_info.pstSession = NULL;
	}

	memset(&pq_dev->display_info.secure_info, 0, sizeof(struct pq_secure_info));
	pq_dev->display_info.secure_info.pstContext = pstContext;
	pq_dev->display_info.secure_info.pstSession = pstSession;
	pq_dev->display_info.secure_info.optee_version = 3;

	if (_mtk_pq_teec_enable(pq_dev)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "teec enable fail\n");
		return -EPERM;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQ SVP init success.\n");
	return 0;
}

void mtk_pq_svp_exit(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	u8 sec_md = 0;
	TEEC_Context *pstContext = NULL;
	TEEC_Session *pstSession = NULL;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return;
	}

	ret = mtk_pq_svp_set_sec_md(pq_dev, &sec_md);
	_mtk_pq_teec_disable(pq_dev);

	pstContext = pq_dev->display_info.secure_info.pstContext;
	pstSession = pq_dev->display_info.secure_info.pstSession;

	if (pstContext != NULL) {
		free(pstContext);
		pq_dev->display_info.secure_info.pstContext = NULL;
	}

	if (pstSession != NULL) {
		free(pstSession);
		pq_dev->display_info.secure_info.pstSession = NULL;
	}
}

int mtk_pq_svp_set_sec_md(struct mtk_pq_device *pq_dev, u8 *secure_mode_flag)
{
	int ret = 0;
	bool secure_mode = 0;
	XC_STI_OPTEE_HANDLER optee_handler;
	TEEC_Operation op = {0};

	memset(&optee_handler, 0, sizeof(XC_STI_OPTEE_HANDLER));
	memset(&op, 0, sizeof(TEEC_Operation));

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}
	if (secure_mode_flag == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	if (pq_dev->input_stream.streaming || pq_dev->output_stream.streaming) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Can not control sec mode while stream on!\n");
		return -EPERM;
	}

	secure_mode = (bool)*secure_mode_flag;
	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQDD secure mode:%s\n",
		(secure_mode ? "ENABLE":"DISABLE"));

	if ((!secure_mode) && (pq_dev->display_info.secure_info.disp_pipeline_valid)) {
		/* destroy disp pipeline */
		optee_handler.u16Version = XC_STI_OPTEE_HANDLER_VERSION;
		optee_handler.u16Length = sizeof(XC_STI_OPTEE_HANDLER);
		optee_handler.enAID = (EN_XC_STI_AID)pq_dev->display_info.secure_info.aid;
		optee_handler.u32DispPipelineID = pq_dev->display_info.secure_info.disp_pipeline_ID;
		op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.buffer = (void *)&optee_handler;
		op.params[PQ_CA_SMC_PARAM_IDX_0].tmpref.size = sizeof(XC_STI_OPTEE_HANDLER);
		op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
				TEEC_NONE, TEEC_NONE, TEEC_NONE);
		ret = _mtk_pq_teec_smc_call(pq_dev,
					E_XC_OPTEE_DESTROY_DISP_PIPELINE,
					&op);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "smc call fail\n");
			return ret;
		}

		/* clear para*/
		pq_dev->display_info.secure_info.disp_pipeline_valid = 0;
		pq_dev->display_info.secure_info.aid = PQ_AID_NS;
		pq_dev->display_info.secure_info.vdo_pipeline_ID = 0;
		pq_dev->display_info.secure_info.disp_pipeline_ID = 0;
	}

	pq_dev->display_info.secure_info.svp_enable = secure_mode;
	return ret;
}

int mtk_pq_svp_cfg_outbuf_sec(
	struct mtk_pq_device *pq_dev,
	struct v4l2_buffer *buffer)
{
	int ret = 0;
	bool secure_playback = false;
	u32 video_pipeline_id = 0;
	enum mtk_pq_aid aid = 0;
	struct v4l2_plane *plane_ptr = NULL;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		ret = -EINVAL;
		goto out;
	}
	if (buffer == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		ret = -EINVAL;
		goto out;
	}

	/* parse source meta */
	if (buffer->m.planes == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		ret = -EINVAL;
		goto out;
	}
	plane_ptr = &(buffer->m.planes[(buffer->length - 1)]);
	ret = _mtk_pq_parse_outbuf_meta(pq_dev,
		plane_ptr->m.fd,
		&aid,
		&video_pipeline_id);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Parse svp meta tag fail\n");
		goto out;
	}

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP,
		"Received video buf fd = %d, aid = %d, pipeline id = 0x%x\n",
			buffer->m.planes[0].m.fd, aid, video_pipeline_id);

	/* check dev/buf secure relation */
	ret = _mtk_pq_svp_check_outbuf_aid(pq_dev, aid, &secure_playback);
	if (ret) {
		if (ret == -EADDRNOTAVAIL) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "AID dynamic change occurs!\n");
			goto out;
		} else {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "_mtk_pq_svp_check_outbuf_aid fail\n");
			goto out;
		}
	}

	if (secure_playback) {
		if (!pq_dev->display_info.secure_info.disp_pipeline_valid) {
			/* create disp pipeline */
			ret = _mtk_pq_svp_create_disp_ppl(pq_dev);
			if (ret) {
				STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
					"_mtk_pq_svp_create_disp_ppl fail\n");
				goto out;
			}
		}
		/* authorize pq buffer */
		ret = _mtk_pq_auth_internal_buf(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "auth pq buf fail\n");
			goto out;
		}
	}

	pq_dev->display_info.secure_info.vdo_pipeline_ID = video_pipeline_id;

out:
	return ret;
}

int mtk_pq_svp_cfg_capbuf_sec(
	struct mtk_pq_device *pq_dev,
	struct mtk_pq_buffer *buffer)
{
	int ret = 0;
	bool s_dev = false;
	struct meta_buffer meta_buf;
	struct meta_pq_disp_svp pq_svp_md;

	memset(&meta_buf, 0, sizeof(struct meta_buffer));
	memset(&pq_svp_md, 0, sizeof(struct meta_pq_disp_svp));

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		ret = -EINVAL;
		goto out;
	}
	if (buffer == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		ret = -EINVAL;
		goto out;
	}

	s_dev = pq_dev->display_info.secure_info.svp_enable;
	s_dev |= pq_dev->display_info.secure_info.force_svp;

	STI_PQ_LOG(STI_PQ_LOG_LEVEL_SVP, "PQ is %s-device, received cap buf iova=0x%llx\n",
			s_dev?"s":"ns", buffer->frame_buf.iova);

	if (s_dev) {
		/* authorize win buf */
		ret = _mtk_pq_auth_win_buf(pq_dev, &buffer->frame_buf);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
				"auth window buf fail\n");
			goto out;
		}

		pq_svp_md.aid = (u8)pq_dev->display_info.secure_info.aid;
		pq_svp_md.pipelineid = pq_dev->display_info.secure_info.disp_pipeline_ID;
	}

	/* write metadata for render */
	meta_buf.paddr = (unsigned char *)buffer->meta_buf.va;
	meta_buf.size = (unsigned int)buffer->meta_buf.size;

	ret = mtk_pq_common_write_metadata_addr(&meta_buf,
		EN_PQ_METATAG_SVP_INFO, (void *)&pq_svp_md);
	if (ret) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Write svp meta tag fail\n");
		goto out;
	}

out:
	return ret;
}

int mtk_pq_svp_out_streamon(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	bool s_dev = false;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	s_dev = pq_dev->display_info.secure_info.svp_enable;
	s_dev |= pq_dev->display_info.secure_info.force_svp;

	if (s_dev) {
		/* authorize pq internal buffer */
		ret = _mtk_pq_auth_internal_buf(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "auth pq internal buf fail\n");
			return ret;
		}

		pq_dev->display_info.secure_info.first_frame = true;
	}
	return 0;
}

int mtk_pq_svp_out_streamoff(struct mtk_pq_device *pq_dev)
{
	int ret = 0;
	bool s_dev = false;

	if (pq_dev == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Received NULL pointer!\n");
		return -EINVAL;
	}

	s_dev = pq_dev->display_info.secure_info.svp_enable;
	s_dev |= pq_dev->display_info.secure_info.force_svp;

	if (s_dev) {
		/* unauthorize pq internal buffer */
		ret = _mtk_pq_unauth_internal_buf(pq_dev);
		if (ret) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "unauthorize pq internal buf fail\n");
			return ret;
		}
	}
	return 0;
}

static int _mtk_pq_svp_status_show_info(
	int win, struct mtk_pq_device *pqdev, char *buf, ssize_t used_count)
{
	int count = 0, ScnSize = 0;

	if ((pqdev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return 0;
	}

	ScnSize = scnprintf(buf + used_count + count,
		SYSFS_MAX_BUF_COUNT - count - used_count,
		" Window ID      :                 %02u\n"
		" PQ Secure Mode :            %7s\n"
		" Force SVP      :            %7s\n"
		" TS Domain AID  :                 %02d\n"
		" Video Pipeline :         0x%08x\n"
		" Disp  Pipeline :         0x%08x\n"
		" Dolby Pipeline :         0x%08x\n"
		" PQ Internal Buf:             %6s\n"
		" Dolby Buf      :             %6s\n"
		"-------------------------------------\n",
		win,
		pqdev->display_info.secure_info.svp_enable?"Enable":"Disable",
		pqdev->display_info.secure_info.force_svp?"Enable":"Disable",
		pqdev->display_info.secure_info.aid,
		pqdev->display_info.secure_info.vdo_pipeline_ID,
		pqdev->display_info.secure_info.disp_pipeline_ID,
		pqdev->dv_win_info.secure.svp_id,
		pqdev->display_info.secure_info.pq_buf_authed?"Locked":"Unlock",
		pqdev->dv_win_info.secure.svp_buf_authed?"Locked":"Unlock");
	count += ScnSize;
	if (!_mtk_pq_svp_check_scnsize(
			ScnSize, SYSFS_MAX_BUF_COUNT - count - used_count))
		return count;

	return count;
}

int mtk_pq_svp_show_cmd(struct device *dev, char *buf)
{
	struct mtk_pq_platform_device *pqdev = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	int count = 0, win_id = 0, ScnSize = 0;
	bool win_open = false;

	if ((dev == NULL) || (buf == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		return -EINVAL;
	}

	ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
		"===== PQ SVP debug command start =====\n");
	count += ScnSize;
	if (!_mtk_pq_svp_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
		return count;

	for (win_id = 0; win_id < pqdev->xc_win_num; win_id++) {
		pq_dev = pqdev->pq_devs[win_id];
		if (!pq_dev) {
			STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pq_dev is NULL!\n");
			continue;
		}
		if (pq_dev->stream_on_ref) {
			count += _mtk_pq_svp_status_show_info(win_id, pq_dev, buf, count);
			win_open = true;
		}
	}
	if (!win_open) {
		ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
			"No Window Opened!\n");
		count += ScnSize;
		if (!_mtk_pq_svp_check_scnsize(ScnSize, SYSFS_MAX_BUF_COUNT - count))
			return count;
	}

	ScnSize = scnprintf(buf + count, SYSFS_MAX_BUF_COUNT - count,
		"\nForce SVP cmd example:\n"
		"echo window=0 force_svp=1 > /sys/devices/platform/mtk-pq/mtk_dbg/mtk_pq_svp\n"
		"===== PQ SVP debug command end =======\n");
	count += ScnSize;
	return count;
}

int mtk_pq_svp_store_cmd(struct device *dev, const char *buf)
{
	int ret = 0, len = 0, ScnSize = 0, idx = 0;
	__u32 win_id = 0, force_svp = 0;
	bool find = false;
	char *cmd = NULL;
	struct mtk_pq_device *pq_dev = NULL;
	struct mtk_pq_platform_device *pqdev = NULL;

	if ((buf == NULL) || (dev == NULL)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "Pointer is NULL!\n");
		return -EINVAL;
	}

	PQ_MALLOC(cmd, sizeof(char) * SYSFS_MAX_BUF_COUNT);
	if (cmd == NULL) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "vmalloc cmd fail!\n");
		return -EINVAL;
	}
	memset(cmd, 0, sizeof(char) * SYSFS_MAX_BUF_COUNT);

	len = strlen(buf);
	ScnSize = snprintf(cmd, len + 1, "%s", buf);
	if (!_mtk_pq_svp_check_scnsize(ScnSize, len + 1)) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"_mtk_pq_enhance_check_scnsize fail!\n");
		ret = -ENOMEM;
		goto exit;
	}

	find = _mtk_pq_svp_get_dbg_value_from_string(cmd, "window", len, &win_id);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, window should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	find = _mtk_pq_svp_get_dbg_value_from_string(cmd, "force_svp", len, &force_svp);
	if (!find) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR,
			"Cmdline format error, bypass should be set, please echo help!\n");
		ret = -EINVAL;
		goto exit;
	}

	pqdev = dev_get_drvdata(dev);
	if (!pqdev) {
		STI_PQ_LOG(STI_PQ_LOG_LEVEL_ERROR, "pqdev is NULL!\n");
		ret = -EINVAL;
		goto exit;
	}

	if (win_id < PQ_WIN_MAX_NUM) {
		pq_dev = pqdev->pq_devs[win_id];
		if (pq_dev && force_svp) {
			pq_dev->display_info.secure_info.force_svp = true;
			pq_dev->display_info.secure_info.first_frame = true;
		}
		if (pq_dev && !force_svp)
			pq_dev->display_info.secure_info.force_svp = false;
	}

	if (win_id == PQ_WIN_MAX_NUM) {
		for (idx = 0; idx < PQ_WIN_MAX_NUM; idx++) {
			pq_dev = pqdev->pq_devs[idx];
			if (pq_dev && force_svp) {
				pq_dev->display_info.secure_info.force_svp = true;
				pq_dev->display_info.secure_info.first_frame = true;
			}
			if (pq_dev && !force_svp)
				pq_dev->display_info.secure_info.force_svp = false;
		}
	}

exit:
	vfree(cmd);
	return ret;
}


MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

#endif // IS_ENABLED(CONFIG_OPTEE)
