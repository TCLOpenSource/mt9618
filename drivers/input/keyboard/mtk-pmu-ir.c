// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author jefferry.yen <jefferry.yen@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <asm/irq.h>
#include <linux/io.h>
#include "input_cli_svc_msg.h"
#include <linux/ctype.h>
#include <linux/compat.h>
#include <linux/sizes.h>


#define MTK_IR_DEVICE_NAME	"MTK PMU IR"
#define INPUTID_VENDOR		0x3697UL
#define INPUTID_PRODUCT		0x0001
#define INPUTID_VERSION		0x0001
#define IPCM_MAX_DATA_SIZE	64

#define MAX_KEY_EVENT_BUFFER_SIZE	(1<<8)
#define MAX_KEY_EVENT_BUFFER_MASK	(MAX_KEY_EVENT_BUFFER_SIZE-1)
#define DUMP_SIZE	16
#define WAIT_RESULT_TIMEOUT msecs_to_jiffies(1000)
#define WAIT_STR_TIMEOUT msecs_to_jiffies(200)
#define PMU_WAKEUP_KEY_SIZE 64
#define PMU_WAKEUP_GET_KEY_STRING_SIZE (7*(PMU_WAKEUP_KEY_SIZE)+1)
//0x1234,0x1234.... 7xPMU_WAKEUP_KEY_SIZE +1
#define PMU_WAKEUP_STRING_MERGE_SIZE 5
#define PMU_WAKEUP_STRING_SIZE 2
#define MAX_RETRY_CNT	3
#define MAX_GET_ID_CONFIG_RETRY_CNT	5
//------------------------------------------------------------------------------
/*
 * This "device" covers _all_ IR proprietary control
 */
static struct platform_device *ir_control_dev;

struct mtk_ir_id_config {
	uint32_t vendor_id;
	uint32_t product_id;
	uint32_t version;
	char  device_name[INPUT_DEVICE_NAME_SIZE];
};

struct mtk_ir_dev {
	struct input_dev *inputdev;
	struct rpmsg_device *rpdev;
	struct task_struct *task;
	wait_queue_head_t kthread_waitq;

	struct completion ack;
	struct input_climsg reply;
	int readpos;
	int writepos;
	struct input_climsg_event keyevent_buffer[MAX_KEY_EVENT_BUFFER_SIZE];
	struct notifier_block pm_notifier;
	struct mtk_ir_id_config id_config;
};
/*
 * TODO: Remove save wkaeup key to dummy register when PMU always live.
 */
static int _ir_wakeup_key;
module_param_named(ir_wakeup_key, _ir_wakeup_key, int, 0644);

static int mtk_ir_get_wakeup_key(struct mtk_ir_dev *mtk_ir)
{
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_open *msg = (struct input_svcmsg_open *)&svcmsg;
	int ret;
	struct input_climsg_get_wakeupkey *reply =
					(struct input_climsg_get_wakeupkey *)&mtk_ir->reply;

	msg->msg_id = INPUT_SVCMSG_GET_WAKEUPKEY;
	ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
	if (ret)
		pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);

	ret = wait_for_completion_interruptible_timeout
						(&mtk_ir->ack, WAIT_RESULT_TIMEOUT);
	if (!reply->ret)
		_ir_wakeup_key = reply->keycode;

	pr_info("[%s](ret, reply->ret)= (%d, %d)\n", __func__, ret, reply->ret);

	return ret;
}

