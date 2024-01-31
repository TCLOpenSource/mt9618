/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_ISDBS3_H
#define CXDREF_INTEG_ISDBS3_H

#include "cxdref_demod.h"
#include "cxdref_integ.h"
#include "cxdref_demod_isdbs3.h"

#define CXDREF_ISDBS3_WAIT_TMCC_LOCK            2000
#define CXDREF_ISDBS3_WAIT_TSTLV_LOCK           3000
#define CXDREF_ISDBS3_WAIT_LOCK_INTERVAL        10

cxdref_result_t cxdref_integ_isdbs3_Tune (cxdref_integ_t * pInteg,
                                      cxdref_isdbs3_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_isdbs3_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB);

#endif
