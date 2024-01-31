/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBT_H
#define CXDREF_DEMOD_DVBT_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct cxdref_dvbt_tune_param_t{
    uint32_t centerFreqKHz;
    cxdref_dtv_bandwidth_t bandwidth;
    cxdref_dvbt_profile_t profile;
}cxdref_dvbt_tune_param_t;

cxdref_result_t cxdref_demod_dvbt_SetProfile (cxdref_demod_t * pDemod,
                                          cxdref_dvbt_profile_t profile);

cxdref_result_t cxdref_demod_dvbt_Tune (cxdref_demod_t * pDemod,
                                    cxdref_dvbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_dvbt_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbt_CheckDemodLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_dvbt_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_dvbt_EchoOptimization (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbt_ClearBERSNRHistory (cxdref_demod_t * pDemod);

#endif
