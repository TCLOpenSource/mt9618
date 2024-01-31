// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <drm/mtk_tv_drm.h>

#include "mtk_tv_drm_pqu_agent.h"
#include "mtk_tv_drm_log.h"
#include "ext_command_client_api.h"
#include "ext_command_render_if.h"
#include "pqu_msg.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_PQU_METADATA
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------
struct pqu_reply_data {
	struct list_head list;
	uint16_t agent_cmd;
	uint16_t pid;
	union {
		struct { // sync
			struct completion completion;
			struct pqu_render_agent_param *output;
		};
		struct { // async
			mtk_tv_pqu_render_reply_cb callback;
			void *priv_data;
		};
	};
};

struct pqu_ready_data {
	struct list_head list;
	enum pqu_render_command_list pqu_cmd;
	struct pqu_render_agnet_info info;
};

struct pqu_dv_reply {
	struct list_head list;
	struct msg_dv_pwm_info info;
};

struct pqu_dv_data {
	int init;
	struct completion completion;
	struct list_head list;
	struct mutex list_mutex;
};

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------
static int _reply_init;
static DEFINE_MUTEX(_reply_mutex);
static LIST_HEAD(_reply_list);

static DEFINE_MUTEX(_ready_mutex);
static LIST_HEAD(_ready_list);

static struct pqu_dv_data _dv_data;

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------
static void mtk_tv_drm_pqu_get_dv_backlight(
	int error_cause, pid_t thread_id, uint32_t user_param, void *reply_param)
{
	struct pqu_dv_data *dv = &_dv_data;
	struct msg_dv_pwm_info *info = (struct msg_dv_pwm_info *)reply_param;
	struct pqu_dv_reply *reply = NULL;

	reply = kvzalloc(sizeof(struct pqu_dv_reply), GFP_KERNEL);
	if (reply) {
		reply->info = *info;
		mutex_lock(&dv->list_mutex);
		list_add_tail(&reply->list, &dv->list);
		mutex_unlock(&dv->list_mutex);
	} else
		ERR("kvzalloc fail");

	complete(&dv->completion);
}

static void mtk_tv_drm_pqu_agent_result_callback(
	int error_cause, pid_t thread_id, uint32_t user_param, void *reply_param)
{
	struct pqu_render_agnet_info *info = (struct pqu_render_agnet_info *)reply_param;
	struct pqu_reply_data *reply = NULL;

	struct list_head *list_node, *tmp_node;
	bool is_found = false;

	LOG("agent_cmd:0x%x, pid:%d", info->agent_cmd, info->pid);

	if (unlikely(list_empty(&_reply_list))) {
		ERR("reply list is empty");
		return;
	}

	mutex_lock(&_reply_mutex);
	list_for_each_safe(list_node, tmp_node, &_reply_list) {
		reply = list_entry(list_node, struct pqu_reply_data, list);
		LOG("    agent_cmd:0x%x, pid:%d", reply->agent_cmd, reply->pid);
		if (reply->agent_cmd == info->agent_cmd && reply->pid == info->pid) {
			list_del(&reply->list);
			is_found = true;
			break;
		}
	}
	mutex_unlock(&_reply_mutex);

	if (unlikely(is_found == false)) {
		ERR("reply not found");
		return;
	}

	switch (PQU_RENDER_AGENT_CMD_GET_REPLY_TYPE(reply->agent_cmd)) {
	case PQU_RENDER_AGENT_CMD_REPLY_TYPE_SYNC:
		memcpy(reply->output, &info->param, sizeof(struct pqu_render_agent_param));
		LOG("sync reply, agent_cmd:0x%x, pid:%d, output:0x%p, done:%d",
			reply->agent_cmd, reply->pid, reply->output, reply->completion.done);
		complete_all(&reply->completion);
		break;
	case PQU_RENDER_AGENT_CMD_REPLY_TYPE_ASYNC:
		reply->callback(reply->priv_data, &info->param);
		LOG("async reply, agent_cmd:0x%x, pid:%d, priv:0x%p, cb:0x%p",
			reply->agent_cmd, reply->pid, reply->priv_data, reply->callback);
		kvfree(reply);
		break;
	default:
		ERR("Unknown reply type: 0x%x", reply->agent_cmd);
		break;
	}
}

