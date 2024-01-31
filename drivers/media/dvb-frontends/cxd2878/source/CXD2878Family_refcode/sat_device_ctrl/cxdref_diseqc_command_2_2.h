/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DISEQC_COMMAND_2_2_H
#define CXDREF_DISEQC_COMMAND_2_2_H

#include "cxdref_common.h"
#include "cxdref_diseqc_command.h"

cxdref_result_t cxdref_diseqc_command_PosStat (cxdref_diseqc_message_t * pMessage,
                                           cxdref_diseqc_framing_t framing,
                                           cxdref_diseqc_address_t address);

#endif
