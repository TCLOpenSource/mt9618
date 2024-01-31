/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBC_H
#define CXDREF_DEMOD_ISDBC_H

#include "cxdref_common.h"
#include "cxdref_demod.h"
#include "cxdref_isdbc.h"

typedef enum {
    CXDREF_DEMOD_ISDBC_TSMF_MODE_SINGLE,
    CXDREF_DEMOD_ISDBC_TSMF_MODE_MULTIPLE,
    CXDREF_DEMOD_ISDBC_TSMF_MODE_AUTO
} cxdref_demod_isdbc_tsmf_mode_t;

typedef struct cxdref_isdbc_tune_param_t {
    uint32_t centerFreqKHz;
    cxdref_demod_isdbc_tsmf_mode_t tsmfMode;
    cxdref_isdbc_tsid_type_t tsidType;
    uint16_t tsid;
    uint16_t networkId;
} cxdref_isdbc_tune_param_t;

cxdref_result_t cxdref_demod_isdbc_Tune (cxdref_demod_t * pDemod, cxdref_isdbc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_isdbc_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock);

#endif
