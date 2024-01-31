/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ATSC3_MONITOR_H
#define CXDREF_DEMOD_ATSC3_MONITOR_H

#include "cxdref_demod.h"

#define CXDREF_DEMOD_ATSC3_MONITOR_PRELDPCBER_INVALID 0xFFFFFFFF
#define CXDREF_DEMOD_ATSC3_MONITOR_PREBCHBER_INVALID  0xFFFFFFFF
#define CXDREF_DEMOD_ATSC3_MONITOR_POSTBCHFER_INVALID 0xFFFFFFFF
#define CXDREF_DEMOD_ATSC3_MONITOR_ALPHEADERLENGTHERROR_INVALID 0xFF

cxdref_result_t cxdref_demod_atsc3_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pSyncStat,
                                                 uint8_t alpLockStat[4],
                                                 uint8_t * pALPLockAll,
                                                 uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_atsc3_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset);

cxdref_result_t cxdref_demod_atsc3_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGC);

cxdref_result_t cxdref_demod_atsc3_monitor_Bootstrap (cxdref_demod_t * pDemod,
                                                  cxdref_atsc3_bootstrap_t * pBootstrap);

cxdref_result_t cxdref_demod_atsc3_monitor_OFDM (cxdref_demod_t * pDemod,
                                             cxdref_atsc3_ofdm_t * pOfdm);

cxdref_result_t cxdref_demod_atsc3_monitor_L1Basic (cxdref_demod_t * pDemod,
                                                cxdref_atsc3_l1basic_t * pL1Basic);

cxdref_result_t cxdref_demod_atsc3_monitor_L1Detail_raw (cxdref_demod_t * pDemod,
                                                     cxdref_atsc3_l1basic_t * pL1Basic,
                                                     cxdref_atsc3_l1detail_raw_t * pL1Detail);

cxdref_result_t cxdref_demod_atsc3_monitor_L1Detail_convert (cxdref_demod_t * pDemod,
                                                         cxdref_atsc3_l1basic_t * pL1Basic,
                                                         cxdref_atsc3_l1detail_raw_t * pL1Detail,
                                                         uint8_t plpID,
                                                         cxdref_atsc3_l1detail_common_t * pL1DetailCommon,
                                                         cxdref_atsc3_l1detail_subframe_t * pL1DetailSubframe,
                                                         cxdref_atsc3_l1detail_plp_t * pL1DetailPlp);

cxdref_result_t cxdref_demod_atsc3_monitor_PLPList (cxdref_demod_t * pDemod,
                                                cxdref_atsc3_plp_list_entry_t plpList[CXDREF_ATSC3_NUM_PLP_MAX],
                                                uint8_t * pNumPLPs);

cxdref_result_t cxdref_demod_atsc3_monitor_PLPError (cxdref_demod_t * pDemod,
                                                 uint8_t * pPLPError);

cxdref_result_t cxdref_demod_atsc3_monitor_SelectedPLPValid (cxdref_demod_t * pDemod,
                                                         uint8_t plpValid[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_atsc3_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR);

cxdref_result_t cxdref_demod_atsc3_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                   uint32_t ber[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                  uint32_t ber[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                   uint32_t fer[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_BBPacketErrorNumber (cxdref_demod_t * pDemod,
                                                            uint32_t pen[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pPPM);

cxdref_result_t cxdref_demod_atsc3_monitor_FecModCod (cxdref_demod_t * pDemod,
                                                  cxdref_atsc3_plp_fecmodcod_t fecmodcod[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_ALPHeaderLengthError (cxdref_demod_t * pDemod,
                                                             uint8_t errorStandard[4],
                                                             uint8_t errorKorean[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_AveragedPreBCHBER (cxdref_demod_t * pDemod,
                                                          uint32_t ber[4]);

cxdref_result_t cxdref_demod_atsc3_monitor_AveragedSNR (cxdref_demod_t * pDemod,
                                                    int32_t * pSNR);

cxdref_result_t cxdref_demod_atsc3_monitor_SignalChange(cxdref_demod_t* pDemod,
                                                    uint8_t* pFlag);

/* 
 *  customization from original reference code
 *  ASTC3.0 standard does not define signal quality spec. This is our original spec.
 */
cxdref_result_t cxdref_demod_atsc3_monitor_Quality (cxdref_demod_t * pDemod,
													uint8_t * pQuality);

#endif
