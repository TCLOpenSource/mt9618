/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_DVBT_H
#define CXDREF_INTEG_DVBT_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_dvbt.h"

#define CXDREF_DVBT_WAIT_DEMOD_LOCK           1000
#define CXDREF_DVBT_WAIT_TS_LOCK              1000
#define CXDREF_DVBT_WAIT_LOCK_INTERVAL        10

typedef struct cxdref_integ_dvbt_scan_param_t{
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

    cxdref_dtv_bandwidth_t bandwidth;

}cxdref_integ_dvbt_scan_param_t;

typedef struct cxdref_integ_dvbt_scan_result_t{
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_dvbt_tune_param_t dvbtTuneParam;

}cxdref_integ_dvbt_scan_result_t;

typedef void (*cxdref_integ_dvbt_scan_callback_t) (cxdref_integ_t * pInteg,
                                                 cxdref_integ_dvbt_scan_result_t * pResult,
                                                 cxdref_integ_dvbt_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_dvbt_Tune(cxdref_integ_t * pInteg,
                                   cxdref_dvbt_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_dvbt_BlindTune(cxdref_integ_t * pInteg,
                                        uint32_t centerFreqKHz,
                                        cxdref_dtv_bandwidth_t bandwidth);

cxdref_result_t cxdref_integ_dvbt_Scan(cxdref_integ_t * pInteg,
                                   cxdref_integ_dvbt_scan_param_t * pScanParam,
                                   cxdref_integ_dvbt_scan_callback_t callBack);

cxdref_result_t cxdref_integ_dvbt_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_dvbt_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

cxdref_result_t cxdref_integ_dvbt_monitor_SSI (cxdref_integ_t * pInteg, uint8_t * pSSI);

#endif
