// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_kms.h"
#include "mtk_tv_drm_log.h"
#include "mtk_tv_sm_ml.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_SCRIPTMGT_ML
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define WRN(fmt, args...) DRM_WARN("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define TO_U64(v)		((uint64_t)(long)(v))
#define TO_PTR(v)		((void *)(long)(v))

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------
enum buf_type {
	E_BUF_TYPE_REG,
	E_BUF_TYPE_CMD,
	E_BUF_TYPE_MAX
};

struct cmd_buf {
	void *alloc_addr;
	void *start_addr[];
};

struct work_node {
	struct list_head list;
	int delay;
	uint16_t count;
	enum buf_type type;
	uint8_t buffer[];
};

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static inline int _sm_ml_add_regs(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs)
{
	struct sm_ml_direct_write_mem_info sm_mem_info;
	enum sm_return_type sm_ret;

	memset(&sm_mem_info, 0, sizeof(struct sm_ml_direct_write_mem_info));
	sm_mem_info.reg            = regs;
	sm_mem_info.cmd_cnt        = count;
	sm_mem_info.va_address     = context->cur_addr;
	sm_mem_info.va_max_address = context->max_addr;
	sm_mem_info.add_32_support = FALSE;
	sm_ret = sm_ml_write_cmd(&sm_mem_info);
	if (sm_ret != E_SM_RET_OK)
		return -EINVAL;

	context->cmd_cnt += count;
	context->cur_addr = sm_mem_info.va_address; // @sm_ml_write_cmd updates the @va_address
	return 0;
}

static inline int _sm_ml_add_regs_delay(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs,
	uint8_t vsync_num)
{
	struct work_node *work_node;
	uint32_t regs_size;

	regs_size = sizeof(struct sm_reg) * count;
	work_node = kvmalloc(sizeof(struct work_node) + regs_size, GFP_KERNEL);
	if (work_node == NULL)
		return -ENOMEM;

	memcpy(work_node->buffer, regs, regs_size);
	work_node->type  = E_BUF_TYPE_REG;
	work_node->count = count;
	work_node->delay = vsync_num;
	context->list_cmd_cnt += count;
	list_add_tail((struct list_head *)work_node, &context->work_list);
	return 0;
}

static inline int _sm_ml_add_cmds(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	void *start_addr)
{
	if (context->cmd_cnt + count > context->ml_cmd_cnt)
		return -ENOBUFS;

	memcpy(TO_PTR(context->cur_addr), start_addr, count * MTK_TV_DRM_SM_ML_CMD_SIZE);
	context->cur_addr += count * MTK_TV_DRM_SM_ML_CMD_SIZE;
	context->cmd_cnt  += count;
	return 0;
}

static inline int _sm_ml_add_cmds_delay(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	void *start_addr,
	uint8_t vsync_num)
{
	struct work_node *work_node;
	uint32_t cmds_size;

	cmds_size = count * MTK_TV_DRM_SM_ML_CMD_SIZE;
	work_node = kvmalloc(sizeof(struct work_node) + cmds_size, GFP_KERNEL);
	if (work_node == NULL)
		return -ENOMEM;

	work_node->type   = E_BUF_TYPE_CMD;
	work_node->count  = count;
	work_node->delay  = vsync_num;
	memcpy(work_node->buffer, start_addr, cmds_size);
	context->list_cmd_cnt += count;
	list_add_tail((struct list_head *)work_node, &context->work_list);
	return 0;
}

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_sm_ml_init(
	struct mtk_sm_ml_context *context,
	enum sm_ml_sync sync_id,
	enum sm_ml_unique_id unique_id,
	uint8_t buffer_cnt,
	uint16_t cmd_cnt)
{
	enum sm_return_type sm_ret;
	struct sm_ml_res sm_res;
	int ret = 0;
	int fd;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == true) {
		ERR("context has already inited");
		return 0;
	}

	// create sm resource
	memset(&sm_res, 0, sizeof(struct sm_ml_res));
	sm_res.sync_id    = sync_id;
	sm_res.buffer_cnt = buffer_cnt;
	sm_res.cmd_cnt    = cmd_cnt;
	sm_res.unique_id = unique_id;

	if (sm_res.unique_id == E_SM_ML_UID_DISP_VIDEO)
		memcpy(&sm_res.cl_name[0], "DISP_VIDEO", sizeof("DISP_VIDEO"));
	else if (sm_res.unique_id == E_SM_ML_UID_DISP_IRQ)
		memcpy(&sm_res.cl_name[0], "DISP_IRQ", sizeof("DISP_IRQ"));
	else if (sm_res.unique_id == E_SM_ML_UID_VGS)
		memcpy(&sm_res.cl_name[0], "VGS", sizeof("VGS"));

	sm_ret = sm_ml_create_resource(&fd, &sm_res);
	if (sm_ret != E_SM_RET_OK) {
		if (sm_ret == E_SM_RET_FAIL_FUN_NOT_SUPPORT) {
			WRN("create resource(id: %d, buf_cnt: %d, cmd_cnt: %d) fail, ret = %d",
				sync_id, buffer_cnt, cmd_cnt, sm_ret);
			ret = -ENODEV;
		} else {
			ERR("create resource(id: %d, buf_cnt: %d, cmd_cnt: %d) fail, ret = %d",
				sync_id, buffer_cnt, cmd_cnt, sm_ret);
			ret = -EINVAL;
		}
		return ret;
	}
	LOG("sm_ml_create_resource(id: %d, buf_cnt: %d, cmd_cnt: %d), fd = %d",
			sync_id, buffer_cnt, cmd_cnt, fd);

	memset(context, 0, sizeof(struct mtk_sm_ml_context));
	mutex_init(&context->mutex);
	context->ml_sync_id = sync_id;
	context->ml_buf_cnt = buffer_cnt;
	context->ml_cmd_cnt = cmd_cnt;
	context->ml_fd      = fd;
	context->init       = true;
	context->mem_idx    = MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX;
	INIT_LIST_HEAD(&context->work_list);
	return 0;
}

