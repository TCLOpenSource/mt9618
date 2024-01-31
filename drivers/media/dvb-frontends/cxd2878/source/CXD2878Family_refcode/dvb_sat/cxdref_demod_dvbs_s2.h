/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBS_S2_H
#define CXDREF_DEMOD_DVBS_S2_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct {
    cxdref_dtv_system_t system;
    uint32_t centerFreqKHz;
    uint32_t symbolRateKSps;
} cxdref_dvbs_s2_tune_param_t;

cxdref_result_t cxdref_demod_dvbs_s2_Tune (cxdref_demod_t * pDemod,
                                       cxdref_dvbs_s2_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_dvbs_s2_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_dvbs_s2_CheckTSLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_dvbs_s2_CheckIQInvert (cxdref_demod_t * pDemod,
                                                uint8_t * pIsInvert);

cxdref_result_t cxdref_demod_dvbs_s2_SetSymbolRate (cxdref_demod_t * pDemod,
                                                uint32_t symbolRateKSps);

#endif
