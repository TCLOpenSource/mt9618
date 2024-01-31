// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/dma-direct.h>

#include <common/mdla_device.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>
#include <common/mdla_ioctl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_trace.h>

#include <interface/mdla_intf.h>
#include <interface/mdla_cmd_data_v2_0.h>

#include <platform/mdla_plat_api.h>
#include <platform/mdla_plat_internal.h>
#include <platform/v2_0/mdla_hw_reg_v2_0.h>
#include <platform/v2_0/mdla_irq_v2_0.h>
#include <platform/v2_0/mdla_pmu_v2_0.h>
#include <platform/v2_0/mdla_sched_v2_0.h>
#include "mdla_mt589x_reg_v2_0.h"
#include "mdla_teec_api.h"
#include <comm_driver.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <linux/panic_notifier.h>
#else
#include <linux/kernel.h>
#endif
#include <soc/mediatek/mtktv-miup.h>

#define AIA0_EMI_PORT		0x4
#define AIA0_SMI_COMM_PORT	0x2
#define AIA1_EMI_PORT		0x5
#define AIA1_SMI_COMM_PORT	0x1
#define EMI_PORT_OFS		13
#define EMI_PORT_MASK		0x7
#define SMI_COMM_PORT_OFS		8
#define SMI_COMM_PORT_MASK		0x7

extern int mtk_miup_get_hit_status(struct mtk_miup_hitlog *hit_log);

static u32 mdla_suspend;

static int mdla_plat_pre_cmd_handle(u32 core_id, struct command_entry *ce)
{
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();

	if (mdla_dbg_read_u32(FS_DUMP_CMDBUF))
		mdla_dbg_plat_cb()->create_dump_cmdbuf(mdla_get_device(core_id), ce);

	if (!mdla_dbg_read_u32(FS_SECURE_DRV_EN)) {
		io->cmde.write(core_id,
			MREG_TOP_G_INTP2, MDLA_IRQ_MASK & ~(INTR_SWCMD_DONE));
	}

	return 0;
}

static int mdla_plat_post_cmd_handle(u32 core_id, struct command_entry *ce)
{
	/* clear current event id */
	mdla_util_io_ops_get()->cmde.write(core_id, MREG_TOP_G_CDMA4, 1);

	return 0;
}

static void mdla_plat_raw_process_command(u32 core_id, u32 evt_id,
						dma_addr_t addr, u32 count)
{
	const struct mdla_util_io_ops *io = mdla_util_io_ops_get();

	/* set command address */
	io->cmde.write(core_id, MREG_TOP_G_CDMA1, addr);
	/* set command number */
	io->cmde.write(core_id, MREG_TOP_G_CDMA2, count);
	/* trigger hw */
	io->cmde.write(core_id, MREG_TOP_G_CDMA3, evt_id);
}

static int mdla_plat_process_command(u32 core_id, struct command_entry *ce)
{
	dma_addr_t addr;
	u32 evt_id, count;
	int ret = 0;

	ce->state = CE_RUN;
	//spin_lock_irqsave(&mdla_get_device(core_id)->hw_lock, flags);
	if (!mdla_dbg_read_u32(FS_SECURE_DRV_EN)) {
		addr = ce->mva;
		count = ce->count;
		evt_id = ce->count;

		mdla_drv_debug("%s: count: %d, addr: %lx\n",
			__func__, ce->count,
			(unsigned long)addr);

		mdla_plat_raw_process_command(core_id, evt_id, addr, count);
	} else {
		ret = mdla_plat_teec_send_command(ce->cmd_svp);
	}
	//spin_unlock_irqrestore(&mdla_get_device(core_id)->hw_lock, flags);

	return ret;
}

static void dump_code_buffer(struct mdla_dev *mdla_device)
{
	int i;
	u32 *cmd_addr;

	pr_emerg("------- dump MDLA code buf -------\n");

	if (mdla_device->cmd_buf_len == 0)
		return;
	pr_emerg("mdla %d code buf:\n", mdla_device->mdla_id);
	mutex_lock(&mdla_device->cmd_buf_dmp_lock);
	cmd_addr = mdla_device->cmd_buf_dmp;
	for (i = 0; i < (mdla_device->cmd_buf_len/4); i++)
		pr_emerg("count: %d, offset: %.8x, val: %.8x\n",
			(i * 4) / 0x1c0,
			(i * 4) % 0x1c0,
			cmd_addr[i]);
	mutex_unlock(&mdla_device->cmd_buf_dmp_lock);
}

static int mdla_plat_suspend(u32 core_id)
{
	struct mdla_dev *mdla_info = mdla_get_device(core_id);

	mdla_drv_debug("%s(): Core (%d) suspend\n", __func__, core_id);

	// wait unfinished inference job complete
	mutex_lock(&mdla_info->cmd_lock);
	mdla_suspend = 1;
	mutex_unlock(&mdla_info->cmd_lock);

	return 0;
}

