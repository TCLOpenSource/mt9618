// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <linux/list.h>
#include "comm_driver.h"
#include "comm_debug.h"
#include "comm_user.h"
#include "plat_mem_wrap.h"

static LIST_HEAD(user_list);
static DEFINE_MUTEX(user_mtx);

static void comm_user_destroy(struct kref *kref);

void comm_user_mem_release(struct kref *kref)
{
	struct comm_user_mem *um =
			container_of(kref, struct comm_user_mem, refcount);

	comm_util_get_cb()->free(&um->mem, 1);
	list_del(&um->list);
	vfree(um);
}

static int comm_kmemcmp(struct comm_kmem *mm1,
		struct comm_kmem *mm2,
		u32 cmp_type)
{
	int ret = -EFAULT;
	u64 offset = 0;

	switch (cmp_type) {
	case COMM_CMP_FD:
		ret = !(mm1->fd == mm2->fd);
		break;
	case COMM_CMP_IOVA:
		if (mm2->mem_type != APU_COMM_MEM_VLM)
			offset = IOVA_START_ADDR;

		ret = !((mm1->mem_type == mm2->mem_type) &&
			((mm2->iova | offset) >= mm1->iova) &&
			(mm2->size <= mm1->size) &&
			((mm2->iova | offset) <= mm1->iova + mm1->size));
		break;
	case COMM_CMP_KVA:
		ret = !((mm2->kva >= mm1->kva) &&
			(mm2->size <= mm1->size) &&
			(mm2->kva <= mm1->kva + mm1->size));
		break;
	default:
		break;
	}

	return ret;
}

int comm_user_mem_get(struct comm_user *u, struct comm_kmem *mem)
{
	int ret = -EFAULT;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct comm_user_mem *um = NULL;

	if (IS_ERR_OR_NULL(u) || (IS_ERR_OR_NULL(mem)))
		return -EFAULT;

	mutex_lock(&u->m_mtx);
	list_for_each_safe(list_ptr, tmp, &u->m_list) {
		um = list_entry(list_ptr, struct comm_user_mem, list);

		if (comm_kmemcmp(&um->mem, mem, COMM_CMP_IOVA) == 0) {
			memcpy(mem, &um->mem, sizeof(*mem));
			kref_get(&u->refcount);
			kref_get(&um->refcount);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&u->m_mtx);

	return ret;
}

int comm_user_mem_put(struct comm_user *u, struct comm_kmem *mem)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct comm_user_mem *um = NULL;
	int ret = -EFAULT;

	if (IS_ERR_OR_NULL(u) || (IS_ERR_OR_NULL(mem)))
		return -EFAULT;

	mutex_lock(&u->m_mtx);
	list_for_each_safe(list_ptr, tmp, &u->m_list) {
		um = list_entry(list_ptr, struct comm_user_mem, list);

		if (comm_kmemcmp(&um->mem, mem, COMM_CMP_IOVA) == 0)
			break;

		um = NULL;
	}

	if (um) {
		kref_put(&um->refcount, comm_user_mem_release);
		ret = 0;
	}
	mutex_unlock(&u->m_mtx);

	if (ret == 0)
		kref_put_mutex(&u->refcount, comm_user_destroy, &user_mtx);

	return ret;
}

int comm_user_mem_free(struct comm_user *u, struct comm_kmem *mem)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct comm_user_mem *um = NULL;
	int ret = -EFAULT;

	if (IS_ERR_OR_NULL(u) || (IS_ERR_OR_NULL(mem)))
		return -EFAULT;

	mutex_lock(&u->m_mtx);
	list_for_each_safe(list_ptr, tmp, &u->m_list) {
		um = list_entry(list_ptr, struct comm_user_mem, list);

		if (comm_kmemcmp(&um->mem, mem, COMM_CMP_IOVA) == 0)
			break;

		um = NULL;
	}

	if (um) {
		kref_put(&um->refcount, comm_user_mem_release);
		ret = 0;
	}
	mutex_unlock(&u->m_mtx);

	return ret;
}

