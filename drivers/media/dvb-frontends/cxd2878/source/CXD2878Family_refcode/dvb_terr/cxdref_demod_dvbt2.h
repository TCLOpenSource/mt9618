/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBT2_H
#define CXDREF_DEMOD_DVBT2_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef enum {
    CXDREF_DEMOD_DVBT2_TUNE_INFO_OK,

    CXDREF_DEMOD_DVBT2_TUNE_INFO_INVALID_PLP_ID
} cxdref_demod_dvbt2_tune_info_t;

typedef struct cxdref_dvbt2_tune_param_t{
    uint32_t centerFreqKHz;

    cxdref_dtv_bandwidth_t bandwidth;

    uint8_t dataPlpId;

    cxdref_dvbt2_profile_t profile;

    cxdref_demod_dvbt2_tune_info_t tuneInfo;
}cxdref_dvbt2_tune_param_t;

cxdref_result_t cxdref_demod_dvbt2_Tune (cxdref_demod_t * pDemod,
                                     cxdref_dvbt2_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_dvbt2_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbt2_CheckDemodLock (cxdref_demod_t * pDemod,
                                               cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_dvbt2_CheckTSLock (cxdref_demod_t * pDemod,
                                            cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_dvbt2_SetPLPConfig (cxdref_demod_t * pDemod,
                                             uint8_t autoPLP,
                                             uint8_t plpId);
cxdref_result_t cxdref_demod_dvbt2_SetProfile (cxdref_demod_t * pDemod,
                                           cxdref_dvbt2_profile_t profile);

cxdref_result_t cxdref_demod_dvbt2_CheckL1PostValid (cxdref_demod_t * pDemod,
                                                 uint8_t * pL1PostValid);

cxdref_result_t cxdref_demod_dvbt2_ClearBERSNRHistory (cxdref_demod_t * pDemod);

#endif
