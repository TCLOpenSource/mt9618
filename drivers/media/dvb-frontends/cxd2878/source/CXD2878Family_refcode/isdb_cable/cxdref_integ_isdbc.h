/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ISDBC_H
#define CXDREF_INTEG_ISDBC_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_isdbc.h"

#define CXDREF_ISDBC_WAIT_DEMOD_LOCK           1000
#define CXDREF_ISDBC_WAIT_LOCK_INTERVAL        10

cxdref_result_t cxdref_integ_isdbc_Tune (cxdref_integ_t * pInteg,
                                     cxdref_isdbc_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_isdbc_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_isdbc_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
