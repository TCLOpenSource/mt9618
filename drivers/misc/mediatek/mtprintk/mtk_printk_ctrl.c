// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/moduleparam.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/kmsg_dump.h>
#include <uapi/linux/sched/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/poll.h>

#include "aee.h"

#define SECS_PER_MIN 60
static struct proc_dir_entry *entry;

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
static int detect_count = CONFIG_LOG_TOO_MUCH_DETECT_COUNT;
static bool logmuch_enable;
static int logmuch_exit;
static void *log_much;
static int log_much_len = 1 << (CONFIG_LOG_BUF_SHIFT + 1);
struct proc_dir_entry *logmuch_entry;
static u32 log_count;
static bool detect_count_after_effect_flag;
static int detect_count_after;
DECLARE_WAIT_QUEUE_HEAD(logmuch_thread_exit);
DECLARE_WAIT_QUEUE_HEAD(logmuch_poll_wait);
static bool log_too_much_happen;

static unsigned int log_too_much_cooldown_sec = CONFIG_LOG_TOO_MUCH_DETECT_GAP * SECS_PER_MIN;
module_param(log_too_much_cooldown_sec, uint, 0644);

#define NS_PER_MS 1000000
#define PRINT_TEST_TIMEOUT 10000
#define PRINT_TEST_DELAY 5

void mt_print_much_log(void)
{
	unsigned long long t1 = 0;
	unsigned long long t2 = 0;
	unsigned long print_num = 0;

	t1 = sched_clock();
	pr_info("printk debug log: start time: %lld.\n", t1);

	for (;;) {
		t2 = sched_clock();
		if ((t2 - t1) / NS_PER_MS > PRINT_TEST_TIMEOUT)
			break;
		pr_info("printk debug log: the %ld line, time: %lld.\n",
			print_num++, t2);
		__delay(PRINT_TEST_DELAY);
	}
	pr_info("mt log total write %ld line in 10 second.\n", print_num);
}

void set_logtoomuch_enable(void)
{
	logmuch_enable = true;
}

void set_logtoomuch_disable(void)
{
	logmuch_enable = false;
}

void set_detect_count(int val)
{
	pr_info("set log_much detect value %d.\n", val);
	if (val > 0) {
		if (val >= detect_count) {
			detect_count_after_effect_flag = false;
			detect_count = val;
		} else {
			detect_count_after_effect_flag = true;
			detect_count_after = val;
		}

	}
}
EXPORT_SYMBOL_GPL(set_detect_count);

int get_detect_count(void)
{
	pr_info("get log_much detect value %d.\n", detect_count);
	return detect_count;
}
EXPORT_SYMBOL_GPL(get_detect_count);

static int logmuch_dump_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};
	struct kmsg_dumper dumper = { .active = true };
	size_t len = 0;
	char aee_str[63] = {0};
	int add_len;
	u64 last_seq;
	unsigned long long old = 0;
	unsigned long long now = 0;
	unsigned long long after_dump = 0, dump_time = 0;
	unsigned long long period = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	log_much = kmalloc(log_much_len, GFP_KERNEL);
	if (log_much == NULL) {
		proc_remove(logmuch_entry);
		pr_notice("printk: failed to create 0x%x log_much memory.\n", log_much_len);
		return 1;
	}

	/* don't detect boot up phase*/
	wait_event_interruptible_timeout(logmuch_thread_exit, logmuch_exit == 1, 60 * HZ);

	while (1) {
		if (log_much == NULL)
			break;
		old = sched_clock();
		kmsg_dump_rewind(&dumper);
		last_seq = dumper.next_seq;
		if ((detect_count_after_effect_flag == true) && (detect_count_after > 0)) {
			detect_count_after_effect_flag = false;
			detect_count = detect_count_after;
		}
		wait_event_interruptible_timeout(logmuch_thread_exit, logmuch_exit == 1, 5 * HZ);
		if (logmuch_enable == false || detect_count <= 0)
			continue;

		now = sched_clock();
		kmsg_dump_rewind(&dumper);
		log_count = dumper.next_seq - last_seq;
		period = now - old;
		do_div(period, 1000000);

		after_dump = sched_clock();
		dump_time = after_dump - now;
		do_div(dump_time, NSEC_PER_MSEC);
		pr_debug("dump time = %llu ms with log line count = %llu\n",
				dump_time, log_count);

		/*
		 * warn on log_count > detect_count
		 * normalize: log_count / period(ms) * 1000 > detect_count
		 */
		if (period / 100 * detect_count < log_count * 10) {
			pr_warn("log_much detect.\n");
			if (log_much == NULL)
				break;

			memset((char *)log_much, 0, log_much_len);
			kmsg_dump_rewind(&dumper);
			dumper.cur_seq = last_seq;
			kmsg_dump_get_buffer(&dumper, true, (char *)log_much,
								 log_much_len, &len);

			log_too_much_happen = true;
			wake_up_interruptible(&logmuch_poll_wait);

			memset(aee_str, 0, 63);
			add_len = scnprintf(aee_str, 63,
					"Printk too much: >%d L/s, L: %llu, S: %llu.%03lu\n",
					detect_count, log_count, period/1000, period%1000);

			/*
			 * don't call aee_kernel_warning_api, when MTK_AEE_AED is disabled,
			 * WARN_ON(1) is useless.
			 */
			if (IS_ENABLED(CONFIG_MTK_AEE_AED))
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_PRINTK_TOO_MUCH | DB_OPT_DUMMY_DUMP,
						aee_str, "Need to shrink kernel log");

			pr_warn("log_much detect %d log.\n", len);
			pr_warn("Printk too much: >%d L/s, L: %llu, S: %llu.%03lu\n",
					detect_count, log_count, period/1000, period%1000);
			wait_event_interruptible_timeout(logmuch_thread_exit,
				logmuch_exit == 1, log_too_much_cooldown_sec * HZ);
		}
	}
	pr_notice("[log_much] logmuch_Detect dump thread exit.\n");
	logmuch_exit = 0;
	return 0;
}

