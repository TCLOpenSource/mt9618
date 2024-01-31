/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_H
#define CXDREF_DEMOD_H

#if defined CXDREF_DRIVER_BUILD_OPTION_CXD2856
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_DVBS_S2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_ISDBS
#define CXDREF_DEMOD_SUPPORT_J83B
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD2857
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_DVBS_S2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_ISDBS
#define CXDREF_DEMOD_SUPPORT_ISDBS3
#define CXDREF_DEMOD_SUPPORT_J83B
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD2878
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_DVBS_S2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_ISDBS
#define CXDREF_DEMOD_SUPPORT_J83B
#define CXDREF_DEMOD_SUPPORT_ATSC
#define CXDREF_DEMOD_SUPPORT_ATSC3
#define CXDREF_DEMOD_SUPPORT_ISDBC_CHBOND
#define CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD2879
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_DVBS_S2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_ISDBS
#define CXDREF_DEMOD_SUPPORT_ISDBS3
#define CXDREF_DEMOD_SUPPORT_J83B
#define CXDREF_DEMOD_SUPPORT_ATSC
#define CXDREF_DEMOD_SUPPORT_ATSC3
#define CXDREF_DEMOD_SUPPORT_ISDBC_CHBOND
#define CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD6800
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_J83B
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD6801
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_J83B
#define CXDREF_DEMOD_SUPPORT_ATSC
#define CXDREF_DEMOD_SUPPORT_ATSC3
#elif defined CXDREF_DRIVER_BUILD_OPTION_CXD6802
#define CXDREF_DEMOD_SUPPORT_DVBT
#define CXDREF_DEMOD_SUPPORT_DVBT2
#define CXDREF_DEMOD_SUPPORT_DVBC
#define CXDREF_DEMOD_SUPPORT_DVBC2
#define CXDREF_DEMOD_SUPPORT_ISDBT
#define CXDREF_DEMOD_SUPPORT_ISDBC
#define CXDREF_DEMOD_SUPPORT_J83B
#define CXDREF_DEMOD_SUPPORT_ATSC
#else
#error CXDREF_DRIVER_BUILD_OPTION value not recognised
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_DVBT
#undef CXDREF_DEMOD_SUPPORT_DVBT
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_DVBT2
#undef CXDREF_DEMOD_SUPPORT_DVBT2
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_DVBC
#undef CXDREF_DEMOD_SUPPORT_DVBC
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_DVBC2
#undef CXDREF_DEMOD_SUPPORT_DVBC2
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_DVBS_S2
#undef CXDREF_DEMOD_SUPPORT_DVBS_S2
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ISDBT
#undef CXDREF_DEMOD_SUPPORT_ISDBT
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ISDBC
#undef CXDREF_DEMOD_SUPPORT_ISDBC
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ISDBS
#undef CXDREF_DEMOD_SUPPORT_ISDBS
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ISDBS3
#undef CXDREF_DEMOD_SUPPORT_ISDBS3
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_J83B
#undef CXDREF_DEMOD_SUPPORT_J83B
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ATSC
#undef CXDREF_DEMOD_SUPPORT_ATSC
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ATSC3
#undef CXDREF_DEMOD_SUPPORT_ATSC3
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ISDBC_CHBOND
#undef CXDREF_DEMOD_SUPPORT_ISDBC_CHBOND
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ATSC3_CHBOND
#undef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
#endif

#if    (defined CXDREF_DEMOD_SUPPORT_DVBT)  || (defined CXDREF_DEMOD_SUPPORT_DVBT2) \
    || (defined CXDREF_DEMOD_SUPPORT_DVBC)  || (defined CXDREF_DEMOD_SUPPORT_DVBC2) \
    || (defined CXDREF_DEMOD_SUPPORT_ISDBT) || (defined CXDREF_DEMOD_SUPPORT_ISDBC) \
    || (defined CXDREF_DEMOD_SUPPORT_J83B)  || (defined CXDREF_DEMOD_SUPPORT_ATSC)  \
    || (defined CXDREF_DEMOD_SUPPORT_ATSC3)
#define CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
#endif

#if (defined CXDREF_DEMOD_SUPPORT_DVBS_S2) || (defined CXDREF_DEMOD_SUPPORT_ISDBS) || (defined CXDREF_DEMOD_SUPPORT_ISDBS3)
#define CXDREF_DEMOD_SUPPORT_SAT
#endif

