/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXD_DMD_COMMON_H
#define CXD_DMD_COMMON_H

#include "cxdref_integ.h"
#include "cxdref_integ_sat_device_ctrl.h"
#include "cxd_dmd_i2c_device.h"

struct cxd_dmd_driver_instance {
	cxdref_demod_t                  demod;
	cxdref_integ_t                  integ;
	cxdref_integ_singlecable_data_t single_cable_data;
	cxdref_i2c_t                    i2c;
	struct cxd_dmd_i2c_device     i2c_device;
	cxdref_tuner_t                  tuner;
	cxdref_lnbc_t                   lnbc;
};

int cxd_dmd_common_initialize_i2c_device (struct i2c_client *client);
int cxd_dmd_common_finalize_i2c_device (struct i2c_client *client);
int cxd_dmd_common_get_dmd_chip_id (struct i2c_client *client);
int cxd_dmd_common_initialize_demod (struct dvb_frontend *fe, u8 enable_lnbc);
int cxd_dmd_common_tune (struct dvb_frontend *fe);
int cxd_dmd_common_sleep (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_sync_stat (struct dvb_frontend *fe, enum fe_status *status);
int cxd_dmd_common_monitor_cnr (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_bit_error (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_strength (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_IFAGCOut (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_quality (struct dvb_frontend *fe);
int cxd_dmd_common_monitor_plp_info (struct dvb_frontend *fe, struct dtv_frontend_properties *props);

#endif
