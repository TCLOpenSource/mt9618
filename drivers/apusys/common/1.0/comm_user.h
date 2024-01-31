/* SPDX-License-Identifier: GPL-2.0*/
#ifndef __COMM_USER_H__
#define __COMM_USER_H__

#include "comm_mem_mgt.h"

struct comm_user {
	/* user info */
	uint64_t id;
	pid_t open_pid;
	pid_t open_tgid;
	char comm[TASK_COMM_LEN];

	/* memory mgt */
	struct list_head m_list;
	struct mutex m_mtx;
	struct kref refcount;

	struct list_head u_list;
};

struct comm_user_mem {
	struct comm_user *user;
	struct comm_kmem mem;
	struct list_head list;
	struct kref refcount;

};

int comm_user_create(struct comm_user **iu);
int comm_user_delete(struct comm_user *u);
int comm_user_get(struct comm_user *u);
int comm_user_put(struct comm_user *u);
int comm_user_mem_free(struct comm_user *u, struct comm_kmem *mem);
int comm_user_mem_alloc(struct comm_user *u, struct comm_kmem *mem);
int comm_user_mem_get(struct comm_user *u, struct comm_kmem *mem);
int comm_user_mem_put(struct comm_user *u, struct comm_kmem *mem);
int comm_user_mem_search(struct comm_user *u,
			struct comm_kmem *mem,
			u32 type,
			int (*cb)(struct comm_kmem *));
int comm_user_mem_search_foreach(struct comm_user **iu,
			struct comm_kmem *mem,
			u32 type,
			int (*cb)(struct comm_kmem *));

#endif
