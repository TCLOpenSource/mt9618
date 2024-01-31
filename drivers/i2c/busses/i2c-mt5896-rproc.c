// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>

#define MTK_I2C_LOG_NONE	(-1)	/* set mtk loglevel to be useless */
#define MTK_I2C_LOG_MUTE	(0)
#define MTK_I2C_LOG_ERR		(3)
#define MTK_I2C_LOG_WARN	(4)
#define MTK_I2C_LOG_INFO	(6)
#define MTK_I2C_LOG_DEBUG	(7)

#ifdef DEBUG
#define DUMP_ROW_BYTES		(16)
#endif

#define i2c_log(i2c_dev, level, args...) \
do { \
	if (i2c_dev->log_level < 0) { \
		if (level == MTK_I2C_LOG_DEBUG) \
			dev_dbg(i2c_dev->dev, ## args); \
		else if (level == MTK_I2C_LOG_INFO) \
			dev_info(i2c_dev->dev, ## args); \
		else if (level == MTK_I2C_LOG_WARN) \
			dev_warn(i2c_dev->dev, ## args); \
		else if (level == MTK_I2C_LOG_ERR) \
			dev_err(i2c_dev->dev, ## args); \
	} else if (i2c_dev->log_level >= level) \
		dev_emerg(i2c_dev->dev, ## args); \
} while (0)

#define rpmsg_log(rpmsg_dev, level, args...) \
		do { \
			if (!rpmsg_dev->i2c_dev) { \
				if (level == MTK_I2C_LOG_DEBUG) \
					dev_dbg(rpmsg_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_INFO) \
					dev_info(rpmsg_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_WARN) \
					dev_warn(rpmsg_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_ERR) \
					dev_err(rpmsg_dev->dev, ## args); \
			} else if (rpmsg_dev->i2c_dev->log_level < 0) { \
				if (level == MTK_I2C_LOG_DEBUG) \
					dev_dbg(rpmsg_dev->i2c_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_INFO) \
					dev_info(rpmsg_dev->i2c_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_WARN) \
					dev_warn(rpmsg_dev->i2c_dev->dev, ## args); \
				else if (level == MTK_I2C_LOG_ERR) \
					dev_err(rpmsg_dev->i2c_dev->dev, ## args); \
			} else if (rpmsg_dev->i2c_dev->log_level >= level) \
				dev_emerg(rpmsg_dev->i2c_dev->dev, ## args); \
		} while (0)

#define HWI2C_DMA_MASK_SIZE		(32)
#define I2C_PROXY_MSG_SIZE		(32 - 1)	// reserved for proxy command

enum i2c_proxy_cmd {
	I2C_PROXY_CMD_XFER = 0,
	//...
	I2C_PROXY_CMD_MAX,
};

struct i2c_proxy_host_i2c_msg {
	uint16_t addr;		/* slave address			*/
	uint16_t flags;
	uint16_t len;		/* msg length				*/
	uint64_t buf_dma_addr;	/* pointer to msg data DRAM bus address	*/
};

struct i2c_proxy_cmd_msg {
	uint8_t cmd;
	uint8_t payload[I2C_PROXY_MSG_SIZE];
};

struct i2c_proxy_ack_msg {
	uint8_t cmd;
	uint8_t payload[I2C_PROXY_MSG_SIZE];
};

struct i2c_proxy_msg_xfer {
	uint8_t cmd;
	uint64_t i2c_msgs_dma_addr;
	int32_t i2c_msgs_num;
};

struct i2c_proxy_ack_xfer {
	uint8_t cmd;
	int32_t i2c_msg_xfer_num;
};

struct mtk_rproc_i2c_bus_dev;

/**
 * struct mtk_rpmsg_i2c - rpmsg device driver private data
 *
 * @dev: platform device of rpmsg device
 * @rpdev: rpmsg device
 * @txn: rpmsg transaction callback
 * @txn_result: rpmsg transaction result
 * @receive_msg: received message of rpmsg transaction
 * @ack: completion of rpmsg transaction
 * @i2c_dev: i2c adapter of rpmsg device.
 * @list: for create a list
 */
struct mtk_rpmsg_i2c {
	struct device *dev;
	struct rpmsg_device *rpdev;
	int (*txn)(struct mtk_rpmsg_i2c *rpmsg_i2c, void *data, size_t len);
	int txn_result;
	struct i2c_proxy_ack_msg receive_msg;
	struct completion ack;
	struct mtk_rproc_i2c_bus_dev *i2c_dev;
	struct list_head list;
};

/**
 * struct mtk_rproc_i2c_bus_dev - i2c bus driver private data
 *
 * @dev: platform device of i2c bus
 * @adapter: i2c adapter of i2c subsystem
 * @rpmsg_dev: rpmsg device for send cmd to rproc
 * @rpmsg_bind: completion when rpmsg and i2c device binded
 * @bind_from_resume: bind function for resume sequence or probe sequence
 * @rproc_i2c_name: Required rproc i2c bus name
 * @dma_phy_off: Due to memory driver implementation, dma_addr_t have offset with DRAM address,
 *               rproc I2C proxy need DRAM address.
 * @list: for create a list
 * @log_level: override kernel log for mtk_dbg
 */
struct mtk_rproc_i2c_bus_dev {
	struct device *dev;
	struct i2c_adapter adapter;
	struct mtk_rpmsg_i2c *rpmsg_dev;
	struct completion rpmsg_bind;
	bool bind_from_resume;
	const char *rproc_i2c_name;
	u64 dma_phy_off;
	struct list_head list;
	int log_level;
};

/* A list of rproc_i2c_bus_dev */
static LIST_HEAD(rproc_i2c_bus_dev_list);
/* A list of rpmsg_dev */
static LIST_HEAD(rpmsg_dev_list);
/* Lock to serialise RPMSG device and I2C device binding */
static DEFINE_MUTEX(rpmsg_i2c_bind_lock);

/**
 * Due to memory driver implementation, need get i2c_dev->dma_phy_off and
 * use CMA for rproc I2C rpmsg bounce buffer
 */
/**
 * rproc I2C bounce buffer APIs
 */
static int _mtk_rproc_i2c_prepare_msg_buf(struct mtk_rproc_i2c_bus_dev *i2c_dev)
{
	struct device_node *target_memory_np = NULL;
	uint32_t len = 0;
	__be32 *p = NULL;
	int ret = 0;

	// find memory_info node in dts
	target_memory_np = of_find_node_by_name(NULL, "memory_info");
	if (!target_memory_np)
		return -ENODEV;

	// query cpu_emi0_base value from memory_info node
	p = (__be32 *)of_get_property(target_memory_np, "cpu_emi0_base", &len);
	if (p != NULL) {
		i2c_dev->dma_phy_off = be32_to_cpup(p);
		dev_info(i2c_dev->dev, "cpu_emi0_base = 0x%llX\n", i2c_dev->dma_phy_off);
		of_node_put(target_memory_np);
		p = NULL;
	} else {
		dev_err(i2c_dev->dev, "can not find cpu_emi0_base info\n");
		of_node_put(target_memory_np);
		return -EINVAL;
	}

	// reserved memory for DMA bounce buffer
	ret = of_reserved_mem_device_init(i2c_dev->dev);
	if (ret) {
		dev_warn(i2c_dev->dev, "reserved memory presetting failed\n");
		return ret;
	}

	ret = dma_set_mask_and_coherent(i2c_dev->dev, DMA_BIT_MASK(HWI2C_DMA_MASK_SIZE));
	if (ret) {
		dev_warn(i2c_dev->dev, "dma_set_mask_and_coherent failed\n");
		of_reserved_mem_device_release(i2c_dev->dev);
		return ret;
	}

	return 0;
}

/**
 * I2C bus device and rpmsg device binder
 */
static void _mtk_bind_rpmsg_to_rproc_i2c(struct mtk_rpmsg_i2c *rpmsg_dev)
{
	struct mtk_rproc_i2c_bus_dev *i2c_bus_dev, *n;

	mutex_lock(&rpmsg_i2c_bind_lock);

	dev_dbg(rpmsg_dev->dev, "Bind rpmsg dev %s to i2c adap\n", rpmsg_dev->rpdev->id.name);

	// check rproc i2c bus device list, link rproc i2c bus dev which is identical with rpmsg dev
	list_for_each_entry_safe(i2c_bus_dev, n, &rproc_i2c_bus_dev_list, list) {
		dev_dbg(rpmsg_dev->dev, "Got I2C adap %s\n", i2c_bus_dev->rproc_i2c_name);
		if (!strcmp(rpmsg_dev->rpdev->id.name, i2c_bus_dev->rproc_i2c_name)) {
			i2c_bus_dev->rpmsg_dev = rpmsg_dev;
			rpmsg_dev->i2c_dev = i2c_bus_dev;
			dev_dbg(rpmsg_dev->dev, "Signal I2C adap %s thread\n",
				i2c_bus_dev->rproc_i2c_name);
			complete(&i2c_bus_dev->rpmsg_bind);
			break;
		}
	}

	mutex_unlock(&rpmsg_i2c_bind_lock);
}

/**
 * RPMSG functions
 */
static int mtk_rpmsg_i2c_txn(struct mtk_rpmsg_i2c *rpmsg_i2c, void *data, size_t len)
{
	int ret;

	/* sanity check */
	if (!rpmsg_i2c) {
		pr_err("%s rpmsg dev is NULL\n", __func__);
		return -EINVAL;
	}

	if (!data || len == 0) {
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_ERR, "Invalid data or length\n");
		return -EINVAL;
	}

#ifdef DEBUG
	pr_info("Send:\n");
	print_hex_dump(KERN_INFO, " ", DUMP_PREFIX_OFFSET, DUMP_ROW_BYTES, 1, data, len, true);
#endif
	rpmsg_i2c->txn_result = -EIO;
	ret = rpmsg_send(rpmsg_i2c->rpdev->ept, data, len);
	if (ret) {
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_ERR, "rpmsg send failed, ret=%d\n", ret);
		return ret;
	}

	// wait for ack
	wait_for_completion_interruptible(&rpmsg_i2c->ack);

	return rpmsg_i2c->txn_result;
}

static int mtk_rpmsg_i2c_callback(struct rpmsg_device *rpdev, void *data, int len, void *priv,
				  u32 src)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c;

	/* sanity check */
	if (!rpdev) {
		pr_err("%s rpdev is NULL\n", __func__);
		return -EINVAL;
	}

	rpmsg_i2c = dev_get_drvdata(&rpdev->dev);
	if (!rpmsg_i2c) {
		pr_err("%s private data is NULL\n", __func__);
		return -EINVAL;
	}

	if (!data || len == 0) {
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_ERR, "Invalid data or length from src:%d\n", src);
		return -EINVAL;
	}

#ifdef DEBUG
	pr_info("%s:\n", __func__);
	print_hex_dump(KERN_INFO, " ", DUMP_PREFIX_OFFSET, DUMP_ROW_BYTES, 1, data, len, true);
#endif
	if (len > sizeof(struct i2c_proxy_ack_msg)) {
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_ERR, "Receive data length %d over size\n", len);
		rpmsg_i2c->txn_result = -E2BIG;
	} else {
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_DEBUG, "Got ack for cmd %d\n", *((u8 *)data));

		/* store received data for transaction checking */
		memcpy(&rpmsg_i2c->receive_msg, data, sizeof(struct i2c_proxy_ack_msg));
		rpmsg_i2c->txn_result = 0;
	}

	complete(&rpmsg_i2c->ack);

	return 0;
}

static int mtk_rpmsg_i2c_probe(struct rpmsg_device *rpdev)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c;

	dev_info(&rpdev->dev, "%s, id name %s\n", __func__, rpdev->id.name);

	rpmsg_i2c = devm_kzalloc(&rpdev->dev, sizeof(struct mtk_rpmsg_i2c), GFP_KERNEL);
	if (!rpmsg_i2c)
		return -ENOMEM;

	rpmsg_i2c->rpdev = rpdev;
	rpmsg_i2c->dev = &rpdev->dev;
	init_completion(&rpmsg_i2c->ack);
	rpmsg_i2c->txn = mtk_rpmsg_i2c_txn;

	dev_set_drvdata(&rpdev->dev, rpmsg_i2c);

	// add to rpmsg_dev list
	INIT_LIST_HEAD(&rpmsg_i2c->list);
	mutex_lock(&rpmsg_i2c_bind_lock);
	dev_dbg(&rpdev->dev, "Add rpmsg dev %s into rpmsg list\n", rpdev->id.name);
	list_add_tail(&rpmsg_i2c->list, &rpmsg_dev_list);
	mutex_unlock(&rpmsg_i2c_bind_lock);

	// trigger i2c_adap_add_resume_thread
	_mtk_bind_rpmsg_to_rproc_i2c(rpmsg_i2c);

	return 0;
}

static void mtk_rpmsg_i2c_remove(struct rpmsg_device *rpdev)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c = dev_get_drvdata(&rpdev->dev);

	rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_INFO, "%s dev %s\n", __func__, rpmsg_i2c->rpdev->id.name);

	// unbind i2c adapter and rpmsg device
	if (rpmsg_i2c->i2c_dev == NULL)
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_INFO, "i2c device already unbinded\n");
	else {
		rpmsg_i2c->i2c_dev->rpmsg_dev = NULL;
		rpmsg_i2c->i2c_dev = NULL;
	}

	list_del(&rpmsg_i2c->list);
	dev_set_drvdata(&rpdev->dev, NULL);
}

