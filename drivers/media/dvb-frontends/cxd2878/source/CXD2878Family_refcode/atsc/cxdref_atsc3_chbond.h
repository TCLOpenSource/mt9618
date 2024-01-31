/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ATSC3_CHBOND_H
#define CXDREF_ATSC3_CHBOND_H

#include "cxdref_common.h"

typedef struct cxdref_atsc3_chbond_l1detail_t {
    uint8_t                     num_rf;
    uint16_t                    bonded_bsid;
    uint16_t                    bsid;
} cxdref_atsc3_chbond_l1detail_t;

#endif
