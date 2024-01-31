/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ATSC_H
#define CXDREF_DEMOD_ATSC_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct cxdref_atsc_tune_param_t {
    uint32_t centerFreqKHz;
} cxdref_atsc_tune_param_t;

cxdref_result_t cxdref_demod_atsc_Tune (cxdref_demod_t * pDemod,
                                    cxdref_atsc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_atsc_TuneEnd (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc_SoftReset (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc_CheckDemodLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_atsc_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_atsc_SlaveRWriteRegister (cxdref_demod_t * pDemod,
                                                   uint8_t  bank,
                                                   uint8_t  registerAddress,
                                                   uint8_t  value,
                                                   uint8_t  bitMask);

cxdref_result_t cxdref_demod_atsc_SlaveRReadRegister (cxdref_demod_t * pDemod,
                                                  uint8_t bank,
                                                  uint8_t registerAddress,
                                                  uint8_t * pValue);

#endif
