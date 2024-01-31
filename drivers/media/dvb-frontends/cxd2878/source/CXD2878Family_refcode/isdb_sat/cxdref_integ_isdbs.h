/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ISDBS_H
#define CXDREF_INTEG_ISDBS_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_isdbs.h"

#define CXDREF_ISDBS_WAIT_TS_LOCK              1000
#define CXDREF_ISDBS_WAIT_LOCK_INTERVAL        10

cxdref_result_t cxdref_integ_isdbs_Tune (cxdref_integ_t * pInteg,
                                     cxdref_isdbs_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_isdbs_WaitTSLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_isdbs_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