#if defined CXDREF_DEMOD_SUPPORT_DVBS_S2
#define CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_SAT_DEVICE_CTRL
#undef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
#endif

#if (defined CXDREF_DEMOD_SUPPORT_ISDBS3) || (defined CXDREF_DEMOD_SUPPORT_DVBC2) || (defined CXDREF_DEMOD_SUPPORT_ISDBC)
#define CXDREF_DEMOD_SUPPORT_TLV
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_TLV
#undef CXDREF_DEMOD_SUPPORT_TLV
#endif

#if defined CXDREF_DEMOD_SUPPORT_ATSC3
#define CXDREF_DEMOD_SUPPORT_ALP
#endif

#if defined CXDREF_DEMOD_SUPPORT_REMOVE_ALP
#undef CXDREF_DEMOD_SUPPORT_ALP
#endif

#include "cxdref_common.h"
#include "cxdref_i2c.h"
#include "cxdref_dtv.h"

#ifdef CXDREF_DEMOD_SUPPORT_DVBT
#include "cxdref_dvbt.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBC
#include "cxdref_dvbc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBT2
#include "cxdref_dvbt2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBC2
#include "cxdref_dvbc2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_DVBS_S2
#include "cxdref_dvbs.h"
#include "cxdref_dvbs2.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBT
#include "cxdref_isdbt.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBC
#include "cxdref_isdbc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBS
#include "cxdref_isdbs.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ISDBS3
#include "cxdref_isdbs3.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_J83B
#include "cxdref_j83b.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
#include "cxdref_atsc.h"
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3
#include "cxdref_atsc3.h"
#endif

#define CXDREF_DEMOD_MAKE_IFFREQ_CONFIG(iffreq) ((uint32_t)(((iffreq)/48.0)*16777216.0 + 0.5))

#define CXDREF_DEMOD_ATSC_MAKE_IFFREQ_CONFIG(iffreq) ((uint32_t)(((iffreq)/24.0)*4294967296.0 + 0.5))

#ifndef CXDREF_DEMOD_MAX_CONFIG_MEMORY_COUNT
#define CXDREF_DEMOD_MAX_CONFIG_MEMORY_COUNT 100
#endif

#define SLVT_FreezeReg(pDemod) ((pDemod)->pI2c->WriteOneRegister ((pDemod)->pI2c, (pDemod)->i2cAddressSLVT, 0x01, 0x01))

#define SLVT_UnFreezeReg(pDemod) ((void)((pDemod)->pI2c->WriteOneRegister ((pDemod)->pI2c, (pDemod)->i2cAddressSLVT, 0x01, 0x00)))

#define CXDREF_DEMOD_CHIP_ID_2856_FAMILY(chipId) ((chipId == CXDREF_DEMOD_CHIP_ID_CXD2856) || \
                                                (chipId == CXDREF_DEMOD_CHIP_ID_CXD2857))

#define CXDREF_DEMOD_CHIP_ID_2878_FAMILY(chipId) ((chipId == CXDREF_DEMOD_CHIP_ID_CXD2878) || \
                                                (chipId == CXDREF_DEMOD_CHIP_ID_CXD2879) || \
                                                (chipId == CXDREF_DEMOD_CHIP_ID_CXD6802) || \
                                                (chipId == CXDREF_DEMOD_CHIP_ID_CXD2878A))

#define CXDREF_DEMOD_CHIP_ID_2878A_FAMILY(chipId) (chipId == CXDREF_DEMOD_CHIP_ID_CXD2878A)

typedef enum {
    CXDREF_DEMOD_CHIP_ID_UNKNOWN = 0,
    CXDREF_DEMOD_CHIP_ID_CXD2856  = 0x090,
    CXDREF_DEMOD_CHIP_ID_CXD2857 = 0x091,
    CXDREF_DEMOD_CHIP_ID_CXD2878 = 0x396,
    CXDREF_DEMOD_CHIP_ID_CXD2879 = 0x297,
    CXDREF_DEMOD_CHIP_ID_CXD6802  = 0x197,
    CXDREF_DEMOD_CHIP_ID_CXD2878A = 0x39A
} cxdref_demod_chip_id_t;

