/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBC_MONITOR_H
#define CXDREF_DEMOD_ISDBC_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_isdbc.h"

cxdref_result_t cxdref_demod_isdbc_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pARLock,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_isdbc_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_isdbc_monitor_QAM (cxdref_demod_t * pDemod,
                                            cxdref_isdbc_constellation_t * pQAM);

cxdref_result_t cxdref_demod_isdbc_monitor_SymbolRate (cxdref_demod_t * pDemod,
                                                   uint32_t * pSymRate);

cxdref_result_t cxdref_demod_isdbc_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset);

cxdref_result_t cxdref_demod_isdbc_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_isdbc_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR);

cxdref_result_t cxdref_demod_isdbc_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                 uint32_t * pBER);

cxdref_result_t cxdref_demod_isdbc_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                          uint32_t * pPEN);

cxdref_result_t cxdref_demod_isdbc_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPER);

cxdref_result_t cxdref_demod_isdbc_monitor_TSMFLock (cxdref_demod_t * pDemod, uint8_t * pTSMFLock);

cxdref_result_t cxdref_demod_isdbc_monitor_TSMFHeader (cxdref_demod_t * pDemod, cxdref_isdbc_tsmf_header_t * pTSMFHeader);

cxdref_result_t cxdref_demod_isdbc_monitor_VersionChange (cxdref_demod_t * pDemod,
                                                      uint8_t * pVersionChange);

cxdref_result_t cxdref_demod_isdbc_ClearVersionChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                  uint8_t * pEWSChange);

cxdref_result_t cxdref_demod_isdbc_ClearEWSChange (cxdref_demod_t * pDemod);

#endif
