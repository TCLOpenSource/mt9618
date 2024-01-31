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

#include "cxdref_common.h"
#include "cxdref_demod.h"
#include "cxdref_integ_dvbs_s2.h"
#include "cxdref_demod_dvbs_s2_monitor.h"

cxdref_result_t cxdref_integ_dvbs_s2_Tune (cxdref_integ_t * pInteg,
                                       cxdref_dvbs_s2_tune_param_t * pTuneParam)
{
    cxdref_stopwatch_t stopwatch;
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dtv_system_t dtvSystem = CXDREF_DTV_SYSTEM_DVBS2;
    uint32_t elapsedTime = 0;
    uint8_t isContinue = 1;
    uint32_t timeout = 0;
    cxdref_demod_lock_result_t lockStatus = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbs_s2_Tune");

    if ((!pInteg) || (!pInteg->pDemod) ||
        (!pTuneParam) || (pTuneParam->symbolRateKSps == 0)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) &&
        (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbs_s2_Tune (pInteg->pDemod, pTuneParam);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (pTuneParam->system == CXDREF_DTV_SYSTEM_ANY){
        dtvSystem = CXDREF_DTV_SYSTEM_DVBS;
    } else {
        dtvSystem = pTuneParam->system;
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->SatTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->SatTune (pInteg->pTuner,
                                          pTuneParam->centerFreqKHz,
                                          dtvSystem,
                                          pTuneParam->symbolRateKSps);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_TuneEnd(pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_stopwatch_start (&stopwatch);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (pTuneParam->system == CXDREF_DTV_SYSTEM_ANY){
        timeout = ((3600000 + (pTuneParam->symbolRateKSps - 1)) / pTuneParam->symbolRateKSps) + 150;
    } else {
        timeout = ((3000000 + (pTuneParam->symbolRateKSps - 1)) / pTuneParam->symbolRateKSps) + 150;
    }

    isContinue = 1;
    while (isContinue) {
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_dvbs_s2_CheckTSLock (pInteg->pDemod, &lockStatus);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (lockStatus == CXDREF_DEMOD_LOCK_RESULT_LOCKED) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }

        if (elapsedTime > timeout) {
            isContinue = 0;
        } else {
            result = cxdref_stopwatch_sleep (&stopwatch, CXDREF_INTEG_DVBS_S2_TUNE_POLLING_INTERVAL);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        }
    }

    {
        uint8_t plscLock = 0;
        uint8_t pilotOn = 0;
        result = cxdref_demod_dvbs_s2_monitor_Pilot (pInteg->pDemod, &plscLock, &pilotOn);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
        if ((plscLock != 0) && (pilotOn == 0)) {
            result = cxdref_stopwatch_start (&stopwatch);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }

            if (pTuneParam->system == CXDREF_DTV_SYSTEM_ANY){
                timeout = ((3600000 + (pTuneParam->symbolRateKSps - 1)) / pTuneParam->symbolRateKSps) + 150;
            } else {
                timeout = ((3000000 + (pTuneParam->symbolRateKSps - 1)) / pTuneParam->symbolRateKSps) + 150;
            }

            isContinue = 1;
            while (isContinue) {
                result = cxdref_integ_CheckCancellation (pInteg);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (result);
                }

                result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
                if (result != CXDREF_RESULT_OK){
                    CXDREF_TRACE_RETURN (result);
                }

                result = cxdref_demod_dvbs_s2_CheckTSLock (pInteg->pDemod, &lockStatus);
                if (result != CXDREF_RESULT_OK){
                    CXDREF_TRACE_RETURN (result);
                }

                if (lockStatus == CXDREF_DEMOD_LOCK_RESULT_LOCKED) {
                    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
                }

                if (elapsedTime > timeout) {
                    isContinue = 0;
                } else {
                    result = cxdref_stopwatch_sleep (&stopwatch, CXDREF_INTEG_DVBS_S2_TUNE_POLLING_INTERVAL);
                    if (result != CXDREF_RESULT_OK){
                        CXDREF_TRACE_RETURN (result);
                    }
                }
            }
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
}

cxdref_result_t cxdref_integ_dvbs_s2_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbs_s2_monitor_RFLevel");

    if ((!pInteg) || (!pInteg->pDemod) || (!pRFLeveldB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->system != CXDREF_DTV_SYSTEM_DVBS) &&
        (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_DVBS2) &&
        (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_ANY)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!pInteg->pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pInteg->pTuner->rfLevelFuncSat == CXDREF_TUNER_RFLEVEL_FUNC_READ && pInteg->pTuner->ReadRFLevel) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->ReadRFLevel (pInteg->pTuner, pRFLeveldB);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else if (pInteg->pTuner->rfLevelFuncSat == CXDREF_TUNER_RFLEVEL_FUNC_CALCFROMAGC
        && pInteg->pTuner->CalcRFLevelFromAGC) {
        uint32_t ifAgc;

        result = cxdref_demod_dvbs_s2_monitor_IFAGCOut (pInteg->pDemod, &ifAgc);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->CalcRFLevelFromAGC (pInteg->pTuner, ifAgc, pRFLeveldB);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    *pRFLeveldB *= 10;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