typedef enum {
    CXDREF_DEMOD_XTAL_16000KHz = 0,
    CXDREF_DEMOD_XTAL_24000KHz = 1,
    CXDREF_DEMOD_XTAL_32000KHz = 2
} cxdref_demod_xtal_t;

typedef enum {
    CXDREF_DEMOD_STATE_UNKNOWN,
    CXDREF_DEMOD_STATE_SHUTDOWN,
    CXDREF_DEMOD_STATE_SLEEP,
    CXDREF_DEMOD_STATE_ACTIVE,
    CXDREF_DEMOD_STATE_INVALID
} cxdref_demod_state_t;

typedef enum {
    CXDREF_DEMOD_TUNER_I2C_CONFIG_DISABLE,
    CXDREF_DEMOD_TUNER_I2C_CONFIG_REPEATER,
    CXDREF_DEMOD_TUNER_I2C_CONFIG_GATEWAY
} cxdref_demod_tuner_i2c_config_t;

typedef enum {
    CXDREF_DEMOD_TUNER_OPTIMIZE_NONCXDREF,
    CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON
} cxdref_demod_tuner_optimize_t;

typedef enum {
    CXDREF_DEMOD_ISDBT_EWS_STATE_NORMAL,
    CXDREF_DEMOD_ISDBT_EWS_STATE_EWS
} cxdref_demod_isdbt_ews_state_t;

typedef enum {
    CXDREF_DEMOD_ATSC3_EAS_STATE_NORMAL,
    CXDREF_DEMOD_ATSC3_EAS_STATE_EAS
} cxdref_demod_atsc3_eas_state_t;

typedef enum {
    CXDREF_DEMOD_TERR_CABLE_SPECTRUM_NORMAL = 0,
    CXDREF_DEMOD_TERR_CABLE_SPECTRUM_INV
} cxdref_demod_terr_cable_spectrum_sense_t;

typedef enum {
    CXDREF_DEMOD_SAT_IQ_SENSE_NORMAL = 0,
    CXDREF_DEMOD_SAT_IQ_SENSE_INV
} cxdref_demod_sat_iq_sense_t;

