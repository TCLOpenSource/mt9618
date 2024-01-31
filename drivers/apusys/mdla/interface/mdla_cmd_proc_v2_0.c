// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/sched/clock.h>
#include <asm-generic/div64.h>

#include <apusys_device.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>
#include <common/mdla_ioctl.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_trace.h>
#include <utilities/mdla_debug.h>
#include "mdla_cmd_data_v2_0.h"
#include "comm_driver.h"

#define MTK_IOVA_START_ADDR 0x200000000

static int check_cmdbuf(struct command_entry *ce)
{
#ifdef MDLA_CMDBUF_DATA_CHECK
	u32 verid;
	u32 *p = (u32 *)(uintptr_t)ce->kva;
	int i, j;

	/* use the first verid as golden verid */
	verid = *(p + 0x1bc/4);

	for (i = 0; i < ce->count; i++) {
		if (*(p + 0x1bc/4) != verid) {
			mdla_drv_debug("%s(): count:%x verid:%x val:%x\n",
					__func__, i, verid, *(p + 0x1bc/4));
			goto fail_dump;
		}

		p += 0x1c0/4;
	}

	return 0;

fail_dump:

	p = (u32 *)(uintptr_t)ce->kva;
	for (i = 0; i < ce->count; i++) {
		for (j = 0; j < 0x1c0/4; j++) {
			mdla_drv_debug("%s(): count:%x offset:%x val:%x\n",
					__func__, i, j * 4, *(p + j));
		}
		p += 0x1c0/4;
	}
	return -EINVAL;
#else
	return 0;
#endif
}

static inline void mdla_fill_kmem(u32 iova, u64 size, u8 iommu, struct comm_kmem *mem)
{
	if (mem) {
		memset(mem, 0, sizeof(*mem));

		mem->iova = iova;
		mem->size = size;

		if (iommu == 1) {
			mem->iova |= MTK_IOVA_START_ADDR;
			mem->mem_type = APU_COMM_MEM_DRAM_DMA;
		} else {
			mem->mem_type = APU_COMM_MEM_VLM;
		}
	}
}

static void *mdla_buf_get(void *cmd_info, u64 iova, u32 size)
{
	struct comm_kmem mem;
	struct comm_user *user;

	if (!IS_ALIGNED(iova, 4))
		return NULL;

	mdla_fill_kmem(iova, size, 1, &mem);

	if (!cmd_info) {
		if (comm_util_get_cb()->query_mem(&mem,
						COMM_QUERY_BY_IOVA,
						NULL))
			return NULL;

	} else {
		user = ((struct mdla_cmd_info *)cmd_info)->cmd_user;

		if (comm_util_get_cb()->get_user_mem(user, &mem))
			return NULL;
	}

	if (mem.status & (1 << COMM_MEM_OBSOLETE))
		return NULL;

	return (void *)(uintptr_t)mem.kva;
}

static void mdla_buf_put(void *cmd_info, u64 iova, u32 size)
{
	struct comm_kmem mem;
	struct comm_user *user;

	if (!IS_ALIGNED(iova, 4))
		return;

	mdla_fill_kmem(iova, size, 1, &mem);

	if (cmd_info) {
		user = ((struct mdla_cmd_info *)cmd_info)->cmd_user;

		if (comm_util_get_cb()->put_user_mem(user, &mem))
			return;
	}
}

static void *mdla_buf_map(void *cmd_info, u64 iova, u32 size)
{
	struct comm_kmem mem;
	struct comm_user *user;

	if (!IS_ALIGNED(iova, 4))
		return NULL;

	mdla_fill_kmem(iova, size, 1, &mem);

	if (!cmd_info) {
		if (comm_util_get_cb()->query_mem(&mem,
						COMM_QUERY_BY_IOVA,
						NULL))
			return NULL;

	} else {
		user = ((struct mdla_cmd_info *)cmd_info)->cmd_user;

		if (comm_util_get_cb()->search_user_mem(user,
						&mem,
						COMM_CMP_IOVA,
						NULL))
			return NULL;
	}

	if (mem.status & (1 << COMM_MEM_OBSOLETE))
		return NULL;

	return (void *)(uintptr_t)mem.kva;
}

void mdla_dev_trace_cmd(struct mdla_dev *mdla_device, struct command_entry *ce, bool secure)
{
	mdla_device->cmd_mva = ce->mva;
	mdla_device->cmd_kva = ce->kva;
	mdla_device->cmd_secure = secure;
	memcpy(mdla_device->cmd_comm, current->comm, 0x10);
	mdla_device->cmd_comm[0x10 - 1] = '\0';
}

