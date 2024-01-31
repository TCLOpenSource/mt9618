// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_drm_metabuf.h"
#include "mtk_tv_drm_autogen.h"
#include "mtk_tv_drm_pqu_wrapper.h"
#include "pqu_msg.h"
#include "ext_command_render_if.h"
#include "hwreg_render_stub.h"
#include "mapi_pq_if.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL     MTK_DRM_MODEL_AUTOGEN
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define ST_PQ_RESET(s)    do { memset(&s, 0, sizeof(s)); s.u32Length = sizeof(s); } while (0)

#define DTS_STR_ALGO_CAPABILITY  "algo_capability"
#define DTS_STR_ALGO_HEADER_SIZE "algo_header_size"

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static int _drm_autogen_read_dts(struct mtk_autogen_context *autogen)
{
	struct device_node *np;
	int ret;

	// Get device node
	np = of_find_compatible_node(NULL, NULL, MTK_DRM_TV_KMS_COMPATIBLE_STR);
	if (np == NULL) {
		ERR("cannot find dts node '%s'", MTK_DRM_TV_KMS_COMPATIBLE_STR);
		return -EINVAL;
	}

	// Get capability array
	ret = of_property_count_u32_elems(np, DTS_STR_ALGO_CAPABILITY);
	if (ret <= 0 || ret >= AUTOGEN_CAPABILITY_SIZE_MAX) {
		ERR("algo capability size error, size = %d, max = %d",
			ret, AUTOGEN_CAPABILITY_SIZE_MAX);
		return -EINVAL;
	}
	autogen->capability_size = ret;
	ret = of_property_read_u32_array(np, DTS_STR_ALGO_CAPABILITY,
		autogen->capability, autogen->capability_size);
	if (ret != 0) {
		ERR("get capability data error, ret = %d", ret);
		return -EINVAL;
	}

	// Get header size
	ret =  of_property_read_u32(np, DTS_STR_ALGO_HEADER_SIZE, &autogen->header_size);
	if (ret != 0) {
		ERR("get header size error, ret = %d", ret);
		return -EINVAL;
	}
	LOG("capability size = 0x%x", autogen->capability_size);
	LOG("header     size = 0x%x", autogen->header_size);

	return 0;
}

