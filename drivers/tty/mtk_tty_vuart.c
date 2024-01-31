// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 STMicroelectronics - All Rights Reserved
 * Copyright (c) 2021 MediaTek Inc.
 *
 */

#define pr_fmt(fmt)		KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/string.h>

#define TTY_VUART_NAME	"ttyVUART"
#define MAX_TTY_COUNT	4
#define RPMSG_MTU_SIZE	496
#define VUART_NAME_SIZE 64
#define UART_REQUEST_TIMEOUT	(5 * HZ)

static DEFINE_IDR(tty_idr);	/* tty instance id */
static DEFINE_MUTEX(idr_lock);	/* protects tty_idr */

static struct tty_driver *tty_vuart_driver;
static unsigned long record_log;

struct tty_vuart_port {
	struct tty_port		port;	 /* TTY port data */
	int			id;	 /* TTY rpmsg index */
	struct rpmsg_device	*rpdev;	 /* rpmsg device */
	char buf[RPMSG_MTU_SIZE];
};

static ssize_t record_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "record_log = %lu\n", record_log);
}

static ssize_t record_log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct rpmsg_device *rpdev = NULL;
	int err;

	err = kstrtoul(buf, 0, &record_log);
	if (err)
		return err;

	if (dev && record_log) {
		uint32_t header = 0;

		rpdev = container_of(dev, struct rpmsg_device, dev);
		/* dummy command to enable rpmsg rx */
		rpmsg_send(rpdev->ept, (void *)&header, sizeof(header));
		pr_debug("Enable vuart rpmsg rx\n");
	}

	return count;
}


static int tty_vuart_cb(struct rpmsg_device *rpdev, void *data, int len, void *priv, u32 src)
{
	struct tty_vuart_port *cport = dev_get_drvdata(&rpdev->dev);
	int copied;

	pr_debug("%s:%d E\n", __func__, __LINE__);
	if (!len)
		return -EINVAL;
	copied = tty_insert_flip_string(&cport->port, data, len);
	if (copied != len)
		dev_err_ratelimited(&rpdev->dev, "Trunc buffer: available space is %d\n", copied);
	tty_flip_buffer_push(&cport->port);

	/* push data into dmesg buffer */
	if (record_log) {
		char *str = NULL, *cur = data;

		while ((str = strsep(&cur, "\n"))) {
			if (*str != '\0')
				pr_info("%s\n", str);
		}
	}

	pr_debug("%s:%d X\n", __func__, __LINE__);
	return 0;
}

static int tty_vuart_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct tty_vuart_port *cport = idr_find(&tty_idr, tty->index);
	struct tty_port *port;

	pr_info("%s:%d E\n", __func__, __LINE__);
	tty->driver_data = cport;

	port = tty_port_get(&cport->port);
	return tty_port_install(port, driver, tty);
}

static void tty_vuart_cleanup(struct tty_struct *tty)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	tty_port_put(tty->port);
}

static int tty_vuart_open(struct tty_struct *tty, struct file *filp)
{
	// dummy command to enable rpmsg rx
	struct tty_vuart_port *cport = tty->driver_data;
	struct rpmsg_device *rpdev = cport->rpdev;
	uint32_t header = 0;

	rpmsg_send(rpdev->ept, (void *)&header, sizeof(header));
	return tty_port_open(tty->port, tty, filp);
}

static void tty_vuart_close(struct tty_struct *tty, struct file *filp)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	return tty_port_close(tty->port, tty, filp);
}

static int tty_vuart_write(struct tty_struct *tty, const u8 *buf, int len)
{
	struct tty_vuart_port *cport = tty->driver_data;
	struct rpmsg_device *rpdev;
	int msg_size, count, ret;

	pr_info("%s:%d E\n", __func__, __LINE__);
	rpdev = cport->rpdev;
	count = 0;
	while (len > 0) {
		/*
		 * Use rpmsg_trysend instead of rpmsg_send to send the message so the caller is not
		 * hung until a rpmsg buffer is available.
		 * In such case rpmsg_trysend returns -ENOMEM.
		 */

		// payload size without 4 byte header
		msg_size = min(len, RPMSG_MTU_SIZE - 4);
		memcpy(cport->buf + 4, buf, msg_size);

		// send payload with header
		ret = rpmsg_trysend(rpdev->ept, (void *)cport->buf, msg_size + 4);
		if (ret) {
			dev_dbg_ratelimited(&rpdev->dev, "rpmsg_send failed: %d\n", ret);
			return ret;
		}
		buf += msg_size;
		count += msg_size;
		len -= msg_size;
	}
	pr_info("%s:%d X\n", __func__, __LINE__);
	return count;
}

static int tty_vuart_write_room(struct tty_struct *tty)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	return RPMSG_MTU_SIZE;
}

static void tty_vuart_hangup(struct tty_struct *tty)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	tty_port_hangup(tty->port);
}

static const struct tty_operations tty_vuart_ops = {
	.install	= tty_vuart_install,
	.open		= tty_vuart_open,
	.close		= tty_vuart_close,
	.write		= tty_vuart_write,
	.write_room	= tty_vuart_write_room,
	.hangup		= tty_vuart_hangup,
	.cleanup	= tty_vuart_cleanup,
};

static struct tty_vuart_port *tty_vuart_alloc_cport(void)
{
	struct tty_vuart_port *cport;
	int ret;

	cport = kzalloc(sizeof(*cport), GFP_KERNEL);
	if (!cport)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&idr_lock);
	ret = idr_alloc(&tty_idr, cport, 0, MAX_TTY_COUNT, GFP_KERNEL);
	mutex_unlock(&idr_lock);

	if (ret < 0) {
		kfree(cport);
		return ERR_PTR(ret);
	}

	cport->id = ret;

	return cport;
}

