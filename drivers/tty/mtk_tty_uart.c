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

#define TTY_UART_NAME		"ttyUART-PROXY"
#define MAX_TTY_COUNT			4
#define UART_REQUEST_TIMEOUT	(5 * HZ)

static DEFINE_IDR(tty_idr);	/* tty instance id */
static DEFINE_MUTEX(idr_lock);	/* protects tty_idr */

static struct tty_driver *tty_uart_driver;

#define MAX_TRANSFER_DATA_SIZE			488

//msg_id
#define UART_MSG_BASE					(0)
#define UART_MSG_START					(UART_MSG_BASE+1)
#define UART_MSG_STOP					(UART_MSG_BASE+2)
#define UART_MSG_WRITE					(UART_MSG_BASE+3)
#define UART_MSG_READ					(UART_MSG_BASE+4)
#define UART_MSG_CONFIG					(UART_MSG_BASE+5)
#define UART_MSG_SEND_BREAK				(UART_MSG_BASE+6)
#define UART_MSG_QUERY_WRITE_ROOM		(UART_MSG_BASE+7)

struct uart_generic_msg {
	uint8_t msg_id;
	uint8_t payload[15];
};

struct uart_svcmsg_start {
	uint8_t msg_id;
};

struct uart_svcmsg_stop {
	uint8_t msg_id;
};

struct uart_svcmsg_write {
	uint8_t msg_id;
	uint16_t length;
	char data[MAX_TRANSFER_DATA_SIZE];
};

struct uart_svcmsg_config {
	uint8_t msg_id;
	uint8_t databits;
	uint8_t paritybit;
	uint8_t stopbits;
	uint32_t baud;
};

struct uart_svcmsg_send_break {
	uint8_t msg_id;
	bool enable;
};

struct uart_climsg_reply {
	uint8_t msg_id;
	int32_t ret;
};

struct uart_climsg_read {
	uint8_t msg_id;
	uint16_t length;
	char data[MAX_TRANSFER_DATA_SIZE];
};

struct uart_climsg_config {
	uint8_t msg_id;
	int8_t databits;
	int8_t paritybit;
	int8_t stopbits;
	int32_t baud;
};

struct tty_uart_port {
	struct tty_port	port;			/* TTY port data */
	int	id;				/* TTY rpmsg index */
	struct rpmsg_device	*rpdev;		/* rpmsg device */
	struct completion ack;
	struct uart_svcmsg_write txmsg;
	struct uart_generic_msg climsg;
};

static int tty_uart_cb(struct rpmsg_device *rpdev, void *data, int len, void *priv, u32 src)
{
	struct tty_uart_port *cport = dev_get_drvdata(&rpdev->dev);
	int copied;
	struct uart_generic_msg *climsg = (struct uart_generic_msg *)data;

	pr_info("%s:%d E, climsg->msg_id %u\n", __func__, __LINE__, climsg->msg_id);
	if (!len)
		return -EINVAL;

	switch (climsg->msg_id) {
	case UART_MSG_QUERY_WRITE_ROOM:
	case UART_MSG_CONFIG:
	case UART_MSG_SEND_BREAK:
		*((struct uart_generic_msg *)&cport->climsg) = *((struct uart_generic_msg *)data);
		complete(&cport->ack);
		break;
	default:
		len -= 4; //remove msg length size
		data += 4; //skip msg length size
		copied = tty_insert_flip_string(&cport->port, data, len);
		if (copied != len) {
			dev_err_ratelimited(&rpdev->dev, "Trunc buffer: available space is %d\n",
				copied);
		}
		tty_flip_buffer_push(&cport->port);
		break;
	}
	pr_info("%s:%d X\n", __func__, __LINE__);
	return 0;
}

static int tty_uart_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct tty_uart_port *cport = idr_find(&tty_idr, tty->index);
	struct tty_port *port;

	pr_info("%s:%d E\n", __func__, __LINE__);
	tty->driver_data = cport;

	port = tty_port_get(&cport->port);
	return tty_port_install(port, driver, tty);
}

static void tty_uart_cleanup(struct tty_struct *tty)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	tty_port_put(tty->port);
}

static int tty_uart_open(struct tty_struct *tty, struct file *filp)
{
	return tty_port_open(tty->port, tty, filp);
}

static void tty_uart_close(struct tty_struct *tty, struct file *filp)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	return tty_port_close(tty->port, tty, filp);
}

