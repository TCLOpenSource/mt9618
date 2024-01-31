/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_LNBC_H
#define CXDREF_LNBC_H

#include "cxdref_common.h"
#include "cxdref_i2c.h"

typedef enum {
    CXDREF_LNBC_STATE_UNKNOWN,
    CXDREF_LNBC_STATE_SLEEP,
    CXDREF_LNBC_STATE_ACTIVE
} cxdref_lnbc_state_t;

typedef enum {
    CXDREF_LNBC_VOLTAGE_LOW,
    CXDREF_LNBC_VOLTAGE_HIGH,
    CXDREF_LNBC_VOLTAGE_AUTO,
} cxdref_lnbc_voltage_t;

typedef enum {
    CXDREF_LNBC_TONE_OFF,
    CXDREF_LNBC_TONE_ON,
    CXDREF_LNBC_TONE_AUTO,
} cxdref_lnbc_tone_t;

typedef enum {
    CXDREF_LNBC_TRANSMIT_MODE_TX,
    CXDREF_LNBC_TRANSMIT_MODE_RX,
    CXDREF_LNBC_TRANSMIT_MODE_AUTO,
} cxdref_lnbc_transmit_mode_t;

typedef enum {
    CXDREF_LNBC_CONFIG_ID_TONE_INTERNAL,
    CXDREF_LNBC_CONFIG_ID_LOW_VOLTAGE,
    CXDREF_LNBC_CONFIG_ID_HIGH_VOLTAGE
} cxdref_lnbc_config_id_t;

#define CXDREF_LNBC_DIAG_STATUS_THERMALSHUTDOWN 0x00000001
#define CXDREF_LNBC_DIAG_STATUS_OVERCURRENT     0x00000002
#define CXDREF_LNBC_DIAG_STATUS_CABLEOPEN       0x00000004
#define CXDREF_LNBC_DIAG_STATUS_VOUTOUTOFRANGE  0x00000008
#define CXDREF_LNBC_DIAG_STATUS_VINOUTOFRANGE   0x00000010
#define CXDREF_LNBC_DIAG_STATUS_BACKBIAS        0x00000020
#define CXDREF_LNBC_DIAG_STATUS_LNBCDISABLE     0x00010000

typedef struct cxdref_lnbc_t {

    uint8_t i2cAddress;
    cxdref_i2c_t * pI2c;
    int32_t isInternalTone;
    int32_t lowVoltage;
    int32_t highVoltage;
    void * user;

    cxdref_lnbc_state_t state;
    cxdref_lnbc_voltage_t voltage;
    cxdref_lnbc_tone_t tone;
    cxdref_lnbc_transmit_mode_t transmitMode;

    cxdref_result_t (*Initialize) (struct cxdref_lnbc_t * pLnbc);

    cxdref_result_t (*SetConfig) (struct cxdref_lnbc_t * pLnbc, cxdref_lnbc_config_id_t configId, int32_t value);

    cxdref_result_t (*SetVoltage) (struct cxdref_lnbc_t * pLnbc, cxdref_lnbc_voltage_t voltage);

    cxdref_result_t (*SetTone) (struct cxdref_lnbc_t * pLnbc, cxdref_lnbc_tone_t tone);

    cxdref_result_t (*SetTransmitMode) (struct cxdref_lnbc_t * pLnbc, cxdref_lnbc_transmit_mode_t mode);

    cxdref_result_t (*Sleep) (struct cxdref_lnbc_t * pLnbc);

    cxdref_result_t (*WakeUp) (struct cxdref_lnbc_t * pLnbc);

    cxdref_result_t (*GetDiagStatus) (struct cxdref_lnbc_t * pLnbc, uint32_t * pStatus, uint32_t * pStatusSupported);

} cxdref_lnbc_t;

#endif
