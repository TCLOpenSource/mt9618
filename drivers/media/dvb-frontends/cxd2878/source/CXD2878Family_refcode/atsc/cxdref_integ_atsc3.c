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

#include "cxdref_integ.h"
#include "cxdref_integ_atsc3.h"
#include "cxdref_demod.h"
#include "cxdref_demod_atsc3_monitor.h"

static cxdref_result_t atsc3_WaitDemodLock (cxdref_integ_t * pInteg);
static cxdref_result_t atsc3_WaitCWTracking (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_atsc3_Tune (cxdref_integ_t * pInteg,
                                     cxdref_atsc3_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_Tune");

    if ((!pInteg) || (!pTuneParam) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pTuneParam->bandwidth != CXDREF_DTV_BW_6_MHZ) && (pTuneParam->bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (pTuneParam->bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    result = cxdref_demod_atsc3_AutoDetectSeq_Init (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    if ((pInteg->pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)
        || (pInteg->pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        result = cxdref_demod_atsc3_chbond_SetPLPConfig (pInteg->pDemod, pTuneParam->plpIDNum, pTuneParam->plpID, pTuneParam->plpBond);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else
#endif
    {
        result = cxdref_demod_atsc3_SetPLPConfig (pInteg->pDemod, pTuneParam->plpIDNum, pTuneParam->plpID);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_atsc3_Tune (pInteg->pDemod, pTuneParam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->TerrCableTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->TerrCableTune (pInteg->pTuner, pTuneParam->centerFreqKHz, CXDREF_DTV_SYSTEM_ATSC3, pTuneParam->bandwidth);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_TuneEnd (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = atsc3_WaitDemodLock (pInteg);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = atsc3_WaitCWTracking (pInteg);
    if ((result != CXDREF_RESULT_OK) && (result != CXDREF_RESULT_ERROR_TIMEOUT)) {
        CXDREF_TRACE_RETURN (result);
    }

    if (!pInteg->pDemod->scanMode) {
        result = cxdref_integ_atsc3_WaitALPLock (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_atsc3_EASTune (cxdref_integ_t * pInteg,
                                        cxdref_atsc3_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_EASTune");

    if ((!pInteg) || (!pTuneParam) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pTuneParam->bandwidth != CXDREF_DTV_BW_6_MHZ) && (pTuneParam->bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (pTuneParam->bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    result = cxdref_demod_atsc3_AutoDetectSeq_Init (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc3_EASTune (pInteg->pDemod, pTuneParam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->TerrCableTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->TerrCableTune (pInteg->pTuner, pTuneParam->centerFreqKHz, CXDREF_DTV_SYSTEM_ATSC3, pTuneParam->bandwidth);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x00);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_atsc3_EASTuneEnd (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = atsc3_WaitDemodLock (pInteg);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = atsc3_WaitCWTracking (pInteg);
    if ((result != CXDREF_RESULT_OK) && (result != CXDREF_RESULT_ERROR_TIMEOUT)) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_atsc3_Scan (cxdref_integ_t * pInteg,
                                     cxdref_integ_atsc3_scan_param_t * pScanParam,
                                     cxdref_integ_atsc3_scan_callback_t callBack)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_atsc3_scan_result_t scanResult;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_Scan");

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

    if ((pScanParam->bandwidth != CXDREF_DTV_BW_6_MHZ) && (pScanParam->bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (pScanParam->bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    scanResult.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.tuneResult = CXDREF_RESULT_OK;
    scanResult.tuneParam.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.tuneParam.bandwidth = pScanParam->bandwidth;
    scanResult.tuneParam.plpIDNum = 1;
    scanResult.tuneParam.plpID[0] = 0;
    scanResult.tuneParam.plpID[1] = 0;
    scanResult.tuneParam.plpID[2] = 0;
    scanResult.tuneParam.plpID[3] = 0;
#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    scanResult.tuneParam.plpBond[0] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE;
    scanResult.tuneParam.plpBond[1] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE;
    scanResult.tuneParam.plpBond[2] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE;
    scanResult.tuneParam.plpBond[3] = CXDREF_DEMOD_ATSC3_CHBOND_PLP_BOND_ENABLE;
#endif

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC3, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    while (scanResult.centerFreqKHz <= pScanParam->endFrequencyKHz) {
        scanResult.tuneParam.centerFreqKHz = scanResult.centerFreqKHz;
        scanResult.tuneResult = cxdref_integ_atsc3_Tune(pInteg, &scanResult.tuneParam);
        switch (scanResult.tuneResult) {
        case CXDREF_RESULT_OK:
            callBack (pInteg, &scanResult, pScanParam);
            break;
        case CXDREF_RESULT_ERROR_UNLOCK:
        case CXDREF_RESULT_ERROR_TIMEOUT:
            callBack (pInteg, &scanResult, pScanParam);
            break;
        default:
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC3, 0x00);
            CXDREF_TRACE_RETURN (scanResult.tuneResult);
        }

        scanResult.centerFreqKHz += pScanParam->stepFrequencyKHz;

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC3, 0x00);
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_ATSC3, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_atsc3_SwitchPLP (cxdref_integ_t * pInteg,
                                          uint8_t plpIDNum,
                                          uint8_t plpID[4])
{
    cxdref_result_t result;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_SwitchPLP");

    if ((!pInteg) || (!pInteg->pDemod) || (plpIDNum > 4)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    if ((pInteg->pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)
        || (pInteg->pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }
#endif

    result = cxdref_demod_atsc3_SetPLPConfig (pInteg->pDemod, plpIDNum, plpID);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_integ_atsc3_WaitALPLock (pInteg);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_atsc3_WaitALPLock (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;
    cxdref_demod_lock_result_t lock;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_WaitALPLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (;;) {
        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= CXDREF_ATSC3_WAIT_ALP_LOCK) {
            continueWait = 0;
        }

        result = cxdref_demod_atsc3_CheckALPLock (pInteg->pDemod, &lock);
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

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC3_WAIT_LOCK_INTERVAL);
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

cxdref_result_t cxdref_integ_atsc3_monitor_RFLevel (cxdref_integ_t * pInteg,
                                                int32_t * pRFLeveldB)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_atsc3_monitor_RFLevel");

    if ((!pInteg) || (!pInteg->pDemod) || (!pRFLeveldB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_ATSC3)  {
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

        result = cxdref_demod_atsc3_monitor_IFAGCOut (pInteg->pDemod, &ifAgc);
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

static cxdref_result_t atsc3_WaitDemodLock (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_lock_result_t lock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    uint16_t timeout = 0;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;

    CXDREF_TRACE_ENTER ("atsc3_WaitDemodLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    timeout = CXDREF_ATSC3_WAIT_DEMOD_LOCK;

    for (;;) {
        result = cxdref_stopwatch_elapsed (&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= timeout) {
            continueWait = 0;
        }

        result = cxdref_demod_atsc3_CheckDemodLock (pInteg->pDemod, &lock);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        switch (lock) {
        case CXDREF_DEMOD_LOCK_RESULT_LOCKED:
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);

        case CXDREF_DEMOD_LOCK_RESULT_UNLOCKED:
            result = cxdref_demod_atsc3_AutoDetectSeq_UnlockCase (pInteg->pDemod, &continueWait);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            if (!continueWait) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_UNLOCK);
            }
            break;
        default:
            break;
        }

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC3_WAIT_LOCK_INTERVAL);
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

static cxdref_result_t atsc3_WaitCWTracking (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint16_t timeout = 0;
    cxdref_stopwatch_t timer;
    uint8_t completed = 1;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;

    CXDREF_TRACE_ENTER ("atsc3_WaitCWTracking");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    timeout = CXDREF_ATSC3_WAIT_CW_TRACKING_TIMEOUT;

    for (;;) {
        result = cxdref_stopwatch_elapsed (&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= timeout) {
            continueWait = 0;
        }

        result = cxdref_demod_atsc3_AutoDetectSeq_SetCWTracking (pInteg->pDemod, &completed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        if (completed) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_ATSC3_WAIT_LOCK_INTERVAL);
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
