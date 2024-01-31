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

#include "cxdref_demod_atsc.h"
#include "cxdref_demod_atsc_monitor.h"

#define WAITCOMMANDRESPONSE_POLL_INTERVAL_MS (10)
#define WAITCPUIDLE_TIMEOUT_MS               (1000)

typedef struct
{
    uint8_t  data[6];
} cxdref_demod_atsc_cmd_t;

static cxdref_result_t SLtoAA(cxdref_demod_t * pDemod);

static cxdref_result_t AAtoAA(cxdref_demod_t * pDemod);

static cxdref_result_t AAtoSL(cxdref_demod_t * pDemod);

static cxdref_result_t sendCommand (cxdref_demod_t *pDemod,
                                  const cxdref_demod_atsc_cmd_t *pCmd);

static cxdref_result_t readResponse (cxdref_demod_t *pDemod,
                                   cxdref_demod_atsc_cmd_t *pResp,
                                   uint8_t cmdId);

static cxdref_result_t demodAbort (cxdref_demod_t * pDemod);

static cxdref_result_t slaveRWriteMultiRegisters (cxdref_demod_t * pDemod,
                                                uint8_t  bank,
                                                uint8_t  registerAddress,
                                                const uint8_t* pData,
                                                uint8_t  size);

cxdref_result_t cxdref_demod_atsc_Tune (cxdref_demod_t * pDemod,
                                    cxdref_atsc_tune_param_t * pTuneParam)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_Tune");

    if ((!pDemod) || (!pTuneParam)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system == CXDREF_DTV_SYSTEM_ATSC)) {
        result = AAtoAA(pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else if ((pDemod->state == CXDREF_DEMOD_STATE_ACTIVE) && (pDemod->system != CXDREF_DTV_SYSTEM_ATSC)) {
        result = cxdref_demod_Sleep(pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }

        pDemod->system = CXDREF_DTV_SYSTEM_ATSC;
        pDemod->bandwidth = CXDREF_DTV_BW_6_MHZ;

        result = SLtoAA (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else if (pDemod->state == CXDREF_DEMOD_STATE_SLEEP) {
        pDemod->system = CXDREF_DTV_SYSTEM_ATSC;
        pDemod->bandwidth = CXDREF_DTV_BW_6_MHZ;

        result = SLtoAA (pDemod);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    pDemod->state = CXDREF_DEMOD_STATE_ACTIVE;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_TuneEnd (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_TuneEnd");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = cxdref_demod_atsc_SoftReset(pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 1);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_SoftReset (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_atsc_cmd_t  cmd;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_SoftReset");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->system != CXDREF_DTV_SYSTEM_ATSC) {
        CXDREF_TRACE_RETURN(CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = demodAbort (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    cmd.data[0] = 0xB3;
    cmd.data[1] = 0x10;

    if (pDemod->atscAutoSignalCheckOff) {
        cmd.data[2] = 0x0C; /* customization from original reference code */
    }
    else {
        cmd.data[2] = 0x08; /* customization from original reference code */
    }

    cmd.data[3] = 0;
    cmd.data[4] = 0;
    cmd.data[5] = 0;

    result = sendCommand (pDemod, &cmd);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = readResponse (pDemod, &cmd, 0xB3);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    pDemod->atscCPUState = CXDREF_DEMOD_ATSC_CPU_STATE_BUSY;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_Sleep (cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_Sleep");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) && (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    result = AAtoSL (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_CheckDemodLock (cxdref_demod_t * pDemod,
                                              cxdref_demod_lock_result_t * pLock)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    uint8_t vqlock = 0;
    uint8_t tslock = 0;
    uint8_t agclock = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_CheckDemodLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_atsc_monitor_SyncStat (pDemod, &vqlock, &agclock, &tslock, &unlockDetected);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->atscUnlockDetection) {
        if (unlockDetected) {
            *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
    } else {
        unlockDetected = 0;
    }

    if (vqlock) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_CheckTSLock (cxdref_demod_t * pDemod,
                                           cxdref_demod_lock_result_t * pLock)
{

    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t vqlock = 0;
    uint8_t tslock = 0;
    uint8_t agclock = 0;
    uint8_t unlockDetected = 0;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_CheckTSLock");

    if (!pDemod || !pLock) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    *pLock = CXDREF_DEMOD_LOCK_RESULT_NOTDETECT;

    result = cxdref_demod_atsc_monitor_SyncStat (pDemod, &vqlock, &agclock, &tslock, &unlockDetected);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (tslock) {
        *pLock = CXDREF_DEMOD_LOCK_RESULT_LOCKED;
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }

    if (pDemod->atscUnlockDetection) {
        if (unlockDetected) {
            *pLock = CXDREF_DEMOD_LOCK_RESULT_UNLOCKED;
            CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
        }
    } else {
        unlockDetected = 0;
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxdref_demod_atsc_SlaveRWriteRegister (cxdref_demod_t * pDemod,
                                                   uint8_t  bank,
                                                   uint8_t  registerAddress,
                                                   uint8_t  value,
                                                   uint8_t  bitMask)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_atsc_cmd_t cmd;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_SlaveRWriteRegister");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) &&
        (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->atscCPUState != CXDREF_DEMOD_ATSC_CPU_STATE_IDLE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    cmd.data[0] = 0xC5;
    cmd.data[1] = bank;
    cmd.data[2] = registerAddress;
    cmd.data[3] = value;
    cmd.data[4] = bitMask;
    cmd.data[5] = 0;

    result = sendCommand(pDemod, &cmd);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_SLEEP(2); /* customization from original reference code */
    
    result = readResponse(pDemod, &cmd, 0xC5);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (result);
}

cxdref_result_t cxdref_demod_atsc_SlaveRReadRegister (cxdref_demod_t * pDemod,
                                                  uint8_t bank,
                                                  uint8_t registerAddress,
                                                  uint8_t * pValue)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    cxdref_demod_atsc_cmd_t cmd;

    CXDREF_TRACE_ENTER ("cxdref_demod_atsc_SlaveRReadRegister");

    if (!pDemod || !pValue) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    if ((pDemod->state != CXDREF_DEMOD_STATE_SLEEP) &&
        (pDemod->state != CXDREF_DEMOD_STATE_ACTIVE)) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_SW_STATE);
    }

    if (pDemod->atscCPUState != CXDREF_DEMOD_ATSC_CPU_STATE_IDLE) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
    }

    cmd.data[0] = 0xC6;
    cmd.data[1] = bank;
    cmd.data[2] = registerAddress;
    cmd.data[3] = 0;
    cmd.data[4] = 0;
    cmd.data[5] = 0;

    result = sendCommand (pDemod, &cmd);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }

    result = readResponse (pDemod, &cmd, 0xC6);
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (result);
    }
    *pValue = cmd.data[1];

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t SLtoAA(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("SLtoAA");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetTSClockModeAndFreq (pDemod, CXDREF_DTV_SYSTEM_ATSC);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x0F) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
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

    result = demodAbort (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    {
        uint8_t data[4];
        
        data[0] = (uint8_t) ( pDemod->iffreqConfig.configATSC        & 0xFF);
        data[1] = (uint8_t) ((pDemod->iffreqConfig.configATSC >> 8)  & 0xFF);
        data[2] = (uint8_t) ((pDemod->iffreqConfig.configATSC >> 16) & 0xFF);
        data[3] = (uint8_t) ((pDemod->iffreqConfig.configATSC >> 24) & 0xFF);
        result = slaveRWriteMultiRegisters (pDemod, 0xA3, 0xA1, data, sizeof(data));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }
    if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA0, 0x13, 0x03, 0x03);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }

    } else {
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA0, 0x13, 0x00, 0x03);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->tunerOptimize == CXDREF_DEMOD_TUNER_OPTIMIZE_CXDREFSILICON) {
        const uint8_t data[] = { 0x31, 0xA5, 0x2E, 0x9F, 0x2B, 0x99, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0x00, 0x2B, 0x9D };
        result = slaveRWriteMultiRegisters (pDemod, 0x06, 0xA0, data, sizeof(data));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    {
        uint8_t noSignalThresh[3];
        uint8_t signalThresh[3];

        noSignalThresh[0] = (uint8_t)(pDemod->atscNoSignalThresh & 0xFF);
        noSignalThresh[1] = (uint8_t)((pDemod->atscNoSignalThresh >> 8) & 0xFF);
        noSignalThresh[2] = (uint8_t)((pDemod->atscNoSignalThresh >> 16) & 0xFF);
        signalThresh[0] = (uint8_t)(pDemod->atscSignalThresh & 0xFF);
        signalThresh[1] = (uint8_t)((pDemod->atscSignalThresh >> 8) & 0xFF);
        signalThresh[2] = (uint8_t)((pDemod->atscSignalThresh >> 16) & 0xFF);
        result = slaveRWriteMultiRegisters (pDemod, 0xA0, 0x6F, noSignalThresh, sizeof(noSignalThresh));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = slaveRWriteMultiRegisters (pDemod, 0x09, 0x80, noSignalThresh, sizeof(noSignalThresh));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = slaveRWriteMultiRegisters (pDemod, 0xA0, 0x73, signalThresh, sizeof(signalThresh));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = slaveRWriteMultiRegisters (pDemod, 0x09, 0x83, signalThresh, sizeof(signalThresh));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0x06, 0x71, 0x05, 0x07);
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (result);
        }
    }

    if (pDemod->rfSpectrumSense == pDemod->confSense){
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA3, 0xA0, 0x10, 0x10);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    } else {
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA3, 0xA0, 0x00, 0x10);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }
    
    /* customization from original reference code --- begin */
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA2, 0x24, 0x15, 0x1F);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA2, 0x25, 0x0A, 0x1F);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA0, 0x1F, 0x20, 0xF0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA0, 0x24, 0x11, 0xFF);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, 0xA3, 0x50, 0x90, 0xFF);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }
    /* customization from original reference code --- end */
    
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

