/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXD_DMD_PRIV_H
#define CXD_DMD_PRIV_H

#include <media/dvb_frontend.h>
#include "cxd_dmd_common.h"
#include "cxd_dmd.h"

struct cxd_dmd_state {
	struct cxd_dmd_driver_instance driver;
	struct cxd_dmd_config config;
	struct dvb_frontend fe;
	struct i2c_adapter *i2c;
};
#endif
