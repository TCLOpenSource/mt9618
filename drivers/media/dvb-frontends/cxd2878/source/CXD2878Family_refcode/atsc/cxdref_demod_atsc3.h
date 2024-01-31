/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ATSC3_H
#define CXDREF_DEMOD_ATSC3_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
#include "cxdref_demod_atsc3_chbond.h"
#endif

typedef struct cxdref_atsc3_tune_param_t {
    uint32_t centerFreqKHz;

    cxdref_dtv_bandwidth_t bandwidth;

    uint8_t plpIDNum;

    uint8_t plpID[4];

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    cxdref_demod_atsc3_chbond_plp_bond_t plpBond[4];
#endif

} cxdref_atsc3_tune_param_t;

cxdref_result_t cxdref_demod_atsc3_Tune (cxdref_demod_t * pDemod,
                                     cxdref_atsc3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_atsc3_EASTune (cxdref_demod_t * pDemod,
                                        cxdref_atsc3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_demod_atsc3_Sleep (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_EASTuneEnd (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_CheckDemodLock (cxdref_demod_t * pDemod,
                                               cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_atsc3_CheckALPLock (cxdref_demod_t * pDemod,
                                             cxdref_demod_lock_result_t * pLock);

cxdref_result_t cxdref_demod_atsc3_SetPLPConfig (cxdref_demod_t * pDemod,
                                             uint8_t plpIDNum,
                                             uint8_t plpID[4]);

cxdref_result_t cxdref_demod_atsc3_ClearBERSNRHistory (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_ClearGPIOEASLatch (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_UnlockCase (cxdref_demod_t * pDemod, uint8_t * pContinueWait);

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_SetCWTracking (cxdref_demod_t * pDemod, uint8_t * pCompleted);

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_Init (cxdref_demod_t * pDemod);

#endif
