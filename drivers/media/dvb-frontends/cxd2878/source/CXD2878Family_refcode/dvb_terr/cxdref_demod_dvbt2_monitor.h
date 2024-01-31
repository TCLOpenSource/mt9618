/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_DVBT2_MONITOR_H
#define CXDREF_DEMOD_DVBT2_MONITOR_H

#include "cxdref_demod.h"

cxdref_result_t cxdref_demod_dvbt2_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pSyncStat,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_dvbt2_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset);

cxdref_result_t cxdref_demod_dvbt2_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGC);

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Pre (cxdref_demod_t * pDemod,
                                              cxdref_dvbt2_l1pre_t * pL1Pre);

cxdref_result_t cxdref_demod_dvbt2_monitor_Version (cxdref_demod_t * pDemod,
                                                cxdref_dvbt2_version_t * pVersion);

cxdref_result_t cxdref_demod_dvbt2_monitor_OFDM (cxdref_demod_t * pDemod,
                                             cxdref_dvbt2_ofdm_t * pOfdm);

cxdref_result_t cxdref_demod_dvbt2_monitor_DataPLPs (cxdref_demod_t * pDemod,
                                                 uint8_t * pPLPIds,
                                                 uint8_t * pNumPLPs);

cxdref_result_t cxdref_demod_dvbt2_monitor_ActivePLP (cxdref_demod_t * pDemod,
                                                  cxdref_dvbt2_plp_btype_t type,
                                                  cxdref_dvbt2_plp_t * pPLPInfo);

cxdref_result_t cxdref_demod_dvbt2_monitor_DataPLPError (cxdref_demod_t * pDemod,
                                                     uint8_t * pPLPError);

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Change (cxdref_demod_t * pDemod,
                                                 uint8_t * pL1Change);

cxdref_result_t cxdref_demod_dvbt2_monitor_L1Post (cxdref_demod_t * pDemod,
                                               cxdref_dvbt2_l1post_t * pL1Post);

cxdref_result_t cxdref_demod_dvbt2_monitor_BBHeader (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt2_plp_btype_t type,
                                                 cxdref_dvbt2_bbheader_t * pBBHeader);

cxdref_result_t cxdref_demod_dvbt2_monitor_InBandBTSRate (cxdref_demod_t * pDemod,
                                                      cxdref_dvbt2_plp_btype_t type,
                                                      uint32_t * pTSRateBps);

cxdref_result_t cxdref_demod_dvbt2_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_dvbt2_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR);

cxdref_result_t cxdref_demod_dvbt2_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                   uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt2_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                   uint32_t * pFER);

cxdref_result_t cxdref_demod_dvbt2_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                  uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt2_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                          uint32_t * pPEN);

cxdref_result_t cxdref_demod_dvbt2_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pPPM);

cxdref_result_t cxdref_demod_dvbt2_monitor_TSRate (cxdref_demod_t * pDemod,
                                               uint32_t * pTSRateKbps);

cxdref_result_t cxdref_demod_dvbt2_monitor_Quality (cxdref_demod_t * pDemod,
                                                uint8_t * pQuality);

cxdref_result_t cxdref_demod_dvbt2_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPER);

cxdref_result_t cxdref_demod_dvbt2_monitor_QAM (cxdref_demod_t * pDemod,
                                            cxdref_dvbt2_plp_btype_t type,
                                            cxdref_dvbt2_plp_constell_t * pQAM);

cxdref_result_t cxdref_demod_dvbt2_monitor_CodeRate (cxdref_demod_t * pDemod,
                                                 cxdref_dvbt2_plp_btype_t type,
                                                 cxdref_dvbt2_plp_code_rate_t * pCodeRate);

cxdref_result_t cxdref_demod_dvbt2_monitor_Profile (cxdref_demod_t * pDemod,
                                                cxdref_dvbt2_profile_t * pProfile);

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedPreBCHBER (cxdref_demod_t * pDemod,
                                                          uint32_t * pBER);

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                    int32_t * pSNR);

cxdref_result_t cxdref_demod_dvbt2_monitor_AveragedQuality (cxdref_demod_t * pDemod,
                                                        uint8_t * pQuality);

#endif
