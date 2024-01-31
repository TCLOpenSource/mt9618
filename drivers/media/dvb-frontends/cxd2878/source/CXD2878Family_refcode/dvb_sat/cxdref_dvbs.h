/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DVBS_H
#define CXDREF_DVBS_H

typedef enum {
    CXDREF_DVBS_CODERATE_1_2,
    CXDREF_DVBS_CODERATE_2_3,
    CXDREF_DVBS_CODERATE_3_4,
    CXDREF_DVBS_CODERATE_5_6,
    CXDREF_DVBS_CODERATE_7_8,
    CXDREF_DVBS_CODERATE_INVALID
} cxdref_dvbs_coderate_t;

typedef enum {
    CXDREF_DVBS_MODULATION_QPSK,
    CXDREF_DVBS_MODULATION_INVALID
} cxdref_dvbs_modulation_t;

#endif