typedef enum {
    CXDREF_DEMOD_CONFIG_PARALLEL_SEL,

    CXDREF_DEMOD_CONFIG_SER_DATA_ON_MSB,

    CXDREF_DEMOD_CONFIG_OUTPUT_SEL_MSB,

    CXDREF_DEMOD_CONFIG_TSVALID_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_TSSYNC_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_TSERR_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_LATCH_ON_POSEDGE,

    CXDREF_DEMOD_CONFIG_TSCLK_CONT,

    CXDREF_DEMOD_CONFIG_TSCLK_MASK,

    CXDREF_DEMOD_CONFIG_TSVALID_MASK,

    CXDREF_DEMOD_CONFIG_TSERR_MASK,

    CXDREF_DEMOD_CONFIG_PARALLEL_TSCLK_MANUAL,

    CXDREF_DEMOD_CONFIG_TS_PACKET_GAP,

    CXDREF_DEMOD_CONFIG_TS_BACKWARDS_COMPATIBLE,

    CXDREF_DEMOD_CONFIG_PWM_VALUE,

    CXDREF_DEMOD_CONFIG_TSCLK_CURRENT,

    CXDREF_DEMOD_CONFIG_TS_CURRENT,

    CXDREF_DEMOD_CONFIG_GPIO0_CURRENT,

    CXDREF_DEMOD_CONFIG_GPIO1_CURRENT,

    CXDREF_DEMOD_CONFIG_GPIO2_CURRENT,

    CXDREF_DEMOD_CONFIG_CHBOND,

    CXDREF_DEMOD_CONFIG_CHBOND_STREAMIN,

    CXDREF_DEMOD_CONFIG_TERR_CABLE_TS_SERIAL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_TERR_CABLE_TS_2BIT_PARALLEL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_TUNER_OPTIMIZE,

    CXDREF_DEMOD_CONFIG_IFAGCNEG,

    CXDREF_DEMOD_CONFIG_IFAGC_ADC_FS,

    CXDREF_DEMOD_CONFIG_SPECTRUM_INV,

    CXDREF_DEMOD_CONFIG_RF_SPECTRUM_INV,

    CXDREF_DEMOD_CONFIG_TERR_BLINDTUNE_DVBT2_FIRST,

    CXDREF_DEMOD_CONFIG_DVBT_BERN_PERIOD,

    CXDREF_DEMOD_CONFIG_DVBC_BERN_PERIOD,

    CXDREF_DEMOD_CONFIG_DVBT_VBER_PERIOD,

    CXDREF_DEMOD_CONFIG_DVBT2C2_BBER_MES,

    CXDREF_DEMOD_CONFIG_DVBT2C2_LBER_MES,

    CXDREF_DEMOD_CONFIG_DVBT_PER_MES,

    CXDREF_DEMOD_CONFIG_DVBC_PER_MES,

    CXDREF_DEMOD_CONFIG_DVBT2C2_PER_MES,

    CXDREF_DEMOD_CONFIG_ISDBT_BERPER_PERIOD,

    CXDREF_DEMOD_CONFIG_ATSC_RSERR_BKLEN,

    CXDREF_DEMOD_CONFIG_ATSC3_BBER_MES,

    CXDREF_DEMOD_CONFIG_ATSC3_LBER_MES,

    CXDREF_DEMOD_CONFIG_DVBT_AVEBER_PERIOD_TYPE,

    CXDREF_DEMOD_CONFIG_DVBT_AVEBER_PERIOD_TIME,

    CXDREF_DEMOD_CONFIG_DVBT2_AVEBER_PERIOD_TYPE,

    CXDREF_DEMOD_CONFIG_DVBT2_AVEBER_PERIOD_TIME,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_0,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_0,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_1,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_1,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_2,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_2,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TYPE_3,

    CXDREF_DEMOD_CONFIG_ATSC3_AVEBER_PERIOD_TIME_3,

    CXDREF_DEMOD_CONFIG_AVESNR_PERIOD_TIME,

    CXDREF_DEMOD_CONFIG_GPIO_EWS_FLAG,

    CXDREF_DEMOD_CONFIG_ISDBC_TSMF_HEADER_NULL,

    CXDREF_DEMOD_CONFIG_ISDBC_NULL_REPLACED_TS_TSVALID_LOW,

    CXDREF_DEMOD_CONFIG_OUTPUT_DVBC2,

    CXDREF_DEMOD_CONFIG_OUTPUT_ATSC3,

    CXDREF_DEMOD_CONFIG_OUTPUT_ISDBC_CHBOND,

    CXDREF_DEMOD_CONFIG_ATSC3_KOREAN_MODE,

    CXDREF_DEMOD_CONFIG_ATSC3_PLPID_PACKET_SPLP,

    CXDREF_DEMOD_CONFIG_ATSC3_AUTO_SPECTRUM_INV,

    CXDREF_DEMOD_CONFIG_ATSC3_CW_DETECTION,

    CXDREF_DEMOD_CONFIG_ATSC3_AUTO_PLP_ID_SPLP,

    CXDREF_DEMOD_CONFIG_ATSC3_GPIO_EAS_PN_STATE,

    CXDREF_DEMOD_CONFIG_ATSC3_GPIO_EAS_PN_TRANS,

    CXDREF_DEMOD_CONFIG_ATSC_UNLOCK_DETECTION,

    CXDREF_DEMOD_CONFIG_ATSC_AUTO_SIGNAL_CHECK_OFF,

    CXDREF_DEMOD_CONFIG_ATSC_NO_SIGNAL_THRESH,

    CXDREF_DEMOD_CONFIG_ATSC_SIGNAL_THRESH,

    CXDREF_DEMOD_CONFIG_SAT_TS_SERIAL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_SAT_TS_2BIT_PARALLEL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_SAT_TUNER_IQ_SENSE_INV,

    CXDREF_DEMOD_CONFIG_SAT_SINGLECABLE_SPECTRUM_INV,

    CXDREF_DEMOD_CONFIG_SAT_IFAGCNEG,

    CXDREF_DEMOD_CONFIG_DVBSS2_BER_PER_MES,

    CXDREF_DEMOD_CONFIG_DVBS2_BER_FER_MES,

    CXDREF_DEMOD_CONFIG_DVBS_VBER_MES,

    CXDREF_DEMOD_CONFIG_DVBS2_LBER_MES,

    CXDREF_DEMOD_CONFIG_DVBSS2_BLINDSCAN_POWER_SMOOTH,

    CXDREF_DEMOD_CONFIG_DVBSS2_BLINDSCAN_VERSION,

    CXDREF_DEMOD_CONFIG_ISDBS_BERNUMCONF,

    CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIOD1,

    CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIOD2,

    CXDREF_DEMOD_CONFIG_ISDBS_BER_PERIODT,

    CXDREF_DEMOD_CONFIG_ISDBS3_LBER_MES1,

    CXDREF_DEMOD_CONFIG_ISDBS3_LBER_MES2,

    CXDREF_DEMOD_CONFIG_ISDBS3_BER_FER_MES1,

    CXDREF_DEMOD_CONFIG_ISDBS3_BER_FER_MES2,

    CXDREF_DEMOD_CONFIG_OUTPUT_ISDBS3,

    CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_HOLD,

    CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_THRESH_H,

    CXDREF_DEMOD_CONFIG_ISDBS_LOWCN_THRESH_L,

    CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_HOLD,

    CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_THRESH_H,

    CXDREF_DEMOD_CONFIG_ISDBS3_LOWCN_THRESH_L,

    CXDREF_DEMOD_CONFIG_TLV_PARALLEL_SEL,

    CXDREF_DEMOD_CONFIG_TLV_SER_DATA_ON_MSB,

    CXDREF_DEMOD_CONFIG_TLV_OUTPUT_SEL_MSB,

    CXDREF_DEMOD_CONFIG_TLVVALID_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_TLVSYNC_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_TLVERR_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_TLV_LATCH_ON_POSEDGE,

    CXDREF_DEMOD_CONFIG_TLVCLK_CONT,

    CXDREF_DEMOD_CONFIG_TLVCLK_MASK,

    CXDREF_DEMOD_CONFIG_TLVVALID_MASK,

    CXDREF_DEMOD_CONFIG_TLVERR_MASK,

    CXDREF_DEMOD_CONFIG_TLVVALID_MASK_IN_ERRNULL,

    CXDREF_DEMOD_CONFIG_TLV_NULL_FILTER,

    CXDREF_DEMOD_CONFIG_TERR_CABLE_TLV_SERIAL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_SAT_TLV_SERIAL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_TERR_CABLE_TLV_2BIT_PARALLEL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_SAT_TLV_2BIT_PARALLEL_CLK_FREQ,

    CXDREF_DEMOD_CONFIG_ALP_PARALLEL_SEL,

    CXDREF_DEMOD_CONFIG_ALP_SER_DATA_ON_MSB,

    CXDREF_DEMOD_CONFIG_ALP_OUTPUT_SEL_MSB,

    CXDREF_DEMOD_CONFIG_ALP_SYNC_LENGTH,

    CXDREF_DEMOD_CONFIG_ALPVALID_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_ALPSYNC_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_ALPERR_ACTIVE_HI,

    CXDREF_DEMOD_CONFIG_ALP_LATCH_ON_POSEDGE,

    CXDREF_DEMOD_CONFIG_ALPCLK_CONT,

    CXDREF_DEMOD_CONFIG_ALPCLK_MASK,

    CXDREF_DEMOD_CONFIG_ALPVALID_MASK,

    CXDREF_DEMOD_CONFIG_ALPERR_MASK,

    CXDREF_DEMOD_CONFIG_ALP_VALIDCLK_IN_GAP,

    CXDREF_DEMOD_CONFIG_BBP_LATCH_ON_POSEDGE

} cxdref_demod_config_id_t;