#ifdef CONFIG_PM
static int mtk_rpmsg_i2c_suspend(struct device *dev)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c = dev_get_drvdata(dev);

	rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_INFO, "%s dev %s\n", __func__, rpmsg_i2c->rpdev->id.name);

	// unbind i2c adapter and rpmsg device
	if (rpmsg_i2c->i2c_dev == NULL)
		rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_INFO, "i2c device already unbinded\n");
	else {
		rpmsg_i2c->i2c_dev->rpmsg_dev = NULL;
		rpmsg_i2c->i2c_dev = NULL;
	}

	return 0;
}

static int mtk_rpmsg_i2c_resume(struct device *dev)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c = dev_get_drvdata(dev);

	rpmsg_log(rpmsg_i2c, MTK_I2C_LOG_INFO, "%s dev %s\n", __func__, rpmsg_i2c->rpdev->id.name);

	// trigger i2c_adap_add_resume_thread
	_mtk_bind_rpmsg_to_rproc_i2c(rpmsg_i2c);

	return 0;
}

static const struct dev_pm_ops mtk_rpmsg_i2c_pm_ops = {
	.suspend = mtk_rpmsg_i2c_suspend,
	.resume = mtk_rpmsg_i2c_resume,
};
#endif

static struct rpmsg_device_id mtk_rpmsg_i2c_id_table[] = {
	// all aupported I2C device names in dts
	{.name = "I2C_0"},
	{.name = "I2C_1"},
	{.name = "I2C_2"},
	{.name = "I2C_3"},
	{.name = "PM_I2C_0"},
	{.name = "PM_I2C_1"},
	{},
};