static int _drm_autogen_init_alg_ctx(struct mtk_autogen_context *autogen)
{
	struct ST_PQ_CTX_SIZE_INFO   size_info;
	struct ST_PQ_CTX_SIZE_STATUS size_stat;
	struct ST_PQ_CTX_INFO   ctx_info;
	struct ST_PQ_CTX_STATUS ctx_stat;
	struct ST_PQ_FRAME_CTX_INFO   frame_info;
	struct ST_PQ_FRAME_CTX_STATUS frame_stat;
	struct ST_PQ_INIT_FRAME_CTX_INFO   frame_ctx_info;
	struct ST_PQ_INIT_FRAME_CTX_STATUS frame_ctx_stat;
	struct ST_PQ_NTS_CAPABILITY_INFO   cap_info;
	struct ST_PQ_NTS_CAPABILITY_STATUS cap_stat;
	struct ST_PQ_NTS_INITHWREG_INFO   hwreg_info;
	struct ST_PQ_NTS_INITHWREG_STATUS hwreg_stat;
	struct ST_PQ_SWREG_SETWRITEBUF_INFO   swreg_set_info;
	struct ST_PQ_SWREG_SETWRITEBUF_STATUS swreg_set_stat;
	struct ST_PQ_SWREG_INITWRITEBUF_INFO   swreg_init_info;
	struct ST_PQ_SWREG_INITWRITEBUF_STATUS swreg_init_stat;
	enum EN_PQAPI_RESULT_CODES ret;

	// reset all struct
	ST_PQ_RESET(size_info);
	ST_PQ_RESET(size_stat);
	ST_PQ_RESET(frame_info);
	ST_PQ_RESET(frame_stat);
	ST_PQ_RESET(ctx_info);
	ST_PQ_RESET(ctx_stat);
	ST_PQ_RESET(cap_info);
	ST_PQ_RESET(cap_stat);
	ST_PQ_RESET(hwreg_info);
	ST_PQ_RESET(hwreg_stat);
	ST_PQ_RESET(swreg_set_info);
	ST_PQ_RESET(swreg_set_stat);
	ST_PQ_RESET(swreg_init_info);
	ST_PQ_RESET(swreg_init_stat);

	// 1. allocate buffer
	MApi_PQ_GetCtxSize(NULL, &size_info, &size_stat);
	if ((size_stat.u32PqCtxSize == 0) || (size_stat.u32SwRegSize == 0) ||
		(size_stat.u32HwReportSize == 0) || (size_stat.u32MetadataSize == 0)) {
		ERR("invalid size (ctx %u, reg %u, hw repot %u, metadata %u)",
			size_stat.u32PqCtxSize, size_stat.u32SwRegSize,
			size_stat.u32HwReportSize, size_stat.u32MetadataSize);
		return -EINVAL;
	}
	autogen->ctx       = kvmalloc(size_stat.u32PqCtxSize, GFP_KERNEL);
	autogen->metadata  = kvmalloc(size_stat.u32MetadataSize, GFP_KERNEL);
	autogen->hw_report = kvmalloc(size_stat.u32HwReportSize, GFP_KERNEL);
	if (autogen->ctx == NULL || autogen->metadata == NULL || autogen->hw_report == NULL) {
		ERR("allocate buffer fail (ctx: %p, %d)(metadata: %p, %d)(hw_report: %p, %d)",
			autogen->ctx, size_stat.u32PqCtxSize,
			autogen->metadata, size_stat.u32MetadataSize,
			autogen->hw_report, size_stat.u32HwReportSize);
		return -EINVAL;
	}
	if (mtk_tv_drm_metabuf_alloc_by_mmap(&autogen->metabuf,
		MTK_TV_DRM_METABUF_MMAP_TYPE_AUTOGEN)) {
		ERR("Get MMAP %d fail", MTK_TV_DRM_METABUF_MMAP_TYPE_AUTOGEN);
		return -ENOMEM;
	}
	if (autogen->metabuf.size < size_stat.u32SwRegSize + autogen->header_size) {
		ERR("LDM MMAP too small (%u < %u + %u)",
			autogen->metabuf.size, size_stat.u32SwRegSize, autogen->header_size);
		return -ENOMEM;
	}
	autogen->sw_reg = autogen->metabuf.addr + autogen->header_size;

	// 2. init ctx
	ctx_info.eWinID = E_PQ_MAIN_WINDOW;
	ret = MApi_PQ_InitCtx(autogen->ctx, &ctx_info, &ctx_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_InitCtx fail (0x%x)", ret);
		return -EINVAL;
	}

	// 3. set frame ctx
	frame_info.pSwReg    = autogen->sw_reg;
	frame_info.pMetadata = autogen->metadata;
	frame_info.pHwReport = autogen->hw_report;
	ret = MApi_PQ_SetFrameCtx(autogen->ctx, &frame_info, &frame_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SetFrameCtx fail (0x%x)", ret);
		return -EINVAL;
	}
	ret = MApi_PQ_InitFrameCtx(autogen->ctx, &frame_ctx_info, &frame_ctx_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_InitFrameCtx fail (%u)", ret);
		return -EINVAL;
	}

	// 4. set none-TS capability
	cap_info.eDomain           = E_PQ_DOMAIN_RENDER;
	cap_info.u16CapabilitySize = autogen->capability_size;
	cap_info.pCapability       = autogen->capability;
	ret = MApi_PQ_SetCapability_NonTs(&cap_info, &cap_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SetCapability_NonTs fail (%u)", ret);
		return -EINVAL;
	}

	// 5. set none-TS HW reg regtable
	hwreg_info.eDomain = E_PQ_DOMAIN_RENDER;
	ret = MApi_PQ_InitHWRegTable_NonTs(&hwreg_info, &hwreg_stat);
	if (CHECK_RC_FAIL(ret)) {
		if (ret == E_PQAPI_RC_FAIL_HW_NOT_SUPPORT) {
			ERR("MApi_PQ_InitHWRegTable_NonTs get HW_NOT_SUPPORT\n");
			return -ENODEV;
		}
		ERR("MApi_PQ_InitHWRegTable_NonTs fail (%u)", ret);
		return -EINVAL;
	}

	// 6. init sw reg
	swreg_set_info.u32Version = API_VERSION_MAPI_PQ_SWREG_SETWRITEBUF;
	swreg_set_info.u32MaxSize = size_stat.u32SwRegSize;
	swreg_set_info.pAddr      = autogen->sw_reg;
	ret = MApi_PQ_SWReg_SetWriteBuf(autogen->ctx, &swreg_set_info, &swreg_set_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SWReg_SetWriteBuf fail (0x%x)", ret);
		return -EINVAL;
	}
	swreg_init_info.u32Version = API_VERSION_MAPI_PQ_SWREG_INITWRITEBUF;
	ret = MApi_PQ_SWReg_InitWriteBuf(autogen->ctx, &swreg_init_info, &swreg_init_stat);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SWReg_InitWriteBuf fail (0x%x)", ret);
		return -EINVAL;
	}

	return 0;
}