typedef enum {
    CXDREF_DEMOD_LOCK_RESULT_NOTDETECT,
    CXDREF_DEMOD_LOCK_RESULT_LOCKED,
    CXDREF_DEMOD_LOCK_RESULT_UNLOCKED
} cxdref_demod_lock_result_t;

typedef enum {
    CXDREF_DEMOD_GPIO_PIN_GPIO0,
    CXDREF_DEMOD_GPIO_PIN_GPIO1,
    CXDREF_DEMOD_GPIO_PIN_GPIO2,
    CXDREF_DEMOD_GPIO_PIN_TSDATA0,
    CXDREF_DEMOD_GPIO_PIN_TSDATA1,
    CXDREF_DEMOD_GPIO_PIN_TSDATA2,
    CXDREF_DEMOD_GPIO_PIN_TSDATA3,
    CXDREF_DEMOD_GPIO_PIN_TSDATA4,
    CXDREF_DEMOD_GPIO_PIN_TSDATA5,
    CXDREF_DEMOD_GPIO_PIN_TSDATA6,
    CXDREF_DEMOD_GPIO_PIN_TSDATA7
} cxdref_demod_gpio_pin_t;

typedef enum {
    CXDREF_DEMOD_GPIO_MODE_OUTPUT = 0x00,

    CXDREF_DEMOD_GPIO_MODE_INPUT = 0x01,

    CXDREF_DEMOD_GPIO_MODE_PWM = 0x03,

    CXDREF_DEMOD_GPIO_MODE_TS_OUTPUT = 0x04,

    CXDREF_DEMOD_GPIO_MODE_FEF_PART = 0x05,

    CXDREF_DEMOD_GPIO_MODE_EWS_FLAG = 0x06,

    CXDREF_DEMOD_GPIO_MODE_UPLINK_FLAG = 0x07,

    CXDREF_DEMOD_GPIO_MODE_DISEQC_TX_EN = 0x08,

    CXDREF_DEMOD_GPIO_MODE_DISEQC_RX_EN = 0x09
} cxdref_demod_gpio_mode_t;

