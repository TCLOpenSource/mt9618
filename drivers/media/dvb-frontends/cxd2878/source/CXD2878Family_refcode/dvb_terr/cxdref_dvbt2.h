/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DVBT2_H
#define CXDREF_DVBT2_H

typedef enum {
    CXDREF_DVBT2_PROFILE_BASE,
    CXDREF_DVBT2_PROFILE_LITE,
    CXDREF_DVBT2_PROFILE_ANY
}cxdref_dvbt2_profile_t;

typedef enum {
    CXDREF_DVBT2_V111,
    CXDREF_DVBT2_V121,
    CXDREF_DVBT2_V131
} cxdref_dvbt2_version_t;

typedef enum {
    CXDREF_DVBT2_S1_BASE_SISO = 0x00,
    CXDREF_DVBT2_S1_BASE_MISO = 0x01,
    CXDREF_DVBT2_S1_NON_DVBT2 = 0x02,
    CXDREF_DVBT2_S1_LITE_SISO = 0x03,
    CXDREF_DVBT2_S1_LITE_MISO = 0x04,
    CXDREF_DVBT2_S1_RSVD3 = 0x05,
    CXDREF_DVBT2_S1_RSVD4 = 0x06,
    CXDREF_DVBT2_S1_RSVD5 = 0x07,
    CXDREF_DVBT2_S1_UNKNOWN = 0xFF
} cxdref_dvbt2_s1_t;

typedef enum {
    CXDREF_DVBT2_BASE_S2_M2K_G_ANY = 0x00,
    CXDREF_DVBT2_BASE_S2_M8K_G_DVBT = 0x01,
    CXDREF_DVBT2_BASE_S2_M4K_G_ANY = 0x02,
    CXDREF_DVBT2_BASE_S2_M1K_G_ANY = 0x03,
    CXDREF_DVBT2_BASE_S2_M16K_G_ANY = 0x04,
    CXDREF_DVBT2_BASE_S2_M32K_G_DVBT = 0x05,
    CXDREF_DVBT2_BASE_S2_M8K_G_DVBT2 = 0x06,
    CXDREF_DVBT2_BASE_S2_M32K_G_DVBT2 = 0x07,
    CXDREF_DVBT2_BASE_S2_UNKNOWN = 0xFF
} cxdref_dvbt2_base_s2_t;

typedef enum {
    CXDREF_DVBT2_LITE_S2_M2K_G_ANY = 0x00,
    CXDREF_DVBT2_LITE_S2_M8K_G_DVBT = 0x01,
    CXDREF_DVBT2_LITE_S2_M4K_G_ANY = 0x02,
    CXDREF_DVBT2_LITE_S2_M16K_G_DVBT2 = 0x03,
    CXDREF_DVBT2_LITE_S2_M16K_G_DVBT = 0x04,
    CXDREF_DVBT2_LITE_S2_RSVD1 = 0x05,
    CXDREF_DVBT2_LITE_S2_M8K_G_DVBT2 = 0x06,
    CXDREF_DVBT2_LITE_S2_RSVD2 = 0x07,
    CXDREF_DVBT2_LITE_S2_UNKNOWN = 0xFF
} cxdref_dvbt2_lite_s2_t;

typedef enum {
    CXDREF_DVBT2_G1_32 = 0x00,
    CXDREF_DVBT2_G1_16 = 0x01,
    CXDREF_DVBT2_G1_8 = 0x02,
    CXDREF_DVBT2_G1_4 = 0x03,
    CXDREF_DVBT2_G1_128 = 0x04,
    CXDREF_DVBT2_G19_128 = 0x05,
    CXDREF_DVBT2_G19_256 = 0x06,
    CXDREF_DVBT2_G_RSVD1 = 0x07,
    CXDREF_DVBT2_G_UNKNOWN = 0xFF
} cxdref_dvbt2_guard_t;

