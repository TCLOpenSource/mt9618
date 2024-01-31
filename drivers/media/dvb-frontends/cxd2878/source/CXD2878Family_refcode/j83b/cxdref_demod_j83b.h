/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_J83B_H
#define CXDREF_DEMOD_J83B_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct cxdref_j83b_tune_param_t {
    uint32_t centerFreqKHz;
    cxdref_dtv_bandwidth_t bandwidth;
} cxdref_j83b_tune_param_t;

cxdref_result_t cxdref_demod_j83b_Tune (cxdref_demod_t * pDemod, cxdref_j83b_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_j83b_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_j83b_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock);

#endif
