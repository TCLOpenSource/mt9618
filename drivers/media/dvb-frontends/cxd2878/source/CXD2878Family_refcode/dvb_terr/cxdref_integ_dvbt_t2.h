/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_DVBT_T2_H
#define CXDREF_INTEG_DVBT_T2_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_integ_dvbt.h"
#include "cxdref_integ_dvbt2.h"
#include "cxdref_demod_dvbt.h"
#include "cxdref_demod_dvbt2.h"

typedef struct cxdref_integ_dvbt_t2_scan_param_t{
    uint32_t startFrequencyKHz;

    uint32_t endFrequencyKHz;

    uint32_t stepFrequencyKHz;

    cxdref_dtv_bandwidth_t bandwidth;

    cxdref_dtv_system_t system;

    cxdref_dvbt2_profile_t t2Profile;
}cxdref_integ_dvbt_t2_scan_param_t;

typedef struct cxdref_integ_dvbt_t2_scan_result_t{
    uint32_t centerFreqKHz;

    cxdref_result_t tuneResult;

    cxdref_dtv_system_t system;

    cxdref_dvbt_tune_param_t dvbtTuneParam;

    cxdref_dvbt2_tune_param_t dvbt2TuneParam;
}cxdref_integ_dvbt_t2_scan_result_t;

typedef void (*cxdref_integ_dvbt_t2_scan_callback_t) (cxdref_integ_t * pInteg,
                                                    cxdref_integ_dvbt_t2_scan_result_t * pResult,
                                                    cxdref_integ_dvbt_t2_scan_param_t * pScanParam);

cxdref_result_t cxdref_integ_dvbt_t2_BlindTune(cxdref_integ_t * pInteg,
                                           uint32_t centerFreqKHz,
                                           cxdref_dtv_bandwidth_t bandwidth,
                                           cxdref_dtv_system_t system,
                                           cxdref_dvbt2_profile_t profile,
                                           cxdref_dtv_system_t * pSystemTuned,
                                           cxdref_dvbt2_profile_t * pProfileTuned);

cxdref_result_t cxdref_integ_dvbt_t2_Scan(cxdref_integ_t * pInteg,
                                      cxdref_integ_dvbt_t2_scan_param_t * pScanParam,
                                      cxdref_integ_dvbt_t2_scan_callback_t callBack);

cxdref_result_t cxdref_integ_dvbt_t2_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