static int tty_uart_write(struct tty_struct *tty, const u8 *buf, int len)
{
	struct tty_uart_port *cport = tty->driver_data;
	struct rpmsg_device *rpdev = cport->rpdev;
	int count = 0, ret;

	pr_info("%s:%d E\n", __func__, __LINE__);
	while (len > 0) {
		/*
		 * Use rpmsg_trysend instead of rpmsg_send to send the message so the caller is not
		 * hung until a rpmsg buffer is available.
		 * In such case rpmsg_trysend returns -ENOMEM.
		 */
		cport->txmsg.msg_id = UART_MSG_WRITE;
		cport->txmsg.length = min(len, MAX_TRANSFER_DATA_SIZE);
		pr_info("%s:%d msg.length %u\n", __func__, __LINE__, cport->txmsg.length);
		memcpy(cport->txmsg.data, buf, cport->txmsg.length);
		ret = rpmsg_trysend(rpdev->ept, (void *)&cport->txmsg, 4 + cport->txmsg.length);
		if (ret) {
			dev_dbg_ratelimited(&rpdev->dev, "rpmsg_send failed: %d\n", ret);
			return ret;
		}
		buf += cport->txmsg.length;
		count += cport->txmsg.length;
		len -= cport->txmsg.length;
	}
	pr_info("%s:%d src 0x%x-> dst 0x%x, ept 0x%p, ept addr 0x%x X\n",
		__func__, __LINE__, rpdev->src, rpdev->dst, rpdev->ept,
		rpdev->ept->addr);
	return count;
}

static int tty_uart_write_room(struct tty_struct *tty)
{
	return MAX_TRANSFER_DATA_SIZE;
}

static void tty_uart_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	struct tty_uart_port *cport = tty->driver_data;
	struct rpmsg_device *rpdev;
	struct uart_svcmsg_config config = {0};
	int ret;
	unsigned int ctrl, cflag = tty->termios.c_cflag;

	pr_info("%s:%d E, cflag %u, c_ispeed %u, c_ospeed %u\n",
		__func__, __LINE__, cflag, tty->termios.c_ispeed, tty->termios.c_ospeed);
	rpdev = cport->rpdev;

	config.baud = tty->termios.c_ospeed;

	ctrl = cflag & CSIZE;
	if (ctrl == CS5)
		config.databits = 5;
	else if (ctrl == CS6)
		config.databits = 6;
	else if (ctrl == CS7)
		config.databits = 7;
	else if (ctrl == CS8)
		config.databits = 8;

	if (cflag & PARENB)	{
		if (cflag & PARODD)
			config.paritybit = 1;
		else
			config.paritybit = 2;
	} else
		config.paritybit = 0;
	if (cflag & CSTOPB)
		config.stopbits = 2;
	else
		config.stopbits = 1;

	config.msg_id = UART_MSG_CONFIG;
	ret = rpmsg_send(rpdev->ept, (void *)&config, sizeof(struct uart_svcmsg_config));
	if (ret) {
		pr_err("%s:%d rpmsg_send failed: %d\n", __func__, __LINE__, ret);
		return;
	}
	ret = wait_for_completion_timeout(&cport->ack, UART_REQUEST_TIMEOUT);
	if (!ret) {
		pr_err("%s:%d wait_for_completion_timeout failed: %d\n", __func__, __LINE__, ret);
		return;
	}

	config = *((struct uart_svcmsg_config *)&cport->climsg);

	// update old termios
	cflag = 0;

	tty->termios.c_ispeed = config.baud;
	tty->termios.c_ospeed = config.baud;

	if (config.databits == 5)
		cflag |= CS5;
	else if (config.databits == 6)
		cflag |= CS6;
	else if (config.databits == 7)
		cflag |= CS7;
	else if (config.databits == 8)
		cflag |= CS8;
	if (config.paritybit == 2)
		cflag |= PARENB;
	else if (config.paritybit == 1)
		cflag |= (PARENB | PARODD);
	if (config.stopbits == 2)
		cflag |= CSTOPB;

	*old = tty->termios;
	old->c_cflag = cflag;
	pr_info("%s:%d X\n", __func__, __LINE__);
}

static void tty_uart_hangup(struct tty_struct *tty)
{
	pr_info("%s:%d E\n", __func__, __LINE__);
	tty_port_hangup(tty->port);
}

static int tty_uart_set_breakctrl(struct tty_struct *tty, int state)
{
	struct tty_uart_port *cport = tty->driver_data;
	struct uart_svcmsg_send_break msg = {0};
	int ret = 0;
	struct rpmsg_device *rpdev;

	pr_info("%s:%d E, state %d\n", __func__, __LINE__, state);
	rpdev = cport->rpdev;
	msg.msg_id = UART_MSG_SEND_BREAK;
	if (state == -1)
		msg.enable = true;
	else
		msg.enable = false;

	ret = rpmsg_send(rpdev->ept, (void *)&msg, sizeof(struct uart_svcmsg_send_break));
	if (ret)
		pr_err("%s:%d rpmsg_send failed: %d\n", __func__, __LINE__, ret);
	ret = wait_for_completion_timeout(&cport->ack, UART_REQUEST_TIMEOUT);
	if (!ret) {
		pr_err("%s:%d wait_for_completion_timeout failed: %d\n", __func__, __LINE__, ret);
		return -ETIMEDOUT;
	}
	ret = ((struct uart_climsg_reply *)&cport->climsg)->ret;
	pr_info("%s:%d ret %d X\n", __func__, __LINE__, ret);
	return ret;
}

static const struct tty_operations tty_uart_ops = {
	.install	= tty_uart_install,
	.open		= tty_uart_open,
	.close		= tty_uart_close,
	.write		= tty_uart_write,
	.write_room	= tty_uart_write_room,
	.set_termios	= tty_uart_set_termios,
	.hangup		= tty_uart_hangup,
	.cleanup	= tty_uart_cleanup,
	.break_ctl	= tty_uart_set_breakctrl,
};

