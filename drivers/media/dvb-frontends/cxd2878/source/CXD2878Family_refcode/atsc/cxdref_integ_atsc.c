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

#include "cxdref_integ_atsc.h"
#include "cxdref_demod_atsc_monitor.h"

static cxdref_result_t atsc_WaitVQLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_atsc_Tune (cxdref_integ_t * pInteg,
                                    cxdref_atsc_tune_param_t * pTuneParam)
{
    cxdref_result_t result;
    CXDREF_TRACE_ENTER ("cxdref_integ_atsc_Tune");

    if ((!pInteg) || (!pTuneParam) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_atsc_Tune (pInteg->pDemod, pTuneParam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->TerrCableTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->TerrCableTune (pInteg->pTuner, pTuneParam->centerFreqKHz, CXDREF_DTV_SYSTEM_ATSC, CXDREF_DTV_BW_6_MHZ);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_atsc_TuneEnd(pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (!pInteg->pDemod->scanMode) {
        result = cxdref_integ_atsc_WaitTSLock(pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = atsc_WaitVQLock(pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_atsc_Scan (cxdref_integ_t * pInteg,
                                    cxdref_integ_atsc_scan_param_t * pScanParam,
                                    cxdref_integ_atsc_scan_callback_t callBack)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_atsc_scan_result_t scanResult;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc_Scan");

    if ((!pInteg) || (!pScanParam) || (!callBack) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pScanParam->endFrequencyKHz < pScanParam->startFrequencyKHz) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pScanParam->stepFrequencyKHz == 0) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    scanResult.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.tuneResult = CXDREF_RESULT_OK;
    scanResult.tuneParam.centerFreqKHz = pScanParam->startFrequencyKHz;

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    while (scanResult.centerFreqKHz <= pScanParam->endFrequencyKHz) {
        scanResult.tuneParam.centerFreqKHz = scanResult.centerFreqKHz;
        scanResult.tuneResult = cxdref_integ_atsc_Tune(pInteg, &scanResult.tuneParam);
        switch (scanResult.tuneResult) {
        case CXDREF_RESULT_OK:
            callBack (pInteg, &scanResult, pScanParam);
            break;

        case CXDREF_RESULT_ERROR_UNLOCK:
        case CXDREF_RESULT_ERROR_TIMEOUT:
            callBack (pInteg, &scanResult, pScanParam);
            break;

        default:
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC, 0x00);
            CXDREF_TRACE_RETURN (scanResult.tuneResult);
        }

        scanResult.centerFreqKHz += pScanParam->stepFrequencyKHz;

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC, 0x00);
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_atsc_WaitTSLock (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_lock_result_t lock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;
    uint32_t waitTimeOut = 0;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc_WaitTSLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
    
    waitTimeOut = CXDREF_ATSC_WAIT_TS_FAST_UNLOCK;

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (;;) {
        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= waitTimeOut) {
            continueWait = 0;
        }

        result = cxdref_demod_atsc_CheckTSLock (pInteg->pDemod, &lock);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        switch (lock) {
        case CXDREF_DEMOD_LOCK_RESULT_LOCKED:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);

        case CXDREF_DEMOD_LOCK_RESULT_UNLOCKED:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_UNLOCK);

        default:
            break;
        }
        
        if ((waitTimeOut != CXDREF_ATSC_WAIT_TS_LOCK) && (elapsed >= 40)) {
        	uint8_t segmentlock;
        	result = cxdref_demod_atsc_custom_monitor_SegmentSync (pInteg->pDemod, &segmentlock);
        	if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        	result = cxdref_demod_atsc_CheckDemodLock (pInteg->pDemod, &lock);
        	if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            if((segmentlock == 0x01) || (lock == 0x01)){
            	waitTimeOut = CXDREF_ATSC_WAIT_TS_LOCK;
            }
        }

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC_WAIT_LOCK_INTERVAL);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            result = CXDREF_RESULT_ERROR_TIMEOUT;
            break;
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_atsc_monitor_RFLevel (cxdref_integ_t * pInteg,
                                               int32_t * pRFLeveldB)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc_WaitTSLock");

    if ((!pInteg) || (!pInteg->pDemod) || (!pRFLeveldB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_ATSC)  {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!pInteg->pTuner) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pInteg->pTuner->rfLevelFuncTerr == CXDREF_TUNER_RFLEVEL_FUNC_READ && pInteg->pTuner->ReadRFLevel) {
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
    } else if (pInteg->pTuner->rfLevelFuncTerr == CXDREF_TUNER_RFLEVEL_FUNC_CALCFROMAGC
        && pInteg->pTuner->CalcRFLevelFromAGC) {
        uint32_t ifAgc;

        result = cxdref_demod_atsc_monitor_IFAGCOut (pInteg->pDemod, &ifAgc);
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

static cxdref_result_t atsc_WaitVQLock (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_lock_result_t lock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;
    uint32_t waitTimeOut = 0;
    CXDREF_TRACE_ENTER ("atsc_WaitVQLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    waitTimeOut = CXDREF_ATSC_WAIT_DEMOD_FAST_UNLOCK;

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (;;) {
        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= waitTimeOut) {
            continueWait = 0;
        }

        result = cxdref_demod_atsc_CheckDemodLock (pInteg->pDemod, &lock);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        switch (lock) {
        case CXDREF_DEMOD_LOCK_RESULT_LOCKED:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);

        case CXDREF_DEMOD_LOCK_RESULT_UNLOCKED:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_UNLOCK);

        default:
            break;
        }

        if ((waitTimeOut != CXDREF_ATSC_WAIT_DEMOD_LOCK) && (elapsed >= 40)) {
        	uint8_t segmentlock;
        	result = cxdref_demod_atsc_custom_monitor_SegmentSync (pInteg->pDemod, &segmentlock);
        	if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        	result = cxdref_demod_atsc_CheckDemodLock (pInteg->pDemod, &lock);
        	if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            if((segmentlock == 0x01) || (lock == 0x01)){
            	waitTimeOut = CXDREF_ATSC_WAIT_DEMOD_LOCK;
            }
        }
        
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC_WAIT_LOCK_INTERVAL);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        else {
            result = CXDREF_RESULT_ERROR_TIMEOUT;
            break;
        }
    }

    CXDREF_TRACE_RETURN (result);
}
