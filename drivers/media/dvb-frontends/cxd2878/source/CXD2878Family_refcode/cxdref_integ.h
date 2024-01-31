/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_INTEG_H
#define CXDREF_INTEG_H

#include "cxdref_demod.h"
#include "cxdref_tuner.h"

#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
#include "cxdref_lnbc.h"
#endif

typedef struct cxdref_integ_t {
    cxdref_demod_t * pDemod;
    cxdref_tuner_t * pTuner;

#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
    cxdref_lnbc_t * pLnbc;
#endif

    cxdref_atomic_t cancel;

    void * user;
} cxdref_integ_t;

cxdref_result_t cxdref_integ_Create (cxdref_integ_t * pInteg,
                                 cxdref_demod_t * pDemod,
                                 cxdref_demod_create_param_t * pCreateParam,
                                 cxdref_i2c_t * pDemodI2c,
                                 cxdref_tuner_t * pTuner
#ifdef CXDREF_DEMOD_SUPPORT_SAT_DEVICE_CTRL
                                 ,cxdref_lnbc_t * pLnbc
#endif
                                 );

cxdref_result_t cxdref_integ_Initialize (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_Sleep (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_Shutdown (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_Cancel (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_ClearCancel (cxdref_integ_t * pInteg);


cxdref_result_t cxdref_integ_CheckCancellation (cxdref_integ_t * pInteg);

#endif