int mtk_tv_sm_ml_deinit(
	struct mtk_sm_ml_context *context)
{
	enum sm_return_type sm_ret;
	struct list_head *list_node, *tmp_node;
	struct work_node *work_node;
	int ret = 0;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return 0;
	}
	mutex_lock(&context->mutex);
	context->init = false;
	mutex_unlock(&context->mutex);

	list_for_each_safe(list_node, tmp_node, &context->work_list) {
		work_node = list_entry(list_node, struct work_node, list);
		list_del(list_node);
		kfree((void *)work_node);
	}
	sm_ret = sm_ml_destroy_resource(context->ml_fd);
	if (sm_ret != E_SM_RET_OK) {
		ERR("sm_ml_destroy_resource(fd: 0x%x) fail, ret = %d", context->ml_fd, sm_ret);
		ret = -EINVAL;
	}
	mutex_destroy(&context->mutex);
	LOG("sm_ml_destroy_resource(fd: 0x%x)", context->ml_fd);
	memset(context, 0, sizeof(struct mtk_sm_ml_context));
	return ret;
}

int mtk_tv_sm_ml_get_mem_index(
	struct mtk_sm_ml_context *context)
{
	struct sm_ml_info sm_mem_info = {0};
	struct list_head *list_node, *tmp_node;
	struct work_node *work_node;
	enum sm_return_type sm_ret;
	uint8_t mem_idx;
	int ret = 0;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return -EINVAL;
	}
	mutex_lock(&context->mutex);
	if (context->mem_idx == MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX) {
		// get memory index
		sm_ret = sm_ml_get_mem_index(context->ml_fd, &mem_idx, false);
		if (sm_ret != E_SM_RET_OK) {
			context->mem_fail_cnt++;
			ERR("sm_ml_get_mem_index(fd:0x%x) fail_cnt:%d, list _cmd:%d, ret = %d",
				context->ml_fd, context->mem_fail_cnt,
				context->list_cmd_cnt, sm_ret);
			ret = -EINVAL;
			goto EXIT;
		}
		context->mem_fail_cnt = 0;
		// get memory info
		sm_ret = sm_ml_get_info(context->ml_fd, mem_idx, &sm_mem_info);
		if (sm_ret != E_SM_RET_OK) {
			ERR("sm_ml_get_info(fd: 0x%x, mem_idx: %d) fail, ret = %d",
				context->ml_fd, mem_idx, sm_ret);
			ret = -EINVAL;
			goto EXIT;
		}

		context->cmd_cnt  = 0;
		context->mem_idx  = mem_idx;
		context->cur_addr = sm_mem_info.start_va;
		context->max_addr = sm_mem_info.start_va + sm_mem_info.mem_size;
		LOG("fd 0x%x, mem_idx %d, cur_addr 0x%llx, sync_id %d buf_cnt %d, cmd_cnt %d",
			context->ml_fd, context->mem_idx, context->cur_addr,
			context->ml_sync_id, context->ml_buf_cnt, context->ml_cmd_cnt);
	}
	// check work list
	list_for_each_safe(list_node, tmp_node, &context->work_list) {
		work_node = list_entry(list_node, struct work_node, list);
		if (work_node->delay > 0) {
			work_node->delay--;
			continue;
		}
		switch (work_node->type) {
		case E_BUF_TYPE_REG:
			ret = _sm_ml_add_regs(context, work_node->count,
					(struct sm_reg *)work_node->buffer);
			break;
		case E_BUF_TYPE_CMD:
			ret = _sm_ml_add_cmds(context,  work_node->count, work_node->buffer);
			break;
		default:
			ret = 0;
			break;
		}
		if (ret == 0) {
			LOG("ml_fd 0x%x: mem_idx %u, type %d, add %u", context->ml_fd,
				context->mem_idx, work_node->type, work_node->count);
			list_del(list_node);
			context->list_cmd_cnt -= work_node->count;
			kvfree((void *)work_node);
		} else {
			ERR("ml_fd 0x%x: mem_idx %u, type %d, add %u fail, ret = %d",
				context->ml_fd, context->mem_idx, work_node->type,
				work_node->count, ret);
			break;
		}
	}
