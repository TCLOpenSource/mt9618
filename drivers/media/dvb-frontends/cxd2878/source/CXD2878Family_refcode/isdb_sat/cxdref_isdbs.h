/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ISDBS_H
#define CXDREF_ISDBS_H

#include "cxdref_common.h"

typedef enum cxdref_isdbs_tsid_type_t {
    CXDREF_ISDBS_TSID_TYPE_TSID,
    CXDREF_ISDBS_TSID_TYPE_RELATIVE_TS_NUMBER,
} cxdref_isdbs_tsid_type_t;

typedef enum cxdref_isdbs_modcod_t {
    CXDREF_ISDBS_MODCOD_RESERVED_0,
    CXDREF_ISDBS_MODCOD_BPSK_1_2,
    CXDREF_ISDBS_MODCOD_QPSK_1_2,
    CXDREF_ISDBS_MODCOD_QPSK_2_3,
    CXDREF_ISDBS_MODCOD_QPSK_3_4,
    CXDREF_ISDBS_MODCOD_QPSK_5_6,
    CXDREF_ISDBS_MODCOD_QPSK_7_8,
    CXDREF_ISDBS_MODCOD_TC8PSK_2_3,
    CXDREF_ISDBS_MODCOD_RESERVED_8,
    CXDREF_ISDBS_MODCOD_RESERVED_9,
    CXDREF_ISDBS_MODCOD_RESERVED_10,
    CXDREF_ISDBS_MODCOD_RESERVED_11,
    CXDREF_ISDBS_MODCOD_RESERVED_12,
    CXDREF_ISDBS_MODCOD_RESERVED_13,
    CXDREF_ISDBS_MODCOD_RESERVED_14,
    CXDREF_ISDBS_MODCOD_UNUSED_15
} cxdref_isdbs_modcod_t;

typedef struct cxdref_isdbs_tmcc_modcod_slot_info_t {
    cxdref_isdbs_modcod_t          modCod;
    uint8_t                      slotNum;
} cxdref_isdbs_tmcc_modcod_slot_info_t;

typedef struct cxdref_isdbs_tmcc_info_t {
    uint8_t                            changeOrder;
    cxdref_isdbs_tmcc_modcod_slot_info_t modcodSlotInfo[4];
    uint8_t                            relativeTSForEachSlot[48];
    uint16_t                           tsidForEachRelativeTS[8];
    uint8_t                            ewsFlag;
    uint8_t                            uplinkInfo;
} cxdref_isdbs_tmcc_info_t;

#endif
