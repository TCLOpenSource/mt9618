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

#include "cxdref_integ_sat_device_ctrl.h"
#include "cxdref_stdlib.h"

static cxdref_result_t WaitTransmit (cxdref_integ_t * pInteg,
                                   uint32_t interval,
                                   uint32_t timeout);

static cxdref_result_t WaitTransmitSW (cxdref_integ_t * pInteg,
                                     uint32_t timeout1,
                                     uint32_t timeout2,
                                     uint32_t timeout3);

static cxdref_result_t singlecableTunerInitialize (cxdref_tuner_t * pTuner);

static cxdref_result_t singlecableTunerTerrCableTune (cxdref_tuner_t * pTuner,
                                                    uint32_t centerFreqKHz,
                                                    cxdref_dtv_system_t system,
                                                    cxdref_dtv_bandwidth_t bandwidth);

static cxdref_result_t singlecableTunerSatTune (cxdref_tuner_t * pTuner,
                                              uint32_t centerFreqKHz,
                                              cxdref_dtv_system_t system,
                                              uint32_t symbolRateKSps);

static cxdref_result_t singlecableTunerSleep (cxdref_tuner_t * pTuner);

static cxdref_result_t singlecableTunerShutdown (cxdref_tuner_t * pTuner);

static cxdref_result_t singlecableTunerReadRFLevel (cxdref_tuner_t * pTuner,
                                                  int32_t * pRFLevel);

static cxdref_result_t singlecableTunerCalcRFLevelFromAGC (cxdref_tuner_t * pTuner,
                                                         uint32_t agcValue,
                                                         int32_t * pRFLevel);

