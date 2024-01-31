/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_J83B_MONITOR_H
#define CXDREF_DEMOD_J83B_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_j83b.h"

cxdref_result_t cxdref_demod_j83b_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                uint8_t * pARLock,
                                                uint8_t * pTSLockStat,
                                                uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_j83b_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_j83b_monitor_QAM (cxdref_demod_t * pDemod,
                                           cxdref_j83b_constellation_t * pQAM);

cxdref_result_t cxdref_demod_j83b_monitor_SymbolRate (cxdref_demod_t * pDemod,
                                                  uint32_t * pSymRate);

cxdref_result_t cxdref_demod_j83b_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pPPM);

cxdref_result_t cxdref_demod_j83b_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                     int32_t * pOffset);

cxdref_result_t cxdref_demod_j83b_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                     cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_j83b_monitor_SNR (cxdref_demod_t * pDemod,
                                           int32_t * pSNR);

cxdref_result_t cxdref_demod_j83b_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER);

cxdref_result_t cxdref_demod_j83b_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                         uint32_t * pPEN);

cxdref_result_t cxdref_demod_j83b_monitor_PER (cxdref_demod_t * pDemod,
                                           uint32_t * pPER);

cxdref_result_t cxdref_demod_j83b_monitor_Interleave (cxdref_demod_t * pDemod,
                                                  cxdref_j83b_interleave_t * pInterleave);

#endif