static int mtk_ir_get_id_config(struct mtk_ir_dev *mtk_ir)
{
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_open *msg = (struct input_svcmsg_open *)&svcmsg;
	int ret;
	struct input_climsg_get_id_config *reply =
					(struct input_climsg_get_id_config *)&mtk_ir->reply;
	int retry = MAX_GET_ID_CONFIG_RETRY_CNT;

	pr_info("%s:%d E\n", __func__, __LINE__);
	msg->msg_id = INPUT_SVCMSG_GET_ID_CONFIG;
	do {
		ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
		if (ret)
			pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);

		ret = wait_for_completion_interruptible_timeout
						(&mtk_ir->ack, WAIT_RESULT_TIMEOUT);
		pr_info("[%s]IR_GET_ID_CONFIG:(ret, retry)= (%d, %d)\n", __func__, ret, retry);
		retry--;
	} while ((!(ret > 0) || reply->ret) && retry > 0);

	if (!reply->ret) {
		mtk_ir->id_config.vendor_id = reply->vendor_id;
		mtk_ir->id_config.product_id = reply->product_id;
		mtk_ir->id_config.version = reply->version;
		//keep one byte for string terminated
		strncpy(mtk_ir->id_config.device_name, reply->device_name,
			sizeof(mtk_ir->id_config.device_name) - 1);
	}

	pr_info("[%s](ret, reply->ret)= (%d, %d)\n", __func__, ret, reply->ret);

	return ret;
}

static ssize_t pwrkeycode_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	//spec: cat pwrkeycode
	//output: 0x12,0x33,0x55,0x11,0x00....	all wakeup keys
	//1. get all keycode from pmu
	//2. strcat together and put into buf
	ssize_t ret = 0;
	int snprint_result = 0;//Fix coverity
	struct mtk_ir_dev *mtk_ir = dev_get_drvdata(dev);
	char *str_end = kmalloc(PMU_WAKEUP_GET_KEY_STRING_SIZE, GFP_KERNEL);
	char str_digittostring[PMU_WAKEUP_STRING_MERGE_SIZE] = {0};
	int rpmg_index = 0;
	int rpmg_max_count = PMU_WAKEUP_KEY_SIZE;
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_get_config_wakeup *msg =
		(struct input_svcmsg_get_config_wakeup *)&svcmsg;
	struct input_climsg_get_config_wakeup *reply =
		(struct input_climsg_get_config_wakeup *)&mtk_ir->reply;
	msg->msg_id = INPUT_SVCMSG_GET_CONFIG_WAKEUP;

	memset(str_end, 0, PMU_WAKEUP_GET_KEY_STRING_SIZE);
	for (rpmg_index = 0; rpmg_index < rpmg_max_count; rpmg_index++) {
		msg->keyindex = rpmg_index;
		ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
		if (ret)
			pr_err("[%s]rpmsg send failed, ret=%zd\n", __func__, ret);
		wait_for_completion_interruptible_timeout(&mtk_ir->ack, WAIT_RESULT_TIMEOUT);
		if (!reply->ret) {
			strncat(str_end, "0x", PMU_WAKEUP_STRING_SIZE);
			snprint_result = snprintf(str_digittostring, sizeof(str_digittostring),
			"%x", reply->keycode);
			if (snprint_result < 0)
				pr_err("[%s]snprintf convert error: keycode=%u\n", __func__,
				reply->keycode);
			else if (snprint_result >= sizeof(str_digittostring))
				pr_err("[%s]snprintf error: keycode=%u, snprint_result=%d\n",
				__func__, reply->keycode, snprint_result);
			strncat(str_end, str_digittostring, PMU_WAKEUP_STRING_MERGE_SIZE);
			if (rpmg_index < rpmg_max_count-1)
				strncat(str_end, ",", PMU_WAKEUP_STRING_SIZE);
		}
	}
	snprint_result = snprintf(buf, PMU_WAKEUP_GET_KEY_STRING_SIZE, "%s\n", str_end);
	if (snprint_result < 0)
		pr_err("[%s]snprintf convert error\n", __func__);
	else if (snprint_result >= PMU_WAKEUP_GET_KEY_STRING_SIZE)
		pr_err("[%s]snprintf str_end error: snprint_result=%d\n", __func__,
				snprint_result);
	ret = (ssize_t)snprint_result;
	kfree(str_end);
	return ret;
}


