/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ISDBT_H
#define CXDREF_ISDBT_H

#include "cxdref_common.h"

typedef enum cxdref_isdbt_mode_t {
    CXDREF_ISDBT_MODE_1,
    CXDREF_ISDBT_MODE_2,
    CXDREF_ISDBT_MODE_3,
    CXDREF_ISDBT_MODE_UNKNOWN
} cxdref_isdbt_mode_t;

typedef enum cxdref_isdbt_guard_t {
    CXDREF_ISDBT_GUARD_1_32,
    CXDREF_ISDBT_GUARD_1_16,
    CXDREF_ISDBT_GUARD_1_8,
    CXDREF_ISDBT_GUARD_1_4,
    CXDREF_ISDBT_GUARD_UNKNOWN
} cxdref_isdbt_guard_t;

typedef enum cxdref_isdbt_system_t {
    CXDREF_ISDBT_SYSTEM_ISDB_T,
    CXDREF_ISDBT_SYSTEM_ISDB_TSB,
    CXDREF_ISDBT_SYSTEM_RESERVED_2,
    CXDREF_ISDBT_SYSTEM_RESERVED_3
} cxdref_isdbt_system_t;

typedef enum cxdref_isdbt_modulation_t {
    CXDREF_ISDBT_MODULATION_DQPSK,
    CXDREF_ISDBT_MODULATION_QPSK,
    CXDREF_ISDBT_MODULATION_16QAM,
    CXDREF_ISDBT_MODULATION_64QAM,
    CXDREF_ISDBT_MODULATION_RESERVED_4,
    CXDREF_ISDBT_MODULATION_RESERVED_5,
    CXDREF_ISDBT_MODULATION_RESERVED_6,
    CXDREF_ISDBT_MODULATION_UNUSED_7
} cxdref_isdbt_modulation_t;

typedef enum cxdref_isdbt_coding_rate_t {
    CXDREF_ISDBT_CODING_RATE_1_2,
    CXDREF_ISDBT_CODING_RATE_2_3,
    CXDREF_ISDBT_CODING_RATE_3_4,
    CXDREF_ISDBT_CODING_RATE_5_6,
    CXDREF_ISDBT_CODING_RATE_7_8,
    CXDREF_ISDBT_CODING_RATE_RESERVED_5,
    CXDREF_ISDBT_CODING_RATE_RESERVED_6,
    CXDREF_ISDBT_CODING_RATE_UNUSED_7
} cxdref_isdbt_coding_rate_t;

typedef enum cxdref_isdbt_il_length_t {
    CXDREF_ISDBT_IL_LENGTH_0_0_0,
    CXDREF_ISDBT_IL_LENGTH_4_2_1,
    CXDREF_ISDBT_IL_LENGTH_8_4_2,
    CXDREF_ISDBT_IL_LENGTH_16_8_4,
    CXDREF_ISDBT_IL_LENGTH_RESERVED_4,
    CXDREF_ISDBT_IL_LENGTH_RESERVED_5,
    CXDREF_ISDBT_IL_LENGTH_RESERVED_6,
    CXDREF_ISDBT_IL_LENGTH_UNUSED_7
} cxdref_isdbt_il_length_t;

typedef struct cxdref_isdbt_tmcc_layer_info_t {
    cxdref_isdbt_modulation_t  modulation;
    cxdref_isdbt_coding_rate_t codingRate;
    cxdref_isdbt_il_length_t   ilLength;
    uint8_t segmentsNum;
} cxdref_isdbt_tmcc_layer_info_t;

typedef struct cxdref_isdbt_tmcc_group_param_t {
    uint8_t isPartial;
    cxdref_isdbt_tmcc_layer_info_t layerA;
    cxdref_isdbt_tmcc_layer_info_t layerB;
    cxdref_isdbt_tmcc_layer_info_t layerC;
} cxdref_isdbt_tmcc_group_param_t;

typedef struct cxdref_isdbt_tmcc_info_t {
    cxdref_isdbt_system_t                systemId;
    uint8_t                            countDownIndex;
    uint8_t                            ewsFlag;
    cxdref_isdbt_tmcc_group_param_t      currentInfo;
    cxdref_isdbt_tmcc_group_param_t      nextInfo;
} cxdref_isdbt_tmcc_info_t;

typedef struct cxdref_isdbt_aceew_area_t {
    uint8_t data[11];
} cxdref_isdbt_aceew_area_t;

typedef struct cxdref_isdbt_aceew_epicenter_t {
    uint8_t data[11];
} cxdref_isdbt_aceew_epicenter_t;

typedef struct cxdref_isdbt_aceew_info_t {
    uint8_t startEndFlag;
    uint8_t updateFlag;
    uint8_t signalId;

    uint8_t isAreaValid;
    cxdref_isdbt_aceew_area_t areaInfo;

    uint8_t isEpicenter1Valid;
    cxdref_isdbt_aceew_epicenter_t epicenter1Info;

    uint8_t isEpicenter2Valid;
    cxdref_isdbt_aceew_epicenter_t epicenter2Info;

} cxdref_isdbt_aceew_info_t;

#endif