typedef enum {
    CXDREF_DEMOD_SERIAL_TS_CLK_HIGH_FULL,
    CXDREF_DEMOD_SERIAL_TS_CLK_MID_FULL,
    CXDREF_DEMOD_SERIAL_TS_CLK_LOW_FULL,
    CXDREF_DEMOD_SERIAL_TS_CLK_HIGH_HALF,
    CXDREF_DEMOD_SERIAL_TS_CLK_MID_HALF,
    CXDREF_DEMOD_SERIAL_TS_CLK_LOW_HALF
} cxdref_demod_serial_ts_clk_t ;

typedef enum {
    CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_HIGH,
    CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_MID,
    CXDREF_DEMOD_2BIT_PARALLEL_TS_CLK_LOW
} cxdref_demod_2bit_parallel_ts_clk_t ;

typedef enum {
    CXDREF_DEMOD_DVBSS2_BLINDSCAN_VERSION1,
    CXDREF_DEMOD_DVBSS2_BLINDSCAN_VERSION2
} cxdref_demod_dvbss2_blindscan_version_t;

typedef enum {
    CXDREF_DEMOD_OUTPUT_ISDBS3_TLV,
    CXDREF_DEMOD_OUTPUT_ISDBS3_TS,
    CXDREF_DEMOD_OUTPUT_ISDBS3_TLV_DIV_TS
} cxdref_demod_output_isdbs3_t;

typedef enum {
    CXDREF_DEMOD_OUTPUT_DVBC2_TS,
    CXDREF_DEMOD_OUTPUT_DVBC2_TLV
} cxdref_demod_output_dvbc2_t;

typedef enum {
    CXDREF_DEMOD_OUTPUT_ATSC3_ALP,
    CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS,
    CXDREF_DEMOD_OUTPUT_ATSC3_BBP
} cxdref_demod_output_atsc3_t;

typedef enum {
    CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS_TLV_AUTO,
    CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TS,
    CXDREF_DEMOD_OUTPUT_ISDBC_CHBOND_TLV
} cxdref_demod_output_isdbc_chbond_t;

typedef enum {
    CXDREF_DEMOD_ATSC_CPU_STATE_IDLE,
    CXDREF_DEMOD_ATSC_CPU_STATE_BUSY
} cxdref_demod_atsc_cpu_state_t;

typedef enum {
    CXDREF_DEMOD_CHBOND_CONFIG_DISABLE,
    CXDREF_DEMOD_CHBOND_CONFIG_ISDBC,
    CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN,
    CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB
} cxdref_demod_chbond_config_t;