EXIT:
	mutex_unlock(&context->mutex);
	return ret;
}

int mtk_tv_sm_ml_write_cmd(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs)
{
	return mtk_tv_sm_ml_write_cmd_delay(context, count, regs, 0);
}

int mtk_tv_sm_ml_write_cmd_delay(
	struct mtk_sm_ml_context *context,
	uint16_t count,
	struct sm_reg *regs,
	uint8_t vsync_num)
{
	int ret = -EAGAIN;

	if (!context || !regs) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (count == 0) {
		//LOG("ml_fd:0x%x no command", context->ml_fd);
		return 0;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return -EINVAL;
	}
	mutex_lock(&context->mutex);
	if (vsync_num == 0 && context->mem_idx != MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX) {
		ret = _sm_ml_add_regs(context, count, regs);
		if (!ret) {
			LOG("ml_fd 0x%x: mem_idx %u, add %u",
				context->ml_fd, context->mem_idx, count);
		} else {
			ERR("ml_fd 0x%x: mem_idx %u, add %u fail, ret = %d",
				context->ml_fd, context->mem_idx, count, ret);
		}
	}
	if (ret != 0) {
		ret = _sm_ml_add_regs_delay(context, count, regs, vsync_num);
		if (!ret) {
			LOG("ml_fd 0x%x: no mem_idx, add %u, delay %u",
				context->ml_fd, count, vsync_num);
		} else {
			ERR("ml_fd 0x%x: no mem_idx, add %u, delay %u fail, ret = %d",
				context->ml_fd, count, vsync_num, ret);
		}
	}
	mutex_unlock(&context->mutex);
	return ret;
}

int mtk_tv_sm_ml_fire(
	struct mtk_sm_ml_context *context)
{
	struct sm_ml_fire_info sm_fire_info;
	enum sm_return_type sm_ret;
	int ret = 0;

	if (!context) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return -EINVAL;
	}
	mutex_lock(&context->mutex);
	if (context->cmd_cnt == 0) {
		// LOG("ml_fd:0x%x no command", context->ml_fd);
		goto EXIT;
	}
	if (context->mem_idx == MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX) {
		ERR("ml_fd:0x%x has invalid memory index %d", context->ml_fd, context->mem_idx);
		ret = -EINVAL;
		goto EXIT;
	}
	memset(&sm_fire_info, 0, sizeof(struct sm_ml_fire_info));
	sm_fire_info.cmd_cnt = context->cmd_cnt;
	sm_fire_info.mem_index = context->mem_idx;
	sm_fire_info.external = FALSE;
	sm_ret = sm_ml_fire(context->ml_fd, &sm_fire_info);
	if (sm_ret != E_SM_RET_OK) {
		ERR("sm_ml_fire(fd: 0x%x, cmd_cnt: %d, mem_idx: %d) fail, ret = %d",
			context->ml_fd, context->cmd_cnt, context->mem_idx, sm_ret);
		ret = -EINVAL;
		goto EXIT;
	}
	LOG("fd 0x%x, cmd_cnt %d, mem_idx %d)", context->ml_fd, context->cmd_cnt, context->mem_idx);
	context->cmd_cnt  = 0;
	context->mem_idx  = MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX;
	context->cur_addr = 0;
	context->max_addr = 0;