static int _drm_gutogen_set_swreg_to_dummy(struct mtk_autogen_context *autogen)
{
	struct hwregDummyValueIn dummy_reg_in;
	struct hwregOut dummy_reg_out;
	struct reg_info reg[sizeof(uint64_t)];

	memset(&dummy_reg_in, 0, sizeof(struct hwregDummyValueIn));
	memset(&dummy_reg_out, 0, sizeof(struct hwregOut));

	dummy_reg_in.RIU = true;
	dummy_reg_in.dummy_type = DUMMY_TYPE_AUTOGEN_SWREG_PHYS_ADDR;
	dummy_reg_in.dummy_value = autogen->metabuf.mmap_info.phy_addr;
	dummy_reg_out.reg = reg;
	if (drv_hwreg_render_display_set_dummy_value(dummy_reg_in, &dummy_reg_out)) {
		ERR("Set Tool dummy reg fail");
		return -EINVAL;
	}
	return 0;
}

static int _drm_autogen_set_swreg_to_pqu(struct mtk_autogen_context *autogen)
{
#if PQU_RENDER_ALGO_CTX_CAPABILITY_SIZE_MAX != MSG_RENDER_ALGO_CTX_CAPABILITY_SIZE_MAX
#error "The array size of PQU-Render's ALG-CTX-CAPABILITY is mismatch"
#endif
	struct msg_render_algo_ctx_init rpmsg;
	struct pqu_render_algo_ctx_init cpuif;
	uint16_t capability_size = autogen->capability_size;
	int i, ret;

	if (autogen->capability_size > PQU_RENDER_ALGO_CTX_CAPABILITY_SIZE_MAX) {
		ERR("capability size too long, %d >= %d", autogen->capability_size,
			PQU_RENDER_ALGO_CTX_CAPABILITY_SIZE_MAX);
		capability_size = PQU_RENDER_ALGO_CTX_CAPABILITY_SIZE_MAX;
	}

	memset(&rpmsg, 0, sizeof(struct msg_render_algo_ctx_init));
	memset(&cpuif, 0, sizeof(struct pqu_render_algo_ctx_init));
	rpmsg.mmap_phys_addr  = cpuif.mmap_phys_addr  = autogen->metabuf.mmap_info.phy_addr;
	rpmsg.mmap_phys_size  = cpuif.mmap_phys_size  = autogen->metabuf.size;
	rpmsg.header_size     = cpuif.header_size     = autogen->header_size;
	rpmsg.capability_size = cpuif.capability_size = autogen->capability_size;
	for (i = 0; i < capability_size; ++i)
		rpmsg.capability[i] = cpuif.capability[i] = autogen->capability[i];

	ret = MTK_TV_DRM_PQU_WRAPPER(PQU_MSG_SEND_RENDER_ALGO_CTX_INIT, &rpmsg, &cpuif);
	return ret;
}