static int mdla_cmd_prepare_v2_0(struct mdla_run_cmd *cd,
			struct apusys_cmd_hnd *apusys_hd,
			struct command_entry *ce,
			bool secure,
			struct mdla_run_cmd_svp *cmd_ut)
{
	struct mdla_dev *mdla_device;

	ce->mva = cd->mva + cd->offset;

	if (mdla_dbg_read_u32(FS_TIMEOUT_DBG))
		mdla_cmd_debug("%s(): mva=0x%08x, offset=0x%x, count: %u\n",
				__func__,
				cd->mva,
				cd->offset,
				cd->count);

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->count = cd->count;
	ce->receive_t = sched_clock();
	mdla_device = mdla_get_device(0);
	mdla_device->ce = ce;

	ce->kva = mdla_buf_map(NULL,
			(u64)ce->mva,
			ce->count * 0x1c0);
	if (!ce->kva) {
		mdla_err("%s(): map fail, iova:%#08x\n",
			__func__, ce->mva);
		return -EINVAL;
	}

	if (secure) {
		memcpy(cmd_ut, ce->kva, sizeof(struct mdla_run_cmd_svp));
		ce->kva = NULL;
	} else {
		if (ce->count == 0)
			return -EFAULT;

		memset(cmd_ut, 0, sizeof(*cmd_ut));
		cmd_ut->pipe_id = UINT_MAX;
		cmd_ut->buf[0].size = cd->size;
		cmd_ut->buf[0].iova = cd->mva;
		cmd_ut->buf[0].buf_type = EN_AI_BUF_TYPE_CODE;
		cmd_ut->buf[0].buf_source = EN_BUF_SOURCE_DRAM;
		cmd_ut->buf_count = 1;
		cmd_ut->cmd_count = cd->count;

		if (check_cmdbuf(ce)) {
			ce->kva = NULL;
			mdla_err("%s(): check_cmd fail\n", __func__);
			return -EINVAL;
		}
	}

	ce->cmd_svp = (void *)cmd_ut;
	mdla_dev_trace_cmd(mdla_device, ce, secure);

	return 0;
}

static int mdla_ut_cmd_prepare_v2_0(void *cmd_ut,
					struct command_entry *ce)
{
	struct mdla_dev *mdla_device;
	struct mdla_run_cmd_sync_svp *cmd = cmd_ut;
	struct mdla_run_cmd_svp *p = &cmd->req;
	int i;

	ce->state = CE_NONE;
	ce->flags = CE_NOP;
	ce->bandwidth = 0;
	ce->result = MDLA_SUCCESS;
	ce->receive_t = sched_clock();
	ce->count = p->cmd_count;
	ce->boost_val = p->boost_value;

	ce->cmd_svp = (void *)cmd_ut;
	mdla_device = mdla_get_device(0);
	mdla_device->ce = ce;

	if (!p->secure) {
		if (ce->count == 0)
			return -EFAULT;

		ce->mva = UINT_MAX;
		for (i = 0; i < p->buf_count; i++) {
			if (p->buf[i].buf_type == EN_AI_BUF_TYPE_CODE) {
				ce->mva	= p->buf[i].iova;
				break;
			}
		}

		ce->kva = mdla_buf_get(&cmd->cmd_info,
				(u64)ce->mva,
				ce->count * 0x1c0);
		if (!ce->kva) {
			mdla_err("%s(): map fail, iova:%#08x dev:%p\n",
				__func__, ce->mva, cmd->cmd_info.cmd_user);
			return -EINVAL;
		}

		if (check_cmdbuf(ce)) {
			mdla_buf_put(&cmd->cmd_info,
				(u64)ce->mva,
				ce->count * 0x1c0);
			ce->kva = NULL;
			mdla_err("%s(): check_cmd fail\n", __func__);
			return -EINVAL;
		}
	}

	mdla_dev_trace_cmd(mdla_device, ce, p->secure);

	return 0;
}

static void mdla_cmd_finish_v2_0(struct mdla_run_cmd *cd,
					struct command_entry *ce,
					struct mdla_run_cmd_svp *cmd_ut)
{
	struct mdla_dev *mdla_device;

	mdla_device = mdla_get_device(0);
	mdla_device->ce = NULL;
}

static void mdla_cmd_ut_finish_v2_0(void *cmd_ut,
					struct command_entry *ce)
{
	struct mdla_dev *mdla_device;
	struct mdla_run_cmd_sync_svp *p = cmd_ut;
	struct mdla_wait_cmd *wt;

	if (!p->req.secure && ce->kva)
		mdla_buf_put(&p->cmd_info,
			(u64)ce->mva,
			ce->count * 0x1c0);
	wt = &p->res;
	wt->queue_time = ce->poweron_t - ce->receive_t;
	wt->busy_time = ce->wait_t - ce->poweron_t;
	wt->bandwidth = ce->bandwidth;

