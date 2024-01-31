/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXD_DMD_H
#define CXD_DMD_H

struct cxd_dmd_config {
	u32 xtal_freq_khz;
	u8 i2c_address_slvx;
};

#endif
