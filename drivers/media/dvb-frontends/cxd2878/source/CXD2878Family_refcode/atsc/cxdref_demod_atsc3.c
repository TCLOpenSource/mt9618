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

#include "cxdref_demod_atsc3.h"
#include "cxdref_demod_atsc3_monitor.h"

static cxdref_result_t SLtoAA3(cxdref_demod_t * pDemod);

static cxdref_result_t SLtoAA3_BandSetting(cxdref_demod_t * pDemod);

static cxdref_result_t AA3toAA3(cxdref_demod_t * pDemod);

static cxdref_result_t AA3toSL(cxdref_demod_t * pDemod);

static cxdref_result_t invertSpectrum (cxdref_demod_t * pDemod);

static cxdref_result_t detectCW (cxdref_demod_t * pDemod);

static cxdref_result_t initCWDetection (cxdref_demod_t * pDemod);

cxdref_result_t cxdref_demod_atsc3_Tune (cxdref_demod_t * pDemod,
                                     cxdref_atsc3_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_Tune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) {
        if ((pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) || (pDemod->atsc3EasState != CXDREF_DEMOD_ATSC3_EAS_STATE_NORMAL)) {
            result = cxdref_demod_Sleep(pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            pDemod->system = CXDREF_DTV_SYSTEM_ATSC3;
            pDemod->bandwidth = pTuneParam->bandwidth;
            pDemod->atsc3EasState = CXDREF_DEMOD_ATSC3_EAS_STATE_NORMAL;
            pDemod->atsc3AutoSpectrumInv_flag = 0;
            pDemod->atsc3CWDetection_flag = 0;

            result = SLtoAA3 (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            pDemod->bandwidth = pTuneParam->bandwidth;
            pDemod->atsc3AutoSpectrumInv_flag = 0;
            pDemod->atsc3CWDetection_flag = 0;

            result = AA3toAA3(pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
    } else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
        pDemod->system = CXDREF_DTV_SYSTEM_ATSC3;
        pDemod->bandwidth = pTuneParam->bandwidth;
        pDemod->atsc3EasState = CXDREF_DEMOD_ATSC3_EAS_STATE_NORMAL;
        pDemod->atsc3AutoSpectrumInv_flag = 0;
        pDemod->atsc3CWDetection_flag = 0;

        result = SLtoAA3 (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;

    CXDREF_TRACE_RETURN (result);

}

cxdref_result_t cxdref_demod_atsc3_EASTune (cxdref_demod_t * pDemod,
                                     cxdref_atsc3_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_EASTune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) {
        if ((pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) || (pDemod->atsc3EasState != CXDREF_DEMOD_ATSC3_EAS_STATE_EAS)) {
            result = cxdref_demod_Sleep (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }

            pDemod->system = CXDREF_DTV_SYSTEM_ATSC3;
            pDemod->bandwidth = pTuneParam->bandwidth;
            pDemod->atsc3EasState = CXDREF_DEMOD_ATSC3_EAS_STATE_EAS;
            pDemod->atsc3AutoSpectrumInv_flag = 0;
            pDemod->atsc3CWDetection_flag = 0;

            result = SLtoAA3 (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            pDemod->bandwidth = pTuneParam->bandwidth;
            pDemod->atsc3AutoSpectrumInv_flag = 0;
            pDemod->atsc3CWDetection_flag = 0;
            result = AA3toAA3(pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
        }
    } else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
        pDemod->system = CXDREF_DTV_SYSTEM_ATSC3;
        pDemod->bandwidth = pTuneParam->bandwidth;
        pDemod->atsc3EasState = CXDREF_DEMOD_ATSC3_EAS_STATE_EAS;
        pDemod->atsc3AutoSpectrumInv_flag = 0;
        pDemod->atsc3CWDetection_flag = 0;

        result = SLtoAA3 (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = AA3toSL (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_EASTuneEnd (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_EASTuneEnd");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state  != CXDREF_DEMOD_STATE_ACTIVE) ||
        (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) ||
        (pDemod->atsc3EasState != CXDREF_DEMOD_ATSC3_EAS_STATE_EAS)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_SoftReset (pDemod);

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_CheckDemodLock (cxdref_demod_t * pDemod,
                                               cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t sync = 0;
    uint8_t alpLockStat[4] = {0};
    uint8_t alpLockAll = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_CheckDemodLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_atsc3_monitor_SyncStat (pDemod, &sync, alpLockStat, &alpLockAll, &unlockDetected);

    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (unlockDetected) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (sync >= 6) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_CheckALPLock (cxdref_demod_t * pDemod,
                                             cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t sync = 0;
    uint8_t alpLockStat[4] = {0};
    uint8_t alpLockAll = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_CheckALPLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_atsc3_monitor_SyncStat (pDemod, &sync, alpLockStat, &alpLockAll, &unlockDetected);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (unlockDetected) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if ((sync >= 6) && alpLockAll) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (sync >= 6) {
        uint8_t plpValid[4];

        result = cxdref_demod_atsc3_monitor_SelectedPLPValid (pDemod, plpValid);
        if (result == CXDREF_RESULT_ERROR_HW_STATE) {
            *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        } else if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        if (!plpValid[0]) {
            *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_SetPLPConfig (cxdref_demod_t * pDemod,
                                             uint8_t plpIDNum,
                                             uint8_t plpID[4])
{
    int i = 0;
    uint8_t data[4];
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_SetPLPConfig");

    if ((!pDemod) || (!plpID)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if ((plpIDNum == 0) || (plpIDNum > 4)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for (i = 0; i < plpIDNum; i++) {
        if (plpID[i] > 0x3F) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x93) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    for (i = 0; i < 4; i++) {
        if (i < plpIDNum) {
            data[i] = plpID[i] | 0x80;
        } else {
            data[i] = 0;
        }
    }
    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, data, sizeof (data)) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x85,
        pDemod->atsc3AutoPLPID_SPLP ? 0x01 : 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9C, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_ClearBERSNRHistory (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_ClearBERSNRHistory");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x02) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x52, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_ClearGPIOEASLatch (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_ClearGPIOEASLatch");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x99) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xAF, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_UnlockCase (cxdref_demod_t * pDemod, uint8_t * pContinueWait)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_atsc3_bootstrap_t bootstrap;
    uint8_t data[6];
    uint32_t sinvdetCorrMax;
    uint32_t sinvdetCorrRMax;
    uint8_t maxState;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_AutoDetectSeq_UnlockCase");

    if ((!pDemod) || (!pContinueWait)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_atsc3_monitor_Bootstrap (pDemod, &bootstrap);
    if (result == CXDREF_RESULT_OK) {
        if (bootstrap.bw_diff) {
            *pContinueWait = 0;
            CXDREF_TRACE_RETURN (result);
        }
    } else if (result != CXDREF_RESULT_ERROR_HW_STATE) {
        CXDREF_TRACE_RETURN (result);
    }

    if (((!pDemod->atsc3AutoSpectrumInv) || (pDemod->atsc3AutoSpectrumInv_flag))
        && ((!pDemod->atsc3CWDetection) || (pDemod->atsc3CWDetection_flag))) {
        *pContinueWait = 0;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    CXDREF_SLEEP (11);

    if (SLVT_FreezeReg (pDemod) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x98) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6F, data, 6) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    sinvdetCorrMax = (data[0] << 16) | (data[1] << 8) | data[2];
    sinvdetCorrRMax = (data[3] << 16) | (data[4] << 8) | data[5];

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x17, data, 1) != CXDREF_RESULT_OK) {
        SLVT_UnFreezeReg (pDemod);
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    maxState = data[0] & 0x07;

    SLVT_UnFreezeReg (pDemod);

    if ((pDemod->atsc3AutoSpectrumInv) && (!pDemod->atsc3AutoSpectrumInv_flag)) {
        if ((sinvdetCorrRMax > 70000) && (sinvdetCorrRMax > (((sinvdetCorrMax * 3) + 1) / 2)) && (maxState < 3)) {
            result = invertSpectrum (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            *pContinueWait = 1;
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pDemod->atsc3CWDetection) && (!pDemod->atsc3CWDetection_flag)) {
        result = detectCW (pDemod);
        if (result == CXDREF_RESULT_OK) {
            if (pDemod->atsc3CWDetection_flag) {
                *pContinueWait = 1;
                CXDREF_TRACE_RETURN (result);
            }
        } else {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if ((pDemod->atsc3AutoSpectrumInv) && (!pDemod->atsc3AutoSpectrumInv_flag)) {
        if ((sinvdetCorrRMax > 70000) && (sinvdetCorrRMax > sinvdetCorrMax) && (maxState < 3)) {
            result = invertSpectrum (pDemod);
            if (result != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (result);
            }
            *pContinueWait = 1;
            CXDREF_TRACE_RETURN (result);
        }
    }

    *pContinueWait = 0;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_SetCWTracking (cxdref_demod_t * pDemod, uint8_t * pCompleted)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t rdata[10];
    uint8_t data[3];
    uint32_t totalPwr;
    uint32_t maxCPwr;
    uint32_t maxCNum;
    int32_t carrierDuration;
    int32_t samplingFreq;
    int32_t freqCW;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_AutoDetectSeq_SetCWTracking");

    if ((!pDemod) || (!pCompleted)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC3) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (!pDemod->atsc3CWDetection_flag) {
        *pCompleted = 1;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x46, rdata, 10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (!(rdata[0] & 0x01)) {
        *pCompleted = 0;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    totalPwr = (rdata[1] << 24) | (rdata[2] << 16) | (rdata[3] << 8) | rdata[4];
    maxCPwr = ((rdata[5] & 0x01) << 16) | (rdata[6] << 8) | rdata[7];
    maxCNum = ((rdata[8] & 0x7F) << 8) | rdata[9];

    if ((maxCPwr <= 10000) || (maxCPwr <= (totalPwr / 200))) {
        *pCompleted = 1;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x5C, rdata, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    switch (rdata[0] >> 5) {
    case 1:
        carrierDuration = 192;
        break;
    case 4:
        carrierDuration = 96;
        break;
    case 5:
        carrierDuration = 48;
        break;
    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    switch (pDemod->bandwidth) {
    case CXDREF_DTV_BW_8_MHZ:
        samplingFreq = 8;
        break;
    case CXDREF_DTV_BW_7_MHZ:
        samplingFreq = 7;
        break;
    case CXDREF_DTV_BW_6_MHZ:
        samplingFreq = 6;
        break;
    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    freqCW = ((int32_t)maxCNum - 16384) * carrierDuration / samplingFreq;
    data[0] = ((uint32_t)freqCW >> 16) & 0x03;
    data[1] = ((uint32_t)freqCW >> 8) & 0xFF;
    data[2] = (uint32_t)freqCW & 0xFF;

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3E, data, 3) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3C, 0x04) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, 0x0C) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    *pCompleted = 1;

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc3_AutoDetectSeq_Init (cxdref_demod_t * pDemod)
{
    cxdref_result_t result;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc3_AutoDetectSeq_Init");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->confSense == pDemod->rfSpectrumSense) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    result = initCWDetection (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t SLtoAA3(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("SLtoAA3");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    if ((pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)
        || (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {

        if (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_BBP) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_NOSUPPORT);
        }

        result = cxdref_demod_atsc3_chbond_Enable (pDemod, 1);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
#endif

    if (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB) {
    } else
#ifdef CXDREF_DEMOD_SUPPORT_ALP
    if (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_ALP) {
        result = cxdref_demod_SetALPClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_ATSC3);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else
#endif
    if (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_BBP) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF1, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        result = cxdref_demod_SetTSClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_ATSC3);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x0E) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x36, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    {
        uint8_t data = 0;

        switch (pDemod->atsc3Output) {
        case CXDREF_DEMOD_OUTPUT_ATSC3_ALP:
        case CXDREF_DEMOD_OUTPUT_ATSC3_BBP:
        default:
            data = 0x02;
            break;

        case CXDREF_DEMOD_OUTPUT_ATSC3_ALP_DIV_TS:
            data = 0x00;
            break;
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, data) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, 0x74) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x11) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x6A, 0x50) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[3] = { 0x00, 0x03, 0x3B };
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x33, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x79, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x7B, 0x10) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9C) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        const uint8_t data50[5] = { 0x93,  0x40,  0x09,  0xB4,  0x01 };
        const uint8_t data65[5] = { 0xFC,  0x00,  0x00,  0x00,  0x00 };
        const uint8_t dataD4[3] = { 0x01,  0xD8,  0x1E };
        const uint8_t dataE0[3] = { 0x01,  0x00,  0x04 };

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, data50, 5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x65, data65, 5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD4, dataD4, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, dataE0, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFC, 0x12) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    } else {
        const uint8_t data50[5] = { 0x93,  0x40,  0x09,  0xC0,  0x01 };
        const uint8_t data65[5] = { 0xD3,  0x40,  0x00,  0x00,  0x00 };
        const uint8_t dataD4[3] = { 0x01,  0xD8,  0x1C };
        const uint8_t dataE0[3] = { 0x01,  0xD8,  0x1D };

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, data50, 5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x65, data65, 5) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD4, dataD4, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE0, dataE0, 3) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFC, 0x14) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    result = SLtoAA3_BandSetting (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, 0x08, 0x1F) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_SetTSDataPinHiZ (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t SLtoAA3_BandSetting(cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("SLtoAA3_BandSetting");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    switch (pDemod->bandwidth) {
    case CXDREF_DTV_BW_8_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x14, 0xD5, 0x55, 0x55, 0x55
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x2F,  0xBA,  0x28,  0x9B,  0x28,  0x9D,  0x28,  0xA1,  0x29,  0xA5,  0x2A,  0xAC,  0x29,  0xB5
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x01) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        } else {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x00) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_8 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_8 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configATSC3_8 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x1D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[10] = {
                0x01, 0x14, 0x65, 0x08, 0xD7, 0x65, 0xEF, 0x14, 0x65, 0x08
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBF, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x99) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[4] = {
                0xA6, 0xAB, 0x0A, 0x6B
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x89, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        break;

    case CXDREF_DTV_BW_7_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x17, 0xCF, 0x3C, 0xF3, 0xCF
            };
        
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x30,  0xB1,  0x29,  0x9A,  0x28,  0x9C,  0x28,  0xA0,  0x29,  0xA2,  0x2B,  0xA6,  0x2B,  0xAD
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x01) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        } else {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x00) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_7 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_7 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configATSC3_7 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x02) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x1D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[10] = {
                0x01, 0x19, 0x94, 0x23, 0xCC, 0xD7, 0xBA, 0x19, 0x94, 0x23
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBF, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x99) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[4] = {
                0xBE, 0x7A, 0x0B, 0xE8
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x89, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        break;

    case CXDREF_DTV_BW_6_MHZ:
        {
            const uint8_t nominalRate[5] = {
                0x1B, 0xC7, 0x1C, 0x71, 0xC7
            };
        
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x9F, nominalRate, 5) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x10) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
            const uint8_t itbCoef[14] = {
                0x31,  0xA8,  0x29,  0x9B,  0x27,  0x9C,  0x28,  0x9E,  0x29,  0xA4,  0x29,  0xA2,  0x29,  0xA8
            };

            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA6, itbCoef, 14) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }

            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x01) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        } else {
            if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA5, 0x00) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        {
            uint8_t data[3];
            data[0] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_6 >> 16) & 0xFF);
            data[1] = (uint8_t) ((pDemod->iffreqConfig.configATSC3_6 >> 8) & 0xFF);
            data[2] = (uint8_t) (pDemod->iffreqConfig.configATSC3_6 & 0xFF);
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD7, 0x04) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x1D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[10] = {
                0x01, 0x1E, 0xC3, 0x3E, 0xC2, 0x79, 0x84, 0x1E, 0xC3, 0x3E
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBF, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x99) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        {
            uint8_t data[4] = {
                0xDE, 0x39, 0x0D, 0xE4
            };
            if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x89, data, sizeof (data)) != CXDREF_RESULT_OK) {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
            }
        }

        break;

    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t AA3toAA3(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AA3toAA3");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = SLtoAA3_BandSetting (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t AA3toSL(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AA3toSL");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (cxdref_i2c_SetRegisterBits (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x80, 0x1F, 0x1F) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    result = cxdref_demod_SetTSDataPinHiZ (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->atsc3Output == CXDREF_DEMOD_OUTPUT_ATSC3_BBP) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x03) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xB6, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9D) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF1, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x95) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x90, 0x01) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x1D) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xBF, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, 0x33) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, 0x21) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xFE, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->atsc3EasState == CXDREF_DEMOD_ATSC3_EAS_STATE_EAS) {
        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x36, 0x00) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

