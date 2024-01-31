// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/of.h>
#include "austin-demod.h"

struct austin_demod_dev *p_austin_dev;

u8 Austin_WriteRegister(u16 u16BusNumSlaveID, u8 u8addrcount, u8 *pu8addr, u16 u16size, u8 *pu8data)
{
	int ret;

	ret = austin_demod_i2c_write(p_austin_dev->client, 0, NULL, u16size, pu8data);

	return ret;
}


int austin_demod_i2c_write(struct i2c_client *client,
u16 u16AddrCnt, u8 *pu8Addr, u16 u16Len, u8 *buf)
{
		struct i2c_msg msg;
		int ret;

		msg.flags = 0;
		msg.addr = client->addr;
		msg.buf = buf;
		msg.len = u16Len;

		ret = i2c_transfer(client->adapter, &msg, 1);

		return ret;
}

u8 Austin_ReadRegister(u16 u16BusNumSlaveID, u8 u8addrcount,
u8 *pu8addr, u16 u16size, u8 *pu8data)
{
	int ret;

	ret = austin_demod_i2c_read(p_austin_dev->client, 0, NULL, u16size, pu8data);

	return ret;
}

int austin_demod_i2c_read(struct i2c_client *client,
u16 u16AddrCnt, u8 *pu8Addr, u16 u16Len, u8 *buf)
{
		struct i2c_msg msg;
		int ret;

		msg.flags = I2C_M_RD;
		msg.addr  = client->addr;
		msg.len   = u16Len;
		msg.buf   = buf;

		ret = i2c_transfer(client->adapter, &msg, 1);

		return ret;
}


u8 austin_chk_exist(void)
{
	char buf[4] = {0, 9, 3, 2};
	int ret = 0;

	ret = i2c_transfer_buffer_flags(p_austin_dev->client, buf, 4, I2C_M_RD);
	pr_emerg("[%s][%d] ret %d, --- %d %d %d %d\n",
	__func__, __LINE__, ret, buf[0], buf[1], buf[2], buf[3]);

	if ((ret == 4)
	&& (buf[0] == 0xFF)
	&& (buf[1] == 0xFF)
	&& (buf[2] == 0xFF)
	&& (buf[3] == 0xFF)) {
		// ret is transfer bytes, and others value is 0xff
		p_austin_dev->austin_exist = 1;
	} else {
		// ret is negative value, and others value is 0932
		p_austin_dev->austin_exist = 0;
	}

	return 0;
}

static const struct cfm_demod_ops mtk_demod_ops = {
	.chip_delsys.size = 1,
	.chip_delsys.element = {SYS_ATSC30},
	.info = {
		.name = "mtk_austin_demod",
	},
	.device_unique_id = 1,  // for overlay_tuner.dtsi
};


static int demod_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	unsigned int temp = 0;
	int i = 0;
	const unsigned int *prop;
	int leng = 0;


	pr_emerg("[austin_demod_dd][%s] name = [%s]\n", __func__, client->name);

	p_austin_dev = kmalloc(sizeof(struct austin_demod_dev), GFP_KERNEL);
	if (!p_austin_dev)
		return -ENOMEM;

	p_austin_dev->client = client;
	i2c_set_clientdata(client, p_austin_dev);
	mutex_init(&p_austin_dev->i2c_mutex);

	pr_emerg("[%s][%d] austin_dev->client->addr = 0x%x\n",
	__func__, __LINE__, p_austin_dev->client->addr);

		p_austin_dev->fpRead = Austin_ReadRegister;
		p_austin_dev->fpWrite = Austin_WriteRegister;
	p_austin_dev->fpChkExist = austin_chk_exist;

	// Parsing DTS
	// unique_id
	prop = of_get_property(client->dev.of_node, "unique_id", &leng);
	if (prop) {
		temp = be32_to_cpup(((__be32 *)prop));
		//pr_emerg("[%s] 'unique_id' = 0x%x, leng = 0x%x\n", __func__,temp,leng);
	} else
		pr_emerg("[%s] Unable to get tuner 'unique_id'\n", __func__);

	//i2c_slave_id
	prop = of_get_property(client->dev.of_node, "i2c_slave_id", &leng);
	if (prop) {
		temp = be32_to_cpup(((__be32 *)prop));
		//pr_emerg("[%s] 'i2c_slave_id' = 0x%x, leng = 0x%x\n", __func__,temp,leng);
	} else
		pr_emerg("[%s] Unable to get tuner 'i2c_slave_id'\n", __func__);

	// assign value
	memcpy(&p_austin_dev->dmd_ops, &mtk_demod_ops, sizeof(struct cfm_demod_ops));

	p_austin_dev->dmd_ops.demodulator_priv = p_austin_dev;
	// cfm_register_demod_device(&dev->dmd_ops); // no need to register to cfm.

	austin_chk_exist();

	return 0;
}

static int demod_remove(struct i2c_client *client)
{
	pr_emerg("[austin_demod_dd][%s][%d] __entry__\n", __func__, __LINE__);

	return 0;
}

static const struct i2c_device_id demod_id_table[] = {
	{"mediatek,austin_demod", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, demod_id_table);

static const struct of_device_id demod_dt_match[] = {
	{ .compatible = AUSTIN_DRV_NAME },
	{},
};

MODULE_DEVICE_TABLE(of, demod_dt_match);

static struct i2c_driver demod_driver = {
	.driver  = {
		.owner  = THIS_MODULE,
		.name   = AUSTIN_DRV_NAME,
		.of_match_table = of_match_ptr(demod_dt_match),
	},
	.probe	  = demod_probe,
	.remove	 = demod_remove,
	.id_table   = demod_id_table,
};

EXPORT_SYMBOL(p_austin_dev);
module_i2c_driver(demod_driver);


MODULE_DESCRIPTION("AUSTIN Demod Driver");
MODULE_AUTHOR("Mediatek Author");
MODULE_LICENSE("GPL");