cxdref_result_t cxdref_integ_sat_device_ctrl_Enable (cxdref_integ_t * pInteg, uint8_t enable)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_Enable");

    if ((!pInteg) || (!pInteg->pDemod)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_sat_device_ctrl_Enable (pInteg->pDemod, enable);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (enable) {
        if ((pInteg->pLnbc) && (pInteg->pLnbc->WakeUp)) {
            if (pInteg->pLnbc->state == CXDREF_LNBC_STATE_SLEEP) {
                result = pInteg->pLnbc->WakeUp (pInteg->pLnbc);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (result);
                }

                CXDREF_SLEEP (CXDREF_INTEG_SAT_DEVICE_CTRL_LNBC_WAKEUP_TIME_MS);
            }
        }
    } else {
        if ((pInteg->pLnbc) && (pInteg->pLnbc->Sleep)) {
            result = pInteg->pLnbc->Sleep (pInteg->pLnbc);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_SetVoltageTone (cxdref_integ_t * pInteg,
                                                         uint8_t isVoltageHigh,
                                                         uint8_t isContinuousToneOn)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_lnbc_voltage_t voltage;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_SetVoltageTone");

    if ((!pInteg) || (!pInteg->pDemod) ||
        (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO) ||
        (pInteg->pLnbc->voltage == CXDREF_LNBC_VOLTAGE_AUTO)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    voltage = isVoltageHigh ? CXDREF_LNBC_VOLTAGE_HIGH : CXDREF_LNBC_VOLTAGE_LOW;
    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, voltage);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, isContinuousToneOn);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitToneBurst (cxdref_integ_t * pInteg,
                                                            uint8_t isVoltageHigh,
                                                            cxdref_toneburst_mode_t toneBurstMode,
                                                            uint8_t isContinuousToneOn)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_lnbc_voltage_t voltage;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitToneBurst");

    if ((!pInteg) || (!pInteg->pDemod) ||
        (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO) ||
        (pInteg->pLnbc->voltage == CXDREF_LNBC_VOLTAGE_AUTO)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (toneBurstMode == CXDREF_TONEBURST_MODE_OFF){
        result =  cxdref_integ_sat_device_ctrl_SetVoltageTone (pInteg, isVoltageHigh, isContinuousToneOn);
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 0, NULL, 0, NULL, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, toneBurstMode);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 0, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    voltage = isVoltageHigh ? CXDREF_LNBC_VOLTAGE_HIGH : CXDREF_LNBC_VOLTAGE_LOW;
    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, voltage);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = WaitTransmit(pInteg, CXDREF_INTEG_DISEQC_TB_POL, 1000);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, isContinuousToneOn);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitDiseqcCommand (cxdref_integ_t * pInteg,
                                                                uint8_t isVoltageHigh,
                                                                cxdref_toneburst_mode_t toneBurstMode,
                                                                uint8_t isContinuousToneOn,
                                                                cxdref_diseqc_message_t * pCommand1,
                                                                uint8_t repeatCount1,
                                                                cxdref_diseqc_message_t * pCommand2,
                                                                uint8_t repeatCount2)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_lnbc_voltage_t voltage;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitDiseqcCommand");

    if ((!pInteg) || (!pInteg->pDemod) || (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage) ||
        (!pCommand1) || (repeatCount1 == 0) || (repeatCount1 >= 16) || (repeatCount2 >= 16)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO) ||
        (pInteg->pLnbc->voltage == CXDREF_LNBC_VOLTAGE_AUTO)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 1, pCommand1, repeatCount1, pCommand2, repeatCount2);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, toneBurstMode);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 0, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    voltage = isVoltageHigh ? CXDREF_LNBC_VOLTAGE_HIGH : CXDREF_LNBC_VOLTAGE_LOW;
    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, voltage);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = WaitTransmit(pInteg,
                          CXDREF_INTEG_DISEQC_DISEQC_TRANSMIT_POL,
                          (uint32_t)1000 * (repeatCount1 + repeatCount2));
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, isContinuousToneOn);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitDiseqcCommandWithReply (cxdref_integ_t * pInteg,
                                                                         uint8_t isVoltageHigh,
                                                                         cxdref_toneburst_mode_t toneBurstMode,
                                                                         uint8_t isContinuousToneOn,
                                                                         cxdref_diseqc_message_t * pCommand,
                                                                         cxdref_diseqc_message_t * pReply)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_lnbc_voltage_t voltage;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitDiseqcCommandWithReply");

    if ((!pInteg) || (!pInteg->pDemod) || (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage) ||
        (!pCommand) || (!pReply)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO) ||
        (pInteg->pLnbc->voltage == CXDREF_LNBC_VOLTAGE_AUTO)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMask (pInteg->pDemod, 0xE4, 0x03);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 1, pCommand, 1, NULL, 0);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, toneBurstMode);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetRxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_RXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC1_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 1, CXDREF_INTEG_SAT_DEVICE_CTRL_DISEQC2_RTO_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    voltage = isVoltageHigh ? CXDREF_LNBC_VOLTAGE_HIGH : CXDREF_LNBC_VOLTAGE_LOW;
    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, voltage);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    if (pInteg->pLnbc->transmitMode == CXDREF_LNBC_TRANSMIT_MODE_AUTO) {
        result = WaitTransmit(pInteg, CXDREF_INTEG_DISEQC_DISEQC_TRANSMIT_POL, 1400);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = WaitTransmitSW(pInteg, 500, 600, 1400);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, isContinuousToneOn);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_GetReplyMessage (pInteg->pDemod, pReply);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecableCommand (cxdref_integ_t * pInteg,
                                                                     cxdref_diseqc_message_t * pCommand)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_result_t tempResult = CXDREF_RESULT_OK;
    uint8_t enableTXEN = 0;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitSinglecableCommand");

    if ((!pInteg) || (!pInteg->pDemod) || (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage) || (!pCommand)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_GetTXENMode (pInteg->pDemod, &enableTXEN);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, CXDREF_TONEBURST_MODE_OFF);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 1, pCommand, 1, NULL, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 0, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    if (enableTXEN == 0){
        result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_HIGH);
        if (result != CXDREF_RESULT_OK){
            goto End;
        }
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = WaitTransmit (pInteg, CXDREF_INTEG_DISEQC_SINGLECABLE_TRANSMIT_POL, 1000);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