MODULE_DEVICE_TABLE(rpmsg, mtk_rpmsg_i2c_id_table);

static struct rpmsg_driver mtk_rpmsg_i2c_driver = {
	.drv.name = KBUILD_MODNAME,
	.drv.owner = THIS_MODULE,
#ifdef CONFIG_PM
	.drv.pm = &mtk_rpmsg_i2c_pm_ops,
#endif
	.id_table = mtk_rpmsg_i2c_id_table,
	.probe = mtk_rpmsg_i2c_probe,
	.callback = mtk_rpmsg_i2c_callback,
	.remove = mtk_rpmsg_i2c_remove,
};

/**
 * mtk_dbg sysfs group for rproc_i2c_bus_dev
 */
static ssize_t rproc_i2c_help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	str += scnprintf(str, end - str, "Debug Help:\n"
			 "- log_level <RW>: To control debug log level.\n"
			 "                  Read log_level for the definition of log level number.\n");
	return (str - buf);
}

static DEVICE_ATTR_RO(rproc_i2c_help);

static ssize_t rproc_i2c_log_level_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct mtk_rproc_i2c_bus_dev *i2c_dev = dev_get_drvdata(dev);
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	str += scnprintf(str, end - str, "none:\t\t%d\nmute:\t\t%d\nerror:\t\t%d\n"
		       "warning:\t\t%d\ninfo:\t\t%d\ndebug:\t\t%d\n"
		       "current:\t%d\n",
		       MTK_I2C_LOG_NONE, MTK_I2C_LOG_MUTE, MTK_I2C_LOG_ERR,
		       MTK_I2C_LOG_WARN, MTK_I2C_LOG_INFO, MTK_I2C_LOG_DEBUG,
		       i2c_dev->log_level);
	return (str - buf);
}

