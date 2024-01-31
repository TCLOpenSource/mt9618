/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ATSC_MONITOR_H
#define CXDREF_DEMOD_ATSC_MONITOR_H

#include "cxdref_demod.h"

cxdref_result_t cxdref_demod_atsc_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pVQLockStat,
                                                uint8_t * pAGCLockStat,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_atsc_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_atsc_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                     int32_t * pOffset);

cxdref_result_t cxdref_demod_atsc_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER);

cxdref_result_t cxdref_demod_atsc_monitor_PreRSSER (cxdref_demod_t * pDemod,
                                                uint32_t * pSER);

cxdref_result_t cxdref_demod_atsc_monitor_PostRSWER (cxdref_demod_t * pDemod,
                                                 uint32_t * pWER);

cxdref_result_t cxdref_demod_atsc_monitor_PacketError (cxdref_demod_t * pDemod,
                                                   uint8_t * pPacketErrorDetected);

cxdref_result_t cxdref_demod_atsc_monitor_SNR (cxdref_demod_t * pDemod,
                                           int32_t * pSNR);

cxdref_result_t cxdref_demod_atsc_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM);

cxdref_result_t cxdref_demod_atsc_monitor_SignalLevelData_ForUnlockOptimization (cxdref_demod_t * pDemod,
                                                                             uint32_t * pSignalLevelData);

cxdref_result_t cxdref_demod_atsc_monitor_InternalDigitalAGCOut (cxdref_demod_t * pDemod,
                                                             uint32_t * pDigitalAGCOut);

/* customization from original reference code */
cxdref_result_t cxdref_demod_atsc_custom_monitor_SegmentSync (cxdref_demod_t * pDemod,
                                                          uint8_t * pSegmentSyncStat);

#endif