static struct tty_uart_port *tty_uart_alloc_cport(void)
{
	struct tty_uart_port *cport;
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

static void tty_uart_destruct_port(struct tty_port *port)
{
	struct tty_uart_port *cport = container_of(port, struct tty_uart_port, port);

	mutex_lock(&idr_lock);
	idr_remove(&tty_idr, cport->id);
	mutex_unlock(&idr_lock);

	kfree(cport);
}

static const struct tty_port_operations tty_uart_port_ops = {
	.destruct = tty_uart_destruct_port,
};


static int tty_uart_probe(struct rpmsg_device *rpdev)
{
	struct tty_uart_port *cport;
	struct device *dev = &rpdev->dev;
	struct device *tty_dev;
	int ret;

	cport = tty_uart_alloc_cport();
	if (IS_ERR(cport)) {
		pr_err("%s:%d Failed to alloc tty port\n", __func__, __LINE__);
		return PTR_ERR(cport);
	}

	tty_port_init(&cport->port);
	cport->port.ops = &tty_uart_port_ops;
	init_completion(&cport->ack);

	tty_dev = tty_port_register_device(&cport->port, tty_uart_driver,
					   cport->id, dev);
	if (IS_ERR(tty_dev)) {
		pr_err("%s:%d Failed to register tty port\n", __func__, __LINE__);
		ret = PTR_ERR(tty_dev);
		tty_port_put(&cport->port);
		return ret;
	}

	cport->rpdev = rpdev;

	dev_set_drvdata(dev, cport);
	{
		char cmd[4] = {UART_MSG_QUERY_WRITE_ROOM, 0, 0, 0};

		rpmsg_send(rpdev->ept, (void *)&cmd, sizeof(cmd));
	}

	pr_info("%s:%d New channel: 0x%x -> 0x%x, ept 0x%p, ept addr 0x%x: " TTY_UART_NAME "%d\n",
		__func__, __LINE__, rpdev->src, rpdev->dst, rpdev->ept,
		rpdev->ept->addr, cport->id);

	return 0;
}

static void tty_uart_remove(struct rpmsg_device *rpdev)
{
	struct tty_uart_port *cport = dev_get_drvdata(&rpdev->dev);

	dev_dbg(&rpdev->dev, "Removing rpmsg tty device %d\n", cport->id);

	/* User hang up to release the tty */
	tty_port_tty_hangup(&cport->port, false);

	tty_unregister_device(tty_uart_driver, cport->id);

	tty_port_put(&cport->port);
}

static struct rpmsg_device_id rpmsg_driver_tty_id_table[] = {
	{ .name	= "uart0" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_tty_id_table);

static struct rpmsg_driver tty_uart_rpmsg_drv = {
	.drv.name	= KBUILD_MODNAME,
	.id_table	= rpmsg_driver_tty_id_table,
	.probe		= tty_uart_probe,
	.callback	= tty_uart_cb,
	.remove		= tty_uart_remove,
};

static int __init tty_uart_init(void)
{
	int ret;

	tty_uart_driver = tty_alloc_driver(MAX_TTY_COUNT, TTY_DRIVER_REAL_RAW |
					    TTY_DRIVER_DYNAMIC_DEV);
	if (IS_ERR(tty_uart_driver))
		return PTR_ERR(tty_uart_driver);

	tty_uart_driver->driver_name = "tty_uart";
	tty_uart_driver->name = TTY_UART_NAME;
	tty_uart_driver->major = 0;
	tty_uart_driver->type = TTY_DRIVER_TYPE_CONSOLE;

	/* Disable unused mode by default */
	tty_uart_driver->init_termios = tty_std_termios;
	tty_uart_driver->init_termios.c_lflag &= ~(ECHO | ICANON);
	tty_uart_driver->init_termios.c_oflag &= ~(OPOST | ONLCR);

	tty_set_operations(tty_uart_driver, &tty_uart_ops);

	ret = tty_register_driver(tty_uart_driver);
	if (ret < 0) {
		pr_err("Couldn't install driver: %d\n", ret);
		goto error_put;
	}

	ret = register_rpmsg_driver(&tty_uart_rpmsg_drv);
	if (ret < 0) {
		pr_err("Couldn't register driver: %d\n", ret);
		goto error_unregister;
	}

	return 0;

error_unregister:
	tty_unregister_driver(tty_uart_driver);

error_put:
	tty_driver_kref_put(tty_uart_driver);

	return ret;
}

static void __exit tty_uart_exit(void)
{
	unregister_rpmsg_driver(&tty_uart_rpmsg_drv);
	tty_unregister_driver(tty_uart_driver);
	tty_driver_kref_put(tty_uart_driver);
	idr_destroy(&tty_idr);
}

module_init(tty_uart_init);
module_exit(tty_uart_exit);

MODULE_AUTHOR("Kris Ma <kris.ma@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DTV SoC remote processor messaging tty driver");
MODULE_LICENSE("GPL v2");
