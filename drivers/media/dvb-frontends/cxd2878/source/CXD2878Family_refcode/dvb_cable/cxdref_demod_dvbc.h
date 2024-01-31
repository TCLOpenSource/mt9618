/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBC_H
#define CXDREF_DEMOD_DVBC_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct cxdref_dvbc_tune_param_t{
    uint32_t centerFreqKHz;

    cxdref_dtv_bandwidth_t bandwidth;

}cxdref_dvbc_tune_param_t;

cxdref_result_t cxdref_demod_dvbc_Tune (cxdref_demod_t * pDemod, cxdref_dvbc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_dvbc_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbc_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock);

#endif
