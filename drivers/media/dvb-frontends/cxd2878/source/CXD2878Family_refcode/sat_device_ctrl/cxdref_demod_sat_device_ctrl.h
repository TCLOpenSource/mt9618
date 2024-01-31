/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_SAT_DEVICE_CONTROL_H
#define CXDREF_DEMOD_SAT_DEVICE_CONTROL_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

#define CXDREF_DISEQC_MESSAGE_LENGTH 12

typedef struct {
    uint8_t data[CXDREF_DISEQC_MESSAGE_LENGTH];
    uint8_t length;
} cxdref_diseqc_message_t;

typedef enum {
    CXDREF_DSQOUT_MODE_PWM,
    CXDREF_DSQOUT_MODE_ENVELOPE
} cxdref_dsqout_mode_t;

typedef enum {
    CXDREF_RXEN_MODE_NORMAL,
    CXDREF_RXEN_MODE_INV,
    CXDREF_RXEN_MODE_FIXED_LOW,
    CXDREF_RXEN_MODE_FIXED_HIGH
} cxdref_rxen_mode_t;

typedef enum {
    CXDREF_TXEN_MODE_NORMAL,
    CXDREF_TXEN_MODE_INV,
    CXDREF_TXEN_MODE_FIXED_LOW,
    CXDREF_TXEN_MODE_FIXED_HIGH
} cxdref_txen_mode_t;

typedef enum {
    CXDREF_TONEBURST_MODE_OFF,
    CXDREF_TONEBURST_MODE_A,
    CXDREF_TONEBURST_MODE_B
} cxdref_toneburst_mode_t;

cxdref_result_t cxdref_demod_sat_device_ctrl_Enable (cxdref_demod_t * pDemod, uint8_t enable);

cxdref_result_t cxdref_demod_sat_device_ctrl_DSQOUTSetting (cxdref_demod_t * pDemod,
                                                        cxdref_dsqout_mode_t mode,
                                                        uint8_t toneFreqKHz);
cxdref_result_t cxdref_demod_sat_device_ctrl_RXENSetting (cxdref_demod_t * pDemod,
                                                      cxdref_rxen_mode_t mode,
                                                      uint8_t posDelay);

cxdref_result_t cxdref_demod_sat_device_ctrl_TXENSetting (cxdref_demod_t * pDemod,
                                                      cxdref_txen_mode_t mode,
                                                      uint8_t posDelay,
                                                      uint8_t negDelay);

cxdref_result_t cxdref_demod_sat_device_ctrl_OutputTone (cxdref_demod_t * pDemod,
                                                     uint8_t isEnable);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetToneBurst (cxdref_demod_t * pDemod,
                                                       cxdref_toneburst_mode_t toneBurstMode);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcCommand (cxdref_demod_t * pDemod,
                                                           uint8_t isEnable,
                                                           cxdref_diseqc_message_t * pCommand1,
                                                           uint8_t count1,
                                                           cxdref_diseqc_message_t * pCommand2,
                                                           uint8_t count2);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetTxIdleTime (cxdref_demod_t * pDemod,
                                                        uint8_t idleTimeMs);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetRxIdleTime (cxdref_demod_t * pDemod,
                                                        uint8_t idleTimeMs);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetR2RTime (cxdref_demod_t * pDemod,
                                                     uint32_t r2rTime);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (cxdref_demod_t * pDemod,
                                                             uint8_t isEnable,
                                                             uint16_t replyTimeoutMs);

cxdref_result_t cxdref_demod_sat_device_ctrl_StartTransmit (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_sat_device_ctrl_GetTransmitStatus (cxdref_demod_t * pDemod,
                                                            uint8_t * pStatus);

cxdref_result_t cxdref_demod_sat_device_ctrl_GetReplyMessage (cxdref_demod_t * pDemod,
                                                          cxdref_diseqc_message_t * pReplyMessage);

cxdref_result_t cxdref_demod_sat_device_ctrl_GetRXENMode (cxdref_demod_t * pDemod,
                                                      uint8_t * pIsEnable);

cxdref_result_t cxdref_demod_sat_device_ctrl_GetTXENMode (cxdref_demod_t * pDemod,
                                                      uint8_t * pIsEnable);

cxdref_result_t cxdref_demod_sat_device_ctrl_SetDiseqcReplyMask (cxdref_demod_t * pDemod,
                                                             uint8_t pattern,
                                                             uint8_t mask);

cxdref_result_t cxdref_demod_sat_device_ctrl_EnableSinglecable (cxdref_demod_t * pDemod,
                                                            uint8_t enable);

#endif
