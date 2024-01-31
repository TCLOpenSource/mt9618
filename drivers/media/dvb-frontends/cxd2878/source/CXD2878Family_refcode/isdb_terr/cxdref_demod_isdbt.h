/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBT_H
#define CXDREF_DEMOD_ISDBT_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

typedef struct cxdref_isdbt_tune_param_t {
    uint32_t centerFreqKHz;
    cxdref_dtv_bandwidth_t bandwidth;
} cxdref_isdbt_tune_param_t;

typedef struct cxdref_demod_isdbt_preset_info_t {
    uint8_t data[13];
} cxdref_demod_isdbt_preset_info_t;

cxdref_result_t cxdref_demod_isdbt_Tune (cxdref_demod_t * pDemod,
                                     cxdref_isdbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_isdbt_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbt_CheckDemodLock (cxdref_demod_t * pDemod,
                                               cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_isdbt_CheckTSLock (cxdref_demod_t * pDemod,
                                            cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_isdbt_SetPreset (cxdref_demod_t * pDemod,
                                          cxdref_demod_isdbt_preset_info_t* pPresetInfo);

cxdref_result_t cxdref_demod_isdbt_EWSTune (cxdref_demod_t*            pDemod,
                                        cxdref_isdbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_isdbt_EWSTuneEnd (cxdref_demod_t * pDemod);

#endif
