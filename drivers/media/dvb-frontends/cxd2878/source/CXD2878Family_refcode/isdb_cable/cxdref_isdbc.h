/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ISDBC_H
#define CXDREF_ISDBC_H

typedef enum {
    CXDREF_ISDBC_TSID_TYPE_TSID,
    CXDREF_ISDBC_TSID_TYPE_RELATIVE_TS_NUMBER
} cxdref_isdbc_tsid_type_t;

typedef enum {
    CXDREF_ISDBC_CONSTELLATION_RESERVED_0,
    CXDREF_ISDBC_CONSTELLATION_RESERVED_1,
    CXDREF_ISDBC_CONSTELLATION_64QAM,
    CXDREF_ISDBC_CONSTELLATION_RESERVED_3,
    CXDREF_ISDBC_CONSTELLATION_256QAM
} cxdref_isdbc_constellation_t;

typedef enum {
    CXDREF_ISDBC_TSMF_RECEIVE_STATUS_GOOD,
    CXDREF_ISDBC_TSMF_RECEIVE_STATUS_NORMAL,
    CXDREF_ISDBC_TSMF_RECEIVE_STATUS_BAD,
    CXDREF_ISDBC_TSMF_RECEIVE_STATUS_UNDEFINED
} cxdref_isdbc_tsmf_receive_status_t;

typedef struct cxdref_isdbc_tsmf_header_t {
    uint8_t syncError;
    uint8_t versionNumber;
    uint8_t tsNumMode;
    uint8_t frameType;
    uint8_t tsStatus[15];
    cxdref_isdbc_tsmf_receive_status_t receiveStatusSelected;
    cxdref_isdbc_tsmf_receive_status_t receiveStatus[15];
    uint16_t tsid[15];
    uint16_t networkId[15];
    uint8_t relativeTSNumForEachSlot[52];
    uint8_t emergency;
    uint8_t privateData[85];
} cxdref_isdbc_tsmf_header_t;

#endif
