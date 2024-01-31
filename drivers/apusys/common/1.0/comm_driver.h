/* SPDX-License-Identifier: GPL-2.0*/

#ifndef __APUSYS_COMM_DRV_H__
#define __APUSYS_COMM_DRV_H__

#include <linux/mm.h>
#include "comm_mem_mgt.h"
#include "comm_user.h"
#include "comm_teec.h"

struct  comm_mem_cb {
	int (*alloc)(struct comm_kmem *mem);
	int (*free)(struct comm_kmem *mem, u32 closefd);
	int (*import)(struct comm_kmem *mem);
	int (*cache_flush)(struct comm_kmem *mem);
	int (*unimport)(struct comm_kmem *mem);
	int (*map_iova)(struct comm_kmem *mem);
	int (*map_kva)(struct comm_kmem *mem);
	int (*unmap_iova)(struct comm_kmem *mem);
	int (*unmap_kva)(struct comm_kmem *mem);
	int (*map_uva)(struct vm_area_struct *vma);
	int (*query_mem)(struct comm_kmem *mem, u32 type, int (*cb)(struct comm_kmem *));
	int (*vlm_info)(struct comm_kmem *mem);
	int (*hw_supported)(void);
	int (*open_teec_context)(u32 index);
	void (*close_teec_context)(u32 index);
	int (*send_teec_cmd)(u32 uuid_idx,
				u32 cmd_id,
				struct TEEC_Operation *op,
				u32 *error);
	int (*alloc_user_mem)(struct comm_user *u, struct comm_kmem *mem);
	int (*free_user_mem)(struct comm_user *u, struct comm_kmem *mem);
	int (*get_user_mem)(struct comm_user *u, struct comm_kmem *mem);
	int (*put_user_mem)(struct comm_user *u, struct comm_kmem *mem);
	int (*search_user_mem)(struct comm_user *u,
			struct comm_kmem *mem,
			u32 type,
			int (*cb)(struct comm_kmem *));
	int (*search_user_mem_foreach)(struct comm_user **iu,
			struct comm_kmem *mem,
			u32 type,
			int (*cb)(struct comm_kmem *));
	int (*create_user)(struct comm_user **iu);
	int (*delete_user)(struct comm_user *u);
	int (*get_user)(struct comm_user *u);
	int (*put_user)(struct comm_user *u);
};

struct comm_mem_cb *comm_util_get_cb(void);
#define COMMAND_UTILIZATION
#ifdef COMMAND_UTILIZATION
extern void *exec_time_log_start(void);
extern void exec_time_log_end(void *p);
#endif
#endif