/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ISDBS3_H
#define CXDREF_ISDBS3_H

#include "cxdref_common.h"

typedef enum cxdref_isdbs3_streamid_type_t {
    CXDREF_ISDBS3_STREAMID_TYPE_STREAMID,
    CXDREF_ISDBS3_STREAMID_TYPE_RELATIVE_STREAM_NUMBER,
} cxdref_isdbs3_streamid_type_t;

typedef enum cxdref_isdbs3_mod_t {
    CXDREF_ISDBS3_MOD_RESERVED_0,
    CXDREF_ISDBS3_MOD_BPSK,
    CXDREF_ISDBS3_MOD_QPSK,
    CXDREF_ISDBS3_MOD_8PSK,
    CXDREF_ISDBS3_MOD_16APSK,
    CXDREF_ISDBS3_MOD_32APSK,
    CXDREF_ISDBS3_MOD_RESERVED_6,
    CXDREF_ISDBS3_MOD_RESERVED_7,
    CXDREF_ISDBS3_MOD_RESERVED_8,
    CXDREF_ISDBS3_MOD_RESERVED_9,
    CXDREF_ISDBS3_MOD_RESERVED_10,
    CXDREF_ISDBS3_MOD_RESERVED_11,
    CXDREF_ISDBS3_MOD_RESERVED_12,
    CXDREF_ISDBS3_MOD_RESERVED_13,
    CXDREF_ISDBS3_MOD_RESERVED_14,
    CXDREF_ISDBS3_MOD_UNUSED_15
} cxdref_isdbs3_mod_t;

typedef enum cxdref_isdbs3_cod_t {
    CXDREF_ISDBS3_COD_RESERVED_0,
    CXDREF_ISDBS3_COD_41_120,
    CXDREF_ISDBS3_COD_49_120,
    CXDREF_ISDBS3_COD_61_120,
    CXDREF_ISDBS3_COD_73_120,
    CXDREF_ISDBS3_COD_81_120,
    CXDREF_ISDBS3_COD_89_120,
    CXDREF_ISDBS3_COD_93_120,
    CXDREF_ISDBS3_COD_97_120,
    CXDREF_ISDBS3_COD_101_120,
    CXDREF_ISDBS3_COD_105_120,
    CXDREF_ISDBS3_COD_109_120,
    CXDREF_ISDBS3_COD_RESERVED_12,
    CXDREF_ISDBS3_COD_RESERVED_13,
    CXDREF_ISDBS3_COD_RESERVED_14,
    CXDREF_ISDBS3_COD_UNUSED_15
} cxdref_isdbs3_cod_t;

typedef enum cxdref_isdbs3_stream_type_t {
    CXDREF_ISDBS3_STREAM_TYPE_RESERVED_0,
    CXDREF_ISDBS3_STREAM_TYPE_MPEG2_TS,
    CXDREF_ISDBS3_STREAM_TYPE_TLV,
    CXDREF_ISDBS3_STREAM_TYPE_NO_TYPE_ALLOCATED = 0xFF,
} cxdref_isdbs3_stream_type_t;

typedef struct cxdref_isdbs3_tmcc_modcod_slot_info_t {
    cxdref_isdbs3_mod_t            mod;
    cxdref_isdbs3_cod_t            cod;
    uint8_t                      slotNum;
    uint8_t                      backoff;
} cxdref_isdbs3_tmcc_modcod_slot_info_t;

typedef struct cxdref_isdbs3_tmcc_info_t {
    uint8_t                             changeOrder;
    cxdref_isdbs3_tmcc_modcod_slot_info_t modcodSlotInfo[8];
    cxdref_isdbs3_stream_type_t           streamTypeForEachRelativeStream[16];
    uint16_t                            packetLengthForEachRelativeStream[16];
    uint8_t                             syncPatternBitLengthForEachRelativeStream[16];
    uint32_t                            syncPatternForEachRelativeStream[16];
    uint8_t                             relativeStreamForEachSlot[120];
    uint16_t                            streamidForEachRelativeStream[16];
    uint8_t                             ewsFlag;
    uint8_t                             uplinkInfo;
} cxdref_isdbs3_tmcc_info_t;

#endif
