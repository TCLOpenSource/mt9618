/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBS3_MONITOR_H
#define CXDREF_DEMOD_ISDBS3_MONITOR_H

#include "cxdref_demod.h"
#include "cxdref_isdbs3.h"

#define CXDREF_DEMOD_ISDBS3_MONITOR_PRELDPCBER_INVALID 0xFFFFFFFF
#define CXDREF_DEMOD_ISDBS3_MONITOR_PREBCHBER_INVALID  0xFFFFFFFF
#define CXDREF_DEMOD_ISDBS3_MONITOR_POSTBCHFER_INVALID 0xFFFFFFFF

cxdref_result_t cxdref_demod_isdbs3_monitor_SyncStat (cxdref_demod_t * pDemod,
                                                  uint8_t * pAGCLockStat,
                                                  uint8_t * pTSTLVLockStat,
                                                  uint8_t * pTMCCLockStat);

cxdref_result_t cxdref_demod_isdbs3_monitor_CarrierOffset (cxdref_demod_t * pDemod,
                                                       int32_t * pOffset);

cxdref_result_t cxdref_demod_isdbs3_monitor_IFAGCOut (cxdref_demod_t * pDemod,
                                                  uint32_t * pIFAGCOut);

cxdref_result_t cxdref_demod_isdbs3_monitor_IQSense (cxdref_demod_t * pDemod,
                                                 cxdref_demod_sat_iq_sense_t * pSense);

cxdref_result_t cxdref_demod_isdbs3_monitor_CNR (cxdref_demod_t * pDemod,
                                             int32_t * pCNR);

cxdref_result_t cxdref_demod_isdbs3_monitor_PreLDPCBER (cxdref_demod_t * pDemod,
                                                    uint32_t * pBERH, uint32_t * pBERL);

cxdref_result_t cxdref_demod_isdbs3_monitor_PreBCHBER (cxdref_demod_t * pDemod,
                                                   uint32_t * pBERH, uint32_t * pBERL);

cxdref_result_t cxdref_demod_isdbs3_monitor_PostBCHFER (cxdref_demod_t * pDemod,
                                                    uint32_t * pFERH, uint32_t * pFERL);

cxdref_result_t cxdref_demod_isdbs3_monitor_TMCCInfo (cxdref_demod_t * pDemod,
                                                  cxdref_isdbs3_tmcc_info_t * pTMCCInfo);

cxdref_result_t cxdref_demod_isdbs3_monitor_ModCod (cxdref_demod_t * pDemod,
                                                cxdref_isdbs3_mod_t * pModH, cxdref_isdbs3_mod_t * pModL,
                                                cxdref_isdbs3_cod_t * pCodH, cxdref_isdbs3_cod_t * pCodL);

cxdref_result_t cxdref_demod_isdbs3_monitor_ValidSlotNum (cxdref_demod_t * pDemod,
                                                      uint8_t * pSlotNumH, uint8_t * pSlotNumL);

cxdref_result_t cxdref_demod_isdbs3_monitor_CurrentStreamModCodSlotNum (cxdref_demod_t *pDemod, cxdref_isdbs3_tmcc_info_t * pTmccInfo,
                                                                    cxdref_isdbs3_mod_t * pModH, cxdref_isdbs3_mod_t * pModL,
                                                                    cxdref_isdbs3_cod_t * pCodH, cxdref_isdbs3_cod_t * pCodL,
                                                                    uint8_t * pSlotNumH, uint8_t * pSlotNumL);

cxdref_result_t cxdref_demod_isdbs3_monitor_SiteDiversityInfo (cxdref_demod_t * pDemod,
                                                           uint8_t * pSiteDiversityInfo);


cxdref_result_t cxdref_demod_isdbs3_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                   uint8_t * pEWSChange);

cxdref_result_t cxdref_demod_isdbs3_monitor_TMCCChange (cxdref_demod_t * pDemod,
                                                    uint8_t * pTMCCChange);

cxdref_result_t cxdref_demod_isdbs3_ClearEWSChange (cxdref_demod_t * pDemod);


cxdref_result_t cxdref_demod_isdbs3_ClearTMCCChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbs3_monitor_StreamIDError (cxdref_demod_t * pDemod,
                                                       uint8_t * pStreamIDError);

cxdref_result_t cxdref_demod_isdbs3_monitor_LowCN (cxdref_demod_t * pDemod,
                                               uint8_t * pLowCN);

cxdref_result_t cxdref_demod_isdbs3_ClearLowCN (cxdref_demod_t * pDemod);

#endif
