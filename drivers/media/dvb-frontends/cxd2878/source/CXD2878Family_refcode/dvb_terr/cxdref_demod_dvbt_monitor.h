/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBT_MONITOR_H
#define CXDREF_DEMOD_DVBT_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_dvbt.h"

cxdref_result_t cxdref_demod_dvbt_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pSyncStat,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_dvbt_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_dvbt_monitor_ModeGuard (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt_mode_t * pMode,
                                                 cxdref_dvbt_guard_t * pGuard);

cxdref_result_t cxdref_demod_dvbt_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                     int32_t * pOffset);

cxdref_result_t cxdref_demod_dvbt_monitor_PreViterbiBER (cxdref_demod_t * pDemod,
                                                     uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt_monitor_TPSInfo (cxdref_demod_t * pDemod,
                                               cxdref_dvbt_tpsinfo_t * pInfo);

cxdref_result_t cxdref_demod_dvbt_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                         uint32_t * pPEN);

cxdref_result_t cxdref_demod_dvbt_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                     cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_dvbt_monitor_SNR (cxdref_demod_t * pDemod,
                                           int32_t * pSNR);

cxdref_result_t cxdref_demod_dvbt_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM);

cxdref_result_t cxdref_demod_dvbt_monitor_Quality (cxdref_demod_t * pDemod,
                                               uint8_t * pQuality);

cxdref_result_t cxdref_demod_dvbt_monitor_PER (cxdref_demod_t * pDemod,
                                           uint32_t * pPER);

cxdref_result_t cxdref_demod_dvbt_monitor_TSRate (cxdref_demod_t * pDemod,
                                              uint32_t * pTSRateKbps);

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedPreRSBER (cxdref_demod_t * pDemod,
                                                        uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                   int32_t * pSNR);

cxdref_result_t cxdref_demod_dvbt_monitor_AveragedQuality (cxdref_demod_t * pDemod,
                                                       uint8_t * pQuality);

#endif