static ssize_t pwrkeycode_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
//echo 0x0,0x11,0x22 ...> pwrkeycode
//1. split string by "," use strsep and kstrdup
	char *s = kstrdup(buf, GFP_KERNEL);
	char *temp = s;
	int set_key_index = 0;
	char *v;
	int err;
	struct mtk_ir_dev *mtk_ir = dev_get_drvdata(dev);
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_config_wakeup *msg =
		(struct input_svcmsg_config_wakeup *)&svcmsg;
	int ret;
	unsigned long val;

	while (s) {
		v = strsep(&s, ",");
		if (!v)
			break;
		//2. send n-times rpmg to pmu
		err = kstrtol(v, 16, &val);
		if (err != 0) {
			pr_err("[%s]kstrtol, err=%d\n", __func__, err);
			break;
		}
		msg->msg_id = INPUT_SVCMSG_CONFIG_WAKEUP;
		msg->keycode = val;
		msg->keyindex = set_key_index;
		ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
		if (ret)
			pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);
		wait_for_completion_interruptible_timeout
			(&mtk_ir->ack, WAIT_RESULT_TIMEOUT);
		set_key_index++;
	}
	kfree(temp);
	return count;
}

static DEVICE_ATTR(pwrkeycode, 0664, pwrkeycode_show,
		   pwrkeycode_store); // receive user get keyindex and return the keycode

static struct attribute *mtk_dtv_ir_attr[] = {
		&dev_attr_pwrkeycode.attr,
		NULL,
};

static const struct attribute_group mtk_dtv_ir_attr_group = {
	.name = "wakeup",
	.attrs = mtk_dtv_ir_attr,
};


static int mtk_ir_callback(struct rpmsg_device *rpdev, void *data,
						   int len, void *priv, u32 src)
{
	struct mtk_ir_dev *mtk_ir;
	struct input_climsg *climsg;
	struct input_climsg_event *event;
	int readbytes;
	unsigned int r, w;
	bool dump_msg = false;

	// sanity check
	if (!rpdev) {
		return -EINVAL;
	}
	mtk_ir = dev_get_drvdata(&rpdev->dev);
	if (!mtk_ir) {
		return -EINVAL;
	}
	if (!data || len == 0) {
		return -EINVAL;
	}
	climsg = (struct input_climsg *)data;

	if (climsg->msg_id == INPUT_CLIMSG_ACK) {
		mtk_ir->reply = *climsg;
		complete(&mtk_ir->ack);
	} else {
		readbytes = 0;
		event = (struct input_climsg_event *)climsg;

		r = mtk_ir->readpos;
		w = mtk_ir->writepos;

		while (((r != (w+1)) & MAX_KEY_EVENT_BUFFER_MASK) && (readbytes < len)) {
			if (event->msg_id == INPUT_CLIMSG_KEY_EVENT) {
				mtk_ir->keyevent_buffer[w] = *event;

			} else {
				dump_msg = true;
				pr_err("msg id(%d) incorrect@idx(%lu)\n",
					event->msg_id, (unsigned long)readbytes / sizeof(*event));
			}
			event++;
			w = (w+1) & MAX_KEY_EVENT_BUFFER_MASK;
			readbytes += sizeof(*event);
		}
		mtk_ir->writepos = w;
		if ((r == (w+1)) & MAX_KEY_EVENT_BUFFER_MASK)
			pr_err("keyevent_buffer full!\n");
		if (readbytes != len) {
			dump_msg = true;
			pr_err("size of total event count(%d) != rpmsg rx length(%d)\n",
			readbytes, len);
		}
#ifdef DEBUG
		if (dump_msg)
			print_hex_dump(KERN_INFO, " ",
					DUMP_PREFIX_OFFSET, DUMP_SIZE, 1, data, len, true);
#endif
		wake_up_interruptible(&mtk_ir->kthread_waitq);
	}
	return 0;
}