static ssize_t rproc_i2c_log_level_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct mtk_rproc_i2c_bus_dev *i2c_dev = dev_get_drvdata(dev);
	ssize_t retval;
	int level;

	retval = kstrtoint(buf, 10, &level);
	if (retval == 0) {
		if ((level < MTK_I2C_LOG_NONE) || (level > MTK_I2C_LOG_DEBUG))
			retval = -EINVAL;
		else
			i2c_dev->log_level = level;
	}

	return (retval < 0) ? retval : count;
}

static DEVICE_ATTR_RW(rproc_i2c_log_level);

static struct attribute *mtk_rproc_i2c_dbg_attrs[] = {
	&dev_attr_rproc_i2c_help.attr,
	&dev_attr_rproc_i2c_log_level.attr,
	NULL,
};

static const struct attribute_group mtk_rproc_i2c_attr_group = {
	.name = "mtk_dbg",
	.attrs = mtk_rproc_i2c_dbg_attrs,
};

/**
 * Common procedures for communicate linux subsystem and rpmsg
 */
static inline int __mtk_rpmsg_txn_sync(struct mtk_rproc_i2c_bus_dev *i2c_dev, uint8_t cmd)
{
	struct mtk_rpmsg_i2c *rpmsg_i2c = i2c_dev->rpmsg_dev;

	if (rpmsg_i2c->receive_msg.cmd != cmd) {
		i2c_log(i2c_dev, MTK_I2C_LOG_ERR, "Out of sync command, send %d get %d\n", cmd,
			rpmsg_i2c->receive_msg.cmd);
		return -ENOTSYNC;
	}

	return 0;
}

/**
 * I2C subsystem callback functions
 */
