/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ISDBT_H
#define CXDREF_INTEG_ISDBT_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_isdbt.h"

#define CXDREF_ISDBT_WAIT_DEMOD_LOCK           1500
#define CXDREF_ISDBT_WAIT_TS_LOCK              1500
#define CXDREF_ISDBT_WAIT_LOCK_INTERVAL        10

typedef struct cxdref_integ_isdbt_scan_param_t {
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;
    cxdref_dtv_bandwidth_t bandwidth;
} cxdref_integ_isdbt_scan_param_t;

typedef struct cxdref_integ_isdbt_scan_result_t {
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_isdbt_tune_param_t tuneParam;
} cxdref_integ_isdbt_scan_result_t;

typedef void (*cxdref_integ_isdbt_scan_callback_t) (cxdref_integ_t * pInteg,
                                                  cxdref_integ_isdbt_scan_result_t * pResult,
                                                  cxdref_integ_isdbt_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_isdbt_Tune(cxdref_integ_t * pInteg,
                                    cxdref_isdbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_isdbt_EWSTune(cxdref_integ_t * pInteg,
                                       cxdref_isdbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_isdbt_Scan(cxdref_integ_t * pInteg,
                                    cxdref_integ_isdbt_scan_param_t * pScanParam,
                                    cxdref_integ_isdbt_scan_callback_t callBack);

cxdref_result_t cxdref_integ_isdbt_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_isdbt_WaitDemodLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_isdbt_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
