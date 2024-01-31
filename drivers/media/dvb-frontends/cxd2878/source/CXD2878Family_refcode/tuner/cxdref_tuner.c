/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include "cxdref_tuner.h"
#include "cxdref_stdlib.h"

static cxdref_result_t cxdref_tuner_sw_combined_Initialize (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_Initialize");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->Initialize)) {
        result = pTuner->pTunerTerrCable->Initialize (pTuner->pTunerTerrCable);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pTuner->pTunerSat) && (pTuner->pTunerSat->Initialize)) {
        result = pTuner->pTunerSat->Initialize (pTuner->pTunerSat);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    pTuner->frequencyKHz = 0;
    pTuner->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_Sleep (cxdref_tuner_t * pTuner);

static cxdref_result_t cxdref_tuner_sw_combined_TerrCableTune (cxdref_tuner_t * pTuner,
                                                           uint32_t centerFreqKHz,
                                                           cxdref_dtv_system_t system,
                                                           cxdref_dtv_bandwidth_t bandwidth)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_TerrCableTune");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!CXDREF_DTV_SYSTEM_IS_TERR_CABLE (system)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (CXDREF_DTV_SYSTEM_IS_SAT (pTuner->system)) {
        result = cxdref_tuner_sw_combined_Sleep (pTuner);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->TerrCableTune)) {
        result = pTuner->pTunerTerrCable->TerrCableTune (pTuner->pTunerTerrCable,
            centerFreqKHz, system, bandwidth);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pTuner->frequencyKHz = pTuner->pTunerTerrCable->frequencyKHz;
    } else {
        pTuner->frequencyKHz = centerFreqKHz;
    }

    pTuner->system = system;
    pTuner->bandwidth = bandwidth;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_SatTune (cxdref_tuner_t * pTuner,
                                                     uint32_t centerFreqKHz,
                                                     cxdref_dtv_system_t system,
                                                     uint32_t symbolRateKSps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_SatTune");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!CXDREF_DTV_SYSTEM_IS_SAT (system)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (pTuner->system)) {
        result = cxdref_tuner_sw_combined_Sleep (pTuner);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pTuner->pTunerSat) && (pTuner->pTunerSat->SatTune)) {
        result = pTuner->pTunerSat->SatTune (pTuner->pTunerSat,
            centerFreqKHz, system, symbolRateKSps);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        pTuner->frequencyKHz = pTuner->pTunerSat->frequencyKHz;
    } else {
        pTuner->frequencyKHz = centerFreqKHz;
    }

    pTuner->system = system;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = symbolRateKSps;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_Sleep (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_Sleep");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->Sleep)
        && CXDREF_DTV_SYSTEM_IS_TERR_CABLE (pTuner->system)) {
        result = pTuner->pTunerTerrCable->Sleep (pTuner->pTunerTerrCable);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pTuner->pTunerSat) && (pTuner->pTunerSat->Sleep)
        && CXDREF_DTV_SYSTEM_IS_SAT (pTuner->system)) {
        result = pTuner->pTunerSat->Sleep (pTuner->pTunerSat);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    pTuner->frequencyKHz = 0;
    pTuner->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_Shutdown (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_Shutdown");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->Shutdown)) {
        result = pTuner->pTunerTerrCable->Shutdown (pTuner->pTunerTerrCable);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pTuner->pTunerSat) && (pTuner->pTunerSat->Shutdown)) {
        result = pTuner->pTunerSat->Shutdown (pTuner->pTunerSat);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    pTuner->frequencyKHz = 0;
    pTuner->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_ReadRFLevel (cxdref_tuner_t * pTuner,
                                                         int32_t *pRFLevel)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_ReadRFLevel");

    if ((!pTuner) || (!pRFLevel)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (pTuner->system)) {
        if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->ReadRFLevel)) {
            result = pTuner->pTunerTerrCable->ReadRFLevel (pTuner->pTunerTerrCable, pRFLevel);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }
    } else if (CXDREF_DTV_SYSTEM_IS_SAT (pTuner->system)) {
        if ((pTuner->pTunerSat) && (pTuner->pTunerSat->ReadRFLevel)) {
            result = pTuner->pTunerSat->ReadRFLevel (pTuner->pTunerSat, pRFLevel);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxdref_tuner_sw_combined_CalcRFLevelFromAGC (cxdref_tuner_t * pTuner,
                                                                uint32_t agcValue,
                                                                int32_t *pRFLevel)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_CalcRFLevelFromAGC");

    if ((!pTuner) || (!pRFLevel)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (CXDREF_DTV_SYSTEM_IS_TERR_CABLE (pTuner->system)) {
        if ((pTuner->pTunerTerrCable) && (pTuner->pTunerTerrCable->CalcRFLevelFromAGC)) {
            result = pTuner->pTunerTerrCable->CalcRFLevelFromAGC (pTuner->pTunerTerrCable, agcValue, pRFLevel);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }
    } else if (CXDREF_DTV_SYSTEM_IS_SAT (pTuner->system)) {
        if ((pTuner->pTunerSat) && (pTuner->pTunerSat->CalcRFLevelFromAGC)) {
            result = pTuner->pTunerSat->CalcRFLevelFromAGC (pTuner->pTunerSat, agcValue, pRFLevel);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_tuner_sw_combined_Create (cxdref_tuner_t * pTuner,
                                             cxdref_tuner_t * pTunerTerrCable,
                                             cxdref_tuner_t * pTunerSat)
{
    CXDREF_TRACE_ENTER ("cxdref_tuner_sw_combined_Create");

    if (!pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    cxdref_memset (pTuner, 0, sizeof (cxdref_tuner_t));

    if (pTunerTerrCable) {
        pTuner->pTunerTerrCable = pTunerTerrCable;
        pTuner->rfLevelFuncTerr = pTunerTerrCable->rfLevelFuncTerr;
    }

    if (pTunerSat) {
        pTuner->pTunerSat = pTunerSat;
        pTuner->rfLevelFuncSat = pTunerSat->rfLevelFuncSat;
        pTuner->symbolRateKSpsForSpectrum = pTunerSat->symbolRateKSpsForSpectrum;
    }

    pTuner->Initialize = cxdref_tuner_sw_combined_Initialize;
    pTuner->TerrCableTune = cxdref_tuner_sw_combined_TerrCableTune;
    pTuner->SatTune = cxdref_tuner_sw_combined_SatTune;
    pTuner->Sleep = cxdref_tuner_sw_combined_Sleep;
    pTuner->Shutdown = cxdref_tuner_sw_combined_Shutdown;
    pTuner->ReadRFLevel = cxdref_tuner_sw_combined_ReadRFLevel;
    pTuner->CalcRFLevelFromAGC = cxdref_tuner_sw_combined_CalcRFLevelFromAGC;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