static int mtk_rproc_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	/**
	 * rpmsg trancsction packet format:
	 *                +-----+-------------------+--------------+
	 * pproxy_msg ->  | cmd | i2c_msgs_dma_addr | i2c_msgs_num |
	 *                +-----+-------------------+--------------+
	 *
	 * memory allocated by dma_alloc_attr():
	 *                      +---------+----------+--------+-----------------+
	 * i2c_msgs_dma_addr -> | addr[0] | flags[0] | len[0] | buf_dma_addr[0] |
	 *                      +---------+----------+--------+-----------------+
	 *                      | addr[1] | flags[1] | len[1] | buf_dma_addr[1] |
	 *                      +---------+----------+--------+-----------------+
	 *                      | ...                                           |
	 *                      +---------+----------+--------+-----------------+
	 *                      | addr[n] | flags[n] | len[n] | buf_dma_addr[n] |
	 *                      +---------+----------+--------+-----------------+
	 * buf_dma_addr[0] ->   | data region of buf[0]                         |
	 *                      +-----------------------------------------------+
	 * buf_dma_addr[1] ->   | data region of buf[1]                         |
	 *                      +-----------------------------------------------+
	 *                      | ...                                           |
	 *                      +-----------------------------------------------+
	 * buf_dma_addr[n] ->   | data region of buf[n]                         |
	 *                      +-----------------------------------------------+
	 */

	// i2c bus device & rpmsg device
	struct mtk_rproc_i2c_bus_dev *i2c_dev = i2c_get_adapdata(adap);
	struct mtk_rpmsg_i2c *rpmsg_i2c = i2c_dev->rpmsg_dev;
	// message pointer passing through rpmsg
	struct i2c_proxy_cmd_msg msg = {0};
	struct i2c_proxy_msg_xfer *pproxy_msg = (struct i2c_proxy_msg_xfer *)&msg;
	// i2c msg(s) packet
	struct i2c_proxy_host_i2c_msg *phost_i2c_msg = NULL;
	// physical address of i2c_proxy_host_i2c_msg
	dma_addr_t host_i2c_msg_dma_addr;
	// data buffer pointers of each data region
	uint8_t **cma_data_bufs;
	// loop counter and return value
	int i, ret = 0;
	// buffer size for dma_alloc_coherent
	size_t dma_alloc_size;
	// pointer(s) and pa(s) for traverse application i2c_msg to i2c_proxy_host_i2c_msg
	struct i2c_proxy_host_i2c_msg *proxy_i2c_msg_ptr = NULL;
	uint8_t *buf_cpu_addr_st;
	dma_addr_t buf_dma_addr_st;

	// sanity check
	if (!i2c_dev->rpmsg_dev)
		// rpmsg device not binded (should not happen since i2c adapter added after bind)
		return -EBUSY;

	// allocate for data buffer pointers of each data region
	cma_data_bufs = devm_kmalloc_array(i2c_dev->dev, num, sizeof(uint8_t *), GFP_KERNEL);
	if (!cma_data_bufs)
		return -ENOMEM;
	for (i = 0 ; i < num ; i++)
		cma_data_bufs[i] = NULL;

	// allocate struct i2c_proxy_host_i2c_msg for rproc i2c
	dma_alloc_size = sizeof(*phost_i2c_msg) * num;
	/**
	 * for memory efficience, allocate whole packet buffer include transaction(s) data at the
	 * same time, need summarize all transaction(s) data length
	 */
	for (i = 0 ; i < num ; i++)
		dma_alloc_size += msgs[i].len;
	i2c_log(i2c_dev, MTK_I2C_LOG_DEBUG, "alloc %zd for pass msg(s) and data\n", dma_alloc_size);

	// Allocate message buffer passing to RPROC
	phost_i2c_msg = dma_alloc_coherent(i2c_dev->dev, dma_alloc_size, &host_i2c_msg_dma_addr,
					GFP_KERNEL);
	if (IS_ERR_OR_NULL(phost_i2c_msg)) {
		ret = -ENOMEM;
		goto mtk_rproc_i2c_dma_alloc_fail;
	}

	i2c_log(i2c_dev, MTK_I2C_LOG_DEBUG, "alloc va %pK map to pa %llx\n", phost_i2c_msg,
		host_i2c_msg_dma_addr);

	// should pass dram pa to rproc
	host_i2c_msg_dma_addr -= i2c_dev->dma_phy_off;

	// fill i2c_proxy_host_i2c_msg, transfer STI i2c_msg(s) to rpmsg i2c_xfer_msg(s)
	memset(phost_i2c_msg, 0, sizeof(*phost_i2c_msg) * num);
	// all transaction data following i2c_xfer_msg(s), initial pointer for data region
	buf_cpu_addr_st = (uint8_t *)phost_i2c_msg + (sizeof(*phost_i2c_msg) * num);
	buf_dma_addr_st = host_i2c_msg_dma_addr + (sizeof(*phost_i2c_msg) * num);

	// traverse all application i2c msgs
	for (i = 0, proxy_i2c_msg_ptr = phost_i2c_msg; i < num; i++, proxy_i2c_msg_ptr++) {
		// pass pointer of data region
		cma_data_bufs[i] = buf_cpu_addr_st;
		proxy_i2c_msg_ptr->buf_dma_addr = (uint64_t)buf_dma_addr_st;

		if (!(msgs[i].flags & I2C_M_RD))
			// write transaction, copy source data to dma buffer
			memcpy(cma_data_bufs[i], msgs[i].buf, msgs[i].len);

		i2c_log(i2c_dev, MTK_I2C_LOG_DEBUG,
			"msgs[%d].buf dma_safe %pK to pa %llx, length %d\n", i, cma_data_bufs[i],
			buf_dma_addr_st, msgs[i].len);

		// pass other i2c_msg information
		proxy_i2c_msg_ptr->addr = msgs[i].addr;
		proxy_i2c_msg_ptr->flags = msgs[i].flags;
		proxy_i2c_msg_ptr->len = msgs[i].len;

		// pointer for next data region
		buf_cpu_addr_st += msgs[i].len;
		buf_dma_addr_st += msgs[i].len;
	}

	// packing RPMSG command package
	pproxy_msg->cmd = I2C_PROXY_CMD_XFER;
	pproxy_msg->i2c_msgs_dma_addr = (uint64_t)host_i2c_msg_dma_addr;
	pproxy_msg->i2c_msgs_num = num;