static void mtk_tv_drm_pqu_agent_ready_callback(void)
{
	struct callback_info cb = {.callback = mtk_tv_drm_pqu_agent_result_callback};
	struct pqu_render_agnet_info reply_info = {0};
	struct pqu_ready_data *ready = NULL;
	int ret = 0;

	mutex_lock(&_ready_mutex);
	ready = list_first_entry(&_ready_list, struct pqu_ready_data, list);
	list_del(&ready->list);
	mutex_unlock(&_ready_mutex);

	LOG("pqu_cmd:0x%x, agent_cmd:0x%x, pid:%d", ready->pqu_cmd,
		ready->info.agent_cmd, ready->info.pid);

	switch (ready->pqu_cmd) {
	case PQU_RENDER_COMMAND_LIST_AGENT_NO_REPLY:
		ret = pqu_render_agent_no_reply(&ready->info);
		break;
	case PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY:
		ret = pqu_render_agent_sync_reply(&ready->info, &reply_info);
		break;
	case PQU_RENDER_COMMAND_LIST_AGENT_ASYNC_REPLY:
		ret = pqu_render_agent_async_reply(&ready->info, &cb);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret != 0)
		ERR("%s fail, ret = %d", __func__, ret);

	kvfree(ready);
}

static inline int mtk_tv_drm_pqu_agent_send(
	enum pqu_render_command_list pqu_cmd,
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input,
	struct pqu_render_agent_param *output,
	mtk_tv_pqu_render_reply_cb callback,
	void *priv_data)
{
	struct callback_info cb = { .callback = mtk_tv_drm_pqu_agent_result_callback };
	struct pqu_render_agnet_info info_in = {0};
	struct pqu_render_agnet_info info_out = {0};
	struct pqu_reply_data *reply = NULL;
	struct pqu_ready_data *ready = NULL;
	int ret = 0;

	memset(&info_in, 0, sizeof(struct pqu_render_agnet_info));
	info_in.agent_cmd = agent_cmd;
	info_in.pid = current->pid;
	memcpy(&info_in.param, input, sizeof(struct pqu_render_agent_param));

	LOG("pqu_cmd:%d, agent_cmd:0x%x, pid:%d", pqu_cmd, info_in.agent_cmd, info_in.pid);

	// prepare reply data if it is in need
	if ((pqu_cmd == PQU_RENDER_COMMAND_LIST_AGENT_ASYNC_REPLY) ||
		(bPquEnable == false && pqu_cmd == PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY)) {
		reply = kvzalloc(sizeof(struct pqu_reply_data), GFP_KERNEL);
		if (reply == NULL) {
			ERR("list node alloc (size: %zu) fail", sizeof(struct pqu_reply_data));
			return -ENOMEM;
		}
		reply->agent_cmd = info_in.agent_cmd;
		reply->pid = info_in.pid;
		if (pqu_cmd == PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY) {
			init_completion(&reply->completion);
			reply->output = output;
			LOG("sync send, agent_cmd:0x%x, pid:%d, output:0x%p",
				reply->agent_cmd, reply->pid, reply->output);
		} else {
			reply->callback = callback;
			reply->priv_data = priv_data;
			LOG("async send, agent_cmd:0x%x, pid:%d, priv:0x%p, cb:0x%p",
				reply->agent_cmd, reply->pid, reply->priv_data, reply->callback);
		}
		mutex_lock(&_reply_mutex);
		list_add_tail(&reply->list, &_reply_list);
		mutex_unlock(&_reply_mutex);
	}

	if (bPquEnable == false) {
		if (unlikely(_reply_init == false)) {
			pqu_msg_register_notify_func(PQU_MSG_REPLY_RENDER_AGENT,
				mtk_tv_drm_pqu_agent_result_callback);
			_reply_init = true;
		}
		pqu_msg_send(PQU_MSG_SEND_RENDER_AGENT, &info_in);

		if (pqu_cmd == PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY) {
			ret = wait_for_completion_interruptible(&reply->completion);
			kvfree(reply);
		}
		return ret;
	}

	switch (pqu_cmd) {
	case PQU_RENDER_COMMAND_LIST_AGENT_NO_REPLY:
		ret = pqu_render_agent_no_reply(&info_in);
		break;
	case PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY:
		ret = pqu_render_agent_sync_reply(&info_in, &info_out);
		break;
	case PQU_RENDER_COMMAND_LIST_AGENT_ASYNC_REPLY:
		ret = pqu_render_agent_async_reply(&info_in, &cb);
		break;
	default:
		return -EINVAL;
	}
	if (ret == -ENODEV) {
		ready = kvzalloc(sizeof(struct pqu_ready_data), GFP_KERNEL);
		if (ready == NULL) {
			ERR("list node alloc fail");
			return -ENOMEM;
		}
		ready->pqu_cmd = pqu_cmd;
		memcpy(&ready->info, &info_in, sizeof(struct pqu_render_agnet_info));
		mutex_lock(&_ready_mutex);
		list_add_tail(&ready->list, &_ready_list);
		mutex_unlock(&_ready_mutex);
		ret = fw_ext_command_register_notify_ready_callback(
			0, mtk_tv_drm_pqu_agent_ready_callback);
		if (ret) {
			ERR("PQU register ready-callback fail, ret = %d", ret);
			return ret;
		}
	} else if (ret) {
		ERR("pqu_render_agent_no_reply fail, ret = %d", ret);
		return ret;
	}
	if (pqu_cmd == PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY)
		memcpy(output, &info_out.param, sizeof(struct pqu_render_agent_param));

	return 0;
}

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
int mtk_tv_drm_pqu_agent_no_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input)
{
	return mtk_tv_drm_pqu_agent_send(PQU_RENDER_COMMAND_LIST_AGENT_NO_REPLY,
		agent_cmd, input, NULL, NULL, NULL);
}