static int mtk_ir_thread(void *priv)
{
	int r, w, i, ret = 0;
	struct input_climsg_event *event;
	struct mtk_ir_dev *mtk_ir = (struct mtk_ir_dev *)priv;
	struct input_dev *inputdev = mtk_ir->inputdev;

	pr_info("%s:%d init inputdev E\n", __func__, __LINE__);
	if (!inputdev) {
		pr_err("%s:%d inputdev is null, goto exit_thread\n", __func__, __LINE__);
		goto exit_thread;
	}
	ret = mtk_ir_get_id_config(mtk_ir);
	if (ret > 0) {
		pr_info("%s:%d vendor_id 0x%x, product_id 0x%x, version 0x%x, device_name %s\n",
			__func__, __LINE__,
			(unsigned int)mtk_ir->id_config.vendor_id,
			(unsigned int)mtk_ir->id_config.product_id,
			(unsigned int)mtk_ir->id_config.version,
			mtk_ir->id_config.device_name);
		inputdev->id.vendor = mtk_ir->id_config.vendor_id;
		inputdev->id.product = mtk_ir->id_config.product_id;
		inputdev->id.version = mtk_ir->id_config.version;
		inputdev->name = mtk_ir->id_config.device_name;
	} else {
		pr_err("%s:%d ret %d, timeout or interrupt happened, so set to default id config\n",
		__func__, __LINE__, ret);
		inputdev->id.vendor = INPUTID_VENDOR;
		inputdev->id.product = INPUTID_PRODUCT;
		inputdev->id.version = INPUTID_VERSION;
		inputdev->name = MTK_IR_DEVICE_NAME;
	}
	inputdev->id.bustype = BUS_HOST;
	set_bit(EV_KEY, inputdev->evbit);
	set_bit(EV_REP, inputdev->evbit);
	set_bit(EV_MSC, inputdev->evbit);
	set_bit(MSC_SCAN, inputdev->mscbit);
	inputdev->phys = "/dev/ir";

	for (i = 0; i < KEY_CNT; i++)
		__set_bit(i, inputdev->keybit);

	__clear_bit(BTN_TOUCH, inputdev->keybit);

	ret = input_register_device(inputdev);
	if (ret) {
		pr_err("%s: failed to register device\n", __FILE__);

		goto exit_thread;
	}

	pr_info("%s:%d init inputdev X\n", __func__, __LINE__);

	for (;;) {
		wait_event_interruptible(mtk_ir->kthread_waitq,
					kthread_should_stop() ||
					(mtk_ir->readpos != mtk_ir->writepos));
		if (kthread_should_stop())
			break;

		r = mtk_ir->readpos;
		w = mtk_ir->writepos;

		while (r != w) {
			event = &mtk_ir->keyevent_buffer[r];
			pr_info("[%s] received_keyevent: 0x%X , 0x%X\n",
			__func__,
			event->keycode,
			event->pressed);
			input_report_key(mtk_ir->inputdev,
					event->keycode,
					event->pressed);

			input_sync(mtk_ir->inputdev);
			r = (r+1) & MAX_KEY_EVENT_BUFFER_MASK;
		}
		mtk_ir->readpos = r;

	}
	return 0;
exit_thread:
	if (inputdev)
		input_free_device(inputdev);
	do_exit(0);
	return ret;
}

