/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ATSC3_H
#define CXDREF_INTEG_ATSC3_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_atsc3.h"

#define CXDREF_ATSC3_WAIT_DEMOD_LOCK              3500
#define CXDREF_ATSC3_WAIT_ALP_LOCK                1500
#define CXDREF_ATSC3_WAIT_CW_TRACKING_TIMEOUT     300
#define CXDREF_ATSC3_WAIT_LOCK_INTERVAL           10

typedef struct cxdref_integ_atsc3_scan_param_t {
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

    cxdref_dtv_bandwidth_t bandwidth;

} cxdref_integ_atsc3_scan_param_t;

typedef struct cxdref_integ_atsc3_scan_result_t {
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_atsc3_tune_param_t tuneParam;

} cxdref_integ_atsc3_scan_result_t;

typedef void (*cxdref_integ_atsc3_scan_callback_t) (cxdref_integ_t * pInteg,
                                                  cxdref_integ_atsc3_scan_result_t * pResult,
                                                  cxdref_integ_atsc3_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_atsc3_Tune (cxdref_integ_t * pInteg,
                                     cxdref_atsc3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_atsc3_EASTune (cxdref_integ_t * pInteg,
                                        cxdref_atsc3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_atsc3_Scan (cxdref_integ_t * pInteg,
                                     cxdref_integ_atsc3_scan_param_t * pScanParam,
                                     cxdref_integ_atsc3_scan_callback_t callBack);

cxdref_result_t cxdref_integ_atsc3_SwitchPLP (cxdref_integ_t * pInteg,
                                          uint8_t plpIDNum,
                                          uint8_t plpID[4]);

cxdref_result_t cxdref_integ_atsc3_WaitALPLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_atsc3_monitor_RFLevel (cxdref_integ_t * pInteg,
                                                int32_t * pRFLeveldB);

#endif