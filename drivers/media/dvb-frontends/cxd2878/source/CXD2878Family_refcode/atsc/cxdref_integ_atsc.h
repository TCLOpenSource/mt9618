/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ATSC_H
#define CXDREF_INTEG_ATSC_H

#include "cxdref_integ.h"
#include "cxdref_demod_atsc.h"

#define CXDREF_ATSC_WAIT_DEMOD_LOCK     1200
#define CXDREF_ATSC_WAIT_TS_LOCK        1200
#define CXDREF_ATSC_WAIT_LOCK_INTERVAL  10
#define CXDREF_ATSC_WAIT_DEMOD_FAST_UNLOCK     400
#define CXDREF_ATSC_WAIT_TS_FAST_UNLOCK        400

typedef struct cxdref_integ_atsc_scan_param_t {
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

} cxdref_integ_atsc_scan_param_t;

typedef struct cxdref_integ_atsc_scan_result_t {
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_atsc_tune_param_t tuneParam;

} cxdref_integ_atsc_scan_result_t;

typedef void (*cxdref_integ_atsc_scan_callback_t) (cxdref_integ_t * pInteg,
                                                 cxdref_integ_atsc_scan_result_t * pResult,
                                                 cxdref_integ_atsc_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_atsc_Tune (cxdref_integ_t * pInteg,
                                    cxdref_atsc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_atsc_Scan (cxdref_integ_t * pInteg,
                                    cxdref_integ_atsc_scan_param_t * pScanParam,
                                    cxdref_integ_atsc_scan_callback_t callBack);

cxdref_result_t cxdref_integ_atsc_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_atsc_monitor_RFLevel (cxdref_integ_t * pInteg,
                                               int32_t * pRFLeveldB);

#endif