	mdla_device = mdla_get_device(0);
	mdla_device->ce = NULL;
}

int mdla_cmd_run_sync_v2_0(struct mdla_run_cmd_sync *cmd_data,
				struct mdla_dev *mdla_info,
				struct apusys_cmd_hnd *apusys_hd,
				bool secure,
				bool can_be_preempted)
{
	u64 deadline = 0;
	struct mdla_run_cmd_svp cmd_ut;
	struct mdla_run_cmd *cd = &cmd_data->req;
	struct command_entry ce;
	int ret = 0;
	u32 core_id = 0;
#ifdef RESET_WRITE_EVT_EXEC //[TODO]
	u16 prio = can_be_preempted ? 1 : 0;
#endif
	u64 result_div;
#ifdef COMMAND_UTILIZATION
	void *entry;
#endif

	memset(&ce, 0, sizeof(struct command_entry));
	core_id = mdla_info->mdla_id;
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	if (mdla_cmd_plat_cb()->check_resource(core_id)) {
		mutex_unlock(&mdla_info->cmd_lock);
		return -EFAULT;
	}
	mdla_pwr_ops_get()->wake_lock(core_id);

	if (mdla_cmd_prepare_v2_0(cd, apusys_hd, &ce, secure, &cmd_ut)) {
		mdla_err("%s(): prepare fail, mva:%#08x\n", __func__, ce.mva);
		goto out;
	}

	deadline = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	if (mdla_dbg_read_u32(FS_TIMEOUT_DBG))
		mdla_cmd_debug("%s(): core: %d id: %d mva:%#08x pid:%d/%d\n",
			__func__, core_id, ce.count, ce.mva,
			current->pid, current->tgid);

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret) {
		if (ret == -EAGAIN)
			mdla_pwr_ops_get()->off(core_id, 0, false);
		goto out;
	}

	mdla_pwr_ops_get()->set_opp_by_bootst(core_id, apusys_hd->boost_val);

	if (mdla_dbg_read_u32(FS_DVFS_RAND))
		mdla_power_set_random_opp(core_id);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, &ce);

	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, &ce);

#ifdef RESET_WRITE_EVT_EXEC //[TODO]
	if (mdla_util_apu_pmu_handle(mdla_info, apusys_hd, prio) == 0)
		mdla_util_pmu_ops_get()->reset_write_evt_exec(core_id, prio);
#endif

#ifdef COMMAND_UTILIZATION
	entry = exec_time_log_start();
#endif
	mdla_cmd_plat_cb()->add_time_stamp(core_id, &ce, "hw trigger");

	/* Fill HW reg */
	ret = mdla_cmd_plat_cb()->process_command(core_id, &ce);
	if (ret) {
		mdla_err("%s(): process_cmd fail, mva:%#08x\n", __func__, ce.mva);
		goto cmd_fail;
	}

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count
			&& time_before64(get_jiffies_64(), deadline)) {

		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));
	}

	mdla_cmd_plat_cb()->add_time_stamp(core_id, &ce, "wait complete");
#ifdef COMMAND_UTILIZATION
	if (entry)
		exec_time_log_end(entry);
#endif

	mdla_cmd_plat_cb()->post_cmd_handle(core_id, &ce);
	mdla_cmd_plat_cb()->post_cmd_info(core_id);

	ce.req_end_t = sched_clock();

	mdla_trace_end(core_id, 0, &ce);
	mdla_prof_stop(core_id, 1);
	mdla_prof_iter(core_id);
	mdla_util_apu_pmu_update(mdla_info, apusys_hd, 0);

	cd->id = mdla_info->max_cmd_id;

	if (mdla_info->max_cmd_id < ce.count) {
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n",
				ce.count,
				mdla_info->max_cmd_id);

		mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);

		mdla_dbg_dump(mdla_info, &ce);

		/* Enable & Relase bus protect */
		mdla_pwr_ops_get()->switch_off_on(core_id);

		mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));

		ret = -1;
	}

cmd_fail:
	if (mdla_dbg_read_u32(FS_POWEROFF_TIME) == 0)
		mdla_pwr_ops_get()->off(core_id, 0, true);
	else
		mdla_pwr_ops_get()->off_timer_start(core_id);

	ce.wait_t = sched_clock();

	result_div = (ce.wait_t - ce.poweron_t);
	do_div(result_div, 1000);
	apusys_hd->ip_time = (u32)(result_div);

	mdla_cmd_plat_cb()->print_time_stamp(core_id, &ce);
out:
	mdla_cmd_finish_v2_0(cd, &ce, &cmd_ut);
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);

	return ret;
}

