// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author Mark-PK Tsai <mark-pk.tsai@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#undef PREFIX
#define PREFIX "mtk_printk_dbg: "
#define DEFAULT_SAVE_TEXT_LEN 128
#define DEFAULT_CONSOLE_FLUSH_WARN_MS 500

struct print_dbg_tracepoint {
	struct tracepoint *tp;
	const char *name;
	void *func;
	void *data;
};

struct print_data {
	char text[DEFAULT_SAVE_TEXT_LEN + 1];
	unsigned int len;
	ktime_t start, end;
	s64 delta_ms;
	struct work_struct work;
};

static bool start_print_len_accouting;
static ktime_t console_unlock_start;
static int log_len_cnt, max_log_cnt;
static struct proc_dir_entry *proc_entry;
static struct print_data slow_print_data;

static unsigned int console_flush_warn_ms = DEFAULT_CONSOLE_FLUSH_WARN_MS;
module_param(console_flush_warn_ms, uint, 0644);

static void print_warn_func(struct work_struct *work)
{
	struct print_data *data = container_of(work, struct print_data, work);
	static DEFINE_RATELIMIT_STATE(rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	if (__ratelimit(&rs) && data->delta_ms > (s64)console_flush_warn_ms) {
		pr_warn("========================================================\n");
		pr_warn(PREFIX "print %u char in %lld ms(%lld, %lld)\n",
				data->len, data->delta_ms,
				ktime_to_ms(data->start),
				ktime_to_ms(data->end));
		pr_warn("================== last 128 chars ======================\n");
		pr_warn("%s", data->text);
		pr_warn("========================================================\n");
	}

	if (data->delta_ms > slow_print_data.delta_ms)
		memcpy(&slow_print_data, data, sizeof(slow_print_data));

	kfree(data);
}

static void console_trace(void *data, const char *text, size_t len)
{
	s64 duration;
	ktime_t now;
	struct print_data *d;

	if (!start_print_len_accouting)
		return;

	log_len_cnt += len;

	now = ktime_get();
	duration = ktime_ms_delta(now, console_unlock_start);
	if (duration < (s64)console_flush_warn_ms &&
		duration < slow_print_data.delta_ms)
		return;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return;

	d->start = console_unlock_start;
	d->end = now;
	d->len = log_len_cnt;
	d->delta_ms = duration;
	memcpy(d->text, text, min(len, sizeof(d->text)));

	INIT_WORK(&d->work, print_warn_func);
	queue_work(system_unbound_wq, &d->work);
}

static struct print_dbg_tracepoint tp_arr[] = {
	{
		.name	= "console",
		.func	= console_trace,
		.data	= NULL,
	}
};

/* lookup tracepoints */
static void tracepoint_lookup(struct tracepoint *tp, void *priv)
{
	int i;

	if (!tp || !tp->name)
		return;

	for (i = 0; i < ARRAY_SIZE(tp_arr); i++) {
		if (tp_arr[i].tp)
			continue;

		if (!strcmp(tp->name, tp_arr[i].name))
			tp_arr[i].tp = tp;
	}
}

static int console_debug_tp_init(void)
{
	int i;

	for_each_kernel_tracepoint(tracepoint_lookup
					, (void *)tp_arr);

	for (i = 0; i < ARRAY_SIZE(tp_arr); i++) {
		int ret;

		ret = tracepoint_probe_register(tp_arr[i].tp,
				tp_arr[i].func, tp_arr[i].data);
		if (ret)
			pr_warn(PREFIX "register tp %s fail\n", tp_arr[i].name);
	}

	return 0;
}

/* kprobe post_handler: called after the probed instruction is executed */
static void start_log(struct kprobe *p, struct pt_regs *regs,
				unsigned long flags)
{
	max_log_cnt = max(log_len_cnt, max_log_cnt);
	log_len_cnt = 0;
	start_print_len_accouting = 1;

	console_unlock_start = ktime_get();
}

static void stop_log(struct kprobe *p, struct pt_regs *regs,
				unsigned long flags)
{
	start_print_len_accouting = 0;
}

/*
 * fault_handler: this is called if an exception is generated for any
 * instruction within the pre- or post-handler, or when Kprobes
 * single-steps the probed instruction.
 */
static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	pr_info(PREFIX "fault_handler: p->addr = 0x%p, trap #%dn", p->addr, trapnr);
	/* Return 0 because we don't handle the fault. */
	return 0;
}

static struct kprobe kp_arr[] = {
	{
		.symbol_name	= "console_unlock",
		.fault_handler	= handler_fault,
		.post_handler	= start_log
	}, {
		.symbol_name	= "__up_console_sem",
		.fault_handler	= handler_fault,
		.post_handler	= stop_log
	}
};

static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	seq_printf(m, "max char print in a single console_unlock call = %d\n", max_log_cnt);
	seq_printf(m, "longest console_unlock: print %u char in %lld ms(%lld, %lld)\n",
			slow_print_data.len,
			ktime_ms_delta(slow_print_data.end, slow_print_data.start),
			ktime_to_ms(slow_print_data.start),
			ktime_to_ms(slow_print_data.end));
	seq_puts(m, "================== last 128 chars ======================\n");
	seq_printf(m, "%s", slow_print_data.text);
	seq_puts(m, "========================================================\n");
	return 0;
}

static int mt_printk_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_printk_ctrl_show, inode->i_private);
}

static const struct file_operations mt_printk_ctrl_fops = {
	.open = mt_printk_ctrl_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init mtk_printk_debug_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(kp_arr); i++) {
		int ret;

		ret = register_kprobe(&kp_arr[i]);
		if (ret < 0)
			pr_err(PREFIX "register kp for sym %s fail\n", kp_arr[i].symbol_name);
	}

	console_debug_tp_init();
	proc_entry = proc_create("mtk_printk_debug", 0664, NULL, &mt_printk_ctrl_fops);
	if (!proc_entry)
		pr_err(PREFIX "mtk_printk_debug proc create fail\n");

	return 0;
}

static void __exit mtk_printk_debug_exit(void)
{
	int i;

	proc_remove(proc_entry);
	for (i = 0; i < ARRAY_SIZE(kp_arr); i++)
		unregister_kprobe(&kp_arr[i]);
}

module_init(mtk_printk_debug_init)
module_exit(mtk_printk_debug_exit)
MODULE_LICENSE("GPL");