//static int mtk_ir_rpmsginit(struct mtk_ir_dev *mtk_ir)
//{
//	struct input_svcmsg svcmsg = { 0 };
//	struct input_svcmsg_open *msg = (struct input_svcmsg_open *)&svcmsg;
//	int ret;
//
//
//	msg->msg_id = INPUT_SVCMSG_OPEN;
//	ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
//	if (ret)
//		pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);
//
//	return ret;
//}
static int mtk_ir_suspend_prepare(struct mtk_ir_dev *mtk_ir)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int mtk_ir_resume_complete(struct mtk_ir_dev *mtk_ir)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int mtk_ir_pm_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *ptr)
{
	struct mtk_ir_dev *mtk_ir = (struct mtk_ir_dev *)container_of(nb,
								   struct mtk_ir_dev, pm_notifier);
	switch (event) {
	case PM_SUSPEND_PREPARE:
		mtk_ir_suspend_prepare(mtk_ir);
		pr_info("[%s]PM_SUSPEND_PREPARE\n", __func__);
		_ir_wakeup_key = 0;//Clean
		break;
	case PM_POST_SUSPEND:
		mtk_ir_resume_complete(mtk_ir);
		pr_info("[%s]PM_POST_SUSPEND\n", __func__);
		mtk_ir_get_wakeup_key(mtk_ir);
		break;
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_PM
static int mtk_ir_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
	_ir_wakeup_key = 0;//Clean

	return 0;
}

static int mtk_ir_resume(struct device *dev)
{
	struct mtk_ir_dev *mtk_ir = NULL;

	pr_info("%s\n", __func__);
	mtk_ir = dev_get_drvdata(dev);
	mtk_ir_get_wakeup_key(mtk_ir);

	return 0;
}
static int mtk_ir_suspend_late(struct device *dev)
{
	struct mtk_ir_dev *mtk_ir = dev_get_drvdata(dev);
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_suspend *msg = (struct input_svcmsg_suspend *)&svcmsg;
	struct input_climsg_suspend *reply = (struct input_climsg_suspend *)&mtk_ir->reply;
	int ret;
	int retry = MAX_RETRY_CNT;

	pr_info("%s\n", __func__);
	pr_info("[%s] Start drop keyevent+", __func__);
	msg->msg_id = INPUT_SVCMSG_SUSPEND;
	do {
		ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
		if (ret)
			pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);
		wait_for_completion_interruptible_timeout(&mtk_ir->ack, WAIT_STR_TIMEOUT);
		pr_info("[%s]PM_SUSPEND_PREPARE:%d\n", __func__, retry);
		retry--;
	} while (!!reply->ret && retry > 0);
	return ret;
}

static int mtk_ir_resume_early(struct device *dev)
{
	struct mtk_ir_dev *mtk_ir = dev_get_drvdata(dev);
	struct input_climsg_resume *reply = (struct input_climsg_resume *)&mtk_ir->reply;
	struct input_svcmsg svcmsg = { 0 };
	struct input_svcmsg_resume *msg = (struct input_svcmsg_resume *)&svcmsg;
	int ret;
	int retry = MAX_RETRY_CNT;

	pr_info("%s\n", __func__);
	msg->msg_id = INPUT_SVCMSG_RESUME;
	do {
		ret = rpmsg_send(mtk_ir->rpdev->ept, (void *)&svcmsg, sizeof(svcmsg));
		if (ret)
			pr_err("[%s]rpmsg send failed, ret=%d\n", __func__, ret);
		wait_for_completion_interruptible_timeout(&mtk_ir->ack, WAIT_STR_TIMEOUT);
		pr_info("[%s]PM_RESUME_PREPARE:%d\n", __func__, retry);
		retry--;
	} while (!!reply->ret && retry > 0);

	pr_info("[%s] Start received keyevent", __func__);


	return ret;
}

static const struct dev_pm_ops mtk_ir_pm_ops = {
	.suspend = mtk_ir_suspend,
	.resume = mtk_ir_resume,
	.suspend_late = mtk_ir_suspend_late,
	.resume_early = mtk_ir_resume_early,
};
#endif