//--------------------------------------------------------------------------------------------------
//  Global Functions
//--------------------------------------------------------------------------------------------------
int mtk_tv_drm_autogen_init(struct mtk_autogen_context *autogen)
{
	int ret = 0;

	if (autogen == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (autogen->init == true) {
		LOG("Autogen had inited");
		return 0;
	}

	memset(autogen, 0, sizeof(struct mtk_autogen_context));
	mutex_init(&autogen->mutex);
	autogen->init = true;

	ret = _drm_autogen_read_dts(autogen);
	if (ret) {
		ERR("_drm_autogen_read_dts fail");
		goto EXIT;
	}

	ret = _drm_autogen_init_alg_ctx(autogen);
	if (ret) {
		ERR("_drm_autogen_init_alg_ctx fail");
		goto EXIT;
	}

	ret = _drm_gutogen_set_swreg_to_dummy(autogen);
	if (ret) {
		ERR("_drm_gutogen_set_swreg_to_dummy fail");
		goto EXIT;
	}

	ret = _drm_autogen_set_swreg_to_pqu(autogen);
	if (ret) {
		ERR("_drm_autogen_set_swreg_to_pqu fail");
		goto EXIT;
	}

EXIT:
	if (ret) {
		mtk_tv_drm_autogen_deinit(autogen);
		autogen->init = ret; // record fail reason
	}
	return 0;
}

int mtk_tv_drm_autogen_deinit(struct mtk_autogen_context *autogen)
{
	struct ST_PQ_NTS_DEINITHWREG_INFO   hwreg_deinit_info;
	struct ST_PQ_NTS_DEINITHWREG_STATUS hwreg_deinit_stat;

	if (autogen->init == false) {
		LOG("Autogen is not inited");
		return 0;
	}

	ST_PQ_RESET(hwreg_deinit_info);
	ST_PQ_RESET(hwreg_deinit_stat);

	hwreg_deinit_info.eDomain = E_PQ_DOMAIN_RENDER;
	MApi_PQ_DeInitHWRegTable_NonTs(&hwreg_deinit_info, &hwreg_deinit_stat);

	kvfree(autogen->ctx);
	kvfree(autogen->metadata);
	kvfree(autogen->hw_report);
	mtk_tv_drm_metabuf_free(&autogen->metabuf);
	mutex_destroy(&autogen->mutex);
	autogen->ctx = NULL;
	autogen->metadata = NULL;
	autogen->hw_report = NULL;
	autogen->sw_reg = NULL;
	autogen->func_table = NULL;
	autogen->init = false;
	return 0;
}

int mtk_tv_drm_autogen_suspend(
	struct mtk_autogen_context *autogen)
{
	// do nothing
	return 0;
}

int mtk_tv_drm_autogen_resume(
	struct mtk_autogen_context *autogen)
{
	int ret = 0;

	if (autogen == NULL) {
		ERR("Invalid input");
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}

	ret = _drm_gutogen_set_swreg_to_dummy(autogen);
	if (ret) {
		ERR("_drm_gutogen_set_swreg_to_dummy fail");
		goto EXIT;
	}

	ret = _drm_autogen_set_swreg_to_pqu(autogen);
	if (ret) {
		ERR("_drm_autogen_set_swreg_to_pqu fail");
		goto EXIT;
	}
EXIT:
	if (ret) {
		mtk_tv_drm_autogen_deinit(autogen);
		autogen->init = ret; // record fail reason
	}
	return 0;
}

int mtk_tv_drm_autogen_lock(
	struct mtk_autogen_context *autogen)
{

	if (autogen == NULL) {
		ERR("invalid parameters");
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}
	mutex_lock(&autogen->mutex);
	return 0;
}

int mtk_tv_drm_autogen_unlock(
	struct mtk_autogen_context *autogen)
{

	if (autogen == NULL) {
		ERR("invalid parameters");
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}
	mutex_unlock(&autogen->mutex);
	return 0;
}

int mtk_tv_drm_autogen_set_mem_info(
	struct mtk_autogen_context *autogen,
	void *start_addr,
	void *max_addr)
{
	struct ST_PQ_ML_CTX_INFO ml_info;
	struct ST_PQ_ML_CTX_STATUS ml_status;
	int ret = 0;

	if (autogen == NULL || start_addr == NULL || max_addr == NULL) {
		ERR("invalid parameters (autogen %p, start addr %p, max addr %p)",
			autogen, start_addr, max_addr);
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}

	ST_PQ_RESET(ml_status);
	ST_PQ_RESET(ml_info);
	ml_info.pDispAddr = start_addr;
	ml_info.pDispMaxAddr = max_addr;
	// ml_info.u16PqModuleIdx = E_PQ_PIM_MODULE;
	ret = MApi_PQ_SetMLInfo(autogen->ctx, &ml_info, &ml_status);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SetMLInfo fail (%d)", ret);
		return -EINVAL;
	}
	return 0;
}

