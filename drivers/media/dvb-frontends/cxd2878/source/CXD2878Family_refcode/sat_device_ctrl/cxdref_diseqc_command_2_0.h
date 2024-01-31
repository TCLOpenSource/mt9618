/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DISEQC_COMMAND_2_0_H
#define CXDREF_DISEQC_COMMAND_2_0_H

#include "cxdref_common.h"
#include "cxdref_diseqc_command.h"

cxdref_result_t cxdref_diseqc_command_ClrReset (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_SetContend (cxdref_diseqc_message_t * pMessage,
                                              cxdref_diseqc_framing_t framing,
                                              cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Contend (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_ClrContend (cxdref_diseqc_message_t * pMessage,
                                              cxdref_diseqc_framing_t framing,
                                              cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Address (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_MoveC (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address,
                                         uint8_t data);

cxdref_result_t cxdref_diseqc_command_Move (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address,
                                        uint8_t data);

cxdref_result_t cxdref_diseqc_command_Status (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Config (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Switch0 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_Switch1 (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_ReadA0 (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_ReadA1 (cxdref_diseqc_message_t * pMessage,
                                          cxdref_diseqc_framing_t framing,
                                          cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LOstring (cxdref_diseqc_message_t * pMessage,
                                            cxdref_diseqc_framing_t framing,
                                            cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LOnow (cxdref_diseqc_message_t * pMessage,
                                         cxdref_diseqc_framing_t framing,
                                         cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LOLo (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address);

cxdref_result_t cxdref_diseqc_command_LOHi (cxdref_diseqc_message_t * pMessage,
                                        cxdref_diseqc_framing_t framing,
                                        cxdref_diseqc_address_t address);

#endif