static int mdla_plat_resume(u32 core_id)
{
	mdla_drv_debug("%s(): Core (%d) resume\n", __func__, core_id);
	mdla_suspend = 0;

	return 0;
}

static void mdla_v2_0_reset(u32 core_id, const char *str)
{
	struct mdla_dev *dev = mdla_get_device(core_id);

	if (unlikely(!dev)) {
		mdla_err("%s(): No mdla device (%d)\n", __func__, core_id);
		return;
	}

	/* use power down==>power on apis insted bus protect init */
	mdla_drv_debug("%s: MDLA RESET: %s\n", __func__,
		str);


	mdla_trace_reset(core_id, str);
}

static int mdla_plat_check_resource(u32 core)
{
#ifdef ENABLE_VIRTUAL_HASHKEY
	u32 hw_support;

	hw_support = comm_util_get_cb()->hw_supported();

	mdla_drv_debug("%s(): enable vir_hashkey option (%x)\n",
			__func__, hw_support);
	return (!hw_support | mdla_suspend);
#endif
	return mdla_suspend;
}

static int mdla_panic_notify(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct mtk_miup_hitlog hit_log;
	u16 cid, emi_port, smi_port;
	struct command_entry *ce;
	struct mdla_dev *mdla_device;
	u32 core_id;

	pr_crit("\033[1;41m%s-%u\033[m\n", __func__, __LINE__);

	if (mtk_miup_get_hit_status(&hit_log)) {
		pr_emerg("[MDLA][MIUP][HIT][CID:0x%llX %s address: 0x%llX - 0x%llX block:%d\n",
			hit_log.cid, (hit_log.attr == 0) ? "read" : "write",
			hit_log.hit_addr, (hit_log.hit_addr + 0x1F), hit_log.hit_blk);

		cid = hit_log.cid;
		emi_port = (cid >> EMI_PORT_OFS) & EMI_PORT_MASK;
		smi_port = (cid >> SMI_COMM_PORT_OFS) & SMI_COMM_PORT_MASK;

		if (((emi_port == AIA0_EMI_PORT) && (smi_port == AIA0_SMI_COMM_PORT)) ||
			((emi_port == AIA1_EMI_PORT) && (smi_port == AIA1_SMI_COMM_PORT))) {

			pr_emerg("%s(): emi_num:%d sc_num:%d\n", __func__, emi_port, smi_port);
			mdla_dbg_write_u32(FS_KLOG, 0xff);

			for_each_mdla_core(core_id) {
				mdla_device = mdla_get_device(core_id);
				ce = mdla_device->ce;

				pr_emerg("%s(): core:%d kva:%llx mva:%x secure:%x comm:%s\n", __func__,
					core_id, (u64)(uintptr_t)mdla_device->cmd_kva,
					mdla_device->cmd_mva, mdla_device->cmd_secure, mdla_device->cmd_comm);

				mdla_dbg_plat_cb()->dump_reg(core_id, NULL);
				if (ce) {
					mdla_dbg_plat_cb()->create_dump_cmdbuf(mdla_device, ce);
					dump_code_buffer(mdla_device);
				}
			}
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = mdla_panic_notify,
	.priority = 0,
};

/* platform public functions */

int mdla_mt5879_v2_0_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdla_plat_drv *drv_data;
	struct mdla_cmd_cb_func *cmd_cb = mdla_cmd_plat_cb();

	dev_info(&pdev->dev, "%s()\n", __func__);

	if (mdla_v2_0_init(pdev)) {
		dev_info(&pdev->dev, "%s() mdla_v2_0 init fail.\n", __func__);
		return -1;
	}

	drv_data = (struct mdla_plat_drv *)of_device_get_match_data(dev);
	if (!drv_data)
		return -1;

	mdla_pwr_reset_setup(mdla_v2_0_reset);

	/* replace command callback */
	cmd_cb->pre_cmd_handle      = mdla_plat_pre_cmd_handle;
	cmd_cb->post_cmd_handle     = mdla_plat_post_cmd_handle;
	cmd_cb->process_command     = mdla_plat_process_command;

	cmd_cb->suspend_cb	= mdla_plat_suspend;
	cmd_cb->resume_cb	= mdla_plat_resume;
	cmd_cb->check_resource	= mdla_plat_check_resource;

	mdla_ioctl_set_efuse(0);

	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

	return 0;
}

void mdla_mt5879_v2_0_deinit(struct platform_device *pdev)
{
	mdla_drv_debug("%s unregister power -\n", __func__);

	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_block);
	mdla_v2_0_deinit(pdev);
}