End:
    if (enableTXEN == 0){
        tempResult = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_LOW);
        if (tempResult != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (tempResult);
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecable2Command (cxdref_integ_t * pInteg,
                                                                      cxdref_diseqc_message_t * pCommand)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_result_t tempResult = CXDREF_RESULT_OK;
    uint8_t enableTXEN = 0;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitSinglecable2Command");

    if ((!pInteg) || (!pInteg->pDemod) || (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage) || (!pCommand)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_sat_device_ctrl_GetTXENMode (pInteg->pDemod, &enableTXEN);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    if (enableTXEN) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, CXDREF_TONEBURST_MODE_OFF);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 1, pCommand, 1, NULL, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 0, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_HIGH);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = WaitTransmit (pInteg, CXDREF_INTEG_DISEQC_SINGLECABLE_TRANSMIT_POL, 1000);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

End:
    tempResult = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_LOW);
    if (tempResult != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (tempResult);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_TransmitSinglecable2CommandWithReply (cxdref_integ_t * pInteg,
                                                                               cxdref_diseqc_message_t * pCommand,
                                                                               cxdref_diseqc_message_t * pReply)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_result_t tempResult = CXDREF_RESULT_OK;
    uint8_t enableTXEN = 0;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_TransmitSinglecable2CommandWithReply");

    if ((!pInteg) || (!pInteg->pDemod) || (!pInteg->pLnbc) || (!pInteg->pLnbc->SetVoltage) ||
        (!pCommand) || (!pReply)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pLnbc->tone != CXDREF_LNBC_TONE_AUTO){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((pInteg->pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pInteg->pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_sat_device_ctrl_GetTXENMode (pInteg->pDemod, &enableTXEN);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    if (enableTXEN) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    result = cxdref_integ_sat_device_ctrl_Enable (pInteg, 1);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMask (pInteg->pDemod, 0x74, 0x93);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_OutputTone (pInteg->pDemod, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcCommand (pInteg->pDemod, 1, pCommand, 1, NULL, 0);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetToneBurst (pInteg->pDemod, CXDREF_TONEBURST_MODE_OFF);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetTxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_TXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetRxIdleTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_RXIDLE_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetR2RTime (pInteg->pDemod, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE_R2R_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_SetDiseqcReplyMode (pInteg->pDemod, 1, CXDREF_INTEG_SAT_DEVICE_CTRL_SINGLECABLE2REPLY_RTO_TIME_MS);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_HIGH);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    result = cxdref_demod_sat_device_ctrl_StartTransmit (pInteg->pDemod);
    if (result != CXDREF_RESULT_OK){
        goto End;
    }

    if (pInteg->pLnbc->transmitMode == CXDREF_LNBC_TRANSMIT_MODE_AUTO) {
        result = WaitTransmit(pInteg, CXDREF_INTEG_DISEQC_SINGLECABLE_TRANSMIT_POL, 1400);
        if (result != CXDREF_RESULT_OK){
            goto End;
        }
    } else {
        result = WaitTransmitSW(pInteg, 500, 600, 1400);
        if (result != CXDREF_RESULT_OK){
            goto End;
        }
    }

End:
    tempResult = pInteg->pLnbc->SetVoltage (pInteg->pLnbc, CXDREF_LNBC_VOLTAGE_LOW);
    if (tempResult != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (tempResult);
    }

    if (result == CXDREF_RESULT_OK) {
        result = cxdref_demod_sat_device_ctrl_GetReplyMessage (pInteg->pDemod, pReply);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_EnableSinglecable (cxdref_integ_t * pInteg,
                                                            cxdref_integ_singlecable_data_t * pSinglecableData,
                                                            cxdref_result_t (*pTransmitByOtherDemod)(cxdref_diseqc_message_t * pMessage))
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_EnableSinglecable");

    if ((!pInteg) || (!pInteg->pDemod) ||
        (!pInteg->pTuner) || (!pInteg->pTuner->SatTune) || (!pSinglecableData)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->isSinglecable){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    } else {
        pSinglecableData->tunerSinglecableData.pInteg = pInteg;
        pSinglecableData->tunerSinglecableData.pTunerReal = pInteg->pTuner;
        pSinglecableData->tunerSinglecableData.version = CXDREF_SINGLECABLE_VERSION_UNKNOWN;

        cxdref_memset (&pSinglecableData->tunerSinglecable, 0, sizeof (cxdref_tuner_t));

        pSinglecableData->tunerSinglecable.i2cAddress = pInteg->pTuner->i2cAddress;
        pSinglecableData->tunerSinglecable.pI2c = pInteg->pTuner->pI2c;
        pSinglecableData->tunerSinglecable.flags = pInteg->pTuner->flags;
        pSinglecableData->tunerSinglecable.symbolRateKSpsForSpectrum = pInteg->pTuner->symbolRateKSpsForSpectrum;

        pSinglecableData->tunerSinglecable.rfLevelFuncTerr = pInteg->pTuner->rfLevelFuncTerr;
        pSinglecableData->tunerSinglecable.rfLevelFuncSat = pInteg->pTuner->rfLevelFuncSat;

        pSinglecableData->tunerSinglecable.user = &pSinglecableData->tunerSinglecableData;

        pSinglecableData->tunerSinglecable.Initialize = singlecableTunerInitialize;
        pSinglecableData->tunerSinglecable.TerrCableTune = singlecableTunerTerrCableTune;
        pSinglecableData->tunerSinglecable.SatTune = singlecableTunerSatTune;
        pSinglecableData->tunerSinglecable.Sleep = singlecableTunerSleep;
        pSinglecableData->tunerSinglecable.Shutdown = singlecableTunerShutdown;
        pSinglecableData->tunerSinglecable.ReadRFLevel = singlecableTunerReadRFLevel;
        pSinglecableData->tunerSinglecable.CalcRFLevelFromAGC = singlecableTunerCalcRFLevelFromAGC;

        pSinglecableData->tunerSinglecableData.pTransmitByOtherDemod = pTransmitByOtherDemod;

        pInteg->pTuner = &pSinglecableData->tunerSinglecable;

        result = cxdref_demod_sat_device_ctrl_EnableSinglecable (pInteg->pDemod, 1);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_DisableSinglecable (cxdref_integ_t * pInteg)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_DisableSinglecable");

    if ((!pInteg) || (!pInteg->pDemod) ||
        (!pInteg->pTuner) || (!pInteg->pTuner->Initialize)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pInteg->pDemod->isSinglecable){
        if (pInteg->pTuner->user) {
            pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)(pInteg->pTuner->user);
            pInteg->pTuner = pSinglecableTunerData->pTunerReal;
            result = cxdref_demod_sat_device_ctrl_EnableSinglecable (pInteg->pDemod, 0);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_SetSinglecableTunerParams (cxdref_integ_t * pInteg,
                                                                    cxdref_singlecable_address_t address,
                                                                    cxdref_singlecable_bank_t bank,
                                                                    uint8_t ubSlot,
                                                                    uint32_t ubSlotFreqKHz,
                                                                    uint8_t enableMDUMode,
                                                                    uint8_t PINCode)
{
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_SetSinglecableTunerParams");

    if ((!pInteg) || (!pInteg->pTuner) || (!pInteg->pTuner->user)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!pInteg->pDemod->isSinglecable){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pInteg->pTuner->user;
    pSinglecableTunerData->version = CXDREF_SINGLECABLE_VERSION_1_EN50494;
    pSinglecableTunerData->address = address;
    pSinglecableTunerData->bank = bank;
    pSinglecableTunerData->ubSlot = ubSlot;
    pSinglecableTunerData->ubSlotFreqKHz = ubSlotFreqKHz;
    pSinglecableTunerData->enableMDUMode = enableMDUMode;
    pSinglecableTunerData->PINCode = PINCode;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_integ_sat_device_ctrl_SetSinglecable2TunerParams (cxdref_integ_t * pInteg,
                                                                     uint8_t ubSlot,
                                                                     uint32_t ubSlotFreqMHz,
                                                                     uint8_t uncommittedSwitches,
                                                                     uint8_t committedSwitches,
                                                                     uint8_t enableMDUMode,
                                                                     uint8_t PINCode)
{
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("cxdref_integ_sat_device_ctrl_SetSinglecable2TunerParams");

    if ((!pInteg) || (!pInteg->pTuner) || (!pInteg->pTuner->user)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (!pInteg->pDemod->isSinglecable){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pInteg->pTuner->user;
    pSinglecableTunerData->version = CXDREF_SINGLECABLE_VERSION_2_EN50607;
    pSinglecableTunerData->scd2_ubSlot = ubSlot;
    pSinglecableTunerData->scd2_ubSlotFreqMHz = ubSlotFreqMHz;
    pSinglecableTunerData->scd2_uncommittedSwitches = uncommittedSwitches;
    pSinglecableTunerData->scd2_committedSwitches = committedSwitches;
    pSinglecableTunerData->scd2_enableMDUMode = enableMDUMode;
    pSinglecableTunerData->scd2_PINCode = PINCode;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t WaitTransmit (cxdref_integ_t * pInteg,
                                   uint32_t interval,
                                   uint32_t timeout)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isContinue = 0;
    uint8_t transmitStatus = 0;
    uint32_t elapsedTime = 0;
    cxdref_stopwatch_t stopwatch;

    CXDREF_TRACE_ENTER ("WaitTransmit");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_stopwatch_start(&stopwatch);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    isContinue = 1;
    while (isContinue){
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_sat_device_ctrl_GetTransmitStatus (pInteg->pDemod, &transmitStatus);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (transmitStatus == 0x00){
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        } else {
            if(elapsedTime > timeout){
                isContinue = 0;
            } else {
                result = cxdref_stopwatch_sleep(&stopwatch, interval);
                if (result != CXDREF_RESULT_OK) {
                    CXDREF_TRACE_RETURN (result);
                }
            }
        }
    }
    CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
}

static cxdref_result_t WaitTransmitSW (cxdref_integ_t * pInteg,
                                     uint32_t timeout1,
                                     uint32_t timeout2,
                                     uint32_t timeout3)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t isContinue = 0;
    uint8_t transmitStatus = 0;
    uint32_t elapsedTime = 0;
    cxdref_stopwatch_t stopwatch;

    CXDREF_TRACE_ENTER ("WaitTransmitSW");

    if (!pInteg) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_stopwatch_start (&stopwatch);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    isContinue = 1;
    while (isContinue){
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_sat_device_ctrl_GetTransmitStatus (pInteg->pDemod, &transmitStatus);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (transmitStatus == 0x02){
            isContinue = 0;
        } else {
            if (elapsedTime > timeout1){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
            }
        }
    }

    isContinue = 1;
    while (isContinue){
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_sat_device_ctrl_GetTransmitStatus (pInteg->pDemod, &transmitStatus);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (transmitStatus != 0x02){
            isContinue = 0;
        } else {
            if (elapsedTime > timeout2){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
            }
        }
    }

    result = pInteg->pLnbc->SetTransmitMode (pInteg->pLnbc, CXDREF_LNBC_TRANSMIT_MODE_RX);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    isContinue = 1;
    while (isContinue){
        result = cxdref_integ_CheckCancellation (pInteg);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_stopwatch_elapsed (&stopwatch, &elapsedTime);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        result = cxdref_demod_sat_device_ctrl_GetTransmitStatus (pInteg->pDemod, &transmitStatus);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (transmitStatus == 0x00){
            isContinue = 0;
        } else {
            if (elapsedTime > timeout3){
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
            }
        }
    }

    result = pInteg->pLnbc->SetTransmitMode (pInteg->pLnbc, CXDREF_LNBC_TRANSMIT_MODE_TX);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t singlecableTunerInitialize (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerInitialize");

    if ((!pTuner) || (!pTuner->user)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->Initialize) {
        result = pSinglecableTunerData->pTunerReal->Initialize (pSinglecableTunerData->pTunerReal);
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

static cxdref_result_t singlecableTunerTerrCableTune (cxdref_tuner_t * pTuner,
                                                    uint32_t centerFreqKHz,
                                                    cxdref_dtv_system_t system,
                                                    cxdref_dtv_bandwidth_t bandwidth)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerTerrCableTune");

    if ((!pTuner) || (!pTuner->user)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->TerrCableTune) {
        result = pSinglecableTunerData->pTunerReal->TerrCableTune (pSinglecableTunerData->pTunerReal,
                                                                   centerFreqKHz,
                                                                   system,
                                                                   bandwidth);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pTuner->frequencyKHz = pSinglecableTunerData->pTunerReal->frequencyKHz;
    } else {
        pTuner->frequencyKHz = centerFreqKHz;
    }

    pTuner->system = system;
    pTuner->bandwidth = bandwidth;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);

}

static cxdref_result_t singlecableTunerSatTune (cxdref_tuner_t * pTuner,
                                              uint32_t centerFreqKHz,
                                              cxdref_dtv_system_t system,
                                              uint32_t symbolRateKSps)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;
    cxdref_diseqc_message_t command;
    uint32_t scFreqKHz = 0;
    uint32_t tuFreqKHz = 0;

    CXDREF_TRACE_ENTER ("singlecableTunerTune");

    if ((!pTuner) || (!pTuner->user)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*) pTuner->user;

    if (pSinglecableTunerData->version == CXDREF_SINGLECABLE_VERSION_1_EN50494) {
        if (pSinglecableTunerData->enableMDUMode) {
            result = cxdref_singlecable_command_ODU_Channel_change_MDU (&command,
                                                                      pSinglecableTunerData->address,
                                                                      pSinglecableTunerData->ubSlot,
                                                                      pSinglecableTunerData->ubSlotFreqKHz,
                                                                      pSinglecableTunerData->bank,
                                                                      centerFreqKHz,
                                                                      pSinglecableTunerData->PINCode);
        } else {
            result = cxdref_singlecable_command_ODU_Channel_change (&command,
                                                                  pSinglecableTunerData->address,
                                                                  pSinglecableTunerData->ubSlot,
                                                                  pSinglecableTunerData->ubSlotFreqKHz,
                                                                  pSinglecableTunerData->bank,
                                                                  centerFreqKHz);
        }
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        scFreqKHz = ((uint32_t) (command.data[3] & 0x03) << 8) | (uint32_t) (command.data[4] & 0xFF);
        scFreqKHz = ((scFreqKHz + 350) * 4000) - pSinglecableTunerData->ubSlotFreqKHz;

        if (pSinglecableTunerData->pTransmitByOtherDemod) {
            result = pSinglecableTunerData->pTransmitByOtherDemod (&command);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            result = cxdref_integ_sat_device_ctrl_TransmitSinglecableCommand (pSinglecableTunerData->pInteg, &command);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        }

        if (pSinglecableTunerData->pInteg->pDemod->isSinglecableSpectrumInv == 1) {
            tuFreqKHz = pSinglecableTunerData->ubSlotFreqKHz - (centerFreqKHz - scFreqKHz);
        } else {
            tuFreqKHz = pSinglecableTunerData->ubSlotFreqKHz + (centerFreqKHz - scFreqKHz);
        }
    } else if (pSinglecableTunerData->version == CXDREF_SINGLECABLE_VERSION_2_EN50607) {
        uint32_t scFreqMHz = (centerFreqKHz + 500) / 1000;
        if (scFreqMHz > 2147) {
            scFreqMHz = 2147;
        }

        if (pSinglecableTunerData->scd2_enableMDUMode) {
            result = cxdref_singlecable2_command_ODU_Channel_change_PIN (&command,
                                                                       pSinglecableTunerData->scd2_ubSlot,
                                                                       scFreqMHz,
                                                                       pSinglecableTunerData->scd2_uncommittedSwitches,
                                                                       pSinglecableTunerData->scd2_committedSwitches,
                                                                       pSinglecableTunerData->scd2_PINCode);
        } else {
            result = cxdref_singlecable2_command_ODU_Channel_change (&command,
                                                                   pSinglecableTunerData->scd2_ubSlot,
                                                                   scFreqMHz,
                                                                   pSinglecableTunerData->scd2_uncommittedSwitches,
                                                                   pSinglecableTunerData->scd2_committedSwitches);
        }
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        scFreqKHz = scFreqMHz * 1000;

        if (pSinglecableTunerData->pTransmitByOtherDemod) {
            result = pSinglecableTunerData->pTransmitByOtherDemod (&command);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            result = cxdref_integ_sat_device_ctrl_TransmitSinglecable2Command (pSinglecableTunerData->pInteg, &command);
            if (result != CXDREF_RESULT_OK){
                CXDREF_TRACE_RETURN (result);
            }
        }
        if (pSinglecableTunerData->pInteg->pDemod->isSinglecableSpectrumInv == 1) {
            tuFreqKHz = (pSinglecableTunerData->scd2_ubSlotFreqMHz * 1000) - (centerFreqKHz - scFreqKHz);
        } else {
            tuFreqKHz = (pSinglecableTunerData->scd2_ubSlotFreqMHz * 1000) + (centerFreqKHz - scFreqKHz);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->SatTune) {
        result = pSinglecableTunerData->pTunerReal->SatTune (pSinglecableTunerData->pTunerReal, tuFreqKHz, system, symbolRateKSps);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

        if (pSinglecableTunerData->version == CXDREF_SINGLECABLE_VERSION_1_EN50494) {
            if (pSinglecableTunerData->pInteg->pDemod->isSinglecableSpectrumInv == 1){
                pTuner->frequencyKHz = scFreqKHz - (pSinglecableTunerData->pTunerReal->frequencyKHz - pSinglecableTunerData->ubSlotFreqKHz);
            } else {
                pTuner->frequencyKHz = scFreqKHz + (pSinglecableTunerData->pTunerReal->frequencyKHz - pSinglecableTunerData->ubSlotFreqKHz);
            }
        } else {
            if (pSinglecableTunerData->pInteg->pDemod->isSinglecableSpectrumInv == 1) {
                pTuner->frequencyKHz = scFreqKHz - (pSinglecableTunerData->pTunerReal->frequencyKHz - (pSinglecableTunerData->scd2_ubSlotFreqMHz * 1000));
            } else {
                pTuner->frequencyKHz = scFreqKHz + (pSinglecableTunerData->pTunerReal->frequencyKHz - (pSinglecableTunerData->scd2_ubSlotFreqMHz * 1000));
            }
        }
    } else {
        pTuner->frequencyKHz = centerFreqKHz;
    }

    pTuner->system = system;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = symbolRateKSps;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t singlecableTunerSleep (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerSleep");

    if ((!pTuner) || (!pTuner->user)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->Sleep) {
        result = pSinglecableTunerData->pTunerReal->Sleep (pSinglecableTunerData->pTunerReal);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    pTuner->frequencyKHz = 0;
    pTuner->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t singlecableTunerShutdown (cxdref_tuner_t * pTuner)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerShutdown");

    if ((!pTuner) || (!pTuner->user)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->Shutdown) {
        result = pSinglecableTunerData->pTunerReal->Shutdown (pSinglecableTunerData->pTunerReal);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    pTuner->frequencyKHz = 0;
    pTuner->system = CXDREF_DTV_SYSTEM_UNKNOWN;
    pTuner->bandwidth = CXDREF_DTV_BW_UNKNOWN;
    pTuner->symbolRateKSps = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t singlecableTunerReadRFLevel (cxdref_tuner_t * pTuner,
                                                  int32_t * pRFLevel)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerReadRFLevel");

    if ((!pTuner) || (!pTuner->user) || (!pRFLevel)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->ReadRFLevel) {
        result = pSinglecableTunerData->pTunerReal->ReadRFLevel (pSinglecableTunerData->pTunerReal, pRFLevel);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t singlecableTunerCalcRFLevelFromAGC (cxdref_tuner_t * pTuner,
                                                         uint32_t agcValue,
                                                         int32_t * pRFLevel)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_integ_singlecable_tuner_data_t * pSinglecableTunerData = NULL;

    CXDREF_TRACE_ENTER ("singlecableTunerCalcRFLevelFromAGC");

    if ((!pTuner) || (!pTuner->user) || (!pRFLevel)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    pSinglecableTunerData = (cxdref_integ_singlecable_tuner_data_t*)pTuner->user;

    if (pSinglecableTunerData->pTunerReal && pSinglecableTunerData->pTunerReal->CalcRFLevelFromAGC) {
        result = pSinglecableTunerData->pTunerReal->CalcRFLevelFromAGC (pSinglecableTunerData->pTunerReal, agcValue, pRFLevel);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