#ifdef DEBUG
	pr_info("msgs:\n");
	print_hex_dump(KERN_INFO, " ", DUMP_PREFIX_OFFSET, DUMP_ROW_BYTES, 1, phost_i2c_msg,
		       sizeof(*phost_i2c_msg) * num, true);
#endif

	// execute RPMSG command
	ret = rpmsg_i2c->txn(rpmsg_i2c, (void *)&msg, sizeof(msg));
	if (ret)
		goto mtk_rproc_i2c_xfer_end;

	// check whether received result sync with command
	ret = __mtk_rpmsg_txn_sync(i2c_dev, pproxy_msg->cmd);
	if (ret)
		goto mtk_rproc_i2c_xfer_end;

	// check I2C transaction resule from rpmsg received message
	ret = ((struct i2c_proxy_ack_xfer *)&rpmsg_i2c->receive_msg)->i2c_msg_xfer_num;
	if (ret < 0) {
		i2c_log(i2c_dev, MTK_I2C_LOG_ERR, "rproc i2c xfer error(%d)\n", ret);
		goto mtk_rproc_i2c_xfer_end;
	}

	// traverse all msgs, retrieve rproc i2c read result for I2C read
	for (i = 0, proxy_i2c_msg_ptr = phost_i2c_msg; i < num; i++, proxy_i2c_msg_ptr++) {
		if ((msgs[i].flags & I2C_M_RD) && (msgs[i].len > 0))
			// copy to dma_rdwr_buf_array[i]
			memcpy(msgs[i].buf, cma_data_bufs[i], msgs[i].len);
	}

mtk_rproc_i2c_xfer_end:

	// retrieve for dma_free_attrs
	host_i2c_msg_dma_addr += i2c_dev->dma_phy_off;
	// free message bounce buffer
	dma_free_coherent(i2c_dev->dev, dma_alloc_size, phost_i2c_msg, host_i2c_msg_dma_addr);

mtk_rproc_i2c_dma_alloc_fail:

	devm_kfree(i2c_dev->dev, cma_data_bufs);

	return (ret >= 0) ? num : ret;
}