static cxdref_result_t AAtoAA(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AAtoAA");

    if (!pDemod) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = cxdref_demod_SetStreamOutput (pDemod, 0);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t AAtoSL(cxdref_demod_t * pDemod)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    CXDREF_TRACE_ENTER ("AAtoSL");

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

    result = demodAbort (pDemod);
    if (result != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (result);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xD3, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x00, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x18, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x49, 0x33) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x4B, 0x21) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0x2C, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVT, 0xA9, 0x00) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVX, 0x17, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t sendCommand (cxdref_demod_t *pDemod, const cxdref_demod_atsc_cmd_t *pCmd)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("sendCommand");

    if ((!pDemod) || (!pDemod->pI2c) || (!pCmd)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    result = pDemod->pI2c->WriteRegister (pDemod->pI2c, pDemod->i2cAddressSLVR, 0x0A, pCmd->data, sizeof(pCmd->data));
    if (result != CXDREF_RESULT_OK){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    CXDREF_TRACE_RETURN (result);
}

static cxdref_result_t readResponse (cxdref_demod_t *pDemod, cxdref_demod_atsc_cmd_t *pResp, uint8_t cmdId)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint32_t waitTime = 0;

    CXDREF_TRACE_ENTER ("readResponse");

    if ((!pDemod) || (!pDemod->pI2c) || (!pResp)){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }
    for (;;) {
        result = pDemod->pI2c->ReadRegister (pDemod->pI2c, pDemod->i2cAddressSLVR, 0x0A, pResp->data, sizeof(pResp->data));
        if (result != CXDREF_RESULT_OK) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
        }

        if (pResp->data[0] == 0x00) {
            CXDREF_SLEEP (WAITCOMMANDRESPONSE_POLL_INTERVAL_MS);
            waitTime += WAITCOMMANDRESPONSE_POLL_INTERVAL_MS;
        }
        else {
            if (pResp->data[5] == cmdId) {
                break;
            }
            else {
                CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_HW_STATE);
            }
        }

        if (waitTime >= WAITCPUIDLE_TIMEOUT_MS) {
            CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_TIMEOUT);
        }
    }

    if ((pResp->data[0] & 0x3F) == 0x30) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
    }
    else {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_OTHER);
    }

}

