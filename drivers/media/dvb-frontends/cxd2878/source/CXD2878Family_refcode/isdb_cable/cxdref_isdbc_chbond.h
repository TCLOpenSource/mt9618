/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_ISDBC_CHBOND_H
#define CXDREF_ISDBC_CHBOND_H

typedef enum cxdref_isdbc_chbond_stream_type_t {
    CXDREF_ISDBC_CHBOND_STREAM_TYPE_TLV,
    CXDREF_ISDBC_CHBOND_STREAM_TYPE_TS_NONE
} cxdref_isdbc_chbond_stream_type_t;

typedef struct cxdref_isdbc_chbond_tsmf_header_ext_t {
    uint8_t aceewData[26];
    cxdref_isdbc_chbond_stream_type_t streamType[15];
    uint8_t groupID;
    uint8_t numCarriers;
    uint8_t carrierSequence;
    uint8_t numFrames;
    uint8_t framePosition;
} cxdref_isdbc_chbond_tsmf_header_ext_t;

#endif
