/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ISDBC_CHBOND_H
#define CXDREF_DEMOD_ISDBC_CHBOND_H

#include "cxdref_demod.h"
#include "cxdref_isdbc_chbond.h"

typedef struct {
    uint8_t bondOK;
    uint8_t groupIDCheck;
    uint8_t streamTypeCheck;
    uint8_t numCarriersCheck;
    uint8_t carrierSeqCheck;
    uint8_t calcNumCarriersCheck;
} cxdref_demod_isdbc_chbond_state_t;

cxdref_result_t cxdref_demod_isdbc_chbond_Enable (cxdref_demod_t * pDemod,
                                              uint8_t enable);

cxdref_result_t cxdref_demod_isdbc_chbond_SetTSMFConfig (cxdref_demod_t * pDemod,
                                                     cxdref_isdbc_tsid_type_t tsidType,
                                                     uint16_t tsid,
                                                     uint16_t networkId);

cxdref_result_t cxdref_demod_isdbc_chbond_SoftReset (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_BondStat (cxdref_demod_t * pDemod,
                                                        cxdref_demod_isdbc_chbond_state_t * pState);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_BondStat_hold (cxdref_demod_t * pDemod,
                                                             cxdref_demod_isdbc_chbond_state_t * pState);

cxdref_result_t cxdref_demod_isdbc_chbond_ClearBondStat (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_TLVConvError (cxdref_demod_t * pDemod,
                                                            uint8_t * pError);

cxdref_result_t cxdref_demod_isdbc_chbond_ClearTLVConvError (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_CarrierDelay (cxdref_demod_t * pDemod,
                                                            uint32_t * pDelay);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_EWSChange (cxdref_demod_t * pDemod,
                                                         uint8_t * pEWSChange);

cxdref_result_t cxdref_demod_isdbc_chbond_ClearEWSChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_VersionChange (cxdref_demod_t * pDemod,
                                                             uint8_t * pVersionChange);

cxdref_result_t cxdref_demod_isdbc_chbond_ClearVersionChange (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_StreamType (cxdref_demod_t * pDemod,
                                                          cxdref_isdbc_chbond_stream_type_t * pStreamType);

cxdref_result_t cxdref_demod_isdbc_chbond_monitor_TSMFHeaderExt (cxdref_demod_t * pDemod,
                                                             cxdref_isdbc_tsmf_header_t * pTSMFHeader,
                                                             cxdref_isdbc_chbond_tsmf_header_ext_t * pTSMFHeaderExt);

#endif
