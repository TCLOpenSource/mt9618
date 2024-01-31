/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_SAT_DEVICE_CTRL_H
#define CXDREF_INTEG_SAT_DEVICE_CTRL_H

#include "cxdref_integ.h"
#include "cxdref_demod_sat_device_ctrl.h"
#include "cxdref_singlecable_command.h"
#include "cxdref_singlecable2_command.h"

#define CXDREF_INTEG_SAT_DEVICE_CTRL_LNBC_WAKEUP_TIME_MS 500

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME 15

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_SPEC_PRE_TX_WAIT 26

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_SPEC_PRE_TX_WAIT 26

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_TXIDLE_MS  (CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_SPEC_PRE_TX_WAIT + CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME)

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_TXIDLE_MS  (CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_SPEC_PRE_TX_WAIT + CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME)

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_RXIDLE_MS 5

#define CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_RXIDLE_MS 5

#define CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_TXIDLE_MS ((CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME < 5) ? 5 : CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME)
#if CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME >= 22
#error "CXDREF_INTEG_SAT_DEVICE_CTRL_DEVICE_VOLTAGE_CHANGE_TIME exceeds single cable standard timing requirement."
#endif

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_R2R_TIME_MS 400

#define CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_R2R_TIME_MS 0

#define CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_RTO_TIME_MS 400

#define CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE2REPLY_RTO_TIME_MS 100

#define CXDREF_INTEG_DISEQC_TB_POL                        100

#define CXDREF_INTEG_DISEQC_DISEQC_TRANSMIT_POL           10

#define CXDREF_INTEG_DISEQC_SINGLECABLE_TRANSMIT_POL      10

typedef enum {
    CXDREF_SINGLECABLE_VERSION_1_EN50494,
    CXDREF_SINGLECABLE_VERSION_2_EN50607,
    CXDREF_SINGLECABLE_VERSION_UNKNOWN
} cxdref_singlecable_version_t;

typedef struct {
    cxdref_integ_t * pInteg;
    cxdref_tuner_t * pTunerReal;

    cxdref_singlecable_version_t version;

    cxdref_singlecable_address_t address;
    cxdref_singlecable_bank_t bank;
    uint8_t ubSlot;
    uint32_t ubSlotFreqKHz;
    uint8_t enableMDUMode;
    uint8_t PINCode;

    uint8_t scd2_ubSlot;
    uint32_t scd2_ubSlotFreqMHz;
    uint8_t scd2_uncommittedSwitches;
    uint8_t scd2_committedSwitches;
    uint8_t scd2_enableMDUMode;
    uint8_t scd2_PINCode;
    
    cxdref_result_t (*pTransmitByOtherDemod)(cxdref_diseqc_message_t * pMessage);
} cxdref_integ_singlecable_tuner_data_t;

typedef struct {
    cxdref_tuner_t tunerSinglecable;
    cxdref_integ_singlecable_tuner_data_t tunerSinglecableData;
} cxdref_integ_singlecable_data_t;

cxdref_result_t cxdref_integ_sat_device_ctrl_Enable (cxdref_integ_t * pInteg, uint8_t enable);

cxdref_result_t cxdref_integ_sat_device_ctrl_SetVoltageTone (cxdref_integ_t * pInteg,
                                                         uint8_t isVoltageHigh,
                                                         uint8_t isContinuousToneOn);

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitToneBurst (cxdref_integ_t * pInteg,
                                                            uint8_t isVoltageHigh,
                                                            cxdref_toneburst_mode_t toneBurstMode,
                                                            uint8_t isContinuousToneOn);

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitDiseqcCommand (cxdref_integ_t * pInteg,
                                                                uint8_t isVoltageHigh,
                                                                cxdref_toneburst_mode_t toneBurstMode,
                                                                uint8_t isContinuousToneOn,
                                                                cxdref_diseqc_message_t * pCommand1,
                                                                uint8_t repeatCount1,
                                                                cxdref_diseqc_message_t * pCommand2,
                                                                uint8_t repeatCount2);

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitDiseqcCommandWithReply (cxdref_integ_t * pInteg,
                                                                         uint8_t isVoltageHigh,
                                                                         cxdref_toneburst_mode_t toneBurstMode,
                                                                         uint8_t isContinuousToneOn,
                                                                         cxdref_diseqc_message_t * pCommand,
                                                                         cxdref_diseqc_message_t * pReply);

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecableCommand (cxdref_integ_t * pInteg,
                                                                     cxdref_diseqc_message_t * pCommand);


cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecable2Command (cxdref_integ_t * pInteg,
                                                                      cxdref_diseqc_message_t * pCommand);

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecable2CommandWithReply (cxdref_integ_t * pInteg,
                                                                               cxdref_diseqc_message_t * pCommand,
                                                                               cxdref_diseqc_message_t * pReply);

cxdref_result_t cxdref_integ_sat_device_ctrl_EnableSinglecable (cxdref_integ_t * pInteg,
                                                            cxdref_integ_singlecable_data_t * pSinglecableData,
                                                            cxdref_result_t (*pTransmitByOtherDemod)(cxdref_diseqc_message_t * pMessage));

cxdref_result_t cxdref_integ_sat_device_ctrl_DisableSinglecable (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_sat_device_ctrl_SetSinglecableTunerParams (cxdref_integ_t * pInteg,
                                                                    cxdref_singlecable_address_t address,
                                                                    cxdref_singlecable_bank_t bank,
                                                                    uint8_t ubSlot,
                                                                    uint32_t ubSlotFreqKHz,
                                                                    uint8_t enableMDUMode,
                                                                    uint8_t PINCode);

cxdref_result_t cxdref_integ_sat_device_ctrl_SetSinglecable2TunerParams (cxdref_integ_t * pInteg,
                                                                     uint8_t ubSlot,
                                                                     uint32_t ubSlotFreqMHz,
                                                                     uint8_t uncommittedSwitches,
                                                                     uint8_t committedSwitches,
                                                                     uint8_t enableMDUMode,
                                                                     uint8_t PINCode);

#endif
