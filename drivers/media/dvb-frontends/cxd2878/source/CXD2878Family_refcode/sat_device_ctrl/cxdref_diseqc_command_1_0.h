/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DISEQC_COMMAND_1_0_H
#define CXDREF_DISEQC_COMMAND_1_0_H

#include "cxdref_common.h"
#include "cxdref_diseqc_command.h"

cxdref_result_t cxdref_diseqc_command_Reset (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Standby (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_PowerOn (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetLo (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetVR (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetPosA (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetSOA (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetHi (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetHL (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetPosB (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetSOB (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_WriteN0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data);

#endif