static u32 mtk_rproc_i2c_func(struct i2c_adapter *adap)
{
	/**
	 * return i2c bus driver FUNCTIONALITY CONSTANTS
	 * I2C_FUNC_I2C        : Plain i2c-level commands (Pure SMBus adapters
	 *                       typically can not do these)
	 * I2C_FUNC_SMBUS_EMUL : Handles all SMBus commands that can be
	 *                       emulated by a real I2C adapter (using
	 *                       the transparent emulation layer)
	 *
	 * text copy from file : Documentation/i2c/functionality
	 */
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm mtk_rproc_i2c_algo = {
	.master_xfer = mtk_rproc_i2c_xfer,
	.functionality = mtk_rproc_i2c_func,
};

/**
 * TODO: Maybe we should use other mechanism (deferred probe mechanism or somthing like that) for
 *       binding rpmsg device driver and i2c bus device driver.
 */
static int i2c_adap_add_resume_thread(void *param)
{
	int ret;
	int dev_bind = 0;
	struct mtk_rproc_i2c_bus_dev *i2c_dev = (struct mtk_rproc_i2c_bus_dev *)param;
	struct mtk_rpmsg_i2c *rpmsg_dev, *n;

	i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "Check i2c adap %s binding rpmsg...\n",
		i2c_dev->rproc_i2c_name);

	mutex_lock(&rpmsg_i2c_bind_lock);

	// check rpmsg device list, link rpmsg device which name is identical with i2c_dev
	list_for_each_entry_safe(rpmsg_dev, n, &rpmsg_dev_list, list) {
		if (!strcmp(rpmsg_dev->rpdev->id.name, i2c_dev->rproc_i2c_name)) {
			rpmsg_dev->i2c_dev = i2c_dev;
			i2c_dev->rpmsg_dev = rpmsg_dev;
			dev_bind = 1;
			break;
		}
	}

	if (!dev_bind) {
		i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "Wait i2c adap %s binding rpmsg device...\n",
			i2c_dev->rproc_i2c_name);
		reinit_completion(&i2c_dev->rpmsg_bind);
		mutex_unlock(&rpmsg_i2c_bind_lock);
		// complete when rpmsg device and i2c chip device binded
		wait_for_completion(&i2c_dev->rpmsg_bind);
		mutex_lock(&rpmsg_i2c_bind_lock);
	}

	mutex_unlock(&rpmsg_i2c_bind_lock);

	i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "i2c adap %s binded rpmsg device\n",
		i2c_dev->rproc_i2c_name);

	if (i2c_dev->bind_from_resume) {
		i2c_mark_adapter_resumed(&i2c_dev->adapter);
		i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "resume i2c bus %d\n", i2c_dev->adapter.nr);
	} else {
		ret = i2c_add_numbered_adapter(&i2c_dev->adapter);
		if (ret) {
			i2c_log(i2c_dev, MTK_I2C_LOG_ERR, "failed to add i2c adapter (%d)\n", ret);
			goto end;
		}
		i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "register i2c bus %d\n", i2c_dev->adapter.nr);
	}

end:
	do_exit(0);
	return 0;
}

static int mtk_rproc_i2c_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mtk_rproc_i2c_bus_dev *i2c_dev;
	static struct task_struct *t;

	dev_info(&pdev->dev, "%s\n", __func__);

	// init mtk_rproc_i2c_bus_dev device struct
	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(struct mtk_rproc_i2c_bus_dev), GFP_KERNEL);
	if (IS_ERR(i2c_dev)) {
		dev_err(&pdev->dev, "unable to allocate memory (%ld)\n", PTR_ERR(i2c_dev));
		return PTR_ERR(i2c_dev);
	}

	platform_set_drvdata(pdev, i2c_dev);

	init_completion(&i2c_dev->rpmsg_bind);

	i2c_dev->rproc_i2c_name = of_node_full_name(pdev->dev.of_node);
	i2c_dev->dev = &pdev->dev;

	// register i2c adapter and bus algorithm
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_HWMON;
	scnprintf(i2c_dev->adapter.name, sizeof(i2c_dev->adapter.name), "mtk rproc I2C adapter");
	i2c_dev->adapter.dev.parent = &pdev->dev;
	i2c_dev->adapter.algo = &mtk_rproc_i2c_algo;
	i2c_dev->adapter.dev.of_node = pdev->dev.of_node;

	// bus number
	ret = device_property_read_u32(&pdev->dev, "bus-number", &i2c_dev->adapter.nr);
	if (ret) {
		dev_warn(&pdev->dev, "DTB no bus number, dynamic assign.\n");
		i2c_dev->adapter.nr = -1;
	}

	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);

	// mtk loglevel
	i2c_dev->log_level = MTK_I2C_LOG_NONE;

	/**
	 * create mtk_dbg sysfs group, if sysfs group creation failed, mtk_dbg function can not work
	 * but I2C bus still works.
	 */
	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_rproc_i2c_attr_group);
	if (ret < 0) {
		dev_warn(&pdev->dev, "mtk_dbg sysfs group creation failed.\n");
		i2c_dev->log_level = MTK_I2C_LOG_NONE;
	}

	// prepare rproc i2c rpmsg bounce buffer
	ret = _mtk_rproc_i2c_prepare_msg_buf(i2c_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to prepare buffer for rpmsg bounce(%d)\n", ret);
		return ret;
	}

	// add to rproc_i2c_bus_dev list
	INIT_LIST_HEAD(&i2c_dev->list);
	mutex_lock(&rpmsg_i2c_bind_lock);
	dev_dbg(&pdev->dev, "Add i2c adap %s into i2c list\n", i2c_dev->rproc_i2c_name);
	list_add_tail(&i2c_dev->list, &rproc_i2c_bus_dev_list);
	mutex_unlock(&rpmsg_i2c_bind_lock);

	// wait rpmsg device, continue rest initial sequence after rpmsg device binded.
	i2c_dev->bind_from_resume = false;
	t = kthread_run(i2c_adap_add_resume_thread, (void *)i2c_dev, "i2c_adap_add");
	if (IS_ERR(t)) {
		dev_err(&pdev->dev, "create thread for add i2c adap failed!\n");
		goto add_adapter_thread_fail;
	}

	return 0;

