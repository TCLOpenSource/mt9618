/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBS_H
#define CXDREF_DEMOD_ISDBS_H

#include "cxdref_common.h"
#include "cxdref_demod.h"
#include "cxdref_isdbs.h"

typedef struct cxdref_isdbs_tune_param_t {
    uint32_t centerFreqKHz;
    uint16_t tsid;
    cxdref_isdbs_tsid_type_t tsidType;
} cxdref_isdbs_tune_param_t;

cxdref_result_t cxdref_demod_isdbs_Tune (cxdref_demod_t * pDemod,
                                     cxdref_isdbs_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_isdbs_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs_CheckTSLock (cxdref_demod_t * pDemod,
                                            cxdref_demod_lock_result_t * pLock);

#endif
