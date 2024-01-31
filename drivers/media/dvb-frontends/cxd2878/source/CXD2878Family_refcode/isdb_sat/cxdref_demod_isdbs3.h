/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBS3_H
#define CXDREF_DEMOD_ISDBS3_H

#include "cxdref_common.h"
#include "cxdref_demod.h"
#include "cxdref_isdbs3.h"

typedef struct cxdref_isdbs3_tune_param_t {
    uint32_t centerFreqKHz;
    uint16_t streamid;
    cxdref_isdbs3_streamid_type_t streamidType;
} cxdref_isdbs3_tune_param_t;

cxdref_result_t cxdref_demod_isdbs3_Tune (cxdref_demod_t * pDemod,
                                      cxdref_isdbs3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_isdbs3_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs3_CheckTMCCLock (cxdref_demod_t * pDemod,
                                               cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_isdbs3_CheckTSTLVLock (cxdref_demod_t * pDemod,
                                                cxdref_demod_lock_result_t * pLock);


cxdref_result_t cxdref_demod_isdbs3_SetStreamID (cxdref_demod_t * pDemod,
                                             uint16_t streamid,
                                             cxdref_isdbs3_streamid_type_t streamidType);

#endif
