/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBT_MONITOR_H
#define CXDREF_DEMOD_ISDBT_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_isdbt.h"
#include "cxdref_demod_isdbt.h"

#define CXDREF_DEMOD_ISDBT_MONITOR_PRERSBER_INVALID  0xFFFFFFFF
#define CXDREF_DEMOD_ISDBT_MONITOR_PER_INVALID       0xFFFFFFFF

cxdref_result_t cxdref_demod_isdbt_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pDmdLockStat,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pUnlockDetected);

cxdref_result_t cxdref_demod_isdbt_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_isdbt_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset);

cxdref_result_t cxdref_demod_isdbt_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                 uint32_t * pBERA, uint32_t * pBERB, uint32_t * pBERC);

cxdref_result_t cxdref_demod_isdbt_monitor_TMCCInfo (cxdref_demod_t * pDemod,
                                                 cxdref_isdbt_tmcc_info_t * pTMCCInfo);

cxdref_result_t cxdref_demod_isdbt_monitor_PresetInfo(cxdref_demod_t * pDemod,
                                                  cxdref_demod_isdbt_preset_info_t * pPresetInfo);

cxdref_result_t cxdref_demod_isdbt_monitor_PacketErrorNumber (cxdref_demod_t * pDemod,
                                                          uint32_t * pPENA, uint32_t * pPENB, uint32_t * pPENC);

cxdref_result_t cxdref_demod_isdbt_monitor_SpectrumSense (cxdref_demod_t * pDemod,
                                                      cxdref_demod_terr_cable_spectrum_sense_t * pSense);

cxdref_result_t cxdref_demod_isdbt_monitor_SNR (cxdref_demod_t * pDemod,
                                            int32_t * pSNR);

cxdref_result_t cxdref_demod_isdbt_monitor_ModeGuard(cxdref_demod_t * pDemod,
                                                 cxdref_isdbt_mode_t * pMode,
                                                 cxdref_isdbt_guard_t * pGuard);

cxdref_result_t cxdref_demod_isdbt_monitor_SamplingOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pPPM);

cxdref_result_t cxdref_demod_isdbt_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPERA, uint32_t * pPERB, uint32_t * pPERC);

cxdref_result_t cxdref_demod_isdbt_monitor_TSRate (cxdref_demod_t * pDemod,
                                               uint32_t * pTSRateKbpsA, uint32_t * pTSRateKbpsB, uint32_t * pTSRateKbpsC);

cxdref_result_t cxdref_demod_isdbt_monitor_ACEEWInfo(cxdref_demod_t * pDemod,
                                                 uint8_t * pIsExist,
                                                 cxdref_isdbt_aceew_info_t * pACEEWInfo);

cxdref_result_t cxdref_demod_isdbt_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                  uint8_t * pEWSChange);

cxdref_result_t cxdref_demod_isdbt_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pTMCCChange);

cxdref_result_t cxdref_demod_isdbt_ClearEWSChange (cxdref_demod_t * pDemod);


cxdref_result_t cxdref_demod_isdbt_ClearTMCCChange (cxdref_demod_t * pDemod);

#endif
