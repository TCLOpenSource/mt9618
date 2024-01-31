/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBS_S2_MONITOR_H
#define CXDREF_DEMOD_DVBS_S2_MONITOR_H

#include "cxdref_common.h"
#include "cxdref_demod.h"

cxdref_result_t cxdref_demod_dvbs_s2_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                   uint8_t * pTSLockStat);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                        int32_t * pOffset);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                   uint32_t * pIFAGC);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_System (cxdref_demod_t * pDemod,
                                                 cxdref_dtv_system_t * pSystem);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_SymbolRate (cxdref_demod_t * pDemod,
                                                     uint32_t * pSymbolRateSps);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_IQSense (cxdref_demod_t * pDemod,
                                                  cxdref_demod_sat_iq_sense_t * pSense);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_CNR (cxdref_demod_t * pDemod,
                                              int32_t * pCNR);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_PER (cxdref_demod_t * pDemod,
                                              uint32_t * pPER);

cxdref_result_t cxdref_demod_dvbs_monitor_CodeRate (cxdref_demod_t * pDemod,
                                                cxdref_dvbs_coderate_t * pCodeRate);

cxdref_result_t cxdref_demod_dvbs_monitor_PreViterbiBER (cxdref_demod_t * pDemod,
                                                     uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbs_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbs2_monitor_PLSCode (cxdref_demod_t * pDemod,
                                                cxdref_dvbs2_plscode_t * pPLSCode);

cxdref_result_t cxdref_demod_dvbs2_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                   uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbs2_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                  uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbs2_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                   uint32_t * pFER);

cxdref_result_t cxdref_demod_dvbs2_monitor_BBHeader (cxdref_demod_t * pDemod,
                                                 cxdref_dvbs2_bbheader_t * pBBHeader);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_ScanInfo (cxdref_demod_t * pDemod,
                                                   uint8_t * pTSLock,
                                                   int32_t * pOffset,
                                                   cxdref_dtv_system_t * pSystem);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_Pilot (cxdref_demod_t * pDemod,
                                                uint8_t * pPlscLock,
                                                uint8_t * pPilotOn);

cxdref_result_t cxdref_demod_dvbs_s2_monitor_TSRate (cxdref_demod_t * pDemod,
                                                 uint32_t * pTSRateKbps);

cxdref_result_t cxdref_demod_dvbs2_monitor_Rolloff (cxdref_demod_t * pDemod,
                                                uint8_t * pRolloff);

#endif