typedef struct {
    uint32_t configDVBT_5;
    uint32_t configDVBT_6;
    uint32_t configDVBT_7;
    uint32_t configDVBT_8;
    uint32_t configDVBT2_1_7;
    uint32_t configDVBT2_5;
    uint32_t configDVBT2_6;
    uint32_t configDVBT2_7;
    uint32_t configDVBT2_8;
    uint32_t configDVBC2_6;
    uint32_t configDVBC2_8;
    uint32_t configDVBC_6;
    uint32_t configDVBC_7;
    uint32_t configDVBC_8;
    uint32_t configATSC;
    uint32_t configATSC3_6;
    uint32_t configATSC3_7;
    uint32_t configATSC3_8;
    uint32_t configISDBT_6;
    uint32_t configISDBT_7;
    uint32_t configISDBT_8;
    uint32_t configISDBC_6;
    uint32_t configJ83B_5_06_5_36;
    uint32_t configJ83B_5_60;
} cxdref_demod_iffreq_config_t;

typedef struct {
    uint8_t slaveAddress;
    uint8_t bank;
    uint8_t registerAddress;
    uint8_t value;
    uint8_t bitMask;
} cxdref_demod_config_memory_t;

typedef struct {
    cxdref_demod_xtal_t xtalFreq;

    uint8_t i2cAddressSLVT;

    cxdref_demod_tuner_i2c_config_t tunerI2cConfig;

#ifdef CXDREF_DEMOD_SUPPORT_ATSC
    uint8_t atscCoreDisable;
#endif

} cxdref_demod_create_param_t;

typedef struct cxdref_demod_t {

    cxdref_demod_xtal_t xtalFreq;

    uint8_t i2cAddressSLVT;

    uint8_t i2cAddressSLVX;

    uint8_t i2cAddressSLVR;

    uint8_t i2cAddressSLVM;

    cxdref_i2c_t * pI2c;

    cxdref_demod_tuner_i2c_config_t tunerI2cConfig;

    uint8_t atscCoreDisable;

    uint8_t parallelTSClkManualSetting;

    uint8_t serialTSClockModeContinuous;

    uint8_t isTSBackwardsCompatibleMode;

    cxdref_demod_chbond_config_t chbondConfig;

    uint8_t chbondStreamIn;

    cxdref_demod_config_memory_t configMemory[CXDREF_DEMOD_MAX_CONFIG_MEMORY_COUNT];

    uint8_t configMemoryLastEntry;

    void * user;

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE

    cxdref_demod_iffreq_config_t iffreqConfig;

    cxdref_demod_tuner_optimize_t tunerOptimize;

    cxdref_demod_terr_cable_spectrum_sense_t confSense;

    cxdref_demod_terr_cable_spectrum_sense_t rfSpectrumSense;

    cxdref_demod_serial_ts_clk_t serialTSClkFreqTerrCable;

    cxdref_demod_2bit_parallel_ts_clk_t twoBitParallelTSClkFreqTerrCable;

    uint8_t blindTuneDvbt2First;

    cxdref_demod_output_dvbc2_t dvbc2Output;

    cxdref_demod_output_atsc3_t atsc3Output;

    cxdref_demod_output_isdbc_chbond_t isdbcChBondOutput;

    uint8_t atsc3AutoSpectrumInv;

    uint8_t atsc3CWDetection;

    uint8_t atsc3AutoPLPID_SPLP;

    uint8_t atscUnlockDetection;

    uint8_t atscAutoSignalCheckOff;

    uint32_t atscNoSignalThresh;

    uint32_t atscSignalThresh;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_SAT

    uint8_t isSinglecable;

    cxdref_demod_sat_iq_sense_t satTunerIqSense;

    uint8_t isSinglecableSpectrumInv;

    cxdref_demod_serial_ts_clk_t serialTSClkFreqSat;

    cxdref_demod_2bit_parallel_ts_clk_t twoBitParallelTSClkFreqSat;

    uint8_t dvbss2PowerSmooth;

    cxdref_demod_dvbss2_blindscan_version_t dvbss2BlindScanVersion;

    cxdref_demod_output_isdbs3_t isdbs3Output;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_TLV

    uint8_t serialTLVClockModeContinuous;

    cxdref_demod_serial_ts_clk_t serialTLVClkFreqTerrCable;

    cxdref_demod_serial_ts_clk_t serialTLVClkFreqSat;

    cxdref_demod_2bit_parallel_ts_clk_t twoBitParallelTLVClkFreqTerrCable;

    cxdref_demod_2bit_parallel_ts_clk_t twoBitParallelTLVClkFreqSat;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP

    uint8_t serialALPClockModeContinuous;

#endif
    
    cxdref_demod_state_t state;

    cxdref_dtv_system_t system;

    cxdref_demod_chip_id_t chipId;

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE

    cxdref_dtv_bandwidth_t bandwidth;

    uint8_t scanMode;

    cxdref_demod_isdbt_ews_state_t isdbtEwsState;

    cxdref_demod_atsc3_eas_state_t atsc3EasState;

    uint8_t atsc3AutoSpectrumInv_flag;

    uint8_t atsc3CWDetection_flag;

    cxdref_demod_atsc_cpu_state_t atscCPUState;

#endif

#ifdef CXDREF_DEMOD_SUPPORT_SAT

    uint8_t dvbss2ScanMode;

#endif

} cxdref_demod_t;

