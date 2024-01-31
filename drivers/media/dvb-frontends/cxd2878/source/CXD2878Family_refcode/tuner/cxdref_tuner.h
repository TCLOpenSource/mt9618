/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_TUNER_H
#define CXDREF_TUNER_H

#include "cxdref_common.h"
#include "cxdref_i2c.h"
#include "cxdref_dtv.h"

typedef enum {
    CXDREF_TUNER_RFLEVEL_FUNC_NOSUPPORT,
    CXDREF_TUNER_RFLEVEL_FUNC_READ,
    CXDREF_TUNER_RFLEVEL_FUNC_CALCFROMAGC
} cxdref_tuner_rflevel_func_t;

typedef struct cxdref_tuner_t {
    uint8_t i2cAddress;
    cxdref_i2c_t * pI2c;
    uint32_t flags;
    uint32_t symbolRateKSpsForSpectrum;

    cxdref_tuner_rflevel_func_t rfLevelFuncTerr;
    cxdref_tuner_rflevel_func_t rfLevelFuncSat;

    struct cxdref_tuner_t * pTunerTerrCable;
    struct cxdref_tuner_t * pTunerSat;

    void * user;

    uint32_t frequencyKHz;
    cxdref_dtv_system_t system;
    cxdref_dtv_bandwidth_t bandwidth;
    uint32_t symbolRateKSps;
    
    cxdref_result_t (*Initialize) (struct cxdref_tuner_t * pTuner);

    cxdref_result_t (*TerrCableTune) (struct cxdref_tuner_t * pTuner,
                                    uint32_t centerFreqKHz,
                                    cxdref_dtv_system_t system,
                                    cxdref_dtv_bandwidth_t bandwidth);


    cxdref_result_t (*SatTune) (struct cxdref_tuner_t * pTuner,
                              uint32_t centerFreqKHz,
                              cxdref_dtv_system_t system,
                              uint32_t symbolRateKSps);

    cxdref_result_t (*Sleep) (struct cxdref_tuner_t * pTuner);

    cxdref_result_t (*Shutdown) (struct cxdref_tuner_t * pTuner);

    cxdref_result_t (*ReadRFLevel) (struct cxdref_tuner_t * pTuner,
                                  int32_t *pRFLevel);

    cxdref_result_t (*CalcRFLevelFromAGC) (struct cxdref_tuner_t * pTuner,
                                         uint32_t agcValue,
                                         int32_t *pRFLevel);
} cxdref_tuner_t;

cxdref_result_t cxdref_tuner_sw_combined_Create (cxdref_tuner_t * pTuner,
                                             cxdref_tuner_t * pTunerTerrCable,
                                             cxdref_tuner_t * pTunerSat);

#endif
