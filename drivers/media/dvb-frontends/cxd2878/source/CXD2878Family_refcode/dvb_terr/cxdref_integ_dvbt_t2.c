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
#include "cxdref_integ_dvbt_t2.h"
#include "cxdref_demod.h"
#include "cxdref_demod_dvbt_monitor.h"
#include "cxdref_demod_dvbt2_monitor.h"

cxdref_result_t cxdref_integ_dvbt_t2_Scan(cxdref_integ_t * pInteg,
                                      cxdref_integ_dvbt_t2_scan_param_t * pScanParam,
                                      cxdref_integ_dvbt_t2_scan_callback_t callBack)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_dvbt_t2_scan_result_t scanResult;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt_t2_Scan");

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

    if ((pScanParam->system != CXDREF_DTV_SYSTEM_DVBT2) && (pScanParam->bandwidth == CXDREF_DTV_BW_1_7_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    scanResult.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.tuneResult = CXDREF_RESULT_OK;
    scanResult.system = CXDREF_DTV_SYSTEM_DVBT;
    scanResult.dvbtTuneParam.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.dvbtTuneParam.bandwidth = pScanParam->bandwidth;
    scanResult.dvbtTuneParam.profile = CXDREF_DVBT_PROFILE_HP;
    scanResult.dvbt2TuneParam.centerFreqKHz = pScanParam->startFrequencyKHz;
    scanResult.dvbt2TuneParam.bandwidth = pScanParam->bandwidth;
    scanResult.dvbt2TuneParam.dataPlpId = 0;
    scanResult.dvbt2TuneParam.profile = CXDREF_DVBT2_PROFILE_BASE;
    scanResult.dvbt2TuneParam.tuneInfo = CXDREF_DEMOD_DVBT2_TUNE_INFO_OK;

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x01);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    while (scanResult.centerFreqKHz <= pScanParam->endFrequencyKHz) {
        cxdref_dtv_system_t systemFound = CXDREF_DTV_SYSTEM_UNKNOWN;
        cxdref_dvbt2_profile_t profileFound = CXDREF_DVBT2_PROFILE_ANY;
        uint8_t channelComplete = 0;
        cxdref_dtv_system_t blindTuneSystem = pScanParam->system;
        cxdref_dvbt2_profile_t blindTuneProfile = pScanParam->t2Profile;

        scanResult.dvbtTuneParam.centerFreqKHz = scanResult.centerFreqKHz;
        scanResult.dvbt2TuneParam.centerFreqKHz = scanResult.centerFreqKHz;

        while (!channelComplete) {
            scanResult.tuneResult = cxdref_integ_dvbt_t2_BlindTune(pInteg, scanResult.centerFreqKHz, pScanParam->bandwidth, blindTuneSystem, blindTuneProfile, &systemFound, &profileFound);
            switch (scanResult.tuneResult) {
            case CXDREF_RESULT_OK:
                scanResult.system = systemFound;

                if (systemFound == CXDREF_DTV_SYSTEM_DVBT){
                    callBack (pInteg, &scanResult, pScanParam);

                    channelComplete = 1;
                }
                else if (systemFound == CXDREF_DTV_SYSTEM_DVBT2) {
                    uint8_t numPLPs = 0;
                    uint8_t plpIds[255];
                    uint8_t mixed;
                    uint8_t i;

                    scanResult.dvbt2TuneParam.profile = profileFound;

                    result = cxdref_integ_dvbt2_Scan_PrepareDataPLPLoop(pInteg, plpIds, &numPLPs, &mixed);
                    if (result == CXDREF_RESULT_ERROR_HW_STATE) {
                        scanResult.system = CXDREF_DTV_SYSTEM_UNKNOWN;
                        scanResult.tuneResult = CXDREF_RESULT_ERROR_UNLOCK;
                        callBack (pInteg, &scanResult, pScanParam);

                        channelComplete = 1;
                    }
                    else if (result != CXDREF_RESULT_OK) {
                        cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);
                        CXDREF_TRACE_RETURN (result);
                    }
                    else {
                        scanResult.dvbt2TuneParam.dataPlpId = plpIds[0];
                        callBack (pInteg, &scanResult, pScanParam);

                        for (i = 1; i < numPLPs; i++) {
                            result = cxdref_integ_dvbt2_Scan_SwitchDataPLP(pInteg, mixed, plpIds[i], profileFound);
                            if (result != CXDREF_RESULT_OK) {
                                cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);
                                CXDREF_TRACE_RETURN (result);
                            }
                            scanResult.dvbt2TuneParam.dataPlpId = plpIds[i];
                            callBack (pInteg, &scanResult, pScanParam);
                        }

                        if (blindTuneProfile == CXDREF_DVBT2_PROFILE_ANY) {
                            if (mixed) {
                                blindTuneSystem = CXDREF_DTV_SYSTEM_DVBT2;

                                if (profileFound == CXDREF_DVBT2_PROFILE_BASE) {
                                    blindTuneProfile = CXDREF_DVBT2_PROFILE_LITE;
                                }
                                else if (profileFound == CXDREF_DVBT2_PROFILE_LITE) {
                                    blindTuneProfile = CXDREF_DVBT2_PROFILE_BASE;
                                }
                                else {
                                    cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);
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
                scanResult.system = CXDREF_DTV_SYSTEM_UNKNOWN;
                callBack (pInteg, &scanResult, pScanParam);

                channelComplete = 1;
                break;

            default:
                {
                    cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);
                    CXDREF_TRACE_RETURN (scanResult.tuneResult);
                }
            }
        }

        scanResult.centerFreqKHz += pScanParam->stepFrequencyKHz;

        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_terr_cable_SetScanMode(pInteg->pDemod, pScanParam->system, 0x00);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_dvbt_t2_BlindTune(cxdref_integ_t * pInteg,
                                           uint32_t centerFreqKHz,
                                           cxdref_dtv_bandwidth_t bandwidth,
                                           cxdref_dtv_system_t system,
                                           cxdref_dvbt2_profile_t profile,
                                           cxdref_dtv_system_t * pSystemTuned,
                                           cxdref_dvbt2_profile_t * pProfileTuned)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t channelFound = 0;
    cxdref_dtv_system_t tuneSystems[2] = {CXDREF_DTV_SYSTEM_UNKNOWN , CXDREF_DTV_SYSTEM_UNKNOWN};
    uint8_t tuneIteration;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt_t2_BlindTune");

    if ((!pInteg) || (!pInteg->pDemod) || (!pSystemTuned) || (!pProfileTuned)) {
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

    if ((system != CXDREF_DTV_SYSTEM_DVBT) && (system != CXDREF_DTV_SYSTEM_DVBT2) && (system != CXDREF_DTV_SYSTEM_ANY)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if ((system != CXDREF_DTV_SYSTEM_DVBT2) && (bandwidth == CXDREF_DTV_BW_1_7_MHZ)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (system == CXDREF_DTV_SYSTEM_ANY) {
        tuneSystems[0] = pInteg->pDemod->blindTuneDvbt2First? CXDREF_DTV_SYSTEM_DVBT2 : CXDREF_DTV_SYSTEM_DVBT;
        tuneSystems[1] = pInteg->pDemod->blindTuneDvbt2First? CXDREF_DTV_SYSTEM_DVBT : CXDREF_DTV_SYSTEM_DVBT2;
    }
    else {
        tuneSystems[0] = system;
    }

    for (tuneIteration = 0; tuneIteration <=1; tuneIteration++) {
#ifdef SAME_TUNER_CONFIG_DVBT_T2
        cxdref_result_t (*SavedTuneFunc) (struct cxdref_tuner_t *, uint32_t,
                                        cxdref_dtv_system_t, cxdref_dtv_bandwidth_t) = NULL;

        if (tuneIteration == 1)
        {
            SavedTuneFunc = pInteg->pTuner->TerrCableTune;
            pInteg->pTuner->TerrCableTune = NULL;
        }

#endif
        if ((tuneSystems[tuneIteration] == CXDREF_DTV_SYSTEM_DVBT) && (!channelFound)) {
            result = cxdref_integ_dvbt_BlindTune(pInteg, centerFreqKHz, bandwidth);
            if (result == CXDREF_RESULT_OK) {
                *pSystemTuned = CXDREF_DTV_SYSTEM_DVBT;
                channelFound = 1;
            }
        }
        if ((tuneSystems[tuneIteration] == CXDREF_DTV_SYSTEM_DVBT2) && (!channelFound)) {
            result = cxdref_integ_dvbt2_BlindTune(pInteg, centerFreqKHz, bandwidth, profile, pProfileTuned);
            if (result == CXDREF_RESULT_OK) {
                *pSystemTuned = CXDREF_DTV_SYSTEM_DVBT2;
                channelFound = 1;
            }
        }

#ifdef SAME_TUNER_CONFIG_DVBT_T2
        if ((tuneIteration == 1)  && (SavedTuneFunc))
        {
            pInteg->pTuner->TerrCableTune = SavedTuneFunc;
        }
#endif

    }
    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_dvbt_t2_monitor_RFLevel (cxdref_integ_t * pInteg, int32_t * pRFLeveldB)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_integ_dvbt_t2_monitor_RFLevel");

    if ((!pInteg) || (!pInteg->pDemod) || (!pRFLeveldB)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pInteg->pDemod->system == CXDREF_DTV_SYSTEM_DVBT) {
        result = cxdref_integ_dvbt_monitor_RFLevel (pInteg, pRFLeveldB);
    }
    else if (pInteg->pDemod->system == CXDREF_DTV_SYSTEM_DVBT2) {
        result = cxdref_integ_dvbt2_monitor_RFLevel (pInteg, pRFLeveldB);
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    CXDREF_TRACE_RETURN (result);
}