cxdref_result_t cxdref_demod_Create (cxdref_demod_t * pDemod,
                                 cxdref_demod_create_param_t * pCreateParam,
                                 cxdref_i2c_t * pDemodI2c);

cxdref_result_t cxdref_demod_Initialize (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_Shutdown (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_TuneEnd (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_SoftReset (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_SetConfig (cxdref_demod_t * pDemod,
                                    cxdref_demod_config_id_t configId,
                                    int32_t value);

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_SetIFFreqConfig (cxdref_demod_t * pDemod,
                                          cxdref_demod_iffreq_config_t * pIffreqConfig);
#endif

cxdref_result_t cxdref_demod_TunerI2cEnable (cxdref_demod_t * pDemod,
                                         cxdref_demod_tuner_i2c_config_t tunerI2cConfig);

cxdref_result_t cxdref_demod_I2cRepeaterEnable (cxdref_demod_t * pDemod,
                                            uint8_t enable);

cxdref_result_t cxdref_demod_GPIOSetConfig (cxdref_demod_t * pDemod,
                                        cxdref_demod_gpio_pin_t pin,
                                        uint8_t enable,
                                        cxdref_demod_gpio_mode_t mode);

cxdref_result_t cxdref_demod_GPIORead (cxdref_demod_t * pDemod,
                                   cxdref_demod_gpio_pin_t pin,
                                   uint8_t * pValue);

cxdref_result_t cxdref_demod_GPIOWrite (cxdref_demod_t * pDemod,
                                    cxdref_demod_gpio_pin_t pin,
                                    uint8_t value);

cxdref_result_t cxdref_demod_ChipID (cxdref_demod_t * pDemod,
                                 cxdref_demod_chip_id_t * pChipId);

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_terr_cable_monitor_InternalDigitalAGCOut (cxdref_demod_t * pDemod,
                                                                   uint16_t * pDigitalAGCOut);
#endif

cxdref_result_t cxdref_demod_SetAndSaveRegisterBits (cxdref_demod_t * pDemod,
                                                 uint8_t slaveAddress,
                                                 uint8_t bank,
                                                 uint8_t registerAddress,
                                                 uint8_t value,
                                                 uint8_t bitMask);

#ifdef CXDREF_DEMOD_SUPPORT_TERR_OR_CABLE
cxdref_result_t cxdref_demod_terr_cable_SetScanMode (cxdref_demod_t * pDemod,
                                                 cxdref_dtv_system_t system,
                                                 uint8_t scanModeEnabled);
#endif

cxdref_result_t cxdref_demod_SetTSClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system);

#ifdef CXDREF_DEMOD_SUPPORT_TLV
cxdref_result_t cxdref_demod_SetTLVClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system);
#endif

#ifdef CXDREF_DEMOD_SUPPORT_ALP
cxdref_result_t cxdref_demod_SetALPClockModeAndFreq (cxdref_demod_t * pDemod, cxdref_dtv_system_t system);
#endif

cxdref_result_t cxdref_demod_SetTSDataPinHiZ (cxdref_demod_t * pDemod, uint8_t enable);

cxdref_result_t cxdref_demod_SetStreamOutput (cxdref_demod_t * pDemod, uint8_t enable);

#endif
