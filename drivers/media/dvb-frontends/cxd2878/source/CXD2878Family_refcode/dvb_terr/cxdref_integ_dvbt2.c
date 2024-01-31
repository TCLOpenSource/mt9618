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
#include "cxdref_integ_dvbt2.h"
#include "cxdref_demod.h"
#include "cxdref_demod_dvbt2_monitor.h"

static cxdref_result_t dvbt2_WaitDemodLock (cxdref_integ_t * pInteg, cxdref_dvbt2_profile_t profile);

static cxdref_result_t dvbt2_WaitL1PostLock (cxdref_integ_t * pInteg);

cxdref_result_t cxdref_integ_dvbt2_Tune(cxdref_integ_t * pInteg,
                                    cxdref_dvbt2_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_Tune");

    if ((!pInteg) || (!pTuneParam) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pTuneParam->bandwidth != CXDREF_DTV_BW_1_7_MHZ) && (pTuneParam->bandwidth != CXDREF_DTV_BW_5_MHZ) &&
        (pTuneParam->bandwidth != CXDREF_DTV_BW_6_MHZ) && (pTuneParam->bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (pTuneParam->bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if ((pTuneParam->profile != CXDREF_DVBT2_PROFILE_BASE) && (pTuneParam->profile != CXDREF_DVBT2_PROFILE_LITE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_dvbt2_SetPLPConfig (pInteg->pDemod, 0x00, pTuneParam->dataPlpId);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_SetProfile(pInteg->pDemod, pTuneParam->profile);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_Tune (pInteg->pDemod, pTuneParam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->TerrCableTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->TerrCableTune (pInteg->pTuner, pTuneParam->centerFreqKHz, CXDREF_DTV_SYSTEM_DVBT2, pTuneParam->bandwidth);
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

    result = dvbt2_WaitDemodLock (pInteg, pTuneParam->profile);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    
    result = cxdref_integ_dvbt2_WaitTSLock (pInteg, pTuneParam->profile);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = dvbt2_WaitL1PostLock (pInteg);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    {
        uint8_t plpNotFound;

        result = cxdref_demod_dvbt2_monitor_DataPLPError (pInteg->pDemod, &plpNotFound);
        if (result == CXDREF_RESULT_ERROR_HW_STATE) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_UNLOCK);
        }
        else if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (plpNotFound) {
            result = CXDREF_RESULT_OK_CONFIRM;
            pTuneParam->tuneInfo = CXDREF_DEMOD_DVBT2_TUNE_INFO_INVALID_PLP_ID;
        }
        else {
            pTuneParam->tuneInfo = CXDREF_DEMOD_DVBT2_TUNE_INFO_OK;
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_integ_dvbt2_Scan(cxdref_integ_t * pInteg,
                                    cxdref_integ_dvbt2_scan_param_t * pScanParam,
                                    cxdref_integ_dvbt2_scan_callback_t callBack)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_dvbt2_scan_result_t scanResult;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_Scan");

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

    if ((pScanParam->bandwidth != CXDREF_DTV_BW_1_7_MHZ) && (pScanParam->bandwidth != CXDREF_DTV_BW_5_MHZ) &&
        (pScanParam->bandwidth != CXDREF_DTV_BW_6_MHZ) && (pScanParam->bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (pScanParam->bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    scanResult.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.tuneResult = CXDREF_RESULT_OK;
    scanResult.dvbt2TuneParam.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.dvbt2TuneParam.bandwidth = pScanParam->bandwidth;
    scanResult.dvbt2TuneParam.dataPlpId = 0;
    scanResult.dvbt2TuneParam.profile = CXDREF_DVBT2_PROFILE_BASE;
    scanResult.dvbt2TuneParam.tuneInfo = CXDREF_DEMOD_DVBT2_TUNE_INFO_OK;

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    while (scanResult.centerFreqKHz <= pScanParam->endFrequencyKHz) {
        cxdref_dvbt2_profile_t profileFound = CXDREF_DVBT2_PROFILE_ANY;
        uint8_t channelComplete = 0;
        cxdref_dvbt2_profile_t blindTuneProfile = pScanParam->t2Profile;

        scanResult.dvbt2TuneParam.centerFreqKHz = scanResult.centerFreqKHz;

        while (!channelComplete) {
            scanResult.tuneResult = cxdref_integ_dvbt2_BlindTune(pInteg, scanResult.centerFreqKHz, pScanParam->bandwidth, blindTuneProfile, &profileFound);
            switch (scanResult.tuneResult) {
            case CXDREF_RESULT_OK:
                {
                    uint8_t numPLPs = 0;
                    uint8_t plpIds[255];
                    uint8_t mixed;
                    uint8_t i;

                    scanResult.dvbt2TuneParam.profile = profileFound;

                    result = cxdref_integ_dvbt2_Scan_PrepareDataPLPLoop(pInteg, plpIds, &numPLPs, &mixed);
                    if (result == CXDREF_RESULT_ERROR_HW_STATE) {
                        scanResult.tuneResult = CXDREF_RESULT_ERROR_UNLOCK;
                        callBack (pInteg, &scanResult, pScanParam);

                        channelComplete = 1;
                    }
                    else if (result != CXDREF_RESULT_OK) {
                        cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);
                        CXDREF_TRACE_RETURN (result);
                    }
                    else {
                        scanResult.dvbt2TuneParam.dataPlpId = plpIds[0];
                        callBack (pInteg, &scanResult, pScanParam);

                        for (i = 1; i < numPLPs; i++) {
                            result = cxdref_integ_dvbt2_Scan_SwitchDataPLP(pInteg, mixed, plpIds[i], profileFound);
                            if (result != CXDREF_RESULT_OK) {
                                cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);
                                CXDREF_TRACE_RETURN (result);
                            }
                            scanResult.dvbt2TuneParam.dataPlpId = plpIds[i];
                            callBack (pInteg, &scanResult, pScanParam);
                        }

                        if (blindTuneProfile == CXDREF_DVBT2_PROFILE_ANY) {
                            if (mixed) {
                                if (profileFound == CXDREF_DVBT2_PROFILE_BASE) {
                                    blindTuneProfile = CXDREF_DVBT2_PROFILE_LITE;
                                }
                                else if (profileFound == CXDREF_DVBT2_PROFILE_LITE) {
                                    blindTuneProfile = CXDREF_DVBT2_PROFILE_BASE;
                                }
                                else {
                                    cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);
                                    CXDREF_TRACE_RETURN (result);
                                }
                            }
                            else {
                                channelComplete = 1;
                            }
                        }
                        else {
                            channelComplete = 1;
                        }
                    }
                }
                break;

            case CXDREF_RESULT_ERROR_UNLOCK:
            case CXDREF_RESULT_ERROR_TIMEOUT:
                callBack (pInteg, &scanResult, pScanParam);

                channelComplete = 1;
                break;

            default:
                {
                    cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);
                    CXDREF_TRACE_RETURN (scanResult.tuneResult);
                }
            }
        }

        scanResult.centerFreqKHz += pScanParam->stepFrequencyKHz;

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, CXDREF_DTV_SYSTEM_DVBT2, 0x00);

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_integ_dvbt2_Scan_PrepareDataPLPLoop(cxdref_integ_t * pInteg, uint8_t pPLPIds[], uint8_t *pNumPLPs, uint8_t *pMixed)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_Scan_PrepareDataPLPLoop");

    if ((!pInteg) || (!pPLPIds) || (!pNumPLPs) || (!pMixed)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt2_monitor_DataPLPs (pInteg->pDemod, pPLPIds, pNumPLPs);
    if (result == CXDREF_RESULT_OK) {
        cxdref_dvbt2_ofdm_t ofdm;

        result = cxdref_demod_dvbt2_monitor_OFDM (pInteg->pDemod, &ofdm);
        if (result == CXDREF_RESULT_OK) {
            *pMixed = ofdm.mixed;
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_integ_dvbt2_Scan_SwitchDataPLP(cxdref_integ_t * pInteg, uint8_t mixed, uint8_t plpId, cxdref_dvbt2_profile_t profile)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint16_t plpAcquisitionTime = 0;
    cxdref_stopwatch_t timer;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_Scan_SwitchDataPLP");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (profile == CXDREF_DVBT2_PROFILE_ANY) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_dvbt2_SetPLPConfig (pInteg->pDemod, 0x00, plpId);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((profile == CXDREF_DVBT2_PROFILE_BASE) && (mixed)) {
        plpAcquisitionTime = 510;
    }
    else if ((profile == CXDREF_DVBT2_PROFILE_LITE) && (mixed)) {
        plpAcquisitionTime = 1260;
    }
    else {
        plpAcquisitionTime = 260;
    }

    result = cxdref_stopwatch_start (&timer);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    for (;;) {
        uint32_t elapsed;

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed(&timer, &elapsed);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (elapsed >= plpAcquisitionTime) {
            break;
        } else {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_DVBT2_WAIT_LOCK_INTERVAL);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_integ_dvbt2_BlindTune(cxdref_integ_t * pInteg,
                                         uint32_t centerFreqKHz,
                                         cxdref_dtv_bandwidth_t bandwidth,
                                         cxdref_dvbt2_profile_t profile,
                                         cxdref_dvbt2_profile_t * pProfileTuned)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_dvbt2_tune_param_t tuneParam;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_BlindTune");

    if ((!pInteg) || (!pInteg->pDemod) || (!pProfileTuned)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((bandwidth != CXDREF_DTV_BW_1_7_MHZ) && (bandwidth != CXDREF_DTV_BW_5_MHZ) &&
        (bandwidth != CXDREF_DTV_BW_6_MHZ) && (bandwidth != CXDREF_DTV_BW_7_MHZ) &&
        (bandwidth != CXDREF_DTV_BW_8_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    tuneParam.bandwidth = bandwidth;
    tuneParam.centerFreqKHz = centerFreqKHz;
    tuneParam.dataPlpId = 0;

    result = cxdref_demod_dvbt2_SetPLPConfig (pInteg->pDemod, 0x01, 0x00);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_SetProfile(pInteg->pDemod, profile);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_Tune (pInteg->pDemod, &tuneParam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((pInteg->pTuner) && (pInteg->pTuner->TerrCableTune)) {
        result = cxdref_demod_I2cRepeaterEnable (pInteg->pDemod, 0x01);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = pInteg->pTuner->TerrCableTune (pInteg->pTuner, tuneParam.centerFreqKHz, CXDREF_DTV_SYSTEM_DVBT2, tuneParam.bandwidth);
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

    result = dvbt2_WaitDemodLock (pInteg, profile);
    switch (result) {
    case CXDREF_RESULT_OK:
        result = dvbt2_WaitL1PostLock (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (profile == CXDREF_DVBT2_PROFILE_ANY) {
            result = cxdref_demod_dvbt2_monitor_Profile (pInteg->pDemod, pProfileTuned);
            if (result == CXDREF_RESULT_ERROR_HW_STATE) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_UNLOCK);
            }
            else if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
        else {
            *pProfileTuned = profile;
        }

        break;

    case CXDREF_RESULT_ERROR_TIMEOUT:
    case CXDREF_RESULT_ERROR_UNLOCK:
        break;

    default:
        CXDREF_TRACE_RETURN(result);
    }

    CXDREF_TRACE_RETURN (result);
}


cxdref_result_t cxdref_integ_dvbt2_WaitTSLock (cxdref_integ_t * pInteg, cxdref_dvbt2_profile_t profile)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_lock_result_t lock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    uint16_t timeout = 0;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_WaitTSLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (profile == CXDREF_DVBT2_PROFILE_BASE) {
        timeout = CXDREF_DVBT2_BASE_WAIT_TS_LOCK;
    }
    else if (profile == CXDREF_DVBT2_PROFILE_LITE) {
        timeout = CXDREF_DVBT2_LITE_WAIT_TS_LOCK;
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
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

        if (elapsed >= timeout) {
            continueWait = 0;
        }

        result = cxdref_demod_dvbt2_CheckTSLock (pInteg->pDemod, &lock);
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
            result = cxdref_stopwatch_sleep (&timer, CXDREF_DVBT2_WAIT_LOCK_INTERVAL);
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

cxdref_result_t cxdref_integ_dvbt2_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_monitor_RFLevel");

    if ((!pInteg) || (!pInteg->pDemod) || (!pRFLeveldB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
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

        result = cxdref_demod_dvbt2_monitor_IFAGCOut(pInteg->pDemod, &ifAgc);
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

cxdref_result_t cxdref_integ_dvbt2_monitor_SSI (cxdref_integ_t * pInteg, uint8_t * pSSI)
{
    int32_t rfLevel;
    cxdref_dvbt2_plp_constell_t qam;
    cxdref_dvbt2_plp_code_rate_t codeRate;
    int32_t prel;
    int32_t tempSSI = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    static const int32_t pRefdBm1000[4][8] = {
        {-96000, -95000, -94000, -93000, -92000, -92000, -98000, -97000},
        {-91000, -89000, -88000, -87000, -86000, -86000, -93000, -92000},
        {-86000, -85000, -83000, -82000, -81000, -80000, -89000, -88000},
        {-82000, -80000, -78000, -76000, -75000, -74000, -86000, -84000},
    };

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt2_monitor_SSI");

    if ((!pInteg) || (!pInteg->pDemod) || (!pSSI)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system != CXDREF_DTV_SYSTEM_DVBT2) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_dvbt2_monitor_RFLevel (pInteg, &rfLevel);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_monitor_QAM (pInteg->pDemod, CXDREF_DVBT2_PLP_DATA, &qam);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_dvbt2_monitor_CodeRate (pInteg->pDemod, CXDREF_DVBT2_PLP_DATA, &codeRate);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if ((codeRate > CXDREF_DVBT2_R2_5) || (qam > CXDREF_DVBT2_QAM256)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

    prel = rfLevel - pRefdBm1000[qam][codeRate];

    if (prel < -15000) {
        tempSSI = 0;
    }
    else if (prel < 0) {
        tempSSI = ((2 * (prel + 15000)) + 1500) / 3000;
    }
    else if (prel < 20000) {
        tempSSI = (((4 * prel) + 500) / 1000) + 10;
    }
    else if (prel < 35000) {
        tempSSI = (((2 * (prel - 20000)) + 1500) / 3000) + 90;
    }
    else {
        tempSSI = 100;
    }

    *pSSI = (tempSSI > 100)? 100 : (uint8_t)tempSSI;

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t dvbt2_WaitDemodLock (cxdref_integ_t * pInteg, cxdref_dvbt2_profile_t profile)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_lock_result_t lock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
    uint16_t timeout = 0;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;

    CXDREF_TRACE_ENTER ("dvbt2_WaitDemodLock");

    if ((!pInteg) || (!pInteg->pDemod)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (profile == CXDREF_DVBT2_PROFILE_BASE) {
        timeout = CXDREF_DVBT2_BASE_WAIT_DEMOD_LOCK;
    }
    else if ((profile == CXDREF_DVBT2_PROFILE_LITE) || (profile == CXDREF_DVBT2_PROFILE_ANY)) {
        timeout = CXDREF_DVBT2_LITE_WAIT_DEMOD_LOCK;
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
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

        if (elapsed >= timeout) {
            continueWait = 0;
        }

        result = cxdref_demod_dvbt2_CheckDemodLock (pInteg->pDemod, &lock);
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
            result = cxdref_stopwatch_sleep (&timer, CXDREF_DVBT2_WAIT_LOCK_INTERVAL);
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

static cxdref_result_t dvbt2_WaitL1PostLock (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_stopwatch_t timer;
    uint8_t continueWait = 1;
    uint32_t elapsed = 0;
    uint8_t l1PostValid;

    CXDREF_TRACE_ENTER ("dvbt2_WaitL1PostLock");

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

        if (elapsed >= CXDREF_DVBT2_L1POST_TIMEOUT) {
            continueWait = 0;
        }

        result = cxdref_demod_dvbt2_CheckL1PostValid (pInteg->pDemod, &l1PostValid);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (l1PostValid) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (continueWait) {
            result = cxdref_stopwatch_sleep (&timer, CXDREF_DVBT2_WAIT_LOCK_INTERVAL);
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