static int mtk_ir_probe(struct rpmsg_device *rpdev)
{
	int ret = 0;
	struct mtk_ir_dev *mtk_ir = NULL;
	struct input_dev *inputdev = NULL;

	mtk_ir = kzalloc(sizeof(struct mtk_ir_dev), GFP_KERNEL);
	inputdev = input_allocate_device();
	if (!mtk_ir || !inputdev) {
		pr_err("%s: Not enough memory\n", __FILE__);
		ret = -ENOMEM;
		goto err_allocate_dev;
	}

	mtk_ir->rpdev = rpdev;
	inputdev->dev.parent = &rpdev->dev;
	mtk_ir->inputdev = inputdev;
	init_completion(&mtk_ir->ack);

	dev_set_drvdata(&rpdev->dev, mtk_ir);
	init_waitqueue_head(&mtk_ir->kthread_waitq);

	mtk_ir->task = kthread_run(mtk_ir_thread, (void *)mtk_ir, "mtk_ir_thread");
	if (IS_ERR(mtk_ir->task)) {
		pr_err("create thread for add spi failed!\n");
		ret = IS_ERR(mtk_ir->task);
		goto err_kthread_run;
	}

	mtk_ir->pm_notifier.notifier_call = mtk_ir_pm_notifier_cb;
	ret = register_pm_notifier(&mtk_ir->pm_notifier);
	if (ret) {
		pr_err("%s: couldn't register pm notifier\n", __func__);

		goto err_register_pm_notifier;
	}
	/*
	 * TODO: Remove the mtk_ir_rpmsginit() & INPUT_SVCMSG_OPEN.
	 * After create a RPMsg endpoint in pmu, initialize it with a name,
	 * source address,remoteproc address.
	 * But Can't get remoteproc address in PMU until the first command is received.
	 */
	//mtk_ir_rpmsginit(mtk_ir);
	//mtk_ir_get_wakeup_key(mtk_ir);

	ir_control_dev = platform_device_alloc("ir_control", -1);
	if (!ir_control_dev) {
		dev_err(&rpdev->dev, "[%s] Can not create platform device for ir control.\n"
					, __func__);
		goto err_attr;
	}
	dev_set_drvdata(&ir_control_dev->dev, mtk_ir);
	ret = platform_device_add(ir_control_dev);
	if (ret) {
		dev_err(&rpdev->dev, "[%s] Can not add platform device for ir control.\n"
					, __func__);
		goto err_attr;
	}

	ret = sysfs_create_group(&ir_control_dev->dev.kobj, &mtk_dtv_ir_attr_group);

	if (ret) {
		dev_err(&rpdev->dev, "[%s] Couldn't create sysfs\n", __func__);
		goto err_attr;
	}
	return 0;

err_attr:
	sysfs_remove_group(&rpdev->dev.kobj, &mtk_dtv_ir_attr_group);
	unregister_pm_notifier(&mtk_ir->pm_notifier);

err_register_pm_notifier:
err_kthread_run:
err_allocate_dev:
	if (inputdev)
		input_free_device(inputdev);

	kfree(mtk_ir);
	return ret;
}

void mtk_ir_remove(struct rpmsg_device *rpdev)
{
	struct mtk_ir_dev *mtk_ir = dev_get_drvdata(&rpdev->dev);

	kthread_stop(mtk_ir->task);
	input_unregister_device(mtk_ir->inputdev);
	unregister_pm_notifier(&mtk_ir->pm_notifier);
	input_free_device(mtk_ir->inputdev);
	kfree(mtk_ir);
}

static struct rpmsg_device_id mtk_ir_id_table[] = {
	{.name = "ir0"},	// this name must match with rtos
	{},
};
MODULE_DEVICE_TABLE(rpmsg, mtk_ir_id_table);

static struct rpmsg_driver mtk_ir_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
#ifdef CONFIG_PM
	.drv.pm		= &mtk_ir_pm_ops,
#endif
	.id_table	= mtk_ir_id_table,
	.probe		= mtk_ir_probe,
	.callback	= mtk_ir_callback,
	.remove		= mtk_ir_remove,
};

module_rpmsg_driver(mtk_ir_driver);

MODULE_AUTHOR("jefferry.yen <jefferry.yen@mediatek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek dtv ir receiver controller driver");
