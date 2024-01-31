/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_SINGLECABLE_COMMAND_H
#define CXDREF_SINGLECABLE_COMMAND_H

#include "cxdref_common.h"
#include "cxdref_demod_sat_device_ctrl.h"

typedef enum {
    CXDREF_SINGLECABLE_ADDRESS_ALL_DEVICES            = 0x00,
    CXDREF_SINGLECABLE_ADDRESS_ALL_LNB_SMATV_SWITCHER = 0x10,
    CXDREF_SINGLECABLE_ADDRESS_LNB_DEVICE             = 0x11
} cxdref_singlecable_address_t;

typedef enum {
    CXDREF_SINGLECABLE_BANK_0 = 0,
    CXDREF_SINGLECABLE_BANK_1 = 1,
    CXDREF_SINGLECABLE_BANK_2 = 2,
    CXDREF_SINGLECABLE_BANK_3 = 3,
    CXDREF_SINGLECABLE_BANK_4 = 4,
    CXDREF_SINGLECABLE_BANK_5 = 5,
    CXDREF_SINGLECABLE_BANK_6 = 6,
    CXDREF_SINGLECABLE_BANK_7 = 7
} cxdref_singlecable_bank_t;

typedef enum {
    CXDREF_SINGLECABLE_NUM_OF_SAT_POS_1,
    CXDREF_SINGLECABLE_NUM_OF_SAT_POS_2
} cxdref_singlecable_num_of_sat_pos_t;

typedef enum {
    CXDREF_SINGLECABLE_RF_BAND_STANDARD,
    CXDREF_SINGLECABLE_RF_BAND_WIDE
} cxdref_singlecable_rf_band_t;

typedef enum {
    CXDREF_SINGLECABLE_NUM_OF_UB_2,
    CXDREF_SINGLECABLE_NUM_OF_UB_4,
    CXDREF_SINGLECABLE_NUM_OF_UB_6,
    CXDREF_SINGLECABLE_NUM_OF_UB_8
} cxdref_singlecable_num_of_ub_slots_t;

typedef enum {
    CXDREF_SINGLECABLE_LOFREQ_NONE            =   0x00,
    CXDREF_SINGLECABLE_LOFREQ_UNKNOWN         =   0x01,
    CXDREF_SINGLECABLE_LOFREQ_9750_MHZ        =   0x02,
    CXDREF_SINGLECABLE_LOFREQ_10000_MHZ       =   0x03,
    CXDREF_SINGLECABLE_LOFREQ_10600_MHZ       =   0x04,
    CXDREF_SINGLECABLE_LOFREQ_10750_MHZ       =   0x05,
    CXDREF_SINGLECABLE_LOFREQ_11000_MHZ       =   0x06,
    CXDREF_SINGLECABLE_LOFREQ_11250_MHZ       =   0x07,
    CXDREF_SINGLECABLE_LOFREQ_11475_MHZ       =   0x08,
    CXDREF_SINGLECABLE_LOFREQ_20250_MHZ       =   0x09,
    CXDREF_SINGLECABLE_LOFREQ_5150_MHZ        =   0x0A,
    CXDREF_SINGLECABLE_LOFREQ_1585_MHZ        =   0x0B,
    CXDREF_SINGLECABLE_LOFREQ_13850_MHZ       =   0x0C,
    CXDREF_SINGLECABLE_LOFREQ_WB_NONE         =   0x10,
    CXDREF_SINGLECABLE_LOFREQ_WB_10000_MHZ    =   0x11,
    CXDREF_SINGLECABLE_LOFREQ_WB_10200_MHZ    =   0x12,
    CXDREF_SINGLECABLE_LOFREQ_WB_13250_MHZ    =   0x13,
    CXDREF_SINGLECABLE_LOFREQ_WB_13450_MHZ    =   0x14
} cxdref_singlecable_lofreq_t;

typedef struct {
    cxdref_singlecable_num_of_sat_pos_t numberOfSatPos;
    cxdref_singlecable_rf_band_t rfBand;
    cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots;
} cxdref_singlecable_config_nb_t;

cxdref_result_t cxdref_singlecable_command_ODU_Channel_change (cxdref_diseqc_message_t * pMessage,
                                                           cxdref_singlecable_address_t address,
                                                           uint8_t userbandId,
                                                           uint32_t ubSlotFreqKHz,
                                                           cxdref_singlecable_bank_t bank,
                                                           uint32_t centerFreqKHz);

cxdref_result_t cxdref_singlecable_command_ODU_PowerOFF (cxdref_diseqc_message_t * pMessage,
                                                     cxdref_singlecable_address_t address,
                                                     uint8_t userbandId);

cxdref_result_t cxdref_singlecable_command_ODU_UbxSignal_ON (cxdref_diseqc_message_t * pMessage,
                                                         cxdref_singlecable_address_t address);

cxdref_result_t cxdref_singlecable_command_ODU_Config (cxdref_diseqc_message_t * pMessage,
                                                   cxdref_singlecable_address_t address,
                                                   uint8_t userbandId,
                                                   cxdref_singlecable_num_of_sat_pos_t numberOfSatPos,
                                                   cxdref_singlecable_rf_band_t rfBand,
                                                   cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots);

cxdref_result_t cxdref_singlecable_command_ODU_LoFreq (cxdref_diseqc_message_t * pMessage,
                                                   cxdref_singlecable_address_t address,
                                                   uint8_t userbandId,
                                                   cxdref_singlecable_lofreq_t loFreq);

cxdref_result_t cxdref_singlecable_command_ODU_Channel_change_MDU (cxdref_diseqc_message_t * pMessage,
                                                               cxdref_singlecable_address_t address,
                                                               uint8_t userbandId,
                                                               uint32_t ubSlotFreqKHz,
                                                               cxdref_singlecable_bank_t bank,
                                                               uint32_t centerFreqKHz,
                                                               uint8_t pin);

cxdref_result_t cxdref_singlecable_command_ODU_PowerOFF_MDU (cxdref_diseqc_message_t * pMessage,
                                                         cxdref_singlecable_address_t address,
                                                         uint8_t userbandId,
                                                         uint8_t pin);

cxdref_result_t cxdref_singlecable_command_ODU_UbxSignal_ON_MDU (cxdref_diseqc_message_t * pMessage,
                                                             cxdref_singlecable_address_t address,
                                                             uint8_t pin);

cxdref_result_t cxdref_singlecable_command_ODU_Config_MDU (cxdref_diseqc_message_t * pMessage,
                                                       cxdref_singlecable_address_t address,
                                                       uint8_t userbandId,
                                                       cxdref_singlecable_num_of_sat_pos_t numberOfSatPos,
                                                       cxdref_singlecable_rf_band_t rfBand,
                                                       cxdref_singlecable_num_of_ub_slots_t numberOfUBSlots,
                                                       uint8_t pin);

cxdref_result_t cxdref_singlecable_command_ODU_LoFreq_MDU (cxdref_diseqc_message_t * pMessage,
                                                       cxdref_singlecable_address_t address,
                                                       uint8_t userbandId,
                                                       cxdref_singlecable_lofreq_t loFreq,
                                                       uint8_t pin);

#endif