int mtk_tv_drm_autogen_get_cmd_size(
	struct mtk_autogen_context *autogen,
	uint32_t *cmd_size)
{
	struct ST_PQ_ML_PARAM ml_param;
	struct ST_PQ_ML_INFO ml_status;
	int ret = 0;

	if (autogen == NULL || cmd_size == NULL) {
		ERR("invalid parameters (atuogen %p, size ptr %p)", autogen, cmd_size);
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}

	ST_PQ_RESET(ml_status);
	ST_PQ_RESET(ml_param);
	// ml_param.u16PqModuleIdx = E_PQ_PIM_MODULE;
	ret = MApi_PQ_GetMLInfo(autogen->ctx, &ml_param, &ml_status);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_GetMLInfo fail (%d)", ret);
		return -EINVAL;
	}
	*cmd_size = ml_status.u16DispUsedCmdSize;
	return 0;
}

int mtk_tv_drm_autogen_set_nts_hw_reg(
	struct mtk_autogen_context *autogen,
	uint32_t reg_idx,
	uint32_t reg_value)
{
	struct ST_PQ_NTS_HWREG_INFO reg_info;
	struct ST_PQ_NTS_HWREG_STATUS reg_status;
	int ret = 0;

	if (autogen == NULL) {
		ERR("invalid parameters (autogen %p)", autogen);
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not ready");
		return autogen->init;
	}

	ST_PQ_RESET(reg_info);
	ST_PQ_RESET(reg_status);
	reg_info.eDomain = E_PQ_DOMAIN_RENDER;
	reg_info.u32RegIdx = reg_idx;
	reg_info.u16Value = reg_value;
	ret = MApi_PQ_SetHWReg_NonTs(autogen->ctx, &reg_info, &reg_status);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SetHWReg_NonTs fail (%d)", ret);
		return -EINVAL;
	}
	LOG("MApi_PQ_SetHWReg_NonTs(RegIdx: 0x%x, Value: 0x%x)", reg_idx, reg_value);
	return 0;
}

int mtk_tv_drm_autogen_set_sw_reg(
	struct mtk_autogen_context *autogen,
	uint32_t reg_idx,
	uint32_t reg_value)
{
	struct ST_PQ_SWREG_INFO reg_info;
	struct ST_PQ_SWREG_STATUS reg_status;
	int ret = 0;

	if (autogen == NULL) {
		ERR("invalid parameters (autogen %p)", autogen);
		return -EINVAL;
	}
	if (autogen->init != true) {
		ERR("Autogen is not inited");
		return autogen->init;
	}

	ST_PQ_RESET(reg_status);
	ST_PQ_RESET(reg_info);
	reg_info.u32RegIdx = reg_idx;
	reg_info.u16Value = reg_value;
	ret = MApi_PQ_SetSWReg(autogen->ctx, &reg_info, &reg_status);
	if (CHECK_RC_FAIL(ret)) {
		ERR("MApi_PQ_SetSWReg fail (%d)", ret);
		return -EINVAL;
	}
	LOG("MApi_PQ_SetSWReg(RegIdx: 0x%x, Value: 0x%x)", reg_idx, reg_value);
	return 0;
}