static cxdref_result_t slaveRWriteMultiRegisters (cxdref_demod_t * pDemod,
                                                uint8_t  bank,
                                                uint8_t  registerAddress,
                                                const uint8_t* pData,
                                                uint8_t  size)
{
    uint8_t wtimes = 0;
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_ENTER ("slaveRWriteMultiRegisters");

    if (!pDemod || !pData ) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    for (wtimes = 0; wtimes < size; wtimes++) {
        result = cxdref_demod_atsc_SlaveRWriteRegister (pDemod, bank, registerAddress + wtimes, *(pData + wtimes), 0xFF);
        if (result != CXDREF_RESULT_OK){
            CXDREF_TRACE_RETURN (result);
        }
    }

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t demodAbort (cxdref_demod_t * pDemod)
{

    if (!pDemod){
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_ARG);
    }

    CXDREF_TRACE_ENTER ("demodAbort");

    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVR, 0x00, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }
    if (pDemod->pI2c->WriteOneRegister (pDemod->pI2c, pDemod->i2cAddressSLVR, 0x48, 0x01) != CXDREF_RESULT_OK) {
        CXDREF_TRACE_RETURN (CXDREF_RESULT_ERROR_I2C);
    }

    pDemod->atscCPUState = CXDREF_DEMOD_ATSC_CPU_STATE_IDLE;

    CXDREF_TRACE_RETURN (CXDREF_RESULT_OK);
}