#ifdef CXDREF_DEMOD_SUPPORT_ATSC3_CHBOND
    if ((pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_MAIN)
        || (pDemod->chbondConfig == CXDREF_DEMOD_CHBOND_CONFIG_ATSC3_SUB)) {
        result = cxdref_demod_atsc3_chbond_Enable (pDemod, 0);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
#endif

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t invertSpectrum (cxdref_demod_t * pDemod)
{
    cxdref_result_t result;
    uint8_t data = 0;
    uint8_t spectrumInv = 0;

    CXDREF_TRACE_ENTER ("invertSpectrum");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x90) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, &data, 1) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if ((data & 0x01) == 0x00) {
        spectrumInv = 0x01;
    } else {
        spectrumInv = 0x00;
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xF3, spectrumInv) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    pDemod->atsc3AutoSpectrumInv_flag = 1;
    pDemod->atsc3CWDetection_flag = 0;

    result = initCWDetection (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SoftReset (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t detectCW (cxdref_demod_t * pDemod)
{
    cxdref_result_t result;
    uint8_t data[9];
    uint32_t totalPwr;
    uint32_t maxCPwr;
    uint32_t maxCNum;
    int32_t freqCW;
    int32_t freqCW2;

    CXDREF_TRACE_ENTER ("detectCW");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x99) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xE6, data, 9) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    totalPwr = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    maxCPwr = ((data[4] & 0x01) << 16) | (data[5] << 8) | data[6];
    maxCNum = ((data[7] & 0x7F) << 8) | data[8];
    if (maxCNum > 2047) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    if ((maxCPwr <= (totalPwr / 100)) || (maxCPwr <= 500)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    freqCW = ((int32_t)maxCNum - 1024) * 128;

    switch (pDemod->bandwidth) {
    case CXDREF_DTV_BW_8_MHZ:
        freqCW2 = (((int32_t)maxCNum - 1024) * 256) / 3;
        break;
    case CXDREF_DTV_BW_7_MHZ:
        freqCW2 = (((int32_t)maxCNum - 1024) * 2048) / 21;
        break;
    case CXDREF_DTV_BW_6_MHZ:
        freqCW2 = (((int32_t)maxCNum - 1024) * 1024) / 9;
        break;
    default:
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    data[0] = ((uint32_t)freqCW >> 16) & 0x03;
    data[1] = ((uint32_t)freqCW >> 8) & 0xFF;
    data[2] = (uint32_t)freqCW & 0xFF;
    data[3] = ((uint32_t)freqCW2 >> 16) & 0x03;
    data[4] = ((uint32_t)freqCW2 >> 8) & 0xFF;
    data[5] = (uint32_t)freqCW2 & 0xFF;
    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3E, data, 6) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, 0x05) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9B) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, 0x24) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    data[0] = 0x03;
    data[1] = 0x03;
    if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3C, data, 2) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, 0x04) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    pDemod->atsc3CWDetection_flag = 1;

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SoftReset (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t initCWDetection (cxdref_demod_t * pDemod)
{
    CXDREF_TRACE_ENTER ("initCWDetection");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x38, 0x04) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9B) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x11, 0x20) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x9A) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    {
        const uint8_t data[8] = { 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        if (pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x3C, data, sizeof (data)) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x50, 0x05) != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
