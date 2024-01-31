/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DVBC_H
#define CXDREF_DVBC_H

typedef enum {
    CXDREF_DVBC_CONSTELLATION_16QAM,
    CXDREF_DVBC_CONSTELLATION_32QAM,
    CXDREF_DVBC_CONSTELLATION_64QAM,
    CXDREF_DVBC_CONSTELLATION_128QAM,
    CXDREF_DVBC_CONSTELLATION_256QAM
} cxdref_dvbc_constellation_t;

#endif