int comm_user_mem_alloc(struct comm_user *u, struct comm_kmem *mem)
{
	struct comm_user_mem *um;

	if (IS_ERR_OR_NULL(u) || (IS_ERR_OR_NULL(mem)))
		return -EINVAL;

	um = vzalloc(sizeof(struct comm_user_mem));
	if (um == NULL)
		return -ENOMEM;

	if (comm_util_get_cb()->alloc(mem)) {
		vfree(um);
		return -EFAULT;
	}

	memcpy(&um->mem, mem, sizeof(struct comm_kmem));
	um->user = u;
	kref_init(&um->refcount);

	mutex_lock(&u->m_mtx);
	list_add_tail(&um->list, &u->m_list);
	mutex_unlock(&u->m_mtx);

	return 0;
}

int comm_user_mem_search(struct comm_user *u,
			struct comm_kmem *mem,
			u32 type,
			int (*cb)(struct comm_kmem *))
{
	int ret = -EFAULT;
	struct comm_user_mem *um;

	if (IS_ERR_OR_NULL(u) || (IS_ERR_OR_NULL(mem)))
		return -EFAULT;

	mutex_lock(&u->m_mtx);
	list_for_each_entry(um, &u->m_list, list) {
		if (comm_kmemcmp(&um->mem, mem, type) == 0) {
			// if cb return fail, find next match mem
			if (cb && cb(&um->mem))
				continue;

			memcpy(mem, &um->mem, sizeof(*mem));
			ret = 0;
			break;
		}
	}
	mutex_unlock(&u->m_mtx);

	return ret;
}

int comm_user_mem_search_foreach(struct comm_user **iu,
			struct comm_kmem *mem,
			u32 type,
			int (*match)(struct comm_kmem *))
{
	struct comm_user *user = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = -EFAULT;

	if (IS_ERR_OR_NULL(iu))
		return -EINVAL;

	*iu = NULL;

	mutex_lock(&user_mtx);
	list_for_each_safe(list_ptr, tmp, &user_list) {
		user = list_entry(list_ptr, struct comm_user, u_list);
		ret = comm_user_mem_search(user, mem, type, match);
		if (ret == 0) {
			*iu = user;
			break;
		}
	}
	mutex_unlock(&user_mtx);

	return 0;
}

int comm_user_create(struct comm_user **iu)
{
	struct comm_user *u = NULL;

	if (IS_ERR_OR_NULL(iu))
		return -EINVAL;

	u = kzalloc(sizeof(struct comm_user), GFP_KERNEL);
	if (u == NULL)
		return -ENOMEM;

	u->open_pid = current->pid;
	u->open_tgid = current->tgid;
	get_task_comm(u->comm, current);
	u->id = (uint64_t)u;

	mutex_init(&u->m_mtx);
	INIT_LIST_HEAD(&u->m_list);

	*iu = u;

	kref_init(&u->refcount);

	/* add to user mgr's list */
	mutex_lock(&user_mtx);
	list_add_tail(&u->u_list, &user_list);
	mutex_unlock(&user_mtx);

	return 0;
}

static void comm_user_destroy(struct kref *kref)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct comm_user_mem *um = NULL;
	struct comm_user *u =
			container_of(kref, struct comm_user, refcount);

	mutex_lock(&u->m_mtx);
	/* delete residual allocated memory */
	list_for_each_safe(list_ptr, tmp, &u->m_list) {
		um = list_entry(list_ptr, struct comm_user_mem, list);
		kref_put(&um->refcount, comm_user_mem_release);
	}
	mutex_unlock(&u->m_mtx);

	list_del(&u->u_list);
	kfree(u);
	mutex_unlock(&user_mtx);
}

int comm_user_delete(struct comm_user *u)
{
	if (IS_ERR_OR_NULL(u))
		return -EINVAL;

	kref_put_mutex(&u->refcount, comm_user_destroy, &user_mtx);

	return 0;
}

int comm_user_get(struct comm_user *u)
{
	struct comm_user *user = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int ret = -EFAULT;

	if (IS_ERR_OR_NULL(u))
		return -EINVAL;


	mutex_lock(&user_mtx);
	list_for_each_safe(list_ptr, tmp, &user_list) {
		user = list_entry(list_ptr, struct comm_user, u_list);
		/* compare pid if specify */
		if (user == u) {
			kref_get(&user->refcount);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&user_mtx);

	return ret;
}

int comm_user_put(struct comm_user *u)
{
	return comm_user_delete(u);
}

