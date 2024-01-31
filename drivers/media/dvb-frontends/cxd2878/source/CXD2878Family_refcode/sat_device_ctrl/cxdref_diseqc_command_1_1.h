/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DISEQC_COMMAND_1_1_H
#define CXDREF_DISEQC_COMMAND_1_1_H

#include "cxdref_common.h"
#include "cxdref_diseqc_command.h"

cxdref_result_t cxdref_diseqc_command_SetS1A (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS2A (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS3A (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS4A (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS1B (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS2B (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS3B (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetS4B (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_WriteN1 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address,
                                           uint8_t data);

cxdref_result_t cxdref_diseqc_command_WriteFreq (cxdref_diseqc_message_t * pMessage,
                                             cxdref_diseqc_framing_t framing,
                                             cxdref_diseqc_address_t address,
                                             uint32_t frequency);

#endif