EXIT:
	mutex_unlock(&context->mutex);
	return 0;
}

int mtk_tv_sm_ml_get_mem_info(
	struct mtk_sm_ml_context *context,
	void **start_addr,
	void **max_addr)
{
	struct cmd_buf *cmd_buf;
	uint32_t alloc_size;
	void *alloc_addr;
	void *align_addr;

	if (!context || !start_addr || !max_addr) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return -EINVAL;
	}

	alloc_size = (context->ml_cmd_cnt * MTK_TV_DRM_SM_ML_CMD_SIZE)
			+ MTK_TV_DRM_SM_ML_CMD_SIZE  /* reserve for align */
			+ MTK_TV_DRM_SM_ML_CMD_SIZE; /* reserve for struct cmd_buf */
	alloc_addr = kvmalloc(alloc_size, GFP_KERNEL);
	if (alloc_addr == NULL) {
		ERR("alloc %u bytes fail", alloc_size);
		return -ENOMEM;
	}
	align_addr = TO_PTR(ALIGN(TO_U64(alloc_addr), MTK_TV_DRM_SM_ML_CMD_SIZE))
			+ MTK_TV_DRM_SM_ML_CMD_SIZE; /* reserve for struct cmd_buf */
	cmd_buf = container_of(align_addr, struct cmd_buf, start_addr);
	cmd_buf->alloc_addr = alloc_addr;

	*start_addr = align_addr;
	*max_addr   = align_addr + (context->ml_cmd_cnt * MTK_TV_DRM_SM_ML_CMD_SIZE);
	LOG("alloc %p, align %p", alloc_addr, align_addr);
	return 0;
}

int mtk_tv_sm_ml_put_mem_info(
	struct mtk_sm_ml_context *context,
	void *start_addr)
{
	struct cmd_buf *cmd_buf;

	cmd_buf = container_of(start_addr, struct cmd_buf, start_addr);
	LOG("alloc %p, align %p", cmd_buf->alloc_addr, start_addr);
	kvfree(cmd_buf->alloc_addr);
	return 0;
}

int mtk_tv_sm_ml_set_mem_cmds(
	struct mtk_sm_ml_context *context,
	void *start_addr,
	uint16_t cmd_cnt)
{
	int ret = 0;

	if (!context || !start_addr) {
		ERR("Invalid input");
		return -ENXIO;
	}
	if (cmd_cnt == 0) {
		LOG("ml_fd:%d no command, destroy %p", context->ml_fd, start_addr);
		return 0;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return -EINVAL;
	}
	mutex_lock(&context->mutex);
	if (context->mem_idx == MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX) {
		ret = _sm_ml_add_cmds_delay(context, cmd_cnt, start_addr, 0);
		if (!ret) {
			LOG("ml_fd 0x%x: no mem_idx, store %u", context->ml_fd, cmd_cnt);
		} else {
			ERR("ml_fd 0x%x: no mem_idx, store %u fail, ret = %d",
				context->ml_fd, cmd_cnt, ret);
		}
	} else {
		ret = _sm_ml_add_cmds(context, cmd_cnt, start_addr);
		if (!ret) {
			LOG("ml_fd 0x%x: mem_idx %u, add %u",
				context->ml_fd, context->mem_idx, cmd_cnt);
		} else {
			ERR("ml_fd 0x%x: mem_idx %u, add %u fail, ret = %d",
				context->ml_fd, context->mem_idx, cmd_cnt, ret);
		}
	}
	mutex_unlock(&context->mutex);
	return ret;
}

bool mtk_tv_sm_ml_is_valid(
	struct mtk_sm_ml_context *context)
{
	bool ret;

	if (!context) {
		ERR("Invalid input");
		return false;
	}
	if (context->init == false) {
		ERR("context has not inited");
		return false;
	}
	mutex_lock(&context->mutex);
	ret = (context->mem_idx != MTK_TV_DRM_SM_ML_INVALID_MEM_INDEX);
	mutex_unlock(&context->mutex);
	return ret;
}
