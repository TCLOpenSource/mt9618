/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXD_DMD_I2C_DEVICE_H
#define CXD_DMD_I2C_DEVICE_H

#include "cxdref_i2c.h"
#include "cxdref_integ.h"

typedef struct cxd_dmd_i2c_device {
	struct i2c_adapter *i2c;
} cxd_dmd_i2c_device;

cxdref_result_t cxd_dmd_i2c_device_initialize (struct cxd_dmd_i2c_device *i2c_device,
		struct i2c_adapter *i2c);

cxdref_result_t cxd_dmd_i2c_device_finalize (struct cxd_dmd_i2c_device *i2c_device);

cxdref_result_t cxd_dmd_i2c_device_create_i2c (struct cxdref_i2c_t *p_i2c,
		struct cxd_dmd_i2c_device *i2c_device);

cxdref_result_t cxd_dmd_i2c_device_create_i2c_gw (struct cxdref_i2c_t *p_i2c,
		struct cxd_dmd_i2c_device *i2c_device, u8 gw_address, u8 gw_sub);
#endif
