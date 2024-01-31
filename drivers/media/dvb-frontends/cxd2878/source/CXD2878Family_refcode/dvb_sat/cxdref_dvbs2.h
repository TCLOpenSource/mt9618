/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DVBS2_H
#define CXDREF_DVBS2_H

#include "cxdref_common.h"

typedef enum {
    CXDREF_DVBS2_CODERATE_1_4,
    CXDREF_DVBS2_CODERATE_1_3,
    CXDREF_DVBS2_CODERATE_2_5,
    CXDREF_DVBS2_CODERATE_1_2,
    CXDREF_DVBS2_CODERATE_3_5,
    CXDREF_DVBS2_CODERATE_2_3,
    CXDREF_DVBS2_CODERATE_3_4,
    CXDREF_DVBS2_CODERATE_4_5,
    CXDREF_DVBS2_CODERATE_5_6,
    CXDREF_DVBS2_CODERATE_8_9,
    CXDREF_DVBS2_CODERATE_9_10,
    CXDREF_DVBS2_CODERATE_RESERVED_29,
    CXDREF_DVBS2_CODERATE_RESERVED_30,
    CXDREF_DVBS2_CODERATE_RESERVED_31,
    CXDREF_DVBS2_CODERATE_INVALID
} cxdref_dvbs2_coderate_t;

typedef enum {
    CXDREF_DVBS2_MODULATION_QPSK,
    CXDREF_DVBS2_MODULATION_8PSK,
    CXDREF_DVBS2_MODULATION_16APSK,
    CXDREF_DVBS2_MODULATION_32APSK,
    CXDREF_DVBS2_MODULATION_RESERVED_29,
    CXDREF_DVBS2_MODULATION_RESERVED_30,
    CXDREF_DVBS2_MODULATION_RESERVED_31,
    CXDREF_DVBS2_MODULATION_DUMMY_PLFRAME,
    CXDREF_DVBS2_MODULATION_INVALID
} cxdref_dvbs2_modulation_t;

typedef enum {
    CXDREF_DVBS2_STREAM_GENERIC_PACKETIZED = 0x00,
    CXDREF_DVBS2_STREAM_GENERIC_CONTINUOUS = 0x01,
    CXDREF_DVBS2_STREAM_RESERVED = 0x02,
    CXDREF_DVBS2_STREAM_TRANSPORT = 0x03
} cxdref_dvbs2_stream_t;

typedef enum {
    CXDREF_DVBS2_ROLLOFF_35 = 0x00,
    CXDREF_DVBS2_ROLLOFF_25 = 0x01,
    CXDREF_DVBS2_ROLLOFF_20 = 0x02,
    CXDREF_DVBS2_ROLLOFF_RESERVED = 0x03
} cxdref_dvbs2_rolloff_t;

typedef struct {
    cxdref_dvbs2_modulation_t modulation;
    cxdref_dvbs2_coderate_t codeRate;
    uint8_t isShortFrame;
    uint8_t isPilotOn;
} cxdref_dvbs2_plscode_t;

typedef struct {
    cxdref_dvbs2_stream_t streamInput;
    uint8_t isSingleInputStream;
    uint8_t isConstantCodingModulation;
    uint8_t issyIndicator;
    uint8_t nullPacketDeletion;
    cxdref_dvbs2_rolloff_t rollOff;
    uint8_t inputStreamIdentifier;
    uint16_t userPacketLength;
    uint16_t dataFieldLength;
    uint8_t syncByte;
} cxdref_dvbs2_bbheader_t;

#endif