typedef enum {
    CXDREF_DVBT2_M2K = 0x00,
    CXDREF_DVBT2_M8K = 0x01,
    CXDREF_DVBT2_M4K = 0x02,
    CXDREF_DVBT2_M1K = 0x03,
    CXDREF_DVBT2_M16K = 0x04,
    CXDREF_DVBT2_M32K = 0x05,
    CXDREF_DVBT2_M_RSVD1 = 0x06,
    CXDREF_DVBT2_M_RSVD2 = 0x07
} cxdref_dvbt2_mode_t;

typedef enum {
    CXDREF_DVBT2_BW_8 = 0x00,
    CXDREF_DVBT2_BW_7 = 0x01,
    CXDREF_DVBT2_BW_6 = 0x02,
    CXDREF_DVBT2_BW_5 = 0x03,
    CXDREF_DVBT2_BW_10 = 0x04,
    CXDREF_DVBT2_BW_1_7 = 0x05,
    CXDREF_DVBT2_BW_RSVD1 = 0x06,
    CXDREF_DVBT2_BW_RSVD2 = 0x07,
    CXDREF_DVBT2_BW_RSVD3 = 0x08,
    CXDREF_DVBT2_BW_RSVD4 = 0x09,
    CXDREF_DVBT2_BW_RSVD5 = 0x0A,
    CXDREF_DVBT2_BW_RSVD6 = 0x0B,
    CXDREF_DVBT2_BW_RSVD7 = 0x0C,
    CXDREF_DVBT2_BW_RSVD8 = 0x0D,
    CXDREF_DVBT2_BW_RSVD9 = 0x0E,
    CXDREF_DVBT2_BW_RSVD10 = 0x0F,
    CXDREF_DVBT2_BW_UNKNOWN = 0xFF
} cxdref_dvbt2_bw_t;

typedef enum {
    CXDREF_DVBT2_L1PRE_TYPE_TS = 0x00,
    CXDREF_DVBT2_L1PRE_TYPE_GS = 0x01,
    CXDREF_DVBT2_L1PRE_TYPE_TS_GS = 0x02,
    CXDREF_DVBT2_L1PRE_TYPE_RESERVED = 0x03,
    CXDREF_DVBT2_L1PRE_TYPE_UNKNOWN = 0xFF
} cxdref_dvbt2_l1pre_type_t;

typedef enum {
    CXDREF_DVBT2_PAPR_0 = 0x00,

    CXDREF_DVBT2_PAPR_1 = 0x01,

    CXDREF_DVBT2_PAPR_2 = 0x02,

    CXDREF_DVBT2_PAPR_3 = 0x03,

    CXDREF_DVBT2_PAPR_RSVD1 = 0x04,
    CXDREF_DVBT2_PAPR_RSVD2 = 0x05,
    CXDREF_DVBT2_PAPR_RSVD3 = 0x06,
    CXDREF_DVBT2_PAPR_RSVD4 = 0x07,
    CXDREF_DVBT2_PAPR_RSVD5 = 0x08,
    CXDREF_DVBT2_PAPR_RSVD6 = 0x09,
    CXDREF_DVBT2_PAPR_RSVD7 = 0x0A,
    CXDREF_DVBT2_PAPR_RSVD8 = 0x0B,
    CXDREF_DVBT2_PAPR_RSVD9 = 0x0C,
    CXDREF_DVBT2_PAPR_RSVD10 = 0x0D,
    CXDREF_DVBT2_PAPR_RSVD11 = 0x0E,
    CXDREF_DVBT2_PAPR_RSVD12 = 0x0F,
    CXDREF_DVBT2_PAPR_UNKNOWN = 0xFF
} cxdref_dvbt2_papr_t;

