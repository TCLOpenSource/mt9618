/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_DVBC_H
#define CXDREF_INTEG_DVBC_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_dvbc.h"

#define CXDREF_DVBC_WAIT_DEMOD_LOCK           1000
#define CXDREF_DVBC_WAIT_LOCK_INTERVAL        10

typedef struct cxdref_integ_dvbc_scan_param_t {
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

    cxdref_dtv_bandwidth_t bandwidth;

} cxdref_integ_dvbc_scan_param_t;

typedef struct cxdref_integ_dvbc_scan_result_t {
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_dvbc_tune_param_t tuneParam;
} cxdref_integ_dvbc_scan_result_t;

typedef void (*cxdref_integ_dvbc_scan_callback_t) (cxdref_integ_t * pInteg,
                                                 cxdref_integ_dvbc_scan_result_t * pResult,
                                                 cxdref_integ_dvbc_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_dvbc_Tune (cxdref_integ_t * pInteg,
                                    cxdref_dvbc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_dvbc_Scan (cxdref_integ_t * pInteg,
                                    cxdref_integ_dvbc_scan_param_t * pScanParam,
                                    cxdref_integ_dvbc_scan_callback_t callBack);

cxdref_result_t cxdref_integ_dvbc_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_dvbc_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
