/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DVBT_H
#define CXDREF_DVBT_H

typedef enum {
    CXDREF_DVBT_CONSTELLATION_QPSK,
    CXDREF_DVBT_CONSTELLATION_16QAM,
    CXDREF_DVBT_CONSTELLATION_64QAM,
    CXDREF_DVBT_CONSTELLATION_RESERVED_3
} cxdref_dvbt_constellation_t;

typedef enum {
    CXDREF_DVBT_HIERARCHY_NON,
    CXDREF_DVBT_HIERARCHY_1,
    CXDREF_DVBT_HIERARCHY_2,
    CXDREF_DVBT_HIERARCHY_4
} cxdref_dvbt_hierarchy_t;

typedef enum {
    CXDREF_DVBT_CODERATE_1_2,
    CXDREF_DVBT_CODERATE_2_3,
    CXDREF_DVBT_CODERATE_3_4,
    CXDREF_DVBT_CODERATE_5_6,
    CXDREF_DVBT_CODERATE_7_8,
    CXDREF_DVBT_CODERATE_RESERVED_5,
    CXDREF_DVBT_CODERATE_RESERVED_6,
    CXDREF_DVBT_CODERATE_RESERVED_7
} cxdref_dvbt_coderate_t;

typedef enum {
    CXDREF_DVBT_GUARD_1_32,
    CXDREF_DVBT_GUARD_1_16,
    CXDREF_DVBT_GUARD_1_8,
    CXDREF_DVBT_GUARD_1_4
} cxdref_dvbt_guard_t;

typedef enum {
    CXDREF_DVBT_MODE_2K,
    CXDREF_DVBT_MODE_8K,
    CXDREF_DVBT_MODE_RESERVED_2,
    CXDREF_DVBT_MODE_RESERVED_3
} cxdref_dvbt_mode_t;

typedef enum {
    CXDREF_DVBT_PROFILE_HP = 0,
    CXDREF_DVBT_PROFILE_LP
} cxdref_dvbt_profile_t;

typedef struct {
    cxdref_dvbt_constellation_t constellation;
    cxdref_dvbt_hierarchy_t hierarchy;
    cxdref_dvbt_coderate_t rateHP;
    cxdref_dvbt_coderate_t rateLP;
    cxdref_dvbt_guard_t guard;
    cxdref_dvbt_mode_t mode;
    uint8_t fnum;
    uint8_t lengthIndicator;
    uint16_t cellID;
    uint8_t reservedEven;
    uint8_t reservedOdd;
} cxdref_dvbt_tpsinfo_t;

#endif
