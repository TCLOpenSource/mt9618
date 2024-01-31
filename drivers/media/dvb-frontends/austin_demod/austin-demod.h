/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * MediaTek	Inc. (C) 2020. All rights	reserved.
 */

#ifndef _AUSTIN_DEMOD_H_
#define _AUSTIN_DEMOD_H_

#include <linux/i2c.h>
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
#include "../cfm/cfm.h"

/*-------------------------------------------------------------------*/
#define AUSTIN_DRV_NAME    "austin_demod"
/*-------------------------------------------------------------------*/

extern int austin_demod_i2c_write(struct i2c_client *client,
u16 u16AddrCnt, u8 *pu8Addr, u16 u16Len, u8 *buf);
extern int austin_demod_i2c_read(struct i2c_client *client,
u16 u16AddrCnt, u8 *pu8Addr, u16 u16Len, u8 *buf);
extern u8 Austin_WriteRegister(u16 u16BusNumSlaveID, u8 u8addrcount,
u8 *pu8addr, u16 u16size, u8 *pu8data);
extern u8 Austin_ReadRegister(u16 u16BusNumSlaveID, u8 u8addrcount,
u8 *pu8addr, u16 u16size, u8 *pu8data);
extern u8 austin_chk_exist(void);

typedef u8 (*demod_i2c_read)(u16 u16BusNumSlaveID,
u8 u8addrcount, u8 *pu8addr, u16 u16size, u8 *pu8data);
typedef u8 (*demod_i2c_write)(u16 u16BusNumSlaveID,
u8 u8addrcount, u8 *pu8addr, u16 u16size, u8 *pu8data);
typedef u8 (*demod_chk_exist)(void);

/* state struct */
struct austin_demod_dev {
	struct cdev cdev;
	struct i2c_client *client;
	struct mutex i2c_mutex;
	u8 austin_exist;

	demod_i2c_read fpRead;
	demod_i2c_write fpWrite;
	demod_chk_exist fpChkExist;
	struct cfm_demod_ops dmd_ops;
};
/*---------------------------------------------------------------------------*/
#endif