add_adapter_thread_fail:

	list_del(&i2c_dev->list);
	return ret;
}

static int mtk_rproc_i2c_remove(struct platform_device *pdev)
{
	struct mtk_rproc_i2c_bus_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "%s\n", __func__);

	// close pmu i2c handle
	if (i2c_dev->rpmsg_dev == NULL)
		i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "rpmsg device already removed\n");
	else {
		// unbind i2c adapter and rpmsg device
		i2c_dev->rpmsg_dev->i2c_dev = NULL;
		i2c_dev->rpmsg_dev = NULL;
	}

	of_reserved_mem_device_release(&pdev->dev);

	// remove i2c adapter
	i2c_del_adapter(&i2c_dev->adapter);
	// remove mtk_dbg sysfs group
	sysfs_remove_group(&pdev->dev.kobj, &mtk_rproc_i2c_attr_group);
	// remove from i2c adapter list
	list_del(&i2c_dev->list);

	return 0;
}

#ifdef CONFIG_PM
static int mtk_rproc_i2c_suspend_late(struct device *dev)
{
	struct mtk_rproc_i2c_bus_dev *i2c_dev = dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&i2c_dev->adapter);

	// unbind i2c adapter and rpmsg device
	if (i2c_dev->rpmsg_dev == NULL)
		i2c_log(i2c_dev, MTK_I2C_LOG_INFO, "rpmsg device already removed\n");
	else {
		i2c_dev->rpmsg_dev->i2c_dev = NULL;
		i2c_dev->rpmsg_dev = NULL;
	}

	return 0;
}

static int mtk_rproc_i2c_resume_early(struct device *dev)
{
	struct mtk_rproc_i2c_bus_dev *i2c_dev = dev_get_drvdata(dev);
	static struct task_struct *t;

	// wait rpmsg device, continue rest resume sequence after rpmsg device binded.
	i2c_dev->bind_from_resume = true;
	t = kthread_run(i2c_adap_add_resume_thread, (void *)i2c_dev, "i2c_adap_resume");
	if (IS_ERR(t)) {
		i2c_log(i2c_dev, MTK_I2C_LOG_ERR, "create thread for resume i2c adap failed!\n");
		return PTR_ERR(t);
	}

	return 0;
}

static const struct dev_pm_ops mtk_rproc_i2c_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(mtk_rproc_i2c_suspend_late, mtk_rproc_i2c_resume_early)
};

#define MTK_RPROC_I2C_PMOPS (&mtk_rproc_i2c_pm_ops)
#else
#define MTK_RPROC_I2C_PMOPS NULL
#endif

static const struct of_device_id mt5896_rproc_i2c_of_match[] = {
	{.compatible = "mediatek,mt5896-i2c-rproc"},
	{},
};

MODULE_DEVICE_TABLE(of, mt5896_rproc_i2c_of_match);

static struct platform_driver mt5896_rproc_i2c_driver = {
	.probe = mtk_rproc_i2c_probe,
	.remove = mtk_rproc_i2c_remove,
	.driver = {
		.of_match_table = of_match_ptr(mt5896_rproc_i2c_of_match),
		.name = "i2c-mt5896-rproc",
		.owner = THIS_MODULE,
		.pm = MTK_RPROC_I2C_PMOPS,
	},
};

static int __init mtk_rproc_i2c_init_driver(void)
{
	int ret = 0;

	ret = platform_driver_register(&mt5896_rproc_i2c_driver);
	if (ret) {
		pr_err("i2c bus driver register failed (%d)\n", ret);
		return ret;
	}

	ret = register_rpmsg_driver(&mtk_rpmsg_i2c_driver);
	if (ret) {
		pr_err("rpmsg driver register failed (%d)\n", ret);
		platform_driver_unregister(&mt5896_rproc_i2c_driver);
		return ret;
	}

	return ret;
}
subsys_initcall(mtk_rproc_i2c_init_driver);

static void __exit mtk_rproc_i2c_exit_driver(void)
{
	unregister_rpmsg_driver(&mtk_rpmsg_i2c_driver);
	platform_driver_unregister(&mt5896_rproc_i2c_driver);
}
module_exit(mtk_rproc_i2c_exit_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT5896 Remote Processor I2C Bus Driver");
MODULE_AUTHOR("Benjamin Lin");