static int log_much_show(struct seq_file *m, void *v)
{
	if (log_much == NULL) {
		seq_puts(m, "log_much buff is null.\n");
		return 0;
	}
	seq_write(m, (char *)log_much, log_much_len);
	return 0;
}

static int log_much_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_much_show, inode->i_private);
}

static __poll_t do_poll(struct file *fp, poll_table *wait)
{
	poll_wait(fp, &logmuch_poll_wait, wait);
	if (log_too_much_happen) {
		log_too_much_happen = false;
		return EPOLLIN | EPOLLRDNORM;
	}

	return 0;
}

static const struct file_operations log_much_ops = {
	.open = log_much_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.poll = do_poll,
};
#else
void set_detect_count(int val)
{

}
EXPORT_SYMBOL_GPL(set_detect_count);

int get_detect_count(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(get_detect_count);
#endif



static int mt_printk_ctrl_show(struct seq_file *m, void *v)
{
	seq_puts(m, "=== mt printk controller ===\n");
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	seq_puts(m, "2:   printk too much disable\n");
	seq_puts(m, "3:   printk too much enable\n");
	seq_puts(m, "4:   printk too much log in 10 seconds.\n");
	seq_puts(m,
"xxx: printk too much detect count(xxx represents for a integer > 50)\n");
	seq_printf(m, "log_much detect %d Line, %d size.\n", log_count, log_much_len);
#endif
	seq_puts(m, "=== mt printk controller ===\n\n");
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	seq_printf(m, "printk too much enable: %d.\n", logmuch_enable);
	seq_printf(m, "printk too much detect count: %d\n", detect_count);
#endif
	return 0;
}

static ssize_t mt_printk_ctrl_write(struct file *filp,
	const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	switch (val) {
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	case 2:
		set_logtoomuch_disable();
		break;
	case 3:
		set_logtoomuch_enable();
		break;
	case 4:
		mt_print_much_log();
		break;
	default:
		if (val > 50)
			set_detect_count(val);
		break;
#else
	default:
		break;
#endif
	}
	return cnt;
}

/*** Seq operation of mtprof ****/
static int mt_printk_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_printk_ctrl_show, inode->i_private);
}

static const struct file_operations mt_printk_ctrl_fops = {
	.open = mt_printk_ctrl_open,
	.write = mt_printk_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init mt_printk_ctrl_init(void)
{
#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	static struct task_struct *hd_thread;
#endif

	entry = proc_create("mtprintk", 0664, NULL, &mt_printk_ctrl_fops);
	if (!entry)
		return -ENOMEM;

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	logmuch_entry = proc_create("log_much", 0444, NULL, &log_much_ops);
	if (!logmuch_entry) {
		pr_notice("printk: failed to create proc log_much entry\n");
		return 1;
	}

	hd_thread = kthread_create(logmuch_dump_thread, NULL, "logmuch_detect");
	if (hd_thread)
		wake_up_process(hd_thread);

#endif
	return 0;
}


#ifdef MODULE
static void __exit mt_printk_ctrl_exit(void)
{
	if (entry)
		proc_remove(entry);

#ifdef CONFIG_LOG_TOO_MUCH_WARNING
	if (logmuch_entry)
		proc_remove(logmuch_entry);

	kfree(log_much);
	log_much = NULL;
	logmuch_enable = false;
	logmuch_exit = 1;
	wake_up_interruptible(&logmuch_thread_exit);
	while (logmuch_exit == 1)
		ssleep(5);
#endif
	pr_notice("log_much exit.");

}

module_init(mt_printk_ctrl_init);
module_exit(mt_printk_ctrl_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Printk enhance");
MODULE_AUTHOR("MediaTek Inc.");
#else
device_initcall(mt_printk_ctrl_init);
#endif