typedef enum {
    CXDREF_DVBT2_L1POST_BPSK = 0x00,
    CXDREF_DVBT2_L1POST_QPSK = 0x01,
    CXDREF_DVBT2_L1POST_QAM16 = 0x02,
    CXDREF_DVBT2_L1POST_QAM64 = 0x03,
    CXDREF_DVBT2_L1POST_C_RSVD1 = 0x04,
    CXDREF_DVBT2_L1POST_C_RSVD2 = 0x05,
    CXDREF_DVBT2_L1POST_C_RSVD3 = 0x06,
    CXDREF_DVBT2_L1POST_C_RSVD4 = 0x07,
    CXDREF_DVBT2_L1POST_C_RSVD5 = 0x08,
    CXDREF_DVBT2_L1POST_C_RSVD6 = 0x09,
    CXDREF_DVBT2_L1POST_C_RSVD7 = 0x0A,
    CXDREF_DVBT2_L1POST_C_RSVD8 = 0x0B,
    CXDREF_DVBT2_L1POST_C_RSVD9 = 0x0C,
    CXDREF_DVBT2_L1POST_C_RSVD10 = 0x0D,
    CXDREF_DVBT2_L1POST_C_RSVD11 = 0x0E,
    CXDREF_DVBT2_L1POST_C_RSVD12 = 0x0F,
    CXDREF_DVBT2_L1POST_CONSTELL_UNKNOWN = 0xFF
} cxdref_dvbt2_l1post_constell_t;

typedef enum {
    CXDREF_DVBT2_L1POST_R1_2 = 0x00,
    CXDREF_DVBT2_L1POST_R_RSVD1 = 0x01,
    CXDREF_DVBT2_L1POST_R_RSVD2 = 0x02,
    CXDREF_DVBT2_L1POST_R_RSVD3 = 0x03,
    CXDREF_DVBT2_L1POST_R_UNKNOWN = 0xFF
} cxdref_dvbt2_l1post_cr_t;

typedef enum {
    CXDREF_DVBT2_L1POST_FEC_LDPC16K = 0x00,
    CXDREF_DVBT2_L1POST_FEC_RSVD1 = 0x01,
    CXDREF_DVBT2_L1POST_FEC_RSVD2 = 0x02,
    CXDREF_DVBT2_L1POST_FEC_RSVD3 = 0x03,
    CXDREF_DVBT2_L1POST_FEC_UNKNOWN = 0xFF
} cxdref_dvbt2_l1post_fec_type_t;

typedef enum {
    CXDREF_DVBT2_PP1 = 0x00,
    CXDREF_DVBT2_PP2 = 0x01,
    CXDREF_DVBT2_PP3 = 0x02,
    CXDREF_DVBT2_PP4 = 0x03,
    CXDREF_DVBT2_PP5 = 0x04,
    CXDREF_DVBT2_PP6 = 0x05,
    CXDREF_DVBT2_PP7 = 0x06,
    CXDREF_DVBT2_PP8 = 0x07,
    CXDREF_DVBT2_PP_RSVD1 = 0x08,
    CXDREF_DVBT2_PP_RSVD2 = 0x09,
    CXDREF_DVBT2_PP_RSVD3 = 0x0A,
    CXDREF_DVBT2_PP_RSVD4 = 0x0B,
    CXDREF_DVBT2_PP_RSVD5 = 0x0C,
    CXDREF_DVBT2_PP_RSVD6 = 0x0D,
    CXDREF_DVBT2_PP_RSVD7 = 0x0E,
    CXDREF_DVBT2_PP_RSVD8 = 0x0F,
    CXDREF_DVBT2_PP_UNKNOWN = 0xFF
} cxdref_dvbt2_pp_t;

