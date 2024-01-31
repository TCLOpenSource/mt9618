// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>

#include "mtk_tv_drm_pqu_wrapper.h"
#include "mtk_tv_drm_log.h"

//--------------------------------------------------------------------------------------------------
//  Local Defines
//--------------------------------------------------------------------------------------------------
#define MTK_DRM_MODEL MTK_DRM_MODEL_PQU_METADATA
#define ERR(fmt, args...) DRM_ERROR("[%s][%d] " fmt, __func__, __LINE__, ##args)
#define LOG(fmt, args...) DRM_INFO("[%s][%d] " fmt, __func__, __LINE__, ##args)

#define PQU_WRAPPER_IMPLEMENT(_msg_id_, _cpuif_)						\
	static DEFINE_MUTEX(ready_mutex_##_cpuif_);						\
	static LIST_HEAD(ready_list_##_cpuif_);							\
	static void rv55_ready_##_cpuif_(void)							\
	{											\
		struct pqu_ready_data *ready = NULL;						\
		int ret = 0;									\
		mutex_lock(&ready_mutex_##_cpuif_);						\
		ready = list_first_entry(&ready_list_##_cpuif_, struct pqu_ready_data, list);	\
		list_del(&ready->list);								\
		mutex_unlock(&ready_mutex_##_cpuif_);						\
		ret = _cpuif_((void *)ready->data, NULL);					\
		if (ret != 0)									\
			ERR("%s fail, ret = %d", #_cpuif_, ret);				\
		kvfree(ready);									\
	}											\
	int mtk_tv_drm_pqu_wrapper_##_msg_id_(							\
		void *param_ptr_lite,								\
		uint32_t param_size_lite,							\
		void *param_ptr_rv55,								\
		uint32_t param_size_rv55)							\
	{											\
		uint32_t data_size = sizeof(struct list_head);					\
		struct pqu_ready_data *ready = NULL;						\
		if (bPquEnable) {								\
			int ret = _cpuif_(param_ptr_rv55, NULL);				\
			if (ret == -ENODEV) {							\
				data_size += param_size_rv55;					\
				ready = kvzalloc(data_size, GFP_KERNEL);			\
				if (ready == NULL) {						\
					ERR("list node alloc fail");				\
					return -ENOMEM;						\
				}								\
				memcpy((void *)ready->data, param_ptr_rv55, param_size_rv55);	\
				mutex_lock(&ready_mutex_##_cpuif_);				\
				list_add_tail(&ready->list, &ready_list_##_cpuif_);		\
				mutex_unlock(&ready_mutex_##_cpuif_);				\
				ret = fw_ext_command_register_notify_ready_callback(		\
					0, rv55_ready_##_cpuif_);				\
				if (ret) {							\
					ERR("PQU register ready-callback fail, ret = %d", ret);	\
					return ret;						\
				}								\
			} else if (ret) {							\
				ERR("%s fail, ret = %d", #_cpuif_, ret);			\
				return ret;							\
			}									\
		} else {									\
			pqu_msg_send(_msg_id_, param_ptr_lite);					\
		}										\
		return 0;									\
	}

//--------------------------------------------------------------------------------------------------
//  Local Enum and Structures
//--------------------------------------------------------------------------------------------------
struct pqu_ready_data {
	struct list_head list;
	uint8_t data[0];
};

//--------------------------------------------------------------------------------------------------
//  Local Variables
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//  Local Functions
//--------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//  Global Functions
//-------------------------------------------------------------------------------------------------
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_PQ_FIRE,            pqu_render_dd_pq_update_info)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_PQSHAREMEM_INFO,    pqu_render_dd_pq_update_sharemem)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_FRAMEINFO_SHAREMEM, pqu_render_frameinfo_sharemem)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_BACKLIGHT,          pqu_render_backlight)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_DEMURA,                    pqu_render_demura)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_BE_SHAREMEM_INFO,   pqu_render_be_sharemem_info)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_BE_SETTING,         pqu_render_be_setting)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_ALGO_CTX_INIT,      pqu_render_algo_ctx_init)
PQU_WRAPPER_IMPLEMENT(PQU_MSG_SEND_RENDER_OS_WRAPPER_INIT,    pqu_render_os_wrapper_init)