int mdla_cmd_ut_run_sync_v2_0(void *cmd_ut, struct mdla_dev *mdla_info)
{
	u64 deadline = 0;
	struct command_entry ce;
	int ret = 0;
	u32 core_id = 0;
#ifdef COMMAND_UTILIZATION
	void *entry;
#endif

	memset(&ce, 0, sizeof(struct command_entry));
	core_id = mdla_info->mdla_id;
	ce.queue_t = sched_clock();

	/* The critical region of command enqueue */
	mutex_lock(&mdla_info->cmd_lock);
	if (mdla_cmd_plat_cb()->check_resource(core_id)) {
		mutex_unlock(&mdla_info->cmd_lock);
		return -EFAULT;
	}

	mdla_pwr_ops_get()->wake_lock(core_id);

	if (mdla_ut_cmd_prepare_v2_0(cmd_ut, &ce)) {
		mdla_err("%s(): prepare fail, mva:%#08x\n", __func__, ce.mva);
		goto out;
	}

	deadline = get_jiffies_64()
			+ msecs_to_jiffies(mdla_dbg_read_u32(FS_TIMEOUT));

	mdla_info->max_cmd_id = 0;

	if (mdla_dbg_read_u32(FS_TIMEOUT_DBG)) {
		mdla_cmd_debug("%s(): core %d: id: %d mva:%#08x dev:%p pid:%d/%d\n",
			__func__, core_id, ce.count, ce.mva,
			((struct mdla_run_cmd_sync_svp *)cmd_ut)->cmd_info.cmd_user,
			current->pid, current->tgid);
	}

	ret = mdla_pwr_ops_get()->on(core_id, false);
	if (ret) {
		if (ret == -EAGAIN)
			mdla_pwr_ops_get()->off(core_id, 0, true);
		goto out;
	}

	mdla_pwr_ops_get()->set_opp_by_bootst(core_id, ce.boost_val);

	if (mdla_dbg_read_u32(FS_DVFS_RAND))
		mdla_power_set_random_opp(core_id);

	mdla_prof_start(core_id);
	mdla_trace_begin(core_id, &ce);

	ce.poweron_t = sched_clock();
	ce.req_start_t = sched_clock();

	mdla_cmd_plat_cb()->pre_cmd_handle(core_id, &ce);

#ifdef COMMAND_UTILIZATION
	entry = exec_time_log_start();
#endif
	mdla_cmd_plat_cb()->add_time_stamp(core_id, &ce, "hw trigger");

	/* Fill HW reg */
	ret = mdla_cmd_plat_cb()->process_command(core_id, &ce);
	if (ret) {
		mdla_err("%s(): process_cmd fail, mva:%#08x\n", __func__, ce.mva);
		goto cmd_fail;
	}

	/* Wait for timeout */
	while (mdla_info->max_cmd_id < ce.count
			&& time_before64(get_jiffies_64(), deadline)) {

		wait_for_completion_interruptible_timeout(
			&mdla_info->command_done,
			mdla_cmd_plat_cb()->get_wait_time(core_id));
	}

	mdla_cmd_plat_cb()->add_time_stamp(core_id, &ce, "wait complete");
#ifdef COMMAND_UTILIZATION
	if (entry)
		exec_time_log_end(entry);
#endif
	mdla_cmd_plat_cb()->post_cmd_handle(core_id, &ce);
	mdla_cmd_plat_cb()->post_cmd_info(core_id);

	ce.req_end_t = sched_clock();

	mdla_prof_iter(core_id);
	mdla_trace_end(core_id, 0, &ce);

	//cd->id = mdla_info->max_cmd_id;

	if (mdla_info->max_cmd_id < ce.count) {
		mdla_timeout_debug("command: %d, max_cmd_id: %d\n",
				ce.count,
				mdla_info->max_cmd_id);

		mdla_cmd_plat_cb()->post_cmd_hw_detect(core_id);

		mdla_dbg_dump(mdla_info, &ce);

		/* Enable & Relase bus protect */
		mdla_pwr_ops_get()->switch_off_on(core_id);

		mdla_pwr_ops_get()->hw_reset(mdla_info->mdla_id,
				mdla_dbg_get_reason_str(REASON_TIMEOUT));

		ret = -1;
	}

	mdla_prof_stop(core_id, 1);

cmd_fail:
	if (mdla_dbg_read_u32(FS_POWEROFF_TIME) == 0)
		mdla_pwr_ops_get()->off(core_id, 0, false);
	else
		mdla_pwr_ops_get()->off_timer_start(core_id);

	ce.wait_t = sched_clock();

	mdla_cmd_plat_cb()->print_time_stamp(core_id, &ce);
out:
	mdla_cmd_ut_finish_v2_0(cmd_ut,  &ce);
	mdla_pwr_ops_get()->wake_unlock(core_id);
	mutex_unlock(&mdla_info->cmd_lock);

	return ret;
}
