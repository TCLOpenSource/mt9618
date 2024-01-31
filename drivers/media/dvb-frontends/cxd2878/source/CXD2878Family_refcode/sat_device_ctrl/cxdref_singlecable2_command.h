/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_SINGLECABLE2_COMMAND_H
#define CXDREF_SINGLECABLE2_COMMAND_H

#include "cxdref_common.h"
#include "cxdref_demod_sat_device_ctrl.h"

cxdref_result_t cxdref_singlecable2_command_ODU_Channel_change (cxdref_diseqc_message_t * pMessage,
                                                            uint8_t userbandId,
                                                            uint32_t ifFreqMHz,
                                                            uint8_t uncommittedSwitches,
                                                            uint8_t committedSwitches);

cxdref_result_t cxdref_singlecable2_command_ODU_Channel_change_PIN (cxdref_diseqc_message_t * pMessage,
                                                                uint8_t userbandId,
                                                                uint32_t ifFreqMHz,
                                                                uint8_t uncommittedSwitches,
                                                                uint8_t committedSwitches,
                                                                uint8_t pin);

cxdref_result_t cxdref_singlecable2_command_ODU_PowerOFF (cxdref_diseqc_message_t * pMessage, uint8_t userbandId);

cxdref_result_t cxdref_singlecable2_command_ODU_PowerOFF_PIN (cxdref_diseqc_message_t * pMessage,
                                                          uint8_t userbandId,
                                                          uint8_t pin);

cxdref_result_t cxdref_singlecable2_command_ODU_UB_avail (cxdref_diseqc_message_t * pMessage);

cxdref_result_t cxdref_singlecable2_command_ODU_UB_freq (cxdref_diseqc_message_t * pMessage,
                                                     uint8_t userbandId);

cxdref_result_t cxdref_singlecable2_command_ODU_UB_inuse (cxdref_diseqc_message_t * pMessage);

cxdref_result_t cxdref_singlecable2_command_ODU_UB_PIN (cxdref_diseqc_message_t * pMessage);

cxdref_result_t cxdref_singlecable2_command_ODU_UB_switches (cxdref_diseqc_message_t * pMessage,
                                                         uint8_t userbandId);

#endif
