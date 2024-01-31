/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_DEMOD_ATSC3_CHBOND_H
#define CXDREF_DEMOD_ATSC3_CHBOND_H

#include "cxdref_demod.h"
#include "cxdref_atsc3_chbond.h"

typedef enum {
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_MAIN,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_FROM_SUB
} cxdref_demod_atsc3_chbond_plp_bond_t;

typedef enum {
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_UNUSED,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_FROM_SUB,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_ERR_ID,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_ERR_BONDCONF,
    CXDREF_DEMOD_ATSC3_CHBOND_PLP_VALID_OK
} cxdref_demod_atsc3_chbond_plp_valid_t;

cxdref_result_t cxdref_demod_atsc3_chbond_Enable (cxdref_demod_t * pDemod,
                                              uint8_t enable);

cxdref_result_t cxdref_demod_atsc3_chbond_SetPLPConfig (cxdref_demod_t * pDemod,
                                                    uint8_t plpIDNum,
                                                    uint8_t plpID[4],
                                                    cxdref_demod_atsc3_chbond_plp_bond_t plpBond[4]);

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_BondStat (cxdref_demod_t * pDemod,
                                                        uint8_t bondLockStat[4],
                                                        uint8_t * pBondLockAll,
                                                        uint8_t * pBondUnlockDetected);

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_SelectedPLPValid (cxdref_demod_t * pDemod,
                                                                cxdref_demod_atsc3_chbond_plp_valid_t plpValid[4]);

cxdref_result_t cxdref_demod_atsc3_chbond_monitor_L1Detail (cxdref_demod_t * pDemod,
                                                        cxdref_atsc3_chbond_l1detail_t * pL1Detail);

#endif
