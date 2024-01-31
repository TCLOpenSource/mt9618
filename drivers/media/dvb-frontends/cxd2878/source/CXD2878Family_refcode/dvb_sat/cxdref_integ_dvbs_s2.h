/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_DVBS_S2_H
#define CXDREF_INTEG_DVBS_S2_H

#include "cxdref_integ.h"
#include "cxdref_demod_dvbs_s2.h"

#define CXDREF_INTEG_DVBS_S2_TUNE_POLLING_INTERVAL        10

cxdref_result_t cxdref_integ_dvbs_s2_Tune (cxdref_integ_t * pInteg,
                                       cxdref_dvbs_s2_tune_param_t * pTuneParam);

cxdref_result_t cxdref_integ_dvbs_s2_monitor_RFLevel (cxdref_integ_t * pInteg,
                                                  int32_t * pRFLeveldB);

#endif
