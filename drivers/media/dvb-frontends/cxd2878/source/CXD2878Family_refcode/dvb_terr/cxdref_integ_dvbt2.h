/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_DVBT2_H
#define CXDREF_INTEG_DVBT2_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_dvbt2.h"

#define CXDREF_DVBT2_BASE_WAIT_DEMOD_LOCK     3500
#define CXDREF_DVBT2_BASE_WAIT_TS_LOCK        1500
#define CXDREF_DVBT2_LITE_WAIT_DEMOD_LOCK     5000
#define CXDREF_DVBT2_LITE_WAIT_TS_LOCK        2300
#define CXDREF_DVBT2_WAIT_LOCK_INTERVAL       10
#define CXDREF_DVBT2_L1POST_TIMEOUT           500

typedef struct cxdref_integ_dvbt2_scan_param_t{
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

    cxdref_dtv_bandwidth_t bandwidth;

    cxdref_dvbt2_profile_t t2Profile;
}cxdref_integ_dvbt2_scan_param_t;

typedef struct cxdref_integ_dvbt2_scan_result_t{
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_dvbt2_tune_param_t dvbt2TuneParam;
}cxdref_integ_dvbt2_scan_result_t;

typedef void (*cxdref_integ_dvbt2_scan_callback_t) (cxdref_integ_t * pInteg,
                                                  cxdref_integ_dvbt2_scan_result_t * pResult,
                                                  cxdref_integ_dvbt2_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_dvbt2_Tune(cxdref_integ_t * pInteg,
                                    cxdref_dvbt2_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_dvbt2_BlindTune(cxdref_integ_t * pInteg,
                                         uint32_t centerFreqKHz,
                                         cxdref_dtv_bandwidth_t bandwidth,
                                         cxdref_dvbt2_profile_t profile,
                                         cxdref_dvbt2_profile_t * pProfileTuned);

cxdref_result_t cxdref_integ_dvbt2_Scan(cxdref_integ_t * pInteg,
                                    cxdref_integ_dvbt2_scan_param_t * pScanParam,
                                    cxdref_integ_dvbt2_scan_callback_t callBack);

cxdref_result_t cxdref_integ_dvbt2_Scan_PrepareDataPLPLoop(cxdref_integ_t * pInteg, uint8_t pPLPIds[], uint8_t *pNumPLPs, uint8_t *pMixed);

cxdref_result_t cxdref_integ_dvbt2_Scan_SwitchDataPLP(cxdref_integ_t * pInteg, uint8_t mixed, uint8_t plpId, cxdref_dvbt2_profile_t profile);

cxdref_result_t cxdref_integ_dvbt2_WaitTSLock (cxdref_integ_t * pInteg, cxdref_dvbt2_profile_t profile);

cxdref_result_t cxdref_integ_dvbt2_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

cxdref_result_t cxdref_integ_dvbt2_monitor_SSI (cxdref_integ_t * pInteg, uint8_t * pSSI);

#endif