typedef enum {
    CXDREF_DVBT2_R1_2 = 0x00,
    CXDREF_DVBT2_R3_5 = 0x01,
    CXDREF_DVBT2_R2_3 = 0x02,
    CXDREF_DVBT2_R3_4 = 0x03,
    CXDREF_DVBT2_R4_5 = 0x04,
    CXDREF_DVBT2_R5_6 = 0x05,
    CXDREF_DVBT2_R1_3 = 0x06,
    CXDREF_DVBT2_R2_5 = 0x07,
    CXDREF_DVBT2_PLP_CR_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_code_rate_t;

typedef enum {
    CXDREF_DVBT2_QPSK = 0x00,
    CXDREF_DVBT2_QAM16 = 0x01,
    CXDREF_DVBT2_QAM64 = 0x02,
    CXDREF_DVBT2_QAM256 = 0x03,
    CXDREF_DVBT2_CON_RSVD1 = 0x04,
    CXDREF_DVBT2_CON_RSVD2 = 0x05,
    CXDREF_DVBT2_CON_RSVD3 = 0x06,
    CXDREF_DVBT2_CON_RSVD4 = 0x07,
    CXDREF_DVBT2_CONSTELL_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_constell_t;

typedef enum {
    CXDREF_DVBT2_PLP_TYPE_COMMON = 0x00,
    CXDREF_DVBT2_PLP_TYPE_DATA1 = 0x01,
    CXDREF_DVBT2_PLP_TYPE_DATA2 = 0x02,
    CXDREF_DVBT2_PLP_TYPE_RSVD1 = 0x03,
    CXDREF_DVBT2_PLP_TYPE_RSVD2 = 0x04,
    CXDREF_DVBT2_PLP_TYPE_RSVD3 = 0x05,
    CXDREF_DVBT2_PLP_TYPE_RSVD4 = 0x06,
    CXDREF_DVBT2_PLP_TYPE_RSVD5 = 0x07,
    CXDREF_DVBT2_PLP_TYPE_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_type_t;

typedef enum {
    CXDREF_DVBT2_PLP_PAYLOAD_GFPS = 0x00,
    CXDREF_DVBT2_PLP_PAYLOAD_GCS = 0x01,
    CXDREF_DVBT2_PLP_PAYLOAD_GSE = 0x02,
    CXDREF_DVBT2_PLP_PAYLOAD_TS = 0x03,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD1 = 0x04,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD2 = 0x05,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD3 = 0x06,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD4 = 0x07,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD5 = 0x08,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD6 = 0x09,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD7 = 0x0A,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD8 = 0x0B,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD9 = 0x0C,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD10 = 0x0D,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD11 = 0x0E,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD12 = 0x0F,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD13 = 0x10,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD14 = 0x11,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD15 = 0x12,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD16 = 0x13,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD17 = 0x14,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD18 = 0x15,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD19 = 0x16,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD20 = 0x17,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD21 = 0x18,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD22 = 0x19,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD23 = 0x1A,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD24 = 0x1B,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD25 = 0x1C,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD26 = 0x1D,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD27 = 0x1E,
    CXDREF_DVBT2_PLP_PAYLOAD_RSVD28 = 0x1F,
    CXDREF_DVBT2_PLP_PAYLOAD_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_payload_t;

typedef enum {
    CXDREF_DVBT2_FEC_LDPC_16K = 0x00,
    CXDREF_DVBT2_FEC_LDPC_64K = 0x01,
    CXDREF_DVBT2_FEC_RSVD1 = 0x02,
    CXDREF_DVBT2_FEC_RSVD2 = 0x03,
    CXDREF_DVBT2_FEC_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_fec_t;

typedef enum {
    CXDREF_DVBT2_PLP_MODE_NOTSPECIFIED = 0x00,
    CXDREF_DVBT2_PLP_MODE_NM = 0x01,
    CXDREF_DVBT2_PLP_MODE_HEM = 0x02,
    CXDREF_DVBT2_PLP_MODE_RESERVED = 0x03,
    CXDREF_DVBT2_PLP_MODE_UNKNOWN = 0xFF
} cxdref_dvbt2_plp_mode_t;


typedef enum {
    CXDREF_DVBT2_PLP_COMMON,
    CXDREF_DVBT2_PLP_DATA
} cxdref_dvbt2_plp_btype_t;

typedef enum {
    CXDREF_DVBT2_STREAM_GENERIC_PACKETIZED = 0x00,
    CXDREF_DVBT2_STREAM_GENERIC_CONTINUOUS = 0x01,
    CXDREF_DVBT2_STREAM_GENERIC_ENCAPSULATED = 0x02,
    CXDREF_DVBT2_STREAM_TRANSPORT = 0x03,
    CXDREF_DVBT2_STREAM_UNKNOWN = 0xFF
} cxdref_dvbt2_stream_t;

typedef struct cxdref_dvbt2_l1pre_t {
    cxdref_dvbt2_l1pre_type_t type;
    uint8_t bwExt;
    cxdref_dvbt2_s1_t s1;
    uint8_t s2;
    uint8_t mixed;
    cxdref_dvbt2_mode_t fftMode;
    uint8_t l1Rep;
    cxdref_dvbt2_guard_t gi;
    cxdref_dvbt2_papr_t papr;
    cxdref_dvbt2_l1post_constell_t mod;
    cxdref_dvbt2_l1post_cr_t cr;
    cxdref_dvbt2_l1post_fec_type_t fec;
    uint32_t l1PostSize;
    uint32_t l1PostInfoSize;
    cxdref_dvbt2_pp_t pp;
    uint8_t txIdAvailability;
    uint16_t cellId;
    uint16_t networkId;
    uint16_t systemId;
    uint8_t numFrames;
    uint16_t numSymbols;
    uint8_t regen;
    uint8_t postExt;
    uint8_t numRfFreqs;
    uint8_t rfIdx;
    cxdref_dvbt2_version_t t2Version;
    uint8_t l1PostScrambled;
    uint8_t t2BaseLite;
    uint32_t crc32;
} cxdref_dvbt2_l1pre_t;

typedef struct cxdref_dvbt2_plp_t {

    uint8_t id;

    cxdref_dvbt2_plp_type_t type;

    cxdref_dvbt2_plp_payload_t payload;

    uint8_t ff;

    uint8_t firstRfIdx;

    uint8_t firstFrmIdx;

    uint8_t groupId;

    cxdref_dvbt2_plp_constell_t constell;

    cxdref_dvbt2_plp_code_rate_t plpCr;

    uint8_t rot;

    cxdref_dvbt2_plp_fec_t fec;

    uint16_t numBlocksMax;

    uint8_t frmInt;

    uint8_t tilLen;

    uint8_t tilType;

    uint8_t inBandAFlag;

    uint8_t inBandBFlag;

    uint16_t rsvd;

    cxdref_dvbt2_plp_mode_t plpMode;

    uint8_t staticFlag;

    uint8_t staticPaddingFlag;

} cxdref_dvbt2_plp_t;


typedef struct cxdref_dvbt2_l1post_t {

    uint16_t subSlicesPerFrame;

    uint8_t numPLPs;

    uint8_t numAux;

    uint8_t auxConfigRFU;

    uint8_t rfIdx;

    uint32_t freq;

    uint8_t fefType;

    uint32_t fefLength;

    uint8_t fefInterval;

} cxdref_dvbt2_l1post_t;

typedef struct cxdref_dvbt2_ofdm_t {

    uint8_t mixed;

    uint8_t isMiso;

    cxdref_dvbt2_mode_t mode;

    cxdref_dvbt2_guard_t gi;

    cxdref_dvbt2_pp_t pp;

    uint8_t bwExt;

    cxdref_dvbt2_papr_t papr;

    uint16_t numSymbols;

} cxdref_dvbt2_ofdm_t;

typedef struct cxdref_dvbt2_bbheader_t {
    cxdref_dvbt2_stream_t streamInput;

    uint8_t isSingleInputStream;

    uint8_t isConstantCodingModulation;

    uint8_t issyIndicator;

    uint8_t nullPacketDeletion;

    uint8_t ext;

    uint8_t inputStreamIdentifier;

    uint16_t userPacketLength;

    uint16_t dataFieldLength;

    uint8_t syncByte;

    uint32_t issy;

    cxdref_dvbt2_plp_mode_t plpMode;
} cxdref_dvbt2_bbheader_t;

#endif