static void tty_vuart_destruct_port(struct tty_port *port)
{
	struct tty_vuart_port *cport = container_of(port, struct tty_vuart_port, port);

	mutex_lock(&idr_lock);
	idr_remove(&tty_idr, cport->id);
	mutex_unlock(&idr_lock);

	kfree(cport);
}

static const struct tty_port_operations tty_vuart_port_ops = {
	.destruct = tty_vuart_destruct_port,
};

static DEVICE_ATTR_RW(record_log);
static struct attribute *mtk_vuart_attrs[] = {
	&dev_attr_record_log.attr,
	NULL,
};

static const struct attribute_group mtk_vuart_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_vuart_attrs
};

static int tty_vuart_probe(struct rpmsg_device *rpdev)
{
	char name[VUART_NAME_SIZE];
	struct tty_vuart_port *cport;
	struct device *dev;
	struct device *tty_dev;
	int ret;

	if (!rpdev) {
		pr_err("%s:%d Null pointer\n", __func__, __LINE__);
		return -EINVAL;
	}
	dev = &rpdev->dev;

	cport = tty_vuart_alloc_cport();
	if (IS_ERR(cport)) {
		pr_err("%s:%d Failed to alloc tty port\n", __func__, __LINE__);
		return PTR_ERR(cport);
	}

	tty_port_init(&cport->port);
	cport->port.ops = &tty_vuart_port_ops;

	if (strcmp(rpdev->id.name, "vuart0") != 0) {
		if (snprintf(name, sizeof(name), "%s-%s", TTY_VUART_NAME, rpdev->id.name) > 0) {
			tty_vuart_driver->name = name;
			tty_vuart_driver->flags = TTY_DRIVER_UNNUMBERED_NODE;
		}
	}

	tty_dev = tty_port_register_device(&cport->port, tty_vuart_driver,
					   cport->id, dev);
	tty_vuart_driver->flags = 0;
	if (IS_ERR(tty_dev)) {
		pr_err("%s:%d Failed to register tty port\n", __func__, __LINE__);
		ret = PTR_ERR(tty_dev);
		tty_port_put(&cport->port);
		return ret;
	}

	cport->rpdev = rpdev;

	dev_set_drvdata(dev, cport);

	sysfs_create_group(&dev->kobj, &mtk_vuart_attr_group);

	pr_info("%s:%d New channel: 0x%x -> 0x%x: " TTY_VUART_NAME "%d\n",
		__func__, __LINE__, rpdev->src, rpdev->dst, cport->id);

	return 0;
}

static void tty_vuart_remove(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;
	struct tty_vuart_port *cport = dev_get_drvdata(&rpdev->dev);

	dev_dbg(&rpdev->dev, "Removing rpmsg tty device %d\n", cport->id);

	sysfs_remove_group(&dev->kobj, &mtk_vuart_attr_group);

	/* User hang up to release the tty */
	tty_port_tty_hangup(&cport->port, false);

	tty_unregister_device(tty_vuart_driver, cport->id);

	tty_port_put(&cport->port);
}

static struct rpmsg_device_id rpmsg_driver_tty_id_table[] = {
	{ .name	= "vuart0" },
	{ .name	= "pqu-vuart" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_tty_id_table);

static struct rpmsg_driver tty_vuart_rpmsg_drv = {
	.drv.name	= KBUILD_MODNAME,
	.id_table	= rpmsg_driver_tty_id_table,
	.probe		= tty_vuart_probe,
	.callback	= tty_vuart_cb,
	.remove		= tty_vuart_remove,
};

static int __init tty_vuart_init(void)
{
	int ret;

	tty_vuart_driver = tty_alloc_driver(MAX_TTY_COUNT, TTY_DRIVER_REAL_RAW |
					    TTY_DRIVER_DYNAMIC_DEV);
	if (IS_ERR(tty_vuart_driver))
		return PTR_ERR(tty_vuart_driver);

	tty_vuart_driver->driver_name = "tty_vuart";
	tty_vuart_driver->name = TTY_VUART_NAME;
	tty_vuart_driver->major = 0;
	tty_vuart_driver->type = TTY_DRIVER_TYPE_CONSOLE;

	/* Disable unused mode by default */
	tty_vuart_driver->init_termios = tty_std_termios;
	tty_vuart_driver->init_termios.c_lflag &= ~(ECHO | ICANON);
	tty_vuart_driver->init_termios.c_oflag &= ~(OPOST | ONLCR);

	tty_set_operations(tty_vuart_driver, &tty_vuart_ops);

	ret = tty_register_driver(tty_vuart_driver);
	if (ret < 0) {
		pr_err("Couldn't install driver: %d\n", ret);
		goto error_put;
	}

	ret = register_rpmsg_driver(&tty_vuart_rpmsg_drv);
	if (ret < 0) {
		pr_err("Couldn't register driver: %d\n", ret);
		goto error_unregister;
	}

	return 0;

error_unregister:
	tty_unregister_driver(tty_vuart_driver);

error_put:
	tty_driver_kref_put(tty_vuart_driver);

	return ret;
}

static void __exit tty_vuart_exit(void)
{
	unregister_rpmsg_driver(&tty_vuart_rpmsg_drv);
	tty_unregister_driver(tty_vuart_driver);
	tty_driver_kref_put(tty_vuart_driver);
	idr_destroy(&tty_idr);
}

module_init(tty_vuart_init);
module_exit(tty_vuart_exit);

MODULE_AUTHOR("Kris Ma <kris.ma@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DTV SoC remote processor messaging tty driver");
MODULE_LICENSE("GPL v2");
