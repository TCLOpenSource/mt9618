/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DISEQC_COMMAND_1_2_H
#define CXDREF_DISEQC_COMMAND_1_2_H

#include "cxdref_common.h"
#include "cxdref_diseqc_command.h"

typedef enum {
    CXDREF_DISEQC_MOTOR_MODE_STEPPED,
    CXDREF_DISEQC_MOTOR_MODE_TIMED,
    CXDREF_DISEQC_MOTOR_MODE_NONSTOP
} cxdref_diseqc_motor_mode_t;

cxdref_result_t cxdref_diseqc_command_WriteA0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data);

cxdref_result_t cxdref_diseqc_command_WriteA1 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data);

cxdref_result_t cxdref_diseqc_command_Halt (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LimitsOff (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LimitE (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LimitW (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_DriveEast (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address,
                                             cxdref_diseqc_motor_mode_t mode,
                                             uint8_t amount);

cxdref_result_t cxdref_diseqc_command_DriveWest (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address,
                                             cxdref_diseqc_motor_mode_t mode,
                                             uint8_t amount);

cxdref_result_t cxdref_diseqc_command_Storenn (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t posNumber);

cxdref_result_t cxdref_diseqc_command_Gotonn (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address,
                                          uint8_t posNumber);

cxdref_result_t cxdref_diseqc_command_SetPosns (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address,
                                            uint8_t * pData,
                                            uint8_t length);

#endif
