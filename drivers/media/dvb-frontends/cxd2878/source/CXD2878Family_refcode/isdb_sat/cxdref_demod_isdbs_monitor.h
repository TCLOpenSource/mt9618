/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBS_MONITOR_H
#define CXDREF_DEMOD_ISDBS_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_isdbs.h"

#define CXDREF_DEMOD_ISDBS_MONITOR_PRERSBER_INVALID  0xFFFFFFFF
#define CXDREF_DEMOD_ISDBS_MONITOR_PER_INVALID       0xFFFFFFFF

cxdref_result_t cxdref_demod_isdbs_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                 uint8_t * pAGCLockStat,
                                                 uint8_t * pTSLockStat,
                                                 uint8_t * pTMCCLockStat);

cxdref_result_t cxdref_demod_isdbs_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                      int32_t * pOffset);

cxdref_result_t cxdref_demod_isdbs_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                 uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_isdbs_monitor_PreRSBER (cxdref_demod_t * pDemod,
                                                 uint32_t * pBERH, uint32_t * pBERL, uint32_t * pBERTMCC);

cxdref_result_t cxdref_demod_isdbs_monitor_CNR (cxdref_demod_t * pDemod,
                                            int32_t * pCNR);

cxdref_result_t cxdref_demod_isdbs_monitor_PER (cxdref_demod_t * pDemod,
                                            uint32_t * pPERH, uint32_t * pPERL);

cxdref_result_t cxdref_demod_isdbs_monitor_TMCCInfo (cxdref_demod_t*           pDemod,
                                                 cxdref_isdbs_tmcc_info_t * pTMCCInfo);

cxdref_result_t cxdref_demod_isdbs_monitor_ModCod (cxdref_demod_t * pDemod,
                                               cxdref_isdbs_modcod_t * pModCodH, cxdref_isdbs_modcod_t * pModCodL);

cxdref_result_t cxdref_demod_isdbs_monitor_SlotNum (cxdref_demod_t * pDemod,
                                                uint8_t * pSlotNumH, uint8_t * pSlotNumL);

cxdref_result_t cxdref_demod_isdbs_monitor_SiteDiversityInfo (cxdref_demod_t * pDemod,
                                                          uint8_t * pSiteDiversityInfo);

cxdref_result_t cxdref_demod_isdbs_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                  uint8_t * pEWSChange);

cxdref_result_t cxdref_demod_isdbs_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pTMCCChange);

cxdref_result_t cxdref_demod_isdbs_ClearEWSChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs_ClearTMCCChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs_monitor_TSIDError (cxdref_demod_t * pDemod,
                                                  uint8_t * pTSIDError);

cxdref_result_t cxdref_demod_isdbs_monitor_LowCN (cxdref_demod_t * pDemod,
                                              uint8_t * pLowCN);

cxdref_result_t cxdref_demod_isdbs_ClearLowCN (cxdref_demod_t * pDemod);

#endif