int mtk_tv_drm_pqu_agent_sync_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input,
	struct pqu_render_agent_param *output)
{
	return mtk_tv_drm_pqu_agent_send(PQU_RENDER_COMMAND_LIST_AGENT_SYNC_REPLY,
		PQU_RENDER_AGENT_CMD_SET_REPLY_TYPE_SYNC(agent_cmd),
		input, output, NULL, NULL);
}

int mtk_tv_drm_pqu_agent_async_reply(
	enum pqu_render_agent_cmd agent_cmd,
	struct pqu_render_agent_param *input,
	mtk_tv_pqu_render_reply_cb callback,
	void *priv_data)
{
	return mtk_tv_drm_pqu_agent_send(PQU_RENDER_COMMAND_LIST_AGENT_ASYNC_REPLY,
		PQU_RENDER_AGENT_CMD_SET_REPLY_TYPE_ASYNC(agent_cmd),
		input, NULL, callback, priv_data);
}

int mtk_tv_drm_pqu_get_dv_pwm_info_ioctl(struct drm_device *drm_dev,
	void *data, struct drm_file *file_priv)
{
	struct pqu_dv_data *dv = &_dv_data;
	struct drm_mtk_tv_dv_pwm_info *output = (struct drm_mtk_tv_dv_pwm_info *)data;
	struct pqu_dv_reply *reply = NULL;
	int ret = 0;

	if (dv->init == false) {
		mutex_init(&dv->list_mutex);
		INIT_LIST_HEAD(&dv->list);
		init_completion(&dv->completion);
		pqu_msg_register_notify_func(PQU_MSG_REPLY_DV_PWM, mtk_tv_drm_pqu_get_dv_backlight);
		dv->init = true;
	}

	pqu_msg_send(PQU_MSG_SEND_DV_PWM, NULL);

	ret = wait_for_completion_interruptible_timeout(&dv->completion, msecs_to_jiffies(1000));
	if (ret <= 0)
		ERR("wait_for_completion fail, ret=%d", ret);

	mutex_lock(&dv->list_mutex);

	if (list_empty(&dv->list)) {
		ret = -EINVAL;
		ERR("List is empty");
	} else {
		reply = list_first_entry(&dv->list, struct pqu_dv_reply, list);
		list_del(&reply->list);
		output->adim = reply->info.adim;
		output->pdim = reply->info.pdim;
		kvfree(reply);
		ret = 0;
		LOG("adim:%u, pdim:%u", output->adim, output->pdim);
	}

	mutex_unlock(&dv->list_mutex);
	return ret;
}
